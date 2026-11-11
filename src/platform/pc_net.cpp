// PC mod: network play (no GameCube counterpart). Two PCs play the GameCube's two-player game together
// over a network: each runs the whole game and only pad input is exchanged (lockstep). See
// docs/netplay.md (how to play) and docs/determinism-and-netplay.md (why it works).
//
//   simmy --host [port]            this PC is player 1 (pad slot 0) and waits for a player to join
//   simmy --join <address>[:port]  this PC is player 2 (pad slot 1)
//   simmy.ini [net]: host = yes | <port>, join = <address>[:port], port = 27450, delay = 2 (ticks),
//   hash_every = 30 (ticks), timeout = 30 (seconds), on_end = continue | quit, skip_intro = yes
//   (--net-port, --net-delay on the command line)
//
// The session:
// - Connect (TCP; the transport is the Stream class below, so UDP or a relay can replace it).
// - Handshake: both send their build (version.hpp), the deterministic math's hash (--mathcheck) and the
//   game data's identity (a hash of index.ind, the neighbourhood, the behaviour trees and the other small
//   archives' contents, and every archive's size; the disc's game id). Any difference: both refuse,
//   saying which. The host then sends what must be the same on both PCs: the simulation's random seed,
//   the settings that change the game (fire_code_limit; players is 2 for both), the session's input delay
//   and hash interval, intro skipping, its SIMMY_* development switches (the client warns when its own
//   differ), and its memory card (every file of Card A). The client puts the card into its session
//   folder (<user folder>/net_session/USA/Card A, headless ./net_session) and plays on that copy, so its
//   own card is never touched. The host plays on its own card (it is the shared game's home; saving in
//   the session saves there, and the client's copy gets the same save).
// - Lockstep from boot (SIMMY_DET timing, as a SIMMY_DET_BOOT replay): tick N runs only when both PCs'
//   input for tick N is known. At the end of tick N each PC sends its input sampled during tick N (every
//   local pad merged, keyboard and mouse included: pc_kbm.cpp's pad and its KbmTick) for tick N + delay,
//   then waits for the partner's input for tick N + 1. Ticks 1..delay have neutral input on both. Both
//   apply the same input: slot 0 = the host's, slot 1 = the client's, slots 2 and 3 empty. The pacing is
//   the game's 60 Hz (fps_cap does not apply); a slow PC slows both. A wait over 0.5 s shows a note.
// - Desync detection: every hash_every ticks both send the tick's state hash (pc_det.cpp's, with its six
//   parts). A difference ends the session on both PCs; each writes desync_<host|client>_tick<N>.txt in
//   its session folder (the tick's every hashed field, as SIMMY_DET_DUMP; with the last matching
//   check's dump), and net_<host|client>.rec (the session's input, a SIMMY_DET_REPLAY file).
// - The end: quitting (the window, F1 -> Quit, Alt+F4) sends a goodbye; the partner sees "player N left".
//   A lost connection or no data for `timeout` seconds ends it too. After the end the game goes on
//   locally (on_end = continue: the partner's pad is idle, this PC keeps its slot; save and quit as
//   usual) or quits (on_end = quit).
//
// Local-only things stay local: the F1 options do not pause the game in a session (its pads are idle
// while it is open), window and display changes and F-keys do not reach the game, audio and particles
// have their own random streams (not in the hash).
//
// Test switches: SIMMY_NET_LAG=<ms> and SIMMY_NET_JITTER=<ms> delay this PC's outgoing messages (in
// order), SIMMY_NET_STALL=<tick>:<ms>[,...] freezes this PC for ms after that tick, SIMMY_NET_DESYNC=<tick>
// draws one extra simulation random number at that tick (a deliberate desync), SIMMY_NET_FAKE_BUILD /
// _FAKE_MATH / _FAKE_ISO change what this PC reports in the handshake, SIMMY_NET_ON_END=quit,
// SIMMY_NET_DIR=<folder> (the session folder), SIMMY_NET_TRACE=1 (per-tick log).
#include "simmy/types_gen.hpp"
#include "simmy/perf.hpp"
#include "simmy/platform.hpp"
#include "simmy/detmath.hpp"
#include "simmy/version.hpp"
#include "pc_net.hpp"
#include "pc_settings.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_video.h>
#include <aurora/aurora.h>
#include <aurora/card.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

// The system's sockets last: windows.h (pulled in by winsock2.h) defines macros the headers above
// should not see.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif


namespace simmy {
void det_net_enable(FILE* record, const std::string& recordPath);  // pc_det.cpp: virtual timing, boot ticks, the record
void platform_present_wait_frame();                    // pc_main.cpp: a frame with only the overlay's notice

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

const char* Env(const char* n) {
  const char* v = std::getenv(n);
  return v && *v ? v : nullptr;
}
bool Trace() {
  static const bool on = Env("SIMMY_NET_TRACE") != nullptr;
  return on;
}
double MsSince(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }

constexpr int kDefaultPort = 27450;
constexpr unsigned kMagic = 0x504E4D53u;  // "SMNP"
constexpr unsigned kProtocol = 1;

// ---- options ([net] in simmy.ini, then the command line) ----------------------------------------
struct Options {
  int mode = -1;  // 0 off, 1 host, 2 join; -1 not given
  std::string join;
  int port = -1, delay = -1, hashEvery = -1, timeoutSec = -1, onEnd = -1, skipIntro = -1;
};
Options g_ini, g_cli;

bool ParseBool(const std::string& v, bool& out) {
  std::string l;
  for (char c : v) l += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (l == "1" || l == "yes" || l == "on" || l == "true") return out = true, true;
  if (l == "0" || l == "no" || l == "off" || l == "false" || l.empty()) return out = false, true;
  return false;
}
std::string Trimmed(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t\r\n\"");
  if (a == std::string::npos) return "";
  const size_t b = s.find_last_not_of(" \t\r\n\"");
  return s.substr(a, b - a + 1);
}
bool ParseInt(const std::string& v, int lo, int hi, int& out) {
  char* end = nullptr;
  const long n = std::strtol(v.c_str(), &end, 10);
  if (v.empty() || *end || n < lo || n > hi) return false;
  out = static_cast<int>(n);
  return true;
}

// ---- byte buffers (little-endian) ----------------------------------------------------------------
struct Writer {
  std::vector<unsigned char> b;
  void u8(unsigned v) { b.push_back(static_cast<unsigned char>(v)); }
  void u16(unsigned v) { u8(v & 0xff), u8((v >> 8) & 0xff); }
  void u32(unsigned v) { u16(v & 0xffff), u16(v >> 16); }
  void u64(unsigned long long v) { u32(static_cast<unsigned>(v)), u32(static_cast<unsigned>(v >> 32)); }
  void f32(float f) {
    unsigned w;
    std::memcpy(&w, &f, 4);
    u32(w);
  }
  void str(const std::string& s) {
    u32(static_cast<unsigned>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
  }
  void bytes(const std::vector<unsigned char>& v) {
    u32(static_cast<unsigned>(v.size()));
    b.insert(b.end(), v.begin(), v.end());
  }
};
struct Reader {
  const unsigned char* p;
  size_t n, i = 0;
  bool bad = false;
  Reader(const std::vector<unsigned char>& v, size_t from) : p(v.data()), n(v.size()), i(from) {}
  unsigned u8() {
    if (i >= n) return bad = true, 0u;
    return p[i++];
  }
  unsigned u16() { const unsigned a = u8(); return a | (u8() << 8); }
  unsigned u32() { const unsigned a = u16(); return a | (u16() << 16); }
  unsigned long long u64() { const unsigned long long a = u32(); return a | (static_cast<unsigned long long>(u32()) << 32); }
  float f32() {
    const unsigned w = u32();
    float f;
    std::memcpy(&f, &w, 4);
    return f;
  }
  std::string str() {
    const unsigned len = u32();
    if (bad || len > n - i) return bad = true, std::string();
    std::string s(reinterpret_cast<const char*>(p + i), len);
    i += len;
    return s;
  }
  std::vector<unsigned char> bytes() {
    const unsigned len = u32();
    if (bad || len > n - i) return bad = true, std::vector<unsigned char>();
    std::vector<unsigned char> v(p + i, p + i + len);
    i += len;
    return v;
  }
};

// ---- the transport: a byte stream (TCP now; UDP with its own ordering or a relay can replace it) ----
class Stream {
 public:
  virtual ~Stream() = default;
  // Bytes written (0: try later), -1 on error.
  virtual int Write(const unsigned char* data, int size) = 0;
  // Bytes read (0: nothing yet), -1: closed or failed.
  virtual int Read(unsigned char* data, int size) = 0;
  virtual std::string Error() const = 0;
};

#ifdef _WIN32
using Socket = SOCKET;
const Socket kNoSocket = INVALID_SOCKET;
int LastSocketError() { return WSAGetLastError(); }
bool WouldBlock(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY; }
void CloseSocket(Socket s) { closesocket(s); }
#else
using Socket = int;
const Socket kNoSocket = -1;
int LastSocketError() { return errno; }
bool WouldBlock(int e) { return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS || e == EALREADY || e == EINTR; }
void CloseSocket(Socket s) { close(s); }
#endif

std::string SocketErrorText(int e) {
#ifdef _WIN32
  char buf[256] = {};
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(e), 0, buf,
                 sizeof buf, nullptr);
  std::string s = buf;
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s + " (" + std::to_string(e) + ")";
#else
  return std::string(std::strerror(e)) + " (" + std::to_string(e) + ")";
#endif
}

