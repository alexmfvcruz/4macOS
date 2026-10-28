#!/usr/bin/env python3
"""Build the isolated native experiment with installed Apple tools; no downloads."""
import subprocess,plistlib,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
out=ROOT/'runtime/legacy/mac-keyboard-poc'
out.mkdir(parents=True,exist_ok=True)
subprocess.run(['clang++','-std=c++17','-fobjc-arc','-Wall','-Wextra','-Werror','-g',
    str(Path(__file__).with_name('probe.mm')),'-framework','Carbon','-framework','ApplicationServices','-framework','AppKit','-framework','CoreFoundation',
    '-framework','IOKit','-o',str(out/'keyboard-probe')],check=True)
print(out/'keyboard-probe')

# Standalone bundle provides a distinct macOS permission identity. Ad-hoc signing
# uses installed Apple tools; no certificate or software is downloaded.
app=out/'4Story Keyboard Test.app'
macos=app/'Contents/MacOS';macos.mkdir(parents=True,exist_ok=True)
shutil.copy2(out/'keyboard-probe',macos/'keyboard-probe')
with (app/'Contents/Info.plist').open('wb') as f:
    plistlib.dump({'CFBundleExecutable':'keyboard-probe','CFBundleIdentifier':'org.fourstory.keyboard-test',
        'CFBundleName':'4Story Keyboard Test','CFBundleDisplayName':'4Story Keyboard Test',
        'CFBundlePackageType':'APPL','CFBundleVersion':'1','NSHighResolutionCapable':True},f)
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
print(app)
