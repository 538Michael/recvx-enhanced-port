# Quarantine Annex — original connected area

Status 2026-10-05: built, validated offline and accepted in game (headless runs, section
"In-game acceptance results"); four bugs found there are fixed in the pipeline.
The annex is an abandoned palace service wing pressed into use as an improvised quarantine
station. Green glazed tile gives way to damp concrete, rusted isolation cages and wood record
shelves. Its story is told with furnishings and original signs, without retail cutscenes.

Six new rooms use stage 1, rooms 16–21, case 0 (RM_1160–RM_1210). RM_1150 remains reserved
for Service Corridor. All work is source-generated; donor enemy bytes are read only during
local builds and remain in ignored mods/. No new game-code hooks are required.

## Graph and progression

```text
Retail RM_1110 piano room <-> [116 Reception] <-> Retail RM_1020/1021 palace hall
                                  |     |
                            [117 Wash] [121 Dispatch]
                                  |     |
                            [118 Records] <-> [119 Isolation]
                                                    |
                                              KEY [120 Supply]
```

Every line is a pair of animated doors (type 22). Reception subdivides the existing
111↔102 connection, leaving a safe route through the retail story. The original retail
conditions still govern access; patch both cases of 102. The annex loop is always open.
The only lock gates optional supplies, and unlocks permanently when the Records token is
picked up. It tests the token's unique saved pickup flag, not a retail key/story flag.
Returning from Supply is always possible, including direct teleports without the token.

## Room briefs

All rooms are single-level, Y=0, with 30-unit perimeter walls, collision independent of
rendering, generous central paths and two fixed cuts split at mid-depth. Each half is shot
from inside the room, from under the ceiling at the opposite end wall (y=29, pitch 33°,
`pers` 14000): the south half from the north wall looking -Z, the north half from the
south wall looking +Z. Nothing is hidden by cut masks. Entrances face inward;
items are away from camera seams. Each room participates in the single Records→Supply lock
puzzle; extra per-room story locks would add risk without improving this small area.

| ID | Name / purpose | Layout and camera beat | Items | Enemies / puzzle role |
|---|---|---|---|---|
| 1160 | Reception | 64×80; check-in counter, benches, sign; first shot explains retail exits, second reveals annex fork | Handgun ammo, green herb | Safe hub; visible route toward token |
| 1170 | Decontamination | 56×100; side wash basins, tiled drain, locker banks; camera changes beyond basin | Handgun ammo, green herb | Two zombies; clear retreat; approach to Records |
| 1180 | Records Office | 64×80; wood shelving, desks and file stacks; rear shot frames key beside desk | Annex token, green herb | Two zombies; token unlocks Supply |
| 1190 | Isolation Gallery | 72×100; cages and examination beds along sides, broad central combat lane | Handgun ammo, green herb | Zombie + Cerberus (compatible helper slots); locked Supply door marked TOKEN |
| 1200 | Emergency Supply | 56×80; pallets, medicine shelves and ammunition cases; second cut frames reward | Handgun ammo, red herb | Safe optional reward behind token lock; exit never locked |
| 1210 | Dispatch Bay | 72×100; freight crates and trolleys; second shot reveals return door | Handgun ammo, green herb | Two Cerberus, variant 0; closes the return loop |

Materials: deterministic stdlib PNGs for weathered concrete, brushed/riveted steel, glazed
tile, wood grain, rust, warning stripes, stencil signs and supplies. Props are original
box assemblies baked into the room mesh with explicit simple colliders; pickups have
separate original models. No extracted textures or meshes are source inputs.

Enemy models/motions are runtime build dependencies on the user's ISO. Use zombie model
1 from RM_0060 record 0, and Cerberus model 0 from RM_3020 record 2, overridden to active
variant 0. No zombie model >=16, no mixed helper-code installers. Worst encounter uses
22 conservatively reserved enemy slots; cap authored textures at 192/6 MiB, reserve 64
entries/4 MiB for persistent assets, and check packed+expanded arena occupancy.

