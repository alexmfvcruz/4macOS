"""Collect crash evidence from the native servers and the Windows client.

    python3 tools/crash_report.py [--minutes N] [--no-client]

Writes runtime/legacy/crash-report.txt and prints it. Read-only: it changes nothing on
the Mac or in the VM. Server side: cluster state, crash markers and stacks the
modules write to their logs (native/server/win32/module_main.cpp), the newest
macOS crash report per role, and the last client packets from the packet trace
(FOURSTORY_PACKET_TRACE=1). Client side, through Parallels: whether TClient is
running, Windows Application-log errors for TClient, and the minidumps the
client writes next to its executable.
"""
import argparse
import base64
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUN = ROOT / 'runtime/server-dev'
ROLES = ['TControlSvr', 'TLoginSvr', 'TWorldSvr', 'TMapSvr']
REPORTS = Path.home() / 'Library/Logs/DiagnosticReports'
VM = 'Windows 11'
GAME = r'C:\4StoryAudit\Client\game'
NOISE = ('from 127.0.0.1', 'Monster have no ATTR data')


def packet_names():
    sys.path.insert(0, str(ROOT / 'native/server/tests'))
    try:
        import game_client
        return game_client.NAMES
    except Exception:
        return {}


def name_packets(line, names):
    return re.sub(r'recv 0x([0-9A-Fa-f]{4})', lambda m: f'recv {names.get(int(m.group(1), 16), "0x" + m.group(1))}', line)


def server_section(minutes, out):
    names = packet_names()
    state = {}
    try:
        state = json.loads((RUN / 'state.json').read_text())
    except Exception as error:
        out.append(f'cluster state unavailable: {error}')
    out.append(f"cluster phase: {state.get('phase')}  error: {state.get('error')}")
    for role in ROLES:
        info = state.get('roles', {}).get(role, {})
        code = info.get('exit_code')
        meaning = '' if code in (None, 0) else f' (killed by signal {-code})' if code < 0 else ' (failed)'
        out.append(f'\n=== {role}: exit code {code}{meaning}')
        log = RUN / f'{role}.log'
        if not log.is_file():
            out.append('  no log')
            continue
        lines = log.read_text(errors='replace').splitlines()
        crash = [i for i, line in enumerate(lines) if 'Native crash:' in line and 'end of stack' not in line
                 or 'terminating due to' in line or 'AddressSanitizer' in line or 'runtime error:' in line]
        if crash:
            out.append('  crash output:')
            out.extend('    ' + line[:260] for line in lines[crash[-1]:crash[-1] + 45])
        quiet = [line for line in lines if not any(text in line for text in NOISE)]
        out.append('  last log lines:')
        out.extend('    ' + name_packets(line, names)[:220] for line in quiet[-14:])
        newest = sorted(REPORTS.glob(f'{role}-*.ips'), key=lambda p: p.stat().st_mtime)[-1:] if REPORTS.is_dir() else []
        for report in newest:
            age = (datetime.datetime.now().timestamp() - report.stat().st_mtime) / 60
            if age > minutes:
                continue
            out.append(f'  macOS crash report {report.name} ({age:.0f} min ago):')
            out.extend('    ' + line for line in read_ips(report))


def read_ips(path):
    """Exception and crashed-thread frames from a macOS .ips report."""
    try:
        header, body = path.read_text(errors='replace').split('\n', 1)
        data = json.loads(body)
    except Exception as error:
        return [f'unreadable: {error}']
    lines = [f"exception: {data.get('exception', {}).get('type')} {data.get('exception', {}).get('signal', '')} "
             f"{data.get('exception', {}).get('subtype', '')}".strip()]
    if data.get('asi'):
        lines.append('info: ' + json.dumps(data['asi'])[:300])
    images = data.get('usedImages', [])
    for thread in data.get('threads', []):
        if not thread.get('triggered'):
            continue
        for frame in thread.get('frames', [])[:28]:
            image = images[frame['imageIndex']].get('name', '?') if frame.get('imageIndex', -1) < len(images) else '?'
            where = frame.get('symbol', hex(frame.get('imageOffset', 0)))
            source = f" ({Path(frame['sourceFile']).name}:{frame.get('sourceLine')})" if frame.get('sourceFile') else ''
            lines.append(f'{image}  {where}{source}')
    return lines


def powershell(script):
    encoded = base64.b64encode(("$ProgressPreference='SilentlyContinue';" + script + ";'REPORT-DONE'").encode('utf-16le')).decode()
    for _ in range(8):
        result = subprocess.run(['prlctl', 'exec', VM, '--current-user', 'powershell', '-NoProfile', '-EncodedCommand', encoded],
                                capture_output=True, text=True, stdin=subprocess.DEVNULL, timeout=120)
        if 'REPORT-DONE' in result.stdout:
            return [line for line in result.stdout.replace('\r', '').splitlines() if line != 'REPORT-DONE']
    return ['could not reach the VM: ' + (result.stderr or result.stdout).strip()[:200]]


