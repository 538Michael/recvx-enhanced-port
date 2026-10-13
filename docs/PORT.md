# Resident Evil Code: Veronica X — native PC port

Native Linux/Windows build of the [recvx-decomp](https://github.com/AshfordFamily/recvx-decomp)
decompilation (US release SLUS-20184). The game code — gameplay, AI, collision, *and* the
original PS2 platform layer — is compiled from the decomp; the PS2 hardware and IOP services
underneath it are emulated at their library/hardware boundaries by the new code in `pc/`.

## Building

```sh
# Arch/CachyOS build deps: gcc multilib (lib32-gcc-libs). SDL3 is fetched and linked statically.
git submodule update --init --recursive   # decomp/ + its katana/cri headers (mwcc is skipped)
# Runtime (32-bit, see "Data model"): lib32-glibc, lib32-mesa (GL driver), lib32-libx11 or lib32-wayland,
# and later lib32-libpulse/pipewire for audio. SDL loads these at runtime.
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/linux-i686.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
ctest --test-dir build        # RECVX_ISO=<image> also decodes and checks every movie
./build/pc/recvx "/path/to/SLUS-20184.iso"
```

`CMAKE_BUILD_TYPE` defaults to RelWithDebInfo when not given (an unset type built at -O0).
Everything is compiled with `_FILE_OFFSET_BITS=64` (disc images over 2 GB; mingw-w64 honours it
too).

**Older Linux systems** (`RECVX_GLIBC_COMPAT`, default ON on Linux): the executable needs glibc
2.34 or newer, whatever the build machine has. Without it, a build on a current distribution
binds symbols only its own glibc has (with glibc 2.43: `fmodf`, `sqrtf`, `atan2f`, ... at
`GLIBC_2.43`, `strlcpy` and the C23 `__isoc23_sscanf`/`strtol` family at 2.38 from SDL, ImGui and
miniupnpc, `_dl_find_object` at 2.35 from the static libgcc unwinder) and refuses to start on
older ones. `cmake/glibc_compat.h` is force-included into every C/C++ file and binds the math
functions to their `GLIBC_2.0` versions; `cmake/glibc_compat.c` (linked into `recvx`) defines
the C23 scanf/strtol names (calling the classic ones), `strlcpy`/`strlcat`/`wcslcpy`/`wcslcat`,
and `_dl_find_object`, which forwards to the system's when it has one (glibc < 2.35: unwinding
through C++ exceptions does not work; the port throws none). Check:

```sh
objdump -T build/pc/recvx | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1   # GLIBC_2.34
```

2.34 is the floor: glibc 2.34 moved `pthread_*`, `dlopen`, `sem_*` and the `time64` functions
into libc with that version (SDL and libstdc++ use them); older versions would need a build
against an old glibc (a container/sysroot), not symbol tricks. `-DRECVX_GLIBC_COMPAT=OFF` binds
whatever the build machine's glibc has.

Without an image argument (or `RECVX_ISO`), the first start opens a file selector (xdg portal or
`zenity` on Linux, the native dialog on Windows) and `recvx.ini` remembers the choice (`iso = ...`);
the selector comes back only if that file is gone. An image on the command line always wins.

Warnings: the decomp's own sources compile with `-w` (unmodified code) except that a call to an
undeclared function is an error (`-Werror=implicit-function-declaration`: an implicit `int`
return breaks float and 64-bit results), the port's (`pc/glue`, `pc/src`) with `-Wall`.
`pc/tools/decomp_warnings.py [build]` recompiles the decomp with return and uninitialized-variable
warnings into `docs/missing-returns.txt` and `docs/uninitialized-locals.txt` (by file and
function). The "'MIN' redefined" style lines from glue files are the
decomp's `macros.h` redefining Katana macros; GCC has no switch for them.

### Windows build

Cross-built from Linux with mingw-w64 **GCC** (tested: Arch's `mingw-w64-gcc` 16.2, UCRT). No root
needed: the Arch packages `mingw-w64-{gcc,binutils,crt,headers,winpthreads}` can be unpacked
anywhere (`tar --zstd -xf PKG -C DIR`; the host needs gmp, mpfr, libmpc, isl, zstd, which a
native GCC already pulls in). `MINGW_PREFIX` is the directory holding `bin/i686-w64-mingw32-gcc`
(omit it when the compiler is on `PATH`):

```sh
cmake -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake \
      -DMINGW_PREFIX=/path/to/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build-win recvx
```

**What to ship: `build-win/pc/recvx.exe` alone.** SDL3, miniupnpc, libnatpmp, libstdc++, libgcc
and winpthreads are linked statically; the remaining imports are Windows system DLLs (UCRT
`api-ms-win-crt-*`, kernel32, user32, gdi32, ws2_32, iphlpapi, ...; opengl32 is loaded by SDL at
run time). `i686-w64-mingw32-strip recvx.exe` drops the debug info (38 MB to 7 MB). Needs
Windows 10 or later (UCRT; on 7/8.1 the "Universal C Runtime" update) and an OpenGL 3.3 driver.
It is a console program (a console window shows the log): double-click it and pick the ISO on
the first start, or `recvx.exe "C:\path\to\SLUS-20184 ROM.iso"`, or drop the ISO onto the exe. Saves and `recvx.ini` go to `save\` in the current directory
(`--save-dir`), `--log` writes `recvx.log` with a crash dump (exception address + frame chain,
the Windows build keeps frame pointers for it: +1..5% game logic measured on Linux;
the image base is fixed, so `i686-w64-mingw32-addr2line -f -e recvx.exe ADDR` names the function).

Same memory model as Linux: image base `0x30000000` with ASLR off (`--disable-dynamicbase`),
scratchpad `VirtualAlloc`ed at `0x70000000`, `.rdata` made writable at start, GCC bitfield
layout (`-mno-ms-bitfields`; mingw defaults to MSVC's). Two Windows-specific pitfalls handled:

* **GCC, not clang/llvm-mingw.** The decomp's EE packet buffers (e.g. `NaCnkStrBufTop`) depend on
  GCC aligning arrays of 32+ bytes to 32 bytes; clang aligns them to 4 on i386 (~650 game
  symbols lose 16-byte alignment) and the chunk-model shadow/modifier polygons draw as dark
  quads. A clang build of the same sources misrenders on Linux too.
* **`rand()`**: the game assumes `RAND_MAX` 0x7fffffff (`njRandom()` divides by 2^31); UCRT's is
  0x7fff. The Windows build gives the game glibc's generator (`pc/src/ps2/stubs.c`), so it gets
  the same random sequence as the Linux build.

Verified under Wine 11.19 (wow64, Xvfb + llvmpipe), against the Linux build on the same Xvfb:
attract demo (`RECVX_FAST_DEMO=1 RECVX_SKIP_MOVIES=1 RECVX_INPUT=200:start,206:-`, 119 frame dumps
over 2 minutes, two rooms) identical except 1-2 pixels off by one in 6 frames (libm rounding);
system data save from the Option menu creates a byte-identical card directory, and the load
screen reads it with the same card command sequence and identical frames; co-op between a
Windows host and a Linux client (pose exchange both ways, the client joins the host's game in
0/11; UPnP/NAT-PMP discovery fails cleanly without a router). Not verified: real Windows,
audio output (headless dummy driver), gamepads, saving/loading an actual game in progress.

The game reads your own disc image in place (ISO9660); nothing is extracted or bundled.
Saves go to `./save/card0` (`--save-dir` to change).

Controls (keyboard, any SDL gamepad also works; all rebindable, see "Controls"): arrows = d-pad, Enter = Start,
Backspace = Select, Z/X/A/S = Cross/Circle/Square/Triangle, Q/W = L1/R1, 1/2 = L2/R2,
Space (gamepad: right trigger) = quick knife (hold; see "Quick knife"),
F11 = fullscreen, F1 = settings overlay (the same settings are in the game's own OPTION menu,
row PC SETTINGS: see "Options menu") (window size incl. 3440x1440, fullscreen, vsync,
aspect, internal resolution, frame-rate counter, music/effects volume; saved in `<save dir>/recvx.ini`).
**Show ping** (F1 -> Display, PC SETTINGS -> DISPLAY, ini `show_ping`, default on): while a co-op
session is connected, the ping to the partner in the frame-rate counter's box at the top right
("59.9 fps   ping 12 ms"; alone when the frame rate is hidden).
`--skip-doors` (F1 -> Gameplay) = room transitions (doors, stairs, ladders) take the game's
own no-animation path (`bhSysCallDoordemo` mode 10, the load screen used by scripted
transitions) instead of the door/stairs animation.
F2 = cheat menu for testing (`pc/glue/cheat_glue.c`, applied on the game thread at the frame
hand-off): infinite health, infinite ammo, add any item (names read from the
game's own message data), teleport via the game's door transition (stage/room/position; door type
255 skips the animation). Inventory: `sys->itm[ply_id*16]` = index of the equipped slot (0 = none),
`[+1]` = the separate slot above the grid (`listcsr_0` 1), `[+2..]` = 8 (10 with the side pack)
slots, each `(item id << 16) | count`; bits 0x7000 of the id word are the ammo kind (grenade
launcher, bow gun), 0x800 = unlimited. Infinite ammo hooks `bhCheckBullet`/`bhCountBullet`
(weapon.c), the only place a shot takes ammo, and makes them take the game's own unlimited-weapon
path for every weapon and ammo kind (also in third person and co-op). It used to restore `[+1]`
after a shot, which is not the equipped weapon, so it never did anything.
Noclip (F2, `RECVX_NOCLIP="gameframe:mode,..."`): "Through walls" skips the player's enemy push
(`bhCheckEnemies`, `flg` 0x8) and wall check (`bhCheckWallEx`, `flg` 0x10; also objects' boxes
and slopes) in `player.c bhControlPlayer` (one hook, `pc_noclip`). Walking never changes the
height otherwise (no gravity, heights are the room's floor levels), so the player keeps its
height outside the room too. "Through walls + fly" also moves up/down (R / Page Up / R2, F /
Page Down / L2; R3 opens the status screen, so not the stick clicks; 1/2 = L2/R2 also work; on the
gamepad the right trigger, the quick knife's, which is off while flying) and
keeps the floor number in step with the height (triggers and walls are per floor). Doors,
action and floor triggers work as before (they run after the hook). Turning it off inside a
wall: the next wall check pushes the player out (tested); outside the room: the player stays
there and can walk freely (no walls) or noclip back; after flying the player is put back on the
floor level it took off from. Only our player (co-op runs the partner through `plp` as well);
the partner sees the movement through the normal pose exchange. Enemies' own code may still
push or grab the player.
Spawn enemy (F2): any enemy type from a list (the "You appear as" creature and zombie models plus
the types it leaves out), placed 3 m in front of the player and facing it, with its own AI. A type
the room does not have is loaded from another room file (model, motions, textures; released at the
next door). Refused with a message when enemy slots, room memory or texture memory run out (the
port raises the PS2's texture heap and table and runs each type's helper code separately, so mixed
types share a room; docs/ENEMIES.md 2.2), and on the co-op guest (the host's spawns appear on
the guest too, protocol 10); spawned enemies have no kill flag and are gone after leaving the room. Which types work and which are limited (bosses, scripted
types): docs/ENEMIES.md section 6.
Enemy options (PC SETTINGS / F1; ini `enemy_extra`, `enemy_place`, `enemy_random`, `enemy_seed`;
all off by default): CO-OP page "Extra enemies (host)": up to 1..3 more enemies per room of the
types the room has; GAMEPLAY page "Rearranged enemies", "Enemy randomizer", "Enemy seed".
Story, cutscene and boss enemies stay as they are; in co-op the host's options count for both.
docs/ENEMIES.md section 7.
Mouse in the inventory (F1 -> Gameplay, ini `menu_mouse`, default on): see "Mouse in menus".
F5 = third-person mode (over-the-shoulder camera, mouse/keyboard, free aim; `--third-person`;
docs/THIRDPERSON.md). "Mouse/keyboard camera" (THIRD PERSON page, ini `tps_free_camera`): B
(default) keeps the camera behind the character, the mouse turns it; A (free) orbits the camera
with the mouse and walks W/A/S/D relative to it (not the KEY CONFIG types A-D).
F3 = online co-op (`docs/COOP.md`; milestone 1: the partner is shown in the same room; T / left
stick click next to the partner = trade request and the trade screen;
`--coop-host PORT` / `--coop-join HOST:PORT#CODE`).

## Mouse in menus

`pc/glue/menu_glue.c` (game thread, hook at the end of `bhSetPad`) drives the status screen's own
cursor state from the mouse; `pc/src/input/menu_mouse.c` (main thread) maps the window position
to the game's 2D screen space (`gs_window_to_screen`: the display rect of `draw_display`, so
pillarbox, stretch and hor+ margins are accounted for; `njDrawSprite2D` coordinates are the
640x480 display area, render scale does not matter) and counts clicks and wheel notches.
Keyboard and gamepad work unchanged; the mouse only sets what their d-pad code sets.

