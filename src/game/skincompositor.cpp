// Port of game/skincompositor.cpp: a sim's skin map quadrant composited on the CPU from the skin
// colour's base skin and the textures of the parts drawn in that quadrant. PC: the result is copied
// into a 32-bit RGBA skin texture (the GameCube quantised it to 8 bits, CopyTexture32To8); tattoos are
// not ported (CasSimPartsS2C::GetTattooTextureID is a stub returning none).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>

namespace {
short Multiplier(int lowRes) { return lowRes ? gSimPartsMapSizeMultiplier_Game : gSimPartsMapSizeMultiplier_CAS; }
}  // namespace

// 80075B10
SkinCompositor::SkinCompositor() { Reset(); }

// 80075B70
SkinCompositor::~SkinCompositor() { Reset(); }

// 80075BB0
void SkinCompositor::Reset() {
  if (m_scratch) {
    _pGfx->Destroy(m_scratch);  // GC: flushes the GPU first when it still uses it
    m_scratch = nullptr;
  }
  delete[] m_mask;
  m_mask = nullptr;
  delete m_simParts;
  m_simParts = nullptr;
}

// 80075C80
void SkinCompositor::CreateSkinQuadrantHiRes(ERTexture* target, unsigned char quadrant, CasSimDescriptionS2C* desc) {
  CreateSkin(target, quadrant, desc, false);
}

// 80075CA4
void SkinCompositor::CreateSkinQuadrantLoRes(ERTexture* target, unsigned char quadrant, CasSimDescriptionS2C* desc) {
  CreateSkin(target, quadrant, desc, true);
}

// 80075CC8
// A saved sim's quadrant: the stored map `id`, when it has the skin maps' size (1: done).
int SkinCompositor::CreateSkinQuadrantPrecomposited(ERTexture* target, unsigned int id, bool highRes) {
  if (!target || !target->m_texture || !id) return 0;
  if (id == target->m_id) return 1;
  auto* res = static_cast<ERTexture*>(_textureman.AddRef(id, nullptr, 0));
  if (!res || !res->m_texture) return 0;
  ETextureDef def;
  SimModel::GetSkinTextureDef(def, highRes);
  int ok = 0;
  if (res->m_texture->m_def.m_width == def.m_width && res->m_texture->m_def.m_height == def.m_height)
    ok = TextureUtils::LoadTexture(id, target->m_texture);
  res->DelRef();
  return ok;
}

// 80075DEC
void SkinCompositor::CreateSkin(ERTexture* target, unsigned char quadrant, CasSimDescriptionS2C* desc, bool lowRes) {
  m_lowRes = lowRes ? 1 : 0;
  m_desc = desc;
  m_target = target;
  m_quadrant = quadrant;
  m_simParts = new CasSimPartsS2C();
  m_simParts->SetGender(desc->m_male != 0);
  m_simParts->m_lowRes = m_lowRes;
  InitScratchpadTextures(static_cast<unsigned short>(gSimPartsMapSize * Multiplier(m_lowRes)));
  InitSkinTexture();
  LoadAllTextureLayers();
  FinalizeSkinTexture();
}

// 80075E8C
// The 32-bit scratch map and the reflection mask, `size` pixels square.
void SkinCompositor::InitScratchpadTextures(unsigned short size) {
  ETextureDef def;
  def.m_flags = 0x80;
  def.m_width = def.m_height = size;
  def.m_format = 1;
  def.m_bpp = 0x20;
  def.m_name = "*skincompositor*";
  if (m_scratch && (m_scratch->m_def.m_width != size || m_scratch->m_def.m_height != size)) {
    _pGfx->Destroy(m_scratch);
    m_scratch = nullptr;
  }
  if (!m_scratch) m_scratch = _pGfx->CreateTexture(def);
  delete[] m_mask;
  m_mask = new unsigned char[static_cast<unsigned int>(size) * size];
}

// 80076000
void SkinCompositor::InitSkinTexture() {
  m_size = static_cast<unsigned short>(gSimPartsMapSize * Multiplier(m_lowRes));
  m_targetIs8Bit = (m_target && m_target->m_texture && m_target->m_texture->m_def.m_paletteFormat) ? 1 : 0;
}

// 8007604C
void SkinCompositor::FinalizeSkinTexture() {
  if (!m_target || !m_target->m_texture) return;
  if (!m_targetIs8Bit) TextureUtils::CopyTexture32To32(m_scratch, m_target->m_texture);
  else TextureUtils::CopyTexture32To8(m_scratch, m_target->m_texture);
}

