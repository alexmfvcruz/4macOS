# C6 preparation tools

Requires Python3.9+ and existing repository C3 comparator. No third-party package
or installation is needed. No original game source or C4 build file is changed.

```sh
python3 native/client/c6-validation/inventory.py \
  --assets '/path/to/4Story 5.0 Client' --map 0 --output runtime/legacy/c6/map0.json
python3 -m unittest discover -s native/client/c6-validation -p 'test_*.py' -v
```

The inventory reads only source-backed map index/chart/header and TMU prefixes.
Its file hashes cover unparsed image tails too; hashes do not validate decoding.
Use `--placements` only when a full placement list is useful (large output).
Object placement finiteness does not establish mesh geometry finiteness.

`scene-candidates.json` supplies three data-backed cameras. None has been rendered
or visually approved. `capture-contract.draft.json` intentionally fails validation
until a reviewed Windows reference supplies its unresolved settings, matrices,
used texture/shader identities and framing. Keep new approved contracts separately
so the original draft and all failed runs remain available.

See `research/c6-validation-plan.md` for source evidence, category matrix, exact
IDs, unknown BSP/glow paths and execution order.

## Paired capture bundle

Each approved scene belongs in `NATIVE_DIR/SCENE_ID/` and
`WINDOWS_DIR/SCENE_ID/`. Each directory has `capture.json`, at least five separate
P6 RGB readbacks, UTF8 runtime log and a timing TSV. Paths are relative to the
scene directory and must resolve inside it; aliases to the same file are rejected.
The bundle's JSON has these required fields:

```json
{
  "platform": "native",
  "scene_sha256": "SHA256_OF_CANONICAL_SCENE_OBJECT",
  "asset_hashes": {"FILE": "SHA256_MATCHING_APPROVED_SCENE"},
  "actual_parameters": {"COPY_OF_ACTUAL_LOGGED_PARAMETERS": "must equal scene.parameters"},
  "build_sha256": "EXECUTABLE_SHA256",
  "backend_sha256": "RENDERER_DLL_OR_DYLIB_SHA256",
  "config_sha256": "EFFECTIVE_CONFIG_SHA256",
  "failed_device_calls": 0,
  "nonfinite_meshes": 0,
  "meshes_checked": 0,
  "draws": {"terrain_base": 1, "terrain_detail": 1},
  "runtime_log": "run.log",
  "runtime_log_sha256": "LOG_SHA256",
  "readbacks": ["f0.ppm", "f1.ppm", "f2.ppm", "f3.ppm", "f4.ppm"],
  "timings": "timing.tsv"
}
```

The Windows platform label is `windows-x64`. Placeholder strings above are
explanations, not valid hashes. `scene_sha256` is the SHA256 of Python
`json.dumps(scene, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()`.
`actual_parameters` must be logged from runtime state, not filled by copying a
requested contract without checking the device/camera. For object/character
scenes, `meshes_checked` must be positive and cover every selected vertex buffer;
submit per-mesh IDs/hashes/audit results in the log. Terrain-only fixtures keep
objects/effects disabled until geometry is audited.

Approved scenes require uint32 gameplay/resource/unit IDs, all fixed ticks/seed,
a finite nondegenerate camera/frustum,16float view and projection matrices,
lighting/fog objects and explicit terrain/bsp/objects/characters/water/effects/
minimap/glow/shadows/terrain_shader Boolean flags. Record actual inherited sampler
states and all other effective rendering options; required draw categories must
remain enabled. The runtime log needs `C6_CAPTURE_COMPLETE` only after successful
readback/timing/file closure; no synthetic success marker in a real capture.
Timing header is `frame<TAB>cpu_draw_present_ms`, followed by at least119 unique
increasing frame IDs and finite nonnegative samples.

```sh
python3 native/client/c6-validation/validate.py --contract APPROVED.json \
  --native NATIVE_DIR --windows WINDOWS_DIR --output NEW_COMPARISON_DIR
```

Exit0 means exact captured RGB matches with review still required; exit1 means
pixel differences; exit2 means invalid/incomplete capture or contract. No state
means C6 completed. The report always sets `c6_completed=false`. A `full-c6`
contract must cover all ten categories; no implicit exclusions are permitted.
Per-category visibility, source instrumentation, integrated world entry,
streaming/portal sequences and an accepted difference report remain separate
milestone evidence. Every differing RGB pixel is listed by the existing C3
comparator without tolerances/masks/alignment/resizing.