| Screen (`swork`) | Mouse |
|---|---|
| items (`mode` 1, `CursorMove`) | hover moves the cursor (`listcsr_0/2`, two-cell items first as in `ItemSort`), left click opens the item's command list, a click on the top bar (exit/file/map/items) selects it, right click closes the inventory |
| top bar (`mode` 2, `MainCommand`) | hover highlights (`maincsr`), left click confirms; a click on the grid goes back to the items |
| command list (`mode` 4, `ItemCommand`) | hover highlights use/equip, examine, combine, ...; click confirms, a click outside or right click closes it |
| combine (`ItemCombination`) | as the items: hover picks the second item, click combines, right click cancels |
| examine (`mode` 8, `ItemModelCheck`) | drag with the left button rotates, wheel zooms (hook in itemview.c), click (no drag) checks the item, right click back |
| item box (`subscreenmode` 4) | inventory side as the items (click picks the item to swap); box list: wheel scrolls, click swaps, right click back |
| file view, pick-up dialog, map | left click = confirm, right click = cancel/leave, wheel = d-pad up/down (map floors, file pages) |
| title menu (game mode, difficulty, extra games) | hover moves the cursor, left click confirms, right click goes back (`pc/glue/lobby_glue.c`, called from `CheckButton`) |
| co-op lobby (title menu MULTIPLAYER) | hover selects a row, click activates or steps a value (left arrow: back), wheel steps a value, right click backs out |
| OPTION (title and in game) | hover selects a row, click confirms (STEREO/MONO, ON/OFF: picks that value), right click leaves; KEY CONFIG, SCREEN SETUP, MONITOR ADJUSTMENT: right click goes back |
| PC SETTINGS pages | as the lobby: hover selects, click activates, the value arrows step back/forward, the page arrows turn the page, wheel steps a value (or scrolls), right click backs out; on a binding the click picks the slot and waits for a key |
| co-op trade screen (`pc/glue/trade_glue.c`) | hover selects a cell of our side, a click on the selected cell or on CONFIRM toggles ready, LEAVE or right click leaves (docs/COOP.md "Trade screen") |

The OS cursor is hidden unless such a screen or an overlay window is open; third person
releases the captured mouse while the screen is open (`tps_update_mouse_mode`) and captures it
again afterwards. Test aids: `RECVX_MENU_MOUSE="frame:x:y[:act],..."` (window pixels; act `l`/`r`
click, `L`/`U` left down/up, `w`/`s` wheel), `RECVX_MENU_TRACE=1` logs clicks with the menu state.

## Options menu (PC SETTINGS)

The game's OPTION screen (title and in game, Select) has an eighth row, **PC SETTINGS**, set in
the screen's own lettering (letters of AUDIO SETUP, KEY CONFIG, SCREEN SETUP, EXIT and SETUP
cut from its label texture; only coordinates are in the code) to the right of EXIT. It opens
option level 4 (`pc/glue/options_glue.c`; hooks in `adv.c`: `OptionDef[8]`, the cursor wraps
through row 7, `DisplayOptionPlate` case 4, `DisplayOptionBg` and the level-change fades treat 4
like KEY CONFIG). The original seven rows, their pages and the system-data save are unchanged.

