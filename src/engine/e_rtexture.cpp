// Port of engine/e_rtexture.cpp: a texture resource ('TXFL', version 9).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>
#include <vector>

// 80320B28
ERTexture::ERTexture() {}
// 80320B6C
ERTexture::~ERTexture() { Deallocate(); }

// 80320BCC
void ERTexture::Attach(ETexture* texture) {
  Deallocate();
  m_texture = texture;
}

// 80320C00
void ERTexture::Deallocate() {
  _pGfx->Destroy(m_texture);
  m_texture = nullptr;
}

// 80320C50
void ERTexture::Refresh(EFile* file) {
  if (!file) return;
  if (m_texture) _pGfx->Flush();
  Load(*file);
  if (m_texture) _pGfx->Flush();
}

// 80320D00
void ERTexture::Load(EFile& file) {
  EDataHeader header;
  if (!header.Read(file, 0x5458464C /* 'TXFL' */, 9, 9)) return;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  std::vector<unsigned char> data(header.m_dataSize);
  file.Read(data.data(), header.m_dataSize);
  LoadFromMemory(data.data());
}

// 80320DF0
void ERTexture::Load(unsigned char* data) { LoadFromMemory(data + 4); }

// 80320E14
// `data` = ETextureDef (0x20 bytes, big-endian), the pixels of each mip level, then the palette.
void ERTexture::LoadFromMemory(unsigned char* data) {
  ETextureDef def;
  def.ReadBE(data);
  def.m_name = "";
  const unsigned char* src = data + ETextureDef::kFileSize;
  m_flags = ((def.m_width * def.m_height * def.m_bpp + def.m_paletteSize * def.m_paletteBpp) / 8) << 8 | (m_flags & 0xff);
  if (!m_texture) m_texture = _pGfx->CreateTexture(def);
  else m_texture->Create(def);
  if (!m_texture) return;
  m_texture->UpdateBegin(ETextureUpdateType::Image);
  const int levels = def.m_mipCount > 1 ? def.m_mipCount : 1;
  int w = def.m_width, h = def.m_height;
  for (int level = 0; level < levels; ++level) {
    int pitch, rows;
    unsigned char* dst = m_texture->UpdateMipLevel(level, pitch, rows);
    const int rowBytes = (w * def.m_bpp + 7) >> 3;
    if (rowBytes == pitch) {
      std::memcpy(dst, src, static_cast<size_t>(pitch) * h);
      src += static_cast<size_t>(pitch) * h;
    } else {
      for (int y = 0; y < h; ++y, dst += pitch, src += rowBytes) std::memcpy(dst, src, rowBytes);
    }
    w >>= 1;
    h >>= 1;
  }
  if (def.m_paletteFormat)
    if (unsigned char* pal = m_texture->UpdatePalette()) std::memcpy(pal, src, (def.m_paletteSize * def.m_paletteBpp + 7) >> 3);
  m_texture->UpdateEnd();
}

// 803210F4
// Not while the renderer still has it bound (the render-state cache is not ported yet).
int ERTexture::IsSafeToDelete() { return 1; }

// 803213C8
void ERTexture::Select(ERC*, int) { SIMMY_STUB("ERTexture::Select (needs ERC)", 0x803213C8); }

// 803212A8 ERTexture::GetTypeInfo
// 803212B4 ERTexture::GetTypeName
// 803212C0 ERTexture::GetTypeKey
// 803212CC ERTexture::GetTypeVersion
// 80321268 ERTexture::SafeDelete
// 803211F0 ERTexture::New
// 80321218 ERTexture::Construct
// 80321244 ERTexture::Destruct
// 803212D8 ERTexture::GetTypeInfoStatic
// 803212E4 ERTexture::GetReadVersion
SIMMY_STORABLE_IMPL(ERTexture, "ERTexture", 0, EResource)
