# Village — retail CVX asset reconstruction

Status 2026-10-07: built, validated offline and **accepted in game** (headless runs, section
"In-game acceptance results"); seven problems found there are fixed in the pipeline and the
area source. This is a compact interpretation
of RE4's opening village using only CVX assets, not a scale-accurate recreation. No RE4
files, generated textures, or runtime game-code hooks are used. The source and build stay
in the `codex-area2` clone. All extracted bytes are under ignored `scratch/` and `mods/`.
The existing Annex is unaffected. Do not commit either output directory.

## Design and map

North is +Z. Player height is approximately 17.5 game units. The square is 160×180 units;
the smaller interiors preserve CVX's readable fixed-camera encounter sizes.

```text
                     [125 Church approach] -> church front closes its north end
                                  |
                       tree      D3 north gate
             +--------------------+--------------------+
             | northwest cottage  |  lamp   east barn  |
             |                    |         [124]  D2  |
             | crate     BONFIRE / PYRE           crate|
             |   D1 (house north face)                 |
             | west two-storey house [123]   southeast |
             |                               cottage   |
             +--------------------D0------------------+
                                  |
                      extra graveyard east-wall door
                           RM_0020, trigger 5
```

The west house, east barn, southeast cottage and northwest house stand against the
50-unit fieldstone wall around an open central square. The timber pyre and three looping
retail flame effects establish the central landmark. A north gate leads to a walled path
with graves and trees that ends at a church front (a composed retail masonry facade). CVX's prison roofs/buildings stand in for RE4's rural architecture; the
church is a composed retail masonry/roof silhouette, not RE4's church mesh. House stairs
lead to a blocked decorative loft; only the ground floor is playable. There is no RE4
siege scripting, chainsaw enemy, ladder traversal or bell event.

Zombie villagers: three in the square, one in the house, two in the barn and two on the
path. All are ordinary type 1, model 1, variant 0, from RM_0060 enemy record 0. Their
`wrk_no` equals the record index. The worst room reserves 33 slots including helpers.
The existing compiler's multiple-type dependency and conflict checks remain available;
this area deliberately uses zombies only, so sound bank 60 is correct throughout.

Pickups use handgun ammunition (12) and green herb (21). Pickup flags 480–486 and death
flags 480–487 are separate banks, checked against all 205 retail scripts and below 512.
They do not overlap the Annex's 460–471 allocations or the corridor's 500/506 reservations.
There are no story/key locks and no modifications to retail event flags.

## Assets and authoring

`pc/tools/assets.py` inventories all 205 rooms: 16,765 TIM2 pictures, 2,878 model table
entries, room node metadata, raw model/motion/texture sections, objects/items with their
model and texture-owner indices, and room collision/floor records. `--previews` exports
first-palette PNGs; the `.tm2` and `.bin` files retain **all** palette banks. `--geometry`
exports each room's geometry as OBJ. Collision is a separate retail array, not reliably
owned by a particular visual node; the catalogue preserves it rather than guessing an
object-to-collider association.

It also discovers 85 MPAC door packs in MRY.AFS, exporting the original pack and its model,
texture pack, sound metadata and PMB animation/transition parameters. Inventory display
models are extracted from both ITEM1.AFS and ITEM2.AFS (284 model packets across 288 archive slots); automatic
item-ID fallback uses ITEM1.AFS. The tool reads `dsptbl`, `itemflg` and `itemidtbl` from the disc's
ELF using symbol addresses already in the project, including the game's 131→5 and 136→33
aliases. It maps display assets for IDs 1–144; IDs 145–155 have no native inventory model
in this disc and are recorded as unavailable. Floor donors remain available independently.

The searchable catalogue is `scratch/assets/catalogue.json`. Search numeric IDs/room names
or semantic tags from the small hand-written `pc/tools/area/village_assets.json`.

| Use | Retail source (zero-based room model nodes unless stated) |
|---|---|
| West house | RM_0030 node 23 |
| Barn / southeast cottage | RM_0030 nodes 9 / 22 |
| Northwest cottage | RM_0050 node 38 |
| Church front (path) | RM_0030 nodes 87, 88, 90, grouped and resized |
| Crates, loose planks, bench | RM_0050 nodes 27, 34, 9 |
| Gravestones, iron fences | RM_0020 nodes 3, 55 |
| Trees and lamp | RM_1000 nodes 79, 82, 115 |
| Pyre stones | RM_0030 node 3 |
| Ground / planks / fieldstone on new support geometry | RM_5810 list 0 index 20; RM_0120 index 26; RM_7150 index 29 (seamless tiles) |
| Ammo / herb floor models | RM_0060 item records 0 / 2 |
| Zombie model, motions and textures | RM_0060 enemy record 0 |
| Visible doors and transition animation | MRY.AFS entry 93, door type 22 |
| Flames | Built-in effect 9, type 0; common retail texture atlas 9 |

