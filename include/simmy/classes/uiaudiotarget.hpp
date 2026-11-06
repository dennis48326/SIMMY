// @class UIAUDIOTarget after UIObjectBase
// The Flash movies' sounds (game/uisupport.cpp): a movie's setVariable("audio:NAME", ...) reaches this
// target through the UIRouter ("audio:" is routed by its prefix); NAME is looked up in UIAUDIO's installed
// entries (each screen target installs its names, e.g. "MMU_UI_BROWSE_SAMPLE") and that sample is played.
#pragma once

// PC: the UI sounds HUDTarget::HUDTarget (GC 8018591C) installs for its movie (HUD_UI_*_SAMPLE), in
// UIAUDIO under the target's name.
void InstallHUDSounds(const char* group);

// 0x84 bytes (EGlobal +0x130).
struct UIAUDIOTarget : UIObjectBase {
  UIAUDIOTarget();  // 80180348
  ~UIAUDIOTarget() override;  // 8018078C
  void SetVariable(char* name, char* value) override;  // 801808D0
};
