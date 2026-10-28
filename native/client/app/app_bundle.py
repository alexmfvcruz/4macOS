"""Package and validate the main client application bundle with its supplied icon."""
import hashlib, json, subprocess
from pathlib import Path
from build_freshness import RECORD, validate_build

STAMP = 'app-bundle.json'

def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as source:
        for data in iter(lambda: source.read(1024 * 1024), b''): h.update(data)
    return h.hexdigest()

def paths(out):
    return [out/'4story-client',
            out/'4Story.app/Contents/MacOS/4story-client',
            out/'4Story.app/Contents/Info.plist',
            out/'4Story.app/Contents/Resources/4story.icns']

def verify_signature(out):
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(out/'4Story.app')],
                   check=True, capture_output=True)

def record_complete(out):
    validate_build(out)
    verify_signature(out)
    record = {str(path.relative_to(out)): digest(path) for path in [*paths(out), out/RECORD]}
    temporary = out/(STAMP+'.new')
    temporary.write_text(json.dumps(record, sort_keys=True)+'\n')
    temporary.replace(out/STAMP)

def select_binary(binary):
    out = binary.parent
    validate_build(out)
    bundled = out/'4Story.app/Contents/MacOS/4story-client'
    if not bundled.exists(): return binary
    try:
        record = json.loads((out/STAMP).read_text())
        for path in [*paths(out), out/RECORD]:
            if record.get(str(path.relative_to(out))) != digest(path):
                raise ValueError('build hashes do not match')
        verify_signature(out)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise RuntimeError('The native app bundle is stale or incomplete. Rebuild native/client/app/build.py before running.') from error
    return bundled


def package_client(out, icon):
    import plistlib, shutil
    out = Path(out)
    validate_build(out)
    (out/STAMP).write_text('{}\n')
    app = out/'4Story.app'
    macos = app/'Contents/MacOS'
    resources = app/'Contents/Resources'
    macos.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    shutil.copy2(out/'4story-client', macos/'4story-client')
    shutil.copy2(icon, resources/'4story.icns')
    with (app/'Contents/Info.plist').open('wb') as stream:
        plistlib.dump({'CFBundleExecutable':'4story-client',
                      'CFBundleIdentifier':'org.fourstory.native-client.main',
                      'CFBundleName':'4Story', 'CFBundleDisplayName':'4Story',
                      'CFBundlePackageType':'APPL', 'CFBundleVersion':'1',
                      'CFBundleIconFile':'4story.icns', 'NSHighResolutionCapable':True}, stream)
    # Identifier-only requirement keeps Accessibility access across rebuilds.
    subprocess.run(['codesign','--force','--sign','-',
                    '-r=designated => identifier "org.fourstory.native-client.main"',str(app)], check=True)
    record_complete(out)
    return app