`kitbash.py` applies node hierarchies, rotations, scales and placements, remaps texture
indices, deduplicates identical TIM2 payloads, and splits output into bounded mesh nodes.
Retail UV edge overshoot is clamped as the renderer does. Original palettes are copied
directly; preview PNGs never pass through a quantizer into the build. Placed solid props
get conservative AABB collision from transformed bounds (only the pyre's ring stones are
decorative). Native material/smoothing distinctions are not fully retained when mesh strips
are baked into the compiler's diffuse/ambient triangles. `rdxbuild.mesh_mlb` emits one
material and one multi-strip chunk per texture and node: the game sets up a draw per chunk,
and the first build (a chunk per triangle) cost the square 4,500 GS flushes a frame (now ~200).

Support boxes (floor slab, shell walls, ceilings, pyre timbers, stairs) use `room_obj`
from the Annex builder, which tiles texture 2 every 16 units and the others every 32 with
UVs in 0..1; the fieldstone is requested first so the planks get the finer grain.

The compiler accepts `retail_textures`, `item_assets`, and `object_assets`. An item reference
is `{"room":"0060","record":2}`; `{}` requests automatic floor-donor discovery with
inventory-model fallback. `{"inventory_item":21}` explicitly requests the inventory model.
Wrong item/model IDs and incompatible shared texture lists are errors. Each item retains
its model table entry, while repeated IDs share one texture upload, matching `room.c`.
Inventory fallback preserves the inventory model's own origin/scale; unusual items need
placement/orientation review in game. No invented fallback cube is emitted.

An object reference `{"door":22}` imports the retail door MLB and texture pack. All village
doors share object ID 900, type 0 (static object dispatch), and share the texture upload.
The trigger's animation type 22 makes the game load the corresponding retail door demo.
The in-room slab is static; the transition screen supplies the animation.

## Build and verify

From this clone's root:

```sh
export RECVX_ISO=/path/to/SLUS-20184.iso
export RECVX_MODS="$PWD/mods"
python3 pc/tools/assets.py inventory --previews --geometry
python3 pc/tools/assets.py search wood
python3 pc/tools/assets.py item 21
python3 pc/tools/build_village.py
python3 pc/tools/village_validate.py
python3 -m unittest discover -s pc/tools -p 'test_*.py'
python3 pc/tools/rdxbuild.py roundtrip --all
```

The inventory is optional for subsequent builds: `build_village.py` resolves references
directly from the ISO. Its one command regenerates all four rooms and the graveyard patch,
checks flags and structure, checks compression by byte-identical expansion, and publishes
to `mods/` only after all checks pass. Each file replacement is atomic; the set of five
files is not a transactional update. Stop the game while rebuilding.

Outputs: `RM_1220.RDX` through `RM_1250.RDX` in increments of ten, `RM_0020.RDX`, four
`.SPQ.key` files containing `60`, and `village-validation.json`. Editable generated room
JSON/OBJ files and source placement bounds are under `scratch/village/`.
Uninstall by removing those five RDX files and four SPQ overrides from this clone's mods.

Checks performed:

- 205/205 retail rooms passed (again after the in-game fixes) semantic and raw-record/model/motion/texture/script round trip.
- 23/23 disc-free tests passed (one added for the per-texture strip chunks); they cover native item ownership, repeated IDs, exact TIM2 preservation,
  item/model mismatch refusal, transforms/deduplication, extraction path confinement,
  unsafe object dispatch rejection and append-only door insertion, alongside the existing tests.
- Catalogue extraction succeeded for every retail room and all discovered door packs.
- Village validation checks connected walking space with measured clearances, entry/enemy/
  item placement, reachable interaction probes, reciprocal doors, camera coverage and
  feet/head framing for any camera yaw, occlusion of the head by props, support boxes and
  the flame volume, the shell rule (camera inside the shell, below its top, pitched down at
  least half the vertical view), 35 units between cameras and flames, texture/model limits
  and enemy budgets.
- Authored rooms retain a 3 MiB packed+expanded arena allowance and the conservative
  192-texture / 6 MiB payload limits. Exact counts and sizes are in the validation JSON.