* Pages: DISPLAY, AUDIO, GAMEPLAY, ENHANCED, THIRD PERSON, CO-OP, CONTROLS, EXTRAS (the F2
  cheats). Drawn like the co-op lobby: the option screen's background and OPTION title, a dark
  panel, the game's font (`sys->ef_tlist`; at the title the option screen loads it itself), the
  game's cursor sounds (`CallSystemSe` 2 move, 3 confirm, 0 back, 1 refused) and its pulsing
  highlight (`FlushCount2`). Seven rows are visible, the list scrolls; the bottom lines are the
  help text of the selected row (F1's tooltips), or why it cannot change now.
* Input: up/down rows, left/right values (pages on the page row), L1/R1 pages from anywhere,
  confirm (the key type's OK button or Start) toggles or runs, cancel or Select goes back; the
  mouse as in "Mouse in menus". Keyboard, gamepad and mouse all go through the bindings.
* Settings that wait for the next start say "(next start)" (Skip intro). Rows that cannot change
  now are dimmed and say why: co-op port, loot and UPnP during a session, window size in
  fullscreen, cheat actions at the title ("In game only"). Cheat actions chosen in game (heal,
  add item, spawn, teleport) run when the menu closes (`pc_options_close`), as F2 would.
* Both UIs edit the same variables: a change in one shows in the other at once. The native menu
  runs on the game thread: `settings_set` stores the value and flags it; the main thread
  (`settings_poll`, every event pump) runs its applier (window, vsync, volumes) and writes
  `recvx.ini` (`config_save`), for F1's changes too ("Threads and shared state").

**One table** (`pc/src/host/settings.c`, `settings.h`): each entry is page, label, the variable
(or get/set), range and step, value names or a printf format, flags, help text, an applier, a
`locked()` reason. The native menu and F1 both draw every entry from it (`settings_imgui`: a
checkbox, a combo for named values, a slider for numbers), with the same help text; only the
CO-OP and EXTRAS pages, which have their own windows (F3, F2), show just their entries flagged
`SET_F1` in F1. **To add a setting**: its
variable's ini key in `config.c` `keys[]` (as before), and one entry in `g_settings[]`, e.g.

```c
{ SP_GAMEPLAY, "Enemy difficulty", V(g_config.enemy_level), 0, 2, 1, levels, .flags = SET_F1,
  .help = "...", .locked = in_session },
```

`SET_F1` for a CO-OP or EXTRAS entry that F1 should show too, `SET_RESTART` for
"takes effect at the next start", `SET_GAME_ONLY` for in-game only, `SET_ACTION` (+ `.apply`)
for a command. Labels and names must be plain ASCII the game's font has: no `&`, `*`, `#`,
`@` (they are other glyphs there).

## Controls

Keyboard/mouse and gamepad have one binding set each (`pc/src/input/bindings.c`), two slots per
row. Rebinding works at the layer that builds the DS2 pad report (`pad.c build_report`): a row
of the game is a **DS2 button**, and the game's own key type (OPTION -> KEY CONFIG, A-C) turns
the buttons into actions in `bhSetPad` as before, so nothing in the game is patched per action
and menus, movement, aiming and the inventory all follow. The menus name each row by what the
button does under the current key type (`pad_type[sys->keytype]`): type A "Forward / Aim up
(Up)", "Fire / Confirm (Cross)", "Run / Cancel (Circle)", "Map (Triangle)", "Aim (R1)", "Change
aim (L1)", "Inventory (Start)", "Options (Select)", ...

| Group | Rows | Defaults |
|---|---|---|
| game (keyboard and gamepad) | the 16 DS2 buttons; Quick turn (holds back, presses the key type's run button from the third frame: the game's own quick turn); Quick knife (hold); Trade request (co-op) | keyboard as before (arrows, Enter, Backspace, Z/X/A/S, Q/W, 1/2), quick knife Space, trade request T; gamepad as a DS2 (south/east/west/north = Cross/Circle/Square/Triangle, shoulders L1/R1, left trigger L2, back/start, right stick click R3), quick knife the right trigger (R2 unbound), trade request the left stick click (L3 unbound); Quick turn unbound |
| third person (keyboard and mouse) | forward, back, turn left/right, run, action, cancel, inventory, map, aim (hold), fire, quick turn | W/S/A/D, Left/Right Shift, E, Q, Tab, M, right/left mouse; quick turn unbound |
| hotkeys (keyboard) | third-person camera on/off, settings overlay, cheat menu, co-op window, fullscreen, noclip fly up/down | F5 (still ini `tps_key`), F1, F2, F3, F11, R/Page Up, F/Page Down |

* While third person is on, keyboard keys it uses no longer press DS2 buttons (by default W, A,
  S, Q, as before). Mouse buttons count while the mouse is captured (third person).
* The left stick still moves as on the DS2 (analog), the right stick as before; stick halves and
  triggers can also be bound as buttons. Noclip fly also follows the R2/L2 rows (and the gamepad
  quick-knife row for up).
* Rebinding: F1 -> Controls (tabs keyboard/mouse and gamepad; click a binding, then press;
  right click clears) or OPTION -> PC SETTINGS -> CONTROLS (DEVICE picks the set; confirm on a
  binding, left/right picks the slot). "Press a key/button ...": Esc cancels, Delete clears,
  5 s time-out cancels; keys held when it starts do not count. A code another row of the same
  context already has is swapped to that row (the message says which); game and third-person
  rows may share keys. Conflicts from a hand-edited file are shown in red with the other row.
  Reset to defaults per set.
* Gamepad names follow the connected pad (`SDL_GetGamepadType`, face buttons by
  `SDL_GetGamepadButtonLabelForType`): Xbox A/B/X/Y, LB/RB, LT/RT, View/Menu; PlayStation
  Cross/Circle/Square/Triangle, L1/R1, L2/R2, Share/Options/Create; Nintendo B/A/Y/X, L/R,
  ZL/ZR, -/+; others South/East/West/North, Left shoulder, ... Which gamepad: "Input source" below
  (default: the first connected one).

`recvx.ini`, after the other keys (all rows are written; a second binding as `<name>_2`). Defaults,
except the last line, an example of a binding to a stick half:

```ini
key_up = Up
key_cross = Z
key_quickturn = none
key_tps_run = Left Shift
key_tps_run_2 = Right Shift
key_tps_aim = Mouse Right
key_settings = F1
key_noclip_up = R
key_noclip_up_2 = Page Up
pad_cross = south
pad_l2 = +lefttrigger
pad_quickturn = -lefty
```

### Input source (per window)

F1 -> Controls (top) or OPTION -> PC SETTINGS -> CONTROLS (first two rows); saved in that
instance's `recvx.ini` (`--save-dir`), so two copies of the game keep their own values.

| Setting (ini) | Values |
|---|---|
| **Input source** (`input_source`) | `0` **Auto** (default, as before): keyboard/mouse and the gamepad both play, while the window has focus. `1` **Keyboard/mouse only**: gamepads are ignored (none is opened, so no rumble either); only while the window has focus, since the system sends keys and clicks to the focused window. `2` **Gamepad only**: keys and mouse buttons no longer press anything in the game, the mouse neither moves the third-person camera (the cursor is never captured) nor the menu cursor; the gamepad keeps working while another window has focus. |
| **Gamepad** (`input_pad`) | `0` **Any**: the first gamepad connected (the open one stays while connected, as before). `1`-`4`: the pad SDL numbers as that player (`SDL_GetGamepadPlayerIndexForID` + 1), shown with its name ("2: Xbox Series X Controller", or "not connected"). Applies in Auto too. |

* Still active with **Gamepad only**: all hotkeys (F1 settings, F2 cheats, F3 co-op window, F5
  third person, F11 fullscreen; rebound ones too), keyboard and mouse inside those overlay
  windows, typing in the co-op lobby's text fields, and the "press a key" of a rebinding. So F1
  always gets you back to Auto. Noclip fly up/down keys are game input and are off.
* The player number: on Windows XInput pads (Xbox) it is the XInput slot, which the pad's ring
  shows; PlayStation (HIDAPI) pads show it on their player LEDs; other pads get the lowest free
  number when they appear. A pad that is unplugged and plugged back in while the other stays
  gets its old number back (tested with virtual pads), so its window keeps it. Both copies of the
  game number the pads the same way as long as they see the same pads in the same order (start
  both with the pads already connected).
* Background: SDL drops joystick input while none of the process's windows has keyboard focus
  unless `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` is set; the port sets it for Gamepad only
  (`bind_apply_source`; the hint may change any time, no restart). An environment variable
  `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1` wins over it and gives Auto background gamepad input
  too. Nothing else stops an unfocused window: there is no pause or mute on focus loss, and the
  frame limiter does not look at focus. Losing focus clears SDL's keyboard state and releases the
  third-person mouse capture (`tps_update_mouse_mode`); a click into the window captures it again.
* Backends: Linux evdev/HIDAPI read the devices directly, any window state. Windows (SDL 3.4
  defaults, mingw build): XInput (polled, works in the background), DirectInput
  (`DISCL_BACKGROUND`) and HIDAPI do; RawInput and Windows.Gaming.Input are off by default (WGI stops in the background, SDL prefers XInput for that reason);
  GameInput is not compiled in. Steam Input, when the game runs through Steam with it on, may
  send its virtual pad only to the focused game: turn Steam Input off for it.
* Rumble goes to the window's own open pad only.

#### Local co-op on one PC

1. Two folders (or two `--save-dir`), each with its own `recvx.ini` and memory card:
   `recvx --save-dir save1 game.iso` and `recvx --save-dir save2 game.iso` (or start each from
   its own working directory as `build/pc/start_recvx_Client2.sh` does).
2. One hosts, the other joins over localhost: F3 or MULTIPLAYER, host port e.g. 7140, the
   other joins `127.0.0.1:7140` with the host's join code (or `--coop-host 7140` /
   `--coop-join 127.0.0.1:7140#CODE`, docs/COOP.md). The co-op player id is kept in each ini, so
   separate save dirs are separate players (one shared save dir would be one player).
3. Per window, F1 -> Controls -> Input source: e.g. window 1 Keyboard/mouse only, window 2 Gamepad
   only; or both Gamepad only with Gamepad 1 and Gamepad 2. Drag each window to its monitor
   (F11 fullscreen works per window). Keyboard/mouse plays in whichever window you last clicked.
4. Which pad is which: the Gamepad setting names the pad of each number ("1: <name>"), the
   Controls' Gamepad tab names the pad the window reads.

### Trade request (co-op)

Row "Trade request (co-op)" (ini `key_trade` / `pad_trade`): **T** on the keyboard, the **left
stick click** on the gamepad. Next to the co-op partner it asks to trade, accepts the partner's
request, or withdraws ours; both then get the trade screen (docs/COOP.md "Trade screen (protocol
14)"). Placeholders chosen because nothing used them: T is in no other row, and L3 is no action
under any key type (A-D; R3 is the inventory in A-C), so L3's own gamepad default moved to the trade
request. An older `recvx.ini` (no `key_trade` line, written before the `pad_` lines) drops its
`pad_l3 = leftstick`. Read on the game thread (`pad.c pad_trade_held`, the third-person key rule as
for game rows); in single player it does nothing.

### Quick knife

Hold **Space** (gamepad: **right trigger**; row "Quick knife (hold)", ini `key_quickknife` /
`pad_quickknife`) with a Combat Knife in the inventory: the knife is equipped and raised at once,
without the inventory; the fire button (key type's Cross/Circle/Square, left mouse in third
person) slashes. Let go and the weapon equipped before comes back once the knife is lowered. On/off:
GAMEPLAY "Quick knife" (F1 Gameplay, ini `quick_knife`, default on).

* Defaults: Space was third person's second action key (action stays on E). On the gamepad the
  right trigger: R2 is no action under any key type (A-D) and the stick clicks are taken (R3 opens
  the inventory in A-C); R2's default binding moves to the quick knife. An older `recvx.ini`
  (no `key_quickknife` line) drops its `key_tps_action_2 = Space` and `pad_r2 = +righttrigger`.
* It goes through the inventory's own `WeaponSet` (`sys->itm[ply*16]` = equipped slot, the
  weapon number, the weapon file load), so ammo, the knife's hit check, auto-aim, third-person
  free aim and pose and the co-op partner's view are the game's own. The knife is found in any slot
  (`[1]`, the separate slot, holds the lighter; `[2..]` the grid), item 8.
* Edge cases: no knife: nothing happens. Knife already equipped: it is raised while held and
  stays equipped. Pressed while aiming, reloading, grabbed, in a menu, an event or a door
  transition, while the weapon file of a change still loads, during stairs, pushing, the quick
  turn or the idle animation: it waits while held and draws once the player is back to standing,
  walking, running or stepping back with the weapon down. Released mid-swing: the swing ends, the
  game lowers the knife, then the weapon before comes back. Nothing equipped before: the knife is
  unequipped again. The weapon before gone (used, combined, moved): the same item in another
  slot, else the knife stays. Weapon changed in the inventory or by a script meanwhile, game over,
  back to the title: nothing is given back. Off during noclip flying. Co-op: no protocol change,
  the partner sees the knife through the normal pose.
* Code: `pc/glue/knife_glue.c` (`knife_pad`, hooked in `pad.c bhSetPad` after `tps_pad`; it
  adds the ready bit 0x10 while held), `pad.c pad_quick_knife`. Test aids: `RECVX_KNIFE_TRACE=1`,
  `RECVX_KNIFE_DROP=1` (see the table below).

Keys are SDL key names (`SDL_GetScancodeName`: `Z`, `Return`, `Left Shift`, `Page Up`, `F1`),
mouse buttons `Mouse Left/Middle/Right/X1/X2`; gamepad inputs positional (`south` = Xbox A =
PlayStation Cross; `east`, `west`, `north`, `back`, `start`, `guide`, `leftshoulder`,
`rightshoulder`, `leftstick`, `rightstick`, `dpup`, `dpdown`, `dpleft`, `dpright`, `misc1`,
`paddle1..4`, `touchpad`) or an axis half (`+lefttrigger`, `+righttrigger`, `-leftx` stick left,
`+lefty` stick down, `rightx`, `righty`). `none` unbinds. Unknown names are logged and ignored.

### Idle dimming (the game's screen saver)

