// Port of game/simimagemaker.cpp: SimImageMaker, a sim's portrait. See simimagemaker.hpp (the PC's
// deferred completion: the render surface's pixels come back through aurora's readback).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <dolphin/gx/GXAurora.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {
// The defaults ({anonymous} in game/simimagemaker.cpp, set by its static initialiser).
const EVec3 kDefaultCameraPos{-0.23f, 1.6f, 1.62f};
const EVec3 kDefaultCameraTarget{0.02f, 0.0f, 1.67f};
const EVec3 kZAxis{0.0f, 0.0f, 1.0f};  // _Z_AXIS
const EVec3 kDefaultLightAmbientColor{0.1f, 0.1f, 0.1f};
const EVec3 kDefaultDirLightColor[3] = {{0.6f, 0.6f, 0.8f}, {0.9f, 0.9f, 0.8f}, {1.2f, 1.2f, 1.2f}};
const EVec3 kDefaultDirLightDir[3] = {{-1.0f, -1.0f, -1.1f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.5f}};
EVec4 kDefaultBackgroundColor{1.0f, 0.0f, 1.0f, 1.0f};  // pink, the transparency key
const unsigned int kDefaultInitAnimID[2] = {0x1fb80af4u, 0xffa60350u};  // 80500608 by gender
const unsigned int kClearPink = 0xff00ff00u;  // kClearPinkArray (R, G, B, A)
const unsigned char kFullAlpha = 0xff;        // TextureUtils::s_fullAlphaValue
// The parts a portrait shows (803D52A0).
const int kPortraitParts[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0x12, 0x13, 0x14, 0x19};

void Normalize(EVec3& v) {
  if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) return;
  const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  v.x /= l, v.y /= l, v.z /= l;
}

// The pink background test of CopyToFinalImage: red and blue at least 0xA2, green at most 2.
bool IsPink(unsigned int rgba) {
  return (rgba >> 24) >= 0xa2 && ((rgba >> 16) & 0xff) <= 2 && ((rgba >> 8) & 0xff) >= 0xa2;
}

ETextureDef ImageDef(unsigned int w, unsigned int h, unsigned int bpp, const char* name) {
  ETextureDef def;
  def.m_field00 = 0;
  def.m_field04 = 0;
  def.m_width = static_cast<unsigned short>(w);
  def.m_height = static_cast<unsigned short>(h);
  def.m_mipCount = 0;
  def.m_name = name;
  def.m_bpp = static_cast<unsigned char>(bpp);
  if (bpp == 8) {
    def.m_flags = 0x80;
    def.m_paletteSize = 0x100;
    def.m_format = 0x8a;
    def.m_paletteFormat = 2;
    def.m_paletteBpp = 0x20;
  } else {
    def.m_flags = 0;
    def.m_paletteSize = 0;
    def.m_format = bpp == 0x10 ? 0x82 : 1;
    def.m_paletteFormat = 0;
    def.m_paletteBpp = 0;
  }
  return def;
}

void DestroyTexture(ETexture* t) {
  if (!t) return;
  if (_pGfx->NeedFlushToDestroy(t)) {
    // GC: _pGfx->Flush first (EGraphics::Flush); PC: aurora keeps what is in flight.
  }
  _pGfx->Destroy(t);
}

// PC: the deferred images.
struct PortraitJob {
  std::unique_ptr<SimImageMaker> maker;
  SimImageMaker::Deferred d;
  int stage = 0;  // 0 to draw, 1 waiting for the pixels
};
std::vector<PortraitJob*>& Jobs() {
  static std::vector<PortraitJob*> s_jobs;
  return s_jobs;
}

