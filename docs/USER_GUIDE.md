# User guide

How to start the game, configure it and play. Building: [BUILDING.md](BUILDING.md).
Online co-op: [COOP.md](COOP.md).

## First start

You need your own disc image of *Resident Evil Code: Veronica X* (PS2, NTSC-U, **SLUS-20184**)
as an `.iso` file. Other regions and releases are not supported. The image is read in place;
nothing is extracted or copied.

- Release downloads: unpack the archive and run `recvx.exe` (Windows) or `./recvx` (Linux).
  Settings, saves (`save/`), `mods/` and logs live next to the program, wherever it is started
  from; a `save/` folder in the current directory, or `--save-dir`, takes precedence.
- Start `recvx` (Linux) or `recvx.exe` (Windows). The first start opens a file selector (on
  Linux the desktop portal or `zenity`); pick the image. The choice is saved in `recvx.ini`
  (`iso = ...`) and the selector only comes back if that file disappears.
- Alternatively pass the image on the command line (`recvx /path/to/game.iso`), set
  `RECVX_ISO=/path/to/game.iso`, or on Windows drop the ISO onto the exe. An image given this way
  always wins over the saved one.
- The Windows build is a console program: the console window shows the log.

### Command-line options

| Option | Effect |
|---|---|
| `--save-dir DIR` | where saves and `recvx.ini` go (default `./save`) |
| `--size WxH` | window size (default 1280x960) |
| `--fullscreen` | start in fullscreen |
| `--enhanced` / `--vanilla` | display-rate frames on / off |
| `--skip-doors` | room transitions without door, stairs and ladder animations |
| `--skip-intro` | boot straight into the title menu |
| `--third-person` | start with the over-the-shoulder camera ([THIRDPERSON.md](THIRDPERSON.md)) |
| `--coop-host PORT`, `--coop-join HOST:PORT#CODE` or `ROOM`, `--coop-server ADDR`, `--coop-conn auto\|direct\|server`, `--coop-pw PASSWORD` | online co-op ([COOP.md](COOP.md)) |
| `--log[=FILE]` | copy all output to a log file (default `recvx.log`), with a backtrace on a crash |
| `--profile[=FILE]`, `--profile-samples=FILE` | performance measurements ([ARCHITECTURE.md](ARCHITECTURE.md#profiling-and-logging)) |
| `--quiet` | less console output |
| `--version` | print the version and exit (also the first line of the log and the F1 window title) |

Command-line options override the saved settings for that run.

## Saves and settings files

Everything lives in the save directory (`./save` relative to the current working directory,
or `--save-dir`):

| Path | Contents |
|---|---|
| `save/saves/` | your saves as files (the default save system, see below) |
| `save/saves/system.dat` | the game's system data: OPTION settings, unlocked extras, best times |
| `save/card0/` | memory card slot 1 (the game's own save files, used by the Memory card save system); slot 2 is empty |
| `save/recvx.ini` | all settings, key bindings, the disc image path and your co-op identity |

`recvx.ini` and the saves are written atomically (a crash never leaves a half-written file). Two
copies of the game with different save directories keep separate settings and saves; this is how
local co-op on one PC works ([COOP.md](COOP.md)).

### Saves

The typewriter's save screen and LOAD GAME look as on the PS2, but by default (**Save system:
Native files**) they list save files instead of the memory card's 15 slots, without the memory
card screens ("which memory card slot", "checking memory card", "not enough space"):

- Each save is its own file in `save/saves`, named by its number and what the menu shows, e.g.
  `001_Claire_Prison-front.sav`, `002_Chris_Ant-Base-CR.sav`, ... `1000_...` There is no limit.
  The list scrolls through all of them; saving offers an empty entry with the next number at the
  end of the list (the cursor starts there), or you overwrite an existing save as before. Loading
  starts on the save written last.
- The file name is only for you: renaming the part after the number does no harm. Deleting a file
  deletes that save. Back them up by copying the folder.
- A file is the memory card's save file with a 64-byte header in front (game, time, play time,
  character, room), so a save can be turned back into a card save by dropping the first 64 bytes
  ([ARCHITECTURE.md](ARCHITECTURE.md#save-system)).
- **First start with native files**: your memory card saves are copied into `save/saves` (slot n
  becomes save number n) together with the system data and co-op partner files; the log says how
  many. The memory card files are kept as they are.
- **Save system: Memory card (original)** (`save_system = 1`) brings back the emulated memory card
  with its screens and 15 slots, using `save/card0` exactly as before. The setting applies from the
  next save or load screen. Saves made in one system are not seen by the other; **Import memory
  card saves** (F1 / ENHANCED SETTINGS GAMEPLAY) copies the card's saves into the save files again (ones
  already there are skipped).

### Back to the title

During a game, the OPTION screen (Select) has **RETURN TO TITLE** on DEFAULT's row, above ENHANCED
SETTINGS: select DEFAULT and press right, or click it. It asks "Return to the title screen?" with
NO selected, so a stray press does nothing; choose YES (left/right, then confirm) to go back to the
title menu, as after a game over. Anything not saved is lost; changed options are kept. Cancel or a
right click answers NO. It works in the Battle Game too. In co-op, leaving ends the session: the
partner plays on alone. The OPTION screen of the title menu does not have it.

## Settings

All PC settings are available in two places that edit the same values:

- **OPTION -> ENHANCED SETTINGS**: in the game's own OPTION screen (title menu, or Select in
  game) next to EXIT: select EXIT and press right (or click it). Drawn in the game's style, with
  the pages DISPLAY, GAMEPLAY, CONTROLS, THIRD PERSON, AUDIO, APPEARANCE, CO-OP and EXTRAS. Left/right or
  L1/R1 on the page row switch pages, up/down (or the mouse wheel) choose a setting, left/right
  change it, cancel or a right click goes back. Long pages scroll; the bottom lines explain the
  selected setting, or why it cannot change right now. "(next start)" marks a setting that takes
  effect when the game starts again.
- **F1**: the settings overlay, in the same order (sections Display, Gameplay, Controls, Third
  person, Audio, Appearance, Co-op, Performance). The co-op and cheat settings live in their own
  windows (F3, F2); F1 shows a few of them.

Every setting of the table below is in both. Changes apply at once and are saved shortly after.
The defaults of a first start are the PC-friendly ones: enhanced mode, widescreen, native
renderer, skipped intro and door animations (Skip door animations: Instant). Turn these off for a presentation closer to the PS2.

### Display

| Setting | ini key | Values (default in bold) |
|---|---|---|
| **Window** | | |
| Fullscreen | `fullscreen` | also F11 |
| Window size | `window_w`, `window_h` | presets 1280x960 to 3840x2160 (incl. 3440x1440), or any size by resizing the window |
| VSync | `vsync` | **on**, off |
| Frame rate limit | `fps_limit` | **Off (display rate)**, 30, 60, 90, 120, 144. Caps the images shown per second, in-between frames included; the game itself always runs at 30 |
| Show frame rate | `show_fps` | **on**: counter at the top right |
| **Picture** | | |
| Aspect ratio | `aspect` | 4:3 (original), Stretch to window, **Widescreen (hor+)**: 3D fills the window, 2D and menus stay 4:3 in the middle |
| Internal resolution | `scale` | **Auto** (follows the window) or 1x-8x of 640x448 |
| Display-rate frames | `interpolate` | "Enhanced mode": in-between frames blended from the last two game frames, up to the display's refresh rate. The game logic stays at its original 30 FPS, so gameplay is unchanged. **On**; off is "vanilla" |
| Frame blend (trail) | `frame_blend` | the PS2's motion trail (each frame mixed over the previous one). Auto, **Off**, On; Auto is on in vanilla and off with display-rate frames |
| **Widescreen** | | |
| Cutscene bars, Camera vignette, Screen effects | `cinema_bars`, `screen_filter`, `screen_effects` | Original (4:3), **Stretch to width**, Off: how these full-screen layers behave in widescreen |
| Movie zoom | `movie_zoom` | **off**; on: in widescreen the movies are enlarged, never stretched, until the black bars above and below them leave the screen or the picture reaches the window's sides. Nothing of the picture is cut off: on 16:9 thin bars stay (the movies are wider than 16:9), on ultrawide the movie fills the height. No effect at 4:3 |
| **Fog and brightness** | | |
| Fog intensity | `fog_intensity` | 0% to 100%, **100%** (as on the PS2): how thick the game's distance fog is, including the black fog many rooms use as darkness (lower values let the unlit cell at the start show more of the room outside). The fog everywhere becomes that of something the value's % as far away: 50% doubles the fog's distance (the fog density x 2.00 of fmil95's PS2 patch), 0% removes it. Applies at once |
| Brightness | `brightness` | -50 to +50, **0**: lifts or lowers the dark end of the picture |
| Contrast | `contrast` | 50% to 150%, **100%**: spreads or narrows the tones around mid-grey |
| Gamma | `gamma` | 0.5 to 2.0, **1.0** (in the ini in tenths, 5 to 20): higher brightens the mid-tones, black and white stay |
| Reset picture calibration | | brightness, contrast and gamma back to neutral |
| **Renderer** | | |
| Native 3D renderer | `native_3d` | **on**: 3D models projected and clipped by the GPU instead of the emulated VU1/GS path |
| Native textures | `native_tex` | **on**: textures uploaded once instead of on every switch |
| Cull off-screen models | `model_cull` | **on** (as on the PS2); off stops models popping at the screen edges |

