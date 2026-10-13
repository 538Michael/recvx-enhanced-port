# Actors: how players and enemies are loaded, spawned, animated and drawn

Reference for three planned addons: (1) player mesh replacement that keeps the normal
animations, (2) spawning enemies from the F2 cheat menu, (3) an enemy/item randomizer.
All paths are relative to `decomp/src/ps2/veronica/prog/` unless noted; types are
in `decomp/include/ps2/veronica/prog/types.h`.

Evidence tags:

* **[code]**: read from the decompiled source (file:function given).
* **[verified]**: checked at runtime, headless, with a temporary probe in `pc/glue` (attract demo,
  Claire, stage 0 room 13 with five bats; see "Verification log" at the end). The probe is not
  committed.
* **[inferred]**: a conclusion from the code that was not run.

Nothing in this document is derived disc content except counts and sizes (file ids, part
counts, aggregate room statistics).

## Summary

* All characters and enemies are `BH_PWORK` (0x580 bytes): the player is `ply` (`plp`), enemies
  are `ene[128]`. A model is an `ML_WORK` = a flat array of `NJS_CNK_OBJECT` nodes stored in
  depth-first (pre-)order plus one `O_WORK` (matrix) per node. Motions are `MN_WORK` sets of
  per-frame keys, one track per node, bound to the skeleton **purely by node index**.
* The four player files (Claire, Chris, Steve, Wesker: sys part ids 10..13, Claire's second
  costume 14) share **the same 22-node hierarchy** [verified]. Their motions contain root
  position + rotations only (no other position keys) [verified], so a body takes its bone lengths
  from its own model: any of the four meshes should play any other's motions [inferred]. Replacing Claire's mesh
  by Chris's or Wesker's while keeping Claire's motion set, weapon motions and game logic works
  at runtime with ~20 lines of glue [verified].
* Enemy models, motions and textures are **not** in global files: each room file
  `rm_SRRC.rdx` carries its own copies for the enemy records it contains (`room.c bhSetRoom`,
  `bhFinishRoom`). Cloning an enemy type the room already loaded works [verified]; any other type
  needs its data pulled from another room file (designed below, not verified).
* Enemy behaviour is `bhJumpEnemy[id]` (100 entries, `eneset.c`). Several types install helper
  functions into shared slots 31..38 at runtime (`bhEne_SetCallFunc`); on the PS2 two such types
  in one room break each other. No retail room combines two of the slot-31 users 2, 3, 5, 6, 13,
  23 [verified census]. The port runs each part with its creator type's function in rooms it
  added enemies to (ENEMIES.md 2.2).
* Room event scripts address enemies by record index (`rom->enep[i].wrk_no`) and items by item
  slot / `it_flg` bit; a randomizer must keep record order and count, and stay away from records
  that scripts animate (room motion set) or link.
* Hard limits: 128 enemy slots, 32 object and 32 item slots, 256 texture entries (`tbuf`), a
  10 MB texture heap, a 12.25 MB main arena with no overflow check against the room file, 512
  motions per set, 6 enemy sound slots. The port raises the texture table to 1024 entries and
  the heap to 64 MB (`pc_prelude.h`, ENEMIES.md 2.2).
* Any enemy or NPC model as the player's (co-op "You appear as") is implemented as a display
  actor drawn in place of the player (section 8): the story cast rides the player's motions
  through a node map, every other model plays its own idle motion [verified].
* Cutscene scripts re-attach the player's head part `sys->obwp[2]` with the offsets of the
  character played (op 52 `ObjLinkSetPly`), which put Wesker's sunglasses on Claire's ponytail
  spot behind the head; a replacement mesh has to keep its own (section 4) [verified].

## 1. Memory and load pipeline

### Main arena [code]

* `main.c njUserInit`: `freemem = syMalloc(12845056)` (12.25 MB). Vertex/view buffers live at
  the top (`njpmemp = freemem + 12255232 - vtx_total`); `sys->endp = njpmemp`.