Flags are checked against every script in all 205 AFS rooms, including generic Ck/Set
and counted death ranges. Allocate pickups 460–471 and deaths 460–467 in their separate
flag banks; reserve demo it_flg 506 / ed_flg 500. Build fails if any allocation is used.

## Retail room census

RDX_LNK.AFS has no filename table: its 205 TOC entries map in order to the executable's
rdx_files[] list. The builder verifies the TOC count and scans every entry. These are all
retail SRRC IDs (prefix RM_, suffix .RDX); none of 1160–1210 collides:

```text
0000 0010 0020 0021 0030 0031 0040 0050 0060 0070 0080 0090 0100 0110 0120 0130
0140 0150 0160 1000 1001 1002 1020 1021 1030 1040 1050 1060 1070 1080 1090 1100
1110 1120 1121 1122 1130 1140 2000 2010 2011 2020 2030 2031 2040 2050 2060 2070
3000 3010 3011 3020 3030 3040 3050 3060 3070 3080 3090 3091 3100 3110 3120 3130
3140 3150 3160 3170 3180 3190 3200 3210 3220 3230 3240 4000 4001 4010 4011 4012
4020 4030 4040 4050 5000 5001 5010 5500 5510 5520 5530 5540 5550 5580 5590 5600
5610 5620 5630 5640 5650 5660 5670 5680 5690 5700 5710 5800 5810 5820 5830 5840
6000 6010 6020 6030 6040 6050 6051 6060 6070 6080 6090 6100 7000 7010 7020 7030
7031 7040 7050 7060 7070 7071 7080 7081 7090 7100 7110 7120 7130 7140 7150 7160
7170 7180 7181 7190 7191 7200 7210 7220 7221 7230 7231 7240 7250 8000 8001 8010
8020 8030 8040 8050 9000 9010 9020 9030 9040 9050 9060 9070 9080 9090 9091 9100
9110 9120 9130 9140 9150 9160 9170 9180 9190 9200 9210 9220 9230 9240 9250 9260
9270 9280 9290 9300 9301 9302 9310 9320 9321 9340 9350 9360 9370
```

Build commands, validation results, acceptance recipes and complete door/teleport tables
follow below.

## Build and import

From this clone's root (Python 3 standard library only):

```sh
export RECVX_ISO=/path/to/SLUS-20184.iso
python3 pc/tools/build_area.py
python3 -m unittest discover -s pc/tools -p 'test_*.py' -v
python3 pc/tools/rdxbuild.py roundtrip --all
```

The one builder reads `pc/tools/area/annex.json`, generates original PNG/OBJ/JSON source
under `mods/quarantine-source/`, scans all 205 retail scripts, imports enemy dependencies,
validates and compresses every room, then publishes:

- `mods/RM_1160.RDX` through `mods/RM_1210.RDX` (six rooms, increments of 10).
- `mods/RM_1110.RDX`, `RM_1020.RDX`, `RM_1021.RDX` (retail integration).
- Six matching `.SPQ.key` text files; RM_1210 requests bank 3020, the others bank 60.
- `mods/quarantine-validation.json`: complete census, resource measurements, reachable
  interaction coordinates, patch counts. Generated `scripts.txt` files show the bytecode.

Everything in mods/ is ignored, including generated sources, which keeps enemy/disc bytes
out of Git. `python3 pc/tools/build_area.py --source-only` generates the original assets
without the ISO. Edit the source JSON for placement/links, the generator for geometry,
materials or scripts. All validation precedes publication; each output replacement is atomic.
The build replaces these specific mod filenames. It neither edits the ISO nor other clones.
To uninstall, remove these nine RDXs and six `.SPQ.key` files from mods/.

New compiler support: arbitrary enemy donor records, shared texture-owner discovery,
one motion set per type, distinct type texture uploads, conflicting donor motion/texture
rejection, helper-slot/variant/model-version checks and conservative child-slot budgets.
Material UVs in the area generator use world scale; signs retain a single legible image.
The stdlib validator is `pc/tools/area_validate.py`.