After 10800 game frames (6 min) without input the game lays a 50% black polygon over the room
(`system.c bhSysCallScreenSaver`, fades in over 300 frames) until the next input; pausing also
clears it. Its idle check reads only the PS2 pad (`sys->p1per`: buttons, stick past ±48), so
input the port adds on top (third-person keys and mouse, quick knife, inventory mouse, trade
screen) neither reset the timer nor ended the dimming: in third person with keyboard and mouse the
view went half dark after 6 minutes in one room, during play, and stayed so (a door or teleport
does not clear it either). `pc_input_active` (`tps_glue.c`: `sys->pad_on` outside the attract
demo, or third-person mouse motion) now counts as input too. Pad-only play is unchanged.

## Layout

| Path | What |
|---|---|
| `decomp/` | upstream decomp as a git submodule, on the `pc-port` branch of the fork. Changes are limited to `#ifdef TARGET_PC` blocks (asm replacements, a few PS2-address idioms) — see `git -C decomp log -p 12a05ddf..HEAD` (upstream base `12a05ddf`) |
| `pc/include/` | `pc_prelude.h` (force-included into decomp TUs), SDK replacement headers (`sdk/`) |
| `pc/src/ps2/` | EE/IOP layer: kernel, DMA, VIF1, VU0/VU1 microcode (statically compiled), libcdvd, libpad, libmc, SIF |
| `pc/src/gs/` | GS emulation: GIF parser, register file, local memory, OpenGL rasterizer |
| `pc/src/audio/` | CRI ADX (ADXF/ADXT), TSNDDRV sound driver HLE |
| `pc/src/host/` | SDL3 window/context, entry point, settings (`config.c`), F1 overlay (`overlay.cpp`, Dear ImGui) |
| `pc/src/movie/` | PSS demuxer, MPEG-2 video decoder, movie player |
| `pc/glue/` | PC implementations of decomp-declared APIs that need decomp types (movie player, debug hooks) |
| `pc/tools/` | `asm2c.py` (EE/VU0 inline asm -> C), `vsm2inc.py` (VU microcode -> tables), `rdx.py` + `room_test.sh` (room files, docs/MAPS.md), helpers |
| `pc/tests/` | self-checks (`ctest`) |

## Architecture

The decomp's `ps2_*.c` files are Sega's Ninja/Shinobi API implemented on PS2 hardware: they
build DMA chains, VIF/GIF packets and run VU microcode. Instead of rewriting that layer (and
risking behavioural drift), the PC build keeps it and emulates what is underneath:

```
game logic (decomp)  ->  Ninja/Shinobi on PS2 (decomp ps2_*.c)
                           |-- inline EE/VU0 asm  -> translated C (asm2c) on a VU0 model
                           |-- vcallms            -> VU0 microcode (static C)
                           |-- sceDmaSend/CHCR    -> pc/src/ps2/dma.c (chain walker)
                                 |-- VIF1 -> vif.c -> VU1 microcode (static C) -> XGKICK
                                 |-- GIF  -> gs/gs.c (PATH1/2/3) -> gs_gl.c (OpenGL)
                           |-- libcdvd/libpad/libmc/SIF RPC -> host implementations
```