bool SetNonBlocking(Socket s) {
#ifdef _WIN32
  u_long on = 1;
  return ioctlsocket(s, FIONBIO, &on) == 0;
#else
  const int flags = fcntl(s, F_GETFL, 0);
  return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

void SetNoDelay(Socket s) {
  int one = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), static_cast<socklen_t>(sizeof one));
}

bool g_socketsUp = false;
bool SocketsInit() {
  if (g_socketsUp) return true;
#ifdef _WIN32
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
#endif
  g_socketsUp = true;
  return true;
}
void SocketsShutdown() {
  if (!g_socketsUp) return;
#ifdef _WIN32
  WSACleanup();
#endif
  g_socketsUp = false;
}

class TcpStream final : public Stream {
 public:
  explicit TcpStream(Socket s) : m_s(s) {}
  ~TcpStream() override {
    if (m_s != kNoSocket) CloseSocket(m_s);
  }
  int Write(const unsigned char* data, int size) override {
    if (m_s == kNoSocket) return -1;
#ifdef _WIN32
    const int n = send(m_s, reinterpret_cast<const char*>(data), size, 0);
#else
#ifdef MSG_NOSIGNAL
    const int n = static_cast<int>(send(m_s, data, static_cast<size_t>(size), MSG_NOSIGNAL));  // no SIGPIPE
#else
    const int n = static_cast<int>(send(m_s, data, static_cast<size_t>(size), 0));
#endif
#endif
    if (n >= 0) return n;
    const int e = LastSocketError();
    if (WouldBlock(e)) return 0;
    m_error = SocketErrorText(e);
    return -1;
  }
  int Read(unsigned char* data, int size) override {
    if (m_s == kNoSocket) return -1;
#ifdef _WIN32
    const int n = recv(m_s, reinterpret_cast<char*>(data), size, 0);
#else
    const int n = static_cast<int>(recv(m_s, data, static_cast<size_t>(size), 0));
#endif
    if (n > 0) return n;
    if (n == 0) {
      m_error = "the other PC closed the connection";
      return -1;
    }
    const int e = LastSocketError();
    if (WouldBlock(e)) return 0;
    m_error = SocketErrorText(e);
    return -1;
  }
  std::string Error() const override { return m_error; }

 private:
  Socket m_s;
  std::string m_error;
};

// ---- messages: [u32 size][u8 type][payload], with the test lag on the way out --------------------
enum Msg : unsigned char {
  kHello = 1,    // u32 magic, u32 protocol, u8 slot, str build, u64 math, u64 data, str data description, str game id
  kRefuse = 2,   // str reason
  kSession = 3,  // str settings ("key=value" lines), u32 files, (str name, bytes data) per file
  kReady = 4,    // (client: the session is set up)
  kStart = 5,    // (host: go)
  kInput = 6,    // u32 tick, the input (see PutInput)
  kHash = 7,     // u32 tick, u64 total, u64 x 6 parts
  kBye = 8,      // str reason
  kDesync = 9,   // u32 tick
  kPing = 10,    // u64 sender's time (us)
  kPong = 11,    // u64 the ping's time
};

