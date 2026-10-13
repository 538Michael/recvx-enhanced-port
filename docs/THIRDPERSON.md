# Third-person mode (addon, off by default)

## Usage

* Toggle: **F5** (changeable in F1 -> Third person), `--third-person`, or `tps = 1` in
  `<save dir>/recvx.ini`. It can be switched at any time; it only takes over in normal play.
* Controls while it is on (the mouse is captured; F1/F2/F3 free it):

  | Key | Action |
  |---|---|
  | W / S | walk forward / back (back + Shift = quick turn, as in the original) |
  | A / D | turn |
  | Shift | run |
  | mouse | X turns the character, Y looks up/down |
  | right mouse (hold) | ready weapon; the mouse now aims (camera closer, narrower FOV, crosshair) |
  | left mouse | fire while aiming; otherwise action like E |
  | E | action / examine / confirm |
  | Space (hold) | quick knife: knife drawn and raised, left mouse slashes (docs/PORT.md "Quick knife") |
  | Q | cancel |
  | Tab | status screen (inventory) |
  | M | map |

  Arrows, Enter, Backspace, Z/X, 1/2 and gamepads keep working; the letter keys W/A/S/Q lose
  their original pad mapping (R1/Square/Triangle/L1) while third person is on (generally: any
  key bound to a third-person row stops pressing its DS2 button meanwhile).
  All of these keys, a quick-turn key (unbound by default; back + Shift still works) and the
  toggle key can be rebound: F1 -> Controls or OPTION -> PC SETTINGS -> CONTROLS (docs/PORT.md
  "Controls"; ini `key_tps_*`, the toggle key also stays `tps_key`).
* **Mouse/keyboard camera** (F1 -> Third person, PC SETTINGS -> THIRD PERSON, ini
  `tps_free_camera`): **B (behind character)**, the default, is the table above: the camera stays
  behind the character and the mouse turns it. **A (free)**: the mouse orbits the camera around
  the character (yaw, pitch within +-60 degrees, walls as in B) without turning it; W/A/S/D walk
  towards that direction on screen (diagonals too) and the character turns to it smoothly
  (0.3 of the angle per game frame, 95% in 9 frames), Shift runs. Aiming turns the character to
  the camera direction (free aim: at once, through the crosshair as in B; "Game auto-aim": raised
  towards the camera unless the game locks on, then as B) and, released, the camera stays where
  it looked. The quick-turn key does the game's quick turn (S + Shift runs towards the camera in
  A). Arrow keys and the gamepad keep tank controls; while they steer, the camera follows behind
  as in B. Doors, events and cutscenes put the camera behind the character again (both). The
  letters A/B are this option's own, not the game's KEY CONFIG types (A-D, which map the pad
  buttons and stay as they are). The gamepad has no third-person camera choice (no right-stick
  camera), so nothing is shared.
* F1 -> **Third person**: field of view (walking/aiming, horizontal over the 4:3 picture;
  hor+ widescreen adds to the sides), camera distance (walking/aiming), camera height,
  shoulder offset, left/right shoulder, mouse sensitivity X/Y, aiming sensitivity, invert Y,
  crosshair, "Game auto-aim" (off = free aim), "Light and fog" (smooth / per cut, see
  "Light and fog" below; also in OPTION -> PC SETTINGS), toggle key.
* `recvx.ini` keys (lengths in tenths of a game unit; the character is about 17 units tall):
  `tps` (0/1), `tps_fov` (75), `tps_aim_fov` (65), `tps_distance` (140),
  `tps_aim_distance` (100), `tps_height` (165), `tps_shoulder` (35), `tps_left_shoulder` (0),
  `tps_sens_x` / `tps_sens_y` (100, percent; 100 = 0.12 degrees per mouse count),
  `tps_aim_sens` (70, percent of the above), `tps_invert_y`, `tps_crosshair` (1),
  `tps_auto_aim` (0), `tps_atmos` (1 = smooth light and fog, 0 = per cut),
  `tps_free_camera` (0 = B behind the character, 1 = A free),
  `tps_key` (SDL scancode, 62 = F5).
