#!/usr/bin/env python3
"""4Story server control for the release package (C8).

Usage: python3 lib/server.py install|start|stop|status|create-account|logs

Runs the four original server programs in bin/ against SQL Server Express in
a Colima virtual machine. Only Python's standard library is used; database
commands run with sqlcmd inside the SQL Server container. The SQL password is
kept in the login Keychain. Settings and state:
~/Library/Application Support/4Story Server; logs: ~/Library/Logs/4Story Server.
"""
import argparse
import fcntl
import getpass
import hashlib
import ipaddress
import json
import os
import re
import secrets
import shutil
import signal
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
PACKAGE = WORKSPACE / 'server'
HOME = Path.home()
SUPPORT = WORKSPACE / 'runtime/server'
LOGS = SUPPORT / 'logs'
WORK = SUPPORT / 'bin'          # running copy of bin/; the programs write logs beside themselves
STATE = SUPPORT / 'state.json'
CONFIG = SUPPORT / 'config.json'
BREW = Path('/opt/homebrew/bin/brew')
PROFILE = '4story-port'
CONTEXT = 'colima-' + PROFILE
CONTAINER = '4story-port-sql'
VOLUME = '4story-port-data'
IMAGE = 'mcr.microsoft.com/mssql/server:2022-latest'
SQL_PORT = 14334                # bound to 127.0.0.1 only
SQLCMD = '/opt/mssql-tools18/bin/sqlcmd'
KEYCHAIN = '4Story Port Server'
DATABASES = {'TGLOBAL': 'TGLOBAL_GSP', 'TGAME': 'TGAME_GSP'}
ROLES = [('TControlSvr', 3615), ('TLoginSvr', 4815), ('TWorldSvr', 3815), ('TMapSvr', 5815)]
READY = 'Native startup complete'
# Original debug text sockets need a receiver; 7000 belongs to AirPlay Receiver.
DEBUG_PORT = 4700
PACKAGES = ['colima', 'docker', 'unixodbc', 'openssl@3']
EULA_URL = 'https://go.microsoft.com/fwlink/?LinkId=746388'


class Failure(Exception):
    """A message for the user; printed without a traceback."""


def say(text=''):
    print(text, flush=True)


def ask(question, default=None):
    if not sys.stdin.isatty():
        if default is None:
            raise Failure('A terminal is needed to answer: ' + question)
        return default
    suffix = f' [{default}]' if default else ''
    answer = input(f'{question}{suffix}: ').strip()
    return answer or (default or '')


def confirm(question, assume):
    if assume:
        say(question + ' yes')
        return True
    return ask(question + ' (yes/no)', 'no' if not sys.stdin.isatty() else None).lower() in ('y', 'yes')


def run(command, check=True, quiet=False, env=None, **kwargs):
    result = subprocess.run(command, env=env, text=True, capture_output=quiet, **kwargs)
    if check and result.returncode:
        detail = ((result.stderr or '') + (result.stdout or '')).strip()[-500:] if quiet else ''
        name = ' '.join(Path(str(part)).name if index == 0 else str(part) for index, part in enumerate(command[:4]))
        raise Failure('This step failed: ' + name + ' ...' + ('\n' + detail if detail else ''))
    return result


def write_private(path, text):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, 'w') as stream:
        stream.write(text)
    path.chmod(0o600)


def load_config():
    return json.loads(CONFIG.read_text()) if CONFIG.exists() else {}


# ---------------------------------------------------------------------------
# Keychain

