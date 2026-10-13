# RE4 village conversion

The RE4 prototype now connects **RM_1260–RM_1300**: square, paths, western
building (barn role), northern house interior and a bounded church approach lane.
The square keeps its existing walking rectangle. Its west-edge passage reaches the
paths without a temporary door model or door animation. Type-22 doors are placed at
the two reviewed building openings in the paths room. Every connection has a return
entry. Each RE4 room has one retail-model pickup and one persistent zombie.

The latest additions pass offline checks but **are not game-tested**: the sandbox
rejects the 32-bit game process with `Bad system call` before it produces a frame.
The earlier square material revision passed in game; those results do not certify
the new connections, cameras, pickups or zombies. See the latest milestone and
room-by-room acceptance below. Historical sections describe earlier revisions.

## Generated artifacts and reproducibility

All source-derived geometry, measurements, previews and extracted retail bytes remain in
ignored `scratch/` and `mods/`. Do not commit them. Rebuild tools belong in `pc/tools/re4/`;
they read the external source at build time. The source textures are not required.

The current machine's exact paths and source-specific recipe are recorded in
`scratch/RE4_VILLAGE_STATUS.md` and `pc/tools/re4/build_square.py`. From the clone root:

```sh
python3 pc/tools/re4/build_square.py
python3 pc/tools/re4/check_square.py
python3 pc/tools/re4/ray_check.py
python3 pc/tools/re4/render_square.py     # camera previews + top-down overview.png
python3 pc/tools/re4/publish_square.py
pc/tools/re4/play_square.sh cams 80        # headless run, explicit vanilla 4:3 recvx.ini
python3 pc/tools/re4/contact_square.py scratch/re4/play/cams 510 600 660 750 840 960
```

`play_square.sh NAME SECONDS [teleport]` wraps `room_test.sh` (collision on, NOCLIP position
log, frames every 30th presented frame in `scratch/re4/play/NAME/`); add `RECVX_WARP` and
`MORE_INPUT` as in AREA_PLAN.md. The frame numbers given to `contact_square.py` are dumped
presented frames, one per cut, picked from that run (`cams` warps into each tile in turn).
The publisher moves an existing `mods/RM_1260.RDX` to `scratch/re4/archive/`. It checks structural
records, resources, native texture payloads, ground support and compression before writing.
The ignored prototype recipe requires NumPy; the software preview also requires Pillow.
Its source-specific constants are intentionally not a distributable converter.

Evidence and continuation files:

- `scratch/assets/catalogue.json`: complete retail asset catalogue, generated with
  `pc/tools/assets.py inventory --previews` and the user's ISO.
- `scratch/re4/retail-budget.json`: fresh counts from all 205 extracted retail room models.
- `scratch/re4/transform.json`: exact source origin, axis, scale and reference measurements.
- `scratch/re4/square/room.json`, `room.obj`, `build-report.json`: generated source and decisions.
- `scratch/re4/square/final-validation.json`, `mods/re4-square-validation.json`: final checks.
- `scratch/re4/square/camera0.png` through `camera5.png`, `overview.png`: software previews.
- `scratch/re4/contact/cameras.png` (in-game frame + preview per cut), `overview.png`,
  `cut*-game.png`, `old-vs-new.png` (per cut: 2026-10-07 revision, material revision as
  delivered, fixed material revision); earlier sheets in `scratch/re4/contact-20261007/` and
  `contact-20261008-grate/`; runs in `scratch/re4/play/{load,walk,tour,cams,perimeter,survey,stab}/`.
  `cams` takes `RECVX_WARP=1:26:10:196:200:0,1:26:90:270:199:0,1:26:170:330:207:0,1:26:250:216.4:215.2:0,1:26:335:270:250:0,1:26:410:340:250:0`.

## Material mapping and donor review

`pc/tools/area/re4_materials.json` contains material-name entries with `class`,
`retail_texture`, `review_required` and `review_reasons`. References use `room`, `list`
and `texture`, with zero-based list/texture indices. Per-material references remain
explicit; the classes table is a donor-selection aid. Every source assignment still
requires review, including provisional stone walls that could be wood or plaster.

| Class | Retail room:list:texture | Review finding |
|---|---|---|
| Ground | 5810:0:20 | Existing village ground donor |
| Wood | 0120:0:26 | Existing plank donor |
| Stone | 7150:0:29 | Existing stone donor |
| Plaster | 0030:0:72 | Previewed weathered grey render |
| Roof | 0030:0:18 | Previewed dark roof surface |
| Foliage | 1100:0:4 | Broadleaf shrub cutout; native alpha preserved; card UVs need review |
| ~~Second roof~~ | ~~0050:0:73~~ | **Rejected in game**: a 56 % transparent grate, roofs showed the sky through it; 047/058 back on 0030:0:18 |
| Ceramic | 0030:0:6 | Upper-left tableware atlas region; provisional vessel/tableware substitute |
| Metal | 5810:0:1 | Opaque dark metal panel |
| Opacity | 1000:0:22 | Decorative iron cutout; binary alpha verified |
| Water | 1040:0:11 | Blue mottled opaque **surrogate**; native water use unverified |

Candidate contact sheets and PNGs were exported and inspected in `scratch/re4/` and
`scratch/assets/`. The water donor is not approved as transparent or animated water and
is not used in the square. The opacity class records unresolved cutout semantics; it does
not establish that every source cutout is a fence. Foliage, fences and decals need separate
per-use decisions. The build copies native TIM2 payloads, preserving all palettes and alpha;
preview PNGs are never re-encoded into room textures.

