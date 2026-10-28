// Native subset of the shell light-weight API (C4-P).
#pragma once
#include <windows.h>
#include <sys/stat.h>
inline BOOL PathFileExists(LPCSTR path) {
    if (!path) return FALSE;
    struct stat st;
    return stat(fourstory::compat::native_path(path).c_str(), &st) == 0 ? TRUE : FALSE;
}
inline BOOL PathIsDirectory(LPCSTR path) {
    if (!path) return FALSE;
    struct stat st;
    return stat(fourstory::compat::native_path(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode) ? TRUE : FALSE;
}
