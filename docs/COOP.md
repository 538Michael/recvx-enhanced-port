# Online co-op (addon, off by default)

Feasibility analysis and staged plan. Co-op is an optional addon: with it off, the game
behaves exactly as without it (every hook is `#ifdef TARGET_PC` + `if (coop_on)`).

## How to play co-op

Two players, each with their own copy of the game and the same build, connected over UDP
(port 7140 by default; the host must be reachable: see "Playing over the internet").

1. Both start the game and choose **MULTIPLAYER** in the title menu (below OPTION; keyboard,
   gamepad or mouse). The lobby shows your character turning in front of the title picture.
2. **NAME**: what your partner sees above your head and in the lobby (Enter or the confirm button
   to edit; type it, or with a gamepad change the letter with up/down and move with left/right).
   **MODEL** ("You appear as"): Claire, Chris, Steve, Wesker, Claire's second costume, the
   character you play, then 50 story-cast, creature and zombie looks (docs/ACTORS.md 8, "The list"): your partner sees you as
   that model, and so do you.
3. The lobby goes in steps. **First: how to connect and whether you host or join.** **CONNECT**
   *Direct (P2P)* (the partner connects straight to the host's address; the host's port must be
   reachable) or *Via server* (a co-op server puts you together by a room code; it tries a direct
   connection first and relays only when that fails), then **HOST A GAME** or **JOIN A GAME**.
   **Then only what that choice needs**, with NAME and MODEL and the game rows:
   * Host, Direct: **PORT**, NAME, MODEL, **START** (a new game - normal, easy, very easy - or one
     of your memory card saves), **GAME RULES**, **OPEN THE LOBBY**. The info lines show this PC's
     LAN address, the public address, the router (UPnP) result and the **join code** of this run
     (8 letters and digits, e.g. `K7QM-3XRA`); **COPY JOIN ADDRESS** puts `ip:port#CODE` on the
     clipboard.
   * Host, Via server: **SERVER**, **PASSWORD** (optional), **RELAY**, NAME, MODEL, START, GAME
     RULES, OPEN THE LOBBY; then the room code (info line and the **COPY ROOM CODE** row).
   * Join, Direct: **ADDRESS** (`ip:port#CODE` as the host copied it, Ctrl+V pastes; without the
     code the host does not answer), NAME, MODEL, **ITEMS** (*Saved with the host*: what the host
     kept for you from your last visit - once you are in its lobby the row says "(available)" or
     "(none)"; *New inventory*; or the items of one of your memory card saves), **FOLLOW**, **JOIN**.
   * Join, Via server: **SERVER**, **CODE** (the host's room code), **PASSWORD**, **RELAY**, NAME,
     MODEL, ITEMS, FOLLOW, JOIN.
   **GAME RULES** (host) is a page of its own: **LOOT** (shared: an item one takes goes to both,
   one item box for both; individual: each takes their own copy and has their own item box; keys
   and story progress stay shared), FREEZE ON TRADE and the enemy options. The joining player
   sees the host's enemy options there (THE HOST'S RULES, read-only) once connected.
   Both now see each other in the lobby, with names and models, and what the other chose. The
   host picks **START THE GAME**: the partner enters the host's game in the host's room once the
   host has control. **BACK** (or cancel, or a right click) goes one step back: from the rules to
   the lobby, from the lobby to the first step (an open lobby stays open; CONNECT and the other
   role are dimmed meanwhile). BACK on the first step returns to the title menu and closes the
   session: hosting stops, the port and its router mapping are freed, and the partner sees "The
   host closed the lobby" (or "<name> left"). Only starting the game keeps it open. With a partner
   in the lobby (or one that stopped answering) the host also has **REMOVE STUCK PLAYER**: it ends
   the session with that partner and frees its place; the same code works again at once. (Hosting
   from F3 at the title and choosing New Game or Load Game in the title menu still works.)
   A port another program (or another copy of the game) holds is refused: "Port 7140 is already in
   use - choose another port" (highlighted under the rows); change PORT and open the lobby again.
   The help line under the rows says what the selected row is for.
   **F3** has the same settings as a compact window, also in game, Connection first (the server
   fields only when it is not Direct), **Items when joining** (the ITEMS row's choices, plus *My
   current game*: joining a running game from your own game through F3 keeps your own items, as
   before), **Remember partner inventories (host)** and, for the host, **Remove stuck player**.
4. In game, F3 shows the status (ping, the partner's room). **Follow host** (the joining player's
   choice: lobby row FOLLOW, F3, F1; ini `coop_follow`): go through every door the host takes, or
   stay and explore on your own; a story cutscene of the host and a story character switch still
   bring you to the host ("Follow host" below).
5. What one player does to the world happens for both, once: a lever pulled, a switch pushed, a
   door unlocked, zombies rising, a cutscene started by picking something up. In the same room
   both see it; a partner elsewhere finds the room changed when it gets there.
6. Each player has an inventory of their own: you join with the ITEMS choice (by default what the
   host kept for you, else a new inventory), and the host keeps what you have when you leave - also
   when your game crashes: start it again and join, you get your place and your items back at once
   ("Protocol 16" below). Shared loot: an item either of you takes
   goes to both, and you share one item box. Individual loot: each takes their own copy and has
   their own item box (keys and story progress stay shared either way).
