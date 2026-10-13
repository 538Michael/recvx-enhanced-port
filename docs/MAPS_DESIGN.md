# Building new Code: Veronica rooms

Status: 2026-10-04, NTSC-U SLUS-20184. This is an authoring study and the contract for
`pc/tools/rdxbuild.py`. Read [MAPS.md](MAPS.md) for binary layouts and
[ACTORS.md](ACTORS.md) for animated assets. [MAPS_PLANS.md](archive/MAPS_PLANS.md) contains four
original room briefs. All measurements below come from the user's ISO at runtime;
only measurements, source code and original procedural asset recipes are committed.

**What is proven:** all 205 retail rooms survive structural re-layout with equivalent
parsed records, byte-identical ordinary record arrays and preserved model/motion/texture
blobs; scripts retain their bytes. Six disc-free compiler tests pass. **In-game (headless
runs, section 3):** the corridor loads by teleport and through the patched RM_1110 door,
renders, switches cameras both ways, collides with walls/cabinet/crate, its supply is
picked up into the inventory, the zombie walks, grabs and dies, the return door leads to
RM_1110, a revisit keeps the item and enemy flags, the sound alias loads RM_0060's bank,
and the room ran 187 s of game time without errors. The rebuilt RM_1110 renders pixel-identically to the
disc's under identical input, including its door transition. **Not proven:** save/reload,
the map screen in RM_1150 (section 2, likely unsafe), audible playback (the runs use a
dummy audio driver; the bank upload is traced), arena/texture-heap headroom.

