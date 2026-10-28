#!/usr/bin/env python3
"""C4 native client build: the original TClient executable for macOS ARM64.

Compiles the original units of TClient.vcxproj and of the libraries it links
(Engine Lib, TComp, TChart, TCML), the compatibility layer, the SDL3 shell and
the native stand-ins, and links them with DXVK Native (C3 option C) and SDL3.

  python3 native/client/app/build.py [--sanitize] [--keep-going] [--compile-only]
                                     [--channel beta|dev|stable] [--version 0.1]

Before compiling, the build asks for the channel and version (build_metadata.py)
unless both are given; without a terminal it refuses missing values.

Output: build/client/c4-plain/4story-client (+ objects); with
--sanitize (ASan/UBSan) build/client/c4-sanitized/4story-client.
Incremental: a unit is recompiled when its source or a header in its
dependency file is newer than the object, or when its compile flags changed. Nothing is downloaded or installed.
"""
import argparse, concurrent.futures, os, re, shlex, subprocess, sys, plistlib, shutil, json
from pathlib import Path
from permission_bundle import STAMP, record_complete
from build_freshness import RECORD, dependencies, record_build, capture_inputs, metadata, inventory, parse_dependencies
import build_metadata

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / 'source/4Story 5.0 Source Files'
LIB = SRC / 'Lib/Own'
ENGINE = LIB / 'Engine Lib/Engine Lib'
CLIENT = SRC / 'Client/TClient'
NATIVE = ROOT / 'native/client'
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import deps
# clang and the linker read the target from the environment (C8: macOS 14).
os.environ.setdefault('MACOSX_DEPLOYMENT_TARGET', deps.MACOS_DEPLOYMENT_TARGET)
SDL_PREFIX = Path(os.environ.get('FOURSTORY_SDL3_PREFIX', deps.SDL3_PREFIX))
DXVK = Path(os.environ.get('FOURSTORY_DXVK_D3D9', deps.DXVK_D3D9))

p = argparse.ArgumentParser()
p.add_argument('--sanitize', action='store_true', help='build c4-sanitized with ASan/UBSan (default: c4-plain)')
# Former flag from when the sanitized build was the default; plain is now the default.
p.add_argument('--no-sanitize', action='store_true', help=argparse.SUPPRESS)
p.add_argument('--keep-going', action='store_true', help='compile every unit and list all failures')
p.add_argument('--compile-only', action='store_true')
p.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
p.add_argument('--channel', choices=build_metadata.CHANNELS, help='build channel (asked when missing)')
p.add_argument('--version', help='version number such as 0.1 (asked when missing)')
a = p.parse_args()
if a.sanitize and a.no_sanitize:
    p.error('--sanitize and --no-sanitize exclude each other')
# Build label metadata, collected before any compilation (Q-C8B1).
try:
    a.channel, a.version = build_metadata.ask(a.channel, a.version)
except (ValueError, EOFError, KeyboardInterrupt) as error:
    p.error(str(error) or 'no channel or version was entered')
BUILD_UTC = build_metadata.utc_now()
print(f'Build label metadata: {a.channel}{a.version} {BUILD_UTC}', flush=True)
a.no_sanitize = not a.sanitize
OUT = Path(os.environ.get('FOURSTORY_CLIENT_OUT', ROOT / 'build/client' / ('c4-plain' if a.no_sanitize else 'c4-sanitized'))).resolve()
OBJ = OUT / 'obj'
OBJ.mkdir(parents=True, exist_ok=True)
# Failed and compile-only builds cannot bless an older executable.
(OUT/STAMP).write_text("{}\n")
(OUT/RECORD).write_text("{}\n")
CXX = os.environ.get('CXX', 'clang++')

# Release builds must not name this Mac's folders (C8 scan): __FILE__ and debug
# paths become relative to the checkout.
PATH_MAP = [f'-ffile-prefix-map={ROOT}=.']
base = ['-std=gnu++14', '-arch', 'arm64', *PATH_MAP, '-ffp-contract=off', '-g', '-O1', '-fno-omit-frame-pointer',
        '-fno-delete-null-pointer-checks', '-DFOURSTORY_CLIENT_NATIVE', '-DFOURSTORY_CLIENT_64', '-DFOURSTORY_CLIENT_MOD',
        '-include', 'fourstory_client_prefix.h', '-Wno-nonportable-include-path', '-Wno-ignored-pragmas', '-MMD']
if not a.no_sanitize:
    base += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