// CreateImage's GC drawing (8006EDE0): an immediate context, the window on its render surface, the
// global shader, the pink, the lights, the pose, SimRenderer::Render of the portrait parts, back to
// the screen. PC: the surface's copy is read back later (ServiceDeferredImages).
void DrawJob(PortraitJob* job) {
  SimImageMaker& m = *job->maker;
  ERC* rc = _pGfx->Open(RCMode{RCMode::Immediate});  // GC: _pGfx vtable 0x84 (0)
  E3DWindow window;
  m.InitWindow(window, rc);
  job->d.surface = window.m_surface;
  if (_globals.m_globalShader)
    if (EShader* s = _globals.m_globalShader->GetShader()) s->Select(rc, 0);
  EVec2 p0{0.0f, 0.0f}, p1{1.0f, 1.0f}, uv0{0.0f, 0.0f}, uv1{1.0f, 1.0f};
  rc->Rect(p0, p1, uv0, uv1, kDefaultBackgroundColor, 0.0f);
  rc->Lights(&m.m_lights);
  m.PoseSim(rc);
  SimRenderer::Render(rc, m.m_models, m.m_skinShader, m.m_anim.m_matrices,
                      static_cast<unsigned int>(m.m_anim.NodeCount()), true);
  // GC: _pGfx->Flush (vtable 0x44): the drawing is done before CreateImage returns, so the caller may
  // free the models at once. PC: aurora has read the models' vertex arrays once it returns.
  _pGfx->Flush();
  rc->RenderSurface(nullptr, 5);  // the surface copied into its texture (read back later)
  window.SetRenderSurface(nullptr, 5);
  window.Select(rc);
  _pGfx->Close(rc);
  // The models were only needed for this draw (GC: CreateImage's caller may free them right after).
  for (EIStaticModel*& p : m.m_models) p = nullptr;
  job->stage = 1;
}

// PC: the surface's texture's GPU copy read back (ENgcRenderSurface's readback); true once there.
bool SurfacePixelsReady(ERenderSurface* surface) {
  auto* s = static_cast<ENgcRenderSurface*>(surface);
  if (!s || !s->m_texture || !s->m_texture->m_impl || !s->m_readbackStream) return false;
  ENgcTextureBase* t = s->m_texture->m_impl;
  AuroraReadbackDest dest{0, t->m_image, GXGetTexBufferSize(t->m_def.m_width, t->m_def.m_height, GX_TF_RGBA8, 0, 0), GX_FALSE};
  return AuroraReadbackGet(s->m_readbackStream, &dest, 1) && dest.found;
}

// PC: a deferred image: a maker with `from`'s settings drawing at w x h (32 bits), the final texture
// (fw x fh, `bpp`) made now and blank (transparent).
PortraitJob* NewJob(const SimImageMaker& from, unsigned int w, unsigned int h, unsigned int fw, unsigned int fh,
            unsigned int bpp, bool halve) {
  auto* job = new PortraitJob;
  job->maker.reset(new SimImageMaker);
  SimImageMaker& m = *job->maker;
  m.m_male = from.m_male;
  m.m_skinShader = from.m_skinShader;
  std::memcpy(m.m_models, from.m_models, sizeof m.m_models);
  m.m_lights = from.m_lights;
  m.m_cameraPos = from.m_cameraPos;
  m.m_cameraTarget = from.m_cameraTarget;
  m.m_cameraUp = from.m_cameraUp;
  m.m_fov = from.m_fov;
  m.m_poseAnim = from.m_poseAnim;
  m.OverrideImageSize(w, h, halve ? 0x20 : bpp);
  job->d.width = w;
  job->d.height = h;
  job->d.finalWidth = fw;
  job->d.finalHeight = fh;
  job->d.bpp = bpp;
  job->d.sepia = from.m_sepia;
  job->d.halve = halve;
  job->d.background = from.m_background ? from.m_background->m_id : 0;
  ETextureDef def = ImageDef(fw, fh, bpp, "*SimImageMaker_final*");
  ETexture* t = _pGfx->CreateTexture(def);
  if (!t) {
    delete job;
    return nullptr;
  }
  const bool swizzled = TextureUtils::PrepareTextureForUpdateBegin(t);
  t->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Image});
  int pitch = 0, rows = 0;
  if (unsigned char* p = t->UpdateMipLevel(0, pitch, rows)) std::memset(p, 0, static_cast<size_t>(pitch) * rows);
  if (bpp == 8)
    if (unsigned char* pal = t->UpdatePaletteDeswizzled()) std::memset(pal, 0, 0x400);
  t->UpdateEnd();
  TextureUtils::PostUpdateEnd(t, swizzled);
  job->d.final = t;
  // GC draws inside CreateImage: the caller's models may be gone by the next frame (the genetics
  // screen deletes the parent's model right after CreateParentTexture). Off the app thread (none
  // known) the draw waits for ServiceDeferredImages.
  if (_pApp && _pApp->IsCallingThread()) DrawJob(job);
  Jobs().push_back(job);
  return job;
}

