# Network play (PC mod)

A **PC mod**, not GameCube behaviour: the GameCube's two-player game played on two PCs over a network
(a LAN, or the internet with a forwarded port). Each PC runs the whole game; only the players' pad input
travels (lockstep). Player 1 hosts, player 2 joins. Code: [`pc_net.cpp`](../src/platform/pc_net.cpp)
(marked "PC mod: network play" wherever it touches other files). Why it works:
[determinism-and-netplay.md](determinism-and-netplay.md).

## How to play

### With SimmyLauncher (the easy way)

1. Both PCs need the **same SIMMY build**, the **same disc image** (The Sims 2, USA) and the **same
   mods** (or none on both): copy the host's `mods` folder (next to simmy.exe) to the other PC. The
   games compare all three (and their deterministic math) when they connect and say what differs.
2. **Player 1 (host)**: open SimmyLauncher, choose your disc, press **Network play**. The left side
   shows this PC's address on your network (e.g. `192.168.1.20`) and the port (27450). Tell your
   friend the address, then press **Host a game**. The game opens and waits ("Waiting for player 2 to
   join"). The first time, Windows asks whether simmy.exe may use networks: allow it on **private
   networks**.
3. **Player 2 (join)**: open SimmyLauncher, choose your disc, press **Network play**, type the host's
   address in the right side's box (keyboard; with a controller use the on-screen keys, X erases) and
   press **Join game**. The launcher remembers the address for next time. Who starts first does not
   matter: the joining game keeps trying for a minute.
4. Both games start together at the title screen; choose **Two Player** as usual (see step 5 below).
5. If the session ends early, the launcher says why when the game closes: the PCs could not play
   together (it lists what differs: build, disc, mods), the host could not be reached, your friend left,
   the connection broke, or the games went out of step (then **Open session folder** shows the files to
   send).

The port is on the same page (bottom); both PCs must use the same one. It is saved as `[net] port` in
simmy.ini. Over the internet the host forwards that TCP port on its router to its PC and gives the
friend the router's public address (search "what is my IP"); the friend types `address` or
`address:port`.

### By hand (command line)

1. Both PCs need the **same SIMMY build**, the **same disc image** (The Sims 2, USA) and the **same
   mods**. The game checks all of them (and its deterministic math) when the PCs connect and says what
   differs.
2. **Host (player 1)**: `simmy.exe --iso "E:\Roms\Gamecube\Sims 2, The (USA).iso" --host`
   (or `run_net_host.bat`, or `host = yes` in simmy.ini's `[net]` section). The window shows "Waiting for
   player 2 to join", the port and this PC's LAN address (`ipconfig` lists it too: the IPv4 address,
   e.g. 192.168.1.20).
3. **Windows firewall**: the first time the host runs, Windows asks whether simmy.exe may use networks.
   Allow it for **private networks** (the home network should be "private"; if it is "public", allow
   that too or change the network's type). Without it the other PC cannot connect ("no answer from ...").
4. **Join (player 2)**: `simmy.exe --iso "..." --join 192.168.1.20` (or `run_net_join.bat 192.168.1.20`).
   The client retries for a minute, so either PC may start first.
5. Both games then start together from the title screen (the intro movies are skipped). The menus are
   shared: whoever presses buttons, both screens follow. Choose Two Player as usual; player 2's
   controller is the joining PC's. In the house each PC shows only its own player's view, full window,
   with that player's HUD. The pause menu, dialogs and build/buy mode are shared as on the GameCube.
6. Keyboard and mouse work on both PCs for their own player. The F1 options are local (they do not
   pause the game in a session; your pad is idle while they are open).

| simmy.ini `[net]` | Command line | Meaning |
|---|---|---|
| `host = yes` (or a port) | `--host [port]` | host (player 1) |
| `join = 192.168.1.20[:port]` | `--join <address>[:port]` | join (player 2) |
| `port = 27450` | `--net-port <port>` | TCP port (both PCs) |
| `delay = 2` | `--net-delay <ticks>` | input delay in 1/60 s ticks (the host's counts); 3-4 for the internet |
| `hash_every = 30` | | ticks between the out-of-step checks (the host's counts) |
| `timeout = 30` | | seconds without anything from the other PC before the session ends |
| `on_end = continue` | | after the session ends: keep playing alone (`continue`) or quit (`quit`) |
| `skip_intro = yes` | | the host's choice for both |
| | `--net-status <file>` | write how the session went to this file (SimmyLauncher; below) |

Over the internet the host forwards TCP port 27450 on its router to its PC; the client joins the
router's public address.

## Mods

Converted mods (PC objects, wall and floor coverings, fences, doors: [mods.md](mods.md)) change the
game: new catalogue entries, their trees, slots and quickdata rows. Both PCs must have **the same mod
set**, so the handshake carries a fingerprint of it: per mod folder a hash of its name, its `mod.ini`
and its resource files' names and sizes (contents are not read: about 0.1 s for 1400 mods), the total,
the counts (objects, doors, wall coverings, floor coverings, fences) and the folder list; the
`SIMMY_MODS_ONLY` screenshot switch counts too. On any difference both PCs refuse, e.g. "Different
mods: this PC has 1301 converted objects, ...; the host has 1374 converted objects, 72 doors, 1477
wall coverings, 645 floor coverings and 191 fences. Different counts of: objects, doors. 97 mod
folder(s) only on the host (pc1_..., ...)". Where each PC keeps its mod folder (`mods=`, `SIMMY_MODS`,
`mods` next to the exe) does not matter. No mod folder on both PCs is the same as no mods.

## What is shared

- **Memory card**: the host's. When the session starts the host sends its card (every file of Card A) to
  the client, which plays on a copy in its session folder (`%APPDATA%\SIMMY\net_session\USA\Card A`;
  headless `./net_session`); **the client's own card is never touched**. Saving in the game saves on
  both PCs: the host's own card (the shared game lives there) and the client's copy.
- **Settings that change the game**: `fire_code_limit` (the host's effective value, its
  `SIMMY_FIRE_CODE_LIMIT` included), the simulation's random seed, `players` (2 in a
  session: players 3 and 4 are not part of network play), intro skipping, the input delay and the check
  interval: the host's apply to both. Graphics, controls, window and audio settings stay each PC's own.
  The host's `SIMMY_*` development switches are compared and the client warns about differences
  (`SIMMY_MODS`, `SIMMY_CARD_DIR` and other per-PC paths are not compared; the mod sets themselves are).

## When something goes wrong

- **Refused at the start**: the message says what differs (build, math, disc, mods). Use the same
  build, the same disc image and the same mods folder on both PCs.
- **"Waiting for player N"** at the top: the other PC is late (loading, a hiccup, the network). The game
  waits; it never guesses. After `timeout` seconds without anything the session ends.
- **Player N left / connection lost**: the other PC quit or vanished. The session ends; with
  `on_end = continue` this PC keeps playing alone (the other player's sim is idle; save and quit as usual).
- **Out of step (desync)**: should not happen. Both PCs compare the game state every 30 ticks; on a
  difference both stop the session and write, into the session folder (`%APPDATA%\SIMMY\net_session`):
  `desync_host_tick<N>.txt` / `desync_client_tick<N>.txt` (every hashed field of that tick and of the
  last tick that still matched) and `net_host.rec` / `net_client.rec` (the session's input: a
  `SIMMY_DET_REPLAY` file). **Please send these files from both PCs**, with both logs
  (`net_host_log.txt` / `net_join_log.txt` from the .bat files).

## The status file (`--net-status`)

SimmyLauncher passes `--net-status <launcher folder>/net_status.txt` and reads it when the game ends.
The game rewrites it (via a `.tmp` and a rename) at each change: `result=` `connecting` (before the
connection), `playing` (handshake done), and at the end `refused` (the PCs differ), `failed` (no
answer, cannot listen, bad address), `cancelled` (the window was closed while waiting), `left` /
`lost` / `timeout` / `desync`, or `quit` (this PC quit). Also `role=host|client`, `tick=` (the last
tick), `folder=` (the session folder, absolute) and one `message=` line per line of the plain-words
reason (the same text as the game's log and message box). A refused or failed session exits with code
3; the others with 0.

## How it works

- **Lockstep from boot.** A session runs with `SIMMY_DET` timing from the first frame, as a
  `SIMMY_DET_BOOT` replay does: every frame is one tick of 1/60 s game time; pacing is 60 Hz
  (`fps_cap` does not apply). Tick N runs only when both PCs' input for tick N is known: at the end of
  tick N each PC sends its input sampled in tick N for tick N + delay, then waits for the other's input for
  tick N + 1 (ticks 1..delay are idle on both). A slow PC slows both.
- **Input** is one pad per PC: every local pad merged (controllers, the keyboard's pad) plus the
  keyboard-and-mouse record (`KbmTick`: the mouse's floor point, camera turn/zoom/pan). The host's goes to
  pad slot 1, the client's to slot 2; slots 3 and 4 are empty. Each player's `KbmTick` goes to the player
  on that pad (`kbm_tick_for_player`), so both PCs' mice work. The session's record (`net_*.rec`) holds
  both, as " m ..." (slot 1) and " n ..." (slot 2).
- **Display.** Each PC uses the two-window layout's frame (both halves at the window's aspect) and window
  1 presents its own player's half (`net_view_player`, pc_splitscreen.cpp / pc_window2.cpp); no second
  window. Outside the split views (menus, CAS, build/buy, a player's full-screen camera) both show the
  whole frame, as the GameCube's one TV did.
- **Local-only things**: the F1 overlay does not pause the game in a session; window size, fullscreen,
  graphics settings, F-keys, audio and particles do not reach the simulation (audio and particles have
  their own random streams, not hashed). Movies end after their length in game time
  (`ENgcMovie::IsFinished` with `SIMMY_DET`), not when the decoder thread's wall clock gets there.
- **Out-of-step checks.** Every `hash_every` ticks both compute the `SIMMY_DET_HASH` state hash (total and
  its six parts rng/sim/obj/per/ngh/inst) with the tick's dump, keep the last few, and send the hash.
- **Protocol** (version 2; TCP, `TCP_NODELAY`, little-endian, each message `u32 size, u8 type, payload`): `Hello`
  (magic, protocol, slot, build, mathcheck hash, game data hash, data description, game id, the mods'
  fingerprint: total, flags, five counts, then each mod folder's name and hash), `Refuse`
  (reason), `Session` (host: settings lines, card files), `Ready`, `Start`, `Input` (tick, PADStatus,
  KbmTick), `Hash` (tick, total, 6 parts), `Desync` (tick), `Bye` (reason), `Ping`/`Pong` (round trip).
  The transport is a byte `Stream` (pc_net.cpp): UDP with its own ordering or a relay can replace TCP.

## Testing (one PC)

Run a host and a client on 127.0.0.1, headless, each with its own scripted input (`SIMMY_PAD_SEQ`,
`SIMMY_SOAK`: a script on the client presses its pad 1, which is player 2's), `SIMMY_DET_HASH` per side,
and compare the hash files. Test switches:

| Variable | Does |
|---|---|
| `SIMMY_NET_LAG=<ms>`, `SIMMY_NET_JITTER=<ms>` | delay this PC's outgoing messages (in order) |
| `SIMMY_NET_STALL=<tick>:<ms>[,...]` | this PC freezes for ms after that tick |
| `SIMMY_NET_DESYNC=<tick>` | one extra simulation random number on this PC (a deliberate desync) |
| `SIMMY_NET_FAKE_BUILD=<text>`, `SIMMY_NET_FAKE_MATH=1`, `SIMMY_NET_FAKE_ISO=1` | a different build / math / data in the handshake |
| `SIMMY_NET_FAKE_MODS=<n>` | this PC's mod list without its last n mod folders (0 or text: one extra fake mod) |
| `SIMMY_NET_ON_END=quit` | quit when the session ends |
| `SIMMY_NET_DIR=<folder>` | the session folder |
| `SIMMY_NET_DELAY`, `SIMMY_NET_HASH_EVERY`, `SIMMY_NET_TIMEOUT` | the `[net]` values |
| `SIMMY_NET_TRACE=1` | every check and every non-idle tick |

## Testing with the launcher

`SimmyLauncher --auto-launch host` / `--auto-launch join:127.0.0.1` press *Host a game* / *Join game*
(with `--game-exe` a wrapper that adds `--headless` and the pad scripts, `SIMMY_LAUNCHER_KEEP_ENV=1`
to pass them through); `--show-status <file>` shows the message for a saved `net_status.txt`.

## Limits

- Free will (autonomy) stays on: the whole simulation is deterministic, autonomy included, and the
  soak tests (30 000 ticks with sims acting on their own, with and without the full mod set, with
  network lag) stay in step.
- Two players, Free Play and Story Mode (lockstep from boot covers both); no joining a running game (it
  would need the whole state, Apt's UI timing included), no reconnecting.
- TCP only; no NAT traversal or relay (forward the port for the internet). IPv4 only (the launcher
  shows the PC's IPv4 addresses; a host name works if it resolves to IPv4).
- The mods check compares mod folders by name, `mod.ini` and resource sizes, not resource contents:
  two different conversions that happen to give every resource the same size are not told apart.
- The game waits for the slower PC; there is no rollback or prediction, so the input delay is felt
  (2 ticks = 33 ms on a LAN).
- The host's slot B card and raw card images are not sent (the game uses slot A, GCI folders).