7. **Trade** (when you stand next to each other, about 3 m): press **T** (gamepad: **left stick
   click**; rebindable, "Trade request (co-op)") - your partner hears a chime and reads "<you>
   wants to trade - press T / Left stick press" at the top of the screen and above your name; its
   press opens the **trade screen** for both: your inventory on the left, the partner's on the
   right, with the game's item icons and counts. Move the cursor to a slot of your own (an empty
   one to only take) and confirm to offer it; confirm it again (or CONFIRM) to be ready; when both
   are ready on the same pair the items swap. The offer stays while you move the cursor, hover with
   the mouse or switch windows; only Cancel (the key, a right click or the CANCEL button) takes it
   back. On the partner's side, confirm a slot to **request** that item: the partner sees it marked
   and "<you> requests: Handgun Bullets" (a click on that line offers it). Cancel steps back one
   thing at a time (request, offer, the partner's request: declined) and then leaves; LEAVE or
   walking apart closes it for both, unchanged. While both are in it the room's enemies stand
   still (host setting "Freeze enemies while trading", on by default). Details: "Trade screen
   (protocol 14)". The F3 window's **Offer** / **Accept** / **Decline** still works: the item is
   held while the offer is open and comes back if it is declined or taken back.
8. A partner in a cutscene, a menu or the item box stays where it was, with a label above its
   head; enemies leave it alone meanwhile (as the paused game does in single player).
9. The partner's name floats above its head while it is in your room and in view; out of view
   (another camera angle of the same room), a marker at the screen edge points at it. Option
   "Show the partner's name above its head" (F3, also F1 > Gameplay; ini `coop_nametag`).

Command line instead of the lobby: `--coop-host PORT` / `--coop-join HOST:PORT#CODE`. The settings
are kept in `recvx.ini` (`coop_port`, `coop_join`, `coop_char`, `coop_loot`, `coop_follow`,
`coop_upnp`, `coop_name`, `coop_nametag`, `coop_items`, `coop_start`, `coop_conn`, `coop_server`,
`coop_trade_freeze`, `coop_remember`, `show_ping`; `coop_player_id` and `coop_player_key` are made once
and identify this installation - keep them, and do not give the key away; the room password is never saved). Through a co-op server: `--coop-server HOST[:PORT]`, `--coop-conn
auto|direct|server`, `--coop-pw PASSWORD`, `--coop-join ROOMCODE`.
Both players control their own character; the host's game runs the enemies of the room both
are in. You hear the partner's footsteps, shots and voice (hits, grabs, death) in the voice of the
character it appears as (protocol 18).

## Playing over the internet

Only the host must be reachable: the partner's game sends first, and the host answers to
wherever that came from. On the same LAN nothing is needed (join the host's LAN address). Over
the internet, in this order:

1. **Automatic (UPnP / NAT-PMP).** With **Open port automatically (UPnP)** on (the default), the
   host's game asks the router to forward UDP `coop_port` (7140) to this PC while hosting (2 h
   lease, renewed while hosting, removed when the session ends and on exit). The F3 window then shows
   this PC's LAN address, the public address, the result and the join code; **Copy join address**
   puts `PUBLICIP:PORT#CODE` on the clipboard for the partner. Needs UPnP (or NAT-PMP) switched on in
   the router; many have it on, some (e.g. FRITZ!Box: per device, "Permit independent port
   sharing") need it allowed.
2. **Manual port forwarding**, if the router has no UPnP or refuses: in the router's settings
   forward **UDP** port 7140 (external and internal) to the host PC's LAN address (shown in
   F3; give the PC a fixed address / DHCP reservation). The partner joins
   `your-public-ip:7140` (the public IP: the router's status page, or any "what is my IP" site).
3. **Firewall** on the host PC, allow incoming UDP 7140:
   * Windows: allow the game when Windows Defender Firewall asks (tick *Private* and *Public*
     if your network is set to public), or `netsh advfirewall firewall add rule name="RE CVX
     co-op" dir=in action=allow protocol=UDP localport=7140` in an admin prompt.
   * Linux firewalld: `sudo firewall-cmd --add-port=7140/udp --permanent && sudo firewall-cmd --reload`;
     ufw: `sudo ufw allow 7140/udp`.
4. **CGNAT / double NAT.** If F3 says the router's own internet address is not public (it starts
   with 100.64-100.127, 10., 172.16-31. or 192.168.), or that address differs from what
   "what is my IP" sites show, the provider (or a second router) shares one public address
   between customers: no port forwarding on your router can make you reachable. Ask the
   provider for a public IPv4 address, or use a VPN overlay.
5. **VPN overlay: the zero-config fallback.** [Tailscale](https://tailscale.com) or
   [ZeroTier](https://www.zerotier.com): both players install the app and join the same
   network (Tailscale: invite the friend to your tailnet or share the machine; ZeroTier: both
   join the same network ID, the owner approves). The partner then joins the host's overlay
   address (Tailscale `100.x.y.z`, ZeroTier as shown in the app) with `:7140`. No router
   changes; it works through CGNAT. Allow the game in the firewall as in 3 if it still does not
   connect.

The joining player's F3 window says when no reply from the host arrives (after 5 s): then one of
the above is missing (or the host has not pressed Host, or the address/port is wrong). A wrong
join code gets "The host did not accept the join code", a host that already has a partner "The
host already has a partner". There is
no self-test of the public address: that needs a server outside your network.

Implementation: `pc/src/net/portmap.c`, a background thread per hosting session (discovery
takes up to a few seconds and never blocks the game): miniupnpc (UPnP IGD: `AddPortMapping`
UDP, `GetExternalIPAddress`, `DeletePortMapping`; a router that only allows permanent mappings is not
asked - a crash would leave the port forwarded for good - and NAT-PMP is tried, else the F3 window
says to forward by hand) and, without an IGD, libnatpmp; both BSD, fetched by CMake, linked statically (Winsock on
Windows). Nothing of this runs unless you host.

## Playing through a co-op server (room codes)

When neither player can open a port (CGNAT, no UPnP, a router you do not control), a co-op server
introduces the two games by a **room code** and, only if no direct path can be found, relays their
traffic. There is no official server: one of you (or a friend with a small Linux server or VPS)
runs `recvx-server` (docs/COOP_SERVER.md "Running a server"), and both enter its address.

1. Both: lobby **CONNECT** *Via server*, then HOST A GAME or JOIN A GAME, and **SERVER** the
   server's address (`host` or `host:port`; port 7141 is implied). **RELAY** *Only if needed* is
   Automatic (`coop_conn = auto`), *Always* is Via server (`server`); *Direct (P2P)* is `direct`.
   Opening or joining without a SERVER is refused ("Enter the co-op server's address first").
   Or F3 (Connection first, then Co-op server, Room password), or F1 / OPTION > PC SETTINGS > CO-OP
   for the connection.
2. Host: **OPEN THE LOBBY** as usual. The server gives the session a room code; the info lines and
   F3 show `ROOM CODE XXXX-XXXX`; **COPY ROOM CODE** (lobby; "ROOM CODE: WAITING FOR THE SERVER"
   until it arrives) or **Copy room code** (F3) puts it on the clipboard. Optional: a **PASSWORD**
   before opening; the partner then needs it too.
3. Partner: **CODE** the room code (`XXXX-XXXX`, case and dash do not matter), the same
   PASSWORD if the host set one, **JOIN**. The lobby's joining row reads **CODE** via a server and
   **ADDRESS** (`ip:port#CODE`) for Direct (both edit `coop_join`); F3's field reads "Room code"
   with a server set (Automatic) or Via server, else "Join address". With the row selected, a help
   line under the rows says what to type; F3's tooltip says the same. The lobby opens on the
   connection last used: Direct for `direct`, and for `auto` without a server.
4. The status line says how you are connected: *connected directly*, *connected directly (LAN)*
   (both behind the same router), or *connected via the relay* (adds the detour through the
   server to the ping), and *encrypted*: through the server, everything the games exchange is
   encrypted end to end (the server operator cannot read it).

Details:

* **Automatic** tries a direct path first (UDP hole punching, about 2 s), then the relay; a direct
  path found later is taken over without reconnecting. **Via server** always uses the relay
  (predictable, slower). **Direct (P2P)** never contacts the server. Without a server address,
  Automatic is exactly Direct.
* Hosting with a server still accepts direct joins (`ip:port#CODE`) as before; an address typed in
  the partner's CODE / ADDRESS row is always joined directly (Automatic or Direct). The first partner to
  arrive gets the session.
* Messages: "Room not found (check the code)", "This room needs a password (PASSWORD)", "Wrong
  password", "The room is full", "Too many attempts: wait a minute", "The co-op server ... does not
  answer (UDP 7141 blocked, or no server there?)" - the host keeps hosting directly then.
* Encryption is decided by each game, not by the server: through the server everything is
  encrypted, and a game refuses a partner that would not encrypt ("The host does not encrypt").
  With a PASSWORD not even the server operator can read or alter the session. A password typed
  for a room that has none is refused ("This room has no password, but you entered one (clear
  PASSWORD to join it)"), so a server cannot quietly put you into another room.
* If the server restarts (or forgets the room) while you host, the game opens a new room by
  itself: the lobby and F3 show the new code ("the co-op server lost the room (restarted?):
  opening a new one"); a direct session goes on, a relayed one ends. The server's name is looked
  up in the background (at most 5 s: "Cannot find the co-op server ..."), so the game never
  stalls on DNS. Game and server must both speak server protocol 3 (docs/COOP_SERVER.md 12).
* Running a server: docs/COOP_SERVER.md "Running a server" and section 10 (config file
  `/etc/recvx-server.conf`, log levels, metrics with `recvx-client 127.0.0.1 status`). Its relay
  allows each player 512 KB/s and 600 datagrams/s by default, ~3x what the busiest rooms send.
* Known limits: UDP only (networks that block UDP entirely, some corporate or hotel networks,
  cannot use it; there is no TCP fallback); IPv4 only for now (IPv6 is planned). A game closed
  without leaving (crash, kill) no longer blocks the room: the same player joining again takes its
  place at once, the host can remove it, and the server frees it by itself after 10 s
  (`member_timeout`); a host's own room stays 30 s (`room_timeout`).

## Verdict

| Feature | Verdict |
|---|---|
| Milestone 1: second player joins from another instance, visible and animated in the same room | feasible, low cost (~4 small hooks) |
| Shared world with enemies reacting to both players | feasible, expensive; enemy grab/bite sequences hardest |
| Player model choice (lobby) | feasible for Claire, Chris, Steve, Wesker (`ply_id` 0..3, costumes via file offset) |
| Shared loot | cheap |
| Individual loot | medium (key items/door locks must still be shared) |
| Both players must be in the same room | yes for a shared world; the host cannot simulate two rooms |
| Players in different rooms | only as "each instance owns its own room", merged on rejoin; cutscenes and story switches force regrouping |
| Lockstep netcode | not recommended (libc rand in ~500 enemy calls, menus pause the simulation) |

## How the game represents the player

* `main.c`: `BH_PWORK ply; BH_PWORK* plp = &ply;` with 5891 `plp` references (player.c 2092,
  enemies 240-280 each, events, cut.c camera, hitchk.c doors).
* Enemies have no target field; they read `plp` (e.g. `en01.c bhEne01_PlayerControl`,
  `pwksub.c bhSearchPlayer`); grabs write `plp->mtn_no = mtn[sys->ply_id][..]`.
* Per-character state in `sys`: `ply_id`, `ply_hp[4]`, `ply_wno[4]`, `ply_stflg[4]`, inventory
  `itm[ply_id*16]` (boxes `itm[64]`, `[192]`, `[224]`); pad input only via `sys->pad_*`.
* Camera cut selection (`cut.c bhCheckCut`) follows `plp->gpx/gpz/flr_no`.

**Key lever: a player context swap** in glue code instead of touching the 5891 references: save
and set `plp`, `sys->ply_id`, `sys->pad_*`, the player model/motion buffers (`plmdlp lmmdlp
wrmdlp wlmdlp plmthp plbmtp plwmtp plzmtp plexwp plhdwp pletcp`), `obwp[4]`, `hd_pos`,
`bmt_size`, `wmt_size`, `pl_htp` (and a 16-word inventory block if both play the same
character). Then the stock player functions run for player 2.

## Architecture

* `pc/src/net/net.c|h`: non-blocking UDP (BSD sockets / Winsock2), no new dependency for M1;
  reliable events from M2 (minimal seq/ack, or ENet via FetchContent).
* `pc/glue/coop_glue.c`: `BH_PWORK ply2`, ~750 KB of static player-2 buffers (must be static:
  game/DMA data has to lie in the 0x30000000 window), context swap, packet pack/apply.
* No network thread: poll/send once per game frame at the frame hand-off next to `cheat_frame()`
  (`pc/src/ps2/libgraph.c sceGsSwapDBuffDc`) - the 30 Hz tick boundary on the game thread.
* Netcode: hybrid. Each instance runs the game locally with its own player authoritative
  (camera, doors, collision unchanged); the host owns enemies, item/world flags and the room;
  the client's enemies become puppets of host snapshots (M2).
* Messages (packed structs with a frame number): `HELLO{version, build hash, ply_id, costume}`,
  `POSE{px py pz ax ay az mtn_no frm_no wpnr_no flg/stflg hp stg rom}`; from M2 `ENEMY_SNAP`,
  `HIT`, `FLAGS`, `PICKUP`, `ROOM`, `SAVE_BLOB`.
* Decomp hooks: `game.c bhMainSequence` after `bhControlPlayer()` (`coop_control_remote`),
  `game.c bhAllDrawModel` after the player's `bhPutModel` (`coop_draw_remote`), `system.c`
  room/player load (`coop_load_remote`), from M2 `eneset.c bhControlEnemy` around
  `bhJumpEnemy`.
* Lobby: first an ImGui "Co-op" window (host port / join IP:port, character, loot mode) and
  `--coop-host PORT`, `--coop-join HOST:PORT`; a title-menu lobby later.

## Milestones

1. **Ghost partner**: second player from another instance visible and animated in the same room
   (pose sync, player-2 model loaded via the context swap: part file `p2id + 10 + costume*4`,
   `bhReadPlayerData`, weapon `wpnr_no + ply_id*30 + 20`, `bhReadWeaponData`; per frame
   `bhSetMotion` + `bhCalcModel`, drawn with `bhPutModel`, hidden in other rooms/events).
   Accept: each sees the other move in the same room with the right model; co-op off is
   byte-identical (frame dumps).
2. **Shared enemies** (host-authoritative, enemy retargeting to the nearest player via context
   swap, client enemies as puppets, `HIT` events; cancel grabs on the remote player at first).
   Accept: both can kill the same zombie, damage applied once.
3. **Session**: client joins with the host's save region (`sys->version..save_end`), OR-merged
   flag sync, shared loot, room follow via `bhSetDoorDemo(..., 255)`.
   Accept: finish 3 rooms together with consistent keys/items.
4. **Loot option**: individual loot by not syncing item flags (type 7 `it_flg`).
5. **Independent rooms**: per-instance room authority, rejoin rule, forced regroup on story
   transitions (`sys->cng_pid`) and cutscenes.
6. **Cosmetics**: Steve/Wesker models, a second voice bank, title-menu lobby (done, see below;
   no second voice bank).

## Milestone 1 as implemented

Use: F3 opens the Co-op window (host port / join address, "You appear as", Host / Join /
Disconnect, status with ping and the partner's room). Command line: `--coop-host PORT`,
`--coop-join HOST:PORT`. Settings `coop_port`, `coop_char`, `coop_join` in `recvx.ini`.
Debugging: `RECVX_COOP_TRACE=1` logs every sent/received pose (stderr, also with `--quiet`);
`RECVX_COOP_ECHO=n` shows our own pose n frames late as the partner, without a network.

* `pc/src/net/net.c`: one non-blocking IPv4 UDP socket. `pc/glue/coop_glue.c`: `coop_frame()`
  at the frame hand-off (after `cheat_frame()`) takes overlay requests, receives, and sends
  the pose built this frame; three decomp hooks, all `#ifdef TARGET_PC` + `if (coop_on)`:
  `game.c bhMainSequence` after `bhControlPlayer` (build our pose, animate the partner),
  `game.c bhAllDrawModel` after the player's `bhPutModel` (draw the partner and its weapon),
  `ps2_NaTextureFunction.c njReleaseTextureAll` (the partner's textures are gone; reload later).
* The host takes the first peer that sends to it; the client sends from the start. 3 s without
  packets = partner lost. Ping = our clock echoed back minus the time the peer held it (since
  "Accurate ping, smooth name tag": network arrival times, not frames).
* The partner is drawn when both are in game, in the same `stg_no/rom_no`, and on both sides
  the player is visible (`pt_flg & 1`, not `stflg & 0x1000000`) and no door transition or event
  runs (`cb_flg & 0x5`, `gm_flg & 0x9`). During loads and menus the control hook does not run,
  so the pose goes out flagged "not in game".

Verified headless with two local instances: new game to Claire's cell on both, the client
appearing as Chris; each draws the other walking (host sees Chris, client sees Claire), hidden
during the opening event and the inventory. Co-op off vs. the previous commit: the title and
loading frames are byte-identical; attract-demo frames differ from run to run in both builds
alike (timing), so they cannot prove identity there.

Findings in the code that changed the plan:

* No `system.c` load hook. The PC `ADXF_ReadNw32` is synchronous, so the partner's part file
  (`chr + 10 + costume*4`, then weapon `wpn + chr*30 + 20`) is read into a static buffer when
  the partner is first seen in our room or changes character/weapon (one short hitch).
* `bhReadPlayerData` reads from `sys->memp`, writes `sys->plmdlp/plbmtp/plmthp/plzmtp`, sets
  `sys->hd_pos`/`bmt_size`, and allocates the O_WORK arrays with `bhGetFreeMemory` behind the
  model data. `bhGetFreeMemory` refuses addresses more than 12.25 MB above `freemem`, and the
  static player-2 buffers lie above the heap, so `freemem` is swapped too. `bhReadWeaponData`
  needs `plp`, `sys->obwp` (two own `O_WRK`), `wrmdlp/wlmdlp/plwmtp`, `wmt_size`, and a
  non-NULL `obwp[i].mlwP` (it releases the old weapon's textures through it).
* `sys->ply_id`, `sys->pad_*`, `obwp[4]`, `pl_htp` and the inventory need no swap for M1: the
  loaders and `bhSetMotion/bhCalcModel/bhPutModel` only use the `BH_PWORK` passed in, `plp`
  and the buffers above. (Needed again once player code runs for player 2.)
* The weapon file exists for "no weapon" too and carries motions 100+ (aiming, firing), so it
  is always loaded. Some actions animate from the room's motion set (`plp->mnwP =
  sys->rmthp`); the pose carries a flag for that.
* `bhSetMotion` writes the joint angles of `mtn_no` at `frm_no` (16.16) into the model and
  blends from the current joints while `hokan_count > 0`. The partner gets the received motion
  and frame with `add = 0` (one frame ahead of what the sender drew), and on a motion change
  the sender's blend count. Motion number and frame are range-checked against the sets.
* Textures are shared by global index (`njLoadTexture` refcounts): the same character twice
  shares them; Claire and Chris together display correctly.
* HELLO is folded into POSE: every packet carries the protocol version and character, so a
  late joiner or a character change needs no handshake.
* Not reproduced on the partner: aim pitch/head turn (`PlyPchMain`), the lighter's arm IK,
  footstep sounds and effects, shadow. Steve/Wesker (chr 2/3): verified in M6.

## Milestone 2 as implemented (shared enemies)

The host's game runs the enemies; the client's enemies are puppets of the host's. Hooks:
`eneset.c bhControlEnemy` around `bhJumpEnemy` (`coop_enemy_begin/end`, `#ifdef TARGET_PC`
+ `if (coop_on)`). Protocol 2: every packet still carries the pose, followed by reliable events
and, from the host, enemy snapshots.

* **Retargeting (host).** Per enemy, the nearer player (XZ, 1.5x squared-distance hysteresis)
  is the target (since "Enemy targets: one victim per attack" below: kept through an attack, and a
  busy target is left for the free player). For the partner, `plp` points at `ply2` for that enemy's step; `ply2` carries
  the partner's position, angles, hp, `stflg`, `mode0/1` and model matrices (milestone 1). What
  the enemy does to `ply2` is diffed: an hp loss or a hit reaction (`mode0 = 2`) becomes an
  `EV_PLY_HIT` (hp delta, `mode0..3`, `flg` bits 4/8, `exp1` bit 4, hit direction); everything
  else is undone (struct and exp work restored), so grabs on the partner are cancelled, as
  planned. The client applies the hp delta and, if not already reacting, the hit reaction.
* **Puppets (client).** An enemy runs its own AI once, for its initialising step (`mode0 == 0`:
  model, collision tables, shadows); after that, when the host is in the same room, it gets
  the host's snapshot at each frame hand-off and only `bhSetMotion` (add 0) + `bhCalcModel`.
  Enemies that use another enemy's model (a zombie's helper parts, ids 31..38, point `mlwP` at
  the parent's `mdl[]`) are not animated, or they would overwrite the parent's joints.
  Snapshots only go to slots active in our room (`flg & 1`; `bhSetEnemy` clears a slot when it
  takes it, free slots keep stale work) whose own init ran (`mode0 != 0`; room loads run enemy
  steps themselves via `bhInitEvent`); a slot still at `mode0 == 0` only gets the host's wake
  bits, so our loop runs its init. Neither side uses snapshots during a door transition: the
  room number already names the next room while the slots still hold the old one's.
* **Snapshots** carry explicit fields per enemy slot (`flg flg2 stflg mdflg`, position, angles,
  `mode0..3`, motion, frame, blend, hp, and the hit capsule `watr`: gun hits test it and only
  the AI updates it). Pointers travel as indices: `mlwP` into the enemy's own `mdl[]`, `mnwP`
  as an offset from `mnwPb` or "room set". The host's hit marker (`flg & 4`) is not applied
  (it would come back as a new hit).
* **Hits on enemies (client → host).** The weapon code's output on a puppet (`dam[joint]`,
  `flg & 4`, `flg2 & 4`, `comb_flg & 0x70`, hit point/direction/angles, weapon, joint) is
  taken at the hand-off, cleared locally, and sent as `EV_ENE_HIT`; the host adds it to its
  enemy before the enemy's next step, so the enemy's own damage code applies it once.
* **Events** are ordered and reliable: ids, resent (up to 8 per packet, 64 queued) until the
  peer's `ev_ack` covers them; the receiver applies only the next id. Events for another room
  are dropped.

Portability check: in both instances the enemy work of the same room had identical
addresses (`mlwP`, `objP`, `mnwP`, `exp0/1`, `memp`), because allocation per room load is
deterministic from `mempb`. It still depends on what was loaded before (e.g. a different
weapon), so only explicit fields are synced, as above.

Not covered yet: enemy sounds and effects on the client (they come from the AI, which does
not run there); enemies in a room only the client is in run locally (milestone 5); boss/event
enemies are untested; grabs and other player-driving attacks on the partner are cancelled
(only their damage arrives; zombie grabs: M7); the host's own `sys->pad_on` is masked for one frame when an
enemy grabs the proxy (en01 clears the d-pad bits).

Verified headless with two local instances (new game, teleport to room 0/6 with 5 zombies,
handgun on both, client as Chris walks off and fires): the client shows all 30 enemy slots as
puppets on the floor where the host has them; the client's 23 hits arrived as `EV_ENE_HIT`
and each lowered the host zombie's hp once by the weapon's 10 (zombie 0: 90 -> 0, killed by
the client's shots on the host); zombies retargeted to the partner (up to 19 slots) and their
hits on the proxy reached the client as `EV_PLY_HIT` (client hp 160 -> 150 -> 132 -> ... -> 82,
each delta once). Co-op off vs. the main build: same frame dumps as main vs. main.
A crash drawing a zombie (`njCnkDrawModelLocal_`, bad chunk type in the model data) also
happens with co-op off in this scenario (teleport + equip test aid), so it is not co-op.

Test aid (both instances): `RECVX_TELEPORT="1:0:6:0"` (game frame : stage : room : position,
further entries relative to the previous teleport) jumps from a new game straight to a room
with zombies; `RECVX_EQUIP="120:8:60"` equips item 8 with 60 rounds. (Correction found in M6: item 8 is the
combat knife, `wpnr_no` 2 - `WpnTab[2].flg & 0x40000000`, the knife path; the handgun is item 5,
`wpnr_no` 3. The M2 test above therefore hit the zombies with knives, 10 damage per hit.)

## Milestone 3 as implemented (session)

No new decomp hooks: everything runs at the frame hand-off in `pc/glue/coop_glue.c`
(`session()`, after the snapshots), using the game's own entry points. Protocol 3: the pose
also carries `pos_no` (room follow), a session id and a "want state" flag; three new reliable
events go through the M2 stream (now 16 per packet, 256 queued).

* **Join (`EV_BLOB`).** The host sends its save region (`sys->version..save_end`, 2100 bytes,
  the bytes a save file holds) as 33 chunk events, with the player fields filled in the way
  `bhPushGameData` does (position, angle, hp, weapon; without touching the retry copy at
  `freemem`). The client copies it to `freemem` and calls `bhPopGameData()` - the "continue"
  path of the game over screen: `bhExitGame`, the region restored, `tk_flg = 0x300020`, so
  `bhFirstGameStart` loads the host's room with the host's flags, inventory, character,
  difficulty and position (`retry_ct` is put back). The client loads only at the title or while
  in control (no menu, event or door); events after the blob wait until it is loaded.
  When: the host sends one when a client appears (or a new client session), when its own game
  (re)starts (`tk_flg & 0x20` at the hand-off: new game, load, continue), and when the client
  asks (its title screen `tk_flg & 0x8` or its own game restart, e.g. after a game over), and
  only while the host is in control in a game (not the attract demo). So a client started at
  the title is pulled into the host's game, and a host restart pulls the client along.
* **Flags (`EV_FLAGS`).** The block `ev_flg it_flg ic_flg ed_flg ky_flg mp_flg` (120 words,
  story, item, item-seen, enemy/door, key/lock and map flags) is diffed against the state
  the partner knows at each hand-off of an in-game frame; changed words go out as set and
  cleared bits (so flags the game clears again stay consistent - stricter than a pure OR).
  Both sides apply what they receive (and update the known state, so nothing echoes back).
  Host authority: each client change carries the last host event id the client had applied;
  the host drops the bits of words it changed itself after that, and the client always
  applies the host's changes, so concurrent changes of the same bit end as the host has them.
  Changes taken before the client's last blob are dropped. Not synced: `rm_flg st_flg sp_flg`
  (per room / per frame) and the item box.
* **Shared loot (`EV_PICKUP`).** A pickup (`bhItmCk`) sets an item flag and hides the item
  object of the examined spot (`sys->itwp[etcp[etc_idx].prm0].stflg |= 0x1000000`), seen a
  few frames apart (a file sets the flag before its "filed" message). When both happen within
  30 in-game frames for the spot `sys->etc_idx` names, the event carries the item (`sb_id`, 0
  when it is not in our inventory: a file), the spot and its trigger bit. The partner, at its
  next in-game frame, adds the item with the game's own `GetItem` (testmode 5: only the add,
  with stacking; a capacity check like GetItem's first step, a full inventory gets nothing)
  and, in the same room, hides the object and copies the trigger bit. An item flag set any
  other way (events) is synced as a flag only.
* **Room follow.** When the host's room changes (seen while it is in game), the client runs
  `bhSetDoorDemo(0, stg, rom, pos_no, 255)` (the cheat teleport: door transition without the
  animation) as soon as it is in control. A client that walks into another room on its own
  stays there until the host changes room (M5 covers independent rooms).
* Sessions: each instance picks a session id at Host/Join; a new peer id resets both event
  streams and the pose sequence check (before, a reconnecting client's events and poses were
  ignored), and the host accepts a new partner once the old one left or timed out.

Verified headless with two local instances (client at the title, joining as Chris, `RECVX_
COOP_TRACE=1`; the trace now prints a flag hash and the inventory per room):

* Host new game, teleport to 0/4; the client at the title received the blob when the host
  had control, loaded into 0/4 at the host's position (same flags hash), each saw the other.
  The host read and filed the fax there (`took item 0 (etc 6)`): the client's flags followed
  and its fax object was hidden. The host then went to 0/9 and 0/8 (teleport test aid); the
  client followed both times (`following the host to stage 0 room 9/8`), and in 0/8 both had
  the same flags hash. That is 3 rooms with consistent flags.
* Host in 0/11 walked to an ammunition box on the floor (item 12, 15 rounds) and took it
  (`took item 12 (etc 13)`): the client got item 12 in the same slot with the same count
  (`000c000f`), the box on its floor disappeared, and the flags hash matched.
* Co-op off vs. the main build: 88 frame dumps (title, new game, teleport to 0/4, walking,
  reading the fax) identical.

Not verified: a pickup by the client (the code path is symmetric; two scripted attempts
missed the spot, the pad script drifts under load), concurrent conflicting
flag changes, keys and locked doors (key use and door unlocks are flags and inventory items,
covered by the same paths, but no key was taken in the tests), a client joining mid-game after
the host's game over/continue, a reconnect, large (two-slot) items, the client loading the
blob from inside a menu (deferred by design). At this test speed (2-4 fps for two instances)
room loads take over 3 s, so "partner timed out" is logged often; it recovers by itself.

Known limits: the partner's copy of a used-up key stays in its inventory; an item picked up
while the partner's inventory is full is lost for the partner; item box contents are per
instance.

## Milestone 4 as implemented (loot option)

Use: F3 window "Loot (host)": Shared loot / Individual loot (ini key `coop_loot` 0/1, set while
not connected). The host's choice travels in every pose (`opts`, protocol 4); the client uses
it and shows it as "Session: ...". Still no new decomp hooks.

Which flag words are which (the 120-word block `ev_flg it_flg ic_flg ed_flg ky_flg mp_flg`):

* **Item**: `it_flg` (an item spot was taken; `bhItmCk` sets it, `bhItmSetCk` hides the item at
  room load) and `ic_flg` (examined but not taken). Not synced with individual loot.
* **Progress**: everything else - `ev_flg` (story), `ed_flg` (enemies, doors), `ky_flg`
  (locks), `mp_flg` (maps). Always synced as in M3.
* **Key items** are items with `itemdata[].type & 0x40` that land in the inventory (files have
  the same type bit, but a file pickup puts nothing in the inventory, so it travels as item 0
  and counts as ordinary loot). Their pickup is shared in both modes: `EV_PICKUP` now carries
  the spot's `it_flg` bit (the bit our own pickup set in the last 30 in-game frames), the partner
  sets it (and clears the same `ic_flg` bit, as `bhItmCk`), gets the item and, in the same room,
  hides it.

Individual loot: the item words are left out of `EV_FLAGS`, and a pickup of a non-key item (or
a file) sends no `EV_PICKUP`, so each player finds and takes their own copy.

M3 limits fixed:

* **Used-up keys.** The event that opens a lock takes the key with `bhPlItemLost` and sets
  progress flags. When a key item leaves our inventory within 30 in-game frames of a local
  progress flag change, `EV_LOST{item}` goes out; the partner removes one copy the way
  `bhPlItemLost` does. A key that leaves through a menu (item box, combining) changes no flag
  and is not sent. Our own removal does not echo back (the partner's copy is taken off the
  "last seen" key counts).
* **Full inventory.** The partner's pickup is not lost any more: when `give_item` finds no room,
  the item stays in our room (object not hidden) and its `it_flg` bit is cleared on our side only
  (flag and "last seen" state, so nothing is echoed). When we later take it ourselves, that
  pickup is not passed on (the partner has its copy); the bit is remembered per session.

Test aids (cheat_glue.c, only with the variables set): `RECVX_WARP="stage:room:frame:x:z:angle,..."`
puts the player at x/z once it has been `frame` game frames in that room (independent of how the
room was reached, e.g. a co-op join); it moves the collision points along and leaves the
leg-placement mode (`flg & 0xC0000`, `bhFixPosition`) that otherwise puts an idling player back.
`RECVX_EQUIP="frame:item:count,item:count,..."` fills further slots after the equipped one.

Verified headless with two local instances (host: new game, teleport, idle; client: at the
title, joins as Chris, warps in front of an item and presses cross; the test scripts are in the
scratchpad `m45/`, `t4.sh`):

* **A pickup by the client** (M3 never had one): 0/11, handgun bullets (item 12, 15 rounds),
  individual loot: `took item 12 (etc 13), individual loot`, client inventory `000c000f`, host
  inventory and `it_flg` unchanged, nothing sent. M3's client attempts had failed because the
  client's pad script started with a separator (`,frame:...`), which the pad script parser
  rejects - the client then gets no scripted input at all.
* **Key item, individual loot**: host and client in 0/8, the client takes the Extinguisher (82):
  `took item 82 (etc 2, flag 4)`, the host `got the partner's item 82`, host `it_flg` bit 4 set
  and item 82 in its inventory.
* **Full inventory, shared loot**: host inventory filled (handgun + 7 herbs), client takes the
  bullets in 0/11: client `took item 12 (etc 13, flag 146)`; host `no room for the partner's item
  12, it stays in the room`, host `it_flg` stays clear, and the host, warped to the spot, gets the
  game's own "You can't take any more items." for the bullets still lying there.
* Co-op off vs. the main build: 45 frame dumps (title, new game, teleport to 0/4, walking, the
  fax) identical.

Not verified: `EV_LOST` (no key was used in a test: the early keys' locks are not near the
teleport targets), taking an item one had left behind earlier (needs freeing a slot in between),
shared loot with a client pickup and room on both sides (the same `give_item` path as the key
test and M3's host pickup), switching the mode mid-session (the window does not allow it), a
client loading the host's game from the attract demo (the blob load now leaves the demo state -
`ss_flg & 0x400000` kept the pad on the demo recording - but this case was not hit in a test).

Limits: the join copies the host's save region, so a client joining with individual loot
starts with the host's item flags (M8: no longer its inventory, and shared loot shares the box);
a key that goes into the item box is not "used up" for the partner; if both players take the
same spot within one round trip with shared loot, both get a copy.

## Milestone 5 as implemented (independent rooms)

Use: F3 window "Follow host (client)" (ini `coop_follow`, default on; the client's own choice,
it can change at any time). Protocol 5: the pose carries a regroup counter from the host. Still
no new decomp hooks.

Rules:

* **Room authority is per instance.** The host's enemies are authoritative only while both
  players are in the same room: the host builds snapshots only then, and the client's enemies
  are puppets only then (`puppets()`: same stage/room, snapshots present, no door transition).
  In a room of its own the client's game runs that room's enemies itself (their AI, damage,
  attacks on the client) - as the M2 code already did when the rooms differed; the host does
  the same for its room, with enemies targeting only the host (the partner proxy is only
  active when the partner is drawn in the room).
* **Follow host** on: as M3, the client goes through every door the host takes
  (`bhSetDoorDemo`). Off: the client stays where it is when the host leaves and may walk or
  teleport anywhere.
* **Rejoin.** Entering the room the partner is in (either way round) makes the host
  authoritative again from the next snapshot: the client's enemies switch back to puppets and
  take the host's state (position, motion, hp, active flag), whatever they did locally before.
  Leaving again hands them back to the client's AI.
* **Regroup.** The host counts regroups: a cutscene starting or ending (`cb_flg & 0x4`, set by
  `bhCineSet`) and a story character switch announced by `cng_pid` (`bhPlayerChangeSet`). On a
  new count the client is pulled into the host's room (as a follow, also with "Follow host"
  off) as soon as it is in control. When the host's `ply_id` actually changes (the switch
  happened), the host also sends its game state again (`EV_BLOB`), so the client continues as
  the new character from the host's state, like after a host restart. Joining takes the count
  over without a pull.

Verified headless with two local instances (`t5.sh`; client at the title joins in 0/11):

* Follow off: the host teleported 0/11 -> 0/9 -> 0/4 -> 0/9 -> 0/4; the client stayed in
  0/11 (no follow). The client's enemies there: puppets while the host was in 0/11, local
  after it left. The client teleported itself to 0/9 while the host was in 0/4: the 36 enemy
  slots ran locally (zombie 0 wandering: x/z changing between trace lines, hp 120); the host
  came back to 0/9: the client's enemies became puppets (zombie 0 took the host's state, hp
  60); the host left for 0/4: local again, zombies moving on the client.
* Follow on (default): the client followed the host 0/11 -> 0/9 -> 0/4 as in M3.
* Regroup trigger: the host logs `regroup (cutscene starts/ends)` for the new game's opening
  cutscene. A teleport scan of 30 rooms (stages 0, 1, 3) found no other cutscene reachable by
  the test teleport, so the pull of a client in another room by a cutscene was not run end to
  end (the pull is the follow code path verified above, keyed by the counter).
* Co-op off vs. the main build: 43 frame dumps identical.

Not verified: a client pulled by a cutscene or a character switch (`cng_pid`, `ply_id`), the
client triggering a cutscene of its own in another room, boss rooms.

Limits:

* A cutscene or event the client triggers alone plays only on the client (its flags reach the
  host as usual); only the host's cutscenes regroup. (M8: world events the client triggers in the
  host's room now play on both.)
* On rejoin, the client's enemies jump to the host's state. Enemies the client killed alone
  stay dead through their kill flag (`ed_flg`, synced) and, since protocol 15, also without one:
  the client names its dead enemies when the host's take over and the host hides its copies
  (`EV_GONE`, "Protocol 15" below).
* While the host has not yet seen the client in its room (a round trip after the client's
  door), the client's enemies there run a few frames of their own AI.
* The pull uses the host's room entry point (`pos_no`) like the room follow, so the client
  lands at the door the host came through, not next to the host.

Bug found outside co-op (also in main, co-op off): teleporting out of 0/6 with the test aid /
F2 cheat teleport (`bhSetDoorDemo(0, stg, rom, 0, 255)`; e.g. 0/11 -> 0/6 -> 0/8 or -> 0/4)
crashes in `bhSetMotion` called from `bhInitRoomChangePlayer` (`bhFinishRoom`). Not
investigated further; the M5 tests use 0/9 instead.

## Milestone 6 as implemented (cosmetics, sounds, lobby)

Still no new decomp hooks. Protocol 6.

**Partner character.** "You appear as" (ini `coop_char` 0..5): the character played, Claire,
Chris, Steve, Wesker, or Claire's second costume (part file 14; files 15..19 are 11-byte
placeholders and are refused). The pose now carries `mfile` = the sender's own part file - 10
(`ply_id + costume*4`) besides the chosen mesh (`chr`, `costume`). The receiver loads
part file `mfile` with the stock loader (motions, z-translation and model), and if the chosen
mesh is another file, loads that file's model over it with its motions going to a sink buffer
(ACTORS.md mesh replacement option A); the weapon file is the sender's character's
(`wpn + (mfile & 3)*30 + 20`). So the sender's motion numbers always index the set they came
from: any mesh plays any character's motions, including the weapon motions 100+.
Claire's hair (object 1211, `mdl[4]`, simulated by `bhObjClpn`) and Wesker's piece (1212, rigid)
are set up for the partner as `bhSetPlayer` does for `sys->obwp[2]` (`bhSetObject` with a
private `O_WRK` array, linked to head node 5 of `ply2`), with the hair's simulation work in
static buffers instead of the room heap; animated after the partner's model and drawn with it.
Chris/Steve meshes have no `mdl[4]` and get no such object. The loader's stale `skp[]` entries
are cleared before the second file (otherwise a skin block of the first file would be applied
to a model of the second).

**Partner sounds.** Investigated the voice bank: `RequestPlayerVoiceSoundBank(ply_id)` loads
`CORE_<ply_id>.SPQ` into SE bank 4 (player voices are SE numbers 0x400 + n, e.g. the hit
reactions 1024..1027 in `bhControlPlayerPad`); the sound driver has 5 SE bank headers (0..4)
and the bank's body goes to fixed sound RAM addresses given by the SPQ file, so a second CORE
bank would overwrite ours - loading one needs a new bank slot and sound RAM area in the PC
sound emulation (`pc/src/audio/snd_iop.c`, `se_hd[5]`) and a bank remap in the SPQ loader. Not
cheap; not done. What is played instead, once per received pose, at the partner's position:

* Footsteps: the player code marks a footstep frame with `stflg |= 0x200` and a shot with
  `0x100` (`bhControlPlayer` clears both each frame; guns set theirs in the weapon object step
  after our control hook, so the pose takes them from `plp->stflg` again at the frame
  hand-off). Footsteps play through `CallPlayerFootStepSeEx(floor, walk/run, 1, Id 1, pos)`
  (the second actor's slots 13/14, common bank 2, floor type from `bhCheckFloorSound`).
* Shots: `CallPlayerWeaponSeEx(partner hand, WpnTab[wpn].seno0, 0)`. Only our own weapon's arms
  bank (bank 1, `WpnTab[].snd_wpno`) is loaded: when the partner's weapon uses another bank, our
  weapon's shot is played instead; with no gun of our own (`seno0` 0) the shot is silent.
  (Fixed in protocol 12: the partner's own bank is loaded beside ours, see "Protocol 12".)
* Voice: a new hit reaction of the partner (`mode0` becoming 2) plays voice 1026 when the
  partner is the same voice character as ours (`mfile & 3 == ply_id`); otherwise none.
  (Replaced in protocol 18: every voice the partner's game makes, in its own voice.)

**Lobby.** At the title screen the overlay shows a hint ("F3: Online co-op"; after Host/Join,
what the session waits for), and F3 opens the co-op window as "Co-op lobby": host port / join
address, character, loot mode, follow host, status. Host and Join at the title close the window,
so the pad drives the game's own title menu again: the host chooses New Game or Load Game, and
the client at the title is pulled into that game by the M3 join (`EV_BLOB` when the host has
control). The game's menu was not hooked then (it is now: "Lobby screen and name tags"); the title is detected as
`tk_flg & 0x8`, published by the game thread in `g_coop.at_title`.

Verified headless:

* Echo mode (`RECVX_COOP_ECHO`, single instance, Claire walking in 0/11): partner as Wesker,
  Steve, and Claire's second costume (with her ponytail) - each loaded (`loaded player-2
  model 3/2/0 costume 0/0/1 ... with the motions of part file 10`), walks with Claire's walk and
  draws in the frame dumps.
* Two instances (`m6/t6.sh`: host Claire appearing as Wesker, new game, 0/11, handgun; client
  at the title appearing as Steve, joins): the client draws Wesker walking, aiming and firing the
  handgun with Claire's motions; the host draws Steve walking. Sounds (trace): host 9 partner
  footsteps, client 4 partner footsteps and 11 partner shots (silent: the client had no gun).
* Co-op off vs. the main build: 47 frame dumps (title, new game, teleport, walking) identical.

Not verified: anything audible (headless audio is the dummy driver; only the calls were traced),
the partner's hurt voice (no hit reaction on the partner in a test), Wesker's piece on screen
(the dumps are too small to tell), the lobby window and the title hint (ImGui is not in the frame
dumps; the flow behind them - host/join at the title, the host starting a game, the client
joining - is the M3 path verified again above with `--coop-host/--coop-join`).

Limits: the partner's voice is not heard when it is another character (fixed in protocol 18); shot sounds of another
weapon class are a stand-in; the partner has no facial animation (blink) and no aim pitch.

## Milestone 7 as implemented (own model, real grabs)

Protocol 7. Still no new decomp hooks.

**Seeing yourself as the chosen character.** While co-op is on (hosting, joining, connected,
or the echo test mode), "You appear as" also swaps our own player's mesh; off co-op, and when
the choice is the character played, the character's own model is shown. (Chosen this way so
co-op off stays byte-identical without an extra setting; a cosmetic-only switch outside co-op
would only need `self_frame()` to run without `coop_on`.) Same recipe as the partner (ACTORS.md
option A), with two differences:

* The chosen file's model goes into a buffer of its own (`self_mdl`, O_WORKs behind it via
  `freemem`), its motions into the sink: our motion set, weapon files, `PlyInfo`, voice and
  everything keyed by `ply_id` stay the character's, and the original model data in
  `sys->plmdlp` stays intact. Putting the original back = copying the saved `mdl[]`, `skp[]`,
  `mbp[]`, `txp[]`, `mdl_n` back; its textures stay loaded meanwhile.
* Sub-models the chosen file lacks keep the original's (Claire's `mdl[5..7]`: the moth egg,
  en27, takes `plp->mdl[6]`, `skp[7]` and motions 93+ from our own set), so the moth-egg caveat
  does not apply. Hair / Wesker's piece follow the mesh (`sys->obwp[2]` as `bhSetPlayer` sets
  it up; none for Chris/Steve meshes, also on a Claire).

When the game reloads the player (game start, load, continue, story switch: `bhReadPlayerData`
in `bhSysCallMonitor`), `plp->mdl[0].objP` no longer points into our buffer: the textures of the
old original we kept are released (unless `njReleaseTextureAll` already took everything) and the
mesh is applied again at the next frame in control. Room changes keep the model (the player is
not reloaded; `bhInitRoomChangePlayer` only resets the eye texture to `txp[0]`).

**Real grabs on the client.** Before, the host ran every enemy and a grab of the proxy (the
partner's stand-in) was undone each step: the client only got the damage. Now a regular zombie
(en01) hands the grab to the client's game:

* Host: when an enemy step with `plp = &ply2` ends with the proxy newly in `mode0` 4 (en01
  `bhEne01_NG00/01` start a grab: `pl->mode0 = 4`, `EXP0 0x40 |= 0x20000`, and the same step's
  `bhEne01_PlayerControl` runs `PlyDG0x` case 0), the enemy's work, its `exp0` (0xB0 bytes) and
  its linked child's (`exp1`), plus the proxy after the step, go to the client as 72 `EV_ESTATE`
  chunks and an `EV_GRAB`. The slot (and the child) is then "client-owned": the host does not run
  its AI but animates it from the client's snapshots (`puppet_step`), forwards its own shots on
  it as `EV_ENE_HIT`, and keeps the proxy undone as before. During the proxy step `sys->ply_id`
  is the partner's (`mfile & 3`): en01 picks its own and the player's grab motions by it.
* Client: `EV_GRAB` copies the state over its enemy (keeping its own pointers: models, motion
  sets, exp areas, and exp0's first 0x24 bytes, which point to the zombie's part slots), and puts
  its player into the grab as the proxy was left: modes, counters, motion and frame (on the
  enemy's set when the proxy was), position, and the `flg`/`stflg`/`flg2`/`exp1` bits the enemy
  changed. Then the client's game runs that zombie with the real player: grab and bite motions,
  `bhEne_LeverCheck` (the client's pad shakes it off), bites on the client's hp, death, release.
  The client sends snapshots of its owned slots; it rejects a grab while it cannot take one
  (another grab, dying, menus/doors/events, another room) and the host lets the zombie go.
* End: when the zombie lets go (`EXP0 0x40 & 0x20000` cleared and the player off its motion set)
  the client sends the zombie's state back (`EV_UNGRAB`); the host copies it and runs it again.
  If the client leaves the room or its game restarts, it hands back without state; the host then
  releases the zombie the way a grab ends (`EXP0 0x40 &= ~0xA4080`, back to `mode0` 1). The host
  also drops client-owned slots when its own room changes, when the partner is seen in another
  room, and when the session stops (a plain time-out keeps them: room loads at test frame rates
  exceed 3 s). On the client, a disconnect leaves the zombie running locally with the grab - the
  game simply goes on alone.
* Motion sets in the pose: `mset` = own set, room set, or the enemy slot whose set the player
  plays. Before, a grabbed host was animated on the client from its own set (wrong motions).
* Room follow waits while our player is in `mode0` 4/6 and sets `plp->mnwP = plp->mnwPb` before
  `bhSetDoorDemo` (the new room frees the enemy's set; same fix as the cheat teleport).

Other grabbers (not handed over; on the proxy their grab is still cancelled, damage forwarded; since "Enemy targets: one victim per attack" it reaches the guest as a hit):
Gulp Worm (en02), Black Widow (en03), Cerberus (en04), Bat (en07), Bandersnatch (en09),
Spotter (en11), Alexia 1/2/3 (en12-14), Nosferatu (en15), Monster Steve (en17), Albinoid (en22),
Giant Black Widow (en23), Anatomist (en26), and others that set `plp->mnwP` to their set. Each
needs the same handover with its own `exp0` size and pointer words (en01's 0xB0 / first 0x24 are
hard-coded), its "holding" condition for the end, and a check of its `ply_id`-indexed tables
(several have only 3 rows: Wesker as partner would overrun them).

Verified headless with two local instances (`m6/t7.sh`, room 0/6, both Claire):

* Zombie grabs the client: host `enemy 1 grabs the partner, handed to the partner's game`,
  client `enemy 1 grabs us`; the client's frames show its Claire grabbed and bitten, the host's
  frames show the partner in the same grab with the zombie, then the push-off; the client's hp
  in the pose went 150 -> 140 -> 130 during the bites (`mode0` 4, `mset` the zombie set); client
  `the grab of enemy 1 ends, handed back`, host `enemy 1 is back from the partner's grab`.
* Zombie grabs the host (client stepped away, `t7.sh` with `CIN`): two host grabs, the client
  sees the host in `mode0` 4 on the zombie set (hp 160 -> 150 -> 140, then 140 -> 120), drawn grabbed.
* Own model: echo mode, Claire shown as Steve (no ponytail) and as Wesker; Wesker kept through a
  room change (0/11 -> 0/9).
* Co-op off vs. the main build: 47 frame dumps identical.

Not verified: struggling free with the pad (no input in the tests; the grabs ended by the
zombie's own count), the client dying in a grab, the host shooting the zombie that holds the
client (`EV_ENE_HIT` to the client), a disconnect or room change during a grab, the fallback
release, the zombie's linked child state, the own model after a load/continue/story switch,
putting the original back on disconnect, and the moth egg with another mesh.

Limits: a handover takes about 5 round trips (72 events at 16 per packet) during which that
zombie stands still on both screens; only en01 grabs are handed over.

## After milestone 7: cutscenes, enemy and NPC models

No new protocol fields. `Pose.chr` (signed char) now also carries 4 + a catalogue index
(`pc/src/host/actors.h`, 62 models); a build before this change refuses such a partner (it
accepts 0..3) and draws nothing for it, so both players need this build. The ini key
`coop_char` goes up to 67 (6 + index).

**Wesker's sunglasses behind the head after a cutscene.** Root cause: at the end of a cutscene
(and in its skip event) the room script re-attaches the player's objects with op 52
`ObjLinkSetPly`, the head part `sys->obwp[2]` with Claire's ponytail offset `0, 1.58, 0.77`
whoever is played. With our mesh swapped to Wesker, that object is his sunglasses, which then
sat behind the head (ACTORS.md section 4, "Cutscenes"). The partner's model was not affected
(its objects are private). Fix (`self_keep`): the link values of our mesh's head part are
kept from the load and put back every tick after `bhControlPlayer`; likewise the mesh's own
node offsets (bone lengths), which 8 room motions overwrite with the played character's (also
for the partner's model when it is another character's mesh).
Verified headless: Claire appearing as Wesker in 0/2, the script of RM_0020's cutscene skip
(event 12) started from a test probe: before the fix the glasses' offset in head space went
from `0, 0.85, -1.03` to `0, 1.58, 0.77`; with it, it stays `0, 0.85, -1.03` through 1200 frames.

**Enemy and NPC models.** ACTORS.md section 8. Our player: `self_actor` loads the catalogue
model into display-actor slot 0, which `game.c` draws in place of `plp` and `self_keep` poses
every tick; the player-file mesh swap is put back meanwhile. The partner: `load_player` loads the
partner's own part file as usual (motions, skeleton) and the catalogue model into slot 1, posed
after the partner's motion and drawn instead of `ply2`; the weapons are drawn as before. If a
model cannot be loaded (texture memory, a modded room), the character's own model is shown.

**Later: single player, cutscene doubles, humanoids on the player's skeleton.** "You appear as"
now also works without co-op: `coop_frame` sets `appear_on` when an appearance is chosen and
runs `self_frame`; `game.c` then calls `coop_control_remote` (which without a partner only does
`self_keep` and the poses) and `njReleaseTextureAll` calls `coop_textures_released`. With "Same
as played" nothing changes. `self_frame` also hands the appearance to `actor_doubles`, which
loads it for the cutscene doubles of the character played (ACTORS.md section 8). Zombies and
the humanoid monsters now move with the player's motions (node map + rest correction).

Verified headless (echo mode, so self and partner): every catalogue model loaded and drawn in
one session (a cycle through the list, about 130 loads and releases, no texture exhaustion);
Chris's cutscene model walking, aiming and firing with Claire's motions, then 0/11 -> 0/6
among zombies (grabbed, no crash); a zombie model in the zombie room 0/6 shares the room's
textures (0 KB loaded) and keeps them after leaving for 0/11; the Tyrant playing its idle.
Not verified: two networked instances (the pose field is the same one echo mode loops back),
a Windows build, a story character switch or game over with a catalogue model shown.

## Milestone 8 as implemented (shared world events, own inventory, trade, item box, busy partner)

Protocol 8 (both players need this build). One new decomp hook: `event.c bhControlEvent` calls
`coop_scd1()` instead of `bhScenarioCheck(scd1)` while co-op is on (`#ifdef TARGET_PC` +
`if (coop_on)`); everything else runs in `pc/glue/coop_glue.c`.

### World events: either player triggers, both see it, once

How the game changes the world (MAPS.md section 5): the room's per-frame script `scd1` has
blocks like `IfelCk; Ck ev_flg 104 clear; player on floor 0; EvtOn 0..7; Set ev_flg 104; Endif`
(zombies rise), `Ck ev_flg 110 clear; player at trigger 4; "Yes" chosen; EvtOn 5, 11; Set ev_flg
110` (a lever), `Ck ev_flg 53 set; at trigger 7; Yes; ...; Set ev_flg 53 clear` (the metal
detector's release switch). A synced world flag (`ev_flg`, `ky_flg`, `ed_flg`, `mp_flg`) is the
block's gate, and the block flips it, so it fires once per change. Its other conditions (on a
floor or trigger, "Yes" chosen, an item used, an item flag set) only hold where the player did
it. Before M8 the partner received only the flag; its own script then never ran the block: no
cutscene, no spawns, no environment change until it re-entered the room (scd0 rebuilds rooms
from flags). That is what made the joining player's pickup cutscene invisible to the host, with
its zombies appearing only after the host re-entered.

* **Which blocks.** At room entry the script is swept (instruction lengths as `pc/tools/rdx.py`
  derives them; 394 such blocks in the 205 rooms): a block whose own `Ck` tests a world flag and
  whose then-part flips it - by a `Set`, or by an `EvtOn` of an event whose code sets it. Item
  flags (`it_flg`, `ic_flg`) are no gates: pickups follow the loot mode (M3/M4).
* **Detecting.** Each instance runs `scd1` itself, exactly as `bhScenarioCheck` does, recording
  every instruction executed. A block fired when its gate had the "before" value at the start
  of the frame and a flip ran (an event flip: at most once per 10 s, the flag changes later).
  Of nested blocks that fired together only the outermost is sent.
* **Replicating.** What the block executed - its actions in order, not its tests or flow ops - goes
  to the partner (`EV_TRIG`, offsets into the room script plus the gate and a script signature).
  The partner, in the same room and script, replays them right after its own `scd1` of the next
  frame: the same `EvtOn`s, `Set`s, `CineSet`, model and trigger changes, as if its player had
  done it (nested choices such as the weapon variant follow the trigger's). The receiving stream
  waits for that replay, so the flags sent behind it cannot arrive first.
* **Exactly once, no arbiter.** The partner replays only while its gate still has the "before"
  value; otherwise it fired the block itself (both did it at the same time) or got it earlier.
  Each instance thus runs each block once; the host's enemies (spawned by its run) stay
  authoritative, the client's are puppets of them. No round trip before the effect: the player
  who triggers it sees it at once, the partner one way trip later; no lockstep.
* **Other room.** A partner elsewhere gets the flags only (as before); the room is built from them
  when it enters. Cutscenes the client triggers in the host's room now regroup like the host's.
* **Enemies driven by an event** (an event task's `LoadWork` target, or all enemies during the
  client's own cutscene) run the client's own AI meanwhile instead of being puppets: the event
  waits for their motions (`FrameCheck`), which a puppet shows only as snapshots allow - before
  this, the client could hang in its own cutscene. They take the host's state again afterwards.

### Joining keeps your inventory

The client takes a copy of its items, item box, health, weapon and status at every frame in a game
(its own before joining, the host's after). When the host's game state is loaded (join, host
restart, story switch, continue) these are put back over the host's (`own_restore`, after
`bhPopGameData`). A client that joins from the title without a game of its own gets the new-game
inventory of the session's game mode (the game's `AllItemInit`, 160 hp, no weapon). With shared
loot the item box is the host's (one box for both, below); with individual loot it is the
client's own. Loot semantics are otherwise unchanged: shared - a pickup of either goes to both
(M3); individual - each takes their own, key items shared (M4). Items taken before the join are
not handed out (the joiner does not get the host's earlier keys; the host uses them).

### Trade

F3 window, section "Trade" (both in game, same room, within 3 m): choose an item of your
inventory (not the equipped weapon or a two-slot item) and a count (stackable items can be
split), **Offer**; the partner sees "Partner offers ..." with **Accept** / **Decline**.
Protocol (`EV_TRADE`): the offerer takes the item into escrow when it offers. The receiver
decides: it adds the item and answers "taken", or answers "refused" (declined, cancelled by the
offerer - **Take back** - or asked about an offer it never took). The offerer gives the escrow
back only on "refused" and drops it on "taken", so an item is never in both inventories and never
lost: without an answer it stays held (F3 shows it). The receiver keeps its answers in a ledger
that outlives sessions; after a reconnect (a new session) the offerer asks again (`QUERY`) and
gets the same answer, and an offer the receiver had not answered is refused. One offer each way
at a time; two offers crossing make a swap. Limit: if the game is closed while an offer is held,
the held item is lost with the unsaved game state. (The in-game trade screen, protocol 14, builds on
the same escrow and ledger: "Trade screen (protocol 14)".)

### Shared item box

With shared loot both players use one item box (`sys->itm` 64..223). The box screen runs while
`cb_flg 0x40000` is set; when it closes, what was taken out and put in (inventory words as a
multiset) goes to the partner (`EV_BOX`), which applies it once its own box screen is closed (the
stream waits). Both may use a box at the same time (boxes are linked across rooms): the host
decides. Its own changes count as they are; the client's are applied where they still fit, and
an item the client took that the host's box no longer has (the host took it first) goes back as
`EV_BOXFIX` and leaves the client's inventory again ("the partner took ... first"). Individual
loot: each player has their own box. Saves keep the box as the player's game has it.

### A busy partner stays visible

The pose carries `busy` (cutscene/event, menu, item box). A partner that was shown in this room
and becomes busy stays drawn in its last pose; the overlay writes "cutscene", "menu" or "item box"
above its head (projected with the camera matrix as `bhCheckScreenOut` does). It is no enemy
target meanwhile (`proxy_ok` is off: enemies that targeted it switch to the host; no grabs, no
damage) and blocks nothing (the model has no collision). A busy host pauses its own game (as in
single player), so its enemies stand still; the client's enemies then run locally and only ever
target the client.

### The guest stuck or thrown out of the map near zombies (bug)

User report: near or grabbed by zombies the joining player sometimes could not move any more, or
was moved outside the map. Root cause: a client enemy that ran its own AI for a while - the host
in a menu (no snapshots), a time-out, the client's cutscene, a few frames after a door - could
grab the client's player locally. When snapshots came back the enemy became a puppet again
mid-grab: its AI (which runs the grab, the bites and the release) stopped, so nothing released
the player - stuck in the grab motion on the zombie's motion set - and the puppet's snapshot
positions dragged the player along (reproduced in a test: the guest frozen in motion 407 for
500 frames, then moved 19 units). Fixes:

* An enemy that holds our player (we play its motion set; of zombies, which share a set, the one
  with the hold bit) keeps running locally until it lets go, whatever made it run locally.
* Failsafe: our player in a grab state with no living enemy here holding it for 15 frames is let
  go the way en01 lets go (`mode0 1`, own motion set, pad and flags cleared).
* Position check: when a grab ends, a player more than 2.5 m from where it last was in normal
  control is put back there.
* A late M7 grab handover (`EV_GRAB`) whose proxy position is more than 1 m from where we are now
  is refused (the host releases the zombie), instead of snapping us back.

### Test aids (headless)

`RECVX_COOP_TRADE="frame:slot:count"` offers at that co-op frame, `"0:0:0:1"` / `"0:0:0:0"`
accepts / declines offers; `RECVX_COOP_BOX="frame:entry[,...]"` takes a box entry out (entry
100 + n puts inventory slot n in) as the box screen would; `RECVX_COOP_DROP="from:to"` sends
nothing in those frames, `RECVX_COOP_DROP_EVENTS` holds back only the events;
`RECVX_COOP_REJOIN="frame"` joins again (new session); `RECVX_COOP_ORPHAN="frame"` puts our player
in a grab of a zombie that does not hold it, 3 m off; `RECVX_INF_HEALTH=1` the F2 cheat.

### Verified headless (two local instances, scripted input, teleports, logs, frame dumps)

Host: new game + test teleport; client: joins from the title (`--coop-join`), `RECVX_COOP_TRACE=1`.

* **Zombies rise, triggered by the client** (0/2, the client warped onto floor area 0): client
  `world event block 0494 fired here (gate flag 1/104), 18 actions, sent`; host `replayed the
  partner's world event block 0494`, the host's frames show the cutscene (burning zombie, Claire)
  and then the graveyard with the zombies up; the client's puppets follow the host's zombies. The
  host never fired the block itself. Before the event-driven-enemy rule the client hung in its own
  cutscene (player frozen, `FrameCheck` waiting on a puppet); with it, the client's cutscene ends
  and play goes on.
* **Race, no double spawn** (0/2, both warped onto the floor at the same host frame): each fired
  the block itself and logged `the partner's world event block 0494 happened here already` for
  the other's: each ran it once.
* **Pickup that starts a cutscene, by the client, shared loot** (0/15, the statue's item, cross
  pulses: examine, Yes): client `block 0302 fired (gate 133)`, `took item 103`; host `replayed
  0302`, regroup "cutscene starts/ends", `got the partner's item 103`; the host's frames show the
  statue cutscene. The client's further presses put the item back (`039c`, replayed too) - the
  room's trap/undo cycle stayed consistent on both.
* **Lever** (0/4, "Put the lever up? Yes" by the client): client `block 0290 fired (gate 110)`,
  frames "The shutter lock has been released."; host `replayed 0290` (lever close-up in its frames).
* **Metal detector** (0/9, the client with the handgun walks through the gate): client `block
  0b86 fired (gate 53)` (alarm, shutter down); host `replayed 0b86`, cutscene on the host.
  The host pushing the release switch afterwards was not reached: the test warp in front of the
  switch ended outside its trigger (the cutscene's end places the player), so that direction
  (host triggers, client replays) is covered by the lever and pickup cases only.
* **Own inventory** (the client starts its own new game first and equips the handgun, then the
  host starts in 0/11): client `keeping our own inventory`, its inventory in the host's game
  `0005001e 000d0003` (handgun, ammo), the host's `00910001` + knife. Joining from the title:
  `no game of our own: starting with the new-game inventory`. Rejoin (new session) mid-game:
  `keeping our own inventory`.
* **Trade** (0/4, next to each other): host `you offer Playing Manual x1`, client (auto-accept)
  `you took Playing Manual x1`, host `the partner took ...`. Across a lost answer and a new
  session (the host holds back its events for 800 frames, the client joins again meanwhile): the
  client asks again and hears `the partner took Playing Manual x1` - no copy, no loss.
* **Shared item box race** (0/4: the client puts an item in; both take that entry at the same
  host frame with both event streams held back): host `1 of the partner's items were taken here
  first`, client `the partner took Playing Manual x1 out of the item box first` - one copy left.
* **Busy partner** (0/6 among zombies, the client in the status screen): host trace `partner
  drawn=1 busy=2`, `targeting partner=0` while busy (the zombies turned to the host), the label
  position computed (`head at screen 365 158`); the client was not grabbed or hit meanwhile.
  The ImGui label is not in the frame dumps.
* **Guest stuck in a grab** (0/6, the host process stopped for 25-40 s while the client stands
  among zombies, `kill -STOP`; and 0/2 after its cutscene): in a run before the fix the client's
  zombies grabbed it locally, became puppets again mid-grab, and the player stayed in the grab
  motion (407) for ~500 frames, then was dragged 19 units by the puppet's positions. With the fix:
  every local grab ended (`the grab ... ends, handed back`), no frozen player. Failsafe (`RECVX_COOP_ORPHAN`: our player put in a zombie's grab
  motion 3 m off, no zombie holding it): `left in a grab ... let go`, then `after a grab we were
  30 units away: back to (53.0 6.4)`.
* **After merging main (appearance follow-up), partner as a monster**: the client appearing as
  the Tyrant (`coop_char = 24`) raised the zombies in 0/2; the host replayed the block, loaded
  the partner as catalogue model 22 and drew the Tyrant among the zombies while they grabbed it
  (M7 handovers ending normally); no crash.

Not verified: the metal detector's release switch and the shutter switch 0/8 (needs the lever first; same mechanism), a door unlocked with a
key (flags only, as before), puzzles with several steps (RM_1050's switches set and clear several
flags inside events: partly covered), item-box use through the real box screen (the test aid
does what the screen does), the busy label on screen, Windows.

## Lobby screen and name tags (protocol 9)

The title menu's **MULTIPLAYER** entry opens a lobby screen drawn by the game itself, and the
partner's name floats above its head in game. Code: `pc/glue/lobby_glue.c` (game thread, the title
task), `pc/src/host/lobby_io.c` (main thread: keyboard text, clipboard, save files), the name tag
in `pc/src/host/overlay.cpp`. Protocol 9 (both players need this build).

### Title menu

Decomp hooks, all `#ifdef TARGET_PC` (`adv.c`, `system.c`, `adv.h`):

* `ADV_WORK.CursorFlag`/`ModeCommandId` get a fifth column (the game has 3 rows, 4 once BATTLE
  GAME is unlocked; only `adv.c` uses the struct). Mode 0 appends the row (command id 4) after
  the game's own (`pc_title_menu_init`); LOAD GAME / NEW GAME / OPTION / BATTLE GAME keep their
  indices and commands.
* `DisplayGameModePlate` draws the rows itself (`pc_title_plates`): the game's plates moved up
  16 px (4 rows: the game's own 4-row spacing; 5 rows: 30 px apart), so the last row clears the
  copyright. MULTIPLAYER is set from letters cut out of the title's own plate texture (M of GAME,
  U of SOUND, L of LOAD, T/I/P of OPTION, A, Y of EASY, E, R of NORMAL; only coordinates are in
  the code), drawn and highlighted exactly like the other plates (drop shadow, pulsing cursor).
* `CheckButton` command 4 fades the menu out to mode 30, the lobby (`pc_lobby`); it returns
  1 (back: mode 8 fades the menu in again), 2 (new game: `TitleCall(port, 2)` with `gm_mode`
  0 normal, 1 easy, 2 very easy, as the difficulty menu sets it) or 3 (a save:
  `TitleCall(port, 8)`; `bhSysCallFirstmovie` return code 8 copies the save region into
  `sys->version..save_end` and sets `ss_flg 0x200`, as the typewriter's LOAD GAME leaves it).
* Mouse (`pc_title_mouse`, from `CheckButton`): hovering a row of the game-mode, difficulty or
  extra-game menu moves the cursor, a left click confirms (the Start bit), a right click cancels;
  the OS cursor shows while a title menu or the lobby is up.

### Lobby screen

* Drawn with the game's own means: the title picture, darkened; the game's font (`bhDispFontEx`,
  `sys->ef_tlist`; the title runs without it - the boot's warning screen releases all textures -
  so the lobby loads it from the system partition as the option menu does, and releases it when
  it closes); rows and values in the font; arrows and panels with `njDrawPolygon`.
* 3D preview: no room at the title, so a scene of its own: `cam.mtx` points at the lobby's view
  while it draws (`cmat`, the camera 34+ units in front, standing back for tall models), an
  ambient and one directional light (easy/simple multi lights), `njSetScreen` as
  `bhFirstGameStart` sets it. Each preview is a player part file loaded with
  `bhReadPlayerData` into buffers of the lobby's own (the context swap coop_glue.c uses for the
  partner), playing the standing motion (PlMtnAct, 42) and turning; Claire's hair and Wesker's
  piece as `bhSetPlayer` sets them up. Enemy/NPC models use the display actors of
  `actor_glue.c` (slot 0 ours, slot 1 the partner's; free at the title), posed from that
  skeleton (`actor_pose`). The name of each is written above the model's top (`actor_work` gives
  the actor's nodes). A model choice loads once it has stayed 8 frames (cycling through the list).
  Textures: released when the lobby closes; `njReleaseTextureAll` drops them
  (`pc_lobby_textures_released`, also the actor slots when co-op is off).
* Steps (since the lobby redesign): **1** CONNECT (Direct (P2P) / Via server), HOST A GAME, JOIN A
  GAME, BACK; **2** the rows of that choice, grouped with a small gap: connection (PORT | SERVER,
  PASSWORD, RELAY | ADDRESS | SERVER, CODE, PASSWORD, RELAY), player (NAME, MODEL), game (host:
  START, GAME RULES; join: ITEMS, FOLLOW, and THE HOST'S RULES once connected), the action (OPEN THE
  LOBBY, then START THE GAME, COPY JOIN ADDRESS or COPY ROOM CODE, CLOSE THE LOBBY | JOIN / LEAVE),
  BACK; **3** the game rules (host: LOOT, FREEZE ON TRADE, EXTRA ENEMIES, REARRANGED, RANDOMIZER,
  SEED, NEW RANDOM SEED; guest: the host's enemy options, read-only), BACK. A heading over the rows
  names the step ("HOST A GAME - DIRECT (P2P)"). BACK, cancel and a right click go one step back
  (from step 1: out of the lobby, closing the session). A lobby opened during a session (F3)
  starts on step 2 of its role. Rows that cannot change in a session are dimmed (CONNECT, the other
  role, port, server, password, relay, address, loot). The help line under the rows explains the
  selected row (the enemy options and FOLLOW use the settings table's help). With too many rows
  they move closer together; a label too long for its column, or a long address, is drawn
  narrower. The 3D preview stays on the left in every step.
* Enemy options (docs/ENEMIES.md section 7): the lobby edits the same settings-table entries as
  F1 and OPTION -> PC SETTINGS (`settings_step` / `settings_set`, saved to `recvx.ini`). SEED is a
  number field (0..99999), NEW RANDOM SEED picks one. The host may change them before opening,
  while waiting and while connected: `EV_EOPTS` goes to every new partner session at once (so the
  guest's lobby shows them before the game starts) and again on every change, in the lobby and
  in game (F1, OPTION). In game a change takes effect at the next room load on both sides; a guest
  that loads a room in the frames before the event arrives (follow off, its own door) builds that
  one room with the old options until it loads it again - not locked, as before this lobby. Pad: up/down rows, left/right values,
  confirm, cancel = back. Mouse: hover selects, click activates (on a value: the left arrow
  steps back, elsewhere forward), the wheel steps a value, right click backs out.
* Text fields (name, address, port): the keyboard types (SDL text input, enabled only while a
  field is edited; the keyboard is no pad meanwhile, `pad.c`), Backspace/Delete/arrows/Home/End,
  Enter accepts, Escape cancels, Ctrl+V pastes. A gamepad changes the letter at the caret with
  up/down and moves with left/right. Names: letters, digits, space and `-_.!?'()+`, 15 characters
  (the game's font; also what the overlay can show).
* Saves: `<save dir>/card0/BASLUS-20184/SAVEDATA-nn` read directly (the game's `SAVEFILE`: the save
  region, then a byte-sum checksum), listed as "Save nn: character stage-room". The host's choice
  is read again when the game starts. The joiner's choice goes to `coop_own_source` (M8: the
  inventory kept on join) when JOIN is pressed and whenever it changes while joining; the
  new-game items: the source is set but empty (`own_restore` gives the new-game inventory).
* When the host's game state arrives while the joiner is at the title (its menu or the lobby),
  `load_blob` first ends the title as its mode -1 does (`pc_title_leave`: lobby closed, title
  textures released, title music stopped, the title's mode reset). Before, the title's textures
  and mode were left behind on that path.
* Settings in `recvx.ini`: `coop_name` (default: the login name), `coop_items` (-1 new-game
  items, 0..14 a save slot), `coop_start` (0..2 new game, 3 + slot), plus the existing keys.

### Protocol 9: the lobby extension

Every pose packet ends with a `PoseExt` (after the snapshots; the size check includes it): the
name, the model chosen ("You appear as": the pose's `chr` only exists in game), whether the lobby
is open, and a note (host: what it starts; joiner: its items). The receiver publishes them
(`g_coop.peer_name`, `peer_lchr`, `peer_lobby`, `peer_note`; only printable ASCII). A partner
connected at the title is shown next to us in the lobby (its model, name and note; "(in the title
menu)" when its lobby is closed, "is in the game" once it has left the title).

### Name tag

`coop_draw_remote` gives the partner's head position (above the head node, projected with the
camera as the busy label of M8 did, and whether it is behind the camera) every frame the partner
is drawn in our room; since "Accurate ping, smooth name tag" it goes with the frame (`stream_point`),
not through the status copy. The overlay draws one element above it: the name (option) and,
while the partner is busy, "cutscene", "menu" or "item box" below it - so M8's busy label and the
name share one box. In view: above the head; out of view or behind the camera: a marker at the
edge of the view pointing at it, with the name. Nothing when the partner is in another room or
not drawn (our cutscenes, doors). The text scales with the window height (1080 lines = 1x).

### Test aids

`RECVX_TEST_SAVE="gameframe:slot[,...]"` writes the game in progress as a save file (game task
frames); `RECVX_TEST_JOIN="n[,n...]"` presses F3's Join at the n-th frame hand-off,
`RECVX_TEST_STOP="n[,n...]"` its Disconnect, `RECVX_TEST_TITLE="n"` goes back to the title from a game
(`bhReturnTitle`, as game over "No");
`RECVX_LOBBY_KEYS="frame:text,..."` types into the field being edited (`|` Enter, `` ` `` Escape,
`<` Backspace); `RECVX_TEST_EXTRA=1` shows the title menu as after a cleared game (5 rows);
`RECVX_DUMP_OVERLAY=1` (with `RECVX_DUMP_FRAMES`) also saves the window with the overlay
(`win_<frame>.bmp`: name tags, windows); `RECVX_SHOW_OVERLAY=3` opens the F3 window at start.
A card directory written only by `RECVX_TEST_SAVE` has no system data: the boot reports it
corrupted, START continues with the default setup (the saves are still read by the lobby).

### Verified headless

Two local instances (scripted pad, mouse and keys, logs, frame and window dumps):

* Menu entry: MULTIPLAYER by d-pad and by mouse (hover, click); the 5-row menu
  (`RECVX_TEST_EXTRA`); BACK / cancel to the menu and in again while hosting (session kept; since "Leaving and
  disconnecting" BACK closes it).
* Lobby: the host opens it, the joiner (role switched to join) joins: each sees the other's model
  and name next to its own, with the other's choice. The joiner changes its model twice while
  connected (the host's preview loads each), types a new name ("Bob Smith", kept in `recvx.ini`),
  leaves and joins again (host: "partner left", new session, preview back). A name typed with
  the pad (up/down/right). 13 catalogue models in a row in the preview (Tyrant, Nosferatu, ...,
  Wesker's cutscene model), the camera standing back for the tall ones.
* Starting: new game from the lobby (the joiner, waiting in the lobby through the host's intro,
  is pulled in once the host has control); save file 1 as the host's start (loaded in 0/11 with
  its items, `00910001`), the joiner's items from save file 3 (handgun, bullets: `0005003c
  000c000f 00010001` in its inventory, the host's unchanged); same progress flags on both.
* Name tag: above the partner's head on both sides (0/11); "Guest / menu" in one box while the
  joiner has its inventory open; a marker at the edge for a partner behind the camera; no tag when
  the host went to 0/9 and the joiner (follow off) stayed.
* F3 / command line ways: `--coop-host` with NEW GAME in the title menu; the joiner in its own
  game (handgun) presses Join in game (`RECVX_TEST_JOIN`): joins in 0/11 keeping its items; a
  rejoin in game (`RECVX_COOP_REJOIN`): new session, tags back.
* Single player vs the main build: attract demo (60 frame dumps) identical; NEW GAME (53 dumps)
  identical except the title menu frames (the rows moved, MULTIPLAYER added); LOAD GAME up to
  the load screen identical except the title menu frames.
* Windows build (mingw-w64) compiles; ctest passes.

* Lobby in steps: every step and mode screenshotted (step 1 Direct / Via server; step 2 host and
  join, each Direct and Via server; step 3 host rules and the guest's read-only rules); BACK /
  cancel / right click from step 3 to 2 to 1 and out at every step; BACK to step 1 while hosting
  keeps the lobby open (CONNECT and JOIN A GAME dimmed), BACK from there closes it ("lobby: left,
  the session closes", the guest reads "The host closed the lobby"). Host and join from the lobby:
  Direct (address typed with `RECVX_LOBBY_KEYS`) and through a local `recvx-server` (server and
  password typed, room code typed by the guest, direct LAN path, encrypted); opening via a server
  without SERVER is refused. Mouse (`RECVX_MENU_MOUSE`: click, right click) and keyboard
  (`RECVX_KEYS`) in the steps. F3 at the title with Direct and Automatic. Single player NEW GAME
  frame dumps identical to the build before (48 frames). Not run: RELAY Always (only the setting).

Not verified: a real network (only 127.0.0.1), real Windows, the clipboard (headless), gamepads
(only the scripted pad), loading a save made by the game's own typewriter (the test saves are
written by `RECVX_TEST_SAVE`), the marker for a partner in view of another camera angle of a big
room (only the behind-the-camera case came up).

Limits: room names of saves are not shown (stage-room numbers; the names are message data loaded
only in game); the preview shows the model without a weapon; a joiner's choice of items applies
only when it joins from the lobby (in game, F3 keeps the current items).

## Added enemies and enemy options (protocol 10)

Protocol 10 (both players need this build). Details, numbers and verification: docs/ENEMIES.md
sections 4 and 7.

* `EV_SPAWN` (host -> guest, one reliable event per enemy): every enemy that exists on the host
  but not in the room file (the F2 spawner, the "Extra enemies" option) with its slot, source room
  file and record, type, model version, variant, position, angle and floor. The guest realizes it
  with the spawner's code into the same slot (loading the type from the source room file if its
  room lacks it); it is then an ordinary puppet (snapshots, hits both ways, death, zombie grabs).
  The guest waits with it through doors, events and menus, drops it for another room, ignores
  duplicates. The host sends the room's list again whenever the guest (re)enters its room
  (door, rejoin, after a menu: `same_room` turning true). A door clears them with the room.
* `EV_EOPTS` (host -> guest): the host's enemy options (extra enemies, rearranged placements,
  randomizer, seed), queued for every new partner session (at the title too: the guest's lobby
  shows them), before the game state (join, rejoin) and on every change; the guest
  uses them instead of its own. Placements and randomizer are computed at the room load from the
  room file and the seed only, so both sides make the same records in the same slots.
* Snapshots: more than 48 active enemy slots (a room with extra zombies) are sent as a moving
  window of 48 per packet; before, slots past the 48th were never synced.
* Fix (all rooms): a puppet no longer plays its motion on a linked part with a model of its own
  (zombie models 5/6/22/35..38 wear a cap, 40/41 a bomb and scope): the parent's motions have more
  tracks than the part has nodes, which wrote past its nodes and crashed the guest (seen with
  randomized lab-coat zombies; the same would happen in RM_3200 in vanilla). Also motion numbers
  are checked against the 512 of a set (was 1024), and a snapshot that marks a part linked is not
  applied to a slot without a parent (a different slot layout would crash `bhControlEnemy`).
* Test aid: `RECVX_COOP_STATS=1` logs packet sizes and the ping every 300 frames.

Measured (two local instances on one loaded machine, 30 packets/s): a pose packet is 160 bytes,
each enemy snapshot 100 bytes, each event 88 bytes while unacknowledged. RM_0060 vanilla, 30 enemy
slots: 3160 bytes per packet; with two extra zombies (46 slots, each zombie 8 slots): 4760 bytes.
RM_1040 (statue room) with one spawned Hunter: 160 -> 360 bytes. `EV_SPAWN` costs one event
(88 bytes, sent until acknowledged, normally one or two packets). Ping 31..34 ms in all of these
runs (spikes to several hundred ms came from the machine's load, with and without added enemies).

## Leaving and disconnecting (protocol 10, no bump)

Every way out ends the session on both sides, back to a clean idle state from which hosting or
joining works again (same port included):

| Way out | This side | The partner |
|---|---|---|
| Leaving the lobby screen (BACK, cancel, right click; not START THE GAME, not being pulled into the host's game) | session stopped | |
| LEAVE / CLOSE THE LOBBY / F3 Disconnect | session stopped | |
| Quitting, closing the window | goodbye sent at exit (`atexit`) | |
| Back at the title from a game of this session (game over "No") | session stopped, "Back at the title: the co-op session ended" | |
| A goodbye from the host | | guest: session stopped, "The host closed the lobby" / "The host left the game"; in game it plays on alone (M8) |
| A goodbye from the guest | | host: keeps hosting, "<name> left - hosting on UDP port ..., waiting for a partner"; in game it plays on alone |
| Crash, network gone | | 3 s: "partner ... not responding (n s)"; 15 s (`LOST_MS`): guest stopped, "Lost the connection to the host"; host back to waiting, "The partner timed out". Protocol 16: the same player joining again replaces its stale session at once; the host can remove it ("Remove stuck player") |

* The goodbye is `PKT_BYE`, which protocol 10 already had (sent once, by Disconnect only, and a guest
  receiving it stayed "lost" with LEAVE shown): now sent three times (a bare `Pose` header with
  our session id; UDP may drop one) by every stop, and from `atexit`. It does not go through the
  reliable event ring, so a stalled stream cannot hold it up. A guest receiving it stops its session
  (`stop()` at the frame hand-off after `receive()`); a host forgets the partner and accepts the
  next. Older protocol-10 builds still understand it (a guest of such a build shows "not responding"
  instead of idling), so the protocol number stays 10.
* The lobby (`lobby_close(keep)`, `pc/glue/lobby_glue.c`) requests `COOP_REQ_STOP` when it closes
  for any reason except a game start (`pc_lobby` returning 2/3) and `pc_title_leave` (the guest pulled
  into the host's game).
* **Busy port** (crash fixed): `net_open` returns `NET_IN_USE` (`EADDRINUSE` / `WSAEADDRINUSE`) or
  `NET_DENIED` (`EACCES` / `WSAEACCES`: Windows reserves port ranges, e.g. for Hyper-V); the lobby and
  F3 show "Port 7140 is already in use - choose another port" (or "... cannot be used (reserved or blocked by the
  system) ..."), nothing stays open, the lobby keeps working. Joining: an address without host or
  with a port outside 1..65535, an unknown host and a socket that cannot be opened each have their
  own message.
* **The lobby crash** (still there after the port mapping fix below): OPEN THE LOBBY with MODEL set to
  an enemy/NPC model ("You appear as" a display actor) crashed on every attempt, busy port or not
  (also CLOSE THE LOBBY, LEAVE, JOIN). `start()` first calls `stop()`, which put our own model back
  with `self_actor(-1)`: at the title display actor 0 is the lobby's preview, so it unloaded it and
  then `setup_hair(sys->obwp, ...)` wrote through `sys->obwp`, NULL at the title (segfault in
  `setup_hair`, coop_glue.c; Windows: access violation 0xC0000005, same frames under Wine). `stop()`
  now leaves actor 0 alone at the title (`sys->tk_flg & 0x8`), as `self_frame` only sets ours in game.
  The busy-port message itself never crashed; a user retrying with such a model saw the crash there.
* **The crash** was in the port mapping (`pc/src/net/portmap.c`): its worker freed its job when it
  finished (UPnP off, or no router found: after a few seconds) but left it as the current job, so
  the next `portmap_stop()` - every host attempt, Disconnect, CLOSE THE LOBBY, and exit - wrote into
  freed memory (AddressSanitizer: heap-use-after-free in `portmap_stop`). Now the worker leaves `cur`
  under the lock before it frees the job (also when the thread cannot be created), and the workers
  run one after the other (a mutex held for a worker's whole run), so the previous session's worker
  removes its mapping before the next one adds one: on the same port it would have deleted the new
  mapping. ctest `net` checks the busy port and start/stop cycles.
* Windows: `SIO_UDP_CONNRESET` is switched off on the socket. Otherwise an ICMP "port unreachable"
  (a partner that left) fails the next `recvfrom` with `WSAECONNRESET`, which ended the frame's
  reading every frame while we still sent there.

### Verified headless

Two local instances (scripted pad, `RECVX_TEST_*`, `kill -9`, logs, frame dumps), UPnP off:

* **Busy port**: UDP 7150 held by another program; OPEN THE LOBBY twice: "Port 7150 is in use -
  choose another" each time, no crash, the lobby usable; PORT typed as 7151, OPEN THE LOBBY:
  hosting. The joiner: `127.0.0.1:99999` ("is no address"), `nosuchhost.invalid:7140` ("Cannot
  find the host"), a port nobody hosts on (joining, the no-reply hint after 5 s).
* **Leaving the lobby screen**: the host (connected guest in its lobby) presses cancel: host "lobby:
  left, the session closes"; guest "The host closed the lobby", JOIN shown again. The host opens
  MULTIPLAYER again ("Not connected"), hosts on the same port, the guest joins again.
* **LEAVE**: host "<name> left - hosting on UDP port 7160, waiting for a partner"; JOIN again works.
* **Quitting** (`RECVX_EXIT_FRAME`, `exit()` as closing the window does): the host quits with a guest
  in its lobby: guest "The host closed the lobby"; a guest quits (lobby and in game): host "partner
  left" (and no longer re-adopts the leaver's last poses); the host quits in game: guest "The host
  left the game", plays on alone.
* **F3 Disconnect / Join in game** (host started save 1 from the lobby, the guest was pulled into it
  from its lobby without either side tearing down): guest Disconnect: host "partner left"; guest
  Join again in game: "keeping our own inventory", joined; host Disconnect: guest "The host left the
  game", its game goes on. Guest Disconnect / Join in the lobby too.
* **Back to the title from a game** (`RECVX_TEST_TITLE`, `bhReturnTitle` as game over "No"): host:
  "back at the title, the session ends", guest "The host left the game"; the guest going back:
  host "partner left".
* **Crash** (`kill -9` while connected in the lobby): the guest killed: host "partner timed out"
  (3 s), "partner gone" (15 s), back to waiting; a new guest joins the same host. The host killed:
  guest "Lost the connection to the host" after 15 s, idle; a new host opens the lobby on the same
  port at once and a guest joins it.
* ctest `net` (busy port, three start/stop cycles of the port mapping); under AddressSanitizer
  with the old `portmap.c` it reports the heap-use-after-free in `portmap_stop`, with the new one
  nothing. Windows build (mingw-w64) compiles.
* After merging main (6cf7730): busy port + retry, the lobby round (cancel, rehost on the same port,
  LEAVE, rejoin, quit), the host killed (guest idle, rehost on the same port, a new guest) and the
  host back at the title from a game, all again as above. Single player against the build before
  this change: title menu, NEW GAME and the prologue, 19 frame dumps identical.

* Lobby crash (enemy/NPC model, actor "Claire (cutscene model)"): before the fix OPEN THE LOBBY
  segfaulted on a free and on a busy port (Linux), access violation in `setup_hair` under Wine (the
  Windows exe). After: UDP 7140 held by another program, and by another running copy hosting through
  its own lobby (both under Wine, one save folder): OPEN THE LOBBY twice ("already in use", the preview
  stays), PORT 7141, OPEN (hosting), CLOSE THE LOBBY, OPEN, BACK, MULTIPLAYER again, OPEN, BACK; the
  same with "As played"; Linux and Wine, UPnP on (no router on the test network) and off. F3 Host
  on the busy port at the title and twice in game (0/6, our model the actor), then Disconnect:
  the message each time, our actor reloaded in game (Linux and Wine). ctest passes.
* Enemy options in the lobby (two instances): the host sets EXTRA ENEMIES 2, RANDOMIZER on, SEED
  4242 and opens; the guest's lobby shows the host's values once connected (`enemies: the host's
  options` logged before the game starts); after START THE GAME and teleports 0/12 -> 0/6 -> 1/0 both
  logged the same randomized records and identical slot lists (ids) in each room.

Not verified: real Windows (the `WSAEADDRINUSE` / `WSAEACCES` / `SIO_UDP_CONNRESET` paths are
compiled only), a real network, UPnP with a router (the serialized workers), the real game over
screen (the test aid calls the function its "No" calls).

## Protocol 11: checked packets, join code, shared world records

Both players need this build (protocol 10 builds are told "partner runs protocol 10, we 11"; protocol 12 below).
Code: `pc/src/net/coop_proto.c|h` (wire format and every check; no game state, so
`pc/tests/test_coop.c` runs it), `pc/glue/coop_glue.c` (applies what passed).

### Shared world changes end with the same collision (the metal shutter)

User report: the partner presses the metal shutter's button; the host sees the shutter roll up and
hears it, but cannot walk through - its collision stays closed.

How the shutter works (RM_0080, MAPS.md section 5): scd1's block `Ck ev_flg 110 set (lever, 0/4);
Ck ev_flg 10 clear; at trigger 8; Yes` starts events 5..11, 40, 41 and sets `ev_flg 10`. Event 5 is
the cutscene, event 6 moves object 6 (the shutter) and switches wall 8 off at once, wall 46 after
~100 frames and wall 10 at its end; event 11 switches walls 56/57 off. So the collision changes are
not in the block M8 replays but inside the events it starts, which then run on the replaying side's
own game: its task levels, its per-room flags (`rm_flg`), its timing. Whatever ends or replaces such
an event there before its last instruction (another event in the same task level, an `EvtKill`, a
state the partner never got) leaves the animation done and the walls as they were. A census of the
205 rooms (the M8 sweep in Python) found 394 world blocks with 44 kinds of actions; 14 events they
start switch collision only under conditions on local state (e.g. RM_0090 events 7/11, RM_6080
events 5/8). In the headless runs below the shutter's events ran to their end on both sides, so the
user's exact divergence did not show by itself; it was reproduced by ending the replayed event 6
early (`RECVX_COOP_TEST_KILL`): shutter up, walls 46 and 10 still on - the reported symptom.

Fix (general, not per room): the side whose block fired keeps a window open until the events the
block started (their task levels) have ended. Then every room record that differs from before the
block - walls, triggers, floors (`ATR_WORK`) and the placement of objects 4.. - goes to
the partner as `EV_ATR` (two records per event, with the room, case and script signature). The
receiver takes a record's on/off flag and placement; its type, attr and prm only as one of its own
room script's `WalAtariSet2` / `EtcAtariSet2` / `FlrAtariSet2` writes them for that record, and never
its id or floor ("Network review" below: retail code indexes tables with those fields). The
partner takes them over at once and again when its own replay of those events has ended (its
events may still be running and change them later). Records are synced only after a shared world
event: outside it many are set from each player's own entry door and position by the room's scripts.
A partner in another room gets the flags as before, and the room's `scd0` builds it from them.

Also (finding B5): a replay runs only as one of our own world blocks - the region the partner names
must be one the sweep of our room's script found, with the same gate flag and value, and every
action must be an instruction start inside that block's then-part, in order; actions that act on
the triggering player's own game do not run (`SetDoorCall`, `RoomCaseNo`, `ReTryPointSet`,
`ArmsItemChange`, `PlyHandChange`, `PlayerPoison2Cr`, `PlCtr`, `LoadWork` of the player and the
controller ops after it, `Sv` other than the examined trigger).

### Join code

The host's game makes an 8-character code per run from the system's randomness, as the server's room
codes (alphabet without 0/O/1/I; `RECVX_COOP_CODE` fixes it for tests, and the log warns then; the
log shows only its first half), shown in the lobby's info line, in F3 ("Join code: K7QM-3XRA") and appended by
**Copy join address** (`ip:port#CODE`). The join address takes `#CODE` or a space before it; case,
`-` and spaces in the code do not matter. Every datagram carries `auth = coop_auth(code, sender's
session id)`: the host adopts a partner only from a datagram that proves the code (and is valid in
every other respect), and the joiner only listens to a host that proves it. A sender without the
code gets `PKT_REJECT` (wrong code / the host already has a partner), at most one per second, and
nothing else. This keeps strangers who find the open port out; it is not encryption (someone who
can read the traffic sees the proofs). It combines with a server room password later
(docs/COOP_SERVER.md): the code can be derived from the room's secret.

### Packet format

Datagrams of at most 1200 bytes (`COOP_DGRAM_MAX`, below every internet path MTU: a bigger one is
split into IP fragments, and losing one loses it all). A frame's content - pose, up to 16
unacknowledged events, up to 48 enemy snapshots, the lobby extension - goes in as many datagrams
as it needs, each starting with the pose, so each is useful alone (acknowledgement, ping, its
events in order, its snapshots); the first carries the `PoseExt` (flag `PF_EXT`). The receiver
gathers the snapshots of the newest frame over its datagrams.

| Part | Bytes | Fields |
|---|---|---|
| `Pose` | 104 | `magic` "COOP", `version` 11, `type` (1 pose, 2 bye, 3 reject), `flags`, `sid`, `auth` - the first 16 bytes, all a relay needs - then `seq`, ping clocks, character, room, position, angles, motion, weapon, hp, modes, `nev`, `nene`, `stflg`, `ev_ack`, `mset`, `busy`, `reject` |
| `Event` | 88 | `id` (reliable, in order), `type`, `idx` (enemy slot), `stg`, `rom`, 80 bytes per type |
| `EneSnap` | 100 | slot, modes, model index, motion-set offset, flags, position, angles, motion, hp, hit capsule |
| `PoseExt` | 60 | name, lobby model, lobby open, note |

Enemy slots are one byte (`Event.idx`, `EneSnap.idx`, grab child) as in the decomp's 128 slots;
widening them is a version bump with a two-byte field at the same places.

Events: `EV_BLOB`/`EV_ESTATE` chunks carry a transfer id and the total size; `EV_GRAB`/`EV_UNGRAB`
name the `EV_ESTATE` transfer they need; `EV_ATR` (16) is new. Event direction is checked: only the
host sends `EV_PLY_HIT`, `EV_BLOB`, `EV_GRAB`, `EV_BOXFIX`, `EV_SPAWN`, `EV_EOPTS`; only the guest
`EV_UNGRAB`.

### What every received datagram passes first (findings B1-B12)

Nothing changes - not the peer, the session, an event stream or the game - until the whole
datagram passed `coop_check_all`: header, version, the join-code proof, counts within 16/48 and the
size exactly what they say, the pose's floats finite, and every event and snapshot within range
for its type and sender. Then, per finding:

| | Check |
|---|---|
| B1 flag words | `EV_FLAGS` 1..6 entries, words < 120, before `mask_host_changes` |
| B2 hits | joints -1 (none) or 0..63 and below the model's node count, damage 0..10000 (none on an absent joint), only the hit bits (`flg`/`flg2` 4, `comb_flg` 0x70), weapon < 23; `dam[]` saturates |
| B3 grabs | no whole `BH_PWORK` copied any more: an allowlist of field ranges (modes, position, sizes, flags, motion state, counters, damage, combo); id, type, model version/count, every pointer, the slot index and the active/linked bits stay local; the transfer must be complete with the id the grab names; the slot must be a zombie with the same linked child here; the host takes a grab back only for a slot it handed over, with the child it handed over |
| B4 snapshots | model index -1..15 and a model there; motion sets as offsets 0..511 from the enemy's own set (or room set / keep), motion 0..511 with that frame |
| B5 world events | see above |
| B6 transfers | chunks of 64 at multiples of 64, inside the total, the total the expected size; complete only when every chunk of the transfer id arrived; a new transfer id restarts it; cleared on a session change; the save region is checked before it is loaded (version, character, stage/room/case a room file exists for, difficulty, finite position, weapons < 23, items < 156); room follow only to a room that exists here |
| B7 event ring | 512 unacknowledged events; full: nothing is dropped - the push fails and the frame ends with a resync (a new session id: both streams restart, the host's game state goes over) |
| B8 flood | per frame at most 96 datagrams / 128 KB / 3 ms of reading; once a partner is there, datagrams from anyone else are dropped unread (the host answers a stranger with a valid proof "busy", rate limited) |
| B9 item box | item words id 1..155, count <= 999; applied only with shared loot; a full box sends the items back (`EV_BOXFIX` add part: back into the partner's inventory, or lost with a message when that is full too) |
| B10 MTU | 1200-byte datagrams (above) |
| B11 overlay | the overlay reads `coop_status_get()`: one `CoopStatus` copied whole under a lock once per game frame; requests stay atomic exchanges; the join address is copied bounded before use |
| B12 join | join code, complete packets validated before peer/session state changes |

### Tests

* ctest `coop` (`pc/tests/test_coop.c`): every event type valid from the side that sends it and
  refused from the other; malformed variants of each (oversized counts, joints 64/127/-2, damage
  out of range or on an absent joint, foreign flag bits, bad models and motion offsets, chunk
  lengths/offsets/totals, flag words, item words, trade/box counts, world flag sets and bits,
  NaN/inf floats); datagrams with wrong magic/version/type/size/counts/proof (wrong join code, a
  proof of another session); transfers (a chunk ending at the total does not complete it, rejected
  chunks change no byte, nothing is written past the buffer); the event ring (full: refused,
  nothing dropped, acknowledgements out of range ignored); join address parsing and codes; world
  event actions (inside an instruction, out of order, outside the block, door/player ops); then a
  fuzz loop of 200000 random and mutated datagrams (accepted ones are checked against independent
  limits). Also run under AddressSanitizer/UBSan (300000 iterations): clean.
* Two local instances (headless, scripted pad, teleports and warps, logs):
  * **Shutter, client presses** (0/4 lever by the client, then 0/8 button by the client): host
    `replayed 0c46 (20 actions)`, walls 8/56/57 off, 46 and 10 off ~100/170 frames later, `EV_ATR`
    9 records; the host walks from (5, -3) straight through the shutter line (z -11..-16) to
    z -44.5.
  * **Reverse** (host pulls the lever and presses the button, the client walks): same records on
    the client, the client walks through to z -44.5.
  * **Re-entry**: both leave 0/8 for 0/9 and come back: collision hash (walls, triggers, floors
    switched on, in the trace) equal on both, walls built by `scd0` from `ev_flg 10`.
  * **Divergence forced** (`RECVX_COOP_TEST_KILL=20:2`: the host's replayed event 6 ends after 20
    frames): walls 46/10 come from the client's records; the host walks through.
  * Joins: command line from the title; the lobby screen (MULTIPLAYER, ROLE join, JOIN, the
    address with `#ABCD2345` from `recvx.ini`); F3's Join in the joiner's own game with the code
    typed as `abcd-2345` ("keeping our own inventory"); F3 Disconnect and Join again (host "partner
    left", new session); `RECVX_COOP_REJOIN`.
  * Trade (offer, accept), item box (put in by the guest, taken out by the host, both applied),
    zombie grabs in 0/6 (17 handovers, each handed back with its state), extra enemies (3) and a
    spawned zombie (`EV_SPAWN` into the same slots on the guest, 8-24 grabs of room and added
    zombies per run handed over and back, as many as main's build in the same scenario).
  * Wrong join code (joiner: "The host did not accept the join code", session ended), no code
    ("The address needs the host's join code"), a protocol 10 joiner (host log and F3: "a partner
    runs co-op protocol 10, we 11"; the old build reads our version from the reply).
  * With main's one-holder rule for rooms with added enemies (eneset.c), a handed-over grab first
    left the guest in the grab motion (the failsafe let it go): the guest's zombie took the grab
    outside any enemy's code, so a previous holder still counted and its releasing step was undone.
    `grab_in` now registers the zombie as the holder (`pc_enemy_holds`).
  * Single player against main (6632350): new game, teleport to 0/4, walking - 145 frame dumps
    identical. Windows (mingw-w64) builds recvx.exe and test_coop.exe; ctest passes (Linux).

Measured (`RECVX_COOP_STATS=1`, two local instances, 30 frames/s):

| | before (protocol 10) | after (protocol 11) |
|---|---|---|
| pose only | 160 bytes, 1 datagram | 164 bytes, 1 datagram |
| RM_0060 vanilla, 30 enemy slots (host) | 3160 bytes in one datagram | 3 datagrams, avg 1124 bytes (max 1164), 3372 bytes per frame |
| RM_0060, 2 extra zombies + randomizer + a spawned zombie, 45 slots | 4460..4548 bytes in one datagram | 5 datagrams, max 1164 bytes, ~4880 bytes per frame |
| ping | 32..34 ms | 32..34 ms |

The pose in every datagram costs 104 bytes per extra datagram (~7%); no datagram is fragmented.

Test aids: `RECVX_COOP_CODE=CODE` (the host's join code), `RECVX_COOP_TEST_KILL="frames:levelmask"`
(ends the replayed events of those task levels that many frames into a replay); the trace prints
the room's collision changes and a collision hash per room.

Not verified: real internet paths (only 127.0.0.1), real Windows (the build compiles), a real
flood (the budget is exercised by the fuzz test only, not with a packet generator), the other 13
events of the census in their rooms (same mechanism as the shutter).

## Follow host, the partner's lighter (after protocol 11, no bump)

### Follow host: when the guest is moved

User report: with "Follow host" off the guest still followed the host through doors in some cases.
Two causes:

* **A world event's door.** M8 replays a world event block of the partner's in our room. In 45 rooms
  such a block starts an event that ends in `SetDoorCall` (a door used the first time with a scene,
  a lift, a scene that moves the player on; census over all 205 rooms). Replaying it took the
  partner through that door too, whatever its setting - and the host as well when the guest did it.
  Reproduced in 0/11 (the door to 0/12, `ev_flg 128`, event 30: rumble and door): before, the guest
  with follow off ended in 0/12 (`replayed the partner's world event block 0558`). Now a block whose
  replayed actions start a door event (directly, or through events it starts, three levels) is not
  replayed: the partner gets its flags only (`... takes it through a door: its flags only here`).
* **Every cinema regrouped.** The M5 regroup counted every start and end of cinema mode (`cb_flg 4`,
  any `CineSet`): levers, shutters, item close-ups, camera pans at a room's entry - 600 cinema events
  in the retail scripts - so a host walking into such a room pulled a guest that had follow off.

Now, with "Follow host" **off** the guest stays where it is; it is moved to the host only by:

1. joining or rejoining (it enters the host's game in the host's room);
2. the host's game (re)starting: new game, load, continue after a game over (`EV_BLOB`);
3. the guest's own game restarting (continue after its game over: it asks for the host's state);
4. a story character switch (`cng_pid`, then `ply_id`: the guest continues as the new character
   from the host's state);
5. a **story cutscene** of the host: cinema mode lasting 15 s or more (`STORY_CINE`, 450 game
   frames); the guest is pulled to the host's room when it **ends**. Shorter cinemas move nobody
   (`a cutscene of 2 s ended: no regroup` in the log). Example: the graveyard 0/2 scene (burning
   zombie, zombies rise) lasts 25 s.

With **on** the guest also follows every room change of the host (as before). The setting is the
joining player's (`g_coop.follow`, read on the guest only; the host's has no effect - F3 shows the
checkbox disabled while hosting). It can be changed at any time, also during a session: a pending
follow is dropped when it is switched off. Shown in the lobby (row FOLLOW for "Join a game"), F3 and
F1 (Co-op page).

Verified (two local instances): host through 0/4 -> 0/9 -> 0/11 -> 0/8 -> 0/7 -> 0/4 (test
teleports, the game's door transition): follow off - the guest stays in 0/4; on - it follows all
five. The 0/11 door event: before (main) the guest went to 0/12 with follow off; now it stays, and
with follow on it follows normally. The 0/2 story scene with the guest in 0/11 and follow off: host
`regroup (a cutscene of 25 s ended)`, guest `regroup: pulled to the host to stage 0 room 2`.

### The partner's lighter

How the game does it (item 55 used from the STANDARD slot makes `wpnr_no` 1, `WeaponSet`):
`bhControlPlayer` raises the arm with an IK after the motion (`bhArmIkMdk` on arm node 6 towards a
point in front of chest node 4 - the offsets differ for Claire and the others - and turns hand node 9),
starts flame effect 9 (type 2) on node 9 at `WpnTab[1].wp_fps1`, and switches on room light 1 from
`lgttab[1]` (a flickering warm point light, range 8..400) linked to node 9. The flame of type 2 and
light 1 both follow our own player only (`plp`).

The partner now gets the same from its pose (weapon 1 and the sender's character were in it
already; no protocol change): the arm IK on its model after its motion, a flame of its own (effect 9,
type 1, linked to the partner's hand; ended by us when the partner stops holding it, leaves, or is
not drawn), and light **slot 3** - reserved in every room's light table (202 rooms) and switched on
by no game code - set from `lgttab[1]` and placed at the partner's hand each frame (the light code
cannot link to the partner's work). Our own lighter keeps slot 1, so both can hold one.

Verified (two local instances, 640x480 frame dumps, the dark graveyard 0/2): the guest holding the
lighter, standing in front of the host: on the host the partner holds the lighter in front of it
with the flame, its face and the host's back lit warm; with main's build the partner stood with the
arm down, unlit. The reverse (the host holding it, seen on the guest) the same.

Not verified: the lighter in cutscenes that use it (`LighterFireSet`, event objects), a partner shown
as a catalogue model (the IK acts on the player skeleton it is posed from).

## Frozen zombies after several grabs (bug, protocol 11, no bump)

User report: some enemies freeze in their animation on both screens, mostly when several zombies
go for a grab; they no longer move, take no damage and cannot be killed.

**Repro** (two local instances, 0/6 with three extra zombies and three spawned ones, both players
warped to the same spot among them, infinite health): within a few minutes two zombies grab the
guest within the hand-over window (the host's proxy is restored after every step, so the second
zombie sees a free partner until the guest's pose shows the first grab). The guest refuses the
second (`enemy 31 grabs us, but not now (... mode 4 ...)`), and from then on that zombie stands
still on both sides. The watchdog's log showed both sides waiting on the other:
host `enemy 31 ... modes 2 0 0 1, motion 8 frame 1, hold 1, puppet here (host: the partner runs
it)`, guest `enemy 31 ... puppet here (guest)`.

**Root cause.** The refusal hands the grab back without state (`EV_UNGRAB`), naming the grab's
linked child as 255: the guest took the child from its own `owned[]`, which a refused grab never
set. Protocol 11's transaction check (finding B3) ends a hand-back on the host only with the child
it handed over - and every zombie has one (its lower body, `exp1`). So the host ignored the
hand-back and kept the slot as the guest's: it animated it only from the guest's snapshots, which
never came (the guest does not own it); the guest kept it as the host's puppet, whose snapshots
never came either (the host does not send slots the guest owns). Hits went in a circle: each side
forwarded its shots to the side it thought ran the zombie, and that side dropped them as not its
own. Every refused grab (another grab, dying, a menu, a door, "moved on") froze one zombie for
the rest of the room. Before protocol 11 the host accepted any hand-back.

**Fixes** (`coop_glue.c`, `eneset.c`):

* The guest names the child of the grab it refuses (the one the host's `EV_GRAB` named), so a
  protocol 11 host from before this fix also takes it back.
* The host ends the hand-over of a slot it handed over on every `EV_UNGRAB` for it, with the child
  it recorded; the guest's state is applied only when the child matches (the transaction), else
  the zombie is released.
* Time-out: a grab the guest runs comes back to the host when the guest, still sending, has sent
  no snapshot of it for 300 of its frames (~10 s): `no word from the partner's grab ..., ours again:
  released`. This also covers a guest that died in the grab and sits at the game over (out of the
  game it sends no snapshots; before, that zombie stood frozen on the host until the guest came
  back).
* A release (`pc_enemy_unhold`, used by every path above, the failsafe and the watchdog) puts the
  zombie and its lower body in the standing state (`mode0 1`: idle motion, then the chase), clears
  the hold bits and makes it collide with the player again (`flg & 0x40`, which the grab clears).
* One-holder rule (docs/ENEMIES.md): a zombie whose grab the rule refuses lets go at once instead
  of playing its grab and bites on nobody.
* Watchdog, a safety net (every enemy step, also single player): an alive enemy whose modes,
  motion, frame, position and hp have not changed for 150 steps (5 s) is logged (`watchdog:
  enemy N ... still for ...`, with who runs it), again every 30 s it stays so. A zombie that runs
  here and is stuck in a grab state (`mode0 2`) is released, and our player let go if it plays
  that zombie's grab. A zombie lying after a push-off (`mode0 1`, MV04, motion stopped) is only
  logged: it waits, as on the PS2, until no other zombie is getting up.

Found on the way (same scenarios):

* The guest's grab failsafe counted only zombies with hp > 0 as holders, but en01 sets a grabbing
  zombie that was shot below 0 to hp 0 (`bhEne01_NG00`): with both players shooting, the guest was
  let go at the start of such a grab (`our player was left in a grab ... let go`) while the zombie
  bit on. hp 0 counts as alive now (the watchdog the same; helpers at hp 0 are not watched).
* The guest dying near added enemies and joining again crashed it (`spawn_glue.c room_tex_push`,
  the room file pointer still NULL when the host's `EV_SPAWN` came before the guest's first room
  load); the main build crashes the same way. The spawn now waits until the room file is there.
* A guest that died kept its own hp (below 0) when it joined again (`keeping our own inventory
  (hp -8)`), so it died again at once, in a loop: a dead player's hp now starts full, as in a new
  game.

Test aid: `RECVX_COOP_LOSS=pct` drops that share of our datagrams at random. The trace line
`coop: enemies ...` ends with `guest grabs=N` (slots the guest runs).

**Verified** (two local instances, headless, 240 s per run, one pair at a time; 0/6 with three
extra zombies and three spawned ones, both players warped together three times; variants: both
standing (crowd), both shooting the handgun from frame 2000 (shoot), shooting with 15% datagram loss
both ways plus 1.5-3 s stalls (loss), the guest struggling (free), the guest without infinite health
dying in grabs (death)). Frozen = a zombie still for 5 s on a side that does not run it, or in a
grab state; counted per run as distinct slots on either side:

| | runs | guest grabs (handed over) | refused grabs | frozen zombies |
|---|---|---|---|---|
| main (with the logging only) | 11 | 161 | 10 | 11 (9 runs with 1-2) |
| this fix | 12 | 150 | 13 | 0 (one helper part, hp 0, logged by an early watchdog: not watched any more) |

* Every refused grab: `... grabs us, but not now` on the guest, `... back from the partner without
  its grab state: released` on the host, the zombie walking again on both sides. With main's build
  the guest's shots on such a zombie arrived on the host and changed nothing (`EV_ENE_HIT ... damage
  18 target_hp 60`, the same hp every time); with the fix a zombie refused early in the run was later
  grabbed again three times and shot from 132 hp to 0 by the guest.
* The guest dying in a grab (death): main's build crashed the guest when it joined again
  (`room_tex_push`); with the fix no crash, four deaths and rejoins in a run, every rejoin with full
  health, the killing zombie a puppet on the host only while the guest was at the game over.
* The watchdog never had to reset a zombie in these runs (the fixes above end every grab); the
  ownership time-out did not fire either. Its zombie-lying case (MV04) was seen and left alone.
* Single player: frame dumps of main and this branch in 0/4 (no added enemies) identical (145
  frames); 0/6 with three extra zombies, three spawned ones and a Cerberus: the same 60 hits on the
  player at the same frames, no watchdog line, no one-holder refusal of a zombie (zombies check the
  busy flag themselves; the refusal path is for mixed types).
* ctest (coop with its 200000-datagram fuzz, rdv, rdv_paths, rdv_nat, ...) passes; the Windows
  build (mingw-w64) compiles.

Not verified: real internet paths and a human pad (the struggle was scripted), two players grabbed
by the same zombie type other than en01 (only zombie grabs are handed over), the time-out firing in
a live game (no run lost a guest's snapshots for 10 s while it was in the game), and why the guest's
failsafe still lets the player go now and then at the start of a grab (seen 7 times in one run after
several deaths and rejoins, rare otherwise, also with main's build; the player is free, the zombie
ends its grab alone and is handed back - the log now lists the guest's grabs when it happens).

## Protocol 12: the partner's gun sounds, effects (EV_FX), disconnect

Both players need this build (protocol 11 builds are told "partner runs protocol 11, we 12"). The
co-op server does not look at the game protocol: no server update. Code: `pc/glue/coop_glue.c`
("effects (protocol 12, EV_FX)", `partner_arms`, `stop`, `grab_failsafe`), `pc/src/net/coop_proto.c|h`
(EV_FX and its checks), `pc/src/audio/snd_synth.c` (the second arms bank), three small decomp hooks
(`game.c` before `bhControlPlayer`, `effect.c bhControlEffect` around an effect's step, `effsub3.c`
`bhSetEffParticle(Mk2)`).

### The partner's shot played our gun's sound

User report: when the other player shoots, you hear your own gun. Each gun has an arms sound bank of
its own (`ARMS_<WpnTab[].snd_wpno>.SPQ`; the shot is its program 5, `seno0` 0x105 for nearly every
gun) and the game loads only the bank of the weapon we hold into SE bank 1
(`RequestArmsSoundBank`). M6 played `seno0` of the partner's weapon from that bank - our gun's
program 5 - whenever the banks differed.

Fix: when the partner's weapon changes (or at its first shot) its bank is read from the sound AFS
(key `snd_wpno | 0x4000`, as the game does; synchronous on the PC) and its shot bank (SPQ entry type
2: HD + BD) is handed to the sound driver emulation, which keeps it beside ours
(`snd_partner_arms`: `g_snd_banks.arms2_hd/bd`). Arms programs `0x40 + n` play program n of that
bank (`tq_bank`); the game's own programs are all below 0x40, so nothing else changes (single
player never loads it). The partner's shot is `CallPlayerWeaponSeEx(hand, seno0 + 0x40)`: the
game's 3D panning, volume and slots as before. If the bank cannot be read the shot is silent rather
than ours.

Verified (two local instances, `RECVX_TRACE_SND` key-on trace, 0/4): host handgun (item 5, wpn 3,
`ARMS_000`), guest shotgun (item 4, wpn 11, `ARMS_006`), both firing. Host log: `the partner's weapon
sounds (ARMS_006): loaded`, the guest's 10 shots key on `prog 5 ... vag @53b0 44100 Hz` - the sample
the guest's own shotgun keys on - where main's code plays program 5 of the host's own bank, `vag
@9a60`, the host's handgun. The guest hears the host's 9 handgun shots as `vag @9a60` from
`ARMS_000`, the sample of the host's own shots.

### Disconnect moving the player

User report: with the relay server closed, the host clicked Disconnect in F3 and was moved out of
the map. Reproduced the reported sequence headless several times - relayed session (Via server) and
Automatic (direct path found through the server), the server killed, the partner timing out (15 s),
the host's room re-registration failing, then `RECVX_TEST_STOP` (F3's Disconnect); also with "You
appear as" another mesh / an enemy model and with room changes before: the host's position before
and after Disconnect was the same every time (e.g. 40.9 0.0 3.8 in 0/6, 50.0 0.0 36.5 in 0/4). So
the exact report was not reproduced. What the session end and the code paths around it could do to
the local player, and is fixed now:

* `grab_failsafe` (the guest's position guard: after a grab, a player more than 2.5 m from where it
  last was in normal control is put back there) kept that point across rooms, game restarts and
  sessions: it is only updated while in control in a room, and the "back" check ran on the first
  free frame after a grab - in another room after a door, a regroup, a death and continue, a rejoin,
  or a new session as the joining side (the static survived Disconnect). That moved the player to
  another room's coordinates: out of the map. The point now belongs to a room (`safe.key`); another
  room or a game (re)start forgets it, and `stop()` clears it.
* `stop()` now also puts our player work back if an enemy step had it swapped for the partner's
  proxy (`swap_plp`; `forget()` zeroes the proxy, which would have left the player at 0,0,0), and
  `sys->ply_id`, and puts out the partner's lighter flame and light (they stayed at its last hand).

Found later (docs/ACTORS.md section 8, "Changing the appearance in game"): the teleport itself.
With "You appear as" a player file, `stop()` put the character's own mesh back and the next frame
loaded the chosen mesh again; both swaps left node matrices the game reads as the last pose
(zeroed, or from when the mesh was swapped), and the first tick of a motion that plants a foot
(`bhFixPosition`, e.g. the idle fidget) moved the player by them: to the room's origin or to an old
position. Reproduced in echo mode (Disconnect at four hand-offs during the fidget: main always at
0.1 0.0 1.1); fixed by carrying the pose over at every mesh swap (`self_carry_pose`). The earlier
runs stood still outside such a motion, so they did not hit it.

### Enemy attacks and effects (EV_FX)

User report: a vomiting zombie's attack was not visible to the joining player (through the relay).
Not the relay: the guest's enemies are puppets (M2) whose AI never runs there, and every effect an
enemy's AI starts - the zombie's vomit (`bhEne01_PoisonEffect`, effect 350, motion 10 frames 14..24),
acid and spit (256, 258: Black Widow, Alexia), the moth's powder (260), Alexia's fire (263..275), the
blood and debris of a hit (`bhSetEffParticle`, 302; 5; 370), sparks (308), Nosferatu's poison (397),
the albinoid's electricity (353, 354) - existed only on the host; direct and relayed alike. Likewise
neither side saw the partner's muzzle flash, the sparks and splashes where its shots hit, or its
grenades, rockets and bolts: those run in the shooter's weapon code only. (Motions came with the
snapshots: a Tyrant's or Hunter's swing showed before, their sparks and blood did not. The
Bandersnatch's stretched arm did not: see below.) Of these, Alexia's acid (256) is still not
sent - its caller sets the entry's matrix, which the record does not carry.

How: effects are entries of `eff[]` started by `bhSetEffect(Tb)` (a few fields set afterwards by the
caller) or `bhSetRapEff` / `bhSetEffParticle`; then each runs by itself in `bhControlEffect`. The
sender compares `eff[]` before and after the code that starts them - each enemy step on the host
(not its initialising step), our player's step on both sides (`bhControlPlayer`: aiming, shooting,
the weapon code's hit checks), and the step of our own projectile (its blast) - and sends every new
entry once as `EV_FX` (event type 17, 80-byte payload, reliable like every event): number, type,
flags, modes, position, link offset/direction, scale, angles, counters, `xn yn zn`, and what it
points to as references - an enemy slot, the sender's player, the receiver's player (the proxy), a
sub-model texture of the enemy or player that started it. Particle bursts (`bhSetEffParticle(Mk2)`:
three entries pointing at each other and at a drawing function) go as the call itself (a decomp hook
at the call), sparks (`bhSetRapEff` 308) as their 18 parameter words. The receiver starts the same
entry (`bhSetEffectTb` + the fields, or the call) and lets it run; it follows the puppet enemy's or
the partner's nodes there.

Not sent: what the receiver makes itself - the muzzle flash, smoke and shotgun blast of
`bhSetGunFire` (the receiver calls it for the partner's model at the shot mark of its pose, with the
room light and the pad vibration left out: they would follow our player) and effects on the
shooter's own hands; entries with a pointer that cannot be named or a matrix set by their caller
(e.g. the shell casings; logged once per effect number with `RECVX_COOP_TRACE`); blood pools (300).

Outcome by the owner: an entry started from `EV_FX` only shows. The step of a mirrored projectile
or blast (13 bolt, 16 blast, 130 grenade/rocket, 127 gas) runs with our enemies and our player saved
before and put back after (`coop_fx_step`), and the blasts it starts are dropped: the owner's game
hits, damages and sends its own blast (its projectile window). Enemy attack effects (vomit, acid,
powder, fire) do hurt the receiver's player where they land - the host's copy only ever checks the
host's player, so a guest standing in a zombie's vomit was never hurt before.

Budget: at most 8 `EV_FX` a frame and none while 48 events wait for the partner's acknowledgement
(cosmetic: a burst of blood on a busy link is dropped, never queued behind important events). No
datagram limit changes: events already share the 1200-byte datagrams (16 a frame, never dropped).

Bandersnatch: its arm stretch is not a motion either - its AI moves the x offsets of arm nodes 7..10
in its model (`en09.c`, `objP + 7`), so its puppet kept a short arm. The host sends them as
`EV_FX` `FX_ARM` (enemy slot, four floats) when they change; the guest writes them into its puppet's
model. Hunter and Tyrant slashes and swings are motions (they always showed); their sparks and
blood now come as effects.

Test aids: `RECVX_COOP_VOMIT="hostframe,..."` (host: the walking zombie nearest the partner starts
its vomit, en01 MV09), `RECVX_COOP_FX=0` (send no `EV_FX`: the bandwidth before), `RECVX_COOP_STATS`
now counts effects sent and dropped.

### Verified headless (two local instances; merged with main 466ec8d)

* **Vomit** (0/6, `RECVX_COOP_VOMIT`): the host's zombie 1 vomits at the partner (`fx 350 out` x6 over
  10 frames); the guest gets `fx 350 in` x6 a few frames later, and its frame dumps show the vomit
  cloud from the zombie's mouth onto the guest's player (frames 520-540 of the guest); main's guest
  showed the zombie's vomit motion with nothing coming out. Before the `lkwkp` fix (the vomit keeps its
  zombie in `lkwkp` without the link flag, its sound reads it) the guest crashed on it - found by this
  test, fixed, then six vomits in a row without a problem.
* **Shots and projectiles** (0/6, host grenade launcher, guest handgun): the guest's frames show the
  host's grenades flying (with their trail), exploding and setting the floor on fire (`fx 130`,
  `fx 352` in); the host gets the guest's hit sparks (`308`) and wall hits (`11`). Blood of hit
  zombies (`5`, `370`, particles `302`) reaches the guest. The mirrored grenades caused no `EV_ENE_HIT`
  from the guest (its first is from its own handgun shot); both sides hear and see the partner's
  muzzle flash from `bhSetGunFire` on the partner's hand.
* **Through the relay** (local `recvx-server`, Via server, encrypted): the same effects arrive
  (`fx 5`, `302`, `130`, `352` ...); the partner's gun sounds as above. Direct and relay behave alike:
  the enemy effects were missing on both before, nothing was dropped by the 1200-byte packetization.
* **Bandersnatch** (`RECVX_SPAWN=500:9@30`): `FX_ARM` sent when its arm moved.
* **Disconnect** (relay, server killed 40 s in, host appearing as Steve): the host's position before
  and after `RECVX_TEST_STOP` the same (40.8 0.0 3.8 in 0/6), the session ending as documented.
* ctest (12 tests, with the coop fuzz test and the new `EV_FX` cases) passes; the Windows build
  (mingw-w64) compiles.

Bandwidth (`RECVX_COOP_STATS`, 0/6 with 30 enemy slots, both players firing handguns for ~900 frames,
300-frame windows, host -> guest is the big direction):

| | host bytes per frame | guest bytes per frame |
|---|---|---|
| `RECVX_COOP_FX=0` (as before) | 3372-3388 | 164-186 |
| with `EV_FX` (busiest window: 68 effects) | 3391-3415 (+1.3% at most) | 169-172 (+8 bytes) |
| through the relay, grenades and burning zombies (busiest window: 66 sent, 100 dropped by the budget) | 3476 -> 3495 (+0.5%) | 164 -> 171 |

**Single player.** Frame dumps of main and this branch (new game, 0/6, handgun, shooting, walking;
`--vanilla`; 43 dumps) differ in 3 late frames (a zombie's pose from frame 1230 on), reproducibly.
Not this branch's logic: main itself, built with 16 KB of unused constant data added (so its code
segment grows as this branch's does), gives exactly this branch's frames and the same difference to
main. The game's heap (`Ps2_malloc_mem` in .bss, behind the code) moves 16 KB higher when the code
grows past a 16 KB step, and the game's single player run depends on where its heap lies (enemy AI
diverged after ~1200 frames). Any change that grows the executable past such a step does the same;
worth a look on its own (an uninitialised read or an address-dependent order in the game code).

Not verified: real internet paths and players; acid, moth powder, Alexia, Nosferatu and Tyrant
effects in their own rooms (the mechanism is the same as the vomit's; some carry pointers or matrices
the record cannot name and are skipped, logged once each: e.g. Alexia's acid 256 "matrix set by its
caller", blood pools 300); the Bandersnatch's arm on screen; rockets and bolts (the grenade path,
`130` and `13`, is the same code).

## Protocol 13: items from events (EV_GRANT)

Both players need this build (protocol 12 builds are told "partner runs protocol 12, we 13"; the
co-op server does not look at it). Code: `pc/glue/coop_glue.c` ("items handed over by events"),
`pc/src/net/coop_proto.c|h` (`EV_GRANT`, type 18, and its checks), one decomp hook (`sub1.c GetItem`).

User report: some cutscenes end by giving the player an item - Claire's handgun in the courtyard
(0/3). The partner who was elsewhere did not see the scene (as intended since "Follow host" off),
but never got the item either: the event is a world event (M8) whose gate flag reaches the partner
as a flag only, so its own script never runs the event again.

### How the game hands items over

Not with the script's item ops (`ItemGetGet(Ex)`, `PlItemChangeM`, `PlItemTamaSet`: character
inventory bookkeeping at character switches, a refill) but through the status screen: the script
sets `sb_id` to the item and `cb_flg` 0x10 (the item-get screen, as a pickup trigger does), and the
screen's `GetItem` adds it. With `cb_flg` 0x4000 as well it adds it without "Take it?" (and 0x8000
equips the handgun with three rounds fewer). `pc/tools/rdx.py grants` lists every such instruction
of the 205 rooms: 145, of which 23 "given" (0x4000) - the rest ask, i.e. they are pickups of the
player's own choice and follow the loot rules (M3/M4):

| Room | Event | Item | Started by |
|---|---|---|---|
| 0/0 (`RM_0000`) | 6, 35 (skip) | 50 Lockpick (key) | `rm_flg 8` (after the items 104 and 55 scene, `ev_flg 96`) |
| 0/3 | 0, 10 (skip) | 9 Handgun, equipped | floor 1 or 2, `ev_flg 107` clear - the courtyard scene |
| 0/15 | 0 | 103 Rusted Sword (key) | the statue: trigger 3, "Yes", `ev_flg 133/135/136`, `it_flg 8` |
| 1/5 | 33 | 33 Gold Lugers | the display case: trigger 8, "Yes", `ev_flg 146`, `14` clear |
| 2/5 | 21 | 78 Music Box Plate (key) | trigger 6, "Yes", `ev_flg 183/184` set, `185` clear |
| 3/10 | 3 | 33 Gold Lugers (after `PlItemLost` 34: the swap with Steve) | floor 17, `ev_flg 216` clear |
| 3/10 | 21 | 34 Sub Machine Gun | trigger 4, "Yes", `it_flg 62` clear |
| 6/2 | 20 | 88 Octa Valve Handle (key) | floor 2, `ev_flg 297` set, `277` clear |
| 7/4 | 0 | 131 Enhanced Handgun | trigger 5, `ev_flg 319` clear |
| 7/23 (cases 0, 1) | 0, 53 (skip) | 55 Lighter | `ed_flg 248` set, `ev_flg 365` set, `369/370` clear (Chris) |
| 9/16 | 51, 52 | 112 Blue Jewel, 111 Red Jewel (keys) | started by its event 40 |
| 9/23 | scd1 (two) | 112 Blue Jewel (key) | per-frame script |
| 9/24 | 6, 10 | 111 Red Jewel, 78 Music Box Plate (keys) | trigger 5 / floor 1 with item 78 used, `ev_flg 433..435` |
| 9/29 | 0, 11 (skip) | 106 Security File (a file: no inventory item, nothing to send) | `ev_flg 450` set, `444` clear |

Every room except 3/10 (Gold Lugers) and 7/4 (Enhanced Handgun) has an item record of that id - the event's prop (the gun
the guard drops, the lighter, the display-case Lugers).

### What happens now

* **The player who gets it.** `GetItem` reports an add the event made without asking
  (`coop_item_granted`; co-op only). Once that player is in control again (status screen, message,
  event over) the item, its count, the room and where it stands - where the event left it - go to
  the partner as `EV_GRANT` (reliable, in order, like every event).
* **The partner** drops it when it was in that event itself (it got the same item from an event
  in that room: its own `GetItem` report, e.g. the replayed world event) or already holds the item
  (inventory, STANDARD slot or item box; not for stackable items). Otherwise it keeps it pending in
  its **own save region**: `sys->itm` words 320..359, 8 entries of 5 words (floor, item, count;
  room; x y z in 1/100 units) - words no game code uses (`AllItemInit` clears them with a new game).
  So the pending item is saved with the game, kept by `own_restore` when the guest (re)joins, and
  stays when the session ends (Disconnect, timeout): it comes back with the next session or save.
* **In that room** (in control, no event or door) the partner's game shows it on the floor where
  the other player stood, with the room's own item record of that id (its model; one the room hides,
  else one it shows only as a prop without a pickup trigger, e.g. the Lugers in the display case),
  and a pickup trigger of the game's kind (type 4, crouch) in the movable objects' extra triggers
  (`sys->metcp`, which the game's action button scans). Taking it is the game's pickup: "Take the
  Handgun? Yes / No", a full inventory leaves it there. Only that player has it: the other's game
  never shows it. A room with no record of the item (3/10's Lugers, 7/4's Enhanced Handgun) gets the
  item's display model for it (protocol 15, below); before, it went straight into the inventory.
* **Duplicates.** Both in the room: both run the event (M8 replay) and each drops the other's record.
  The partner taking it later in the event itself (it was pending, then the partner replayed or did
  the event): the pending entry goes. A key item the loot code already gave (`EV_PICKUP`, e.g. the
  Rusted Sword) is "already held".

**Loot modes.** Shared loot: every event item goes to both (as every pickup does; since protocol 15
never into the inventory: the pickup the giver's loot code reports when the event's script hides the
prop and sets the item flag carries the spot only, the item comes as `EV_GRANT`). Individual loot:
weapons (with the lighter) and key items still go to both - the partner cannot run the event again,
and story items must not be lost; ammunition and other consumables an event hands over (none in the
retail game) stay the triggering player's, as their pickups do.

Test aids: `RECVX_SKIP_CINE=n` (skip each cutscene after n game frames), `RECVX_FLAGS="1:183,1:184"`
(story flags at the first game frame), `RECVX_COOP_TAKE=n` (stand 8 units from a shown event item,
then on it: cross pulses take it).

### Verified headless (two local instances, `coop_follow = 0` on the guest)

* **Courtyard handgun, full scene** (0/3; the guest joined there, then went to 0/4; the host walked
  onto floor 1): the guest got the flags only; after the 96 s scene the host `an event gave us
  Handgun x12`, `the partner gets Handgun x12 too (room 0/3 at -12.8 115.1)`; the guest `the partner
  got Handgun x12 ...: it waits there for us`, was pulled to 0/3 by the story-scene regroup, `the
  partner's Handgun x12 lies here (item slot 0, trigger 8)`, and the guest's frames show "Take the
  Handgun? Yes", "You've taken the Handgun", the handgun with 15 rounds in its inventory: `taken (we
  have it now)`. The host keeps its own (nothing is sent on the partner's pickup).
* **Skipped scene** (event 10, same grant): the same, the guest teleporting into 0/3 later.
* **Both watched** (both in 0/3, the block replayed on the guest): each `an event gave us Handgun`
  and `the partner got Handgun ... too: we were in that event` - nothing shown, no second gun.
* **Reverse** (the guest triggers in 0/3, the host in 0/4 comes back later): the host gets the floor
  pickup and takes it; at 640x480 the gun lies on the floor in front of the host.
* **The partner has one** (`RECVX_EQUIP` handgun on the guest): `we have one`, nothing kept.
* **Disconnect and rejoin** (`RECVX_TEST_STOP`, `RECVX_TEST_JOIN` with the item pending): `keeping
  our own inventory`, back in 0/3 the item lies there and is taken.
* **Key item**: Music Box Plate, 2/5 (`RECVX_FLAGS=1:183,1:184`, the host examines trigger 6, "Yes"):
  the guest got it pending, came in later, `lies here ... item slot 2`, took it.
* **Weapon, individual loot**: Gold Lugers, 1/5 (display case): no `EV_PICKUP` (individual loot),
  `EV_GRANT` - the case's prop record shown on the floor for the guest, taken.
* ctest (with `EV_GRANT` in the coop checks and the fuzz test) passes; the Windows build compiles.
  Single player against main ef639b8 (new game, 0/3, the handgun scene, `--vanilla`): 133 frame
  dumps identical.

Not verified: the other rooms of the list in their own story state (same mechanism), a full
inventory at the pickup (the game's own "can't carry" path), a save and load with an item pending
(the words are part of the save region like the inventory), more than one pending item in a room
(shown one after another).

## Accurate ping, smooth name tag (protocol 13, no bump)

**Ping.** It showed 31..34 ms on any network: both sides read the socket and sent once per 30 Hz
game frame, and the receive time and the hold time were frame times, so the round trip carried
about a frame of waiting. Now (`pc/src/net/net.c`, `receive`/`send_frame` in `pc/glue/coop_glue.c`):

* Each datagram's arrival time comes from the network, not from the frame that reads it
  (`net_rx_us`, a microsecond clock `net_us`):
  * Linux: the kernel's receive timestamp (`SO_TIMESTAMPNS`, read with `recvmsg`), a wall-clock
    time turned into "how long ago" and taken off the monotonic clock.
  * Windows: a small thread waits on the socket's `FD_READ` event (`WSAEventSelect`) and notes the
    time data arrived into an empty queue; it reads nothing (the game thread still receives and
    processes everything once per frame). That note belongs to the first datagram read after an
    empty read; later ones get the time they were read. Winsock posts the event again after a
    recv that leaves data queued, so the thread only notes when data is queued and a note older
    than the game thread's last empty read is not used. Chosen over Winsock's own receive
    timestamps (`SIO_TIMESTAMPING` + `WSARecvMsg`, Windows 10 2004+): those are produced by the
    network driver and many do not support them. Error: the thread's wake-up latency.
* The pose's `t_ms` is stamped when the frame is sent (not at the frame's start), and `echo_age`
  - how long we held the partner's clock - is send time minus that datagram's arrival, rounded
  to ms. The echoed clock is the first datagram read in a frame (the one whose arrival is exact
  on both systems). The wire format and the meaning of the fields are unchanged (ms): no
  protocol bump; against an older build the ping is as before.
* Sample = arrival - our clock - the partner's hold (our clock is whole ms, so + 0.5 ms on
  average); smoothed as an EWMA (0.1 per sample, ~10 frames); "ping N ms" in F3 and the lobby
  (both show the status line). `RECVX_COOP_STATS=1` logs it with two decimals, `RECVX_COOP_TRACE=1`
  each sample.
* `test_net` (ctest `net`) checks it: a datagram to ourselves read 50 ms after it came is stamped
  when it came.

**Name tag in enhanced mode.** The tag moved at 30 Hz over a 3D picture interpolated at display
rate, and it came from the status copy, so it could be a frame ahead of or behind the image shown.
Now `coop_draw_remote` gives the point to the frame being recorded (`stream_point`: screen x, y and
view depth, in front / behind); `stream_submit` keeps it with the frame. At every refresh the render
thread asks `stream_point_at(prev, cur, t)` with the t of the in-between frame and hands the result
to the overlay (`overlay_point`): x*w, y*w and w blended as the vertices are, so the tag stays on
the interpolated head. It snaps (cur's point) whenever the 3D does not blend: vanilla mode (t = 1
always), camera cuts and room/stage changes (`stream_match` fails), a point behind the camera or
missing in the previous frame. In vanilla nothing moves differently; the tag now belongs to the
frame shown (no 300 ms hold after the partner stops being drawn: no point in the frame, no tag).

**Tag height.** `TAG_ABOVE` = 4 units above the head node (was 2.5) for a player (head node about
`TAG_HEAD_Y` = 16 above the feet), in proportion for taller models. For a catalogue model the head
is its own: the rig's mapped head node, or its highest node when it has no humanoid rig
(`actor_head`); before, the player skeleton's node 5 was used, inside a tall model's chest.

Test aids: `RECVX_DUMP_EVERY` now also applies to the in-between (`interp_`) and window (`win_`)
dumps, and the window dumps honour `RECVX_DUMP_FROM`.

Verified (two local instances; the Codex play-test ran a third instance on the machine):

| | before | after |
|---|---|---|
| 127.0.0.1 | 31..34 ms | 0.08..0.40 ms (shown "ping 0 ms") |
| netem 20 ms each way (rootless netns, `ping`: 40.0 ms) | - | 39.93..40.08 ms |
| netem 50 ms +-10 ms each way (`ping`: 92..101 ms) | - | 92.7..105.7 ms smoothed, mean of 753 samples 100.8 ms |
| host dumping every frame (2..3 s per frame) | - | 0.1..0.3 ms (the partner held ours up to 2.7 s, taken off) |

* Tag: host in enhanced mode with `RECVX_INTERP_T=0.5`, the guest walking in 0/6: in 308 moving
  in-between presents the tag lay at the blend of the two frames' points (within 1.25 px of the
  linear midpoint; larger only where the depth changes a lot, as the 3D's perspective-correct
  blend); dumps show it above the walking partner's head in consecutive in-between images; with
  both walking (camera cuts) 1047 presents snapped. Height: in 0/6 the box clears Claire's head
  (before it touched it); the partner as Monster Steve: the tag above its head (checked with 5 units, before settling on 4).
* ctest passes (serially; `rdv_paths` failed once under `-j8` on the loaded machine, passes alone
  and in a serial run); the Windows build compiles.
* Single player against main 7a50036 (new game, teleport to 0/6, walking): all 33 frame dumps
  identical; the window dumps differ only in the fps counter.

Not verified: real Windows (the timestamp thread is only compiled; no Wine here), a real network.

## Trade screen (protocol 14)

Both players need this build (protocol 13 builds are told "partner runs protocol 13, we 14"; the
co-op server does not look at it). Code: `pc/glue/coop_glue.c` ("trade screen": request, protocol,
swap, inventory blocks), `pc/glue/trade_glue.c` (the screen: drawing, icons, input),
`pc/src/net/coop_proto.c|h` (`EV_TSCREEN`, type 19, `BUSY_TRADE`, `OPT_TRADE_FREEZE`), the binding
in `pc/src/input/bindings.c` / `pad.c`, three small decomp hooks (`pad.c bhSetPad` after `menu_pad`:
`trade_pad`; `game.c bhMainSequence` after the frame's drawing: `trade_draw`;
`ps2_NaTextureFunction.c njReleaseTextureAll`: `trade_textures_released`).

### How a trade goes

1. **Request.** Next to the partner (the F3 trade's distance, 30 units, about 3 m; same room, both
   in control: no menu, event, door or grab - the host also refuses while the guest runs a grab of
   one of its zombies) press **Trade request** (default **T**, gamepad **left stick click**, see
   docs/PORT.md "Controls"). You hear the game's confirm sound and read "Waiting for <name>...
   (T / Left stick press: cancel)" at the top of the screen. The partner hears it too and reads
   "<name> wants to trade - press T / Left stick press" at the top and in the name tag above you.
   The request ends when you press again, walk apart (more than 3 m), it is not answered within 12
   s, or a grab, menu or event starts; the partner's indicator goes after at most 15 s anyway.
   Both asking at the same time opens the screen as well (the host takes the guest's request).
2. **The screen** opens for both when the partner presses its key: our inventory on the left
   ("YOU"), the partner's on the right (its name), in the game's own look - the status screen's
   item icons (68x48 cells, two-slot items 136x48, in the order the game keeps them), the counts
   the inventory shows (weapons and ammunition; "INF" for an unlimited weapon), "E" on the equipped
   weapon - the game's font, its cursor sounds. The world behind is dimmed, not paused.
3. Each player picks one cell of **its own** side (arrows / d-pad / stick; the mouse: hover
   selects, as in the inventory) - an empty cell to only take - and **confirms** (Cross or the key
   type's confirm, Z by default; a click on the selected cell or on CONFIRM). The partner's choice
   shows live on its side (blue frame; green when ready), what goes which way in the middle. A
   pair that cannot swap says why in the line below and confirming it plays the game's buzzer.
   Any change of either selection takes back our ready.
4. When both are ready on the same pair the items swap: the selected item goes to the other side
   (a stack whole; stackable items join a stack of the same kind there, as a pickup does), and the
   other's comes back - or nothing, for a give-only / take-only swap. "Traded your X for the Y",
   "Gave the X", "Got the Y". The screen stays open for the next swap.
5. **Leaving**: cancel (Circle / X by default), a right click, LEAVE, or walking apart (a warp, an
   event): the screen closes for both, "<name> closed the trade", nothing changes. A lost partner
   (time-out, Disconnect, quit) closes it as well.

While the screen is open the player stands still (the pad and keys work the screen; buttons still
held when it closes stay off until released), the pose carries `busy` = `BUSY_TRADE` (the M8 busy
label above the head says "trading"; our own name tag over the partner is hidden while our screen
is open) and the partner is no enemy target (`proxy_ok`, as for every busy partner).

### Two-slot items, equipped weapons, key items, the lighter

* **Two-slot items** (rocket launcher, assault rifle, sniper rifle, Gold Lugers, sub machine guns,
  item 142: `itemdata[].type & 0x100`) are one inventory word drawn over a row of two cells; the
  game keeps them first (`sub1.c ItemSort`). The receiving side needs two free cells after the item
  it gives away has left - the game's own rule (`GetItem` asks for two free cells; `ItemSort` then
  makes them one row, so any two free cells do). Otherwise the pair is refused before anyone can
  confirm: "No room for the Assault Rifle: it needs two free slots" on the receiver, "<name> has no
  room for the Assault Rifle: ..." on the giver. Swapping it for an item of the receiver frees the
  cell it needs. A received two-slot item is put first as ItemSort does; the equipped index follows
  its weapon (verified: the guest's equipped knife moved from word 2 to 3).
* **Equipped weapons are refused** with "Your Handgun is equipped: unequip it in the inventory
  first" (the partner sees "<name>'s Handgun is equipped"). Unequipping from the trade screen would
  need the weapon model, the aim state and the reload to follow (`WeaponSet`) while the player
  stands in the world; the inventory already does that properly.
* **Key items** trade like any item (as F3's trade); the key-item bookkeeping (`keys_had`) follows,
  so a traded key is not read as "used up". **The lighter** lives in its own slot (word 1, not the
  grid) and is not offered, as in F3.

### Protocol: `EV_TSCREEN`, an atomic swap

| op | from | what |
|---|---|---|
| `TS_REQ` | either | request (its id, `sid`: distinct per side) |
| `TS_CANCEL` | either | the request withdrawn (key, distance, time-out) |
| `TS_OPEN` | either | accepted: both open the screen `sid`; one for a request we no longer have is answered `TS_CLOSE` |
| `TS_CLOSE` | either | left the screen |
| `TS_STATE` | either | our side, sent when it changes: grid words (10), cells (8/10), the equipped word, our cell, our word there, the partner's cell and word as we see them, ready |
| `TS_SWAP` | host | commit: transaction id, the host's cell and word, the guest's cell and word |
| `TS_DONE` | guest | the answer: made or not (and why) |
| `TS_QUERY` | host | after a reconnect: what became of transaction `tid`? |

The swap is a transaction the host commits and the guest applies, built on the F3 trade's escrow
and answer ledger (M8 "Trade"):

* The host commits only when both are ready and the guest's last `TS_STATE` shows it saw the host's
  current cell and word. It takes its own item out into **escrow** (nothing is lost if it never
  comes back) and sends `TS_SWAP`.
* The guest checks that it is still ready on exactly that pair, tries the swap on a copy of its
  inventory (its item out, the host's in: room, two-slot, equipped) and then applies it **whole or
  not at all**; it answers `TS_DONE` and keeps the answer in the trade ledger, which outlives sessions.
* The host, on "made", adds the guest's item (it checked the room before; if something filled the
  inventory meanwhile it keeps trying: "make room: X comes") and drops the escrow; on "not made" it
  gives the escrow back.
* **Disconnect mid-swap.** A session ends (timeout, Disconnect, rejoin): the screen closes on both,
  the escrow stays held. At the next session the host asks `TS_QUERY` and the guest answers from its
  ledger: "made" if it applied the swap (the host then takes the guest's item), "not made" if the
  `TS_SWAP` never arrived (recorded as refused from then on, so a late copy cannot apply). So an
  item is never in both inventories and never gone. Limit (as F3's): if the host's game is closed
  while a swap is held, the held item goes with the unsaved state; the guest's ledger is per run.
* Events are reliable and in order; every field passes `coop_check_event` (ops, direction: only the
  host sends `TS_SWAP`/`TS_QUERY`, only the guest `TS_DONE`; cells 0..9, grid sizes 8/10, item
  words as the game has them). `Pose.busy` may now be `BUSY_TRADE`; `Pose.opts` carries
  `OPT_TRADE_FREEZE`.

### Enemies stand still while both trade

Setting **"Freeze enemies while trading (host)"** (CO-OP page, F3, the lobby's FREEZE ON TRADE row;
ini `coop_trade_freeze`, default on; the host's counts and goes to the guest in `Pose.opts`). While
the screen is open on **both** sides (ours open and the partner's pose says `BUSY_TRADE`, same
room; not while a request waits) `coop_enemy_begin` holds every enemy: no AI step, its pose kept
(`puppet_step` of the unchanged state; the guest's puppets get unchanged snapshots), so nothing
walks, attacks or starts an effect. It ends the frame the screen closes, whatever closes it
(leaving, distance, an event, a lost partner; `stop()`, a time-out and a new session all clear
it); a swap keeps the screen and the freeze. The frozen-enemy watchdog ignores the time. Effects already flying when it began
(e.g. vomit) keep flying. A trade cannot start while either player is held by a zombie. With the
setting off the world runs; the guest is no target meanwhile (busy), the host's own player is.

### Input

Keyboard, gamepad and mouse as in the inventory: the key type's d-pad/stick moves, confirm toggles
ready, cancel leaves (the game's buttons through the bindings, so third person's keys work too).
Mouse: the OS cursor shows while the screen is open (`g_menu_mouse.active`; `menu_pad` leaves the
mouse to the screen), hover selects a cell of our side, a click on the selected cell or on CONFIRM
toggles ready, LEAVE or a right click leaves. F3's trade (Offer / Accept / Decline) is unchanged.

### Test aids

`RECVX_COOP_TSCREEN="hostframe:act,..."` (k the trade key, `s<cell>` select, r confirm, l leave,
d Disconnect, i log the inventory), `RECVX_COOP_TS_LOSE="s<n>"` (host: its n-th `TS_SWAP` is not
sent) / `"d<n>"` (guest: its n-th `TS_DONE`), as if lost with the old session - a rejoin then asks
again; `RECVX_COOP_REJOIN` takes a list (`"800,1170"`); `RECVX_TRADE_TRACE=1` logs every 10 frames
whether the enemies are frozen, a hash of all enemy positions, angles, motions and frames, the
nearest zombie and our position (every 300 frames every enemy). The log has "trade screen: ..."
lines and the inventory at every open, swap and close ("trade inventory (...)").

Since protocol 17 the cursor and the offer are apart and Cancel no longer leaves at once; see
"Protocol 17: the trade offer stays, item requests" below (it replaces step 3's "hover selects" and
the input paragraph above).

### Verified headless (two local instances; host new game + teleport, guest joins from the title)

Screenshots in `/tmp/trade/shots/` (copies of the window dumps named below; `runs/<name>/{h,g}/frames`).

* **Request, indicator, accept, screen on both** (0/4, `RECVX_DUMP_OVERLAY`): host "Waiting for
  Guesty... (T / Left stick press: cancel)" (`host_waiting_for_partner.png`), guest "Hosty wants to
  trade - press T / Left stick press" at the top and in the name tag (`guest_request_indicator.png`),
  then both screens (`guest_screen_open.png`, `host_screen_after_one_slot_swap.png`).
* **Swaps** (host: handgun equipped, bullets x30, assault rifle, green herb; guest: knife equipped,
  red herb, bullets x10, gas mask): one-slot (green herb for red herb), give-only (bullets x30 to the
  guest's empty cell: they joined its bullets, x40), take-only (the guest's gas mask to the host's
  empty cell), two-slot with room (the rifle to the guest's empty cell; it went to the front row).
  Inventory words before and after on both sides in the logs: every item exactly once.
* **Two-slot without room** (guest 7 of 8 cells used): refused on both sides with the message
  (`guest_two_slot_no_room.png`); the guest then picked its mixed herb instead: swapped, the rifle
  first and the knife's equipped index moved along (`guest_two_slot_swapped.png`).
* **Leaving and cancelling**: cancel key on the guest (both closed, unchanged); distance (the host
  warped 50 units away with the screen open: "Too far apart: trade closed" on the host,
  `host_closed_too_far.png`, "Hosty closed the trade" on the guest); a request withdrawn by its key,
  by walking away, and one not answered ("Guesty did not answer" after 12 s).
* **Input** without test aids: guest requested with the virtual gamepad's left stick, host accepted
  with T, moved with Left/Down, confirmed with Z; the guest moved with the d-pad and confirmed with
  south: "Gave the Blue Herb". Mouse on the host: hover selected a cell, a click confirmed, a click on
  LEAVE closed the screen.
* **Disconnect**: during an open screen with `d` (Disconnect) - host "partner left", "Lost the
  partner: trade closed", enemies move again; the guest process killed with the screen open (no
  goodbye) - host "partner timed out" after 3 s, closed, unfrozen. Mid-swap: the guest applied a swap
  whose `TS_DONE` was lost (`RECVX_COOP_TS_LOSE=d1`) and rejoined - the host asked again and took the
  guest's item (host `0009000f 00170001 00160001`, guest `00080000 00180001 00150001`: each herb once);
  the host's `TS_SWAP` lost (`s2`) and a rejoin - "not made (it did not reach the partner)", the
  escrow came back, the guest unchanged.
* **Enemies frozen** (0/4 with a zombie spawned 28 units away, `RECVX_TRADE_TRACE`): while both were
  in the screen (~210 frames) the enemy state hash stayed `b192cff1` on the host and on the guest
  (identical: the guest's puppets did not drift), the zombie at (44.01 18.22) in motion 117 frame 11
  19 units away; the frame the guest left it walked on and grabbed the host ~70 frames later. In 0/6
  (30 enemies, zombies at the door) a request while one of the players was held: "Not now: both of
  you must be free". Screenshots `host_zombie_before_trade.png`, `host_trade_open_enemies_frozen.png`.
* ctest passes (coop: `EV_TSCREEN` in the per-type checks, its malformed variants and the fuzz
  loop); the Windows build compiles. Single player against main 8097c0f (new game, 0/6, walking,
  `--vanilla`): all 78 dumps identical.

Not verified: real Windows and a real network (only 127.0.0.1), a full inventory on the host when
a swap's answer comes after the screen closed (the "make room" retry), the trade screen in enhanced
mode at other window sizes than 640x480 (the screen is 2D in the 4:3 area like the inventory),
two requests crossing (the host then opens the guest's; implemented, not provoked in a test).


## Protocol 15: event items on the floor, kills that stay, the guest's OPTION screen

Both players need this build (a protocol 14 build hears "the host runs co-op protocol 15, we 14: both need the same build"). Code:
`pc/glue/coop_glue.c` ("items handed over by events", "enemies killed while the guest ran them"),
`pc/glue/lobby_glue.c` (`pc_adv_in_game`), `pc/src/net/coop_proto.c|h` (`EV_GONE`, type 20).

### Items from missed events never go straight into the inventory

User report: a weapon handed over by a cutscene the player missed ("the Calico") was in the
inventory at once instead of lying in the room.

* **Cause.** Two paths gave it directly. (1) A room without an item record of that id - 3/10's Gold
  Lugers (event 3, the scene with Steve) and 7/4's Enhanced Handgun (event 0) - took the "no model
  here: give it" branch of the floor pickup. (2) With shared loot, when an event's script hides the
  item's prop and sets its item flag (a pickup to `diff_flags`), `EV_PICKUP` carried the item and the
  partner's `give_item` added it, wherever the partner was.
* **Fix.** (1) Such a room gets a record for the visit: the item's display model - the one the status
  screen shows for it (ITEM AFS file display id - 1, chosen as `sub1.c` does: `dsptbl` -> `itemflg` ->
  `itemidtbl`, 131 -> 5, 136 -> 33; the same mesh and size as the rooms' floor records of the item,
  measured with `pc/tools/assets.py`) - read from the disc, realized into the next free item slot as the
  room loader realizes a record (`bhSetItmMdl`; its texture uploaded, flagged as the slot's own so
  `bhReleaseMainTexture` releases it with the room's items; `rom->itm_n` grows by one until the room is
  loaded again), lying flat; then the usual per-player floor pickup (type 4 trigger in `sys->metcp`).
  (2) The pickup of an item an event gave us here in the last 300 frames goes out without the item
  (`EV_PICKUP` item 0: the spot's flag and hidden prop only); the item reaches the partner as
  `EV_GRANT`, i.e. on the floor. The "already has it" and "watched it" rules are unchanged.
* **Where it lies.** Where the giver stood when back in control in that room, as before; a giver
  hidden in its scene and then moved elsewhere (3/10: the scene ends as Steve in 3/16) sends where it
  last stood in control in that room; with no such spot (`flr_no` 63) it lies where the partner
  stands when it comes in.

### Killed enemies stand up again when the host comes back

User report: killed enemies sometimes came back when the host left and re-entered a room.

* **Repro** (two instances, 0/9, `RECVX_COOP_KILL`): the guest kills all five zombies, the host goes
  to 0/11 and back (guest Follow off, stays in 0/9). Before: records 0, 2, 4 (kill flags `ed_flg`
  51..53) hidden on the host's return; records 1 and 3 alive again - for the host and, through the
  snapshots, in front of the guest who never left. The same with the host busy in a menu while the
  guest kills (its enemies run locally then, the host's copies are untouched), and with extra
  enemies.
* **Cause.** Not the kill flags: they were set and synced in every case tested (guest kills with the
  host in the room, with the host elsewhere, during a grab handover - the dead state comes back with
  the hand-back and the host's `DieCk` sets the flag -, kill flags arriving during the host's room
  load). But RM_0090 #1/#3 and about one in five hostile records have no `DieCk` (ENEMIES.md 3.3), and
  extra enemies have none by design: the host's room load makes them alive again, and when the host's
  enemies take over the guest's, its live copies replace the guest's dead ones.
* **Fix (`EV_GONE`, guest -> host).** At the moment the host's enemies take over (the host enters the
  guest's room, or comes back from a menu: `puppets()` turns true) the guest names its dead enemies
  (slot, type, model version; active, `flg & 2`, not hidden, not a linked part) and keeps them dead
  meanwhile (snapshots that would revive them are skipped, at most 150 frames). The host, once its room
  runs, hides each still living copy of the same type and version in that slot as `EneSetCk` does
  (`stflg 0x1000000`, its linked parts go with it) and marks it dead (`flg & 2`, so a kill flag the
  record has is set by its own `DieCk` as well); entries for slots not filled yet (extra enemies come
  at the room's first frame) wait up to 300 frames. Event-driven enemies are left alone.
* **Within a visit.** This keeps what a player who stayed in the room saw. When both have left, the
  room is loaded the game's way again: enemies without a kill flag and extra enemies come back, as in
  single player. Extra enemies of a new visit may be other ones (they are chosen from the living
  records), so a killed extra is hidden only if the same type and version is in its slot again.
* **Doors.** A shot that lands in the last frames before the host's door (its `EV_ENE_HIT` arrives
  after the host's room changed) is dropped - the host's enemy is never hit; as in single player, a
  kill whose death has not run before the door (no `flg & 2`, no `DieCk`) does not count.

### The guest's crash on leaving the OPTION screen

User report: the joined player's game crashed when leaving the settings menu.

* **Repro** (two instances, the guest joined from the title; Select, PC SETTINGS, change a page,
  back, exit): segfault in `njCnkCsUvh` drawing the room model, after `Ps2CurrentTexLoad ERROR`.
* **Cause.** `AdvWork.AppMode` is the option screen's "in a game" switch; the title's own game choices
  set it to 1, the title sets it to 0. A guest that joins from the title (menu, lobby) or from the
  attract demo starts the game from the host's state (`load_blob`) and kept 0, so the in-game option
  screen left the title's way: `AdvEasyReleaseAllTexture` (`njReleaseTextureAll`, the room's textures
  too) and no `AdvPopRoomTexture`. Not the settings or their saving (`settings_poll`), nor F1.
* **Fix.** `load_blob` sets it (`pc_adv_in_game`).

### Test aids

`RECVX_COOP_KILL="hostframe:slot,..."` a lethal hit on our enemy in that slot (a puppet's goes to the
host as `EV_ENE_HIT`); `RECVX_COOP_GRANT="item:stage:room[:floor:x:y:z],..."` as if the partner got
these items from events there; with `RECVX_COOP_TRACE=1` kill flag changes (`ed_flg word ...`, ours
and the partner's), dropped flag changes, and per room the enemies' hp, dead and hidden state.

### Verified headless (two local instances)

* **Option screen** (host new game in 0/4, guest joins from the title; guest Select, up, PC SETTINGS,
  right, back, exit; then Select again): crashed before the fix, now the frames show OPTION, PC
  SETTINGS (AUDIO page), OPTION, the room again, the map. A second run (OPTION, cursor moved, exit; then
  F1 opened and closed as the guest): no crash.
* **Kills** (0/9, `RECVX_COOP_KILL` on all five zombies, guest Follow off): host away and back -
  `the host's enemies take over: 4 of ours died meanwhile`, host `2 enemies the partner killed while we
  were away hidden here` (records 1 and 3; 0 and 2 hidden by their kill flags), no `alive again` on the
  guest. Host in its inventory while the guest killed records 1 and 3: both hidden when the host came
  back. Guest kills a zombie that grabs it (slot owned by the guest): the host's `DieCk` set its flag.
  Both in the room, guest kills: flags set by the host. Extra enemies (0/6, extra 3; the guest killed
  both extras and record 1 while the host was in 0/9): the host's new extras in slots 30/31 (the same
  records, so the same types and slots) hidden, record 1 by its kill flag. Kill 11 frames before the host's door
  (Follow on): the hit arrived after the door and was dropped (the zombie lived, as documented).
* **Event items** (guest elsewhere, Follow off): 3/10 Sub Machine Gun (host at trigger 4, "Yes"):
  `the partner got Sub Machine Gun ... it waits there for us`, back in 3/10 `lies here (item slot 1)`,
  taken. 3/10 Gold Lugers (scene with Steve, not skipped, host pulses through its messages): the host,
  Steve in 3/16 by then, sends `room 3/10 at 44.0 53.0` (where it stood when the scene began; before:
  0.0 0.0, its hidden spot in the scene); the guest keeps it pending. Gold Lugers and Enhanced Handgun via `RECVX_COOP_GRANT`: `no Gold Lugers in this
  room: its display model 33 loaded into item slot 7`, `no Enhanced Handgun ...: display model 5 ...
  item slot 3`, "Take the Gold Lugers? Yes", "You've taken the Gold Lugers"; the golden Lugers drawn on
  3/10's upper floor at 640x480; the room left and entered again: slot 7 again (the file's count). 1/5
  display case (shared loot): the case's record shown on the guest's floor.
* ctest (with `EV_GONE` in the coop checks and the fuzz test) passes; the Windows build compiles;
  single player (new game, 0/6, walking, the status screen) against the base cd15399: all 49 frame
  dumps identical.

Not verified: a full inventory or a full texture heap at the display model (refused and kept pending,
logged), a save and load between the kill and the take-over, `EV_GONE` with more than 26 dead enemies
(sent in several events), real network latency for the door window.


## Protocol 16: rejoin after a crash, remove stuck player, saved partner inventories, ping

User report: after the partner's game crashed, the room stayed blocked - the partner could not
join again until a time-out passed. Requests: a crashed player takes its place back at once, the
host can remove a stuck player, the server frees a silent player's place sooner; each player's
items survive a disconnect or crash (kept by the host, per player); the ping next to the frame
rate. Both players need this build (a protocol 15 build hears "the host runs co-op protocol 16, we
15"); through a server, game and server need rendezvous protocol 3 (docs/COOP_SERVER.md 12). Code:
`pc/glue/coop_glue.c` ("saved partner inventories, rejoin"), `pc/src/net/coop_proto.c|h`,
`pc/src/net/rendezvous.c`, `pc/server/server.c`, `pc/glue/lobby_glue.c`, `pc/src/host/overlay.cpp`,
`config.c`, `settings.c`, `lobby_io.c`, `pc/src/ps2/mc.c`.

### Who is who: the player id

Each installation draws a random **player id** (16 bytes) and a **secret** (32 bytes) once, from
the system's randomness, and keeps them in `recvx.ini` (`coop_player_id`, `coop_player_key`). The
name stays display-only. A joining player sends its id and a **rejoin tag** in its `PoseExt`:
`rdv_rejoin_tag` = BLAKE2b keyed with the secret over the session's code (the host's join code,
or the room code through a server). Only that player can make the tag; it differs per code, so a
host or server that saw it can replay it only for that same session. The secret is never sent.

### A crashed partner comes back at once

* **Direct (P2P).** Before: the host kept the dead session; a rejoin within 3 s got "The host
  already has a partner" (the joiner gave up), after 3 s of silence the host took the newcomer, and
  only after 15 s (`LOST_MS`) did it give up the old one. Now a datagram from a new address that
  passes every check and carries the current partner's player id **and** rejoin tag replaces the
  stale session at once (`takeover`: "Player joined again from ... (the same player): the stale
  session is replaced"). A name, or an id without the tag, changes nothing: nobody can push a
  player out by claiming its name.
* **Through a server.** The JOIN carries the tag (protocol 3); the server keeps it with the member
  and gives a JOIN with the same tag the stale place at once (new member, new INTRO; the host's
  rendezvous takes the newer epoch and new keys, its game sees a new session).
* **Session ids are random now** (`new_sid`: `rdv_random`). They were made from the clock and an
  address, so a game started again after a crash could draw the same id as its dead session: the
  host took the new game's datagrams for the old session (no restart of the event streams, the
  frame counter older: dropped) and the rejoin hung. Found by the server test below.
* A rejoin after a session that ended without a goodbye (crash, time-out, takeover, removed) gets
  the player's saved items whatever ITEMS says (below): the host remembers the player id of a
  partner that went without a goodbye (`partner_gone`), and a goodbye (Disconnect, LEAVE, quit)
  clears it.

### Remove stuck player

Host: F3 **Remove stuck player** (next to Disconnect, while a partner is connected or not
responding) and the lobby row **REMOVE STUCK PLAYER** (same condition). It ends the session with
that partner: a goodbye that says why (`PKT_BYE` with `reject` 3: a partner that is still there
reads "The host removed you from the session (join again to come back)"), its later datagrams
ignored (`gone_sid`), and through a server **KICK** (host token + the member sequence it means):
the server frees the place, the room and its code stay, and the host's rendezvous drops the
session's keys and waits for the next introduction (`rdv_kick`; a lost KICK goes again on the next
keepalive answer that still shows the member). The same code (or address) works again at once.

### The server frees a silent partner's place

`member_timeout` (recvx-server.conf, default 10 s) frees a joiner's place when its keepalives and
relayed traffic stop; the host's room keeps `room_timeout` (30 s). The games send keepalives every
3 s now (were 10 s; protocol 3), so two may be lost. See docs/COOP_SERVER.md 12.

### Saved partner inventories

* **What the host keeps.** Per player id, the guest's own items as the guest has them: the item
  words (`sys->itm`: the four characters' inventories, the item box, pending event items), status
  flags, weapons and health - `own`, 1564 bytes. The guest sends them as `EV_OWN` once it plays in
  the host's game: the 64-byte chunks that changed, at most once a second, the change's last
  chunk marked (`pad` 1, so the host takes a change whole). The host keeps up to 16 players
  (`ponytail:` the oldest goes first).
* **Where.** With the host's saves: when the host's game writes save slot n (the memory card,
  `mc.c` -> `coop_save_written`), `<save dir>/coop-partners-nn.bin` gets the records (none: the file
  goes, so an old one never sticks to a new save). A game started from a save (title Load Game,
  the lobby's START) takes the file of the save whose item words and room are the game's; a new
  game has none. Kept with the save rather than written as it changes: after the host loads an
  older save, a key item the partner took later lies in its room again; a newer record would give
  it twice. Within a running game the records are live (a rejoin gets what the guest had a second
  before its crash). The server stores nothing.
* **What the joiner chooses** (lobby ITEMS, F3 "Items when joining"; ini `coop_items`):
  *Saved with the host* (default, -2), *New inventory* (-1), a save slot (0..14), and in F3 *My
  current game* (-3: joining from your own game keeps it). In the host's lobby the row says
  "(available)", "(none)" or "(host: off)": the host answers in its `PoseExt` (`rec`: unknown /
  none / have / off) for the save its START will load, or in a game for that game.
* **Rules.** Saved with the host and the host has a record: the joiner gets it ("Restored your
  saved items from the host"; the host's F3 status "Restored <name>'s saved items"). None, or
  "Remember partner inventories" off on the host: a new inventory, "No saved items at the host -
  starting with a new inventory" ("The host keeps no saved items - ..."). A rejoin after a crash:
  the record, whatever was chosen. A new inventory or a save chosen explicitly (no crash) wins
  over the record - and becomes the record. With shared loot the item box stays the host's.
* **How it arrives.** The host puts the record into the game state it sends that partner first
  (its `EV_BLOB` chunks carry `pad` 1), where the guest's own items would go; the guest keeps
  them instead of the items it brought (`own_restore`; not over a game it is already playing in,
  e.g. a resync). Later states of the session (story switch, host restart) keep the guest's own
  items as before.
* **Trust.** The records are the host's data, on the host's disk; a host could give a partner any
  items anyway, so the guest trusts its host (fine among friends). A player id is seen only by the
  hosts it played with; someone who knows it and the host's join code could claim that player's
  saved items at that host. Taking over a running session needs the tag (the secret).
* Setting **Remember partner inventories (host)** (F1 Co-op, PC SETTINGS CO-OP, F3; ini
  `coop_remember`, default on).

### Ping next to the frame rate

**Show ping** (F1 Display, PC SETTINGS DISPLAY; ini `show_ping`, default on): while connected,
"ping N ms" in the frame-rate counter's box at the top right, in the same style ("59.9 fps   ping
12 ms"; alone when the frame rate is off). Hidden when not connected.

### Protocol

| | |
|---|---|
| `PoseExt` (92 bytes, was 60) | `items` (guest: `ITEMS_NEW/SAVE/HOST/CURRENT`), `rec` (host: `REC_UNKNOWN/NONE/HAVE/OFF`), then `pid[16]`, `tag[16]` (guest); both enumerations checked in `coop_check_dgram` |
| `EV_OWN` (21, guest -> host) | `u.blob`: `total` = 1564, chunks of 64 at multiples of 64, `pad` 0/1 (the change's last) |
| `EV_BLOB` | `pad` 0/1 (1: its items are the guest's saved ones); `EV_ESTATE` keeps `pad` 0 |
| `PKT_BYE` | `reject` 3 (`COOP_REJ_REMOVED`): the host removed us; older builds ignore it |

### Test aids

`RECVX_COOP_KICK=n` (host: "Remove stuck player" at co-op frame n). `RECVX_TEST_SAVE` now also
writes the partners file, as the memory card's save does. The guest logs its inventory when it
joins (`trade inventory (joined)`).

### Verified headless (two instances, a local `recvx-server` where noted)

* **Crash and rejoin, direct** (host 0/4; the guest joins with its save slot 1 - a handgun - takes
  the key item 43 "Alexander's Pierce" from the floor (`RECVX_COOP_GRANT`, `RECVX_COOP_TAKE`), is
  `kill -9`ed 50 s in and started again at once): host `Player joined again ... the stale session
  is replaced`, `back after its session ended without a goodbye; we have its saved items`,
  `Restored Player's saved items`; guest 2.3 s after its start `Restored your saved items from the
  host`, inventory `0005003c 002b0001` (the handgun and the key, which its save does not have), and
  the key no longer on the floor (no "lies here"): not twice.
* **The same through a server**, Automatic (punched, LAN) and Via server (relay): server `the
  joiner left (the same player joined again)`, `a partner joined`; host restored the record 50.6 s
  in, 0.6 s after the kill; guest the same inventory. Before the random session ids this hung (see
  above).
* **Remove stuck player through a server** (`member_timeout` 60 s so only the kick can free it;
  the guest killed 40 s in, the host removes it 47 s in): server `the joiner left (removed by the
  host)`; another player (another save folder) joins with the same room code (member 2, relayed)
  and plays with a new inventory. A JOIN before the removal got "The room is full".
* **Removal, direct, live guest**: the guest reads "The host removed you from the session (join
  again to come back)".
* **Member time-out**: guest killed, nobody removes it: server `the joiner left (timed out)` ~10 s
  later, another player joins with the same code 12 s after the kill.
* **Choices**: first join with a save slot (the save's handgun), with a new inventory (`00910001`),
  "Saved with the host" without a record ("No saved items at the host - starting with a new
  inventory"); Disconnect and Join in the same game (`RECVX_TEST_STOP`/`JOIN`, a goodbye: no crash):
  "Saved with the host" restores the record, a save slot gives the save's items; "Remember partner
  inventories" off on the host: crash-rejoin gets a new inventory with "The host keeps no saved
  items".
* **Host restart**: the host saves (`RECVX_TEST_SAVE`, `coop-partners-00.bin` written), quits; a new
  host run starts save 1 from the lobby (`save 1: the saved items of 1 partner`); the guest, joining
  from the title, logs "the host has saved items for us" while the host is still in its lobby and
  gets the key back. A new game started from the same lobby: "the host has no saved items for us".
* **Ping**: window dumps with the overlay: "160.1 fps   ping 0 ms" with `show_ping` 1, "106.2 fps"
  alone with 0.
* ctest (12 tests; `coop` with `EV_OWN`, the `PoseExt` enumerations and `EV_BLOB` pads, the fuzz
  loop; `rdv` with rejoin, KICK, `member_timeout`, config; `rdv_client` with the kick; `rdv_fuzz`)
  passes; the Windows build compiles.
* Single player against the base a15c09c (new game, 0/6, walking, the status screen): the game log
  is the same line for line (also every 30th frame's draw and decode counts); 53 of 66 frame
  dumps are identical, 13 differ in about 1600 pixels by at most 10 of 255 around the characters,
  and the frame between them is identical again. The difference follows the binary, not the code
  path: this branch with one more `getenv` call at game start gives identical dumps, so does the
  base with 20 KB of unused padding; it looks like a read of uninitialized memory in the rendering
  path that the stack's history changes (not investigated further).

Not verified: real Windows, a real network, a full inventory or a record larger than the
inventory can hold, a save between a partner's crash and its rejoin, the lobby rows on screen
(their requests and labels were exercised through the same code: the F3/test-aid request and the
logged `rec` answers).


## Protocol 17: the trade offer stays, item requests

Both players need this build (a protocol 16 build hears "the host runs co-op protocol 17, we 16";
the co-op server does not look at it, rendezvous protocol 3 unchanged). Code: `pc/glue/coop_glue.c`
("the screen": `coop_trade_cursor`, `_activate`, `_back`, `_want`, `_wanted`, `TS_WANT`/`TS_DECLINE`
in `apply_tscreen`), `pc/glue/trade_glue.c` (drawing, input), `pc/src/net/coop_proto.c|h`.

### Bug: the chosen item was deselected by moving the cursor or tabbing out

User report: "when you select an item you want to trade and tab out of the window it somehow
deselects what you selected". Cause: the screen had no selection apart from the cursor. Every
cursor move was a new offer: the d-pad, and the mouse hover (`trade_pad` offered the cell under the
pointer whenever the pointer moved). `menu_mouse_update` reads the pointer position every presented
frame, also while the window is unfocused, so alt-tabbing (the pointer leaving over another cell, or
coming back over one) changed the offer, and the partner saw it change. Network updates never
touched our offer (`TS_STATE` sets only the partner's side).

Fix: `TradeView` has a cursor (`cur_side`, `cur`) and the offer (`sel[0]`) apart. The cursor moves
with the d-pad/stick, the mouse hover and the status button (Tab in third person: the other side);
the d-pad crosses from one grid's edge to the other's facing column. Only an **activation** offers:
confirm (Cross / the key type's fire) or a left click on one of our cells; on the offered cell it
toggles ready (as before). The screen opens with nothing offered (the cursor on the first item that
can go). Only **Cancel** takes the offer back: the cancel key (Circle / X by default), a right click
or the CANCEL button next to CONFIRM. After a swap the offer is cleared (the item went). Cancel
steps back one thing at a time - our request (when the cursor is on the partner's side), our offer,
our request, the partner's request (declined) - and leaves only when there is nothing to take back
(the help line at the bottom says what it does next); LEAVE still leaves at once.

### Requests

Confirm (or a click) on a cell of the **partner's** side requests that item; again withdraws it.
Requesting an empty cell or the partner's equipped weapon is refused (buzzer). The partner hears the
game's confirm sound and sees the cell marked (orange, "WANTED") and "<name> requests: Handgun
Bullets" at the top (a framed line: a click on it offers that item; with keys/pad move to the marked
cell and confirm). We see our request marked on the partner's side ("ASKED") and "You request: ..".
A request moves nothing; only the usual both-ready swap does. It ends when withdrawn (Cancel or
confirm again: the partner reads "<name> withdrew the request"), declined (the partner's Cancel:
"<name> declined the request"), when the item at that cell changes on either side (a swap, the stack
joined: logged "request ended"), and with the screen (closing, distance, disconnect, a new session).
For a computer partner: `coop_trade_wanted(cell, word)` sets / clears "the partner requests our
cell" with the same validation and display, without the network; `coop_trade_want` is our side.

### Packets (`EV_TSCREEN`, two new ops)

| op | from | what |
|---|---|---|
| `TS_WANT` (9) | either | we request the partner's `cell` (0..9), its item word as we saw it in `take`; `cell` = `TSC_NONE` and `take` = 0: withdrawn |
| `TS_DECLINE` (10) | either | we decline the partner's request: its `cell` and `take` (a later different request stays) |

Checks before anything changes: `coop_check_event` - op range, `cell` 0..9 or `TSC_NONE`, `TS_WANT`
a cell with an item word that passes `coop_item_word_ok` or neither, `TS_DECLINE` both; the receiver
then takes a request only for the open screen (`sid`) and only when our real inventory (read again)
holds exactly that word at that cell, else ignores it (logged "refused: not what we have there"); a
decline only for the request we have. Fixed size, no new lengths. ctest (`coop`): valid requests from
both sides, the withdrawal, and malformed ones (a withdrawal with an item, a cell without one, cell
10, item 156, count 1000, a decline without cell or item); the fuzz oracle knows the new rules.

### Test aids

`RECVX_COOP_TSCREEN` actions (host frames as before): `c<cell>` / `cp<cell>` the cursor on our / the
partner's cell, `a` confirm on the cursor, `b` cancel, `w<cell>` request (`w-1` withdraws), `o` offer
the requested item, `x<cell>` send a request naming a different item than the one there, `P<hex>` pad
presses through the screen's input (`8` right, `2` down, `800` confirm, `1000` cancel, `4000` status),
`M<x>/<y>[l|r]` the mouse at game 2D x/y, l/r click (run with `RECVX_MENU_MOUSE=` empty so the real
pointer does not move it). The log has "cursor on ..", "we offer cell ..", "<name> offers cell ..",
"we request ..", "<name> requests: ..", "request ended", "declined".

### Verified headless (two local instances, 0/4, host Handgun (equipped), Bullets x30, Assault Rifle, Green Herb; guest Knife, Red Herb, Bullets x10, Gas Mask)

* **Offer stays**: the host clicked its Green Herb (offered cell 4), then the pointer hovered its
  Bullets, the d-pad went right (onto the guest's side), Tab back, the pointer hovered the guest's
  Knife, the d-pad down: the cursor moved six times, the offer stayed cell 4 (the guest logged
  "Hosty offers cell 4" once). Screenshot `scratch/trade_selection_kept_host.png` (white cursor on
  the Handgun, yellow offer on the Herb).
* **Request and swap**: the guest requested the host's Bullets (`trade_request_guest_asks.png`:
  "You request: Handgun Bullets", the host's cell marked ASKED; `trade_request_host_sees.png`:
  "Guesty requests: Handgun Bullets", the cell marked WANTED); a request naming another item than
  the one there was refused; the host clicked the request line (offered the Bullets), the guest
  offered its Red Herb, both confirmed: "Traded your Handgun Bullets for the Red Herb", the guest's
  stack x40; the request ended on both sides.
* **Cancel**: the host requested the Gas Mask and pressed cancel ("Hosty withdrew the request" on
  the guest); offered its Green Herb and clicked CANCEL ("we offer nothing", the guest saw cell -1);
  cancel again declined the guest's Red Herb request ("Hosty declined the request"). A guest request
  open when the guest disconnected: the host closed the screen ("Lost the partner"), requests gone.
* ctest passes; the Windows build compiles.

Not verified: a real alt-tab (headless has no window focus; the pointer moves that alt-tab causes
were played through the same path), a real gamepad, real Windows.

## Protocol 18: the partner's voice

User request: the voice should match the model shown ("You appear as"), for us and the partner.
The voice banks and the mapping are in docs/ACTORS.md section 8, "Voices"; the co-op part:

* `Pose.voice_n` counts the sender's own player voices (every `CallPlayerVoice` of SE bank 4:
  hits, death, grab and struggle screams, scripted ones), `Pose.voice` is the last one's program
  (`0x400 + voice`); `Pose` grows from 104 to 108 bytes (`pad[2]`). The receiver
  (`partner_sounds`, once per new pose, before the motion-set check so the partner's grabs are
  heard) plays each new count at the partner's position, in the voice of `Pose.chr` (or of
  `Pose.mfile & 3`, the partner's character, for "As played" and creatures that keep it):
  `CORE_00n` as an extra bank when that is not our character, else our bank 4. A count that
  changes after a gap of 10 or more of the partner's frames (other room, busy) is taken without
  playing. Voices played for the partner are not counted, so nothing echoes back.
* Replaces M6's rule (1026 on `mode0` becoming 2, only for the same character): the partner's
  death and its grabs are heard now, and in its voice.
* Host: a voice call during a host enemy's step on the proxy (`plp` is `ply2`) plays nothing. The
  partner's own game runs what follows (the zombie's grab after `grab_out`, a hit through
  `EV_PLY_HIT`) and sends that voice; before, the host played these in its own voice, a second
  time for each grab that went over.
* No guest voice choice needs to be sent: `Pose.chr` already carries the appearance.
* An AI partner (branch ai-partner, which fills the partner's pose itself) is silent unless it sets
  `voice_n`/`voice`.

Verified (two local instances, 0/6, `RECVX_TRACE_SND=1`, `RECVX_COOP_TRACE=1`): see docs/ACTORS.md
section 8, "Voices". ctest passes; the Windows build compiles.

## Enemy targets: one victim per attack (protocol 18, no bump)

User reports: enemies "in attack range keep walking next to the player without properly attacking,
even for the host"; and "an enemy attacked both of us standing together, and one player stayed stuck
in the animation afterwards".

Test aid: `RECVX_ENEMY_TRACE=dist` logs one line per step of each living enemy within `dist` units of
a player (single player, host and guest): its target (`t=H` the host's player, `P` the partner's
stand-in, `-` single player or guest), both distances, modes, motion, `hp.mode0` of both players, a
zombie's flag word (0x20000 holding), run here or puppet, positions (`coop_glue.c enemy_trace`).

Root causes (two local instances, traces):

1. **Targets changed in the middle of an attack.** The nearer-player rule ran at every step, also
   while the enemy was grabbing, biting or releasing. A grab started on one player and continued on
   the other: the zombie's grab code (`bhEne01_PlayerControl`, which drives the held player) went on
   with the other player's work, so the first player stayed in the grab motion with nothing to release
   it, and the second got the rest of the grab (the hand-over to the guest, or a Cerberus's release
   hit). Before the fix, 0/6 with both players standing together at the entry: one such flip left
   the host in a zombie's grab motion for 1867 frames (to the end of the run); with the guest walking
   around the host, 14 flips mid-attack and 1139 frames held by nobody (longest 520). Now an enemy past
   its idle/chase state (`mode0 > 1`: grab, hit, death; a zombie still holding) keeps its victim.
2. **The stand-in had no hit capsule.** `ply2.watr` stayed 0,0,0: every attack that tests the
   player's capsule (`npCollisionCheckSC(.., &plp->watr)`, `bhEne_AttackHitCheck`) missed the partner -
   Cerberus's leaps, Hunter, Tyrant, Bandersnatch, Nosferatu, Gulp Worm, Giant Black Widow, the
   zombie's lunge grab variant. The enemy swung or leapt, missed, and tried again next
   to the guest. Now set every frame from the partner's model as `player.c` sets ours.
3. **Enemies waited beside a free player.** With both players together, all enemies kept the host
   (hysteresis on equal distances); when one of them held the host, the others saw a busy player
   (`flg & 4`) and waited next to it (zombie `mode2 6`) while the partner stood free beside them
   (1341 such enemy frames within 0.9 m of the free partner in one run, no attack on the partner at
   all). Now a busy target (being hit or held, dying) is left for the free player within 4 m, and a
   busy player is never chosen. The partner counts as busy from its pose (below) and while the guest
   runs a grab handed over to it (also during the round trip before its pose shows it): the enemies
   then see `flg & 4` on the stand-in and wait their turn, as for our player.
4. **The stand-in's floor was the host's** (outside the PC slope rooms): enemies attack only on their
   own floor, so a partner on another level of a room was never attacked there.
5. **Grabs the guest's game does not run** (every type except the zombie, M7) were undone on the
   stand-in while the enemy played its grab on nothing for seconds, the guest getting only the bites'
   damage. Now such a grab reaches the guest as a hit (the hit reaction, from the enemy's side), and a
   Cerberus lets go after its first bite.
6. **Grab failsafe never fired for zombies.** The guest's failsafe (M8) counted a zombie's helper parts
   (ids 31..38, which share its motion set) as holders, so an orphaned zombie grab was always "held".
   Parts are no holders now; the host runs the failsafe too (only for its player on an enemy's motion
   set, holders being enemies whose steps run against the host's player). `RECVX_COOP_ORPHAN` on
   either side: `our player was left in a grab ...: let go` (before: never, on both).

Code: `coop_proto.c coop_pick_target` (the rule, ctest `coop` checks it: margin, no flip-flop on equal
distances, commit, busy, reach), `coop_glue.c coop_enemy_begin/end`, `partner_held`, `grips_us`,
`grab_failsafe`, the partner's pose (`watr`, `flr_no`, `flg`).

**Pose.** The two pad bytes of protocol 18's `Pose` now carry the sender's `flg & 6` (`pflg`: hit or
held, taken at the frame hand-off after the enemies' steps) and its floor (`flr` = `flr_no + 3`). A
protocol 18 build sends zeros there: the receiver then falls back to the old floor rule and to the
partner's modes for "busy". So builds before and after this change still play together; both players
should update to get the fixes for the partner's side.

Measured (two local instances, `RECVX_ENEMY_TRACE`, `RECVX_INF_HEALTH`, the same scripts before and
after; attacks = hit reactions and grabs of each player, frames at the 60 Hz frame counter):

| Scenario | single player | before: host / guest | after: host / guest |
|---|---|---|---|
| 0/6 zombies, both standing at the entry, 6000 frames | 30 grabs | 18 / 6 (host held by nobody 1867 frames) | 21 / 20 (0) |
| 0/6, the guest walking around the host | | 21 / 9 (14 target flips mid-attack, 1139 frames held by nobody) | 16 / 17 (0, 0) |
| 1/4, two spawned Cerberus, both standing, 5000 frames | 19 hits (2 grabs) | 19 / 0 (the guest: 2 bites as bare damage) | 11 / 9 (16 hit events) |
| 1/4 Cerberus, the guest walking around | | 26 / 0 | 11 / 19 |

Time to the guest's first attack after an enemy got within 0.9 m of it (0/6, standing together):
2523 frames before, 181 after (the host's: 19 single player, 19 before, 34 after - the zombies now
share). Single player: `pc/tools/layout_check.sh` HEAD against this build, vanilla 155 and enhanced
299 frame dumps identical.

Not verified: a room with two floor levels (the floor field), Hunter, Bandersnatch and Tyrant in
co-op (same capsule test as the dog's leap), real internet latency (the stand-in is the partner's pose
one way behind: an attack lands where the host saw it).

## Network review (protocol 18, no bump)

A read-only review of the network code (`pc/src/net`, the receiving and checking parts of
`coop_glue.c`, `pc/server`) found the issues below; all are fixed without a wire change, so builds
before and after still play together (the server side: docs/COOP_SERVER.md section 13).

* **World records (`EV_ATR`) could set any field of a room record.** The receiver copied the whole
  `ATR_WORK`, so a partner could give a trigger a `prm0` up to 255; retail code indexes 32-entry
  tables with it (`sys->itwp[prm0]` when an item trigger is examined, `sys->obwp[prm0/prm2]`,
  `rom->etcp[prm3]`, `kaidan_ang[prm2]`, ...). Now the receiver takes the on/off flag (`flg`) and
  the placement (`px..d`); `type`, `attr` and `prm0..3` only when they are exactly what one of the
  room script's own `WalAtariSet2` / `EtcAtariSet2` / `FlrAtariSet2` (ops 123, 37, 124: they write
  all of these at once) writes for that record - found by the M8 sweep of `scd1` and the events at
  room entry (`coop_atr_set`, `coop_atr_merge` in coop_proto.c) - otherwise ours stay; `id` and
  `flr_no` always stay ours (no script op changes them). Objects only in slots 4..31. Retail world
  events change records exactly that way (event.c: the flag by `*AtariSet`, the fields by
  `*AtariSet2`, positions by the trigger moves), so nothing a shared event needs is lost:
  the shutter below sends the same 9 records, all taken. The trace says when a field was not taken.
* **Pose floor and motions.** `Pose.flr` must be 0 or `flr_no + 3` with `flr_no` -2..29
  (`rom->grand[2 + flr_no]` has 32 entries). A motion number from the partner (its pose on an enemy's
  set, a snapshot, a grab's state) is checked against what is left of the 512-entry set it indexes:
  an enemy's set may start mid-way (`mnwP = mnwPb + offset`), so `mtn < 512` could read up to 511
  entries past its 12 KB (`set_left`).
* **Receiving.** The 3 ms read budget is checked before a datagram is read: before, a datagram could
  be read (and decrypted, which uses its counter) and then dropped. `net_recv` skips a datagram
  too big for the buffer (Windows `WSAEMSGSIZE`) and ICMP errors (`WSAECONNRESET`, `ECONNREFUSED`,
  ...) instead of ending the frame's reading.
* **Join code** from the system's randomness (`rdv_random`, `rdv_code_from`, as room codes); it
  was a hash of the clock and an address (about 2^20-2^28 tries). The log shows only its first half.
  Not added: a limit on wrong join proofs per address (a guesser needs ~2^39 datagrams on average,
  years at the 96-a-frame read budget; spoofed sources would get around a per-address limit anyway).
* **Direct joins by host name** are looked up on a thread (as the server's name, at most 5 s: "Cannot
  find the host ..."); before, DNS stalled the game thread. Nothing is sent until the address is there.
* Smaller: a stale partner's item chunks (`own_in`) are cleared when it goes; a takeover needs a
  non-zero rejoin tag; the UPnP permanent-mapping fallback is gone (above); `_Static_assert`s pin the
  raw structs on the wire (`BH_PWORK` 1408 bytes for `EV_ESTATE`, the 0x834-byte save region for
  `EV_BLOB`, 512 motions per set) - a 64-bit build would be another wire format; the log warns when
  `RECVX_COOP_E2E=0` or `RECVX_COOP_CODE` is set; the join code is checked with the room-code
  function (`coop_code_norm` and `coop_code_new` are gone), 16-byte compares use `crypto_verify16`.
* Not changed: `partner_held()` stays per enemy (128 byte reads per enemy step, microseconds a frame;
  a per-frame cache would miss a grab handed over earlier in the same frame).

Tests: ctest `coop` - `EV_ATR` objects outside 4..31 and angles past an int refused, objects 4..31
pass; `coop_atr_set` decodes the three ops (and not the flag-only `EtcAtariSet`); `coop_atr_merge`:
a record with `prm0` 255 and every field changed keeps type, id, floor, attr and prm and takes flag
and placement; the script's own write is taken for that record only, not for another index or kind,
not with one byte off; `Pose.flr` 33 and 255 refused, 1 and 32 pass; 100 join codes from
`rdv_random` round-trip through `rdv_code_norm` and differ; the fuzz loop's independent limits
include the object slots. ctest `net`: an oversized datagram before a small one. Two local
instances (host new game, guest from the title, `--coop-join 127.0.0.1:PORT#ABCD-2345`, the host's
name looked up on the thread):
* **Shutter, guest presses** (0/8, `RECVX_FLAGS=1:110` for the lever, the guest warped facing the
  button, cross + Yes): guest `world event block 0c46 fired here`, `9 room records to the partner`;
  host `replayed ... 0c46 (20 actions)`, walls 8/56/57 and trigger 9 off at once, 46 ~100 and 10 ~170
  frames later on both sides, `its 9 room records again`, no record field refused.
* **Divergence forced** (`RECVX_COOP_TEST_KILL=20:2` on the host): walls 10/46 off from the guest's
  records when the window closed.
* **Trade screen** (0/8, host handgun + bullets x30): host offers the bullets, the guest an empty
  cell, both confirm: the guest has `000d001e`.
* **Grabs** (0/6, 30 zombies, 2 minutes): 10 handovers, each handed back; no stuck player.
* A direct join to `nosuchhost.invalid:7140#...`: "Cannot find the host 'nosuchhost.invalid'", the game
  never stalled.
* Single player: `pc/tools/layout_check.sh` main (5140d47) against this build: vanilla 156 and
  enhanced 299 frame dumps identical. The Windows cross build (mingw-w64) compiles; not run.

## Glue fixes (protocol 18, no bump)

From a review of the co-op glue (`coop_glue.c`, `spawn_glue.c`, `enemyopt_glue.c`, `actor_glue.c`,
`lobby_glue.c`, `cheat_glue.c`). Protocol 18 unchanged: a build before and after these still plays
together.

* **A grab the guest ran stayed a frozen puppet** when the guest left (goodbye), was removed or timed
  out: `partner_gone` now drops the guest's grabs (`owned_drop`: "coop: N enemies of the partner's grabs
  are ours again: released"). Before, the zombie stood in its grab until the room was left.
* **Individual loot in a stalled link.** The guest took the loot mode only from poses that arrived:
  3-15 s without one (a stall) counted as shared, and its item flags and pickups took the host's copies.
  It keeps the host's last options now (`peer_opts`, until the session ends); the host also ignores
  the guest's item flag words and non-key pickups while it plays individual loot ("the partner's pickup
  of item N ignored (individual loot)": an older guest).
* **A host's added enemy that came while the guest's room did not run** (a door, an event) stopped
  the guest's event stream for up to 30 s (`spawn_note_apply` returned "not yet"). It waits in a list
  now (`spawn_pending`, "waits until our room runs"), placed once the room runs, in order, dropped after
  900 frames; the stream goes on.
* **Randomizer: host and guest could differ.** Each side refused a randomized record whose type did not
  fit its own texture memory, and the guest's puppets follow the host's slots. Now the host sends the
  records it had to keep (`EV_SPAWN`, `SpawnNote.pad = SPAWN_NOTE_KEPT`, idx = the record; an unused
  byte no build checks: an older guest finds the record in its slot and ignores the note) once the guest
  is in game and in control, and before each game state it sends; the guest keeps those records too at
  its next load of that room. The guest's randomized spawns and the host's added enemies take no texture
  reserve on the guest (`SPAWN_DICTATED`: only what the upload needs, 4 entries and 704 KB heap), so it
  does not refuse what the host placed. Limit: a guest that loads the room at the same moment as the host
  (following it through the door) decides before the host's note can come; it logs "the host kept record
  N ... (this room was loaded before the host's)". With the 64 MB texture heap a refusal needs a nearly
  full heap.
* **Trade escrow on disconnect.** An item of ours held for an F3 offer or a trade screen swap without its
  answer (or refused/taken and waiting for room) lived in memory only: a save before a reconnect lost it.
  When the session ends (host: the partner gone; both: Disconnect, back to the title) it goes back into
  the inventory, else the item box ("trade not answered (session ended): ... back in your inventory").
  The next session still asks (`QUERY`); "taken" after all takes our copy out again ("was taken after
  all: our copy is gone again"; if it was used meanwhile it stays and is logged).
* **World event windows** (`world_sync`) start again on continue, game over and load in the same room
  (`atr_key` changed only with the room).
* **Single player in retail rooms as retail**: the frozen-enemy watchdog only logs there (its repair -
  a stuck zombie back to standing, the player let go - runs with co-op on or in a room with added
  enemies), and effect.c hides an effect with a room effect texture the room lacks only in rooms with
  added enemies (`pc_enemy_added`).
* The guest's copy of the host's enemy options and kept records ends with the session
  (`enemy_opts_from_host(NULL)` in `stop`).
* Fewer reads: the host at the title reads the chosen save's partners file once (again after
  `coop_save_written`), not every 64 frames; the lobby reads the 15 saves when it opens, not every
  second; a lobby preview whose part file cannot be read is tried again after 2, 4 .. 32 s.
* Code: `coop_frame`'s test aids are in `coop_testaids.c` (one call, `CoopTest`); one player part file
  loader for co-op, the lobby and the display actors (`actor_glue.c afs_read`, `player_model_read`,
  `player_hair`); room keys from `coop_room_key()` / `room_file_now()`; the lobby and OPTION screens
  share `menu_mouse_take`. The trade screen stays in `coop_glue.c`: it shares about 25 of the file's
  statics (the event queue, the inventory block helpers, the ledger, the session state).

### Test aids

`RECVX_COOP_BYE_GRABBED="n"` (guest): Disconnect n frames into a zombie grab the guest's game runs.
The test aids moved to `coop_testaids.c` (REJOIN, KICK, KILL, WARP, VOMIT, `RECVX_TRADE_TRACE`) act at
the start of the frame: KILL, WARP and VOMIT one host frame later than the number given.

### Verified headless (two local instances; `--coop-host` with a scripted new game, the guest joining from the title)

* **Grab, then the guest leaves** (0/6, both standing at the entry, `RECVX_INF_HEALTH`): with
  `RECVX_COOP_BYE_GRABBED=20` the host logs "partner left", "2 enemies of the partner's grabs are ours
  again: released"; the guest `kill -9`ed 0.5 s into a grab: "partner gone (nothing for 15 s)", then
  released, no watchdog line afterwards. The base build in the same run: the zombie (enemy 2, hold 1)
  "still for 2850 steps ... puppet here (host: the partner runs it)" to the end.
* **Loot in a stall** (0/11, individual loot, the host sends nothing for host frames 640..1060, the
  guest takes the handgun bullets 4.7 s into it): "took item 12 (etc 13), individual loot", nothing at
  the host. The base build: "took item 12 (etc 13, flag 146)" and the host "got the partner's item 12".
  This host with a base guest: "the partner's pickup of item 12 ignored (individual loot)".
* **Waiting added enemies** (a guest build whose room counted as not running for 150 frames, extra
  enemies 3 in 0/6): "partner's enemy for slot 30 waits until our room runs", both placed 5 s later in
  slots 30 and 31 (the host's).
* **Randomizer** (0/6, seed 7): host and guest "record 1 -> Cerberus, record 2 -> Zombie: prisoner 2".
  A host build that refuses record 1: host "1 records the randomizer kept here sent to the partner",
  joining guest "record 1 stays 1 (as the host's)"; the base guest made it a Cerberus. The guest
  following the host through the door: the difference logged (the limit above).
* **Trade screen** (0/4, host Handgun equipped + Bullets x30, guest the new-game inventory): the guest
  requests the Bullets, the host offers the request, both ready: "Traded your Handgun Bullets for the
  Playing Manual". The guest's DONE lost (`RECVX_COOP_TS_LOSE=d1`) and the guest joining again: the host
  puts the Bullets back ("escrow home"), asks again, hears "made": "was taken after all: our copy is
  gone again", takes the Manual - one copy each. The host's SWAP lost (`s1`): Bullets back, the
  question answered "refused": they stay, nothing lost.
* After merging main d96f1fb (network, render/host/audio/build fixes): the grab, loot, randomizer, extra enemy and trade runs again,
  the same results; ctest (12) passes; the Windows build compiles. Single player: `layout_check.sh`
  main d96f1fb against this branch, vanilla 155 and enhanced 299 dumps identical.

Not verified: a real door or cutscene delaying an added enemy (a forced wait stood in), the kick path
(`RECVX_COOP_KICK`: the same `partner_gone`), a save between a disconnect and the escrow's answer
(the item is in the inventory at once), a full inventory and item box on escrow return (logged, stays
held).

## Risks

* Voices (protocol 18): the partner's voice is the program its game played, sent in the pose:
  two voices within one lost datagram's frames play only the last.
* Raw struct snapshots across instances assume identical room memory layout (deterministic
  `memp` allocation per room load, identical binaries) - verify early (M2).
