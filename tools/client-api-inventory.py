#!/usr/bin/env python3
"""Inventory of Windows SDK and MFC names used by original client sources (C4-P).

Method: names declared in a copy of the Windows SDK and MFC headers (from the
Windows build machine, not committed) are matched against the identifier
tokens of the original sources, after comments and string literals are
removed. A name that the project itself declares (for example a method
`OnChar` of an engine class) is reported separately as "shadowed". The
result is a token count, not a resolved call graph.

Usage: client-api-inventory.py HEADER_DIR OUT_JSON SCOPE=PATH[,PATH...] ...
"""
import collections, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'source/4Story 5.0 Source Files'
IDENT = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')

# Header -> category. Unlisted SDK headers fall into "win32-other".
CATEGORY = {
    'winuser.h': 'user32 (windows, messages, input)', 'wingdi.h': 'gdi', 'imm.h': 'ime',
    'shlwapi.h': 'shell', 'shellapi.h': 'shell', 'shlobj_core.h': 'shell', 'shlobj.h': 'shell',
    'winreg.h': 'registry', 'mmsystem.h': 'multimedia', 'timeapi.h': 'multimedia', 'mmeapi.h': 'multimedia',
    'playsoundapi.h': 'multimedia', 'winnls.h': 'text conversion', 'stringapiset.h': 'text conversion',
    'processthreadsapi.h': 'threads and processes', 'synchapi.h': 'threads and processes',
    'handleapi.h': 'threads and processes', 'libloaderapi.h': 'modules', 'psapi.h': 'modules',
    'dbghelp.h': 'diagnostics', 'debugapi.h': 'diagnostics', 'errhandlingapi.h': 'diagnostics',
    'fileapi.h': 'files', 'sysinfoapi.h': 'system information', 'winbase.h': 'win32-base',
    'profileapi.h': 'timers', 'commctrl.h': 'common controls', 'wininet.h': 'internet', 'urlmon.h': 'internet',
    'exdisp.h': 'web browser control', 'mshtml.h': 'web browser control', 'exdispid.h': 'web browser control',
    'mshtmdid.h': 'web browser control', 'ole2.h': 'com/ole', 'oleauto.h': 'com/ole', 'objbase.h': 'com/ole',
    'winver.h': 'version', 'iphlpapi.h': 'network information', 'tlhelp32.h': 'diagnostics',
    'processenv.h': 'process environment', 'consoleapi.h': 'console', 'wincon.h': 'console',
}