* Testing: `RECVX_TPS_MOUSE="frame:dx:dy,..."` moves the mouse dx/dy counts per game frame
  from that presented frame on; `RECVX_TPS_TRACE=1` logs the free aim each frame and checks
  the game's shot direction against the target, each shot's wall hit against the crosshair
  target ("SHOT"), with "You appear as" the drawn weapon's barrel ("drawn weapon"), and per
  frame the camera ("tps: cam A|B": yaw, pitch, the character's angle, position, modes); `=2`
  also turns the camera to the enemy the game would lock on to (test aid).
  `RECVX_TPS_ORBIT=degrees` turns the view about the player (side views; aim unchanged).
  `RECVX_ATMOS_TRACE=1` logs each game frame's cut, the
  game's fog, lights and filter and what "smooth" shows.

## Limits

* **Room geometry**: rooms are modelled for their fixed cameras. Some corners have no wall,
  ceiling or back side; looking there shows the clear colour. The camera stays inside the
  collision walls, but cannot invent missing geometry. With the back to a wall the camera
  slides forward along its view ray up to the shoulder, so the character can leave the
  picture (the view and the aim stay put).
* Native 3D renderer: walls near the camera used to vanish into the fog colour (screen-linear
  fog across triangles reaching far off-screen); fixed by perspective-correct colour/fog.
* Aim poses: the game has three (straight, up, down at 45 degrees). Free aim keeps the
  straight one and bends spine and chest so the weapon points at the crosshair target at any
  pitch (limit 60 degrees); see "Free-aim pose" below. No laser sight (would need a renderer
  hook).
* Near walls: a wall closer than 40 units on the crosshair ray counts as 40 units away for
  the character's yaw/pitch (else the shoulder offset and the shot origin below the camera
  turn it by tens of degrees); the shot then hits the wall a little beside the crosshair.
  Enemies are aimed at exactly at any distance; walls farther than 40 units too (within 0.1).
* The crosshair marks the screen centre, which is where the shot goes; target selection uses
  the enemies' hit capsules and the room's collision walls (not every visible prop).
* Mouse turning rotates the character directly (standing still it turns without a turning
  animation). Not while climbing stairs/ladders, pushing or in other special moves.
* Events, cutscenes, door transitions, first person/scope, menus: original camera. While an
  event runs without its own camera (rare) the original camera is used too (`mode0 == 7`).
* Messages, examine texts, pick-up prompts and the fades to/from the inventory, map, files and
  options keep the third-person view; the camera holds still (no mouse look) while the game
  is frozen.

An optional over-the-shoulder camera with mouse/keyboard movement and free aim, in the style
of RE4/RE5. With it off, the game behaves exactly as without it: every decomp hook is
`#ifdef TARGET_PC` + `if (tps_on)`, and the new logic lives in `pc/glue/tps_glue.c`
(game thread) and `pc/src/host/tps.h` + the F1 overlay (settings, mouse).

## How the original camera works

* **Cuts** (`cut.c`): each room has `rom->cut_n` camera cuts (`CUT_WORK`, two versions
  `cam[camver]`). `bhCheckCut` (game.c `bhMainSequence`, after drawing) picks the cut whose
  floor area (`bhCheckCutArea`, `CUT_WRK` rectangles/triangles per floor) contains the
  player (`plp->gpx/gpz/flr_no`); `bhSetCut` loads its camera (`cam.px/py/pz`, `ax/ay/az`,
  `ln`, `pers`), fog, filter, the per-cut **hidden room parts** (`hidobj` bit list ->
  `NJS_CNK_OBJECT.evalflags |= 8`) and hidden lights (`hidlgt`). `bhControlActiveCamera`
  follows the player inside a cut (tracking/panning cuts). Rooms flagged `ts_flg & 0x200`
  also change the near/far clip per cut (`event.c bhChangeViewClipRM`, near up to -22).