* **VU microcode** is not rewritten by hand: `vsm/*.vsm` (from the decomp) is converted to
  decoded tables (`vsm2inc.py`). The execution model handles FMAC latency, delayed MAC/clip
  flags, Q latency and branch/[E] delay slots, because the original programs are
  hand-scheduled around those (e.g. `FMAND` reading the flags of a specific earlier `SUB`).
  The programs are compiled statically: `vu_step()` (one instruction pair) is always
  inlined into a switch with one case per micro address (`vu_static.inc`), so the C compiler
  folds every decoded field into straight-line code per instruction. Because both paths share
  `vu_step()`, they are equivalent by construction; `RECVX_VU_CHECK=1` runs every call both
  ways and compares the whole VU state (0 mismatches over 10.8 M calls through the title and
  attract demo), and `RECVX_VU_INTERP=1` selects the interpreter.
  Two hot spots of chunk-model drawing have hand-written C with the same arithmetic
  (`upper_on`'s clamping, in program order) and the same resulting VU state:
  `VU0_CALCPOINT` without point lights (`vu.c calcpoint_fast`, SSE2: the program's timing has
  no data-dependent branches there, so every Q/MAC/CLIP read is resolved once; called once
  per vertex of every node drawn, ~0.8 us through `vu_step`, ~60 ns now) and the strip clip
  test of `njCnkCsUvh/Uvn` (`vu_strip_clip`, the asm2c block's macro ops inlined).
  `RECVX_VU_CHECK=1` checks `calcpoint_fast` too (0 mismatches over 23.9 M calls through
  RM_1310); `pc/tests/test_vu.c` compares both against the interpreter / the `vu_upper`
  sequence on random states, including overflow and denormal inputs. Point-light vertices
  still run the static code. The native CALCPOINT is not timed (two clock reads cost as much
  as the call): it counts as game logic in `--profile` and the dump statistics.
* **GS**: local memory (all PSM swizzles, CLUT buffer/CLD semantics, transfers) is exact;
  rasterization is OpenGL at `scale x` resolution with render targets per framebuffer address,
  so render-to-texture effects (e.g. `PS2_swap`'s 96/128 frame blend) work unchanged.
* **Widescreen (hor+)**: render targets get GS-pixel margins on both sides; the game's clip
  matrix (`_Make_ClipMatrix`) and CPU culling cone (`Ps2CalcScreenCone`) are widened by
  `pc_hor_scale()` while the projection is untouched, so 3D fills the margins and 2D drawn at
  0..639 stays 4:3 in the middle. Model culling is widened the same way (`fNaViwHalfW` for
  the chunk-model clip test, the joint window of `bhCheckClipModel`/`bhCheckClipPoint` for
  characters and objects). Each room camera also hides room parts it cannot see (`hidobj`
  lists in the camera data, applied in `cut.c`, authored for 4:3); in widescreen such a part
  is drawn anyway if its bounding sphere lies entirely outside the 4:3 view, while hidden
  parts inside it (occluders between camera and player) stay hidden (`pc_show_hidden`).
  Full-width scissors, untextured full-width fills (clears,
  fades) and full-width copies of a render target are extended into the margins.
* **Threads**: the game (decomp main loop, VU, GIF parsing) runs on its own thread and only
  records each frame's GS command stream (register writes, image uploads, display buffer;
  `gs/stream.c`). The main thread executes those streams against the GS emulation and OpenGL
  and presents in step with the display (`host/render.c`). At most one frame is in flight;
  StoreImage is a sync point (the render target is read back into GS memory).
  Who owns what: "Threads and shared state" below.

* **Enhanced mode (`--enhanced`, F1; vanilla is the default)**: game logic, physics,
  animation and audio stay at the original 30 Hz. They are frame-stepped throughout the game
  code, so running them faster would change gameplay. The engine tags every 3D mesh it draws
  (`njCnkDrawModelLocal[Mod]` -> `pc_mesh_begin/end` -> `stream_tag`), which gives the renderer
  exact vertex correspondence between consecutive frames; each strip is tagged too
  (`pc_mesh_strip`), so a strip culled in one frame only drops itself. Of several instances of
  one mesh (enemies of a type), the one that moved least is used. TEX0 is not part of the key
  (the game streams textures into rotating GS memory). The game's clip volume reaches into
  the GS guard band (`pc_clip_guard`), so geometry crossing a screen edge is not re-cut every
  frame; OpenGL clips it. At every display refresh the frame is replayed with matched
  vertices blended in homogeneous space (x*w, y*w and w = 1/Q; S/T/Q rescaled to the blended
  w, so textures keep their perspective) at t = k/n for a display rate n times the game's (60/120/240 Hz, even
  spacing), otherwise t = time since arrival / frame period. Untagged output (2D, particles,
  screen filters) is shown as is. Frames are never blended across a stage/room change or a
  camera cut (`cam.ncut`).
  A mesh is blended if at least half its vertices moved less than 128 px; single vertices
  moving more than 512 px (off screen, wrapped GS coordinates) are not. On the attract demo
  99.5% of the 3D vertices are blended.
  `PS2_swap` mixes each new frame 75% over the previous display image (FBP 0 over FBP 150,
  `ALPHA FIX 96`), a motion trail. Replays start from the display image as the frame found
  it (copied in `gs_frame_begin`, put back in `gs_replay_begin`; `RECVX_REPLAY_RTSTART=0`:
  from the executed frame's result, which added a 25% pre-echo of it). Only the display
  target carries pixels across frames; FBP 0 and depth are cleared after every swap.
  A replay draws into a second set of render targets swapped in for its duration, so the
  executed frame's targets are never drawn over (no copy back); it finds in each target what
  the frame found, but only where the frame reads before overwriting (see "In-between images"
  under "Cost per frame").
* **Frame blend (motion trail)** (F1 -> Display, `recvx.ini` `frame_blend`: 1 on, 0 off,
  -1 auto = on in vanilla, off in enhanced; default auto): off, the swap copies the frame
  (`FIX 128`, as during movies) for clean, sharp frames. Nothing else reads the display
  buffer (StoreImage/StoreRenderTex read FBP 0), so fades, flashes, the distortion effect
  and menus are drawn as before, only without the trail's one-frame fade-out.

## Threads and shared state

| Thread | Owns |
|---|---|
| main (`main()`, `render_loop`) | SDL window and events (`host_pump_events`), OpenGL, GS emulation and presentation, the F1/F2/F3 overlay (ImGui), keyboard/gamepad binding capture, lobby text input and clipboard (`lobby_io_frame`), the setting appliers (window, vsync, volumes) and **every `recvx.ini` write** (`settings_poll` -> `config_save`; before the game thread starts, `main()` may save the chosen disc image directly) |
| game (`game_thread`) | the decomp and `pc/glue` (native OPTION/PC SETTINGS menu, lobby screen, co-op incl. its UDP socket polled once per frame, cheats applied at the frame hand-off), disc reads (`cdvd.c`), memory card (`mc.c`), creating and destroying audio streams (ADX voices `adx.c`, movie soundtrack `movie/player.c`) |
| SDL audio | ADX decoding in the stream callbacks (`audio.c feed`, own file handle per voice), the sound-effect synth mix |
| portmap | UPnP/NAT-PMP while hosting (`net/portmap.c`), results read via `portmap_status` |
| co-op arrival times (Windows) | waits on the co-op socket's `FD_READ` event and notes when data arrived (`net/net.c`, `net_rx_us`); reads nothing |
| log (Linux, `--log`) | copies stdout/stderr into the log file |

Shared state and how it crosses threads:
* Settings variables (`g_config`, `g_tps`, `g_coop` options, `g_cheat` toggles, bindings) are
  aligned ints written by both menus; a reader sees the old or the new value. A change is
  handed over with `settings_set`/`settings_save_later` (release store of the dirty flags);
  `settings_poll` takes them with acquire, so the main thread applies and saves the value that
  raised the flag. Nobody else calls `config_save`: the lobby (game thread) and F1/hotkeys
  (main thread) all go through `settings_save_later`. Values are applied at once; the file is
  written 500 ms after the last change (a slider drag changes it every frame) and at exit.
* `recvx.ini` is written to `recvx.ini.<pid>.tmp`, flushed to disk and renamed over the old
  file (`pc_save_file`, `MoveFileEx` on Windows): a crash or full disk leaves the old file.
* Commands from the overlay to the game are request words (`g_cheat.request` bits with
  atomics, `g_coop.request`, `g_coop.trade_req`), taken by the game thread at its frame.
* Audio: `audio.c` has one lock covering a stream's publication (`used`, `movie_stream`),
  its gain and its removal; the main thread's `audio_apply_volumes` only touches published
  streams under it, the game thread unpublishes under it before destroying.
* Co-op status (`g_coop.status`, peer name, trade fields, `state`) is written by the game
  thread and read by the overlay without a lock; every string's last byte stays 0, so a read
  is bounded (the overlay copies `status` first) but may show a half-updated line.

File paths are UTF-8 everywhere (SDL's convention): `pc_fopen` (`host/fileio.c`) opens with
`_wfopen` on Windows, the memory card uses SDL's path functions, the Windows build rebuilds
`argv` from the UTF-16 command line and reads path variables with `SDL_getenv`.

## Rendering rework (native paths)

The emulated pipeline (VU1 microprograms -> GIF -> GS emulation -> OpenGL) is being replaced
by native paths step by step; each is switchable at runtime for comparison (F1 -> Display,
`RECVX_NATIVE3D`, `RECVX_NATIVE_TEX`), and `pc/tools/compare_native.sh` diffs the two paths
headless.

* **Native 3D** (`pc/glue/native_glue.c`, `pc/src/gs/stream.c`, `gs_gl.c gs_draw_native`):
  chunk-model strips (`njCnkCsUvh/Uvn`) no longer go to VU1. The per-vertex results of the
  VU1 programs (colour routines, ST/Q, fog, normal culling of single-sided strips) are
  computed on the CPU; the position is passed homogeneously, `(offset*Vz + k*V.xy,
  -b - a*Vz, Vz)`, exact by the VU0 `CALCPOINT` formulas, and the GPU projects. Triangles
  are clipped against near/far on the render thread (`native_draw`), as the VU1 `SCISSOR`
  does. Strips are recorded unclipped, one-to-one between frames, which is what the
  enhanced-mode interpolation blends (homogeneous position = view-space position).
* **Ordering-table primitives**: the CPU strip functions (`ps2_Vu1Strip.c`) and the general
  builder `Ps2AddPrim` (2D polygons, effects, particles; triangle types) produce native
  records too. Their GS packets keep the render state (TEST, ALPHA, texture) but carry, in
  place of vertices, a write of the record number to GS register 0x7F (`NATIVE_MARKER_REG`),
  so the record is drawn exactly where the GS would have drawn it in the sorted order.
  Sprites, lines and points and the full-screen sprite operations of `ps2_dummy.c` (fades,
  screen copies) still take the GS path.
* **Native textures** (`pc/glue/texture_glue.c`, `pc/src/gs/textures.c`; default on since 2026-10-07): the
  game re-uploads the whole TIM2 to its single GS texture slot on every switch. Each TIM2
  content is recorded once as a definition; switches bind it. CLUT uploads are executed for
  real (so the CLUT buffer behaves as on the GS); image uploads are deferred in an ordered
  queue and written into GS memory only before something else reads or overwrites them. A
  bound slot is decoded once per TEX0/palette in a scratch copy of GS memory. Memory is
  capped (definitions 96 MB, decoded textures 256 MB).
* **Render textures on the GPU** (`RECVX_NATIVE_RTEX`, default on): the game's only GS
  readback is `StoreRenderTex` (ps2_loadtim2.c; the full-screen distortion of `bhEff110`,
  the stage-7 small screen, Type 11 textures from `Ps2TextureMalloc`), three StoreImages of
  FBP 0 into a 512x512 TIM2 that the CPU only uploads again. Each was a partial frame and a
  game/render sync point. Now the game thread records a capture event instead
  (`pc/glue/texture_glue.c pc_rtex_capture`): the render thread blits the render target area
  at render resolution (widescreen margin and scale as `gs_rt_to_mem`) into a texture that
  becomes a native texture definition (`ntex_define_capture`), and the TIM2's image is never
  written except for a 16-byte marker (id, generation). `Send_Tim2_dataEx` of a TIM2 with a
  current marker binds the capture (no upload, GS memory under the slot is left as it was);
  a stale marker falls back to a plain upload. Replays draw with the executed frame's
  capture (replay-resources memo; replaced captures are deleted through `gs_tex_delete`).
  Frames using it are no longer split, so enhanced mode now interpolates them.
* Still emulated: GS register/state model, GS memory (for render-to-texture, CLUTs, the
  non-native textures), render targets (OpenGL FBOs per GS framebuffer address), VIF/DMA
  for the remaining packets.

### Cost per frame (measured 2026-10-09, RM_1310: 130k-triangle PC room)

Headless (`SDL_VIDEO_DRIVER=offscreen`, AMD iGPU radeonsi, scale 4 = 4584x2048 targets,
enhanced). What a frame costs, and what was removed:

* **Statistics line** (`RECVX_DUMP_FRAMES`, gs_gl.c): `prims` counts native strips and GS
  primitives since the last executed frame, i.e. the executed frame **plus its in-between
  replays** (enhanced mode redraws the frame for each in-between image), so it is 1x, 2x,
  3x... the frame's own count. `ms/frame avg` includes the dump stalls (every 30th frame
  all render targets are read back and saved: ~1 s at scale 4, ~33 ms per frame); the
  `--profile` CSV median is the better measure.
* **Per vertex** (game thread): VU0 `CALCPOINT` through the interpreter was 0.8 us, 166 ms a
  frame for the 205k vertices of the drawn nodes; native now (above, "VU microcode").
* **Per draw** (render thread): `glGetError` after every flush made the thread wait for
  Mesa's GL thread each time (~250 ms a frame at 1,200 flushes; the CPU was idle meanwhile).
  It is read every 64th executed frame now (errors stay set until read). The depth texture is attached to an FBO only
  when it changes (`attach_depth`).
* **Presentation**: with vsync on but a swap that returns at once (the offscreen driver,
  some compositors) the render loop presented as fast as the GPU allowed, and those
  full-window presents starved the frames (vanilla RM_1310: 197 ms a game frame). It now
  paces to the refresh when the swap does not wait, by waiting for the next game frame until
  the refresh is due (a frame that arrives is executed at once, its image waits for the
  refresh). An in-between replay is skipped when the next game frame is already queued.
  Whether the swap waits is its own duration (`gs_present` returns it, averaged over 8
  presents: below 1/8 refresh = it does not wait); presents then start on a fixed grid one
  refresh apart. The first version compared the time between presents instead, so after each
  new frame two presents went out ~0.5 ms apart and the in-between images were on screen for
  2% of the time (headless 60 Hz, `RECVX_TRACE_PRESENT`: t = 0.5, 1.0 at 0.5 ms, 1.5, 2.0 at
  0.1 ms); now t = 0.5, 1.0 each for 16.7 ms (vsync off: every 16.7 ms by time), RM_1310
  still 33.4 ms a game frame.
* **Frame rate limit** (F1 Display / PC SETTINGS, ini `fps_limit`: 0 off, 1..5 = 30/60/90/120/144):
  `render.c present_hz` caps the presents per second below the display's refresh (e.g. 60 on a
  175 Hz monitor, for the GPU cost of enhanced + native 3D); while limited, presents are always
  paced on that grid, also when the swap waits. Headless 60 Hz with limit 30: 1.00 present per
  game frame, 33.33 ms apart (off: 2.00, 16.67 ms). The game itself stays at 30.
* Remaining per strip: the strip clip test (`vu_strip_clip`), `pc_native_strip` and the
  stream events on the game thread; `native_draw`/`gs_draw_native`, the interpolation
  matching (`stream_match`, `gpos`) and the CLUT upload per texture bind (`ntex_bind`,
  `gs_exec_upload`) on the render thread. GPU: ~16 ms per pass for RM_1310 at scale 4.

| Room (headless, enhanced) | game frame, `--profile` median | game thread: logic / vu0 | render: exec / interp | stats: prims, flushes per executed frame |
|---|---|---|---|---|
| RM_1310 before | 208-305 ms | 43 / 165 ms | 61 / 105 ms (mostly waiting) | 68,847 / 605 |
| RM_1310 after | 33.4 ms (30 Hz) | 27 (incl. CALCPOINT) / 0.2 ms, 5.5 ms vblank wait | 6.6 / 3.4 ms | 68,847 / 605 |
| RM_1220 before | 54 ms | 41 (waiting) / 10 ms | 48 / 1 ms | 4,166 / 259 |
| RM_1220 after | 33.4 ms | 3 / 0 ms, 30 ms vblank wait | 2.7 / 0.5 ms | 4,166 / 259 |
| new game (start room) before | 33.4 ms | 4.6 / 4.7 ms | 17 / 18.5 ms | 2,670 / 376 |
| new game after | 33.4 ms | 1.4 / 5.4 ms | 1.5 / 0.4 ms | 2,670 / 376 |

Retail frames are unchanged: new game frames 300-900 and RM_1310 frames 1400-1560 dumped
by the base and the new build are byte-identical in vanilla mode, and in enhanced mode
except frames the unchanged base itself renders differently depending on run timing (base
run normally vs. with `RECVX_VU_CHECK=1` or `vsync = 0`: RM_1310 frame 1410, 4k pixels
around the player differ by ~3/255 right after the teleport; see "Determinism": not
reproduced since, the likely cause is the clip-plane stack read fixed there).

### In-between images (measured 2026-10-10)

An in-between image replays the executed frame's whole GS stream (`stream_replay`) with the
matched vertices blended, then presents. Where it cost (AMD iGPU, scale 4 widescreen
3416x2048 RGBA16F targets, room 0/6): GPU 14.2 ms = 9.2 draws (inherent: the same draws as
the executed frame), 3.0 copying every render target back after the replay (3 targets), 1.1
the start copy of the display target, 0.9 the window present; CPU (render thread) 0.4-0.7
ms in normal rooms, 3.5 ms in RM_1310 (per vertex: blend, clip, convert, upload; per strip:
the batch state comparison and a dummy triangle; per draw: ~21 uniforms; CLUT loads; the
matching, once per game frame, projected every native vertex 4 times). What changed, all
with byte-identical images:

* **Replay targets** (`gs_gl.c` "replay targets"): the replay's targets are swapped in, the
  executed frame's stay untouched (no copy back, no copy per executed frame). The executed
  frame notes per target whether its first access was an opaque write over an area (the
  clear after the swap, `Ps2ShadowStart`, `PS2_swap`'s copy with frame blend off: FIX 128,
  GL's Cs * 1 + Cd * 0) and only the rest (a few rows and columns) is copied in, from the
  executed frame's target, for the display target from its start image; otherwise the whole
  target. A partly executed frame (download) has no replays (it used to copy the previous
  frame's targets over the half-drawn ones).
* **Static strips**: a matched native strip whose blended fields are bitwise the same in both
  frames (fixed camera, nothing moving: blending returns the same values unless -0/Inf/NaN)
  is drawn unblended. The executed frame keeps the GL vertices of its native strips per event
  and uploads them once (`svbo`); a replay draws unblended strips from there (static runs in
  the batch, one draw call per run; batch boundaries unchanged, so alpha-fail two-pass draws
  stay as they were) instead of clipping, converting and uploading them again.
* Replays load no CLUT (nothing in a replay reads it); native strips set up the batch without
  a dummy triangle and skip the state comparison when no register, texture epoch or other
  batch changed (`gs_reg_serial`); uniforms are set only when their value changes; the
  matching projects each native vertex once per frame (`frame_spos`).

`RECVX_GPU_TIME=1` logs per executed frame and in-between image the render-thread CPU time
(without the swap) and GPU time (timestamps), and the in-between's parts; `RECVX_DISPLAY_HZ=1000`
with `vsync = 0` and `fps_limit = 0` presents as fast as possible. Headless, 2560x1440 window,
widescreen, native 3D and textures, Mesa defaults (glthread on); rate = in-between images per
second at that load (the game thread keeps 30 Hz unless the GPU is saturated):

| Room | scale | before: images/s, CPU, GPU ms | after: images/s, CPU, GPU ms |
|---|---|---|---|
| start (0/0) | 1 | 553, 0.33, 1.69 | 811, 0.07, 1.18 |
| start (0/0) | 4 | 51, 0.41, 14.8 | 68, 0.09, 10.9 |
| courtyard (0/3, rain, fire) | 1 | 519, 0.35, 1.81 | 729, 0.13, 1.30 |
| courtyard (0/3) | 4 | 46, 0.47, 14.1 | 75, 0.17, 10.0 |
| zombies (0/6) | 1 | 483, 0.43, 1.94 | 661, 0.21, 1.41 |
| zombies (0/6) | 4 | 54, 0.54, 14.2 | 72, 0.20, 9.8 |
| RM_1310 (130k triangles) | 1 | 222, 3.17, 3.34 | 492, 0.74, 1.84 |
| RM_1310 | 4 | 28, 4.58, 16.6 | 49, 1.09, 11.5 |

With Mesa's glthread off (the driver's work on the render thread, closer to a driver without a
GL thread), scale 1, CPU per in-between image: start 0.52 -> 0.18 ms, courtyard 0.56 -> 0.26,
0/6 0.67 -> 0.35, RM_1310 3.53 -> 0.99 (199 -> 611 images/s). At scale 1 the GPU column
includes waiting for commands. Not done: drawing parts that do not move once into a cached
layer (depth and blend order make that inexact), blending on the GPU (float rounding and the
CPU near/far clipping would change).

**2026-10-13** (all with byte-identical images: `layout_check.sh` 155/155 vanilla and 299/299
enhanced, RM_1310 41 + 82 dumps, 0/6 at 1000x750 (scaled present) with and without frame blend,
courtyard 1280x720 widescreen, in-between images at `RECVX_INTERP_T=0.5`):

* **Start image = previous final image**: the display target as a frame finds it is the
  previous frame's final image (replays draw into their own targets), so the two swap roles
  (`img[2]` in `gs_gl.c`) instead of a full-target copy per executed frame; the copy remains
  after a partial (download) frame or when the scale, the margins or the display buffer changed.
* The executed frame is no longer drawn into the window (only `RECVX_DUMP_FRAMES` needs that).
* **Same-value register writes** (TEST, ALPHA, TEX1, CLAMP, ... rewritten before most
  primitives) no longer end the batch (`gs.c flush_for`), except in a two-pass alpha-test
  batch, whose passes would interleave differently. Flushes per executed frame + one
  in-between: 0/6 654 -> 261, courtyard 1356 -> 1077, RM_1310 1071 -> 699, start room 243.
* Native texture identities (`texture_glue.c known[]`) are 8-way set-associative: the
  direct-mapped table evicted two TIM2s of a room on the same slot from each other every frame
  (0/6: 2, courtyard: 2-6 definitions recorded again per frame; now none in steady play).
* The present samples with its own LINEAR filter (a replay target sampled by a NEAREST draw
  kept that filter for the window); no image of the checks above changed.

`RECVX_GPU_TIME`, 2560x1440 headless as above (exec: rate, CPU, GPU ms; in-between: images/s,
GPU ms; at scale 4 the GPU is saturated, so fewer than 30 executed frames per second):

| Room, scale | before: exec /s, CPU, GPU; in-between /s, GPU | after |
|---|---|---|
| 0/6, 1 | 30, 2.07, 1.98; 583, 1.95 | 30, 1.80, 1.33; 635, 1.49 |
| 0/6, 4 | 20.6, 2.37, 12.4; 60, 10.2 | 24.2, 1.77, 10.9; 71, 9.9 |
| courtyard 0/3, 4 | 10.7, 1.96, 12.8; 72, 10.3-15.2 | 13.6, 1.3-1.55, 10.7; 82, 10.3 |
| RM_1310, 1 | 30, 9.3, 4.2; 441, 1.96 | 29-30, 9.1-10.9, 3.1-4.0; 469-513, 1.8 |
| RM_1310, 4 | 30, 10.0, 15.9; 37, 13.7 | 21.3, 9.9, 12.9; 57, 12.7 |

`--profile` medians (enhanced, 2560x1440, scale 3, vsync off): 0/6 r_exec 1.74 -> 1.39 ms, game
logic 1.81 -> 1.45 ms; RM_1310 r_exec 5.57 -> 5.44, r_interp 4.75 -> 4.50 ms.

## Data model and memory

* **32-bit (x86) for now.** Disc data contains 32-bit pointers that are relocated in place
  (`binfunc.c` and friends), and the platform code stores host pointers in 32-bit DMA tag fields.
  A 64-bit build needs those structures to keep 32-bit pointer slots (all PS2-visible memory in
  a 4 GB window) — planned after the port is complete, with this build as the reference.
* **EE `long` is 64-bit**: `pc_prelude.h` includes the host headers, then `#define long long long`.
* **Address window**: the executable is linked at `0x30000000` so the EE's uncached/accelerated
  address aliases (`addr | 0x30000000`) are identities and DMA tag addresses fit in 31 bits.
  The 16 KB scratchpad is mapped at its real address, `0x70000000`.
* **The window is 256 MB**: all static game memory (the room arena, texture heap, spawn pool, ...)
  is the image's `.bss` and must end below 0x40000000, checked at start (`ps2/memory.c`). 10.4 MB
  are left on Linux, 15 MB on Windows (docs/MAPS.md 7.1). Host allocations need not be in it.
* String literals are writable (the game writes into them; the EE has no MMU protection).

## 1:1 notes

* Game logic is compiled unchanged. Where the decomp relied on MWCC behaviour that standard C
  does not have, the C was rewritten to the evidently intended semantics:
  * `*mkaP->key++` on array members (en03.c, en23.c) -> `key[0]`, `key[1]`, `key[2]`.
  * implicit returns of asm functions through `v0`/`$f0` (njSin, njCos, Ps2BitCount, ...).
  * C functions without a `return` on some path, where MWCC returned whatever was in `v0`:
    checked against the retail code and fixed where the value is used (`docs/missing-returns.txt`).
* PS2 FPU/VU arithmetic is not IEEE (no denormals, no Inf/NaN, truncating rounding). Results
  are clamped like the VU (`vu_clampf`) and the build uses SSE single precision without FMA
  contraction (`-ffp-contract=off`), but bit-exactness with the EE FPU is not guaranteed.
* Things that were empty on the retail build stay empty (e.g. `ADXT_Pause`).
* `ps2_sg_sd.c Volume_Control`, a volume fade up (`sdShotSetVol`, MIDI): retail (0x2DC814) writes
  the current volume into the fade's *target* and returns the caller's stale `$f20` (a register
  it never sets on that path); the fade down computes and returns it. The PC returns the fade
  up's current volume the same way and leaves the target alone (before: 0, so a MIDI fade up
  ended silent).
* `player.c bhCPM0_damage`: a damage level above 2 (a Black Widow's grab level 4/5 while another
  enemy's hit took over) is taken as 2; there are three levels (`PlyTrsZ`, en29's `dmg_mde < 3`).

### Checked against the retail ELF

These were hand-written asm in the original, so the decomp's blocks were not reliable. They were
translated from the retail code instead: `bhObj005` (objitm.c), `njMirror`, `_ClipInter`,
`ScissorTriangle`, and `KazuSet` (whose stale registers produce an empty range).

## Determinism

The same build run twice with the same input, and two builds of the same code with a
different binary layout, must give byte-identical frames, in vanilla and in enhanced mode.
Game logic is stepped per game frame, so wall-clock timing does not enter it; what the
retail code reads from memory it never wrote (stale stack, bytes past an array) does, and on
the PC such bytes are often addresses, which change with the layout (code, data and the
game heap `Ps2_malloc_mem` move together), with ASLR (thread stacks) or with what ran before
on the stack.

**Check** (`pc/tools/layout_check.sh BIN_A BIN_B OUT [ISO]`): runs both binaries headless with
the same input (new game, teleport to 0/6 with the handgun, walking, shooting, idling while
two zombies change motions) in vanilla and enhanced mode (in-between images at
`RECVX_INTERP_T=0.5`), dumps every 10th frame up to 1800 and compares them byte for byte; exit
status 1 on any difference. Second layout: configure another tree with
`-DRECVX_LAYOUT_PAD=20500` (`pc/tests/layout_pad.c`, linked first: that many unused bytes
before all code, data and bss). Run to run: pass the same binary twice. About 2 minutes.
Not a ctest: it needs the disc image and a GPU.

**Fixed** (2026-10-09; both reads are retail bugs, so the PS2's own values were stale data too):

| Where | What retail reads | Effect on the PC | Now |
|---|---|---|---|
| `Motion.c` (`bhSetMotion`, `bhGetObjMotion`) | `bhGetFrameNum` returns the new motion's frame *count* for any frame >= 1, so `bhEne_ChgMtn(..., bhGetFrameNum(...) * 65536, ...)` (zombie walk changes, en01.c and others) starts one past the last key; that frame reads one key past each array: the next joint's keys, and after the last array the motion's table of key pointers | the last joint blended towards key bytes that are heap addresses; with another layout the `sum > 0` flip went the other way and the joint took another path for the 20 blend frames (0/6: a zombie's arm from game frame 1473, visible from 1565; the "zombie pose" and "~1600 px around the characters" differences between builds noted in COOP.md and ACTORS.md) | an array whose past-the-end read would come from the pointer table is swapped, for the duration of the reads, for a copy ending in its first key (frame `frm_num` of a looping motion is its frame 0); every other read stays as retail. Attract demo and 0/6 frames identical to the base until the first such read |
| `ps2_NaGraphics3D.c Ps2SetPlane` | `*(long128*)p->pos = *(long128*)&v0`: `v0` is a pointer parameter; retail spills it and copies the pointer plus 12 stale stack bytes as the plane's point (checked in the retail ELF at 0x2DFA00) | the cone planes of `CalcIntersectionCone` (near-plane clipping of `njDrawLine3D`/`Ex`: particle lines, line effects, search light, laser sight) went through game-thread stack addresses read as floats (about -1e19), which differ with ASLR from run to run and with the layout | the point is `v0`, the camera point (0, 0, -dist), which the planes are meant to pass through (retail's x is ~0 too: a stack address read as a float) |

