// Port of engine/e_ngcmovie.cpp, rewritten for PC: the GameCube THP movie player.
//
// The original was Nintendo's THP player sample adapted by EA: a reader thread streamed frames from
// the disc (DVDReadPrio on "data/movies.arc"), video and audio decoder threads filled queues of
// texture sets and audio buffers, a VI post-retrace callback (PlayControl) picked the frame to show
// at each retrace, and an AI DMA callback mixed the movie's audio.
//
// Here one host thread does the reading, decoding (aurora's THPVideoDecode, which writes the same
// GX I8 tiles as the SDK decoder) and timing: it publishes frame n when the host clock reaches
// playStart + n / frameRate. The app thread draws the latest published frame with the original's GX
// path (THPGXYuv2RgbDraw: three I8 textures; the YUV->RGB TEV set-up is in ENgcRenderer::MovieFrame).
// The class interface used by ERMovie (Load/Start/Stop/Reset/IsFinished/GetNextFrame) is unchanged.
//
// Audio: the worker reads a few frames ahead and decodes each frame's audio (THPAudioDecode) into a
// PCM voice of the PC mixer (platform/pc_audio.cpp), which starts and pauses with the movie clock and
// is pulled back into step with it as each video frame is shown. The original mixed the decoded
// buffers into the AI DMA output (THPAudioMixCallback) at the THP player volume, which this game never
// changes from full (127).
//
// Not on PC: disc-error pausing
// (bPausedDueToDiskError: there is no disc drive), on-memory playback (Load never asked for it),
// looping (likewise) and the EAllocGroup the buffers came from (they are std::vectors).
#include "simmy/types_gen.hpp"
#include "simmy/pc_audio.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <dolphin/gx.h>
#include <dolphin/thp.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern ENgcRenderer _ngcRenderer;

namespace {
using Clock = std::chrono::steady_clock;

// THP player states (THPPlayer.h).
enum : unsigned char { kStop = 0, kPrepare = 1, kPlay = 2, kPlayed = 3, kPause = 4 };

struct TextureSet {  // one decoded frame: GX I8 tiles of Y and of the half-size U and V planes
  std::vector<unsigned char> y, u, v;
  int frameNumber = -1;  // counted from the first frame read (ActivePlayer.initReadFrame)
};

// The original's ActivePlayer (THPPlayer, 0x1D0 bytes) plus the PC thread state.
struct THPPlayerState {
  // THPHeader, offsets made absolute (relative to the start of movies.arc).
  unsigned int version = 0, bufSize = 0, audioMaxSamples = 0;
  float frameRate = 0.0f;
  unsigned int numFrames = 0, firstFrameSize = 0, movieDataSize = 0;
  unsigned int compInfoDataOffsets = 0, offsetDataOffsets = 0, movieDataOffsets = 0, finalFrameDataOffsets = 0;
  // THPFrameCompInfo
  unsigned int numComponents = 0;
  unsigned char frameComp[16] = {};
  THPVideoInfo videoInfo;
  THPAudioInfo audioInfo;

  simmy::DataFile* file = nullptr;  // the original's DVDFileInfo (movies.arc)
  int open = 0;                   // +0xA0
  unsigned char state = kStop;    // +0xA4 (written by the worker too: guarded by `mutex`)
  unsigned char internalState = kStop;
  unsigned char playFlag = 0;     // loop (not supported; never requested)
  unsigned char audioExist = 0;
  int onMemory = 0;
  int initReadFrame = 0;          // first frame read
  unsigned int initOffset = 0, initReadSize = 0;
  int curAudioTrack = 0;

  // PC: the worker thread and what it hands to the app thread.
  std::thread worker;
  std::mutex mutex;
  std::condition_variable cv;
  bool cancel = false;
  bool prepared = false;          // first frame decoded (or the worker gave up)
  bool failed = false;
  TextureSet ready;               // latest frame due for display
  bool readyValid = false;
  Clock::time_point playStart;    // when frame 0 was due
  Clock::time_point pauseTime;
  // PC (SIMMY_DET, network play): the same in the game's virtual time (simmy::game_clock_ns), which
  // decides when the movie has ended: the worker's wall clock must not reach the game.
  unsigned long long detStart = 0, detPause = 0;
  // App side: copies of the frames drawn recently (aurora may still be reading the previous one).
  TextureSet shown[3];
  int shownIndex = -1;
  simmy::audio::Voice* audioVoice = nullptr;  // the audio track, while the worker runs

