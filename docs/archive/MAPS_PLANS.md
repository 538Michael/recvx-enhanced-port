# Original room briefs

These designs borrow spatial/compositional techniques, not meshes, textures or script bytes.
Room IDs are exact references to the user's retail files; inspect them with `rdxbuild
 decompile` locally. The measured evidence and compiler contract are in
[MAPS_DESIGN.md](../MAPS_DESIGN.md). All dimensions below are game units (approximately ten
per metre). Plans show x to the right and +z upwards. `D` door, `I` item, `Z` zombie,
`C` camera, `P` puzzle, `#` solid, `.` walkable. Camera symbols mark approximate mounts,
not their full switch areas. Estimates are person-days for a developer/artist familiar
with the tools, after the current runtime acceptance blocker is resolved; original polished
art, voice acting and new animation production are additional work.

## 1. Service Corridor — first playable blockout

**References:** RM_0060's 60x122-unit camera-area span, five cuts and route-guided zombie
space; RM_1110's focused item framing and return-to-one-entrance rhythm. Our room reduces
this to a 40x100 rectangle, two cuts and one zombie. `rdx_demo.py` already generates this
plan's technical blockout; it passed the in-game acceptance runs (MAPS_DESIGN section 3).

```text
z=100  +--------------------+
       |             Z      |
       |                    |
       | ###                |   crate center (8,65), 6x6
       | ###                |
z=53   |          ##########|
       |          #cabinet #|   cabinet (22,45), 18x8
z=45   |          ##########|
       |         C1         |   camera mount (20,38,38)
       |     I              |
       |                    |
z=10   |         entry      |
z=0    +-------DDDDDD-------+
                 C0             mount (20,38,-12)
       x=0                x=40
```

Camera C0 looks along +z at a downward pitch, covering z=0..52; C1 repeats the direction
from farther into the corridor, covering z=48..101. The four-unit overlap holds the old
shot while crossing the cabinet's near edge. First acceptance task: verify pitch/sign,
foreground door occlusion and visibility of the pickup in the native renderer. Keep the
exit legible in the return direction. For finished art, hide/split the front door surround
per shot rather than drawing all faces twice.

One optional ammunition pickup at (12,1,26) has its own trigger and saved flag. One zombie
at (28,0,78) should approach around the cabinet; the crate provides a second turn but must
not make an unavoidable choke. No puzzle in phase one. Phase two could add a switch that
restores a ceiling lamp and reveals a painted maintenance code; use an examine event and
a saved flag, without moving the exit behind a lock.

Assets: tiled floor, plaster/painted wall and warning-stripe materials (three original
64x64 blockout textures); room shell; baked cabinet; separate crate OBJ; pickup display
OBJ; a visible door slab; runtime zombie dependency and donor sound key. Finished version:
add pipes, two lamps, an exit sign and chipped paint decals baked into opaque atlases.
Target 500–1,500 triangles and 6–12 textures after replacing the blockout boxes.

Effort: 0.5–1 day runtime validation and tuning, 1–2 days original art, 0.5 day persistence/
door regression. Optional lamp event adds 0.5–1 day.

Build steps: generate source with `rdx_demo.py`; compile to RM_1150; add sound key 60;
retarget RM_1110 trigger 0 using `rdxbuild link`; run walk/pickup/enemy/return tests from
MAPS_DESIGN. Tune clearances before replacing art. Recompile each change, inspect the OBJ
re-export, then repeat both-direction camera and door checks. Keep item flag 506 and enemy
flag 500 reserved for this example only (persistence flags must be below 512).

## 2. Records Annex — a quiet room with an exposed reward

**Reference:** RM_1110 (piano room), four cuts, six item records, no enemy records, roughly
91x82 units. Borrow the focused examination shots and furniture as composition, while
building an original municipal records office. Proposed footprint 70x65, three cuts,
one optional pickup and one progression clue. Connect from a different existing door only
after checking the world graph; provisional new slot RM_1160 must be checked for conflicts.