class Channel {
 public:
  explicit Channel(std::unique_ptr<Stream> s) : m_io(std::move(s)) {
    if (const char* v = Env("SIMMY_NET_LAG")) m_lag = std::max(0, std::atoi(v));
    if (const char* v = Env("SIMMY_NET_JITTER")) m_jitter = std::max(0, std::atoi(v));
    m_rng.seed(static_cast<unsigned>(Clock::now().time_since_epoch().count()));
  }
  void Send(Msg type, const Writer& payload) {
    std::vector<unsigned char> m;
    const unsigned size = static_cast<unsigned>(payload.b.size() + 1);
    m.reserve(size + 4);
    for (int k = 0; k < 4; ++k) m.push_back(static_cast<unsigned char>(size >> (8 * k)));
    m.push_back(type);
    m.insert(m.end(), payload.b.begin(), payload.b.end());
    m_sent += m.size();
    if (m_lag == 0 && m_jitter == 0) {
      m_out.insert(m_out.end(), m.begin(), m.end());
      return;
    }
    // Test lag: each message leaves after lag + 0..jitter ms, never before the one sent earlier.
    Clock::time_point due = Clock::now() + std::chrono::milliseconds(m_lag);
    if (m_jitter > 0) due += std::chrono::milliseconds(m_rng() % static_cast<unsigned>(m_jitter + 1));
    if (!m_held.empty() && due < m_held.back().first) due = m_held.back().first;
    m_held.emplace_back(due, std::move(m));
  }
  // Writes what is due and reads what has come. False once the connection is gone.
  bool Pump() {
    if (m_dead) return false;
    const Clock::time_point now = Clock::now();
    while (!m_held.empty() && m_held.front().first <= now) {
      m_out.insert(m_out.end(), m_held.front().second.begin(), m_held.front().second.end());
      m_held.pop_front();
    }
    while (m_outPos < m_out.size()) {
      const int chunk = static_cast<int>(std::min<size_t>(m_out.size() - m_outPos, 1 << 16));
      const int n = m_io->Write(m_out.data() + m_outPos, chunk);
      if (n < 0) return Die();
      if (n == 0) break;
      m_outPos += static_cast<size_t>(n);
    }
    if (m_outPos == m_out.size()) m_out.clear(), m_outPos = 0;
    else if (m_outPos > (1u << 20)) m_out.erase(m_out.begin(), m_out.begin() + static_cast<std::ptrdiff_t>(m_outPos)), m_outPos = 0;
    unsigned char buf[16384];
    for (;;) {
      const int n = m_io->Read(buf, sizeof buf);
      if (n < 0) return Die();
      if (n == 0) break;
      m_in.insert(m_in.end(), buf, buf + n);
      m_received += static_cast<size_t>(n);
      m_lastRecv = Clock::now();
    }
    return true;
  }
  // The next complete message (type in [0], payload after it).
  bool Next(std::vector<unsigned char>& msg) {
    if (m_in.size() - m_inPos < 4) return Compact(), false;
    const unsigned char* p = m_in.data() + m_inPos;
    const unsigned size = p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<unsigned>(p[3]) << 24);
    if (size == 0 || size > (64u << 20)) {
      m_error = "a malformed message came";
      m_dead = true;
      return false;
    }
    if (m_in.size() - m_inPos < 4 + static_cast<size_t>(size)) return Compact(), false;
    msg.assign(p + 4, p + 4 + size);
    m_inPos += 4 + size;
    return true;
  }
  // Sends what is queued (lag included), for up to `ms` (before closing).
  void Flush(int ms) {
    const Clock::time_point until = Clock::now() + std::chrono::milliseconds(ms);
    while (Clock::now() < until && (!m_held.empty() || m_outPos < m_out.size())) {
      if (!Pump()) return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  bool Dead() const { return m_dead; }
  std::string Error() const { return m_error.empty() ? m_io->Error() : m_error; }
  Clock::time_point LastReceive() const { return m_lastRecv; }
  size_t Sent() const { return m_sent; }
  size_t Received() const { return m_received; }

 private:
  bool Die() {
    m_dead = true;
    if (m_error.empty()) m_error = m_io->Error();
    return false;
  }
  void Compact() {
    if (m_inPos > (1u << 16) || m_inPos == m_in.size()) {
      m_in.erase(m_in.begin(), m_in.begin() + static_cast<std::ptrdiff_t>(m_inPos));
      m_inPos = 0;
    }
  }
  std::unique_ptr<Stream> m_io;
  std::vector<unsigned char> m_out, m_in;
  size_t m_outPos = 0, m_inPos = 0;
  std::deque<std::pair<Clock::time_point, std::vector<unsigned char>>> m_held;
  int m_lag = 0, m_jitter = 0;
  std::mt19937 m_rng;
  bool m_dead = false;
  std::string m_error;
  Clock::time_point m_lastRecv = Clock::now();
  size_t m_sent = 0, m_received = 0;
};

// ---- per-tick input ------------------------------------------------------------------------------
struct Input {
  PADStatus pad{};
  KbmTick kbm{};
};
void PutInput(Writer& w, const Input& in) {
  const PADStatus& p = in.pad;
  w.u8(static_cast<unsigned char>(p.err));
  w.u16(p.button);
  w.u8(static_cast<unsigned char>(p.stickX)), w.u8(static_cast<unsigned char>(p.stickY));
  w.u8(static_cast<unsigned char>(p.substickX)), w.u8(static_cast<unsigned char>(p.substickY));
  w.u8(p.triggerLeft), w.u8(p.triggerRight), w.u8(p.analogA), w.u8(p.analogB);
  w.u32(static_cast<unsigned>(p.extButton));
  w.u32(in.kbm.flags);
  w.f32(in.kbm.cx), w.f32(in.kbm.cy), w.f32(in.kbm.rot), w.f32(in.kbm.zoom), w.f32(in.kbm.panX), w.f32(in.kbm.panY);
}
Input GetInput(Reader& r) {
  Input in;
  PADStatus& p = in.pad;
  p.err = static_cast<s8>(r.u8());
  p.button = static_cast<u16>(r.u16());
  p.stickX = static_cast<s8>(r.u8()), p.stickY = static_cast<s8>(r.u8());
  p.substickX = static_cast<s8>(r.u8()), p.substickY = static_cast<s8>(r.u8());
  p.triggerLeft = static_cast<u8>(r.u8()), p.triggerRight = static_cast<u8>(r.u8());
  p.analogA = static_cast<u8>(r.u8()), p.analogB = static_cast<u8>(r.u8());
  p.extButton = r.u32();
  in.kbm.flags = r.u32();
  in.kbm.cx = r.f32(), in.kbm.cy = r.f32(), in.kbm.rot = r.f32(), in.kbm.zoom = r.f32(), in.kbm.panX = r.f32(),
  in.kbm.panY = r.f32();
  return in;
}
// A connected pad with nothing pressed.
Input Neutral() {
  Input in;
  in.pad.err = PAD_ERR_NONE;
  return in;
}

constexpr int kRing = 1024;  // ticks of input kept (the partner is at most `delay` ticks ahead)
struct Slot {
  int tick = -1;
  bool have[2] = {false, false};
  Input in[2];
};

struct Check {
  int tick = 0;
  unsigned long long total = 0, part[6] = {};
  std::string dump;
};

enum class End { None, Left, Lost, Timeout, Desync, LocalQuit };

struct Session {
  bool set = false, active = false;
  int slot = 0;  // 0 host, 1 client
  int delay = 2, hashEvery = 30, timeoutMs = 30000;
  bool quitOnEnd = false, skipIntro = true;
  std::unique_ptr<Channel> ch;
  Slot ring[kRing];
  Input local = Neutral();  // this PC's input sampled in the tick running now
  std::string dir;          // the session folder
  std::string clientCardDir;
  std::deque<Check> mine;   // this PC's last checks (with their dumps)
  std::map<int, Check> theirs;  // the partner's checks not compared yet
  int lastMatched = 0;
  int lastTick = 0;
  End ended = End::None;
  std::string notice;
  Clock::time_point noticeUntil;
  bool noticeForever = false;
  // statistics
  double waitedMs = 0, longestWaitMs = 0;
  int waits = 0, longWaits = 0;
  double rttMs = -1;
  int desyncTick = 0;
  std::vector<std::pair<int, int>> stalls;  // SIMMY_NET_STALL
  int desyncAt = 0;                          // SIMMY_NET_DESYNC
};
Session& N() {
  static Session s;
  return s;
}

const char* PlayerName(int slot) { return slot == 0 ? "player 1 (host)" : "player 2"; }
const char* Role(int slot) { return slot == 0 ? "host" : "client"; }

void SetNotice(const std::string& text, double seconds) {
  Session& s = N();
  s.notice = text;
  s.noticeForever = seconds <= 0.0;
  s.noticeUntil = Clock::now() + std::chrono::milliseconds(static_cast<long long>(seconds * 1000.0));
}

void SetTitle(const std::string& text) {
  if (platform_headless()) return;
  if (SDL_Window* w = aurora_get_window()) SDL_SetWindowTitle(w, (std::string(SIMMY_PORT_VERSION) + " - " + text).c_str());
}

void ShowError(const std::string& title, const std::string& text) {
  std::fprintf(stderr, "[net] %s: %s\n", title.c_str(), text.c_str());
  if (!platform_headless() && !dev_options().offscreen)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, ("SIMMY - " + title).c_str(), text.c_str(), aurora_get_window());
}

// ---- what both PCs must have in common -------------------------------------------------------------
unsigned long long Fnv(unsigned long long h, const unsigned char* p, size_t n) {
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}
unsigned long long FnvStr(unsigned long long h, const std::string& s) {
  return Fnv(h, reinterpret_cast<const unsigned char*>(s.data()), s.size() + 1);
}

// The game data's identity: the contents of the index (every resource's place and size), the
// neighbourhood file, the behaviour trees and the small archives, and every archive's size. The same
// for a disc image and an extracted folder of the same disc.
unsigned long long DataIdentity() {
  unsigned long long h = 1469598103934665603ull;
  static const char* const kHashed[] = {"data/index.ind", "data/index1.ind", "default.ngh",      "data/edithtre.arc",
                                        "data/binaries.arc", "data/characte.arc", "data/levels.arc", "data/quickdat.arc",
                                        "data/soundeve.arc", "data/emitters.arc"};
  for (const char* name : kHashed) {
    std::vector<unsigned char> bytes;
    h = FnvStr(h, name);
    if (data_read_file(name, bytes)) h = Fnv(h, bytes.data(), bytes.size());
    else h = FnvStr(h, "(missing)");
  }
  static const char* const kSized[] = {"data/ambients.arc", "data/animatio.arc", "data/audiostr.arc", "data/datasets.arc",
                                       "data/effectsa.arc", "data/effectse.arc", "data/effectss.arc", "data/flashes.arc",
                                       "data/fonts.arc",    "data/models.arc",   "data/movies.arc",   "data/samples.arc",
                                       "data/shaders.arc",  "data/soundtra.arc", "data/textures.arc"};
  for (const char* name : kSized) {
    h = FnvStr(h, name);
    DataFile* f = data_open(name);
    const unsigned long long size = f ? data_size(f) : ~0ull;
    if (f) data_close(f);
    for (int k = 0; k < 8; ++k) h = (h ^ ((size >> (8 * k)) & 0xff)) * 1099511628211ull;
  }
  return h;
}

std::string GameId() {
  const std::string d = data_source_description();  // "disc image G4ZE69" / "folder ..."
  return d.rfind("disc image ", 0) == 0 ? d.substr(11) : std::string();
}

fs::path U8Path(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }
std::string PathU8(const fs::path& p) {
  const std::u8string u = p.u8string();
  return std::string(u.begin(), u.end());
}
// fopen of a path (UTF-8 on every system: the user folder may have any name on Windows).
FILE* OpenUtf8(const fs::path& p, const char* mode) {
#ifdef _WIN32
  std::wstring wmode;
  for (const char* m = mode; *m; ++m) wmode += static_cast<wchar_t>(*m);
  return _wfopen(p.c_str(), wmode.c_str());
#else
  return std::fopen(p.c_str(), mode);
#endif
}

std::string Hex(unsigned long long v) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "%016llx", v);
  return buf;
}