  ~THPPlayerState() {  // PC: a movie still playing at exit
    {
      std::lock_guard lock(mutex);
      cancel = true;
    }
    cv.notify_all();
    if (worker.joinable()) worker.join();
  }
};
THPPlayerState ActivePlayer;

// Positional: the decoder thread reads frames while the game thread may read the header tables.
bool ReadAt(const simmy::DataFile* f, unsigned int offset, void* dst, size_t size) {
  return simmy::data_read_at(f, offset, dst, size) == size;
}

size_t LumaSize(const THPPlayerState& p) { return (p.videoInfo.xSize * p.videoInfo.ySize + 0x1f) & ~0x1fu; }
size_t ChromaSize(const THPPlayerState& p) { return ((p.videoInfo.xSize * p.videoInfo.ySize >> 2) + 0x1f) & ~0x1fu; }

// Waits until `due` (or until stopped). Pausing holds the clock: PlayerPlay moves playStart on by
// the time spent paused, so `due` is recomputed from it after every wake-up.
bool WaitForFrame(std::unique_lock<std::mutex>& lock, THPPlayerState& p, double frameTime) {
  for (;;) {
    if (p.cancel) return false;
    if (p.state == kPlay) {
      const auto due = p.playStart + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(frameTime));
      if (Clock::now() >= due) return true;
      p.cv.wait_until(lock, due);
    } else {
      p.cv.wait(lock);  // prepared but not started yet, or paused
    }
  }
}

// The frame's component `kind` (0 video, 1 audio): frame header (next size, previous size, one size
// per component), then the components in order. `size` receives the component's size.
const unsigned char* Component(const THPPlayerState& p, const std::vector<unsigned char>& frame, unsigned char kind,
                               unsigned int* size = nullptr) {
  const size_t header = 8 + 4 * static_cast<size_t>(p.numComponents);
  if (frame.size() < header) return nullptr;
  size_t pos = header;
  for (unsigned int i = 0; i < p.numComponents; ++i) {
    const unsigned int compSize = simmy::be32(frame.data() + 8 + 4 * i);
    if (p.frameComp[i] == kind) {
      if (size) *size = compSize;
      return pos + compSize <= frame.size() ? frame.data() + pos : nullptr;
    }
    pos += compSize;
  }
  return nullptr;
}

// AudioDecode: the current track of the frame's audio (one track per component-size step) to the
// movie's voice. THPAudioDecode writes each stereo pair in AI order (right, left).
void QueueAudio(THPPlayerState& p, const std::vector<unsigned char>& frame, std::vector<s16>& pcm) {
  if (!p.audioVoice) return;
  unsigned int compSize = 0;
  const unsigned char* audio = Component(p, frame, 1, &compSize);
  if (!audio) return;
  const size_t track = static_cast<size_t>(compSize) * static_cast<size_t>(p.curAudioTrack);
  if (audio + track + 80 > frame.data() + frame.size()) return;
  const unsigned int samples = simmy::be32(audio + track + 4);
  if (samples == 0 || samples > p.audioMaxSamples) return;
  pcm.resize(static_cast<size_t>(samples) * 2);
  THPAudioDecode(pcm.data(), audio + track, 0);
  for (size_t i = 0; i < pcm.size(); i += 2) std::swap(pcm[i], pcm[i + 1]);  // to left, right
  simmy::audio::PushPcm(p.audioVoice, pcm.data(), samples);
}