```text
z=65   +----------------------------+
       | file shelves   | desk P  I |
       |  ###########   |           |
       |  ###########   +---  ------|
       |        C2                  |
       |                    ######  |
       |     reading table  ######  |
       |     ########               |
       |     ########     C1        |
       |                            |
z=0    +---DDD----------C0----------+
       x=0                        x=70
```

C0 at (55,32,-8) frames entry and reading table; its zone covers z=0..24. C1 at (65,30,26)
looks toward the shelf aisle, zone z=22..48 excluding the desk alcove. C2 at (34,24,58)
frames desk, clue and item; zone x=38..70,z=42..65. Give the desk alcove explicit cut
priority and test the approach around its partition. Maintain a 16-unit aisle and at least
12 units behind the desk. Scripted close-ups are a later polish step, not required to read
the puzzle.

Puzzle: three filing labels encode the order of three desk drawers; the correct order
unlocks a document with the maintenance-wing code. Implement three examine triggers and
a saved completion flag. Use original text, not exported retail messages. No initial
enemy; after collecting the document, a knock from the corridor creates pressure without
spawning an enemy inside a narrow desk interaction. Optional healing supply is always
accessible so the puzzle cannot consume a required key item.

Assets: walls/floor/ceiling, reading table, two shelves, desk with three separate drawer
nodes, folders, lamp, door, supply prop; 12–18 opaque textures at 64/128/256; 2,500–4,000
triangles; warm desk accent against cool ambient. The sound bank can use a quiet retail
donor initially, with a documented alias.

Effort: 1 day blockout/cameras, 2–3 days original art, 1–2 days puzzle script and message
support, 1 day persistence/QA. Message authoring is a compiler dependency still outstanding.

Build steps: derive a fresh JSON from the corridor recipe, remove enemy dependency, add
three cuts and furniture boxes; create meshes in Blender with named texN materials;
export triangulated OBJ and PNG; compile/re-export to validate scale; implement message
encoding and drawer state script; wire both directions; test wrong sequences, successful
sequence, leaving halfway and save/reload after completion. Do not use the untouched retail
sidecar as the authored room's geometry.

## 3. Pump Gallery — an angled reveal and a safe retreat

**References:** RM_0130 (attract-demo room), four cuts, only fifteen room textures and a
roughly 90x97 span; RM_0060's zombie route coverage. Borrow the low texture count and
segmented traversal, using an original industrial utility space rather than the retail
architecture. Proposed L-shaped footprint 85x95; three cuts; one zombie and two supplies.
Provisional room RM_1170.

```text
z=95   +---------------------------+
       |  Z       pump P     I     |
       |          #######          |
       |          #######     C2   |
z=60   +--------------+            |
                      |            |
                      |  ########  |  raised machinery: visual only
                      |  ########  |
                      |            |
                      | C1         |
                      |     I      |
z=0                   +---DDD---C0-+
       x=0                       x=85
```

C0 at (91,28,-8) covers the first hall z=0..30 and shows its turn. C1 at (92,35,55)
looks diagonally into the elbow, zone x=50..85,z=28..69. C2 at (12,34,96) covers the pump
bay x=0..85,z=66..95, with a triangular transition around the elbow. The plan deliberately
reveals the zombie after the player sees a retreat route, not while a pickup prompt is
open. Keep at least 18 units around the pump and 22 through the elbow. Adjacent areas must
share the same floor number and leave no gap outside the current-cut hysteresis.

Puzzle: a stopped pump hides a fuse holder. Examine it, obtain the spare fuse on the
entry-side shelf, then restore power to unlock the far maintenance hatch. The supply near
the zombie is optional. Gate the far hatch, never the original exit. Phase one has a single
level at y=0; fake raised machinery with colliders instead of importing a sloped walkway.
Add a water effect only after opaque rendering and gameplay are stable.

Assets: modular concrete shell, pump, two pipe sets, railing, fuse box, shelf, warning
sign, two doors, fuse/supply models; 12–16 textures, 2,000–4,000 triangles. A looped pump
sound would require new sound authoring or a proven compatible bank, not merely a texture
or mesh change. Start with a donor bank containing the zombie's sound assets.