* Event cameras (`st_flg & 1`: `bhControlEventCamera`, fixed cuts `bhSetFixedCut`), the
  first-person camera (`gm_flg & 0x40`: `bhControlPlEyeCamera`/`bhSetPlEyeCamera`, also the
  sniper scope `st_flg & 0x800000`) and door transitions (`cb_flg & 1`) are separate paths.
* **View** (`camera.c bhControlCamera`, once per frame before drawing): `njSetPerspective
  (cam.pers)`, then `cam.mtx = T(0,0,-ln) Rz(az) Rx(ax) Ry(ay) T(-(p + of))` and the
  derived `cam.vx/vy/vz` (view direction) and `cam.wpx/wpy/wpz` (camera world position).
  Everything 3D uses `cam.mtx`: room/model drawing (`MdlPut.c`), billboards (`effsub*.c`),
  character culling (`pwksub.c bhCheckClipModel/bhCheckClipPoint`), 3D sound
  (`sdfunc.c`, `cam.ay`, `cam.wpx`).
* **Projection** (`ps2_NaView.c`): `njSetPerspective(ang)` sets `_nj_screen_.dist =
  320 / tan(ang/2)` (ang = horizontal FOV over the 640-wide 4:3 screen; 65536 = 360°),
  rebuilds the culling cone (`Ps2CalcScreenCone`) and the VU1 projection
  (`fVu1Projection`, aspect `fVu1AspectW`), from which `_Make_ClipMatrix` builds the clip
  volume. The hor+ widescreen work widens clip volume, cone, `fNaViwHalfW` and the
  `PC_CLIP_X0/X1` window by `pc_hor_scale()`; `pc_clip_guard()` reaches into the GS guard
  band. All of this follows whatever `cam.mtx`/`pers` say, so a different camera needs no
  change there.
* The game's own first-person mode is the precedent for a free camera: `bhInitPlEyeCamera`
  un-hides **all** room parts (`evalflags &= ~8`) and sets `njClipZ(-1, -20000)`;
  `bhSetPlEyeCamera` places the camera at the head joint (`owP[5]`) with `cam.ln = 0`,
  `cam.ay = -plp->ay`, `cam.ax = -plp->wax`.

## How the player moves, turns and aims

* Input: host `pc/src/input/pad.c` builds a DualShock 2 report (SDL gamepad + keyboard);
  `ps2_sg_pad.c` turns it into libpad bits, `pad.c bhSetPad` maps those through the key
  type table (`pad_type[sys->keytype]`) into virtual bits `sys->pad_on/pad_ps`:
  `0x1` forward, `0x2` back, `0x4` turn left, `0x8` turn right, `0x10` ready weapon, `0x20`
  / `0x40` aim up / down, `0x100` fire/confirm, `0x200` examine, `0x400` run, `0x1000`
  cancel, `0x2000` map, `0x4000` status screen, `0x8000` options, `0x20000` pause.
* `player.c bhControlPlayer` -> `bhCtrPly_mode0[plp->mode0]` (1 = `bhCPM0_action`) ->
  `bhControlPlayerPad` (tank controls: `mode2` 1/2 turn, 3 walk, 9 back, ...),
  `mode1` 0 = move (`bhCPM1_act_bas`), 1 = weapon (`bhCPM1_act_atk`). Turning adds a fixed
  rate to `plp->ay` (65536 = 360°; forward = `(-sin ay, 0, -cos ay)`, y is up).
* Weapon: `bhCPM2_act_suw` (raise) calls `bhSearchNearEnemy` = **auto-aim**: it sets
  `plp->ayp` and `flg |= 0x100000`, and `bhControlPlayer` then turns `ay` to `ayp` over
  `yrct` frames. Aim pitch is three-way: `0x20`/`0x40` pick the up/down aim motions and
  `plp->waxp = +-8192` (45°), `plp->wax` follows. Firing (`bhCPM2_act_atk`) casts the hit
  along a ray: origin `(px, py + 11, pz) + forward * (3 + r)`, direction `Rx(wax) Ry(ay)`
  `(0, 0, -l)`, tested against enemy capsules (`weapon.c bhCheckGunAtari`); the wall test
  (`bhCheckL2Wall`) runs along the same direction but from the muzzle, the point
  `(0, 0, wp_fps1.z)` of the hand node `owP[9]` (about 3 units above that origin). So free aim
  needs no new hit logic: setting `ay` and `wax` to point at the crosshair target makes the
  game's own ray hit what is aimed at (from the muzzle when the target is a wall, see
  "Free-aim pose").