def password():
    result = subprocess.run(['/usr/bin/security', 'find-generic-password', '-s', KEYCHAIN, '-a', 'sa', '-w'],
                            capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


def store_password(value):
    # security -i reads the command from stdin, so the password is not in the process list.
    command = f'add-generic-password -U -s "{KEYCHAIN}" -a sa -l "{KEYCHAIN} database" -w "{value}"\n'
    result = subprocess.run(['/usr/bin/security', '-i'], input=command, text=True, capture_output=True)
    if result.returncode or password() != value:
        raise Failure('Could not save the database password in the Keychain.')


def new_password():
    # SQL Server needs upper case, lower case, digits and symbols; no quote characters.
    alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789'
    body = ''.join(secrets.choice(alphabet) for _ in range(28))
    return body + 'Aa9#'


# ---------------------------------------------------------------------------
# Colima, container and sqlcmd

def docker(*args, **kwargs):
    return run(['/opt/homebrew/bin/docker', '--context', CONTEXT, *args], **kwargs)


def colima_running():
    result = subprocess.run(['/opt/homebrew/bin/colima', 'status', '-p', PROFILE], capture_output=True, text=True)
    return result.returncode == 0


def start_colima():
    if colima_running():
        return
    say('Starting the database virtual machine (Colima profile ' + PROFILE + ') ...')
    # colima start makes its profile the current Docker context; keep the user's.
    previous = subprocess.run(['/opt/homebrew/bin/docker', 'context', 'show'], capture_output=True, text=True)
    try:
        run(['/opt/homebrew/bin/colima', 'start', '-p', PROFILE, '--arch', 'aarch64', '--vm-type', 'vz',
             '--vz-rosetta', '--cpus', '2', '--memory', '4', '--disk', '20'])
    finally:
        if previous.returncode == 0 and previous.stdout.strip():
            subprocess.run(['/opt/homebrew/bin/docker', 'context', 'use', previous.stdout.strip()],
                           capture_output=True)


def container_state():
    result = docker('inspect', '-f', '{{.State.Running}}', CONTAINER, check=False, quiet=True)
    if result.returncode:
        return None
    return result.stdout.strip() == 'true'


def sql(script, database='master', secret=None):
    """Run T-SQL with sqlcmd in the container; returns rows split on '|'."""
    env = dict(os.environ, SQLCMDPASSWORD=secret or password() or '')
    command = ['/opt/homebrew/bin/docker', '--context', CONTEXT, 'exec', '-i', '-e', 'SQLCMDPASSWORD', CONTAINER,
               SQLCMD, '-S', 'localhost', '-U', 'sa', '-C', '-b', '-x', '-d', database, '-h', '-1', '-W', '-s', '|']
    result = subprocess.run(command, input='SET NOCOUNT ON;\n' + script + '\nGO\n', text=True,
                            capture_output=True, env=env)
    if result.returncode:
        # sqlcmd prints server messages, never the password.
        raise Failure('Database command failed:\n' + (result.stdout + result.stderr).strip()[-800:])
    return [line.split('|') for line in result.stdout.splitlines() if line.strip()]


def wait_sql(secret=None, timeout=240):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if sql('SELECT 1', secret=secret) == [['1']]:
                return
        except Failure:
            pass
        time.sleep(3)
    raise Failure('SQL Server did not start. Run Logs.command and look at sql-server.log.')


def installed():
    return BREW.exists() and Path('/opt/homebrew/bin/colima').exists() and (HOME / '.colima' / PROFILE).is_dir()


def start_database():
    if not installed():
        raise Failure('The server is not installed. Run Install.command first.')
    if password() is None:
        raise Failure('The database password is not in the Keychain, or the Keychain is locked. '
                      'Log in to this Mac, or see "Start again from nothing" in README.md.')
    start_colima()
    state = container_state()
    if state is None:
        raise Failure('The database is not installed. Run Install.command first.')
    if not state:
        docker('start', CONTAINER, quiet=True)
    wait_sql()


# ---------------------------------------------------------------------------
# Install

def literal(value):
    return "N'" + value.replace("'", "''") + "'"


def install_packages(assume):
    if os.uname().machine != 'arm64':
        raise Failure('This server package needs a Mac with Apple silicon (M1 or later).')
    if not BREW.exists():
        say('Homebrew is not installed. Homebrew installs the tools the server needs.')
        say('See https://brew.sh. The official installer runs:')
        say('  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"')
        if not confirm('Run the Homebrew installer now?', assume):
            raise Failure('Install Homebrew, then run Install.command again.')
        script = run(['/usr/bin/curl', '-fsSL', 'https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh'],
                     quiet=True).stdout
        run(['/bin/bash', '-c', script])
    missing = [name for name in PACKAGES
               if run([str(BREW), 'list', '--versions', name], check=False, quiet=True).returncode]
    if missing:
        say('Installing with Homebrew: ' + ', '.join(missing))
        run([str(BREW), 'install', *missing])
    if run([str(BREW), 'list', '--versions', 'msodbcsql18'], check=False, quiet=True).returncode:
        say('The servers connect to the database with the Microsoft ODBC Driver 18.')
        say('Its license: https://aka.ms/odbc18eula')
        if not confirm('Do you accept the Microsoft ODBC Driver license?', assume):
            raise Failure('The server cannot run without the ODBC driver.')
        run([str(BREW), 'tap', 'microsoft/mssql-release', 'https://github.com/Microsoft/homebrew-mssql-release'])
        run([str(BREW), 'install', 'msodbcsql18'], env=dict(os.environ, HOMEBREW_ACCEPT_EULA='Y'))
    if subprocess.run(['/usr/bin/arch', '-x86_64', '/usr/bin/true'], capture_output=True).returncode:
        say('SQL Server runs with Rosetta, Apple\'s Intel translation.')
        if not confirm('Install Rosetta now?', assume):
            raise Failure('The database cannot run without Rosetta.')
        run(['/usr/sbin/softwareupdate', '--install-rosetta', '--agree-to-license'])


def install_container(assume):
    start_colima()
    secret = password()
    if container_state() is None:
        say('SQL Server 2022 Express runs in a container. Its license terms: ' + EULA_URL)
        if not confirm('Do you accept the SQL Server Express license terms?', assume):
            raise Failure('The server cannot run without SQL Server.')
        if secret is None:
            secret = new_password()
            store_password(secret)
        say('Downloading and starting SQL Server (the first time takes a few minutes) ...')
        docker('run', '-d', '--name', CONTAINER, '--platform', 'linux/amd64', '--restart', 'unless-stopped',
               '-e', 'ACCEPT_EULA=Y', '-e', 'MSSQL_PID=Express', '-e', 'MSSQL_SA_PASSWORD',
               '-p', f'127.0.0.1:{SQL_PORT}:1433', '-v', f'{VOLUME}:/var/opt/mssql', IMAGE,
               env=dict(os.environ, MSSQL_SA_PASSWORD=secret), quiet=True)
    elif secret is None:
        raise Failure('The database exists, but its password is not in the Keychain. '
                      'See "Start again from nothing" in README.md.')
    elif not container_state():
        docker('start', CONTAINER, quiet=True)
    wait_sql(secret)
    edition = sql("SELECT CAST(SERVERPROPERTY('Edition') AS nvarchar(100))")[0][0]
    say('Database ready: ' + edition)


def restore_databases():
    present = {row[0] for row in sql('SELECT name FROM sys.databases')}
    if all(name in present for name in DATABASES.values()):
        say('The game databases already exist. They are kept (players and characters stay).')
        return
    if any(name in present for name in DATABASES.values()):
        raise Failure('Only one of the two game databases exists. See "Start again from nothing" in README.md.')
    docker('exec', CONTAINER, 'mkdir', '-p', '/var/opt/mssql/backup', quiet=True)
    try:
        for release in DATABASES.values():
            restore_one(release)
    except Failure:
        # Neither database existed before; leave none behind (also a half
        # restored one), so Install can simply run again.
        for release in DATABASES.values():
            sql(f"IF DB_ID('{release}') IS NOT NULL BEGIN ALTER DATABASE [{release}] SET SINGLE_USER "
                f"WITH ROLLBACK IMMEDIATE; DROP DATABASE [{release}]; END")
        raise


def restore_one(release):
    source = PACKAGE / 'database' / (release + '.bak')
    if not source.is_file():
        raise Failure('Missing ' + str(source))
    docker('cp', str(source), f'{CONTAINER}:/var/opt/mssql/backup/{release}.bak', quiet=True)
    backup = f'/var/opt/mssql/backup/{release}.bak'
    files = sql(f'RESTORE FILELISTONLY FROM DISK={literal(backup)}')
    moves = []
    for index, row in enumerate(files):
        suffix = 'ldf' if row[2] == 'L' else 'mdf'
        moves.append(f"MOVE {literal(row[0])} TO {literal(f'/var/opt/mssql/data/{release}-{index}.{suffix}')}")
    say('Restoring ' + release + ' ...')
    sql(f"RESTORE DATABASE [{release}] FROM DISK={literal(backup)} WITH {', '.join(moves)}, RECOVERY, CHECKSUM")
    docker('exec', CONTAINER, 'rm', '-f', backup, quiet=True)


def lan_address():
    for interface in ('en0', 'en1'):
        result = subprocess.run(['/usr/sbin/ipconfig', 'getifaddr', interface], capture_output=True, text=True)
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
    return None


def valid_address(value):
    try:
        address = ipaddress.IPv4Address(value)
    except ValueError:
        return False
    return not (address.is_loopback or address.is_unspecified or address.is_multicast)


def choose_address(given):
    config = load_config()
    default = given or config.get('public_address') or lan_address()
    say('Players connect to the address you enter here.')
    say('Same network (home, office): this Mac\'s address, e.g. ' + (lan_address() or '192.168.1.20') + '.')
    say('Internet: your public IP address, with router port forwarding (see README.md).')
    while True:
        value = given or ask('Address players use', default)
        if valid_address(value):
            return value
        if given or not sys.stdin.isatty():
            raise Failure('Invalid address: ' + value)
        say('Enter an IPv4 address such as 192.168.1.20. 127.0.0.1 does not work for players.')


def apply_topology(public):
    # The same single-Mac topology as tools/database/native_cluster_db.py reset().
    sql(f"""SET XACT_ABORT ON; BEGIN TRANSACTION;
DELETE FROM TSERVER; DELETE FROM TIPADDR; DELETE FROM TNETWORK; DELETE FROM TMACHINE;
INSERT TMACHINE(bMachineID,szNAME,bRouteID) VALUES(1,'localhost',1);
INSERT TIPADDR(bMachineID,szIPAddr,szPriAddr,bActive) VALUES(1,{literal(public)},'127.0.0.1',1);
DELETE FROM TGROUP WHERE bGroupID NOT IN(0,1);
UPDATE TGROUP SET szNAME='4Story',szDSN='4Story_Native_Game',szUserID='sa',szPasswd='env:FOURSTORY_DB_PASSWORD',bStatus=1 WHERE bGroupID=1;
INSERT TSERVER(bGroupID,bServerID,bType,bMachineID,wPort,szName) VALUES
(0,1,1,1,3615,'TControlSvr'),(0,1,2,1,4815,'TLoginSvr'),
(1,1,3,1,3815,'TWorldSvr'),(1,1,4,1,5815,'TMapSvr'),
(1,1,8,1,4015,'InactiveRelay');
UPDATE TSVRTYPE SET bControl=0 WHERE bType=8;
-- Keep channel 50 (BR): the client drops the last channel of the list.
DELETE FROM TCHANNEL WHERE bGroupID<>1 OR bChannel NOT IN(1,50);
IF NOT EXISTS(SELECT 1 FROM TCHANNEL WHERE bGroupID=1 AND bChannel=50)
  INSERT TCHANNEL(bGroupID,bChannel,szNAME,wFull,wBusy,bStatus) VALUES(1,50,'BR',350,300,1);
COMMIT;""", 'TGLOBAL_GSP')
    # TGAME_GSP.TCHANNEL is a view of TGLOBAL_GSP.TCHANNEL; deleting through it
    # would remove channel 50 again.
    sql("""SET XACT_ABORT ON; BEGIN TRANSACTION;
SELECT DISTINCT CAST(1 AS tinyint) AS bGroup,CAST(1 AS tinyint) AS bServerID,wMapID,wUnitID INTO #units FROM TUNITCHART WHERE bGroup=1;
SELECT DISTINCT CAST(1 AS tinyint) AS bGroupID,wMapID,CAST(1 AS tinyint) AS bServerID,CAST(1 AS tinyint) AS bChannel INTO #maps FROM TMAPCHART WHERE bGroupID=1;
SELECT DISTINCT CAST(1 AS tinyint) AS bGroupID,wMapID,wUnitID,CAST(1 AS tinyint) AS bLogChannel,CAST(1 AS tinyint) AS bPhyChannel INTO #channels FROM TCHANNELCHART WHERE bGroupID=1;
DELETE FROM TUNITCHART; INSERT TUNITCHART SELECT * FROM #units;
DELETE FROM TMAPCHART; INSERT TMAPCHART SELECT * FROM #maps;
DELETE FROM TCHANNELCHART; INSERT TCHANNELCHART SELECT * FROM #channels;
DELETE c FROM TCHANNELCHART c WHERE NOT EXISTS(SELECT 1 FROM TSVRCHART s WHERE s.bGroup=c.bGroupID AND s.wMapID=c.wMapID AND s.wUnitID=c.wUnitID AND s.bChannel=c.bPhyChannel);
UPDATE TDBITEMINDEXTABLE SET bWorld=1;
COMMIT;""", 'TGAME_GSP')
    for database in DATABASES.values():
        for group, service, port in [(0, 1, 3615), (1, 8, 4015)]:
            row = sql(f'DECLARE @ip varchar(50),@port smallint; EXEC dbo.TLoadService {group},{service},'
                      '@ip OUTPUT,@port OUTPUT; SELECT @ip,@port', database)
            if row != [['127.0.0.1', str(port)]]:
                raise Failure('The server address setup did not verify in ' + database)


def manager_login(name, secret):
    if not re.fullmatch(r'[A-Za-z0-9_]{3,20}', name):
        raise Failure('The Happy Tool name must have 3 to 20 letters, digits or _.')
    if not 4 <= len(secret) <= 20 or any(c in secret for c in "'$:\r\n"):
        raise Failure('The Happy Tool password must have 4 to 20 characters, without \' $ : or line breaks.')
    sql(f"""DELETE FROM TMANAGER WHERE szID={literal(name)};
INSERT TMANAGER(szID,szPasswd,bOPAuthority,bAuthority,szName,dCreateDate)
VALUES({literal(name)},{literal(secret)},1,1,'Happy Tool',GETDATE());""", 'TGLOBAL_GSP')


def install(args):
    SUPPORT.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    if state_running():
        raise Failure('The server is running. Run Stop.command first.')
    say('Step 1 of 4: tools (Homebrew, Colima, Docker, ODBC driver, Rosetta)')
    install_packages(args.yes)
    # Downloaded files are quarantined; the server programs are not signed by Apple.
    run(['/usr/bin/xattr', '-dr', 'com.apple.quarantine', str(PACKAGE)], check=False, quiet=True)
    say('Step 2 of 4: database (SQL Server Express)')
    install_container(args.yes)
    restore_databases()
    say('Step 3 of 4: player address')
    public = choose_address(args.address)
    apply_topology(public)
    config = load_config()
    config.update(public_address=public)
    write_private(CONFIG, json.dumps(config, indent=2) + '\n')
    say('Players use: ' + public)
    say('Step 4 of 4: Happy Tool login (optional)')
    say('Happy Tool stores this password as plain text in the database (original design).')
    name = args.manager or ask('Happy Tool login name (press Enter to skip)', '' if not sys.stdin.isatty() else None)
    if name:
        secret = os.environ.get('FOURSTORY_MANAGER_PASSWORD') or getpass.getpass('Happy Tool password: ')
        manager_login(name, secret)
        say('Happy Tool login saved.')
    say()
    say('The database password is in Keychain Access, entry 4Story Port Server.')
    say('Install finished. Next: open Start.command.')


# ---------------------------------------------------------------------------
# Server processes (adapted from tools/server/native_cluster.py)

def config_files():
    for path in (SUPPORT, LOGS, SUPPORT / 'drive'):
        path.mkdir(parents=True, exist_ok=True)
    SUPPORT.chmod(0o700)
    ini = ''
    for kind, dsn in [('TGLOBAL', '4Story_Native_Global'), ('TGAME', '4Story_Native_Game')]:
        ini += (f'[{dsn}]\nDriver=ODBC Driver 18 for SQL Server\nServer=127.0.0.1,{SQL_PORT}\n'
                f'Database={DATABASES[kind]}\nEncrypt=yes\nTrustServerCertificate=yes\n\n')
    write_private(SUPPORT / 'odbc.ini', ini)
    for role, port in ROLES:
        is_game = role in ('TWorldSvr', 'TMapSvr')
        entries = {'DBUser': 'sz:sa', 'DSN': 'sz:4Story_Native_Game' if is_game else 'sz:4Story_Native_Global',
                   'DBPasswd': 'env:FOURSTORY_DB_PASSWORD', 'Port': f'dword:{port}'}
        if role != 'TControlSvr':
            entries['ServerID'] = 'dword:1'
        if is_game:
            entries['GroupID'] = 'dword:1'
        if role == 'TMapSvr':
            entries['GameDSN'] = entries.pop('DSN')
            entries['GamePasswd'] = entries.pop('DBPasswd')
            entries['GamePort'] = entries.pop('Port')
            entries.update(WorldIP='sz:127.0.0.1', WorldPort='dword:3815')
        if role in ('TLoginSvr', 'TMapSvr'):
            entries.update(LogIP='sz:127.0.0.1', LogPort=f'dword:{DEBUG_PORT}')
        # AutoStart is intentionally absent: its presence enables original SCM behavior.
        text = '[SYSTEM\\CurrentControlSet\\Services\\' + role + '\\Config]\n'
        text += ''.join(f'{key}={value}\n' for key, value in entries.items())
        write_private(SUPPORT / (role + '.registry'), text)
    # Running copy of the programs; they write some logs next to themselves.
    WORK.mkdir(exist_ok=True)
    for role, _ in ROLES:
        source, target = PACKAGE / 'bin' / role, WORK / role
        if not target.exists() or target.read_bytes() != source.read_bytes():
            shutil.copy2(source, target)
    link = LOGS / 'Program logs'
    if not link.is_symlink():
        link.symlink_to(WORK)


def environment(role, secret):
    tls = '/opt/homebrew/opt/openssl@3/lib'
    if not Path(tls, 'libssl.3.dylib').is_file():
        raise Failure('OpenSSL 3 is missing. Run Install.command again.')
    result = os.environ.copy()
    result.update(ODBCSYSINI='/opt/homebrew/etc', DYLD_LIBRARY_PATH=str(WORKSPACE/'deps/odbc/lib') + os.pathsep + tls, FOURSTORY_DB_PASSWORD=secret,
                  FOURSTORY_REGISTRY=str(SUPPORT / (role + '.registry')), ODBCINI=str(SUPPORT / 'odbc.ini'),
                  FOURSTORY_SCM_DIR=str(SUPPORT / 'scm'), FOURSTORY_DEBUG_LOG_PORT=str(DEBUG_PORT),
                  FOURSTORY_DRIVE_ROOT=str(SUPPORT / 'drive'),
                  # StartService (win32.cpp) runs this with the service name as $1; the
                  # supervisor owns every process and picks the request up.
                  FOURSTORY_SCM_START=': > "$FOURSTORY_SCM_DIR/$1.start"')
    return result


def read_state():
    try:
        return json.loads(STATE.read_text())
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def save_state(state):
    temp = STATE.with_suffix('.tmp')
    write_private(temp, json.dumps(state, indent=2) + '\n')
    temp.replace(STATE)


def alive(pid):
    if not pid:
        return False
    try:
        os.kill(pid, 0)
        return True
    except (ProcessLookupError, PermissionError):
        return False


def owned_supervisor(state):
    pid = state.get('supervisor_pid')
    if not alive(pid):
        return False
    result = subprocess.run(['ps', '-p', str(pid), '-o', 'command='], capture_output=True, text=True)
    return str(Path(__file__).resolve()) in result.stdout and '_supervise' in result.stdout


def state_running():
    return owned_supervisor(read_state())


def port_open(port):
    with socket.socket() as sock:
        sock.settimeout(.2)
        return sock.connect_ex(('127.0.0.1', port)) == 0


def owns_listener(pid, port):
    result = subprocess.run(['/usr/sbin/lsof', '-nP', '-a', '-p', str(pid), f'-iTCP:{port}', '-sTCP:LISTEN', '-t'],
                            capture_output=True, text=True)
    return str(pid) in result.stdout.split()


def debug_receiver():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', DEBUG_PORT))
    server.listen(8)
    output = (LOGS / 'debug.log').open('a', buffering=1)

    def client(connection, address):
        with connection:
            while True:
                data = connection.recv(4096)
                if not data:
                    break
                output.write(f'{time.strftime("%Y-%m-%d %H:%M:%S")} [{address[1]}] {data.decode("latin1").rstrip()}\n')

    def accept():
        while True:
            connection, address = server.accept()
            threading.Thread(target=client, args=(connection, address), daemon=True).start()
    threading.Thread(target=accept, daemon=True).start()


def supervisor(timeout):
    lock = (SUPPORT / 'server.lock').open('a')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise Failure('Another server supervisor is running') from None
    secret = password()
    if not secret:
        raise Failure('The database password is not in the Keychain.')
    stopping = False

    def request_stop(signum, frame):
        nonlocal stopping
        stopping = True
    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)
    state = {'supervisor_pid': os.getpid(), 'phase': 'starting', 'started': time.time(), 'roles': {}}
    children = []
    save_state(state)
    scm = SUPPORT / 'scm'
    ports = dict(ROLES)

    def note(text):
        print(time.strftime('%Y-%m-%d %H:%M:%S ') + text, flush=True)

    def launch(role, mode):
        if mode == 'w' and (LOGS / (role + '.log')).exists():
            (LOGS / (role + '.log')).replace(LOGS / (role + '.previous.log'))
        with (LOGS / (role + '.log')).open(mode) as log:
            offset = log.tell()
            child = subprocess.Popen([str(WORK / role)], cwd=WORK, env=environment(role, secret),
                                     stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
        state['roles'][role] = {'pid': child.pid, 'port': ports[role], 'ready': False, 'exit_code': None}
        save_state(state)
        return child, offset

    def ready(role, child, offset):
        # The completion marker comes after OnEnter and the database start; a
        # listening socket alone is not enough.
        with (LOGS / (role + '.log')).open('rb') as log:
            log.seek(offset)
            text = log.read().decode(errors='replace')
        return f'{role}: {READY}' in text and owns_listener(child.pid, ports[role])
    try:
        shutil.rmtree(scm, ignore_errors=True)
        scm.mkdir(mode=0o700)
        debug_receiver()
        for role, port in ROLES:
            if stopping:
                raise Failure('Startup interrupted')
            child, offset = launch(role, 'w')
            children.append((role, child))
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline and not stopping:
                for name, process in children:
                    if process.poll() is not None:
                        raise Failure(name + ' stopped during startup; see ' + name + '.log')
                if ready(role, child, offset):
                    state['roles'][role]['ready'] = True
                    save_state(state)
                    note(role + ' ready')
                    break
                time.sleep(.2)
            else:
                raise Failure(role + ' did not finish starting in time')
        state['phase'] = 'running'
        save_state(state)
        note('All game servers are running')
        # Happy Tool can stop and start the other roles: ControlService sends
        # SIGTERM, StartService leaves scm/<role>.start. A role that ends with
        # exit code 0 stays stopped until such a request arrives. Any other exit,
        # and any exit of Control, stops the server.
        starting = {}
        while not stopping:
            for index, (role, child) in enumerate(children):
                row = state['roles'][role]
                request = scm / (role + '.start')
                if child.poll() is None:
                    try:
                        if time.time() - request.stat().st_mtime > 60:
                            request.unlink()
                    except FileNotFoundError:
                        pass
                    if role in starting:
                        offset, deadline = starting[role]
                        if ready(role, child, offset):
                            del starting[role]
                            row['ready'] = True
                            save_state(state)
                        elif time.monotonic() > deadline:
                            raise Failure(role + ' did not finish restarting in time')
                    continue
                if row['exit_code'] is None:
                    row.update(exit_code=child.returncode, ready=False)
                    save_state(state)
                    starting.pop(role, None)
                    if role == ROLES[0][0] or child.returncode != 0:
                        raise Failure(f'{role} stopped with code {child.returncode}')
                if request.exists():
                    request.unlink()
                    child, offset = launch(role, 'a')
                    children[index] = (role, child)
                    starting[role] = (offset, time.monotonic() + timeout)
            time.sleep(.2)
    except Exception as error:
        state['error'] = str(error) if isinstance(error, Failure) else type(error).__name__
        note('Problem: ' + state['error'])
    finally:
        state['phase'] = 'stopping'
        save_state(state)
        note('Stopping the game servers')
        # Reverse order: Map saves and updates before World disappears.
        for role, child in reversed(children):
            if child.poll() is None:
                child.send_signal(signal.SIGTERM)
            try:
                code = child.wait(timeout=30)
            except subprocess.TimeoutExpired:
                # No SIGKILL: saving may still be in progress.
                state['roles'][role]['stop_timeout'] = True
                save_state(state)
                code = child.wait()
            if state['roles'][role]['exit_code'] is None:
                state['roles'][role]['exit_code'] = code
            state['roles'][role]['ready'] = False
            save_state(state)
        state['phase'] = 'failed' if 'error' in state else 'stopped'
        save_state(state)
        note('Game servers stopped (' + state['phase'] + ')')


def programs_running():
    """PIDs of server programs started from this Mac's running copy."""
    listing = subprocess.run(['ps', '-axo', 'pid=,command='], capture_output=True, text=True).stdout
    paths = {str(WORK / role) for role, _ in ROLES}
    return [line.split(None, 1)[0] for line in listing.splitlines()
            if len(line.split(None, 1)) == 2 and line.split(None, 1)[1].split(' ')[0] in paths]


def assign_gm():
    """The first character becomes a GM; TMapSvr reads TOPERATORTABLE at startup."""
    rows = sql("""IF NOT EXISTS(SELECT 1 FROM TOPERATORTABLE)
BEGIN
  INSERT TOPERATORTABLE(dwOperatorID) SELECT TOP 1 dwCharID FROM TCHARTABLE WHERE bDelete=0 ORDER BY dwCharID;
  SELECT c.szNAME FROM TOPERATORTABLE o JOIN TCHARTABLE c ON c.dwCharID=o.dwOperatorID;
END""", 'TGAME_GSP')
    if rows:
        say('GM: the first character, ' + rows[0][0] + ', is now a game master.')


def start(args):
    if state_running():
        say('The server is already running.')
        return
    if programs_running():
        raise Failure('Server programs from an earlier start are still running. Restart the Mac.')
    for role, _ in ROLES:
        if not os.access(PACKAGE / 'bin' / role, os.X_OK):
            raise Failure('Missing server program bin/' + role)
    for role, port in ROLES + [('debug log', DEBUG_PORT)]:
        if port_open(port):
            raise Failure(f'Port {port} is already in use by another program (needed by {role}).')
    start_database()
    assign_gm()
    config_files()
    environment(ROLES[0][0], 'check')
    with (LOGS / 'supervisor.log').open('w') as log:
        child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '_supervise',
                                  '--timeout', str(args.timeout)], cwd=SUPPORT, stdin=subprocess.DEVNULL,
                                 stdout=log, stderr=log, start_new_session=True)
    say('Starting the game servers ...')
    shown = set()
    deadline = time.monotonic() + args.timeout * len(ROLES) + 10
    while time.monotonic() < deadline:
        state = read_state()
        if state.get('supervisor_pid') == child.pid:
            for role, row in state.get('roles', {}).items():
                if row.get('ready') and role not in shown:
                    shown.add(role)
                    say('  ' + role + ' ready')
            if state.get('phase') == 'running':
                say('The server is running. Players connect to ' + load_config().get('public_address', '?') + '.')
                return
            if state.get('phase') in ('failed', 'stopped'):
                raise Failure((state.get('error') or 'The server stopped during startup') +
                              '. Open Logs.command for details.')
        if child.poll() is not None:
            raise Failure('The server supervisor stopped. Open Logs.command and read supervisor.log.')
        time.sleep(.25)
    child.send_signal(signal.SIGTERM)
    raise Failure('Startup took too long; the server is stopping. Open Logs.command for details.')


