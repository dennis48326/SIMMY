// PC platform services used by the ported engine (implemented in src/platform/pc_main.cpp).
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ERC;

namespace simmy {

// Threading model on PC: the game's app "thread" (EApp::Main) runs on the process main thread, which
// owns the window, the GPU and input. Window events are pumped once per game frame.

// Pump window events. Returns false once the user has asked to quit (window closed); the first time
// that happens `onQuitRequested` (registered below) is called so the game can wind down.
bool platform_pump_events();
void platform_set_quit_handler(void (*onQuitRequested)());

// Frame boundary helpers for ENgcGraphics: wrap aurora_begin_frame / aurora_end_frame. Returns false
// (no frame) when headless or when the swapchain is unavailable.
bool platform_begin_frame();
void platform_end_frame();

// ---- Game data (data_path.cpp, gcm_disc.cpp) ----
// The game's files come from a GameCube disc image of The Sims 2 (a plain .iso / .gcm: `--iso <file>`,
// else iso= in simmy.ini, which SimmyLauncher writes) or, for development and tests, a folder holding
// the extracted disc (`--data <dir>`: DATA\, default.NGH, ...). pc_main.cpp opens one at startup;
// every game read (ENgcFile, movies, music streams, the neighbourhood file, the card icons) then
// goes through a DataFile. Game paths ("data/index.ind", "/DATA\\Index.ind", "dvd:/x") match
// case-insensitively, like the disc's file system. All of it is read-only.
// Opening fails with a message for the user in `error` (wrong format, damaged, not The Sims 2).
bool data_source_open_disc(const std::string& path, std::string& error);
bool data_source_open_folder(const std::string& dir, std::string& error);
bool data_source_ready();
std::string data_source_description();  // "disc image G4ZE69" / "folder <dir>", for logs

struct DataFile;
DataFile* data_open(const char* gamePath);  // null if there is no such file
void data_close(DataFile* f);
bool data_exists(const char* gamePath);
std::uint64_t data_size(const DataFile* f);
// Sequential reads from the handle's position (buffered; one thread at a time per handle).
std::size_t data_read(DataFile* f, void* dst, std::size_t size);
void data_seek(DataFile* f, std::uint64_t pos);
std::uint64_t data_tell(const DataFile* f);
bool data_error(const DataFile* f);  // a read came up short of the file's size (damaged image)
// Positional read, independent of the position: any number of threads may call it on one handle.
std::size_t data_read_at(const DataFile* f, std::uint64_t offset, void* dst, std::size_t size);
bool data_read_file(const char* gamePath, std::vector<unsigned char>& out);  // the whole file

// End of one game frame (EApp::SystemUpdate): pumps events, shows a placeholder frame if the game
// did not render one, and paces to ~60 Hz while the renderer's own pacing is unported.
void platform_frame_tick();

bool platform_headless();

// Asks the game to quit (the F1 overlay's Quit, closing the window): the game winds down through its
// own exit path, then pc_main.cpp's platform_shutdown_and_exit ends the process.
void platform_request_quit();

// ---- PC development options (pc_main.cpp / pc_devtools.cpp) ----
//   --offscreen               render without a window (aurora's headless surface), e.g. for CI
//   --screenshot N file.bmp   save frame N (counting from 1) as a BMP
//   --frames N                quit after N frames
//   --render-test             draw a test pattern with the game's shaders over each frame
//   --qd-test                 (or SIMMY_QD_TEST=1) after the first frame, load the UI quickdatas, list
//                             their tables and print a few NewUIStrings rows (pc_devtools.cpp)
struct DevOptions {
  bool offscreen = false;
  bool renderTest = false;
  unsigned screenshotFrame = 0;
  std::string screenshotPath;
  unsigned quitAfterFrames = 0;
  bool quickdataTest = false;
};
DevOptions& dev_options();
// The frame number --screenshot / SIMMY_SHOTS count in for the frame being drawn (from the house with
// SIMMY_FRAMES_FROM_HOUSE=1). Dev traces: dev_trace_frame("SIMMY_X") is true in the frame the variable
// names: "<n>" a game frame (_framecount), "d<n>" such a dev frame.
unsigned platform_dev_frame();
bool dev_trace_frame(const char* variable);
// Called by ESimsApp::UpdateDraw with the frame's immediate render context.
void dev_draw_overlay(ERC* rc);
// SIMMY_PAD_SCRIPT="frame:buttons[,frame:buttons...]" holds pad 1's buttons (start, a, b, x, y, up,
// down, left, right, joined with '+') for 6 frames from each game frame given; called after PADRead.
// In it and in SIMMY_PAD_SEQ a name prefixed "p2:" presses pad 2 ("lup+p2:lleft/40": both sticks at once);
// a script with "p2:" keeps pad 2 plugged in.
void dev_pad_script(PADStatus* pads);

// ---- Determinism test harness (pc_det.cpp; SIMMY_DET, SIMMY_DET_HASH, ..._RECORD, ..._REPLAY) ----
bool det_timing();                  // SIMMY_DET: the game's clock is virtual (one retrace a frame)
void det_frame_begin();             // EEngine::PreFrameUpdate
void det_live_update();             // ELiveMode::Update ran a house this frame
void det_pads(PADStatus* pads);     // ENgcControllerManager::Update, after the scripted input
bool det_replaying();               // a SIMMY_DET_REPLAY file drives the pads now
bool det_frame_end();               // platform_frame_tick; true: no pacing (SIMMY_DET_NOPACE)

// ---- Network play (pc_net.cpp; PC mod: network play) ----
// Two PCs, lockstep from boot: each runs the whole game (SIMMY_DET timing) and only pad input is
// exchanged; the host is player 1 (pad slot 0), the client player 2 (slot 1). docs/netplay.md.
bool net_session();                // a session was set up (also after it ended)
bool net_active();                 // ... and still runs in lockstep
int net_local_slot();              // this PC's pad slot (0 host, 1 client); 0 without a session
int net_view_player();             // the player whose view this PC shows full window; -1: no session
// det_pads (pc_det.cpp): takes this PC's input for a later tick and puts the tick's agreed input of
// both PCs into the pads (and the keyboard-and-mouse records). False: not in a session.
bool net_pads(int tick, PADStatus* pads);
// det_frame_end: tick `tick` has run. True: hash this tick (net_frame_end gets it).
bool net_wants_hash(int tick);
struct NetHash {
  unsigned long long total = 0, part[6] = {};
  std::string dump;  // every hashed field of the tick (SIMMY_DET_DUMP's lines)
};
// Sends this PC's input, compares hashes, and waits (pumping the window) until the next tick's input of
// both PCs is known. `hash`: this tick's, when net_wants_hash said so.
void net_frame_end(int tick, const NetHash* hash);
void net_local_quit();             // platform_request_quit: tell the partner and leave the session

// ---- Keyboard and mouse for player 1 (pc_kbm.cpp) ----
// The keyboard is a GameCube pad on port 1 (merged with the first controller when p1_input=auto, alone
// with p1_input=keyboard_mouse); the mouse adds what a pad cannot give, once per game frame, in this
// record (taken with the pads: SIMMY_DET_RECORD / _REPLAY keep it, so the game stays a function of its
// per-frame input). It belongs to the player whose controller is port 1 (KbmPlayer in siminteractor.cpp).
struct KbmTick {
  unsigned flags = 0;        // kKbmCursor, kKbmKeysMoved
  float cx = 0.0f, cy = 0.0f;  // kKbmCursor: the floor point under the mouse (tiles, the cursor's plane)
  float rot = 0.0f;          // camera turn this frame, degrees (mouse drag)
  float zoom = 0.0f;         // camera zoom this frame, in tenths of the zoom range (wheel, drag)
  float panX = 0.0f, panY = 0.0f;  // camera pan, -1..1 (right, away from the camera; keys, screen edges)
  bool operator==(const KbmTick& o) const {
    return flags == o.flags && cx == o.cx && cy == o.cy && rot == o.rot && zoom == o.zoom && panX == o.panX && panY == o.panY;
  }
};
enum : unsigned {
  kKbmCursor = 1,     // the mouse points at the floor in the player's 3D view: the classic cursor goes there
  kKbmKeysMoved = 2,  // the keyboard's stick keys are held: a mouse-driven classic control goes back to direct
};
const KbmTick& kbm_tick();      // this frame's (pad port 1's)
KbmTick& kbm_tick_for_det();    // pc_det.cpp records / replaces it with the pads
// PC mod: network play: each PC's keyboard and mouse belongs to its own pad slot, so the frame has one
// record per port (0, 1; port 2's is only ever filled by a network session). kbm_tick() is port 0's.
const KbmTick& kbm_tick_port(int port);
KbmTick& kbm_tick_port_for_det(int port);
int kbm_local_port();           // the port this PC's keyboard and mouse drive (net_local_slot)
void kbm_pads(PADStatus* pads);  // ENgcControllerManager::Update, right after PADRead
// ELiveMode's draw, after a player's 3D view is selected: what the mouse is mapped through.
void kbm_note_view(int player, const float lookAtProj[4][4], const float eye[3], const float dir[3],
                   const float viewport[4]);
// The game's side (siminteractor.cpp), asked once a frame before the input is made.
struct KbmGame {
  int player = -1;          // the player whose controller is port 1 (-1: none)
  bool live = false;        // live mode shows that player's 3D view (a house, the camera, not paused)
  bool cameraFree = false;  // ... and the C stick reaches the camera (no menu has it)
  bool cursorFree = false;  // ... and the left stick reaches the direct / classic control (no menu)
  bool classicReady = false;  // the classic control runs and takes A
  bool mouseClassic = false;  // the classic control was taken over by the mouse
  float cursorZ = 0.0f;     // the classic cursor's plane
};
void kbm_game_state(KbmGame& out);
// PC mod: network play: the frame's keyboard-and-mouse record for `player` (the record of the pad port
// the player is mapped to; empty for a player no keyboard drives).
const KbmTick& kbm_tick_for_player(int player);
// The nearest object the ray (eye, direction) meets before the floor (z = floorZ), by its bounding box
// (not `player`'s own sim); its point under the ray goes to (x, y). False: the floor is first. Reads only.
bool kbm_pick(int player, const float eye[3], const float dir[3], float floorZ, float& x, float& y);
int kbm_p1_input();         // 0 auto, 1 controller, 2 keyboard_mouse (p1_input=)

// ---- Co-op split screen (pc_splitscreen.cpp) ----
// `--split=gc|4x3|wide|windows` (split= in simmy.ini, SIMMY_SPLIT): how two-player live mode's two
// halves are shown.
//   gc       the original: a 4:3 frame (640x480) cut in two, each half projected at half the aspect.
//   4x3      two 4:3 views side by side (an 8:3 frame, 1280x480 by default), as the GC coop4x3 build.
//   wide     the window's aspect cut in two (1280x720 by default).
//   windows  each half in its own window (pc_window2.cpp: the second player's window), projected at
//            that window's aspect, its 2D undistorted (display_map_viewport); 1280x720 each by default.
// SIMMY_TWO_PLAYER=1 (dev): live mode starts in two-player mode with pad 2 as player 2.
enum class SplitMode { Gc, FourByThree, Wide, Windows };
bool set_split_mode(const char* name);
SplitMode split_mode();
// The window / offscreen frame size the mode starts with.
void split_default_frame_size(unsigned& width, unsigned& height);
// Each frame live mode draws: true while the two halves are drawn (the frame then fills the window and
// split_screen_aspect is its real aspect), false otherwise (4:3, fitted).
void split_screen_frame(bool split);
float split_screen_aspect();
void split_screen_end_frame();  // called by platform_frame_tick
// split=windows: ELiveMode::InitPlayerXWindow says whose half is drawn next (split_screen_aspect is
// then twice that player's window aspect, so the half's projection gets the window's aspect).
void split_screen_player(int player);
// False while each half has its own window (no divider bar between the halves).
bool split_draws_divider();
// This frame's two halves go to two windows (split=windows, a split frame, the second window open).
bool split_windows_frame();
// The second window's present for this frame (pc_window2.cpp, called by platform_end_frame).
// `windows`: the halves are shown each in its window, else window 2 mirrors window 1.
void window2_present(bool windows);
// The second window is there to draw into (shown, or the virtual one of offscreen runs), and its size.
bool window2_open();
bool window2_pixel_size(unsigned& width, unsigned& height);
bool dev_two_player();
// ---- PC mod: 4 players (pc_splitscreen.cpp, pc_window2.cpp; the game side is src/game/mp4.cpp) ----
// players= in simmy.ini (SIMMY_PLAYERS): the most players a game takes, 1..4 (default 2, the GC's).
int settings_max_players();
// split=windows: how many windows a split frame is cut into: 2 as before, up to kMaxPlayers while
// extra free-play players have joined (the EFB is that many windows wide, window k shows the k-th
// strip). Live mode sets it each frame before split_screen_frame.
void split_set_window_count(int windows);
int split_window_count();            // this frame's strips (2 outside a split=windows frame)
int split_window_count_requested();  // what live mode asked for last (the windows to keep open)
// Window 3 or 4 was closed by its player (once per close): the game drops that player out.
bool split_window_take_close_request(int index);
// Window `index` (0 = aurora's, player 1's; 1 = player 2's; 2, 3 = players 3 and 4) is there to draw
// into (shown, or a virtual one in offscreen runs), and its size in pixels.
bool split_window_open(int index);
bool split_window_pixel_size(int index, unsigned& width, unsigned& height);
// fire_code_limit= in simmy.ini (pc_settings.cpp): the GameCube's lot object limit (fire-code meter) is on.
bool settings_fire_code_limit();

// ---- PC mod: mods (pc_mods.cpp; the game side is src/game/modobjects.cpp) ----
// Off unless a mod folder exists (mods= in simmy.ini, SIMMY_MODS, or "mods" next to the exe). Each
// sub-folder with a mod.ini is a mod: [object] sections (objects the converter made from the user's own
// PC install, tools/pcobj/) and resource files "<manager>_<ID>.res" (e.g. models_9A3F0C12.res: the
// resource's bytes as an archive would hold them). Nothing of a mod is ever read from or written to
// the disc data or the saves except the objects' GUIDs.
struct ModTile {
  int x = 0, y = 0;              // the tile's offset from the group's first tile (sub index x << 8 | y)
  unsigned int guid = 0;         // its own GUID (saves name every part by GUID)
  float surface = -1.0f;         // a surface's top over this tile (tiles), -1: the object's
};
struct ModObject {
  std::string mod;               // the mod's folder name
  unsigned int guid = 0;         // the new object's GUID (a multi-tile object's: its lead's)
  unsigned int baseGuid = 0;     // the GameCube object it copies (behaviour, slots, catalogue tab), or
  std::string baseFile;          //   its object file name
  std::u16string name, desc;     // catalogue name and description
  int price = -1;                // -1: the base object's
  unsigned int model = 0;        // its model resource (every state), 0: the base object's
  unsigned int thumbnail = 0;    // its catalogue thumbnail shader, 0: the base object's
  int functions = -1;            // catalogue function flags (the buy tab), -1: the base object's
  int room = -1;                 // catalogue room rating, -1: the base object's
  std::vector<ModTile> tiles;    // more than none: a multi-tile object of these tiles (a group made from
                                 // the base: its lead, its first tile drawing the model)
  std::string pack;              // where it came from (informational)
  // A surface (table, counter, desk): the height of its top in tiles, where the base's object slots
  // put things (the slots keep their kind: what may go there); -1 the base's own (1.0).
  float surface = -1.0f;
  // Counter pieces (connect = counter / island): the models drawn instead of `model` by its neighbours
  // (ISimsCounterTopObject::SetObjOrient): 0 corner, 1 end closing the model's +x side, 2 its -x side,
  // 3 both ends; 0: none.
  int connect = 0;               // 0 none, 1 counter, 2 island
  unsigned int pieces[4] = {};
};
void mods_init();                // once at start-up (main thread), after settings_load
bool mods_enabled();             // a mod folder was found (also when it holds no mods)
const std::vector<ModObject>& mods_objects();
// A mod's resource for manager `manager` ("models", "shaders", "textures", ...) by id, read from its
// file into `out` (on demand: nothing is kept in memory); false if no mod has it. Only asked for ids
// the disc's archives lack (the disc always wins). Thread-safe (house loads run on a thread).
bool mods_resource(const char* manager, unsigned int id, std::vector<unsigned char>& out);
// The bytes the "SimsObjects" quickdata image keeps after its cells for the mod objects' rows
// (modobjects.cpp), sized from the mods' objects and tiles; 0 without mods.
unsigned int mods_tail_bytes();

// ---- Widescreen (pc_splitscreen.cpp, pc_hud.cpp; settings in pc_settings.cpp) ----
// aspect=16:9 (single player): the frame fills the window (aurora's "stretch" viewport policy) and
// the 3D views are projected at its aspect (split_screen_aspect). Everything else the game draws in
// 640x480 coordinates (Apt, movies, 2D rects, text) is mapped into the 4:3 centre of the frame by
// ENgcRenderStateCache::Apply calling display_map_viewport, unless it is a perspective view already
// projected at the frame's aspect. hud=edges moves Apt elements that sit in the left or right half
// of the stage to the frame's edges (apt_hud_enter / apt_hud_leave around each Apt instance).
void display_settings_changed();  // after aurora_initialize: the viewport policy for the aspect
extern bool g_displayMap2D;       // display_map_viewport has work to do this frame
// Returns true when the draw's projection must be replaced by `outProj` (same projection type).
bool display_map_viewport(float viewport[6], int scissor[4], const float (*proj)[4], float outProj[4][4]);
// pc_hud.cpp: the horizontal shift (640x480 units) of the Apt instance being drawn and how it is
// placed: 0 in the 4:3 centre, 1 stretched over the frame width, 2 at the centre's scale but not
// clipped to it (art drawn wider than the stage on purpose, e.g. the pause menu's dimmer).
void display_set_hud_anchor(float shiftX, int mode);
float display_side_bar();  // the width of each side bar around the 4:3 centre, in 640x480 units
bool display_hud_edges();  // hud=edges is in effect this frame
// split=windows (pc_hud.cpp): the window the Apt instance being drawn goes to (-1: by its viewport),
// whether it is laid out over the whole stage (a dialog both players see) or over the window's half,
// and its anchor: 0 centred, 1 stretched, 2 bleed (as display_set_hud_anchor's mode), 3 left edge,
// 4 right edge.
void display_set_hud_window(int window, bool wholeStage, int anchor);
// The stage width full-stage textured art is extended by on each side in that window (0: none).
float display_window_extend(float stageWidth);

// ---- Co-op private screens (pc_splitscreen.cpp, pc_hud.cpp) ----
// PC co-op: a full-stage screen one player uses on their own while the other keeps playing (food
// creation "ING", its recipe book "RCP", and the dialog box "0G2D" while it shows their help or exit
// prompt). On the GC these covered the whole TV and took both pads. Here the Apt instance of that
// name (a screen at the root of the movie) is drawn only into its player's window (split=windows) or
// half of the frame (gc, 4x3, wide), laid out over the whole stage and fitted undistorted.
// `player` < 0: the screen is no longer private.
void coop_set_private_screen(const char* instance, int player);
int coop_private_screen_owner(const char* instance);  // -1: not a private screen now
int coop_private_player();  // the player with a private screen up (-1 none): the rest of their HUD is hidden
// A one-window split frame with a private screen up: Apt draws go through display_map_viewport, and
// the private screen's (display_set_hud_window) are fitted into its player's half.
bool split_half_frame();

}  // namespace simmy

// Apt instance render hooks (AptDisplayList::render, libaptz/aptplayer.cpp): the instance's stage
// matrix is the current one. Global so the Apt code needs no simmy names.
struct AptCIH;
struct AptMatrix;
// Returns how many times the instance is drawn: 2 for a whole-stage dialog in the two-window co-op
// mode (once into each window; simmy_apt_hud_next_pass between the two).
int simmy_apt_hud_enter(AptCIH* inst, const AptMatrix& m);
void simmy_apt_hud_next_pass();
void simmy_apt_hud_leave();
// Widescreen: for textured art covering the whole stage, the stage width to extend it by on each
// side with its outer texel columns (0: draw as is).
float simmy_apt_hud_extend();