## What hides room parts

* Per cut `hidobj` (above). `ps2_NinjaCnk.c pc_show_hidden` already overrides it for
  widescreen margins. Third person draws every room part while its camera is active (as
  the game's first-person mode does). Light, fog and filters follow the cut (the cut still follows the player), faded by
  default: "Light and fog" below.
* Per-cut objects and items: room records with `flg 0x40000000` are drawn only in the cuts
  listed in `hide[0..3]` (or in first person); `objitm.c` (`bhDrawObjItm`, `bhDrawObject`,
  `bhDrawSpObject`) also draws them while the third-person camera is active (164 records in
  all rooms, some listed in no cut at all, i.e. first person only).
* Model culling (`fNaViwHalfW`, cone, `bhCheckClipModel`) uses the active view - fine.
* **Known risk**: rooms are modelled for the authored camera angles. Seen from elsewhere,
  some walls may be missing, single-sided (back faces culled), or untextured behind
  furniture; there is nothing to show behind some windows/doors. This is data, not code;
  it cannot be fixed generally. The camera is kept inside the room's collision walls
  (`bhCheckL2Wall`), which avoids most of it: the shoulder point is pulled in sideways if the
  shoulder itself is in a wall, the eye slides forward along the view ray in front of the
  first wall behind the shoulder.

## Design

* The **cut system keeps running** (`bhCheckCut` unchanged): doors, event triggers,
  per-cut lights/fog/filters that depend on `cam.ncut` keep working. Only the view is
  replaced: hook in `game.c bhMainSequence` before `bhControlCamera()` writes the TPS
  camera into `cam.px/py/pz, ax, ay, az, ln, pers` (so `cam.mtx`, `cam.v*`, `cam.wp*`,
  3D sound all agree). Applied only in normal play: not with an event/fixed cut
  (`st_flg & 1`), first person/scope (`gm_flg & 0x40`, `st_flg & 0x800000`), door
  transitions or events (`cb_flg & 5`, `gm_flg & 9`). When it stops applying,
  `gm_flg |= 0x800` makes `bhCheckCut(1)` re-initialise the current cut (the game's own
  re-init path) and the room's clip range is restored.
* **Frozen cut camera**: messages, examine texts, pick-up prompts and the fades to/from the
  inventory, map and options clear `sys->sp_flg 0x40`, which only freezes the cut camera
  (`bhCheckCut` does nothing). The TPS camera stays active then (it used to drop out: the view
  stayed ours but the cut's hidden parts, clip range and model culling applied, so room
  geometry vanished while a text was shown, and the camera snapped behind the player after).
  It holds still (no mouse look) until `sp_flg 0x40` is back.
* **The cut camera underneath**: `bhCheckCut` runs after drawing and leaves the cut's camera in
  `cam` (tracking cuts: `px/py/pz`, `ax/ay` every frame; `bhSetCut`: the view vector `vx/vy/vz`;
  cuts with camera-relative fog, `flg & 0xC0`, 84 of 205 rooms: the eye `wpx/wpy/wpz` every
  frame). The next frame's game logic runs before `tps_camera` and reads them: effects placed
  towards the camera (gore sprites `bhEff_Sub352/354` `-cam.v*`, `bhEff305/306` towards
  `cam.wp*`, explosions/splashes `- 4 * cam.vx`), 3D sound (`sdfunc.c`, `cam.ay`). `tps_after_cut`
  (game.c, after `bhCheckCut`) puts our view back while our camera stays, and clears
  `gm_flg 0x20` ("the view cut", which wipes blood off the lens, `bhEff301` mode 24).
* Hidden parts: `pc_show_hidden` returns 1 for room parts while the TPS camera is active.
* Enhanced-mode interpolation: `pc_scene_id` ignores cut changes while the TPS camera is
  active (the view is continuous).
* Movement: hook `bhSetPad` (before `pad_ps` is derived) ORs virtual bits from W/S/A/D,
  Shift (run), mouse buttons etc.; the host pad drops its letter-key mapping in TPS mode.
  A second hook in `bhControlPlayer` right after the mode handler (before the auto-turn,
  motion and `bhCalcModel`) applies mouse yaw to `plp->ay` and, in aim mode, sets `ay` and
  `wax` towards the crosshair target and cancels the auto-aim turn. A third one replaces the
  `bhCalcModel(plp)` of `bhControlPlayer` (`tps_calc_model`, the free-aim pose).
* Camera A (`g_tps.free_cam`): `tps_pad` turns the W/A/S/D keys into "forward" (0x1) while the
  player is in move mode (`mode1` 0) and not quick-turning (`TPS_K_QUICKTURN`), and keeps the keys;
  `tps_player` turns `ay` towards camera yaw minus the keys' angle (not during the game's own
  turns, `flg 0x100000`; not in special moves, `mode2 > 13`, as B's mouse turn); `tps_camera` adds
  the mouse X to the camera yaw instead of following `ay` (it follows while arrow keys or the
  gamepad press a direction). Aiming takes B's path (`aim_at_crosshair` / the mouse turns the
  character). The pose, aim and co-op code read `ay`/`wax`, not the camera; name tags project
  through `cam.mtx`; nothing is sent differently.