// PC development aid: SIMMY_PORTRAIT_DUMP=<prefix> writes each finished portrait as <prefix>N.bmp
// (32-bit, the palette applied; transparent pixels white).
void DumpPortrait(ETexture* t) {
  static const char* prefix = std::getenv("SIMMY_PORTRAIT_DUMP");
  static int count = 0;
  if (!prefix || !t || t->m_def.m_bpp != 8) return;
  const unsigned int w = t->m_def.m_width, h = t->m_def.m_height;
  const bool swizzled = TextureUtils::PrepareTextureForUpdateBegin(t);
  t->UpdateBegin(ETextureUpdateType{ETextureUpdateType::PaletteOnly});
  int pitch = 0, rows = 0;
  const unsigned char* px = t->UpdateMipLevel(0, pitch, rows);
  const unsigned char* pal = t->UpdatePaletteDeswizzled();
  std::vector<unsigned char> out(w * h * 4);
  for (unsigned int y = 0; y < h && px && pal; ++y)
    for (unsigned int x = 0; x < w; ++x) {
      const unsigned char* c = pal + px[y * static_cast<unsigned int>(pitch) + x] * 4u;
      unsigned char* o = &out[(y * w + x) * 4];  // BMP rows bottom up, as the texture's (game images are stored bottom row first)
      const bool clear = c[3] == 0;
      o[0] = clear ? 0xff : c[2], o[1] = clear ? 0xff : c[1], o[2] = clear ? 0xff : c[0], o[3] = 0xff;
    }
  t->UpdateEnd();
  TextureUtils::PostUpdateEnd(t, swizzled);
  char name[512];
  std::snprintf(name, sizeof name, "%s%d.bmp", prefix, count++);
  if (FILE* f = std::fopen(name, "wb")) {
    const unsigned int size = 54 + static_cast<unsigned int>(out.size());
    unsigned char hd[54] = {'B', 'M'};
    auto put = [&](int at, unsigned int v) { for (int i = 0; i < 4; ++i) hd[at + i] = static_cast<unsigned char>(v >> (8 * i)); };
    put(2, size), put(10, 54), put(14, 40), put(18, w), put(22, h);
    hd[26] = 1, hd[28] = 32;
    put(34, static_cast<unsigned int>(out.size()));
    std::fwrite(hd, 1, 54, f);
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    std::printf("[portrait] dumped %s\n", name);
  }
}
}  // namespace

// 8006E814
SimImageMaker::SimImageMaker() {
  m_cameraPos = kDefaultCameraPos;
  m_cameraTarget = kDefaultCameraTarget;
  m_cameraUp = kZAxis;
  Init();
}

// 8006E918
SimImageMaker::~SimImageMaker() { Shutdown(); }

// 8006E964
int SimImageMaker::Init() {
  SetToDefaultValues();
  return 1;
}

// 8006E988
void SimImageMaker::Shutdown() {
  if (m_background) {
    m_background->DelRef();
    m_background = nullptr;
  }
  for (EIStaticModel*& m : m_models) m = nullptr;
}

// 8006E9D8
void SimImageMaker::Reset() {
  Shutdown();
  Init();
  SetToDefaultValues();
}