Brightness, contrast and gamma change the game's picture (menus and movies included), not the F1
overlay; at the neutral values the picture is exactly the original. While one of them is chosen,
both menus show a test pattern: a black strip and a white strip with five grey squares each (in F1
in the setting's tooltip). Turn Brightness until the left square on the black strip is barely
visible, and Contrast until the right square on the white strip is barely visible.

### Gameplay

| Setting | ini key | Effect |
|---|---|---|
| Skip intro | `skip_intro` | on: boot straight into the title menu (next start) |
| Skip door animations | `skip_doors` | the door, stairs, ladder and lift animations between rooms. Off (0): always played, as the original; **Instant** (1): the game's load screen instead (no animation, so no door sounds); Skippable (2): the animation plays, and any key, mouse button or gamepad button pressed while it runs ends it and the next room follows at once. The press that opened the door does not count; a press made while the next room is still loading ends the animation as soon as the room is ready. In co-op each player's own choice counts for its own doors; following the host always uses the load screen |
| Battle Game unlocked | `battle_unlocked` | **on**: the title menu offers BATTLE GAME without a finished playthrough (Claire to start with; the other characters unlock as in the original). Saves and system data are not changed; off: only after a cleared game, as the original |
| Quick knife | `quick_knife` | on: hold the quick-knife key to draw the combat knife without opening the inventory; release to get the previous weapon back |
| **Saves** | | |
| Save system | `save_system` | **Native files** (0): every save a file in `save/saves`, no limit, no memory card screens; Memory card (original) (1): see [Saves](#saves) |
| Import memory card saves | (action) | copies the memory card's saves into the save files |
| No ink ribbons needed | `no_ink_ribbon` | **off** (original); on: typewriters save without an ink ribbon and use none up. In co-op the host's setting counts for both ([COOP.md](COOP.md#saving-without-ink-ribbons)) |
| **Enemies** | | |
| Extra enemies | `enemy_extra` | **Off**, up to 1-3 more per room, of the types the room already has; in co-op the host's |
| Rearranged enemies, Enemy randomizer, Enemy seed | `enemy_place`, `enemy_random`, `enemy_seed` | optional enemy changes; bosses stay as they are ([ENEMIES.md](ENEMIES.md#enemy-options)) |
| Randomizer variety | `enemy_variety` | **Wild** (1): any enemy kind that fits the spot, a different one for each enemy of a room where possible, Tyrants, Giant Black Widows and bats included; Mild (0): two common kinds per room |
| Randomize event enemies | `enemy_random_events` | **on**: enemies a room shows later become other kinds too, and the ones a scene animates get another look of their own kind (the scene still plays); off: only free-roaming enemies change |

### Controls

Input source and Gamepad (see [Input source](#input-source-per-window)), **Mouse in menus**
(`menu_mouse`, **on**: point and click in the inventory, item box, map, files, OPTION and these
menus; right click goes back; drag to rotate and wheel to zoom when examining an item), **Fixed
camera controls** (`fixed_cam_controls`: **Classic (tank)** 0, Modern (camera-relative) 1; see
[Modern controls](#modern-controls-fixed-cameras)), then the bindings ([Rebinding](#rebinding)).

### Third person

Over-the-shoulder camera with mouse/keyboard or twin-stick gamepad controls and free aim,
toggled with **F5** (`tps`). The page holds field of view, camera distance and height, shoulder
side, mouse sensitivity, invert Y, crosshair, the game's auto-aim and the gamepad settings
([Third person with a gamepad](#third-person-with-a-gamepad)). Settings worth knowing:

- **Mouse/keyboard camera** (`tps_free_camera`): **B** (default) keeps the camera behind the
  character and the mouse turns it; **A** lets the mouse orbit the camera freely and W/A/S/D walk
  relative to the camera. This is unrelated to the game's KEY CONFIG types.
- **Light and fog** (`tps_atmos`): Per cut (original) or Smooth (fades over a second when the
  room's camera cut changes).
- **Gamepad controls** (`tps_pad_controls`): **Modern (twin-stick)** (default) or Classic (tank
  controls through the game's buttons, the camera behind).

Details and limits: [THIRDPERSON.md](THIRDPERSON.md).

### Audio

Music and voice (`music_volume`) and Sound effects (`sfx_volume`), 0-100%.

### Appearance

Your model, in single player and co-op (see [ACTORS.md](ACTORS.md)):

| Setting | ini key | Effect |
|---|---|---|
| You appear as | `coop_char` | **As played**, or another player file, story cast member, creature or zombie; in co-op your partner sees it too (also in F3 and the lobby's MODEL row) |
| Use gender-matched animations | `appear_gender_anim` | **on**: a man shown while you play Claire moves with Chris's motions, a woman shown while you play a man with Claire's |
| Your model in cutscenes | `appear_in_cutscenes` | **on**; off keeps the real Claire or Chris in cutscenes, with their own hands, faces and voice, and shows your model only while you play. In co-op each player chooses for their own screen |
| Talking faces in cutscenes (experimental) | `appear_cutscene_face` | **off**; on: your model in cutscenes moves its mouth with the lines |

### Co-op

The CO-OP page (also in F3) holds your own choices: name tags, the ping display, follow host; the host's game rules: loot
mode (Shared: each item lies there once and whoever takes it has it, so you split the supplies; key
items go to both. Individual: each player takes their own copy), "Important items for both" (with
shared loot, on by default: weapons and ink ribbons lie there for each player), "Synchronized cutscene skip" (a cutscene both watch is skipped once both pressed skip),
freezing enemies while trading, remembering partner inventories; and the connection: mode, host
port and UPnP. See [COOP.md](COOP.md#settings). The address, room code, server, name, password
and the save to start from or join with are chosen in the lobby (MULTIPLAYER) or F3.

## Controls

Keyboard defaults (all rebindable):

| Key | PS2 button |
|---|---|
| Arrow keys | D-pad |
| Z / X / A / S | Cross / Circle / Square / Triangle |
| Q / W | L1 / R1 |
| 1 / 2 | L2 / R2 |
| Enter / Backspace | Start / Select |
| Space (hold) | Quick knife |
| T | Co-op trade request (next to the partner) |

Any gamepad SDL supports works as a DualShock 2 (south/east/west/north = Cross/Circle/Square/
Triangle, shoulders L1/R1, left trigger L2, right stick click R3). The right trigger is the
quick knife and the left stick click the trade request, since those buttons have no game action.
The game's own key types (OPTION -> KEY CONFIG) still decide what each PS2 button does.

Third person (keyboard and mouse): W/S/A/D move and turn, Shift run, E action, Q cancel,
Tab inventory, M map, right mouse aim, left mouse fire, R reload ([Reload](#reload)). With a
gamepad see [Third person with a gamepad](#third-person-with-a-gamepad).

### Third person with a gamepad

THIRD PERSON page: **Gamepad controls** = **Modern (twin-stick)**, the default (ini
`tps_pad_controls = 1`; 0 = Classic, the tank controls of the fixed cameras with the camera
behind). As in the Resident Evil 2 and 4 remakes:

| Gamepad (Xbox / PlayStation) | Action | ini |
|---|---|---|
| Left stick | walk where it points seen from the camera; pushed far it runs | |
| Right stick | turn the camera; while aiming, aim (the crosshair) | |
| LT / L2 (hold) | aim: the character faces the camera's direction, the camera moves to the shoulder | `pad_tps_aim` |
| RT / R2 | fire while aiming; otherwise action | `pad_tps_fire` |
| A / Cross | action: doors, items, examine, confirm | `pad_tps_action` |
| B / Circle | cancel | `pad_tps_cancel` |
| X / Square (hold) | run | `pad_tps_run` |
| Y / Triangle | inventory | `pad_tps_inventory` |
| View / Share (Back) | map | `pad_tps_map` |
| LB / L1 (hold) | quick knife: knife drawn and raised, RT slashes, the weapon before on release | `pad_tps_knife` |
| RB / R1 | reload ([Reload](#reload)) | `pad_tps_reload` |
| D-pad down | quick turn (the camera swings round with the character) | `pad_tps_quickturn` |
| Right stick click | camera behind the character | `pad_tps_recenter` |
| Left stick pulled back + run | quick turn | |
| D-pad up / left / right, Start | as before (d-pad: tank turn and walk; Start: options) | |

Reload sits on RB as in Resident Evil 4 and the RE4 remake's type C (the remakes' other types use
X / Square, which runs here as in the game's type A); the quick turn, which the stick gesture also
does, moved from RB to the d-pad's down. A `recvx.ini` from before the reload button that still
has the quick turn on RB gets the new default.

- **Moving**: the character turns quickly towards the stick's direction and walks; pointing more
  than 100 degrees away turns it on the spot first. After a quick turn or "camera behind" the
  held stick keeps its meaning until it turns or is let go, so you keep running the way you
  were going.
- **Aiming** is free aim, exactly as with the mouse (the same crosshair, shot and pose). With
  **Game auto-aim** on, the gamepad gets a gentle aim assist instead of the lock-on: near an
  enemy the aim slows down and drifts slightly towards it while you move the stick.
- **Menus, messages, events**: the gamepad works as the game's KEY CONFIG type sets it (as in
  Classic); the third-person buttons only play while the third-person camera does. A button held
  through the change (the one that opened the inventory, say) does nothing until let go.
- **Mouse and keyboard together** (input source Auto): the camera follows the device used last;
  the gamepad's right stick turns it freely, the mouse as set in "Mouse/keyboard camera".
- Settings (THIRD PERSON page, GAMEPAD): stick camera speed X / Y (`tps_pad_sens_x`,
  `tps_pad_sens_y`, 100%: 180 / 120 degrees a second fully pushed), stick aiming speed
  (`tps_pad_aim_sens`, 50% of that), invert Y (`tps_pad_invert_y`), left and right stick dead
  zones (`tps_pad_dead_move` 20%, `tps_pad_dead_look` 15%). The right stick's response is finer
  near the centre.
- The third-person gamepad buttons are their own bindings: rebinding one (CONTROLS, Gamepad,
  "Third person (gamepad, Modern)") leaves the fixed-camera binding of the same button alone.

### Reload

Reloads the equipped weapon from the ammo in the inventory without opening it, as the game does
by itself when you fire with an empty magazine: the same motion, sound and magazine size, and it
takes the first ammo in the inventory that reloads that weapon, as combining them in the
inventory would (a magazine that fills up leaves the rest in the stack; a stack used up leaves
the inventory). It works whenever the magazine is not full and matching ammo is there: while
aiming (also through the sniper rifle's scope) and with the weapon lowered while standing,
walking or running (the weapon comes up for the reload and goes down again; holding aim keeps
it up). Without matching ammo, with a full magazine, unlimited ammo, during events, menus,
doors or when grabbed it does nothing. Weapons the game does not reload by itself, the grenade
launcher (its rounds are combined in the inventory) and the knife, are left alone; the bow gun
reloads the kind of arrows it holds. The sniper rifle and linear launcher reload only through
their scope (their reload ends in it).

Defaults: R in third person, RB / R1 with the Modern third-person gamepad controls. The fixed
cameras and the Classic third-person gamepad have no free button for it: bind the "Reload" row
(CONTROLS, `key_reload` / `pad_reload`, unbound by default), which works in every mode. In co-op
it is your own action: the partner sees the reload and the ammo stays in your inventory.

### Modern controls (fixed cameras)

F1 -> Controls or ENHANCED SETTINGS -> CONTROLS: **Fixed camera controls** = Modern
(camera-relative), ini `fixed_cam_controls = 1`. Off (Classic, tank controls) by default. Like the
alternate controls of the Resident Evil HD remasters:

- The **left stick** or the **d-pad** (arrow keys on the keyboard) points where the character
  walks on screen; it turns there quickly (180 degrees in about 0.3 s) and walks. Pushed far
  (70%) the stick runs; the run button runs as before (on the keyboard and d-pad, which have no
  half push). Pointing more than 100 degrees away from where the character faces turns it on the
  spot first.
- **Camera cuts**: while the direction is held through a cut, it keeps meaning what it meant in
  the previous camera, so the character keeps going the same way. The new camera takes over when
  the direction turns by more than 35 degrees or is let go.
- **Aiming** works as in Classic: holding the aim button stops, the stick or d-pad turns and aims
  up / down, the game's auto-aim and the quick knife are unchanged. Letting go of aim returns to
  modern movement.
- **Quick turn**: bind the "Quick turn" row (unbound by default; it holds back, then run). While
  it is held the stick does not walk.
- Stairs, steps and ladders still start with the action button; while climbing or pushing a box
  the direction only counts as "forward" when it points roughly where the character faces.
- Third person keeps its own controls; this option only applies to the fixed cameras. In co-op
  it changes only your own player.

Hotkeys:

| Key | Action |
|---|---|
| F1 | settings overlay |
| F2 | cheat menu |
| F3 | co-op window |
| F5 | third-person camera on/off |
| F11 | fullscreen |
| Page Up, F / Page Down | noclip fly up / down (when flying is on; R reloads now) |

### Rebinding

F1 -> Controls (tabs Keyboard / mouse and Gamepad: click a binding, then press the key; right
click clears) or OPTION -> ENHANCED SETTINGS -> CONTROLS (DEVICE picks the set). Each row has two
slots. While waiting for a key, Esc cancels and Delete clears. A key already used by another row
is swapped to that row. Gamepad buttons are named after the connected pad (Xbox, PlayStation,
Nintendo or generic).

Bindings are saved in `recvx.ini` as `key_<row>` and `pad_<row>` (second slot `<row>_2`):

```ini
key_cross = Z
key_tps_run = Left Shift
key_tps_run_2 = Right Shift
key_tps_aim = Mouse Right
pad_cross = south
pad_l2 = +lefttrigger
pad_quickturn = -lefty
```

Keys use SDL key names (`Z`, `Return`, `Left Shift`, `PageUp`), mouse buttons
`Mouse Left/Middle/Right/X1/X2`. Gamepad inputs are positional (`south` = Xbox A = PlayStation
Cross, `east`, `west`, `north`, `back`, `start`, `guide`, `leftshoulder`, `rightshoulder`,
`leftstick`, `rightstick`, `dpup`, `dpdown`, `dpleft`, `dpright`, `misc1`, `paddle1`-`paddle4`,
`touchpad`) or an axis half (`+lefttrigger`, `-leftx`, `+lefty`, ...). `none` unbinds.
Extra rows: "Quick turn" (unbound by default: back, then run), "Reload" (unbound by default,
[Reload](#reload)), the third-person keys, the
third-person gamepad set (`pad_tps_*`, [Third person with a gamepad](#third-person-with-a-gamepad))
and the hotkeys.

### Input source (per window)

F1 -> Controls or ENHANCED SETTINGS -> CONTROLS:

| Setting | ini key | Values |
|---|---|---|
| Input source | `input_source` | **Auto** (keyboard/mouse and gamepad, while the window has focus), **Keyboard/mouse only** (gamepads ignored), **Gamepad only** (keys and mouse no longer play; the gamepad keeps working when another window has focus) |
| Gamepad | `input_pad` | **Any** (the first connected pad) or **1-4**: the pad the system numbers so (Xbox: its slot light; PlayStation and Switch pads: their player lights) |

With Gamepad only, the hotkeys and the overlay windows still respond to keyboard and mouse, so
F1 always gets you back. Use this to run two copies side by side for local co-op
([COOP.md](COOP.md#local-co-op-on-one-pc)). If the game runs through Steam with Steam Input
enabled, Steam may send its virtual pad only to the focused window: turn Steam Input off for it.

## Cheats and debug tools (F2)

The F2 window (also the EXTRAS page of ENHANCED SETTINGS) is meant for testing and fun:

- infinite health, heal now, infinite ammo;
- noclip: Through walls, or Walls + fly (fly with the noclip keys, or R2/L2);
- add any item to the inventory, and view the inventory;
- spawn an enemy of any type in front of you ([ENEMIES.md](ENEMIES.md));
- teleport to any room through the game's door transition: choose a **location** (an area of the
  game; Claire's and Chris's halves are listed separately, Chris's are the last three), a
  **room** and an **entry** (where you arrive). Rooms carry the save menu's name where the game
  has one (read from your disc, e.g. "Prison: front"), every other room a name of ours after its
  area and what is there ("Prison: Graveyard", "Military Training Facility: Pool"; rooms that only
  play a cutscene start with "Cutscene:"). Each entry says which room's door leads there
  ("Entry 1: from Prison: Metal detector room"; "+1" when there are more, F2 lists them on hover).
  The EXTRAS page leaves the area out of these names ("Metal detector room") where the location
  already says it, so they fit.
  Rooms you added in the mods folder show as "Custom room RM_xxxx", or by the name tag in their
  file header. The list is read from the disc the first time you open F2 or
  the EXTRAS page (a few seconds; until then the plain numbers show). In F2, typing in the open
  Room list searches every location, and **Raw ids** brings back the stage/room/position numbers.
  A room may not work in every story state (wrong case, missing scripts); an unlisted number may
  crash.

In co-op, enemy spawning is host-only. Actions chosen in ENHANCED SETTINGS run when the menu closes.

## Mods

Files in the `mods` folder (next to where you start the game, or `RECVX_MODS=DIR`) replace disc
files without touching the image: `mods/NAME` for files opened by name (for example room files
`RM_xxxx.RDX`) and `mods/<archive>.AFS/<index>` for entries of AFS archives. The log names
every replaced file. Modding reference and tools: [modding/README.md](modding/README.md).

## Troubleshooting

| Problem | What to try |
|---|---|
| Linux: "GLIBC_2.xx not found" | the build needs glibc 2.34 or newer; build with the default `RECVX_GLIBC_COMPAT=ON` ([BUILDING.md](BUILDING.md#running-on-older-distributions-glibc)) |
| Linux: no window, no sound, or "cannot open shared object" | the 32-bit OpenGL, X11/Wayland and audio libraries are missing ([BUILDING.md](BUILDING.md#linux)) |
| No file selector on the first start | Linux needs the desktop portal or `zenity`; otherwise pass the image on the command line or via `RECVX_ISO` |
| "cannot open game disc image" | check the path; only SLUS-20184 images work |
| Low frame rate at high resolution | lower Internal resolution, set a Frame rate limit, or turn off Display-rate frames; F1 -> Performance shows where time goes |
| Gamepad not working in the second window | set Input source to Gamepad only and pick the pad number; check Steam Input |
| A crash | run with `--log` and attach `recvx.log` (it contains a backtrace) to an issue |
