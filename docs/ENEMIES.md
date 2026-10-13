# Enemies: how a room spawns and runs them, and how to change that

Reference for variable difficulty (more enemies in co-op, other placements) and an enemy
randomizer, plus the F2 enemy spawner built on it. Companion documents, not repeated here:
`docs/ACTORS.md` (models, skeletons, motions, texture sharing, the actor catalogue, the earlier
spawning recipes), `docs/MAPS.md` (room file layout, records, scripts and opcodes),
`docs/COOP.md` (host-authoritative enemies, M8 event replay), `docs/PORT.md` (cheats, test aids).

Paths are relative to `decomp/src/ps2/veronica/prog/` unless noted.

Evidence tags:

* **[code]** read from the decompiled source (file:function).
* **[data]** counted over all 205 room files with `pc/tools/enemies.py` (census below).
* **[verified]** code read and exercised in the running port (headless runs, logs, frame
  dumps; "Verification log" at the end).
* **[inferred]** concluded from code or data, not run.

## Summary

* Every enemy of a room is a **record** of the room file (`ETTY_WORK`, 759 in all rooms), realized
  at room load into the slot of the same index (`ene[k]` = record `k`; `wrk_no == k` in all
  rooms) [code][data]. **No script op creates an enemy**: scripts hide and show records
  (`InitModelSet`), hide dead ones (`EneSetCk`), drive them in cutscenes (`LoadWork` + controller
  ops) and change their HP. A "script-spawned" enemy is a record hidden at entry and shown when a
  flag condition holds (RM_0060 record 3: shown on entry once `ev_flg` 116 is set and its kill bit
  `ed_flg` 15 is clear) [code][data].
* Everything else is spawned by enemies themselves at run time: a zombie's body parts (up to 8
  more slots each), a Hunter's helper, spider babies, moth children and eggs, worm segments,
  Alexia's tentacles [code][verified for zombies and Hunters].
* An enemy type's model, motions and textures exist **only in room files**; each room carries
  copies for its own records. Variants of a room are separate files (`RM_SRRC`, C = case; 22 rooms
  have cases with different enemies, picked by the story through op 55 `RoomCaseNo`) [data].
  **Difficulty never changes placements**: no room script tests `gm_mode`; easy mode only scales
  HP tables in the C code (enemies) and doubles the player's HP [code][data].
* Kills persist through `ed_flg` bits named by the room script (`DieCk` sets, `EneSetCk` hides on
  the next visit). 111 hostile records have no kill bit and come back on every visit [data].
* On the PS2 the budgets for adding enemies were the **enemy slots** (128, a zombie takes 8), the
  **texture heap** (10 MB, rooms use up to 7.6 MB) with its 256 entries, and the helper code in the
  shared `bhJumpEnemy` slots 31..38 (two types using them could not share a room) [code][data].
  The port lifts all but the slots (section 2.2): a 64 MB texture heap with 1024 entries, helper
  code per creator type, `et_lp` rows of their own for zombie models >= 16, and a 32 MB pool for
  the spawned types' data outside the room arena. The binding budget is now the **128 enemy
  slots** (section 6, "Budgets on the PC") [verified].
* The F2 cheat window spawns any of 58 enemy entries in front of the player with working AI (most
  types; bosses and parent-bound types are limited or refused), loading the type from another room
  file when needed (section 6) [verified].
* Co-op (protocol 10): enemies the host adds (F2 spawner, extra enemies) reach the guest as one
  reliable event each and become ordinary puppets in the same slot; the host's enemy options go
  along, and placements and the randomizer are computed identically on both sides (section 4).
* Enemy options (section 7, off by default): extra enemies of the room's own types, rearranged
  placements, and a randomizer with the constraints of section 5 and a fallback to the original.

## 1. Where a room's enemies come from

### 1.1 Records, realized at room load [code][data]

`system.c bhSysCallMonitor` mode 4 reads the room file, `room.c bhSetRoom` walks the enemy
records: every record with `flg & 1` (all 759; the ceiling trap also has `0x80000000`, "drawn as
an enemy object") goes through `room.c bhSetEneMdl` -> `eneset.c bhSetEnemy`, which takes the
**first free slot** of `ene[128]`; at load that is slot `k` for record `k`. `bhFinishRoom` then
assumes slot `k` = record `k` for motion sets and matrices, and every script op addresses a
record as `ene[rom->enep[k].wrk_no]`. Record order and count are therefore load-bearing
(ACTORS.md section 6).

Init runs on the slot's first enemy tick (`mode0 == 0`). Enemies in the room at load get their
first tick from `bhInitEvent` (the entry script `scd0` runs enemy steps), before the first frame
is shown [code, COOP.md M2].

### 1.2 What the scripts do to records [code][data]

Ops that take an enemy record (operand positions in `pc/tools/enemies.py`):

| Op | Effect |
|---|---|
| 13 `DieCk rec, bit` (every frame in `scd1`) | the record's enemy has `flg & 2` (dead): set `ed_flg` bit (`& 0x3FF`) |
| 34 `EneSetCk rec, bit` (in `scd0`) | `ed_flg` bit set: hide the slot (`stflg |= 0x1000000`); **bit masked with `0x1FF`** |
| 33 `DieEventCk n, bit, cmp, count` | count of set bits `bit..bit+n-1` compared ("all dead" checks) |
| 36 `InitModelSet 1, rec, 0/1` | hide (also `flg |= 0x8000`) / show the slot |
| 101/104/0xD3 `LoadWork 1, rec` | an event task works on this enemy (then controllers 100/103/105 move and animate it) |
| 45/48 `InitMotionPause(Ex) rec` | the enemy plays room motions for the script (`mode0 = 5`, `bhEne_Event`) |
| 7 `CmpW rec, 10, cmp, v` / 9 `SvW 11..15` | test / set HP (SvW only records 0..4) |
| 153 `EnemyHpUp rec, pct`, `EneLifeSetM` | raise / set HP |
| 142 `ZombieUpDieCk rec, bit` | a zombie that got up and died: set an `rm_flg` bit |
| 81/83/84 `ObjLinkSet*Ene*`, 121 `EneSeOn`, 125 `MotionPosSetEnePly`, `EneRenderSet`, `EneLookFlgSet`, face ops | links, sounds, positioning, drawing, look-at, face masks |

Census of the 569 hostile records (ids 0..30, 53..56, 71) [data]:

| Class | Records | Meaning |
|---|---|---|
| free | 283 | not hidden at entry, not touched by an event (186 with a kill bit, 97 without) |
| event-driven | 252 | `LoadWork` targets: cutscene actors, enemies that rise/burst in on cue, bosses |
| hidden at entry, shown on a condition | 13 | e.g. RM_0060 #3, RM_0020 #0..2/#5, RM_1021 |
| hidden at entry, shown by other means | 16 | shown by an event's controller ops (RM_3020 Cerberus, RM_3160 zombies) |

Typical conditions found (`pc/tools/enemies.py room ROOM` prints each reference with the
conditions of its enclosing blocks):

* **only after story event X**: `IfelCk; Ck ev_flg X set; Ck ed_flg K clear; InitModelSet 1 rec
  show` (RM_0060 #3, `ev_flg` 116; RM_7080 Black Widows, `ev_flg` 340).
* **only on a later visit**: the same pattern, where X was set by the previous visit or another
  room.
* **only until killed**: `EneSetCk rec, K` in `scd0` + `DieCk rec, K` in `scd1`.
* **per scenario**: a different room file (case) - 22 rooms, e.g. RM_1120 case 0 zombies, case 1
  two Bandersnatches, case 2 the Tyrant; RM_0400/0401 bats or Bandersnatches; RM_9300..9302 the
  final fight stages. 14 rooms also branch on the character played (`CmpB ply_id`).
* **per difficulty**: none (no `CmpB gm_mode` in any room script).
* **random**: ops `RandamSet`/`RandTest` give event tasks a random value; not used to choose
  enemies in the census rooms looked at [inferred].

### 1.3 Spawned at run time by enemies [code]

| Parent | Children (slots) | Where |
|---|---|---|
| zombie (1) | linked parts, ids 31..38: one link, head, 2 arms, 2 legs, + cap / worm / bomb + scope / parent by model version (`bhEne01_SetLinkEnemy`) | `en01.c bhEne01_Init`; 7 extra slots for a plain zombie [verified: a zombie spawned into slot 0 put the next spawn in slot 8] |
| Hunter (5) | 1 helper, id 31 | `en05.c` ~1015 [verified: slot 1] |
| Black Widow (3) | leader, children (31), 15 babies (24) | `en03.c` |
| Giant Black Widow (23) | children (31) | `en23.c` |
| moth (6) | leader, children (31), eggs (27) on the player | `en06.c`; eggs also from `player.c bhCheckMothEgg` |
| Gulp Worm (2) | segments | `en02.c` |
| Alexia 2 (13) | tentacles (18), body part 13B (31) | `en13.c` |
| Monster Steve (17) | arms (31, 32) | `en17.c` |
| tentacle (29) | a damage dummy | `en29.c` |

`bhSetEnemy` returns NULL when the 128 slots are full and none of these callers checks it.

## 2. Data an enemy type needs

### 2.1 Per room file, loaded with the room [code][data]

| Data | Room file table | Realized by | Lives in |
|---|---|---|---|
| Model: chunk list (MLB sub-models, `SKIN`, `MASK` for ids > 90) | `hed01`, one entry per record | `bhSetEneMdl` (relocates in place) | room arena, until the next room |
| Motions: 512-entry set, one per type | `hed02`, first record of the type that has one; later records share `sys->emtp[id]`; none -> zeroed set | `bhFinishRoom` / `bhSetEneMtn` | room arena (12 KB headers + data) |
| Textures | `hed04`, one entry per first (type, model version, textured sub-model) in record order | upload list built by `bhSetRoom`, one list per frame | texture heap; released by `bhReleaseMainTexture` at the door |
| `O_WORK` matrices | - | `bhKeepObjWork` | room arena |
| AI work (`exp0` ...) | - | the type's Init, `bhEne_CallocWork` | room arena |

Also used from the room, not part of the enemy's data: **enemy floor areas** (floor records of
type 2 whose `prm0` is the enemy type: Cerberus wake-up areas, Hunter stairs and jump spots,
zombie areas; 21 rooms for zombies, 9 Bandersnatch, 8 Hunter, 6 Cerberus) read by
`zonzon1.c bhEne_CheckEventAtr` / `zonzon.c bhEne_EnemyAtariCheck`; **route boxes** (`rutp`/
`ruttp`, 114 rooms) for path finding (`rutchk.c bhCheckRoute`); water areas (fish, albinoids);
the room's effect textures (effect ids 30..99 and 400+, `room.c bhSetRoom`) and **sound bank**
(`RM_SRRC.SPQ` from `MULTSPQ1/2.AFS`: SE bank 3 holds the enemy sounds of the room's own types;
6 enemy sound slots, `sdfunc.c`; section 6, "Sounds of added types").

Global, not per room: the effect code (all ids), the global effect textures (effect texture ids
0..29), the player's part file (the moth egg uses Claire's sub-models 6/7 and player motions
93..95, ACTORS.md 2), the enemy code itself. An effect's *texture* id (`O_WRK.tex_id`, looked up
through `sys->ef_tn`) is not its effect id: a few global enemy effects draw with a room effect
texture (section 6, "Effect textures of added types").