def stop(args):
    state = read_state()
    if owned_supervisor(state):
        say('Stopping the game servers (players are disconnected; this can take a minute) ...')
        os.kill(state['supervisor_pid'], signal.SIGTERM)
        deadline = time.monotonic() + 130
        while time.monotonic() < deadline:
            if read_state().get('phase') in ('stopped', 'failed'):
                break
            time.sleep(.3)
        else:
            raise Failure('The servers are still saving after 130 seconds. Wait, then run Stop.command again.')
        say('Game servers stopped.')
    elif state.get('phase') in ('starting', 'running', 'stopping') and programs_running():
        raise Failure('Server programs run without their supervisor. Restart the Mac to stop them safely.')
    elif state.get('phase') in ('starting', 'running', 'stopping'):
        state.update(phase='failed', error=state.get('error') or 'The supervisor ended unexpectedly')
        save_state(state)
        say('The game servers are not running (the last start ended unexpectedly; see supervisor.log).')
    else:
        say('The game servers are not running.')
    if not args.keep_database and Path('/opt/homebrew/bin/colima').exists() and colima_running():
        say('Stopping the database ...')
        if container_state():
            docker('stop', '-t', '60', CONTAINER, quiet=True)
        run(['/opt/homebrew/bin/colima', 'stop', '-p', PROFILE], quiet=True)
        say('Database stopped.')