Effort: 1 day collision/cameras, 2–3 days art, 1–2 days fuse event, 1 day playtesting;
new sound effects are a separate task.

Build steps: author L-shaped collision and matching camera rectangles first; split mesh
elbow/foreground walls into hideable groups; compile with one donor zombie and independent
item meshes; test chase routing around the pump; implement fuse acquisition/use flags and
return-door state; add triangle camera area and verify from both sides; test leaving before
and after power restoration. Multiple distinct item IDs are supported; duplicate item-ID
texture ownership needs the resolver follow-up before using two identical pickups.

## 4. Dispatch Mezzanine — multilevel stretch goal

**Reference:** RM_1020's eight cuts, five entrances, 22 floor records, 38 route boxes and
entry heights 0/8 show the extra systems involved in vertical navigation. Borrow the
cross-level framing and multiple approaches, not its much larger 36-event-camera setup.
Proposed 80x90 footprint with a 30x45 upper platform, four cuts, two doors, one zombie and
one key item. Provisional room RM_1180. Build only after step/stair events and the map
screen have reliable authoring support.

```text
Ground y=0                         Mezzanine y=8
+----------------------------+     +--------------+
|       counter / sightline  |     | radio P  I   |
|       ############         |     |              |
| Z     ############    stairs---->|  C3          |
|                       C2   |     +------DDD-----+
|        C1                  |       upper exit
| loading crates             |
+--DDD-------------------C0--+
```

C0 covers the ground entrance and counter, C1 covers its far side, C2 frames the staircase
and landing, C3 covers the entire upper platform with the radio and exit visible together.
Suggested mounts: (78,32,-10), (5,30,40), (72,35,62), (42,37,94); zones and angles need
blockout iteration. Each staircase endpoint needs matching floor-specific switch areas,
triggers, landing positions and `grand` heights. No camera cut should occur mid-animation
until the stair transition behavior is understood.

Puzzle: replace a radio battery, hear an original dispatch message, and release the upper
exit. The ground exit stays open. A ground-floor zombie patrol cannot teleport to the
mezzanine: either use proven stair-aware enemy behavior or restrict its routes to floor 0.
Optional ammunition sits behind the counter; the battery is accessible without killing
the enemy. Door entries must carry the intended floor via their y/landing state.

Assets: loading office shell, counter, three crates, staircase/rail, mezzanine structure,
radio with an indicator mesh, two doors, battery and supply props; 18–24 textures;
3,500–5,000 triangles. Split upper floor and railing into camera-hideable groups while
preserving collision on both levels. Use cool overhead light downstairs and a warmer radio
area upstairs, with no fog gradient until both floors render correctly.

Effort: 2–3 days vertical-navigation research/blockout, 3 days art, 2 days event and sound
work, 1–2 days return transitions/save-state QA. Missing message/stair/map import support
can increase this estimate substantially.

Build steps: create a stairs-only room first and prove up/down landing behavior; assign
`grand[2]=0`, `grand[3]=8`; export the visual mesh and author collision independently;
add floor-specific camera areas and entrances; compile and test repeated traversal; add
zombie and restrict route graph; add radio state/event; connect both levels' doors; verify
map lookup and saves on each floor. Do not promote this plan to production while those
engine-specific acceptance checks remain open.

## Shared production gates

1. Check room-slot and persistent-flag allocations against the current mod set.
2. Block out only route, collision, cameras and entrances. Test both directions.
3. Add one enemy and one pickup, then verify persistence and all exits.
4. Implement puzzle conditions with a way back available in every intermediate state.
5. Replace art, recompute hide masks and inspect texture/arena budgets at runtime.
6. Add lighting, fog, sound and map representation; test every cut and save state.
7. Package original source and runtime dependency declarations. Compiled files containing
   donor assets, retail source packages and gameplay frame dumps remain local.