Results after the fixes (two layouts and the same build twice; 0/6 scenario 156 vanilla, 300
enhanced dumps; attract demo 448 dumps per mode): all byte-identical, also with
`RECVX_VU_CHECK=1` and `vsync = 0` in enhanced mode, and in RM_1310 (mod room). Executed
frames in enhanced mode do not depend on in-between replays being skipped (the render thread
skips one when the next game frame is already queued).

**Real time, by design** (as on the PS2, which runs these off the display and the sound
hardware): movies play by vblank count (`RECVX_SKIP_MOVIES` counts game frames instead, so
test runs repeat; before, the skipped movie took 30 or 31 game frames and shifted the title
sequence and the demo after it by a frame now and then), ADX streams report their position
and end from the audio stream (`ADXT_GetStat`/`ADXT_GetTime`: voice-timed waits), the play
time `sys->time` counts vblanks, and an in-between image's *t* follows the clock unless
`RECVX_INTERP_T` fixes it. Not determinism bugs; a run that waits on them can shift by frames.

Also not fixed: other reads of stale or out-of-range data the scenarios above do not reach.
Searched: `&` of array parameters (`Ps2SetPlane` was the only one); uninitialized locals are
zero (`-ftrivial-auto-var-init=zero`); `rand()` is called only by the game thread.