In-game review (2026-10-07) reclassified the obviously wrong uses, keeping review flags:
016 (939 thin posts/rails/beams: the fences and timber), 017 (log pile) and 022 (thin boards)
stone → wood; 003, 007, 011, 014 (plant clumps, leaf clusters, grass cards) opacity →
foliage, they had shown as ironwork; 041, 043, 044, 045 (only in the terrain group SMD_021,
including the bonfire mound) unknown → ground, they had shown as planks. The build also
drops the three map-wide alpha overlay layers 001/002/006 (their sloped parts had shown
as ironwork on the ground) alongside the sky shell 000.

Upper sloped context agreed across two measurement grids for additional provisional roof
assignments. Ambiguous materials remain unknown. The ignored prototype logs its provisional
wood/roof substitutions and foliage/ironwork choices per material use. Horizontal alpha
decals are deliberately omitted rather than made opaque. These choices are not promoted to
approved mappings. The builder resolves explicit material donors and group overrides using `materials.py`.
Precedence is group/material, group `*`, then material; a class override selects that
class donor unless a donor is supplied. An explicit null donor fails. Unknown base uses
may still use a logged provisional wood/roof fallback; height alone never selects dirt.
`retail_texture.uv_rect` maps tiled UVs into an atlas region without modifying native TIM2
bytes. Exact source group names remain in ignored `scratch/re4/group-overrides.json`,
loaded by default, or a file selected with `--group-overrides`. The tracked mapping can
also supply `group_overrides`; keep source-derived group names out of commits.

## Axes, scale and partition

Y-up is supported by the source plan view and the flat square ground. The prototype uses
uniform scale approximately **0.8565**, translating by the origin stored in the ignored
transform file and applying no axis reflection. The calibration compares a framed,
door-sized source panel with retail door type 22, measured at **20 CVX units high and
9 wide** through the catalogue helpers. The CVX player-height reference is approximately
**17.5 units**, as used by the existing village camera checks.

The source panel has a slightly sloped upper edge. In game the scale reads right: fences
reach Claire's waist (posts 8–10 units), ground-floor facades are about 1.5 Claire heights,
and the bonfire mound (8 units) is knee-to-hip high. The scale keeps CVX's own door/player
ratio (20/17.5); RE4's own ratio (Leon about 18 source units under a 23.35 door) would mean
scale ≈0.97, about 13 % larger buildings. Revisit only if the doors of the next rooms look
small next to Claire. Do not infer scale from the full-map bounding box.

Only the central square is built. Traversal stays on the surveyed base plane; floor records
provide footsteps/enemy regions, while the room's floor height supplies support. Visual
stairs do not imply walkable height transitions. Seven conservative solid-component
footprints and four outer prototype blockers define navigation. Future rooms should use
reviewed building footprints and the engine's stair/step mechanisms where needed.

The mesh combines visibility culling for the fixed cuts with vertex clustering.
Roof candidates are retained even outside those views; opaque hidden detail, the background
shell and dense tree detail are reduced or omitted. Alpha cards do not occlude geometry
during culling. Opaque materials use projected UVs split at tile boundaries; cutout UVs
remain provisional. The final clustering is coarse and requires silhouette review.
Changing cameras requires regenerating the visibility selection, not just editing camera
records. Visibility sampling can miss subpixel detail; software checks do not prove absence
of holes or all possible visual occlusions.

The measured budgets come from the complete 205-room census described in
[MAPS_DESIGN.md](MAPS_DESIGN.md). The room stays under the retail maximum triangle count and
P90 node/texture counts; compiler vertices (3 per triangle, the compiler duplicates them)
exceed the retail maximum, which is no runtime limit: memory is measured in game. The build now picks the finest clustering cell
whose tiled triangle count stays under 12,000 (retail maximum 13,639). The play-tested
revision used a 1-unit cell, 10,578 triangles, 1.54 MB expanded and 523 KB packed;
the current material revision counts are recorded below. The 6-unit cell of the first prototype
tore roofs and trees into spikes in game. Runtime headroom is measured (below).

Six fixed cuts split the walk rectangle into 3 columns × 2 rows. The build searches each
camera: the north row is shot from the south half looking north, the south row from the north
half looking south, aimed 40 units past the tile at the facades, at the lowest and closest
position (height 30 found for all six) whose 4:3 view holds every walkable sample's feet and
head, which is not under any geometry, and from which every head is visible past the
footprints and the opaque mesh. The first prototype's three cameras 120 units up left Claire
about 10 px tall in a 480-line frame. The sky is the room clear colour (overcast grey-brown).
Cut 3 (south-west) is mostly the bonfire mound; it is reached along z≈215 from the east.

## Validation and in-game acceptance

Offline checks pass for entry clearance, connected walking space, camera-cut coverage,
feet/head frusta, conservative footprint occlusion, sampled actual-mesh head visibility,
source base-ground support, compiled geometry budgets, authored records/scripts, TIM2
FormatId 1, byte-identical native texture payloads and packed-file round trip. The publisher
also checks that RM_1260 is absent from the retail table and archives an existing local room.
There are no enemies, items or outgoing doors in this first prototype.

### In-game results (2026-10-07)

Headless `play_square.sh` runs (32-bit RelWithDebInfo build of this clone, offscreen video,
dummy audio, explicit vanilla 4:3 `recvx.ini`, new game, case 0, Claire, collision on):

