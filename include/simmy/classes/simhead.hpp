// @class ESims3DHead after E3DWindow
// A sim's head in the HUD's mood widget (game/simhead.cpp): a small 3D view (14 degree field of view)
// whose camera follows the sim's head bone (node 0xF), drawing just the head parts of the sim's model
// (head, face, hair, hat, the top of the torso, glasses) skinned by the sim's own animation, under two
// lights tinted for ghosts or green sims. Each ESim makes one (ESim +0x628); MODTarget's "MOD_head"
// draw callback draws player 1's and player 2's (Draw, vtable 0x1C). While the sim's model is being
// rebuilt only the frame is drawn (yellow; red for player 2).
#pragma once

// 0x3B8 bytes (GC: 16-aligned, zeroed, from the main heap); the vtable is at +0x3B0 on GC.
struct ESims3DHead : public E3DWindow {
  explicit ESims3DHead(ESim* sim);  // 8006D2FC
  static void InitShaders();  // 8006D35C
  static void ResetShaders();  // 8006D3CC
  void InitHead(cXPerson* person);  // 8006D418
  ~ESims3DHead() override;  // 8006D4C4
  virtual void Update();  // 8006D518
  virtual void Draw(ERC* rc);  // 8006D51C
  void Draw2D(ERC* rc, cXPerson* person);  // 8006E634

  cXPerson* m_person = nullptr;  // +0x350
  ESim* m_sim = nullptr;         // +0x354
  EMat4 m_lastHead;              // +0x358 last frame's head orientation (no translation)
  float m_width = 0.0f;          // +0x398 the viewport's size (0.0845 x 0.118 of the screen)
  float m_field39C = 0.0f;       // +0x39C
  float m_height = 0.0f;         // +0x3A0
  EVec3 m_pos;                   // +0x3A4 the viewport's top left (_p1head_xoff/_p2head_xoff, ...)

  static ERShader* m_pShd;         // the global shader (0x1a18ca65)
  static ERShader* m_pHeadBorder;  // the frame (0x1239c594)
};
