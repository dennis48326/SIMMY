// The PC replacement for the GameCube's AX/MIX/AI audio (src/platform/pc_audio.cpp): a small voice
// mixer on an SDL3 audio stream. The e_ngc* audio layer (ENgcSFXStreamer, ENGCAudioStreamer, the THP
// movie player) drives it the way the original drove AX voices and MIX channels.
//
// Voices play one of three sources:
//  - DSP-ADPCM in memory (8-byte frames: a predictor/scale byte and 14 4-bit samples; a 16-entry
//    coefficient table per sample), addressed in nibbles like AX (start, end and loop addresses);
//  - streamed DSP-ADPCM: the same, read from a StreamBuffer that a reader thread fills while it plays;
//  - a queue of 16-bit stereo PCM frames (movie audio), pushed by the producer.
// Each voice has a sample rate (pitch), a MIX-style fader in 0.1 dB (-904 = silent), a pan (0..127,
// 64 = centre, equal power), an input level and a mute flag.
//
// Locking: the mixer runs on SDL's audio thread. Calls that change voices take the mixer lock; code
// that needs several changes to land together (the GC code disabled interrupts around them) holds a
// Lock (recursive) across them. The frame callback (AXRegisterCallback) runs on the audio thread
// every 5 ms of output with the lock held.
//
// Dev switches: SIMMY_AUDIO_WAV=<file> (or SIMMY_AUDIO_DUMP=<file>) also writes everything that is
// mixed to a 16-bit stereo WAV file;
// SIMMY_AUDIO=0 disables audio. Headless runs use SDL's "dummy" driver (no sound card is opened).
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace simmy::audio {

constexpr int kOutputRate = 48000;     // mixer output (SDL converts to the device's format)
constexpr float kDspRate = 32028.66f;  // the GameCube DSP's output rate, as the game's ratios use it

// Opens the audio device (idempotent). Returns false when there is no audio at all; every other call
// is then a harmless no-op.
bool Open();
void Close();
bool IsOpen();

// AXRegisterCallback: called every 5 ms of mixed output, on the audio thread, with the lock held.
void SetFrameCallback(void (*callback)());

// Recursive mixer lock (the GC code's disable_IRQ/enable_IRQ).
void LockMixer();
void UnlockMixer();
struct Lock {
  Lock() { LockMixer(); }
  ~Lock() { UnlockMixer(); }
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;
};

// DSP-ADPCM decoder state (AXPBADPCM: coefficients, gain, predictor/scale, history).
struct AdpcmState {
  std::int16_t coef[16] = {};
  std::uint16_t gain = 0;
  std::uint16_t predScale = 0;
  std::int16_t yn1 = 0, yn2 = 0;
};

// Streamed ADPCM data: `data` is sized up front and filled by a reader; `available` bytes of it are
// valid. A voice that catches up with the reader plays silence until more arrives.
struct StreamBuffer {
  std::vector<unsigned char> data;
  std::atomic<std::size_t> available{0};
  std::atomic<bool> failed{false};  // the reader gave up; the voice ends where the data ends
};

struct Voice;

// AXAcquireVoice / AXFreeVoice. Null when there is no audio or all voices are taken.
Voice* AcquireVoice();
void FreeVoice(Voice* v);

// AXSetVoiceAddr + AXSetVoiceAdpcm for ADPCM in memory: `data` holds the frames (big-endian bytes as
// on disc), addresses are nibble indices into it (the first sample of a frame is at 16n + 2, the end
// address is the last nibble played). A looping voice continues at `loopNibble`; with `stream` set
// the decoder history carries over the loop (AX's streaming voice type), otherwise it restarts from
// zero. `data` must stay valid while the voice can play it.
void SetAdpcm(Voice* v, const unsigned char* data, std::uint32_t startNibble, std::uint32_t endNibble, bool loop,
              std::uint32_t loopNibble, const AdpcmState& state, bool stream = false);
// The same for data that a reader is still filling.
void SetAdpcmStream(Voice* v, std::shared_ptr<StreamBuffer> buffer, std::uint32_t startNibble,
                    std::uint32_t endNibble, bool loop, std::uint32_t loopNibble, const AdpcmState& state);
// A PCM queue (movie audio): 16-bit frames, stereo interleaved left/right.
void SetPcmQueue(Voice* v);
void PushPcm(Voice* v, const std::int16_t* frames, std::uint32_t count);
// Keeps a PCM queue in step with an outside clock: `expected` is the frame that should be playing
// now. Drops queued frames when playback lags behind and holds (plays silence) when it is ahead,
// beyond 60 ms either way. Returns the frames dropped (> 0) or held (< 0).
std::int64_t SyncPcm(Voice* v, std::uint64_t expected);
std::uint64_t PcmPosition(Voice* v);  // frames played (or dropped) since SetPcmQueue

// Input sample rate in Hz (AXSetVoiceSrc / AXSetVoiceSrcRatio: ratio * kDspRate).
void SetRate(Voice* v, float hz);
float GetRate(Voice* v);
// AXSetVoiceState: running or stopped (a stopped voice keeps its position).
void SetRunning(Voice* v, bool running);
bool IsRunning(Voice* v);
// True once a non-looping voice has played its end address (AX stops such a voice by itself).
bool HasEnded(Voice* v);
// Current nibble address of an ADPCM voice (AXVPB's current address).
std::uint32_t CurrentNibble(Voice* v);

// MIXSetFader / MIXSetInput (0.1 dB, -904 and below is silent), MIXSetPan (0..127), MIXMute.
void SetFader(Voice* v, int dB10);
void SetInput(Voice* v, int dB10);
void SetPan(Voice* v, int pan);
void SetMute(Voice* v, bool mute);
int GetFader(Voice* v);

// Mixer buses: every voice belongs to a bus whose gain (0..1) scales it (a PC-side stage after the
// game's own volumes; the options reach the game through cSoundPlayer's SetOldFXVolume /
// SetMusicVolume / SetAmbientVolume / SetVOXVolume, so the buses stay at 1 unless a PC feature sets
// them). Voices start on the SFX bus; FreeVoice puts them back there.
enum { kBusSfx, kBusMusic, kBusAmbient, kBusVoice, kBusMovie, kBusCount };
void SetVoiceBus(Voice* v, int bus);
void SetBusVolume(int bus, float gain);
float GetBusVolume(int bus);
int GetPan(Voice* v);

}  // namespace simmy::audio