### 2.2 Budgets and limits

PS2 values, and what the port changed (PC column) [code][verified]:

| Resource | Limit | PC | Use in retail rooms | Notes |
|---|---|---|---|---|
| Enemy slots | 128 (`ene[]`) | unchanged | up to 76 records (RM_9170, ants) | children count; a zombie takes 8 [verified]. Slots are byte-sized in the decomp and in the co-op packets |
| Room arena | 12.25 MB incl. ~1.8 MB persistent player data | **+64 MB** (`PC_ARENA_EXTRA`, docs/MAPS.md 7.1; the free amounts in this row were measured before it); the spawner no longer copies its types' data into it | room data part (file up to the texture section) 0.3..2.8 MB -> 7.6..10.2 MB free; 1/11: 9.9 MB, 1/4: 10.0 MB free [verified] | no check against `sys->endp` in the allocators (ACTORS.md 1). The map, file and option screens copy the room model's textures above `sys->memp` (`adv.c AdvPushRoomTexture`, `map.c`): 2.2..6.1 MB in the surveyed rooms, which the arena must keep free |
| Texture heap | 10 MB (`Ps2_tex_buff`) | **64 MB** (`PC_TEX_HEAP_SIZE`, `pc_prelude.h`) | room texture section up to 7.6 MB (RM_4030); in 1/11 (6 MB of room textures) 2.8 MB were left [verified] | full heap or table = `exit(0)`. The heap holds the EE copies the GS loads from; the PC renders through GL, so its size only costs address space (bss, untouched pages) |
| Texture entries | 256 (`tbuf`) | **1024** (`PC_TEX_ENTRIES`) | 62..172 free on arrival in the surveyed rooms [verified] | allocation is first-free and bump, so vanilla rooms use the same entries and bytes as on the PS2 |
| Texture slots per type | `sys->et_lp[100][16][16]` | model versions 16..63 in a table of their own (`room.c pc_et_lp`) | | indexed `[id][mdlver][k]`: on the PS2 **model versions >= 16 alias other types** (zombie model 21 = Gulp Worm model 5's slot, 32..42 = Black Widow 0..10); only zombies have them (up to 42) and no retail room combines them [data] |
| Spawned types' data | in the room arena | **32 MB pool** (`spawn_glue.c`), emptied by `bhSetRoom` | | model + motion chunk lists, 6 KB..1.1 MB per type |
| Motion sets | 512 motions per type, `sys->emtp[128]` | unchanged | | |
| Enemy sound bank | one SE bank 3 per room (the room's types), in the SPU2's 2 MB with the room's other banks | **31 extra banks** (heap blobs; the limit is the 5 bits they take in the request's port byte), one per source room, released with the room | room banks 0.6..5.9 KB headers + 28..486 KB samples | section 6, "Sounds of added types" |
| Helper code | `bhJumpEnemy[31..38]`, global, never restored | **per creator type** (`eneset.c pc_call_enemy`) in rooms the PC added enemies to | | zombie 31..38; Monster Steve 31, 32; Gulp Worm, Black Widow, Hunter, moth, Alexia 2, Giant Black Widow 31. One retail room mixes two (RM_6030: an event-driven zombie with moths) [code][data]. The PC remembers which type's code created each part (`bhSetEnemy` while that enemy's code runs) and what it registered; a part runs its creator's function. Retail rooms keep the shared table (RM_6030 runs as on the PS2) |

### 2.3 What breaks when a type the room did not load is added

| Missing | Effect | Handled by the spawner |
|---|---|---|
| model / motions | nothing to draw, `bhSetMotion` on an empty set | loaded from the source room file [verified] |
| textures | `et_lp` slot empty -> the instance becomes owner with no uploaded texture: wrong or no texture, and `bhReleaseMainTexture` never frees it | uploaded from the source room's `hed04` entry, released with the room [verified: "released N texture lists" at the door, no texture errors after] |
| helper slots | a zombie's head code runs the Hunter's helper or vice versa: wrong behaviour, crashes [inferred] | each type's parts run its own (PC, section 2.2) [verified] |
| children's slots | `bhSetEnemy` NULL -> crash in the parent's Init (seen with a Gulp Worm at 121 slots) | refused unless the type and its Init's children fit and 6 slots stay free [verified] |
| floor areas | waiting variants never wake (Cerberus types 1/2 waited forever in 1/11 [verified]); no stairs/jumps for the Hunter | spawns variant 0 |
| effect textures | an effect whose texture id is a room effect's (30..99/400+) the room did not load draws effect texture 0 (a page of kanji): Alexia's fire and burning blood (80), the Gulp Worm's sand (82/83) [verified] | uploaded from the source room's effect texture entry, released with the room's; none: the effect is not drawn (section 6, "Effect textures of added types") [verified]. (An earlier version said "not needed: enemy code only makes effects 0..29/100..399"; that was from the effect ids, but the texture ids differ.) |
| sounds | the type plays its sound ids against the room's bank 3: another type's sounds [verified] | its source room's bank 3 as an extra bank; none found: silent (section 6, "Sounds of added types") [verified] |
| CLUT banks | an asset's fixed palette bank may overwrite the room's -> wrong colours [inferred] | not handled; none seen in the frame dumps |

