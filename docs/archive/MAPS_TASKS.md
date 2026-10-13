# Map authoring: refreshed hand-off briefs

Updated 2026-10-04. Read [MAPS_DESIGN.md](../MAPS_DESIGN.md) for measurements, source format,
commands and exact proof status; [MAPS_PLANS.md](MAPS_PLANS.md) for four original designs;
[MAPS.md](../MAPS.md) and [ACTORS.md](../ACTORS.md) for the engine structures.

## Current baseline

* `rdx.py`: existing room edits, script disassembly, OBJ/PNG export and compression.
* `rdxbuild.py`: outer-section re-layout, changing record arrays/camera areas, preserved
  retail source packages, fresh room/object/item geometry, opaque RGB332 TIM2 generation,
  minimal raw-byte scripts, donor enemy dependency, door retargeting and extended census.
* `rdx_demo.py`: original Service Corridor OBJ/PNG/JSON recipe, including separate crate
  and pickup meshes; zombie model/motion/texture dependency resolved from RM_0060 at build.
* All 205 retail rooms passed structural re-layout comparisons. This includes raw ordinary
  record bytes, preserved asset blobs and scripts, not gameplay equivalence. Six synthetic
  compiler tests pass. RM_1110 decompile/build succeeds. 32-bit CMake/Ninja build succeeds.
* `RM_1150` is generated in ignored `mods/`, with a return door to RM_1110. The reciprocal
  RM_1110 patch changes one door record and four script instructions. Optional
  `RM_1150.SPQ.key` selects donor bank 60 using a small TARGET_PC hook.
* **Runtime verified (P0, 2026-10-04)** in headless runs: load by teleport and door, both
  camera switches, collision, pickup, zombie AI/attack/death, return door, revisit flags,
  sound alias bank, 187 s stable; the rebuilt RM_1110 is pixel-identical to the disc's under
  identical input. Results and evidence paths: MAPS_DESIGN section 3. The runs found and
  fixed camera yaw, TIM2 FormatId, the 9-bit `EneSetCk` flag limit and an OBJ mirror.
  Still open: save/reload, map screen, audible sound, texture/arena headroom.

## Common setup and ownership

Work inside your assigned worktree; use its own build, mods, scratch and save directories.
Read the user's ISO via `RECVX_ISO`. Do not commit retail exports, sidecars, compiled rooms
containing donor data or screenshots. `/scratch/`, `/mods/` and `/build*/` are ignored.
Do not kill another agent's processes. Respect concurrent owners of renderer, actor and
co-op files. Any game-code hook should be minimal and guarded by TARGET_PC.

```sh
export RECVX_ISO='/path/to/SLUS-20184 ROM.iso'
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/linux-i686.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
python3 -m unittest discover -s pc/tools -p test_rdxbuild.py -v
python3 pc/tools/rdxbuild.py roundtrip --all
```

## P0. Complete runtime acceptance before extending scope (done except items marked open)

Status: items 1-3 and 5 pass; 4 passes for leave/re-enter (save/reload and headroom logging
**open**); 6 passes for the alias and the traced bank upload (audible check **open**). See
MAPS_DESIGN section 3 for commands, results and evidence.

**Goal:** prove the generated and re-laid-out rooms actually load and play. Requires an
execution environment that can launch the project's 32-bit binary; do not bypass sandbox
restrictions or mistake startup SIGSYS for a room-loader crash.

1. Rebuild the corridor and reciprocal door patch with MAPS_DESIGN's commands. Use fresh
   saves and isolate the mods directory from unrelated mods.
2. Run its four provided input scenarios. Start with the mesh/texture screenshot: check
   fixed-camera angle signs, inward/outward face winding, materials and foreground door.
3. Record switch-before/switch-after frames; positions stopping at walls with collision
   enabled; inventory and item visibility before/after pickup; moving/attacking zombie;
   door animation and RM_1110 load. Run continuously for 60 seconds in the new room.
4. Leave/re-enter and save/reload to prove pickup/death flags. Ensure the original room's
   flags are independent. Log texture entries, texture heap and arena headroom.
5. Install the preserved RM_1110 rebuild in a separate mods directory, compare with a
   vanilla run under identical inputs and confirm its events and door transitions.
