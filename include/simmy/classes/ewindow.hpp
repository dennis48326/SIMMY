// @class EWindow after EMat4 EVec2
// A 2D view (engine/e_window.cpp): maps input coordinates (usually 0..1) to an output rectangle in
// pixels of the screen or a render surface, with a clip rectangle. Select() makes it current and sets
// the render context's surface, scissor and window matrix.
#pragma once

struct EWindow {
  EWindow();  // 802FFB98
  virtual ~EWindow();  // 802FFC80
  virtual void SetRenderSurface(ERenderSurface* surface, int output);  // 802FFCF0
  static void ScaleForScreenshot(bool, int, int, int, int);  // 802FFD98
  static void ScaleOutputRectForScreenshot(TRect<float> *);  // 802FFF10
  static void ScaleScissorRectForScreenshot(TRect<float> *, TRect<float> *);  // 802FFF94
  virtual void Select(ERC* rc);  // 80300040
  void TransformToPixel(EVec2 &, EVec2 &);  // 80300330
  void CalcWindowMat();  // 8030035C
  void CalcClip();  // 803004A0
  void CalcClipInv();  // 803005AC
  void SetClip(TRect<float> &);  // 80300600
  void SetInputCoordinates(TRect<float> &);  // 80300648
  void SetOutputCoordinates(TRect<float> &);  // 80300710
  void SetInputCoordinatesAndClip(TRect<float> &);  // 803008A0
  void SetRect(TRect<float> &);  // 803008DC
  void Transform(EVec2 &, EVec2 &);  // 80300924
  void Transform(float, float, float &, float &);  // 80300950
  void TransformInv(EVec2 &, EVec2 &);  // 80300974
  void TransformInv(float, float, float &, float &);  // 803009A8
  void TransformScale(float, float, float &, float &);  // 803009D4
  void TransformScale(EVec2 &, EVec2 &);  // 803009F0
  bool ClipTest(EVec2 &);  // 80300A14
  bool ClipTest(float, float);  // 80300A3C
  bool ClipTest(TRect<float> &);  // 80300A84
  static EWindow* GetCurrentWindow();  // 80300AE8
  static EWindow* GetCurrent3DWindow();  // 80300AF0
  virtual void WindowMatrixChanged();  // 80300AF8
  virtual void InputCoordinatesChanged();  // 80300AFC
  virtual void OutputCoordinatesChanged();  // 80300B00
  virtual EWindow* Cast3DWindow();  // 80300B04

  EMat4 m_mat;                       // +0x00 input -> output (scale and translate)
  TRect<float> m_input{0, 0, 1, 1};  // +0x40
  TRect<float> m_output{0, 0, 1, 1}; // +0x50
  TRect<float> m_clip{0, 0, 1, 1};   // +0x60 in input coordinates
  TRect<float> m_clipOut;            // +0x70 the clip in output coordinates
  TRect<float> m_clipVisible;        // +0x80 m_clipOut limited to the output rectangle
  ERenderSurface* m_surface = nullptr;  // +0x90 null: the screen
  int m_outputIndex = 5;             // +0x94
  int m_dirty = 1;                   // +0x98 output rectangle must be re-read on Select

  static EWindow* m_pCurrentWindow;
  static EWindow* m_pCurrent3DWindow;
  static float m_globalScale[2], m_globalOffset[2];  // screenshot tiling (ScaleForScreenshot)
};