def status(args):
    state = read_state()
    running = owned_supervisor(state)
    say('Game servers: ' + (state.get('phase', 'not started') if running else 'stopped'))
    for role, row in state.get('roles', {}).items():
        if running:
            say(f"  {role}: {'ready' if row.get('ready') else 'not ready'}")
    if state.get('error') and not running:
        say('Last problem: ' + state['error'])
    database = 'running' if Path('/opt/homebrew/bin/colima').exists() and colima_running() and container_state() else 'stopped'
    say('Database: ' + database)
    say('Player address: ' + load_config().get('public_address', 'not set (run Install.command)'))


def create_account(args):
    # Login compares TACCOUNT_PW.szPasswd with the SHA-1 hex the client sends
    # (CTClientGame::GetSHA1String); bAgreement=1 skips the first-login agreement.
    name = args.name or ask('Account name (4 to 16 letters or digits)')
    if not re.fullmatch(r'[A-Za-z0-9]{4,16}', name):
        raise Failure('The account name must have 4 to 16 letters or digits.')
    secret = os.environ.get('FOURSTORY_ACCOUNT_PASSWORD')
    if secret is None:
        secret = getpass.getpass('Password: ')
        if getpass.getpass('Password again: ') != secret:
            raise Failure('The passwords are different.')
    if not 4 <= len(secret) <= 16:
        raise Failure('The password must have 4 to 16 characters.')
    if not (installed() and colima_running() and container_state()):
        start_database()
    digest = hashlib.sha1(secret.encode()).hexdigest()
    rows = sql(f"""SET XACT_ABORT ON; BEGIN TRANSACTION;
DECLARE @existing int = (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID={literal(name)});
IF @existing IS NULL
  INSERT TACCOUNT_PW(szUserID,szPasswd,bCheck,dFirstLogin,dLastLogin) VALUES({literal(name)},'{digest}',0,GETDATE(),GETDATE());
ELSE
  UPDATE TACCOUNT_PW SET szPasswd='{digest}' WHERE szUserID={literal(name)};
DECLARE @id int = (SELECT dwUserID FROM TACCOUNT_PW WHERE szUserID={literal(name)});
IF EXISTS(SELECT 1 FROM TUSERINFOTABLE WHERE dwUserID=@id) UPDATE TUSERINFOTABLE SET bAgreement=1 WHERE dwUserID=@id;
ELSE INSERT TUSERINFOTABLE(dwUserID,bCanCreateCharCount,bAgreement) VALUES(@id,6,1);
COMMIT;
SELECT CASE WHEN @existing IS NULL THEN 'created' ELSE 'password changed' END;""", 'TGLOBAL_GSP')
    say(f'Account {name}: {rows[-1][0]}.')