// 8006EA14
// 32x32, 8 bits, not sepia, no models; the default camera (14 degrees), pose, ambient light and
// three directional lights (directions normalised).
void SimImageMaker::SetToDefaultValues() {
  SetGender(false);
  SetSkinShader(nullptr);
  m_sepia = 0;
  m_width = 0x20;
  m_height = 0x20;
  m_bpp = 8;
  for (EIStaticModel*& m : m_models) m = nullptr;
  m_poseAnim = 0xfccc90beu;
  m_cameraPos = kDefaultCameraPos;
  m_cameraTarget = kDefaultCameraTarget;
  m_cameraUp = kZAxis;
  m_fov = 14.0f;
  m_lights.m_ambient[0] = kDefaultLightAmbientColor.x;
  m_lights.m_ambient[1] = kDefaultLightAmbientColor.y;
  m_lights.m_ambient[2] = kDefaultLightAmbientColor.z;
  m_lights.m_numDir = 3;
  m_lights.m_numPoint = 0;
  for (int i = 0; i < 3; ++i) {
    EVec3 dir = kDefaultDirLightDir[i];
    Normalize(dir);
    for (int j = 0; j < 3; ++j) {
      m_lights.m_dir[i].m_color[j] = kDefaultDirLightColor[i][j];
      m_lights.m_dir[i].m_dir[j] = dir[j];
    }
  }
}

// 8006EBCC
void SimImageMaker::SetGender(bool male) { m_male = male ? 1 : 0; }
// 8006EBD4
void SimImageMaker::SetSkinShader(EShader* shader) { m_skinShader = shader; }
// 8006EBDC
void SimImageMaker::SetModel(eBodyPartS2C part, EIStaticModel* model) {
  if (part > 0x19) return;
  m_models[part] = model;
}

// 8006EBF4
// The portrait's parts (803D52A0) from the sim's model.
void SimImageMaker::SetPortraitModels(SimModel* model) {
  if (!model) return;
  for (int part : kPortraitParts) SetModel(eBodyPartS2C{part}, model->m_parts[part]);
}

// 8006ECAC
void SimImageMaker::OverrideImageSize(unsigned int width, unsigned int height, unsigned int bpp) {
  m_bpp = bpp;
  m_width = width;
  m_height = height;
}

// 8006ECBC
void SimImageMaker::OverrideLights(ELights& lights) { m_lights = lights; }

// 8006ED18
void SimImageMaker::OverridePose(unsigned int anim, EVec3& pos, EVec3& target, EVec3& up, float fov) {
  m_poseAnim = anim;
  m_cameraPos = pos;
  m_cameraTarget = target;
  m_cameraUp = up;
  m_fov = fov;
}

// 8006ED78
void SimImageMaker::OverrideBackground(unsigned int texture) {
  if (m_background) {
    m_background->DelRef();
    m_background = nullptr;
  }
  if (texture) m_background = static_cast<ERTexture*>(_textureman.AddRef(texture, nullptr, 0));
}

// 8006EDE0
// GC: an immediate render context; the window on a new render surface; the global shader; the
// surface filled pink; the lights; the sim posed and drawn; a texture of the surface's size gets the
// surface's pixels (CopyToTexture), the surface goes, the texture is flipped (and sepia toned) and
// becomes the final image. 32x32 images go through CreateImage32x32.
// PC: the final image (blank) at once; the drawing and the rest are done by ServiceDeferredImages
// (the draw on the next frame's draw, the copy once the pixels are back).
ETexture* SimImageMaker::CreateImage() {
  if (m_width == 0x20 && m_height == 0x20) return CreateImage32x32();
  PortraitJob* job = NewJob(*this, m_width, m_height, m_width, m_height, m_bpp, false);
  return job ? job->d.final : nullptr;
}