# Contract item 11: the shared d3d9.h comes before the compatibility headers.
includes = [NATIVE/'d3d9/include', NATIVE/'compat/include', NATIVE/'d3dx/include', NATIVE/'shell/include',
            NATIVE/'build/include', NATIVE/'app/include', LIB/'TProtocol/include', LIB/'TCML/TCML', LIB/'TChart/TChart',
            ENGINE, LIB/'TComp/TComp']
inc = []
for d in includes: inc += ['-I', str(d)]
# MSVC accepts narrowing in brace initializers (C4838) and the address of a temporary.
original = [*base, *inc, '-Wno-c++11-narrowing', '-Wno-address-of-temporary', '-ferror-limit=0', '-w']
# Release x64 definitions of TClient.vcxproj:303.
client_flags = [*original, '-DNDEBUG', '-DNEW_IF', '-DUSE_HWID_AUTH', '-iquote', str(CLIENT), '-I', str(LIB/'HwidLib/HwidLib'),
                '-I', str(SRC/'Client/History Files')]
own = [*base, *inc, '-Wall', '-Wextra', '-Werror']

def units(project):
    return sorted(set(m.replace('\\', '/') for m in re.findall(r'<ClCompile Include="([^"]+)"',
                                                               project.read_text(encoding='latin-1'))))

# Units replaced by native code. Each replacement is listed in native/client/app/README.md.
ENGINE_REPLACED = {'D3DSound.cpp', 'T3DMusic.cpp', 'TShow.cpp', 'TachyonMedia.cpp', 'TConsole.cpp',
                   'TModuleProtector.cpp', 'TNICEventSink.cpp', 'StdAfx.cpp'}
TCOMP_REPLACED = {'THtmlDocWnd.cpp', 'THttpCtrl.cpp', 'TOccManager.cpp', 'StdAfx.cpp'}
CLIENT_REPLACED = {'StdAfx.cpp',
                   'TMiniDump.cpp',                    # replaced: app/src/minidump_native.cpp
                   'TSystemInfo.cpp', 'TMailSender.cpp',  # used only by TMiniDump.cpp
                   'PacketSpyDlg.cpp', 'PacketSpyAcks.cpp',  # creation is commented out (TClient.cpp:557)
                   'IPDialog.cpp',                     # no caller
                   'TClientWebCtrl.cpp',               # replaced: app/src/web_ctrl_native.cpp
                   'HwidManager.cpp'}                  # replaced: app/src/hwid_native.cpp (WMI)
CLIENT_REPLACED |= set(os.environ.get('FOURSTORY_C4_SKIP', '').split())

source_folders = [NATIVE/'compat/src', NATIVE/'shell/src', NATIVE/'app/src', NATIVE/'audio']
# Snapshot project/helper inputs before deriving jobs or compiling the helper.
fixed_inputs = [NATIVE/'app/start-client.sh', Path(__file__), NATIVE/'app/build_freshness.py', NATIVE/'app/build_metadata.py', NATIVE/'app/gen_resources.py',
                NATIVE/'app/assets/4story.icns', NATIVE/'mac-keyboard/helper.cpp',
                ENGINE/'Engine Lib.vcxproj', LIB/'TComp/TComp/TComp.vcxproj',
                CLIENT/'TClient.vcxproj', CLIENT/'TClient.rc']
# These files describe dynamically read helper/resource inputs. Capture them
# before enumeration so a concurrent edit cannot change the dependency graph.
fixed_inputs.append(NATIVE/'reference/shaders/manifest.json')
# The linked account code: Rust bridge and core, Objective-C platform files and
# the embedded artwork. None of these has a compiler .d file.
ACCOUNTS = ROOT/'native/accounts'
fixed_inputs += [ACCOUNTS/'build.py', ACCOUNTS/'Cargo.toml', ACCOUNTS/'Cargo.lock']
for part in ('core', 'bridge', 'platform', 'include', 'assets'):
    fixed_inputs += sorted(p for p in (ACCOUNTS/part).rglob('*') if p.is_file())
pre_scan = capture_inputs(fixed_inputs)
helper_deps = parse_dependencies(subprocess.check_output(
    [CXX,'-std=c++17','-arch','arm64','-MM',str(NATIVE/'mac-keyboard/helper.cpp')], text=True))
fixed_inputs.extend(helper_deps)
fixed_inputs.extend(Path(p) for p in json.loads(subprocess.check_output(
    [sys.executable,str(NATIVE/'app/gen_resources.py'),'--list-inputs'],text=True)))