The token is an original brass key mesh but uses inventory ID **72, Key with Tag**. The
pickup message/inventory art/name therefore remain the retail item's; no new inventory
localization is claimed. **Only this pickup's it_flg 464 opens Supply**; already owning a
retail Key with Tag cannot open it. The frame and entry scripts disable Isolation trigger
1 then re-enable it if 464 is set (IfelCk/Ck/EtcAtariSet). No key is consumed and no retail
key flag is written. The TOKEN / RECORDS sign is the locked-door cue; there is no text dialog
when the locked trigger is disabled. All twelve pickups have original display meshes.

### Retail integration and case handling

The existing piano door (1110 trigger 0) now targets 116 entry 0. Its record and four
script target literals change; its one RoomCaseNo 1 becomes 0 for the annex. All its
other conditions and assets remain intact. In each hall case, trigger 9 starts as examine
and later becomes a door: patch its two (1020) / four (1021) **script** targets from 111
entry 0 to 116 entry 1, leaving story locks in place. The records that are initially examine
remain so. Exactly 11/4/8 expanded bytes change in 1110/1020/1021 respectively.

Reception's hall return is an examine-trigger script using the same cb_flg 23 interaction
pattern as retail. It repeats RM_1110's own hall exit: case 0 and door attr 4 while
ev_flg 151 is clear; once set, case 1 with attr 4, 0x24 or 0x64 as ev_flg 181/142 select
(RoomCaseNo and RoomSoundCase), then SetDoorCall to 102 entry 3, and clears the interaction
bit. Thus the detour returns to the retail hall case the piano exit would select. Other annex entry scripts set room and sound
case to 0. No case-1 annex file or C hook is required. Validate both story states in game;
teleporting a late-game character into early rooms is not equivalent to normal progression.

## Offline validation results and limits

- All **205** retail rooms passed structural re-layout, semantic comparison, raw ordinary
  record equality, model/motion/texture preservation and script-byte preservation.
- **14** disc-free tests passed: the original six plus eight covering non-owner/nonzero
  donor records, multiple enemy compilation and deduplication (and `wrk_no`), unsafe mixes,
  all six room layouts, graph/flag/camera failures, token lock/re-entry behavior, the hall
  return's case/door attributes and deterministic materials.
- All nine emitted RDXs were compressed then expanded and compared byte-for-byte.
- Census: 369 referenced retail pickup flags (max 451), 321 death flags (max 357), including
  generic Ck/Set and DieEventCk ranges. Twelve new pickup and eight death flags are unique,
  below 512, and disjoint from retail and the Service Corridor's allocations.
- Every new room passed closed visual/collision shell checks, floor/entrance/actor placement,
  navigation connectivity on a 1-unit grid with the clearances measured in game (3.5 units
  from low boxes, 5.5 from boxes taller than 12 and the shell), reachable 4.5-unit
  interaction probes, route/camera-area coverage, cameras inside the shell with no hidden
  parts and the ceiling out of view, the player's head (16.5) never behind a prop, and
  conservative 4:3 framing of feet and a 17.5-unit head in **both** potentially active cuts
  at overlap samples. Door targets are reciprocal;
  the key is reachable before its lock, the locked reward has no bypass, and its return is open.
- Per-room resource measurements below include donor enemies; 4 MiB/64 texture entries are
  reserved for persistent assets. Packed+expanded data plus a 3 MiB runtime allowance must
  fit the 12.25 MiB arena. These are conservative offline estimates, not measured live usage.

| Room | Mesh triangles / nodes | Walkable samples | TIM2 count | Image+CLUT bytes | Enemy slot allowance |
|---|---:|---:|---:|---:|---:|
| 1160 | 1,384 / 43 | 3,075 | 11 | 160,768 | 0 |
| 1170 | 1,864 / 32 | 3,409 | 22 | 634,880 | 22 |
| 1180 | 1,600 / 52 | 3,036 | 22 | 634,880 | 22 |
| 1190 | 1,840 / 42 | 4,597 | 28 | 1,034,240 | 15 |
| 1200 | 1,792 / 54 | 2,411 | 11 | 160,768 | 0 |
| 1210 | 1,352 / 31 | 4,377 | 17 | 560,128 | 8 |

