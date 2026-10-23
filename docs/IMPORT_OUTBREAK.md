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
| 1. Format research | done: disc layout, archives, compression, model format (below) |
| 2. Extractor (C, `pc/src/import/`) | done: the 30 player models (8 characters, all costumes) to glTF + PNG; NPCs and enemies optional |
| 3. Conversion to CVX player models | converter written (`.cvxm`), in-game loading and per-frame skinning: see "Conversion" |
| 4. F4 window | in progress |
| 5. Notes for later (voices, ad-libs, animations) | first findings below |
| 6. Verification | glTF checked (structure, skin, bind pose, a test pose) and rendered |

Resume notes: build with `cmake --build build`; headless run:
`RECVX_MODS=<out> RECVX_IMPORT=outbreak2 RECVX_IMPORT_ISO=<image> build/pc/recvx --save-dir <tmp>`
(`RECVX_IMPORT_WHAT` = 1 players, 2 NPCs, 4 enemies, summed). Exploration scripts (Python,
not shipped) live outside the repository.

## Disc format

ISO 9660, 4.7 GB. Root files:

| File | Content |
|---|---|
| `SLUS_209.84` | the executable |
| `NETBIO00.DAT` (1.0 GB) | AFS: `romdata_usa.afs` (common data, 1087 files) and per scenario `rNNN.afs` + `rNNN_h.bin` (r006, r010, r015, r020..r030, r040, rfff) |
| `NETBIO01.DAT` (1.4 GB) | AFS: `adxsnd_usa.afs` (1853 CRI ADX streams: dialogue/voice), `bgm.afs` (96 ADX music tracks), `pl00_vo.afs` (997 ADX voice lines, many `dmy.adx` placeholders) |
| `BIN/01.DAT` .. `05.DAT` | AFS of CRI Sofdec movies (`.sfd`) |
| `BIN/0.DAT` .. `9.DAT` | `MWo3` overlays (program modules) |
| `0FLIST.DIR` | list of the big archives |

**AFS**: `"AFS\0"`, file count n, n x (offset, size), then (offset, size) of a name table of n
48-byte records (32-byte name, date, size). Offsets are relative to the archive; archives nest.

**romdata_usa.afs** (names as on the disc; NBD = model):

| Files | What |
|---|---|
| `P<cc>_<vv>.nbd` | the 8 playable characters (cc 00..07: Kevin, Mark, Jim, George, David, Alyssa, Yoko, Cindy), costumes vv 00..02 (Alyssa, Yoko, Cindy also 03, 04): 30 files |
| `EP<cc>_<vv>.nbd` | the same characters' event (cutscene) models |
| `HP<cc>.nbd` | high-detail player models (512x512 textures, a second model pair: the head) |
| `N*.NBD`, `HN*.NBD` | NPCs (normal / high detail); `*_N.NBD` parts |
| `E<tt>_<vv>.NBD`, `HE*.NBD` | enemies; `EF01.NBD` effects |
| `pl<cc>_pc_000.bin`, `pc_<nnn>.bin` | motion data (player characters' and shared; see "Animations") |
| `e<tt>_<nnn>.bin`, `ep<tt>_tbl.bin`, `rn<nn>_tbl.bin` | enemy motions, enemy-player tables (grabs), room tables |
| `pl<cc>_voi.snd`, `pl<cc>_npc.snd` | per character sound banks (MOMO containers of SCEI HD/BD banks: PS-ADPCM) |
| `*.snd`, `*.snp` | other sound banks (common, weapons, enemies) |
| `*.MAT`, `*.mat`, `SHADOW_*.SDT`, `*.tm2` | material tables, shadows, 2D textures |

### NBD (model)

Little endian. A table of 16-byte entries at 0 (`type[4], offset, size, count`), as long as the
first entry's offset (0x40 or 0x80):

| Entry | Content |
|---|---|
| `TEX` | `count` textures: u32 size + SLD-compressed TIM2 |
| `AMO` / `AHI` | the model and its node hierarchy (an optional second pair: an add-on model, e.g. HP heads) |
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
* Material (`0x13xxxx` etc.): ambient, diffuse, specular (float4 each), specular decay, texture
  count, 200 bytes, texture indices into the texture list. Texture list entry: id = TEX slot.
* AHI `0xC0000000`: a root list (`0x80000000`: root ids), then per node a `0x40000001` (node),
  `0x40000002` (mesh node) or `0x40000003` (joint) chunk: id, parent, child, next, scale[4],
  rotation[4] (Euler x, y, z in radians, w unused; stored negated for joints), translation[4],
  mesh id, group.
* Positions of skinned meshes are in model space, in the bind pose: a T-pose with almost all
  node rotations 0. Units are centimetres-like (Kevin is 173 high); y up, the character faces +z.

**TIM2** textures: standard PS2 TIM2, 8-bit indexed 256x256 with a 32-bit CLUT (CSM1: entries
8..15 and 16..23 of every 32 swapped), alpha 0x80 = opaque. Both header alignments occur (format
id 0: 16-byte file header; 1: 128 bytes). The GS registers in the picture header are the same as
Code: Veronica's for the same size and depth.

**The human skeleton** (147 models share it: all P, EP, most N): 36 nodes (Cindy's main costume
43, an extra hair chain). 0 root, 1 (pivot), 2 hips (y 110), 3 hip pivot, 4..6 and 7..9 legs
(thigh, knee, ankle), 10 spine, 11 chest, 12..15 and 16..19 arms (clavicle, shoulder, elbow,
wrist), 20 neck, 21 head, 22 face (23..26 eyes/lids, 27 mouth), 28..31 and 32..35 fingers.
Groups: 0 lower body, 1 upper body, 2 face, 3/4 hands.

Sources: format names and the chunk ids follow the public notes of the Outbreak modding
community, chiefly Fothsid's [libthanatos](https://github.com/Fothsid/libthanatos) and
[OBTool](https://github.com/Fothsid/OBTool) (both GPL-3.0: read as documentation, **no code
copied**; ours is MIT and written from the data), and the [Outbreak decomp](https://github.com/GalaxyNostalgia/Outbreak-decomp)
project's existence (not used yet: a better source for motion and sound formats later).

## Extraction (`pc/src/import/`)

| File | Role |
|---|---|
| `import.h`, `import.c` | sources table, extraction thread and progress, manifest, the registry for "You appear as" |
| `ob2.c` | Outbreak File #2: ISO 9660 root lookup, AFS, SLD, NBD reading into an `ImpModel` |
| `impmodel.h`, `impmodel.c` | glTF 2.0 binary writer (skeleton, skin, PNG textures), TIM2 decoding, the `.cvxm` converter |

Output per model in `<dir>/outbreak2/<id>/`: `<id>.glb` (viewable in Blender: one skinned mesh, a
primitive per texture, the 36-node skeleton, scale 0.01 to metres), `<id>_tex<k>.png`, and for the
playable characters `<id>.cvxm`. `<dir>/outbreak2/manifest.txt` lists every model (counts, whether
it is a player model, failures with their reason); F4 shows it after a restart.

The disc is identified by `SYSTEM.CNF` (`SLUS_209.84`); a European or Japanese disc is refused.

## Conversion

See the next commits: `.cvxm` layout in `impmodel.h`.

## Unreleased (outbreak-import branch)

- F4: import window; Resident Evil Outbreak File #2 character models to glTF and as player models.
