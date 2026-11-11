// PC development and support aid: the frame-hitch profiler (simmy/perf.hpp, docs/performance.md).
//
// Each frame (one EApp::SystemUpdate, from the end of one pacing sleep to the end of the next) is cut
// into phases by Scope markers on the main thread; a phase's time is exclusive (a nested phase's time
// is taken out of its parent's). Counters (allocations, disc reads, resource loads, evictions) come
// from any thread. aurora's pipeline and texture counters are read once a frame. A frame is a hitch
// when it took longer than SIMMY_PERF_MS (default 25 ms) and 1.5x the median of the last 300 frames;
// each hitch gets a few lines in perf_log.txt (next to the executable, like game_log.txt; or
// SIMMY_PERF_LOG=<file>), the exit writes a summary. The game never sees any of it.
#include "simmy/perf.hpp"
#include "simmy/version.hpp"

#include <aurora/gfx.h>
#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace aurora::gx {
struct TextureStats;
// lib/gx/texture.hpp (not in aurora's public headers): the per-frame counters summed over all frames
// (cmake/aurora_patches/0004-texture-stat-totals.patch).
const TextureStats& texture_stats_total() noexcept;
}  // namespace aurora::gx

namespace simmy {
bool platform_headless();
bool settings_perf();  // pc_settings.cpp: [debug] perf = on
}