Geometry consists of original box assemblies, not production art; room-specific lights,
bespoke animated enemies, unique inventory text and new map-screen assets are outside this
build. Bank 60 cannot guarantee correct Cerberus sounds in the mixed Isolation encounter.
No room-specific effect lists are imported. The map screen has no new floor/tag entries and
may be unsafe here; it was not opened in the acceptance runs. No multiplayer or alternative
character acceptance is claimed.

## In-game acceptance results (2026-10-05)

Headless runs of `pc/tools/room_test.sh` on branch `codex-area` (32-bit RelWithDebInfo build,
offscreen video, dummy audio, the user's ISO, own save directories), new game, case 0,
normal player model, collision **on**. Common environment:

```sh
export RECVX_ISO=/path/to/SLUS-20184.iso RECVX_MODS="$PWD/mods"
export RECVX_NOCLIP=1:0 RECVX_INF_HEALTH=1 RECVX_TRACE_SND=1   # position log every 15 game frames
# combat: RECVX_EQUIP=20:5:60 (handgun) RECVX_INF_AMMO=1, MORE_INPUT R1 held + R1+Cross every 20-24 frames
```

Positions come from the `RECVX_NOCLIP` log; kill flags were read with a temporary,
uncommitted print of `ed_flg` 460..467 and each record's `ene[wrk_no]` `flg`/`stflg`/hp.
Inputs are presented frames, `RECVX_WARP` uses room-local game frames, so the runs repeat
Cross every 40 frames and let warps place the player. The log loses its buffered tail when
`timeout` kills the game; the frame dumps are complete. Evidence lives in the ignored
`scratch/area/` (paths below are relative to it). Per-room contact sheets:
`scratch/area/sheets/RM_11x0-room.png` (fight, revisit and the collision/camera walk) and
`RM_11x0-play.png` (the one-run playthrough), plus `RM_1110-play.png`.

### Bugs found in game and fixed at the root

| Symptom | Cause | Fix |
|---|---|---|
| Second zombie and both dogs came back after a revisit | every authored enemy record had `wrk_no` 0, so `EneSetCk`/`DieCk` (`ene[enep[rec].wrk_no]`) tested and hid only the first enemy; all 759 retail records hold their own index | `rdxbuild.validate_authored` rejects `wrk_no != index`; the generator sets it |
| Floors and walls smeared into stripes | world-scaled UVs up to ~3; the game clamps room textures (all retail room UVs are 0..1, tiny-chunk flags 0x34) | `rdxbuild` rejects UVs outside 0..1; `build_area` tiles materials by splitting faces |
| Black void at the bottom and corners of every first shot | cameras at y=65 behind the hidden south wall, above the ceiling | each half is shot from inside the room under the ceiling at the opposite end wall; no hide masks |
| A wedge of background at the top of every shot | the game drops large ceiling triangles passing over the camera | pitch 33° keeps the ceiling out of view; validator checks it |
| Player hidden in corner pockets behind tall shelves/cages/lockers | pockets 3–9 units deep at the end walls | those props now stand against the end walls; validator checks head visibility |
| Clearance differs from the validator's 3.5 | measured stops: 3.5 from low boxes (≤10 high), 5.5 from tall ones and the shell | validator uses the measured clearances; herbs in 1160/1170 moved off the shelves |

Found by reading RM_1110's script, not in a run: Reception's hall return always used door
attr 4. It now repeats the piano room's own exit (attr 4/0x24/0x64 by ev_flg 181/142 once
151 is set, plus RoomSoundCase); a disc-free test interprets the four flag combinations.

### Per room

Final build (`cdb550e`). "Doors" = both directions, animation 22, correct file and landing
(door table below). "Collision" = walking into every shell wall and each prop from the side
and both ends; stops listed are the logged positions. "Stable" = one visit of more than 60 s
of game time without errors or a crash.