* Camera collision never feeds back into the aim: the crosshair ray is cast from the
  wall-free shoulder point (`view()`), and the camera position depends on the player's
  position only, not on its facing.

## Light and fog

With the fixed cameras each cut's atmosphere changes exactly when the picture cuts. With
ours the cut still follows the player underneath, so light and fog switched while walking.

**What a cut sets** (`cut.c bhSetCut`, `CAM_WRK` of `rom->cutp[ncut].cam[camver]`; census of
cam version 0 over all 205 rooms with `pc/tools/rdx.py`, 191 rooms have more than one cut):

| Setting | Where it acts | Rooms whose cuts differ |
|---|---|---|
| fog on/off (`flg & 8`) | `sync.c bhControlEOR` (njFogEnable/Disable) | 31 |
| fog colour (`fog_col`) | `njSetFogColor` (fog colour of every vertex) | 110 |
| fog range (`fog_nr`, `fog_fr`, linear from -nr to -fr in view z) | `njGenerateFogTable3` | 172 |
| camera-relative fog (`flg & 0x40` near, `0x80` far): range shifted by (eye - cut camera base) along the view, every frame (`bhControlActiveCamera`) | | 82 |
| light set: lights 4.. hidden per cut (`hidlgt` -> `LGT_WORK.flg & 2`); `bhControlLight` takes the first directional light (`lsrc` 2) and the first three point lights (`lsrc` 4, slots 2..4; VU1 lights at most 4) that are on | `light.c bhControlLight` | 181 |
| point lights only if on screen (`flg & 0x10`, margin `frame`; uses `cam.mtx`, so with our camera it also changes as the view turns) | `bhControlLight` | 9 |
| screen filter (vignette, effects 90..92: `flg & 0x8000`, `fil_no`, `fil_rt` = alpha and rotation) | `effsub1.c bhEffFil` | 62 |
| full-screen distortion (`flg & 0x400` -> `cam.flg & 0x20`, `bhEff110`) | | 2 |
| `ts_flg & 0x200` rooms: view clip, clip volume, background colour per cut | `event.c`, `ps2_event.c` tables | (third person keeps its own clip) |