## 3. Runtime management

### 3.1 The work array and the tick [code]

* `ene[128]` (`BH_PWORK`, 0x580 bytes), `sys->ewk_n` = highest used slot + 1
  (`bhCheckEneWorkNum`). Free = `!(flg & 1)`; `bhInitEnemy` zeroes all at room load.
* `eneset.c bhControlEnemy` (game task, `sp_flg & 2`): per slot `< ewk_n` with `flg & 1`, not
  hidden/frozen (`stflg & 0x41000000`), not linked to a hidden parent: `bhJumpEnemy[id](ep)`, then
  `bhEne_InitDamage`. Co-op hooks around the call (COOP.md M2).
* `bhDrawEnemy` draws the same set (clip by `clp_jno` unless `mdflg & 0x20`); `flg & 0x80000000`
  ones are drawn as "enemy objects".

### 3.2 AI entry points [code]

`bhJumpEnemy[id]` -> `bhEneNN(ep)` (`enNN.c`) -> the type's `bhEneNN_Mode0[mode0]`:

| `mode0` | Handler | |
|---|---|---|
| 0 | `Init` | allocates `exp0` work, HP from the difficulty table, collision, sets `mode0 = 1` |
| 1 | `Move` | the brain (`BR`/`Brain*`) picks `mode1`/`mode2`; `MoveMode2[mode2]` runs the behaviour (`MV00`..), e.g. Hunter MV02 walks the route to the player |
| 2 | `Nage` | grabs/throws: the player is animated from the enemy's motion set |
| 3 | `Damage` | `DG*` reactions |
| 4 | `Die` | `DD*`; sets `flg & 2` (dead) |
| 5 | `bhEne_Event` (`subpl.c`) | script acting: motions chosen by the event (`InitMotionPause`) |

Types with their own entry code: 0 (`eneset.c bhEne00`), 1..27, 29, 30 (`en01.c`..`en30.c`), 53
(`en15.c bhEne53`), 54, 55, 71; 56 (`eneset.c bhEne56`) is a scripted object; 31..99 otherwise
`subpl.c bhSubpl` (NPCs, no AI). Targeting: the AI reads `plp` (co-op swaps it per enemy);
sight checks use `bhSearchPlayer` + a line test against the room model (Hunter
`bhEne05_SearchPlayer`).

### 3.3 Death, kill flags, respawn [code][data]