## Logging and profiling

* `--log[=file]` copies all output (the game's diagnostics and the port's messages) to a log
  file (default `recvx.log`) with a timestamp per line; a crash appends a backtrace.
* `--profile[=file]` writes one CSV line per game frame (default `recvx_profile.csv`): total
  frame time and exclusive time in game logic, VU0, VU1, GS emulation + GL submission, the
  interpolation (matching and replays), presentation and idle wait. F1 -> Performance shows the same live.
* `--profile-samples=file` samples the CPU at 1 kHz; `pc/tools/profile.py build/pc/recvx file`
  summarises it by function. Each thread has its own timer on its own CPU clock (a
  process-wide `ITIMER_PROF` signal mostly hit threads asleep in a syscall); samples are
  tagged `r` (render/main) or `g` (game): `profile.py build/pc/recvx file 25 g` for one
  thread. `file.maps` (`/proc/self/maps` at exit) lets it name the library of samples
  outside the executable (`libc.so.6`: memcpy or a syscall, `libgallium`: the GL driver).
  Buffers are allocated only when sampling (not in the PS2 address window).
* Render-thread wall time in `r_exec`/`r_interp` with low CPU use (thread `oncpu` from
  `/proc/<pid>/task/*/schedstat`) is time waiting for the GL driver or GPU.
* `room: RM_xxxx texture table has N list(s) more/fewer than the room reads` (`room.c`, mod
  rooms): `bhSetRoom` takes the texture lists in table order, one per textured object or item
  *id* and one per effect id, and skips none, so a surplus list moves every later effect onto
  its neighbour's atlas and palette (2026-10-09: the graveyard mod with a second id-900 door
  list drew boxed explosions instead of the truck's short-circuit sparks and wrong rain
  splashes). `pc/tools/assets.py dependencies` applies the same rule (all 205 retail rooms
  match) and the door patchers (`build_village.py`, `re4/graveyard_link.py`) check with it.

## Debugging switches (environment variables)

