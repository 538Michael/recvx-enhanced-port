# Maps: how rooms are stored, loaded and connected, and how to add our own

Reference for room ("map") modding. Companion documents: `docs/ACTORS.md` (models, skeletons,
motions, enemy records and spawning, randomizer notes: not repeated here), `docs/ENEMIES.md`
(which scripts show, hide and keep enemies, kill flags, budgets, enemy census), `docs/archive/MAPS_TASKS.md`
(ready-to-hand-off task briefs), `docs/PORT.md` (build, test switches).

Paths are relative to `decomp/src/ps2/veronica/prog/` unless noted; types are in
`decomp/include/ps2/veronica/prog/types.h` (`RDT_WORK` in `system.h`).

Evidence tags (as in ACTORS.md):

* **[code]** read from the decompiled source (file:function).
* **[data]** checked against all 205 room files of the user's disc with `pc/tools/rdx.py`
  (census, round trips, decoding output inspected).
* **[verified]** exercised in the running port, headless (see "Verification log").
* **[inferred]** concluded from code or data, not run.

Nothing in this document or in the tools is disc content beyond counts, offsets of the format
and short operand examples; the tools read the user's own image at run time.

## Summary

For the new authoring compiler, measured design study, original room plans and current
verification limits, see [MAPS_DESIGN.md](MAPS_DESIGN.md), [MAPS_PLANS.md](archive/MAPS_PLANS.md)
and the refreshed [MAPS_TASKS.md](archive/MAPS_TASKS.md). The older recipes below describe the
original tooling baseline; section re-layout and opaque OBJ/PNG import now have a tested
structural implementation, while gameplay acceptance remains pending.

* A room is one file `RM_SRRC.RDX` (S = stage 0..9, RR = room 00..99, C = "case" variant 0..9),
  stored LZ-compressed in `RDX_LNK.AFS` (205 files). The game opens it by name; the name ->
  AFS index table is `ps2_dvd_image.c rdx_files[]` [code][data].
* The expanded file is one relocatable blob: a 0x80-byte header with five section offsets,
  a fixed 1004-byte room block (`ROM_WORK`) whose first 16 words are offsets of fixed-size
  record arrays (cameras, lights, enemies, objects, items, effects, walls, triggers, floors,
  entry positions, enemy routes, event cameras, messages, event lights), then model, motion,
  script and texture sections [code][data].
* Everything a room *does* beyond static geometry and records is in its **event script**
  (bytecode, 213 opcodes; `event.c bhScenarioJmpT`). Scripts rewrite trigger records every
  frame (doors, item pickups), hide/show models, start events and cutscenes, set story flags.
  `pc/tools/rdx.py script` disassembles all 205 rooms cleanly [data].
* Doors are trigger records (`etc` type 0: target stage/room/entry position/door animation)
  or script calls (`SetDoorCall`); arrival uses the target room's entry position list [code][verified].
* The PC port now has a **mods folder**: `$RECVX_MODS` (default `./mods`) files replace disc
  files without touching the image, and new room names (new slots) load too [verified].
* Proof of concept [verified]: an edited room (item moved with its pickup trigger, door
  retargeted in record *and* script) and a cloned room in a new slot `RM_1150.RDX` with a
  replaced texture, both loaded and played headless.

## 1. Files on the disc

### Room files [code][data]

* `RDX_LNK.AFS` (588 MB): AFS archive, `"AFS\0"`, `u32 count` (205), then `count` x
  (`u32 offset`, `u32 size`) from the archive start; data 2048-aligned from 0x80000; no name
  table. Entry *i* is `rdx_files[i]` (`ps2_dvd_image.c`, sorted names).
* Name: `system.c bhSysCallMonitor` mode 4 builds `rm_%1d%02d%1d.rdx` from `sys->stg_no`,
  `sys->rom_no`, `sys->rcase`. `gdlib.c GetFileSize` -> `ps2_sg_gd.c gdFsGetDirInfo` looks the
  upper-cased name up in a directory built at boot from the AFS TOC; `adxwrap.c
  OpenAfsIsoFile` maps the name to its `rdx_files[]` index and opens AFS partition 17 entry
  *i* (`ADXF_OpenAfs(17, i)`); an unknown name prints `not found` and **hangs** (`while (TRUE)`)
  on the original; the port's mods hook (section 9) runs first.
* Cases: several files of one room differ by the case digit (e.g. `RM_1000/1001/1002`): story
  variants. The case is `sys->rcase`, set by script op 55 `RoomCaseNo` (or op 8 `Sv` 21)
  before a transition and **not reset** by doors [code]. A door into a room whose
  `rm_SRRC` file does not exist for the current case leaves the loader waiting
  (`GetFileSize` = 0) [code, inferred effect].
* Stages: 0..9 (`stg_no`). On the two-disc versions `ss_flg & 1` selects disc 2 for `stg_no >= 6`;
  the PS2 (one DVD) skips that check when `ss_flg & 0x400000` is set [code; that the PS2 build always sets it: inferred].

### Compression (`expand.c Expand`) [code][data]

LZ with 8-bit descriptor fields read LSB first; a field byte is fetched when its first bit is
needed (the very first one before any data):

| Bits | Meaning |
|---|---|
| `1` | copy one literal byte |
| `0 0 a b` + byte `o` | copy `2a + b + 2` bytes (2..5) from `o - 256` back |
| `0 1` + u16 `w` (LE) | `w = 0`: end; else offset `(w >> 3) - 8192`, length `(w & 7) + 2` (3..9), or if `w & 7 = 0` a further byte `n`: length `n + 1` |

Copies may overlap (run-length). `rdx.py` has `expand()` and a greedy `compress()`; repacking a
retail room gives a file 4..6% larger than the original (RM_0060, RM_0130, RM_1110), and
`expand(compress(x)) == x` for every tested room [data].

### Other per-room data outside the room file

| What | Where | Key | Notes |
|---|---|---|---|
| Room sound bank | `MULTSPQ1/2.AFS` (partition 2) | `stage*1000 + room*10 + case` looked up in `MULTSPQ?.IDX` (u16 list, index = AFS entry) | `sdfunc.c RequestRoomSoundBank/LoadSoundPackFile`. A key that is not in the list loads nothing: the previous banks stay (wrong/missing room sounds, no hang) [code]. Case for the key: script op 182 `RoomSoundCase` |
| Door animation | `MRY.AFS` (partition 5, `sys->dor_partid`) entry `71 + door type` ("MPAC" pack: door model, motion, ...) | door type = `prm3` of the door trigger | `door.c bhReadDoorData`; type 255 or door attr bit 31 = no animation (load screen) |
| Door sound | `DOOR_%03u.SPQ` key `0x8000 | type` | | `sdfunc.c RequestDoorSoundBank` |
| Map screen | `MRY.AFS` entry `ply_id` (index pack: marker models, textures, messages, room -> map table), entries `5 + stage*6 + floor` (map models) | rooms by `stage*100 + room` (`map.c MapGetFloorNo`) | a room not in the table has no map; opening the map there is untested [inferred] |
| Music | script ops (`BgmOn2`, `SetNextRoomBgm`, ...) | | `BGM1/2.AFS` |
| View clip / clip volume / background colour per camera | tables in the executable (`event.c bhChangeViewClip*/bhChangeClipVolume*`, `ps2_event.c bhChangeBackColor*`) | stage/room/case/cut | PS2 tuning; a new room number gets the defaults [code] |

Other code that tests `stg_no`/`rom_no` directly (special cases a clone at a new number will not
get, an edited room keeps): `system.c` (ice/albinoid/render-texture flags for 9/5, 9/6, 7/10, 3/6,
1/5), `objitm.c`, `sub1.c`, `effsub1.c`, `effsub1b.c`, `en01sub.c`, `screen.c`, `event.c` [code].

## 2. Load path [code][verified]

`system.c bhSysCallMonitor` mode 4 (after a door or teleport; ACTORS.md section 1 has the memory
side):

1. case 0: `GetFileSize(name)`; `bhInitReadRDT` (clears camera, enemies, room block,
   effects); the packed file is read to `sys->rdtp = ALIGN_DOWN(endp - size, 64)` (top of the
   arena). Per-room flags for a few hard-coded rooms are set here.
2. case 1: `Expand(rdtp, memp)` -> `sys->rdtsz`; `sys->rdtp = memp` (expanded room at the
   bottom; for big rooms the output reaches the packed copy while it is still being read:
   RM_9350 is 8.6 MB expanded + 5.5 MB packed in a ~11 MB space [data, inferred overlap]).