| Room | Teleport | Doors | Cameras | Collision | Pickups | Enemies | Sound key | Stable | Result |
|---|---|---|---|---|---|---|---|---|---|
| 1160 Reception | pass | S, W, N, E pass | 2 cuts, seam both ways, player always in view | shell 5.5; desk/benches 3.5; shelf 5.5 | ammo + green herb taken, absent on revisit | none | 60 | 143 s | **pass** |
| 1170 Decontamination | pass | S, N pass | pass | shell 5.5; basins 3.5; lockers 5.5; drain does not block | ammo + herb taken, absent on revisit | 2 zombies walk, grab, die; ed 460/461, hidden on revisit | 60 | 129 s (second visit) | **pass** |
| 1180 Records | pass | S, E pass | pass | shell 5.5; shelves 5.5; desks 3.5 | Key with Tag (464) + herb taken | 2 zombies walk, grab, die; ed 462/463, hidden on revisit | 60 | 144 s | **pass** |
| 1190 Isolation | pass | W, N pass; E refused without 464, opens with it, again after re-entry | pass | shell 5.5; cages 5.5; beds 3.5 | ammo + herb taken | zombie dies, 464 hidden on revisit; dog moves, attacks, takes damage (120 to 15), died in `combat-19` (465 set, hidden on revisit) | 60 | 129 s | **pass** |
| 1200 Supply | pass | W pass, also without the token | pass | shell 5.5; shelves 5.5; crate 3.5 | ammo + red herb taken | none | 60 | 129 s | **pass** |
| 1210 Dispatch | pass | S, W pass | pass | shell 5.5; crates 5.5; trolleys 3.5 (logs disturbed by live dogs) | ammo taken; herb with a full inventory: "Use it now?" Yes, gone | 2 dogs active at once (variant 0), attack, die; ed 466/467, hidden on revisit (`combat-21`) | 3020 | 145 s | **pass** |

Every visit logged `mods: room sound key 11x0 uses 0060` (`3020` for Dispatch) followed by
the bank upload (22 / 30 `2c` transfers); nothing was listened to. Textures read as tile,
concrete, metal, wood, rust and hazard stripes; there are no transparency holes; the signs
are legible. Evidence: `room-11x0/` (walk legs in `legs.txt`, stops via the NOCLIP log),
`combat-17/18/19/21/` (kills, revisit; build with the `wrk_no` fix, before the camera pitch
change, which touches no game data), `pickups/` (first build, all twelve pickups and a
revisit), `token/` (lock), `chain/` (every open door, first build; door data unchanged since).

### Normal-play entry and retail rooms

- RM_1110's piano exit loads Reception at entry 0 and Reception's south door returns to the
  piano room (`chain/`, `playthrough/`).
- Reception's west door returns to the hall, RM_1020 entry 3 (upper landing, floor 3).
- The hall's trigger 9 becomes a door only with ev_flg 147 (piano puzzle). `hall/` uses a
  test-only copy of the mods (`scratch/area/mods-hall`) whose scd0 test of 147 is inverted:
  the door then loads Reception at entry 1 and the west door brings the player back.
  RM_1021 (hall case 1) needs story state and was not run; its patch is the same retarget.
- With identical input (teleport, walk, no door use) the rebuilt RM_1110 and RM_1020 render
  pixel-identically to the disc's: 66 and 67 dumped frames, 0 differ (`van1110/` vs
  `mod1110/`, `van1020/` vs `mod1020/`).

### One-run playthrough

`playthrough/`: new game → Reception (both pickups) → Decontamination (2 zombies killed,
both pickups) → Records (2 zombies killed, token and herb) → Isolation (zombie killed, both
pickups; the dog took 105 damage but survived) → token door → Supply (both pickups) → back to
Isolation → Dispatch (both pickups; dogs damaged, alive) → Reception → piano room →
Reception → Decontamination revisit (zombies and both pickups stay gone) → Reception. 6,330
game frames (211 s) through 12 door transitions without an error; `ed_flg` 460..464 set.

### Still open