| Check | Result |
|---|---|
| Loads by teleport `1:1:26:0` | pass (`mods:` replaces RM_1260, sound key 1260 uses 0060); also re-entry from RM_0020 |
| Cameras | pass after the fix: six cuts, every frame shows the square and its facades, no void or wall interior; in-game frames match the software previews (`scratch/re4/contact/cameras.png`) |
| Scale | pass (provisional, see above) |
| Collision | pass: stops 5.5 from every footprint and bound, as logged: fence pens z 197 and 205.3, cart/log pile z 231.3, mound x 244.6 / z 215.3, bounds x 194 / 345.7, z 176.9 / 251.5. Houses are outside the rectangle, so not reachable |
| Textures | pass with review: dirt, stone walls, slate roofs, wood fences/carts read plausibly, no smearing, no black or transparent holes, foliage cutouts render |
| Stability | pass: 2,100 game frames in one visit plus status screen open/close, teleport to RM_0020 and back |
| Headroom | `RECVX_SPAWN_SURVEY=1:26`: 970 texture entries, 63,905 KB texture heap and 9,676 KB room memory free; the menu screens copy 295 KB |

Known visual limits: the walk rectangle's east edge stops Claire in open ground; the region
crop shows hill tops against the sky in the south-looking cuts; cutout UVs are per-face
normalized; material substitutes below are still to be judged by the user.

Materials flagged during the earlier game test (addressed provisionally in the material revision below): every house wall uses one noisy granite (025,
048, 055, 056; some RE4 walls are plastered); roofs all use one dark slate (024, 047, 058);
028 (3,072 small high-poly triangles in SMD_018/019, unknown → wood), 086/088 (small
high-poly props, stone), 023 (house floors at Y=0 → ground, rest → wood), 082 (one
map-wide unknown mesh) and the palm-frond foliage donor for European shrubs.

Original hand-off recipe:

1. Start a fresh normal Claire game using this clone's `mods/` directory, fixed cameras and
   collision enabled. Set `RECVX_TELEPORT=1:1:26:0`, or use F2 stage 1 / room 26 / entry 0.
2. Compare the player's height with door frames and nearby steps. Confirm Y-up, handedness,
   feet contact and the provisional scale. Check roof silhouettes and visible holes.
3. Inspect every cut, especially alpha edges, repeated textures, palette appearance and
   coarse clustered props. Compare with the software previews; record game screenshots.
4. Walk the whole allowed centre and all seven prop/building footprints. Check the explicit
   outer limits, head clearance, floor contact and both directions of every camera switch.
   Inaccessible stairs and interiors must remain inaccessible.
5. Teleport out and back in to exercise reload. Open menus and remain for at least two
   minutes. Record loading, sound-alias, memory, camera and collision failures separately.

### Next steps (for the next session)

1. Walk area: replace the rectangle with the flat-ground region bounded by real building and
   fence footprints (raster the opaque mesh in the 0.5–17 unit band over source Y=0 ground),
   then add cuts for the new edges with the same camera search.
2. Materials: judge the list above in `scratch/re4/contact/`; add a plaster/whitewash wall
   donor and a second roof donor, and per-group overrides (the `group_overrides` placeholder
   has no consumer yet). Keep review flags until the user accepts each.
3. More rooms: partition the paths, barn, house interiors and church approach as separate
   rooms with their own region, cameras and budget; allocate IDs after RM_1260 (check the
   retail table as the publisher does).
4. Doors: place CVX door type 22 triggers at RE4 door openings (door reference component
   6794 is 20 × 9 after scaling), reciprocal entries, and the door slab/objects as in
   `build_village.py`.
5. Items and zombies: reuse the village recipe (RM_0060 enemy record 0, `wrk_no` = record
   index, flags < 512, sound key 60, ammo/herb floor donors) once the walk area is final.
6. Graveyard chain: retarget or add the RM_0020 east-wall door as `build_village.py` does,
   to RM_1260 instead of RM_1220, then test the whole chain from a new game.

## Surface analysis tool

The generic analysis remains available independently of the ignored prototype recipe:

```sh
python3 pc/tools/analyze_obj_surfaces.py /path/to/village.obj \
  --mtl /path/to/village.mtl --grid-size 100
python3 -m unittest discover -s pc/tools -p test_obj_surfaces.py
```

Outputs are confined to ignored `scratch/`. The report measures area-weighted orientation,
height quantiles, group-relative height and local lower-horizontal anchors. It does not
apply a transform or overwrite the hand-edited mapping. Roof/ground/prop classifications
are hypotheses: compare grid sizes, review all uses, and keep uncertain assignments flagged.
BIN/TPL purpose remains unverified; this prototype uses only the OBJ geometry.

## Navigation survey continuation (2026-10-07)

The first prerequisite for replacing the rectangular boundary is implemented in
`pc/tools/re4/navigation.py`. It reads an external OBJ and transform, clips opaque
triangles to the actor-height band, rasterizes their actual projected polygons (including
edge-on wall segments), expands obstacles for actor clearance, and flood-fills supported
flat ground from an entry. It writes only beneath `scratch/`. This is an evidence tool;
it does not yet change the square's collision or cameras.

`report.json`, `grid.npz`, and `overview.png` in `scratch/re4/navigation/` contain the
survey. Green is reachable ground, tan is solid geometry, brown is other supported ground,
and dark is unsupported space. The inspected overview follows the houses, fences and
central obstacles and exposes the paths out of the square. The exact local invocation is
in the status file. The tool reports open survey edges and unsupported-ground boundaries
separately and refuses to label such a region ready for collider generation. Unknown
opaque materials are conservative solids; this still requires review, especially raised
terrain and mixed materials. The ground test samples cell centers, so it is not proof of
support over the player's entire footprint.