// 8006F25C
// A render surface of the image's size (cleared, 32 bits unless the image is 16), the 14 degree
// (m_fov) projection, the camera, selected. GC: the graphics flushed and VRAM discarded first.
void SimImageMaker::InitWindow(E3DWindow& window, ERC* rc) {
  ERenderSurfaceDef def;
  def.m_xSize = static_cast<int>(m_width);
  def.m_ySize = static_cast<int>(m_height);
  def.m_flags = 3;
  def.m_background[0] = def.m_background[1] = def.m_background[2] = 0.0f;
  def.m_field18 = m_bpp != 0x10 ? 1 : 0;
  def.m_field1C = 0;
  ERenderSurface* surface = _pGfx->CreateRenderSurface(def);
  // PC: the surface's copy is read back (ENgcRenderSurface's readback, armed before it is drawn).
  if (auto* s = static_cast<ENgcRenderSurface*>(surface)) {
    s->m_readbackWanted = true;
    if (!s->m_readbackStream) s->m_readbackStream = AuroraReadbackCreateStream();
  }
  window.SetProjection(m_fov, 1.0f, 0.05f, 5.0f);
  window.SetRenderSurface(surface, 5);
  window.SetLookAt(m_cameraPos, m_cameraTarget, m_cameraUp);
  window.Select(rc);
}

// 8006F408
// The gender's skeleton in the portrait pose at the origin (the controller's scale 1/4096, as the
// game's sims), computed for this frame; the matrices go to the context.
void SimImageMaker::PoseSim(ERC* rc) {
  m_anim.Init(kDefaultInitAnimID[m_male ? 1 : 0], eAnimatedObjectType{5}, 0);
  m_anim.SetTrackAnim(eTrackFlags{1}, m_poseAnim, -1, -1.0f, nullptr);
  m_anim.m_scale = 1.0f / 4096.0f;
  EVec3 pos{0.0f, 0.0f, 0.0f}, rot{0.0f, 0.0f, 0.0f}, scale{1.0f, 1.0f, 1.0f};
  EMat4 orient;
  EAnimController::CalcOrientMatrix(pos, rot, scale, orient);
  m_anim.m_field34 = -1;
  m_anim.m_computedAt = -1;
  m_anim.Compute(orient, _framecount);
  m_anim.m_computedAt = -1;
  m_anim.m_field34 = -1;
  rc->ModelMatrixList(m_anim.m_matrices, m_anim.NodeCount());
}

