# Co-op through a server

The direct connection (docs/COOP.md, protocol 11) stays as it is; the server is optional.
Sections 1-8 are the approved plan; section 9 is what was built and where it deviates. How to
play: docs/COOP.md "Playing through a co-op server".

## Status (keep this up to date)

| Milestone | State |
|---|---|
| M1 server: HELLO/cookie, server-assigned unique room codes (grace period, idempotent CREATE, host-bound tokens), JOIN/INTRO, keepalive, close, password, relay, limits; ctest `rdv` | **done** |
| M2 rendezvous in the game: Connection / server / password / room code in the lobby, F3, settings table, ini, command line; join by room code | **done** |
| M3 hole punching with LAN detection (same public address: the LAN candidate), sequential-port candidates | **done** |
| M4 relay fallback after 2 s, punching continues 10 s and upgrades to direct; soft bandwidth caps and abuse limits | **done** |
| M5 end-to-end encryption (X25519 + XChaCha20-Poly1305, Argon2id password) in every server session, direct or relayed | **done** |
| M6 ops (systemd unit, Dockerfile), CLI client `recvx-client`, ctest `rdv_paths` (local paths) and `rdv_nat` (NAT types + netem in rootless namespaces), docs | **done**; the Dockerfile was not built (no docker/podman here) |
| M8 protocol 2 (Codex review, section 11): INTRO checked and epoch-bound keys, local encryption policy, authenticated RELAY frames, address tables and global budgets, code grace under churn, fail closed, server restart detection, DNS on a thread, wiping; tests `rdv_client`, `rdv_fuzz` | **done** |
| M7 config file (`/etc/recvx-server.conf`, `--config`, flags override, SIGHUP reload), several listen addresses, relay limits raised from measurements, normal/verbose log with a line budget, metrics (summary line, STATUS in Prometheus text, `recvx-client status`); lobby CODE/ADDRESS label | **done** (section 10) |
| M9 protocol 3 (section 12): a crashed joiner takes its place back at once (rejoin tag in JOIN), host KICK ("Remove stuck player"), `member_timeout` (10 s) apart from `room_timeout`, keepalives every 3 s | **done** |
| M10 network review (section 13, still protocol 3): CREATE budget per address, no server-wide CREATE refusal under churn, RELAY crash with a recycled address entry, re-registration after a stalled game, late punches ignored | **done** |

Verified: ctest (10 tests) on Linux; the Windows cross build compiles (`recvx.exe`, `test_rdv.exe`;
no Wine here to run them); two headless games through a local server in 0/6 with 30 enemies:
punched (LAN), forced relay, relay fallback, and today's `ip:port#CODE` unchanged; single player
frame dumps identical to main (105 frames).

Next steps (in this order): a real two-PC test across the internet with a server on a VPS; Windows
under Wine/real Windows against the Linux server; build and run the Dockerfile; IPv6 (sockets in
net.c and main.c dual-stack, IPv6 candidates in rendezvous.c `on_intro`; the wire format already
carries 16-byte addresses); a full-cone NAT case in `test_nat.sh` (cone/symmetric pairs then punch
with port prediction, not implemented: only +1/+2).

## 1. Architecture and connection flow

```
 host game ---- UDP ----\                       /---- UDP ---- joining game
                          rendezvous + relay server (one UDP port)
 host game <=========== direct UDP (after hole punching) ===========> joining game
```

Setting **Connection**: *Automatic* (default), *Direct (P2P)*, *Via server*.

* **Direct**: today's behaviour (`ip:port#CODE`). No server contact at all.
* **Via server**: always through the relay (predictable, adds latency, see 3).
* **Automatic**: room code through the server, then hole punching for a direct path; the relay
  only when punching fails.

### Automatic, step by step

1. **Host creates a room.** `HELLO` (client version, nonce) -> server answers `WELCOME` (server
   version, a server cookie for this address - see 4). Then `CREATE {password verifier?}` ->
   `ROOM {code "K7QM3X", host token}`. The host's game shows the code ("Copy code"). The room lives
   as long as the host keeps it alive (keepalive every 5 s; gone 30 s after the last one, or at
   `CLOSE`).
2. **Joiner joins.** `HELLO`/`WELCOME`, then `JOIN {code, password proof?}` -> the server checks the
   room, the password proof (4) and that the room has no member yet; it answers both sides with
   `INTRO {peer's public addr:port, peer's LAN addr:port as the peer reported it, session key
   material, member token}`.
3. **LAN detection.** Same public IPv4 on both sides: try the reported LAN address first (it works
   behind the same router, where hairpinning often does not).
4. **Hole punching.** Both send `PUNCH {member token, attempt}` to each other's candidates
   (LAN, public, public with port +1..+2 for sequential NATs) every 50 ms for up to 2 s. The first
   valid punch or game packet that arrives fixes the path; each side then sends normal protocol-11
   game packets there. Most cone NATs open within 1-3 round trips.
5. **Relay fallback.** No path after 2 s (configurable, 1-5 s) or 20 attempts: both switch to
   `RELAY` framing to the server (below). Punching continues in the background for another 10 s
   at a low rate; a direct path found later is taken over without a reconnect (the game protocol
   does not care where its datagrams travel; the relay only adds the frame).
6. **Keepalive / NAT bindings.** UDP mappings expire after 30-120 s of silence on typical NATs
   (some 20 s). In a game the stream is continuous (30 packets/s), so only idle phases matter: the
   lobby and menus keep sending poses already; towards the server a `KEEPALIVE` every 15 s while
   in a room. The relay path is kept warm by the game traffic itself.
7. **Close.** `CLOSE {token}` from either side (and PKT_BYE between the games as today); the
   server forgets the room and both addresses immediately.

### IPv4 / IPv6

The game is IPv4-only today (net.c). Plan: the server listens on both (dual-stack socket); the
clients keep IPv4 for the first version and gain IPv6 candidates later (punching between two IPv6
hosts is usually trivial: no NAT, only stateful firewalls that open on outgoing packets). The
`INTRO` carries address family + 16 bytes so the format does not change later.

## 2. Wire protocol (client <-> server)

All server messages share the protocol-11 frame start so one socket and one receive loop serve
both, and so the server can tell its traffic apart from game packets:

```
u32 magic  "CVXS" (server)  /  "COOP" (game packet, protocol 11, unchanged)
u16 version (server protocol 1)
u8  type
u8  flags
... type-specific body
```

| type | direction | body |
|---|---|---|
| HELLO 1 | c->s | client build, nonce |
| WELCOME 2 | s->c | server version, cookie (HMAC of address + time, 16 B), limits |
| CREATE 3 | c->s | cookie, has_password, password salt + verifier (4) |
| ROOM 4 | s->c | room code (6 chars), host token (16 B), salt |
| JOIN 5 | c->s | cookie, room code, password proof (4) |
| INTRO 6 | s->c | peer public addr, peer LAN addr, member tokens, room id |
| PUNCH 7 | c->c | member token (proves the peer came through the server), attempt |
| RELAY 8 | c->s->c | 8-byte header + the game datagram unchanged |
| KEEPALIVE 9 | c->s | token |
| CLOSE 10 | c->s | token |
| ERROR 11 | s->c | code (no such room, wrong password, room full, rate limited, version) + text |

**Relay framing.** `RELAY` = `"CVXS" ver type=8 flags` + 4-byte room id + the game datagram as it
is (protocol-11 pose header first). 12 bytes per datagram, about 1% of a full 1164-byte datagram;
the 1200-byte packetization of protocol 11 leaves room for it (1176 bytes is the most a game
datagram may use when relayed; the client lowers `COOP_DGRAM_MAX` by 24 when it relays). The
server checks only that the sender's address is one of the room's two members and forwards to the
other; it never parses the game part. `PKT_BYE` travels as any game datagram; the server also
learns a member left from `CLOSE` or the keepalive timeout.

**Versioning.** Server protocol version in every frame; the server answers an unknown version with
`ERROR version` carrying the versions it supports. The game protocol version (11) stays end to end:
the two games still check each other's.

## 3. Latency

* **Direct after punching**: identical to today's direct connection (the server is out of the path).
* **Relay**: the path becomes host -> server -> joiner: added latency = (d(host, server) +
  d(server, joiner)) - d(host, joiner), typically +5..+30 ms one way inside Europe with a server
  in a central location (Frankfurt/Amsterdam), more across continents. Server processing is
  microseconds (one hash lookup and a sendto per datagram).
* **Relay overhead**: 12 bytes per datagram; at 30 frames/s and 1-5 datagrams per frame,
  0.4-1.8 KB/s extra per direction. Bandwidth per room through the relay: ~5 KB/s per direction
  in busy rooms (protocol 11 measurements), ~10 KB/s both ways - 80 kbit/s.
* **Server location**: where most players are; for a German/European group a VPS in Frankfurt or
  Nuremberg. One server per region if players are spread.
