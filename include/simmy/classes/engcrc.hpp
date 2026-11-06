// @class ENgcRC after ERC ENgcShader
// GameCube render context (engine/e_ngcrc.cpp). In immediate mode most commands skip the display list
// and call the renderer's handler directly with an entry built on the stack.
#pragma once

struct ENgcRC : public ERC {
  ENgcRC();  // 80338334
  virtual ~ENgcRC();  // 80338370
  virtual void Init(RCMode mode);  // 803383A0
  virtual void EndCommand();  // 803383C0
  virtual void TriStrip(EGEVert *, int);  // 80338428
  virtual void TriStrip(int, float *, float *, unsigned char *, signed char *, unsigned char *, bool);  // 80338484
  virtual RET_UNKNOWN TriStripRef(int, float *, float *, unsigned char *, signed char *, unsigned char *, bool);  // 803387D8
  virtual RET_UNKNOWN TriStrip(int, int, unsigned short *, float *, float *, unsigned char *, signed char *, unsigned char *);  // 803389D0
  virtual RET_UNKNOWN TriStripRef(int, int, unsigned short *, float *, float *, unsigned char *, signed char *, unsigned char *);  // 80338CDC
  virtual RET_UNKNOWN TriStrip(int, short *, short *, unsigned char *, signed char *, unsigned char *, bool);  // 80338EBC
  virtual RET_UNKNOWN TriStripRef(int, short *, short *, unsigned char *, signed char *, unsigned char *, bool);  // 803391C4
  virtual RET_UNKNOWN TriStrip(int, int, unsigned short *, short *, short *, unsigned char *, signed char *, unsigned char *);  // 803393DC
  virtual RET_UNKNOWN TriStripRef(int, int, unsigned short *, short *, short *, unsigned char *, signed char *, unsigned char *);  // 803396E4
  virtual void TextureCacheInvalidate();  // 803398CC
  virtual void TriStripDL(unsigned char *, int, int, float *, float *, unsigned char *, signed char *);  // 80339928
  virtual void TriStripDLRef(unsigned char* dl, int dlSize, int count, float* positions, float* uvs, unsigned char* colors, signed char* normals, bool twoUVs);  // 80339B58
  virtual RET_UNKNOWN TriStripDL(unsigned char *, int, int, short *, short *, unsigned char *, signed char *);  // 80339CBC
  virtual void TriStripDLRef(unsigned char* dl, int dlSize, int count, short* positions, short* uvs, unsigned char* colors, signed char* normals, bool twoUVs);  // 80339E38
  virtual RET_UNKNOWN TriList(int, short *, short *, unsigned char *, signed char *, unsigned char *, bool);  // 80339F98
  virtual RET_UNKNOWN TriList(int, float *, float *, unsigned char *, signed char *, unsigned char *, bool);  // 8033A1BC
  virtual void ScreenTriList(int, float *, float *, unsigned char *);  // 8033A1C0
  virtual void ReadMetrics();  // 8033A1C4
  virtual void Shader(ENgcShader *, unsigned int);  // 8033A220
  virtual void ScrambleRect(EVec2 &, EVec2 &, float);  // 8033A268
  virtual void ModelMatrixList(EMat4 *, int);  // 8033A290
  virtual void ModelMatrixIndex(int, int);  // 8033A2BC
  virtual void ModelMatrices(EMat4 *, int);  // 8033A310
  virtual void DisplayList(EDL *);  // 8033A3B0
  virtual void Texture(ETexture *, int);  // 8033A430
  virtual void SetMipMap(float, int);  // 8033A480
  virtual void EnableGeometryModes(unsigned int);  // 8033A484
  virtual void DisableGeometryModes(unsigned int);  // 8033A4BC
  virtual void SetGeometryModes(unsigned int);  // 8033A4F8
  virtual void EnableRasterModes(unsigned int, int);  // 8033A530
  virtual void DisableRasterModes(unsigned int, int);  // 8033A56C
  virtual void SetRasterModes(unsigned int, int);  // 8033A5AC
  virtual void Viewport(EViewport *);  // 8033A5E8
  virtual void ClipRect(TRect<float> &);  // 8033A620
  virtual void Scissor(TRect<float> *);  // 8033A678
  virtual void ViewMatrix(EMat4 *, int, float);  // 8033A6B0
  virtual void ProjectionMatrix(EMat4 *);  // 8033A6E8
  virtual void WindowMatrix(EMat4 *);  // 8033A720
  virtual void SaveState();  // 8033A758
  virtual void RestoreState();  // 8033A78C
  virtual void Lights(ELights *);  // 8033A7C0
  virtual void Material(EMaterial *);  // 8033A81C
  virtual void Rect(EVec2 &, EVec2 &, EVec2 &, EVec2 &, EVec4 &, float);  // 8033A854
  virtual void RectList(int count, float* rects, EVec4& color, float z);  // 8033A8F0
  virtual void RectListRot(int count, float* rects, EVec4& color, float z);  // 8033A958
  virtual RET_UNKNOWN DirectRect(EVec2 &, EVec2 &, EVec4 &, float);  // 8033A9C0
  virtual RET_UNKNOWN EnvironmentMap(bool, bool, int);  // 8033AA38
  virtual void TextureMatrix(EMat4 *, ETCTransformSource, bool, bool, int);  // 8033AACC
  virtual RET_UNKNOWN RecalcMatrices(int, int);  // 8033AB70
  virtual void GeometrySetup();  // 8033ABAC
  virtual void ZTest(bool, int, int, int);  // 8033ABB0
  virtual RET_UNKNOWN ZClear(float, float, float, float, float);  // 8033ABF4
  virtual void AlphaTest(bool enable, int compare, float ref, int pass);  // 8033AE4C
  virtual RET_UNKNOWN Stencil(int, int, int);  // 8033AEC8
  virtual void SetBlendMode(int, int, int, int, float, int);  // 8033AF08
  virtual void SetCombineMode(int mode, int layer);  // 8033AF80
  virtual void SetAlpha(float);  // 8033AFBC
  virtual void RenderSurface(ERenderSurface *, int);  // 8033AFE4
  virtual void QuadList(int, float *, float *, unsigned char *, signed char *, bool);  // 8033B020
  virtual RET_UNKNOWN SpriteList(int, float *, float *, unsigned char *, signed char *, unsigned char *);  // 8033B304
  virtual void LineList(EGEVert *, int);  // 8033B350
  virtual void Debug(unsigned int, unsigned int);  // 8033B3AC
  void ParticleListInfo(EGEParticleListInfo* info) override;  // 8033B3B0
  void* ParticleListBegin(unsigned int count, unsigned int type) override;  // 8033B3B4
  virtual void ParticleListEnd(unsigned int);  // 8033B3DC
};