namespace simmy::perf {

std::atomic<bool> g_on{false};

namespace {
using Clock = std::chrono::steady_clock;
constexpr int kPhases = static_cast<int>(Phase::Count);
const char* const kPhaseNames[kPhases] = {"game", "sim", "route", "draw", "gpu", "acquire", "events",
                                          "audio", "load", "disc", "card", "net", "pace", "late"};

// Mirror of the first fields of aurora::gx::TextureStats (lib/gx/texture.hpp).
struct AuroraTextureStats {
  std::uint64_t uploadBytes, hashedBytes, objectHits, contentHits, replacementHits, misses, evictions;
};

std::thread::id g_mainThread;

struct Agg {
  unsigned count = 0;
  std::uint64_t bytes = 0, ns = 0;
};

// What happened in the current frame (reset by frame_done).
struct FrameData {
  std::int64_t ph[kPhases] = {};
  std::atomic<unsigned> allocs{0};
  std::atomic<std::uint64_t> allocBytes{0};
  std::atomic<unsigned> discReads{0}, discReadsBg{0};
  std::atomic<std::uint64_t> discBytes{0}, discNs{0}, discBytesBg{0};
  std::atomic<unsigned> loads{0}, loadsBg{0}, evicts{0}, ticks{0};
  std::mutex mutex;  // the maps and notes
  std::map<std::string, Agg> loadsBy, discBy, evictBy;
  std::vector<std::string> notes;
  unsigned droppedNotes = 0;
};

struct Hist {
  // Frame-time histogram in 0.5 ms buckets up to 500 ms.
  std::array<std::uint64_t, 1001> b{};
  std::uint64_t n = 0;
  double sum = 0, max = 0;
  void add(double ms) {
    ++n, sum += ms, max = std::max(max, ms);
    b[std::min<std::size_t>(1000, static_cast<std::size_t>(ms * 2.0))]++;
  }
  double pct(double p) const {
    if (!n) return 0;
    const std::uint64_t want = static_cast<std::uint64_t>(p * static_cast<double>(n));
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < b.size(); ++i)
      if ((acc += b[i]) > want) return static_cast<double>(i) / 2.0 + 0.25;
    return max;
  }
};

struct Context {
  const char* name;
  Hist hist;
  double phaseMs[kPhases] = {};
  std::uint64_t hitches = 0, over25 = 0, over33 = 0, over50 = 0, over100 = 0;
  double hitchMs = 0;  // time over the median in hitches
  // What happened in these frames, in all.
  std::uint64_t loads = 0, loadsBg = 0, discReads = 0, discBytes = 0, evicts = 0, pipelines = 0, texMisses = 0,
                texBytes = 0, texEvict = 0, allocs = 0, allocBytes = 0, ticks = 0;
  double discMs = 0, devSum = 0;  // devSum: |frame - 16.67 ms| for the pacing's steadiness
};

struct Worst {
  double ms;
  std::string text;
};

struct State {
  std::FILE* log = nullptr;
  std::string logPath;
  double thresholdMs = 25.0;
  bool started = false;
  Clock::time_point frameStart, mark;
  Phase stack[64];
  int depth = 0;
  FrameData f;
  std::vector<float> ring;  // last frame times
  std::size_t ringPos = 0;
  double avgPhase[kPhases] = {};  // running averages of normal frames (for the cause)
  unsigned frames = 0;
  Context ctx[3] = {{"menus"}, {"loading"}, {"in a house"}};
  std::map<std::string, std::uint64_t> causeCount;
  std::map<std::string, double> causeMs;
  std::map<std::string, std::uint64_t> tagCount;  // hitch frames that also had X
  std::map<std::string, Agg> houseLoads;           // resource loads while in a house (not loading)
  std::vector<Worst> worst;
  std::atomic<int> loadingScreens{0};
  std::uint64_t lastPipelines = 0, lastTexMisses = 0, lastTexUpload = 0, lastTexEvict = 0;
  long long lastLogBytes = -1;
  Clock::time_point minuteStart;
  Hist minute;
  unsigned minuteHitches = 0;
  char status[200] = "";
  double recentWorst = 0;
  Clock::time_point recentStart;
  std::uint64_t hitchesTotal = 0;
};

State& St() {
  static State s;
  return s;
}

std::int64_t Ns(Clock::duration d) { return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(); }

long long LogBytes() {
  // Where the game's own log (stdout + stderr, redirected to files by the launcher) is: its growth this
  // frame. A terminal or pipe has no position (-1): not measured.
#ifdef _WIN32
  const long long a = _lseeki64(_fileno(stderr), 0, SEEK_CUR), b = _lseeki64(_fileno(stdout), 0, SEEK_CUR);
#else
  const long long a = lseek(2, 0, SEEK_CUR), b = lseek(1, 0, SEEK_CUR);
#endif
  if (a < 0 && b < 0) return -1;
  return std::max(0LL, a) + std::max(0LL, b);
}

void OpenLog(State& s) {
  if (s.log) return;
  if (const char* p = std::getenv("SIMMY_PERF_LOG"); p && *p) {
    s.logPath = p;
  } else {
    const char* base = SDL_GetBasePath();
    s.logPath = std::string(base ? base : "") + "perf_log.txt";
  }
  s.log = std::fopen(s.logPath.c_str(), "w");
  if (!s.log) {
    std::fprintf(stderr, "[perf] cannot write %s\n", s.logPath.c_str());
    return;
  }
  const std::time_t now = std::time(nullptr);
  char when[64];
  std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  std::fprintf(s.log, "SIMMY frame-hitch profiler - %s - %s\n", SIMMY_PORT_VERSION, when);
  std::fprintf(s.log,
               "A hitch is a frame over %.0f ms and 1.5x the median of the last 300 frames. Phases are main-thread\n"
               "milliseconds (exclusive): %s",
               s.thresholdMs, kPhaseNames[0]);
  for (int i = 1; i < kPhases; ++i) std::fprintf(s.log, ", %s", kPhaseNames[i]);
  std::fprintf(s.log, ".\n\n");
  std::fflush(s.log);
  std::fprintf(stderr, "[perf] frame-hitch profiler on: %s\n", s.logPath.c_str());
}

void Charge(State& s, Clock::time_point now) {
  const Phase cur = s.depth ? s.stack[s.depth - 1] : Phase::Other;
  s.f.ph[static_cast<int>(cur)] += Ns(now - s.mark);
  s.mark = now;
}

void AddAgg(std::map<std::string, Agg>& m, const char* key, std::uint64_t bytes, std::uint64_t ns) {
  Agg& a = m[key ? key : "?"];
  ++a.count, a.bytes += bytes, a.ns += ns;
}

std::string Kb(std::uint64_t bytes) {
  char b[32];
  if (bytes >= 1024 * 1024) std::snprintf(b, sizeof b, "%.1f MB", static_cast<double>(bytes) / 1048576.0);
  else std::snprintf(b, sizeof b, "%.1f KB", static_cast<double>(bytes) / 1024.0);
  return b;
}

std::string AggList(const std::map<std::string, Agg>& m, bool withTime) {
  std::string out;
  for (const auto& [k, a] : m) {
    char b[160];
    if (withTime)
      std::snprintf(b, sizeof b, "%s%s x%u %s %.2f ms", out.empty() ? "" : ", ", k.c_str(), a.count, Kb(a.bytes).c_str(),
                    static_cast<double>(a.ns) / 1e6);
    else
      std::snprintf(b, sizeof b, "%s%s x%u", out.empty() ? "" : ", ", k.c_str(), a.count);
    out += b;
  }
  return out;
}
}  // namespace