initial_snapshot = capture_inputs(fixed_inputs, source_folders)
initial_snapshot['inputs'].update(pre_scan['inputs'])
jobs = []  # (label, source, flags)
for u in units(ENGINE/'Engine Lib.vcxproj'):
    if u in ENGINE_REPLACED: continue
    # Units with the C1 data/session or C4-P interface header keep it; the C2
    # loader units and the units without a native include get the full engine
    # header set that T3D.h gives them on Windows.
    head = (ENGINE/u).read_text(encoding='latin-1')[:1500]
    jobs.append(('engine', ENGINE/u, original if ('<fourstory_engine_data' in head or '<fourstory_engine_session' in head or '<fourstory_engine_ui' in head)
                 else [*original, '-include', 'fourstory_engine_ui.h']))
for u in units(LIB/'TComp/TComp/TComp.vcxproj'):
    if u not in TCOMP_REPLACED: jobs.append(('tcomp', LIB/'TComp/TComp'/u, original))
for u in ['stdafx.cpp', 'TChart.cpp']: jobs.append(('chart', LIB/'TChart/TChart'/u, original))
# HwidLib: only the SHA-256 unit; the WMI query units stop with #error off Windows (hwid_native.cpp).
jobs.append(('hwid', LIB/'HwidLib/HwidLib/__Sha256.cpp', original))
for u in ['cinterf.cpp', 'TCML.cpp', 'TCML.l.cpp', 'TCMLParser.cpp']: jobs.append(('tcml', LIB/'TCML/TCML'/u, original))
for u in units(CLIENT/'TClient.vcxproj'):
    if u not in CLIENT_REPLACED: jobs.append(('client', CLIENT/u, client_flags))
# Only the label unit gets the metadata; its .flags record makes a new choice
# or build time recompile it and enters the freshness record.
for s in sorted((NATIVE/'compat/src').glob('*.cpp')):
    jobs.append(('compat', s, [*own, *build_metadata.defines(a.channel, a.version, BUILD_UTC)] if s.name == 'build_info.cpp' else own))
sdl_inc = ['-I', str(SDL_PREFIX/'include')]
for s in sorted((NATIVE/'shell/src').glob('*.cpp')):
    jobs.append(('shell', s, [*(own if s.name == 'sdl_backend.cpp' else original), *sdl_inc]))
for s in sorted((NATIVE/'app/src').glob('*.cpp')): jobs.append(('app', s, [*client_flags, *sdl_inc]))
# Isolated Cocoa manager: compile without Windows compatibility prefix headers.
platform_sanitizers=[] if a.no_sanitize else ['-fsanitize=address,undefined','-fno-sanitize-recover=all']
jobs.append(('keyboard', NATIVE/'mac-keyboard/manager.mm', ['-std=c++17','-arch','arm64',*PATH_MAP,'-fobjc-arc','-g','-O1','-Wall','-Wextra','-Werror','-MMD',*platform_sanitizers]))
jobs.append(('audio', NATIVE/'audio/backend.mm', ['-std=c++17','-arch','arm64',*PATH_MAP,'-fobjc-arc','-g','-O1','-Wall','-Wextra','-Werror','-MMD',*platform_sanitizers]))
helper=OUT/'keyboard-mode-helper'
subprocess.run([CXX,'-std=c++17','-arch','arm64',*PATH_MAP,'-O1','-g','-Wall','-Wextra','-Werror',*platform_sanitizers,
    str(NATIVE/'mac-keyboard/helper.cpp'),'-framework','CoreFoundation','-framework','IOKit','-o',str(helper)],check=True)
# Generated resource and shader tables (gen_resources.py); rewritten only when the content changes.
GEN = OUT/'gen'; GEN.mkdir(exist_ok=True)
tmp = [GEN/'resources_generated.cpp.new', GEN/'shaders_generated.cpp.new']
subprocess.run([sys.executable, str(NATIVE/'app/gen_resources.py'), *map(str, tmp)], check=True)
for t in tmp:
    final = t.with_suffix('')
    if not final.exists() or final.read_bytes() != t.read_bytes(): t.replace(final)
    else: t.unlink()
    jobs.append(('gen', final, original))