The first in-game runs found four compiler/demo bugs that the static checks could not:
the corridor cameras looked out of the room (yaw convention), authored TIM2s had FormatId 0
so the game read their picture header 112 bytes early and exited ("This texture is not
adequate size!!"), the enemy death flag 858 was beyond what `EneSetCk` tests (low 9 bits),
and OBJ import/export mirrored the room (Z reflection). All are fixed; see the sections.

## 1. Design study: rooms as staged spaces

CV's room is a real-time 3D scene with separately authored cameras, collision, interaction
zones, actors and scripts. A photograph-like camera composition does not imply a flat
background or collision derived from the rendered mesh. Room loading is the unit of asset
ownership; the camera cut is the unit of composition. Door records and scripts form the
world graph. There is no automatic neighbour discovery from geometry.

This is a practical model for designing classic fixed-camera RE spaces, grounded here in
CV's actual implementation rather than a claim about every RE engine. Start with a route,
a decision, a reward and the player's view of them. Then author the walls and visuals that
support those decisions. A useful small room offers an intelligible entrance, a visible
reason to enter, an obstructed or risky approach and a readable return route.

### Census methodology and budgets

Run `rdxbuild.py census -o scratch/maps/census.json` with `RECVX_ISO` set. It visits all
205 files, including alternate cases, with no room sampling. P90 is nearest-rank. Counts
are stored records, including inactive/event actors; they are not simultaneous enemies
or visible draw calls. AABB spans below enclose camera areas, including multiple floors;
they are a layout proxy, not navigable area. A room with no areas gets a span of zero.
Strip triangles count `abs(strip_length)-2`, including degenerates and hidden surfaces,
only in the room model, excluding props, items and actors. Vertex counts sum each model's
indexed vertices. Texture counts include stored TIM2s, not deduplicated runtime uploads.

| Per room measurement | Minimum | Median | P90 | Maximum |
|---|---:|---:|---:|---:|
| Camera cuts | 1 | 5 | 9 | 14 |
| Switch areas | 0 | 8 | 18 | 31 |
| Lights | 0 | 14 | 26 | 44 |
| Enemy records | 0 | 3 | 6 | 76 |
| Object records, including 4 reserved | 4 | 9 | 18 | 29 |
| Items | 0 | 2 | 7 | 13 |
| Collision walls | 0 | 49 | 89 | 168 |
| Floor records | 0 | 5 | 16 | 47 |
| Interaction triggers | 0 | 10 | 18 | 27 |
| Entrances | 0 | 2 | 5 | 60 |
| Route boxes | 0 | 7 | 33 | 95 |
| Camera-area X span, units | 0 | 89.16 | 179.91 | 426.99 |
| Camera-area Z span, units | 0 | 95.71 | 160.03 | 324.99 |
| Room-model nodes | 13 | 82 | 120 | 129 |
| Room-model vertices | 362 | 5,347 | 9,652 | 16,404 |
| Room-model strip triangles | 374 | 4,638 | 7,629 | 13,639 |
| Room-model textures | 0 | 43 | 72 | 126 |
| All room TIM2s | 8 | 79 | 122 | 160 |
| Image + CLUT bytes | 1,213,568 | 4,822,400 | 6,558,976 | 7,812,224 |
| Expanded file bytes | 1,563,384 | 6,220,840 | 8,262,480 | 8,931,872 |

There are 1,072 cuts, 1,952 switch areas, 10,406 walls and 1,008,630 room-model strip
triangles across the census. These are not targets to fill. For a first original room,
aim for 2–4 cuts, 10–30 simple colliders, 1–2 enemies, 1–3 items, under 5,000 triangles
and 16–32 textures, then measure the complete loaded scene.

Texture dimensions: 8,522 are 256x256, 3,974 are 128x128, 1,969 are 64x64, 1,383 are
512x512, 848 are 32x32 and 59 are 16x16; ten others are rectangular. Total: 16,765 TIM2s.
At 8 bits, a 256x256 image plus one RGBA CLUT costs 66,560 bytes before headers; a 512x512
costs 263,168. Reserve large textures for surfaces that occupy much of a shot. Repeating
floor/wall materials, an atlas for small props, and small labels usually buy more than
adding unique textures to every object.

### Fixed cameras and switch zones

`cut.c:bhCheckCutArea` takes the first active containing area on the player's floor.
Rectangles use inclusive minima and exclusive maxima; triangles occupy half a rectangle.
The current camera holds until all four probes at ±1 unit have left its area. This gives
hysteresis even where zones do not overlap. In overlapping zones, file order determines
initial selection, while the current cut can persist. Author a modest overlap only when
that hold is desirable; avoid a large overlap swallowing a later cut. The census finds
169 unique cut pairs with positive-area overlap between active rectangular areas on the
same floor, in 93 rooms (per-room median 0, P90 2, maximum 18). Triangle overlaps are
excluded from that count; it is not a measurement of all possible switching ambiguity.

1,048 cuts are active in the file, 24 initially inactive; every cut has type 0. The most
common first-camera flags are 264/0x108 (496), 8 (105), 456/0x1c8 (63), 266/0x10a (63)
and 33032/0x8108 (61). The common scalar FOV is approximately 55 degrees (P90 60 degrees).
A camera's `pers` can instead pack four corner FOVs; the census excludes those and values
outside 0..180 degrees from the scalar FOV sample. Camera height minus its cut's floor
height has median 25.15 units and P90 43.85; negative heights/outliers exist, so this is
not a placement constraint.

Camera angles are 16-bit like actor angles, but a camera's view direction is 180 degrees
minus `ay` (measured on retail cuts and in-game): `ay=32768` looks along +z, 0 along -z,
16384 along +x. The player's `ay` instead faces -z at 0, -x at 16384 (MAPS.md section 8),
so a camera behind a player with `ay=32768` also uses 32768. `ax` pitches down for positive
values. In-game the scene is right-handed: looking along +z, +x is on the screen's left.

For each shot, draw the intended player path and screen-space positions of the exit,
reward and threat. Put the entrance landing and first few steps in one readable shot.
Switch after a landmark is understood, rather than exactly on the item interaction zone.
Use a corner or foreground obstruction to motivate a reveal. Let the player hear a threat
before seeing it only when the escape route is already established. Test both directions
and both sides of each switch boundary. A 2-unit band takes about five healthy walking
frames; boundary chatter is noticeable.

Retail example RM_1110 (the piano room) has four cuts and thirteen areas over roughly
91x82 units, six items and no enemy records. RM_0060 has five cuts, eleven areas and five
enemy records over 60x122 units. The former supports close reward/examination framing;
the latter is a useful circulation/threat reference. These roles are design interpretations
of the records and the existing map investigation, not recovered designer intent.

Camera hide masks address model nodes by index, MSB-first. Split ceilings and foreground
walls into separate mesh groups so a shot can hide them. A visible wall can disappear for
a camera while retaining collision. The compiler clears legacy room-object hide masks
when replacing a preserved room mesh; author new masks after finalizing group order.
Fixed-angle cameras are the first supported authoring target. Retail follow/slide and
interpolated-FOV combinations are preserved, but no friendly follow-mode editor is supplied.

### Scale and motion

`player.c:PlyInfo` gives Claire radius 3.5 and height 16.5 units; the other listed characters
are 17.5 units tall. Treat 10 game units as approximately one metre for art blockouts.
Measured in the corridor (collision on, `RECVX_NOCLIP=1:0` positions): walking head-on the
player stops 3.5 units from the cabinet and crate boxes and 5.5 units from the 2-unit-thick
outer walls. Allow at least 7 units between solid walls for the diameter, with substantial additional
clearance for turning, props and enemy contact. Suggested starting clear widths are 14–18
units for an intentionally tight passage and 24–40 for a combat corridor.

The census tool also reads the SYSTEM.AFS player files' Z-translation blocks using
`dread.c:bhReadPlayerData` and `player.c:bhGetTransZ/PlyTrsZ`. Healthy Claire walking samples
average **0.388889 units/frame** (range 0.358642..0.412146); running averages **1.052631**
(range 1.052629..1.052632); backward motion averages **-0.270000**. At the port's ~29.97
logic frames/s, that is about 11.66 and 31.55 units/s. A clear 100-unit corridor takes about
8.6 s walking or 3.2 s running at steady speed. Startup, turns, injury, water and collisions
change traversal time. Injured Claire's walk/run means are 0.358975/0.769231. Chris's healthy
means are 0.388889/1.054011. These are measured motion samples and inferred travel times,
not a recorded corridor walk.

### Collision and floors versus visible geometry

Walls are explicit boxes, cylinders, triangles, slopes and climbable shapes. Across all
rooms: types 0/1 boxes 4,434/1,945; type 2/3 cylinders 875/1,117; type 4/5 triangles 303/259;
type 6 slopes 156; type 7 climbable boxes 1,317. `attr=0` accounts for 9,202 wall records.
The raw width/depth medians are 6/5 units, but cylinders interpret width as radius and
special/sentinel records can have negative dimensions. Use positive box extents for new
blockouts; decorative detail should not make jagged collision.

`grand[2]` is floor 0, `grand[3]` floor 1. There is no arbitrary walkable triangle mesh.
Floor records primarily describe behaviours: type 1 footstep regions (838), type 0
climb/push regions (355), type 2 enemy regions (325), type 3 effects (52). Visual stairs
alone do not create movement between heights. Use the engine's step/stair triggers and
correct landing floor numbers, or postpone multilevel design. In `"budget": "pc"` rooms the
player also walks on type 6 slope walls: ramps and terrain, section "Slopes" below.

A crate mesh must have a separately authored collision box. A shelf above player height
should not block the floor below it. Leave entrances outside wall volumes, test player
radius clearance, and provide zombie route coverage across the intended chase path.

### Light, fog and materials

Room ambient colours have four slots selected independently for room/characters/objects/
items. This allows brighter readable actors against a darker background. Retail has a
median 14 light records; these include changing, linked and cut-specific lights, not
fourteen equivalent modern point lights. Lights and camera hide masks must be tested
together. Start with ambient lighting, then add a motivated doorway/window light and a
reward accent. Do not use darkness to conceal a broken interaction marker.

Fog is stored at room and camera level. Its distances use the engine's signed camera-Z
convention: the common `fog_nr=0, fog_fr=-100` is not a positive 100-metre distance.
`fog_nr <= fog_fr` disables it in the cut logic. A far negative distance can soften the
background; verify camera-specific overrides in each shot. The blockout uses 0/-1000 and
uniform ambient, with no light records, to simplify the first rendering check.

The new importer emits opaque material chunk 19 (diffuse+ambient), texture selection 8,
vertex chunk 51 and UV strip chunk 66. It generates face normals, independent triangle
strips sharing vertices of equal position and normal, and a negative on-disc bounding
sphere (what a big room costs per vertex, node and strip: MAPS.md 7.5). It does not reproduce retail material
variations, skinning or baked lighting by reimporting OBJ. Preserve the sidecar for an
identical retail round trip.

### Doors, entrances, rewards and threats

The 277 stored type-0 door triggers have median width 7.99 and depth 9 units; P90 is
13.99/19.99. A trigger tests a point in front of the actor, not just the actor origin.
Separate the door's visible slab, blocking wall and action rectangle. Give each target
an entry position that faces into the destination, clear of collision, and a camera that
explains where the player arrived. Maintain return edges explicitly.

`room.c:bhSetDoorDemo` starts the fade/transition. Door type selects MRY.AFS entry 71+type;
255 uses the loading presentation. `attr` bit 31 and `--skip-doors` also skip the animation.
Reusing type 22 uses a retail animation at runtime; no door movie/model is compiled into
the room. The world target is stage/room/position; room case comes from separate script
state. A door patch does not reset that state automatically.

Median two items and three enemy records is a useful scale cue, but the 76-record maximum
includes event actors. Item IDs 12 and 21 each occur 91 times; zombie ID 1 occurs 273 times.
An item record does not implement pickup persistence by itself: item slot, pickup trigger
and script flag must agree. Start with an optional supply placed where the camera makes it
readable, beyond a threat or detour. Use a separate saved flag for each original pickup.
Do not put the item and cut boundary on top of each other. Leave enough room for the pickup
animation, and verify that a nearby enemy cannot permanently trap the player there.

A single zombie around a cabinet can test approach, retreat and aiming while keeping the
exit accessible. The corridor references RM_0060's first zombie model/motions/textures;
its geometry and supply display are original. Building it still requires the disc.
A wholly independent animated creature requires a skeleton/motion/AI authoring pipeline,
which this compiler does not implement.

### Scripts, locks and puzzles

Entry script `scd0` restores persistent state; `scd1` checks ongoing interactions; event
blocks handle longer actions. The demo uses `ItmSetCk/ItmCk` with `it_flg` 506 and
`EneSetCk/DieCk` with `ed_flg` 500; retail scripts use `it_flg` up to 451 and `ed_flg` up to
357. Persistence flags must be below 512: `it_flg` has 512 bits, and `ItmSetCk`/`EneSetCk`
(event.c) mask their flag to 9 bits while `ItmCk`/`DieCk` use 10, so a higher bit is set on
pickup/death but tested as another bit on re-entry (the first corridor used 858, and its
zombie came back). The compiler rejects such flags. Treat these as this example's
allocation, not a global mod registry.

For a locked door: test a saved key/event flag, expose an examine trigger while locked,
and turn it into a door after solving the puzzle. For a simple power puzzle: an examine
trigger starts an event, the event validates the inventory condition, changes a flag and
updates the door/light state; entry script must reproduce that state on revisit. Confirm
both failed and successful interactions and save/reload. The existing disassembler knows
all 213 opcodes, but many operands remain unnamed. This compiler accepts hex blocks with
relocated offset tables, not a complete symbolic assembler or automatic puzzle generator.

## 2. Authoring source and compiler contract

### Source choice

Use a `.blend` as an optional art workspace; the portable build input is **room.json +
triangulated OBJ + opaque PNG**. This keeps the compiler stdlib-only, auditable and usable
without Blender, glTF libraries or downloaded dependencies. Geometry uses OBJ's named
`o`/`g` groups and `tex0`, `tex1`, ... materials. JSON owns all gameplay records and scripts.
An automatic Blender collection/custom-property exporter is a follow-up, not implemented.

Recommended Blender collections: `visual`, `collision`, `camera`, `entrance`, `trigger`,
`item`, `enemy`, `light`. Keep collision boxes and camera empties as editor guides; manually
transfer their values to JSON for now. Bake transforms and triangulate the visual export.
OBJ coordinates are game coordinates (Y up, both right-handed; `rdx.py obj` exports the same
way), so a Blender export with "Y up, -Z forward" shows in-game as in the viewport. Earlier
versions negated z on export/import, which mirrored rooms. JSON uses game axes too. UV v is
inverted on import. Verify an asymmetric test object before authoring a whole room. Use game-sized units in the
interchange; an artist working in metres must multiply lengths by ten before export.

`format` must be `recvx-room-1`. `room`, `cuts`, `walls`, `floors`, `positions`, `lights`,
`triggers`, `items`, `objects`, `enemies`, `routes`, `route_next` use `rdx.py dump` field
names. Unspecified arrays are empty in an authored room, inherited in a preserved room.
`rdx_demo.py` is the runnable complete example and source of defaults. `rdxbuild.zero()`
initializes record fields; there is no need to hand-author unused padding.

| JSON field | Meaning |
|---|---|
| `mesh`, `textures` | OBJ path and ordered PNG paths for room model |
| `gindex` | first unique global texture ID; example uses `0x4d000000` as integer |
| `object_meshes` | one `{mesh,textures,gindex}` per object record after the four reserved slots |
| `item_meshes` | one equivalent descriptor per item record |
| `enemy_assets` | one `{room:"0060",record:0}` dependency per enemy, arbitrary donor records resolved from ISO |
| `scripts` | array of hex blocks: entry, frame, then events; compiler rebuilds offsets |
| `preserved` | local expanded-file sidecar for lossless retail assets/unknown data |

All paths are relative to the JSON. Declared objects/items need distinct IDs within their
class so texture ownership is unambiguous. Enemy dependencies support multiple records and donor rooms, including donor records that
share textures with an earlier record. The resolver replays runtime ownership by
`(id, mdlver, textured-part ordinal)` and emits one motion set per type. ID/model version
must match the donor. Authored combat types currently allowed are zombies, Cerberus,
Hunters, bats, Bandersnatches and anatomist zombies, variant 0 only. Helper-code conflicts,
model versions >=16 and conservative child-slot totals above 112 are rejected.
Other types need encounter-specific support; retail preserved round trips are unaffected. Static
props may instead be baked into room geometry and paired with collision, avoiding an
object slot entirely. New object assets use the `MDL` magic required by `bhSetObjMdl`.

### Re-layout and preserved round trip

The packer writes a fresh header/room block, camera array and area lists, ordinary record
arrays, route matrix and preserved message/event-camera payloads. It aligns top-level asset
sections to 64 bytes and relocates model, motion and texture outer tables; offsets inside
MLBs, motion blobs and scripts retain their own bases. It accepts changes in counts and
section sizes. Asset count changes require matching asset descriptors/tables, rather than
silently letting records bind to the wrong models.

The `preserved.bin` sidecar retains unknown material, animation, message and event-camera
bytes. `decompile` also exports a viewable room OBJ/MTL and room texture PNGs, but does not
automatically enable reimport of them: that would lose hierarchy/material information.
Add `mesh` and `textures` to explicitly replace the visible room model. Unchanged record
bytes are copied, including legacy NaN payloads. Special floats in exported strict JSON
use `{"$float":"nan"}`; the sidecar preserves their actual bits. Retail ROM `evtp` contains
stale editor values and is overwritten from `hed03` by the loader; the packer preserves it.

Messages and event-camera edits are rejected; existing bytes survive relocation. Other
unmodeled metadata survives in the sidecar. Absolute `patches` are rejected by this compiler:
apply them with `rdx.py build` before decompiling. New minimal rooms need no message or
event-camera data. This is a lossless container/record pipeline, not lossless reconstruction
of every retail asset from modern editable meshes.

### Geometry and texture restrictions

The writer makes one hideable node per nonempty OBJ group, with flat sibling links and
baked world transforms. Every triangle gets its own vertices, face normal and strip.
Groups are limited to 666 triangles (2,729 in the PC budget, below); split larger meshes in the editor. UVs must lie in
0..1: the game clamps room textures (every retail room UV is in 0..1, tiny-chunk flags 0x34),
so a UV beyond 1 smears the edge texel into stripes instead of repeating (found in-game on the
Quarantine Annex floors). Tile a material by splitting faces. Material IDs must exist. There is no smooth-normal,
vertex-colour, hierarchy, animation, skin or strip-optimization importer. The procedural
blockout emits both windings to avoid relying on a culling assumption in its first test.

PNG input is the existing tool's RGB/RGBA, 8-bit, non-interlaced subset. Images must be
opaque, powers of two from 8 through 512. Nearest RGB332 quantization deterministically maps to
256 palette entries (maximum rounding errors 18/18/43 in RGB). The compiler creates fresh
TIM2 headers, pixel data and CSM1-ordered CLUTs, with private palettes (`PictFormat=3`).
No proprietary palette or material bytes are used. It terminates authored texture lists.
Header fields follow retail: FormatId 1 (`Send_Tim2_dataEx` reads the picture header at
0x80 only then), OrgColorType 1 (opaque), GsTex1 0x260 (bilinear). `Ps2CheckTextureAlpha`
turns exact-black CLUT entries transparent, so the palette's black is stored as (0,0,1).
Transparency, 4-bit output, better perceptual quantization and shared palette banks remain
unsupported. Alpha is rejected rather than silently discarded.

Runtime limits are 256 texture entries, 10 MiB texture heap, 32 objects including four
reserved, 32 items and 128 enemies. The compiler's conservative authored-room caps are
192 TIM2s, 6 MiB combined TIM2 payloads and 9 MiB expanded file size, leaving headroom for
the player/effects. They are not a guarantee: packed input and persistent assets also occupy
the ~12.25 MiB arena. Check actual free arena/texture entries at runtime before increasing
content. Gindex collisions within authored room assets are rejected; there is no complete
cross-mod or player-asset namespace allocator. Keep original textures in a chosen high-ID
range and coordinate allocations between mods.

### Budgets: retail (default) and PC

`room.json` selects one with `"budget"`. **`retail`** (the default, all rooms built so far) keeps
a room within what the PS2 build's memory holds, as the retail rooms do. **`pc`** uses the
limits the PC build raised (MAPS.md 7.1, `pc/include/pc_prelude.h`): such a room needs this PC
build and does not load in an older build or on a PS2 (an older build overruns the arena
silently). `rdxbuild.py` holds them in `BUDGETS`; record limits that neither budget can raise
are in `RECORD_LIMITS` and checked for both.

| Check | `retail` | `pc` | Engine limit behind it |
|---|---:|---:|---|
| Expanded file | 9 MiB | 44 MiB | room arena: ~10 MB free on arrival (PS2), 74.6 MB (PC) |
| Packed + expanded (`build` checks) | (not checked; retail max 14.1 MB) | 64 MiB | both copies are in the arena while the room loads |
| TIM2s in the room | 192 | 768 | 256 / 1024 texture entries; player, effects and enemies keep the rest |
| TIM2 payload | 6 MiB | 40 MiB | 10 / 64 MB texture heap |
| Triangles per mesh group (node) | 666 | 2,729 | one vertex chunk per node: 5120 / 8192 vertex work-buffer entries, <= 8189 vertices (u16 chunk size); the compiler writes 3 vertices per triangle |
| Mesh groups | 512 | 65,535 | MLB node count u16; camera hide masks cover nodes 0..511 only |
| Cameras / areas per camera | 255 / 255 | same | byte script operands / `ctab_n` u8 |
| Walls | 32,767 | same | `psh_idx` short; scripts and co-op reach 0..255 |
| Triggers, floors | 256 | same | scripts compare the index with a byte |
| Entry positions | 128 | same | `sys->pos_no` signed char |
| Routes | 255 | same | u8 ids, 255 = none |
| Lights / event lights / effects | 256 / 128 / 256 | same | hide masks, `efid[256]` |
| Enemies / objects / items | 128 / 32 / 32 | same | runtime slots |
| Player stands on slopes (type 6 walls) | no (level heights only) | yes: the compiler sets `ROM_WORK.flg` 0x100 | `bhCheckWallEx` has no slope case on the PS2 (MAPS.md 4.3.1) |

The census maxima above (13,639 triangles, 168 walls, 8.9 MB) are what retail rooms use, not
limits; the retail budget's 666-triangle groups and 512 groups are below the PS2's real
5120-vertex/65535-node limits by choice.

**Stress test (2026-10-08, PC budget).** `pc/tools/rdx_bigroom.py OUT [--grid N]` generates a
disc-free room: a 1200x1200-unit floor tiled into 16x16-quad nodes, a 4x4 pillar (wall record
plus 10-triangle box) every 48 units, outer walls, 16 fixed cuts, 300 textures (44 of them
256x256), entries at both corners. Built as `RM_1290` and loaded headless (teleport, warps,
`RECVX_NOCLIP=1:0` positions, `RECVX_SPAWN=999999:1` for the budget line):

| Room | Nodes | Triangles | Walls | Expanded / packed | Arena left after expansion | After load |
|---|---:|---:|---:|---|---|---|
| default (`--grid 224`) | 199 | 106,602 | 629 | 16.4 / 2.0 MB | 60.0 MB | 64.3 MB arena, 676 texture entries, 59.9 MB heap free |
| `--grid 352` | 487 | 254,058 | 629 | 33.1 / 4.8 MB | 43.3 MB | 47.5 MB arena free |

Both rendered (frame dumps), ran to the scripted exit without errors or dropped primitives, and
collided at both ends: walking -z into the pillar at z 22..26 (wall 0) stopped at z 31.5, walking +z
into the one at z 1174..1178 (wall 624) stopped at 1168.5. The bigger room's process had 1.44 GB of
the 4 GB address space mapped (largest free block 988 MB) and 195 MB resident. Compiling the
bigger one takes ~60 s (pure-Python LZ). The cameras are a grid for the test, not composed shots:
the player is not always in view.

### Slopes: how to author ground heights for the player (PC rooms)

The engine has no walkable mesh. An actor's y is a floor level (`room.grand`) or, standing in
a **type 6 slope wall**, the slope's surface. On the PS2 only enemies use slopes; the PC build
lets the player (and zombies) use them in rooms built with `"budget": "pc"`: the compiler sets
`ROM_WORK.flg` bit 0x100 there, and only that bit enables it (retail rooms are unchanged). Engine
side: MAPS.md 4.3.1-4.3.2. Records (in `walls`, `rdx.py dump` field names):

| Field | Slope |
|---|---|
| `flg` | 1 (active) |
| `type` | 6 |
| `id` | direction the surface rises: 0 `+x`, 1 `-z`, 2 `+z`, 3 `-x` |
| `px, pz, w, d` | the box: `px <= x < px + w`, `pz <= z < pz + d` (min edges in, max edges out) |
| `py`, `h` | surface height at the low edge and the rise: `py .. py + h`; `h` must be > 0 (0 means the room's wall height); a flat patch uses `h` 0.001 |
| `flr_no`, `attr`, `prm0..3` | `flr_no`: a floor in `grand[]` (validation); not used by the height test. Leave `attr` and `prm` 0 (enemy code reads wall `prm` values as per-type hints) |

Height at x/z: `py + h * s`, `s` = `(x - px) / w` (id 0), `1 - (z - pz) / d` (1), `(z - pz) / d`
(2), `1 - (x - px) / w` (3); `rdxbuild.slope_height(rec, x, z)` computes the same.
Python helpers in `rdxbuild.py`:

* `slope_wall(x, z, w, d, low, high, rise)` returns the record (`rise` `'+x' '-x' '+z' '-z'`;
  `low == high` gives a flat patch).
* `heightfield_slopes(heights, x0, z0, cell)` turns a grid of corner heights (`heights[row z][col
  x]`, `cell` units apart, corner `[0][0]` at `x0, z0`) into one slope per cell along its steeper
  axis.
* `pc/tools/rdx_slopes.py OUT` builds a complete test room (ramp, plateau, ramp down, hill) whose
  visible mesh is the slope records' own surfaces; `rdxbuild.py build OUT/room.json -o
  mods/RM_1280.RDX`, teleport `1:28:0` or `1:28:1` (hill top).

Rules the engine imposes:

1. **Which slope.** Every frame the player takes, among the slopes whose box holds its x/z and
   whose band `py - ah .. py + h + |ah|` holds its y (`ah` = actor height, ~17), the surface nearest to its
   current y. So boxes may overlap (a bridge over a ramp): the player stays on the surface it is
   nearest to. And it steps onto a slope from any side, from as low as ~17 below the slope's low
   edge, snapping to the surface: fence sides that must not be climbed with ordinary walls.
2. **Outside every slope** the player is on its floor level, `grand[flr_no + 2]`, and the floor
   number follows the height (`flr_no` = highest level `<= y`, recomputed on slopes). A ramp must
   therefore end at a level height or meet another slope; walking off its side or its high end
   onto nothing drops to the level. For continuous terrain, cover the whole walkable ground with
   slopes (flat patches included) and keep it on **one floor**: `grand[2]` = the lowest ground
   (or 0) and `grand[3..]` 0 (unset). Then every height is floor 0 and the cameras' areas,
   triggers and floor records all stay on floor 0. Use more levels only for real storeys, and
   then author the cut areas, triggers and `attr 0x1` walls for each floor the terrain reaches.
3. **Seams.** A slope is planar and constant across its rise, so a twisted terrain patch cannot
   be matched exactly; `heightfield_slopes` keeps the rising axis continuous and leaves a step at
   the other seams of about the dropped cross gradient times the cell size. Measured on the test
   hill (15 units over a 35-unit radius, 10-unit cells): steps up to 4.3 units, 1.2 along its
   centre line; the player crosses them without snagging but visibly pops. Halve the cell to
   halve the step and fit cells to the terrain's steepest direction: a 10 % cross grade over
   8-unit cells steps by up to 0.8 units (8 cm).
   **Check downhill eligibility separately.** Since the merged downhill fix, the upper
   band extends by `fabsf(ah)`: the engine skips when `y + ah < py` or
   `y - fabsf(ah) > py + h`. Previous y 10.1 can therefore cross onto a descending
   patch whose maximum y is 10. Gap-free coverage and small seams still need movement
   checks in both directions, including fallback to `grand[2]`. The RM_1310 authoring
   harness extracts, hashes and compiles the current `pc_slope_ground` function,
   then simulates all surveyed trajectories using compiled room records and C floats
   (see [RE4_VILLAGE.md](RE4_VILLAGE.md), "Published whole-map PC room").
4. **Entrances** inside a slope: the player arrives on the surface under the entry's `py`
   (author `py` on the surface). Items: give them the surface y at their x/z.
5. **Collision is separate.** Slopes do not push. Buildings, fences and cliffs are ordinary
   walls (types 0..5) with `py`/`h` covering the terrain height there (a wall's vertical band
   must include the player's y: `py <= y + ah` and `y <= py + h`).
6. **Budget.** Slopes are wall records (32,767 per room; scripts and co-op address 0..255, so put
   slopes after the walls scripts change). Each one is a loop iteration per actor per frame: a few
   thousand are fine.
7. **Enemies** that move with `bhCheckWall` (most types, MAPS.md 4.3.1) use the PS2
   projection on the same records: they follow the surface but drift slightly along it. Zombies
   move with `bhCheckWallEx` and ignore slopes on the PS2; in PC rooms they get the player's rule
   (not yet tested with enemies in a sloped room).
8. **Shadows** read slope surfaces already (`bhGetGroundPosition`, which divides by `h`: never
   0). The camera's tracking height follows the player's y.

### World wiring, sound and map screen

`rdxbuild link` changes the selected type-0 record, every type-0 `EtcAtariSet2` for that
trigger and `SetDoorCall` instructions matching its previous target. Conditions and
`RoomCaseNo` remain intact; inspect the resulting disassembly. It refuses unclean script
sweeps. A complete puzzle can still intentionally turn the trigger off or into examine,
so this is not an unconditional unlock command.

`mods/RM_1150.SPQ.key` containing decimal `60` makes the room request RM_0060's retail
sound bank. The TARGET_PC hook changes only the room-bank key, with a log line. Missing or
invalid aliases retain previous behaviour. Aliases are not recursive and the donor must
exist in the retail sound index. The alias selects a whole bank, including its enemy sounds;
it does not author new sound effects. Without an alias, an unknown room bank is not found
and old sound-bank state can remain. Verified with `RECVX_TRACE_SND=1`: key 60 is entry 9
of `MULTSPQ1.IDX`; after the alias line the driver receives the bank upload (62 transfer
commands `2c`), without the key file none. Audible playback was not checked (dummy driver).

The map screen is separate authored data: `map.c` loads a floor lookup, models and tags,
not room collision. `MapGetFloorNo` searches labels derived from `(stage*100+room)*100+y`;
`MapTagConnect` searches room tags and may return NULL. The floor search does not establish
that an unknown room has an exact entry, and its result is used without a general missing-
room guard. Thus a new RDX does **not** create a map outline or a supported player marker.
Opening the map in RM_1150 is unverified and potentially unsafe. A floor-table/tag importer
or explicit unknown-room fallback is required before distributing new rooms. No silent
alias to an unrelated map location is installed.

## 3. Usage and verification

Run from the worktree. Keep `scratch/` and `mods/` ignored. Never commit a retail export,
compiled donor-containing room, frame dump or sidecar.

```sh
export RECVX_ISO=/path/to/SLUS-20184.iso
mkdir -p scratch/maps mods
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/linux-i686.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build

# Statistics and structural correctness (add --lz to compress all 205: much slower).
python3 pc/tools/rdxbuild.py census -o scratch/maps/census.json
python3 pc/tools/rdxbuild.py roundtrip --all
python3 -m unittest discover -s pc/tools -p test_rdxbuild.py -v

# Retail source round trip. Keep mesh/textures absent in JSON for lossless assets.
python3 pc/tools/rdxbuild.py decompile 1110 -o scratch/maps/retail-source
python3 pc/tools/rdxbuild.py build scratch/maps/retail-source/room.json -o scratch/maps/retail-roundtrip.RDX

# Original room assets generated by our recipe; zombie resolved only while building.
python3 pc/tools/rdx_demo.py scratch/maps/corridor
python3 pc/tools/rdxbuild.py build scratch/maps/corridor/room.json -o mods/RM_1150.RDX
printf '60\n' > mods/RM_1150.SPQ.key
python3 pc/tools/rdxbuild.py link 1110 0 --to 1:15:0 -o mods/RM_1110.RDX
python3 pc/tools/rdx.py script mods/RM_1150.RDX
python3 pc/tools/rdx.py obj mods/RM_1150.RDX -o scratch/maps/corridor-check.obj
```

### Original blockout: Service Corridor, RM_1150

40x100 units, floor 0, two cameras with switch areas z=0..52 and 48..101. Entry is
(20,0,10), facing +z. The return door is x=14..26,z=0..4, targeting 1/11/entry 0 with
door animation 22. A cabinet at x=22..40,z=45..53 constricts the +x side (screen left from
the entrance); an independent
original crate object is centered at (8,0,65), with a matching collision box. The optional
supply (inventory ID 12, original cube display) is at (12,1,26), trigger x=8..16,z=23..29.
The zombie starts at (28,0,78). Three 64x64 original checker materials plus separate prop
and pickup lists exercise new texture counts. The room itself has 144 triangles; the
crate and item add 24 each. Both cameras sit at x=20, y=38 (z=-12 and 38), `ay=32768`
(looking +z), `ax=6000`; the first hides the door slab (room node 5), which otherwise hides
the player at the return door. This is a technical blockout, not a finished art claim.

### Gameplay acceptance runs (2026-10-04)

Headless, 32-bit build of branch `codex-maps`, `room_test.sh` (new game, `RECVX_TELEPORT`,
offscreen video, dummy audio), user's disc, own save directories. Positions come from
`RECVX_NOCLIP=1:0` (logs every 15 game frames; collision stays **on**). Input times are
presented frames, `RECVX_WARP` times are game frames in the room; on a loaded machine the
room arrives around presented frame 450..500. Interaction probes sit in front of the
player (`hitchk.c`: 2 units for item triggers), so the item warp is z=23, not 20.

```sh
export RECVX_MODS=$PWD/mods
p() { python3 -c "import sys;a,b,s,k,r=sys.argv[1:];print(','.join('%d:%s,%d:%s'%(f,k,f+5,r) for f in range(int(a),int(b),int(s))))" "$@"; }
# Kill the zombie (handgun), then walls -x, +x, cabinet, crate, far wall, walk back to cut 0.
RECVX_INF_HEALTH=1 RECVX_NOCLIP=1:0 RECVX_EQUIP=20:5:60 \
RECVX_WARP=1:15:10:20:30:32768,1:15:800:20:20:16384,1:15:950:30:20:49152,1:15:1100:30:30:32768,1:15:1250:8:50:32768,1:15:1400:20:85:32768,1:15:1550:20:90:0 \
MORE_INPUT="560:r1,$(p 600 1100 24 r1+cross r1),1100:-,1150:up" pc/tools/room_test.sh scratch/maps/collide2 1:15:0 600
# Pickup, return door, RM_1110's patched door back in (revisit).
RECVX_NOCLIP=1:0 RECVX_WARP=1:15:30:12:23:32768,1:15:500:20:5.5:0,1:11:60:66:7.5:49152 \
MORE_INPUT="600:cross,605:-,660:cross,665:-,$(p 760 1060 60 triangle -),$(p 1100 3600 40 cross -)" \
  pc/tools/room_test.sh scratch/maps/pickup4 1:15:0 600
# Kill, leave, come back: the zombie must stay dead.
RECVX_INF_HEALTH=1 RECVX_NOCLIP=1:0 RECVX_EQUIP=20:5:90 RECVX_WARP=1:15:10:20:30:32768 \
MORE_INPUT="560:r1,$(p 600 1300 24 r1+cross r1),1300:-" \
  pc/tools/room_test.sh scratch/maps/enemy4 '1:15:0,900:1:11:0,150:1:15:0' 600
```

| Check | Result | Evidence (local, `scratch/maps/`, not committed) |
|---|---|---|
| Loads via teleport | pass: `mods: .../RM_1150.RDX`, room rendered | every run's `log.txt`, `enemy4/frames/frame_00600.bmp` |
| Loads via the patched RM_1110 door | pass: door animation, `RM_1150` reloaded | `pickup4/log.txt` (warp 66/7.5 then `mods: RM_1150`), `pickup4/frames/frame_01980.bmp` |
| Camera switches | pass both ways: cut 1 after z>52 walking +z, cut 0 after z<48 walking -z | `collide2/frames/frame_01200.bmp` -> `01230.bmp`, `02100.bmp` -> `02130.bmp` (`collide2/switches.png`) |
| Collision | pass: -x wall x=5.5, +x wall 34.5, cabinet front z=41.5 and side x=18.5, crate z=58.5, far wall z=94.5, door wall z=5.5 | `collide2/log.txt`, `collide/log.txt` |
| Supply pickup | pass: "Take the Handgun Bullets? Yes" -> "You've taken the Handgun Bullets." with the inventory panel, cube gone | `pickup2/frames/frame_00660..00690.bmp`, `pickup4/frames/frame_00780.bmp`, `01380.bmp` |
| Zombie | pass: walks to the player, grabs/bites (blood; without `RECVX_INF_HEALTH` the player dies, "YOU DIED"), hp 90 -> -33 with the handgun, dead flag `flg&2`, corpse stays | `walk/frames/frame_00720..01800.bmp`, `pickup4/frames/frame_03480..05580.bmp`, `enemy4/frames/`, `collide/frames/frame_02250.bmp` |
| Return door | pass: animation 22, `mods: .../RM_1110.RDX` | `pickup4/log.txt`, `pickup2/frames/frame_01530..01710.bmp` |
| Revisit flags | pass: `it_flg` 506 keeps the supply gone, `ed_flg` 500 keeps the zombie away (a temporary debug print showed the flags set; with the old flag 858 the zombie returned) | `pickup4/frames/frame_01980.bmp`, `enemy4/frames/frame_01680..02580.bmp`, `enemy2/frames/frame_01680.bmp` (old flag) |
| Sound alias | pass: `mods: room sound key 1150 uses 0060` on each entry; key 60 = `MULTSPQ1.IDX` entry 9; bank upload traced (62 `2c` transfers) vs none without the key; not listened to | `enemy/log.txt`, `nokeyrun/log.txt` |
| Stability | pass: 5610 game frames (187 s of game time) in one visit, 5190 over two visits; no error lines | `collide2/log.txt`, `enemy4/log.txt` |
| RM_1110 rebuild vs vanilla | pass: identical input (walk, warp, door to RM_1020), all 63 dumped frames pixel-identical, identical positions | `van1110/`, `rt1110/`, `rtmods/RM_1110.RDX` |

Not covered: save/reload of the flags, map screen in RM_1150, audible sound, texture/arena
headroom. Build/test logs: `build-claude.log`, `ctest-claude.log` (2/2 pass),
`roundtrip-claude.log` (205 passed), `unit-tests-claude.log`. Codex's earlier sandbox
attempt (`poc1/`, SIGSYS) is superseded.

## Quarantine Annex extension

[AREA_PLAN.md](AREA_PLAN.md) documents six connected original rooms, a saved pickup-flag
lock, two-way retail integration and exact acceptance commands. Run
`python3 pc/tools/build_area.py` with `RECVX_ISO` set from the repository root. The
source recipe is `pc/tools/area/annex.json`; original OBJ/PNG/JSON sources are regenerated
under ignored `mods/quarantine-source/`, while the imported RDXs and sound aliases go
directly into `mods/`. No donor bytes are committed. `--source-only` needs no disc.
Its in-game acceptance (2026-10-05) found four more pipeline bugs, now fixed and checked by
the compiler/validator: enemy `wrk_no` must equal the record index (kill flags), UVs must
stay in 0..1 (textures clamp), cameras must sit inside the shell, and the game drops large
ceiling triangles passing over a camera. See AREA_PLAN.md "In-game acceptance results".