// SIMMY_* development switches that can change the game (everything but this PC's own input, output,
// tracing and display switches): the host sends its own, the client warns about differences.
bool LocalOnlySwitch(const std::string& name) {
  static const char* const kLocal[] = {
      "SIMMY_NET",        "SIMMY_PAD_",     "SIMMY_SOAK",       "SIMMY_DET_HASH",  "SIMMY_DET_RECORD", "SIMMY_DET_REPLAY",
      "SIMMY_DET_DUMP",   "SIMMY_DET_LINES", "SIMMY_DET_REF",   "SIMMY_DET_JITTER", "SIMMY_DET_NOPACE", "SIMMY_DET_QUIT",
      "SIMMY_DET_RANDLOG", "SIMMY_GFX",     "SIMMY_INI",        "SIMMY_KBM",       "SIMMY_SHOTS",      "SIMMY_FRAME",
      "SIMMY_AUDIO",      "SIMMY_BACKEND",  "SIMMY_SPLIT",      "SIMMY_OVERLAY",   "SIMMY_LSAN",       "SIMMY_VIRTUAL_PADS",
      "SIMMY_LOG",        "SIMMY_DATA",     "SIMMY_IO_STATS",   "SIMMY_GPU",       "SIMMY_GX_",        "SIMMY_WIN2",
      "SIMMY_FAKE_PADS",  "SIMMY_DET",      "SIMMY_SKIP_INTRO", "SIMMY_PORTRAIT",  "SIMMY_WAV",
      "SIMMY_PERF",       "SIMMY_DISC_CACHE", "SIMMY_DRAWS_BLOCKING"};  // (SIMMY_FRAME: _STATS, _PACING)
  for (const char* p : kLocal)
    if (name.rfind(p, 0) == 0 && !(std::strcmp(p, "SIMMY_DET") == 0 && name.rfind("SIMMY_DET_SPEED", 0) == 0) &&
        !(std::strcmp(p, "SIMMY_DET") == 0 && name.rfind("SIMMY_DET_SEED", 0) == 0))
      return true;
  return name.find("TRACE") != std::string::npos || name.find("DUMP") != std::string::npos;
}
}  // namespace
}  // namespace simmy

#ifndef _WIN32
extern char** environ;
#endif

namespace simmy {
namespace {
std::map<std::string, std::string> GameSwitches() {
  std::map<std::string, std::string> out;
#ifdef _WIN32
  char** env = _environ;
#else
  char** env = environ;
#endif
  for (char** e = env; e && *e; ++e) {
    const std::string kv = *e;
    const size_t eq = kv.find('=');
    if (eq == std::string::npos || kv.rfind("SIMMY_", 0) != 0) continue;
    const std::string name = kv.substr(0, eq);
    if (!LocalOnlySwitch(name)) out[name] = kv.substr(eq + 1);
  }
  return out;
}

std::string SessionFolder() {
  if (const char* d = Env("SIMMY_NET_DIR")) return d;
  if (!platform_headless()) {
    if (char* pref = SDL_GetPrefPath(nullptr, "SIMMY")) {
      std::string p = std::string(pref) + "net_session";
      SDL_free(pref);
      return p;
    }
  }
  return "net_session";
}


// The host's memory card (slot A's GCI folder) as files.
std::vector<std::pair<std::string, std::vector<unsigned char>>> CardFiles(std::string& folder) {
  std::vector<std::pair<std::string, std::vector<unsigned char>>> out;
  char buf[4096] = {};
  aurora_card_get_path("G4ZE", AURORA_CARD_GCI_DIRECTORY, 0, buf, sizeof buf);
  folder = buf;
  std::error_code ec;
  if (folder.empty() || !fs::is_directory(U8Path(folder), ec)) return out;
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(U8Path(folder), ec))
    if (e.is_regular_file(ec)) files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for (const fs::path& f : files) {
    std::vector<unsigned char> bytes;
    if (FILE* in = OpenUtf8(f, "rb")) {
      unsigned char chunk[65536];
      size_t n;
      while ((n = std::fread(chunk, 1, sizeof chunk, in)) > 0) bytes.insert(bytes.end(), chunk, chunk + n);
      std::fclose(in);
      out.emplace_back(PathU8(f.filename()), std::move(bytes));
    }
  }
  return out;
}

// ---- the connection and the handshake ---------------------------------------------------------------
// Pumps the window while waiting (the frame shows the notice); false: the player closed the game.
bool KeepWindowAlive(Clock::time_point& lastFrame) {
  if (platform_headless()) return true;
  if (!platform_pump_events()) return false;
  if (MsSince(lastFrame) >= 100.0) {
    platform_present_wait_frame();
    lastFrame = Clock::now();
  }
  return true;
}

std::string LanAddress() {
  // The address of the interface that would reach the internet (no packet is sent).
  Socket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == kNoSocket) return "";
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(53);
  inet_pton(AF_INET, "8.8.8.8", &to.sin_addr);
  std::string out;
  if (connect(s, reinterpret_cast<sockaddr*>(&to), static_cast<socklen_t>(sizeof to)) == 0) {
    sockaddr_in me{};
    socklen_t len = static_cast<socklen_t>(sizeof me);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&me), &len) == 0) {
      char buf[64] = {};
      inet_ntop(AF_INET, &me.sin_addr, buf, sizeof buf);
      out = buf;
    }
  }
  CloseSocket(s);
  return out;
}

std::unique_ptr<Stream> Listen(int port, std::string& error) {
  Socket ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ls == kNoSocket) return error = "cannot make a socket: " + SocketErrorText(LastSocketError()), nullptr;
  int one = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), static_cast<socklen_t>(sizeof one));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<unsigned short>(port));
  if (bind(ls, reinterpret_cast<sockaddr*>(&addr), static_cast<socklen_t>(sizeof addr)) != 0 || listen(ls, 1) != 0 || !SetNonBlocking(ls)) {
    error = "cannot listen on port " + std::to_string(port) + ": " + SocketErrorText(LastSocketError()) +
            " (is another SIMMY hosting on it?)";
    CloseSocket(ls);
    return nullptr;
  }
  const std::string lan = LanAddress();
  std::fprintf(stderr, "[net] hosting on port %d (this PC's LAN address: %s); waiting for a player to join\n", port,
               lan.empty() ? "unknown" : lan.c_str());
  const std::string wait = "Waiting for player 2 to join: port " + std::to_string(port) +
                           (lan.empty() ? std::string() : ", this PC's address " + lan);
  SetNotice(wait, 0);
  SetTitle(wait);
  Clock::time_point frame = Clock::now() - std::chrono::seconds(1);
  for (;;) {
    sockaddr_in from{};
    socklen_t len = static_cast<socklen_t>(sizeof from);
    const Socket s = accept(ls, reinterpret_cast<sockaddr*>(&from), &len);
    if (s != kNoSocket) {
      char buf[64] = {};
      inet_ntop(AF_INET, &from.sin_addr, buf, sizeof buf);
      std::fprintf(stderr, "[net] %s connected\n", buf);
      CloseSocket(ls);
      SetNonBlocking(s);
      SetNoDelay(s);
      return std::make_unique<TcpStream>(s);
    }
    const int e = LastSocketError();
    if (!WouldBlock(e)) {
      error = "accept failed: " + SocketErrorText(e);
      CloseSocket(ls);
      return nullptr;
    }
    if (!KeepWindowAlive(frame)) {
      error = "cancelled";
      CloseSocket(ls);
      return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

std::unique_ptr<Stream> Connect(const std::string& target, int defaultPort, int timeoutSec, std::string& error) {
  std::string host = target;
  int port = defaultPort;
  const size_t colon = target.rfind(':');
  if (colon != std::string::npos && target.find(':') == colon) {
    host = target.substr(0, colon);
    if (!ParseInt(target.substr(colon + 1), 1, 65535, port)) return error = "bad port in \"" + target + "\"", nullptr;
  }
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo* found = nullptr;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found)
    return error = "cannot find the address \"" + host + "\"", nullptr;
  sockaddr_in addr{};
  std::memcpy(&addr, found->ai_addr, std::min<size_t>(sizeof addr, found->ai_addrlen));
  freeaddrinfo(found);
  char shown[64] = {};
  inet_ntop(AF_INET, &addr.sin_addr, shown, sizeof shown);
  const std::string text = "Connecting to " + std::string(shown) + ":" + std::to_string(port) + " ...";
  std::fprintf(stderr, "[net] %s\n", text.c_str());
  SetNotice(text, 0);
  SetTitle(text);
  const Clock::time_point start = Clock::now();
  Clock::time_point frame = start - std::chrono::seconds(1);
  std::string last;
  while (MsSince(start) < timeoutSec * 1000.0) {
    Socket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kNoSocket) return error = "cannot make a socket: " + SocketErrorText(LastSocketError()), nullptr;
    SetNonBlocking(s);
    int rc = connect(s, reinterpret_cast<sockaddr*>(&addr), static_cast<socklen_t>(sizeof addr));
    bool ok = rc == 0;
    if (!ok && WouldBlock(LastSocketError())) {
      // Wait up to a second for this attempt.
      const Clock::time_point attempt = Clock::now();
      while (!ok && MsSince(attempt) < 1000.0) {
        fd_set wr, ex;
        FD_ZERO(&wr);
        FD_ZERO(&ex);
        FD_SET(s, &wr);
        FD_SET(s, &ex);
        timeval tv{0, 20000};
        rc = select(static_cast<int>(s) + 1, nullptr, &wr, &ex, &tv);
        if (rc > 0) {
          int soErr = 0;
          socklen_t len = static_cast<socklen_t>(sizeof soErr);
          getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soErr), &len);
          if (soErr == 0 && FD_ISSET(s, &wr)) ok = true;
          else {
            last = SocketErrorText(soErr);
            break;
          }
        } else if (rc < 0) {
          last = SocketErrorText(LastSocketError());
          break;
        }
        if (!KeepWindowAlive(frame)) {
          CloseSocket(s);
          return error = "cancelled", nullptr;
        }
      }
    } else if (!ok) {
      last = SocketErrorText(LastSocketError());
    }
    if (ok) {
      SetNoDelay(s);
      std::fprintf(stderr, "[net] connected to %s:%d\n", shown, port);
      return std::make_unique<TcpStream>(s);
    }
    CloseSocket(s);
    // Not there yet (the host starts later, or a firewall): try again.
    const Clock::time_point pause = Clock::now();
    while (MsSince(pause) < 500.0) {
      if (!KeepWindowAlive(frame)) return error = "cancelled", nullptr;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  error = "no answer from " + std::string(shown) + ":" + std::to_string(port) + " in " + std::to_string(timeoutSec) +
          " s" + (last.empty() ? std::string() : " (" + last + ")") +
          ". Is the host's game started with --host, the address right, and the port allowed through its firewall?";
  return nullptr;
}

