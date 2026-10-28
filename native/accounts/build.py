#!/usr/bin/env python3
"""Build/test account Rust crates using existing tools; never install/download.
Run --fetch only after approving local dependencies. Other commands are offline.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--fetch', action='store_true', help='Download locked Rust crates into this checkout only')
p.add_argument('--test', action='store_true', help='Run Rust tests and the C++/Rust ABI check')
a = p.parse_args()
explicit = os.environ.get('FOURSTORY_RUST_TOOLCHAIN')
candidates = [Path(explicit)] if explicit else [ROOT/'deps/tools/rust']
toolchain = next((d/'bin' for d in candidates if (d/'bin/cargo').is_file() and (d/'bin/rustc').is_file()), None)
if toolchain is None:
    raise SystemExit('Existing Rust toolchain missing. Set FOURSTORY_RUST_TOOLCHAIN; no installation attempted.')
cache=ROOT/'deps/cargo'
# C code built by crates (cc) targets the release's oldest macOS (C8). Rust
# itself already targets macOS 11; MACOSX_DEPLOYMENT_TARGET would also apply
# to host proc-macro dylibs, which the linker then emits malformed.
env = dict(os.environ, CFLAGS=(os.environ.get('CFLAGS','')+' -mmacosx-version-min=14.0').strip(), PATH=str(toolchain)+os.pathsep+os.environ.get('PATH',''), CARGO_HOME=str(cache), RUSTC=str(toolchain/'rustc'), RUSTDOC=str(toolchain/'rustdoc'))
# Release builds must not name this Mac's folders (C8 scan): remap the paths
# Rust keeps for panic messages. The encoded form allows the space in the
# checkout path; CFLAGS is split on spaces, so it maps only the home folder.
# --target keeps the flags off host build scripts: without it Cargo drops the
# rustversion build dependency of encoding_rs (E0463).
remap = [f'--remap-path-prefix={Path.home()}=~', f'--remap-path-prefix={ROOT}=/4story', f'--remap-path-prefix={cache}=/cargo']
release_env = dict(env, CFLAGS=env['CFLAGS']+f' -ffile-prefix-map={Path.home()}=~', CARGO_ENCODED_RUSTFLAGS='\x1f'.join(remap))
TRIPLE = 'aarch64-apple-darwin'
cargo = [str(toolchain/'cargo')]
manifest = ['--manifest-path', str(ROOT/'native/accounts/Cargo.toml')]
target = ROOT/'build/accounts'
def run(args, environment=None): subprocess.run(args, cwd=ROOT, env=environment or env, check=True)
if a.fetch:
    run([*cargo, 'fetch', *manifest, '--locked'])
    raise SystemExit(0)
run([*cargo, 'fmt', *manifest, '--all', '--', '--check'])
run([*cargo, 'build', *manifest, '--offline', '--locked', '--release', '--target', TRIPLE, '--target-dir', str(target)], release_env)
# Callers use target/release: the client links the bridge library;
# manage.py and tests/transport.py run the service.
(target/'release').mkdir(parents=True, exist_ok=True)
for name in ('libfourstory_account_bridge.a', 'fourstory-account-service'):
    shutil.copy2(target/TRIPLE/'release'/name, target/'release'/name)
if a.test:
    run([*cargo, 'test', *manifest, '--offline', '--locked', '--target-dir', str(target)])
    binary = target/'core-ffi-test'
    run([shutil.which('clang++') or 'clang++', '-std=c++14', '-g', '-fsanitize=address,undefined', '-I', str(ROOT/'native/accounts/include'), str(ROOT/'native/accounts/tests/core_ffi.cpp'), str(target/'release/libfourstory_account_bridge.a'), '-framework','Security','-framework','CoreFoundation','-o',str(binary)])
    subprocess.run([str(binary)], cwd=ROOT, env=dict(env, ASAN_OPTIONS='detect_leaks=0'), check=True)