- Save/reload of the twelve pickup flags, eight kill flags and the unlock (not run).
- The map screen (never opened; likely unsafe without floor/tag data).
- The full-inventory case at the token (a full inventory at the Dispatch herb took the game's
  normal "Use it now?" path), audible sound (dummy driver), Cerberus sounds from
  bank 60 in Isolation.
- The hall's piano door by real story progression and RM_1021; late-game ev_flg 151/181/142
  return paths (disc-free test only).
- The lock is silent: with the token missing the trigger is off, the TOKEN sign is the only
  cue (annex rooms have no message data).
- A corpse right in front of a camera fills much of the shot (the end-wall cameras are close
  to the doors); a cosmetic follow-up would move the cameras or the fight positions.

### Re-running

The scripts used live in the ignored `scratch/area/` (`chain.sh`, `pickups.sh`, `token.sh`,
`hall.sh`, `roomrun.sh`, `play.sh`, `retail.sh`); they only combine the environment above
with `RECVX_WARP` lists built from the door table, the pickup witnesses in
`mods/quarantine-validation.json` (`trigger_witnesses`) and, for walks, one warp per shell
wall and prop face with `up` held. Minimal examples:

```sh
# token lock: refused 400 frames, token in Records, door opens, back, opens again
crosses=$(python3 -c "print(','.join('%d:cross,%d:-'%(f,f+4) for f in range(500,6000,40)))")
RECVX_WARP=1:19:20:64:50:49152,1:18:20:14:68:0,1:19:20:64:50:49152,1:20:60:8:40:16384,1:19:60:64:50:49152 \
MORE_INPUT=$crosses pc/tools/room_test.sh scratch/area/token '1:19:0,400:1:18:0,400:1:19:0' 1200
# fight in Decontamination, leave, return: kill flags hide both zombies
shots=$(python3 -c "print(','.join('%d:r1+cross,%d:r1'%(f,f+5) for f in range(600,2000,20)))")
RECVX_EQUIP=20:5:60 RECVX_INF_AMMO=1 RECVX_WARP=1:17:10:32:14:32768 MORE_INPUT="560:r1,$shots,2000:-" \
  pc/tools/room_test.sh scratch/area/combat-17 '1:17:0,1500:1:16:0,150:1:17:0' 1100
```

Pickup centres in game coordinates (Y=1) and flags; approach from the `trigger_witnesses`:

| Room | First pickup x,z / id / it_flg | Second pickup x,z / id / it_flg | ed_flg in record order |
|---|---|---|---|
| 1160 | 12,28 / 12 / 460 | 50,58 / 21 / 461 | — |
| 1170 | 12,26 / 12 / 462 | 40,72 / 21 / 463 | 460,461 |
| 1180 | 14,64 / 72 / 464 | 52,25 / 21 / 465 | 462,463 |
| 1190 | 16,24 / 12 / 466 | 56,77 / 21 / 467 | 464,465 |
| 1200 | 14,26 / 12 / 468 | 42,61 / 22 / 469 | — |
| 1210 | 16,25 / 12 / 470 | 56,77 / 21 / 471 | 466,467 |

## Complete room IDs and teleport values

All new files use **stage 1, case 0**. F2 fields are **Stage / Room / Entry**, in decimal;
F2/RECVX_TELEPORT do not explicitly set a case, so use a new-game/case-0 context for direct
teleport acceptance. The supported retail entry scripts normalize the annex case.
WARP alone only repositions a player already in that stage/room. All six teleports and
entry-0 landings below were verified in game (`mods: .../RM_11x0.RDX`, positions logged).

