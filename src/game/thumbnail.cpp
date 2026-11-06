// Family sims' thumbnails (game/objselector.cpp: ObjSelector::SetThumbnail, ThumbnailLoader; the
// thumbnail's shader is made by ERShader::CreateEShaderForTexture, engine/e_rshader.cpp).
//
// A sim made in Create-a-Sim has a 32x32 portrait: in the save it is the neighbourhood file's 'thmb'
// resource (one of 36 thumbnail sections; NghResFile::GetResIDFromThumbGUID finds the sim's), which
// ThumbnailLoader reads into a new C8 texture with a 32-bit palette; ObjSelector::SetThumbnail wraps
// the texture in a shader of its own (ObjSelector +0x94) for the UI (the free-play family heads, the
// action menu, the relationship panel). Create-a-Sim makes it (CasSimRenderer::CreateFaceImage,
// SimImageMaker); ObjectFolderImpl::SaveUserData writes it. GC: a sim without one has it rendered
// (EGlobal::CreateThumbnail -> ESim::CreateThumbnail), not ported.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>

namespace {
constexpr int kThumbResType = 0x74686d62;  // 'thmb'
unsigned int s_nextShaderId = 1;   // nNextID_4181 (ObjSelector::SetThumbnail)
unsigned int s_nextTextureId = 1;  // nNextID_3153 (ERShader::CreateEShaderForTexture)
}  // namespace

// 801118C4
// Read: a new 32x32 C8 texture with a 256-entry 32-bit palette ("*thumbnail1*", GX-swizzled, a linear
// copy kept), its 32 rows of pixels and the palette read in; a palette whose alphas are all 0x80 or
// less (the card's 7-bit alpha) has them doubled; the texture becomes the selector's thumbnail.
// Written: the selector's thumbnail's pixels and palette.
void ThumbnailLoader::DoStream(ReconBuffer* b, int) {
  ETexture* texture = nullptr;
  const bool reading = b->m_mode == ReconBuffer__Mode::kRead;
  if (reading) {
    ETextureDef def;
    def.m_field00 = 0;
    def.m_field04 = 0;
    def.m_flags = 0x880;
    def.m_width = 0x20;
    def.m_height = 0x20;
    def.m_paletteSize = 0x100;
    def.m_mipCount = 0;
    def.m_format = 0x8a;
    def.m_paletteFormat = 2;
    def.m_bpp = 8;
    def.m_paletteBpp = 0x20;
    def.m_name = "*thumbnail1*";
    texture = _pGfx->CreateTexture(def);
  } else {
    ERShader* shader = nullptr;
    m_selector->GetThumbnail(&shader);
    if (shader) {
      EShader* s = shader->m_shader ? shader->m_shader : (shader->m_stateShader ? shader->m_stateShader->m_shader : nullptr);
      texture = s ? s->GetTexture(0) : nullptr;
      shader->DelRef();
    }
    // PC: only a 32x32 C8 portrait is written (a story sim's thumbnail is its portrait shader's texture,
    // which may be another size or format; GC read past it).
    if (texture && (texture->m_def.m_bpp != 8 || texture->m_def.m_width != 0x20 || texture->m_def.m_height != 0x20))
      texture = nullptr;
  }
  if (!texture) return;
  texture->UpdateBegin(ETextureUpdateType{reading ? ETextureUpdateType::Image : ETextureUpdateType::PaletteOnly});
  int pitch = 0, rows = 0;
  unsigned char* row = texture->UpdateMipLevel(0, pitch, rows);
  for (int y = 0; y < 0x20; ++y, row += pitch) b->Recon8(reinterpret_cast<signed char*>(row), 0x20);
  // PC: the palette as RGBA entries (the save's thumbnails hold them so; nghsave.cpp), which the
  // texture splits into its two halves at UpdateEnd. GC: UpdatePalette.
  unsigned char* palette = texture->UpdatePaletteDeswizzled();
  b->Recon8(reinterpret_cast<signed char*>(palette), 0x400);
  if (reading) {
    bool halfAlpha = true;
    for (int i = 0; i < 0x100 && halfAlpha; ++i) halfAlpha = palette[i * 4 + 3] <= 0x80;
    if (halfAlpha)
      for (int i = 0; i < 0x100; ++i) {
        int a = palette[i * 4 + 3] * 2 - 1;
        palette[i * 4 + 3] = static_cast<unsigned char>(a < 0 ? 0 : a);
      }
  }
  texture->UpdateEnd();
  if (reading) m_selector->SetThumbnail(texture);
}

// 80110F24
// The sim's thumbnail shows `texture`: its shader (made the first time, an unmanaged resource with an id
// of its own) is remade around the texture unless it shows it already.
void ObjSelector::SetThumbnail(ETexture* texture) {
  if (m_thumbnail) {
    EShader* s = m_thumbnail->m_shader ? m_thumbnail->m_shader
                                       : (m_thumbnail->m_stateShader ? m_thumbnail->m_stateShader->m_shader : nullptr);
    if (s && s->GetTexture(0) == texture) return;
  }
  if (!m_thumbnail) {
    m_thumbnail = new ERShader();  // GC: 0x6C bytes from the shader manager's heap
    m_thumbnail->m_id = s_nextShaderId++;
  }
  m_thumbnail->CreateEShaderForTexture(texture);
}