Next: inspect those open boundaries against source geometry, extend the survey along the
paths, and choose real door/gate transitions before authoring collider records. Preserve
the outlines when compacting the raster; do not merge across courtyards or passages.
Then regenerate fixed cuts and visibility culling for the enlarged region and run the
existing square checks, source-support validation, and publication checks. No revised room
has been published in this milestone, and the earlier in-game results apply only to the
existing rectangular prototype.

Additional acceptance for that future revision: walk the former east limit and verify
there is no stop in open ground; trace each facade and fence with collision enabled;
exercise both sides of every new camera transition; verify each open path leads to its
intended room transition and return entry. Check feet contact at terrain boundaries.

## Material revision (2026-10-08)

The current RM_1260 has been rebuilt and validated offline with plaster, a second roof,
broadleaf foliage and cropped tableware donors. It retains the tested walking rectangle,
11 walls and six cuts. Materials 023, 028, 082 and 086/088 now resolve respectively to
wood (including indoor floors), metal, retaining stone and provisional ceramic. A local
group override varies one facade while other stone facades remain. All review flags stay.

As delivered: 10,590 triangles, nine textures, 585,589 bytes packed (archived as
`scratch/re4/archive/RM_1260.1791429677.RDX`). Current output after the in-game fixes:
10,578 triangles, 31,734 vertices, 18 nodes, eight native retail textures; 580,825 bytes
packed / 1,612,912 expanded, SHA256 `06e38826…`. All 6,550 walk samples and 768 head rays
pass, as do source support, authored-record checks and compression roundtrip.

### In-game results of the material revision (2026-10-08)

Same headless setup as 2026-10-07; contact sheets in `scratch/re4/contact/`.

| Check | Result |
|---|---|
| Load, stability | pass: loads by `1:26:0`; status screen open/close, out to RM_0020 and back, 1,470 game frames, no errors (`play/stab`) |
| Headroom | `RECVX_SPAWN_SURVEY=1:26`: 968 texture entries, 63,835 KB texture heap, 9,676 KB room memory free (was 970 / 63,905 / 9,676), all 56 survey spawns fit |
| Plaster vs granite | pass with review: facades 025 (and 048 in one group) read as grey render; granite stays on chimneys (062), the 055/056 house east of the square and stone panels between timbers |
| Roofs | **fail as delivered, fixed**: second donor 0050:0:73 is a 56 % transparent grate; roofs 047/058 (cuts 0–2) showed the sky through a lattice. Both are back on the tested slate 0030:0:18; a real second roof donor is still wanted (an opaque periodic-texture search of the catalogue found no tiled roof, `scratch/re4/roof-review/`) |
| Wrong class | **fixed**: 079 (SMD_137/138, two open wooden boxes on legs, in cuts 0 and 1) rendered as granite blocks; now wood |
| Shrub cutout | pass: binary alpha, no black fringes; very dark green, and wall-climbing cards repeat a whole shrub silhouette (per-face UVs) instead of reading as ivy |
| Chandelier metal (028) | not visible: fully culled from the exterior cuts (metal texture absent from the room); check in an interior |
| Vessels/tableware (086/088) | 17 triangles survive, a few pixels on a shelf behind the upper window of cut 1; no bleed or black. Judge in an interior |
| Retaining walls (082) | stone as intended (plinth around a terrace); not prominent in these cuts |
| Indoor floors (023) | wood: steps and floors visible through doorways (cuts 1, 2), no dirt |
| UV regions / atlas | pass: no smearing, holes or black textures in any cut |

The builder now refuses an opaque-class donor whose native texture has transparent texels
(`build_square.py`, after compile), so a cutout donor cannot be mapped to walls or roofs again.

Material acceptance (done 2026-10-08, above; repeat after donor changes): teleport to `1:1:26:0`, inspect all six cuts and compare the current
software previews. Check plaster/stone variation, both roof patterns, shrub transparency,
wood indoor floors and vessel/tableware atlas edges. Chandelier metal may be culled from
this exterior and must also be checked when an interior is built. Look for seams, atlas
bleed and black alpha borders. Repeat the walking perimeter, menu visit and room reload,
and record screenshots and memory headroom. Existing silhouette/detail holes and cropped
scenery remain separate visual limitations; passing offline checks does not resolve them.

## Outline reduction candidate (2026-10-08)

`navigation.py` now records nonvertical terrain separately from walls, as floor-transition
requirements. `outline.py --reduce` merges collinear runs and neighbouring boxes only
inside an allowed footprint envelope. `--tolerance` permits a bounded loss of cells near
walls (rounded upward to the survey cell size); paths joining clear regions are protected
before merging, including narrow bends. Clear projected terrain is protected by the same
envelope. Source solids remain covered; unsupported ground never generates a perimeter wall.

The current **candidate**, in `scratch/re4/navigation-terrain/outline-final.json`, has
145 records, below the retail maximum of 168. It uses a three-unit tolerance. The strict
box-clearance audit finds 41,069 connected supported samples versus 43,959 before reduction:
2,890 samples near walls are removed, with **zero surviving baseline samples disconnected**.
Those losses need visual and in-game review; meeting the record budget alone is insufficient.

