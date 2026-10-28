"""Live Milestone 4 readiness check for the original server modules.

Resets the writable TGLOBAL_GSP/TGAME_GSP copies, seeds test rows, starts the
four native module executables through tools/server/native_cluster.py and
verifies against the original code paths:

* each role opens its databases, loads its original state and listens;
* Control finds every service through the emulated Service Control Manager and
  completes its management connection (original CT_NEWCONNECT_REQ path);
* Map registers with World (MW_CONNECT_ACK) only after World is ready;
* Login's original startup cleanup removes stale current-user rows only;
* a scripted client logs in, creates a character and enters the world on
  several maps (native_play_test.py);
* a manager session authenticates (TOPLogin) and adds/deletes an event through
  Control, persisting to and removing from TEVENTCHART;
* a manager stop request stops Login via the SCM, Control records the stop,
  and every role exits cleanly.

Requires the isolated 4story-audit-sql container and the venv described in
research/database/native-cluster.md. The copies are reset again at the end.
"""
import json
import os
import secrets
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/database'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import native_cluster_db as db  # noqa: E402
import game_client  # noqa: E402
import native_play_test  # noqa: E402

RUN = ROOT / 'runtime/server-dev'
CLUSTER = [sys.executable, str(ROOT / 'tools/server/native_cluster.py')]
ROLES = ['TControlSvr', 'TLoginSvr', 'TWorldSvr', 'TMapSvr']
SERVICE_IDS = {'TLoginSvr': 513, 'TWorldSvr': 66305, 'TMapSvr': 66561}
CT_CONTROL = 0x9301
CT_OPLOGIN_REQ, CT_OPLOGIN_ACK = CT_CONTROL + 0x01, CT_CONTROL + 0x02
CT_SERVICECONTROL_REQ, CT_SERVICECONTROL_ACK = CT_CONTROL + 0x08, CT_CONTROL + 0x09
CT_EVENTCHANGE_REQ, CT_EVENTCHANGE_ACK = CT_CONTROL + 0x61, CT_CONTROL + 0x62
EK_DEL, EK_ADD = 0, 1
EVENT_EXPADD = 1
SVRGRP_MAPSVR = 4
MANAGER = 'native_check'
# Logs a clean shutdown may contain: the original accept loops report their
# aborted pending AcceptEx when the listener closes, and Control reports each
# graceful peer close as zero-byte "Connect Error 0, 0" (TControlSvr.cpp:1148).
EXPECTED_SHUTDOWN = ('WaitForConnect : Error', 'Socket acception has failed!', 'Connect Error 0, 0')


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def cluster(*args, timeout=600):
    result = subprocess.run(CLUSTER + list(args), cwd=ROOT, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise AssertionError(f'native_cluster {" ".join(args)} failed: {result.stdout[-2000:]}{result.stderr[-2000:]}')
    return result.stdout


def sql(statement, params=None, database='TGLOBAL_GSP'):
    connection = db.connect()
    try:
        db.query(connection, f'USE [{database}]')
        return db.query(connection, statement, params)
    finally:
        connection.close()


def log(role):
    return (RUN / f'{role}.log').read_text(errors='replace')


def wait_for(predicate, message, seconds=60):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.25)
    raise AssertionError(message)


class Manager:
    """Minimal Control manager client using the original packet framing."""

    def __init__(self):
        self.sock = socket.create_connection(('127.0.0.1', 3615), timeout=10)

    def send(self, message, payload=b''):
        header = struct.pack('<HHIq', 16 + len(payload), message, 0, 0)
        self.sock.sendall(header + payload)

    def receive(self, wanted, seconds=15):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            header = self._exact(16)
            size, message, _, _ = struct.unpack('<HHIq', header)
            body = self._exact(size - 16)
            if message == wanted:
                return body
        raise AssertionError(f'No packet 0x{wanted:04X} from Control')

    def _exact(self, count):
        data = b''
        while len(data) < count:
            chunk = self.sock.recv(count - len(data))
            if not chunk:
                raise AssertionError('Control closed the manager session')
            data += chunk
        return data

    def close(self):
        self.sock.close()


def text(value):
    raw = value.encode('latin1')
    return struct.pack('<i', len(raw)) + raw


def event(index, start, end, title):
    # EVENTINFO::WrapPacketIn/WrapPacketOut field order (TControlType.h).
    return (struct.pack('<IBBBBBqqHHII', index, EVENT_EXPADD, 0, 1, SVRGRP_MAPSVR, 0, start, end, 150, 0xFF, 0, 0)
            + text('start') + text('end') + text(title) + struct.pack('<B', 0) + text('')
            + struct.pack('<H', 0) + struct.pack('<BBH', 0, 0, 0) + struct.pack('<H', 0) + struct.pack('<H', 0))


def stamps(role, needle):
    return [line[:19] for line in log(role).splitlines() if needle in line]