# Original binary-format readers that load fields through unaligned pointer
# casts on every platform (Windows behavior). AArch64 performs unaligned
# scalar loads on normal memory, so only the UBSan alignment check is left out
# for these units; every other sanitizer check stays on (C6, research/c6-completion.md).
NO_ALIGNMENT_CHECK = {'TachyonHUGEMAP.cpp'}
if not a.no_sanitize:
    jobs = [(l, s, [*f, '-fno-sanitize=alignment'] if s.name in NO_ALIGNMENT_CHECK else f) for l, s, f in jobs]
# Embed the approved mockup artwork; no network or runtime source-tree paths.
asset_header=GEN/'account_assets.h'
asset_text='// Generated from native/accounts/assets; provenance.json records sources.\n'
for name,symbol in [('logo.png','account_logo'),('panel.jpg','account_panel'),('gold-button-sprite.jpg','account_button')]:
    data=(ROOT/'native/accounts/assets'/name).read_bytes()
    asset_text+='static const unsigned char '+symbol+'[] = {'+','.join(map(str,data))+'};\n'
if not asset_header.exists() or asset_header.read_text()!=asset_text: asset_header.write_text(asset_text)
jobs=[(label,source,[*flags,'-I',str(GEN)] if source.name=='accounts_ui.cpp' else flags) for label,source,flags in jobs]

def obj_of(label, source):
    return OBJ / (label + '-' + source.name + '.o')

def stale(source, obj, flags):
    if not obj.exists(): return True
    # A change of the compile flags (for example a macro) rebuilds the object.
    record = obj.with_suffix('.flags')
    if not record.exists() or record.read_text() != '\n'.join(flags): return True
    t = obj.stat().st_mtime
    dep = obj.with_suffix('.d')
    if not dep.exists(): return True
    text = dep.read_text(errors='replace').replace('\\\n', ' ')
    paths = shlex.split(text.split(':', 1)[1]) if ':' in text else [str(source)]
    for path in paths:
        try:
            if os.stat(path).st_mtime > t: return True
        except OSError:
            return True
    return False

def compile_one(job):
    label, source, flags = job
    obj = obj_of(label, source)
    if not stale(source, obj, flags): return label, source, 0, ''
    r = subprocess.run([CXX, *flags, '-MF', str(obj.with_suffix('.d')), '-c', str(source), '-o', str(obj)],
                       capture_output=True, text=True, errors='replace')
    if r.returncode and obj.exists(): obj.unlink()
    if not r.returncode: obj.with_suffix('.flags').write_text('\n'.join(flags))
    return label, source, r.returncode, r.stderr

# Existing .d files give transitive headers before compilation. Newly discovered
# dependencies must predate this snapshot; otherwise a change during compilation
# could be falsely certified as present in an earlier-compiled object.
known_inputs = set(fixed_inputs)
for label, source, flags in jobs:
    known_inputs.add(source)
    dep = obj_of(label, source).with_suffix('.d')
    if dep.exists(): known_inputs.update(p for p in dependencies(dep) if p.exists())
input_snapshot = capture_inputs(known_inputs, source_folders)
input_snapshot['inputs'].update(initial_snapshot['inputs'])
input_snapshot['inventories'] = initial_snapshot['inventories']

failed = []
with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
    for label, source, code, err in ex.map(compile_one, jobs):
        if code:
            failed.append((label, source))
            errors = [l for l in err.splitlines() if ' error: ' in l]
            print(f'FAIL {label}/{source.name}: {len(errors)} errors', flush=True)
            for l in errors[:8 if a.keep_going else 40]: print('   ', l.replace(str(SRC) + '/', ''))
            if not a.keep_going: break
print(f'Compiled {len(jobs) - len(failed)} of {len(jobs)} units', flush=True)
if failed:
    print('FAIL c4 compile:', ' '.join(f'{l}/{s.name}' for l, s in failed), flush=True)
    sys.exit(1)
if a.compile_only:
    print('PASS c4 compile', OUT, flush=True); sys.exit(0)