// 80076098
// The quadrant's base skin (map 2, hair and accessories, starts transparent), its tattoo, then each of
// its parts' textures in order, and the reflection masks into the alpha.
void SkinCompositor::LoadAllTextureLayers() {
  if (!m_scratch) return;
  unsigned int parts[26] = {};
  unsigned int count = 0;
  const auto torso = Sim__OuterLayerTorsoStyleS2C{m_desc->m_outerOption1};
  const auto collar = Sim__OuterLayerCollarStyleS2C{m_desc->m_outerOption2};
  const unsigned int outer = static_cast<unsigned int>(m_desc->m_outerTop);
  switch (m_quadrant) {
    case 1:
      parts[count++] = 7;
      parts[count++] = 9;
      if (!m_simParts->OuterLayerTorsoIsAHood(outer, torso)) parts[count++] = 0xc;
      break;
    case 0:
      for (unsigned int p : {8u, 0xbu, 0xeu, 0x10u, 0x12u}) parts[count++] = p;
      break;
    case 2:
      for (unsigned int p : {2u, 3u, 4u, 5u, 6u, 0x13u, 0x19u}) parts[count++] = p;
      if (m_simParts->OuterLayerTorsoIsAHood(outer, torso)) parts[count++] = 0xc;
      else if (m_simParts->OuterLayerCollarIsAHood(outer, collar)) parts[count++] = 0xd;
      for (unsigned int p : {0u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u}) parts[count++] = p;
      break;
    case 3:
      parts[count++] = 0xf;
      break;
    default:
      break;
  }
  unsigned int skin = 0, tattoos[3] = {}, textures[26] = {}, reflections[26] = {};
  GetTextureIDs(skin, tattoos, textures, reflections);
  int pitch, rows;
  if (m_quadrant == 2) {
    const bool swizzled = TextureUtils::PrepareTextureForUpdateBegin(m_scratch);
    m_scratch->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Image});
    if (unsigned char* p = m_scratch->UpdateMipLevel(0, pitch, rows)) std::memset(p, 0, static_cast<size_t>(m_size) * m_size * 4);
    m_scratch->UpdateEnd();
    TextureUtils::PostUpdateEnd(m_scratch, swizzled);
  } else {
    TextureUtils::LoadTexture(skin, m_scratch);
  }
  const bool swizzled = TextureUtils::PrepareTextureForUpdateBegin(m_scratch);
  m_scratch->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Full});
  m_pixels = m_scratch->UpdateMipLevel(0, pitch, rows);
  std::memset(m_mask, 0, static_cast<size_t>(m_size) * m_size);
  unsigned int tattooPart = 0xf, tattoo = 0;
  switch (m_quadrant) {
    case 1: tattooPart = 7, tattoo = tattoos[1]; break;
    case 0: tattooPart = 8, tattoo = tattoos[0]; break;
    case 3: tattoo = tattoos[2]; break;
    default: break;
  }
  if (m_pixels) {
    if (tattoo) BlendTexture(eBodyPartS2C{static_cast<int>(tattooPart)}, tattoo, 0);
    for (unsigned int i = 0; i < count; ++i) {
      const unsigned int p = parts[i];
      if (!textures[p]) continue;
      if (textures[p] == reflections[p]) {
        std::fprintf(stderr, "SkinCompositor: texture and reflection mask are the same for body part %u\n", p);
        continue;
      }
      BlendTexture(eBodyPartS2C{static_cast<int>(p)}, textures[p], reflections[p]);
    }
    GetFinalReflectionMask();
  }
  m_scratch->UpdateEnd();
  TextureUtils::PostUpdateEnd(m_scratch, swizzled);
  m_pixels = nullptr;
}

// 80076524
void SkinCompositor::GetTextureIDs(unsigned int& skin, unsigned int* tattoos, unsigned int* textures, unsigned int* reflections) {
  skin = m_simParts->GetSkinTextureID(eSkinColorType{m_desc->m_skinColor}, eBodyTypeS2C{m_desc->m_bodyType},
                                      eSimPartsMapLocation{m_quadrant});
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) {
    textures[p] = m_simParts->GetTextureID(eBodyPartS2C{p}, m_desc, false);
    reflections[p] = m_simParts->GetReflectionTextureID(eBodyPartS2C{p}, m_desc);
  }
  for (int t = 0; t < 3; ++t)
    tattoos[t] = static_cast<unsigned int>(
        m_simParts->GetTattooTextureID(eTattooTextureTypeS2C{}, static_cast<unsigned int>(m_desc->m_tattoos[t])));
}