def main():
    for role in ROLES:
        check((Path(os.environ.get('FOURSTORY_MODULE_DIR', ROOT / 'build/server/module')) / role).is_file(), f'{role} is not built (run sh tools/build-modules.sh)')
    cluster('reset-db')
    password = secrets.token_hex(12)
    # Seed: one stale login (no character) and one in-game session, plus a
    # temporary manager account. Only the writable copies are changed.
    sql("INSERT TCURRENTUSER(dwUserID,dwCharID,bGroupID,bChannel,szIPAddr,wPort,bLocked,szLoginIP) VALUES"
        "(900001,0,1,1,'127.0.0.1',0,0,'127.0.0.1'),(900002,4242,1,1,'127.0.0.1',0,0,'127.0.0.1')")
    sql("INSERT TMANAGER(szID,szPasswd,bOPAuthority,bAuthority,szName) VALUES(%s,%s,1,1,'native cluster check')",
        (MANAGER, password))
    started = False
    try:
        cluster('start', '--timeout', '240')
        started = True
        for role in ROLES:
            check(f'{role}: Native startup complete' in log(role), f'{role} did not complete original startup')
        # Map can only finish InitNetwork after World accepted its connection.
        world_ready = stamps('TWorldSvr', 'Native startup complete')[0]
        map_ready = stamps('TMapSvr', 'Native startup complete')[0]
        check(world_ready <= map_ready, 'Map became ready before World')
        wait_for(lambda: 'Native handshake: Map server' in log('TWorldSvr'), 'World never registered Map')
        for role, service in SERVICE_IDS.items():
            wait_for(lambda: f'New Connect {service}' in log('TControlSvr'), f'Control did not connect to {role}')
        lsof = subprocess.run(['/usr/sbin/lsof', '-nP', '-iTCP', '-sTCP:ESTABLISHED'], capture_output=True, text=True).stdout
        for port in (4815, 3815, 5815):
            check(any(line.startswith('TControlS') and f'->127.0.0.1:{port}' in line for line in lsof.splitlines()),
                  f'No Control management session to port {port}')
        check(any(line.startswith('TMapSvr') and '->127.0.0.1:3815' in line for line in lsof.splitlines()),
              'No Map session to World')

        rows = {r['dwUserID']: r for r in sql('SELECT dwUserID FROM TCURRENTUSER WHERE dwUserID IN (900001,900002)')}
        check(900001 not in rows and 900002 in rows, 'Login startup cleanup did not remove only the stale session')

        # Account login, character creation and world entry through the
        # original client handlers (native_play_test.py).
        if game_client.local_address():
            native_play_test.play()
        else:
            print('SKIP scripted client: no non-loopback IPv4 address (Login and Map treat 127.0.0.1 as Control)')

        manager = Manager()
        try:
            manager.send(CT_OPLOGIN_REQ, text(MANAGER) + text(password))
            result, authority, _ = struct.unpack('<BBI', manager.receive(CT_OPLOGIN_ACK)[:6])
            check(result == 0 and authority == 1, 'Manager authentication failed')
            index = 900000 + secrets.randbelow(90000)
            start = int(time.time()) + 30 * 86400
            title = 'native-' + secrets.token_hex(4)
            manager.send(CT_EVENTCHANGE_REQ, struct.pack('<B', EK_ADD) + event(index, start, start + 86400, title))
            ack = manager.receive(CT_EVENTCHANGE_ACK)
            check(ack[0] == 0 and ack[1] == EK_ADD, f'Event add rejected with result {ack[0]}')
            saved = sql('SELECT bID,szTitle,bGroupID,bSvrType,wValue FROM TEVENTCHART WHERE dwIndex=%s', (index,))
            check(saved == [{'bID': EVENT_EXPADD, 'szTitle': title, 'bGroupID': 1, 'bSvrType': SVRGRP_MAPSVR, 'wValue': 150}],
                  f'Event was not persisted as sent: {saved}')
            manager.send(CT_EVENTCHANGE_REQ, struct.pack('<B', EK_DEL) + event(index, start, start + 86400, title))
            ack = manager.receive(CT_EVENTCHANGE_ACK)
            check(ack[0] == 0 and ack[1] == EK_DEL, f'Event delete rejected with result {ack[0]}')
            check(not sql('SELECT dwIndex FROM TEVENTCHART WHERE dwIndex=%s', (index,)), 'Event delete left its row')

            # Stop Login through Control: StopService -> SCM ControlService(STOP).
            manager.send(CT_SERVICECONTROL_REQ, struct.pack('<IB', SERVICE_IDS['TLoginSvr'], 0))
            check(manager.receive(CT_SERVICECONTROL_ACK)[0] == 1, 'Control refused to stop Login')
        finally:
            manager.close()
        wait_for(lambda: 'QS_Stopped Server : TLoginSvr' in log('TControlSvr'), 'Control did not observe Login stopping')
        wait_for(lambda: json.loads((RUN / 'state.json').read_text())['phase'] in ('stopped', 'failed'),
                 'Cluster did not finish stopping after Login exited', 180)
        started = False
        state = json.loads((RUN / 'state.json').read_text())
        for role in ROLES:
            check(state['roles'][role]['exit_code'] == 0, f'{role} exited with {state["roles"][role]["exit_code"]}')
        for role in ROLES:
            for line in log(role).splitlines():
                lowered = line.lower()
                if ('error' in lowered or 'exception' in lowered or 'failed' in lowered) and not line.endswith(EXPECTED_SHUTDOWN):
                    raise AssertionError(f'Unexpected {role} log line: {line}')
    finally:
        if started:
            cluster('stop')
        cluster('reset-db')
    print('PASS original Control/Login/World/Map native startup, SCM discovery and management connections, '
          'Map-to-World registration after World readiness, Login cleanup, manager event persistence/deletion, '
          'SCM stop and clean shutdown')


if __name__ == '__main__':
    main()