This is **not installed** in RM_1260. Twenty open boundary clusters still require floor or
room transitions. Floor records do not constrain engine movement, so treating the support
mask as a wall would hide a real hole in the navigation. The candidate also extends beyond
the current square mesh crop and local room origin. Next: review each boundary against
source heights/openings, choose room partitions and author real transitions, rebase geometry
and entries where necessary, then regenerate cameras and visibility culling. Run compiled
integer-grid navigation, source support and camera checks before replacing the rectangle.
No new room IDs, doors, items, zombies or graveyard links have been created.

Reproduce reduction after the terrain survey (the external input invocation is in the
ignored status file):

```sh
python3 pc/tools/re4/outline.py scratch/re4/navigation-terrain --reduce --tolerance 3 --output scratch/re4/navigation-terrain/outline-final.json
python3 pc/tools/re4/validate_outline.py scratch/re4/navigation-terrain scratch/re4/navigation-terrain/outline-final.json --output scratch/re4/navigation-terrain/outline-final-validation.json
python3 -m unittest discover -s pc/tools/re4 -p 'test_*.py'
```

The audit uses strict box inequalities at survey cell centers and applies the actor radius
once. Its PNG shows reachable samples in green, lost samples in red and expanded collision
in tan. It deliberately remains `publishable=false`: this is neither compiled integer-grid
validation nor runtime proof. Seventeen synthetic tests cover material precedence, atlas
UVs, source rasterization, terrain separation, courtyard/passages, strict clearance and
detection of a disconnected region.

Acceptance once the outline is integrated: trace building and fence edges, especially red
loss bands in the audit preview; traverse every protected narrow passage both ways; test
all floor transitions for feet contact and correct floor number; exercise every camera
switch in both directions. Verify former rectangle limits are freely crossable and every
open path has a valid transition and return entry. Repeat collision tests with enemies when
added. Keep the earlier rectangular room available until these checks pass.

## Boundary height review (2026-10-08)

The new generic `pc/tools/re4/boundary_heights.py` probes source triangles beyond each
open boundary, stopping at candidate collision. It keeps ground-class terrain separate
from other opaque support, applies material exclusions and optional group overrides, and
writes only to `scratch/`. Missing geometry is never sufficient evidence for a drop wall.
The current survey finds ground-class support throughout the probes at 19 openings;
the remaining opening needs material review. These results do **not** close the openings.

`pc/tools/re4/fit_transitions.py` measures how well one axis-aligned incline fits each
opening's outward probes. Four pass the provisional 0.25-unit error threshold, fifteen
fail this least-squares approximation, and one lacks complete ground-class support.
Failure is a reason to review the fit or subdivide terrain, not a mathematical proof that
no alternative approximation exists. Even passing fits need full footprint support,
safe landings and boundary continuity checks. No slope records have been installed.

Engine inspection confirms that type-6 **wall** records implement slopes; behavioural
floor records do not move the actor vertically. `hitchk.c:bhCheckWall`, case 6 (enemies),
projects an eligible actor onto an incline and updates/clamps its height through
`grand[]`. Direction IDs are 0 rising +X, 1 rising -Z, 2 rising +Z and 3 rising -X.
The player's `bhCheckWallEx` has no case 6. **Update (2026-10-08):** in `"budget": "pc"`
rooms the PC build sets the player's y from the slope surface at its x/z, without the
horizontal drift, so a source-height lookup per slope box is now what to simulate. Format,
helpers (`rdxbuild.slope_wall`, `slope_height`, `heightfield_slopes`) and rules:
[MAPS_DESIGN.md](MAPS_DESIGN.md) "Slopes". Slopes share the wall budget, leaving 23 records beyond
the current 145-box outline. Below-zero terrain also needs deliberate floor-level setup.

Continuation: select terrain patches and room partitions from the ignored evidence;
fit complete rectangular slope footprints, including both landing edges; simulate the
engine projection and floor changes in both travel directions; rebuild collision at each
height and test actor clearance, support and connectivity. Include slopes in wall counts.
Only then search eye-level cameras over the final walk area, regenerate visible geometry,
and run compiled structural/navigation checks before publishing. RM_1260 retains the
rectangle, six cameras and teleport `1:1:26:0`. No additional rooms or links are published.

Additional in-game acceptance for a future terrain revision:

1. With collision enabled, walk each incline up, down and diagonally; stop and reverse
   midway. Check feet contact and unwanted sideways movement at every slope seam.
2. Cross both landing edges repeatedly. Check floor-number/camera changes in both
   directions, especially where terrain dips below the square's base height.
3. Walk near each slope's side and adjacent fence/building. Confirm the actor cannot
   enter unsupported ground or get trapped between a ramp and an expanded blocker.
4. Test each room entry/return on its intended landing, then menu/reload. After enemies
   are added, repeat with zombies and verify their slope eligibility and floor behavior.

Reproduce the fit review after generating boundary evidence with the source paths recorded
in the ignored status file:

```sh
python3 pc/tools/re4/fit_transitions.py scratch/re4/navigation-terrain/boundary-heights.json --output scratch/re4/navigation-terrain/transition-fits.json
python3 -m unittest discover -s pc/tools/re4 -p 'test_*.py'
```

## Connected rooms milestone (2026-10-08)

The current recipes remain ignored: `scratch/re4/rooms-recipe.json` stores source
partitions and corridor bounds; `scratch/re4/connections.json` stores links, return
entries, pickups, enemies and the graveyard patch. Generic tools live in
`pc/tools/re4/`. No extracted geometry, source coordinates, ISO data or room binaries
belong in commits.

