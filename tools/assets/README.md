# Terminal asset renderer

Preview supplied 4Story game assets from their real meshes, textures and charts.
Run from any directory. Game code, data and database state are never changed.

```sh
sh tools/render-asset.sh list behemoth
sh tools/render-asset.sh show monster:31036
sh tools/render-asset.sh list 'Goddess' --category item
sh tools/render-asset.sh show monster:31036 --yaw 90 --width 100
sh tools/render-asset.sh show monster:31036 --plain
```

The full generated catalogue is [research/renderable-assets.md](../../research/renderable-assets.md).
Names are not unique: select an exact entry ID from `list` or the catalogue.
NPCs currently use template IDs because location/dialogue names are separate
from the model template. Archive names label unnamed raw resources.

## Output and installation

`show` creates no image files. It renders in memory and writes ANSI true-colour
half-blocks to stdout by default. Normal colour terminals can display these.
For higher detail in a supporting terminal:

```sh
sh tools/render-asset.sh show monster:31036 --format iterm
sh tools/render-asset.sh show monster:31036 --format kitty
sh tools/render-asset.sh show monster:31036 --format auto
```

iTerm/Kitty modes encode an in-memory PNG in the terminal image protocol; nothing
is saved to disk. `auto` detects iTerm2/Kitty and otherwise uses ANSI. Unsupported
terminal emulators may show escape codes; use ANSI in that case. Image modes
have been checked for protocol output locally, not visually in every terminal.
An IDE terminal must support ANSI true colour/Unicode to display the default.

The wrapper creates `deps/python/assets/` on first use and installs pinned
numpy/Pillow from requirements.txt. This needs Python 3.12 or newer and network
access on first setup. Both versions were validated with Python 3.14 on ARM64.
The environment is ignored. Metadata cache is `runtime/legacy/asset-renderer/catalog.json`.

```sh
sh tools/render-asset.sh catalog --refresh
sh tools/render-asset.sh list --category texture --limit 100
sh tools/render-asset.sh --assets '/path/to/client/data' catalog --refresh
```

A refresh parses all mesh/object records, decodes texture pixels and checks UI
image records and their tile-image dependencies. It writes Markdown and JSON
metadata, never PNG files. It can take longer than a single preview. Cache
freshness includes the index/chart/data file sizes and modification timestamps.

## Supported scope

- Named monster/mount chart entries, with matching chart equipment selections.
- NPC templates, using the same chart-driven selection path.
- Named items with standalone object geometry or a compatible equipment fragment.
  A fragment is a piece of geometry, not a character wearing the item. The default
  matching model is stated; exact alternatives are accessible as `part:` entries.
- Indexed objects and all supported appearance parts, including scenery props.
- Raw mesh geometry, untextured; decoded texture sheets and assembled UI artwork.

The tool reads stored static vertex coordinates and highest-detail (LOD 0)
triangle data. It simplifies lighting and primary-texture cutout rendering.
It does not simulate skeletal animation, character attachment transforms,
equipment hide rules/grade effects, texture animation/secondary stages, material
blending, particles or a complete map/terrain scene. Default object part selection
is deterministic, not a reconstruction of every possible in-game combination.
Monster/NPC item overrides match visual kind/cloth/normal-mesh selections where
present; additional attached equipment is not assembled. Raw resources from
MapSFX/ESFX can be static mesh previews without recreating the effect.

Unsupported/empty records are recorded in the catalogue's exclusions section.
Geometry whose primary texture is unavailable can still be viewed with `--plain`.
No FPS/performance or engine pixel-parity claim is made.

## Format evidence and maintenance

Original sources under `source/4Story 5.0 Source Files/`:

- TChart/TChart/TChart.cpp: InitTMONTEMP, InitTNPCTEMP, InitTITEMTEMP and
  InitTITEMVISUAL (sequential MBCS charts; explicit Windows-width scalar fields).
- Engine Lib/Engine Lib/TachyonRes.cpp: LoadOBJ, LoadMESH, LoadTEX, LoadIMG and
  LoadIMGBUF (index ID/file/offset triples and resource record layouts).
- Engine Lib/Engine Lib/TachyonCompressor.cpp: FromFile (two size fields + zlib).
- Engine Lib/Engine Lib/T3D.h: WMESHVERTEX 48 bytes, MESHVERTEX 40 bytes,
  TVERTEX 24 bytes; three explicit blend weights.
- Engine Lib/Engine Lib/D3DImage.cpp: tiled UI image vertices and image buffers.

Regression checks, including real Behemoth/fox appearance selections,
heterogeneous static resources, UI assembly and terminal output without saved
images: `sh tools/check-assets.sh`. No server or database reset is involved.