bool main_thread() { return std::this_thread::get_id() == g_mainThread; }

void init() {
  State& s = St();
  g_mainThread = std::this_thread::get_id();
  if (const char* t = std::getenv("SIMMY_PERF_MS"); t && std::atof(t) > 0) s.thresholdMs = std::atof(t);
  bool want = false;
  if (const char* e = std::getenv("SIMMY_PERF"); e && *e && *e != '0') want = true;
  if (settings_perf()) want = true;
  if (want) set_enabled(true);
}

void set_enabled(bool enable) {
  State& s = St();
  if (enable == on()) return;
  if (enable) {
    OpenLog(s);
    const auto now = Clock::now();
    s.frameStart = s.mark = now;
    if (!s.started) s.minuteStart = s.recentStart = now, s.started = true;
    s.depth = 0;
    for (auto& p : s.f.ph) p = 0;
    s.lastLogBytes = LogBytes();
    g_on = true;
    if (s.log) std::fprintf(s.log, "[perf] on at game frame (next)\n");
  } else {
    g_on = false;
    if (s.log) std::fprintf(s.log, "[perf] off\n"), std::fflush(s.log);
    std::snprintf(s.status, sizeof s.status, "off");
  }
  std::fprintf(stderr, "[perf] profiler %s\n", enable ? "on" : "off");
}

void enter(Phase p) {
  State& s = St();
  const auto now = Clock::now();
  Charge(s, now);
  if (s.depth < 64) s.stack[s.depth] = p;
  ++s.depth;
}

void leave() {
  State& s = St();
  if (s.depth <= 0) return;  // turned on inside a scope
  const auto now = Clock::now();
  if (s.depth <= 64) Charge(s, now);
  --s.depth;
}

void count_alloc(std::size_t bytes) {
  if (!on()) return;
  FrameData& f = St().f;
  f.allocs.fetch_add(1, std::memory_order_relaxed);
  f.allocBytes.fetch_add(bytes, std::memory_order_relaxed);
}

void count_disc(const char* file, std::uint64_t bytes, std::uint64_t ns) {
  if (!on()) return;
  FrameData& f = St().f;
  if (main_thread()) {
    f.discReads.fetch_add(1, std::memory_order_relaxed);
    f.discBytes.fetch_add(bytes, std::memory_order_relaxed);
    f.discNs.fetch_add(ns, std::memory_order_relaxed);
    std::lock_guard lock(f.mutex);
    AddAgg(f.discBy, file, bytes, ns);
  } else {
    f.discReadsBg.fetch_add(1, std::memory_order_relaxed);
    f.discBytesBg.fetch_add(bytes, std::memory_order_relaxed);
  }
}