| Room | Role | Teleport | Return entries |
|---|---|---|---|
| RM_1260 | Square | `1:1:26:0` | 1 from paths; 2 from graveyard |
| RM_1270 | Paths | `1:1:27:0` | 1 from square; 2 from barn; 3 from house; 4 from approach |
| RM_1280 | Western building / barn role | `1:1:28:0` | 1 from paths |
| RM_1290 | Northern house, ground floor | `1:1:29:0` | 1 from paths |
| RM_1300 | Church approach lane | `1:1:30:0` | 1 from paths |
| RM_0020 | Graveyard | `1:0:2:0` | 3 from RE4 square; existing entry 2 still serves CVX village |

The first teleport field is the trigger frame. In F2 use stage 1, rooms 26–30,
entry 0, case 0. These are separate from the CVX-only village RM_1220–RM_1250.
The latter files and sound aliases are preserved locally so the old graveyard
connection still has its destinations.

The paths use a union of bounded rectangles with conservative source-mesh blockers.
The western landing contains a tiny terrain seam: only portions of near-horizontal
source surfaces within 0.15 game units of floor zero count as support. Missing ground
is not filled. This is a flat-floor approximation, not the paused outline/slope work.
The barn and house retain their ground-floor source geometry; upstairs areas remain
inaccessible. “Barn” is the requested room role for the western building, not a verified
source asset name. The church room is an approach-lane prototype, not a church interior
or a completed approach extending to every landmark.

Base-room checks cover connected integer walking samples, entry clearance, bounded
source support, compiled camera coverage/frusta including shared cut edges, native
retail textures, opaque-donor alpha, geometry/memory budgets and compression round trips.
Compiled mesh checks found zero head occlusions in 340 paths, 77 barn, 150 house and
168 approach rays. The sampled rays do not certify every pixel or the added door/actor
models. Software previews are in `scratch/re4/rooms/ROOM/camera*.png`; layouts are in
`walking.png`. Some exterior crops expose sky at terrain edges; interior furniture
still has provisional material substitutions. These require in-game visual review.

`connect_rooms.py` stages all builds before installation, checks clear return entries,
reachable trigger neighborhoods and placement clearance, imports type-22 door models,
and adds retail RM_0060 ammo/herb display models and zombie record 0 assets. Each room
has one zombie (`wrk_no = 0`), conservatively well below the 128-slot budget even with
its child parts, and sound alias 60. Pickup and death bits 490–494 are checked against
the retail script census and existing custom rooms. Entry/frame scripts preserve
pickups and deaths across revisits. Offline checks cannot establish runtime AI,
interaction-probe reach, sound, resource headroom or persistence.

The second graveyard door is appended beside the existing east-side CVX door, farther
south. It targets RM_1260 entry 2. Its return entry is RM_0020 entry 3. The square's
south-edge return is an unanimated passage, with no added square-edge door model.
`graveyard_link.py` verifies byte-identical prefixes for every original record array,
unchanged original scripts/motions and the retained RM_1220 trigger. The old graveyard
base is read from the parent project's mods; the resulting file is written only here.

Rebuild from this clone root, with the external inputs in the ignored recipes available:

```sh
python3 pc/tools/re4/build_rooms.py scratch/re4/rooms-recipe.json
python3 pc/tools/re4/connect_rooms.py scratch/re4/connections.json --publish
python3 -m unittest discover -s pc/tools/re4 -p 'test_*.py'
```

The flag census used by the connection recipe is `scratch/re4/retail-flags.json`,
measured from all 205 retail scripts. Regenerate it when the input disc changes using
`build_area.census`; do not substitute an assumed empty flag set. Rebuilding the square
with its historical publisher alone removes the links; rerun `connect_rooms.py` afterward.
Final installed hashes and sizes are in `scratch/re4/connected/validation.json`.
Previous installed revisions are archived in `scratch/re4/archive/`.

### In-game acceptance for the connected revision

Use a fresh Claire game, case 0, collision enabled, fixed cameras, enemy extras and
randomization disabled, and this clone's `mods/`. For every room, inspect all camera
cuts in both travel directions, walk its full bounded perimeter, check feet/head
clearance, open/close the status screen, leave/re-enter and remain for two minutes.
Record screenshots and texture/room-memory headroom. The headless attempts in
`scratch/re4/play/*-connected/` produced zero frames because of the sandbox failure;
they are not passing tests.

1. **Square (RM_1260):** verify the original six-cut layout and material appearance.
   At the west rectangle edge, use the passage into paths; return through paths entry 1
   and confirm a clear landing. There must be no temporary square-edge door slab or
   door demo. At the south rectangle edge, use the graveyard return passage and come
   back to square entry 2. Confirm neither landing overlaps the central props.
2. **Paths (RM_1270):** from the square, follow the western branch to the western
   building opening and the northern branch to the house opening. Inspect door slabs
   against the jambs, activate both from ordinary walking distance, and return from
   each interior. Check the western terrain seam for visible hovering or snagging.
   Follow the eastern passage to the approach and back. Confirm entry 4 is clear and
   no unrelated doorway appears at the square edge.
3. **Barn (RM_1280):** enter through its paths door, inspect the ground-floor timber
   interior, furniture and retail materials, then return through the same opening.
   Check both sides of the type-22 animation and that the landing faces into the room.
   Walk the allowed area around props; verify no access through walls or upstairs.