* **Hole punching success** (published measurements, e.g. Ford/Srisuresh/Kegel and later WebRTC
  statistics): full-cone / address-restricted / port-restricted cone NATs on both sides: >90%;
  one side symmetric, the other cone: ~50-60% with port prediction; both symmetric or CGNAT with
  random ports: rarely - relay. Expect ~80-90% of real home pairs direct, the rest relayed.
* **MTU**: game datagrams <= 1200 bytes direct, <= 1176 + 12 relayed; server messages < 200 bytes.
  No fragmentation on any path with an MTU >= 1280 (IPv6 minimum; PPPoE 1492; most VPN overlays
  1280-1420).

## 4. Security and abuse

* **Room codes**: 6 characters from 32 symbols (2-9 and A-Z without I and O, as the protocol-11
  join codes) = 30 bits, ~1 billion. With per-IP limits of 5 failed joins per minute and a
  global limit, guessing an active room needs millions of attempts: impractical. Codes expire with
  the room and are never reused within a day.
* **Password**: never sent or stored in clear. The host sends `salt` + `verifier = H(salt,
  password)` (H = a slow KDF, e.g. Argon2id/scrypt from libsodium or PBKDF2-SHA256 100k on the
  client); the joiner gets the salt (in `ERROR need password` or from the room lookup) and proves
  knowledge with `HMAC(H(salt, password), server challenge)`. The server keeps the verifier only in
  memory for the room's lifetime. Combined with the join code of protocol 11: the code that the
  games prove to each other is derived from the room secret (`code = KDF(room code, password,
  "join")`), so even the relay operator cannot inject packets into the game without the password,
  and a direct connection after punching keeps the same protection as today.
* **Amplification / DDoS**: no reply is bigger than its request (`WELCOME` <= `HELLO` size,
  padded requests); everything after `HELLO` needs the cookie bound to the source address (a
  spoofed source never gets the cookie), so the server cannot be used to reflect at a third party.
  The relay forwards only between the two members of a room that both completed the cookie
  exchange and the join, only from the address that joined, and caps each member at e.g. 50 KB/s
  and 100 datagrams/s; rooms per IP (e.g. 4), joins per IP per minute, total relay bandwidth.
* **The server sees traffic**: relayed game packets are readable by the operator (positions,
  inventories, names). Optional end-to-end encryption: a session key from the room secret via the
  same KDF and an AEAD per datagram (XChaCha20-Poly1305 via libsodium or Monocypher: ~1-2 us per
  datagram, +24 bytes nonce/tag, compatible with the 1176-byte budget). Recommendation: do it from
  the start for relayed and direct traffic alike when a password is set - it also replaces the
  join-code proof with real authentication. Cost: one small vendored library (Monocypher is a
  single C file, public domain/CC0) and ~150 lines.
* **Logging and privacy**: by default only counters (rooms, relayed bytes, errors) and no IP
  addresses; an optional debug mode logs anonymised addresses (truncated /24). Nothing on disk.

## 5. Server operations

* **Build**: a CMake target `recvx-server` (C11, `pc/server/server.c` + `pc/src/net/net.c` with
  IPv6 added, + the KDF/AEAD library), no SDL. A static Linux binary (musl or `-static` glibc),
  ~200 KB.
* **Config** (flags and environment): `--port 7141` (`RECVX_SERVER_PORT`), `--bind`, limits
  (`--max-rooms`, `--relay-kbps`, `--joins-per-min`), `--allow CIDR` (optional allowlist for a
  private server), `--stats-interval`.
* **systemd unit**:

  ```ini
  [Service]
  ExecStart=/usr/local/bin/recvx-server --port 7141
  DynamicUser=yes
  ProtectSystem=strict
  ProtectHome=yes
  PrivateTmp=yes
  PrivateDevices=yes
  NoNewPrivileges=yes
  CapabilityBoundingSet=
  RestrictAddressFamilies=AF_INET AF_INET6
  RestrictNamespaces=yes
  SystemCallFilter=@system-service
  MemoryMax=64M
  Restart=on-failure
  ```

* **Docker**: `FROM scratch` + the static binary, `EXPOSE 7141/udp`, `USER 65534`.
* **Resources per room**: ~300 bytes state; relay ~10 KB/s while relaying. A 1-vCPU / 1 GB VPS
  with 1 TB traffic per month: CPU for >10,000 relayed rooms, the limit is traffic: 1 TB / (10
  KB/s x 2) ~ 15,000 relayed room-hours per month. Rendezvous-only rooms cost nothing measurable.
* **Monitoring**: a `STATUS` request from localhost (rooms, relayed rooms, bytes, errors) and a
  periodic stats line to the journal; a health check = `HELLO` round trip.

## 6. Client integration

