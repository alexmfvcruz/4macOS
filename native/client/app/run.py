#!/usr/bin/env python3
"""Run the native client (C4) with the C3 runtime environment.

  python3 native/client/app/run.py [--plain] [--seconds N] [--game DIR] [-- client arguments]

Environment as native/client/c3/check.py: MoltenVK first in DYLD_LIBRARY_PATH,
the MoltenVK ICD, DXVK_WSI_DRIVER=SDL3, an explicit empty DXVK configuration
and the shader cache in the run directory. The game directory defaults to
game (an APFS clone of "game", so the tracked
data is not written). Client arguments default to the Mac route-selected non-loopback IPv4 address,
port 4815 and channel 1 (the local native server). The run directory
runtime/client/runs/<time>/ holds the log (stdout and stderr) and the exit status.
With --seconds the process is stopped after N seconds (SIGTERM); the exit
status then records the stop, not a client result.
"""
import argparse, os, signal, socket, subprocess, sys, time
from pathlib import Path
from permission_bundle import select_binary

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import deps

def ppm_to_png(path):
    import struct, zlib
    data = path.read_bytes()
    parts = data.split(b'\n', 3)
    width, height = map(int, parts[1].split())
    pixels = parts[3]
    raw = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    chunk = lambda kind, body: struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
    path.with_suffix('.png').write_bytes(png)

def local_server_address():
    """Find this Mac's routed IPv4 without sending a packet or changing settings."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.connect(('1.1.1.1', 53))
            address = probe.getsockname()[0]
        if address.startswith('127.') or address == '0.0.0.0':
            raise OSError('no non-loopback address')
        return address
    except OSError as error:
        raise SystemExit('Cannot choose a local server address. Pass -- SERVER_IP 4815 1. ' + str(error))


def main():
    p = argparse.ArgumentParser(description='Start the native macOS game.')
    p.add_argument('--plain', action='store_true', help='Play without the extra checks used to find memory errors (default: checks on).')
    p.add_argument('--seconds', type=float, default=0, help='Stop the game after this many seconds (default: keep running).')
    p.add_argument('--game', default=str(ROOT / 'game'), metavar='FOLDER', help='Choose the game files folder (default: game).')
    p.add_argument('--capture', type=int, default=0, metavar='MS', help='Save a game image every MS milliseconds; 1000 means once a second.')
    p.add_argument('--screen', metavar='WxH', help='Set the game resolution, for example 1280x800.')
    p.add_argument('--fullscreen', action='store_true', help='Start the game in fullscreen (same as the green window button).')
    p.add_argument('--input', metavar='FILE', help='Run a test file that controls the game automatically.')
    p.add_argument('args', nargs='*', help='Optional server address, port and channel.')
    a = p.parse_args()
    output = Path(os.environ.get('FOURSTORY_CLIENT_OUT', ROOT / 'build/client' / ('c4-plain' if a.plain else 'c4-sanitized'))).resolve()
    binary = output / '4story-client'
    try: binary = select_binary(binary)
    except RuntimeError as error: p.error(str(error))
    if not binary.is_file(): p.error('Game executable not found: %s. Build native/client/app/build.py first.' % binary)
    if a.seconds < 0: p.error('--seconds must be zero or positive')
    run = ROOT / 'runtime/client/runs' / time.strftime('%Y%m%d-%H%M%S')
    (run / 'cache').mkdir(parents=True, exist_ok=True)
    (run / 'dxvk.conf').write_text('# C4 uses the pinned backend defaults (as C3).\n')
    env = os.environ.copy()
    # This checkout's optional registration service. Existing explicit endpoint
    # configuration wins; local CA trust is private to this process.
    account_record=ROOT/'runtime/accounts/service.json'
    if account_record.is_file() and 'FOURSTORY_ACCOUNTS_URL' not in env:
        import json
        info=json.loads(account_record.read_text())
        env['FOURSTORY_ACCOUNTS_URL']='https://127.0.0.1:'+str(int(info['port']))+'/v1/accounts'
        env['FOURSTORY_ACCOUNTS_CA']=str(ROOT/'runtime/accounts/tls/ca.pem')
    env['VK_DRIVER_FILES'] = str(deps.MOLTENVK_ICD)
    env['DYLD_LIBRARY_PATH'] = os.pathsep.join(map(str, deps.RUNTIME_LIBRARY_DIRS))
    env['DXVK_WSI_DRIVER'] = 'SDL3'
    if a.fullscreen: env['FOURSTORY_START_FULLSCREEN'] = '1'
    env['DXVK_STATE_CACHE_PATH'] = str(run / 'cache')
    env['DXVK_CONFIG_FILE'] = str(run / 'dxvk.conf')
    env.pop('DXVK_CONFIG', None)
    env.setdefault('ASAN_OPTIONS', 'detect_leaks=0:abort_on_error=0')
    # Settings (the Windows registry store) stay in the repository's .local tree.
    env.setdefault('FOURSTORY_SETTINGS_DIR', str(ROOT / 'runtime/client/settings'))
    store = Path(env['FOURSTORY_SETTINGS_DIR']) / '4Story/4Story/settings.reg'
    if a.screen:
        import re
        w, h = (int(v) for v in a.screen.lower().split('x'))
        text = store.read_text(encoding='latin-1')
        for key, value in (('ScreenX', w), ('ScreenY', h)):
            text, n = re.subn(r'"%s"=dword:[0-9a-f]{8}' % key, '"%s"=dword:%08x' % (key, value), text)
            if n != 1: raise SystemExit('settings store has no single %s entry: %s' % (key, store))
        store.write_text(text, encoding='latin-1')
    if a.capture:
        (run / 'captures').mkdir()
        env['FOURSTORY_C4_CAPTURE_DIR'] = str(run / 'captures')
        env['FOURSTORY_C4_CAPTURE_EVERY_MS'] = str(a.capture)
    if a.input: env['FOURSTORY_C4_INPUT_SCRIPT'] = str(Path(a.input).resolve())
    args = a.args or [local_server_address(), '4815', '1']
    if args[0] in ('127.0.0.1', 'localhost', '::1'):
        print('Warning: loopback login can be classified as a server connection by the local native server. Use the Mac LAN address.', file=sys.stderr)
    print('run directory', run, flush=True)
    with open(run / 'log.txt', 'wb') as log:
        proc = subprocess.Popen([str(binary), '--data', a.game, *args], env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            code = proc.wait(timeout=a.seconds or None)
            status = f'exit {code}'
            result = code if code >= 0 else 128 - code
        except subprocess.TimeoutExpired:
            proc.send_signal(signal.SIGTERM)
            forced = False
            try: code = proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                forced = True
                code = proc.wait()
            expected_signal = -signal.SIGKILL if forced else -signal.SIGTERM
            if code == expected_signal:
                status = f'stopped after {a.seconds:g} s ({"SIGKILL" if forced else "SIGTERM"})'
                result = 0  # Only the requested termination counts as a normal stop.
            else:
                status = f'exit {code} during requested stop after {a.seconds:g} s'
                result = code if code >= 0 else 128 - code
    (run / 'status.txt').write_text(status + '\n')

    if a.capture:
        for ppm in sorted((run / 'captures').glob('*.ppm')): ppm_to_png(ppm)
        print('captures', len(list((run / 'captures').glob('*.ppm'))))
    print(status)
    print((run / 'log.txt').read_text(errors='replace')[-4000:])

    return result

if __name__ == '__main__':
    sys.exit(main())