// The PC replacement for Reader, VideoDecoder, AudioDecoder and PlayControl: reads frames (a few
// ahead, so the audio is queued in time), decodes each frame's video when it is next, and publishes
// it when it is due.
void MovieWorker() {
  THPPlayerState& p = ActivePlayer;
  constexpr size_t kReadAhead = 8;
  const double period = p.frameRate > 0.0f ? 1.0 / p.frameRate : 1.0 / 30.0;
  TextureSet scratch;
  scratch.y.resize(LumaSize(p));
  scratch.u.resize(ChromaSize(p));
  scratch.v.resize(ChromaSize(p));
  std::deque<std::vector<unsigned char>> ahead;
  std::vector<s16> pcm;
  unsigned int offset = p.initOffset, size = p.initReadSize;
  unsigned int next = static_cast<unsigned int>(p.initReadFrame);
  bool readFailed = false;
  int count = 0;
  for (;; ++count) {
    while (ahead.size() < kReadAhead && next < p.numFrames && !readFailed) {
      std::vector<unsigned char> frame(size);
      if (size < 8 || !ReadAt(p.file, offset, frame.data(), size)) {
        std::fprintf(stderr, "[movie] cannot read frame %u\n", next);
        readFailed = true;
        break;
      }
      QueueAudio(p, frame, pcm);
      offset += size;
      size = simmy::be32(frame.data());  // the next frame's size
      ++next;
      ahead.push_back(std::move(frame));
    }
    if (ahead.empty()) break;
    const std::vector<unsigned char> frame = std::move(ahead.front());
    ahead.pop_front();
    const unsigned char* video = Component(p, frame, 0);
    const bool ok = video && THPVideoDecode(video, scratch.y.data(), scratch.u.data(), scratch.v.data(), nullptr) == 0;
    std::unique_lock lock(p.mutex);
    if (!ok) {
      std::fprintf(stderr, "[movie] cannot decode frame %d\n", count);
      if (!p.prepared) p.failed = true;
      break;
    }
    if (count > 0 && !WaitForFrame(lock, p, count * period)) return;
    scratch.frameNumber = count;
    std::swap(scratch, p.ready);
    p.readyValid = true;
    if (!p.prepared) {
      p.prepared = true;  // WaitUntilPrepare / PrepareReady
      p.cv.notify_all();
    }
    // The audio follows the movie clock (not this frame, which may be late).
    const double clock = std::chrono::duration<double>(Clock::now() - p.playStart).count();
    lock.unlock();
    if (count > 0 && p.audioVoice) {
      const auto expected = static_cast<std::uint64_t>(clock * p.audioInfo.sndFrequency);
      if (const std::int64_t fix = simmy::audio::SyncPcm(p.audioVoice, expected))
        std::fprintf(stderr, "[movie] audio %s %lld samples to stay with the picture (frame %d)\n",
                     fix > 0 ? "skipped" : "held", static_cast<long long>(fix > 0 ? fix : -fix), count);
    }
    if (scratch.y.size() != LumaSize(p)) {
      scratch.y.resize(LumaSize(p));
      scratch.u.resize(ChromaSize(p));
      scratch.v.resize(ChromaSize(p));
    }
  }
  // The last frame stays up for one frame period; then the movie has played.
  std::unique_lock lock(p.mutex);
  if (!p.prepared) {
    p.prepared = true;
    p.failed = true;
    p.cv.notify_all();
    return;
  }
  if (!WaitForFrame(lock, p, count * period)) return;
  p.state = kPlayed;
  p.internalState = kPlayed;
  std::fprintf(stderr, "[movie] played %d frames\n", count);
}

void StopWorker(THPPlayerState& p) {
  {
    std::lock_guard lock(p.mutex);
    p.cancel = true;
  }
  p.cv.notify_all();
  if (p.worker.joinable()) p.worker.join();
  simmy::audio::FreeVoice(p.audioVoice);
  p.audioVoice = nullptr;
  p.cancel = false;
  p.prepared = false;
  p.failed = false;
  p.readyValid = false;
  p.shownIndex = -1;
}
}  // namespace

// 80335108
// Makes sure the audio is up (GC: with the game's audio off it still initialised AI/AX/MIX for the
// movie; on PC there is nothing to play into without the mixer).
ENgcMovie::ENgcMovie() {
  _pActualAudio->InitAudio();
  PlayerInit(1);
}

// 803351D8
// PC: also stops a movie that is still playing (a running std::thread must be joined).
ENgcMovie::~ENgcMovie() {
  if (m_ownsPlayer) {
    PlayerStop();
    PlayerClose();
  }
  PlayerQuit();
}

