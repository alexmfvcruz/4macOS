"""Account, character and world-entry check against the running native cluster.

Drives the original Login and Map handlers with the scripted client in
game_client.py:

* create an account row (the original did this from a web site);
* log in: wrong password, first-login agreement, then success;
* list groups, channels and characters; create a character; reject a
  duplicate name; reject an unknown channel;
* start the game, connect to Map, enter the world on the listed channel;
* move, leave, and confirm the position was saved;
* re-enter with the character placed on several other maps.

Run standalone against a cluster started with tools/server/native_cluster.py:

    .local/codex-env/native-cluster-venv/bin/python native/server/tests/native_play_test.py

native_cluster_test.py calls play() as part of tools/check-server.sh.
"""
import hashlib
import secrets
import struct
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/database'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import native_cluster_db as db  # noqa: E402
import game_client as gc  # noqa: E402

LOGIN_PORT, MAP_PORT = 4815, 5815
LR_SUCCESS, LR_INVALIDPASSWD, LR_NEEDAGREEMENT = 0, 2, 8
GROUP = 1
NEW_COUNTRY = 4  # TCreateChar accepts only the "no nation yet" country for a new character
OTHER_MAPS = 3   # how many additional maps to enter


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def sql(statement, params=None, database='TGLOBAL_GSP'):
    connection = db.connect()
    try:
        db.query(connection, f'USE [{database}]')
        return db.query(connection, statement, params)
    finally:
        connection.close()


def wait_for(predicate, message, seconds=30):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.25)
    raise AssertionError(message)


def login(source, user, password_hash):
    session = gc.Session('127.0.0.1', LOGIN_PORT, source)
    session.send('CS_LOGIN_REQ', struct.pack('<H', gc.TVERSION) + gc.string('') + gc.string(password_hash)
                 + gc.string('') + gc.string('') + gc.string(user)
                 + struct.pack('<qq', 0, gc.request_checksum(gc.TVERSION * 2 - 500)))
    ack = session.expect('CS_LOGIN_ACK')
    result, user_id, char_id, key, address, port, create_count = ack.take('BIIIIHB')
    return session, {'result': result, 'user_id': user_id, 'key': key, 'create_count': create_count}


def read_characters(reader):
    reader.take('I')  # check-file point
    characters = []
    for _ in range(reader.take('B')):
        char_id = reader.take('I')
        name = reader.string()
        fields = reader.take('13B')  # start act, slot, level, class, race, country, sex, hair .. foot
        region, fame, fame_color, helmet, items = reader.take('IIIBB')
        for _ in range(items):
            reader.take('BHBBHHBH')
        characters.append({'id': char_id, 'name': name, 'slot': fields[1], 'level': fields[2], 'class': fields[3],
                           'race': fields[4], 'country': fields[5], 'sex': fields[6], 'region': region})
    check(reader.left() == 0, 'CS_CHARLIST_ACK has unread bytes; layout mismatch')
    return characters


def list_characters(session):
    session.send('CS_CHARLIST_REQ', struct.pack('<B', GROUP))
    return read_characters(session.expect('CS_CHARLIST_ACK'))


def read_character_info(reader):
    """Leading fields of CTPlayer::SendCS_CHARINFO_ACK (TMapSvr/CSSender.cpp)."""
    info = {'id': reader.take('I')}
    reader.take('3B')  # secure-code flags
    reader.take('H')   # title
    info['name'] = reader.string()
    looks = reader.take('14B')  # start act, class, race, country, aid country, sex, hair .. foot, helmet, level
    info['country'], info['level'] = looks[3], looks[13]
    reader.take('HIIIBB')  # party, guild, fame, fame colour, guild duty, guild peer
    reader.string()
    reader.take('I')
    reader.string()
    reader.take('3I')  # gold, silver, cooper
    reader.take('3I')  # previous, next and current experience
    info['max_hp'], info['hp'], _, _ = reader.take('4I')
    reader.take('IH')  # party chief, commander
    info['region'], info['map'] = reader.take('IH')
    info['x'], info['y'], info['z'] = reader.take('3f')
    info['dir'] = reader.take('H')
    return info


def start_game(session, channel, char_id):
    session.send('CS_START_REQ', struct.pack('<BBI', GROUP, channel, char_id))
    result, address, port, server_id = session.expect('CS_START_ACK').take('BIHB')
    return result, address, port


def enter_world(source, user_id, char_id, key, channel, address):
    world = gc.Session('127.0.0.1', MAP_PORT, source)
    seed = (gc.TVERSION * user_id + key * char_id) & 0xFFFFFFFF
    world.send('CS_CONNECT_REQ', struct.pack('<HBIIIIHq', gc.TVERSION, channel, user_id, char_id, key,
                                             address, MAP_PORT, gc.request_checksum(seed)))
    connect = world.expect('CS_CONNECT_ACK', 30)
    result = connect.take('B')
    check(result == 0, f'Map connect returned {result}')
    for _ in range(connect.take('B')):
        connect.take('B')
        world.send('CS_CONREADY_REQ')
    info = read_character_info(world.expect('CS_CHARINFO_ACK', 30))
    world.drain(2)
    return world, info


