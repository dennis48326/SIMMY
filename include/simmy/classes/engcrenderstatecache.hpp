// @class ENgcRenderStateCache
// Everything the renderer wants GX to be set to (engine/e_ngcrenderstatecache.cpp). Renderer commands
// edit it and mark what changed in m_dirty; Apply then issues the GX calls. Offsets are the
// GameCube layout, kept in comments because the renderer's code refers to fields by them.
#pragma once

struct ENgcTevStage {          // 0x5C bytes
  int texCoord, texMap, channel;            // +0x00 GXSetTevOrder
  int colorA, colorB, colorC, colorD;       // +0x0C GXSetTevColorIn
  int colorOp, colorBias, colorScale;       // +0x1C GXSetTevColorOp
  unsigned char colorClamp; unsigned char pad0[3];
  int colorOut;
  int alphaA, alphaB, alphaC, alphaD;       // +0x30 GXSetTevAlphaIn
  int alphaOp, alphaBias, alphaScale;       // +0x40 GXSetTevAlphaOp
  unsigned char alphaClamp; unsigned char pad1[3];
  int alphaOut;
  int swapRas, swapTex;                     // +0x54 GXSetTevSwapMode
};
static_assert(sizeof(ENgcTevStage) == 0x5C);

struct ENgcChanCtrl {          // 0x18 bytes, GXSetChanCtrl
  unsigned char enable; unsigned char pad[3];
  int ambSrc, matSrc, lightMask, diffFn, attnFn;
};

struct ENgcTexGen {            // 0x1C bytes, GXSetTexCoordGen2
  int dst, type, src, mtx;
  unsigned char normalize; unsigned char pad[3];
  int postMtx;
  int envMapped;               // concatenate the environment-mapping matrix into the texture matrix
};

struct ENgcRenderStateCache {
  // Dirty bits (m_dirty, +0x6CC): 1 vertex desc, 2/4 position matrices, 8 texture matrices,
  // 0x10 post-texture matrices, 0x20 projection, 0x40 blend/z/alpha, 0x80 TEV, 0x100 lighting,
  // 0x200 viewport, 0x400 scissor; 0x80000000 = something changed.
  unsigned int m_currentMtx;          // +0x000 GXSetCurrentMtx
  int m_vtxDescPnMtx;                 // +0x004 GX_VA_PNMTXIDX
  int m_vtxDesc08[8];                 // +0x008 (texture matrix indices, unused here)
  int m_vtxDescPos;                   // +0x028 GX_VA_POS
  int m_vtxDescNrm;                   // +0x02C GX_VA_NRM
  int m_vtxDescClr0;                  // +0x030 GX_VA_CLR0
  int m_vtxDescClr1;                  // +0x034
  int m_vtxDescTex[6];                // +0x038 GX_VA_TEX0..5
  int m_field050[7];                  // +0x050
  unsigned char m_numTevStages;       // +0x06C
  unsigned char m_pad06D[3];
  ENgcTevStage m_tev[9];              // +0x070
  unsigned char m_zEnable; unsigned char m_pad3AD[3];   // +0x3AC GXSetZMode
  int m_zFunc;                        // +0x3B0
  unsigned char m_zUpdate; unsigned char m_pad3B5[3];   // +0x3B4
  int m_alphaComp0;                   // +0x3B8 GXSetAlphaCompare
  unsigned char m_alphaRef0; unsigned char m_pad3BD[3];
  int m_alphaOp;                      // +0x3C0
  int m_alphaComp1;                   // +0x3C4
  unsigned char m_alphaRef1;          // +0x3C8
  unsigned char m_zCompBeforeTex;     // +0x3C9 GXSetZCompLoc
  unsigned char m_pad3CA[2];
  GXColor m_ambColor;                 // +0x3CC GXSetChanAmbColor(GX_COLOR0A0)
  GXColor m_matColor;                 // +0x3D0 GXSetChanMatColor(GX_COLOR0A0)
  ENgcChanCtrl m_chan[4];             // +0x3D4
  GXLightObj m_lights[8];             // +0x434
  int m_matrixMode;                   // +0x6C8 0 = one position matrix, 1 = four (skinning)
  unsigned int m_dirty;               // +0x6CC
  unsigned int m_tevDirty;            // +0x6D0 one bit per stage (ENgcRSTevFlagMap)
  unsigned int m_lightDirty;          // +0x6D4 one bit per light, 0x100.. channels, 0x8000/0x10000 colors
  int m_blendType, m_blendSrc, m_blendDst, m_blendLogic;  // +0x6D8 GXSetBlendMode
  int m_field6E8;                     // +0x6E8
  unsigned int m_rasterModes;         // +0x6EC (0x40 = blending on)
  unsigned char m_numTexGens;         // +0x6F0
  unsigned char m_pad6F1[3];
  ENgcTexGen m_texGen[6];             // +0x6F4
  int m_cullMode;                     // +0x79C
  Mtx m_modelMtx[4];                  // +0x7A0
  Mtx m_viewMtx;                      // +0x860
  Mtx m_posMtx;                       // +0x890 view * model
  Mtx m_nrmMtx;                       // +0x8C0
  Mtx m_texMtx[2];                    // +0x8F0 GX_TEXMTX0, GX_TEXMTX1
  Mtx m_postTexMtx[2];                // +0x950 GX_PTTEXMTX0, GX_PTTEXMTX1
  float m_viewport[6];                // +0x9B0 x, y, w, h, near, far
  int m_scissor[4];                   // +0x9C8 x, y, w, h
  int m_projection;                   // +0x9D8 0 = 0..1 rect ortho, 1 = screen ortho, 2 = projMtx