* `system.c bhInitSystem`: `sys->memp = freemem` + save region; `keepmem = sys->mempb = memp`.
* `system.c bhFirstGameStart`: `sys->obwp` (32 `O_WRK`, objects, `[0..3]` reserved for the
  player's weapons/hair) and `sys->itwp` (32 `O_WRK`, room items), 39936 bytes each.
* `system.c bhSysCallMonitor` mode 1 (game start / load): reads sys file 1 (common data), then
  `bhInitPlayer` allocates the **persistent player buffers** and sets `sys->mempb = sys->memp`.
  Everything above `mempb` is per room and is discarded at every room change
  (`sys->memp = sys->mempb`, `bhSysCallMonitor` mode 2 / `event.c` line ~9076).
* Allocators: `pwksub.c bhGetFreeMemory(size)` (64-byte aligned bump from `sys->memp`, refuses
  only beyond `freemem + 12.25 MB`) and `eneset.c bhEne_CallocWork(size)` (refuses beyond
  `mempb + 0xC40000`; on the PC both limits grow by `PC_ARENA_EXTRA`). **Neither checks `sys->endp`**: an overrun writes into the vertex buffers
  [code]. There is no free; runtime allocations live until the next room load.

Player buffers (`player.c bhInitPlayer`) [code]:

| Pointer | Size | Content |
|---|---|---|
| `sys->plmdlp` | 131072 | player model block (all sub-models) + their `O_WORK` arrays |
| `sys->lmmdlp` | 32768 | (allocated, no reader found) |
| `sys->wrmdlp` / `wlmdlp` | 32768 each | right / left hand weapon model |
| `sys->plmthp` | 12288 | 512 `MN_WORK` (motion headers), `plp->mnwPb` |
| `sys->plbmtp` | 393216 | base motion data |
| `sys->plwmtp` | 65536 | weapon motion data (motions 100+) |
| `sys->plzmtp` | 8192 | per-motion forward translation tables (`player.c bhGetTransZ`) |
| `sys->plexwp` / `plhdwp` / `pletcp` | 124 / 124 / 32768 | `plp->exp0` (`EXP_WORK`), `exp1` (head/eye work), `exp3` |

### Room load [code]

`system.c bhSysCallMonitor` mode 4:

1. case 0: `rm_%1d%02d%1d.rdx` (stage, room, room case) is read from the ISO (not the AFS) to
   `endp - size`, `bhInitReadRDT` clears enemies (`bhInitEnemy`: `ene[]` zeroed), camera, room.
2. case 1: `Expand` decompresses it to `sys->memp`; case 2: `bhSetRDT` -> `room.c bhSetRoom`
   realizes the room model, **every enemy record's model** (`bhSetEneMdl`), objects, items and
   builds the texture upload list `sys->txlp/txdp` (max 256).
3. case 3: one `bhSetMemPvpTexture` per frame, then `room.c bhFinishRoom`: enemy motion sets
   (`bhSetEneMtn`), the room motion set (`sys->rmthp`), `O_WORK` arrays (`bhKeepObjWork`),
   player placement, `bhInitRoomChangePlayer`. `bhSetRoom` ends with `sys->memp = start of the
   texture section`, so the texture part of the room file is reused after upload.
4. If the story switched character (`sys->cb_flg & 0x80`, set by script op 137
   `bhPlayerChangeSet`): player textures released, `sys->ply_id = sys->cng_pid`, then cases 4..7
   read the new part file and weapon file (same calls as at game start).
5. case 10: `bhInitEvent` starts the room script; the room's sound bank `RM_SRRC.SPQ` is requested
   (`system.c` ~2225, `sdfunc.c RequestRoomSoundBank`).

Measured in the attract-demo room (stage 0, room 13): room data `memp - mempb` = 339 KB, free
`endp - memp` = 10.3 MB, 68 of 256 texture entries in use [verified]. The largest expanded room
file is 8.9 MB (census over all 205 room files) [verified], so large rooms leave far less.

### Textures [code]

* `ps2_texture.c bhSetMemPvpTexture(texlist, data, offset)` walks `PLI`/`TIM2` chunks and calls
  `ps2_NaTextureFunction.c njLoadTexture` per texture. Entries are keyed by the TIM2 global
  index (`Gindex`) + CLUT bank: the same `Gindex` loaded twice is shared and refcounted
  (`SearchNumber`). This is why two instances of a type, or Claire and Chris in co-op, coexist.
* `Ps2TextureMalloc` copies the TIM2 into a **10 MB texture heap** (`Ps2_tex_buff`), so source
  buffers may be freed after upload. Table: `main.c tbuf[256]` (`njInitTexture(tbuf, 256)`).
  A full table or heap calls `exit(0)` [code]. PC: 64 MB and 1024 entries (`PC_TEX_HEAP_SIZE`,
  `PC_TEX_ENTRIES`).
* Palettes: each `PLI` entry carries a fixed CLUT bank id (`bk_id`, 255 = allocate dynamically
  from `sys->ef_pbnk`). Banks are authored per asset; loading a model into a context it was not
  authored for (foreign room) can overwrite another asset's palette -> wrong colours [inferred].
* Release: `bhReleaseMainTexture` (door transition) releases the room model, the textures of
  every **room record's** enemy (`ene[rom->enep[i].wrk_no]`, only `mdl[j].flg & 0x200` owners),
  objects and items; `bhGarbageTexture` compacts the heap. Anything loaded outside that list
  (cheat-spawned foreign enemies, a replacement player mesh) must be released by its loader.
  `njReleaseTextureAll` (title, game over) resets everything.

## 2. Models and skeletons (A)

### Structures [code]

* `ML_WORK` (0x18): `flg` (status byte of the file: `0x80` = chunk model, else basic model;
  `0x200` = this instance owns the textures), `obj_num`, `datP`, `objP` (first
  `NJS_CNK_OBJECT`), `texP` (`NJS_TEXLIST`), `owP` (`O_WORK[obj_num]`).
* `NJS_CNK_OBJECT` (0x34): `evalflags, model, pos[3], ang[3], scl[3], child, sibling`.
  `pos/ang` are the **live pose**: motions overwrite them every frame.
* `O_WORK` (0x50): `flg` + world matrix `mtx[16]`. `flg` bits used by the motion code:
  `0x1` matrix is set externally (`bhCalcTree` uses it instead of computing),
  `0x2` motion does not write this node's rotation (procedural: head look, aiming),
  `0x4` node has no motion track (tracks are consumed only by nodes without it),
  `0x8` blend this node in YZX order (player upper arms 7 and 11).
* `BH_PWORK`: `mdl[16]` (sub-models), `mdl_n`, `mlwP` (the one drawn/animated), `mdl_no`,
  `skp[16]` (skin data per sub-model), `mbp[16]`/`txp[16]` (copies of `objP`/`texP`), `mnwP`
  (current motion set) / `mnwPb` (own set), `mtn_no`, `frm_no`, linking fields `lkwkp`, `lkono`,
  `lox/loy/loz`, `clp_jno[8]` (nodes used for view clipping), `cpcl` (hit capsules).
  `O_WRK` (objects, weapons, effects) has the same layout up to `0x3E4`, so the model functions
  take either (`bhCalcModel((BH_PWORK*)op)`).

### Binary format and realization [code]

`binfunc.c bhMlbBinRealize(bin, mlw)`: header `char[3]` + status byte, `u16[2]` header size,
`u16[3]` `obj_num`, `int[2]` texlist offset, `int[3]` object offset (offsets relative to
`bin + header size`, `-1` = none). Objects are **one contiguous array**; `child`, `sibling`,
`model` and inside the model `vlist`/`plist` are relocated in place (so a buffer can be realized
only once). `bhMnbBinRealize` does the same for a motion: `NJS_MOTION` (`nbFrame`) and per node
an `NJS_MDATA2` pair `p[0]` (position keys, float[3] per frame) / `p[1]` (rotation keys, 3 angles
per frame), plus an optional per-frame attribute array (`atrP` -> `mtn_attr`).

### Hierarchy and matrices [code]

`MdlPut.c bhCalcModel`: base matrix = translate(`px,py,pz`) * rotateXYZ(`ax,ay,az`) (scale
`sx..` unused here), or, if `flg & 0x80`, the linked parent's node matrix
`lkwkp->mlwP->owP[lkono].mtx` * translate(`lox,loy,loz`) * rotate. Then `bhCalcTree` walks the
**flat array in order**: push if the node has a sibling, translate(`pos`), rotateXYZ(`ang`),
store into `owP[i].mtx`, pop if it has no child. The array order is therefore the depth-first
order of the tree and the parent of a node is implicit. Only the null-ness of `child`/`sibling`
matters at run time.

### Drawing [code]

`MdlPut.c bhPutModel`: `njSetTexture(mlwP->texP)`; chunk models with skin data:
`npPushMdlstr`, `njplus.c npCalcSkin` (or `npCalcSkinFM` when `mdflg & 0x100`) blends the
skinned vertices of each node from up to several node matrices (weights in 10% steps), then
`bhJumpCnkDraw[draw_tp]` draws each node's model with `owP[i].mtx`; a mirror pass follows in
mirror rooms (`st_flg & 0x100`). Skin data is converted once at load against the model's own
bind pose (`npSkinConvert`), so skin data belongs to its model and references its node indices.
Callers: `game.c bhAllDrawModel` (room, enemy objects, items, effects, `eneset.c bhDrawEnemy`,
then the player if `pt_flg & 1` and not hidden by `stflg & 0x1000000`). The shadow is a blob
(`bhSetShadow(NULL, plp, 1, 4.5, 4.0, 3.5)`), not model-dependent.

### Player part file (sys AFS partition, file `ply_id + 10 + costume*4`) [code + verified]

Layout read by `dread.c bhReadPlayerData` (input at `sys->memp`):

```
u32 size; model block      -> copied to sys->plmdlp
    repeated: u32 n, n bytes (an MLB, or "SKIN" skin data for the next MLB); u32 -1
u32 size; motion block     -> copied to sys->plbmtp, realized into sys->plmthp[0..]
    repeated: u32 n, n bytes (an MNB; n = 0 means an empty slot); u32 -1
u32 size; z-translation    -> copied to sys->plzmtp
per sub-model with a texlist: u32 size (bit31 = 32-byte align), PLI/TIM2 data -> bhSetMemPvpTexture
```

After loading: `O_WORK` arrays are placed right behind the model data inside `plmdlp`;
`sys->hd_pos = objP[5].pos` (the head node offset). The loader does **not** clear `plp->skp[]`.

Contents [verified, probe parse + runtime state]:

| File | Who | Sub-models (`mdl[i]`: nodes) | Motion slots / used | Size |
|---|---|---|---|---|
| 10 | Claire (`ply_id` 0) | 0: 22 skinned body, 1: texlist only, 2: texlist only, 3: 22 untextured, 4: 4 skinned, 5: 1, 6: 2, 7: 5 skinned | 111 / 60 | 1.29 MB |
| 11 | Chris (1) | 0: 22 skinned, 1, 2: texlists, 3: 22 untextured | 105 / 55 | 1.06 MB |
| 12 | Steve (2) | same as Chris | 105 / 56 | 1.00 MB |
| 13 | Wesker (3) | same as Chris + 4: 1 node | 106 / 55 | 1.13 MB |
| 14 | Claire, costume 1 | 0..3 as above, 4: 4 skinned (hair); no 5..7 | 107 / 60 | 1.25 MB |
| 15..19 | other costumes | 11-byte placeholder files | | |

Roles of the sub-models [code]:

* `mdl[0]` body (drawn): **one skinned mesh attached to node 1**; all other nodes are pure bones
  without geometry [verified]. `mdl[1]`, `mdl[2]`: only texlists; eye blink swaps
  `plp->mlwP->texP = plp->txp[PlyEyeTab[..]]` (`player.c bhControlPlayerHead` ~6982).
* `mdl[3]`: 22-node untextured copy with a rigid mesh on every node except 0, used for line/ray hits against the player
  (`effsub2.c` ~2311, `bhCollisionCheckL2MDL`).
* `mdl[4]`: Claire's hair (id 1211) / Wesker's 1-node piece (id 1212): a separate object
  `sys->obwp[2]` linked to head node 5 and simulated by `player.c bhCalcHair`
  (`player.c bhSetPlayer`/`bhResetPlayer`, `lkmtab`, only for `ply_id` 0 and 3).
* `mdl[6]`/`mdl[7]` (Claire): used by the moth egg enemy (id 27, `en27.c bhEne27_Init` takes
  `plp->mdl[6]`, `plp->skp[7]` and player motions `93 + type` from `sys->plmthp`).

### The player skeleton (22 nodes) [code + verified]

Identical hierarchy in files 10..14 (per node: has model / child / sibling) [verified]. Tree
(decoded from the pre-order flags):

```
0 -> 1 -> 2 -> 3 -> 4 -> 5            spine, neck, head
               3 -> 6 -> 7 -> 8 -> 9  right arm  (6, 10 are siblings of 4)
               3 -> 10 -> 11 -> 12 -> 13  left arm
          1 -> 14 -> 15 -> 16 -> 17   leg        (14, 18 are siblings of 2)
          1 -> 18 -> 19 -> 20 -> 21   leg
```

Node roles from the code (`PlyFlip`, `PlyCapColTab`, `PlyLegRoute`, `bhSetWeapon`, footstep
code):

| Node | Role (evidence) |
|---|---|
| 0 | root (root motion position, `clp_jno`) |
| 1 | pelvis, carries the body mesh; parent of spine and legs |
| 2, 3 | spine, chest (capsules 1-2, 2-3; `bhControlPlayerHead` sums 0..5) |
| 4 | neck (procedural look, `objP[4].ang`) |
| 5 | head (`hd_pos`, camera look-at in `cut.c`, hair link, `clp_jno`) |
| 6..9 | right arm: 6 shoulder, 7 upper arm, 8 forearm, 9 hand (right weapon `lkono = 9`) |
| 10..13 | left arm, mirror of 6..9 (left weapon `lkono = 13`) |
| 14..17 | leg (`PlyLegRoute[0] = 0,1,14..17`), 17 = foot end (footstep sound/splash) |
| 18..21 | other leg, 21 = foot |

`PlyFlip` (`player.c`) is the left/right mirror map used when a motion is played mirrored
(`mtn_md & 2`). Bone offsets differ per character: RMS difference of the 22 node offsets vs
Claire 0.043 (Chris), 0.059 (Steve), 0.113 (Wesker); head offset y 0.45 vs 0.52/0.45/0.55
[verified]. Per-character constants: `PlyInfo[ply_id]` collision radius/height (Claire 16.5,
others 17.5), `PlKDU`, `PlFootSnd` (`player.c`).

### Attachments ("bones" used by other systems) [code]

* Weapons: `sys->obwp[0]` (right, `lkono 9`) and `[1]` (left, `lkono 13`), `flg 0x81` (linked),
  `weapon.c bhSetWeapon`; per-weapon angles in the hand `WpnAngTab`, muzzle positions `WpnTab`.
* Enemies link to the player or to each other the same way (`flg & 0x80`, `lkwkp`, `lkono`),
  e.g. moth egg on node 3 (`en27.c`), Black Widow children on its own nodes (`en03.c`).
* View clipping uses `clp_jno` (player 0, 5, 9, 13); enemies 41..48 and 91..98 have fixed tables
  in `eneset.c bhSetEnemy`.
* Hard-coded player node numbers appear in ~40 places (`player.c`, `pl_evt.c`, `cut.c`,
  `weapon.c`, `playpch*.c`, enemy grab code): 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15, 17, 19, 21.

### What makes two models animation-compatible [code + verified]

1. **Same node order and parentage** (the flat array order = tree order). Motions address node
   `k` by position `k` (`NormalTbl`), mirrored motions through `PlyFlip`.
2. **Node count >= track count**: `SetMtn*` loop over `mnwP[mtn].obj_num` tracks and write
   `objP[k]` with no bound check; a motion with more tracks than the model has nodes writes past
   the object array (memory corruption) [code]. Player motions have 17..22 tracks; Claire's also
   5-track motions for `mdl[7]` [verified].
3. Similar bind orientation of each joint (rotations are absolute local Euler angles, not
   deltas). Proportions may differ: player motions carry no position keys below the root
   [verified], so each model keeps its own bone lengths.
4. For the player specifically: nodes 5, 9, 13, 17, 21 must be head, hands and feet, and the
   file should provide `mdl[1..3]` (blink texlists, hit proxy).

## 3. Animation (B)

### Playback [code]

`Motion.c bhSetMotion(ew, add, mode, flip_tbl)`, called by every actor once per 30 Hz tick,
followed by `bhCalcModel`:

* `frm_no` is 16.16 fixed point; `add` (`mtn_add`, usually 65536 = one frame per tick) advances
  it; wrap returns 1 (end) or -1 (start), used by the AI to detect motion end. `mode & 1`
  ping-pongs.
* Track writer chosen by the motion's `flg` / `mode`:
  * *Fast* (default): root position from track 0 `p[0]`, rotations for all nodes; integer frame.
  * *Normal* (`flg & 0x10`): position keys for every node that has them.
  * *Slow* (`flg & 0x20` or `mode & 8`): like Normal with linear interpolation between frames
    using the fraction of `frm_no` (sub-frame playback, slowed animations).
* Blending ("hokan"): while `hokan_count > 0`, each node moves from its current pose toward the
  target by `rate = (65536 - hokan_rate) / 65536` per tick (or a linear schedule with
  `mode & 0x20`), `hokan_count--`. Changing motion = set `mtn_no`, `frm_no = 0`,
  `hokan_count` (e.g. 3..8) and `hokan_rate`. `mode & 0x100` snaps positions.
* `mode & 2` mirrors (negates x position, y/z angles, remaps nodes through `flip_tbl`).
* `atrP[frame]` -> `ew->mtn_attr` (per-frame event attributes).
* `bhGetObjMotion` reads one node's key without applying it; `bhFixPosition`/`bhCalcFixOffset`
  compensate root drift.

### Where motion sets come from [code + verified]

| Set | Pointer | Source | Used for |
|---|---|---|---|
| Player base | `plp->mnwPb` = `sys->plmthp` | part file, slots 0.. | walk/run/aim/damage/etc. |
| Player weapon | `plp->mnwP[100..]` | weapon file `wpnr_no + ply_id*30 + 20` (`dread.c bhReadWeaponData`) | gun stances/firing; files exist for all 30 weapon ids x 4 characters [verified]; Claire's weapon 0/1 files and the others' weapon 0 file have an empty motion block (`wmt_size` stays 0 with the knife) [verified]; `docs/COOP.md` assumes the no-weapon file carries motions 100+, which the parse does not confirm |
| Room | `sys->rmthp` | room file, header `hed02` after the enemy entries | event acting of the player (`plp->mnwP = sys->rmthp`, e.g. `player.c` ~4397, `event.c` ~1904) and of enemies/NPCs (`eneset.c bhEne56`, `bhInitMotionPause*`) |
| Enemy | `ep->mnwPb`, shared per type `sys->emtp[id]` | room file, one entry per record (`room.c bhSetEneMtn`, 512 slots) | enemy actions |
| Enemy-owned player motions | `plp->mnwP = ep->mnwP` | inside the enemy's set | grabs/bites: the player is animated from the enemy's set, motion chosen by `sys->ply_id`, e.g. zombie `En01_PlyMtn_OffsetTbl = {400, 425, 450, 425}`, bat `ply_motion[3][3]` (30..38), `en13.c mtn[3][8]` |

`plzmtp` (z-translation per motion, `bhGetTransZ(PlyTrsZ[..])`) belongs to the base set.

### Player motion selection [code]

`player.c bhControlPlayer` -> `bhCtrPly_mode0[mode0]` -> action handlers `bhCPM2_act_*`
(std, srt, sta, wlk, run, bak, bk2, kdu/kdd stairs, dnu/dnd, psh, hsu/hsd ladders, wpn, atk,
rld, knf, ...). Locomotion numbers come from `PlMtnAct[wpntp][dmlvl][k]`: `wpntp` = 0 below
weapon 10 else 1 (`EXP_WORK.wpntp`), `dmlvl` = health level 0/1/2 (fine/caution/danger,
from `hp >= 120 / >= 30`), `k` = column per handler (std 0, srt 1, wlk 2, run 3, bak 4, bk2 5,
sta 6; e.g. standing = 42/43/44 by health).
Weapon stances use `PlMtnWpn = {100, 104, 109, 114, 101}`. Numbers are shared by all four
characters; each part file fills the same slots.

### Enemy motions [code]

Per type `bhEneNN_*` code sets `ep->mtn_no`, `hokan_*`, `mtn_add` and calls `bhSetMotion`
itself (e.g. `en07.c bhEne07`). The set is per type (`sys->emtp[id]`); the first record of a
type in a room normally carries the data, later records reuse it (`bhFinishRoom`); a record
without data and no earlier set gets an empty zeroed set. Runtime children often share the
parent's set (`ep_leader->mnwP = epw->mnwP`, `en03.c`).

## 4. Player mesh replacement (C)

### Options

| Option | Effort | Result |
|---|---|---|
| A. Another character's part file as mesh, keep own motions | ~20 lines glue | works for Claire/Chris/Steve/Wesker/costume in any combination [verified for Claire -> Chris, Claire -> Wesker] |
| B. Change the file id only (load Chris's file while `sys->ply_id` = 0) | 1 line | Chris's mesh **and** motions, Claire's logic/weapon files/voice [inferred] |
| C. Change `sys->ply_id` | 1 line | full character switch (what `bhPlayerChangeSet` does); events, items, voice change too; Wesker (3) overruns 3-row enemy grab tables (`en02/en07/en13/en23 mtn[3][..]`) [code] |
| D. Enemy/NPC mesh as player | done (section 8) | the story cast and a few NPCs are built on the player's skeleton (+5 face nodes): node map, no rest-pose correction needed (all bind angles 0 [verified]); other models play their own idle motion |
| E. Custom mesh | large | the body is one skinned chunk mesh on node 1 of a 22-bone skeleton, so a new mesh = new chunk model + `SKIN` weights against the existing bones (offsets may change), `mdl[1..3]` (blink texlists, hit proxy) and TIM2 textures with unused `Gindex`/CLUT banks; no tool for this exists in the repo |

### Recipe for option A (tested code, from the probe)

Run after `bhReadPlayerData` + `bhReadWeaponData` completed (game start `bhSysCallMonitor`
mode 1 case 9 or a character switch mode 4 case 7); in glue, at the frame hand-off when the game
task runs, is enough:

```c
static unsigned char file[1 << 21] __attribute__((aligned(64)));      /* part file as read */
static unsigned char bmt[393216] __attribute__((aligned(64))), mth[12288] __attribute__((aligned(64))),
                     zmt[8192] __attribute__((aligned(64)));            /* sink for the other motions */
/* read sys part file `id` into `file` (ADXF_OpenAfs/ADXF_ReadNw32, see coop_glue.c read_part) */
unsigned char *memp_b = sys->memp, *bmt_b = sys->plbmtp, *mth_b = sys->plmthp, *zmt_b = sys->plzmtp;
unsigned int bmt_size_b = sys->bmt_size;
MN_WORK *mnw_b = plp->mnwP, *mnwb_b = plp->mnwPb;
for (i = 0; i < plp->mdl_n; i++) if (plp->mdl[i].texP) njReleaseTexture(plp->mdl[i].texP);
bhGarbageTexture(NULL, 0);
for (i = 0; i < 16; i++) plp->skp[i] = NULL;      /* the loader leaves stale skin pointers */
sys->obwp[2].flg = 0;                              /* Claire/Wesker hair object points into old data */
sys->memp = file; sys->plbmtp = bmt; sys->plmthp = mth; sys->plzmtp = zmt;
bhReadPlayerData();                                /* models -> sys->plmdlp, textures uploaded */
sys->memp = memp_b; sys->plbmtp = bmt_b; sys->plmthp = mth_b; sys->plzmtp = zmt_b;
sys->bmt_size = bmt_size_b;
plp->mnwP = mnw_b; plp->mnwPb = mnwb_b;            /* keep the original motions (incl. 100+) */
plp->mlwP->owP[7].flg |= 0x8; plp->mlwP->owP[11].flg |= 0x8;   /* as bhSetPlayer */
```

Notes:

* The model goes into the normal `plmdlp` buffer (131072 bytes; the four files' model blocks
  are 48..57 KB plus `O_WORK`s). Textures are copied into the texture heap, so `file` can be
  reused. `sys->hd_pos` becomes the new head offset (fine).
* `bhReadPlayerData` restores `sys->memp` to its *input* (the file buffer), hence the explicit
  restore. `freemem` need not be moved because `plmdlp` lies in the arena (unlike
  `coop_glue.c`'s static buffers).
* Re-apply after every event that reloads the player: game start/load, retry, story character
  switch (`cb_flg & 0x80`), and after `njReleaseTextureAll`.
* Keeping `sys->ply_id` keeps everything else of the character: `PlyInfo` size, weapon files and
  their models (they sit in the new hand node 9 correctly), voice bank
  (`RequestPlayerVoiceSoundBank(ply_id)` -> `CORE_%03u.SPQ`; on the PC the voice follows the
  model shown, section 8 "Voices"), inventory, enemy grab motions,
  room event motions.

### What breaks or needs care [code unless marked]

* Claire's hair (`obwp[2]`, `mdl[4]`) and Wesker's piece: re-created by `bhSetPlayer` /
  `bhResetPlayer` for `ply_id` 0/3 from `plp->mdl[4]`; with a 4-sub-model file (Chris/Steve)
  `mdl[4]` is stale (the loader never clears entries beyond the new `mdl_n`; they point into
  the overwritten `plmdlp`) -> must stay disabled (or keep the original `mdl[4]`, giving a ponytail on the
  new head). The probe disabled it; nothing else complained [verified].
* Moth egg (id 27) uses `plp->mdl[6]`, `skp[7]` and player motions 93..95: only Claire's file
  has them; with another mesh on Claire, an egg would use a stale `ML_WORK` -> crash [inferred].
  Keep Claire's `mdl[5..7]` (copy the `ML_WORK`s and `skp` before the swap and load the new
  file into a second buffer) if this matters.
* Texture budget: 68 entries in use before, 69 after swapping to Chris, 74 to Wesker (the old
  entries are freed) [verified]. Palette banks of the four players do not collide with each other
  (co-op shows both); with other rooms' assets this is unverified.
* Facial animation: blink swaps texlists `txp[0..2]`; lip sync / masks for NPCs only.
* Proportions: Wesker's taller model with Claire's motions keeps feet on the floor because only
  the root is positioned [verified visually]; hand-to-object contacts in events are authored for
  the original proportions (small offsets).
* Option B/C also change `plzmtp` (walking speed per step) and weapon motions.
* Cutscenes [verified]: at their end (and in their skip events) room scripts re-attach the
  player's objects with op 52 `ObjLinkSetPly`: weapons to nodes 9/13, and `sys->obwp[2]` to the
  head node 5 at `0, 1.58, 0.77` - Claire's ponytail spot, written for every character (87 of
  the 213 uses in the room scripts). On another mesh's head part this is wrong: Wesker's
  sunglasses went behind the head (headless test: RM_0020's skip event 12 run with
  `Event_exec`, the glasses' offset in head space went from `0, 0.85, -1.03` to
  `0, 1.58, 0.77`). Scripts also change only these link fields (`lkwkp`, `lkono`, `lox/loy/loz`,
  `flg 0x80`), so the co-op mesh swap keeps the part's own values and puts them back every tick
  (`coop_glue.c self_keep`, after `bhControlPlayer`, before the objects follow their links).
  The same for node positions: 8 room motions (rooms 9090, 9370) carry position keys below the
  root, i.e. the played character's bone lengths, which a mesh of another character would keep
  after the cutscene; the swap restores the mesh's own node offsets 1..21 every tick (self and
  co-op partner).