def client_section(minutes, out):
    out.append('\n=== Windows client (TClient.exe)')
    script = rf'''
$p = Get-Process TClient -EA 0
if ($p) {{ $p | % {{ "running: pid=" + $_.Id + " responding=" + $_.Responding + " memMB=" + [math]::Round($_.WorkingSet64/1MB) + " started=" + $_.StartTime.ToString('HH:mm:ss') }} }} else {{ "running: no" }}
"build: " + (Get-FileHash '{GAME}\TClient.exe' -Algorithm SHA256).Hash.Substring(0,16) + " " + (Get-Item '{GAME}\TClient.exe').LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')
$events = Get-WinEvent -FilterHashtable @{{LogName='Application'; StartTime=(Get-Date).AddMinutes(-{minutes})}} -EA 0 | ? {{ $_.Message -match 'TClient' -and $_.LevelDisplayName -in 'Error','Critical','Warning','Information' -and $_.ProviderName -match 'Application Error|Windows Error Reporting|Application Hang' }} | select -First 4
if (-not $events) {{ "windows error events: none in the last {minutes} minutes" }}
foreach ($e in $events) {{ "event " + $e.TimeCreated.ToString('HH:mm:ss') + " " + $e.ProviderName + ": " + (($e.Message -replace '\s+',' ').Substring(0,[Math]::Min(700,$e.Message.Length))) }}
Get-ChildItem '{GAME}\*.dmp' -EA 0 | sort LastWriteTime -Descending | select -First 3 | % {{ "minidump: " + $_.Name + " " + $_.Length + " bytes " + $_.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss') }}
'''
    out.extend('  ' + line[:760] for line in powershell(script))
    # Bring the newest recent minidump and the symbol map of the installed
    # build to runtime/legacy/crash on the Mac (the VM writes there; nothing else is changed).
    crash = ROOT / 'runtime/legacy/crash'
    crash.mkdir(exist_ok=True)
    shared = '\\\\Mac\\Home' + str(crash)[len(str(Path.home())):].replace('/', '\\')
    fetched = powershell(rf'''
$h = (Get-FileHash '{GAME}\TClient.exe' -Algorithm SHA256).Hash.Substring(0,16)
"hash=$h"
$m = "C:\4StoryAudit\Client\out\TClient-$h.map"
if (Test-Path $m) {{ Copy-Item $m '{shared}\' -Force }}
Get-ChildItem '{GAME}\*.dmp' -EA 0 | ? {{ $_.LastWriteTime -gt (Get-Date).AddMinutes(-{minutes}) }} | sort LastWriteTime -Descending | select -First 1 | % {{ Copy-Item $_.FullName '{shared}\' -Force; "dump=" + $_.Name }}
''')
    values = dict(line.split('=', 1) for line in fetched if '=' in line)
    if 'dump' not in values:
        out.append(f'  no client minidump in the last {minutes} minutes')
        return
    map_file = crash / f"TClient-{values.get('hash')}.map"
    symbols = load_map(map_file) if map_file.is_file() else []
    out.append(f"  minidump {values['dump']}" + ('' if symbols else ' (no symbol map for the installed build; addresses only)'))
    dump = crash / values['dump']
    if dump.is_file() and symbols and dump.stat().st_mtime < map_file.stat().st_mtime:
        out.append('  note: this dump is older than the installed build, so function names are approximate')
    out.extend('  ' + line for line in read_minidump(dump, symbols))


def load_map(path):
    """(rva, name) pairs for the functions in an MSVC linker map, sorted by rva."""
    base, symbols, started = 0x400000, [], False
    for line in path.read_text(errors='replace').splitlines():
        match = re.match(r'\s*Preferred load address is ([0-9A-Fa-f]+)', line)
        if match:
            base = int(match.group(1), 16)
        if 'Publics by Value' in line or 'Static symbols' in line:
            started = True
            continue
        parts = line.split()
        if started and len(parts) >= 3 and re.fullmatch(r'[0-9a-f]{4}:[0-9a-f]{8}', parts[0]) and re.fullmatch(r'[0-9a-f]{8}', parts[2]):
            address = int(parts[2], 16)
            # Functions only (flag "f"): data symbols would turn stack values into noise.
            if address >= base and not parts[0].startswith('0000:') and len(parts) >= 4 and parts[3] == 'f':
                symbols.append((address - base, parts[1]))
    return sorted(symbols)


