#!/usr/bin/env python3
"""Build the 4Story macOS release (C8): 4Story (Beta).app and its DMG.

Usage: python3 tools/release/build_release.py [--skip-build] [--no-dmg]
                                             [--channel beta|dev|stable] [--version 0.1]

Output: release/4Story macOS.dmg and, beside it, release/SHA256SUMS.txt (check
with `shasum -a 256 -c SHA256SUMS.txt` in release/). The app is staged in
runtime/legacy/release-build/. The client is built with native/client/app/build.py
(plain, macOS 14 target); the build asks for channel and version once, before
compiling. --skip-build reuses the channel, version and build time compiled into
the existing executable and refuses a different --channel/--version.
Game data comes from game (pristine), minus Windows-only, log, cache and
unused files; tools/release/patch_game_data.py applies the release changes.
Installs and downloads nothing. The server part is added separately.
"""
import argparse, hashlib, json, os, plistlib, shutil, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'native/client'))
sys.path.insert(0, str(ROOT / 'native/client/app'))
import deps  # noqa: E402
import build_metadata  # noqa: E402

APP_NAME = '4Story (Beta)'
BUNDLE_ID = 'org.fourstory.client'
CLIENT = ROOT / 'build/client/c4-plain'
# Pristine original client data (the development folder is written during play).
GAME = ROOT / 'game'
STAGE = ROOT / 'runtime/legacy/release-build'
RELEASE = ROOT / 'release'

# Top-level game entries the native client does not use (VERIFIED SOURCE where
# noted in research/c8-packaging-plan.md).
EXCLUDE_TOP = {'GameGuard', 'HShield', 'GameGuard.des', 'Uninstall.exe', 'Uninstall.ini',
               '4Story.txt', 'agreement.txt', 'URL.txt', 'LOL.txt', 'TCharTitle.tcd',
               '4storyEU.ini'}
EXCLUDE_SUFFIX = ('.dll', '.exe', '.dmp', '.log', '.orig', '.araz', '.DS_Store', '.BAK', '.bak')
EXCLUDE_NAMES = {'.DS_Store', 'Thumbs.db', 'dxvk.conf'}

FRAMEWORKS = {
    'libSDL3.0.dylib': deps.SDL3_PREFIX / 'lib/libSDL3.0.dylib',
    'libdxvk_d3d9.0.dylib': deps.DXVK_D3D9,
    'libMoltenVK.dylib': deps.MOLTENVK_LIB_DIR / 'libMoltenVK.dylib',
    'libvulkan.1.dylib': deps.VULKAN_LIB_DIR / 'libvulkan.1.dylib',
}


def run(*cmd, **kw):
    return subprocess.run([str(c) for c in cmd], check=True, **kw)


def copy_game(target):
    def ignore(directory, names):
        skip = set()
        top = Path(directory) == GAME
        for name in names:
            if name in EXCLUDE_NAMES or name.endswith(EXCLUDE_SUFFIX) or (top and name in EXCLUDE_TOP):
                skip.add(name)
        return skip
    # APFS clones: instant and no extra disk space until a file changes.
    shutil.copytree(GAME, target, ignore=ignore, copy_function=lambda s, d: run('cp', '-c', s, d))
    run(sys.executable, ROOT / 'tools/release/patch_game_data.py', target)


def thin(path):
    archs = subprocess.run(['lipo', '-archs', str(path)], capture_output=True, text=True).stdout.split()
    if len(archs) > 1 and 'arm64' in archs:
        run('lipo', path, '-thin', 'arm64', '-output', path)


