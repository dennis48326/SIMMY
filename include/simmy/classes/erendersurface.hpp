// @class ERenderSurfaceDef
// @class ERenderSurface after ERenderSurfaceDef
// @class ENgcRenderSurface after ERenderSurface
// Offscreen render targets (engine/e_rendersurface.cpp, e_ngcrendersurface.cpp): a window drawn
// into a surface (EWindow::SetRenderSurface) renders there instead of the screen; on GameCube the
// EFB region was then copied into the surface's texture (GXCopyTex) when the renderer switched away.
// Used by the credits, reflections, thumbnails and the EyeToy.
#pragma once

// The surface's description; ERenderSurface keeps a copy as its first 0x20 bytes.
struct ERenderSurfaceDef {
  int m_xSize = -1;             // +0x00
  int m_ySize = -1;             // +0x04
  unsigned int m_flags = 3;     // +0x08 2: clear to the background colour when selected
  float m_background[3] = {};   // +0x0C
  int m_field18 = 1;            // +0x18
  int m_field1C = 1;            // +0x1C
};

struct ERenderSurface : ERenderSurfaceDef {
  ERenderSurface();  // 80364DAC
  virtual ~ERenderSurface();  // 80364E24
  virtual int Create(ERenderSurfaceDef& def);  // 80364E58
  virtual int GetXSize();  // 80364EB8
  virtual int GetYSize();  // 80364EC0
  virtual void SetFlags(unsigned int flags);  // 80364EC8
  int SetSize(int x, int y, int field18);  // 80364ED0
  virtual void GetOutputRect(TRect<float>& out);  // 80364EE8
  virtual unsigned int GetFlags();  // 80364F5C
};

struct ENgcRenderSurface : ERenderSurface {
  ENgcRenderSurface();  // 8034C260
  ~ENgcRenderSurface() override;  // 8034C2A8
  void DestroySubtextures();  // 8034C2D8
  virtual ETexture* GetTexture();  // 8034C314
  int Create(ERenderSurfaceDef& def) override;  // 8034C31C
  void GetOutputRect(TRect<float>& out) override;  // 8034C3E4
  virtual void SetBackgroundColor(EVec3& color);  // 8034C404
  virtual void GetImageData(void* out);  // 8034C424
  virtual int CopyToTexture(ETexture* texture);  // 8034C45C
  virtual void Select();  // 8034C70C
  virtual void Deselect();  // 8034C718

  int m_selected = 0;              // +0x24
  ENgcTexture* m_texture = nullptr;  // +0x28 RGBA8, the copy's destination
  // PC: the copy lives on the GPU; CPU readers (GetImageData, CopyToTexture) get it back a few
  // frames later through aurora's readback.
  bool m_readbackWanted = false;
  unsigned long long m_readbackStream = 0;
};
