# C3 native D3D9 probes

The same probe source draws alpha-tested/blended quads and a render-to-texture
pass through IDirect3DDevice9. macOS uses the pinned DXVK Native/MoltenVK/SDL3
chain; Windows uses original Direct3D 9 and SDL3. The analytical probes do not load engine assets. C3-B3 now has a separate original-loader
scene harness, described below. C3 remains in progress.

## Native build/run

Install the approved dependencies as described in research/c3-plan.md.
The scripts do not download tools or run the Vulkan SDK installer.

```
sh tools/build-c3.sh --deps
python3 native/client/c3/check.py
sh tools/build-c3.sh --no-sanitize
python3 native/client/c3/check.py --plain
```

`--deps` configures/builds SDL3 into the local prefix, fixes its generated
pkg-config quoting for the space in this repository path, and builds the pinned
DXVK D3D9 target. Default build compiles only the probe. All tool paths resolve
inside `runtime/legacy/`; existing Xcode is selected per process. Source checkouts must
match the plan. SDK 1.4.363.0 and MoltenVK 1.4.2 are independent local packages.
The local-icd.json from installation points to the selected MoltenVK dylib.

check.py uses the selected MoltenVK directory FIRST in DYLD_LIBRARY_PATH,
then SDK, SDL3 and DXVK. This prevents dyld substituting the SDK's MoltenVK.
It fixes DXVK_WSI_DRIVER=SDL3, writes an explicit default config and puts shader
state caches in the ignored output directory. It runs one visible 640×480 SDL
window at a time, then closes it. It preserves all driver warnings.

Artifacts: `runtime/legacy/c3-results/` (sanitized) and `runtime/legacy/c3-results-plain/`.
Each has PPM render-target readback, lossless PNG, raw timing rows, complete logs
and results.json. Small native baseline images and plain results are committed
in baseline/; generated binaries, caches and full logs are not committed.

## What is checked

- Every one of 307,200 RGB pixels per scene against independent analytical
  region/texture/blending expectations. Only blended regions permit one LSB of
  rounding; current outputs have zero difference everywhere.
- Quads: left uses alpha >=128 with texture alpha 255 or64; right uses
  SRCALPHA/INVSRCALPHA over a blue background, point filtering, no sRGB conversion.
- RTT: four solid quadrants drawn to a128×128 texture, sampled in the main frame.
- All Direct3D HRESULT calls, completion marker, dimensions/format/pitch and
  write/close results are checked. Failed device/window/readback does not PASS.
- 30 warmup frames, 120 measured-phase frames; final readback frame excluded,
  leaving119 CPU wall-time samples. These are Draw/Present elapsed times, not
  GPU timestamps or gameplay FPS. Plain results are used in the report.
- ASan/UBSan instrument the probe only. DXVK, SDL3 and MoltenVK are release
  dependencies; no sanitizer coverage is claimed inside them.

The historical C3-A1 reference comparisons passed all24 readbacks across Windows
x64/Win32 (see native/client/reference/c3-windows/README.md). These cover analytical scenes. Geometry shaders/cull distance are unavailable in MoltenVK;
primitive-restart warnings occurred. These scenes pass despite those warnings;
original objects, characters and game shaders are not covered by those two scenes.

## Same source on Windows (Claude's resource)

Build `probe.cpp` as a console executable with MSVC C++17, /EHsc, the Windows
SDK's d3d9.h, and the matching SDL3 3.4.18 include directory/library/DLL. Link
`d3d9.lib` and `SDL3.lib`. Do not add native/compat/d3d9 include directories,
FOURSTORY_CLIENT_NATIVE, native prefix headers or the DXVK library on Windows.
SDL_MAIN_HANDLED and SDL_SetMainReady explicitly support this console entry.
Use architecture-matching SDL3 and build both x64/Win32 when available.

```
c3-probe.exe OUTPUT_PREFIX quads 120 30
c3-probe.exe OUTPUT_PREFIX rtt 120 30
```

Use the same fixed640×480 dimensions. Save actual Windows screenshots and raw
PPM readbacks; compare all pixels with native readbacks and list every difference.
Record display settings/scale and frame times separately. Windows build/run and
new original-object images are NOT TESTED by Codex. C2 is complete; the original
object scene build awaits REQUEST Q-B25.

## Synthetic R1 feature preflights

```
python3 native/client/c3/check.py --features
python3 native/client/c3/check.py --features --plain
```

These run three additional scenes using mixed vertex processing with software
processing enabled: `skinning` checks indexed fixed-function blending with
unequal weights and palette indices 0..3 and 252..255; `constants` uploads a
512-float4 bank starting at c397 and draws using c397/c908; `relative` reaches
those registers through vertex-driven relative addressing. Every pixel must
match exactly. Outputs go to `runtime/legacy/c3-features/` and
`runtime/legacy/c3-features-plain/`; committed feature baselines preserve the plain run.

On Windows keep `features.h` next to `probe.cpp`, build as above, then run:

```
c3-probe.exe OUTPUT_PREFIX skinning 120 30
c3-probe.exe OUTPUT_PREFIX constants 120 30
c3-probe.exe OUTPUT_PREFIX relative 120 30
```

