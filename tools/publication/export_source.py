#!/usr/bin/env python3
"""Create a new, filtered GitHub source candidate; never modify the input tree."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = 'source/4Story 5.0 Source Files'
MAX_FILE = 100 * 1024 * 1024
# Source, build descriptions, notices and embedded resources. Unknown formats
# stay out and are recorded for review; this is not a blanket directory copy.
EXTENSIONS = {
    '.h', '.hpp', '.cpp', '.c', '.m', '.mm', '.inl', '.inc', '.swift', '.rs',
    '.py', '.sh', '.ps1', '.command', '.toml', '.lock', '.sql', '.md', '.txt',
    '.sln', '.vcproj', '.vcxproj', '.filters', '.rc', '.rc2', '.rgs', '.idl',
    '.psh', '.vsh', '.l', '.y', '.in', '.am', '.ac', '.manifest',
    '.ico', '.icns', '.cur', '.bmp', '.png', '.jpg', '.wav', '.bin', '.json', '.htm',
}
TEXT_EXTENSIONS = EXTENSIONS - {'.ico', '.icns', '.cur', '.bmp', '.png', '.jpg', '.wav', '.bin'}
ROOT_FILES = {'4story.command', 'Build.command', 'Check.command', 'Play.command', 'Server.command'}
# These tools operate on historic/local deployments, not the supported portable build.
OMIT_TOOLS = {'database', 'server', 'windows', 'profiling', 'c7', 'loadtest'}
PRIVATE_DIRS = {'__pycache__', '.git', '.local', 'baseline'}
PATTERNS = {
    'home path': rb'/Users/[A-Za-z0-9._-]+/',
    'private key': rb'-----BEGIN [A-Z ]*PRIVATE KEY-----',
    'GitHub token': rb'\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})',
    'AWS access key': rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b',
    'historic deployment address': rb'(?<![0-9.])(?:192\.168\.178\.76|10\.211\.55\.[24])(?![0-9.])',
}


def reason(path):
    rel = path.as_posix()
    if any(p in PRIVATE_DIRS for p in path.parts) or path.name in {'.DS_Store', 'Thumbs.db'}:
        return 'local/generated state'
    if rel in ROOT_FILES:
        return None
    if rel.startswith(ORIGINAL + '/'):
        return None if path.name in {'Makefile', 'LICENSE', 'COPYING', 'NOTICE', 'AUTHORS'} or path.suffix.lower() in EXTENSIONS else 'non-source original payload or IDE artifact'
    if rel.startswith('native/happy/'):
        return 'unfinished native Happy Tool excluded by project owner'
    if path.parts[0] == 'native':
        if 'reference' in path.parts and not rel.startswith('native/client/reference/shaders/'):
            return 'historical runtime/reference evidence'
        return None if path.suffix.lower() in EXTENSIONS else 'unselected native format'
    if path.parts[0] == 'tools':
        if len(path.parts) > 1 and path.parts[1] in OMIT_TOOLS:
            return 'historical/local deployment utility'
        if rel == 'tools/release/scan_release.py':
            return 'machine-specific release scanner'
        return None if path.suffix.lower() in EXTENSIONS else 'unselected tool format'
    return 'outside source allowlist (dependency/data/private/generated payload)'


def scrub(data, rel):
    """Redact only the exported copy. Keep original code bytes otherwise intact."""
    changes = []
    if Path(rel).suffix.lower() not in TEXT_EXTENSIONS:
        return data, changes
    if rel == 'tools/workspace.py':
        edits = [
            (b"['accounts', 'server', 'happy', 'client']", b"['accounts', 'server', 'client']"),
            (b"('all', 'client', 'server', 'accounts', 'happy')", b"('all', 'client', 'server', 'accounts')"),
            (b"        elif target == 'happy':\n            run(['/bin/sh', ROOT/'native/happy/scripts/build-app.sh'])\n", b''),
        ]
        count = 0
        for old, new in edits:
            count += data.count(old)
            data = data.replace(old, new)
        if count:
            changes.append({'category': 'remove unfinished native Happy build target', 'count': count})
    if Path(rel).suffix.lower() == '.md':
        data, count = re.subn(rb'(?ims)^\#{1,6}[^\r\n]*\bhappy\b[^\r\n]*\r?\n.*?(?=^\#{1,6} |\Z)', b'', data)
        data, lines = re.subn(rb'(?im)^[^\r\n]*\bhappy\b[^\r\n]*(?:\r?\n|$)', b'', data)
        if count or lines:
            data = data.replace(b'Install does four things:', b'Install does three things:')
            changes.append({'category': 'remove excluded tool documentation', 'count': count + lines})
    substitutions = [
        (re.escape(str(ROOT).encode()), b'/path/to/4story', 'developer checkout path'),
        (re.escape((str(Path.home()) + '/').encode()), b'/path/to/user/', 'developer home path'),
        (rb'(?<![0-9.])192\.168\.178\.76(?![0-9.])', b'192.0.2.1', 'historic LAN address'),
        (rb'(?<![0-9.])10\.211\.55\.[24](?![0-9.])', b'192.0.2.2', 'historic VM address'),
    ]
    # Unused, commented credentials in the supplied SMTP example.
    if rel.endswith('/Server/TLoginSvr/CSmtp.cpp'):
        data, count = re.subn(rb'(?m)^([^\r\n]*//[^\r\n]*m_sPassword\s*=)[^\r\n]*',
                              rb'\1 ""; // example credential removed for publication', data)
        if count:
            changes.append({'category': 'commented SMTP credential', 'count': count})
    for pattern, replacement, category in substitutions:
        data, count = re.subn(pattern, replacement, data)
        if count:
            changes.append({'category': category, 'count': count})
    return data, changes


def known_secrets(root):
    """Read local values without printing or copying them into a report."""
    values = set()
    for path in (root/'source/4Story 5.0 Registry and Services').rglob('*.reg'):
        raw = path.read_bytes()
        text = raw.decode('utf-16') if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else raw.decode('latin-1')
        for key, value in re.findall(r'"([^"\r\n]+)"\s*=\s*"([^"\r\n]*)"', text):
            if re.search(r'pass|pwd|secret|token', key, re.I) and len(value) >= 4:
                values.add(value)
    runtime = root/'runtime'
    for path in runtime.rglob('*.env'):
        for line in path.read_text(errors='replace').splitlines():
            key, sep, value = line.partition('=')
            if sep and re.search(r'PASSWORD|SECRET|TOKEN', key) and len(value.strip()) >= 8:
                values.add(value.strip().strip('"\''))
    return values


def scan(data, secrets):
    hits = []
    # UTF-16 source/resources are checked as well as raw bytes.
    views = [data]
    for encoding in ('utf-16-le', 'utf-16-be'):
        views.append(data.decode(encoding, errors='ignore').encode('utf-8'))
    for name, pattern in PATTERNS.items():
        if any(re.search(pattern, view) for view in views):
            hits.append(name)
    if any(value.encode(encoding) in data for value in secrets
           for encoding in ('utf-8', 'utf-16-le', 'utf-16-be')):
        hits.append('known local credential')
    return hits


def export(root, output, report):
    root, output, report = root.resolve(), output.resolve(), report.resolve()
    if output.exists() or report.exists():
        raise ValueError('Output/report already exists; choose new paths')
    # Never let an output location overwrite an input, or recurse into itself.
    if output == root or root.is_relative_to(output):
        raise ValueError('Output must not contain the input workspace')
    if report == output or report.is_relative_to(output):
        raise ValueError('Keep the local audit report outside the public export')
    output.mkdir(parents=True)
    selected, excluded, redacted, findings = [], [], [], []
    secrets = known_secrets(root)
    candidates = [root/n for n in sorted(ROOT_FILES)]
    for folder in (root/ORIGINAL, root/'native', root/'tools'):
        candidates.extend(sorted(p for p in folder.rglob('*') if p.is_file() or p.is_symlink()))
    for path in candidates:
        rel = path.relative_to(root)
        why = reason(rel)
        if path.is_symlink() or any(parent.is_symlink() for parent in path.parents if parent != root and parent.is_relative_to(root)):
            why = 'symlink or symlinked ancestor excluded; review target separately'
        if not path.resolve().is_relative_to(root):
            why = 'external input excluded'
        if not path.is_file():
            why = 'missing input'
        if why:
            excluded.append({'path': rel.as_posix(), 'reason': why})
            continue
        if path.stat().st_size >= MAX_FILE:
            excluded.append({'path': rel.as_posix(), 'reason': 'GitHub size limit; separate input'})
            continue
        data, changes = scrub(path.read_bytes(), rel.as_posix())
        hits = scan(data, secrets)
        if hits:
            findings.append({'path': rel.as_posix(), 'categories': hits})
        target = output/rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        target.chmod(0o755 if path.stat().st_mode & 0o111 else 0o644)
        selected.append({'path': rel.as_posix(), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        if changes:
            redacted.append({'path': rel.as_posix(), 'changes': changes})
    # Publisher-maintained docs override the portable bundle's offline instructions.
    for name, template in [('README.md', 'SOURCE-README.md'), ('.gitignore', 'source.gitignore')]:
        source = root/'tools/publication'/template
        if source.is_symlink() or any(p.is_symlink() for p in source.parents if p != root and p.is_relative_to(root)):
            raise ValueError('Publication templates must not be symlinked')
        data, changes = scrub(source.read_bytes(), name)
        hits = scan(data, secrets)
        if hits:
            findings.append({'path': name, 'categories': hits})
        (output/name).write_bytes(data)
        selected.append({'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        if changes:
            redacted.append({'path': name, 'changes': changes})
    (output/'SOURCE-MANIFEST.json').write_text(json.dumps({'files': selected}, indent=2)+'\n')
    result = {'status': 'scan-blocked' if findings else 'source-candidate',
              'files': len(selected), 'bytes': sum(x['bytes'] for x in selected),
              'excluded': excluded, 'redactions': redacted, 'findings': findings,
              'known_credential_values_checked': len(secrets),
              'limits': ['Pattern scan is not a complete secret audit.',
                         'Redistribution permission is unresolved.',
                         'Dependencies, game data and database seeds are separate inputs.',
                         'Fresh build must be recorded separately.']}
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2)+'\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'.publication/4macOS')
    parser.add_argument('--report', type=Path, default=ROOT/'.publication/4macOS-audit.json')
    args = parser.parse_args()
    result = export(ROOT, args.output, args.report)
    print(json.dumps({k: result[k] for k in ('status', 'files', 'bytes', 'findings')}, indent=2))
    return 1 if result['findings'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