def readable(name):
    """?Method@Class@@... -> Class::Method; other names unchanged."""
    match = re.match(r'\?\??([A-Za-z_]\w*)@([A-Za-z_]\w*)@@', name)
    if match:
        return f'{match.group(2)}::{match.group(1)}'
    match = re.match(r'\?([A-Za-z_]\w*)@@', name)
    return match.group(1) if match else name


def symbol_at(symbols, rva):
    import bisect
    index = bisect.bisect_right(symbols, (rva, '\uffff')) - 1
    if index < 0:
        return f'+0x{rva:X}'
    return f'{readable(symbols[index][1])}+0x{rva - symbols[index][0]:X}'


def read_minidump(path, symbols):
    """Exception, faulting module and a stack scan from a Windows minidump."""
    import struct
    data = path.read_bytes()
    if data[:4] != b'MDMP':
        return ['not a minidump']
    count, directory = struct.unpack_from('<II', data, 8)
    streams = {}
    for i in range(count):
        kind, size, rva = struct.unpack_from('<III', data, directory + 12 * i)
        streams[kind] = (rva, size)
    modules = []
    if 4 in streams:
        rva = streams[4][0]
        for i in range(struct.unpack_from('<I', data, rva)[0]):
            base, size, _, _, name_rva = struct.unpack_from('<QIIII', data, rva + 4 + 108 * i)
            length = struct.unpack_from('<I', data, name_rva)[0]
            name = data[name_rva + 4:name_rva + 4 + length].decode('utf-16le', 'replace').split('\\')[-1]
            modules.append((base, size, name))

    def locate(address):
        for base, size, name in modules:
            if base <= address < base + size:
                where = symbol_at(symbols, address - base) if symbols and name.lower() == 'tclient.exe' else f'+0x{address - base:X}'
                return name, where
        return None, f'0x{address:08X}'

    lines = []
    if 6 not in streams:
        return ['no exception record in dump']
    rva = streams[6][0]
    thread_id, _, code, flags, _, address, parameters = struct.unpack_from('<IIIIQQI', data, rva)
    information = struct.unpack_from('<15Q', data, rva + 40)
    meaning = {0xC0000005: 'access violation', 0xC00000FD: 'stack overflow', 0xC0000094: 'integer divide by zero',
               0xC0000409: 'stack buffer overrun / fast fail', 0xE06D7363: 'C++ exception', 0x80000003: 'breakpoint',
               0xC000001D: 'illegal instruction', 0xC0000374: 'heap corruption'}.get(code, '')
    module, where = locate(address)
    lines.append(f'exception 0x{code:08X} {meaning} at {module or "?"} {where}')
    if code == 0xC0000005 and parameters >= 2:
        lines.append(f'  {"write to" if information[0] == 1 else "execute at" if information[0] == 8 else "read of"} address 0x{information[1]:08X}')
    # Stack scan of the faulting thread: every stack word that points into
    # TClient.exe code is a likely return address (approximate call chain).
    if 3 in streams:
        rva = streams[3][0]
        for i in range(struct.unpack_from('<I', data, rva)[0]):
            tid, _, _, _, _, stack_start, stack_size, stack_rva = struct.unpack_from('<IIIIQQII', data, rva + 4 + 48 * i)
            if tid != thread_id:
                continue
            seen = []
            for offset in range(0, stack_size - 3, 4):
                value = struct.unpack_from('<I', data, stack_rva + offset)[0]
                name, at = locate(value)
                if name and name.lower() == 'tclient.exe' and symbols and not at.startswith('+') \
                        and int(at.rsplit('+0x', 1)[1], 16) < 0x4000:
                    function = at.rsplit('+0x', 1)[0]
                    if not seen or seen[-1] != function:
                        seen.append(function)
                if len(seen) >= 25:
                    break
            if seen:
                lines.append('  likely call chain (innermost first, from a stack scan):')
                lines.extend('    ' + function for function in seen)
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--minutes', type=int, default=60, help='how far back to look (default 60)')
    parser.add_argument('--no-client', action='store_true', help='skip the Windows VM')
    arguments = parser.parse_args()
    out = [f'4Story crash report {datetime.datetime.now():%Y-%m-%d %H:%M:%S} (last {arguments.minutes} minutes)']
    server_section(arguments.minutes, out)
    if not arguments.no_client:
        try:
            client_section(arguments.minutes, out)
        except Exception as error:
            out.append(f'\n=== Windows client: not collected ({error})')
    text = '\n'.join(out) + '\n'
    target = ROOT / 'runtime/legacy/crash-report.txt'
    target.write_text(text)
    print(text)
    print(f'Saved to {target}')


if __name__ == '__main__':
    main()