// 80335244
// Opens the THP data at `offset` in movies.arc, centres it on the screen and decodes the first frame.
// The original panicked ("Fail to open the thp file", "Fail to prepare"); here the movie just fails
// to load and ERMovie reports it finished.
int ENgcMovie::Load(EFile*, unsigned int offset, unsigned int, unsigned int) {
  if (!PlayerOpen(offset, false)) {
    std::fprintf(stderr, "[movie] Fail to open the thp file (movies.arc offset 0x%x)\n", offset);
    return 0;
  }
  PlayerGetVideoInfo(&m_videoInfo);
  PlayerGetAudioInfo(&m_audioInfo);
  m_renderMode = &_ngcRenderer.m_renderMode;
  m_drawX = (m_renderMode->fbWidth - m_videoInfo.xSize) >> 1;
  m_drawY = (m_renderMode->efbHeight - m_videoInfo.ySize) >> 1;
  // GameCube: the buffers came from an EAllocGroup on _pApp->GetMovieHeap().
  PlayerCreateBuffers();
  m_field0C = 0;
  if (!PlayerPrepare(0, 0, 0)) {
    std::fprintf(stderr, "[movie] Fail to prepare\n");
    PlayerClose();
    return 0;
  }
  m_ownsPlayer = true;
  std::fprintf(stderr, "[movie] %ux%u, %u frames at %.2f fps (movies.arc offset 0x%x)\n", m_videoInfo.xSize,
               m_videoInfo.ySize, static_cast<unsigned int>(PlayerGetTotalFrame()), PlayerGetFrameRate(), offset);
  return 1;
}

// 803353A8
// -1 centres the movie on that axis.
void ENgcMovie::Start(int x, int y) {
  if (x == -1) {
    const unsigned int w = m_renderMode->fbWidth;
    m_drawX = w < m_videoInfo.xSize ? 0 : (w - m_videoInfo.xSize) >> 1;
  } else {
    m_drawX = static_cast<unsigned int>(x);
  }
  if (y == -1) {
    const unsigned int h = m_renderMode->efbHeight;
    m_drawY = h < m_videoInfo.ySize ? 0 : (h - m_videoInfo.ySize) >> 1;
  } else {
    m_drawY = static_cast<unsigned int>(y);
  }
  PlayerPlay();
  m_x = static_cast<int>(m_drawX);
  m_y = static_cast<int>(m_drawY);
}

// 80335460
void ENgcMovie::Stop() {
  PlayerStop();
  PlayerClose();
  m_ownsPlayer = false;
}

// 8033549C
void ENgcMovie::Reset() {}

// 803354A0
// (The original also kept a movie "playing" while it was paused for a disc error.)
int ENgcMovie::IsFinished() {
  const int state = static_cast<int>(PlayerGetState());
  if (simmy::det_timing()) {
    // PC (SIMMY_DET, network play): played once its frames' time (and the last frame's period, as the
    // worker keeps it up) has passed in game time, whatever the worker's wall clock says (kPlayed).
    if (state == kStop || state == kPrepare || state == kPause) return state == kStop;
    const THPPlayerState& p = ActivePlayer;
    if (!p.open || p.frameRate <= 0.0f) return 1;
    const int frames = std::max(1, static_cast<int>(p.numFrames) - std::max(0, p.initReadFrame)) + 1;
    const auto length = static_cast<unsigned long long>(static_cast<double>(frames) * 1e9 / static_cast<double>(p.frameRate));
    return simmy::game_clock_ns() - p.detStart >= length;
  }
  return state != kPrepare && state != kPlay;
}

// 803354FC
// Called by the renderer (ENgcRenderer::MovieFrame) with the YUV->RGB TEV set-up in place. The
// original first checked the disc drive and paused the movie while the cover was open.
bool ENgcMovie::GetNextFrame() {
  const int frame = static_cast<int>(PlayerDrawCurrentFrame(m_renderMode, m_drawX, m_drawY, m_videoInfo.xSize,
                                                            m_videoInfo.ySize));
  m_lastFrame = frame;
  PlayerDrawDone();
  return frame >= 0;
}

// 8033560C
// (The original also hooked the AI DMA callback to mix the movie's audio; the movie's voice is
// acquired by PlayerPrepare instead.)
RET_UNKNOWN ENgcMovie::PlayerInit(int) {
  if (!THPInit()) return 0;
  m_playerInitialized = 1;
  return 1;
}

// 80335744
void ENgcMovie::PlayerQuit() { m_playerInitialized = 0; }