Room ambient (`amb_*`) and lights 0..3 (global: muzzle flash, lighter, ...) do not change
per cut. Trace, room 0/6 walking from cut 0 into cut 1 (`RECVX_ATMOS_TRACE=1`, per cut):
fog 100/-140 -> 50/-130 and lights {5, 8} -> {6, 9} in one frame (frame 528); room 0/1:
fog 07060f 128/-110 -> off -> f5ffff -25/-770; room 0/2 (outdoors): fog 128/-90 -> -20/-120
camera-relative; room 0/7: filter 2 (alpha 0x40) on in cut 0 only, fog 0a0f13 -> 1d1d24.
Camera-relative cuts with our camera (per cut): the range moved by up to 62 (0/7), 77 (0/2)
and 47 (0/8) units from the cut's own while walking inside one cut.

**Smooth (default)**, F1 / PC SETTINGS "Light and fog", ini `tps_atmos` (1 smooth, 0 per
cut = the game's values as before). `tps_atmos` (`game.c` after `bhControlLight`, before
drawing) and `tps_filter` (`bhEffFil`) keep the game's per-cut values as the target and fade:

* A change in a frame where the cut changed (`cam.ncut`/`camver`) fades; any other change
  (scripts, flickering/thunder lights, muzzle flash, a linked actor dying, fog set by an
  event) applies at once, as in the game. Lights 0..3 are never faded.
* Lights: a weight per room light, 0 -> 1 in 24 game frames (0.8 s). The slots are filled
  from the lights' values the game computed this frame (`lp->w*`, flicker included), point
  lights scaled by their weight, directional lights blended (the first one on wins, as in
  the game: weight x (1 - weights before it)). A point light fading in waits for a free
  slot (never more than three in use, so no slot changes hands mid-fade); while one waits
  the outgoing ones fade twice as fast, a short dip instead of a pop. The on-screen test
  of `flg & 0x10` is not applied (it made lights depend on where our camera looks).
* Fog: density (the table scaled towards "no fog") fades for on/off; range and colour ease
  (8% per game frame, 95% in 1.2 s); the colour follows the denser fog faster (else a thin
  bright fog and a dense dark one mix to a grey haze). The range is the cut's own one
  measured from our camera: the camera-relative shift measured our eye against the cut
  camera's base position along our view, so the fog came and went as the camera turned.
* Screen filter: alpha fades in/out over 0.8 s (also between two alphas of one filter).
* Back to the game's own values when our camera stops (events, doors, first person): the
  lights are recomputed every frame, the fog is re-applied (`gm_flg 0x10`).
* Not faded: the distortion effect of 2 rooms, the per-cut background colour of
  `ts_flg & 0x200` rooms (only visible where geometry is missing).

Fixed camera (third person off): the hooks are `if (tps_on)`, nothing changes.

Verified headless (320x240, new game, `RECVX_TELEPORT`, `RECVX_NOCLIP=1:1`, three
`RECVX_WARP` starts per room and W held, frame dumps every 4th game frame, the same run per
cut and smooth; scripts, contact sheets, brightness plots and traces in `scratch/atmos/`,
not committed): rooms 0/1 (fog on/off, dark/bright fog), 0/2 (outdoors, camera-relative fog),
0/6 (light set and fog range), 0/7 (camera-relative fog, screen filter), 0/8 (outdoors,
camera-relative fog), 15 walked cut changes. Per cut the fog, the lights on the character and
the vignette switch from one dumped frame to the next; smooth they change over 0.8-1.2 s
with no step. Game logic is the same in both (identical position traces). Per cut is
byte-identical to the main build with third person (0/7, 279 dumps), and the fixed camera
to the main build (0/6 walk, 139 dumps). Known: a point light that waits for a slot shows as
a short dip (0/6 cut 0 -> 1, about 0.6 s); after a warp (not possible in play) the old
cut's light/fog fades out on the new spot.

## Effects (blood, hits)

What was checked for blood sprays, hit sparks, muzzle flash, gore, bloodstains, shell casings
and knife hits (`effect.c`, `effsub*.c`, `zonzon*.c`, `weapon.c`, enemy damage code):