void count_load(const char* manager, unsigned id, unsigned bytes, std::uint64_t ns) {
  if (!on()) return;
  FrameData& f = St().f;
  const bool mainT = main_thread();
  (mainT ? f.loads : f.loadsBg).fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lock(f.mutex);
  std::string key = std::string(manager ? manager : "?") + (mainT ? "" : " (bg)");
  AddAgg(f.loadsBy, key.c_str(), bytes, ns);
  if (mainT && ns > 1000000 && f.notes.size() < 24) {  // a slow single load: name it
    char b[128];
    std::snprintf(b, sizeof b, "load %s %08X %s %.2f ms", manager ? manager : "?", id, Kb(bytes).c_str(),
                  static_cast<double>(ns) / 1e6);
    f.notes.emplace_back(b);
  }
}

void count_evict(const char* manager, int resources) {
  if (!on() || resources <= 0) return;
  FrameData& f = St().f;
  f.evicts.fetch_add(static_cast<unsigned>(resources), std::memory_order_relaxed);
  std::lock_guard lock(f.mutex);
  Agg& a = f.evictBy[manager ? manager : "?"];
  a.count += static_cast<unsigned>(resources);
}

void pace_late(std::int64_t ns) {
  if (!on() || !main_thread() || ns <= 0) return;
  State& s = St();
  s.f.ph[static_cast<int>(Phase::Pace)] -= ns;
  s.f.ph[static_cast<int>(Phase::Late)] += ns;
}

void count_sim_tick() {
  if (on()) St().f.ticks.fetch_add(1, std::memory_order_relaxed);
}

void loading_screen(int delta) { St().loadingScreens.fetch_add(delta); }

void note(const char* fmt, ...) {
  if (!on()) return;
  char b[256];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  FrameData& f = St().f;
  std::lock_guard lock(f.mutex);
  if (f.notes.size() < 24) f.notes.emplace_back(b);
  else ++f.droppedNotes;
}