// Waits for the next message of `type` (other types are an error here). False: refused, lost, cancelled.
bool Expect(Channel& ch, Msg type, std::vector<unsigned char>& msg, int timeoutSec, std::string& error) {
  const Clock::time_point start = Clock::now();
  Clock::time_point frame = start - std::chrono::seconds(1);
  for (;;) {
    const bool alive = ch.Pump();
    if (ch.Next(msg)) {
      if (msg[0] == kRefuse) {
        Reader r(msg, 1);
        return error = "the other PC refused: " + r.str(), false;
      }
      if (msg[0] == kBye) {
        Reader r(msg, 1);
        return error = "the other PC left: " + r.str(), false;
      }
      if (msg[0] == type) return true;
      return error = "unexpected message " + std::to_string(msg[0]), false;
    }
    if (!alive || ch.Dead()) return error = "the connection was lost: " + ch.Error(), false;
    if (MsSince(start) > timeoutSec * 1000.0) return error = "the other PC did not answer in time", false;
    if (!KeepWindowAlive(frame)) return error = "cancelled", false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

struct Hello {
  unsigned magic = 0, protocol = 0, slot = 0;
  std::string build, dataDesc, gameId;
  unsigned long long math = 0, data = 0;
};

Hello LocalHello(int slot) {
  Hello h;
  h.magic = kMagic, h.protocol = kProtocol, h.slot = static_cast<unsigned>(slot);
  h.build = Env("SIMMY_NET_FAKE_BUILD") ? Env("SIMMY_NET_FAKE_BUILD") : SIMMY_PORT_VERSION;
  const Clock::time_point t0 = Clock::now();
  h.math = dm::mathcheck_hash();
  if (Env("SIMMY_NET_FAKE_MATH")) h.math ^= 0x5a5a5a5a5a5a5a5aull;
  h.data = DataIdentity();
  if (Env("SIMMY_NET_FAKE_ISO")) h.data ^= 0x1234567812345678ull;
  h.dataDesc = data_source_description();
  h.gameId = GameId();
  std::fprintf(stderr, "[net] this PC: %s, mathcheck %s, game data %s (%s), %.0f ms\n", h.build.c_str(), Hex(h.math).c_str(),
               Hex(h.data).c_str(), h.dataDesc.c_str(), MsSince(t0));
  return h;
}

void SendHello(Channel& ch, const Hello& h) {
  Writer w;
  w.u32(h.magic), w.u32(h.protocol), w.u8(h.slot);
  w.str(h.build), w.u64(h.math), w.u64(h.data), w.str(h.dataDesc), w.str(h.gameId);
  ch.Send(kHello, w);
}

// Both PCs check the same things and so refuse together (each also sends why).
std::string Compare(const Hello& mine, const Hello& theirs) {
  if (theirs.magic != kMagic) return "the other program is not SIMMY";
  if (theirs.protocol != kProtocol)
    return "the two PCs speak different network play versions (" + std::to_string(mine.protocol) + " and " +
           std::to_string(theirs.protocol) + "): both players need the same SIMMY build";
  if (theirs.slot == mine.slot)
    return mine.slot == 0 ? "both PCs are hosting: one must host (--host), the other join (--join <address>)"
                          : "both PCs are joining";
  std::string why;
  if (theirs.build != mine.build)
    why += "Different SIMMY builds: this PC has \"" + mine.build + "\", the other \"" + theirs.build +
           "\". Both players need the same build.\n";
  if (theirs.math != mine.math)
    why += "The deterministic math differs (simmy --mathcheck: " + Hex(mine.math) + " here, " + Hex(theirs.math) +
           " there): this build cannot keep the two PCs in step.\n";
  if (!mine.gameId.empty() && !theirs.gameId.empty() && theirs.gameId != mine.gameId)
    why += "Different discs: " + mine.gameId + " here, " + theirs.gameId + " there.\n";
  if (theirs.data != mine.data)
    why += "The game data differs (" + Hex(mine.data) + " here from the " + mine.dataDesc + ", " + Hex(theirs.data) +
           " there from the " + theirs.dataDesc + "): both need the same disc image of The Sims 2 (USA).\n";
  if (!why.empty() && why.back() == '\n') why.pop_back();
  return why;
}

bool Handshake(Session& s, std::string& error) {
  Channel& ch = *s.ch;
  const Hello mine = LocalHello(s.slot);
  SendHello(ch, mine);
  std::vector<unsigned char> msg;
  if (!Expect(ch, kHello, msg, 60, error)) return false;
  Reader r(msg, 1);
  Hello theirs;
  theirs.magic = r.u32(), theirs.protocol = r.u32(), theirs.slot = r.u8();
  theirs.build = r.str(), theirs.math = r.u64(), theirs.data = r.u64(), theirs.dataDesc = r.str(), theirs.gameId = r.str();
  std::fprintf(stderr, "[net] the other PC: %s, mathcheck %s, game data %s (%s)\n", theirs.build.c_str(),
               Hex(theirs.math).c_str(), Hex(theirs.data).c_str(), theirs.dataDesc.c_str());
  const std::string why = Compare(mine, theirs);
  if (!why.empty()) {
    Writer w;
    w.str(why);
    ch.Send(kRefuse, w);
    ch.Flush(500);
    error = why;
    return false;
  }
  const std::string folder = s.dir;
  std::error_code ec;
  fs::create_directories(U8Path(folder), ec);
  if (s.slot == 0) {
    // The host: what the session runs with.
    Writer w;
    std::string settings;
    settings += "seed=" + std::to_string(sim_rand_state()) + "\n";
    settings += "fire_code_limit=" + std::string(graphics_settings().fireCodeLimit ? "1" : "0") + "\n";
    settings += "delay=" + std::to_string(s.delay) + "\n";
    settings += "hash_every=" + std::to_string(s.hashEvery) + "\n";
    settings += "skip_intro=" + std::string(s.skipIntro ? "1" : "0") + "\n";
    for (const auto& kv : GameSwitches()) settings += "env:" + kv.first + "=" + kv.second + "\n";
    w.str(settings);
    std::string cardFolder;
    const auto files = CardFiles(cardFolder);
    w.u32(static_cast<unsigned>(files.size()));
    size_t bytes = 0;
    for (const auto& f : files) w.str(f.first), w.bytes(f.second), bytes += f.second.size();
    std::fprintf(stderr, "[net] sending the session: %zu card file(s), %zu bytes, from %s\n", files.size(), bytes,
                 cardFolder.empty() ? "(no card folder)" : cardFolder.c_str());
    ch.Send(kSession, w);
    if (!Expect(ch, kReady, msg, 120, error)) return false;
    ch.Send(kStart, Writer());
    ch.Pump();
    return true;
  }
  // The client: take the host's settings and card.
  if (!Expect(ch, kSession, msg, 120, error)) return false;
  Reader rs(msg, 1);
  const std::string settings = rs.str();
  std::map<std::string, std::string> hostEnv;
  size_t pos = 0;
  while (pos < settings.size()) {
    size_t nl = settings.find('\n', pos);
    if (nl == std::string::npos) nl = settings.size();
    const std::string line = settings.substr(pos, nl - pos);
    pos = nl + 1;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
    if (key == "seed") set_sim_rand_state(static_cast<unsigned>(std::strtoul(value.c_str(), nullptr, 10)));
    else if (key == "fire_code_limit") graphics_settings().fireCodeLimit = value == "1";
    else if (key == "delay") s.delay = std::atoi(value.c_str());
    else if (key == "hash_every") s.hashEvery = std::atoi(value.c_str());
    else if (key == "skip_intro") s.skipIntro = value == "1";
    else if (key.rfind("env:", 0) == 0) hostEnv[key.substr(4)] = value;
    std::fprintf(stderr, "[net] session: %s\n", line.c_str());
  }
  const auto mineEnv = GameSwitches();
  for (const auto& kv : hostEnv)
    if (!mineEnv.count(kv.first) || mineEnv.at(kv.first) != kv.second)
      std::fprintf(stderr, "[net] WARNING: the host has %s=%s, this PC %s: the game may go out of step\n", kv.first.c_str(),
                   kv.second.c_str(), mineEnv.count(kv.first) ? mineEnv.at(kv.first).c_str() : "(not set)");
  for (const auto& kv : mineEnv)
    if (!hostEnv.count(kv.first))
      std::fprintf(stderr, "[net] WARNING: this PC has %s=%s, the host not: the game may go out of step\n", kv.first.c_str(),
                   kv.second.c_str());
  // The card: <session>/USA/Card A, emptied first.
  const fs::path cardDir = U8Path(folder) / "USA" / "Card A";
  fs::create_directories(cardDir, ec);
  for (const auto& e : fs::directory_iterator(cardDir, ec))
    if (e.is_regular_file(ec)) fs::remove(e.path(), ec);
  const unsigned count = rs.u32();
  size_t bytes = 0;
  for (unsigned k = 0; k < count && !rs.bad; ++k) {
    const std::string name = rs.str();
    const std::vector<unsigned char> data = rs.bytes();
    if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name == "." || name == "..") continue;
    if (FILE* out = OpenUtf8(cardDir / U8Path(name), "wb")) {
      std::fwrite(data.data(), 1, data.size(), out);
      std::fclose(out);
      bytes += data.size();
    } else {
      error = "cannot write the session's memory card in " + PathU8(cardDir);
      return false;
    }
  }
  if (rs.bad) return error = "the host's session data was cut short", false;
  aurora_card_set_base_dir(folder.c_str());
  s.clientCardDir = PathU8(cardDir);
  std::fprintf(stderr, "[net] the host's memory card: %u file(s), %zu bytes, in %s (this PC's own card is not used)\n",
               count, bytes, s.clientCardDir.c_str());
  ch.Send(kReady, Writer());
  if (!Expect(ch, kStart, msg, 120, error)) return false;
  return true;
}

// ---- lockstep --------------------------------------------------------------------------------------
Slot& At(int tick) {
  Slot& sl = N().ring[static_cast<unsigned>(tick) % kRing];
  if (sl.tick != tick) {
    sl = Slot{};
    sl.tick = tick;
  }
  return sl;
}

void WriteDesyncFile(Session& s, int tick, const Check* mine, const Check* theirs, const char* how) {
  std::error_code ec;
  fs::create_directories(U8Path(s.dir), ec);
  const std::string name = s.dir + "/desync_" + Role(s.slot) + "_tick" + std::to_string(tick) + ".txt";
  FILE* f = OpenUtf8(U8Path(name), "w");
  if (!f) {
    std::fprintf(stderr, "[net] cannot write %s\n", name.c_str());
    return;
  }
  static const char* const kParts[6] = {"rng", "sim", "obj", "per", "ngh", "inst"};
  std::fprintf(f, "# SIMMY network play desync: %s, %s (%s)\n# tick %d, %s\n", SIMMY_PORT_VERSION, Role(s.slot),
               PlayerName(s.slot), tick, how);
  std::fprintf(f, "# last matching check: tick %d; checks every %d ticks, input delay %d\n", s.lastMatched, s.hashEvery,
               s.delay);
  if (mine && theirs) {
    std::fprintf(f, "# total: this PC %s, the other %s\n", Hex(mine->total).c_str(), Hex(theirs->total).c_str());
    for (int k = 0; k < 6; ++k)
      std::fprintf(f, "#   %-4s this PC %s, the other %s%s\n", kParts[k], Hex(mine->part[k]).c_str(), Hex(theirs->part[k]).c_str(),
                   mine->part[k] != theirs->part[k] ? "   <-- differs" : "");
  }
  // The last matching check's state first (the tick the two still agreed), then the differing tick's.
  for (const Check& c : s.mine)
    if (c.tick == s.lastMatched && !c.dump.empty()) {
      std::fprintf(f, "# ---- tick %d (still the same on both PCs)\n", c.tick);
      std::fputs(c.dump.c_str(), f);
    }
  if (mine) {
    std::fprintf(f, "# ---- tick %d (differs)\n", mine->tick);
    std::fputs(mine->dump.c_str(), f);
  }
  std::fclose(f);
  std::fprintf(stderr, "[net] wrote %s\n", name.c_str());
}

void EndSession(End why, const std::string& text);

void CompareChecks(Session& s, const Check& mine, const Check& theirs) {
  if (mine.total == theirs.total) {
    s.lastMatched = std::max(s.lastMatched, mine.tick);
    if (Trace()) std::fprintf(stderr, "[net] tick %d: in step (%s)\n", mine.tick, Hex(mine.total).c_str());
    return;
  }
  std::string parts;
  static const char* const kParts[6] = {"rng", "sim", "obj", "per", "ngh", "inst"};
  for (int k = 0; k < 6; ++k)
    if (mine.part[k] != theirs.part[k]) parts += (parts.empty() ? "" : ", ") + std::string(kParts[k]);
  std::fprintf(stderr, "[net] DESYNC at tick %d (last in step: %d): differing parts: %s\n", mine.tick, s.lastMatched,
               parts.c_str());
  s.desyncTick = mine.tick;
  WriteDesyncFile(s, mine.tick, &mine, &theirs, ("differing parts: " + parts).c_str());
  Writer w;
  w.u32(static_cast<unsigned>(mine.tick));
  s.ch->Send(kDesync, w);
  EndSession(End::Desync, "The two PCs went out of step at tick " + std::to_string(mine.tick) +
                              ". Network play ended. Please send the files in " + s.dir + " from both PCs (desync_*.txt, net_*.rec).");
}

void Handle(Session& s, const std::vector<unsigned char>& msg) {
  Reader r(msg, 1);
  switch (msg[0]) {
  case kInput: {
    const int tick = static_cast<int>(r.u32());
    const Input in = GetInput(r);
    if (r.bad) break;
    Slot& sl = At(tick);
    sl.in[1 - s.slot] = in;
    sl.have[1 - s.slot] = true;
    break;
  }
  case kHash: {
    Check c;
    c.tick = static_cast<int>(r.u32());
    c.total = r.u64();
    for (unsigned long long& p : c.part) p = r.u64();
    if (r.bad) break;
    const Check* mine = nullptr;
    for (const Check& m : s.mine)
      if (m.tick == c.tick) mine = &m;
    if (mine) CompareChecks(s, *mine, c);
    else s.theirs[c.tick] = c;
    break;
  }
  case kDesync: {
    const int tick = static_cast<int>(r.u32());
    if (s.ended != End::None) break;
    const Check* mine = nullptr;
    for (const Check& m : s.mine)
      if (m.tick == tick) mine = &m;
    s.desyncTick = tick;
    WriteDesyncFile(s, tick, mine, nullptr, "reported by the other PC");
    EndSession(End::Desync, "The other PC found the game out of step at tick " + std::to_string(tick) +
                                ". Network play ended. Please send the files in " + s.dir + " from both PCs.");
    break;
  }
  case kBye: {
    const std::string why = r.str();
    EndSession(End::Left, std::string(PlayerName(1 - s.slot)) + " left the game" + (why.empty() ? "" : " (" + why + ")") + ".");
    break;
  }
  case kPing: {
    Writer w;
    w.u64(r.u64());
    s.ch->Send(kPong, w);
    break;
  }
  case kPong: {
    const unsigned long long sent = r.u64();
    const unsigned long long now = static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count());
    s.rttMs = static_cast<double>(now - sent) / 1000.0;
    break;
  }
  default: break;
  }
}