# Account implementation uses the approved checkout-local Rust dependencies.
account_objects=[]
try:
    subprocess.run([sys.executable, str(ROOT/'native/accounts/build.py')], check=True)
    for name in ['artwork']:
        obj=OUT/('accounts-'+name+'.o')
        subprocess.run([CXX,'-std=c++14',*PATH_MAP,'-fobjc-arc','-g','-O1',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all'] if not a.no_sanitize else []),'-c',str(ROOT/('native/accounts/platform/'+name+'.mm')),'-o',str(obj)],check=True)
        account_objects.append(str(obj))
except subprocess.CalledProcessError as e:
    # The Rust step needs the local toolchain and the deps/cargo cache.
    print('FAIL c4 accounts:', ' '.join(map(str, e.cmd[:3])), 'exit', e.returncode, flush=True)
    sys.exit(1)
objects = [str(obj_of(l, s)) for l, s, _ in jobs]
objects += account_objects+[str(ROOT/'build/accounts/release/libfourstory_account_bridge.a')]
link = [CXX, *[f for f in base if f != '-MMD'], *objects, str(DXVK), '-L', str(SDL_PREFIX/'lib'), '-lSDL3',
        '-Wl,-rpath,' + str(SDL_PREFIX/'lib'), '-Wl,-rpath,' + str(DXVK.parent), '-lz', '-liconv',
        '-framework','AVFoundation','-framework','AppKit','-framework','Carbon','-framework','ApplicationServices', '-framework', 'CoreText', '-framework', 'CoreGraphics', '-framework', 'CoreFoundation', '-framework', 'IOKit', '-framework', 'Foundation', '-framework', 'ImageIO', '-framework', 'Security', '-framework', 'SystemConfiguration',
        '-o', str(OUT/'4story-client')]
r = subprocess.run(link, capture_output=True, text=True, errors='replace')
if r.returncode:
    undefined = sorted(set(re.findall(r'"([^"]+)", referenced from', r.stderr)))
    (OUT/'link-undefined.txt').write_text('\n'.join(undefined) + '\n')
    print(r.stderr[-6000:] if not undefined else '', flush=True)
    print(f'FAIL c4 link: {len(undefined)} undefined symbols (list in {OUT/"link-undefined.txt"})', flush=True)
    sys.exit(1)
print('PASS c4 build:', OUT/'4story-client', flush=True)
# Preserve the input graph from the compilation that produced this executable.
build_inputs = set(fixed_inputs)
for label, source, flags in jobs:
    obj = obj_of(label, source)
    if stale(source, obj, flags):
        raise RuntimeError('Compilation inputs changed during the build; rebuild with build.py.')
    build_inputs.update(dependencies(obj.with_suffix('.d')))
    flagfile = obj.with_suffix('.flags')
    # These files are produced by compilation, so snapshot their verified value
    # now rather than rejecting their expected change from the previous build.
    flag_info = metadata(flagfile)
    if flagfile.read_text() != '\n'.join(flags) or metadata(flagfile) != flag_info:
        raise RuntimeError('Compilation flags changed during the build; rebuild with build.py.')
    input_snapshot['inputs'][str(flagfile.resolve())] = flag_info
    build_inputs.add(flagfile)
record_build(OUT, build_inputs, source_folders, input_snapshot)

# Distinct permission identity for native keyboard mode. Runtime libraries still
# come from the existing local C3 environment; this is not a distributable app.
app=OUT/'4Story.app';macos=app/'Contents/MacOS';macos.mkdir(parents=True,exist_ok=True)
for name in ('4story-client','keyboard-mode-helper'):shutil.copy2(OUT/name,macos/name)
resources=app/'Contents/Resources';resources.mkdir(parents=True,exist_ok=True)
shutil.copy2(NATIVE/'app/assets/4story.icns',resources/'4story.icns')
with (app/'Contents/Info.plist').open('wb') as f:
    plistlib.dump({'CFBundleExecutable':'start-client','CFBundleIdentifier':'org.fourstory.native-client',
        'CFBundleName':'4Story','CFBundleDisplayName':'4Story','CFBundlePackageType':'APPL',
        'CFBundleShortVersionString':a.version,'CFBundleVersion':a.version,
        'CFBundleIconFile':'4story.icns','NSHighResolutionCapable':True},f)
# Finder entrypoint selects this app's native executable, data and dependencies.
launcher = macos/'start-client'
script = (NATIVE/'app/start-client.sh').read_text()
launcher.write_text(script.replace('@WORKSPACE_ROOT@', shlex.quote(os.path.relpath(ROOT, macos))))
launcher.chmod(0o755)
# A provisioning profile left by an earlier signed build would expire; ad-hoc needs none.
(app/'Contents/embedded.provisionprofile').unlink(missing_ok=True)
subprocess.run(['codesign','--force','--sign','-',str(macos/'keyboard-mode-helper')],check=True)
# Identifier-only requirement keeps Accessibility access across rebuilds.
subprocess.run(['codesign','--force','--sign','-','-r=designated => identifier "org.fourstory.native-client"',str(app)],check=True)
record_complete(OUT)
print('Local permission bundle:',app,flush=True)