| Variable | Effect |
|---|---|
| `RECVX_DUMP_FRAMES=dir` | every 30th frame (and its interpolated predecessor) + all render targets as BMP, frame statistics |
| `RECVX_DUMP_EVERY=n` | with `RECVX_DUMP_FRAMES`: also every n-th frame, its in-between and window (`RECVX_DUMP_OVERLAY`) images too (render targets and statistics stay at every 30th) |
| `RECVX_DUMP_FROM=n` | with `RECVX_DUMP_FRAMES`: no dumps before presented frame n (each dump stalls the GPU; headless runs reach the room much sooner) |
| `RECVX_EFF_TRACE=1` | log the live effects each frame: `id.type@slot`, `*` if queued for drawing, `h` if hidden, view z (`pc/glue/tps_glue.c`) |
| `RECVX_TRACE_TASKS=1` | log game task (`sys->tk_flg`) changes |
| `RECVX_TRACE_DMA=1` | log GIF DMA chain tags |
| `RECVX_DUMP_TEX=dir` | dump decoded textures |
| `RECVX_DUMP_CALCPOINT=n`, `RECVX_DUMP_VU1=n` | dump VU0/VU1 state at the n-th call |
| `RECVX_INPUT="frame:buttons,..."` | scripted pad input by presented frame, e.g. `200:start,205:-` |
| `RECVX_WATCHDOG=sec` | print a backtrace after *sec* seconds and exit |
| `RECVX_TRACE_SND=1` | log sound driver commands and SE key-ons |
| `RECVX_SKIP_MOVIES=1` | movies end after 30 game frames (one second) without decoding (testing; counted in game frames, not vblanks, so test runs repeat exactly) |
| `RECVX_SHOW_OVERLAY=1` | open the F1 overlay at start (screenshots); `=3` the co-op window (F3) |
| `RECVX_TRACE_MC=1` | log memory card commands and results |
| `RECVX_REPLAY_CHECK=1` (or `=n`) | interpolation self-test (with `--enhanced`): every interpolated frame is also replayed at t = 1, which must reproduce it; logs `replaycheck frame N: P px differ` (max channel difference > 24, or > n) and saves (at most every 30th frame) `replaycheck_N_{exec,replay,diff}.bmp` (diff: differing pixels red) to `RECVX_REPLAY_CHECK_DIR` (or `RECVX_DUMP_FRAMES`) when over 0.2% (`pc/src/gs/replay_check.c`) |
| `RECVX_REPLAY_RTSTART=0` | enhanced: in-between frames start from the executed frame's display image instead of the frame's starting one (old behaviour, pre-echo of the frame blend) |
| `RECVX_INTERP_T=t` | enhanced, test aid: the first refresh after each game frame shows the in-between frame at *t* (then the frame itself), so `RECVX_DUMP_FRAMES` dumps comparable in-between images whatever the run speed |
| `RECVX_INTERP_STATS=1` | per-frame interpolation matching (meshes, vertices, scene changes) |
| `RECVX_TRACE_PRESENT=file` | one line per present: executed frame, t, start time and swap duration (us) |
| `RECVX_GPU_TIME=1` | every 2 s: executed frames and in-between images per second, render-thread CPU and GPU ms each (GL timestamps, read back without a stall), the in-between's GPU parts (start copies, replay draws, present, copy back) |
| `RECVX_DISPLAY_HZ=n` | test aid: assume an n Hz display (e.g. 1000 with `vsync = 0`: present as fast as possible) |
| `RECVX_NATIVE3D=0/1`, `RECVX_NATIVE_TEX=0/1` | rendering rework paths off/on (native 3D/2D triangles; native TIM2 textures) |
| `RECVX_TRACE_NTEX=1` | log native texture binds, unbinds and misses, and render-texture captures |
| `RECVX_NATIVE_RTEX=0` | `StoreRenderTex` through the emulated download (partial frame, glReadPixels, game waits) instead of the GPU capture |
| `RECVX_EFFECT=f:effno:type:mode1` | test aid: spawn effect *effno* at the player at game frame f, set its mode1 two frames later (`60:110:0:1`: full-screen distortion, `pc/glue/cheat_glue.c`) |
| `RECVX_VU_CHECK=1` / `RECVX_VU_INTERP=1` | compare static VU code against the interpreter on every call / use the interpreter |
| `RECVX_PROFILE=file` | sample each thread's CPU time (1 kHz) and write addresses (tagged `r`/`g`) and `file.maps` at exit; summarise with `pc/tools/profile.py build/pc/recvx file [top] [r\|g]` |
| `RECVX_COOP_TRACE=1` | log every co-op pose sent/received |
| `RECVX_COOP_ECHO=n` | co-op without a network: the partner is our own pose n frames ago |
| `RECVX_TELEPORT=f:stg:rom:pos,...` / `RECVX_EQUIP=f:item:count` | test aid: teleport / equip a weapon at game frame f (`pc/glue/cheat_glue.c`); `;f:item:count` after it equips again later (a weapon switch) |
| `RECVX_APPEAR=f:chr,...` | test aid: set "You appear as" (`coop_char` value) at game frame f (`pc/glue/cheat_glue.c`) |
| `RECVX_RETRY=f,...` / `RECVX_HP=f:hp,...` | test aid: a retry point (`bhPushGameData`, as the scripts' ReTryPointSet) at game frame f, so a game over's Continue resumes in that room / our health set at game frame f (`pc/glue/cheat_glue.c`) |
| `RECVX_INF_AMMO=1` | test aid: infinite ammo cheat on |
| `RECVX_SKIP_CINE=n` | test aid: skip every cutscene after n game frames, as its skip button (`cb_flg 0x10000000`) |
| `RECVX_FLAGS="set:bit,..."` | test aid: set story flags at the first game frame (set numbers as in the room scripts: 1 `ev_flg`, 2 `ky_flg`, 3 `ed_flg`, 7 `it_flg`, 8 `mp_flg`) |
| `RECVX_COOP_KILL="hostframe:slot,..."` / `RECVX_COOP_GRANT="item:stage:room[:floor:x:y:z],..."` | co-op test aids: a lethal hit on our enemy in that slot (a puppet's goes to the host); as if the partner got these items from events in that room (docs/COOP.md "Protocol 15") |
| `RECVX_COOP_KICK=n` | co-op test aid (host): "Remove stuck player" at co-op frame n (docs/COOP.md "Protocol 16") |
| `RECVX_COOP_TAKE=n` | co-op test aid: n frames after a partner's event item is shown here, stand 8 units from it facing it; n frames later on it (cross takes it) |
| `RECVX_COOP_TSCREEN="hostframe:act,..."` / `RECVX_COOP_TS_LOSE=s<n>\|d<n>` / `RECVX_TRADE_TRACE=1` | co-op trade screen test aids: k trade key, s<cell> select, r confirm, l leave, d Disconnect, i log the inventory, c<cell>/cp<cell> cursor, a confirm on the cursor, b cancel, w<cell> request, o offer the requested, x<cell> a wrong request, P<hex> pad presses, M<x>/<y>[l\|r] mouse (protocol 17); the n-th swap commit (host) / answer (guest) not sent; log the enemies' frozen state and a hash of their positions and motions every 10 frames (docs/COOP.md "Trade screen") |
| `RECVX_KNIFE_TRACE=1` / `RECVX_KNIFE_DROP=1` | quick knife test aids: log its state, the player's mode, the inventory and the room's enemy health every 15 frames while the key is held; drawing the knife empties the previous weapon's slot (the weapon-gone case) |
| `RECVX_ENEMY_EXTRA=0..3`, `RECVX_ENEMY_PLACE=0/1`, `RECVX_ENEMY_RANDOM=0/1`, `RECVX_ENEMY_SEED=n` | test aid: the enemy options (extra enemies, rearranged placements, randomizer, seed; ini `enemy_extra/place/random/seed`) over the settings (docs/ENEMIES.md section 7) |
| `RECVX_ENEMY_TRACE=dist` | test aid: one stderr line per step of every living enemy within `dist` units (10 = 1 m) of a player: target, distances, modes, motion, both players' hp and mode (docs/COOP.md "Enemy targets: one victim per attack") |
| `RECVX_COOP_STATS=1` | log every 300 co-op frames what was sent: datagrams per frame, average/maximum datagram size, bytes per frame, events and enemy snapshots per frame, ping, rejected datagrams, the path, and the busiest frame (datagrams and bytes on the wire) |
| `RECVX_COOP_LOSS=pct` | test aid: drop that percentage of our co-op datagrams at random (own generator, the game's `rand()` stays untouched); with `RECVX_COOP_DROP` for stalls (docs/COOP.md, frozen zombies) |
| `RECVX_COOP_CODE=CODE` | test aid: the host's co-op join code (8 of `A-Z` without I/O and `2-9`) instead of a random one |
| `RECVX_COOP_FORCE_RELAY=1` / `RECVX_COOP_NO_PUNCH=1` / `RECVX_COOP_PUNCH_MS=n` / `RECVX_COOP_E2E=0` | co-op through the server (docs/COOP_SERVER.md), test aids: always the relay; drop received hole punches (relay fallback); punch for n ms before the relay (default 2000); no end-to-end encryption (the host sends plaintext, a joiner accepts a plaintext host; never with a password) |
| `RECVX_COOP_TEST_KILL=frames:levelmask` | test aid: end the replayed world event's tasks of those levels that many frames into a replay (docs/COOP.md, protocol 11) |
| `RECVX_SPAWN=f:id[.mdlver][@dist],...` | test aid: the F2 enemy spawner at game frame f (enemy type id, optional model version and distance); traces the spawned enemies (state every 30 game frames, HP changes, hits on the player; docs/ENEMIES.md) |
| `RECVX_SPAWN_SURVEY="stg:rom,..."` (`RECVX_SPAWN_SURVEY_ALL=1`) | test aid: tries every spawner entry in each room (each realized and undone before its Init; with `_ALL` they stay, so the room shows how many fit), logs `survey:` lines with each result and the room's budgets, then the free address space, and exits (docs/ENEMIES.md 6) |
| `RECVX_NOCLIP=f:mode,...` | test aid: noclip mode 0/1/2 from game frame f; logs room, position and floor every 15 game frames |
| `RECVX_MENU_MOUSE=f:x:y[:act],...` / `RECVX_MENU_TRACE=1` | test aid: scripted mouse in the inventory screens; log menu clicks (see "Mouse in menus") |
| `RECVX_DUMP_OVERLAY=1` | with `RECVX_DUMP_FRAMES`: every 30th frame (`RECVX_DUMP_EVERY`, from `RECVX_DUMP_FROM`) also as shown in the window, with the overlay (`win_<frame>.bmp`) |
| `RECVX_LOBBY_KEYS="f:text,..."` / `RECVX_TEST_SAVE=f:slot,...` / `RECVX_TEST_JOIN=n` / `RECVX_TEST_STOP=n` / `RECVX_TEST_TITLE=n` / `RECVX_TEST_EXTRA=1` | co-op lobby test aids: type into a lobby text field, write a save file, press F3's Join / Disconnect, back to the title from a game, the 5-row title menu (docs/COOP.md "Lobby screen and name tags") |
| `RECVX_KEYS="f:key+key,f:-,f:@x:y"` | test aid: hold keyboard keys (SDL names, `_` for a space) from presented frame f on, read per game frame like `RECVX_INPUT` and sent as key events (captures, hotkeys, the overlay); `@x:y` clicks the left button at window pixel x/y (as events: the overlay) |
| `RECVX_VPAD="f:input+input,..."` / `RECVX_VPAD_TYPE=xbox\|ps5\|generic` | test aid: a virtual SDL gamepad (default Xbox One) holding the inputs (ini spelling: `south`, `dpup`, `+lefttrigger`) from frame f |
| `RECVX_UNFOCUS="f:1,f:0"` | test aid: hide the window at presented frame f (SDL drops its keyboard focus, as another window taking it), `0` shows it again; with the offscreen driver the only way to test focus loss |
| `RECVX_PAD_TRACE=1` | log the DS2 buttons of the pad report when they change (`pad: frame N buttons xxxx`) |
| `RECVX_EXIT_FRAME=n` | test aid: exit at presented frame n |
| `RECVX_STRESS_SETTINGS=1` | test aid: music and effect volumes change every pumped frame (applied each time; `recvx.ini` saved 500 ms after the last change), against audio stream start/stop on the game thread |
| `RECVX_TPS_MOUSE=frame:dx:dy,...` / `RECVX_TPS_TRACE=1/2` | third person: scripted mouse; log the free aim (2: camera to the locked enemy) (docs/THIRDPERSON.md) |
| `RECVX_ATMOS_TRACE=1` | third person: log each game frame's cut, the game's fog/lights/filter and what "smooth" light and fog shows (docs/THIRDPERSON.md "Light and fog") |
| `RECVX_MODS=dir` | mods folder (default `./mods`): files there replace disc files, new room names add rooms (docs/MAPS.md) |
| `SDL_VIDEO_DRIVER=offscreen` | headless run |

## Status

| Area | State |
|---|---|
| Build, asm translation, linking | done |
| Boot, menus, memory card, title, attract demo with 3D | working |
| GS renderer | working; known artifacts in some effects |
| Audio: ADX streams (music, voices) | working |
| Audio: TSNDDRV sound effects | working (TQ layer from the retail IRX, HD/BD synth on modelled SPU2 voices, SPU2 reverb with presets read from the disc) |
| Audio: TSNDDRV MIDI (SQ: ambient/door/event sounds) | working (sequencer; loops per MODMIDI.IRX: CC99 0/1 + CC6 id + CC38 count; compressed key-ons) |
| FMV (PSS) | working: own MPEG-2 decoder + PCM soundtrack; frames drawn by the game's original upload/sprite code |
| F1 overlay (Dear ImGui), settings file, runtime internal resolution | done |
| PC SETTINGS in the game's OPTION menu, control rebinding (keyboard/mouse, gamepad) | done (headless: keyboard, mouse, virtual SDL gamepad; real pads untested) |
| Widescreen/ultrawide (hor+, any window aspect up to 4:1) | done; 2D/menus stay 4:3 centred |
| Enhanced mode (display-rate frames, `--enhanced`) | working: engine-tagged meshes, exact matching, cut-aware; game logic unchanged at 30 Hz |
| VU0/VU1 microcode | statically compiled to C (interpreter kept as reference) |
| Windows build | working under Wine (mingw-w64 GCC cross build, see "Windows build"); untested on real Windows |
| 64-bit | planned after the 32-bit port is complete |