* Death = `flg & 2` (set by the type's `Die`); the body stays in its slot until the room is left
  (bats, Bandersnatch, zombies [verified]).
* `scd1` runs `DieCk` every frame for each record it names: the `ed_flg` bit is set once the
  record is dead. `ed_flg` is in the save (`SYS_WORK`), so kills persist across visits and loads.
* `scd0` runs `EneSetCk` on the next visit: dead records are hidden. Records without a `DieCk`
  respawn on every visit (111 hostile records: fish, ants, exploding props, some zombies) [data].
  `EneSetCk` masks the bit with `0x1FF`: kill bits above 511 would not hide (retail uses up to
  357) [code][data].
* `DieEventCk` counts kill bits for "all dead" checks: rooms 3210, 4012 and 5550..5710 [data]
  (rooms 55xx..58xx hold copies of other stages' enemies and bosses with "all dead" checks: the
  Battle Game's, by all appearances [inferred]).
* Rooms reset everything else at load: per-room flags `rm_flg` are cleared by the scripts at entry.

### 3.4 Difficulty [code]

`sys->gm_mode` is set by the title menu (`adv.c`, `NewGameDef = {0, 1, 2}`, `ExGameDef = {3,
3}`): 0 and 1 normal [inferred: every check is `== 2`, `== 0` or `== 3`], 2 easy, 3 Battle Game.
Easy: player HP 320 instead of 160 (`system.c`, `player.c`), enemy HP from the second row of the
type's table (`en01_hp_tbl[2][16]` 60..120 vs 20..60 for zombies, `en04`, `en05`, `en09`, `en22`,
`en03`; Gulp Worm 300/200, Alexia 2 700/400, Giant Black Widow 250/160, Nosferatu 600/360,
Tyrant `En19HpTbl[4][3]`), ammo pickups `item.c getbulletmax[item][gm_mode]`. Most HP values are
`table[rand() % 16]`. Nothing else (counts, placement, speed) depends on it.

## 4. Co-op [code, COOP.md]

* The host runs the AI; the guest's enemies are puppets updated from **per-slot snapshots**
  (`EneSnap`: slot index, modes, motion, position, HP, flags; no type, no model). The guest's
  slots are set up by its own room load, so both sides must hold the same enemy in the same slot.
  That holds for retail rooms because both load the same file and run the same inits.
* Snapshots for a slot the guest has not set up are skipped (`apply_snapshots`, `flg & 1`).
* **Added enemies (protocol 10, implemented).** Every enemy that exists on the host but not in the
  room file (the F2 spawner, the extra enemies of section 7a) is announced with one reliable event
  `EV_SPAWN` {slot; source room file and record; type, model version, variant; position, angle,
  floor} (88 bytes, once per enemy). The guest realizes it with the spawner's own code into the
  same slot (`spawn_glue.c spawn_note_apply` -> `spawn_at`, loading the type's data from the
  source room file when its room lacks it); the enemy's own Init then creates the same child slots
  as on the host, and from then on it is a puppet like any other: snapshots, the guest's hits
  (`EV_ENE_HIT`), its attacks on the guest (`EV_PLY_HIT`), deaths, zombie grabs (M7).
  * The guest places it while its room runs; one that comes in a door, an event or a menu waits in
    a list (`spawn_pending`, from `coop_frame`, at most 900 frames) while the event stream goes on.
    It is dropped when it is for another room and ignored when the slot already holds that type (a
    replay). The guest's spawns of the host's enemies take no texture reserve (`SPAWN_DICTATED`).
  * Randomizer: the records the host's randomizer had to keep (texture memory) go to the guest as
    `EV_SPAWN` notes with `SpawnNote.pad = SPAWN_NOTE_KEPT`; the guest keeps them too at its next
    load of that room (docs/COOP.md "Glue fixes").
  * The host keeps the room's added enemies; when the guest (re)enters the host's room (a door,
    a rejoin, after a menu) it sends all of them again (`spawn_replay`). A door clears them on
    both sides with the room (the slots and the textures go with it).
  * If the guest's first free slot is not the host's slot, the spawner skips the free slots
    below it, so the slot still matches.
* **The host's options** (extra enemies, placements, randomizer, seed) go to the guest as
  `EV_EOPTS` when a partner session starts (the guest's lobby shows them), before the game state
  (join, rejoin) and whenever they change; the guest uses them
  instead of its own. Placements (7b) and the randomizer (7c) are computed at the room load from
  the room file and the seed only, so both sides make the same records with the same slots.
* **Snapshot window.** A packet holds at most 48 enemy snapshots; with more active slots (extra
  enemies) each packet sends the next 48, so every slot is refreshed every second or third packet
  instead of never (before: slots beyond the 48th were never synced).
* **Cost** (`RECVX_COOP_STATS=1`, two local instances): a pose packet is 160 bytes plus 100 bytes
  per enemy slot snapshot (unchanged), plus 88 bytes per event while unacknowledged. An added
  Hunter (2 slots) makes a packet 360 instead of 160 bytes, exactly its two snapshots; the
  `EV_SPAWN` itself is one event (one to three packets until acknowledged). Ping 33 ms (one frame)
  with and without added enemies. No lockstep, no waiting on the host.
* The guest never spawns itself (F2 refuses there); in a room only the guest is in, it runs its
  vanilla enemies plus placements/randomizer, which match the host's when the host comes in.
* **Grab ownership.** A zombie grab on the guest makes the slot the guest's until it hands it
  back; every hand-back (with or without state, also a refusal) ends it on the host, and a grab the
  guest stops reporting for ~10 s comes back to the host by itself. Rooms with many zombies made
  refused grabs common, and each refusal used to freeze a zombie on both sides (COOP.md, frozen
  zombies).
* **Targets (host).** Each enemy goes for one player at a time (`coop_proto.c coop_pick_target`): the
  nearer by a margin, kept through an attack (`mode0 > 1`, a zombie still holding: before, a target
  flip mid-grab left one player in the grab motion and ran the rest on the other), a busy target
  (hit, held, dying) left for the free player within 4 m, never a busy one. The partner's stand-in
  carries what the AI reads of a player: position, angles, modes, hp, `stflg`, the hit capsule `watr`
  (most attacks test it: dogs, Hunters, Tyrant, Bandersnatch, Nosferatu - they all missed the stand-in
  before), its own floor `flr_no` and its busy bits `flg & 6` (pose fields `flr`, `pflg`). Attacks on
  the stand-in reach the guest as `EV_PLY_HIT` (damage, hit reaction) or, for zombie grabs, as the
  grab itself (`EV_GRAB`); other grabs arrive as a hit. COOP.md "Enemy targets: one victim per
  attack"; `RECVX_ENEMY_TRACE=dist` traces every enemy step.
* M8 replays the script blocks that show/hide/drive records; event-driven records run the
  guest's own AI during its cutscenes (COOP.md M8).
* **Kills while the guest runs the room (protocol 15, `EV_GONE`).** Kill flags are synced like
  every progress flag. Records without one (3.3) and extra enemies come back with every room load,
  so when the host's enemies took over the guest's again (the host enters, or leaves its menu) the
  host's live copies replaced the guest's dead ones in front of the guest. The guest now names its
  dead enemies (slot, type, model version) at the take-over; the host hides its matching copies as
  `EneSetCk` does and marks them dead. This lasts the visit: once both have left, the room is loaded
  as in single player (COOP.md "Protocol 15").

## 5. Requirements for randomizers and difficulty options

Keep consistent [code][data]:

1. **Record count and order** (scripts, `wrk_no`, model/motion/texture tables, co-op slots).
2. **Kill bits**: a replaced record keeps its `DieCk`/`EneSetCk` bit; extra enemies get no bit
   (they respawn) or reuse the source record's (hidden once it is dead). New bits must be < 512.
3. **Leave alone**: event-driven records (252: cutscene actors, rising zombies, the moth scene,
   bosses), records shown only by events, NPC ids >= 31, ids 0, 25, 28, 53, 56 (props, traps,
   cores, scripted objects).
4. **Bosses and set pieces** (2, 11..17, 19 in its story rooms, 23, 25, 29, 30, 53..55): their
   rooms' scripts wait for their death or events (`DieEventCk`, motion pauses, `CameraControl`).
   Replacing them can softlock the room; replacing something *by* them brings their arena
   assumptions.
5. **Types that cannot die or need a weapon/event to die** (tentacle 29, Nosferatu's core 53,
   ceiling trap 25, exploding prop 0) must not replace a record that a `DieEventCk` or a door
   condition counts.
6. **Per-room type set**: children slots within 128; water types (8, 21, 22) only where there is
   water; fliers (7, 6) need head room. PS2 only (lifted by the port, section 2.2): at most one
   helper-code user family (zombie / Monster Steve / {2, 3, 5, 6, 13, 23}); no zombie model
   version >= 16 together with types 2 or 3 (`et_lp` aliasing); textures within the heap the
   room leaves (about 8 MB minus the room's texture section).
7. **Variant (`type`)**: in range for the type (zombie 0..15 through `bhEne01_InitType[16]`, Tyrant
   0..2 through 3-entry tables, others mostly 0..3); waiting variants need the room's floor areas.
8. **Grab motions** for every character (`ply_id` 0..3) come with the type's own motion set; Wesker
   (3) overruns some 3-row tables (ACTORS.md 4).
9. **Co-op**: everything decided from the room file, the flags and a shared seed, applied at the
   same point of the load on both sides.

Softlock risks: rooms that open only when all enemies are dead (`DieEventCk`), events that
`LoadWork` a record expecting its motion numbers (another type has other motions: the event's
`FrameCheck` may never come true), items or keys given by an event after a scripted kill,
enemies placed on the entry position or in a door's trigger box (instant grabs on entry).

## 6. The F2 enemy spawner (implemented)

F2 -> "Spawn enemy": a list of every enemy type (the "You appear as" catalogue entries with ids
up to 30 - 24 zombie looks, the creatures, Alexia's first form - plus Gulp Worm, Alexia 2 and 3,
Alexia's baby, tentacle, both knights, ceiling trap, Nosferatu's core, Alfred, the ants, the baby
spider, the exploding prop); "Spawn in front of me" puts it 3 m in front of the player, facing it.
Test aid: `RECVX_SPAWN="gameframe:id[.mdlver][@dist],..."` (first list entry of that type/model
version; game frames since start, as `RECVX_EQUIP`), which also traces what it spawned (state
every 30 game frames, every HP change, hits on the player).

How (`pc/glue/spawn_glue.c`), as the room loader does it for a record:

1. Checks: in a room, no door transition/event, not the co-op guest; enough free slots for the
   type and what its Init spawns (zombie 10, Gulp Worm 19, Black Widow 25, moth 16, Alexia 2 12,
   Monster Steve and Giant Black Widow 3, Hunter 2, others 1; `spawn_slots_needed`) with 6 more
   kept free for what enemies spawn later (moth eggs, a tentacle's damage dummy); room arena
   (`endp - memp`) for the work memory (0.33 MB) plus the screens' copy of the room's textures
   plus 0.5 MB; the spawn pool for model and motions; texture entries (16 spare) and heap (1 MB
   spare, more than the largest model's textures). No helper-code check (section 2.2).
2. `actor_find` (`pc/glue/actor_glue.c`, shared with the appearance loader) reads the source room
   file from `RDX_LNK.AFS` and finds the record's model chunk list, the type's motion list and the
   texture table entry of each textured sub-model.
3. The model list is copied into the spawn pool (alignment kept) and realized
   with `bhSetEneMdl` on a copy of the source record (position, angle facing the player, floor;
   variant 0 except ants C, tentacle, core): it takes the first free slot and shares the textures
   of a (type, model version) the room already has (`sys->et_lp`).
4. Sub-models that became texture owners get their textures uploaded from the source room's
   entries (`bhSetMemPvpTexture`); copies of their texlists are kept and released by a hook in
   `ps2_texture.c bhReleaseMainTexture` (and forgotten by `njReleaseTextureAll`).
5. The motion set: the room's own for the type, else the source's copied (spawn pool) and realized
   (`bhSetEneMtn`: 512 `MN_WORK`s from the arena, sets `sys->emtp[id]`). Then `bhKeepObjWork` + `bhCalcModel`; the type's Init
   runs on the next tick.

The spawned enemy has no record: no kill bit, scripts never see it, gone after a room change.
Sources: the smallest room holding the model and its type's motion set (several are the 55xx..58xx
rooms); `pc/tools/enemies.py sources` lists them.

### Results [verified unless marked]

Headless runs in rooms without enemies of their own: the statue room 1/4 (RM_1040, wide camera,
3.4 MB of room textures) and the piano room 1/11 (RM_1110, cramped, 6 MB of room textures);
player standing still at the entry with infinite health, firing the handgun or the rocket
launcher in bursts; each type in its own visit, then a door to the other room and back
("Verification log"). "Attacks" = the player's HP dropped while the enemy was in an attack mode
next to it; "dies" = `flg & 2` and the type's `Die` mode.

| Entry (id) | Source room | Moves | Attacks | Damaged | Dies | Notes |
|---|---|---|---|---|---|---|
| Zombie: soldier (1, model 2) | RM_1070 | yes | yes (15 per bite) | | | 8 slots per zombie (1 + 7 parts) |
| Zombie: lab coat (1, model 5) | RM_3200 | yes | yes (10) | yes | | |
| Zombie: soldier 5 (1, model 32) | RM_5680 | yes | | yes | yes | |
| Zombie: prisoner (1, model 0) x 17 | RM_5520 | yes | | | | 17 in one room: 119 slots, all running; the 18th would be refused (then: 11 free needed; now 10 + 6 kept free) |
| Hunter (5) | RM_8010 | yes | yes (15..20) | yes | yes | 2 slots; in 1/11 Hunters and a Tyrant got stuck at furniture out of camera |
| Bandersnatch (9) | RM_2000 | yes | yes (10..30) | yes | yes | |
| Cerberus (4) | RM_3020 | yes | yes (6..12) | yes | yes | the source record's variant 1 waits for the room's wake-up areas forever: spawned as variant 0 |
| Black Widow (3) | RM_7080 | yes | yes (12) | yes | yes | |
| Giant Black Widow (23) | RM_9050 | yes | yes (40) | yes | yes | |
| Tyrant (19) | RM_5840 | yes | yes (40) | yes | yes | dies into its brain's DIE state (frozen death pose); the story continues with scripted scenes |
| Nosferatu (15) | RM_5800 | yes | yes (50, then an instant kill through infinite health: game over) | yes | yes | |
| Monster Steve (17) | RM_9280 | yes | yes (120) | | | arms as helper slots 31, 32 |
| Anatomist zombie (26) | RM_0110 | yes | yes (grab) | | | |
| Alexia, first form (12) | RM_5820 | yes | yes (grab; the grab ended in a game over) | | | |
| Albinoid adult (22) | RM_7100 | yes (walks on land) | not attributed | | | a water creature |
| Bat (7) | RM_0130 | flies | none seen | yes | yes | hovered at ceiling height above the player |
| Moth (6) | RM_6030 | flies | none seen | yes | yes | |
| Spotter (11) | RM_7081 | flies | none seen | | | |
| Gulp Worm (2) | RM_3091 | burrows | one hit (5) | | | 19 slots (segments); wanders out of the room's bounds: needs its sand arena |
| Fish (8), ants (10, 20, 71), tentacle (29), ceiling trap (25), Nosferatu's core (53), exploding prop (0) | | barely / no | | | | spawn and run their code without errors; they belong to water, walls, their boss or an event |
| Baby Black Widow (24), Alexia's baby (30) | | | | | | **refused**: their code reads the parent that spawns them (`lkwkp`); Alexia's baby crashed on a NULL parent |
| Alexia second / third form (13, 14) | | | | | | **refused**: the second form read its partner pointer (`exp0 + 112`, NULL) and crashed |
| Alfred (16) | | | | | | **refused**: outside his room his boss logic corrupted memory (crash in the audio mixer and in the next weapon load) |

Refusals checked (PS2 budgets, before the port lifted them): a Hunter after a zombie ("shares
helper code with type 1"), Monster Steve after a Giant Black Widow, the Tyrant, Nosferatu and
Alexia in 1/11 after five other foreign types ("texture memory full (82 entries, 701 KB free)"),
the parent-bound and boss types above. Every
door after a spawn logged `spawn: released N texture lists`; the rooms reloaded and later spawns
of the same and other types worked; a game over (Nosferatu) went through `njReleaseTextureAll`.
Memory per foreign type: model 6..140 KB + motions 0..935 KB (then in the arena, now in the spawn
pool); 1..20 textures.

### Budgets on the PC [verified]

What refused spawns was the PS2's budgets: measured with the `RECVX_SPAWN_SURVEY` test aid over
13 rooms of stages 0..3 (RM_0020, 0050, 0060, 0110, 1000, 1040, 1110, 2010, 2030, 2070, 3090,
3130, 3140; 2.2..6.1 MB of room model textures, 62..172 texture entries free on arrival), all 58
entries in each, before (main + the test aid) and after the port's changes:

| | before | after |
|---|---|---|
| each entry alone in the freshly loaded room | 654 of 754 spawn; refused: helper code 35 (the 7 helper types in the 5 zombie rooms), parent-bound or boss 65 | 689 of 754; refused: parent-bound or boss 65 (Baby Black Widow, Alexia 2 and 3, Alexia's baby, Alfred: section 6 table) |
| all 58 in a row in one room (they stay) | 114 of 754: 3..21 types per room; refused: texture heap or entries 360, helper code 215, parent/boss 65 | 376 of 754: 25..30 types per room; refused: enemy slots 313 (122..127 in use at the end), parent/boss 65 |

At the end of the second survey each room still had 675..785 texture entries, 50..55 MB of the
texture heap, 8.3..10.3 MB of the arena (with the screens' copy of 2.2..6.1 MB kept free) and
25..26.5 MB of the spawn pool free. The 128 enemy slots are what is left: they are byte-sized
in the decomp (`idx_ct`, slot fields) and in the co-op packets, so raising them is a protocol
change.

Crashes the higher budgets exposed and fixed (none occur in retail rooms):
* a Gulp Worm with too few slots left (`bhEne02_Init`, NULL `bhSetEnemy`): its Init takes 19
  slots, the spawner counted 8; counts are now from the Inits, plus 6 kept free;
* an added albinoid adult's light (`light.c bhControlLight`): `en22.c` links it by slot, which
  the light code reads as a record number; beyond the room's records it is now the slot;
* a Black Widow's grab and another type's hit at once (`player.c bhCPM0_damage`): the grab
  writes damage level 4/5, beyond the player's damage motions, after the hit took the player
  over; clamped to the heaviest level;
* Monster Steve's throw code reading a zombie's grab motions (`en17.c bhEne17_PlyDG01`). Within
  a family an enemy checks the player's busy flag (`flg & 4`) before it attacks; across
  families not always. In rooms with added enemies, one enemy at a time takes a player
  (`eneset.c pc_call_enemy`): the enemy whose code set the flag owns the player until it
  clears, and the player work another enemy's code changed meanwhile is put back (parts and
  children count as their parent; per player work, so co-op's partner too). A zombie whose grab
  the rule refuses (its step put the held player into a grab mode) lets go at once, as at the end
  of a grab (`coop_glue.c pc_enemy_unhold`: standing, then the chase): before, it played its grab
  and bites on nobody for a few seconds. Other types keep their own behaviour (their attack runs
  on, its effect on the player undone).
* Frozen-enemy watchdog (`coop_glue.c pc_enemy_watch`, called from `bhControlEnemy` for every
  enemy step, single player and co-op): an alive enemy whose modes, motion, frame, position and hp
  stay the same for 150 steps is logged; a zombie stuck in a grab state is released. A safety net
  only - the co-op freeze it found had its own cause (COOP.md, frozen zombies).

Mixing types the PS2 never mixed can still show odd behaviour the survey does not catch (a
Hunter outside its rooms leapt out of the room's bounds in 1/4).

### Sounds of added types

How enemy sounds are addressed [code][data]: enemy code requests a sound id (`RequestEnemySe`,
`bhEne_CallSE`, effects through `bhEne_CallEffectSE` as enemy 127); bits 0..7 are the list
(program), 8..11 the SE bank, 12..15 how many may play at once, 16..19 the priority. Every enemy
id in the enemy code is in **bank 3** (98 distinct literals; besides that only the player's bank 0x400 and
one Nosferatu sound in the room bank 2). The sound manager (`ExecEnemySeManager`) plays it on one of
6 enemy SE handles (`CallEnemySeMain` -> `sdShotPlay`), which the driver plays from SE port 3. A
room's sound pack `RM_SRRC.SPQ` (key `stage*1000 + room*10 + case` in `MULTSPQ1.IDX` for stages
0..5, `MULTSPQ2.IDX` for 6..9; entries {offset, size, type, bank}, type 2 = SE bank: {header
offset, size, body offset, size}) loads SE bank 2 (room sounds) and, in rooms with enemies, bank 3
with the sounds of the room's own types; rooms without enemies have no bank 3 and keep the
previous room's. List numbers are per type and collide: zombie lists 0..19 are the same sounds in
every zombie room (RM_0150, 1070, 3060, 5520, 9120 ...), while the Hunter's (RM_8010) and Cerberus'
(RM_3020) lists 0..20 are other sounds. So an added type played another type's sounds, or silence
where the room's bank was shorter.

The port: when `spawn_at` realizes a type the room file has no record of (F2 spawner, randomizer,
co-op guest; the extra enemies copy the room's own types), `spawn_glue.c sound_load` reads bank 3
of its source room from the disc (`snd_iop.c snd_xbank_load`, either half; a source room without
one, e.g. Bandersnatch 2's RM_3130, falls back to another catalogue room of the type) into an extra
synth bank (one per source room, at most 31), and remembers type -> bank. Each request is
attributed when it is made (`sdfunc.c RequestEnemySeBasic` -> `eneset.c pc_enemy_sound_type`): the
enemy whose code runs, a part or child as its creator's type, an effect's request (enemy 127 from
the effect's step, e.g. the Black Widow's acid) as the type whose code made the effect (`effect.c
bhSetEffectTb`). A bank-3 sound of a type with an extra bank goes out with the bank in bits 3..7 of
the request's port byte (`ps2_sg_sd.c sdSysServer`, kept per SE handle; 0 on the PS2) and the
driver plays it from that bank (`snd_synth.c`); a type whose bank could not be found (31 in use,
none on the disc) plays nothing instead of another type's sound. The room's own types, event
sounds (`EneSeOn`) and everything else keep the room's banks, so vanilla rooms send the same driver
commands. `bhSetRoom` (`spawn_room_setup`) stops the voices on extra banks and releases them.
Variants of one type share a bank (the first spawned one's source room): zombie lists 0..19 agree;
room-specific extras (e.g. RM_5520's lists 36..39 = RM_0150's 52..55) may not. `RECVX_TRACE_SND=1`
logs `SE port 3 ch C prog P from extra bank B`; the spawner logs `snd: extra bank B = RM_xxxx.SPQ
bank 3` and `spawn: type T plays its sounds from extra bank B`. The player's voice in another
character's voice takes one or two of the same 31 (`CORE_00n.SPQ` bank 4, docs/ACTORS.md 8
"Voices").

**Co-op** (protocol 10, two local instances, section 4 and the verification log): the host's
spawned Hunter appears on the guest in the same slots (`0:5 1:31` on both); the guest's handgun
took it from 135 HP to its death, seen dying on both sides; a spawned zombie and Cerberus bit the
guest; after the guest left and came back, and after a rejoin, the Hunter was sent again and
realized; a door cleared it on both sides. The spawner refuses on the guest.

### Effect textures of added types

User report: "Alexia First Form's blood is invisible and the fire is a broken texture with
Japanese kanji". Context: Alexia's first form added by the F2 spawner (or, the same path, a co-op
guest realizing the host's) in any room without an effect record 80, i.e. every room but her own
(RM_5820, RM_9301, RM_9321) [verified]. Her "blood" is the burning liquid she leaves on the floor
when hit (265, below): drawn from the kanji page with the fire's UVs, mostly blank there, so it
looked missing [inferred]; her red blood bursts are global and were fine. Her own room is unchanged and was fine (frame dumps
byte-identical to main's, with and without a spawned Alexia). The randomizer never picks her; the
player appearing as her only draws her model (no enemy code, no her effects) [verified].

Cause [code][data]: effect *ids* 250..299 are the enemy effects, global code, but an effect picks
its texture by `tex_id` through `sys->ef_tn[tex_id]`, and some take a room effect texture (ids
30..99 and 400+), which `room.c bhSetRoom` uploads only for the room's own effect records (one
texture table entry per id, the table's last entries) and zeroes in `ef_tn` otherwise. Index 0 of
the effect texlist (effect 0's first texture) is a page of kanji glyphs in the frame dumps. Every enemy effect with such a texture,
from the effect code (`effsub2.c`) and the en*.c tables:

| Type | Effects (id: function) | Room effect texture | Rooms with it |
|---|---|---|---|
| Alexia, first form (12) | 262 `bhEff_E12_FireManager`, 263 `bhEff_E12_Fire` (her fire), 265 `bhEff_E12_FrameLiquid` (her burning blood on the floor, `bhEne12_Blood`) | 80 (fire, 2 TIM2) | RM_5820, 9301, 9321, 5810, 9302, 9370 |
| Gulp Worm (2) | 253 `bhEff_E02_SandDust` | 82 (variant 0), 83 (others) | 82: RM_3091, 7220/7221; 83: RM_5830, 5810, 7230/7231, 9302 |
| Alexia, third form (14; refused) | 269 `bhEff_E14_Fire`, made by her acid (`en14.c`: `bhEff_E03_Acid` type 6) | 80 | as above |
| Alexia's baby (30; refused) | 267 `bhEff_E13_Fluid` (`en30.c`) | 81 | RM_5810, 9301, 9302 |

(273 `bhEff_E14_Mucus`, 81, and 147/149, 80, are only made by room effect records.)

Everything else enemies make uses global effect textures or the enemy's own model texlist
(`stflg & 0x20`, `txp`): blood bursts and drops (296/297, 251 `DropBlood`: 5 + colour), floor
blood (266: 9), acid (256/257/298: 2 and the Black Widow's own), Nosferatu's gas (397, own),
Tyrant/Bandersnatch/zombie parts (35x..37x, own), the moth's scales (260 `Rinpun`: 1..4), sparks
and stains (301..308: `ef_tn[4]`/`[5]`), fire on burning enemies (252 `E00_Fire`: 8) [code].

Fix (`spawn_glue.c fx_load`, after `sound_load` in `spawn_at`): for a type in the table (12: 80;
2: 82 or 83 by variant; 14 and 30 are refused) whose texture this room has not loaded
(`sys->ef_tn[fx] == 0`), the entry is read from the type's source room (`actor_glue.c
actor_fx_tex`: the room file's distinct effect ids 30..99/400+ in record order map to the last
texture table entries) and uploaded exactly as `bhSetRoom` does: appended to `sys->ef_tlist` at
`ef_ct`, counted in `ef_extn`, `ef_tn`/`ef_pbkb` set. `bhReleaseMainTexture` releases `ef_extn`
textures with the room's, and the next `bhSetRoom` resets `ef_tn`, so nothing else is needed. Log:
`spawn: type 12: effect texture 80 from RM_5820 (2 textures)`. Fallback (`effect.c`, after each
effect step): an effect whose room texture is not loaded (not found, effect texlist full: 450
entries) gets the "not drawn" flag (`flg 0x1000000`, which every draw path checks) instead of
drawing texture 0; logged once per effect id (`effect N: room effect texture T not in this room,
not drawn`). In the vanilla runs below it never fired.

## 7. Enemy options (implemented)

OPTION -> PC SETTINGS (and F1): CO-OP "Extra enemies (host)", GAMEPLAY "Rearranged enemies", "Enemy
randomizer", "Enemy seed" (`pc/src/host/settings.c`); ini keys `enemy_extra` (0..3), `enemy_place`
(0/1), `enemy_random` (0/1), `enemy_seed`; test aids `RECVX_ENEMY_EXTRA`, `RECVX_ENEMY_PLACE`,
`RECVX_ENEMY_RANDOM`, `RECVX_ENEMY_SEED`. All off by default; off, the code returns before touching
anything (frame dumps of the attract demo and a new game are byte-identical to main). In co-op the
host's options count for both (section 4). Code: `pc/glue/enemyopt_glue.c`, one decomp hook at the
end of `room.c bhFinishRoom` (`enemy_room_finished`).

Every option first reads the room script once per load (`scan_script`, the same instruction
lengths as the co-op script sweep) and marks records that an event drives (`LoadWork`, motion
pauses, links, positioning, zombie/face/look/render ops), that the script shows or hides
(`InitModelSet`), whose HP the script tests or sets, and rooms with an "all dead" count
(`DieEventCk`). Marked records are never changed. Choices come from a xorshift generator seeded
with (seed, room file, record), never from the game's `rand()`.

### (a) Extra enemies (co-op difficulty)

* "Up to N per room" (N = 1..3): at the first frame a room runs (after its enemies' Init), the
  host (or the single player) picks up to N of the room's unmarked, visible, living records of the
  roaming types zombie, Black Widow, Cerberus, Hunter, Bandersnatch, albinoid adult, anatomist,
  and spawns a copy of each from the current room file (`spawn_add`, so it reaches the guest as an
  added enemy): same type and model version, so its textures and motions are the room's own (no
  upload); variant 0; placed 15..35 units from the source in one of 16 directions where a line
  test against the room's walls (`hitchkl.c bhCollisionCheckLine2`, as the Hunter's sight test)
  is clear for a body's width beyond the spot, at least 40 units from every entry position and
  from the player.
* Deterministic per (seed, room) and the room's living records: the same extras on every visit while
  the same records live (a record killed since is no source any more). They have no kill bit: they
  come back with the room, and "all dead" counts (`DieEventCk`) ignore them. In co-op an extra the
  guest killed while the host was away stays dead for that visit if the host's new one is the same
  type in the same slot (section 4).
* Budgets: the spawner's checks (slots: a zombie extra takes 8; arena; textures). A room that
  runs out stops adding.
* Verified (single player): RM_0060 (4 zombie records, one script-shown): 2 extra zombies in slots
  30/31 at the same spots on a second visit; RM_1000: 3 extra Cerberus; RM_0080: none (all its
  zombies are event-driven). Co-op: section 4 and the verification log.

### (b) Rearranged placements

* At the room load, each unmarked record of a roaming type (as above plus bats) starts 8..30 units
  from its own spot or (half the time) from another such record's spot on the same floor, in a
  direction with a clear line, 40+ units from every entry position; random angle. Kill bits,
  slots and types stay; a waiting Cerberus (variant 1/2) becomes variant 0 (it would wait outside
  its wake-up area forever).
* Not moved: story/cutscene/scripted enemies (marked records), bosses and set pieces (not of the
  roaming types), NPCs.
* Because only the room file and the seed decide, host and guest place identically; positions are
  also in the snapshots.

### (c) Randomizer

* At the room load, unmarked records of the types zombie, Black Widow, Cerberus, Hunter,
  Bandersnatch become other types from a pool: zombies (all 24 looks), Cerberus, Bandersnatch (both
  looks) from stage 0, plus Black Widow and Hunters (both looks) from stage 2.
* Per room up to two new kinds from the pool. On the PC any mix is allowed (helper code per
  type, `et_lp` rows of their own, a 64 MB texture heap: section 2.2; until then the room's
  whole type set had to avoid helper-code conflicts, and a texture budget from the room's
  texture section allowed 0..2 kinds the room did not load). Slots are counted with what each
  Init spawns (`spawn_slots_needed`: zombie 10, Black Widow 25, others 1..3) up to 122 (6 stay
  free).
* Whole rooms are skipped: rooms whose script counts kills (`DieEventCk`), rooms with a boss or
  set piece (ids 0, 2, 11..19, 23..25, 29, 30, 53..56). Fliers (bats, moths) are not replaced
  (their records hover).
* The record keeps its slot and kill bit: `spawn_at` realizes the new type into the record's own
  slot (`want = k`, during the load); `bhReleaseMainTexture` releases the new type's textures with
  the record, the original's go through the spawner's release list.
* **Softlock-safe fallback**: any refusal (data not found, memory, textures) puts the original
  record back exactly as it was; the room is never left with fewer enemies or other kill bits.
* **Offline alternative (no runtime code):** `pc/tools/rdxbuild.py` resolves multiple
  `enemy_assets` donors (deduplicated textures, per-type motions) and rejects incompatible helper
  families, model versions >= 16 and excessive child-slot budgets; the six-room
  [Quarantine Annex](AREA_PLAN.md) uses it. Randomized room files built this way would still need
  progression checks and per-seed validation.
* Verified (single player, seed 1234): RM_0060, RM_0120, RM_1030 randomized (zombie looks, frozen,
  lab coat, dark uniform; Bandersnatches for bats), RM_0160 skipped (exploding prop = set piece),
  RM_1000 kept (its texture budget allowed nothing new and the pool only offered its own type).
  The rooms ran with their new enemies attacking (frame dumps: a lab-coat zombie in RM_0120).
  Seed 777 through 19 rooms of stages 0..3 (verification log 7): no crash; in co-op both sides
  made the same choices (verification log 6).

What stays out and why: bosses and set pieces (their scripts, arenas, cameras), event-driven and
script-shown records (cutscenes wait for their motions), records with scripted HP, "all dead"
rooms (a door counts the kills), water types (fish, albinoids need water), fliers, parent-bound
and boss-only types (section 6), Tyrant/Giant Black Widow (story bosses).

### Limits

* The extra enemies only on the host: in a room only the guest is in, it has no extras until the
  host comes in (then they arrive as added enemies).
* New types play their own sounds from their source room's bank (section 6, "Sounds of added
  types"); two looks of one type share the first one's bank. Their effects get the room effect
  textures they need (section 6, "Effect textures of added types"; none of the randomizer's pool
  needs one).
* A refusal by `spawn_at` (slots, memory) keeps the original record (logged as `record N stays`);
  both co-op sides decide the same unless one side's memory is fuller.

## 8. Tools

`pc/tools/enemies.py` (Python 3, stdlib, imports `rdx.py` without changing it; reads the disc at
run time):

* `room ROOM` - the room's enemy records, its enemy floor areas and route boxes, and every script
  reference to each record with the conditions of the enclosing blocks.
* `census [--rooms]` - counts per type and room, kill flags, hidden/shown/event-driven records,
  helper conflicts, case variants, `gm_mode`/`ply_id` tests, floor areas, routes.
* `sources` - per (type, model version) the smallest room carrying its model and motions.
* `selftest` - the script walker on RM_0060.

## Verification log

All runs: 32-bit build of this branch, `SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
RECVX_SKIP_MOVIES=1`, `--skip-intro --size 320x240`, new game from the title by scripted input,
`RECVX_TELEPORT` to 1/4 or 1/11 and back, `RECVX_EQUIP` (item 5 handgun or item 1 rocket
launcher), `RECVX_INF_AMMO=1`, `RECVX_INF_HEALTH=1`, `RECVX_INPUT` with R1 held and Cross pressed in
bursts, `RECVX_SPAWN` with its trace, `RECVX_DUMP_FRAMES` (frames looked at for the Hunter,
Cerberus and Tyrant runs: the creatures render textured, the Hunter leaps and claws, blood on
Claire). The machine was shared and slow (5..15 game frames/s), so each run covered 2..4 visits.

1. Census (`pc/tools/enemies.py census`, `sources`, `room`): numbers in sections 1..3.
2. Spawner, first version in 1/11: a Hunter took handgun damage but stayed at one spot out of
   camera; texture release fired at a wrong time (the frame counter `gfrm_ct` restarts every 60
   vsyncs) -> `Ps2CurrentTexLoad ERROR` on the still drawn enemy: replaced by the
   `bhReleaseMainTexture` hook plus a room-change fallback outside transitions.
3. A zombie, a Cerberus and a Bandersnatch in 1/11: the zombie bit the player repeatedly, the
   Cerberus (source variant 1) never woke, the Bandersnatch was refused for texture memory with the
   appearance loader's 2 MB margin -> spawner margins of its own (1 MB heap, 16 entries), variant 0.
4. Per-type runs A..H2, G..G7 in 1/4 and 1/11 (table above); crashes found and turned into
   refusals: Alexia's baby (`en30.c bhEne30`, NULL `lkwkp`), Alfred (memory corruption, crash in
   the audio thread and `bhReadWeaponData`), Alexia's second form (`en13.c bhEne13_MV03`, NULL
   partner).
5. After merging main (lobby, protocol 9): a single-player run (Bandersnatch and a zombie in 1/4,
   door to 1/11 and back) and the two-instance co-op run above.

6. Co-op sync (protocol 10, two local instances, `scratch`-style scripts: host new game + teleport,
   client joining from the title, `RECVX_COOP_STATS=1`, `RECVX_SPAWN` traces on both):
   * statue room 1/4, host spawns a Hunter: the guest logs `partner's enemy type 5 ... into slot 0:
     done`, both slot lists `0:5/1 1:31/1`; the guest's handgun takes it 135 -> 122 -> ... -> -8,
     the host's trace shows each hit and the death (mode 4, `flg & 2`), the guest's copy dies too;
     a spawned zombie and Cerberus next to the guest bite it (`player hit` on the guest);
   * the guest teleports to 1/11 (its copy released with the room) and back: the host logs
     `1 added enemies sent to the partner (it entered the room)`, the guest realizes it again;
     `RECVX_COOP_REJOIN`: the same after the new session's game state;
   * the host's door to 1/11: both slot lists empty there;
   * options on the host only (extra 2, placements, randomizer, seed 1234), tour 0/12 -> 0/6 ->
     1/0: the guest logs the host's options, makes the same randomized records and placements,
     gets the extras into the host's slots (`first free here` equal), slot lists identical (45
     and 46 slots in 0/12 and 0/6). The first run crashed the guest in 0/12: randomized lab-coat
     zombies wear a cap (a linked part with a model of its own) and the puppet code played the
     zombie's motions on it - fixed in `coop_glue.c puppet_step` (COOP.md).
   * packet sizes and ping: section 4 / COOP.md.
7. Enemy options, single player: extras in RM_0060 / RM_1000 (same spots on a second visit);
   randomizer + placements + extras with seed 1234 through RM_0060, 0090, 0120, 0130, 0140, 0160,
   1000, 1030, 1060 and with seed 777 through 0020, 0030, 0050, 0070, 0080, 0090, 1020, 1060, 1070,
   1100, 1120, 2000, 2010, 2020, 3000, 3020, 3030, 3050, 3060 (teleport into each, a few hundred
   frames, rocket bursts): no crash, randomized enemies initialised and attacked; skipped rooms
   logged (RM_0080/0160 exploding props, RM_1020 Alfred). Found: extra enemies copied from a
   randomized record looked up the old type - they now copy the new type.
8. Vanilla: with all options off, the attract demo and a new game (scripted input, every 30th
   frame) are byte-identical to main's build (all frames both runs reached).

9. PS2 limits raised (texture heap 64 MB, 1024 entries, helper code per creator type, `et_lp` rows
   for model versions >= 16, spawn pool, slot counts from the Inits), headless, 32-bit Linux
   build, shared machine:
   * survey (section 6, "Budgets on the PC"): before 654/754 alone and 114/754 together, after
     689/754 and 376/754; the remaining refusals are the five parent-bound/boss types and the
     128 enemy slots;
   * the slot-exhaustion refusal: reached in every room of the second survey (122..127 slots);
   * 1/11 (RM_1110): Monster Steve, a zombie, a Hunter, a Black Widow, zombies of models 32 and
     21, a moth, a Bandersnatch, a Cerberus and an anatomist zombie at once (formerly refused:
     helper code, texture memory): all spawned, ran their AI (motions advancing, the player hit
     9 times); the door to 1/4 logged `released 17 texture lists`, 1/4 and 1/11 arrived with
     the same texture entries, heap and arena as at the first visit (minus the equipped weapon),
     no spawned enemy left in 1/4's slot list;
   * 1/4 (RM_1040), handgun: Monster Steve and a Hunter behind the player, a zombie in front: the
     zombie took four hits and died (mode 4, `flg & 2`, death pose) next to Steve's arms and the
     Hunter's helper running their own code; Steve's throw and the Hunter's slash hit the player.
     A zombie, Monster Steve, a Hunter, a Black Widow and a zombie of model 32 together for 21000
     game frames with a door to 1/11 and back (`released 9 texture lists`, both rooms arrived with
     the budgets of the first visit), 20 hits on the player, no crash. This is how the two
     crashes of cross-type player handling in section 6 ("Budgets on the PC") were found;
   * a 12-room session (1/4, 1/11, 2/3, 0/6 three times each, 25..30 types added per visit,
     `RECVX_NATIVE_TEX=1`): every visit arrived with the same texture entries, heap, arena and
     spawn pool as the first; resident memory grew to 240 MB during the first cycle (the larger
     heap and pool being touched) and stayed at 241..247 MB, virtual size at most 837 MB;
     address space left at the end (`malloc` in 16 MB blocks): 3232 MB of the 4 GB a 32-bit
     process has on 64-bit Linux (on Windows, without large-address-awareness, 2 GB in all);
   * co-op (two instances, `--coop-host`/`--coop-join`, host in 1/4): the host spawned a zombie,
     Monster Steve, a Hunter, a Black Widow and a zombie of model 32 (helper families that could
     not share a room before); the guest realized each into the host's slot (`first free here`
     equal, 0, 8, 11, 13, 38), both slot lists identical (ids and modes), the guest was bitten;
   * vanilla (options off, no spawns): a new game (scripted input, 2400 frames, every 30th
     dumped) gives the same 77 of 79 frames as main's build; frames 1590 and 1620 differ between
     any two runs of the same build too (main's included). The attract demo (memory card notice
     skipped with Start, 4000 frames, 133 dumps) also varies from run to run with main's build
     (two different outcomes in three runs: the title after the first demo starts a few frames
     earlier or later); one of the two final runs gave main's majority outcome byte for byte;
   * `ctest` (with the disc image) passes; the Windows build (mingw-w64 GCC) links: image
     0x30000000..0x3C773000 (199 MB, of which the raised heap and the pool are 86 MB of `.bss`),
     not large-address-aware, so 2 GB of address space.

10. Sounds of added types (section 6), headless, 32-bit Linux build, `RECVX_TRACE_SND=1`:
   * 1/4 (RM_1040: no enemy records; its bank 3 is the bats' from RM_0130), audio through SDL's
     `disk` driver (150 s mixed, no crash): a Hunter, a Cerberus, the Tyrant and a prisoner zombie
     spawned: `extra bank 1 = RM_8010.SPQ` (Hunter), `2 = RM_3020` (Cerberus), `3 = RM_5840`
     (Tyrant), `4 = RM_5520` (zombie); 148 requests went out on the extra banks (Hunter lists 3,
     4, 7; Cerberus 0..2, 4; Tyrant 0; zombie 0, 3, 9, 10, 15, 50, 51), 139 of them followed by
     the key-on of their bank's sample (the rest were cancelled by the next request);
   * 1/4, a Black Widow and a Hunter (`RM_7080`, `RM_8010`): Black Widow lists 3, 5, 6, 8 (its
     acid splashes are 4/5/8, made by the effect) from its bank; the door to 1/11 logged
     `released 2 extra banks`;
   * randomizer (seed 2, RM_3060): a record became a Hunter (Sweeper): `extra bank 1 =
     RM_9130.SPQ`, its lists 3, 4, 7 from it; the zombie record stayed on the room's bank (the
     room's own type); seed 777: zombies only, no extra bank;
   * vanilla (no options, no spawns): a new game, 2400 frames: the frames equal main's build except
     1590/1620 (which differ between any two runs, item 9), and the driver command trace is the
     same as main's apart from how many times the per-frame status/volume lines repeat, which also
     differs between two runs of main's build.

11. Effect textures of added types (section 6), headless, 32-bit Linux build, 640x480, new game,
    `RECVX_TELEPORT`, handgun bursts, `RECVX_SPAWN`, `RECVX_DUMP_FRAMES` every 10th frame,
    `RECVX_EFF_TRACE` where noted; main = 2de0549:
   * Alexia's first form in 1/4 (RM_1040): main drew her fire columns and floor fire as kanji
     pages (frames 660..700); fixed: `effect texture 80 from RM_5820 (2 textures)`, fire on her
     hands, columns and burning blood on the floor (frames 640..760), red blood bursts on hits;
   * Gulp Worm in RM_5830 (its sand arena has only texture 83, the spawned variant 0 needs 82):
     main drew its sand spray as glyphs (hearts, digits; frame 2550), fixed: sand clumps from
     RM_3091's texture 82; in 1/4 the trace shows its 253 effects queued for drawing;
   * her own room 5/82 (RM_5820): vanilla and with a spawned Alexia (her fire and floor fire
     shown), frames byte-identical to main (377 and 434 frames);
   * Black Widow, moth, Bandersnatch, then Tyrant, Hunter, Nosferatu in 1/4: no "not drawn" log,
     no kanji page in any frame (scan for grey/white areas), Nosferatu's gas drawn;
   * the player as Alexia's first form (`RECVX_APPEAR=5:21`) bitten by a spawned zombie in 1/4:
     no missing or wrong effect texture (only her model is drawn; her effects come from en12.c);
   * vanilla RM_0060 (zombies, handgun, 381 frames): no effect hidden; frames equal main's up to
     frame 1180, then sub-pixel differences on a zombie from 1190 and game-state drift later.
     Main's own sources with a 4 KB data array added (the same addresses as the fixed build,
     `malloc area` 0x1000 higher) give exactly the same frames as the fixed build up to 1680,
     where two runs of any build part; so the 1190 differences come from the address layout,
     not from the change;
   * `ctest` passes (movies skipped); the Windows build (mingw-w64 GCC) compiles and links.

Not verified: CLUT bank collisions, a whole stage played through with
the randomizer (only teleports into each room), zombie grabs of the guest by an added zombie
(the M7 path is the same as for room zombies), a guest whose texture heap is fuller than the
host's (the randomizer's fallback could then differ between the two).