* Creation does not depend on the camera: enemy damage (`bhEne01_SetBlood`, `bhEne_SetBlood*`,
  `bhSetGunSplash`) spawns the same effects whatever the view. Headless, a zombie shot from the
  same spots with the fixed and the third-person camera spawns and queues the same effects
  (`RECVX_EFF_TRACE=1`: blood 5, particles 302, muzzle flash 3, smoke 2, gore 362/363/370/372).
* Drawing uses the current view (`cam.mtx`), billboards included. The camera-relative parts
  were the cut camera's values seen by game logic (previous section, fixed) and the lens blood
  (`bhEff301`: a spray crossing the camera plane near the lens becomes a 2D splat; it follows
  the TPS camera, so blood sprayed back at the player can land on the lens).
* Culling: `njDrawTexture3DEx` (most effect quads) drops a quad with a vertex behind the
  camera or all vertices outside the screen (`_Clip_Screen`). The screen window was the 4:3
  one, so in widescreen effects in the side margins vanished while models there were drawn;
  it is now widened by `pc_hor_scale()` like the clip volume (4:3 unchanged). Particles
  (`njPtclDrawSprite`) and the ordering table cull nothing.
* Verified headless (4:3, 640x480): a spawned zombie (`RECVX_SPAWN=40:1@25`) shot with the
  handgun and the shotgun from 4 spots in rooms 0/6 and 0/7 (different cuts, facing towards
  and away from the cut cameras), free aim locked on (`RECVX_TPS_TRACE=2`). Blood was visible
  in every case: compared with a run that drops blood and gore from the draw lists, 17-33 of
  309 dumped frames differ at the zombie. The fixed camera spawns the same effects. No case
  without blood was found in these rooms; the fixes above remove the camera dependencies
  that were found in the code.
* Messages: room 0/6, "Nothing useful here" examined at (4, 66): before, the cut's hidden
  parts (the wall right of the player) went black while the text was shown.

## Free-aim pose

* The weapon motions (file `wpnr_no + ply_id*30 + 20`, motions 100+) have the same layout
  for all four characters and all weapons [verified, parsed from the disc]: 100 raise, 101 fire,
  102 second fire/pump (assault rifle, twin weapons), 103 knife slash, 104 hold, 116 reload,
  117..119 twin-weapon variants; up/down variants at +5/+10 (106..114). Exceptions: rocket
  launcher (20) has no up/down motions, linear launcher (18) no up/down fire, Chris's and
  Wesker's sniper rifle (13) no fire motion (scope), Claire's grenade launcher "down hold" is
  a 140-frame motion.