// Writes and reads; false once the session ended.
bool PumpSession(Session& s) {
  if (!s.active) return false;
  const bool alive = s.ch->Pump();
  std::vector<unsigned char> msg;
  while (s.active && s.ch->Next(msg)) Handle(s, msg);
  if (!s.active) return false;
  if (!alive || s.ch->Dead()) {
    EndSession(End::Lost, "The connection to " + std::string(PlayerName(1 - s.slot)) + " was lost (" + s.ch->Error() + ").");
    return false;
  }
  return true;
}

void EndSession(End why, const std::string& text) {
  Session& s = N();
  if (!s.active) return;
  s.active = false;
  s.ended = why;
  std::fprintf(stderr, "[net] session ended at tick %d: %s\n", s.lastTick, text.c_str());
  std::fprintf(stderr, "[net] waits: %d (%d over 0.5 s), %.0f ms in all, longest %.0f ms; %zu bytes sent, %zu received\n",
               s.waits, s.longWaits, s.waitedMs, s.longestWaitMs, s.ch ? s.ch->Sent() : 0u, s.ch ? s.ch->Received() : 0u);
  if (why == End::LocalQuit || why == End::Desync) {
    if (why == End::LocalQuit) {
      Writer w;
      w.str("quit");
      s.ch->Send(kBye, w);
    }
    s.ch->Flush(1000);
  }
  s.ch.reset();
  SocketsShutdown();
  std::fflush(nullptr);  // the session's record (net_*.rec) and logs
  if (why == End::LocalQuit) return;
  SetNotice(text + (s.quitOnEnd ? "" : " You can go on playing on this PC alone (save from the pause menu)."), 0);
  SetTitle("network play ended");
  if (!platform_headless() && !dev_options().offscreen && why != End::Left)
    SDL_ShowSimpleMessageBox(why == End::Desync ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_WARNING, "SIMMY - network play",
                             text.c_str(), aurora_get_window());
  if (s.quitOnEnd) platform_request_quit();
}
}  // namespace