6. Verify the sound alias log and actual enemy/footstep sounds. Map UI is a separate task.

**Acceptance:** actual logs and inspected frame paths, with each criterion passed or a
specific reproduction of the failure. Fix the compiler before adding runtime workarounds
for malformed files. Update the proof status in MAPS_DESIGN and this document.

**Pitfalls:** input timestamps count presented frames; warps count room game frames.
`RECVX_NOCLIP=1:0` logs the path while leaving collision on. `room_test.sh` swallows timeout/
process exit, so its zero status is not proof of success. Check the game log and frames.

## P1. Harden compiler validation and asset ownership

**Goal:** repeated item/object IDs and the remaining asset-ownership checks (multiple
enemies from arbitrary donor records are done).

Done (`rdxbuild.py`, used by the Quarantine Annex): `enemy_assets` declares one
`{room, record}` dependency per authored enemy, any record of any retail donor room (donors
loaded once). `enemy_dependency` replays `room.c:bhSetRoom/bhSetEneMdl`: models belong to the
records with flg&1 in order, texture lists to the first owner of `(id, mdlver,
textured-part ordinal)`, the motion set to the first non-empty one of the enemy type, so a
record that does not own its model still resolves. The authored record's id/mdlver must
match the donor's; one motion set per type and one texture list per key are emitted, and
conflicting donors for the same type/key are refused. `validate_enemy_mix` allows types 1,
4, 5, 7, 9, 26 with variant 0, at most one helper-code type, mdlver < 16 and 112 enemy
slots including children; `wrk_no` must equal the record index (kill flags). Fresh
object/item IDs must still be unique within their class. Preserved files retain opaque
sections with their tables.

Still open: first item/object ID texture owner (repeated IDs); effects in upload order.
Keep donor dependencies as IDs, never checked-in blobs. Add cross-asset Gindex
allocation/conflict detection, type-specific model/AI compatibility beyond the allowed list
and script-index validation. Account for packed+expanded file, player buffers, effects and
loaded texture metadata in budgets.

**Acceptance:** two zombies sharing textures, two pickups with the same inventory ID and
one imported static prop all load, behave, survive revisit and release cleanly. Negative
fixtures must reject mismatched asset tables and invalid script references before writing
output. Keep all 205 structural round trips passing.

## P2. Blender exporter and better geometry conversion

**Goal:** author gameplay guides alongside art without manually copying coordinates.

Use the JSON/OBJ/PNG interchange already implemented. Export named collections `visual`,
`collision`, `camera`, `entrance`, `trigger`, `item`, `enemy`, `light`; document custom
properties for floor, target, item ID, saved flag and asset dependency. Bake transforms;
keep Y-up (no axis reflection: OBJ = game axes) and unit scale explicit. Generate cameras from position/look-at
rather than asking artists to enter 16-bit angles. Export area order and camera hide masks
from stable object IDs after final mesh ordering.

Importer improvements: smooth normals, explicit diffuse/ambient settings, node hierarchy,
vertex deduplication, splitting oversized groups and stripification. Existing limits are
666 triangles/group, 512 nodes, opaque materials, no skeleton/animation import.

**Acceptance:** an asymmetric Blender scene with one crate collider, two cameras and a
door exports without manual JSON edits, then matches placement and UVs in-game. Include
negative-axis/transform, floor and both-direction switching checks. OBJ flattening is not
claimed to reproduce all retail material/skin semantics.

## P3. Textures with transparency and higher quality palettes

**Goal:** perceptual 16/256-colour quantization, alpha, shared-bank control and texture
replacement independent of mesh replacement. Current compiler creates opaque private-CLUT
8-bit TIM2s and new sizes/counts; current `rdx.py teximport` still does same-size edits.

Read `ps2_texture.c`, `ps2_dummy.c:Ps2CheckTextureAlpha` and the texture manager before
choosing OrgColorType/PictFormat/alpha conversion. Preserve one TIM2 per texlist entry.
Handle palette ownership deliberately, including multiple PLI entries. Validate dimensions,
GsTex0, payload sizes and global IDs against all assets that coexist.

**Acceptance:** opaque and cutout test charts, 4-bit and 8-bit, nonsquare power-of-two
textures and a resized replacement render correct colours/alpha without altering actors.
Measure actual heap use and round-trip PNG decoding. Never silently flatten input alpha.