void frame_done(bool inHouse, int framecount) {
  if (!on() || !main_thread()) return;
  State& s = St();
  const auto now = Clock::now();
  Charge(s, now);
  const double total = static_cast<double>(Ns(now - s.frameStart)) / 1e6;
  s.frameStart = now;
  FrameData& f = s.f;
  double ph[kPhases];
  for (int i = 0; i < kPhases; ++i) ph[i] = static_cast<double>(f.ph[i]) / 1e6, f.ph[i] = 0;

  // aurora's counters (pipelines built, textures converted and uploaded) since the last frame.
  std::uint64_t pipelines = 0, queued = 0, texMisses = 0, texUpload = 0, texEvict = 0;
  if (!platform_headless()) {
    if (const AuroraStats* a = aurora_get_stats()) {
      pipelines = a->createdPipelines - s.lastPipelines;
      s.lastPipelines = a->createdPipelines;
      queued = a->queuedPipelines;
    }
    const auto& t = reinterpret_cast<const AuroraTextureStats&>(aurora::gx::texture_stats_total());
    texMisses = t.misses - s.lastTexMisses, s.lastTexMisses = t.misses;
    texUpload = t.uploadBytes - s.lastTexUpload, s.lastTexUpload = t.uploadBytes;
    texEvict = t.evictions - s.lastTexEvict, s.lastTexEvict = t.evictions;
  }
  long long logBytes = -1;
  if (const long long lb = LogBytes(); lb >= 0) {
    if (s.lastLogBytes >= 0) logBytes = lb - s.lastLogBytes;
    s.lastLogBytes = lb;
  }

  // The median of the last 300 frames.
  if (s.ring.size() < 300) s.ring.push_back(static_cast<float>(total));
  else s.ring[s.ringPos++ % 300] = static_cast<float>(total);
  std::vector<float> sorted(s.ring);
  std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
  const double median = sorted[sorted.size() / 2];

  const int ci = s.loadingScreens.load() > 0 ? 1 : inHouse ? 2 : 0;
  Context& c = s.ctx[ci];
  ++s.frames;
  c.hist.add(total);
  for (int i = 0; i < kPhases; ++i) c.phaseMs[i] += ph[i];
  c.over25 += total > 25.0, c.over33 += total > 34.0, c.over50 += total > 50.0, c.over100 += total > 100.0;
  s.minute.add(total);
  if (total > s.recentWorst) s.recentWorst = total;

  std::map<std::string, Agg> loadsBy, discBy, evictBy;
  std::vector<std::string> notes;
  unsigned dropped;
  {
    std::lock_guard lock(f.mutex);
    loadsBy.swap(f.loadsBy), discBy.swap(f.discBy), evictBy.swap(f.evictBy), notes.swap(f.notes);
    dropped = f.droppedNotes, f.droppedNotes = 0;
  }
  const unsigned allocs = f.allocs.exchange(0), loads = f.loads.exchange(0), loadsBg = f.loadsBg.exchange(0);
  const unsigned discReads = f.discReads.exchange(0), discReadsBg = f.discReadsBg.exchange(0);
  const unsigned evicts = f.evicts.exchange(0), ticks = f.ticks.exchange(0);
  const std::uint64_t allocBytes = f.allocBytes.exchange(0), discBytes = f.discBytes.exchange(0);
  const std::uint64_t discBytesBg = f.discBytesBg.exchange(0);
  f.discNs.exchange(0);
  c.loads += loads, c.loadsBg += loadsBg, c.discReads += discReads, c.discBytes += discBytes, c.evicts += evicts;
  c.pipelines += pipelines, c.texMisses += texMisses, c.texBytes += texUpload, c.texEvict += texEvict;
  c.allocs += allocs, c.allocBytes += allocBytes, c.ticks += ticks, c.discMs += ph[static_cast<int>(Phase::Disc)];
  c.devSum += std::abs(total - 1000.0 / 60.0);
  if (ci == 2)
    for (const auto& [k, a] : loadsBy) {
      Agg& h = s.houseLoads[k];
      h.count += a.count, h.bytes += a.bytes, h.ns += a.ns;
    }

  const bool hitch = s.frames > 30 && total > s.thresholdMs && total > 1.5 * median;
  if (!hitch) {
    // Running averages of a normal frame's phases (the cause of a hitch is the phase most over its average).
    const double k = s.frames < 60 ? 1.0 / static_cast<double>(s.frames) : 1.0 / 60.0;
    for (int i = 0; i < kPhases; ++i) s.avgPhase[i] += (ph[i] - s.avgPhase[i]) * k;
  } else {
    ++s.hitchesTotal, ++s.minuteHitches, ++c.hitches;
    c.hitchMs += total - median;
    int cause = 0;
    double best = -1e9;
    for (int i = 0; i < kPhases; ++i) {
      if (i == static_cast<int>(Phase::Pace)) continue;
      const double over = ph[i] - s.avgPhase[i];
      if (over > best) best = over, cause = i;
    }
    std::string causeName = kPhaseNames[cause];
    if (cause == static_cast<int>(Phase::Gpu) && pipelines) causeName += " (pipelines)";
    else if (cause == static_cast<int>(Phase::Gpu) && texMisses) causeName += " (texture uploads)";
    const std::string key = std::string(c.name) + ": " + causeName;
    ++s.causeCount[key];
    s.causeMs[key] += total - median;
    auto tag = [&](bool cond, const char* name) {
      if (cond) ++s.tagCount[std::string(c.name) + ": " + name];
    };
    tag(loads > 0, "resource loads on the main thread");
    tag(loadsBg > 0, "resource loads on a background thread");
    tag(discReads > 0, "disc reads on the main thread");
    tag(evicts > 0, "cache evictions");
    tag(pipelines > 0, "GPU pipelines built");
    tag(texMisses > 0, "textures converted + uploaded");
    tag(ticks > 1, "two or more sim ticks");
    tag(ph[static_cast<int>(Phase::Card)] > 0.0, "memory card I/O");
    tag(ph[static_cast<int>(Phase::Route)] > 2.0, "path finding over 2 ms");
    tag(logBytes > 4096, "over 4 KB of log output");

    std::string text;
    char b[512];
    std::snprintf(b, sizeof b, "[hitch] frame %d (%s): %.1f ms (median %.1f) cause %s\n  phases:", framecount, c.name,
                  total, median, causeName.c_str());
    text += b;
    for (int i = 0; i < kPhases; ++i)
      if (ph[i] >= 0.05) {
        std::snprintf(b, sizeof b, " %s %.1f%s", kPhaseNames[i], ph[i],
                      i == static_cast<int>(Phase::Sim) && ticks ? (" (" + std::to_string(ticks) + " ticks)").c_str() : "");
        text += b;
      }
    std::snprintf(b, sizeof b,
                  "\n  counts: allocs %u (%s), disc reads %u (%s) + %u bg (%s), loads %u + %u bg, evictions %u, "
                  "pipelines +%llu (queued %llu), textures +%llu (%s, %llu evicted)",
                  allocs, Kb(allocBytes).c_str(), discReads, Kb(discBytes).c_str(), discReadsBg, Kb(discBytesBg).c_str(),
                  loads, loadsBg, evicts, static_cast<unsigned long long>(pipelines),
                  static_cast<unsigned long long>(queued), static_cast<unsigned long long>(texMisses),
                  Kb(texUpload).c_str(), static_cast<unsigned long long>(texEvict));
    text += b;
    if (logBytes >= 0) text += ", log " + std::to_string(logBytes) + " B";
    text += "\n";
    if (!loadsBy.empty()) text += "  loads: " + AggList(loadsBy, true) + "\n";
    if (!discBy.empty()) text += "  disc: " + AggList(discBy, true) + "\n";
    if (!evictBy.empty()) text += "  evicted: " + AggList(evictBy, false) + "\n";
    for (const std::string& n : notes) text += "  " + n + "\n";
    if (dropped) text += "  (" + std::to_string(dropped) + " more events)\n";
    if (s.log) {
      std::fputs(text.c_str(), s.log);
      std::fflush(s.log);
    }
    s.worst.push_back({total, text});
    if (s.worst.size() > 40) {
      std::sort(s.worst.begin(), s.worst.end(), [](const Worst& a, const Worst& b) { return a.ms > b.ms; });
      s.worst.resize(20);
    }
  }

  // Once a minute a line: the minute's frames.
  if (now - s.minuteStart >= std::chrono::seconds(60)) {
    if (s.log) {
      std::fprintf(s.log, "[minute] %llu frames: median %.1f ms, p99 %.1f ms, max %.1f ms, %u hitches\n",
                   static_cast<unsigned long long>(s.minute.n), s.minute.pct(0.5), s.minute.pct(0.99), s.minute.max,
                   s.minuteHitches);
      std::fflush(s.log);
    }
    s.minute = Hist{};
    s.minuteHitches = 0;
    s.minuteStart = now;
  }
  if (now - s.recentStart >= std::chrono::seconds(5)) {
    std::snprintf(s.status, sizeof s.status, "median %.1f ms, worst of 5 s %.1f ms, %llu hitches", median,
                  s.recentWorst, static_cast<unsigned long long>(s.hitchesTotal));
    s.recentWorst = 0;
    s.recentStart = now;
  }
  // The phases after this point (pump, pacing) belong to the next frame's record.
  s.mark = now;
}

