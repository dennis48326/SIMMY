# Network play (PC mod)

A **PC mod**, not GameCube behaviour: the GameCube's two-player game played on two PCs over a network
(a LAN, or the internet with a forwarded port). Each PC runs the whole game; only the players' pad input
travels (lockstep). Player 1 hosts, player 2 joins. Code: [`pc_net.cpp`](../src/platform/pc_net.cpp)
(marked "PC mod: network play" wherever it touches other files). Why it works:
[determinism-and-netplay.md](determinism-and-netplay.md).

## How to play

1. Both PCs need the **same SIMMY build** and the **same disc image** (The Sims 2, USA). The game checks
   both (and its deterministic math) when the PCs connect and says what differs.
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

Over the internet the host forwards TCP port 27450 on its router to its PC; the client joins the
router's public address.

## What is shared

- **Memory card**: the host's. When the session starts the host sends its card (every file of Card A) to
  the client, which plays on a copy in its session folder (`%APPDATA%\SIMMY\net_session\USA\Card A`;
  headless `./net_session`); **the client's own card is never touched**. Saving in the game saves on
  both PCs: the host's own card (the shared game lives there) and the client's copy.
- **Settings that change the game**: `fire_code_limit`, the simulation's random seed, `players` (2 in a
  session: players 3 and 4 are not part of network play), intro skipping, the input delay and the check
  interval: the host's apply to both. Graphics, controls, window and audio settings stay each PC's own.
  The host's `SIMMY_*` development switches are compared and the client warns about differences.

## When something goes wrong

- **Refused at the start**: the message says what differs (build, math, disc). Use the same build and
  the same disc image on both PCs.
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
- **Protocol** (TCP, `TCP_NODELAY`, little-endian, each message `u32 size, u8 type, payload`): `Hello`
  (magic, protocol, slot, build, mathcheck hash, game data hash, data description, game id), `Refuse`
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
| `SIMMY_NET_ON_END=quit` | quit when the session ends |
| `SIMMY_NET_DIR=<folder>` | the session folder |
| `SIMMY_NET_DELAY`, `SIMMY_NET_HASH_EVERY`, `SIMMY_NET_TIMEOUT` | the `[net]` values |
| `SIMMY_NET_TRACE=1` | every check and every non-idle tick |

## Limits (step 1)

- Two players, Free Play and Story Mode (lockstep from boot covers both); no joining a running game (it
  would need the whole state, Apt's UI timing included), no reconnecting.
- TCP only; no NAT traversal or relay (forward the port for the internet).
- The game waits for the slower PC; there is no rollback or prediction, so the input delay is felt
  (2 ticks = 33 ms on a LAN).
- The host's slot B card and raw card images are not sent (the game uses slot A, GCI folders).
