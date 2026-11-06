// @class SpriteSlot
// A sprite over an object or a person (game/slots.cpp): see sprites.hpp.
// PC: SpriteSlot does not derive from Slot (its +0x04..+0x0C are kept here).
#pragma once

// 0x6C bytes.
struct SpriteSlot {
  explicit SpriteSlot(cXObject* object);  // 80142DFC
  virtual ~SpriteSlot();  // 80142F8C
  void SetSprite(unsigned int, int sprite, int frames, bool reverse);  // 80142FC0
  void ChangeToSprite(int sprite);  // 8014308C
  void AlternateWithSprite(int sprite, int ms);  // 801430BC
  int Tick();  // 801430D4
  void ActivateForTicks(int ticks);  // 80143398
  void ActivateForLoops(int loops);  // 80143438
  void Reset();  // 80143464
  void Deactivate();  // 80145390
  int IsActive() const;  // 801453B4

  // Not ported (generated declarations):
  SpriteSlot(SlotDescriptor *, cXObject *);  // 80142EAC
  virtual void WriteSlot(SlotDescriptor *);  // 80142F60
  RET_UNKNOWN SetSprite(ObjSelector *);  // 80143030
  static RET_UNKNOWN GetTicksPerFrame();  // 80145364
  void ActivateForever();  // 8014536C
  void UseBalloonSprite(int);  // 801453CC
  void UseOverlaySprite(int);  // 801453D4
  void SetOptions(int);  // 801453DC
  void SetPopSize(float);  // 801453E4
  RET_UNKNOWN GetSelector() const;  // 801453EC
  RET_UNKNOWN GetSpriteID() const;  // 80145408
  RET_UNKNOWN GetBalloonSpriteID() const;  // 80145410
  RET_UNKNOWN GetOverlaySpriteID() const;  // 80145418
  RET_UNKNOWN GetCurrentFrame() const;  // 80145420
  RET_UNKNOWN GetPPerson();  // 80145428
  void SetPriority(int);  // 80145430
  RET_UNKNOWN GetPriority() const;  // 80145438
  RET_UNKNOWN GetOptions() const;  // 80145440
  RET_UNKNOWN GetIsSkillMeter() const;  // 80145448
  RET_UNKNOWN GetShowOverInactivePeople() const;  // 80145454
  void SetShowOverInactivePeople(bool);  // 8014545C
  double GetAltOffset() const;  // 80145464
  void SetAltOffset(float);  // 8014546C
  double GetSizeMultiplierX() const;  // 80145474
  double GetSizeMultiplierY() const;  // 8014547C
  double GetAlpha() const;  // 80145484

  // Options (+0x48).
  enum : int { kRise = 1, kSink = 2, kPop = 4, kFade = 8, kSkillMeter = 0x10, kEndWithFade = 0x20, kAlternate = 0x40 };

  float m_offset[3] = {};       // +0x04 Slot: x, y, z over the object (z rises/sinks)
  int m_ticks = 0;              // +0x10 ticks left (-1 for ever, 0 off)
  int m_tickCount = 0;          // +0x14
  int m_sprite = 0;             // +0x18 the sprite id (-1: the selector's thumbnail)
  int m_altSprite = 0;          // +0x1C
  int m_altPeriod = 400;        // +0x20 ms
  int m_altTime = 0;            // +0x24
  ObjSelector* m_selector = nullptr;  // +0x28
  int m_frames = 1;             // +0x2C
  int m_frame = 0;              // +0x30
  int m_frameStep = 1;          // +0x34
  int m_frameTicks = 0;         // +0x38 a frame every 12 ticks
  int m_balloon = -1;           // +0x3C the balloon sprite behind it
  int m_overlay = -1;           // +0x40 the overlay sprite over it
  int m_priority = 0;           // +0x44
  int m_options = 0;            // +0x48
  float m_baseZ = 0.0f;         // +0x4C
  float m_sizeX = 1.0f;         // +0x50
  float m_sizeY = 1.0f;         // +0x54
  float m_alpha = 1.0f;         // +0x58
  float m_popSize = 1.0f;       // +0x5C
  int m_overInactive = 0;       // +0x60 shown over people not selected
  cXObject* m_object = nullptr; // +0x64
};