// 803357A4
// Reads the THP header, the component table and the video/audio descriptions. All offsets in the
// header are relative to the movie, so they are made relative to the archive.
RET_UNKNOWN ENgcMovie::PlayerOpen(unsigned int offset, bool onMemory) {
  THPPlayerState& p = ActivePlayer;
  if (!m_playerInitialized || p.open) return 0;
  p.videoInfo = {};
  p.audioInfo = {};
  p.file = simmy::data_open("data/movies.arc");
  if (!p.file) return 0;
  unsigned char h[0x30];
  if (ReadAt(p.file, offset, h, sizeof h) && std::memcmp(h, "THP", 4) == 0 && simmy::be32(h + 4) == 0x11000) {
    p.version = simmy::be32(h + 0x04);
    p.bufSize = simmy::be32(h + 0x08);
    p.audioMaxSamples = simmy::be32(h + 0x0C);
    p.frameRate = simmy::befloat(h + 0x10);
    p.numFrames = simmy::be32(h + 0x14);
    p.firstFrameSize = simmy::be32(h + 0x18);
    p.movieDataSize = simmy::be32(h + 0x1C);
    p.compInfoDataOffsets = simmy::be32(h + 0x20) + offset;
    p.offsetDataOffsets = simmy::be32(h + 0x24) + offset;
    p.movieDataOffsets = simmy::be32(h + 0x28) + offset;
    p.finalFrameDataOffsets = simmy::be32(h + 0x2C) + offset;
    unsigned char c[0x20];
    unsigned int pos = p.compInfoDataOffsets;
    bool ok = ReadAt(p.file, pos, c, 0x14);
    if (ok) {
      pos += 0x14;
      p.numComponents = simmy::be32(c);
      std::memcpy(p.frameComp, c + 4, 16);
      p.audioExist = 0;
      for (unsigned int i = 0; ok && i < p.numComponents && i < 16; ++i) {
        if (p.frameComp[i] == 0) {  // video
          ok = ReadAt(p.file, pos, c, 0x0C);
          pos += 0x0C;
          p.videoInfo = {simmy::be32(c), simmy::be32(c + 4), simmy::be32(c + 8)};
        } else if (p.frameComp[i] == 1) {  // audio
          ok = ReadAt(p.file, pos, c, 0x10);
          pos += 0x10;
          p.audioInfo = {simmy::be32(c), simmy::be32(c + 4), simmy::be32(c + 8), simmy::be32(c + 12)};
          p.audioExist = 1;
        } else {
          ok = false;
        }
      }
    }
    if (ok && p.videoInfo.xSize && p.videoInfo.ySize) {
      p.open = 1;
      p.state = kStop;
      p.internalState = kStop;
      p.playFlag = 0;
      p.onMemory = onMemory;
      return 1;
    }
  }
  simmy::data_close(p.file);
  p.file = nullptr;
  return 0;
}

// 80335ABC
RET_UNKNOWN ENgcMovie::PlayerClose() {
  THPPlayerState& p = ActivePlayer;
  if (!p.open || p.state != kStop) return 0;
  p.open = 0;
  simmy::data_close(p.file);
  p.file = nullptr;
  return 1;
}

// 80335B0C
// PC: the texture sets are vectors sized here (Y, then U and V at a quarter of the size, each
// rounded up to 32 bytes as before); the read and audio buffers are the worker's.
RET_UNKNOWN ENgcMovie::PlayerCreateBuffers() {
  THPPlayerState& p = ActivePlayer;
  if (!p.open || p.state != kStop) return 0;
  for (TextureSet& set : p.shown) {
    set.y.assign(LumaSize(p), 0);
    set.u.assign(ChromaSize(p), 0);
    set.v.assign(ChromaSize(p), 0);
    set.frameNumber = -1;
  }
  return 1;
}