// 8006F538
// The image (32 bits) into the final texture of the size and depth asked (PC: m_finalTarget when
// set, else a new one): 32 bits: pink pixels clear (or the background's), the rest opaque; 16 bits
// (RGB5A3) likewise; 8 bits: the colours quantised to 255 (ERTQuantize, YUV), the last palette entry
// clear for the pink and the image's top row and right column. Null when it cannot be made.
ETexture* SimImageMaker::CopyToFinalImage(ETexture* image) {
  ETexture* bgTex = m_background ? m_background->m_texture : nullptr;
  if (bgTex && (bgTex->m_def.m_width != m_width || bgTex->m_def.m_height != m_height)) {
    m_background->DelRef();
    m_background = nullptr;
    bgTex = nullptr;
  }
  ETexture* out = m_finalTarget;
  if (!out) {
    ETextureDef def = ImageDef(m_width, m_height, m_bpp, "*SimImageMaker_final*");
    out = _pGfx->CreateTexture(def);
  }
  if (!out || !image) return out;
  const bool imageSwizzled = TextureUtils::PrepareTextureForUpdateBegin(image);
  const bool outSwizzled = TextureUtils::PrepareTextureForUpdateBegin(out);
  image->UpdateBegin(ETextureUpdateType{ETextureUpdateType::PaletteOnly});
  out->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Image});
  bool bgSwizzled = false;
  const unsigned char* bg = nullptr;
  const unsigned char* bgPalette = nullptr;
  int pitch = 0, rows = 0;
  if (bgTex) {
    bgSwizzled = TextureUtils::PrepareTextureForUpdateBegin(bgTex);
    bgTex->UpdateBegin(ETextureUpdateType{ETextureUpdateType::PaletteOnly});
    bg = bgTex->UpdateMipLevel(0, pitch, rows);
    if (bgTex->m_def.m_bpp == 8) bgPalette = bgTex->UpdatePaletteDeswizzled();
  }
  auto bgPixel = [&](unsigned int i) -> unsigned int {
    if (!bg) return 0;
    if (bgTex->m_def.m_bpp == 8) return bgPalette ? TextureUtils::GetPixel(bgPalette + bg[i] * 4u) : 0;
    if (bgTex->m_def.m_bpp == 0x20) return TextureUtils::GetPixel(bg + i * 4);
    return 0;
  };
  const unsigned int n = m_width * m_height;
  const unsigned char* src = image->UpdateMipLevel(0, pitch, rows);
  unsigned char* dst = out->UpdateMipLevel(0, pitch, rows);
  bool ok = src && dst;
  if (ok && m_bpp == 0x20) {
    for (unsigned int i = 0; i < n; ++i) {
      const unsigned int p = TextureUtils::GetPixel(src + i * 4);
      TextureUtils::PutPixel(dst + i * 4, !IsPink(p) ? (p & 0xffffff00u) | kFullAlpha : bgPixel(i));
    }
  } else if (ok && m_bpp == 0x10) {
    auto* d16 = reinterpret_cast<unsigned short*>(dst);
    auto* s16 = reinterpret_cast<const unsigned short*>(src);  // GC: a 16-bit source here
    for (unsigned int i = 0; i < n; ++i) {
      const unsigned short v = s16[i];
      const unsigned int r = (v >> 7) & 0xf8, g = (v >> 2) & 0xf8, b = (v << 3) & 0xff;
      if (r < 200 || g > 8 || b < 200) {
        d16[i] = v;
      } else if (bg) {
        const unsigned int p = bgPixel(i);
        d16[i] = static_cast<unsigned short>((p & 0xff) << 15 | (p >> 24) << 10 | ((p >> 16) & 0xff) << 5 | ((p >> 8) & 0xff));
      } else {
        d16[i] = 0;
      }
    }
  } else if (ok && m_bpp == 8) {
    std::unique_ptr<ERTQuantize> q(new ERTQuantize);
    if (!q->Init(0xff, 0x7c00, nullptr, nullptr, true)) {
      ok = false;
    } else {
      unsigned char* palette = out->UpdatePaletteDeswizzled();
      for (unsigned int i = 0; i < n; ++i) {
        unsigned char c[4] = {src[i * 4], src[i * 4 + 1], src[i * 4 + 2], src[i * 4 + 3]};
        q->AddPixel(c);
      }
      q->Compute();
      const unsigned int colors = q->GetFullPaletteSize();
      for (unsigned int i = 0; i < colors; ++i) {
        unsigned char c[4] = {};
        q->GetPaletteEntry(static_cast<int>(i), c);
        c[3] = kFullAlpha;
        std::memcpy(palette + i * 4, c, 4);
      }
      std::memset(palette + colors * 4, 0, 4);
      for (unsigned int y = 0; y < m_height; ++y)
        for (unsigned int x = 0; x < m_width; ++x) {
          const unsigned int i = y * m_width + x;
          unsigned char c[4] = {src[i * 4], src[i * 4 + 1], src[i * 4 + 2], src[i * 4 + 3]};
          if (x == m_width - 1 || y == 0) c[0] = 0xff, c[1] = 0, c[2] = 0xff;
          const unsigned int p = TextureUtils::GetPixel(c);
          dst[y * static_cast<unsigned int>(pitch) + x] =
              static_cast<unsigned char>(!IsPink(p) ? q->GetClosestColor(c) : colors);
        }
    }
  }
  image->UpdateEnd();
  out->UpdateEnd();
  image->SetSwizzleFlag();
  out->SetSwizzleFlag();
  (void)imageSwizzled;
  (void)outSwizzled;
  if (bgTex) {
    bgTex->UpdateEnd();
    TextureUtils::PostUpdateEnd(bgTex, bgSwizzled);
  }
  if (!ok && out != m_finalTarget) {
    DestroyTexture(out);
    return nullptr;
  }
  return out;
}