// ---- the API ---------------------------------------------------------------------------------------
bool net_parse_arg(int argc, char** argv, int& i) {
  const char* a = argv[i];
  auto next = [&](bool optional) -> const char* {
    if (i + 1 < argc && (!optional || (argv[i + 1][0] != '-'))) return argv[++i];
    return nullptr;
  };
  if (std::strcmp(a, "--host") == 0) {
    g_cli.mode = 1;
    if (const char* p = next(true)) {
      if (!ParseInt(p, 1, 65535, g_cli.port)) std::fprintf(stderr, "[net] --host: \"%s\" is not a port\n", p);
    }
    return true;
  }
  if (std::strcmp(a, "--join") == 0) {
    if (const char* t = next(false)) {
      g_cli.mode = 2;
      g_cli.join = t;
    } else {
      std::fprintf(stderr, "[net] --join needs the host's address (--join 192.168.1.20)\n");
    }
    return true;
  }
  if (std::strcmp(a, "--net-port") == 0) {
    if (const char* p = next(false)) ParseInt(p, 1, 65535, g_cli.port);
    return true;
  }
  if (std::strcmp(a, "--net-delay") == 0) {
    if (const char* p = next(false)) ParseInt(p, 1, 30, g_cli.delay);
    return true;
  }
  return false;
}

void net_ini_value(const std::string& rawKey, const std::string& rawValue, const std::string& where) {
  std::string key;
  for (char c : rawKey)
    if (c != ' ' && c != '\t' && c != '_' && c != '-') key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const std::string value = Trimmed(rawValue);
  bool b = false;
  int n = 0;
  if (key == "host") {
    if (ParseInt(value, 1, 65535, n)) g_ini.mode = 1, g_ini.port = n;
    else if (ParseBool(value, b)) {
      if (b) g_ini.mode = 1;
      else if (g_ini.mode == 1) g_ini.mode = 0;
    } else std::fprintf(stderr, "[net] %s: host = yes, no or a port\n", where.c_str());
  } else if (key == "join") {
    if (!value.empty()) g_ini.mode = 2, g_ini.join = value;
    else if (g_ini.mode == 2) g_ini.mode = 0;
  } else if (key == "port") {
    if (!ParseInt(value, 1, 65535, g_ini.port)) std::fprintf(stderr, "[net] %s: port = 1..65535\n", where.c_str());
  } else if (key == "delay") {
    if (!ParseInt(value, 1, 30, g_ini.delay)) std::fprintf(stderr, "[net] %s: delay = 1..30 (ticks)\n", where.c_str());
  } else if (key == "hashevery") {
    if (!ParseInt(value, 1, 6000, g_ini.hashEvery)) std::fprintf(stderr, "[net] %s: hash_every = 1..6000 (ticks)\n", where.c_str());
  } else if (key == "timeout") {
    if (!ParseInt(value, 3, 3600, g_ini.timeoutSec)) std::fprintf(stderr, "[net] %s: timeout = 3..3600 (seconds)\n", where.c_str());
  } else if (key == "onend") {
    if (value == "quit") g_ini.onEnd = 1;
    else if (value == "continue") g_ini.onEnd = 0;
    else std::fprintf(stderr, "[net] %s: on_end = continue or quit\n", where.c_str());
  } else if (key == "skipintro") {
    if (ParseBool(value, b)) g_ini.skipIntro = b ? 1 : 0;
  } else {
    std::fprintf(stderr, "[net] %s: unknown [net] setting \"%s\"\n", where.c_str(), rawKey.c_str());
  }
}

int net_start_session() {
  // The command line wins over simmy.ini.
  Options o = g_ini;
  if (g_cli.mode >= 0) o.mode = g_cli.mode, o.join = g_cli.join;
  if (g_cli.port > 0) o.port = g_cli.port;
  if (g_cli.delay > 0) o.delay = g_cli.delay;
  if (const char* v = Env("SIMMY_NET_DELAY")) o.delay = std::atoi(v);  // tests
  if (const char* v = Env("SIMMY_NET_HASH_EVERY")) o.hashEvery = std::atoi(v);
  if (const char* v = Env("SIMMY_NET_TIMEOUT")) o.timeoutSec = std::atoi(v);
  if (o.mode <= 0) return 0;
  Session& s = N();
  s.slot = o.mode == 1 ? 0 : 1;
  s.delay = o.delay > 0 ? o.delay : 2;
  s.hashEvery = o.hashEvery > 0 ? o.hashEvery : 30;
  s.timeoutMs = (o.timeoutSec > 0 ? o.timeoutSec : 30) * 1000;
  s.quitOnEnd = o.onEnd == 1 || (Env("SIMMY_NET_ON_END") && std::strcmp(Env("SIMMY_NET_ON_END"), "quit") == 0);
  s.skipIntro = o.skipIntro != 0;
  s.dir = SessionFolder();
  const int port = o.port > 0 ? o.port : kDefaultPort;
  if (const char* v = Env("SIMMY_NET_STALL"))
    for (const char* p = v; *p;) {
      char* end = nullptr;
      const int tick = static_cast<int>(std::strtol(p, &end, 10));
      int ms = 1000;
      if (*end == ':') ms = static_cast<int>(std::strtol(end + 1, &end, 10));
      s.stalls.emplace_back(tick, ms);
      p = end;
      while (*p == ',' || *p == ' ') ++p;
      if (end == p && !*p) break;
    }
  if (const char* v = Env("SIMMY_NET_DESYNC")) s.desyncAt = std::atoi(v);
  std::fprintf(stderr, "[net] network play: %s, input delay %d ticks, hash every %d ticks, session folder %s\n",
               s.slot == 0 ? "hosting (player 1)" : ("joining " + o.join + " (player 2)").c_str(), s.delay, s.hashEvery,
               s.dir.c_str());
  // Players 3 and 4 (PC mod: 4 players) are not part of network play: a session has two.
  if (graphics_settings().maxPlayers != 2) {
    std::fprintf(stderr, "[net] players=%d: network play has exactly two players; players = 2 for this session\n",
                 graphics_settings().maxPlayers);
    graphics_settings().maxPlayers = 2;
  }
  if (Env("SIMMY_DET_REPLAY")) {
    ShowError("network play", "SIMMY_DET_REPLAY cannot be used in a network session.");
    return 3;
  }
  if (!SocketsInit()) {
    ShowError("network play", "The network could not be started (WSAStartup failed).");
    return 3;
  }
  std::string error;
  std::unique_ptr<Stream> stream = s.slot == 0 ? Listen(port, error) : Connect(o.join, port, 60, error);
  if (!stream) {
    if (error != "cancelled") ShowError("network play", error);
    SocketsShutdown();
    return error == "cancelled" ? -1 : 3;
  }
  s.ch = std::make_unique<Channel>(std::move(stream));
  SetNotice("Connected: checking that both PCs can play together ...", 0);
  if (!Handshake(s, error)) {
    if (error != "cancelled")
      ShowError("network play refused", error);
    s.ch.reset();
    SocketsShutdown();
    return error == "cancelled" ? -1 : 3;
  }
  s.set = s.active = true;
  for (Slot& sl : s.ring) sl = Slot{};
  {
    const std::string rec = s.dir + "/net_" + Role(s.slot) + ".rec";
    std::error_code ec;
    fs::create_directories(U8Path(s.dir), ec);
    FILE* f = Env("SIMMY_DET_RECORD") ? nullptr : OpenUtf8(U8Path(rec), "w");
    if (!f && !Env("SIMMY_DET_RECORD")) std::fprintf(stderr, "[net] cannot write %s\n", rec.c_str());
    det_net_enable(f, rec);
  }
  SetNotice(std::string("Network play: you are ") + PlayerName(s.slot) + ".", 6);
  SetTitle(std::string("network play, ") + PlayerName(s.slot));
  std::fprintf(stderr, "[net] session started: this PC is %s (pad slot %d)\n", PlayerName(s.slot), s.slot + 1);
  return 0;
}