def leave_world(world, user_id):
    world.close()
    wait_for(lambda: not sql('SELECT 1 x FROM TCURRENTUSER WHERE dwUserID=%s', (user_id,)),
             'Session row was not removed after the client left', 60)


def near(a, b, tolerance=1.0):
    return abs(a - b) <= tolerance


def play(source=None, log=print):
    source = source or gc.local_address()
    check(source, 'No non-loopback IPv4 address; Login and Map treat 127.0.0.1 peers as Control')
    suffix = secrets.token_hex(3)
    user, password = 'play' + suffix, secrets.token_hex(8)
    password_hash = hashlib.sha1(password.encode()).hexdigest()  # CTClientGame::GetSHA1String
    name = 'Nat' + suffix
    evidence = {'source': source, 'account': user, 'worlds': []}

    sql("INSERT TACCOUNT_PW(szUserID,szPasswd,bCheck,dFirstLogin,dLastLogin) VALUES(%s,%s,0,GETDATE(),GETDATE())",
        (user, password_hash))
    user_id = sql('SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID=%s', (user,))[0]['dwUserID']

    session, ack = login(source, user, 'not-the-password')
    check(ack['result'] == LR_INVALIDPASSWD, f'Wrong password returned {ack["result"]}')
    check(session.closed_by_server(), 'Login kept a session after a wrong password')
    session.close()

    session, ack = login(source, user, password_hash)
    check(ack['result'] == LR_NEEDAGREEMENT and ack['user_id'] == user_id,
          f'First login returned {ack["result"]} for user {ack["user_id"]}')
    session.send('CS_AGREEMENT_REQ')
    key = ack['key']
    check(key, 'Login returned no session key')
    check(sql('SELECT dwUserID FROM TCURRENTUSER WHERE dwUserID=%s', (user_id,)), 'Login did not record the session')

    session.send('CS_GROUPLIST_REQ')
    groups = session.expect('CS_GROUPLIST_ACK')
    count = groups.take('B')
    groups.take('I')
    listed = []
    for _ in range(count):
        group_name = groups.string()
        group_id, kind, status, characters = groups.take('BBBB')
        listed.append((group_id, group_name, status, characters))
    check(any(group[0] == GROUP for group in listed), f'Group {GROUP} is not listed: {listed}')
    wait_for(lambda: sql('SELECT 1 x FROM TUSERINFOTABLE WHERE dwUserID=%s AND bAgreement=1', (user_id,)),
             'Agreement was not saved')

    session.send('CS_CHANNELLIST_REQ', struct.pack('<B', GROUP))
    channels = session.expect('CS_CHANNELLIST_ACK')
    count = channels.take('B')
    channels.take('I')
    channel_list = []
    for _ in range(count):
        channel_name = channels.string()
        channel_list.append((channels.take('B'), channel_name, channels.take('B')))
    check(channel_list, 'No channels listed')
    channel = channel_list[0][0]
    evidence['groups'], evidence['channels'] = listed, channel_list

    check(list_characters(session) == [], 'New account already has characters')

    # bGroupID, name, slot, class, race, country, sex, hair, face, body, pants, hand, foot, level option
    session.send('CS_CREATECHAR_REQ', struct.pack('<B', GROUP) + gc.string(name)
                 + struct.pack('<12B', 0, 0, 0, NEW_COUNTRY, 0, 0, 0, 0, 0, 0, 0, 0))
    created = session.expect('CS_CREATECHAR_ACK')
    result, char_id = created.take('BI')
    check(result == 0 and char_id, f'Character creation returned {result}')
    check(created.string() == name, 'Character created under a different name')

    session.send('CS_CREATECHAR_REQ', struct.pack('<B', GROUP) + gc.string(name)
                 + struct.pack('<12B', 1, 0, 0, NEW_COUNTRY, 0, 0, 0, 0, 0, 0, 0, 0))
    check(session.expect('CS_CREATECHAR_ACK').take('B') != 0, 'Duplicate character name was accepted')

    characters = list_characters(session)
    check([c['id'] for c in characters] == [char_id] and characters[0]['name'] == name,
          f'Character list after creation: {characters}')
    evidence['character'] = characters[0]

    unknown = max(c[0] for c in channel_list) + 50
    result, _, _ = start_game(session, unknown, char_id)
    check(result != 0, f'Start on unlisted channel {unknown} succeeded')

    result, address, port = start_game(session, channel, char_id)
    check(result == 0 and port == MAP_PORT, f'Start returned {result}, port {port}')
    check(session.closed_by_server(), 'Login kept the session after handing over to Map')
    session.close()
    evidence['login_packets'] = session.summary()

    world, info = enter_world(source, user_id, char_id, key, channel, address)
    stored = sql('SELECT wMapID,fPosX,fPosZ,bLevel FROM TCHARTABLE WHERE dwCharID=%s', (char_id,), 'TGAME_GSP')[0]
    check(info['id'] == char_id and info['name'] == name, f'Map sent another character: {info}')
    check(info['map'] == stored['wMapID'] and near(info['x'], stored['fPosX']) and near(info['z'], stored['fPosZ'])
          and info['level'] == stored['bLevel'] and info['hp'] > 0, f'Character state differs from the database: {info}')
    row = wait_for(lambda: sql('SELECT bGroupID,bChannel FROM TCURRENTUSER WHERE dwUserID=%s AND dwCharID=%s',
                               (user_id, char_id)), 'World did not record the character as in game')
    check(row[0]['bGroupID'] == GROUP and row[0]['bChannel'] == channel, f'In-game session recorded as {row[0]}')
    evidence['worlds'].append({'map': info['map'], 'x': round(info['x'], 1), 'z': round(info['z'], 1),
                               'channel': channel, 'packets': world.summary()})

    # Walk a few steps and leave: the position must be saved.
    target_x, target_z = info['x'] + 3.0, info['z'] + 2.0
    for step in range(1, 4):
        world.send('CS_MOVE_REQ', struct.pack('<HfffHHBBBBf', info['map'], info['x'] + step, info['y'],
                                              info['z'] + step * 2.0 / 3.0, 0, info['dir'], 0, 0, 0, 0, 1.0))
        time.sleep(0.2)
    world.drain(1)
    leave_world(world, user_id)
    saved = wait_for(lambda: [r for r in sql('SELECT fPosX,fPosZ FROM TCHARTABLE WHERE dwCharID=%s', (char_id,), 'TGAME_GSP')
                              if near(r['fPosX'], target_x) and near(r['fPosZ'], target_z)],
                     f'Moved position ({target_x:.1f}, {target_z:.1f}) was not saved', 30)
    evidence['saved_position'] = (round(saved[0]['fPosX'], 1), round(saved[0]['fPosZ'], 1))

    # Enter other maps: place the character on a spawn point of each and log in again.
    spawns = sql('SELECT s.wMapID, MIN(s.wID) wID FROM TSPAWNPOSCHART s JOIN TMAPCHART m ON m.wMapID=s.wMapID '
                 'AND m.bGroupID=%s AND m.bChannel=%s WHERE s.wMapID<>%s GROUP BY s.wMapID ORDER BY s.wMapID',
                 (GROUP, channel, info['map']), 'TGAME_GSP')
    check(len(spawns) >= OTHER_MAPS, f'Only {len(spawns)} other maps have spawn points')
    step = len(spawns) // OTHER_MAPS
    for spawn in spawns[::step][:OTHER_MAPS]:
        point = sql('SELECT wMapID,fPosX,fPosY,fPosZ FROM TSPAWNPOSCHART WHERE wID=%s', (spawn['wID'],), 'TGAME_GSP')[0]
        sql('UPDATE TCHARTABLE SET wMapID=%s,fPosX=%s,fPosY=%s,fPosZ=%s WHERE dwCharID=%s',
            (point['wMapID'], point['fPosX'], point['fPosY'], point['fPosZ'], char_id), 'TGAME_GSP')
        session, ack = login(source, user, password_hash)
        check(ack['result'] == LR_SUCCESS, f'Login after agreement returned {ack["result"]}')
        check([c['id'] for c in list_characters(session)] == [char_id], 'Character missing on a later login')
        result, address, port = start_game(session, channel, char_id)
        check(result == 0, f'Start for map {point["wMapID"]} returned {result}')
        session.close()
        world, info = enter_world(source, user_id, char_id, ack['key'], channel, address)
        check(info['map'] == point['wMapID'] and near(info['x'], point['fPosX']) and near(info['z'], point['fPosZ']),
              f'Entered map {info["map"]} at ({info["x"]:.1f}, {info["z"]:.1f}), expected {point}')
        evidence['worlds'].append({'map': info['map'], 'x': round(info['x'], 1), 'z': round(info['z'], 1),
                                   'channel': channel, 'packets': world.summary()})
        leave_world(world, user_id)

    maps = [w['map'] for w in evidence['worlds']]
    log(f'PASS scripted client: account {user} rejected with a wrong password, accepted the agreement, logged in, '
        f'created character {name} (id {char_id}), entered channel {channel}, moved and saved its position, '
        f'and entered maps {maps}')
    return evidence


if __name__ == '__main__':
    result = play()
    for key, value in result.items():
        if key == 'worlds':
            for world in value:
                print('world:', world)
        else:
            print(f'{key}: {value}')