## P4. Script assembler and message/event-camera authoring

**Goal:** labels, checked operands and puzzle-friendly source, retaining raw escapes.

Current `scripts` is hex blocks with automatic table offsets; messages/event cameras are
preserved only and edits rejected. Use `rdx.py OP_NAMES/OP_LEN/CTRL_LEN` and the actual
`event.c` handlers. Implement IfelCk/ElseCk labels and controller op lengths; preserve
unknown subcommands. Add symbolic door, item, enemy, flag, event and message commands.
Implement raw glyph/message encoding and event-camera key records, including opaque fields
and any pointer relocation. Add an allocation manifest for saved flags across rooms.

**Acceptance:** assemble/disassemble every retail block byte-exactly; two-state door puzzle
and original text work on entry, failed use, successful use and revisit/save reload. An
extra event-camera shot survives relocation and links to the intended actor. Follow the
Records Annex plan after this gate, not before message support exists.

## P5. Map screen format and unknown-room behavior

**Goal:** make original rooms safe to view in the map and eventually draw their outlines.

Inspect `map.c:bhControlMap`, `MapGetFloorNo`, `MapTagConnect`, map AFS floor index, MDL
geometry, tag/code records and text. An RDX does not define the map UI. The current floor
lookup can choose a neighbouring label for an unknown room, and tag lookup can return NULL;
there is no proven safe missing-room path. Reproduce this in an isolated save before
proposing a guarded PC fallback. Do not map a new room onto a retail room silently.

**Acceptance phase 1:** opening/closing map in RM_1150 is safe and explicitly handles the
missing location. Phase 2: original outline, correct floor/position, doors and pickup
markers, acquired-map flags and room-visited states. Document the binary index contract
and add a source/compiler representation without committing retail map data.

## P6. Sound alias validation and original room banks

**Goal:** validate the implemented `.SPQ.key` alias, then support genuinely new banks.

`pc/src/audio/adx.c:pc_mod_room_sound_key`, called only under TARGET_PC from
`RequestRoomSoundBank`, reads one decimal donor key. It does not recursively alias, replace
an SPQ bank or synthesize audio. Confirm selection, cache behavior and release across
retail -> new -> retail transitions. Cover missing, invalid and nonexistent donor keys.

**Acceptance:** RECVX_TRACE_SND=1 plus audible playback confirms intended banks for zombie,
player steps and doors; no-mod behavior matches baseline. For original audio, choose a
bank authoring format based on SPQ/SPB parsing and license-clean WAV inputs. No binary bank
bytes from the disc in the repository.

## P7. Camera follow modes and vertical navigation

**Goal:** document and export the currently opaque follow/slide combinations, then stairs.

Use `cut.c` flags 0x1/0x2/0x4/0x20/0x40/0x80, corner heights/rolls/FOVs and look-at limits.
Create minimal controlled fixtures instead of copying unrelated retail flags. Test all
area shapes and overlaps with the ±1-unit hysteresis, camera ordering and floor numbers.
For stairs inspect type 1/2 triggers, grand[], player movement animations and floor updates;
visual sloped geometry is not a walkable mesh.

**Acceptance:** measured player/camera trajectories for each supported mode; no boundary
chatter; a two-level blockout traversable up/down repeatedly with correct landing and saved
position. This unlocks the Dispatch Mezzanine plan. Document unsupported flag combinations.

## P8. Room graph and editor-friendly analysis

**Goal:** produce DOT/JSON graph and annotated layout sketches from every retail room.

Extract stored type-0 edges and script `EtcAtariSet2/SetDoorCall`, annotate conditions and
`RoomCaseNo` choices, check entrance indices and reverse edges. All 277 stored door records
must be represented; script-only edges and locks remain distinct. Export camera rectangles/
triangles, collision and entrances to SVG, with coordinate labels and active-floor filters.
No screenshot/mesh data needs to be committed to retain the analysis tool.

**Acceptance:** RM_1110's changing door is correctly represented; missing targets/cases,
unreachable entrances and conflicting room-slot/flag allocations are reported. Use this
to choose safe insertion points for the three room plans beyond the corridor.