4. **House (RM_1290):** enter through the northern house's real opening. Inspect the
   ground floor, stairs, furniture and ceiling from every cut. Confirm stairs remain
   bounded, the camera never sits above the ceiling, and the exit returns to paths
   entry 3 without catching the jamb or immediately transitioning back.
5. **Approach (RM_1300):** enter via the paths passage and walk the bounded lane in
   both directions. Inspect crop edges and nearby fences/buildings, floor contact and
   all six cuts. Return to paths entry 4. The church interior is not part of this room.
6. **Items/zombies in every RE4 room:** verify the ammo box or herb is visible, collect
   it, confirm inventory changes, and revisit to verify it stays gone. Let the zombie
   approach and attack, check sounds, kill it and revisit to verify it stays dead.
   Check door/entry clearance with the zombie nearby and monitor memory headroom.
7. **Graveyard (RM_0020):** check both east-side doors separately. The original door
   must still reach RM_1220 and return to entry 2. The second door must play type 22,
   reach RM_1260 entry 2 and return to graveyard entry 3 through the square passage.
   Verify the new landing is clear of the low obstacle and the original retail exits,
   items, enemies and scripts still work. Finally traverse the whole connected chain
   from the graveyard without teleporting.

## Whole-map PC room inspection history (RM_1310)

These historical inspection notes are superseded by “Published whole-map PC room”
below, which describes the installed traversal revision.

RM_1310 is a separate **inspection build**, teleport `1:1:31:0` (F2 stage 1,
room 31, entry 0, case 0). It contains the uncropped village mesh, excluding the
background dome and logged overlays, but still confines walking to the previous
square enclosure. It is **not the finished whole-village traversal room**. The old
RM_1260–RM_1300 rooms and both existing graveyard connections remain unchanged.
All publication is into this clone's `mods/`; do not synchronize parent directories.

The current PC-budget mesh has 130,498 triangles, 217 nodes and nine native retail
textures. Maximum node size is 8,187 vertices; maximum strip chunk is 27,291 shorts.
Packed plus expanded room data totals 20,397,869 bytes. Existing material mapping
and ignored group overrides are applied. Material substitutions and alpha-card UVs
still need visual review. Camera-specific distant vegetation hiding bounds all
unhidden alpha triangles, without relying on frustum clipping, and reserves 4,096
sorted primitives for actors/effects. The current installed cuts peak at 25,138.

The current terrain survey produces 16,313 reachable samples. Its independently
fitted ramps disagree at many shared edges. `joint_terrain.py` fits height samples
and shared edge endpoints together, with either weighted least squares or a
bounded-error feasibility search. Exact-edge smoothing alone can distort terrain;
both source fit and seam error must pass. Smaller patches improve agreement but do
not resolve every seam. Adjacencies exceeding the survey's slope threshold require
separate drop/transition review even when both sides are reachable via another path.
An iteration limit or passing local patch is never treated as a completed floor
layout. Source corner support, floor-number assignment, engine projection and open
boundary checks remain required. No candidate slopes have been installed.

The refreshed camera candidate set contains 90 cuts covering the current connected
survey, with no uncovered samples or frustum failures. Dense mode tests visibility
at feet, torso and head positions for every survey sample within each cut; shell
hides never remove terrain nodes. This remains discrete survey coverage, not proof
of continuous movement or coverage of every potentially walkable part of the map.
The candidates are not installed until terrain and collision pass. Reports include
input hashes to identify stale geometry/survey results.

Reproduce from this clone root with the existing ignored recipe and external inputs:

```sh
python3 pc/tools/re4/build_whole.py scratch/re4/whole-recipe.json --publish-inspection
python3 pc/tools/re4/survey_whole.py scratch/re4/whole-recipe.json
python3 pc/tools/re4/audit_terrain_seams.py scratch/re4/whole/terrain
python3 pc/tools/re4/joint_terrain.py scratch/re4/whole/terrain --tolerance .24 --tile 1
python3 pc/tools/re4/cameras_whole.py scratch/re4/whole --dense-rays
python3 pc/tools/re4/validate_whole.py scratch/re4/whole-recipe.json
python3 pc/tools/re4/render_whole.py scratch/re4/whole --candidate-cameras --cuts 0 30 60 89
python3 -m unittest discover -s pc/tools/re4 -p 'test_*.py'
```

Evidence lives in `scratch/re4/whole/`: `validation.json` and `offline-audit.json`
refer to the installed inspection build; `terrain/joint-slope-candidates.json` and
`terrain/joint-fit-checks.png` identify passing and failing candidate patches;
`cameras/report.json` describes candidate coverage; `previews/` contains an overview
and camera images. These software renders are not game captures. The compiled
inspection audit checks actual mesh support, connectivity, camera framing/occlusion,
alpha budget, compression and unchanged fallback room hashes. Runtime remains untested.

Next: refine failing terrain patches using actual source edge/corner heights,
review steep adjacencies and the open boundary evidence, assign floor levels and
simulate both directions across all landings. Then install terrain/collision and
validate continuous camera switches before placing items/zombies and linking the
interiors/graveyard. Do not retarget the graveyard to the inspection enclosure.

### Adaptive source probes and static footprint candidates

`refine_terrain.py` provides an independent refinement of the center-only joint fit.
It samples reviewed source triangles at each rectangle's corners, edge midpoints and
center, then subdivides where an axis incline misses the source tolerance. Steep
inclines remain explicit transition candidates. The shared-edge audit handles unequal
subdivision sizes, including T junctions, and reports connectivity after rejected
inclines are removed. Passing these discrete probes does not prove continuous polygon
coverage or movement through the engine wall loop. It never installs room records.

