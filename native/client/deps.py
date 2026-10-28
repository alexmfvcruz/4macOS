"""Locations of the native client's third-party builds (local, not in git).

deps/sdl3        SDL3 install prefix (include, lib, pkgconfig)
deps/dxvk        DXVK Native build tree (src/d3d9/libdxvk_d3d9.0.dylib)
deps/moltenvk    MoltenVK 1.4.2 and local-icd.json
deps/vulkan-sdk  Vulkan SDK 1.4.363.0 (macOS/lib holds the loader)
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Oldest macOS the release supports (C8, user decision 2026-10-22).
MACOS_DEPLOYMENT_TARGET = '14.0'
DEPS = ROOT / 'deps'
SDL3_PREFIX = DEPS / 'sdl3'
DXVK_D3D9_DIR = DEPS / 'dxvk/lib'
DXVK_D3D9 = DXVK_D3D9_DIR / 'libdxvk_d3d9.0.dylib'
MOLTENVK_LIB_DIR = DEPS / 'moltenvk/MoltenVK/MoltenVK/dynamic/dylib/macOS'
MOLTENVK_ICD = DEPS / 'moltenvk/local-icd.json'
VULKAN_SDK = DEPS / 'vulkan-sdk/macOS'
VULKAN_LIB_DIR = VULKAN_SDK / 'lib'
RUNTIME_LIBRARY_DIRS = [MOLTENVK_LIB_DIR, VULKAN_LIB_DIR, SDL3_PREFIX / 'lib', DXVK_D3D9_DIR]