- Graveyard original records remain byte-identical prefixes; original motion and script
  bytes remain unchanged. Its new model/texture entry is inserted before the item assets
  without changing any retail object/item record indices or existing door destinations.

The graveyard patch adds 84,016 expanded bytes (build check: at most 128 KiB). Measured in
game with `RECVX_SPAWN_SURVEY` on arrival (same settings, patched vs disc file):

| Room | Texture entries free | Heap free | Room arena free (screens copy) | Enemy slots in use |
|---|---:|---:|---:|---:|
| RM_0020 disc | 875 | 58,894 KB | 8,797 KB (3,732 KB) | 37 |
| RM_0020 patched | 872 | 58,821 KB | 8,787 KB (3,732 KB) | 37 |
| RM_1220 | 918 | 60,899 KB | 9,064 KB (2,632 KB) | 22 |
| RM_1230 | 955 | 63,302 KB | 9,453 KB (230 KB) | 8 |
| RM_1240 | 955 | 63,350 KB | 9,307 KB (182 KB) | 15 |
| RM_1250 | 948 | 62,845 KB | 9,547 KB (752 KB) | 15 |

The door costs the graveyard 3 texture entries, 73 KB heap and 10 KB of arena; the item,
file and map screens' texture copy still has 5 MB to spare. 53 of 58 spawner types fit in
every room (the five refusals are the spawner's own rules). Evidence: `scratch/vt/survey-*`.

## Camera plan

Every room is a closed shell: 50-unit fieldstone walls outdoors, walls up to the ceiling
indoors (house 30, barn 32). Every camera sits inside the shell below its top and looks
down at least half the vertical view (`pers` 13000, 71° horizontal at 4:3), so no shot can
reach above the shell: no void, also in widescreen (hor+ only widens the sides). The first
build put the square and path cameras 30 units outside the south wall, 60 up, over 25-unit
walls; interiors showed the void through the gap between 25-unit walls and the ceiling.

Cuts are explicit in `SPECS`: areas plus a camera position and the ground point it looks
at (`camera()`). They were chosen by a coverage search (feet and head of every walkable
1-unit sample in view, head not hidden by any prop, support box or the flames) and are
checked by `village_validate.py`. Retail-style hide masks remain for props that would still
hide the player; the chosen cameras need none.

| Room | Cut areas (x0,z0,x1,z1) | Camera (x,y,z) → ground point |
|---|---|---|
| 1220 | 56,0,120,60 · 56,52,120,82 · 96,54,160,112 · 0,70,40,116 · 34,70,66,116 · 40,108,160,180 | (123,46,87)→(86,43) · (75,46,27)→(89,77) · (75,46,51)→(110,79) · (3,46,117)→(26,91) · (21,46,93)→(51,93) · (39,46,81)→(74,131) |
| 1230 | 0,0,64,44 · 0,36,64,80 | (57,28,75)→(38,35) · (57,28,3)→(32,42) |
| 1240 | 0,0,72,49 · 0,45,72,90 | (69,30,75)→(47,38) · (63,30,21)→(37,64) |
| 1250 | 0,0,64,50 · 0,42,64,92 · 0,84,64,140 | (3,45,69)→(31,26) · (3,45,21)→(31,66) · (3,45,69)→(30,101) |

The flames are billboards: a camera beside or above the pyre showed mostly fire, so cameras
keep 35 units from them. Fog and background are a dusky brown-grey (near −90, far −320).

## In-game acceptance results (2026-10-07)

Headless runs on branch `codex-area2` (32-bit RelWithDebInfo build, offscreen video, dummy
audio, the user's ISO), one game at a time, new game, case 0, normal Claire, collision on,
enemy options off. Each run's save directory gets the same `recvx.ini`: widescreen (hor+,
640×360 window), native textures and 3D, interpolation and frame blend off, door skip off.
Positions come from the `RECVX_NOCLIP=1:0` log; pickup/kill flags (it/ed 480..487, the
graveyard's ed 1..5), `ev_flg` 104, the current cut and the inventory from a temporary,
uncommitted print beside that log. Evidence is in the ignored `scratch/vt/` (run folders
named below; `run.sh` = `room_test.sh` with the fixed ini, `fight.sh`, `walk.sh`, `play.sh`,
`trace.py`/`legs.py` summarise logs). Contact sheets per room (playthrough, fights, revisits;
gain ×2): `scratch/village-sheets/RM_0020.png`, `RM_1220.png` … `RM_1250.png`.

### Problems found in game and fixed

| Symptom | Cause | Fix |
|---|---|---|
| Square and path shots looked down from outside the room over the walls into the void; Claire hidden behind the south wall | cameras 30 units outside the south wall, 60 up, 25-unit walls | closed shells and cameras inside them, below the shell top, pitched down at least half the view (validator rule); explicit cuts from a coverage search |
| Interiors showed the void above the walls | walls 25 high, ceiling at 30/32 | walls reach the ceiling |
| Floors tiled into a dark waffle, plank floors showed vents | the dirt/plank/stone sources were lightmapped/atlas pieces, not tiles | CVX's own seamless sand, plank and fieldstone tiles |
| Square at 250–480 ms per frame, 4,500 GS flushes | one material + strip chunk per triangle | one chunk per texture and node (~200 flushes) |
| Graveyard return landed inside a retail obstacle, pushed to z=92.5 | (168,88) lies in the blocked strip x≥164, z≤89 | entry at (168,93) |
| Fire filled the west and plaza shots | cameras beside/above the pyre; flames are billboards | flames are occluders, cameras keep 35 units away |
| Zombies pushed Claire into a dead-end strip behind the southeast cottage that no cut covers | 10-unit gap to the south wall | cottage reaches z=6 (the validator had found the same kind of strip behind the west house, north trees and barn) |

### Graveyard (RM_0020)

- **Same as the disc:** with identical input (teleport to entry 0, no input) the patched
  and disc graveyard differ only by rain noise (`g-van` / `g-mod`: no pixel over 24 levels,
  ≤ 844 pixels over 8). The rising-zombie event (`ev_flg` 104, all cutscene shots, five
  zombies) plays the same with the patched file (`ge-van` / `ge-mod`, `g1`). Both retail
  doors still lead to their rooms: trigger 1 to RM_0010, landing (-35.9,52.6) with the disc
  file and the patched one alike (`gd1-van` / `gd1-mod`); trigger 0 to RM_0030 (`gdoors`).
- **New door:** a retail door model stands on the east wall behind the gravestones; after the
  event and after killing all five zombies (ed 1..5 set, `g1`) examining it facing east plays
  the type-22 door demo and loads the square at (80,10) facing north. The square's south door
  returns to the graveyard at (168,93) facing west (first build: 168,88, pushed to 92.5),
  zombies stay dead, the event does not replay; the door works again (`g1`, `play`).
- **Memory:** see the survey table above: 3 texture entries, 73 KB heap, 10 KB arena.
- Not run: walking from the opening cell into the graveyard (teleports from a new game land
  on the entry the courtyard door uses); a second case (RM_0021 is untouched).

### Per room

"Doors" = both directions of each edge in one run (`play`), animation 22, correct file
and landing. "Collision" = walking into walls and props with up held (`w12x0`, `w1220b`,
stops from the position log; expected 5.5 from shell and boxes taller than 12, 3.5 from low
ones). Pickups/enemies/revisit/stability: `f12x0` (handgun, 1,400 frames of fire, pickups at
the validation witnesses, then another room and back). Sound: every visit logged
`mods: room sound key 12x0 uses 0060` and the bank upload (20–22 `2c` transfers); dummy audio,
nothing was listened to.

| Room | Teleport | Doors | Cameras | Collision | Pickups | Enemies | Stable | Result |
|---|---|---|---|---|---|---|---|---|
| 1220 Square | pass | S, W house, E barn, N gate: pass | 6 cuts, inside, no void, no hidden player (final cuts checked offline and in `play`) | houses/cottage/barn 5.5; pyre 5.5; crates 3.5; fences 5.5; lamp, trees 5.5 | ammo (it 480) and herb (481) taken, absent on revisit | 3 walk, attack, die (ed 480–482); dead on revisit | 77 s + 41 s | **pass** |
| 1230 House | pass | S pass | 2 cuts | shell 5.5; stairs 3.5; crate 3.5; bench 3.5 | ammo (482) + herb (483), absent on revisit | 1 attacks, dies (483), gone on revisit | 77 s + 260 s | **pass** |
| 1240 Barn | pass | S pass | 2 cuts | shell 5.5; crates 3.5; planks 3.5 | ammo (484) + herb (485), absent on revisit | both attack; one died (485) and stays dead; the other was knocked down, alive, and attacked again on revisit (correct) | 77 s + 350 s | **pass** |
| 1250 Church approach | pass | S pass | 3 cuts; church front closes the north shot | graves 3.5; trees 5.5; shell 5.5 (the walk toward the church front ran out of frames at z=114.6) | herb (486), absent on revisit | 2 die (486, 487), gone on revisit | 77 s + 323 s | **pass** |

Textures are the retail TIM2s with their own palettes: no stripes, holes or black screen
(native textures; the same files feed the emulated path). The pyre's three flames animate
(changing shapes from frame to frame). Item models on the floor are the retail ammo box and
herb; the pickup prompt shows "Take the Handgun Bullets?" and the herb with their inventory
models. Zombies pushed Claire up to 2 units past the walking clearance (corners at 3.5).

### Playthrough

`play`: new game → graveyard → new door → square (every cut) → house (both cuts) → square
→ barn → square → church approach (all cuts) → square → graveyard, nine loads in one run
without an error; arrivals (80,10), (32,10), (32,76), (36,10), (128,98), (32,10), (80,170),
(168,93), each facing into its room. `play-v1` is the same graph on the build before the
last camera change.

### Still open

- Save/reload of flags 480–487; the map screen (no floor/tag data; never opened); options.
- Audible sound; alternative characters, co-op, late case of the graveyard.
- Walking from the opening cell into the graveyard instead of teleporting to its entry.
- Cameras are high (y 46, 28–30 indoors) and wide; framing is functional rather than cinematic.

### Re-running

```sh
export RECVX_ISO=/path/to/SLUS-20184.iso
python3 pc/tools/build_village.py
# scratch/vt/run.sh OUT STAGE:ROOM:POS SECS  (room_test.sh + fixed recvx.ini, RECVX_MODS=mods)
RECVX_INF_HEALTH=1 RECVX_NOCLIP=1:0 \
RECVX_WARP=0:2:90:171:93:49152,1:22:120:80:6:0 \
MORE_INPUT=$(python3 -c "print(','.join('%d:cross,%d:-'%(f,f+4) for f in range(520,6000,30)))") \
  scratch/vt/run.sh scratch/vt/door 0:2:0 400      # graveyard -> square -> graveyard
```

## Final room IDs, teleports and doors

Case is 0 throughout. F2 fields are decimal stage / room / entry. Use
`RECVX_TELEPORT=1:stage:room:entry`; the leading 1 is the trigger frame, not part of the ID.

| File | Stage/room/entry | Name and contents | RECVX_TELEPORT | F2 |
|---|---|---|---|---|
| RM_1220.RDX | 1/22/0 (also entries 1–3) | Village square; houses, pyre/flames, 3 zombies, ammo + herb | `1:1:22:0` | `1/22/0` |
| RM_1230.RDX | 1/23/0 | House ground floor; blocked loft, 1 zombie, ammo + herb | `1:1:23:0` | `1/23/0` |
| RM_1240.RDX | 1/24/0 | Barn; crates and beams, 2 zombies, ammo + herb | `1:1:24:0` | `1/24/0` |
| RM_1250.RDX | 1/25/0 | Church approach; graves, trees, church front, 2 zombies, herb | `1:1:25:0` | `1/25/0` |
| RM_0020.RDX (patched retail) | 0/2/2 (new entry) | Graveyard; original contents plus extra door | `1:0:2:2` | `0/2/2` |

| Source / trigger | Door position (X,Z) | Destination stage/room/entry | Return landing (X,Z) | Animation |
|---|---|---|---|---|
| Graveyard / **5 new** | 178,88 (examine from 171,93 facing east) | 1/22/0 | Square 80,10 | 22 |
| Square / 0 south | 80,0 | 0/2/2 | Graveyard 168,93 | 22 |
| Square / 1 west house | 32,66 | 1/23/0 | House 32,10 | 22 |
| House / 0 south | 32,0 | 1/22/1 | Square 32,76 | 22 |
| Square / 2 east barn | 128,108 | 1/24/0 | Barn 36,10 | 22 |
| Barn / 0 south | 36,0 | 1/22/2 | Square 128,98 | 22 |
| Square / 3 north | 80,180 | 1/25/0 | Path 32,10 | 22 |
| Path / 0 south | 32,0 | 1/22/3 | Square 80,170 | 22 |

Every row was verified in game (`play`, `g1`): the listed file loaded and the player landed
at the listed position facing into the room. Door witnesses (stand here, face, press Cross):
square S (80,6) facing 0, house door (32,72) 0, barn door (128,102) 32768, north gate
(80,174) 32768; inside each room (x,6) facing 0 at its south door.

From the beginning of the game: reach the graveyard (Claire's way out of the prison yard;
for testing, teleport `1:0:2:0` after starting a new game), then use the **new door on the
east wall at (178,88)**, behind the gravestones, before or after the rising-zombie event.
Neither existing graveyard door is changed.
