#!/usr/bin/env python3
"""C4-P native build: window/GDI/GDI+ compatibility library, original UI units, shell and tests.

Targets:
  compat-ui  libcompat_ui.a (native/client/compat/src) and its unit tests
  text       c4-text-dump (native/client/reference/c4-text-dump.cpp)
  ui         original TComp, TachyonApp, TachyonWnd, D3DFont units (compile only)
  shell      SDL3 backend and the shell test (needs the approved local SDL3)
  tclient    syntax-only measurement of every TClient unit (report, no failure)
  all        compat-ui, text, ui, shell
Output: build/client/c4p-<sanitized|plain>/
"""
import argparse, os, subprocess, sys, json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import deps
SRC = ROOT / 'source/4Story 5.0 Source Files'
LIB = SRC / 'Lib/Own'
p = argparse.ArgumentParser()
p.add_argument('--target', choices=['compat-ui', 'text', 'ui', 'shell', 'tclient', 'all'], default='all')
p.add_argument('--no-sanitize', action='store_true')
a = p.parse_args()
OUT = ROOT / 'build/client' / ('c4p-plain' if a.no_sanitize else 'c4p-sanitized')
OUT.mkdir(parents=True, exist_ok=True)
CXX = os.environ.get('CXX', 'clang++')
SDL_PREFIX = Path(os.environ.get('FOURSTORY_SDL3_PREFIX', deps.SDL3_PREFIX))

base = ['-std=gnu++14', '-arch', 'arm64', '-ffp-contract=off', '-g', '-O1', '-fno-omit-frame-pointer', '-fno-delete-null-pointer-checks',
        '-DFOURSTORY_CLIENT_NATIVE', '-DFOURSTORY_CLIENT_64', '-DFOURSTORY_CLIENT_MOD',
        '-include', 'fourstory_client_prefix.h', '-Wno-nonportable-include-path', '-Wno-ignored-pragmas']
if not a.no_sanitize:
    base += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
# Contract item 11: the shared d3d9.h comes before the compatibility headers.
includes = [ROOT/'native/client/d3d9/include', ROOT/'native/client/compat/include', ROOT/'native/client/d3dx/include',
            ROOT/'native/client/shell/include', ROOT/'native/client/build/include',
            LIB/'TProtocol/include', LIB/'TCML/TCML', LIB/'TChart/TChart', LIB/'Engine Lib/Engine Lib', LIB/'TComp/TComp']
inc = []
for d in includes: inc += ['-I', str(d)]
frameworks = ['-liconv', '-framework', 'CoreText', '-framework', 'CoreGraphics', '-framework', 'CoreFoundation']

def run(command, label):
    print(label, flush=True)
    r = subprocess.run(command)
    if r.returncode: print('FAIL', label, flush=True); sys.exit(r.returncode)

def compile_all(sources, prefix, flags):
    objects = []
    for s in sources:
        obj = OUT / (prefix + '-' + Path(s).name + '.o')
        run([CXX, *flags, '-c', str(s), '-o', str(obj)], 'Compile ' + prefix + '/' + Path(s).name)
        objects.append(str(obj))
    return objects

def compat_ui():
    srcs = sorted((ROOT/'native/client/compat/src').glob('*.cpp'))
    objs = compile_all(srcs, 'compat-ui', [*base, '-Wall', '-Wextra', '-Werror', *inc])
    run(['ar', 'rcs', str(OUT/'libcompat_ui.a'), *objs], 'Archive libcompat_ui.a')
    tests = ROOT/'native/client/compat/tests/ui_test.cpp'
    if tests.exists():
        run([CXX, *base, '-Wall', '-Wextra', *inc, str(tests), str(OUT/'libcompat_ui.a'), *frameworks, '-o', str(OUT/'c4-compat-ui-test')], 'Link c4-compat-ui-test')

def text():
    run([CXX, *base, *inc, '-Wno-address-of-temporary', str(ROOT/'native/client/reference/c4-text-dump.cpp'), str(OUT/'libcompat_ui.a'),
         *frameworks, '-o', str(OUT/'c4-text-dump')], 'Link c4-text-dump')

if a.target in ('compat-ui', 'all'): compat_ui()
if a.target in ('text', 'all'): text()
# Original interface units (C4-P criterion 2). Not built natively: THtmlDocWnd.cpp,
# THttpCtrl.cpp and TOccManager.cpp (Internet Explorer control; native stand-in
# shell/src/http_ctrl_native.cpp) and TachyonMedia.cpp (DirectSound and DirectShow;
# stand-in shell/src/media_native.cpp). The shared d3d9.h comes first (contract 11).
TCOMP = ['GDIFont', 'TButton', 'TCombo', 'TComp', 'TComponent', 'TEdit', 'TFrame', 'TGauge', 'TImageList', 'TList',
         'TListItem', 'TMeter', 'TMultiLineEdit', 'TScroll', 'TTabCtrl', 'TToolTip']