def logs(args):
    LOGS.mkdir(parents=True, exist_ok=True)
    if Path('/opt/homebrew/bin/colima').exists() and colima_running() and container_state() is not None:
        result = docker('logs', '--tail', '2000', CONTAINER, check=False, quiet=True)
        write_private(LOGS / 'sql-server.log', result.stdout + result.stderr)
    say('Logs: ' + str(LOGS))
    if not args.no_open:
        run(['/usr/bin/open', str(LOGS)], check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('install', help='Install or update the server on this Mac.')
    p.add_argument('--yes', action='store_true',
                   help='Answer yes to every question: licenses, Homebrew and Rosetta (you have read the terms).')
    p.add_argument('--address', help='Address players use.')
    p.add_argument('--manager', help='Happy Tool login name (password from FOURSTORY_MANAGER_PASSWORD or a prompt).')
    p = sub.add_parser('start')
    p.add_argument('--timeout', type=float, default=180, help='Seconds per server program')
    p = sub.add_parser('stop')
    p.add_argument('--keep-database', action='store_true', help='Leave SQL Server running.')
    sub.add_parser('status')
    p = sub.add_parser('create-account')
    p.add_argument('name', nargs='?')
    p = sub.add_parser('logs')
    p.add_argument('--no-open', action='store_true')
    p = sub.add_parser('_supervise')
    p.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    commands = {'install': install, 'start': start, 'stop': stop, 'status': status,
                'create-account': create_account, 'logs': logs}
    if args.command == '_supervise':
        supervisor(args.timeout)
    else:
        commands[args.command](args)


if __name__ == '__main__':
    try:
        main()
    except Failure as error:
        print('\n' + str(error), file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print('\nStopped.', file=sys.stderr)
        sys.exit(130)