  // The original's inline setters: store the value and mark what changed.
  static const unsigned int kTevFlag[16];    // ENgcRSTevFlagMap: stage -> m_tevDirty bit
  static const unsigned int kLightFlag[4];   // ENgcRSLightFlagMap: channel -> m_lightDirty bit
  ENgcTevStage& Tev(int stage) {
    m_dirty |= 0x80000080;
    m_tevDirty |= kTevFlag[stage];
    return m_tev[stage];
  }
  void SetTevOrder(int stage, int texCoord, int texMap, int channel) {
    ENgcTevStage& t = Tev(stage);
    t.texCoord = texCoord, t.texMap = texMap, t.channel = channel;
  }
  void SetTevColorIn(int stage, int a, int b, int c, int d) {
    ENgcTevStage& t = Tev(stage);
    t.colorA = a, t.colorB = b, t.colorC = c, t.colorD = d;
  }
  void SetTevAlphaIn(int stage, int a, int b, int c, int d) {
    ENgcTevStage& t = Tev(stage);
    t.alphaA = a, t.alphaB = b, t.alphaC = c, t.alphaD = d;
  }
  void SetTevColorOp(int stage, int op, int bias, int scale, bool clamp, int out) {
    ENgcTevStage& t = Tev(stage);
    t.colorOp = op, t.colorBias = bias, t.colorScale = scale, t.colorClamp = clamp, t.colorOut = out;
  }
  void SetTevAlphaOp(int stage, int op, int bias, int scale, bool clamp, int out) {
    ENgcTevStage& t = Tev(stage);
    t.alphaOp = op, t.alphaBias = bias, t.alphaScale = scale, t.alphaClamp = clamp, t.alphaOut = out;
  }
  void SetTevSwapMode(int stage, int ras, int tex) {
    m_dirty |= 0x80000080;
    m_tevDirty |= 0x10000;
    m_tev[stage].swapRas = ras, m_tev[stage].swapTex = tex;
  }
  void SetBlendMode(int type, int src, int dst, int logic) {
    m_blendLogic = logic, m_blendType = type, m_blendSrc = src;
    m_dirty |= 0x80000040;
    m_blendDst = dst;
  }
  void SetChanCtrl(int chan, bool enable, int ambSrc, int matSrc, unsigned int mask, int diffFn, int attnFn) {
    m_dirty |= 0x80000100;
    m_lightDirty |= kLightFlag[chan];
    m_chan[chan] = {enable, {}, ambSrc, matSrc, static_cast<int>(mask), diffFn, attnFn};
  }
  void SetLight(int slot, const GXLightObj& light) {
    m_dirty |= 0x80000100;
    m_lightDirty |= 1u << slot;
    m_lights[slot] = light;
  }
  void SetAmbColor(GXColor c) {
    m_ambColor = c;
    m_dirty |= 0x80000100;
    m_lightDirty |= 0x8000;
  }
  void SetMatColor(GXColor c) {
    m_dirty |= 0x80000100;
    m_lightDirty |= 0x10000;
    m_matColor = c;
  }
  void SetZMode(bool enable, int func, bool update) {
    m_zFunc = func, m_zUpdate = update, m_zEnable = enable;
    m_dirty |= 0x80000040;
  }
  void SetAlphaCompare(int comp0, int ref0, int op, int comp1, int ref1) {
    m_alphaComp0 = comp0, m_alphaRef0 = static_cast<unsigned char>(ref0), m_alphaOp = op;
    m_alphaComp1 = comp1, m_alphaRef1 = static_cast<unsigned char>(ref1);
    m_dirty |= 0x80000040;
  }
  void SetZCompLoc(bool beforeTex) {
    m_zCompBeforeTex = beforeTex;
    m_dirty |= 0x80000040;
  }
  void SetCurrentMtx(unsigned int id) {
    m_currentMtx = id;
    m_dirty |= 0x80000000;
  }
  void SetProjection(int which) {
    m_projection = which;
    m_dirty |= 0x80000020;
  }
  // Vertex descriptors for direct position/normal/colour/uv0 (the 2D primitives' layout).
  void SetDirectVtxDesc() {
    m_vtxDescTex[5] = 0, m_vtxDescPnMtx = 0, m_vtxDescPos = 0;
    m_dirty |= 0x80000001;
    m_vtxDescNrm = 0, m_vtxDescClr0 = 0;
    for (int i = 0; i < 5; ++i) m_vtxDescTex[i] = 0;
    m_vtxDescPos = 1, m_vtxDescNrm = 1, m_vtxDescClr0 = 1, m_vtxDescTex[0] = 1;
  }

  ENgcRenderStateCache& operator=(ENgcRenderStateCache& other);
  void SaveState();
  void RestoreState();
  void SaveViewportState();
  void RestoreViewportState();
  void Apply(bool skinned);

  static Mtx44 rectOrthoMtx;
  static Mtx44 screenOrthoMtx;
  static Mtx44 projMtx;
};
