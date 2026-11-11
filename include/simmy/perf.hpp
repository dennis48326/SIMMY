// PC development and support aid: the frame-hitch profiler (src/platform/pc_perf.cpp, docs/performance.md).
//
// Off by default. On with SIMMY_PERF=1, `perf = on` in simmy.ini's [debug] section, or F3 / the F1
// overlay while playing. It splits each frame's main-thread time into phases (exclusive time: a load
// inside the simulation counts as load, not as simulation), counts what happened in the frame (resource
// loads, disc reads, cache evictions, allocations, GPU pipelines, texture uploads, card I/O, log
// output), and writes every frame that took much longer than usual to perf_log.txt with what happened
// in it, plus a summary when the game exits. Nothing it does reaches the game: it only reads clocks and
// counters.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace simmy::perf {

enum class Phase : std::uint8_t {
  Other,    // the game's update outside the phases below (UI, camera, animation, objects, ...)
  Sim,      // simulation ticks (cSimulatorImpl::SimulateOneTick)
  Route,    // path finding (XRoute::FindPath / ConstructGoals), inside the ticks
  Draw,     // the game's draw recording (EGameStateMan::Draw): display lists, GX commands
  Gpu,      // aurora's end of frame: GX command processing, encode, submit, present
  Acquire,  // aurora's begin of frame: waiting for the swapchain image (vsync back-pressure)
  Events,   // window / input event pump (SDL)
  Audio,    // the audio update on the main thread
  Load,     // resource loads (decode, allocation, sub-resources), without their disc reads
  Disc,     // disc / data-file reads (system calls)
  Card,     // memory card I/O
  Net,      // network play: waiting for the partner
  Pace,     // the frame limiter's sleep, as long as it was asked to be
  Late,     // the frame limiter's sleep beyond its deadline (timer granularity, the OS scheduler)
  Count
};

extern std::atomic<bool> g_on;  // read without a lock by the hooks
inline bool on() { return g_on.load(std::memory_order_relaxed); }

void init();               // pc_main.cpp, after the settings are read (SIMMY_PERF, [debug] perf)
void set_enabled(bool on); // F3 / the overlay
bool main_thread();

void enter(Phase p);  // main thread only (Scope checks)
void leave();

// Times a phase on the main thread while the profiler is on; elsewhere it does nothing.
struct Scope {
  bool active;
  explicit Scope(Phase p) : active(on() && main_thread()) {
    if (active) enter(p);
  }
  ~Scope() {
    if (active) leave();
  }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
};

// Counters and events (any thread; cheap no-ops while off).
void count_alloc(std::size_t bytes);  // EAHeap allocations
void count_disc(const char* file, std::uint64_t bytes, std::uint64_t nanoseconds);
void count_load(const char* manager, unsigned id, unsigned bytes, std::uint64_t nanoseconds);
void count_evict(const char* manager, int resources);  // cached resources freed to make room
void count_sim_tick();
void loading_screen(int delta);  // CLoadingScreen alive (+1 / -1)
void note(const char* fmt, ...);  // a free-form event line for this frame's record
void pace_late(std::int64_t nanoseconds);  // the pacing sleep woke this late (moved from pace to late)

// pc_main.cpp: the frame's end (after the pacing sleep). `context`: 0 menu, 1 house (live), set by
// the caller; `framecount` the game's frame number.
void frame_done(bool inHouse, int framecount);
// One line for the F1 overlay / a corner readout: median, worst of the last seconds, hitches.
const char* status_line();
void shutdown_summary();  // platform_shutdown_and_exit
void log_line(const char* fmt, ...);  // a line in perf_log.txt (the setup: settings, display, CPU)

}  // namespace simmy::perf

#define SIMMY_PERF_CAT2(a, b) a##b
#define SIMMY_PERF_CAT(a, b) SIMMY_PERF_CAT2(a, b)
#define SIMMY_PERF_SCOPE(phase) ::simmy::perf::Scope SIMMY_PERF_CAT(perfScope_, __LINE__)(::simmy::perf::Phase::phase)