* **Lobby / F3**: Connection (Automatic / Direct / Via server); Server address (default the
  project's server, `host` or `host:port`, port 7141 implied); host: **Create room** shows the
  code big ("Room K7QM3X") with **Copy code**, optional password field; joiner: **Room code** and
  **Password** fields instead of the address (Direct keeps the address field). The status line
  says which path is in use ("direct", "direct (LAN)", "via relay, +18 ms").
* **ini keys**: `coop_conn` (0 auto, 1 direct, 2 server), `coop_server`, `coop_room_pw` (not
  saved by default: password fields are forgotten at exit unless "remember" is ticked).
* **Code**: a new `pc/src/net/rendezvous.c` (state machine: hello, create/join, punch, relay
  switch), driven from `coop_frame` at the frame hand-off like `receive()`. `net_send` gets a
  "via relay" variant that prepends the frame; `receive()` strips it. The game protocol, the
  checks, the join-code proof and everything in coop_glue.c stay as they are; the peer address is
  simply the punched address or the relay.
* **P2P untouched**: with Direct (or with a full address in the join field) none of this runs.
* **Errors**: "No such room (check the code)", "Wrong password", "The room is full", "Server not
  reachable (UDP 7141 blocked?) - try Direct", "Your partner could not be reached directly: using
  the relay", "Server version too old/new".

## 7. Test plan

* Headless: a local server and two clients on 127.0.0.1 (server on 7141, clients as today's
  `run2.sh`), create/join/punch/relay forced by flags (`RECVX_COOP_FORCE_RELAY`).
* NAT types in network namespaces: `ip netns` for host, joiner and "internet"; nftables masquerade
  per namespace for cone NAT, `masquerade random,fully-random` for symmetric NAT, a double NAT for
  CGNAT; check direct for cone/cone, relay for symmetric/symmetric.
* Loss and latency: `tc qdisc ... netem delay 40ms 10ms loss 2%` on the namespace links; check the
  event stream (protocol 11 resends), ping display, relay switch-over.
* Abuse: a spoofed-source flood (scapy) against the server - no replies to spoofed sources; join
  guessing hits the rate limit; a third address sending `RELAY` with a valid room id is dropped.
* Windows client under Wine against the Linux server; one real two-PC test across the internet.

## 8. Milestones, effort, open questions

| Milestone | Effort |
|---|---|
| Server: HELLO/cookie, rooms, codes, JOIN, INTRO, keepalive, limits; systemd/Docker | 2-3 days |
| Client rendezvous state machine, lobby/F3 fields, Automatic/Via server | 2-3 days |
| Hole punching incl. LAN candidates, relay fallback and later upgrade | 2 days |
| Password (KDF + challenge), join code derived from the room secret | 1 day |
| Optional end-to-end encryption (AEAD) | 1-2 days |
| netns/nftables NAT test rig, netem tests, docs | 2 days |

Open questions and recommendations:

1. **Who runs the server?** A default server address in the build means someone pays for and
   operates it. Alternative: ship the server and let groups run their own; the field then has no
   default. Recommendation: ship both - an official default if you are willing to run it, and the
   self-hosting docs.
2. **6-character codes** are fine with rate limits; I recommend 6 from a 32-symbol alphabet (2-9,
   A-Z without I and O) - the same alphabet as the protocol-11 join codes, so players see one style.
3. **Password vs. join code**: I recommend making encryption + authentication the default in
   server mode (derived from the room code even without a password) rather than an option -
   relayed traffic otherwise exposes inventories and names to the operator, and the cost is small.
   The password then only keeps out people who learned the room code.
4. **Automatic default**: agreed; but the first attempt should try Direct if the joiner typed an
   address (`ip:port#CODE`), so existing habits keep working.
5. **IPv6**: worth adding to the game's net layer at the same time - it makes direct connections
   work for many CGNAT users (who usually have native IPv6) without the relay.
6. **Relay limits**: per-room bandwidth cap of ~50 KB/s each way is generous for the measured
   ~5 KB/s; confirm you want a hard cap (protects the server bill) rather than a soft one.
7. TCP fallback for networks that block UDP entirely (some corporate/hotel networks) is out of
   scope by your choice (plain UDP); worth stating in the docs as a known limit.

## 9. As implemented

### Code layout

* `pc/src/net/rdv_proto.{c,h}`: the CVXS messages (hand-packed little-endian, exact sizes per
  type), room codes, OS randomness, and the crypto (Monocypher 4.0.2, BSD-2/CC0, fetched by CMake:
  BLAKE2b, Argon2id, X25519, XChaCha20-Poly1305). Shared by the server, the game and the tests.
* `pc/server/server.c`: the server core (no sockets, no clock: fed by `main.c` and the tests).
  `pc/server/main.c`: the UDP loop and flags. `pc/server/CMakeLists.txt`: target `recvx-server`
  (Linux, static), buildable on its own (`cmake -S pc/server -B build-server`) or as part of the
  game's build.
* `pc/tests/test_rdv.c`: ctest `rdv`.

### Room codes and rooms (guaranteed by the server)

* **Assigned by the server, never by a client.** CREATE carries no code; the server draws 8
  characters from the 32-symbol alphabet (2-9, A-Z without I and O; 40 bits, shown `XXXX-XXXX`)
  with the OS's randomness and returns them in ROOM.
* **Unique among open rooms**, and not handed out again for `--grace` seconds (default 120) after
  its room closed or expired: a colliding draw is retried (up to 64 draws, counted as
  `code retries` in the stats), so a late joiner never lands in a newer room with an old code.
  Codes are freed only when the room closes (host CLOSE) or expires (no host keepalive for
  `--timeout`, default 30 s), plus the grace period.
* **Joins name a code that exists**: an unknown, closed or expired code gets `ERROR no room`
  ("Room not found") and counts as a failed join for the address.
* **One host session per room**: the room is bound to the address that created it and a random
  16-byte host token returned only to that address in ROOM. KEEPALIVE and CLOSE must come from
  that address with that token (the joiner likewise with its own member token from INTRO);
  anything else is dropped and counted (`foreign`). Another client cannot claim, overwrite or
  close a room; CREATE from another address always makes a new room with a new code.
* **CREATE is idempotent**: it carries a 64-bit request id chosen by the host per hosting session.
  A retry (same address, same request id) gets the same room, code and token back; a CREATE with a
  new request id from the same address (the game hosts again) closes the old room first.
* **Simultaneous creates** get different codes (uniqueness is checked against every open room
  before ROOM is sent; the server is single-threaded, so there is no race).
* **Guessing**: 2^40 codes; 5 failed joins per address per minute (`--joins-per-min`), 20
  requests per address per second; with 1000 open rooms one guess hits with p ~ 1e-9, an address
  gets 7200 guesses a day. A password (Argon2id-verified, below) keeps out even someone who has
  the code.

Tests (ctest `rdv`): 200 creates in the same millisecond all differ; a forced collision (test
hook `srv_code_rng` gives the same code three times: the second room gets another code after 2
retries); an unknown code; hijack attempts (KEEPALIVE/CLOSE with the host's token from another
address, the joiner's token from the host's address); a duplicate CREATE (same room, code, token)
and a new request id (new room, old code gone); code reuse only after expiry plus grace; NEED_PW /
wrong / right password; relay only between the two members, soft cap; per-address limits; a fuzz
loop of 300000 random and mutated datagrams through the parser and the server, with the check that
no reply is larger than its request (or than the 64-byte HELLO minimum).

### Messages as built

Header `"CVXS"` u32, version u16 (3 since section 12, 2 since section 11; 1 before), type u8, flags u8. Sizes are exact;
HELLO is padded to 64. A version-1 client gets `ERROR version` ("update the game").

| type | body |
|---|---|
| HELLO 1 | nonce u32, padding to 64 bytes |
| WELCOME 2 | nonce, cookie 16 (BLAKE2b keyed with a per-run secret over address + minute; valid 1-2 min), your public address |
| CREATE 3 | cookie, request id u64, flags (password, e2e), salt 16, verifier 32, X25519 public key 32, LAN address |
| ROOM 4 | request id, room id u32 (slot + generation), code 8, host token 16 |
| JOIN 5 | cookie, code 8, flags (proof given), proof 32, public key 32, LAN address, rejoin tag 16 (protocol 3; zero: none) |
| INTRO 6 | room id, member sequence u32, role, flags, your token 16, peer public address, peer LAN address, peer public key |
| PUNCH 7 | room id, attempt, MAC 16 (game to game; header flag 1 = "I hear you") |
| RELAY 8 | room id, counter u32, MAC 8 (BLAKE2b keyed with the sender's member token over header, room id, counter and payload) + the (encrypted) game datagram: 24 bytes of header |
| KEEPALIVE 9 | request: room id, token, member sequence the host knows; reply: room id, partner present |
| CLOSE 10 | room id, token |
| ERROR 11 | code, server version, salt (with "password needed" / "wrong password") |
| STATUS 12 | from 127.0.0.1 only: the counters as text |
| KICK 13 | room id, host token, member sequence (protocol 3): the host removes its partner; the answer as a KEEPALIVE's |

Addresses are family u8 + port u16 + 16 bytes, IPv4 in the first 4 (IPv6 deferred: the format
carries it, the sockets are IPv4).

Password: the host draws a salt, computes `K = Argon2id(password, salt)` (4 MB, 3 passes) and sends
`verifier = BLAKE2b(K, "recvx verify")`. A joiner without a proof gets `ERROR need password` with
the salt, then proves `BLAKE2b-keyed(verifier, cookie | code | its public key)`. The server keeps the
verifier in memory for the room's life only.

Limits: section 10 "Configuration" (config file keys and flags, defaults and why). Relay limits
are soft: datagrams above them are dropped and the room is logged at most every 10 s; the room
stays.

### Client (game) integration

* `pc/src/net/rendezvous.{c,h}`: the client state machine on net.c's one socket (HELLO, CREATE /
  JOIN with retries every 500 ms for 10 s, keepalives, punching, relay switch, encryption). Driven
  from `coop_frame` (`rdv_frame` after `receive()`); also built into `recvx-client`.
* `pc/glue/coop_glue.c`: the partner of a server session is the fixed pseudo address `rdv_peer`
  (0.0.0.0:65535) whatever the path, so the protocol-11 logic is untouched. Three hooks: `receive()`
  passes every datagram to `rdv_input` first (server messages and punches are taken there; relayed
  or encrypted game datagrams come back decrypted, `from = rdv_peer`); every send goes through
  `xsend` (rdv_send for `rdv_peer`, else net_send); the datagram budget is 1200 - 52 for a server
  partner (24 relay header + 28 encryption, so the wire stays <= 1200; protocol 1: 1200 - 40). The join-code proof uses a
  code both games derive from the session keys (`code_for`), the host's own code for direct joiners.
* Hosting with Automatic or Via server and a server address: the port opens as before (direct
  joins keep working) and a room is created besides; no server, or Direct: exactly today's code
  path. Joining: an 8-character room code (no `.`, `:` or `#`) goes through the server; anything
  else is today's `ip:port#CODE` join (Via server refuses an address with a message).
* Settings: `coop_conn` (settings table "Connection" - F1 and OPTION > PC SETTINGS > CO-OP - lobby
  row CONNECTION, F3 combo), `coop_server` (string: lobby row SERVER, F3 "Co-op server", ini; the
  settings table holds only numbers), the password (lobby row PASSWORD shown as stars, F3 masked;
  never saved), the room code (lobby info line + COPY ROOM CODE row, F3 "Room code" + "Copy room
  code"). `COOP_DEFAULT_SERVER` (coop.h) is the one constant for a built-in default address (empty:
  self-hosted only); `COOP_E2E` (coop.h) the encryption default. Command line: `--coop-server`,
  `--coop-conn auto|direct|server`, `--coop-pw`, `--coop-join ROOMCODE`.
* Status line: "hosting on UDP port 7140, waiting for a partner - server: room XXXX-XXXX open",
  "joining through the server: looking for room ...", "connected to the partner (connected
  directly (LAN), encrypted), ping ...". `RECVX_COOP_STATS=1` adds the path and the bytes per
  datagram the framing adds on the wire.

### Hole punching as built

After INTRO each side sends PUNCH every 50 ms to the candidates: the peer's LAN address when both
have the same public address (`direct (LAN)`), its public address, and public port +1/+2 when the
public addresses differ. PUNCH carries a BLAKE2b MAC with a key from the session (the server cannot
forge one) and a flag "I hear you". A side that receives a punch without the flag answers it once
with the flag; a punch with the flag (or an authenticated encrypted game datagram arriving directly)
confirms that address as the path, and the confirming side sends one flagged punch back, so the
other side confirms too (no ping-pong: flagged punches are never answered). No confirmation within
2 s (`RECVX_COOP_PUNCH_MS`): relay; punching goes on every 500 ms for 10 s more and a confirmation
switches to direct. "Via server" never punches and ignores punches.

### End-to-end encryption as built (deviation: always on in server sessions)