// 8006FE40
void SimImageMaker::UpdateRepShaders(int) {
  SIMMY_STUB("SimImageMaker::UpdateRepShaders (the EyeToy representation images)", 0x8006FE40);
}

// 80070154
// Rendered at 64x64 in 32 bits, halved (pixels mostly clear become the pink), then the final image.
// PC: the halving is in ServiceDeferredImages (Deferred::halve).
ETexture* SimImageMaker::CreateImage32x32() {
  PortraitJob* job = NewJob(*this, 0x40, 0x40, 0x20, 0x20, m_bpp, true);
  return job ? job->d.final : nullptr;
}

// PC: once a frame (ESimsApp::Update): an image not drawn yet (CreateImage called off the app thread)
// is drawn (DrawJob); once its pixels are back, the rest of CreateImage (and CreateImage32x32) runs into the
// texture handed out. An image whose pixels never come (no GPU: headless) stays blank.
void SimImageMaker::ServiceDeferredImages() {
  std::vector<PortraitJob*>& jobs = Jobs();
  for (size_t i = 0; i < jobs.size();) {
    PortraitJob* job = jobs[i];
    SimImageMaker& m = *job->maker;
    bool done = false;
    if (job->stage == 0) {
      DrawJob(job);
      ++i;
      continue;
    }
    if (SurfacePixelsReady(job->d.surface)) {
      // The rest of CreateImage: the surface's pixels into a texture, the surface gone, flipped,
      // (sepia,) the final image; CreateImage32x32: halved first.
      ETextureDef def = ImageDef(job->d.width, job->d.height, 0x20, "*SimImageMaker_temp*");
      def.m_flags = 0x80;
      ETexture* temp = _pGfx->CreateTexture(def);
      if (temp) static_cast<ENgcRenderSurface*>(job->d.surface)->CopyToTexture(temp);
      _pGfx->Destroy(job->d.surface);
      job->d.surface = nullptr;
      if (temp) {
        TextureUtils::VerticalFlip(temp);
        if (job->d.sepia) TextureUtils::MakeTextureSepiaTone(temp);
        if (job->d.background) m.OverrideBackground(job->d.background);
        if (job->d.halve) {
          m.m_finalTarget = nullptr;
          m.OverrideImageSize(job->d.width, job->d.height, 0x20);
          ETexture* full = m.CopyToFinalImage(temp);
          DestroyTexture(temp);
          temp = nullptr;
          if (full) {
            ETextureDef half = full->m_def;
            half.m_width = static_cast<unsigned short>(job->d.finalWidth);
            half.m_height = static_cast<unsigned short>(job->d.finalHeight);
            temp = _pGfx->CreateTexture(half);
            if (temp) TextureUtils::CopyTexture32To32Half(full, temp, 0x20, kClearPink);
            DestroyTexture(full);
          }
        }
        if (temp) {
          m.m_finalTarget = job->d.final;
          m.OverrideImageSize(job->d.finalWidth, job->d.finalHeight, job->d.bpp);
          m.CopyToFinalImage(temp);
          DestroyTexture(temp);
        }
      }
      DumpPortrait(job->d.final);
      std::printf("[portrait] %ux%u %u-bit image made (%d frames)\n", job->d.finalWidth, job->d.finalHeight, job->d.bpp,
                  job->d.frames);
      done = true;
    } else if (++job->d.frames > 600) {
      std::printf("[portrait] the image's pixels never came back (no GPU?): it stays blank\n");
      _pGfx->Destroy(job->d.surface);
      done = true;
    }
    if (done) {
      jobs.erase(jobs.begin() + static_cast<std::ptrdiff_t>(i));
      delete job;
    } else {
      ++i;
    }
  }
}