ENGINE_UI = ['TachyonApp', 'TachyonWnd', 'D3DFont']
# MSVC accepts narrowing in brace initializers (warning C4838) and the address of a temporary.
uiflags = [*base, *inc, '-Wno-c++11-narrowing', '-Wno-address-of-temporary', '-ferror-limit=0', '-w']

def ui():
    sources = [LIB/'TComp/TComp'/(n + '.cpp') for n in TCOMP] + [LIB/'Engine Lib/Engine Lib'/(n + '.cpp') for n in ENGINE_UI]
    sources += sorted((ROOT/'native/client/shell/src').glob('*_native.cpp'))
    objects, failed = [], []
    for s in sources:
        obj = OUT / ('ui-' + Path(s).name + '.o')
        print('Compile ui/' + Path(s).name, flush=True)
        if subprocess.run([CXX, *uiflags, '-c', str(s), '-o', str(obj)]).returncode: failed.append(Path(s).name)
        else: objects.append(str(obj))
    if failed: print('FAIL ui:', ' '.join(failed), flush=True); sys.exit(1)
    run(['ar', 'rcs', str(OUT/'libui.a'), *objects], 'Archive libui.a')

if a.target in ('ui', 'all'): ui()

# SDL3 backend and the shell test (C4-P criterion 3). SDL3 is the approved local
# build of C3 (research/c3-plan.md); nothing is installed.
def shell():
    sdl_inc, sdl_lib = SDL_PREFIX/'include', SDL_PREFIX/'lib'
    if not (sdl_inc/'SDL3/SDL.h').exists(): print('FAIL shell: SDL3 not found under', SDL_PREFIX, flush=True); sys.exit(1)
    flags = [*base, '-Wall', '-Wextra', '-Werror', *inc, '-I', str(sdl_inc)]
    objs = compile_all([ROOT/'native/client/shell/src/sdl_backend.cpp'], 'shell', flags)
    run(['ar', 'rcs', str(OUT/'libshell.a'), *objs], 'Archive libshell.a')
    run([CXX, *flags, str(ROOT/'native/client/shell/tests/shell_test.cpp'), str(OUT/'libshell.a'), str(OUT/'libcompat_ui.a'),
         *frameworks, '-L', str(sdl_lib), '-lSDL3', '-Wl,-rpath,' + str(sdl_lib), '-o', str(OUT/'c4-shell-test')], 'Link c4-shell-test')

if a.target in ('shell', 'all'): shell()

# C4-P criterion 5: syntax-only compile of every TClient unit of TClient.vcxproj
# with the Release x64 definitions (NEW_IF, USE_HWID_AUTH). A measurement: the
# target reports counts and writes tclient-measure.json; it does not fail.
def tclient():
    import re, json, collections, concurrent.futures
    client = SRC/'Client/TClient'
    units = sorted(set(re.findall(r'<ClCompile Include="([^"]+)"', (client/'TClient.vcxproj').read_text(encoding='latin-1'))))
    flags = [f for f in uiflags if not f.startswith('-fsanitize') and f != '-fno-sanitize-recover=all']
    flags += ['-fsyntax-only', '-DNDEBUG', '-DNEW_IF', '-DUSE_HWID_AUTH', '-iquote', str(client),
              '-I', str(LIB/'HwidLib/HwidLib'), '-I', str(SRC/'Client/History Files')]
    def one(u):
        r = subprocess.run([CXX, *flags, str(client/u.replace('\\', '/'))], capture_output=True, text=True, errors='replace')
        return u, [l.replace(str(SRC) + '/', '') for l in r.stderr.splitlines() if ' error: ' in l]
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        results = dict(ex.map(one, units))
    (OUT/'tclient-measure.json').write_text(json.dumps(results, indent=1))
    messages = collections.Counter(re.sub(r'^.*? error: ', '', e) for errs in results.values() for e in errs)
    clean = sum(1 for e in results.values() if not e)
    print(f'TClient units {len(results)}: {clean} without error, {len(results) - clean} with errors, {sum(map(len, results.values()))} errors', flush=True)
    for m, c in messages.most_common(25): print(f'  {c:4d} {m[:160]}')

if a.target == 'tclient': tclient()
print('PASS c4p build:', a.target, OUT, flush=True)
