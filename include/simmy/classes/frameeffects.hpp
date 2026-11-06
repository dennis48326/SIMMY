// @class FrameEffect after ERC EAnimNote
// @class MotionBlur after ERC EAnimNote
// @class Bloom after ERC EAnimNote
// @class DepthOfField after ERC EAnimNote
// @class MotionBlurNGC after ERC EAnimNote
// @class BloomNGC after ERC EAnimNote
// @class DepthOfFieldNGC after ERC EAnimNote
// @class MotionBlurSettings after ERC EAnimNote
// @class BloomSettings after ERC EAnimNote
// @class DepthOfFieldSettings after ERC EAnimNote
// @class FrameEffectsManager after ERC EAnimNote
// Full-screen frame effects (engine/frameeffects.cpp, frameeffectsmanager.cpp, frameeffectsngc.cpp;
// src/engine/frameeffects.cpp). Each CameraDirector owns a FrameEffectsManager with one motion blur,
// one bloom and one depth of field. An effect is switched on and off by animation notes (social stance
// animations and camera shots: game/animeventhandlersupport.cpp), the camera director's interpolations
// and the loading screen; it fades its parameters in and out over its fade times, and while it runs
// its Draw copies the EFB (the 3D window's rectangle), and blends a blurred / delayed / bright-passed
// copy back over it with four jittered taps (frameeffectsngc.cpp). Motion blur and bloom run after a
// player's view is drawn (ELiveMode::DrawMain), depth of field inside ERLevel::Draw before the
// foreground list (the sims in the social interaction stay sharp).
#pragma once

// The effect's state (FrameEffect +0x0C).
enum FrameEffectState : int { kFrameEffectOff = 1, kFrameEffectFadingIn = 2, kFrameEffectOn = 4, kFrameEffectFadingOut = 8 };

struct FrameEffect {
  float m_fadeInTime = 0.0f;   // +0x00
  float m_fadeOutTime = 0.0f;  // +0x04
  float m_time = -1.0f;        // +0x08 since the last Enable
  int m_state = kFrameEffectOff;  // +0x0C
  // +0x10 vtable (GC)

  virtual ~FrameEffect() = default;
  virtual void UpdateTargetParameters(float time) = 0;   // vtable +0x14
  virtual void Draw(ERC* rc) = 0;                         // vtable +0x1C
  virtual void EffectStateChanged(int state) = 0;         // vtable +0x24
  virtual bool EffectIsFinishedFadingIn(float time) const = 0;   // vtable +0x2C
  virtual bool EffectIsFinishedFadingOut(float time) const = 0;  // vtable +0x34

  void Enable(bool on);  // 80356FB0
  void Update(float dt, ERC* rc);  // 80357020
  bool IsEffectRamping() const;  // 803570B4
  bool IsEffectRunning() const;  // 803570D4
  void UpdateEffectState();  // 803570EC
};

struct MotionBlurSettings {  // 0x0C bytes
  float colorOffset;         // +0x00
  float previousFrameBlend;  // +0x04 how much of the previous frame (4 taps of a quarter each)
  int downSampleLevel;       // +0x08 0: the half-size previous frame as is; n: scaled to 256 >> (n-1)
};

struct MotionBlur : FrameEffect {  // 0x38 bytes
  MotionBlurSettings m_target{0.12f, 0.7f, 0};   // +0x14
  MotionBlurSettings m_current{0.12f, 0.7f, 0};  // +0x20 what Draw uses
  MotionBlurSettings m_start{0.12f, 0.7f, 0};    // +0x2C the current ones when a fade began

  ~MotionBlur() override;  // 80357658
  void UpdateTargetParameters(float time) override;  // 803571C0
  void EffectStateChanged(int state) override;  // 8035728C
  bool EffectIsFinishedFadingIn(float time) const override;  // 803572C0
  bool EffectIsFinishedFadingOut(float time) const override;  // 803572D8
  void SetTargetSettings(MotionBlurSettings& s);  // 8035768C
};