bool net_skip_intro() { return N().set && N().skipIntro; }

const std::string& net_notice() {
  Session& s = N();
  static const std::string none;
  if (s.notice.empty()) return none;
  if (!s.noticeForever && Clock::now() > s.noticeUntil) s.notice.clear();
  return s.notice;
}

bool net_session() { return N().set; }
bool net_active() { return N().active; }
int net_local_slot() { return N().set ? N().slot : 0; }
int net_view_player() { return N().set ? N().slot : -1; }

bool net_pads(int tick, PADStatus* pads) {
  Session& s = N();
  if (!s.set) return false;
  // This PC's input: every local pad merged (the keyboard is on port 1 here, a controller may be on any).
  Input local = Neutral();
  for (int k = 0; k < 4; ++k) {
    const PADStatus& p = pads[k];
    if (p.err != PAD_ERR_NONE) continue;
    local.pad.button = static_cast<u16>(local.pad.button | p.button);
    if (!local.pad.stickX && !local.pad.stickY) local.pad.stickX = p.stickX, local.pad.stickY = p.stickY;
    if (!local.pad.substickX && !local.pad.substickY) local.pad.substickX = p.substickX, local.pad.substickY = p.substickY;
    local.pad.triggerLeft = std::max(local.pad.triggerLeft, p.triggerLeft);
    local.pad.triggerRight = std::max(local.pad.triggerRight, p.triggerRight);
    local.pad.analogA = std::max(local.pad.analogA, p.analogA);
    local.pad.analogB = std::max(local.pad.analogB, p.analogB);
    local.pad.extButton |= p.extButton;
  }
  local.kbm = kbm_tick();
  s.local = local;
  if (s.desyncAt && tick == s.desyncAt) {
    const int r = sim_rand();  // SIMMY_NET_DESYNC: a deliberate desync (one extra draw on this PC only)
    std::fprintf(stderr, "[net] SIMMY_NET_DESYNC: an extra simulation random number at tick %d (%d)\n", tick, r);
  }
  Input in[2] = {Neutral(), Neutral()};
  if (s.active) {
    if (tick > s.delay) {
      Slot& sl = At(tick);
      if (!sl.have[0] || !sl.have[1]) {
        // (net_frame_end waits for this; a second pad read in one tick finds it there.)
        const Clock::time_point t0 = Clock::now();
        while (s.active && !(At(tick).have[0] && At(tick).have[1])) {
          if (!PumpSession(s)) break;
          if (MsSince(t0) > s.timeoutMs) EndSession(End::Timeout, "No input from the other PC.");
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
      if (s.active) in[0] = At(tick).in[0], in[1] = At(tick).in[1];
    }
  }
  if (!s.active) {
    // After the session: this PC's input in its own slot, the partner's idle.
    in[s.slot] = local;
    in[1 - s.slot] = Neutral();
  }
  pads[0] = in[0].pad;
  pads[1] = in[1].pad;
  for (int k = 2; k < 4; ++k) {
    pads[k] = PADStatus{};
    pads[k].err = PAD_ERR_NO_CONTROLLER;
  }
  kbm_tick_port_for_det(0) = in[0].kbm;
  kbm_tick_port_for_det(1) = in[1].kbm;
  if (Trace() && (in[0].pad.button || in[1].pad.button))
    std::fprintf(stderr, "[net] tick %d: pads %04x %04x\n", tick, in[0].pad.button, in[1].pad.button);
  return true;
}

bool net_wants_hash(int tick) {
  const Session& s = N();
  return s.active && s.hashEvery > 0 && tick % s.hashEvery == 0;
}

void net_frame_end(int tick, const NetHash* hash) {
  SIMMY_PERF_SCOPE(Net);  // the frame-hitch profiler (perf.hpp)
  Session& s = N();
  if (!s.active) return;
  s.lastTick = tick;
  // This tick's input goes out for tick + delay.
  {
    const int target = tick + s.delay;
    Slot& sl = At(target);
    sl.in[s.slot] = s.local;
    sl.have[s.slot] = true;
    Writer w;
    w.u32(static_cast<unsigned>(target));
    PutInput(w, s.local);
    s.ch->Send(kInput, w);
  }
  if (hash) {
    Check c;
    c.tick = tick;
    c.total = hash->total;
    for (int k = 0; k < 6; ++k) c.part[k] = hash->part[k];
    c.dump = hash->dump;
    Writer w;
    w.u32(static_cast<unsigned>(tick));
    w.u64(c.total);
    for (unsigned long long p : c.part) w.u64(p);
    s.ch->Send(kHash, w);
    s.mine.push_back(std::move(c));
    // Keep the last checks with their dumps, and the last matching one.
    while (s.mine.size() > 24 && s.mine.front().tick != s.lastMatched) s.mine.pop_front();
    while (s.mine.size() > 48) s.mine.pop_front();
    auto it = s.theirs.find(tick);
    if (it != s.theirs.end()) {
      const Check theirs = it->second;
      s.theirs.erase(it);
      CompareChecks(s, s.mine.back(), theirs);
      if (!s.active) return;
    }
  }
  if (tick % 60 == 0) {
    Writer w;
    w.u64(static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count()));
    s.ch->Send(kPing, w);
  }
  for (const auto& st : s.stalls)
    if (st.first == tick) {
      std::fprintf(stderr, "[net] SIMMY_NET_STALL: this PC stops for %d ms at tick %d\n", st.second, tick);
      std::this_thread::sleep_for(std::chrono::milliseconds(st.second));
    }
  if (!PumpSession(s)) return;
  // Wait for the next tick's input from the other PC.
  const int next = tick + 1;
  if (next > s.delay && !At(next).have[1 - s.slot]) {
    const Clock::time_point t0 = Clock::now();
    Clock::time_point frame = t0;
    bool noted = false;
    while (s.active && !At(next).have[1 - s.slot]) {
      if (!PumpSession(s)) break;
      if (At(next).have[1 - s.slot]) break;
      const double waited = MsSince(t0);
      if (waited > 500.0 && !noted) {
        noted = true;
        std::fprintf(stderr, "[net] tick %d: waiting for %s ...\n", next, PlayerName(1 - s.slot));
        SetNotice(std::string("Waiting for ") + PlayerName(1 - s.slot) + " ...", 0);
        SetTitle(std::string("waiting for ") + PlayerName(1 - s.slot));
      }
      if (MsSince(s.ch->LastReceive()) > s.timeoutMs) {
        EndSession(End::Timeout, "Nothing came from " + std::string(PlayerName(1 - s.slot)) + " for " +
                                     std::to_string(s.timeoutMs / 1000) + " s: network play ended.");
        break;
      }
      if (!platform_headless() && waited > 500.0) {
        if (!platform_pump_events()) {
          net_local_quit();
          break;
        }
        if (waited > 1500.0 && MsSince(frame) > 100.0) {
          platform_present_wait_frame();
          frame = Clock::now();
        }
      }
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    const double waited = MsSince(t0);
    ++s.waits;
    s.waitedMs += waited;
    s.longestWaitMs = std::max(s.longestWaitMs, waited);
    if (noted) {
      ++s.longWaits;
      std::fprintf(stderr, "[net] tick %d: waited %.0f ms\n", next, waited);
      if (s.active) {
        SetNotice(std::string("Waited ") + std::to_string(static_cast<int>(waited)) + " ms for " + PlayerName(1 - s.slot), 2);
        SetTitle(std::string("network play, ") + PlayerName(s.slot));
      }
    }
  }
  if (tick % 3000 == 0 && s.active)
    std::fprintf(stderr, "[net] tick %d: in step up to tick %d; waits %d (%d over 0.5 s, %.0f ms in all); round trip %.1f ms\n",
                 tick, s.lastMatched, s.waits, s.longWaits, s.waitedMs, s.rttMs);
}

void net_local_quit() {
  Session& s = N();
  if (!s.active) return;
  EndSession(End::LocalQuit, "this PC quit");
}

}  // namespace simmy