// 80335CE4
// Starts reading at `frame` (the offset table gives where each frame starts) and waits until the
// first frame is decoded. The movie is then shown (state "prepare") but its clock runs only once
// PlayerPlay is called.
RET_UNKNOWN ENgcMovie::PlayerPrepare(int frame, int playFlag, int audioTrack) {
  THPPlayerState& p = ActivePlayer;
  if (!p.open || p.state != kStop) return 0;
  if (frame < 1) {
    p.initOffset = p.movieDataOffsets;
    p.initReadSize = p.firstFrameSize;
  } else {
    if (p.offsetDataOffsets == 0 || p.numFrames <= static_cast<unsigned int>(frame)) return 0;
    unsigned char o[8];
    if (!ReadAt(p.file, p.offsetDataOffsets + (frame - 1) * 4, o, 8)) return 0;
    p.initReadSize = simmy::be32(o + 4) - simmy::be32(o);
    p.initOffset = p.movieDataOffsets + simmy::be32(o);
  }
  p.initReadFrame = frame;
  if (p.audioExist) {
    if (audioTrack < 0 || audioTrack >= static_cast<int>(p.audioInfo.sndNumTracks)) return 0;
    p.curAudioTrack = audioTrack;
  }
  p.playFlag = static_cast<unsigned char>(playFlag & 1);
  StopWorker(p);
  if (p.audioExist && p.audioInfo.sndChannels) {  // a stopped PCM voice at the track's rate, full volume
    p.audioVoice = simmy::audio::AcquireVoice();
    simmy::audio::SetVoiceBus(p.audioVoice, simmy::audio::kBusMovie);
    simmy::audio::SetPcmQueue(p.audioVoice);
    simmy::audio::SetRate(p.audioVoice, static_cast<float>(p.audioInfo.sndFrequency));
  }
  p.worker = std::thread(MovieWorker);
  bool ok;
  {
    std::unique_lock lock(p.mutex);
    p.cv.wait(lock, [&] { return p.prepared; });
    ok = !p.failed;
  }
  if (!ok) {
    StopWorker(p);
    return 0;
  }
  std::lock_guard lock(p.mutex);
  p.state = kPrepare;
  p.internalState = kStop;
  return 1;
}

// 80335F10
RET_UNKNOWN ENgcMovie::PlayerPlay() {
  THPPlayerState& p = ActivePlayer;
  std::lock_guard lock(p.mutex);
  if (!p.open || (p.state != kPrepare && p.state != kPause)) return 0;
  if (p.state == kPrepare) p.playStart = Clock::now(), p.detStart = simmy::game_clock_ns();
  else p.playStart += Clock::now() - p.pauseTime, p.detStart += simmy::game_clock_ns() - p.detPause;
  p.state = kPlay;
  simmy::audio::SetRunning(p.audioVoice, true);
  p.cv.notify_all();
  return 1;
}

// 80335F6C
void ENgcMovie::PlayerStop() {
  THPPlayerState& p = ActivePlayer;
  if (!p.open || p.state == kStop) return;
  StopWorker(p);
  std::lock_guard lock(p.mutex);
  p.state = kStop;
  p.internalState = kStop;
}

// 80336018
RET_UNKNOWN ENgcMovie::PlayerPause() {
  THPPlayerState& p = ActivePlayer;
  std::lock_guard lock(p.mutex);
  if (!p.open || p.state != kPlay) return 0;
  p.state = kPause;
  p.internalState = kPause;
  p.pauseTime = Clock::now();
  p.detPause = simmy::game_clock_ns();
  simmy::audio::SetRunning(p.audioVoice, false);
  return 1;
}

// 80336054
// While paused: show the next frame (by moving the clock on one frame).
RET_UNKNOWN ENgcMovie::PlayerSkip() {
  THPPlayerState& p = ActivePlayer;
  std::lock_guard lock(p.mutex);
  if (!p.open || p.state != kPause || p.frameRate <= 0.0f) return 0;
  p.pauseTime += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / p.frameRate));
  p.detPause += static_cast<unsigned long long>(1e9 / static_cast<double>(p.frameRate));
  return 1;
}

