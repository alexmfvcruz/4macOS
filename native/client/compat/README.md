# Client compatibility layer

Headers that let the original client libraries compile with Apple Clang for
ARM64 (roadmap step C1). Owner: Claude Code. Codex requests additions with a
REQUEST entry in `research/coordination/codex.md`.

Scope: what `TCML` and `TChart` need (C1), the session (C2) and the window,
input, text and settings layer of the interface (C4-P, section "Window
system"). Device and sound classes are not provided.

## Use

```
clang++ -std=gnu++14 -DFOURSTORY_CLIENT_NATIVE -DFOURSTORY_CLIENT_64 \
  -DFOURSTORY_CLIENT_MOD -include fourstory_client_prefix.h \
  -I native/client/compat/include ...
```

- `-include fourstory_client_prefix.h` is required. Original files that
  include only C headers use `strcpy_s` and `_fileno`.
- The headers compile as C++14 and C++20, with and without
  `-fms-extensions`.
- The target must be little endian with a 64-bit pointer.

Check: `sh native/client/compat/check.sh` (three builds with AddressSanitizer
and UndefinedBehaviorSanitizer, `-Wall -Wextra -Werror`). The script mounts a
temporary 8 MB case-sensitive disk image for the file name test and removes
it at the end.

## Headers

| Header | Content |
| --- | --- |
| `windows.h` | Scalar and pointer types, handles, `RECT`/`POINT`/`SIZE`, macros (`MAKELONG`, `RGB`, ...), memory helpers, Microsoft CRT names, `native_path`, `GetTickCount`, `Sleep`, `GetLastError`/`SetLastError`, `FormatMessage`, `LocalAlloc`/`LocalFree`, `GetComputerName` |
| `afx.h` | `CString`, `CFile`, `CStdioFile`, `CArchive`, `CTime`, `CTimeSpan`, MFC exceptions with `TRY`/`CATCH`, `CPoint`, `CSize`, `CRect`, `TRACE`/`ASSERT`/`VERIFY` |
| `winsock2.h`, `winsock.h` | Winsock names over BSD sockets: `SOCKET`, `WSASocket`, `ioctlsocket(FIONBIO)`, `closesocket`, `WSAGetLastError`, `WSAStartup`, Winsock error codes, an aligned IPv4 `gethostbyname` (C2) |
| `wincrypt.h` | CryptoAPI subset: `PROV_RSA_FULL` provider, MD5 hash, RC4 key from the hash, `CryptEncrypt`/`CryptDecrypt` (C2) |
| `Security.h`, `nb30.h` | `GetComputerNameEx`; `nb30.h` is empty (C2) |
| `afxwin.h` | Includes `afx.h` only |
| `afxsock.h` | Includes `afx.h` and `winsock2.h` |
| `d3d9types.h` | `D3DCOLOR` and its macros, `D3DVECTOR`, `D3DMATRIX`, `D3DCOLORVALUE`, `D3DRECT` |
| `malloc.h`, `io.h`, `vld.h` | Microsoft header names |
| `fourstory_client_prefix.h` | Prefix header |

Not here: `d3dx9.h` and all D3DX types and functions. They belong to the
D3DX replacement (Codex).

## Differences from Windows and MFC

Type widths:

- `LONG`, `ULONG` and `DWORD` are 32 bits (`int`, `unsigned int`). The C
  types `long` and `unsigned long` are 64 bits on this target. `CArchive`
  reads and writes them as 32 bits, as on Windows.
- Pointer-sized integers (`DWORD_PTR`, `INT_PTR`, `LPARAM`, ...) are
  `long long` types. `CArchive` keeps them at 64 bits.
- `size_t` is `unsigned long` on this target, so `CArchive` stores a `size_t`
  as 32 bits. Windows x64 stores 64 bits. No use in `TCML` or `TChart`;
  check each use in later code.
- `WCHAR` is `char16_t` (16 bits). `wchar_t` is 32 bits on this target.
- `__int64` is a typedef name. `unsigned __int64` does not compile; write
  `UINT64`.

`CString`:

- One pointer wide. Each object owns its buffer; MFC shares buffers between
  copies. No code in scope depends on sharing.
- Text is bytes in the code page of the data. No conversion is made.
- `MakeUpper`, `MakeLower` and `CompareNoCase` change or ignore the case of
  ASCII letters only. MFC uses the current locale for bytes above 0x7F.
- Search, trim and token functions work on single bytes. MFC (MBCS build)
  skips the trail byte of a double-byte character. Western data has no
  double-byte characters; Asian code pages are NOT SUPPORTED.
- A `CString` cannot pass through `...` in Clang. `Format`, `AppendFormat`,
  `sprintf_s` and `_snprintf` are templates that convert `CString` arguments
  to `const char*`. A call of another variadic function (for example
  `printf`) with a `CString` argument does not compile: add `LPCSTR(...)` at
  the site.
- Format strings are converted before use: `%ld`, `%lu`, `%lx` (32-bit on
  Windows) lose the `l`; `%I64d` becomes `%lld`; `%I32d` becomes `%d`. This
  applies to `Format`, `FormatV`, `sprintf_s`, `_snprintf` and the `v`
  forms. It does not apply to `printf`, `fprintf` or `sprintf`: a call with
  `%ld` and a `LONG`, or with `%I64`, gives wrong text there. Correct the
  site.
- `%s` with a null pointer prints `(null)`, as in the Microsoft CRT.
- `GetBuffer(0)` on an empty string gives room for 7 characters, as the MFC
  allocator does.

`CFile`:

- File names go through `fourstory::compat::native_path`: `\` becomes `/`,
  and a component that does not exist is matched without regard to case.
  Drive letters are not translated.
- Sharing flags are ignored. `typeText` is ignored: no line-end translation.
- `fopen_s` and `fopen` do not translate line ends either. A file opened
  with `"r"` keeps its CR bytes; the Microsoft CRT removes them. Affected:
  the `.tcml` script parser (`TCMLParser.cpp:193`). The binary layout loader
  (`TCMLParser::Load`, mode `"rb"`) is not affected.

`CArchive`:

- Scalars and `CString` in the MFC binary format. No `CObject` serialization.
- No internal buffer: each operation reads or writes the file. The file
  position is always exact; MFC reads ahead.
- A UTF-16 string (prefix `FF FFFE`) is converted one code unit to one byte;
  units above 0xFF become `?`.
- A string length that exceeds the rest of the file throws
  `CArchiveException` (`endOfFile`) before any allocation.
- `operator>>` needs an lvalue. MSVC accepts `ar >> (WORD) wCount`; Clang
  does not. Correct the site, do not add an overload: MSVC reads into the
  variable when the cast names the type of the variable, and into a
  temporary when it names another type.

Microsoft CRT:

- The secure functions (`strcpy_s`, `memcpy_s`, ...) return an error code
  and leave an empty destination. The Microsoft invalid-parameter handler,
  which ends the process by default, is not emulated.

Diagnostics: `TRACE` and `ASSERT` produce no code, as in a Windows Release
build. `VERIFY` evaluates its expression.

### Additions for C2 (networking)

- `GetLastError()` maps `errno` to the Win32 or Winsock code when the last
  failure came from a POSIX call (`EINPROGRESS` and `EAGAIN` become
  `WSAEWOULDBLOCK`). `SetLastError()` stores a code and tags `errno`; the
  next failing POSIX call replaces it. A successful call does not clear the
  code on either system. Unmapped `errno` values become `0x20000000 | errno`.
- `FormatMessage` supports only `FORMAT_MESSAGE_FROM_SYSTEM` and returns
  "Error <code>." with CR LF, not the Windows text.
- `WSAAsyncSelect` and the other window-message functions are not provided.
  The session owner polls (board, C2 contract item 9).
- Sockets from `WSASocket` set `SO_NOSIGPIPE`: a send to a closed peer fails
  with `WSAESHUTDOWN` instead of stopping the process.
- `gethostbyname` resolves IPv4 only, through `getaddrinfo`. The macOS
  function returns a misaligned address list for numeric names.
- CryptoAPI: only the calls of `CryptographyExt.cpp`. The RC4 key is the
  whole MD5 digest, as with the Microsoft Strong Cryptographic Provider (the
  default `PROV_RSA_FULL` provider of current Windows). Key lengths other
  than 128 bits, salts, other algorithms and key containers fail with an
  `NTE_*` code. The key stream continues across `CryptEncrypt` calls and
  restarts after `final = TRUE`; both behaviors were observed on Windows.
- `CStdioFile` in text mode writes `\n` as CR LF and removes the CR when it
  reads; Ctrl-Z is not an end-of-file mark.
- `CTime` uses local time through `localtime_r`/`mktime`, with only the
  members listed in the header.

## Verification state

- VERIFIED (RUNTIME, 2026-10-14): `wincrypt.h` reproduces 15 RC4 vectors and
  2 key-stream cases made with Windows CryptoAPI on Windows 11 (x86 and x64,
  identical; `tests/fixtures/rc4-cryptoapi.txt`, generator
  `tests/fixtures/rc4vectors-windows.cpp`). MD5 passes the RFC 1321 vectors.

- VERIFIED (BUILD, RUNTIME): `check.sh` passes; the tests include the known
  bytes of the MFC archive format.
- VERIFIED (BUILD, syntax only): `TCMLParser.cpp`, `cinterf.cpp`, `TCML.cpp`
  and `TCML.l.cpp` compile without an error against these headers.
- Independent critic review (2026-10-13): 11 findings, all corrected or
  documented above; a test covers each corrected one.
- The expected archive bytes in the tests come from the documented MFC
  format, not from a file that MFC wrote.
- NOT TESTED: comparison of any result with a Windows program built with
  MFC. Task C1-J1 covers this for the chart and layout data.
- NOT MEASURED: the cost of the unbuffered `CArchive` (one `read` call per
  scalar) on the large chart files.

## Window system (C4-P)

Library `libcompat_ui.a` (`src/*.cpp`), built by
`python3 native/client/shell/build.py --target compat-ui`; it needs
`-framework CoreText -framework CoreGraphics -framework CoreFoundation -liconv`.
Tests: `c4-compat-ui-test` (scripted backend), `c4-shell-test` (SDL3).

| Header | Content |
| --- | --- |
| `fourstory_user32.h` | windows, message queue, input, timers, cursor, clipboard, code pages, the backend interface `fourstory::platform::Backend` |
| `fourstory_win32_constants.h` | generated numeric constants of the Windows SDK 10.0.26100.0 headers (values only) |
| `fourstory_gdi.h` | fonts, memory DCs, 32-bit DIB sections, stock objects |
| `fourstory_kernel32.h` | events, threads, waits, performance counter, local time, TCHAR names |
| `afxwin.h` | MFC message maps, `CWnd`, `CWinThread`, `CWinApp` (profile store), GDI wrappers, `AfxWinMain` |
| `gdiplus.h` | GDI+ subset over Core Text |
| `imm.h`, `winsvc.h`, `shlwapi.h`, `mmsystem.h`, `dsound.h`, `dshow.h`, `Wbemidl.h`, `afxext.h`, `afxcmn.h`, `afxdtctl.h`, `afxdisp.h`, `afxdhtml.h`, `direct.h`, `dbghelp.h`, `tlhelp32.h`, `dxdiag.h`, `strsafe.h`, `Psapi.h` | declarations; calls fail (`imm.h`, `winsvc.h`) or the names exist for includes only |

Differences from Windows:

- One GUI thread: the thread that creates the first window. `PostMessage`
  and `SendMessage` work from any thread (a cross-thread `SendMessage` runs
  when the GUI thread retrieves messages); `PeekMessage`, `GetMessage`,
  `WaitMessage` and window creation work on the GUI thread only.
- Top-level windows are backend windows (SDL3); child windows exist only in
  the window tree (no native view). Coordinates are window points; no high
  pixel density is requested. `AdjustWindowRect` and `CreateWindowEx` share
  one frame model: a captioned window has a 28-point title bar, no border.
- Retrieval order: sent, posted, `WM_QUIT`, input, `WM_PAINT`, `WM_TIMER`.
  The position of `WM_QUIT` between posted and input messages is INFERRED from
  Windows documentation, not measured.
- Keys: Option is Alt (`VK_LMENU`, `VK_RMENU`), Command is `VK_LWIN`/`VK_RWIN`.
  Text typed with Option arrives as `WM_CHAR`; no `WM_SYSCHAR` is generated.
  Keyboard layout switching and East Asian input composition are NOT SUPPORTED.
- `WM_CHAR` carries bytes of the ANSI code page: 1252 by default,
  `FOURSTORY_ACP` = 1250, 1251, 1253 or 1254 (iconv). Characters that the code
  page lacks become `?`.
- The window DC has no pixels: GDI and GDI+ drawing to a window is not shown.
  Memory DCs with 32-bit top-down DIB sections work (all the client uses).
- Program resources (accelerators, cursors, icons, string table) do not exist
  yet: the load calls fail. `SetCursor(NULL)` hides the cursor.
- The profile store is the REGEDIT4 file
  `$FOURSTORY_SETTINGS_DIR` or `~/Library/Application Support/4Story`, then
  `<registry key>/<profile>/settings.reg`, written at every change. Names are
  compared without ASCII case. Values with line breaks are refused. Values of
  other types (`hex:`) in an imported file are not kept when the file is written.
- GDI+ (Core Text): measurement follows rules measured on Windows (`gdiplus.cpp`
  head comment); word wrap is NOT IMPLEMENTED (a layout-rectangle measurement
  of text wider than the rectangle returns `NotImplemented`); glyph pixels come
  from the Core Text rasterizer. A face that does not exist resolves to Arial
  (as GDI on a Western Windows); `@` faces resolve to Malgun Gothic or, absent
  on macOS, Apple SD Gothic Neo.
- MFC calls `GetSafeHwnd`, `GetSafeHandle` and `GetSafeHdc` through null
  pointers. That is undefined in C++; the native build uses
  `-fno-delete-null-pointer-checks` so that the `this` test stays.
- `PulseEvent`, `TerminateThread`, global hooks, the gamma ramp and the service
  control manager fail with `ERROR_NOT_SUPPORTED`.