## 5. Enemies (D)

How rooms spawn and keep their enemies (script conditions, kill flags, respawn, difficulty,
budgets, co-op), the census tool and the randomizer/difficulty plans: `docs/ENEMIES.md`.

### Data flow [code]

1. Room file: `ROM_WORK` (`rom`, copied from the file, pointers relocated): `enep`/`ene_n`
   (enemy records, `ETTY_WORK`/`EGG_WORK` 0x24: `flg`, `id` (type), `type` (variant), `flr_no`,
   `mdlver` (model version), `wrk_no` (target slot), `prm1`, `px,py,pz`, `ax,az,ay`, `aspd`,
   `hide[4]`), `objp`/`obj_n`, `itmp`/`itm_n`, `effp`/`eff_n`, `evtp` (script), atari tables.
2. `room.c bhSetRoom`: for each record with `flg & 1`: `bhSetEneMdl(model, rec, mdlver, i)` ->
   `eneset.c bhSetEnemy(rec)` takes **the first free slot** in `ene[128]`, zeroes it, copies
   flg/id/type/flr/mdlver/position/`ay`, sets defaults (`mtn_add 65536`, scale 1, `mode0 = 0`).
   Then the model chunks (`"MASK"` -> `mskp` face mask, `"SKIN"` -> `skp`, else an MLB) are
   realized into `ep->mdl[]`. Textures are shared per `et_lp[id][mdlver][k]`: the first record
   owns them (`flg |= 0x200`).
3. `bhFinishRoom`: per record `i` (assuming slot `i` = record `i`, as `wrk_no` does) motion
   set from the header table or the type's earlier set `sys->emtp[id]`; `O_WORK`s; initial
   `bhCalcModel`; `bhInitMask` for ids > 90.
4. Per tick `eneset.c bhControlEnemy` (if `sp_flg & 2`): for slots `0 .. sys->ewk_n-1` with
   `flg & 1`, not `stflg & 0x41000000`, not linked to a hidden parent: `bhJumpEnemy[id](ep)`
   (the type's code: `mode0 == 0` runs its Init, which allocates work memory with
   `bhEne_CallocWork`, sets hp, collision, shadow, motion), then `bhEne_InitDamage`.
   `sys->enow` = current slot (used for sound slots).
5. Draw: `bhDrawEnemy` (clip by `clp_jno` unless `mdflg & 0x20`, hidden by `mdflg & 1`;
   morphing when `mdflg & 2`); `flg & 0x80000000` enemies are drawn earlier as "enemy objects".
6. Death: the type sets `flg & 2`; script op 13 `bhDieCk(record, ed_flg bit)` records it in
   `sys->ed_flg`; on the next visit op 34 `bhEneSetCk` hides the slot (`stflg |= 0x1000000`).
   Which records respawn is therefore decided by the room script.

### Enemy types (`bhJumpEnemy`, names from the `// ENEMY:` comments in the source)