struct BloomSettings {  // 0x2C bytes
  float frameAmp;        // +0x00 the frame's colour scale before the threshold (>1: x2 / x4 TEV scale)
  float bloomThreshold;  // +0x04 subtracted
  float bloomAmp;        // +0x08 what is left scaled by
  float frameBlends[4][2];  // +0x0C (bloom, frame) weights; Draw uses pair 1
};

struct Bloom : FrameEffect {  // 0x98 bytes
  BloomSettings m_target;   // +0x14
  BloomSettings m_current;  // +0x40
  BloomSettings m_start;    // +0x6C

  Bloom();  // 803C78E0
  ~Bloom() override;  // 803576AC
  void UpdateTargetParameters(float time) override;  // 803572F0
  void EffectStateChanged(int state) override;  // 803573BC
  bool EffectIsFinishedFadingIn(float time) const override;  // 80357444
  bool EffectIsFinishedFadingOut(float time) const override;  // 8035745C
  void SetTargetSettings(BloomSettings& s);  // 803576E0
};

struct DepthOfFieldSettings {  // 0x14 bytes
  int downSampleLevel;  // +0x00 the copy is 256 >> level square
  float tint[3];        // +0x04 the blurred copy's colour
  float blurFactor;     // +0x10 how much of it
};

struct DepthOfField : FrameEffect {  // 0x50 bytes
  DepthOfFieldSettings m_target;   // +0x14
  DepthOfFieldSettings m_current;  // +0x28
  DepthOfFieldSettings m_start;    // +0x3C

  DepthOfField();  // 803C7A14
  ~DepthOfField() override;  // 80357754
  void UpdateTargetParameters(float time) override;  // 80357474
  void EffectStateChanged(int state) override;  // 803575E4
  bool EffectIsFinishedFadingIn(float time) const override;  // 80357628
  bool EffectIsFinishedFadingOut(float time) const override;  // 80357640
  void SetTargetSettings(DepthOfFieldSettings& s);  // 80357788
};

struct MotionBlurNGC : MotionBlur {
  void Draw(ERC* rc) override;  // 8035917C
  ~MotionBlurNGC() override;  // 80359D70
};
struct BloomNGC : Bloom {
  void Draw(ERC* rc) override;  // 803595E4
  ~BloomNGC() override;  // 80359DA4
};
struct DepthOfFieldNGC : DepthOfField {
  void Draw(ERC* rc) override;  // 80359950
  ~DepthOfFieldNGC() override;  // 80359DD8
};

struct FrameEffectsManager {  // 0x0C bytes
  MotionBlur* m_motionBlur = nullptr;      // +0x00
  Bloom* m_bloom = nullptr;                // +0x04
  DepthOfField* m_depthOfField = nullptr;  // +0x08

  void Init();  // 803577B8
  void Shutdown();  // 803578B0
  MotionBlur* MotionBlurObject();  // 80357968
  Bloom* BloomObject();  // 80357970
  DepthOfField* DepthOfFieldObject();  // 80357978
};

// game/animeventhandlersupport.cpp: an animation note (kinds 0xD bloom, 0xE motion blur, 0xF depth of
// field) names a quickdata row ("row:name:value:..." overrides fields; "disable" turns it off).
void HandleBloomEvent(EAnimNote& note, FrameEffectsManager* fx);  // 80003998
void HandleMotionBlurEvent(EAnimNote& note, FrameEffectsManager* fx);  // 80003C40
void HandleDepthOfFieldEvent(EAnimNote& note, FrameEffectsManager* fx);  // 80003DE0

// PC dev aid (SIMMY_FX_FRAME=bloom|blur|dof[,...]): turns the named effects on for the given director
// (frameeffects.cpp). Returns whether anything was forced.
bool simmy_force_frame_effects(FrameEffectsManager* fx);
