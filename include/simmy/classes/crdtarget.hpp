// @class structDrawCBparams after EVec2 EVec4
// @class UIUserDrawCBTable after structDrawCBparams
// @class CRDTarget after UIObjectBase structDrawCBparams ERenderSurface ETexture ERShader
// User-drawn Apt controls (game/uiuserdrawcb.cpp): a "CCRCB" custom control in a movie calls the
// callback installed under its name with the control's rectangle (Apt coordinates through the current
// matrix) and the Apt colour transform. The credits (CRDTarget, game/targets_s2c.cpp) are one: the
// scrolling text is drawn into "0CRD_credits_box".
#pragma once

#include <functional>
#include <string>
#include <vector>

// s_drawCBparams (80487DA4): what customControlRender passes to the callback.
struct structDrawCBparams {
  EVec2* topLeft = nullptr;      // +0x00
  EVec2* bottomRight = nullptr;  // +0x04
  EVec4* scale = nullptr;        // +0x08 the colour transform (r, g, b, a)
  EVec4* translate = nullptr;    // +0x0C
};

struct UIUserDrawCBTable {
  using Callback = std::function<void(ERC*, structDrawCBparams*)>;
  // UserDrawCBTableRecord (GC: 32 pooled list records; a hit moved to the front).
  struct Record {
    std::string name;   // +0x08
    Callback callback;  // +0x0C
    std::string group;  // +0x10 the target that installed it
  };
  UIUserDrawCBTable();  // 80181DD4
  virtual ~UIUserDrawCBTable();  // 80181E78
  bool InstallEntry(const char* name, Callback callback, const char* group);  // 8018204C
  bool UnInstallEntry(const char* name);  // 801820C0
  bool UnInstallAllName(const char* group);  // 80182104
  Record* FindEntry(const char* name);  // 80182174
  void ExecuteCallback(ERC* rc, const char* name);  // 801821D0
  static UIUserDrawCBTable* Instance();  // 80182284
  static void Shutdown();  // 801822C0
  // 80181F18 AddRecord, 80181F80 RemoveRecord: the record pool (a std::vector here, at most 32)

  std::vector<Record> m_records;
  static UIUserDrawCBTable* s_pInstance;
  static structDrawCBparams* s_drawCBparams;
};

// The credits screen (UI screen 0x2A) over the main menu: a background and four team pictures, and
// the credit text ("creditsblock0".."creditsblock9" NewUI strings) scrolling up through
// "0CRD_credits_box", one centred line at a time, fading in at the bottom and out at the top. Pad
// command 8 scrolls five times faster, 4 holds it.
struct CRDTarget : UIObjectBase {
  CRDTarget();  // 801A4070
  ~CRDTarget() override;  // 801A4848
  void SetVariable(char* name, char* value) override;  // 801A4A04
  char* GetVariable(char* name) override;  // 801A4AE4
  unsigned short* GetLocalizable(char* name) override;  // 801A4BE8
  void Draw(ERC* rc) override;  // 801A4C6C
  void Update() override;  // 801A4C70
  void ShowCredits();  // 801A4DC0
  void HideCredits();  // 801A4EEC
  static void UpdateCredits();  // 801A4F80
  void StartCredits();  // 801A4F84
  void EndCredits();  // 801A5000
  void SetupForCreditsRender(int);  // 801A50D0
  void InitCreditsTextFetch();  // 801A566C
  void ScrollingCallback(ERC* rc, structDrawCBparams* params);  // 801A56FC
  void ShowPhoto();  // 801A6108
  static void HidePhoto();  // 801A6188

  int m_exists = 0;          // +0x84 the movie asked for CRD_exists
  int m_shown = 0;           // +0x88
  int m_tweening = 0;        // +0x8C a show/hide tween is running
  int m_visible = 0;         // +0x90
  int m_photoFrame = 0;      // +0x94
  int m_photoOn = 0;         // +0x98
  int m_done = 0;            // +0x9C
  int m_running = 0;         // +0xA0 1 scrolling, 2 held
  float m_startTime = 0;     // +0xA4
  float m_fade = 0.4f;       // +0xA8 the part of the box (about the middle) at full alpha
  float m_speed = 1.0f;      // +0xAC
  int m_nextBlock = 0;       // +0xB0 the next "creditsblock<n>" to append
  float m_lineHeight = 0;    // +0xB4
  unsigned short* m_text = nullptr;  // +0xB8 0x1501 characters
  unsigned short* m_top = nullptr;   // +0xBC the first line still on screen
  int m_cheatOn = 0;         // +0xC0
  ETexture* m_maskTexture = nullptr;  // +0xC4 (SetupForCreditsRender)
  EWindow* m_window = nullptr;        // +0xC8
  int m_fieldCC = 0;         // +0xCC
  ERShader* m_shader = nullptr;       // +0xF0 (SetupForCreditsRender)
  ERenderSurface* m_surface = nullptr;  // +0xF4 (SetupForCreditsRender)
  float m_yBase = 0;         // yBaseOffset (static on GC): where the first line is drawn
};