Each game makes a fresh X25519 key pair per session; the public keys travel in CREATE / JOIN and
INTRO. Both derive `BLAKE2b-512(keyed with the password key if any, "recvx e2e v2" | X25519 shared
secret | host pk | joiner pk | room id | epoch | flags | room code)` (protocol 2, section 11; v1
had no room id, epoch, flags or code) -> two directional XChaCha20-Poly1305 keys, the punch MAC key
and the protocol-11 join code. Encrypted datagram: `"CVXE"` | 64-bit counter (the nonce) |
ciphertext | 16-byte MAC (+28 bytes); a 64-entry replay window drops repeats. Without a password a
passive observer (including a logging server) learns nothing; an *active* malicious server could
swap the public keys (man in the middle). With a password it cannot, because the keys also depend on
the Argon2id password key, which the server never has (it holds only the verifier, from which it
could try to guess weak passwords offline).

Correction (Codex review, finding 1): in protocol 1 that password claim did not hold. Both games
took the INTRO's E2E flag as it came, so a malicious server or an on-path attacker could clear it
and both sides then exchanged plaintext, password or not. Protocol 2 decides encryption locally:
the host encrypts by its own setting and always with a password; a joiner refuses a plaintext host
unless it allows plaintext itself, and never when it has a password. Since then the claim holds:
with a password, neither a malicious server nor anyone on the path can read, alter or downgrade the
session. Direct P2P (`ip:port#CODE`) stays unencrypted, as before.

### Running a server

```sh
# build (any Linux with a C compiler and CMake; static binary, ~1.1 MB)
cmake -S pc/server -B build-server -DCMAKE_BUILD_TYPE=Release
cmake --build build-server --target recvx-server recvx-client
./build-server/recvx-server                         # as any user, the defaults; Ctrl+C stops it
./build-server/recvx-server --config my.conf -v     # a config file, verbose log
./build-server/recvx-server --port 7200 --relay-kb-per-sec 1024   # flags (they override the file)
./build-server/recvx-client 127.0.0.1 status        # the metrics, on the server machine
# systemd (DynamicUser, read-only system, no capabilities; systemd-analyze security: 1.2 OK)
sudo install -m 755 build-server/recvx-server build-server/recvx-client /usr/local/bin/
sudo install -m 644 pc/server/recvx-server.conf.example /etc/recvx-server.conf   # then edit it
sudo cp pc/server/recvx-server.service /etc/systemd/system/
sudo systemctl enable --now recvx-server
sudo systemctl reload recvx-server                   # after editing the limits / log level
journalctl -u recvx-server                           # the log and the metrics lines
# or a container (FROM scratch + the static musl binary, user 65534, the example as its config)
docker build -f pc/server/Dockerfile -t recvx-server .
docker run -d --name recvx-server -p 7141:7141/udp --read-only --cap-drop ALL \
    -v "$PWD/recvx-server.conf:/etc/recvx-server.conf:ro" recvx-server
docker kill -s HUP recvx-server                      # reload
```

Open UDP 7141 in the firewall. No disk state; the log goes to stdout (the journal, `docker
logs`). A multi-homed server should list its public addresses in `listen` (replies must leave
from the address the request went to; `test_nat.sh` needs it for that reason). Test it from a
client machine without the game:
`recvx-client SERVER host` prints a room code; on another machine `recvx-client SERVER join CODE`
(options `--relay`, `--pw`, `--loss PCT`, `--no-punch`) prints the path and round trip.

### Tests and measurements

* ctest `rdv` (0.1 s): see "Room codes" above, plus message round trips, every type one byte short
  and long, bad magic/version/type/code characters/address family, the encryption (both sides get
  the same keys, tampered ciphertext or counter, replay, out-of-order within the window, other
  direction, without the password other keys).
* ctest `rdv_paths` (~50 s, Linux): a local server and two `recvx-client`s: Automatic (punched,
  LAN), Via server, punches dropped (fallback after 2 s), joiner Via server with host Automatic,
  password right / wrong / missing, 10% loss both ways (`--loss`, no tc needed), unencrypted host,
  unknown room.
* ctest `rdv_nat` (~45 s, Linux, skipped without unprivileged user namespaces): rootless network
  namespaces (`unshare -rn`, `nsenter`), nftables masquerade (cone) / `masquerade random,fully-random`
  (symmetric) on two routers with a WAN input firewall, the server behind its own link. Results:
  cone/cone direct; (port-restricted) cone/symmetric relay; symmetric/symmetric relay; same router
  `direct (LAN)`; with netem 20 ms + 2% loss on each router link and 15 ms to the server: Automatic
  direct, RTT 81 ms; Via server, RTT 141 ms (+60 ms = 4 x the server link, as expected).
* Two headless games + server on 127.0.0.1 (host new game, teleported to 0/6 with 30 enemies; the
  partner joins from the title by room code), `RECVX_COOP_STATS=1`:

| path | host datagrams per frame | avg / max game bytes | game bytes per frame | wire bytes added | host bytes per frame on the wire | ping (frame-quantized) |
|---|---|---|---|---|---|---|
| today's `ip:port#CODE` (direct partner) | 3.0 | 1124 / 1164 | 3372 | 0 | 3372 | 31-32 ms |
| Automatic: punched, `direct (LAN)` | 4.0 | 869 / 1104 | 3476 | +28 per datagram | 3588 (+6%) | 31-34 ms |
| Via server (relay) | 4.0 | 869 / 1104 | 3476 | +40 per datagram | 3636 (+8%) | 17-37 ms |
| relay fallback (`RECVX_COOP_NO_PUNCH=1`) | 4.0 | 869 / 1104 | 3476 | +40 per datagram | 3636 (+8%) | 22-33 ms |

  The 40-byte reserve (only for a server partner) costs one more datagram per frame in this room
  (3 -> 4: +104 bytes of pose). On localhost the relay adds no measurable latency (the ping is quantized to frames); on
  the netem rig it adds exactly the detour. Server CPU: one hash-free array lookup and a sendto per
  relayed datagram.
* Found and fixed on the way: the partner time-out compared `now` (frame start) with a later
  `rx_at` from `receive()` unsigned, so a clock tick in between read as 49 days and logged "partner
  timed out" spuriously (visible with the slightly longer receive path); now signed.

### Deviations from the plan

* Room codes have 8 characters (40 bits), as the protocol-11 join codes, not 6: guessing stays
  impractical even with many open rooms (section "Room codes").
* Encryption is always on in server sessions (with or without a password), and keyed by an X25519
  exchange rather than by the room code (which the server knows). The protocol-11 join code is
  derived from the session keys.
* A room has one host session bound to its address + token; a CREATE retry is recognized by its
  request id (coordinator requirement), codes have a grace period after closing.
* The relay cap is soft (decision 5): over-limit datagrams are dropped and logged, the room stays.
* Hosting with a server also keeps the direct port open (Automatic and Via server); "Via server"
  forces the relay for the server session only.