3. case 2: `memp += rdtsz`; `bhSetRDT` -> `room.c bhSetRoom` (below). Version check: the float at
   offset 0 must be >= 1.72 (retail: 10.0) or the "RDT version error" screen appears (case 20).
4. case 3: one texture list per frame (`bhSetMemPvpTexture`), then `room.c bhFinishRoom`.
5. case 10: `bhInitEvent` runs script `scd0` once, room sound bank requested.

`room.c bhSetRoom`:

* copies 1004 bytes from `hed00` into the global `rom` (`ROM_WORK`) and adds the file base to
  its first 16 words (the section pointers); each camera's area pointer `cuttp` too;
  `rom->grand[31] = 0`.
* realizes the room model (`hed01` entry 0), every enemy record with `flg & 1` (`bhSetEneMdl`,
  ACTORS.md), objects 4.. (`bhSetObjMdl`), items (`bhSetItmMdl` -> `objitm.c bhSetItem`, slot =
  record index).
* `rom->evtp = file + hed03` (the `evtp` word in the room block is ignored).
* builds the texture upload list from `hed04` (room, texture-owning enemy models, objects
  (shared per object id), items (shared per item id), effects 30..99/400+).
* ends with `sys->memp = start of the texture section`: everything after `hed04` is scratch
  after the uploads, so **the texture section must be last** in the file.

`bhFinishRoom`: enemy motion sets (`hed02`), room motion set, mouth/lip data, `O_WORK` arrays,
lights reset, effects placed, `bhResetAtariAttr`, player placed at `rom->posp[sys->pos_no]`
(y from the floor table, see 4.3) unless a save/continue restores the position,
`bhInitRoomChangePlayer`.

## 3. Room file layout

All offsets are from the start of the expanded file; all numbers little-endian.

### 3.1 Header (`system.h RDT_WORK`, 0x80 bytes) [code][data]