| id | Code | Name / role |
|---|---|---|
| 0 | `eneset.c bhEne00` | (no comment) static object that explodes when damaged (`bhSetExplosion`), wall collider |
| 1 | `en01.c` (+`en01b`, `en01sub`) | Zombie; 16 variants by `type` (`bhEne01_InitType[16]`); detachable head/arms/legs/worm etc. as helper slots 31..38 |
| 2 | `en02.c` (+`en02sub`) | Gulp Worm (sand effects, camera control) |
| 3 | `en03.c` (+`en03sub`) | Black Widow (spawns id 31 helpers and 15 id 24 babies) |
| 4 | `en04.c` | Cerberus |
| 5 | `en05.c` (+`en05sub`) | Hunter/Sweeper |
| 6 | `en06.c` (+`en06sub`) | Moth (lays id 27) |
| 7 | `en07.c` | Bat |
| 8 | `en08.c` | Fish |
| 9 | `en09.c` | Bandersnatch |
| 10, 20, 71 | `en10.c`, `en20.c`, `en71.c` | Ant A, Ant B, Ant C |
| 11 | `en11.c` | Spotter |
| 12, 13, 14 | `en12.c`, `en13.c` (+`en13sub`), `en14.c` | First, Second, Third Form Alexia (13 spawns id 18 tentacles) |
| 15 | `en15.c` | Nosferatu |
| 16 | `en16.c` | Alfred Ashford |
| 17 | `en17.c` (+`en17sub`) | Monster Steve |
| 18 | `en18.c` | Alexia's Tentacle |
| 19 | `en19.c` | Tyrant T-078 |
| 21, 22 | `en21.c`, `en22.c` | Albinoid Infant, Albinoid Adult |
| 23 | `en23.c` | Giant Black Widow |
| 24 | `en24.c` | Baby Black Widow |
| 25 | `en25.c` | Ceiling Trap |
| 26 | `en26.c` | Anatomist Zombie |
| 27 | `en27.c` | Moth Egg (on the player; uses the player's model/motions) |
| 28 | `subpl.c bhEne28` | empty |
| 29 | `en29.c` | Tentacle |
| 30 | `en30.c` | Alexia's Baby |
| 31..52, 57..70, 72..99 | `subpl.c bhSubpl` | scripted NPC actors ("sub players"): play room-script-selected motions (`subpl_mv00_tbl`), no AI. 41..48 and 91..98 get player-like clip tables, > 90 face masks/lip sync |
| 53 | `en15.c bhEne53` | Nosferatu's core (`CoreInit/CoreMove/CoreDie`) |
| 54, 55 | `en54.c`, `en55.c` | Armored Knight A (sword, medium shield), B (lance, large shield) |
| 56 | `eneset.c bhEne56` | scripted object driven by `sys->ufo_*` and `ev_flg 0x3B`, room motions |

Slots 31..38 are overwritten at runtime with helper functions (`bhEne_SetCallFunc`):
31 by ids 2, 3, 5, 6, 13, 23 (and 17: 31, 32), 31..38 by id 1 (zombie parts, `en01.c` ~2605).
The table is global and never restored, so two of these types in one room run the wrong helper
code [code]; the retail data never combines two slot-31 users in a room [verified census]. PC:
in rooms with added enemies each part runs what its creator's type registered (`eneset.c
pc_call_enemy`, ENEMIES.md 2.2).

Room census over all 205 room files [verified]: 180 contain enemy/NPC records; at most 76
records and 8 distinct ids in one room; enemy ids 18, 27, 28 never appear in room data (runtime
only); zombie variants `type` 0..14; 165 rooms contain item records, at most 13.

### What a type needs to work in a room [code + inferred]

* Model `ep->mdl[]` (+ skin, + mask for ids > 90), realized; texture uploaded.
* Motion set (512 `MN_WORK`) with the numbers its code uses, including the player-grab motions
  for the current `ply_id` (zombie 400+, etc.).
* Work memory from `bhEne_CallocWork` (room arena). Init code does not check for NULL [code].
* Helper slots 31..38 free of conflicts (above); child types it spawns (24 for 3, 18 for 13,
  27 for 6) with their models (they share the parent's `mdl[1]` etc.).
* Effects: `bhSetEffect`/`bhSetEffectTb` ids 30..99 and 400+ need textures loaded from the
  room's effect list (`bhSetRoom`, `rom->effp`); others are global. What counts is the effect's
  texture id, not its effect id: global enemy effects of Alexia and the Gulp Worm draw with room
  effect textures 80..83; the spawner loads them (ENEMIES.md 6, "Effect textures of added
  types") [verified].
* Sounds: `RequestEnemySe(sys->enow, pos, se)` plays from the room's sound bank
  (`RM_SRRC.SPQ`, only 6 enemy sound slots `EnemySlotInfo[6]`, `EnemyInfo[128]`); a foreign type
  plays wrong or no samples [inferred].
* Environment assumptions: bosses and set pieces (2, 11..19, 25, 29, 53..56) rely on their arena
  geometry, cameras (`CameraControl`), event flags (`en15.c`, `bhEne56`) and room scripts.
  Free-roaming types suitable for spawning: 1, 4, 5, 7, 9, 10, 20, 24, 26, 71, 21/22/8 (water),
  3, 6, 23, 30 (with their children) [inferred].

### Recipe 1: spawn a clone of a type the room already loaded (tested)

```c
/* src: a loaded enemy of type `id`; rec: its record */
BH_PWORK *src = NULL, *ep; EGG_WORK egg;
for (i = 0; i < rom->ene_n; i++)
    if (ene[rom->enep[i].wrk_no].id == id && ene[rom->enep[i].wrk_no].mdl_n) { src = &ene[rom->enep[i].wrk_no]; egg = rom->enep[i]; break; }
if (!src) return NULL;                                  /* type not in this room */
egg.px = plp->px + 10.0f; egg.py = plp->py; egg.pz = plp->pz; egg.flr_no = plp->flr_no;
if (!(ep = bhSetEnemy(&egg, 0))) return NULL;           /* 128 slots full */
ep->mdl_n = src->mdl_n;
for (j = 0; j < src->mdl_n; j++) {
    ep->mdl[j] = src->mdl[j];                           /* share nodes, models, textures */
    ep->mdl[j].flg &= ~0x200;                           /* the room record owns the textures */
    ep->mdl[j].owP = bhEne_CallocWork(src->mdl[j].obj_num * sizeof(O_WORK), 0);   /* own matrices */
    ep->skp[j] = src->skp[j]; ep->mbp[j] = src->mbp[j]; ep->txp[j] = src->txp[j];
}
ep->mskp = src->mskp;
ep->mlwP = ep->mdl;
ep->mnwP = ep->mnwPb = sys->emtp[id] ? sys->emtp[id] : src->mnwPb;
bhCalcModel(ep);                                        /* type Init runs on its next tick */
```

* Call it on the game thread between ticks (`cheat_glue.c cheat_frame`), only while the game task
  runs (`sys->tk_flg & 0x80`, not suspended) and not during door transitions/events
  (`gm_flg & 0x9`, `cb_flg & 1`), like the teleport cheat.
* Sharing `objP` between instances is what the game itself does for children (`en03.c`,
  `mlwP = &epw->mdl[1]`): each enemy writes the pose and immediately computes its own matrices.
  Side effect: a motion blend starts from the other instance's last pose [inferred, harmless].
* The clone has no record: scripts never see it, it is not saved, it is gone after a room change
  (`bhInitEnemy`), its kill is not recorded. It uses the type's sound slot logic as any instance.
* Verified: a bat clone in the demo room initialized, flew and followed the player for 20 s;
  `ewk_n` 5 -> 6, +2.8 KB arena [verified].

### Recipe 2: spawn a type that is not in the room (design; implemented as the F2 enemy spawner, docs/ENEMIES.md section 6)

1. Find a room file containing the type (scan once: `GetFileSize`, `ReadFileEx`, `Expand` as in
   `bhSysCallMonitor`; ROM block at `hed00` (+16), record array and count in it).
2. Keep that file expanded in a static buffer (up to 8.9 MB) for as long as the enemy may exist:
   the realized model points into it.
3. Model: the model offset table at `hed01` (+20) has the room model first, then one entry per
   record with `flg & 1`, in record order. Realize the record's chunks like `bhSetEneMdl` but
   without its `bhSetEnemy` call and without touching `sys->et_lp` (or call it on a dummy
   record and then copy). Realize only once per buffer (in-place relocation).
4. Motions: table at `hed02` (+24), one entry per record; take the first non-zero entry of the
   type and realize it like `bhSetEneMtn` (allocates 12 KB from `sys->memp`).
5. Textures: table at +32: count, room texture, then one entry per *texture-owning* enemy model
   in `bhSetRoom` order (record order, sub-model order, first owner per `(id, mdlver, k)`); so
   the table index must be computed by simulating that loop over the source room's records.
   Upload with `bhSetMemPvpTexture(mdl.texP, data, 0)`; release with `njReleaseTexture` before
   the room is left (hook the door transition), because `bhReleaseMainTexture` only knows room
   records.
6. Accept wrong colours (CLUT banks), missing effect textures and wrong sounds unless the effect
   list / sound bank of the source room are loaded too; avoid slot-31 conflicts; check free
   arena (`endp - memp`) and texture entries first.

## 6. Randomizer (E)

### Where things are placed [code]

* Enemies: room file records (`rom->enep`); models/motions/textures per record in the same file.
  Script references by record index (`rom->enep[k].wrk_no`): op 7 `bhCmpW` (enemy hp test),
  9 `bhSvW` (set hp of records 0..4), 13 `bhDieCk`, 33 `bhDieEventCk` (count `ed_flg` bits),
  34 `bhEneSetCk`, 36 `bhInitModelSet` (show/hide), 43/45/48 motion pause and room-motion
  acting, 81/83/84 object links with enemies, 121/122 enemy sounds, 125
  `bhMotionPosSetEnePly`, 142 `bhZombieUpDieCk`, 153 `bhEnemyHpUp` (`event.c bhScenarioJmpT`).
* Items: room file records `rom->itmp` -> `sys->itwp[i]` (`objitm.c bhSetItem`, `id` = item id
  0..155 as in the inventory, model from the record). Pickup trigger: an `etc` atari of type 4
  whose `prm0` is the item slot (`hitchk.c bhCheckExmAtari`: `sys->sb_id = itwp[prm0].id`).
  Amount: `item.c getbulletmax[id][gm_mode]` for stackables plus special cases in `sub1.c`.
  Persistence: script op 14 `bhItmCk(it_flg bit, etc index)` sets the `it_flg` bit when picked
  up and hides the item; op 35 `bhItmSetCk` hides it on later visits. The flag bit is a script
  literal, bound to the trigger, not to the item id.
* Key items: `map.c ItmDat` lists 22 ids the map tracks; door locks and key uses are room-script
  checks (ops 16 `bhUseItemCheck`, 17 `bhPlItemCheck`, `ky_flg`, door attributes in `etc` type 0).
  Items given or taken by events (op 39 `bhArmsItemChange`, `bhPlItemLost`, scripted inventory writes) are outside the records.
* Flags (`SYS_WORK`): `ev_flg[32]` story, `it_flg[16]` picked items, `ed_flg[32]` dead enemies,
  `ky_flg[16]` keys/locks, `mp_flg[8]` map, `ic_flg[16]` item check state; script ops 4/5
  (`bhCk`/`bhSet`) address all of them.

### Safe approach

Patch **after** the room is loaded (`bhFinishRoom`, before `bhInitEvent`'s first script tick)
or patch the decompressed room file in memory right after `Expand` (before `bhSetRDT`).

Enemies:

* Never change record count or order (`wrk_no`, script indices, motion/model/texture table
  order all depend on it).
* Swapping `id` of a record also needs the model, motions and textures of the new type: either
  choose from types already present in that room (shuffle within the room: swap models by
  swapping slots, cheap and safe), or load foreign data as in Recipe 2 (with its risks).
* Keep `type` in range for the new id (zombie 0..15 via `bhEne01_InitType[16]`, other ids
  mostly 0..3; census maxima per id are a guide).
* Skip records referenced by script ops 36/43/45/48/81/83/84/125 (scripted acting or links),
  boss and set-piece ids, NPC ids >= 31, ids that need children (or bring the children), and
  any combination of two slot-31 users or zombies with a slot-31 user in one room.
* Keep `ed_flg` semantics: a randomized enemy that dies still sets the record's bit, so the
  room script logic stays consistent.

Items:

* Change `sys->itwp[i].id` after load: the pickup reads it (`sys->sb_id`), the flag bit stays
  with the trigger, so persistence is unaffected. The displayed 3D model stays the original's
  (models come from the record); swap whole `itwp` entries between items of one room to keep
  model and id together.
* Progression: keep key items obtainable before the locks that need them (needs a room graph;
  `etc` type 0 door records give stage/room/pos targets) and do not move items that scripts
  give or check (ops 16 `bhUseItemCheck`, 17 `bhPlItemCheck`, 77 `bhPlItemBlockCk`).

Main risks: script/slot mismatches (wrong enemy hidden or animated), out-of-range `type`
(function table overrun), motion track > node count (memory corruption), missing children or
helper conflicts, texture/arena exhaustion (`exit(0)` on texture overflow; no check on arena
overflow), wrong sounds/effects for foreign types, softlocks from moved key items.

## 8. Any model as the player (implemented)

"You appear as" (F3 window) offers, after the player files (Claire, Chris, Steve, Wesker, Claire's
second costume), 62 models found in the room files: the story cast, the creatures and the zombie
variants (`pc/src/host/actors.c`). It applies to our own player in single player and in co-op
(where the partner sees it too), and in cutscenes to the event double that acts for the
character played.

### The list [code + verified]

The catalogue has 62 models; the menus (F3, the lobby's MODEL row, PC SETTINGS -> CO-OP) list
one entry per distinct look, 50 of them after "As played" and the five player files: 56 entries
instead of 68. All three call the same functions in `actors.c` (`appear_count`, `appear_at`,
`appear_pos`, `appear_step`, `appear_name`, `appear_canon`); the list is filtered for display
only. Catalogue indices and `g_coop.chr` values are unchanged, so `Pose.chr`, the lobby's
`PoseExt` choice, the cutscene doubles (`actor_doubles`, slot 2) and older builds work as before;
a hidden value in `recvx.ini` (`coop_char`) is mapped to its listed twin when the file is read.

Left out (`twins[]`, compared in the lobby preview from several sides, frame dumps):

| Hidden (catalogue index) | Shown instead | Why |
|---|---|---|
| Claire (cutscene model) 0 | Claire (player file) | same look; the double with the face mask |
| Chris (cutscene model) 1, Chris (event model) 2 | Chris | same look |
| Steve (cutscene model) 3, Steve (event model) 4 | Steve | same look |
| Wesker (cutscene model) 5, (event model) 6, (event model 2) 7 | Wesker | same look; the doubles wear the sunglasses as a separate prop, so they had none |
| Alfred Ashford 2 (9), Alfred Ashford (event model) 11 | Alfred Ashford (8) | the same uniform (the event model without the trouser stripe) |
| Tyrant 2 (18) | Tyrant (17) | the same with one blood stain |
| Bandersnatch 2 (23) | Bandersnatch (22) | only its long arm (the event piece through the wall) |

Kept although related: Alfred Ashford 3, now "Alfred Ashford (wounded)" (blood-stained
uniform), Hunter 2, now "Hunter (Sweeper)" (red/purple skin), and every zombie variant (each a
different model version with its own body, clothes or wounds). Order unchanged: As played, the
player files, the cast, the creatures, the zombies. Check: ctest `appear` (`pc/tests/test_appear.c`).

### Cutscenes [code + verified]

* Pre-rendered movies (FMV) cannot change: the opening and ending movies and 17 in-room movies
  started by script op 94 `MovieStart` (rooms 0000, 0020, 1050, 1090, 5000, 5001, 6060, 6100,
  9150, 9280, 9300..9302, 9320, 9360).
* Everything else is in-engine. 142 rooms have in-engine cutscenes (`CineSet 0`). In 77 of them
  the character played is the player model itself (hidden, or acting with the room motion
  set): the replacement applies as everywhere else. In the others the script hides the player
  and an event double acts: an enemy record of the story cast with a face mask (lip sync):
  Claire 91 (38 rooms), Chris 92 / 42, Steve 93 / 43, Wesker 97 / 49 / 47.
* `eneset.c bhDrawEnemy` calls `actor_put_enemy` in place of `bhPutModel(ep)`: a double of
  `sys->ply_id` is drawn as the chosen model (display slot `ACTOR_DOUBLE`), posed from the
  double's skeleton like the player's (below). The chosen model is loaded at the hand-off while
  the room has such a double (player files too: `ACTOR_PART + n` loads part file `10 + n` as a
  display model). Creatures without a humanoid node map do not replace doubles. The double is
  posed every tick before the objects follow their links, so what the script links to its hands
  (the cast's hand pieces, props) sits on the model's hands; with the option "Talking faces in
  cutscenes" a cast model also moves its mouth with the double's lines (below, "Cutscene doubles:
  hands and talking faces").

### How [code + verified]

* **Catalogue** (`actors.c`): per model the room file and enemy record that carry it, its type
  and model version (checked at load), its animation set (a man's or a woman's player motions,
  or its own idle; below) and its idle motion. Chosen by a census over the 205 room files: the smallest room holding the model
  and its type's motion set. Only ids, no disc data.
* **Loading** (`pc/glue/actor_glue.c`): the room file is read from `RDX_LNK.AFS` (partition 17,
  index in `rdx_files[]`) and expanded into a static 12 MB buffer (the game's own arena size,
  so any room the game loads fits). The record's model chunk list (`hed01`) and its type's
  motion list (`hed02`) are copied into a buffer of the display slot (1.5 MB) and realized as
  `bhSetEneMdl` / `bhSetEneMtn` do. Textures: the texture table (`hed04`) has, after the room
  model's entry, one entry per first `(type, model version, textured sub-model)` in `bhSetRoom`
  order; the loader replays that order and uploads its model's entries with
  `bhSetMemPvpTexture` (same TIM2 index = shared refcounted entry: a model the room already
  shows costs nothing). Player part files: `bhReadPlayerData`'s layout. Released when the model
  changes; `njReleaseTextureAll` drops them. Refused (the character's own model stays) with fewer
  than 32 texture entries or 2 MB texture heap left. 0..654 KB per model.
* **Display actor**: a `BH_PWORK` per slot (our player, the co-op partner, the cutscene double),
  drawn in place of the source (`game.c` `actor_put_player`, `coop_draw_remote`,
  `eneset.c` `actor_put_enemy`). The source stays what the game animates, collides with, clips
  with and links weapons, the head part and effects to; it is only not drawn.
* **Humanoid retarget** (the cast, Alexia's first form, zombies, Tyrant, Hunter, Nosferatu,
  Bandersnatch, Monster Steve, the anatomist): all these models, like the player, have bind
  angles 0 and a T-pose. A node map is found at load from the rest skeleton (`rig_build`):
  pelvis = node 1; legs = chains going down from it (hip, knee, ankle, toe; side by x);
  spine = up from the pelvis to the first node with a sideways child (chest); neck/head = up
  from the chest (or the only child of the neck: the hunter's head hangs in front); arms =
  sideways chains from the chest or a spine node below it (clavicle if the first bone is
  shorter than 1 unit, then upper arm, forearm, hand); a child at its parent's own position (a
  zero-length bone) is passed through and the chain goes on below it; a sideways chain that ends
  further up or down than out is a tentacle or tail and stays unmapped. The source's skeleton (the player,
  the partner, a double) is mapped the same way from its current node offsets. Every tick, per
  mapped node, the model's world rotation = rest correction x the source node's final world
  rotation, where the rest correction turns the model's rest bone direction (towards the next
  node of the chain) onto the source's; positions come from the model's own bone offsets and
  the root takes the source's root position (feet on the floor). Unmapped nodes follow their
  parent rigidly (fingers, claws, face, the Nosferatu's tentacle, the Bandersnatch's long arm
  past the hand, zero-length bones). The node angles are set to the local rotations (world x
  parent world^T, `euler_xyz`) for the skin's joint blending; the left arm is solved by IK
  when both hands are on a weapon (below, "Hands, joints, lighter and aim"). The source's world matrices are read after `bhControlPlayer` and again at
  draw time, so third-person aim pitch (`tps_calc_model` bends nodes 2 and 3) carries over
  [verified: the chest's up vector follows the source's within the rest correction]. The
  source's hand nodes 9 / 13 are moved to the model's hands (turned as the pose's hands) for
  the tick's effects, objects and drawing, so weapons sit in them and the muzzle flash starts
  there; the game's own come back before the next `bhControlPlayer` (`actor_hands_back`).
  Walk, run, back steps, aim, fire, grabs (the player plays the enemy's grab motions) and
  cutscene acting all come through. Limits: skinned areas near joints whose rest orientation
  differs (shoulders of zombies in T vs arms down) stretch a little; proportions differ (a
  Tyrant's long arms with Claire's arm angles), weapon grips are where the model's wrist is.
* **Other models**: their own motion set's idle motion (motion 0) looped, gliding with the
  player (bat, moth, spiders, Cerberus, albinoids, fish, spotter, ant).
* The head part (`sys->obwp[2]`, ponytail or sunglasses) is disabled while a catalogue model is
  our player's and set up again afterwards.
* **Without co-op**: `game.c` calls `coop_control_remote` also while `appear_on` (set at the
  hand-off when an appearance other than the character played is chosen or still shown);
  without a partner it only does the appearance work. With "Same as played" nothing runs, as
  before.
* **Outside a room** (lobby preview): `actor_set(slot, idx)` needs only the boot state (AFS
  partitions, texture manager, `sys`); `actor_pose_alone(slot, base)` poses it at an NJS base
  matrix with its idle motion or rest pose; `actor_draw(slot)` needs the caller's camera
  matrix and lights.

### Gender-matched animations [code + verified]

Option "Use gender-matched animations" (PC SETTINGS -> CO-OP and F1, ini `appear_gender_anim`,
default 1). A man shown while Claire is played moves with **Chris's** motions; a woman shown while
a man (Chris, Steve, Wesker) is played moves with **Claire's**. Otherwise the played character's
own motions are used, as before. Gameplay stays the played character's: the hidden real player
is what the game moves, collides, hits and times with.

* **Catalogue**: the `anim` field is the animation set: `ANIM_MALE`, `ANIM_FEMALE` (driven by
  the player's skeleton with a man's / a woman's motions), `ANIM_IDLE` (own idle motion);
  `ANIM_PLAYED` (always the played character's) is kept for models of neither sex but used by
  none now. `appear_motion_chr(anim, ply_id)` picks the set (`actors.c`), player part files
  count by their file (`appear_part_anim`: Claire's two files female, the others male).

  | Set | Models |
  |---|---|
  | male (Chris's while Claire is played) | Chris, Steve, Wesker (player files and every cast/event model), Alfred (all four), Rodrigo, the man in the hazmat suit, Nosferatu, Tyrant (both), Monster Steve, Hunter (both), Bandersnatch (both), the anatomist, every zombie (the 24 variants with motions and the three scripted ones: all male bodies) |
  | female (Claire's while a man is played) | Claire (player files, cutscene model), Alexia Ashford, Alexia's first form |
  | own idle (unchanged) | Cerberus, albinoids, Black Widows, moth, bat, fish, spotter, ant |

* **Motion source** (`actor_glue.c`, "gender-matched motions"): per display slot (0 our player,
  1 the partner) a hidden copy of the source's 22-node skeleton (its objects copied once, its
  bone offsets every tick) with its own motion set: the motion block of part file `10 + chr`
  (Chris 11, Claire 10) and the weapon motions of weapon file `wpn + chr * 30 + 20` over slots
  100.. (re-read when the weapon changes), realized like `bhReadPlayerData` /
  `bhReadWeaponData` into 500 KB of our own per slot. Every tick, after the source's
  `bhSetMotion` / `bhCalcModel`, the copy plays the equivalent motion with `bhSetMotion(add 0)`
  and the source's `mtn_md` and flip table, at the source's phase (frame scaled by the two
  motions' lengths), blending (`hokan_count + 1`, `hokan_rate`) when the source changes
  motion; neck and head (nodes 4, 5) take the source's angles (`bhControlPlayerHead` looks at
  items and enemies); base matrix the source's; for our player the third-person aim bend is
  applied to it too (`tps_bend`, the bend of `tps_calc_model` kept for display copies).
* **Motion mapping** (the numbers are the same for all four characters: `PlMtnAct`,
  `PlMtnWpn` and each part file fills the same slots; frame counts match for most, a parse of
  files 10 and 11 at runtime):

  | Source motion | Shown as |
  |---|---|
  | base set (stand 42/43/44, walk 0..3, run 4/6/7/8, back steps 16..19, turns 9/10, stairs, push, damage, death, ...) and weapon set 100.. (aim per weapon class 100/104/109/114/101, fire, reload) | the same number in the other character's set |
  | a slot the other character lacks (Claire's 90..97: the moth egg and her special motions) | the source's own motion, played on the copy |
  | grabbed: the enemy's set (`plp->mnwP = ep->mnwP`) | the other character's motion of that set: `appear_grab_motion`, first motion per character from the enemy code: zombie 400/425/450/425 (Claire/Chris/Steve/Wesker), gulp worm 21/29, Black Widow 14 * ply_id, Cerberus 100/105, bat 30/33/36, Bandersnatch 100/110/120, Alexia second form 10/16, giant Black Widow 60/66; the source's own where the set lacks it |
  | room set (event acting) and anything else | the source's own motion, played on the copy |

* **Lighter**: the copy gets the game's lighter IK (`bhArmIkMdk` on its arm towards a point in
  front of its own chest, the played character's offsets, hand turned) whenever the source's arm
  is held by it (`owP[7]`, `[9]` flag 2: our player's `bhControlPlayer`, the partner's
  `lighter_arm`); the other set's motions alone had the arm hanging with the flame at the hip.
* **Display**: a catalogue model is retargeted from the copy instead of from the source (same
  node map, same hand fix-up). A player-file mesh on the real skeleton (`self_load`, the
  partner's `p2_other_mesh`) is drawn with the copy's node matrices and angles (`actor_put_body`:
  swapped in for `bhPutModel` and back; the skin blends joints with the angles), and the source's nodes 5, 9 and 13 get the copy's matrices
  before the objects follow their links, so the head part (Wesker's glasses, Claire's ponytail)
  and the weapons sit where the head and hands are drawn.
* **Co-op**: the partner (`ply2`) is handled the same way from its chosen model and the motion
  file it plays (`rx.mfile`); the pose exchange is unchanged (no protocol change). Hooks in
  `coop_glue.c`: `actor_source` before each `actor_pose`, `actor_put_body` for the partner's draw.
* **Not covered**: the lobby preview and the cutscene doubles keep their source's motions
  (no `actor_source`); positions stay the real player's (a grab places Claire for Claire's grab
  motion, Chris's is played there); a motion switch between the matched set and a fallback
  (the source's own) does not blend; Chris's root height and bob come with his motions.
* **Verified** headless (32-bit, room 2/0 fixed camera at 640x480, scripted walk in a circle,
  run, aim and fire the handgun, step back; frame dumps on vs off, not committed): Alfred,
  Wesker and Steve (player-file meshes), the prisoner zombie: Chris's walk, run and his upright
  two-handed aim stance (Claire's leans forward), mean node angle difference to Claire's pose
  4..7 degrees walking/running, 14..30 aiming; glasses and handgun in place. Alexia while
  Claire is played: frames identical to main. Alexia while Chris is played (character switched
  with a temporary probe): Claire's set. Grabs in 1/7: the zombies' Claire grab 406/410 shown
  as Chris's 431/435. Third person (aim pitch up/down): the upper body bends on both kinds of
  model. Co-op echo mode (`RECVX_COOP_ECHO`): the partner (Wesker on Claire's motions) uses
  Chris's. All 27 zombie variants looked at (`RECVX_APPEAR` cycle): male bodies. Without an
  appearance: 413 frame dumps (title to aiming) identical to main.

### Hands, joints, lighter and aim with replacement models (fixed) [code + verified]

User reports: larger models did not hit the crosshair centre in third person; male models held
the lighter wrong; Rodrigo (and others) had a broken walk and hands not connected to the body;
in animations the hands often did not fit the model (off the arms, in the body).

* **Node map, zero-length bones** (`rig_next`): Rodrigo and the man in the hazmat suit have
  their upper arm as a child at the shoulder node's own position (bone length 0). The chain
  search took the child going furthest sideways (> 0.3) and stopped there: each arm had one
  mapped node (map `7=11`, `11=15`, forearms and hands unmapped), so the forearm and hand
  followed the upper arm rigidly, sticking out straight in every motion ("broken walk",
  "hands not connected"), and the weapon stayed at the source's hand. A child at its parent's
  position is now passed through (it follows its parent rigidly): map `7=11 8=13 9=14`,
  `11=15 12=17 13=18`. All 62 catalogue maps compared before/after (`RECVX_APPEAR` cycle):
  only these two changed. Other unmapped arms are by design: Nosferatu's right side (the
  tentacle), the Bandersnatch's left stub, the bandaged zombies' bound arms.
* **Joint blending** (`retarget`): the cast and creature meshes are one skinned mesh (Capcom's
  skin, `npCalcSkin`): a vertex near a joint is the blend of the node's world matrix and that
  matrix with the node's own angles undone (`npInitCalcSkin`: `njRotXYZ(-ang)`, i.e. the
  parent's rotation at the joint). The retarget wrote world matrices only and left the bind
  angles 0, so no joint blended: hard creases at wrists, elbows and shoulders, hands standing off
  the forearm. Each node's angles are now its local rotation (world x parent world^T, decomposed
  as `njRotateXYZ` builds it: Rx Ry Rz, checked against `njRotateXYZ` to 1 unit of 65536). The
  same for player-file meshes drawn in a gender-matched pose (`actor_put_body` swaps in the
  copy's angles with its matrices: the angles were the source's own motion's).
* **Weapon hands** (`actor_pose`): with a gender-matched copy the source's hand nodes got the
  model's hand position but kept the source's (Claire's) hand rotation, so the weapon was turned
  as Claire holds it while the model's arm was posed by Chris's motion: now the copy's hand
  rotation goes with the position.
* **Two-handed grips** (`retarget`, `arm_ik`): the source's left hand near its right (closer
  than 6 units, full below 3: two-handed aim, grips) gives the model's left hand a target at the
  model's right hand + (source left - source right); the model's upper arm and forearm are turned
  by a two-bone IK on the model's own bone lengths (elbow kept in its bend plane, hand keeps its
  rotation), and the positions are placed again from the bone offsets (`place`). Proportions
  that put the hands apart (Rodrigo's, Alfred's, the Tyrant's longer arms and wider shoulders)
  no longer leave the left hand floating beside the weapon. One-handed poses are untouched.
* **The shot** (`actor_hands_back`, game.c before `bhControlPlayer`): the source's nodes 5, 9 and
  13 (head, hands) moved to the model's stayed moved into the next tick. `bhCPM2` fires there,
  before `bhCalcModel`: `bhCheckGunAtari` tests walls from the muzzle at `owP[9]` (and the knife,
  sounds, the lighter light read it too), so a taller model's shots started higher and hit walls
  above the crosshair (the Tyrant +9.6 units, 6 degrees). The game's own matrices are saved
  before they are moved and put back before the next player control (also before the redraw at
  draw time), so the game plays with the real skeleton; weapons, the head part, the muzzle flash
  and the lighter flame are still drawn at the model's head and hands. The third-person side
  (muzzle aim, aim bend for the drawn hand): docs/THIRDPERSON.md "Free-aim pose".
* **Lighter** (`shadow_step`): the lighter is held by an arm IK after the motion (player.c:
  `bhArmIkMdk` on node 6 towards a point in front of the chest node 4, hand node 9 turned, nodes
  7..9 flagged 2 so the motion leaves them). The gender-matched copy played the other set's
  motion without it: a male model (Chris's motions) held the lighter at the hip with the arm
  hanging. The same IK now runs on the copy whenever the source's arm is flagged (the played
  character's offsets), for our player and the partner (`lighter_arm` flags ply2 the same way).

Verified headless (32-bit, 640x480, third person with the test aid `RECVX_TPS_ORBIT` for side
views; room 1/2 ground floor via `RECVX_WARP=1:2:30:20:24:0`, handgun; scripts and contact
sheets in the worktree's `scratch/`, not committed): Claire, Chris's, Steve's and Wesker's player
files, Rodrigo, the hazmat man, Alfred, the Tyrant, the prisoner zombie, Alexia, Alexia's first
form: walk circle, run circle, aim with pitch up and down, fire. Before (the same build with
the fixes switched off): Rodrigo's and the hazmat man's arms straight in every frame, the gun
above Rodrigo's flat hand, Wesker's and the zombie's gun above the right hand (Claire's hand
rotation), the left hand away from the gun; after: elbows bend, the gun in the right hand, the
left hand at the gun. Lighter (`RECVX_EQUIP=20:55:1`): Rodrigo (Chris's motions) held it at the
hip before, now in front of the chest like Claire and Chris's player file. Grab: Rodrigo in 0/6,
grabbed, bitten and pushing the zombie off. Aim numbers: docs/THIRDPERSON.md. Without an
appearance (fixed camera, 0/6 walk, aim, fire, run): 127 of 128 frames identical to main; frame
1000 (a zombie's arm at a table) differs the same way in a build of main with only unused code
added (binary layout, known: an uninitialized read in the decomp), so it is not this change.
Not run with these changes: the co-op partner and the cutscene doubles (the same retarget and
copy code; the doubles get the joint angles and the grip IK, not the hand restore). Still off:
the Tyrant's weapon is lost inside its big claw hand (it sits at the wrist node; not checked:
the Bandersnatch, Hunters, Nosferatu), and in Chris's two-handed stance the left palm lies under the gun rather than
around it on every male model (the motion's own grip).

### Cutscene doubles: hands and talking faces [code + verified]

User report: with another model, in cutscenes the hands did not always attach, and the mouth did
not move when the character talks.

* **Hands are objects.** The cast's cutscene models (ids 91..99) have no hands: the body mesh ends
  at the wrists, and the room script links hand pieces (fist, open hand, holding poses: room
  objects such as 1012/1020/1000/1029 for Claire's double in RM_0000, 1128..1137 for Rodrigo) to
  the double's hand nodes (18 and 22 in its 31-node face mask array, `obwp[k].lkwkp` = the double)
  and swaps them per gesture (`mdflg 1` hides the unused ones). Props are linked the same way. The
  player's hands are likely the same: objects 0 and 1 (the weapon models, `wrmdlp`/`wlmdlp`) sit on nodes 9/13 even
  with no weapon [inferred].
  The display actor was posed only at draw time (`actor_put_enemy`), after `bhControlObjItm` had
  placed the objects at the hidden double's wrists: on a model of other proportions Claire's hands
  hung beside the model's wrists, and a cast model itself shows no hands of its own.
* **Fix** (`actor_double_tick`, called from `coop_control_remote`, which runs after `bhControlEnemy`
  and before `bhControlObjItm`): the visible double of the character played is posed there and its
  hand nodes (`sr.map[9]`, `[13]`) get the model's hand positions (rotation the double's, as the
  hand pieces are authored in that frame), so the pieces and props sit on the model's wrists. The
  double's own matrices come back with its next `bhCalcModel`; the draw reuses this tick's pose
  (`dbl_posed`). Only while a model is shown for the double (`dg[ACTOR_DOUBLE].model >= 0`); the
  pieces stay the double's (Claire's gloved hands on Steve): the model's own hand pieces live in
  the rooms where that character acts and are not loaded.
* **Faces** (option "Talking faces in cutscenes (experimental)", F1 / PC SETTINGS -> CO-OP, ini
  `appear_cutscene_face`, default 0): the cast talks with a face mask (`face.c`, `face_bh.c`, the
  `MASK` chunk of the model): a muscle model morphs the body mesh's face vertices and turns the jaw
  (node 7), tongue (11) and eyes (9, 13) through pivots it inserts (`fmCnkInitContext`, 27 -> 31
  nodes; `npCalcSkinFM` skins that array, `mdflg 0x100`). Script op `bhLipSet` starts a line: lip
  data (pairs phoneme 0..5, time; 0xFF ends) and a bank; a phoneme is entry `bank * 6 + phoneme`
  of the model's own face table (`nFace`: Steve 42, Wesker 24, Alfred, Alexia, Rodrigo 18, Chris
  12, the hazmat man 1); `bhMaskSet` plays expression sets authored for that character's mask.
  With the option, a cast model for the double gets its own mask (`face_init`: `bhInitMask` with
  its allocations in our `face_mem`, about 55 KB), and every tick the double's lip state
  (`exp0` bit 4, bank, line, `LIP_WORK`) is copied onto it before `bhControlMask` (`face_step`); the
  retarget then turns the face nodes by their own angles (`follow`; unmapped nodes otherwise follow
  their parent rigidly). A line whose phonemes lie past the model's face table is not shown
  (hazmat man). Expressions are not copied (another character's sets); the model's rest face is
  its first mouth shape. A player file chosen as the appearance is shown on the double as the same
  character's cutscene model (Claire, Chris, Steve, Wesker: catalogue 0, 1, 3, 5), which has a face;
  Claire's second costume and the creatures keep theirs without a moving mouth.
* **Two traps found on the way:** (1) the mask's own face table holds the mouth shapes, not key
  frames: played as the face (`fmCnkSetCurrentFrame` over `nFace` entries) its last shape was
  taken; the face count is set to 1 so the rest face is shape 0. (2) `_fmCnkCalcMuscle` /
  `_fmCnkCalcTang` read the rest vertices with `lq`, which drops the low 4 address bits; the copy
  of Steve's model from RM_0040's list had its vertex data 8 bytes off a 16-byte boundary (the
  double's in RM_0000 is aligned), so every morphed vertex was read two floats early: skin-coloured
  spikes out of the head. `face_src` copies the rest meshes to 64-byte boundaries.
* **Verified** headless (640x480, new game, `RECVX_EQUIP=60:55:1` equips the lighter in the cell,
  which starts the Rodrigo scene in RM_0000: Claire's double 91 walks, talks in close-ups, frame
  dumps): hands before/after for the Steve cutscene model (Claire's hand pieces at the wrist band
  instead of below it); with faces on, every cast model (Chris, Steve, Wesker cutscene models,
  Alfred (3), Alexia, Rodrigo, the hazmat man) and the Chris, Steve and Wesker player files: face
  mask set up, no crash, Steve's mouth opening and closing with Claire's lines (Steve before: shut).
  Without an appearance (with the option on): 224 frame dumps of the intro and the scene identical
  to main. Not run: other rooms' cutscenes, co-op, Chris's doubles (92, 42) with a model.

### Changing the appearance in game: two bugs (fixed) [code + verified]

User reports: changing "You appear as" in game (F3, lobby, PC SETTINGS) did not change our own
model (the partner saw the change), and the host was suddenly out of the map after several
changes, once also after the relay server was stopped and the partner disconnected.

* **Our own model stayed** (`coop_glue.c self_frame`, since cb33dae, not the gender-matched
  merge): `if (want == self_mesh || want == self_fail) return;` with `self_fail` -1 when nothing
  failed also returned for `want` -1 ("Same as played" or a catalogue model), so a player-file
  mesh (Wesker, Steve, ...) was never put back: "Same as played" kept showing it, and so did
  every later catalogue model's slot once that model was released. Now only a real failure
  (`want >= 0`) is skipped.
* **The teleport**: `bhFixPosition` (player.c, motions that plant a foot, `plp->flg 0x40000`: the
  idle fidget after ~10 s standing, and others) moves the player by the difference between last
  tick's world matrix of the toe (`owP[17]` / `[21]`) and the toe of the new pose. A swapped mesh
  brought other node matrices: `self_load` reads the part file with `bhReadPlayerData`, whose
  `bhGetFreeMemory` zeroes the new O_WORKs (toe at 0,0,0: the player went to the room's origin,
  -0.7 0.0 0.4 in 2/0, out of the map), and `self_put_back` restored the original's matrices as
  they were when it was swapped out (back to where the player stood then: another room's
  coordinates after a door). Only the first tick after a swap counts, so it hit when a change
  coincided with such a motion: rarely, more often with many changes. On main a Disconnect did
  it too: `stop()` puts the original back and the next hand-off loads the mesh again (fresh
  O_WORKs). Fix (`self_carry_pose`): the swapped-in model takes the old one's node angles and
  root offset and its matrices are computed at the player's base matrix (`bhCalcTree`), so the
  next `bhFixPosition` sees only the motion's own step. Not involved: the bone moves (5, 9, 13
  are head and hands; the leg route is 0, 1, 14..17 / 18..21), `actor_source` / `actor_pose`
  (display copies only), catalogue models (drawn only; the player's model is not swapped),
  `self_keep`, the grab position guard (already per room, cleared by `stop()`).
* A player-file mesh moves the player a little differently than the character's own (as since
  M7): its bone lengths are on the real skeleton, so a planted foot steps by its leg length
  (Wesker in 2/0: 45.1 instead of 45.0 after the idle fidget, the same on main).
* Verified headless (`RECVX_NOCLIP` logs the position every 15 game frames and any jump over 2
  units in one tick; `RECVX_PLAYER=1` plays Chris): Claire and Chris, Same as played -> player
  file -> catalogue -> Same as played -> player file -> player file -> zombie -> Same as played
  (frame dumps: our model right every time; on main "Same as played" kept Wesker). 80 changes
  every 12 game frames (player files, cast, creatures, idle models, Same as played) while walking,
  turning and stepping back through 2/0 and 0/4, also in third person: positions identical to a
  run without appearance. 120 changes every 7 frames standing (through the idle fidget): main
  ends at -0.7 0.0 0.4, this build where the single-change run ends. Disconnect (echo mode,
  Wesker, at hand-offs 770..815 during the fidget): main at 0.1 0.0 1.1 each time, this build
  unmoved. Co-op through a local relay (host changing every 15 frames, the guest's frames show
  each model; relay killed, partner gone, more changes, Disconnect): neither player moved.
  Without an appearance: 56 frame dumps (0/6 handgun, walk, 0/4) identical to main.

### Game over with a catalogue model: crash on continue (fixed) [code + verified]

User report: as co-op host, appearing as another model, killed by Alexia's first form; the game
crashed on choosing to continue.

* **Cause** (`coop_glue.c`, any death, any catalogue model, single player and co-op, continue
  and "No" alike): `tx_in_game` ("our player was in control this frame", set by
  `coop_control_remote`) was still 1 at the frame hand-off after the game over's
  `bhPopGameData` / `bhReturnTitle`. Both run `bhExitGame`: `njReleaseTextureAll` (which
  unloads the display actors: `actor_textures_released`) and `bhInitSystem`, which zeroes the
  game state from `sys->tk_flg` on, `sys->obwp` among it. `self_frame` then took the game for
  running, loaded the model into slot 0 again and hid the head part:
  `sys->obwp[2].flg = 0` on a NULL `sys->obwp` (`self_actor`). Player-file meshes were not hit
  (no reload in that frame: the mesh is still ours). Not involved: the boss (a zombie death
  crashes the same way), its grab motions, the gender-matched copy, slot 2.
* **Fix**: `coop_textures_released` (called from `njReleaseTextureAll`, i.e. on every game
  exit) clears `tx_in_game`. The appearance comes back at the first hand-off with the player in
  control after the reload, as after a load: catalogue models and the cutscene double reload,
  a player-file mesh is applied again (`self_restore` sees the reloaded model). In co-op the
  same flag also keeps the pose, the host's game state blob and the guest's inventory snapshot
  from being taken in that frame.
* **Verified** headless (test aids `RECVX_HP=frame:hp` for a quick death, `RECVX_RETRY=frame`
  for a retry point in the teleport room so that Continue resumes there; `RECVX_SPAWN=60:12@30`
  puts Alexia's first form next to the player; it grabs and kills within ~250 game frames).
  Main (cd15399): Claire as Alexia's first form, killed by zombies in 0/6 or by the boss in
  5/82, crashes in `self_actor` on Continue and on "No". This build, Continue (each with the
  model shown again after it, in the log and frame dumps): Claire as Alexia's first form (zombie,
  boss), the Tyrant (zombie, boss), Alfred, the anatomist, Wesker's part file (zombie deaths:
  two deaths and continues in a row); Chris (`RECVX_PLAYER=1`) as Alexia's first form (Claire's motions) and as Claire's part
  file, killed by the boss in 5/82 (slot 0 and the double reload). "No": title, the attract demo
  (with the appearance) and back to the title, Claire as Alexia, Tyrant and Wesker. Co-op
  (two local instances, the guest in 0/5 meanwhile): the host as Alexia's first form and as
  Wesker dies in 0/6 and continues; the guest is pulled back and shows the host's model.
  Without an appearance: 83 frame dumps (new game, 0/6, death) identical to main.

### Voices [code + verified]

The player's vocal sounds follow the model shown: hit reactions, death, grab and struggle
screams, and whatever room script plays as the player's voice (not cutscene dialogue, which is
streamed ADX).

**Where they come from.** Every player vocal sound is SE bank 4, `0x400 + program`, and every
call goes through `sdfunc.c CallPlayerVoice` (slot 7 at `PlayerPos`): `player.c bhCPM0_damage`
(1026 light/medium, 1027 heavy, 1024 the other hit direction), `bhCPM0_die` (1025), the enemies'
grabs (`bhEne_CallPlayerVoice(1|2)` = 1025/1026 in en02, en03, en07, en12..14, en21, en23;
`bhEne_PlayerSePlay(1025|1026)` in en01 (zombie bite), en09, en17, en22, en26; en15, en19,
en29 directly) and the room scripts (`PlayGameSe4Event` type 0). Bank 4 is
`CORE_<ply_id>.SPQ` (`RequestPlayerVoiceSoundBank`, loaded at the stage's sound setup,
`system.c`; AFS key `0xFFF0 | ply_id` in MULTSPQ1/2, the same bytes on both discs). The disc has
exactly four: Claire, Chris, Steve, Wesker, each with the same 4 programs (all 32 kHz, 704-byte
header, 38..54 KB of samples). No other character has player vocal sounds anywhere: the Battle
Game uses the same four; Alfred, Alexia, Rodrigo speak only in streamed cutscene voices;
creatures' sounds are enemy sounds in a room's bank 3 (below). The climb, step and push
sounds (`CallPlayerActionSe`, bank 2 programs 10, 11, 18, 27..32) are the same samples in
every room of both characters: body sounds, not a voice.

**Mapping** (`actors.c appear_voice`, ctest `appear`):

| Shown as | Voice |
|---|---|
| As played | the character played |
| Claire, Claire 2nd costume, Claire's cutscene double | Claire (`CORE_000`) |
| Chris and his doubles | Chris (`CORE_001`) |
| Steve and his doubles, Monster Steve | Steve (`CORE_002`) |
| Wesker and his doubles | Wesker (`CORE_003`) |
| other men (catalogue `anim` = a man's): Alfred (all), Rodrigo, the hazmat man, Nosferatu, Tyrant, Hunter, Bandersnatch, the anatomist, every zombie | Chris |
| women (`anim` = a woman's): Alexia, Alexia's first form | Claire |
| creatures with their own idle motion: Cerberus, Albinoids, spiders, moth, bat, fish, Spotter, ant | the character played |

Creatures keep a human voice because their own sounds are not cleanly mappable: enemy sounds
are motion- and table-driven per type (`mtn_se_tbl`, `SndTbl`, en01's per-variant
`add_seno`), list numbers collide between types and rooms (docs/ENEMIES.md "Sounds of added
types"), and no list is labelled "hurt" or "death"; they also play through the enemy sound
manager's 6 slots, not the player's voice slot.

**How.** `CallPlayerVoice` (TARGET_PC) asks `coop_glue.c pc_player_voice(se)` for bank 4
requests. When the voice is the character played's it returns 0: the request is the game's own,
byte for byte. Otherwise it loads that `CORE_00n.SPQ` bank 4 as an extra bank
(`snd_iop.c snd_xbank_load(0xFFF0 + n)`, the loader of the spawner's extra enemy banks, read
straight from the disc) and the request names it in the port byte (`pc_se_xbank`, bits 3..7),
so the synth plays program n of that bank on the same port, slot, volume and pan. The game's bank
4 is never touched. The extra bank is released with the room (`spawn_room_setup`) and loaded
again at the next voice (a 40..55 KB read, once per room).

**Co-op** (protocol 18). Each side's `pc_player_voice` counts its own player's voices into the
pose (`Pose.voice_n`, `voice` = the program); the partner plays each new one at the partner's
position in the voice of the character the partner appears as (`Pose.chr`, else the partner's
character: `Pose.mfile & 3`). So the guest hears the host's bites, hits, screams and death
in the host's chosen voice and the other way round; nothing is sent twice (a voice played for
the partner is not counted). A voice that arrives after a gap of 10 or more frames (another
room, a menu) is skipped. While a host enemy's step targets the partner (`plp` is `ply2`) its
voice calls play nothing: the partner's game decides what happens to the partner (the zombie's
grab runs there, a hit arrives as `EV_PLY_HIT`) and sends the voice it makes. Before, the host
played those in its own character's voice and the partner's hurt voice only when both played the
same character (M6).

**Memory.** A voice bank is a heap blob of 0.7 KB header + 38..54 KB samples; at most two at a
time (ours and the partner's, when neither is the character played), so at most ~110 KB. They
take extra-bank numbers from the same 31 as the spawner's enemy banks (port byte limit), one
each, released with the room; the room's own banks (SE 0..4, the room's bank 3) are not changed.
On the PS2 sound RAM would have to hold them; here banks are heap memory (docs/ENEMIES.md).

**Verified** headless (32-bit Linux build, `RECVX_TRACE_SND=1`, new game, teleport to the
zombie room 0/6; `RECVX_HP=500:10` for a death; `RECVX_SPAWN` of a Bandersnatch / Black Widow
in 1/4 for grabs; logs in the worktree's `scratch/`, not in the repo):

* Identity: no appearance, the whole driver command trace (7360 lines: bites, hits, the
  zombies, the room) is byte-identical to d7a096c's build; with a death (5650 lines) too, and
  Chris played (`RECVX_PLAYER=1`, death, 5845 lines). Claire as Alexia (Claire's voice) and as
  Cerberus (the character played): identical as well. (Runs compared two at a time: with a dozen
  instances at once both builds drop a few coalesced change commands at random.)
  With an appearance that changes the voice, every command except the port-4 voice requests is
  identical to d7a096c's run with the same appearance (Wesker death run, Steve with the
  Bandersnatch's and the Black Widow's grabs, the spawned type's extra bank beside the voice
  bank).
* Voices: Steve (`CORE_002`; Bandersnatch grab 1025 + 8 x 1026, Black Widow 98 x 1026 struggle),
  Wesker (`CORE_003`), Chris on Claire (`CORE_001`), Rodrigo and the Tyrant (Chris), Monster Steve
  (Steve), Claire on Chris (`RECVX_PLAYER=1`: `CORE_000`), unswapped Chris (bank 4 as before);
  the key-ons play the target bank's samples (Steve's program 1/2 at `@1d40`/`@6aa0` instead of
  Claire's `@2480`/`@8ab0`). Room re-entry: `released 1 extra banks`, reloaded at the next bite.
* Audio (`SDL_AUDIO_DRIVER=disk`, 135 s of Claire appearing as Steve in 0/6): the capture's
  normalised cross-correlation with each character's hurt voice (program 2) is Steve 0.80,
  Wesker 0.35, Chris 0.18, Claire 0.09; death/grab voice (program 1) Steve 0.72, others
  <= 0.16. The same run without an appearance: Claire 0.81 / 0.57, Steve 0.10 / 0.10.
* Co-op, two local instances in 0/6 (host enters at entrance 1, the guest at 0): host Claire as
  Steve, guest as Wesker; the guest hears the host's 6 bites and grab scream in Steve's voice
  (`CORE_002`, 6 `partner voice` lines), the host the guest's bite in Wesker's (`CORE_003`) and
  nothing from its own enemy steps on the proxy (before that rule: 3 extra Wesker voices). Host as
  Chris with a guest as Alexia: the guest hears the host in Chris's voice (`CORE_001`).

### Review fixes [code]

* The cutscene double's face mask: when it does not fit, the model loaded again without it stays shown
  (`load()` leaves `model` -1, so `actor_set` reloaded it every frame); a failed reload is not retried;
  `bhGetFreeMemory` returning NULL (`face_mem` full) leaves the mask out instead of writing through NULL.
* `actor_pose` builds the source's rig every pose (a few dozen nodes); a cache keyed by the objects'
  address kept a stale rig when a room load put another model at that address.
* Two doubles of the played character in one scene: each is drawn in its own pose (`dbl_posed` is the
  double the current pose was taken from).
* `load_part_model` checks the motion and z blocks and each texture chunk against the file's end.
* Gender-matched motions: a weapon change of the same character reads only the weapon file; the base
  motions stay realized (`Shadow.base_mth`) - before, the ~400 KB part file was read again.
* One loader for player part files (`afs_read`, `player_model_read`, `player_hair`; actors.h), used by
  co-op, the lobby's previews and the display actors.
* Rejected: the "32-bit overflow" of the phase scaling in `shadow_step` (`frm * frm_num`): `long` is 64
  bits in the game code (`pc_prelude.h`: `#define long long long`).

### What was left out, and why [verified unless marked]

| Model (id) | Why |
|---|---|
| Alexia second form (13) | 14 sub-models, the body is one of many linked parts; `mdl[0]` alone is a few spikes |
| Alexia third form (14), Gulp Worm (2), Tentacle (29) | nothing visible near the player: arena-sized or below the floor (no scaling) |
| Armored knights (54, 55) | armour pieces lie scattered on the floor in motions 0 and 1 |
| Alexia's baby (30) | motions 0 and 1 show a thin stalk |
| Ant A (10), Ant C (71), baby Black Widow (24), Nosferatu's core (53) | parts of 2 nodes, no model, or nothing visible |
| NPC 68 | a black figure without a head (one texture) |
| ids 0, 16, 25, 56, 58..63, 65..67, 69, 70, 72..79, 81..85, 87..90 | props and set pieces (a ceiling trap, a wall, rats, debris, poles, Alfred's rifle; those of 2..11 nodes not all looked at) |

### Not verified / limits

* Verified headless, single player (`RECVX_INPUT` walk / run / back / turn / aim + fire
  cycles, frame dumps): zombie, Tyrant (both), Monster Steve, Hunter (both), Bandersnatch,
  Nosferatu, anatomist, Claire's cutscene model, Wesker's event model 2, the three scripted
  zombies; each walks, runs, steps back and raises the knife/gun in its own hand. Third person
  (`tps = 1`, `RECVX_TPS_MOUSE` pitch up/down while aiming): the model's chest up vector
  followed the player's (z component -0.07 -> +0.16 on the player, +0.07 -> +0.29 on the
  zombie; the difference is the rest correction). Cutscenes: RM_0040's Claire-and-Steve scene
  (event 0 started from a test probe) with Claire's double drawn as Wesker (part file 13);
  RM_9090's Chris arrival scene, triggered naturally (teleport 9/9 position 1) after switching
  the played character to Chris with a test probe (`cng_pid`), Chris's double drawn as the
  Tyrant. Grabs: the Tyrant in the zombie room 0/6, grabbed and bitten twice, played the
  player's grab and struggle motions on the zombies' sets (frame dumps).
* The cast doubles' face mask context (`face.c fmCnkInitContext`) gives them 31 nodes (9 face
  nodes with child pieces) and a root that carries the world position; the node map and the
  retarget handle both (unit-tested `rig_build` on that topology).
