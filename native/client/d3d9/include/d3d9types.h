// Dependency wrapper: upstream d3d9types.h assumes Win32/COM names exist.
// Keep D3DX-first includes valid without changing the vendored declarations.
#pragma once
#include "objbase.h"
#include "upstream/d3d9types.h"
