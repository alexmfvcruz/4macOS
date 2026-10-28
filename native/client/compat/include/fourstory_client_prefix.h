// Prefix header for every original source file of the native client build:
//   clang++ -include fourstory_client_prefix.h ...
// The Microsoft CRT declares strcpy_s, _fileno and similar names in the
// standard C headers. Original files that include only <stdio.h> or
// <string.h> (for example the generated TCML parser) get them from here.
#pragma once
#include "windows.h"