`collision_whole.py` turns the survey's solid mask into an exact rectangle cover and
static type-0 wall candidates. It checks strict box clearance with actor radius applied
once and verifies that the existing reachable survey loses no samples. Its conservative
vertical band encloses the surveyed terrain heights and needs review before supporting
stacked floors. Unsupported perimeter edges remain explicit; they are not automatically
closed by invisible walls. All candidates are static, with no script toggles.

```sh
python3 pc/tools/re4/refine_terrain.py scratch/re4/whole-recipe.json
python3 pc/tools/re4/collision_whole.py scratch/re4/whole-recipe.json
python3 -m unittest discover -s pc/tools/re4 -p 'test_refine_terrain.py'
```

The reports and plan previews are `terrain/refined-slope-candidates.json`,
`terrain/refined-fit-checks.png`, `terrain/static-collision-candidates.json` and
`terrain/static-collision-checks.png` below the recipe's scratch output directory.
Reports record input hashes; regenerate after changing the source, transform, mapping
or survey. These tools do not alter RM_1310, its cameras, contents or graveyard links.

## Published whole-map PC room (2026-10-09)

This revision supersedes the inspection/WIP and staging notes above. RM_1310 is
published in **this clone's `mods/` only**; teleport `1:1:31:0` (F2 stage 1,
room 31, entry 0, case 0) opens the completed authored traversal room. RM_0020's
existing RE4 graveyard door now reaches RM_1310 entry 1. Only its three destination
operands change; the CVX village door, original graveyard bytes and return entry 3
are preserved. The RM_1260–RM_1300 fallback rooms remain unchanged.

The room contains the uncropped village mesh with the previously logged dome and
overlay exclusions, retail material substitutions, 93 camera cuts, six pickups and
five zombies. Geometry remains 130,498 triangles / 217 nodes, with 22 native retail
textures including actor assets. There are no additional interior room links.
Provisional materials and distant vegetation hiding remain as documented above.

Terrain uses 11,492 explicit type-6 slopes, including flat patches, on floor 0
(`grand[2] = -2`). Collision contains 3,866 building/fence/prop boxes plus 106
invisible support-boundary walls. Those walls add no terrain. Their faces account
for actor radius and stop centers 0.03125 units inside the half-open slope edge.
Clearance excludes 134 boundary samples and one isolated sample; the remaining
16,461 surveyed samples are connected. Entries, pickups and zombies use this
connected region and pass compiled surface-height and collision-clearance checks.

The current engine includes the merged downhill height-band fix. `slope_engine.py`
extracts and hashes `pc_slope_ground` from `hitchk.c`, compiles it into an ignored
scratch shared library, and runs movement with C float arithmetic over actual
compiled slope records. Spatial bins omit only impossible X/Z candidates and retain
record order. All 499,168 steps across the original survey's bidirectional
trajectories pass, including the boundaries subsequently restricted by walls.
The standalone two-ramp reproducer now returns `slope_hit = 1`, y 9.9.
No engine changes were made by this authoring continuation.

Compiled quarter-unit rectangle coverage passes 1,062,144 probes with zero gaps,
including lattice seams. Maximum compiled shared-edge step is 0.248323 units;
maximum source-center error is 0.119959. All 106 compiled boundary walls pass
full-edge closure checks. Compiled camera coverage/framing, placement clearance,
geometry/texture preservation, native asset budgets and compression round trips
pass. Maximum unhidden alpha remains 28,657 with 4,096 reserved for actors/effects.
Packed and expanded room sizes are 6,154,800 and 18,187,204 bytes, respectively.

The earlier continuous source-polygon certificate was an additional authoring
check, not an engine requirement. It is **superseded for this acceptance** by the
compiled quarter-unit rectangle coverage and current engine movement simulation,
as requested. Its historical report remains in scratch and is not presented as a
passing certificate. The accepted checks cover the authored conservative survey;
they do not claim access to every source surface or resolution of stacked floors.

Validation evidence is in `scratch/re4/whole/final/validation.json` and
`mods/re4-whole-validation.json`; both record `published=true`. The installed
file/unchanged-fallback audit is `scratch/re4/whole/final/installed-audit.json`. Engine
function evidence is in `scratch/re4/slope-engine/function.json`. Previous installed
room versions are archived in `scratch/re4/archive/`. No source geometry or ISO
payloads belong in tracked files. No game run was performed; the user tests the map.
There are no remaining offline publication blockers. The 54 synthetic tests pass.

Rebuild with the existing ignored recipes and external inputs, from this clone:

```sh
python3 pc/tools/re4/terrain_pc.py scratch/re4/whole-recipe.json
python3 pc/tools/re4/collision_whole.py scratch/re4/whole-recipe.json
python3 pc/tools/re4/finish_whole.py scratch/re4/whole-recipe.json --publish
python3 -m unittest discover -s pc/tools/re4 -p 'test_*.py'
```

The publisher stages and checks before installing. For later revisions, archive the
prior publication and refresh the installed baseline in `final-before.json` before
staging; the baseline must reflect the intended preserved files. The graveyard
retargeter expects the historical RM_1260 destination, so rebuild against the
archived pre-retarget graveyard or extend it explicitly for subsequent revisions.
Do not run the historical inspection publisher over the finished RM_1310.