def build_app(version):
    app = STAGE / f'{APP_NAME}.app'
    if app.exists():
        shutil.rmtree(app)
    contents = app / 'Contents'
    macos, frameworks, resources = contents / 'MacOS', contents / 'Frameworks', contents / 'Resources'
    for d in (macos, frameworks, resources / 'vulkan/icd.d'):
        d.mkdir(parents=True)

    exe = macos / '4Story'
    shutil.copy2(CLIENT / '4story-client', exe)
    shutil.copy2(CLIENT / '4Story.app/Contents/MacOS/keyboard-mode-helper', macos / 'keyboard-mode-helper')
    # Replace the development library paths with the app's Frameworks folder.
    rpaths = subprocess.run(['otool', '-l', str(exe)], capture_output=True, text=True).stdout
    for line in rpaths.splitlines():
        line = line.strip()
        if line.startswith('path ') and '(offset' in line:
            run('install_name_tool', '-delete_rpath', line[5:line.rindex(' (offset')], exe)
    run('install_name_tool', '-add_rpath', '@executable_path/../Frameworks', exe)
    run('strip', '-x', exe)

    for name, source in FRAMEWORKS.items():
        target = frameworks / name
        shutil.copy2(Path(os.path.realpath(source)), target)
        os.chmod(target, 0o755)
        thin(target)
        run('install_name_tool', '-id', f'@rpath/{name}', target)

    (resources / 'vulkan/icd.d/MoltenVK_icd.json').write_text(json.dumps({
        'file_format_version': '1.0.0',
        'ICD': {'library_path': '../../../Frameworks/libMoltenVK.dylib',
                'api_version': '1.4.0', 'is_portability_driver': True}}, indent=2) + '\n')
    (resources / 'dxvk.conf').write_text('# 4Story uses the DXVK defaults.\n')
    shutil.copy2(ROOT / 'native/client/app/assets/4story.icns', resources / '4story.icns')
    copy_game(resources / 'Game')
    # Player README (the "View README" button) and the client checker.
    shutil.copy2(ROOT / 'tools/release/docs/README.md', resources / 'README.md')
    (resources / 'tools').mkdir()
    checker = resources / 'tools/Client Checker.command'
    shutil.copy2(ROOT / 'tools/release/client_checker.sh', checker)
    os.chmod(checker, 0o755)
    game_files = sorted(str(f.relative_to(resources / 'Game')) for f in (resources / 'Game').rglob('*') if f.is_file())
    (resources / 'tools/game-files.txt').write_text('\n'.join(game_files) + '\n')

    with (contents / 'Info.plist').open('wb') as f:
        plistlib.dump({
            'CFBundleExecutable': '4Story',
            'CFBundleIdentifier': BUNDLE_ID,
            'CFBundleName': APP_NAME,
            'CFBundleDisplayName': APP_NAME,
            'CFBundlePackageType': 'APPL',
            'CFBundleShortVersionString': version,
            'CFBundleVersion': version,
            'CFBundleIconFile': '4story.icns',
            'LSMinimumSystemVersion': deps.MACOS_DEPLOYMENT_TARGET,
            'LSApplicationCategoryType': 'public.app-category.role-playing-games',
            'NSHighResolutionCapable': True,
            'LSMultipleInstancesProhibited': True,
            'NSHumanReadableCopyright': 'Game content belongs to its original owners.',
        }, f)

    # Ad-hoc signature: libraries and helper first, then the app.
    for item in sorted(frameworks.iterdir()):
        run('codesign', '--force', '--sign', '-', item)
    run('codesign', '--force', '--sign', '-', macos / 'keyboard-mode-helper')
    # Identifier-only requirement: macOS keeps Accessibility access across
    # rebuilds (research/accessibility-permission-signing.md).
    run('codesign', '--force', '--sign', '-',
        f'-r=designated => identifier "{BUNDLE_ID}"', app)
    run('codesign', '--verify', '--deep', '--strict', app)
    return app


def layout_window(mount, app_name):
    """Finder stores the window layout in the volume's .DS_Store; set it via Finder."""
    script = f'''
    tell application "Finder"
        tell disk "{mount.name}"
            open
            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            set the bounds of container window to {{200, 120, 860, 568}}
            set viewOptions to the icon view options of container window
            set arrangement of viewOptions to not arranged
            set icon size of viewOptions to 112
            set text size of viewOptions to 13
            set background picture of viewOptions to file ".background:background.tiff"
            set position of item "{app_name}" of container window to {{165, 210}}
            set position of item "Applications" of container window to {{495, 210}}
            close
            open
            update without registering applications
            delay 3
            close
        end tell
    end tell'''
    run('osascript', '-e', script)
    run('sync')