def strip(text):
    """Remove comments, string and character literals, keep identifiers and line structure."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i); i = n if j < 0 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2); seg = text[i:n if j < 0 else j + 2]
            out.append('\n' * seg.count('\n')); i = n if j < 0 else j + 2
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(' '); i = j + 1
        else:
            out.append(c); i += 1
    return ''.join(out)

def header_names(directory):
    """Map declared name -> header file for SDK (um, shared) and MFC headers."""
    names = {}
    patterns = [
        re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)', re.M),
        re.compile(r'\b(?:WINAPI|APIENTRY|WINAPIV|CALLBACK|STDAPI|WINGDIPAPI|STDMETHODCALLTYPE|AFXAPI|AFX_CDECL)\s+([A-Za-z_]\w*)\s*\('),
        re.compile(r'\b(?:class|struct|union|enum)\s+(?:AFX_\w+\s+|__declspec\(\w+\)\s+)?([A-Za-z_]\w*)'),
        re.compile(r'\}\s*([A-Za-z_]\w*)\s*(?:,|;)'),
        re.compile(r'\btypedef\b[^;{]*?\b([A-Za-z_]\w*)\s*;'),
        re.compile(r'\btypedef\b[^;{(]*\(\s*(?:WINAPI|CALLBACK|APIENTRY)?\s*\*\s*([A-Za-z_]\w*)\s*\)'),
    ]
    method = re.compile(r'^\s*(?:virtual\s+|static\s+|AFX_INLINE\s+|afx_msg\s+|inline\s+)*[A-Za-z_][\w:<>,\s\*&]*?[\s\*&]([A-Za-z_]\w*)\s*\([^;{]*\)\s*(?:const\s*)?(?:=\s*0\s*)?;', re.M)
    for sub, label in (('um', None), ('shared', None), ('mfc', 'mfc')):
        for path in sorted((Path(directory) / sub).glob('*')):
            if path.suffix.lower() not in ('.h', '.inl'):
                continue
            text = strip(path.read_text(errors='replace'))
            found = set()
            for pattern in patterns:
                found.update(pattern.findall(text))
            if label == 'mfc':
                found.update(method.findall(text))
            for name in found:
                key = ('mfc/' + path.name) if label == 'mfc' else path.name.lower()
                names.setdefault(name, key)
    return names

def category(header):
    if header.startswith('mfc/'):
        return 'mfc'
    return CATEGORY.get(header, 'win32-other')

def project_declared(files):
    """Names that the project declares itself (methods, members, classes, macros)."""
    decl = set()
    rx = [re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)', re.M),
          re.compile(r'\b(?:class|struct)\s+([A-Za-z_]\w*)'),
          re.compile(r'[\s\*&]([A-Za-z_]\w*)\s*\([^;{)]*\)\s*(?:const\s*)?;'),
          re.compile(r'\b[A-Za-z_]\w*::([A-Za-z_]\w*)\s*\(')]
    for path in files:
        text = strip(path.read_text(encoding='latin-1'))
        for r in rx:
            decl.update(r.findall(text))
    return decl

def main():
    headers, out_json, scopes = sys.argv[1], Path(sys.argv[2]), sys.argv[3:]
    names = header_names(headers)
    # Ignore C/C++ keywords, the standard library and very generic tokens that the SDK also defines.
    ignore = set('''if else for while do switch case default return break continue sizeof new delete this
        true false NULL void int char short long float double unsigned signed const static virtual inline
        struct class union enum typedef public private protected operator friend template typename using
        namespace extern volatile register auto goto try catch throw bool min max far near
        IN OUT OPTIONAL interface small'''.split())
    result = {'method': __doc__.strip().splitlines()[2:7], 'scopes': {}}
    for scope in scopes:
        label, paths = scope.split('=', 1)
        files = []
        for p in paths.split(','):
            p = SRC / p
            files += [p] if p.is_file() else sorted(x for x in p.rglob('*') if x.suffix.lower() in ('.cpp', '.h', '.inl', '.hpp'))
        own = project_declared(files)
        use = collections.defaultdict(lambda: {'count': 0, 'files': collections.Counter(), 'first': None})
        for path in files:
            text = strip(path.read_text(encoding='latin-1'))
            for line_no, line in enumerate(text.splitlines(), 1):
                if line.lstrip().startswith('#include'):
                    continue
                for name in IDENT.findall(line):
                    if name in ignore or name not in names:
                        continue
                    entry = use[name]
                    entry['count'] += 1
                    rel = str(path.relative_to(SRC))
                    entry['files'][rel] += 1
                    if entry['first'] is None:
                        entry['first'] = f'{rel}:{line_no}'
        cats = collections.defaultdict(dict)
        for name, entry in use.items():
            header = names[name]
            cats[category(header)][name] = {'header': header, 'count': entry['count'], 'files': len(entry['files']),
                                            'first': entry['first'], 'shadowed': name in own}
        result['scopes'][label] = {'files': len(files), 'categories': cats}
    out_json.write_text(json.dumps(result, indent=1, sort_keys=True))
    for label, data in result['scopes'].items():
        print(f'== {label}: {data["files"]} files')
        for cat, entries in sorted(data['categories'].items()):
            real = {k: v for k, v in entries.items() if not v['shadowed']}
            print(f'{cat:40s} names {len(real):4d} uses {sum(v["count"] for v in real.values()):6d}'
                  f'   (shadowed names {len(entries) - len(real)})')

if __name__ == '__main__':
    main()