| File | Stage | Room | Case | Name and contents | RECVX_TELEPORT | F2 stage/room/entry | RECVX_WARP (entry 0 landing) |
|---|---:|---:|---:|---|---|---|---|
| RM_1160.RDX | 1 | 16 | 0 | Reception: counter, benches, ammo, green herb; safe | `1:1:16:0` | `1 / 16 / 0` | `1:16:20:32:11:32768` |
| RM_1170.RDX | 1 | 17 | 0 | Decontamination: basins, lockers, ammo, green herb, 2 zombies | `1:1:17:0` | `1 / 17 / 0` | `1:17:20:28:11:32768` |
| RM_1180.RDX | 1 | 18 | 0 | Records Office: desks, shelves, token key, green herb, 2 zombies | `1:1:18:0` | `1 / 18 / 0` | `1:18:20:32:11:32768` |
| RM_1190.RDX | 1 | 19 | 0 | Isolation Gallery: cages, beds, ammo, green herb, zombie + dog, locked supply door | `1:1:19:0` | `1 / 19 / 0` | `1:19:20:11:50:49152` |
| RM_1200.RDX | 1 | 20 | 0 | Emergency Supply: shelves, pallet, ammo, red herb; safe reward | `1:1:20:0` | `1 / 20 / 0` | `1:20:20:11:40:49152` |
| RM_1210.RDX | 1 | 21 | 0 | Dispatch Bay: crates, trolley, ammo, green herb, 2 dogs | `1:1:21:0` | `1 / 21 / 0` | `1:21:20:36:11:32768` |

## Complete directed door table

Local trigger index equals local arrival entry for the new rooms. This makes each return
landing explicit. S/N/W/E refer to local -Z/+Z/-X/+X walls; no global coordinate continuity
is implied across a loading door. Every edge uses animation type 22.

| From file / trigger (wall) | To stage:room / entry | Door test RECVX_WARP | Condition | Verified landing x, (y,) z |
|---|---|---|---|---|
| RM_1110 / 0 | 1:16 / 0 | `1:11:20:65:7:49152` | Original piano exit logic, normalized annex case | 32, 11 |
| RM_1020 or RM_1021 / 9 | 1:16 / 1 | `1:2:20:-8:7:16384` | Original piano-door unlock (ev_flg 147); warp only after using entry 3 (Y=40) | 11, 40 (RM_1020, flag-inverted test copy) |
| RM_1160 / 0 (S) | 1:11 / 0 | `1:16:20:32:8:0` | Open | 66, 7.5 |
| RM_1160 / 1 (W) | 1:2 / 3 | `1:16:20:8:40:16384` | Open; restores the hall case and door attr of RM_1110's exit | -6.5, 40, 7.4 (floor 3) |
| RM_1160 / 2 (N) | 1:17 / 0 | `1:16:20:32:72:32768` | Open | 28, 11 |
| RM_1160 / 3 (E) | 1:21 / 1 | `1:16:20:56:40:49152` | Open | 11, 50 |
| RM_1170 / 0 (S) | 1:16 / 2 | `1:17:20:28:8:0` | Open | 32, 69 |
| RM_1170 / 1 (N) | 1:18 / 0 | `1:17:20:28:92:32768` | Open | 32, 11 |
| RM_1180 / 0 (S) | 1:17 / 1 | `1:18:20:32:8:0` | Open | 28, 89 |
| RM_1180 / 1 (E) | 1:19 / 0 | `1:18:20:56:40:49152` | Open | 11, 50 |
| RM_1190 / 0 (W) | 1:18 / 1 | `1:19:20:8:50:16384` | Open | 53, 40 |
| RM_1190 / 1 (E) | 1:20 / 0 | `1:19:20:64:50:49152` | Pickup flag 464 required | 11, 40 (refused for 390 frames without it) |
| RM_1190 / 2 (N) | 1:21 / 0 | `1:19:20:36:92:32768` | Open | 36, 11 |
| RM_1200 / 0 (W) | 1:19 / 1 | `1:20:20:8:40:16384` | Always open, even without token | 61, 50 |
| RM_1210 / 0 (S) | 1:19 / 2 | `1:21:20:36:8:0` | Open | 36, 89 |
| RM_1210 / 1 (W) | 1:16 / 3 | `1:21:20:8:50:16384` | Open | 53, 40 |

All landings were read from `RECVX_NOCLIP` position logs (`scratch/area/chain/`, `token/`,
`hall/`); every door played animation 22 and loaded the listed file. RM_1021 (hall case 1)
was not run: it needs the story state; its patch is byte-for-byte the same retarget.