void log_line(const char* fmt, ...) {
  State& s = St();
  if (!s.log) return;
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(s.log, fmt, ap);
  va_end(ap);
  std::fputc('\n', s.log);
  std::fflush(s.log);
}

const char* status_line() {
  State& s = St();
  if (!on()) return "off";
  return s.status[0] ? s.status : "measuring...";
}

void shutdown_summary() {
  State& s = St();
  if (!s.log || !s.started) return;
  std::FILE* o = s.log;
  std::fprintf(o, "\n==== summary ====\n");
  for (const Context& c : s.ctx) {
    if (!c.hist.n) continue;
    std::fprintf(o,
                 "%s: %llu frames, mean %.2f ms, median %.2f, p95 %.2f, p99 %.2f, p99.9 %.2f, max %.1f ms; over 25 ms "
                 "%llu, over 34 ms %llu, over 50 ms %llu, over 100 ms %llu; hitches %llu (%.0f ms over the median in all)\n",
                 c.name, static_cast<unsigned long long>(c.hist.n), c.hist.sum / static_cast<double>(c.hist.n),
                 c.hist.pct(0.5), c.hist.pct(0.95), c.hist.pct(0.99), c.hist.pct(0.999), c.hist.max,
                 static_cast<unsigned long long>(c.over25), static_cast<unsigned long long>(c.over33),
                 static_cast<unsigned long long>(c.over50), static_cast<unsigned long long>(c.over100),
                 static_cast<unsigned long long>(c.hitches), c.hitchMs);
    std::fprintf(o,
                 "  in all: %llu resource loads (+%llu on other threads), %llu disc reads (%s, %.0f ms on the main thread), "
                 "%llu cache evictions, %llu GPU pipelines built, %llu textures converted (%s, %llu evicted), %llu "
                 "allocations (%s), %llu sim ticks; mean |frame - 16.67 ms| %.2f ms\n",
                 static_cast<unsigned long long>(c.loads), static_cast<unsigned long long>(c.loadsBg),
                 static_cast<unsigned long long>(c.discReads), Kb(c.discBytes).c_str(), c.discMs,
                 static_cast<unsigned long long>(c.evicts), static_cast<unsigned long long>(c.pipelines),
                 static_cast<unsigned long long>(c.texMisses), Kb(c.texBytes).c_str(),
                 static_cast<unsigned long long>(c.texEvict), static_cast<unsigned long long>(c.allocs),
                 Kb(c.allocBytes).c_str(), static_cast<unsigned long long>(c.ticks),
                 c.devSum / static_cast<double>(c.hist.n));
    std::fprintf(o, "  mean phase ms:");
    for (int i = 0; i < kPhases; ++i)
      std::fprintf(o, " %s %.2f", kPhaseNames[i], c.phaseMs[i] / static_cast<double>(c.hist.n));
    std::fprintf(o, "\n");
  }
  std::fprintf(o, "\nhitches by cause (the phase most over its usual time; ms = time over the median):\n");
  std::vector<std::pair<std::string, std::uint64_t>> causes(s.causeCount.begin(), s.causeCount.end());
  std::sort(causes.begin(), causes.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  for (const auto& [k, n] : causes) std::fprintf(o, "  %-40s %6llu  %8.0f ms\n", k.c_str(), static_cast<unsigned long long>(n), s.causeMs[k]);
  std::fprintf(o, "\nhitch frames that also had:\n");
  for (const auto& [k, n] : s.tagCount) std::fprintf(o, "  %-60s %6llu\n", k.c_str(), static_cast<unsigned long long>(n));
  if (!s.houseLoads.empty()) {
    std::fprintf(o, "\nresource loads while in a house (no loading screen):\n");
    for (const auto& [k, a] : s.houseLoads)
      std::fprintf(o, "  %-24s %7u loads %10s %9.1f ms\n", k.c_str(), a.count, Kb(a.bytes).c_str(),
                   static_cast<double>(a.ns) / 1e6);
  }
  std::sort(s.worst.begin(), s.worst.end(), [](const Worst& a, const Worst& b) { return a.ms > b.ms; });
  if (s.worst.size() > 10) s.worst.resize(10);
  std::fprintf(o, "\nworst frames:\n");
  for (const Worst& w : s.worst) std::fputs(w.text.c_str(), o);
  std::fflush(o);
}

}  // namespace simmy::perf