These are translation-only, unlit fixtures with authored SM2 shaders. They do
not load original assets or execute the original VS_WMESH shader. Feature
frame times include transient resource creation and are diagnostic only.
See research/archive/c3/c3-feature-preflights.md for source contracts and runtime evidence.

## Texture formats and resource lifecycle

```
python3 native/client/c3/check.py --resources
python3 native/client/c3/check.py --resources --plain
```

`formats` checks A8R8G8B8, X8R8G8B8, A4R4G4B4, R5G6B5, A8 and DXT1–DXT5.
Each texture has four 8/4/2/1 mip levels. MAXMIPLEVEL chooses a different level
for each strip; alpha >=128 reveals alpha/punchthrough behavior. A8 uses diffuse
white for RGB and the texture as the alpha mask. DXT2/4 are tested with alpha
testing only; premultiplied blending and interpolated BC colors remain untested.
`lifecycle` verifies a small stateblock roundtrip, four same-size device resets,
managed texture preservation and recreation of a default-pool render target.
All six captures must match the analytical image exactly. Reset/readback frames
are excluded from the 115 retained timing samples. This is a backend probe;
the original engine uses manual UI state restoration, not these stateblocks.
Outputs are in `runtime/legacy/c3-resources/` and `runtime/legacy/c3-resources-plain/`.

Windows commands (keep both features.h and formats.h beside probe.cpp):

```
c3-probe.exe OUTPUT_PREFIX formats 120 30
c3-probe.exe OUTPUT_PREFIX lifecycle 120 30
```

## Exact native/Windows readback comparison

```
python3 native/client/c3/compare.py NATIVE.ppm WINDOWS.ppm runtime/legacy/c3-comparisons/SCENE
python3 native/client/c3/test_compare.py
```

The comparator accepts the portable probe's P6 readbacks. It does no resizing,
alignment, color conversion, masking or rounding tolerance. It writes all
changed RGB pixels to a TSV, a JSON summary, a raw absolute-delta PNG and
lossless PNGs of both inputs. Exit status is 0 for identical, 1 for different,
2 for invalid input. Keep actual Windows screenshots as separate visual evidence.
Historical Windows references have passed exact comparison; source snapshots and
limits are recorded in the Windows reference report.

## Original objects (C3-B3 and C3-J1)

See `research/c3-original-scenes.md` for IDs, fixed scene parameters, source list
and commands. `research/c3-completion.md` selects option C for C4 with explicit
limits: original scenes differ from Windows and the well intermittently fails
strict frozen-frame equality. Successful runs do not supersede failed attempts.

```
sh tools/build-c3.sh --assets
python3 native/client/c3/check_assets.py --assets ASSET_DIR --output NEW_OUTPUT_DIR
sh tools/build-c3.sh --assets --no-sanitize
python3 native/client/c3/check_assets.py --assets ASSET_DIR --plain --output NEW_PLAIN_DIR
```

`run-status.json` prevents an old results.json from establishing current success.
Use fresh output directories to retain failed attempts. Zero first/final equality
is still enforced; some well runs legitimately return nonzero. Do not suppress it.
`--shaders` / `check_assets.py --shaders-only` check original shader creation only.
Windows original Engine Lib builds/runs: `windows-assets.ps1` (serial x64/Win32);
actual window evidence: `windows-screens.ps1`. Both need the existing Windows VM
and installed dependencies; they do not install anything. The bounded capture hold
is outside measured frames. `baseline/original-scenes/` preserves exact RGB
comparisons, P6s, screenshots, raw timing rows, hashes and repeatability failures.

`check.py --checked` (also with `--features` or `--resources`) runs analytical
scenes through the same real-device HRESULT observer used by original objects.

## Texture sharpness regression

`sh tools/build-c3.sh --lod` builds the analytic mip-selection oracle;
`python3 native/client/c3/check_lod.py` runs it (add `--plain` for a plain build).
Use `--output NEW_DIR` to preserve a run. `--config FILE` selects an explicit
DXVK config; `d3d9.emulateSamplerLodBias = False` is an intentional failing control
on this M1 Max. `--backend` explicitly rebuilds the pinned, patched DXVK library.
Assembly/bytecode identities are in `lod-shaders/manifest.json`. Windows fixture
assembly and reference builds use `windows-lod-shaders.ps1` and `windows-lod.ps1`.

Original scenes accept `--scene static|character|character-ffp` (repeatable),
`--config`, and `--verify-only` with matching binary/backend/config/run identities.
Opt-in `C3_DIAGNOSTICS=1` dumps actual first-frame bound mip bytes and sampler
states. `C3_FORCE_LOD_BIAS` changes the draw's bias for causal controls only.
Default captures use original material state. Windows-assets.ps1 has explicit
`-diagnostics`, `-forceLodBias` and `-outputTag` parameters and restores inherited
environment state; diagnostics require a fresh output tag. Coordinate VM access.

`baseline/texture-sharpness/` retains corrected captures, Windows references,
every changed RGB pixel, timing tables, shader oracle and temporal failures.
`research/c3-texture-sharpness.md` records the result and limitations.
`sharpness_metrics.py --before BEFORE.ppm.gz --after AFTER.ppm.gz --reference
WINDOWS.ppm.gz` reports descriptive spatial detail without changing image data.