// 803361FC
// Draws the current frame at (x, y), w x h pixels; returns its frame number (or -1 before the first
// frame). A newly published frame is copied out of the worker's hands first.
RET_UNKNOWN ENgcMovie::PlayerDrawCurrentFrame(GXRenderModeObj* rmode, unsigned int x, unsigned int y,
                                              unsigned int w, unsigned int h) {
  THPPlayerState& p = ActivePlayer;
  if (!p.open || p.state == kStop) return -1;
  {
    std::lock_guard lock(p.mutex);
    if (p.readyValid && (p.shownIndex < 0 || p.ready.frameNumber != p.shown[p.shownIndex].frameNumber)) {
      p.shownIndex = (p.shownIndex + 1) % 3;
      TextureSet& dst = p.shown[p.shownIndex];
      dst.y = p.ready.y;
      dst.u = p.ready.u;
      dst.v = p.ready.v;
      dst.frameNumber = p.ready.frameNumber;
    }
  }
  if (p.shownIndex < 0) return -1;
  TextureSet& set = p.shown[p.shownIndex];
  THPGXYuv2RgbSetup(rmode);
  THPGXYuv2RgbDraw(set.y.data(), set.u.data(), set.v.data(), static_cast<short>(x), static_cast<short>(y),
                   static_cast<short>(p.videoInfo.xSize), static_cast<short>(p.videoInfo.ySize),
                   static_cast<short>(w), static_cast<short>(h));
  return static_cast<int>((set.frameNumber + p.initReadFrame) % p.numFrames);
}

// 803362B8
RET_UNKNOWN ENgcMovie::PlayerGetVideoInfo(THPVideoInfo* info) {
  if (!ActivePlayer.open) return 0;
  *info = ActivePlayer.videoInfo;
  return 1;
}

// 803362F8
RET_UNKNOWN ENgcMovie::PlayerGetAudioInfo(THPAudioInfo* info) {
  if (!ActivePlayer.open) return 0;
  *info = ActivePlayer.audioInfo;
  return 1;
}

// 80336340
double ENgcMovie::PlayerGetFrameRate() { return ActivePlayer.open ? ActivePlayer.frameRate : 0.0; }

// 80336368
RET_UNKNOWN ENgcMovie::PlayerGetTotalFrame() { return ActivePlayer.open ? ActivePlayer.numFrames : 0; }

// 80336388
RET_UNKNOWN ENgcMovie::PlayerGetState() {
  std::lock_guard lock(ActivePlayer.mutex);
  return ActivePlayer.state;
}

// 80336394
// Waits until GX has consumed the frame's textures. (The original then returned the texture sets it
// had drawn to the decoder's free queue; on PC the drawn frames are the app thread's own copies.)
void ENgcMovie::PlayerDrawDone() { GXDrawDone(); }

// 803363E0
// Empty in this game: ENgcRenderer::MovieFrame sets up the TEV stages, texture coordinates and
// constants for the YUV->RGB conversion through the render-state cache.
void THPGXYuv2RgbSetup(GXRenderModeObj*) {}

// 803363E4
// One quad textured with the Y plane (map 0) and the half-size U and V planes (maps 1 and 2), all
// GX_TF_I8, clamped and point-sampled, in vertex format 0 (s16 position, u16 texture coordinates).
void THPGXYuv2RgbDraw(unsigned char* y, unsigned char* u, unsigned char* v, short x, short top, short textureWidth,
                      short textureHeight, short polygonWidth, short polygonHeight) {
  GXTexObj texY, texU, texV;
  const u16 tw = static_cast<u16>(textureWidth), th = static_cast<u16>(textureHeight);
  GXInitTexObj(&texY, y, tw, th, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GXInitTexObjLOD(&texY, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
  GXLoadTexObj(&texY, GX_TEXMAP0);
  GXInitTexObj(&texU, u, tw >> 1, th >> 1, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GXInitTexObjLOD(&texU, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
  GXLoadTexObj(&texU, GX_TEXMAP1);
  GXInitTexObj(&texV, v, tw >> 1, th >> 1, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GXInitTexObjLOD(&texV, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
  GXLoadTexObj(&texV, GX_TEXMAP2);
  const s16 right = static_cast<s16>(x + polygonWidth), bottom = static_cast<s16>(top + polygonHeight);
  GXBegin(GX_QUADS, GX_VTXFMT0, 4);
  GXPosition3s16(x, top, 1);
  GXTexCoord2u16(0, 0);
  GXPosition3s16(right, top, 1);
  GXTexCoord2u16(1, 0);
  GXPosition3s16(right, bottom, 1);
  GXTexCoord2u16(1, 1);
  GXPosition3s16(x, bottom, 1);
  GXTexCoord2u16(0, 1);
  GXEnd();
}
