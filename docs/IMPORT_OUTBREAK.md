# Importing from Resident Evil Outbreak File #2

**Private work (local branch `outbreak-import`), not for the public repository yet.** Nothing from
any disc is committed: the importer reads the user's own image in place and writes what it
extracts to the user's mods folder (`mods/import/outbreak2/`, or `import_dir`).

Goal: take the character models of *Resident Evil Outbreak File #2* (US, SLUS-20984) and use them
as player models in this port ("You appear as", [ACTORS.md](ACTORS.md#you-appear-as)); later
voices and the social-call ("ad-lib") animations for co-op. The F4 window runs the extraction and
lists what was extracted; it is built so other games can be added as further sources.

## PROGRESS

| Stage | State |
|---|---|
| 1. Format research | done: disc layout, archives, compression, model and texture format ("Disc format") |
| 2. Extractor (C, `pc/src/import/`) | done: all 30 player models (8 characters, every costume) to glTF + PNG; NPCs and enemies optional (glTF) |
| 3. Conversion to CVX player models | done for the playable characters: `.cvxm`, listed in "You appear as", skinned every frame, retargeted from the player's motions; checked in game (walk, aim, grabs, third person) |
| 4. F4 window | done: disc image, what to extract, progress, results table from the manifest (also after a restart) |
| 5. Notes for later (voices, ad-libs, animations) | first findings ("Notes for later") |
| 6. Verification | headless extraction, glTF/`.cvxm` checks and previews, in-game shots, ctest (`import`, `appear`), Linux and Windows builds, layout check |
| 7. Posture (round 2) | fixed: the retargeting's rest correction bent the imported meshes (hunched shoulders, head down); imported models now take the source's rotations as they are; the left arm's two-handed grip scaled to the model's shoulders, the palm onto the gun ("Posture") |
| 8. Faces (round 2) | blinking (lids, the player characters' timing) and the jaw from the cutscene double's lip sync ("Faces"); the jaw path is checked with a forced angle, not yet in a talking cutscene |

Resume notes: build with `cmake --build build`; headless run:
`RECVX_MODS=<out> RECVX_IMPORT=outbreak2 RECVX_IMPORT_ISO=<image> build/pc/recvx --save-dir <tmp>`
(`RECVX_IMPORT_WHAT` = 1 players, 2 NPCs, 4 enemies, summed; exit status 0 = no failures).
Check the output with `pc/tools/import_check.py <model>.glb|.cvxm out.png`. In game:
`RECVX_MODS=<out> RECVX_APPEAR=30:68` shows the first imported model (chr `COOP_CHR_IMPORT` = 68).
Test aids: `RECVX_IMPORT_POSE=file` (per pose of slot 0 the node matrices in the base's frame;
the double's jaw angle is logged), `RECVX_IMPORT_JAW=deg` (the jaw held open). Posture and face
previews were rendered offline from such dumps (exploration scripts outside the repository).

### Next steps

1. Faces: check the jaw in a talking cutscene (a room whose cutscene starts on entry with the
   story flags set: `pc/tools/enemies.py room` lists the doubles' `FaceRep`/lip ops); eye
   direction (Outbreak's eye mesh scrolls its UVs) and expressions are not done.
2. Fingers are merged into the hands (OB's fist / open-hand poses are lost); the weapon grip
   could close them.
3. The high-detail models (`HP*.nbd`: 512x512 textures, a separate head model with face bones)
   for cutscenes.
4. Co-op: the partner sees the played character instead of an imported model (the pose's `chr`
   has no room for it and the partner may not have the file); a "model hash" in the pose and a
   fallback would allow it.
5. NPCs as player models: the 147 models with the human skeleton (P, EP, most N) convert the
   same way; only the players are converted now (names of the NPCs are not known).
6. Voices and ad-lib motions (below).

## Disc format

ISO 9660, 4.7 GB. Root files:

| File | Content |
|---|---|
| `SLUS_209.84` | the executable |
| `NETBIO00.DAT` (1.0 GB) | AFS: `romdata_usa.afs` (common data, 1087 files) and per scenario `rNNN.afs` + `rNNN_h.bin` (r006, r010, r015, r020..r030, r040, rfff) |
| `NETBIO01.DAT` (1.4 GB) | AFS: `adxsnd_usa.afs` (1853 CRI ADX streams), `bgm.afs` (96 ADX music tracks, named), `pl00_vo.afs` (997 ADX voice lines, 680 of them `dmy.adx` placeholders) |
| `BIN/01.DAT` .. `05.DAT` | AFS of CRI Sofdec movies (`.sfd`) |
| `BIN/0.DAT` .. `9.DAT` | `MWo3` overlays (program modules) |
| `0FLIST.DIR` | list of the big archives |

**AFS**: `"AFS\0"`, file count n, n x (offset, size), then (offset, size) of a name table of n
48-byte records (32-byte name, date, size). Offsets are relative to the archive; archives nest.

**romdata_usa.afs** (names as on the disc; NBD = model):

| Files | What |
|---|---|
| `P<cc>_<vv>.nbd` | the 8 playable characters (cc 00..07: Kevin Ryman, Mark Wilkins, Jim Chapman, George Hamilton, David King, Alyssa Ashcroft, Yoko Suzuki, Cindy Lennox), costumes vv 00..02 (Alyssa, Yoko, Cindy also 03, 04): 30 files |
| `EP<cc>_<vv>.nbd` | the same characters' event (cutscene) models |
| `HP<cc>.nbd` | high-detail player models (512x512 textures, a second model pair: the head) |
| `N*.NBD`, `HN*.NBD` | NPCs (normal / high detail); `*_N.NBD` parts |
| `E<tt>_<vv>.NBD`, `HE*.NBD` | enemies; `EF01.NBD` effects |
| `pl<cc>_pc_000.bin`, `pc_<nnn>.bin` | motion data (see "Animations") |
| `e<tt>_<nnn>.bin`, `ep<tt>_tbl.bin`, `rn<nn>_tbl.bin` | enemy motions, enemy-player tables (grabs), room tables |
| `pl<cc>_voi.snd`, `pl<cc>_npc.snd` | per character sound banks (see "Voices") |
| `*.snd`, `*.snp` | other sound banks (common, lobby, weapons, enemies) |
| `*.MAT`, `*.mat`, `SHADOW_*.SDT`, `*.tm2` | material tables, shadows, 2D textures |

The scenario archives (`rNNN.afs`) hold the rooms (`r*.nbd`, `r*.rdt`, `*.sld` textures),
objects (`OBJ*.NBD`), enemy sets (`e*.bin`), and per character (A..H) `dm*<c>.Bin` files.

### NBD (model)

Little endian. A table of 16-byte entries at 0 (`type[4], offset, size, count`), as long as the
first entry's offset (0x40 or 0x80):

| Entry | Content |
|---|---|
| `TEX` | `count` textures: u32 size + SLD-compressed TIM2 |
| `AMO` / `AHI` | the model and its node hierarchy (an optional second pair: an add-on model, e.g. the HP heads) |
| `SDW` | shadow data |
| `TID` | (room models) texture ids instead of `TEX` |

**SLD** (texture compression): 16-bit words; a flag word gives the kind of the next 16 items
(highest bit first): 0 = a literal word, 1 = a copy word `c`: count `c >> 11` (0: the count is the
next word), distance back `c & 0x7ff` words (0: that many zero words); count 0 and distance 0 ends.

**Chunks** (AMO and AHI): `u32 type, u32 count, u32 size` (size includes the 12-byte header).

* AMO `1` model: `0x20000` date, `2` mesh list, `9` material list, `0xA` texture list.
* Mesh `4`: `5` strip lists (up to 2: `0x30000` single-weight vertices, `0x40000` blended; each
  strip `u32 n` (bit 31 a flag) + n u32 indices), `0x50000` material refs, `0x60000` material per
  strip (index into the refs, over both lists in order), `0x70000` positions (float3),
  `0x80000` normals, `0xA0000` UVs (float2; values above 1 repeat the texture), `0xB0000`
  colours (float4, 0..255), `0xC0000` weights (per vertex `u32 k` + k x (joint, weight %)),
  `0x100000` joints (node ids the weights index), `0xF0000` render attributes.
* Material (`0x120000`..`0x150000`: constant, Lambert, Phong, vertex colours): ambient, diffuse,
  specular (float4 each), specular decay, texture count, 200 bytes, texture indices into the
  texture list. Texture list entry (`0`): id = TEX slot, width, height, tiling.
* AHI `0xC0000000`: a root list (`0x80000000`: root ids), then per node a `0x40000001` (node),
  `0x40000002` (mesh node) or `0x40000003` (joint) chunk: id, parent, child, next, scale[4],
  rotation[4] (Euler x, y, z in radians, w unused; stored negated for joints), translation[4],
  mesh id, group.
* Positions of skinned meshes are in model space, in the bind pose: a T-pose with almost all
  node rotations 0. Units are centimetre-like (Kevin is 173 high); y up, the character faces +z.

**TIM2** textures: standard PS2 TIM2, 8-bit indexed 256x256 with a 32-bit CLUT (CSM1: entries
8..15 and 16..23 of every 32 swapped), alpha 0x80 = opaque. Both header alignments occur (format
id 0: 16-byte file header; 1: 128 bytes, e.g. most third costumes). The GS registers in the
picture header are the same as Code: Veronica's for the same size and depth.

**The human skeleton** (147 models share it: all P, EP, most N): 36 nodes (Cindy's main costume
43, an extra hair chain). 0 root, 1 (pivot), 2 hips (y 110), 3 hip pivot, 4..6 and 7..9 legs
(thigh, knee, ankle), 10 spine, 11 chest, 12..15 and 16..19 arms (clavicle, shoulder, elbow,
wrist), 20 neck, 21 head, 22 face (23..26 eyes/lids, 27 mouth), 28..31 and 32..35 fingers.
Groups: 0 lower body, 1 upper body, 2 face, 3/4 hands. A player model has 3 or 4 meshes (body,
eyes, hands, head) and 2 textures (head and body), 1900..3600 vertices.

Sources: format names and chunk ids follow the public notes of the Outbreak modding community,
chiefly Fothsid's [libthanatos](https://github.com/Fothsid/libthanatos) and
[OBTool](https://github.com/Fothsid/OBTool) (both GPL-3.0: read as documentation, **no code
copied**; ours is MIT and written from the data), the Xentax/zenhax and residentevil123 forum
threads on NBD textures, and the [Outbreak decomp](https://github.com/GalaxyNostalgia/Outbreak-decomp)
(not used yet: the best source for the motion and sound formats).

## Extraction (`pc/src/import/`)

| File | Role |
|---|---|
| `import.h`, `import.c` | the sources table, extraction thread and progress, manifest, the registry for "You appear as" |
| `ob2.c` | Outbreak File #2: ISO 9660 root lookup, AFS, SLD, NBD reading into an `ImpModel` |
| `impmodel.h`, `impmodel.c` | glTF 2.0 binary writer (skeleton, skin, PNG textures), TIM2 decoding, the `.cvxm` converter |

Output per model in `<dir>/outbreak2/<id>/`: `<id>.glb` (viewable in Blender: one skinned mesh, a
primitive per texture, the full skeleton, scale 0.01 to metres, textures embedded),
`<id>_tex<k>.png`, and for the playable characters `<id>.cvxm`. `<dir>/outbreak2/manifest.txt`
lists every model (counts, whether it is a player model, failures with their reason); F4 shows
it, also after a restart, and the registry reads it at start. The disc is identified by
`SYSTEM.CNF` (`SLUS_209.84`); another region's disc is refused. All 30 player models: 23 MB of
output, a few seconds.

Settings (recvx.ini): `import_outbreak2_iso` (the disc image), `import_dir` (default
`<mods>/import`, `RECVX_MODS` respected). Environment: `RECVX_IMPORT=outbreak2` (headless, exits),
`RECVX_IMPORT_ISO`, `RECVX_IMPORT_WHAT`, `RECVX_SHOW_OVERLAY=4` (F4 open at start).

## Conversion

A player model of this port is a Ninja chunk model whose skeleton the "You appear as" retargeting
can map ([ACTORS.md](ACTORS.md#retargeting)). `imp_write_cvxm` (impmodel.c) builds one:

* **Skeleton**: the nodes of the lower and upper body groups that bend (offset not zero) are
  kept: root, hips (node 1, the pelvis), spine, chest, neck, head, both arms (clavicle, shoulder,
  elbow, wrist), both legs (hip, knee, ankle) = 20 nodes, plus the five face bones (four lids, the
  jaw; "Faces") = 25, in pre-order with children by index. Fingers, the face root and the
  zero-length pivots go to their nearest kept ancestor (weights included).
  All bind angles are 0 (a T-pose), as `rig_build` wants; the model is turned 180 degrees about y
  (Outbreak faces +z, Code: Veronica -z: a rotation, no mirroring) and scaled by 0.1 (the hips at
  11 units, as Claire's). `rig_build` finds the full map (pelvis, spine, chest, neck, head, 2 x 4
  arm nodes, 2 x 3 leg nodes; no toes).
* **Mesh**: all of the model's meshes as one mesh on the pelvis: a vertex chunk 51 (`CvVnPs2`:
  64-byte header, position and normal per vertex, w = 1) and per texture a material chunk, a
  texture chunk and strip chunks 66 (`CsUvh`: index, u, v with 1023 = 1.0; double-sided). UVs
  above 1 are moved back by whole tiles per strip (Code: Veronica clamps u). Strips that show
  see-through texels (fringes, lashes) are drawn last and alpha-blended, else they hide the face.
* **Skin**: Code: Veronica's own skin (`SKIN`, `npCalcSkin`) blends a vertex between a node and
  its parent's rotation only; Outbreak's are real weights (up to 3 joints). So the `.cvxm` carries
  per vertex the bind position, normal and 4 (node, weight), and `actor_glue.c imp_skin` does
  linear blend skinning after every pose (`actor_pose`), into the pelvis's frame (up to 3600
  vertices x 4 weights per pose).
* **Textures**: the TIM2 pictures as the game's texture chunks (32-byte chunk header, the 0x80
  extended file header with `Gindex` = `0xF00000 + 16 * disc index + k`, picture format 3: loaded
  without a palette list); the chunks follow each other without padding.
* **In game** (`actor_glue.c load_import_model`): the file is read into the display slot's buffer,
  realized like a room model, its textures uploaded (refused with too little texture memory, as
  the catalogue's). Actor index `ACTOR_MODELS + k`, chr `COOP_CHR_IMPORT + k`; listed after the
  cutscene models; women move with Claire's motions, men with Chris's (gender-matched animations),
  and speak with Claire's / Chris's voice.

Checked in game: walking, running, the handgun aimed and fired (the gun in the model's hand),
zombie grabs, third person; front and back views (`RECVX_TPS_ORBIT`).

Limits: no finger poses or expressions; costume pieces that hang (Cindy's ponytail chain,
coats) follow their body node rigidly; skirts tear open a little in long strides (Outbreak's own
weights put the skirt on the legs); the feet sink up to ~0.5 units (5 cm) into the floor at the
toe-off of the walk (the model has no toe bone; Claire's foot pitch is copied to its ankle); the
co-op partner sees the played character.

### Posture

The first version looked hunched: shoulders pulled up, the head bent forward and down, sleeves
crushed, arms tucked in. Cause: the retargeting (`actor_glue.c retarget`) turns every mapped bone
by a rest correction, the rotation taking the model's rest bone onto the source's (the player's),
so that a catalogue model whose arms rest lower or more forward bends at the same joints. An
imported model is skinned to its own T-pose, which is the player's rest pose too, and its bones
are placed differently: Claire's clavicle bone runs from the sternum up to the shoulder joint,
Outbreak's starts at the shoulder and runs sideways, so the correction turned Outbreak's whole
shoulder region up by ~60 degrees; Claire's neck-to-head bone leans, Outbreak's is upright, so the
head went forward. The skinned mesh follows these rotations even at rest. Fix: an imported model
(`d->skin`) takes the source's world rotations unchanged (the delta from the common T-pose);
positions still come from its own bone offsets, the root height and feet adjustment and the
two-hand IK as before. Checked for all 30 models in idle, walk, run, aim and fire, front and side
(offline renders of the dumped poses), and in game.

**The left arm** (second report: broken and clipping in every motion set; in the two-handed aim
the left hand reached past the gun to the right). Cause: the two-handed grip (`retarget`,
`arm_ik`) solves the left arm to "the model's right hand + the source's left-to-right hand
offset", weighted fully below 3 units apart and faded out at 6. The source's hands hang 3.5 apart
with the arms down, so the solve also runs in idle and walk; Outbreak's shoulders are about a
quarter wider than Claire's/Chris's (shoulder joints 3.5 apart against 2.8), so the unscaled offset
pulled only the left hand in across the hips. In the aim the left wrist was put on the right
wrist, and Outbreak's hand has straight fingers (Code: Veronica's grip pieces from the weapon file
curl around the gun), so the left fingers stuck out past the gun. Fix (imported models only): the
offset is scaled by the ratio of the two shoulder spans, and in a grip (hands up to 2 apart, fading
out at 4) the left wrist goes back along its forearm by half the model's hand length (measured at
load from the hand's vertices), which puts the palm under the right hand. Idle and walking now hang
both arms alike; the two-handed aim cups the right hand. The right hand and the gun are unchanged
(the shot still comes from the game's own skeleton). Dual weapons hold the hands further apart and
keep the scaled offset only.

### Faces

Outbreak's player models move their faces with five bones under the head (group 2): upper and
lower lid per eye (23..26, a centimetre apart, the lid vertices weighted 25..100% to them) and
the jaw (27, at the ear, the chin and lower lip weighted to it; the mouth has teeth and an
inside). The eyes are a separate small mesh behind the lids. The converter keeps these five bones
(roles found from the positions: the lowest is the jaw, per side the higher lid is the upper one)
and lists them in the `.cvxm` header (version 2; version 1 files still load, without a face).

Code: Veronica: the player characters blink by switching the head's texture list (`player.c`,
`PlyEyeTab`: after 30 ticks a 3% chance per tick, then 8 ticks: half, closed, half, open); the
story cast's cutscene models talk with a face mask (`face.c`, `face_bh.c`: muscles and a jaw
driven per phoneme from the room's lip data, `bhLipSet`; `MASK_WORK.param.jawang` is the jaw
angle in degrees).

`actor_glue.c imp_face`, before the skinning of every pose: the same blink (own random numbers:
the game's `rand()` is left alone), the upper lids moved down along the head's up axis by 1.7 x
the lid gap at the closed step (the partial weights need more than the gap), the lower lids up by
0.3; and while the model stands in for a cutscene double, the jaw turned by the double's current
`jawang` (the lip sync the game computes for the double anyway). In gameplay the player has no
lip data (voice calls are short sounds), so the mouth stays closed there.

## F4 window

`overlay.cpp import_window`, hotkey row "Import window" (F4, `B_IMPORT`; F4 was free). A section
per source: the disc image (file selector, saved as `import_outbreak2_iso`), the output folder,
checkboxes (characters, NPCs, enemies), "Extract characters" (a thread; progress bar with the
model being read), then the manifest as a table (character, variant, file, meshes, vertices,
triangles, textures, player model yes/no/failed, notes). After an extraction the new player
models are added to "You appear as" at once.

## Notes for later

### Voices

* `NETBIO01.DAT/adxsnd_usa.afs`: 1853 CRI ADX streams without names: 1600 mono 24 kHz (median
  1.8 s: the scenario dialogue and, probably, the ad-lib lines), 222 stereo 48 kHz (music cues,
  ambience), the rest placeholders. ADX is what this port already decodes
  (`pc/src/audio/adx.c`), so they can be played as they are; which index is which line needs the
  game's tables (the Outbreak decomp, or a listening pass).
* `pl00_vo.afs`: 317 named ADX lines (`c00-245_01-03.adx`: line, take, variant), mono 24 kHz,
  0.3..4.8 s (median 1.1 s): short calls; the name suggests one character's (pl00, Kevin) set; the other characters' sets
  are not in separate archives (to be found: `adxsnd_usa.afs` or the scenario archives).
* `romdata_usa.afs/pl<cc>_voi.snd` (and `_npc.snd`, the AI partner's): MOMO containers of four
  parts: a Sony SCEI HD bank (`SCEIVers`, `SCEIHead`, `SCEIVagi` 20 samples, `SCEISmpl`,
  `SCEISset`, `SCEIProg`), its BD (PS-ADPCM, ~90 KB = ~7 s), and two tables. Short vocal sounds per
  character: pain, death, effort, likely the ad-lib shouts. The port's sound driver already plays
  PS-ADPCM programs (`snd_iop.c` extra banks, as the CORE voice banks of "You appear as"), so a
  converter to its bank format would let an imported character speak with its own voice.

### Social calls ("ad-libs") and animations

* `pl<cc>_pc_000.bin` (per character, 60..225 KB) and `pc_<nnn>.bin` (shared, 50 KB..1.9 MB): motion
  banks; header `u32 0, u32 count`, then (u32 frames, u32 offset) entries. The gestures that go
  with the calls (wave "Come on!", "Help!" with raised arms, nod "Thanks") are most likely in the
  per character file; the shared ones hold the common action set.
* For co-op the cheapest path: a call = a voice line + a motion index sent in the pose (like
  `voice_n`); the motion converted to the CVX 22-node skeleton by the same node map as the mesh
  (Outbreak motions animate the 36-node skeleton: rotations per node; the root carries the
  position).
* Facial/lip data: the face nodes (22..27) and the HP models' face bones; `LIP23_00_0.bin` in
  romdata is a lip track (one, for a cutscene); the event models `EP*` and `HP*` carry the faces.

## Verification (this round)

* Headless extraction from the real disc: all 30 player models (30 converted), and with
  `RECVX_IMPORT_WHAT=6` 285 NPC and enemy models, none failed (a few seconds each run).
* `pc/tools/import_check.py` over all 315 `.glb`: GLB header, accessors within their views, joints
  in range, weights summing to 1, inverse bind matrices against the node tree; previews of the rest
  pose and a skinned test pose (arms lowered) look right; the `.cvxm` read the way the game reads
  it renders the same.
* In game (headless, frame dumps): Kevin, Jim and Alyssa as "You appear as": walking, the
  handgun aimed, zombie grabs, third person from behind and (orbit) the front; the cutscene double
  loads the same model (textures shared). F4: the "Extract characters" button clicked in a run
  with an empty mods folder (progress, then the table); F3 shows the imported model chosen in
  recvx.ini. Screenshots stay in the job directory (game content, not committed).
* ctest 21/21 (`import` new, `appear` extended), Linux and Windows (mingw) builds,
  `layout_check.sh` main (339a4ca) vs this branch: frames identical (vanilla and enhanced).

## Unreleased (outbreak-import branch)

- F4: import window. Resident Evil Outbreak File #2 (SLUS-20984) character models from your own
  disc image to glTF (with PNG textures) and as player models in "You appear as".
