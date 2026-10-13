# Changelog

## 0.1.0 (unreleased)

First release. Native Linux and Windows (32-bit, i686) port of Resident Evil Code: Veronica X
(PS2, SLUS-20184), built on the recvx-decomp decompilation. No game data is included; you need your
own disc image.
Co-op protocol version 18, rendezvous protocol version 3.

### Platform and port

- Game code, including the original PS2 Ninja/Shinobi layer, compiled from the decomp; DMA, VIF,
  GIF, GS, CD/DVD, pad, memory card and SIF RPC emulated underneath.
- VU0/VU1 microcode compiled statically to C (interpreter kept as a reference, `RECVX_VU_CHECK`
  compares both); hand-written fast paths for vertex lighting and strip clipping.
- Audio: ADX streams, TSNDDRV sound effects on modelled SPU2 voices with reverb, MIDI sequencer.
- FMV playback with its own MPEG-2 decoder.
- Windows build cross-compiled with mingw-w64 GCC (static, large-address-aware); runs the same
  saves and co-op sessions as Linux.
- First start without a disc image opens a file selector and remembers the choice (`iso` in
  `recvx.ini`).
- Atomic save writes, UTF-8 paths on Windows, glibc compatibility shim so the Linux binary runs on
  glibc 2.34 and newer.

### Renderer

- OpenGL renderer with runtime internal resolution, fullscreen (F11) and an FPS counter.
- Widescreen and ultrawide (hor+, up to 4:1): 3D fills the margins, room parts hidden for 4:3
  cameras are shown when they lie outside the 4:3 view, 2D and menus stay 4:3 centred; fades,
  pause darkening and the screen saver cover the whole view.
- Native 3D path for chunk models and ordering-table primitives, perspective-correct colour and
  fog; native TIM2 textures with real CLUT uploads and memory caps; render-to-texture effects
  captured on the GPU instead of read back.
- Enhanced mode: in-between frames at display rate (game logic stays at 30 Hz), blended from the
  executed frame's native geometry and resources; cheaper replays into a second render-target set.
- Frame rate limit (off/30/60/90/120/144), present pacing on the display refresh, optional
  "Frame blend (motion trail)", optional off-screen model culling.
- Larger PC room limits (bigger room arena, vertex and primitive buffers) for custom maps.

### Co-op

- Online two-player co-op (F3 or MULTIPLAYER on the title menu): host-authoritative enemies, flag
  and script-event sync, cutscene items and levers shared, independent rooms with optional follow.
- Lobby in the game's style, in steps (connection, host/join, game rules), with 3D previews,
  player names, name tags, ping display and a "Remove stuck player" action for the host.
- Shared or individual loot, own or shared item box, per-player inventories kept by the host,
  event item gifts for a partner who missed the event.
- Trade screen with both inventories, item requests and atomic swaps; optional enemy freeze while
  trading.
- Partner shown as any chosen model, with its weapon sounds, voice, lighter and effects; enemy
  effects (acid, vomit, grenades) shown and harmful on both sides.
- Enemies choose the nearer free player; zombie grabs work on both players.
- Direct play via `ip:port#CODE` with UPnP/NAT-PMP port mapping, or via a rendezvous server with
  hole punching and relay fallback; end-to-end encryption (X25519 + XChaCha20-Poly1305) in every
  server session.
- Rejoin with a per-install player ID; lost sessions time out; all datagrams validated before use.
- Local co-op on one PC: per-window input source (keyboard/mouse or a chosen gamepad).

### Server

- `recvx-server` rendezvous and relay server (C) with server-assigned room codes, Argon2id room
  passwords, authenticated relay frames, per-address and global rate limits and relay budgets.
- Config file with SIGHUP reload, Prometheus-style status metrics, anonymised logging, systemd
  unit, Dockerfile and a `recvx-client` CLI.

### Third person

- Over-the-shoulder camera (F5, `--third-person`) with WASD/mouse controls and free aim; spine
  and chest bend to the aim pitch.
- Two mouse/keyboard camera styles: behind the character, or a free orbit camera with
  camera-relative movement.
- Light and fog changes between camera cuts fade smoothly; effects, sound panning and culling use
  the third-person view.

### Actors (model swaps, voices)

- "You appear as": play as Claire, Chris, Steve, Wesker, costumes, or one of dozens of enemy and cast
  models retargeted onto the player skeleton; the choice applies to cutscene doubles too.
- Gender-matched animations: male models play the male equivalent of each motion.
- Replacement models hold weapons and the lighter in their own hands; shots leave from the muzzle.
- Optional talking faces for swapped models in cutscenes.
- Player voices follow the chosen appearance (Claire, Chris, Steve or Wesker voice banks).

### Enemies

- F2 enemy spawner for any type, loaded from its source room on demand with its sounds and
  effect textures.
- Enemy options: extra enemies, rearranged placements, randomizer with seed; synced to the co-op
  partner.
- Higher spawner limits (texture heap, spawn pool, helper slots).

### Maps and mods

- Mods folder (`RECVX_MODS`, default `./mods`) for replacement and new rooms.
- RDX room tooling: `rdx.py` (read and patch rooms), `rdxbuild.py` (build rooms from OBJ/PNG
  geometry, props, items, scripts, doors and sound banks), a "pc" budget for large rooms.
- Slope walking in rooms flagged for it, for terrain-based custom maps.
- Asset catalogue and kitbash tools to build new rooms from the player's own disc: the Quarantine
  Annex area and a village area as examples.
- RE4 village conversion tooling for a player's own RE4 export (materials mapped to CVX textures).
- Rooms built from game data must not be shared.

### Input and settings

- F1 settings overlay and a native PC SETTINGS page in the game's OPTION menu, both driven by one
  settings table and saved to `recvx.ini`.
- Rebinding for keyboard/mouse and gamepad (two slots per action).
- Mouse in the inventory and menus.
- Quick knife (hold Space / right trigger) and a trade request key.
- Skip intro and skip door animations (on by default on first start).
- F2 cheat menu: heal, items, infinite ammo, teleport, noclip with fly mode, enemy spawn.

### Tooling and tests

- ctest suites for the co-op and rendezvous protocols (including fuzzing), networking, VU,
  bindings, appearances, file I/O and movies.
- Determinism checks: `layout_check.sh` compares frames across binary layouts; replay-vs-executed
  frame checker; native vs emulated renderer comparison.
- Profiler (`--profile`), GPU timing, extensive `RECVX_*` debug and test switches.
- `decomp_warnings.py` regenerates the lists of missing returns and uninitialised locals.

### Fixes

- Decomp code made deterministic: a retail out-of-bounds motion key read and a stack-garbage near
  plane point replaced by the intended values.
- Ordering-table depth sorting fixed (transparency sorting was missing).
- MIDI volume fade-up no longer ends silent (follows the retail code's intent).
- C functions without a return checked against the retail ELF; several hand-written asm routines
  translated from the retail code.
- Idle screen dimming (the game's screen saver) is lifted by PC input, as a pad press does on PS2.
- Crash fixes: game over with a swapped model, lobby open/close, teleport during a grab, texture
  queue recursion, port mapping use-after-free.
- Graveyard effects drawn with the right textures after a door patch (room texture-list check).

Developed with AI coding agents (Claude; OpenAI Codex for parts of the map tooling), see README.