// 8031D1E8
// A plain one-layer shader around `texture` (surface 3, no geometry modes, layer flags 0x48/0), the
// texture held by a new unmanaged texture resource sized after it. The old texture resource is let go
// and a managed shader is detached first. GC: in engine/e_rshader.cpp.
void ERShader::CreateEShaderForTexture(ETexture* texture) {
  if (m_textures[0]) m_textures[0]->DelRef();
  m_textureIds[0] = 0;
  m_textures[0] = nullptr;
  if (m_manager) Detach();
  EShaderDef def;
  def.m_layerCount = 1;
  def.m_field01 = 0;
  def.m_field02 = 0;
  def.m_surface = 3;
  def.m_geometryModes = 0;
  EShaderLayer& layer = def.m_layers[0];
  layer.m_texture = texture;
  layer.m_field0C = 8 | 0x40;
  layer.m_flags = 0;
  layer.m_field14 = 0.5f;
  layer.m_blend[0] = 0;
  layer.m_blend[1] = 1;
  layer.m_blend[2] = 0;
  layer.m_blend[3] = 1;
  layer.m_alpha = 0xff;
  m_shader = _pGfx->CreateShader(def);
  m_textureIds[0] = 0;
  auto* res = new ERTexture();  // GC: 0x18 bytes from the texture manager's heap
  res->m_id = s_nextTextureId++;
  m_textures[0] = res;
  if (texture)
    res->m_flags = (static_cast<unsigned int>(texture->m_def.m_bpp >> 3) * texture->m_def.m_width * texture->m_def.m_height) << 8 |
                   (res->m_flags & 0xff);
  res->m_texture = texture;
}

// 80110D44
// A sim's thumbnail, kept once made (a reference for the caller): a story sim's portrait (its
// "RelationshipTable" icon); else the neighbourhood file's 'thmb' resource for the sim
// (NghResFile::GetResIDFromThumbGUID, ThumbnailLoader); else one is rendered from the sim in the house
// (EGlobal::CreateThumbnail). GC: a selector with a record at +0x90 (a shader id at +0x0C) takes that shader; nothing
// sets it.
bool ObjSelector::GetThumbnail(ERShader** out) {
  if (!m_thumbnail && m_simDesc) {
    const int id = static_cast<int>(GetNpcShaderId(static_cast<int>(GetGUID()), false));
    if (id == 0) {
      NghResFile* file = Globs_pNghResFile;
      const short resId = file ? file->GetResIDFromThumbGUID(GetGUID()) : 0;
      Memory__HandleNode* handle = resId ? file->GetByID(kThumbResType, resId, nullptr) : nullptr;
      if (!handle || !handle->m_data) {
        _globals.CreateThumbnail(this);  // rendered from the sim in the house, if it is there
      } else {
        // ReconLoadObject<ThumbnailLoader> on the handle.
        ThumbnailLoader loader;
        loader.m_selector = this;
        struct Adapter : ReconObject {
          ThumbnailLoader* m_loader;
          explicit Adapter(ThumbnailLoader* l) : m_loader(l) {}
          void DoStream(ReconBuffer* b, int v) override { m_loader->DoStream(b, v); }
          int GetType() override { return kThumbResType; }
        } adapter(&loader);
        ReconBuilder builder;
        builder.Reconstitute(&adapter, handle, nullptr);
      }
    } else {
      // GC: the shader's first texture is fetched twice (to touch it in); nothing else.
      m_thumbnail = static_cast<ERShader*>(_shaderman.AddRef(static_cast<unsigned int>(id), nullptr, 0));
    }
  }
  if (out) *out = m_thumbnail;
  if (m_thumbnail) m_thumbnail->AddRef();
  return m_thumbnail != nullptr;
}

// 800417DC
// A portrait for the selector's sim rendered from its person in the house (ESim::CreateThumbnail): 1
// when the person and its 3D instance are there.
RET_UNKNOWN EGlobal::CreateThumbnail(ObjSelector* selector) {
  if (!Globs_pObjectModule || !selector) return 0;
  cXPerson* person = Globs_pObjectModule->GetPersonByGUID(static_cast<int>(selector->GetGUID()));  // vtable 0xEC
  cXObject* object = person ? person->AsObject() : nullptr;
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  auto* sim = impl ? dynamic_cast<ESim*>(impl->m_instance) : nullptr;  // GC: the person's ESim (+0x14)
  if (!sim) return 0;
  sim->CreateThumbnail();
  return 1;
}

// 80034690
// The sim's portrait (SimImageMaker: its gender, its portrait parts) as its selector's thumbnail. Any
// queued model change is finished first. PC: the image is finished a few frames later
// (SimImageMaker::ServiceDeferredImages); the selector holds it (blank) at once.
void ESim::CreateThumbnail() {
  if (HasQueuedOperation()) UpdateQueuedOperation();  // GC: ESimsDataManager::FlushCommands first (PC: run at once)
  SimImageMaker maker;
  maker.SetGender(IsMale() != 0);
  maker.SetPortraitModels(m_simModel);
  ETexture* image = maker.CreateImage();
  cXObject* object = m_person ? m_person->AsObject() : nullptr;
  auto* impl = object ? dynamic_cast<cXPersonImpl*>(object->GetObjectImplementation()) : nullptr;
  if (ObjSelector* selector = impl ? impl->GetSelector() : nullptr) selector->SetThumbnail(image);  // vtable 0x36C
  std::printf("[portrait] %s: portrait rendered on demand\n", impl ? impl->DebugName() : "?");
}