* Not covered: the double's hand pieces are its own (Claire's gloves on another model, now on the
  model's wrists); lip sync only for cast models and with the option; sounds of the creature, scaling (a Tyrant is taller than Claire; feet stay
  on the floor). A room with little texture memory left refuses the model (logged as
  `actor: ... not enough texture memory`). Per-model choice of the animation set: the `anim`
  field of the catalogue (a man's, a woman's or the own idle; "Gender-matched animations").
* The partner path was tested in echo mode (`RECVX_COOP_ECHO`), which runs the same pose fields
  through the same receive code as the network.
* Wrong colours are possible where a model's CLUT bank collides with the room's (section 1).
* No sounds of the creature (the player's footsteps stay; the voice is Chris's or the character
  played's, "Voices"); creatures do not scale.

## 7. Limits (F)

| Resource | Limit | Where |
|---|---|---|
| Enemy slots | 128 `ene[]`, `sys->ewk_n` = highest used + 1 | `eneset.c bhSetEnemy`, `bhCheckEneWorkNum` |
| Objects / items | 32 `obwp` (0..3 player), 32 `itwp` | `system.c bhFirstGameStart` |
| Effects | 512 `eff[]`, 450 effect models `sys->efm` | `main.c`, `SYS_WORK` |
| Enemy types | 100 jump entries; textures `et_lp[100][16][16]` | `eneset.c`, `room.c` |
| Motions | 512 per set (player and per enemy type) | `bhSetEneMtn`, `bhInitPlayer` |
| Tracks | 64 per motion (`NormalTbl`), must not exceed model nodes | `Motion.c` |
| Sub-models | 16 per actor (`mdl[16]`) | `BH_PWORK` |
| Main arena | 12.25 MB incl. persistent data; room part measured 0.34 MB with 10.3 MB free in the demo room; room files up to 8.9 MB expanded | `main.c`, `system.c` |
| Allocation checks | `bhGetFreeMemory`: `freemem + 12.25 MB`; `bhEne_CallocWork`: `mempb + 0xC40000` (both + `PC_ARENA_EXTRA` on the PC); none against `endp` | `pwksub.c`, `eneset.c` |
| Textures | 256 entries (`tbuf`), 10 MB heap, room upload list 256; overflow = `exit(0)` | `main.c`, `ps2_NaTextureFunction.c` |
| Player buffers | model 128 KB, base motions 384 KB, weapon motions 64 KB | `bhInitPlayer` |
| Sound | room bank per room file; 6 enemy SE slots; one player voice bank (PC: others as extra banks, section 8 "Voices") | `sdfunc.c` |

## Verification log

Temporary probe (`pc/glue`, included from `cheat_frame`, removed again), 32-bit build,
`SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=offscreen RECVX_FAST_DEMO=1 RECVX_SKIP_MOVIES=1
RECVX_INPUT="200:start,206:-"`, `--size 320x240 --quiet`, disc read in place:

1. Parsed sys part files 10..29 and weapon files with the loader's own layout: player files and
   their hierarchy signatures, node offsets, motion counts and track counts (section 2), weapon
   files present for 30 ids x 4 characters; files 20, 21, 50, 80, 110 without motions, 22 and 23
   with 10 and 8 motions of 22 tracks.
2. In-game state after the demo room loaded: `plp->mdl_n` 8, sub-model node counts and skin
   flags, 60 base motions, `hd_pos`, room `ene_n` 5 bats (14 nodes, 27 motions, shared texture),
   arena and texture use.
3. Recipe 1 with id 7: clone created in slot 5, ran its Init and AI for 20 s, no crash.
4. Recipe A with part files 11 and 13: mesh replaced at frame 60 of the room, Claire's demo
   (walk, aim, fire, run) played on the new mesh with weapons in hand, frame dumps inspected,
   no crash until the demo ended.
5. Census over all `rm_*.rdx`: counts only (section 5).

Not verified: Recipe 2, randomizer patches, mesh swaps for Steve's file, the moth egg with a
swapped mesh, CLUT bank collisions, enemy sounds/effects in foreign rooms.