namespace {
// A texture resource's pixels for reading: 32-bit RGBA, or palette indices with their RGBA palette.
struct ReadLock {
  ERTexture* res = nullptr;
  ETexture* tex = nullptr;
  const unsigned char* pixels = nullptr;
  const unsigned char* palette = nullptr;  // 8-bit textures
  bool swizzled = false;
  explicit ReadLock(unsigned int id) {
    res = id ? static_cast<ERTexture*>(_textureman.AddRef(id, nullptr, 0)) : nullptr;
    tex = res ? res->m_texture : nullptr;
    if (!tex) return;
    swizzled = TextureUtils::PrepareTextureForUpdateBegin(tex);
    tex->UpdateBegin(ETextureUpdateType{ETextureUpdateType::PaletteOnly});
    int pitch, rows;
    pixels = tex->UpdateMipLevel(0, pitch, rows);
    if (tex->m_def.m_paletteFormat)
      palette = tex->m_def.m_format == 0x8a ? tex->UpdatePaletteDeswizzled() : tex->UpdatePalette();
  }
  ~ReadLock() {
    if (tex) {
      tex->UpdateEnd();
      TextureUtils::PostUpdateEnd(tex, swizzled);
    }
    if (res) res->DelRef();
  }
  bool Is8Bit() const { return tex->m_def.m_paletteFormat != 0; }
  unsigned int Pixel(unsigned int i) const {
    return Is8Bit() ? TextureUtils::GetPixel(palette + pixels[i] * 4u) : TextureUtils::GetPixel(pixels + i * 4);
  }
  unsigned char Mask(unsigned int i) const {
    return Is8Bit() ? static_cast<unsigned char>(TextureUtils::GetPixel(palette + pixels[i] * 4u)) : pixels[i];
  }
};
}  // namespace

// 80076618
// The part's texture into its rectangle of the map (map 2: copied where opaque; otherwise blended by
// its alpha), and its reflection mask (or its alpha) into the mask where it is drawn.
void SkinCompositor::BlendTexture(eBodyPartS2C part, unsigned int texture, unsigned int reflection) {
  const SimPartsMapEntry& e = SimModel::s_SimPartsMap[part];
  unsigned int x0 = e.m_x0, y0 = e.m_y0, x1 = e.m_x1, y1 = e.m_y1;
  if (m_quadrant == 2 && (part == 0xc || part == 0xd)) x0 = y0 = 0x40, x1 = y1 = 0x80;  // a hood
  const unsigned int mul = static_cast<unsigned int>(Multiplier(m_lowRes));
  const unsigned int left = x0 * mul, top = y0 * mul;
  unsigned int w = (x1 * mul - left) & 0xffff, h = (y1 * mul - top) & 0xffff;
  const unsigned int mapW = m_scratch->m_def.m_width;
  ReadLock src(texture);
  if (!src.tex || !src.pixels || (src.Is8Bit() && !src.palette)) return;
  const unsigned int srcW = src.tex->m_def.m_width, srcH = src.tex->m_def.m_height;
  if (w != srcW || h != srcH) {
    w = w < srcW ? w : srcW;
    h = h < srcH ? h : srcH;
  }
  unsigned int maskW = 0;
  ReadLock* mask = nullptr;
  ReadLock reflLock(reflection);
  if (reflection) {
    if (!reflLock.tex || !reflLock.pixels || (reflLock.Is8Bit() && !reflLock.palette)) return;
    maskW = reflLock.tex->m_def.m_width;
    if (maskW != srcW || reflLock.tex->m_def.m_height != srcH) {
      std::fprintf(stderr, "SkinCompositor: reflection mask size does not match texture %08X\n", texture);
      return;
    }
    mask = &reflLock;
  }
  for (unsigned int y = 0; y < h; ++y) {
    if (top + y >= mapW) break;
    for (unsigned int x = 0; x < w && left + x < mapW; ++x) {
      const unsigned int s = src.Pixel(y * srcW + x);
      if (!(s & 0xff)) continue;
      const unsigned int di = (top + y) * mapW + left + x;
      unsigned char* d = m_pixels + di * 4;
      if (m_quadrant == 2) TextureUtils::PutPixel(d, s);
      else TextureUtils::PutPixel(d, TextureUtils::BlendPixels32(TextureUtils::GetPixel(d), s));
      m_mask[di] = mask ? mask->Mask(y * maskW + x) : static_cast<unsigned char>(s);
    }
  }
}

// 80076DA4
// The reflection mask's top three bits into the map's alpha.
void SkinCompositor::GetFinalReflectionMask() {
  for (unsigned int i = 0; i < static_cast<unsigned int>(m_size) * m_size; ++i) m_pixels[i * 4 + 3] |= m_mask[i] & 0xe0;
}