* So free aim does not use them: `tps_pad` keeps the straight pose (clears the up/down bits
  while aiming) and `tps_calc_model` runs `bhCalcModel`, turns the world matrices of nodes
  2 (spine) and 3 (chest) by half the pitch each about the model's x axis (row 0 of
  `plp->mtx`), marks both `O_WORK flg 1` (matrix set externally) and runs `bhCalcModel`
  again, so the arms, head, hair link and weapons follow. The node angles (the motion state
  the next frame's motion blend starts from) are never touched, so there is no feedback into
  the game's animation. The bend eases in/out (0.35 per frame) with raising/lowering.
* Pitch used: from the weapon hand (node 9) of the unbent pose to the target,
  not `wax` (the game's shot starts at 11 units and needs a steeper angle for close targets).
  Muzzle flash, knife hit points and attachments use the bent matrices; the shot ray is the
  game's (`wax`).
* **Shots on walls** (`aim_at_crosshair`): `wax`/`ay` were aimed from the enemy-test origin
  (11 units up), but walls are tested from the muzzle, about 3 units higher: Claire's shots hit
  walls 3 units above the crosshair at any distance (2 degrees at 70 units), and where the
  higher ray met another surface first, 9..10 units off (6..7 degrees). When the crosshair
  target is a wall (no enemy on the crosshair ray), the aim now starts at the muzzle (last
  tick's hand, turned with the character's turn to the target). Enemies keep the 11-unit
  origin.
* **"You appear as" another model** (docs/ACTORS.md section 8): the weapon is drawn in the
  model's hand, which a Tyrant has 6 units above Claire's, so its weapon pointed 6 units over
  the target (parallel to Claire's, 5 degrees). The bend's pitch is taken from where the weapon
  is drawn: `actor_aim_offset` gives the drawn hand minus the real one, kept in the frame of the
  real chest (node 3; the arms are rigid to it in the straight aim pose, so the offset does not
  change with the bend). The shot itself is the real skeleton's: the model's hands are put back
  into the source's nodes before the next tick's player control (`actor_hands_back`); before,
  the wall test started at the model's muzzle (the Tyrant's shots 9.6 units high).
* Measured headless (`RECVX_TPS_TRACE=1`: per shot the game's muzzle, direction and wall hit
  against the crosshair target; per aiming frame how far the drawn weapon's barrel, the hand's
  x axis from the muzzle, passes from the target). Room 1/2 facing the far wall (70 units),
  handgun, pitch up / down / level, one shot each; before = the same build with the fixes
  switched off:

  | Model | Wall hit vs crosshair before | after | Drawn barrel off the target before | after |
  |---|---|---|---|---|
  | Claire (no appearance) | 3.3 / 10.6 / 9.1 units (2.1 / 6.6 / 6.5 deg) | 0.06 / 0.07 / 0.06 (0.04 deg) | - | - |
  | Tyrant (hand +6.1 over Claire's) | 9.6 / 9.3 / 12.2 (6.2 / 5.8 / 8.6 deg), 8.7..9.6 high | 0.07 / 0.07 / 0.06 | 6.96 (4.84 deg) | 0.67 (0.47 deg) |
  | Alexia's first form (+0.65) | 3.9 / 10.8 / 9.5 | 0.06 / 0.07 / 0.06 | 1.23 (0.88 deg) | 0.37 (0.26 deg) |
  | Alexia Ashford (+0.65) | 3.9 / 10.8 / 9.5 | 0.06 / 0.07 / 0.06 | 1.23 (0.88 deg) | 0.37 (0.26 deg) |
  | Zombie (prisoner; Chris's motions, +1.1) | 4.4 / 11.0 / 9.2 | 0.07 / 0.07 / 0.06 | 1.74 (1.22 deg) | 0.48 (0.35 deg) |
  | Rodrigo (+1.0; before: hand unmapped) | 3.3 / 10.6 / 9.1 | 0.07 / 0.07 / 0.06 | 0.87 (0.62 deg) | 0.62 (0.44 deg) |

  (barrel: median over 255 aiming frames; the rest is the bend's own approximation.) Test aid
  `RECVX_TPS_ORBIT=degrees` turns only the view about the player (side views for contact
  sheets; the camera then ignores walls), the aim keeps the unturned view.
* Verified headless (room 0/4, pitch +42/-48/-3, firing at each): Claire with knife, M93R,
  shotgun, assault rifle, grenade launcher, bow gun, rocket launcher, items 32/33/142 (single
  and twin handguns, twin SMGs); Chris and Wesker with the Glock, Steve with the twin SMGs,
  Claire's second costume. Sniper rifle and linear launcher go to the game's scope after
  raising (first person, `st_flg & 0x800000`), so the bend does not apply to them.
* Mouse: SDL relative mode while TPS is on and no overlay window is open; deltas
  accumulated on the main thread, consumed once per game frame.

## Milestones

1. **M1 camera**: over-the-shoulder camera following the player, all room parts drawn,
   camera kept inside walls, original camera for events/doors/menus. Toggle key, flag, ini.
2. **M2 movement**: W/S forward/back, A/D turn, Shift run, mouse X turns the character, mouse
   Y pitches the camera; E action (Space was a second action key until the quick knife took it),
   Tab status, M map.
3. **M3 aim**: right mouse = ready weapon, left = fire; crosshair; free aim (yaw/pitch)
   through the game's own hit ray; upper body bent to the pitch; option to keep
   the game's auto-aim.
4. **M4 options**: F1 "Third person" section: FOV, aim FOV, shoulder side/offset, distance,
   height, mouse sensitivity X/Y, invert Y, aim sensitivity, crosshair, auto-aim, toggle key.