* The password field is never saved (no "remember" option).
* PUNCH carries a MAC, not the member token (the peer cannot check the other's token).
* KEEPALIVE: the host sends one every 1 s while waiting for a partner (its INTRO comes back in the
  reply if it was lost), every 10 s in a session; the joiner every 10 s.

### Known limits

* No TCP fallback: networks that block outgoing UDP cannot use co-op at all.
* IPv4 only (IPv6 deferred; the wire format carries it).
* Cone/symmetric pairs end on the relay (no port prediction beyond +1/+2).
* Argon2id (4 MB, 3 passes) runs once on the game thread when a password is used: a ~20 ms hitch in
  the lobby.
* A game that quits without leaving (crash, kill) leaves its room open until the 30 s time-out.
* Punches carry a MAC but no counter: an on-path attacker can replay one from another address while
  either side still punches (2 s, then 10 s more on the relay) and move a not yet direct session's
  path there - a denial of service, not a data leak (game data is authenticated and encrypted).
  After that window punches are ignored (section 13); before it, an encrypted datagram from the
  real partner (counter-protected) confirms the real path.
* Section 11 lists what protocol 2 still leaves open (on-path attackers, a server that withholds
  or invents introductions, room-wide password lockout).

## 10. Configuration, logging, metrics

### Configuration

`recvx-server` reads `/etc/recvx-server.conf` when it exists, or the file `--config PATH` names
(which must exist). Order: defaults < config file < `RECVX_SERVER_PORT` < command-line flags.
Format: one `key = value` per line, `#` or `;` starts a comment (also after a value), blank lines
and `[section]` headers are ignored, keys are case-insensitive and `-` = `_`, a UTF-8 BOM and CRLF
line ends are fine, lines up to 511 bytes. An unknown key is a warning (the server starts); an
invalid value (not a number, out of range, not an IPv4 address) is an error with file and line,
and the server refuses to start (exit 2). Every key is also a flag: `--relay-kb-per-sec 1024` or
`--relay-kb-per-sec=1024`; an unknown flag is an error. `recvx-server --help` lists them all.
`pc/server/recvx-server.conf.example` is the commented file with every key at its default (ctest
`rdv` checks that it parses without warnings and equals the defaults).

| key (flag `--key-name`) | default | range | what, and why this default |
|---|---|---|---|
| `port` | 7141 | 1-65535 | UDP port (`RECVX_SERVER_PORT` overrides the file). The games' default. |
| `listen` (old flag `--bind`) | 0.0.0.0 | up to 8 IPv4 addresses, comma-separated | Local addresses, one socket each; replies leave from the address a game talked to (multi-homed servers behind NAT need it). 0.0.0.0 = all. IPv6: refused with a clear error (not supported yet). |
| `max_rooms` | 1024 | 1-65535 | Open rooms at once (~300 bytes each, allocated at start). |
| `rooms_per_ip` | 4 | 1-65535 | Rooms one address hosts at once: a few players behind one router. |
| `room_timeout` (`--timeout`) | 30 | 5-86400 s | No keepalive from the host (games send one every 1-3 s): the room closes (and its code goes into the grace period). |
| `member_timeout` | 10 | 6-86400 s | No keepalive (every 3 s) or relayed traffic from the joiner: its place is free (section 12). The same player joining again takes it at once, the host can remove it (KICK); this is for everyone else. |
| `code_grace` (`--grace`) | 120 | 0-86400 s | A closed room's code is not handed out again (late joiners). |
| `requests_per_sec` | 20 | 1-1000000 | Requests (not relay) per address and second, bursts of 2 s. A game needs ~1 a second. |
| `failed_joins_per_min` (`--joins-per-min`) | 5 | 1-1000000 | Failed joins (unknown code, wrong password) per address and minute: code / password guessing. |
| `rooms_per_ip_per_min` | 6 | 1-1000000 | New rooms (CREATE, not its retries) per address and minute; over it "Too many attempts". A game opens one per lobby. Keeps one address from churning the code grace table (section 13). |
| `relay_kb_per_sec` (`--relay-kbps`) | **512** (was 64) | 1-10000000 | Relay bandwidth per player, KB/s (1024 bytes) of UDP payload, a bucket holding 1 s. |
| `relay_datagrams_per_sec` (`--relay-pps`) | **600** (was 200) | 1-10000000 | Relayed datagrams per player and second, a bucket holding 1 s. |
| `relay_address_kb_per_sec` | 1024 | 1-10000000 | Per IP address, all its players and rooms. This budget outlives rooms and memberships: leaving and joining again, or a new room, does not refill it (review finding 5). Two busy players behind one router, or ~6 hosts. |
| `relay_address_datagrams_per_sec` | 1200 | 1-10000000 | The same in datagrams. |
| `relay_total_kb_per_sec` (`--total-kbps`) | **25600** (was 0) | 0-100000000 | All relayed traffic of the server: the deliberate production cap (25 MB/s, ~210 Mbit/s each way, ~150 busy relayed rooms). 0 = no limit. Size it to the line and the bill. |
| `relay_total_datagrams_per_sec` | 50000 | 0-100000000 | All relayed datagrams per second (~280 busy hosts). 0 = no limit. |
| `log_level` | normal | normal, verbose | See "Logging". |
| `verbose` (`-v`, `--verbose`, old `--debug`) | no | yes/no | The same as `log_level = verbose`. |
| `stats_interval` | 300 | 0-86400 s | The metrics summary line in the log; 0 = off. |

Every value is parsed with full checking (`strtol` to the end of the value, the range above, no
`atoi`): garbage, a sign where none is allowed, overflow or an empty value is an error, also on
the command line and in `RECVX_SERVER_PORT` (review finding 12). 0 means "no limit" only where
the table says so (`relay_total_*`; `stats_interval` 0 = off, `code_grace` 0 = none); every other
limit is at least 1, and no time-out can be 0 or negative.

**Why the relay defaults were raised** (64 KB/s and 200 datagrams/s per player dropped game
traffic of busy rooms). Measured with `RECVX_COOP_STATS=1` (which now also logs the busiest frame
of each 300), two headless games through a local server, Via server (relay + encryption, +40
bytes per datagram), the host in 0/6 and 0/12 with extra enemies 3, rearranged placements and the
randomizer (46 / 53 enemy slots, the 48-snapshot window full), a room load (teleport), a rejoin
(join blob, resync):

| | datagrams per frame | bytes per frame on the wire | at 30 frames/s |
|---|---|---|---|
| host, vanilla 0/6 (30 slots) | 4 | 3636 | 107 KB/s, 120 datagrams/s |
| host, 46-53 slots, steady | 5 | 5380-5580 | 158-163 KB/s, 150 datagrams/s |
| host, busiest single frame seen (room load, join) | 6 | 6932 | 203 KB/s, 180 datagrams/s if it lasted |
| protocol ceiling (16 events + 48 snapshots per frame) | 7 | ~7280 | 213 KB/s, 210 datagrams/s |
| joiner (pose only) | 1 | 204 | 6 KB/s, 30 datagrams/s |

The defaults, 512 KB/s and 600 datagrams/s per player, are 2.5x the busiest frame sustained for a
whole second, 3.1x / 4x the steady worst case, and still 2.4x / 2.9x the protocol's ceiling;
each bucket holds a second's worth, so a burst (a join blob or a room load) never meets the cap.
In the measurement above the server dropped nothing (drops 0 in the metrics, 12.6 MB relayed in
two minutes; again 0 of 8048 datagrams in a second run on the final code); with
`--relay-kb-per-sec 50` the same room dropped 1634 of the host's datagrams in a minute and logged
`limit: room XXXX-xxxx: the host over relay_kb_per_sec 50, dropping` once per 10 s. ctest
`rdv_paths` now also checks sustained delivery, not just a first answer: a game-sized stream (5
datagrams of 1164 bytes a frame each way plus the answers, ~350 KB/s and 300 datagrams/s per
player) through the relay at the default limits must deliver >= 97% in both directions (it
delivers 100%: 450 of 450, 900 of 900) with no relay drops in the server's metrics, and both
ends must exit 0.

Abuse: one player gets at most its 512 KB/s; leaving and joining again does not refill anything
beyond its address's 1024 KB/s (the per-address bucket lives in the address table, not in the
membership; ctest `rdv`: 20 CLOSE/JOIN cycles in one second pass exactly the address's 150 of
2000 datagrams); the server as a whole stops at `relay_total_*`. (Until review finding 3 is
fixed, a spoofed hash collision in the 4096-entry address table can still reset an address's
entry, this budget included.)