def build_dmg(app):
    RELEASE.mkdir(exist_ok=True)
    for old in Path('/Volumes').glob('4Story macOS*'):
        subprocess.run(['hdiutil', 'detach', str(old), '-force'], capture_output=True)
    folder = STAGE / 'dmg'
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir()
    run('cp', '-cR', app, folder / app.name)
    os.symlink('/Applications', folder / 'Applications')
    icon = ROOT / 'native/client/app/assets/4story.icns'
    shutil.copy2(icon, folder / '.VolumeIcon.icns')
    # Window background (1x + 2x in one TIFF), drawn from game art.
    background = folder / '.background'
    background.mkdir()
    run('swift', ROOT / 'tools/release/dmg_background.swift', ROOT / 'native/client/app/assets/dmg/background_art.png',
        ROOT / 'native/client/app/assets/intro/4story_logo.png', STAGE / 'bg.png', STAGE / 'bg@2x.png')
    run('tiffutil', '-cathidpicheck', STAGE / 'bg.png', STAGE / 'bg@2x.png', '-out', background / 'background.tiff')
    dmg = RELEASE / '4Story macOS.dmg'
    # Finder's alias for the window background records the path of this image,
    # so it lives outside the home folder (C8 sensitive-data scan).
    scratch = Path(tempfile.mkdtemp(prefix='4story-dmg-'))
    writable = scratch / 'writable.dmg'
    # The old checksum leaves with the DMG it describes; main() writes the new one.
    for old in (dmg, writable, RELEASE / 'SHA256SUMS.txt'):
        if old.exists():
            old.unlink()
    # Writable image first: the volume's custom-icon flag can only be set on a
    # mounted volume. Then compress it (LZFSE).
    run('hdiutil', 'create', '-volname', '4Story macOS', '-srcfolder', folder,
        '-fs', 'HFS+', '-format', 'UDRW', '-ov', writable)
    # Visible mount: Finder cannot lay out a volume mounted with -nobrowse.
    attached = subprocess.run(['hdiutil', 'attach', '-noautoopen', str(writable)],
                              check=True, capture_output=True, text=True).stdout
    mount = Path([line.split('\t')[-1] for line in attached.splitlines() if '/Volumes/' in line][-1].strip())
    try:
        run('SetFile', '-a', 'C', mount)
        # Finder writes .DS_Store late; without the background alias the window
        # shows no picture. Repeat the layout until it is stored.
        for attempt in range(3):
            layout_window(mount, app.name)
            store = mount / '.DS_Store'
            if store.exists() and b'backgroundImageAlias' in store.read_bytes():
                break
        else:
            raise SystemExit('Finder did not store the DMG window background. No DMG was written; build again.')
        shutil.rmtree(mount / '.fseventsd', ignore_errors=True)
        print('window layout:', (mount / '.DS_Store').stat().st_size, 'bytes')
    finally:
        run('hdiutil', 'detach', mount)
    run('hdiutil', 'convert', writable, '-format', 'ULFO', '-o', dmg)
    shutil.rmtree(scratch)
    # Icon of the .dmg file itself (Finder metadata on this Mac).
    run('swift', '-e', 'import AppKit; NSWorkspace.shared.setIcon(NSImage(contentsOfFile: CommandLine.arguments[1]), '
        'forFile: CommandLine.arguments[2], options: [])', icon, dmg)
    return dmg


def write_checksum(dmg):
    """Last packaging step: SHA256SUMS.txt beside the finished DMG."""
    digest = hashlib.sha256()
    with dmg.open('rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            digest.update(block)
    sums = dmg.parent / 'SHA256SUMS.txt'
    temporary = dmg.parent / '.SHA256SUMS.txt.new'
    temporary.write_text(f'{digest.hexdigest()}  {dmg.name}\n')
    try:
        # The same check a player runs in release/.
        run('shasum', '-a', '256', '-c', temporary.name, cwd=dmg.parent, capture_output=True)
    except subprocess.CalledProcessError:
        temporary.unlink()
        raise
    temporary.replace(sums)
    return sums, digest.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--skip-build', action='store_true', help='Use the existing plain client build.')
    p.add_argument('--no-dmg', action='store_true', help='Stage the app only.')
    p.add_argument('--channel', choices=build_metadata.CHANNELS, help='build channel (asked when missing)')
    p.add_argument('--version', help='version number such as 0.1 (asked when missing)')
    a = p.parse_args()
    if a.skip_build:
        # The existing executable keeps the metadata it was compiled with.
        stamp = build_metadata.read_stamp(CLIENT / '4story-client')
        if not stamp:
            p.error(f'{CLIENT / "4story-client"} has no build stamp; rebuild without --skip-build')
        if (a.channel and a.channel != stamp[0]) or (a.version and a.version != stamp[1]):
            p.error(f'the existing build is {stamp[0]}{stamp[1]}; rebuild without --skip-build to change it')
    else:
        try:
            channel, version = build_metadata.ask(a.channel, a.version)
        except (ValueError, EOFError, KeyboardInterrupt) as error:
            p.error(str(error) or 'no channel or version was entered')
        run(sys.executable, ROOT / 'native/client/app/build.py', '--channel', channel, '--version', version)
        stamp = build_metadata.read_stamp(CLIENT / '4story-client')
        if not stamp or stamp[:2] != (channel, version):
            p.error('the new build does not carry the chosen channel and version')
    print(f'build: {stamp[0]}{stamp[1]} built {stamp[2]}')
    STAGE.mkdir(parents=True, exist_ok=True)
    app = build_app(stamp[1])
    print(f'app: {app}')
    if not a.no_dmg:
        dmg = build_dmg(app)
        print(f'dmg: {dmg}')
        sums, digest = write_checksum(dmg)
        print(f'sha256: {digest}  ({sums})')


if __name__ == '__main__':
    main()
