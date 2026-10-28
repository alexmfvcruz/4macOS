"""Validate the local permission bundle against its last complete native build."""
import hashlib, json, subprocess
from pathlib import Path
from build_freshness import RECORD, validate_build

STAMP = 'keyboard-bundle.json'

def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as source:
        for data in iter(lambda: source.read(1024 * 1024), b''): h.update(data)
    return h.hexdigest()

def paths(out):
    return [out/'4story-client', out/'keyboard-mode-helper',
            out/'4Story.app/Contents/MacOS/4story-client',
            out/'4Story.app/Contents/MacOS/keyboard-mode-helper',
            out/'4Story.app/Contents/Info.plist', out/'4Story.app/Contents/MacOS/start-client',
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