| Offset | Field | Content |
|---|---|---|
| 0x00 | `float ver` | 10.0 in all retail rooms [data] (>= 1.72 required) |
| 0x04..0x0C | `flg, dat0, dat1` | 0 in all rooms [data] |
| 0x10 | `hed00` | room block (`ROM_WORK`, 1004 bytes); 0x80 in all rooms |
| 0x14 | `hed01` | model offset table |
| 0x18 | `hed02` | motion offset table |
| 0x1C | `hed03` | event script block |
| 0x20 | `hed04` | texture table (last section) |
| 0x24..0x5C | `hed05..hed19` | 0 in all rooms [data] |
| 0x60 | `usrname[32]` | author tag string (e.g. a designer's name) |

### 3.2 Room block (`ROM_WORK`, 0x3EC bytes at `hed00`) [code][data]

| Offset | Field | Meaning |
|---|---|---|
| 0x00..0x3C | 16 section offsets | `cutp lgtp enep objp itmp effp walp etcp flrp posp rutp ruttp evtp evcp mesp evlp` (0 = none; `evtp` unused, see `hed03`) |
| 0x40..0x7C | `dmp00..15` | 0 |
| 0x80..0xB4 | counts | `cut lgt ene obj itm eff wal etc flr pos rut evc mes evl` (int each) |
| 0xB8..0xFC | `dmy00..17` | 0 |
| 0x100 | `flg` | room flags, 0..3 in retail: bit 1 = positional sounds are muffled behind walls (`sdfunc.c CheckCollision4Sound`); bit 0 is compared with `ss_flg & 2` at room entry and then all textures are pushed (`system.c` mode 4 case 11; meaning unclear) |
| 0x104 | `bak_col` | background colour 0x00RRGGBB |
| 0x108 | `fog_col`, 0x10C `fog_nr`, 0x110 `fog_fr` | room fog (cameras carry their own, 4.1); `fog_nr <= fog_fr` = off (`cut.c bhCheckCut`) |
| 0x114 | `w`, 0x118 `h`, 0x11C `d` | `h` = default height of walls whose own `h` is 0 (`hitchk.c`) |
| 0x120 | `grand[32]` | floor heights, see 4.3 |
| 0x1A0 | `mdl` (`ML_WORK`) | runtime: the realized room model |
| 0x1B8 | `fog[128]` | 0 in all rooms [data] |
| 0x3B8 | `amb_rom, amb_chr, amb_obj, amb_itm` | index 0..3 into the ambient colours below, per draw class (`game.c`) |
| 0x3BC | `amb_r[4], amb_g[4], amb_b[4]` | ambient light colours |

In all rooms the record arrays lie between the room block and `hed01`, in the order `cutp lgtp
enep objp itmp effp walp etcp flrp posp rutp ruttp evcp mesp evlp` (absent ones skipped), camera
areas after the cameras [data]; `hed00 < hed01 < hed02 < hed03 < hed04` [data]. The game only
uses the offsets, so a tool must too. Record sizes:

| Array | Struct | Size | JSON key in `rdx.py dump` |
|---|---|---|---|
| `cutp` | `CUT_WORK` (+ `CUT_WRK` areas elsewhere) | 0x2A8 | `cuts` (`areas` inside) |
| `lgtp`, `evlp` | `LGT_WORK` | 0xE0 | `lights`, `event_lights` |
| `enep`, `objp`, `itmp` | `ETTY_WORK` | 0x24 | `enemies`, `objects`, `items` |
| `effp` | `EF_WRK` | 0x44 | `effects` |
| `walp`, `etcp`, `flrp`, `rutp` | `ATR_WORK` | 0x24 | `walls`, `triggers`, `floors`, `routes` |
| `posp` | `POS` | 0x10 | `positions` |
| `evcp` | `EVC_WORK` | 0x808 | `event_cameras` |
| `ruttp` | `u8[rut_n][rut_n]` | | `route_next` |
| `mesp` | u32 table + u16 text | | `messages` |

Counts over all rooms [data]: cameras <= 14, lights <= 44, enemy records <= 76, objects <= 29
(incl. 4 reserved), items <= 13, effects <= 195, walls <= 168, triggers <= 27, floors <= 47,
positions <= 60, routes <= 95, event cameras <= 64, messages <= 65, event lights <= 50.
Runtime caps: 32 objects, 32 items (`sys->obwp/itwp`), 128 enemies, 512 room-model nodes per
camera hide mask (16 words), lights 4.. hideable per camera (8 words) [code].

### 3.3 Section tables

* `hed01` model table [code][data]: u32 offsets: room model, then one per enemy record with
  `flg & 1` (an enemy model list: `u32 size, data` chunks, `"MASK"`/`"SKIN"`/MLB, ended by
  `-1`, ACTORS.md), one per object record 4.. (MLB, or `u32 skin size, u32, SKIN data, MLB`),
  one per item record (MLB).
* `hed02` motion table: one u32 per enemy record (0 = reuse the type's set), then the room
  motion set (player/NPC acting), mouth data (`sys->mspp`), lip data (`sys->lspp`); motion
  sets are `u32 size, MNB` lists ended by `-1` (ACTORS.md).
* `hed03` script block: see 5.
* `hed04` texture table: `u32 n`, then `n` offsets of texture lists, in upload order: room
  model, each texture-owning enemy model (record order, sub-model order, first per
  `(type, mdlver, k)`), each object id (first object of an id), each item id, each effect id
  (30..99, 400+) of the effect records [code]. In 204 of 205 rooms the last list ends with the
  file, without its `-1` terminator (the loader then reads past the data) [data]. The count must
  match exactly: `room.c` reads the lists in this order without skipping, so a list too many
  (e.g. one for a second object of an id) shifts every effect onto its neighbour's textures
  and palette (`assets.py dependencies` checks it; the game logs `room: ... texture table`).

### 3.4 MLB model container and the room model [code][data]

`binfunc.c bhMlbBinRealize`: header `char[3]` + status byte (0x80 = chunk model), u16 header
size, u16 node count, s32 texlist offset, s32 object offset (offsets after the header, -1 =
none). Objects are a flat `NJS_CNK_OBJECT` array (0x34: `evalflags, model, pos[3], ang[3],
scl[3], child, sibling`; pointers are offsets, -1 = NULL), in depth-first order (ACTORS.md
2). A model is `NJS_CNK_MODEL` (`vlist, plist, center[3], r`; `r < 0` on disc = not yet
relocated). The texlist is `NJS_TEXLIST` (`textures, nbTexture`) of `NJS_TEXNAME` (12 bytes).

The room model is one MLB with up to 129 objects [data]; each object with geometry is one hideable
piece (camera masks, 4.1). Chunk types used by **all** room models [data]:

* vertex list: only type **51** (PS2 variant of `CV_VN`, `ps2_NinjaCnk.c njCnkCvVnPs2`):
  `u8 51, u8 flags, u16 size (words after this u32), u16 index offset, u16 count`, 56 unused
  bytes, then per vertex `float x, y, z, 1, nx, ny, nz, 1` (32 bytes); then `0xFF` end.
* poly list: `8` texture id (`CT_TID`, u16: id & 0x1FFF = texlist index), `19` material
  diffuse+ambient (`CM_DA`), rarely 17/18/20/22/23 materials, `64` plain strips (20 uses) and
  **66** `CS_UVH` strips: `u8 66, u8 flags (bit 3 alpha, bit 5 flat), u16 size (2-byte units),
  u16 strip count (& 0x3FFF), then per strip s16 n (negative = reversed winding) and n x
  (u16 vertex index, s16 u, s16 v)` with u/v in 1/1024 (UVH; 65 = 1/256); `0xFF` ends.
* The PS2 renderer supports only vertex chunks 41 and 51 and strip chunks 64..66 (other ids are
  skipped as "default" chunks, `pCnkFuncTbl`). An importer should emit 51 + 8/19/66.

`rdx.py obj` exports the room model (world transforms, strips -> triangles, UVs, one
material per texture id) to OBJ [verified visually: the piano room's walls, windows and floors].

### 3.5 Textures [code][data]

`ps2_texture.c bhSetMemPvpTexture(texlist, data, first)`: a list of chunks, each a 32-byte
header (`u32 code, u32 payload size`, padding) + payload, ended by code -1:

* `"PLI\0"`: palette info: `u32 n`, n x 8 bytes; the second word of each is the CLUT
  placement: byte 0 flags, byte 1 CLUT bank (255 = allocate from `sys->ef_pbnk`), u16 offset.
* `"TIM2"`: one picture with an extended header (`TIM2_PICTUREHEADER_EX`, 0xD0): the TIM2
  file header with `Gindex` (global texture id, sharing key) at 0x08, palette data at 0x14,
  the standard picture header at 0x80 (`TotalSize, ClutSize, ImageSize, HeaderSize,
  ClutColors, PictFormat, MipMaps, ClutType, ImageType, Width, Height, GsTex0...`); pixels at
  0x80 + `HeaderSize`, then one CLUT per PLI entry (32-bit RGBA, alpha 0x80 = opaque).
  `ImageType` 4 (4 bit, 16 colours) or 5 (8 bit, 256 colours, CSM1-ordered CLUT) only; other
  types `exit(0)`. `PictFormat` (0x90) apparently keeps the Dreamcast source format: 5..8 =
  palettized source, the CLUTs go into the shared palette banks named by the PLI entries;
  any other value (the code calls it `isVQ`) = the TIM2 is loaded with its own CLUT. Census of
  all room textures [data]: 8-bit with format 3 (11708), 16 (2413), 1 (2030), 9, 2, 7; 4-bit
  with format 5 (594); all with one mip level.

The k-th TIM2 of a list fills `texlist[first + k]`, so a strip's texture id is the index of its
TIM2 in the room list. Upload copies into the 10 MB texture heap (256 entries, `exit(0)` when
full, ACTORS.md 1). `rdx.py tex` exports all lists as `.tm2` + `.png` [verified visually];
`rdx.py teximport` writes a PNG of the same size into a texture in place (nearest colour of
the existing palette) [verified in game].

## 4. Records

All positions are floats in game units (about 10 cm: the player is ~17 units tall), y up.
Angles are 16 bit (65536 = 360 degrees); the player moves by `px -= sin(ay)`, `pz -= cos(ay)`,
so `ay = 0` faces -z, 16384 -x, 32768 +z, 49152 +x [code][verified with RECVX_WARP].

### 4.1 Cameras: cuts, areas, camera setups [code]

`CUT_WORK` (0x2A8): `flg` (bit 0 = active), `type`, `flr_no`, `ctab_n` (area count),
`cuttp` (offset of `ctab_n` `CUT_WRK`), `cx, cy, cz, cw, ch, cd` (box used to interpolate
"active" cameras), `cam[2]` (`CAM_WRK`, 0xC4 each), `exd[256]`.

Switching (`cut.c bhCheckCut`, every frame): `bhCheckCutArea(player x, z, floor)` returns the
first active cut with an area on the player's floor that contains the point; areas are
boxes `minx <= x < maxx, minz <= z < maxz` (`atr_tp` 0) or right triangles of that box
(`atr_tp` 1..4 = which corner pair forms the hypotenuse). A switch needs all four points at
+-1 unit to have left the current cut (hysteresis). The area's `attr` bit 0x8000 selects
`cam[1]` instead of `cam[0]` (`cam.camver`); other `attr` bits (0x1, 0x8, 0x10, 0x20, 0x40)
clamp the tracking reference to the area (`bhCheckNearAttrOffCutRange`).

`CAM_WRK`: position `px py pz`, angles `ax ay az` (int, 16-bit units), `pers` (field of view as
16-bit angle, e.g. 10012 = 55 degrees), `flg`, tracking parameters, `hidobj[16]` (bit *i* hides
room-model object *i*: `bhCkFlg`, MSB first per word), `hidlgt[8]` (lights 4.. off),
`fog_col, fog_nr, fog_fr`, `fil_no/fil_rt` (screen filter, `flg & 0x8000`). Tracking
(`bhInitActiveCamera`, `bhControlActiveCamera`): non-zero `w`/`d` slide the camera along x/z
with the player inside the cut box (`cx..cx+cw`); `y0..y3` corner heights; `laz0..3` corner
rolls; `lax/lay` limit look-at turning; `flg 0x1` = fixed angles; `flg 0x2` = smooth
transition from the previous camera; `flg 0x20` = `pers` holds four corner FOVs in degrees
(bytes) interpolated over the box; `flg 0x4`, `0x40/0x80` further follow modes;
`am_spd/aa_spd` speeds. Census: all 1072 cuts have `type 0` [data].

Event cameras (`EVC_WORK`, 0x808): `flg, type, nxt_no, keyf_n` + 16 key frames
(`CAM_KEYF_WORK` 0x80: frame, position, angles, pers, hide masks, fog, link to an actor,
...), driven by script op 19 `CamSet`/42 `CamSet2` (`cut.c bhSetEventCamera`).

### 4.2 Walls (`walp`, `ATR_WORK`) [code]

`ATR_WORK` (0x24): `u8 flg` (bit 0 active), `u8 type`, `u8 id`, `s8 flr_no`, `u32 attr`,
`px py pz` (min corner), `w h d` (extent; `h` 0 = `rom->h`), `prm0..3`.
`hitchk.c bhCheckWallEx` (player/enemy movement), `bhCheckWallType*` (probes):

| type | Shape | Notes |
|---|---|---|
| 0, 1 | box `px..px+w` x `pz..pz+d`, height `py..py+h` | pushes out |
| 2, 3 | cylinder at (px, pz), radius `w` | |
| 4, 5 | right-triangle prism of the box, `id` 0..3 = orientation | `bhCheckInnerTriangle` |
| 6 | slope: a surface across the box rising by `h` from `py`, `id` = direction it rises: 0 +x, 1 -z, 2 +z, 3 -x | no push; sets the height of actors with `flg & 0x100` standing in the box, see 4.3. The player's check (`bhCheckWallEx`) has no case 6 on the PS2 [code] |
| 7 | box that can be stepped over / climbed (`pw->flg & 0x100`) | |

Odd types are ignored by actors with `flg & 0x400`. `attr`: 0x1 = only on its own floor,
0x2/0x4 = ignored by some actors/states, 0x10000 + 0x40000..0x200000 = player direction
specific, 0x40 = breakable/special. Census: 4434/1945/875/1117/303/259/156/1317 records of
types 0..7 [data]. Scripts change walls with ops 10 `WalAtariSet` (on/off) and 123
`WalAtariSet2` (attr, prm, type).

### 4.3 Floors and heights [code]

* Height levels: `rom->grand[32]`. `pwksub.c bhSetFloorNum`: an actor's floor number is
  `i - 2` for the highest `i` with `grand[i] != 0` and `py >= grand[i]` (`grand[2]` counts even
  when 0). So `grand[2]` is floor 0, `grand[3]` floor 1, ...; `grand[0..1]` give floors -2/-1.
  On arrival the player's y is set to `grand[flr_no + 2]`. Every record has a `flr_no` and is
  only active on that floor (cut areas, triggers, floors, most walls).
* Floor records (`flrp`, `ATR_WORK`), `hitchk.c bhCheckFloorP` and friends: type 0 = area where
  the player can climb/push (with `attr 0x1`: probe in front; `attr 0x400..0x2000` direction
  limits; sets `sys->flr_idx`); type 1 = footstep sound `prm0` (attr 0x1: only while
  running); type 2 = enemy area (`bhCheckFloorEnemy`); type 3 = effect area: `prm0` 1 water
  (`bhCheckWater`, uses y and h), 3 footstep effect, 4 special. Census: 355/838/325/52 [data].
* There is no walkable-surface mesh: walking is free inside walls, height comes from the floor
  level (stairs/steps are animations, below).

### 4.3.1 How an actor gets its y: levels, stairs, steps, slopes [code][data][verified 2026-10-08]

* **Level.** Every frame the wall check of an actor with `flg & 0x100` (the player: `flg` 0x119,
  `player.c bhSetPlayer`; walking enemies) ends with `y = grand[flr_no + 2]` when it touched no wall
  (`hitchk.c bhCheckWall` and `bhCheckWallEx`, last lines). The floor number itself only changes
  through `bhSetFloorNum` (arrival, end of a stair/step animation) or a slope (below). So on the
  PS2 the player is always at one of the 32 levels, except during animations.
* **Stairs** are not ramps. A type 1 trigger (`etcp`, 4.4) starts `player.c bhCPM2_act_kdu/kdd`
  (`prm0` 0 up / 1 down, `prm1` facing in 90-degree steps, `prm2` steps): the target floor is
  `bhCheckFloorNum(py ± 2 * prm2)` (2 units per step), `yn = grand[that + 2]`, the motion moves the
  player there, then `bhSetFloorNum` and `py = grand[flr_no + 2]`. Type 2 triggers (`dansa`) do the
  same for one step (`bhCPM2_act_dnu/dnd`); `hsu/hsd` are ladders (by the name). Measured in
  RM_1020: cross at the stair foot (trigger 1, 11 steps) takes the player from y 0 floor 0 to
  y 28 floor 2 in ~90 frames.
* **Slopes** are type 6 wall records. Enemies that move with `bhCheckWall` (`en02`, `en05`,
  `en10`, `en12`, `en15`, `en17`, `en19`, `en21`, `en23`, the generic `bhEne00`; also the player
  while `en03/06/12/19` hold it) handle them in its case 6: inside the box (x/z, and y within
  `py - ah .. py + h`) the actor's position is projected onto the slope's centre line
  (`njDistanceP2L`), which sets y and also moves it a little along the slope; the floor number is
  recomputed from y; if it differs from the slope's `flr_no`, y is clamped to that level. The slope
  counts as a hit, so the "no wall: back to the level" rule does not apply. `bhGetGroundPosition`
  (shadows, `plp->gpy`, falling objects) also reads slope surfaces (formula as in 4.3.2).
* **The player ignores slopes.** Normal movement (`player.c bhControlPlayer`) calls `bhCheckWallEx`,
  which has cases 0..5 and 7 but not 6, so standing in a slope box the player stays on its level
  (zombies, `en01`, and `en04`, `en26` move through `bhCheckWallEx` too and ignore slopes as
  well). Census of the 205 retail rooms (`rdx.py`, all walls of type 6): **156 slope records in
  85 room files** (ids 0/1/2/3:
  30/64/23/39; `attr` 0 in 151; no script turns a wall into a slope: the two `WalAtariSet2` in
  retail set type 7). 75 of them lie at a stair or step trigger: they are the enemies' way up the
  stairs the player climbs by animation (RM_1020 slope 32 under stair trigger 1, RM_1100 slope 56,
  ...). Others are ramps in rooms the player walks on the level (RM_1130 six slopes, RM_9280
  two 180-unit ramps). For 32 slopes more than half of the box lies outside every box wall that
  blocks the player (sampled, all floors), so retail rooms cannot be shown to keep slopes out of
  the player's path: changing the player's check for all rooms could change retail, hence the flag.

### 4.3.2 Slopes for the player in PC rooms [code][verified 2026-10-08]

`#ifdef TARGET_PC`, gated by a room flag: `ROM_WORK.flg` bit **0x100** (`pc_prelude.h
PC_ROOM_SLOPES`). Retail rooms have `flg` 0..3 (census: 0 ×80, 1 ×1, 2 ×120, 3 ×4), the game tests
bits 0 and 1 only (`system.c`, `sdfunc.c`). `rdxbuild.py` sets the bit in every `"budget": "pc"`
room (MAPS_DESIGN.md "Slopes"). In such a room:

* `hitchk.c pc_slope_ground`, called at the end of `bhCheckWallEx` for actors with `flg & 0x100`
  (the player's body check; its forward probe runs with 0x100 cleared and is unaffected): among the
  active type 6 walls whose box contains x/z (`px <= x < px + w`, `pz <= z < pz + d`) and whose
  height band holds the actor (`y + ah >= py` as `bhCheckWall` tests, and `y - |ah| <= py + h`: walking
  down onto the next box, last frame's y is above its top), the surface
  `py + h * s` nearest to the current y is taken, with `s` = `u/w` (id 0), `1 - v/d` (1), `v/d` (2),
  `1 - u/w` (3), `u = x - px`, `v = z - pz`. y is set to it (x/z unchanged: no drift as in the
  enemies' projection) and the floor number to `bhCheckFloorNum(y)` (`flr_nob` kept). Outside every
  slope the PS2 rule applies (`grand[flr_no + 2]` when no wall was hit). Enemies moving through
  `bhCheckWallEx` with `flg & 0x100` (zombies) get the same; those using `bhCheckWall` keep the
  PS2 projection. Not run with enemies yet.
* Arrival (`room.c bhFinishRoom`): an entry position inside a slope gets the slope height under
  the entry's authored y instead of the level.
* Camera (`cut.c`): the tracking reference `cam.ply` is `plp->py + h` instead of `grand[flr] + h`.
* Co-op (`coop_glue.c`): the partner's pose carries its y already; its floor number is taken from
  that y (`bhCheckFloorNum`) instead of ours.
* Unchanged and already right [code]: triggers, items and cut areas test x/z and the floor number
  only (`bhCheckExmAtari`, `bhCheckCutArea`); the shadow (`effsub1.c bhEff001`) and `plp->gpy` use
  `bhGetGroundPosition`, which includes slope surfaces (shadow not checked in the frames); the
  third-person camera uses `plp->py`.
* Verified: retail frame dumps identical to the build without the change (new game + RM_1020 with
  the stair climb above: all 97 `frame_*.bmp` and 280 render-target dumps byte-identical; the
  in-between `interp_*` dumps differ between two runs of the same build too). The synthetic room of
  `rdx_slopes.py`: the player walks a 0→10 ramp (y 1.1, 2.5, 4.0, ... 9.8 every 6 units), the
  plateau at 10, the ramp down and a 15-unit heightfield hill up and down (y 0 → 14.4 → 0) without
  stopping; arriving at entry 1 on the hill top puts it at y 14.4.

### 4.4 Triggers (`etcp`, `ATR_WORK`) [code][verified for 0 and 4]

`hitchk.c bhCheckExmAtari` (action button) tests a point in front of the player (distance
depends on the type) against each active trigger box on the player's floor:

| type | Use | Parameters |
|---|---|---|
| 0 | **door** | `prm0` stage, `prm1` room, `prm2` entry position, `prm3` door animation type; `attr` -> `sys->door.flg` (bit 31 no animation, bits 7..8 case of the animation, 0x400/0x800/0x1000/0x2000 = not usable while facing +z/-x/-z/+x) |
| 1 | stairs (kaidan) | `prm0` 0 up / else down, `prm2` steps, `attr 0x1` variant |
| 2 | step (dansa) | `prm0` 0 climb up, else drop down (also area test in `bhCheckDansa`) |
| 3 | examine / event | `attr 0x8000` = show message `prm1` (room message table); `attr 0x1/0x8` = fixed camera `prm0`, key `prm2`; direction bits as type 0; scripts test "player activated trigger N" |
| 4 | item pickup / push object | `prm0` = item slot (`sys->itwp[prm0]`, = item record index) unless `attr & 0x2` (then object slot, `0xFF` = special); `attr 0x1` = pickup animation |

Activating sets `sys->etc_idx` and `cb_flg 0x100`; scripts test it with `Ck cb_flg bit 23`
(`rdx.py` prints "player is at trigger N"). **Scripts rewrite trigger records**: op 37
`EtcAtariSet2` sets type/attr/prm0..3 of trigger N (position and size are kept), op 11
`EtcAtariSet` switches it on/off, ops 179/180 `EtcAtariEnePosSet`/`EtcAtariEvtPosSet` place it (names; not read in detail). Most rooms rewrite their item and door
triggers in `scd0` (room entry) or even every frame in `scd1`; an edit of those fields in the
record alone has no effect then [verified: RM_1110's door is set every frame].

### 4.5 Entry positions (`posp`) [code][verified]

`POS` (0x10): `px, py, pz, s32 ay`. The door/teleport `pos_no` indexes this list; y is replaced by
the floor height (4.3). Rooms have up to 60 entries.

### 4.6 Enemies, objects, items (`ETTY_WORK`) [code][verified for items]

`ETTY_WORK` (0x24): `u32 flg, u16 id, u16 type, s8 flr_no, u8 mdlver, u8 wrk_no, s8 prm1,
px, py, pz, s16 ax, az, ay, aspd, s8 hide[4]`.

* Enemies: ACTORS.md section 5 (record order/count is load-bearing; models/motions/textures per
  record in `hed01/hed02/hed04`).
* Objects: records 0..3 are placeholders (slots 0..3 are the player's weapons and hair); 4..
  are `sys->obwp[i]` with a model each (`hed01`).
* Items: record *i* -> `sys->itwp[i]` (`objitm.c bhSetItem`): `id` = item id (as in the
  inventory, names in the F2 cheat menu), `type` low byte type / high byte param, `flr_no`,
  `mdlver`, `prm1` = draw type, position, angles, `aspd` (rotation speed), `hide[4]`. The model
  is the record's MLB (`hed01`); the textures are shared per item id. Pickup is a type 4
  trigger with `prm0 = i`; persistence is `it_flg` bits named by script ops 14 `ItmCk` /
  35 `ItmSetCk` (5).

### 4.7 Effects, lights, routes, messages [code]

* Effects (`EF_WRK` 0x44): `flg, id, type, flr_no, mdlver, position, scale, ay/ax, link
  (lkflg 0 none / 1 player / 2 enemy / 3 object / 4 item / 5 effect, lkno, lkono node,
  offset), param`; placed by `room.c bhSetEffectTable` -> `effect.c bhSetEffectTb`. Ids 30..99
  and 400+ need their textures from the room's list.
* Lights (`LGT_WORK` 0xE0, first 29 words authored, the rest runtime): `flg` (bit 1 on),
  `type`, link fields, `lsrc`, position `px py pz`, target `lx ly lz`, direction `vx vy vz`,
  `spc dif amb`, colour `r g b`, range `nr fr`, cone `iang oang`, angles. Lights 0..3 are
  global; 4.. can be hidden per camera (`hidlgt`). Script ops 53/75/115/116/120 change them.
  Room geometry uses `amb_*` ambient + these lights (normals in the vertex data).
* Routes (`rutp` boxes + `ruttp` next-hop matrix `[rut_n][rut_n]`): enemy path finding
  (`rutchk.c bhCheckRoute`: from box A towards box B go to the centre of box `ruttp[B*n + A]`).
* Messages (`mesp`): u32 table, entry `i + 1` = offset (from `mesp`) of message *i*; text is u16
  glyphs: `glyph = ASCII - 32`, `0xFF01` space, `0xFF00` new line, `0xFF02` wait/page,
  `0xFF04` choice, `0xFFFF` end (`message.c`). `rdx.py dump` decodes them.

## 5. Event scripts [code][data]

### Structure

`hed03` points to a block of u32 offsets relative to its own start: word 0 `scd0` (run once
on room entry, `event.c bhInitEvent`), word 1 `scd1` (run every frame, `bhControlEvent`), words
2.. = events 0.. (started by op 20 `EvtOn` into one of 16 tasks, `Event_exec`; run each
frame by `bhEventScheduler2` until `EvtEnd`/`End`). The first code offset ends the table.

### Execution

`bhScenarioJmpT[256]`: the byte at the pointer is the opcode; the handler reads its operands,
advances the pointer and returns 1 (continue) or 0 (stop). Stop at top level ends the script
for this frame (tasks resume where they stopped: `Sleep`, `EvtNext`, waits). Conditions are
instructions that return 0 when false:

* `IfelCk n` (op 1, 2 bytes): opens a block; `n` = distance from the `IfelCk` to its `Endif` or
  `ElseCk`. The condition instructions follow; if one returns 0, execution continues at the
  block end (else part). Several conditions in a row = AND.
* `ElseCk n` (op 2): ends the then-part; `n` = distance from the `ElseCk` to the first
  instruction after the else-part (no `Endif` then). `Endif` (op 3) closes a block without
  else. `End` (op 0, 2 bytes) ends a script.
* Loops: `For n` / `Next`, `While len` + condition / `Ewhile`, `Sleep n` (4 bytes, resumes after
  n frames).
* Controller ops 100 `PlCtr`, 102 `ObjCtr`, 103 `SubCtr`, 105 `CommonCtr`: 1 byte + a
  sub-command (own length table) acting on the task's current work (`LoadWork` op 101 selects
  player/enemy record/object/item/effect). An unknown sub-command is not consumed (the next
  byte runs as an opcode; RM_3020 event 6 relies on this).

Instruction lengths for all 213 opcodes and the controller sub-commands were derived from the
handlers (path analysis of the pointer increments) and are in `pc/tools/rdx.py` (`OP_LEN`,
`CTRL_LEN`, names `OP_NAMES`); a linear sweep with them ends exactly at every script's `End`/
`EvtEnd` in all 205 rooms (3 flow ops fixed by hand) [data]. Operands of individual ops: read
the handler in `event.c` (named `bh<Name>`).

### Flags

`Ck`/`Set` (ops 4/5, 6 bytes: `op, set, u16 bit, u8 ?, u8 value/mode`) address a bit `n` of a
flag set: word `n >> 5`, mask `0x80000000 >> (n & 31)`. Sets: 1 `ev_flg` (story, 1024), 2
`ky_flg` (keys/locks, 512), 3 `ed_flg` (dead enemies, 1024), 4 `rm_flg` (per room, cleared by
scripts), 5 `st_flg`, 6 `sp_flg`, 7 `it_flg` (picked items, 512), 8 `mp_flg` (map, 256), 9
`ic_flg`, 10 `cb_flg` (bits 22/23 with the floor/trigger index = "player activated floor/
trigger N"), 11 `gm_flg`, 12 `ts_flg`, 13..15 player flags, 16 `ssd_flg`. `Ck` continues
when (bit set) != byte 5; `Set` byte 5: 0 set, 1 clear, 2 toggle. `ev/ky/ed/it/mp_flg` are in
the save (`SYS_WORK` between `version` and `save_end`, `room.c bhPushGameData`). Which bits
are used by retail scripts: `rdx.py census` prints the never-referenced bit ranges [data]:
`ev_flg` 463..699, 701..803, 805..962, 964..1023; `it_flg` 452..511; `ed_flg` 358..1023; `ky_flg`
216..511; `mp_flg` 247..255 (plus scattered single bits). C code sets/tests some `ev_flg` bits
directly (e.g. 2, 57..66, 72, 75 in `effsub1b.c`, `en15.c`, `playpch2.c`; `it_flg` 326 in
`ranking.c`): grep `bhCkFlg(sys->ev_flg` / `bhStFlg(sys->ev_flg` before taking a bit.

### Opcodes a map maker needs (operand layouts) [code]

| Op | Name | Bytes | Operands |
|---|---|---|---|
| 0 | End | 2 | |
| 1/2/3 | IfelCk/ElseCk/Endif | 2 | skip |
| 4 | Ck | 6 | set, u16 bit, ?, expected (see above) |
| 5 | Set | 6 | set, u16 bit, ?, mode |
| 6 | CmpB | 4 | what (0 stage, 1 room, 2 cut, 8 sb_id, 15 pos, 17 weapon, 21 case, 23 game mode, 24 player, 25 costume), cmp (== > >= < <= !=), value |
| 8 | Sv | 4 | what (0 stage, 1 room, 2 cut, 15 pos, 16 floor, 18 weapon, 20 etc_idx, 21 case, ...), value |
| 11 | EtcAtariSet | 4 | trigger, off (non-zero) / on |
| 13 | DieCk | 4 | enemy record, u16 `ed_flg` bit: sets it when the enemy is dead |
| 14 | ItmCk | 6 | item slot, u16 `it_flg` bit, trigger, mode: on pickup through trigger: set bit, disable trigger, hide item |
| 16 | UseItemCheck | 2 | item id used on the active trigger (with `cb_flg 0x400`) |
| 20 | EvtOn | 4 | ?, task level, event number |
| 31 | MessageSet | 4 | type, message, ? |
| 34 | EneSetCk | 4 | enemy record, u16 `ed_flg` bit: hide the enemy if the bit is set |
| 35 | ItmSetCk | 6 | item slot, u16 `it_flg` bit, trigger, mode: if picked: disable trigger + hide item, else enable trigger |
| 36 | InitModelSet | 4 | class (0 player, 1 enemy record, 2 object, 3 item), index, 0 hide / 1 show |
| 37 | EtcAtariSet2 | 10 | trigger, u16 attr, prm0, prm1, prm2, prm3, type, pad |
| 51 | SetDoorCall | 8 | ?, u16 attr, stage, room, pos, door type: start a transition |
| 55 | RoomCaseNo | 2 | case for the next room file |
| 123/124 | WalAtariSet2/FlrAtariSet2 | 10 | as 37 for walls/floors |
| 149 | BgmOn2 | 4 | track, ... |
| 182 | RoomSoundCase | 2 | case of the next room sound bank |

## 6. Rooms, stages and the world [code][verified]

* Transition: a type 0 trigger copies its fields into `sys->door` and sets `cb_flg 0x1`; script
  op 51 `SetDoorCall` or the cheat/test teleport call `room.c bhSetDoorDemo(attr, stage, room,
  pos, type)` directly. `game.c bhMainSequence` -> `room.c bhStartDoorDemo`: fade, sound
  command, `sys->stg_no/rom_no/pos_no` = target, `mn_mode0 = 2` -> `system.c
  bhSysCallDoordemo` (door animation from `MRY.AFS 71 + type`, or with type 255 / attr bit 31
  / the PC option `--skip-doors` the load screen) -> monitor mode 4 (section 2) with the
  current `rcase`.
* There is no global room graph: connectivity is only in the trigger records and scripts of
  each room. `rdx.py census`/`dump` list each room's door targets; scripts may change them by
  story state (examples: RM_1000 picks case 0/1 and target 1/2 or 1/12 by flags; RM_1110
  turns its door into an examine trigger once `ev_flg` 151 is set).
* Arrival: entry position `pos_no` of the target room; the target's `scd0` runs (music, flags,
  hiding picked items/dead enemies), then `scd1` every frame.
* Saves keep `SYS_WORK` from `version` to `save_end` (flags, inventory, stage/room/pos, case,
  player position/angle) [code]; room records themselves are never saved, so edits to room
  files apply to existing saves the next time the room loads.

## 7. Limits and budgets

### 7.1 Memory and drawing: PS2 limit and the PC's [code][verified 2026-10-08]

The PC raises these under `#ifdef TARGET_PC` (constants in `pc/include/pc_prelude.h`); retail rooms
load and draw as before (new game + RM_9350: all dumped frames byte-identical to the build
without the change). The compiler's budgets follow from them: [MAPS_DESIGN.md](MAPS_DESIGN.md)
"Budgets". What happened on overflow is what the PS2 code does; the PC now stops or skips instead.

| Resource | PS2 | PC | Overflow on the PS2 / on the PC |
|---|---|---|---|
| Room arena (`freemem`, `main.c njUserInit`; `sys->memp..endp`) | 12.25 MB block, ~11.7 MB arena, ~10 MB free on arrival (1.8 MB persistent player data) | **+64 MB** (`PC_ARENA_EXTRA`): 74.6 MB free on arrival | no check (`bhGetFreeMemory` only tests the block end): the packed file is read below `memp` over the player data; the expanded one runs into the vertex buffers / **fatal error with sizes** (`system.c` mode 4) |
| Room load | packed file at the top, expanded at the bottom, both at once; RM_9350 (8.6 + 5.5 MB) overlapped them | packed + expanded <= the free arena | |
| Room after load | file up to the texture section + `O_WORK`/AI/effect allocations; the texture section is scratch after the uploads; the map/file/option screens copy the room model's textures above `memp` (`adv.c AdvPushRoomTexture`) | same, in 74.6 MB | |
| Vertex work buffer (`njInit3D(vwbmemp, 8192)`, 64 B per vertex of the node being drawn) | 5120 entries fit (320 KB to the block end; the 8192 is never checked) | **8192** (`PC_VERTEX_EXTRA`) | writes past the block into the sound work memory / **node not drawn** if `index offset + count > 8192` (`ps2_NinjaCnk.c njCnkDrawModelLocal`) |
| Vertex chunk | one per node (only `vlist`'s first chunk is processed); u16 size in words: <= 8189 vertices; count 0 runs away | unchanged | |
| Strip chunk | `npCnkFlatOff` (room load) reads its size as a signed short: <= 32767 shorts (~3,276 three-vertex triangles) | unchanged | negative offset, corruption |
| Strip length | translucent strips <= 194 vertices (`NaCnkStrBufTop[200]`), type-64 strips <= 200; strips are drawn in batches of 60 | unchanged (the compiler writes 3-vertex strips) | static variables overwritten |
| Ordering table (translucent/sorted primitives per frame, `ps2_dummy.c Ps2AddOT`) | 8192 entries, 1.75 MB packet buffer (native path: 128 B per entry; emulated path ~3 KB per 60-vertex batch) | **32768 entries, 4.5 MB** (`PC_OT_ENTRIES`, `PC_PBUFF_SIZE`) | no check, overwrites what follows / **primitive dropped**, logged once |
| Room-model nodes | MLB node count u16 (65535); no per-node arrays for the room (`owP` NULL); matrix stack 128 = tree depth ~126 (siblings do not count) | unchanged | |
| Camera hide masks | cut cameras `hidobj[16]`: nodes 0..511 (mirror pass 0..383); event key frames `hidobj[8]`: 0..255 (mirror 0..127) | nodes 512+ are **never hidden** (`cut.c HIDE_BIT`); event key frames and the mirror pass still read past their lists for nodes 256..511 / 384..511 | node 512+ read `hidlgt`, fog and the next camera as hide bits: random parts hidden |
| Texture heap / entries | 10 MB, 256 entries | 64 MB, 1024 entries (ENEMIES.md 2.2) | `exit(0)` |
| Texture definitions (native renderer) | | 96 MB kept by the render thread (`texture_glue.c KNOWN_BYTES_MAX`) | |
| Native renderer per frame | | vertices pooled per frame (44 B each, 4 frames recorded, grown by `realloc` without a NULL check); GL batches flush | 32-bit address space (below) |
| Texture lists per room | `sys->txlp[256]` (room model, textured enemy parts, object ids, item ids) | unchanged | overwrites `txdp` |

**The address window is the PC's real ceiling.** Game memory must lie in 0x30000000..0x3FFFFFFF
(the EE's uncached aliases are identities there, and DMA addresses are masked to 28 bits, e.g.
`ps2_NinjaCnk.c` ORs 0x30000000 into the model pointer). The executable is linked at 0x30000000
and all static game memory is its `.bss`: 178 MB before this change (64 MB texture heap, 32 MB
spawn pool, 12 MB actor room buffer, ...), now 0x3077CB40..0x3F5927E8 = 238 MB, **10.4 MB left**
below 0x40000000 (Windows: data end 0x3F0F4064, 15 MB left; checked at start: `pc/src/ps2/memory.c`). More arena needs room in the window:
host-only buffers (`gs_mem`/`gs_mem_scratch` 8 MB, the profiler's `samples` 4 MB, GL `vbuf` 3.6
MB, actor_glue's 12 MB retail-room reader) could move to `malloc`, or the texture heap could
trade some of its 64 MB. Host allocations (the brk heap, starting after the image, and `mmap`)
need not be in the window. Measured in the 250k-triangle test room: 1.44 GB of the 4 GB address
space mapped, 2.6 GB free, the largest free block 988 MB, 195 MB resident.

### 7.2 Records [code]

The counts in the room block are `int`; collision, camera and trigger loops have no fixed arrays.
The limits are byte-wide indices elsewhere (scripts, record fields, `SYS_WORK`, co-op packets), so
they are **file/save format limits** the PC does not raise:

| Records | Limit | Why (beyond: effect) |
|---|---|---|
| Walls | 32767 (engine), 0..255 addressable by scripts | `BH_PWORK.psh_idx` is a short; `WalAtariSet(2)` take a byte; co-op syncs walls/triggers/floors 0..255 only (`coop_glue.c atr_table`). Tested: 629 walls, collision at both ends of a 1200-unit room |
| Triggers, floors | 256 | `etc_idx`/`flr_idx` are compared with a byte in scripts (`Ck cb_flg` 22/23, `ItmCk`): a trigger 256+ never fires its event; co-op packs the pickup trigger as u8. Stair partner (`prm3`) 0..254 |
| Cameras (cuts) | 255 (127 if objects/items list them in `hide[4]`, signed char) | script operands (`CamSet2`, `CmpB`, `Sv`), type-89 wall `prm`, PC `tps_glue.c` cut key (`ncut | camver << 8`). Areas per cut: 255 (`ctab_n` u8) |
| Entry positions | 128 | `sys->pos_no` is a signed char (in the save): 128..255 index before `posp` |
| Routes | 255 | u8 route ids, 255 = none (`rutchk.c`); the next-hop table is `rut_n^2` bytes |
| Lights | 256 (4..255 hideable) | `hidlgt[8]`; script ops take a byte. At most 3 point lights per frame are used (`light.c`) |
| Event lights | 128 | key frame `hidlgt[4]` |
| Effects | 256 records, and free `eff[512]` slots | `sys->efid[256]`; effect ids < 450 |
| Enemies, objects, items | 128 slots (incl. children), 32 objects (4 reserved), 32 items | `ene[128]`, `obwp`/`itwp` (ENEMIES.md 2.2) |
| Floor levels | `flr_no` -2..28 | `grand[32]`, `grand[31]` forced to 0, `flr_no` is a char |
| Messages, event cameras | 256 addressable; 16 key frames per event camera | byte operands |

### 7.3 Workarounds for a whole-area room

* **Meshes**: a node holds at most 2,729 triangles the way `rdxbuild` writes them (3 own
  vertices each); split a big mesh into more nodes (65535 allowed, but cameras can hide only
  nodes 0..511, so give the hideable parts low indices). `rdxbuild` shares a vertex between
  triangles of a node where position and face normal are the same (UVs live in the strips):
  RM_1310's data would drop from 391k to 308k vertices. Smooth (per-vertex) normals would
  allow 92k (7.5).
* **Walls/triggers**: thousands of walls work; keep the ones scripts or co-op must change below
  256. One wall list per room; no streaming.
* **Size**: the arena, not the format, limits a room: ~44 MB expanded with ~20 MB packed. Beyond
  that, more address-window space (7.1) or several rooms joined by doors.

### 7.4 Other limits

| Item | Limit | Source |
|---|---|---|
| Names | stage 1 digit, room 2 digits, case 1 digit; `RDX_LNK.AFS` TOC is read from one sector (<= 255 entries), irrelevant with the mods folder | [code] |
| Version | float at offset 0 >= 1.72 | `room.c bhSetRoom` |
| Checksums | none on room files (only the memory-card save has a checksum) | [code] |

### 7.5 Drawing cost of a whole-area room [verified 2026-10-09]

RM_1310 (the RE4 village, 130,498 triangles in 217 nodes) cost 210-310 ms a frame headless,
a retail room ~33 ms (the game's 30 Hz). Measured causes (docs/PORT.md "Cost per frame"):

* **Not drawn twice.** The statistics line counts an executed frame plus its in-between
  replays (enhanced mode), so 275k "prims" = 4 x 68,847. One executed frame draws 68,523
  room strips (`rdxbuild` writes one 3-vertex strip per triangle) plus the actors.
* **Culling works already.** Every room node is tested against the view by its bounding
  sphere (`njCnkDrawModelLocal` model clip, on through `bhAllDrawModel`'s `njControl3D(0x2500)`;
  `rdxbuild` writes the sphere of each node's vertices), and every strip against the
  (guard-band) clip volume before it is drawn. In the test view 77 of 217 nodes were culled,
  and 63,353 of the 68,523 drawn strips lie inside the 4:3/hor+ view (5,170 wholly outside
  it): finer frustum culling could save <8% there. A far cutoff tied to the fog would not
  help either: RM_1310's `fog_fr` is -4000, the village is ~2000 units across. No culling
  was added; retail and PC rooms draw as before.
* **The cost per drawn vertex and per draw was the problem**: VU0 `CALCPOINT` emulated per
  vertex (166 ms for 205k vertices), `glGetError` per draw (~250 ms of waiting), and
  presents not paced in headless runs. All three are gone (docs/PORT.md); RM_1310 now runs
  at the 30 Hz cap like a retail room (game thread ~20 ms of CPU a frame, 13 of them CALCPOINT).

What the authoring can still do (for `pc/tools/re4` / any generator feeding `rdxbuild`):

* **Vertices**: the game transforms and lights every vertex of a drawn node each frame.
  `rdxbuild` now shares identical vertices (-21% for RM_1310). Shared smooth normals would
  cut the village to 92k vertices (-76%): it needs per-vertex normals in the OBJ (`vn`), which
  `rdxbuild` does not read yet (its face tuples are shared with the area and RE4 tools).
  Normals only matter for lights and for the single-sided culling of the VU1 path.
* **Nodes**: keep them spatially compact. RM_1310's 7 terrain nodes with a radius over 500
  units (12,784 triangles; source groups split only at the 2,729-triangle limit, in file
  order) are drawn from almost every camera; tiling such groups into cells of ~100-200 units
  lets the model clip drop the parts behind and beside the camera. Median node radius is 28
  units, which is fine; very small nodes only add per-node overhead (a matrix push and the
  clip test). Hideable nodes must keep indices < 512 (7.1).
* **Strips**: one strip per triangle costs a strip test, a native strip record and an
  interpolation match per triangle; longer strips (shared vertices first) would divide that.

## 8. Coordinates cheat sheet

* World: x right/left, y up, z depth; units ~10 cm; angles 65536 = 360 degrees, `ay 0` faces -z.
* Record boxes (`ATR_WORK`, cut areas): min corner + extent (`w` along x, `d` along z).
* The scene is right-handed as rendered: looking along +z, +x is on the screen's left
  [verified in-game, docs/MAPS_DESIGN.md]. `rdx.py obj` therefore writes game coordinates
  unchanged; Blender (import "Y up, -Z forward") shows the room as the game does. (Earlier
  versions negated z, which mirrored it.)
* Camera `ay`: the view direction is 180 degrees minus `ay` (`ay 32768` looks along +z).

## 9. Mods folder (implemented in the port) [verified]

`pc/src/audio/adx.c` + two `#ifdef TARGET_PC` hooks:

* `$RECVX_MODS` (default `mods` in the working directory). A file `mods/RM_1110.RDX` replaces
  the disc's room; a name the disc does not have (e.g. `mods/RM_1150.RDX`) adds a room:
  `ps2_sg_gd.c gdFsGetDirInfo` (size) and `adxwrap.c OpenAfsIsoFile` (open) ask
  `pc_mod_file_size` / `pc_mod_open` first.
* Entries of AFS archives opened by partition/index (`ADXF_OpenAfs`: sound banks, door
  animations, map files, player/weapon files) are replaced by `mods/<ARCHIVE>/<index>`, e.g.
  `mods/MRY.AFS/76` [code, not exercised].
* Plain files opened through `ADXF_Open` are replaced by `mods/<NAME>` [code].
* Streamed ADX music and voices (`ADXT_StartAfs`) the same way: `mods/<ARCHIVE>/<index>` holding
  an ADX file (2 channels at most, 18-byte frames) [verified with a copy of a disc entry].
* Not covered: `ReadFileEx` (`gdFsOpen`, e.g. `MULTSPQ?.IDX`, `SYSMES.ALD`, `COMMON.MLT`).
* The log shows `mods: <path> replaces the disc file (<size> bytes)` for every use.

Files are read as from the disc (whole 2048-byte sectors, zero padded), so the game sees the
same bytes as from an image that contains them.

## 10. Adding our own content (ranked by effort)

### (a) Edit an existing room — works now

Fixed-size records (positions, types, parameters of items, enemies, objects, effects, walls,
triggers, floors, entry positions, cameras and their areas, lights, room colours/fog/heights)
and script operands: `rdx.py dump` -> edit JSON -> `rdx.py build ROOM edit.json -o
mods/RM_xxxx.RDX`. Byte patches (`"patches"` in the JSON, offsets as printed by `rdx.py
script`) cover script operands. Textures: `rdx.py tex` / `teximport` (same size, existing
palette). Rules: record **counts** stay the same; check the room's scripts for ops that
rewrite the record you edit (37/123/124, 11/10/12, 36, 179/180); item pickups need the item
and its type 4 trigger moved together; enemies: ACTORS.md 6.

Missing for (a): changing counts (adding an item/wall/trigger/camera), resizing textures,
editing palettes. Needs the section re-layout tool (task T9 in archive/MAPS_TASKS.md): all record
arrays are addressed through the room block offsets, the models/motions/textures through
`hed01/02/04` tables and their in-section offsets, so a re-layout must rewrite those
offsets (models are relocated relative to their own MLB start and can move as a whole;
scripts too, the event offset table is relative to `hed03`).

### (b) A new room cloned from an existing one — works now

Copy a room file to an unused name (`RM_1150.RDX` = stage 1, room 15) in the mods folder,
edit it as in (a), and point a door at it (trigger record and/or script operands of the
source room) and its door back. What a clone keeps/loses:

* keeps: geometry, cameras, collision, scripts (incl. flag numbers: a clone shares `it_flg`/
  `ed_flg`/`ev_flg` bits with the original; give it own bits, census), messages, models.
* loses: room sound bank (key not in `MULTSPQ?.IDX`: previous room's sounds stay), map
  entry, per-room tables in the executable (view clip, background colour, special cases);
  music only as its scripts start it.
* the source room's door often lives in the script (`EtcAtariSet2`, `SetDoorCall`): patch the
  operands there as well as the record.

### (c) New geometry from a modern tool

Pipeline to build (tasks T9..T11): Blender (or OBJ) -> room model (one MLB, objects of chunk
51 vertices + 8/19/66 strips, texlist) + TIM2 textures (4/8 bit) + collision boxes (walls,
floor heights, triggers) + camera cuts (area boxes + fixed cameras) + entry positions +
minimal scripts (`scd0`/`scd1` = `End`) -> assemble a room file (header, room block, record
arrays, `hed01..04`) -> `mods/RM_xxxx.RDX`. Simplest first target: take a retail room as a
template, keep its records and scripts, replace only the room model and texture list (both are
self-contained sections), then add own collision/cameras. Hard parts: fixed cameras are
pre-rendered-style compositions (one camera per area; author them in Blender as camera
objects), hide masks per camera (can be all zero), lighting (normals + ambient; no
lightmaps), strip generation (any triangle -> 3-vertex strips works, at a cost).

## 11. Tools

* `pc/tools/rdx.py` (Python 3, stdlib only): `list`, `dump`, `build`, `extract`, `pack`,
  `script`, `tex`, `teximport`, `obj`, `census`, `grants` (items the scripts hand over through the
  status screen: `Sv sb_id` + `Set cb_flg` 0x10, "given" with 0x4000 = no question; docs/COOP.md
  "Protocol 13"), `selftest`. ISO from `--iso`, `$RECVX_ISO` or
  `ISO/SLUS-20184 ROM.iso`. `ROOM` = `RM_1110`, `1110`, or a file (packed or expanded).
  Compression takes ~5 s per MB in CPython.
* `pc/tools/room_test.sh OUT STAGE:ROOM:POS [SECONDS]`: headless new game + teleport with frame
  dumps and log; extra input via `MORE_INPUT`, positioning via `RECVX_WARP`, mods via
  `RECVX_MODS`, another binary (e.g. a build of main to compare with) via `RECVX_BIN`.

## Verification log

Headless, 32-bit build of this branch, `SDL_VIDEO_DRIVER=offscreen`, `room_test.sh` (new game,
`RECVX_TELEPORT`), user's disc image. Frame dumps inspected (not committed).

1. Format: all 205 rooms expand; `rdx.py census` parses every header, room block, record
   array, camera area, model (chunk types above), texture list; the script sweep ends cleanly
   in 205/205 rooms; LZ and JSON round trips are byte-exact (`selftest`).
2. Item move + door retarget (`RM_1110`, the piano room, no enemies): item record 3 (handgun
   bullets) and its type 4 trigger 2 moved next to the entry position; door trigger 0 retargeted
   1/2/3 -> 1/4/0 in the record and in the `scd1` `EtcAtariSet2` operands (`patches`). Result:
   `mods: .../RM_1110.RDX replaces the disc file`, the bullets lie on the floor at the new spot,
   warping in front of them and pressing cross shows "Take the Handgun Bullets? Yes/No" and takes
   them; warping to the door and pressing cross plays the door animation and loads
   `RM_1040.RDX` (statue room). The same input without mods: the bullets stay on the cabinet,
   the warp+cross picks up nothing, the door loads `RM_1020.RDX`.
3. New slot: `RM_1150.RDX` (not on the disc) = RM_1110 with texture 0 of its room list (the
   512x512 carpet) replaced by a checkerboard (`teximport`, nearest palette colours: grey/black);
   RM_1110's door retargeted to 1/15/0 (record + script patch). Result: `mods: .../RM_1150.RDX`
   is opened through both hooks (size + data), the door animation plays, the clone appears with
   a checkered floor, the game ran on for 100 s (3900 frames) in it without errors. Its room
   sound bank key (1150) is not in `MULTSPQ1.IDX`: nothing is loaded and nothing hangs (the banks of
   RM_1110 stay, inferred from sdfunc.c).
4. Final build (decomp hooks re-applied whitespace-exact): `RECVX_TELEPORT` straight to 1/15/0
   loads `RM_1150.RDX` from the mods folder (size + data), checkered floor visible, 70 s in the
   room without errors.
5. Speed note: on a loaded machine a run reaches the room after ~100 s (3-4 frames/s); unloaded
   ~30 frames/s.
6. Slopes (2026-10-08, 4.3.2). `rdx_slopes.py` room built as `RM_1280` (`"budget": "pc"`, `flg`
   0x100, 67 slope records): `RECVX_NOCLIP=1:0` (position log every 15 game frames),
   `MORE_INPUT="420:up,1300:-"`, `RECVX_WARP="1:28:400:80:75:32768"`. Along z 30: y 0.0, 1.1, 2.5,
   4.0, 5.5, 6.9, 8.4, 9.8 (ramp, x 18..59), 10.0 ×3 (plateau), 9.3 .. 0.6 (ramp down), 0.0;
   along x 80 over the hill: 0, 1.3, 3.3, 6.8, 10.0, 12.2, 14.4 ×3, 13.0, 10.8, 8.1, 4.5, 2.0, 0.1,
   0; floor 0 throughout, ~5.8 units per 15 frames all the way (no snag), frames show the feet on
   the surfaces. Teleport to entry 1 (hill top, authored y 14): y 14.4 from the first frame.
   Retail: `RECVX_BIN=build-main/pc/recvx` vs this build, RM_1020 entry 2, warp to the stair foot,
   cross: same positions in both logs, frame and render-target dumps byte-identical.

## Unknowns / not verified

* Camera follow modes in detail (`flg` 0x4/0x40/0x80, `spd`, `am_spd/aa_spd`),
  floor type 0 semantics beyond "climb/push area", light `type/lsrc` values, `exd[256]` in
  cuts, `rom->flg`, `hide[4]` of items/enemies.
* Operand meanings of most script ops (lengths are known for all; semantics only for the ops
  in the table above).
* Map screen data format and how a new room could get a map entry.
* Sound bank (`.SPQ`) format; adding a bank for a new room needs `MULTSPQ?.IDX` (read with
  `ReadFileEx`, not yet covered by the mods folder) or a hook in `SearchAfsInsideFileId`.
* The overlap of the packed and the expanded copy during loading (RM_9350) is harmless on the
  PS2; on the PC no retail room overlaps any more, and custom rooms must not (section 7.1).