Traffic budget (corrects section 5's estimate, which assumed ~10 KB/s): a relayed busy room costs
~170 KB/s of egress (the host's stream to the joiner plus the joiner's), a vanilla room ~115 KB/s:
1 TB of egress per month is ~1,600-2,400 relayed room-hours. Punched (direct) rooms cost nothing.

**Reload**: SIGHUP (`systemctl reload recvx-server`, `docker kill -s HUP`) reads the file and the
flags again and applies all limits, `log_level` and `stats_interval` at once (the buckets and
counters are kept; it is a copy of a few integers between packets, single-threaded). `port`,
`listen` and `max_rooms` keep their values until a restart (sockets and the room table; the log
says so). A file with an error on reload is refused and the running settings stay.

**systemd**: the unit runs `recvx-server` without flags, so `/etc/recvx-server.conf` (when present)
is the configuration; `ProtectSystem=strict` keeps `/etc` read-only and `ReadOnlyPaths=-/etc/
recvx-server.conf` states it; `DynamicUser` and the rest of the hardening stay (`systemd-analyze
security`: 1.2 OK); `ExecReload` sends SIGHUP. **Docker**: the image carries the example as
`/etc/recvx-server.conf` and starts with `--config` on it; mount your own read-only over it.

### Logging

Two levels, to stdout (the journal / `docker logs`; no files):

* **normal** (default): the start line (every setting in effect, the config file used), errors,
  rooms opened (`room K7QM-xxxx opened (password)`) and closed (`... closed (closed by the host)
  after 912 s, 41230 KB relayed`, or `(the host timed out)`), and limit hits: request rate,
  failed joins, rooms per address, all rooms in use, the relay caps per room and the total. Each
  limit logs at most once per 10 s per address / room / server; reload results.
* **verbose** (`log_level = verbose`, `verbose = yes`, `-v`): also every request (HELLO, CREATE
  retries, JOIN results: unknown room, password needed, wrong password, full; stale cookies,
  wrong tokens on KEEPALIVE / CLOSE, invalid datagrams, STATUS), per-room events (a partner joined
  or left, an INTRO sent again) and the relay state per room (`relaying (from the host)`, `the
  relay idle for 5 s (a direct path now, or a pause)`, `relaying (... again)`). KEEPALIVEs that
  change nothing are not logged.

Non-blocking (review finding 10): the sockets are `O_NONBLOCK` and replies are sent with
`MSG_DONTWAIT` (a full send buffer drops that datagram and counts it, `recvx_send_failures_total`);
log lines never touch stdout during packet processing: they go into a 64 KB queue in the core,
which `main.c` writes out between packet batches (`send(MSG_DONTWAIT)` to the journal's socket,
else only when `poll` says stdout is writable, at most 4 KB at a time). A stalled reader therefore
cannot stall the relay: when the queue is full, lines are dropped and counted (tested: stdout into
a full pipe nobody reads, verbose, 400 junk datagrams per 10 ms for 10 s: STATUS still answers,
190,000 invalid datagrams counted, the lines held back counted). Configuration errors at start
and on reload go to stderr (not during packet processing).

Privacy (section 4 above): normal lines carry no IP addresses at all; verbose lines carry them
truncated to /24 (`198.51.100.x`), so no extra option is needed. Room codes appear as their first
half only (`K7QM-xxxx`: tells rooms apart, does not let anyone join). Never logged: passwords,
verifiers, salts, keys, tokens, cookies, game data. Flood protection: every event line takes a
token from one budget (20 lines a second, bursts of 200); lines over it are dropped and counted,
and the next line that passes says `(N log lines held back: too many events)`; the count is also
a metric. (ctest `rdv`: 5000 invalid datagrams in verbose mode log ~200 lines; the log of a
password room with relay traffic holds neither the password, the full code, the payload nor a
full address; normal mode logs no address.)

### Metrics

Kept by the server core as plain counters (an increment per event, no allocation per packet);
rates are computed once per 10 s in the timer, the busiest rooms only when asked.

* **Counters**: rooms opened, closed by the host, timed out; joins ok / failed; sessions direct /
  relayed (a session counts as relayed when its first datagram goes through the relay, as direct
  when it ends without one - punched, LAN, or never connected); requests; relay datagrams and
  bytes in (received) and out (forwarded); drops by reason: `request_rate`, `relay_datagram_rate`
  and `relay_bandwidth` (per player), `relay_address` (per address), `relay_total` (the server's
  cap), `invalid` (does not parse, wrong direction),
  `unknown_room` (RELAY for no room / a room without a partner), `foreign` (a third address, or
  a wrong token); room code retries; log lines held back; send failures.
* **Gauges**: open rooms, players (hosts + partners), requests per second and relay bytes per
  second in / out (last ~10 s), uptime, CPU seconds user / system (`getrusage`), resident memory
  (`/proc/self/statm`, else the peak from `getrusage`), and the 5 busiest rooms by relay rate.

Two ways to read them:

* **Log line** every `stats_interval` seconds (default 300, 0 = off):
  `stats: up 120 s, cpu 0.06 s (0.05%), rss 1.1 MB | rooms 1, players 2 | opened 1, closed 0, timed
  out 0 | joins 2 ok, 0 failed | sessions 0 direct, 2 relayed | 0.2 requests/s | relay in 14758
  datagrams 11.9 MB, out 14758 11.9 MB, now 164.9 / 164.9 KB/s | drops: request rate 0, relay rate
  0, bandwidth 0, address 0, total 0, invalid 0, unknown room 0, foreign 1 | top rooms: E9XM-xxxx 164.9 KB/s`
* **STATUS** (type 12, answered only to 127.0.0.1): `recvx-client 127.0.0.1[:PORT] status` prints
  the full set in the Prometheus text format (`recvx_rooms 1`, `recvx_relay_bytes_total{dir="in"}
  12656100`, `recvx_drops_total{reason="relay_bandwidth"} 0`, `recvx_room_relay_bytes_per_second
  {room="E9XM-xxxx"} 168827`, ...), in as many datagrams of whole lines as it needs (~1.1-1.5 KB).
  The same output is the optional Prometheus dump without any server code: a cron job or a
  node_exporter textfile collector runs `recvx-client 127.0.0.1 status > recvx.prom.$$ && mv
  recvx.prom.$$ /var/lib/node_exporter/textfile_collector/recvx.prom`. No HTTP port was added.

**Overhead.** Relay load from two `recvx-client`s on 127.0.0.1, both Via server, `--dgrams 100`
(100 datagrams of 1164 bytes a frame each way plus the answers: ~11,900 relayed datagrams/s,
~12.9 MB/s through the server), 30 s per run, server CPU from `/proc/PID/stat` (10 ms ticks),
limits raised out of the way, three runs each:

| server | CPU per 30 s run | mean |
|---|---|---|
| main (4574321, no metrics) | 530 / 550 / 660 ms | 580 ms |
| this branch, metrics line off (`stats_interval = 0`) | 550 / 680 / 640 ms | 623 ms |
| this branch, metrics line every 1 s + STATUS polled every 1 s | 550 / 690 / 570 ms | 603 ms |
| final code (+ per-address and total budgets, log queue), line every 1 s + STATUS every 1 s | 550 / 550 / 560 ms | 553 ms |

The differences are inside the run-to-run spread (530-690 ms within one variant): the metrics cost
nothing measurable. The server spends ~1.6 us of CPU per relayed datagram (mostly `recvfrom` /
`sendto`), 1.8% of one core at 12.9 MB/s; RSS stays 1.1 MB. In the game runs above: 0.06 s CPU
in two minutes for one busy relayed room.

### Tests and verification (M7)

* ctest (Linux, 10 tests) passes. `rdv` adds: the config parser (every key, comments, blanks,
  BOM, CRLF, sections, old names; unknown keys warn and the rest applies; 22 invalid values each
  an error with the setting unchanged; overrides file < flag; the shipped example parses without
  a warning and equals the defaults), the metrics (five relaying rooms, rates, the busiest room
  first, STATUS only from 127.0.0.1 and in whole-line datagrams, the summary line), the log (the
  budget under 5000 invalid datagrams, no password / code / payload / full address, no address in
  normal mode), the relay budgets (membership churn, the server total) and the log queue
  (ordered, bounded under a stalled reader). `rdv_paths` adds the sustained-delivery case and
  checks both ends.
* The server starts with the example config (`--config pc/server/recvx-server.conf.example`),
  with flags on top (`--relay-kb-per-sec 1024` wins over the file), with no file; it refuses to
  start (exit 2, file:line message) on `port = 0`, a missing `--config` file, an unknown flag,
  `listen = ::1`, a flag without its value; SIGHUP applies a changed limit and log level, says
  that `max_rooms` needs a restart, and keeps the running settings when the file has an error.
* Two headless games through a local server, Via server, extras 3 + placements + randomizer, a
  room load and a rejoin: no drops at the defaults; `--relay-kb-per-sec 50`: drops counted and
  logged once per 10 s per room. Verbose on (`-v`): HELLO, the room opened, the partner joined,
  `relaying (from the joiner)`, STATUS, all with /24 addresses; off: only the room and the limit
  lines, no addresses.
* Metrics: the `stats:` line every `stats_interval` and `recvx-client 127.0.0.1 status` (above).
* systemd: `systemd-analyze verify` of the unit passes, `systemd-analyze security`: 1.2 OK (not
  run under systemd here: no root). The Dockerfile was not built (no docker/podman here).
* Lobby: the joining row reads CODE (Automatic with a server, Via server) or ADDRESS (Direct) with
  the matching help line; F3 "Room code" / "Join address" (screenshots, headless).
* Windows cross build compiles; single player: 105 frame dumps identical to main's build.

## 11. Protocol 2: the Codex security review (findings 1-4, 7-9, 11, 13, 14 and test gaps)

A read-only Codex review of the server and client (main at 4574321) found 14 issues; 5, 6, 10 and
12 were fixed with section 10. These are the rest, each checked against the code first (all held).
Wire format and crypto changed, so the rendezvous protocol is now **version 2**: a protocol-1
game gets "The co-op server speaks protocol 2, this game 1: update the game" and a protocol-1
server the reverse. The game protocol (11) and direct P2P (`ip:port#CODE`) are unchanged.

| finding | what was wrong | fixed how |
|---|---|---|
| 1 (high) INTRO / downgrade | The host took any INTRO from the server's address (no room, role, token or state check), so a spoofed one reset its state. Both sides took the E2E flag from the INTRO, so the server or an on-path attacker could switch encryption off, even with a password. | The host takes an INTRO only for its room, with its own host token (a blind spoofer does not have it), role 0, in a state that expects one (`rendezvous.c on_intro`). Encryption is decided locally (section 9 "End-to-end encryption"): the host by its setting and always with a password; the joiner refuses a plaintext host unless it allows plaintext itself, and always with a password; a joiner that typed a password refuses a room without one ("This room has no password, but you entered one"). The keys are bound to the whole introduction (room id, epoch, flags, room code, both public keys; `RdvIntro`), so any change on the way gives the two sides different keys: nothing decrypts, nothing goes out in clear. Key confirmation: the first punch with a valid MAC or the first authenticated datagram proves the partner has the same keys, so the same transcript (and password); the status says "encrypted" only then (before: "keys not yet confirmed"), and the log `the partner's keys match (introduction and password confirmed)`. No explicit confirmation message: all game data is AEAD-protected, so it is the confirmation. |
| 2 (high) key/nonce reuse | Any INTRO with another sequence re-derived the same keys (same X25519 keys) and reset the counters: the same key and nonce for new plaintext, old ciphertext accepted again. | The member sequence is the epoch: a repeat (same epoch) changes nothing, an older one is dropped, only a newer one is taken, and the epoch is in the key derivation: a new epoch is new keys, counters start at zero only with them. A degenerate (low-order) peer key, whose shared secret is zero, is refused. A server restart means a new registration with a new key pair (below). |
| 3 (high) hash-collision reset | One 4096-entry table, a collision overwrote the entry: spoofed HELLOs from a colliding address reset an address's failed joins and request budget (and since section 10 its relay budget). | Two tables (server.c): a direct-mapped pre-cookie table that only rate-limits replies (a collision there only gives more replies, capped by the new global `requests_total_per_sec`, default 5000), and a table of verified addresses (a valid cookie or token) with failed joins and the relay budget: keyed hash (BLAKE2b with the per-run secret), open addressing over 16 slots, and an entry is only replaced after 2 minutes idle, when all its budgets would have refilled anyway. A full neighbourhood refuses the newcomer ("server busy", counted as `busy`) instead of evicting. HELLO cannot create or touch a verified entry, so spoofing cannot reset anything. Global guess budget: `failed_joins_total_per_min` (default 600; with 2^40 codes ~1e-9 per guess with 1000 open rooms), and a per-room soft backoff for passwords (below, "Open limits closed"). |
| 4 (medium) fail closed | Code and token generation ignored randomness failures (zero tokens); the client used zero or stale keys; Argon2 allocation failure returned an all-zero password key. | `rdv_random` retries EINTR and wipes its output on failure; the server refuses CREATE / JOIN ("server busy") without randomness before any room or member exists; the client fails ("No randomness from the system") before sending a key; `rdv_pw_key` returns 0 with a wiped key on allocation failure and the caller fails ("Cannot derive the password key"). |
| 5, 6, 10, 12 | relay churn and budgets, relay capacity, blocking output, config validation | section 10 |
| 5 (rest) relay authentication | RELAY was authenticated by the source address only: whoever knew the room id and a member's address (the partner does) could spend that member's budget and keep it alive. | RELAY frames carry a counter and an 8-byte MAC keyed with the sender's member token (16 random bytes the server gave only to that member; the partner never sees it). The server checks MAC and a 64-entry counter window first; only then does the frame cost budget, count as a sign of life (`last_ms`) or get forwarded. A new member's per-player budget starts empty. Throttled frames do not refresh liveness. New drop reasons `relay_auth`, `relay_replay`. Header 12 -> 24 bytes. |
| 7 (medium) code grace under churn | The retired-code table (4 x max_rooms) silently dropped retirements when full, so codes could come back early. | CREATE is refused when open rooms + codes in their grace period would not leave a slot for its own later retirement ("too many rooms opened and closed"), so every retirement is kept for the whole grace period. **Replaced in section 13**: that refusal let one client lock every CREATE on the server. |
| 8 (medium) server restart | Clients ignored keepalive answers: after a restart a host showed a dead code forever. | The client tracks the answers. A waiting host that hears nothing for 5 s registers again (new key pair, new code; lobby and F3 show the new code: "the co-op server lost the room (restarted?): opening a new one"). In a session (keepalives every 10 s): after 35 s without an answer a direct session goes on (status "- the co-op server lost the room") and the host registers again once the partner has been silent for 10 s; a relayed session is over (the joiner: "The co-op server lost the room (restarted?): the relayed session ended"; the host registers again at once). |
| 11 (medium) DNS | The server's name was resolved synchronously on the game thread. | `net_resolve_start/poll/cancel` (net.c: a detached thread, pthreads or Win32, reference-counted so cancelling never blocks); `rdv_start` takes the name (`server_name`), shows "looking up the co-op server ...", fails after 5 s ("no answer from DNS within 5 s"), caches a result for 10 minutes; a numeric address needs no lookup. The lobby only checks the syntax. |
| 13 (low) HELLO builder | `rdv_build(HELLO)` with less than 64 bytes of room looped forever. | The padding loop stops at the first failed write; tested for every type and every size below its length. |
| 14 (low) secrets | The client kept the password, the lobby its edit buffer, the server its verifiers and cookie secret. | The client wipes the password once the key is derived (host at start, joiner after "password needed"), and keys, token and password on stop and on failure; the lobby wipes its edit copy; the game wipes its config copy; the server wipes rooms (verifiers, tokens) and the cookie secret on shutdown. The lobby's PASSWORD field itself stays (what the player typed, never saved). |
| 9 (medium) / test gaps | see "Tests" below | |

What protocol 2 still does not do (known limits): an on-path attacker can drop or delay anything
(denial of service; it cannot read or forge game data); without a password a malicious server can
still swap the public keys (man in the middle) - use a password against an untrusted server, or
compare the check code (below) by voice; the
member token travels in clear in ROOM / INTRO, so an on-path attacker could forge RELAY frames for a
member (costing its budget, not reaching its data). These two are inherent (the server is on the path
and gives out the tokens).

### Open limits closed

* **Room lockout by password guessing.** Before, 10 wrong passwords a minute from any addresses
  closed a room for joins, so anyone who knew its code could keep it closed. Now there is no
  lockout: wrong passwords count per verified address (`failed_joins_per_min`, as every failed
  join) and in the global `failed_joins_total_per_min`; per room, past 10 wrong ones a minute, an
  address that already failed a join this minute must wait 2 s after the room's last wrong
  password (`RDV_E_RATE` otherwise, not checked, no guess). An address without failures is always
  checked, so a joiner from another address gets in however hard the room is being guessed. A
  guesser gets one try per fresh address and then at most one per 2 s per room (all addresses
  together), inside its own and the global budget. Test (`rdv`): 11 wrong passwords from 11
  addresses, a failed address is refused at once and checked again after 2 s (even with the right
  password it waits), the real joiner from a new address gets in.
* **Far-future epoch.** Before, a server (or anyone with the host token) could send an INTRO with a
  huge epoch; the host took it, and every real introduction after it was "old" until the host
  registered again. Now the host takes only epochs `ours + 1 .. ours + 64` (`EPOCH_STEP`,
  rendezvous.c); the server's epoch is its count of joiners in this room, so a real gap stays far
  below 64. A new registration (start, server restart) starts again from 0, so the restart path is
  unchanged (`rdv_client` restart test). A token holder can still step 64 ahead per forged INTRO,
  but each one must be sent and is a session the real partner's keys do not match. Test
  (`rdv_client`): an INTRO 65 ahead is dropped (keys and session kept), 64 ahead is taken.
* **Passwordless man in the middle: visible.** Once the keys are confirmed, the status (lobby and
  F3, both sides) reads "..., encrypted, check code K7Q2MX": 6 characters (30 bits) from the same
  transcript hash as the session's keys (`RdvSession.verify`, rdv_proto.c), so they are equal on
  both sides only if both have the same keys: a server that swapped the public keys leaves two
  different codes. The players compare it by voice; no protocol change. Tests: `rdv` (equal on
  both sides, different without the password or with a swapped key), `rdv_client` (the host's
  status shows it and the partner derives the same).

ctest (12 tests, movies skipped) passes with all three; the Windows cross build compiles (`recvx.exe`).
The two above stay inherent: on-path dropping, and the member token in clear (on-path RELAY
forgery costs a member's budget, never reaches its data).

### Tests

* ctest `rdv` adds: keys bound to each part of the introduction, the low-order key, RELAY MACs
  (payload, counter, room, other token), the password KDF and the randomness failing closed,
  every message into every too-small buffer; on the server: relay frames with a wrong MAC, from the
  partner's address, replayed, altered; liveness not refreshed by unauthenticated frames; spoofed
  HELLOs and 9000 new addresses do not reset an address's failed joins (the table refuses
  newcomers instead); the global failed-join budget; the per-room password limit; no randomness:
  no room, no member; the code grace under churn (16 slots for 4 rooms: the 17th room is refused
  until codes leave their grace period); the amplification invariant tightened: no reply bigger
  than its request goes to an unverified source, and nothing goes anywhere else (fuzz loop).
* ctest `rdv_client` (new, deterministic: rendezvous.c against the real server core on a fake
  network and clock, the partner played by the test): the host ignores INTROs with another role,
  room, token or source; an INTRO with the E2E flag cleared changes nothing (the host's policy and
  flags); a repeat and an older epoch keep the session and the counters; a replayed INTRO with a
  newer epoch gets new keys (counters from zero only with them; the old keys no longer decrypt);
  member replacement; Via server through the relay with authenticated frames; punches lost for 2.5 s
  (relay), then a late flagged punch -> direct; a forged punch changes nothing; the joiner refuses a
  plaintext host, joins one when both allow plaintext, refuses a room without the password it
  typed, refuses a password room whose INTRO lost its E2E flag, and matches the host's
  password-bound keys; server restart while waiting (new code, the new code works) and during a
  direct session (it goes on, then a new room); DNS deadline, success and cache; no randomness, no
  memory for the KDF.
* ctest `rdv_fuzz` (new, Linux): `pc/tests/fuzz_rdv.c` under AddressSanitizer + UBSan, 300,000
  inputs per run (~9 s): the parser (what parses rebuilds into the same type), `rdv_open`,
  `rdv_relay_ok`, the server as a state machine (datagram sequences from 8 addresses with valid
  cookies patched in, so CREATE / JOIN reach the room logic: ~5000 rooms opened per run) and the
  client state machine (host and joiner fed datagrams from the server's and the partner's
  address). `LLVMFuzzerTestOneInput` is libFuzzer-ready (`clang -fsanitize=fuzzer -DRDV_LIBFUZZER`;
  no clang here); without it a seeded mutation loop runs. Not coverage-guided.
* `rdv_paths`: both ends must succeed (since section 10) and new cases: both `--plain`, a plaintext
  host refused by an encrypting joiner, a password typed for a room without one, 30% loss on the
  host only (asymmetric). `rdv_nat`: both ends must succeed.
* Two headless games through a local server (protocol 2, after merging main 1ef593f; host new
  game teleported to 0/6 with 30 enemies, the partner joins from the title by room code, 60 s):
  Automatic (punched, `direct (LAN)`, keys confirmed 30 ms after the introduction), Via server
  (relay, +52 bytes per datagram), relay fallback (`RECVX_COOP_NO_PUNCH=1`: relay after 2 s),
  password (`--coop-pw` on both: "introduction and password confirmed", direct (LAN)), no password
  (the first three). No partner time-outs; the server's metrics show no drops of any kind (relay
  auth, replay, budgets). Server restart while hosting: the server killed 8 s after the room
  opened and started again 3 s later; the host logged "the co-op server lost room ... opening a
  new one" 13 s in, had a new code 0.1 s later, the partner joined with the new code and played on
  (direct (LAN)). ctest (12 tests, movies skipped) passes; the Windows cross build compiles (`recvx.exe`,
  `test_rdv.exe`, `test_rdv_client.exe`); single player: 105 frame dumps identical to main's build
  (1ef593f).

Cost on the wire: the RELAY header grew from 12 to 24 bytes, so a relayed datagram carries +52
bytes instead of +40 (busy room: 5 datagrams a frame, +60 bytes a frame, ~1%), inside the headroom
of section 10's defaults. Server CPU: the MAC check is one BLAKE2b over each relayed datagram.
Section 10's load test (two `recvx-client`s, `--dgrams 100`, ~11,900 relayed datagrams/s,
12.9 MB/s, 30 s): 1070 ms of server CPU instead of 553-623 ms, i.e. ~1.4 us more per datagram,
~3.1 us in all: 3.6% of one core at 12.9 MB/s (a busy relayed room is ~0.17 MB/s).

## 12. Protocol 3: a crashed partner rejoins at once, KICK, member_timeout

User report (docs/COOP.md "Protocol 16"): after the partner's game crashed, its place in the room
stayed taken until the 30-s time-out, so the friend could not join again. Three changes, so the
rendezvous protocol is now **version 3** (a protocol-2 game or server gets "speaks protocol 3,
this game 2: update"); the game protocol is 16.

* **Rejoin tag (JOIN).** JOIN carries 16 more bytes: the player's rejoin tag for this room,
  `rdv_rejoin_tag(secret, room code)` = BLAKE2b keyed with the player's 32-byte secret (made once
  per installation, recvx.ini `coop_player_key`) over `"recvx rejoin v1"` and the code. The server
  keeps it with the member (in memory, for the member's life). A JOIN to a full room with the
  *same* non-zero tag replaces the stale member at once: the old member leaves ("the same player
  joined again"), the new one gets a new token and member sequence, and both sides get INTROs as
  for any new partner (the host's rendezvous takes the newer epoch, new keys). Any other tag, no
  tag or a zero tag: "room is full" as before. The password check comes first. Who can replace a
  member: whoever has its tag - the player (from its secret), the server, and an on-path observer
  of that JOIN (who can disrupt the session anyway). A name or a player id alone does nothing. The
  tag is per room code, so a tag seen in one room is useless in another.
* **KICK 13** (host -> server: room id, host token, member sequence; 24 bytes): "Remove stuck
  player". Only from the host's address with the host's token (`owner`); only the member it names
  (a late copy cannot remove the next partner). The server frees the place (`removed by the
  host`), keeps the room and its code, and answers like a keepalive (`partner present` 0). The
  host's rendezvous (`rdv_kick`) drops the session's keys at once - nothing of the old partner is
  decrypted or relayed any more - waits for the next introduction, and sends KICK again on a
  keepalive answer that still shows the member it removed.
* **`member_timeout`** (config key / flag, default **10 s**, 6..86400): a joiner without a keepalive
  or an authenticated relayed datagram for that long leaves; the host's room keeps `room_timeout`
  (30 s). The games send keepalives every **3 s** in a session (were 10 s), so two may be lost
  before a live direct partner is dropped (a relayed one is alive through its traffic anyway).
  The KICK and the extra keepalives count against `requests_per_sec` (20) like any request.

Metrics: `recvx_partners_left_total{why="rejoined|removed|timeout"}` (STATUS) and "partners
rejoined, removed, timed out" in the summary line. The log says why a joiner left (verbose).

Tests: `rdv` - the tag depends on key and code; a member with a tag; other tags, none and zero
are turned away; the same tag from a new address takes the place (new token, member sequence 2,
the old token dead); KICK from another address, with the member before, twice; the room code
works again after a KICK; a joiner's own token cannot KICK; a silent joiner leaves after 10 s
while the host's keepalives keep the room (and a zero tag never matches); the liveness test now
expects `member_timeout`; config `member_timeout = 12`, `member_timeout = 5` refused. `rdv_client`
- `rdv_kick`: the server frees the place, the client is back to waiting without keys, the room
and code stay through 4 s of keepalives, a new partner joins with the same code and its datagrams
decrypt. `rdv_fuzz` builds KICK and the longer JOIN as seeds. Two games and a local server:
docs/COOP.md "Protocol 16" (kill -9 and an immediate rejoin, Automatic and Via server; KICK and a
new player with the same code; the time-out alone).

## 13. Network review (rendezvous protocol 3, no bump; game protocol 18, no bump)

A read-only review of `pc/src/net` and `pc/server` found these; all are fixed without a protocol
change (old games and servers work with new ones).

* **CREATE churn locked the server (high).** Two addresses opening and closing rooms in a loop
  filled the retired-code table (4 x `max_rooms`); from then on every CREATE from anyone got
  "server full" (section 11 finding 7 refused CREATE to keep every code's grace period). Now: each
  address may open `rooms_per_ip_per_min` (6) new rooms a minute (a retry of the same request does
  not count; over it `RDV_E_RATE`, "Too many attempts: wait a minute"), kept in its address entry
  with the other budgets. When many addresses together still fill the table, a closed room's code
  is simply not kept for its grace period (logged once per 10 s): a late joiner of that code could
  meet a newer room only if the 2^40 draw hit it again. CREATE no longer scans the grace table.
* **RELAY crash (high).** `on_relay` read the member's address entry before checking for NULL.
  The entry is NULL when the member's address was idle for 2 minutes (room and member time-outs
  above 120 s) and new addresses took its slot neighbourhood meanwhile. Now the frame is dropped
  and counted as busy first.
* **Re-registration after a stall.** The game's rendezvous timers run on its frames; after the
  game thread stood still for more than 5 s (a long load, a debugger) the host took the missing
  keepalive answers for a lost room and opened a new one with a new code. Now the server counts as
  lost only when a keepalive sent at least 2 s ago (after the stall) is unanswered; a stall itself
  (more than 1 s between frames) makes what was sent before it wait afresh.
* **Replayed punches (medium).** See "Known limits": punches more than `punch_ms` + 10 s + 2 s after
  the introduction are ignored, so a recorded punch cannot move a relayed session later.
* Also: the RELAY and E2E replay windows are one function (`rdv_replayed`, `rdv_replay_mark`); the
  keepalive answer is built in one place (KEEPALIVE and KICK).

Tests: `rdv` - 40 addresses opening and closing rooms with `max_rooms` 4 (was: 16, then "server
full"), one address limited to 6 rooms a minute (its retry still answered, others unaffected, a
minute later again), RELAY from a member whose address entry was recycled (40000 failed joins
from new addresses after 130 s idle): dropped as busy, no crash (the review's `churn.c` and
`nullrelay.c` pass, the latter under ASan/UBSan); `rdv_client` - an 8 s stall keeps the room and
its code, a valid punch from another address 15 s after the introduction leaves the session on
the relay (both fail on the code before). `rdv_paths` and `rdv_nat` start the server with
`--rooms-per-ip-per-min 1000` (all their games come from one address).
