// Port of the sprites over objects and people: SpriteSlot (game/slots.cpp), IconGroupImpl
// (game/icongroup.cpp), ESpriteRender (game/espriterender.cpp), ESpriteRenderMan
// (game/espriterenderman.cpp), EGlobal's sprite calls (game/global.cpp), and the users ported with
// them: cXObjectImpl::GetSpriteSlot, cXPersonImpl::SpawnAspirationBalloon, the house-objects list of
// the order table (EHouse/ERLevel::InsertHouseObject). See sprites.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {
ESpriteRenderMan* s_spriteMan = nullptr;  // EGlobal +0x10C
ERQuickdata* s_spriteTable = nullptr;     // EGlobal +0xEC, quickdata 0x0c33db41

// sIDMaps (8042402C): icon group -> first sprite id (-1 none), labels string set.
const short kIDMaps[12][4] = {{0, 0, -1, 0xa1},   {0, 1, 0x64, 0x1f9},  {0, 2, 0x190, 0x1f5}, {0, 3, 0xc8, 0x1f6},
                              {0, 4, 0x12c, 0x1f7}, {0, 5, 0x1f4, 0x1f8}, {0, 6, 0x258, 0x1fa}, {0, 7, -1, 0x1fb},
                              {0, 8, 0x2bc, 0x1fc}, {0, 9, 0x320, 0x1fd}, {0, 10, 0x384, 0x200}, {0, 11, 0x3e8, -1}};
// sOldStyleSpriteIDs (803D9E3E): group 0's icons.
const short kOldStyleSpriteIDs[19] = {0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1,
                                      0xd2, 0xd3, 0xd4, 0xd5, 0x190, 0x191, 0x192, 0x193, 0x194};
// gValueIndexShaders: the digits 0..9 (not recovered: numbered sprites draw nothing).
const unsigned int kNoSprite = 0xd59c7bb5u;

ERShader* AddShader(unsigned int id) { return id ? static_cast<ERShader*>(_shaderman.AddRef(id, nullptr, 0)) : nullptr; }
void DelShader(ERShader*& s) {
  if (s) s->DelRef();
  s = nullptr;
}
void SelectShader(ERShader* r, ERC* rc) {
  if (EShader* s = r ? r->GetShader() : nullptr) s->Select(rc, 0);
}
cXObjectImpl* Impl(cXObject* o) { return o ? o->GetObjectImplementation() : nullptr; }
SpriteSlot* SlotOf(cXObject* o) {
  cXObjectImpl* impl = Impl(o);
  return impl ? reinterpret_cast<SpriteSlot*>(impl->GetSpriteSlot()) : nullptr;
}
ELights s_spriteLights;  // PC: the order table entry's lights (unlit sprites)
EMat4 s_identity;
}  // namespace

// ---------------------------------------------------------------------------------------------------
// SpriteSlot

// 80142DFC
// GC: also EGlobal::AllocSpriteRenderer for the object (each slot makes one).
SpriteSlot::SpriteSlot(cXObject* object) : m_object(object) {
  EGlobal::AllocSpriteRenderer(object);
  m_baseZ = 0.0f;
  m_altPeriod = 400;
  m_overInactive = 0;
  m_overlay = -1;
  m_balloon = -1;
  Reset();
}

// 80142EAC
// A slot of the object's slot list (a type-1 entry, PC: the quickdata SlotDescriptor's 0x48 bytes):
// its offset over the object (+4, +8, +0xC; z is also the slot's base height), nothing showing.
SpriteSlot::SpriteSlot(SlotDescriptor* descriptor, cXObject* object) : m_object(object) {
  const auto* d = reinterpret_cast<const unsigned char*>(descriptor);
  m_offset[0] = ERQuickdata::F32(d, 4);
  m_offset[1] = ERQuickdata::F32(d, 8);
  m_offset[2] = ERQuickdata::F32(d, 0xc);
  EGlobal::AllocSpriteRenderer(object);
  m_overInactive = 0;
  m_altPeriod = 400;
  m_overlay = -1;
  m_sprite = 0;
  m_altSprite = 0;
  m_altTime = 0;
  m_selector = nullptr;
  m_frame = 0;
  m_ticks = 0;
  m_priority = 0;
  m_balloon = -1;
  m_baseZ = m_offset[2];
  Reset();
}

// PC part of SlotLoader::Load (80143828, called by the object's constructor): the sprite slots of the
// object's slot list (definition +0x28; ids over 999 in its semi-global file), its type-1 entries in
// order. GC: the same pass makes its object slots (type 0) and routing slots (type 3).
void LoadObjectSpriteSlots(cXObjectImpl* object) {
  if (!object || !object->m_def) return;
  const short id = ERQuickdata::S16(object->m_def, 0x28);
  const unsigned char* file = SelectorResFile(object->m_selector);
  if (id == 0 || !file) return;
  if (id > 999) file = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x34));
  const auto* lists = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x28)) : nullptr;
  if (!lists) return;
  const unsigned int count = ERQuickdata::U32(lists - 4, 0);
  for (unsigned int i = 0; i < count; ++i) {
    if (ERQuickdata::S32(lists, i * 8 + 4) != id) continue;
    const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(lists, i * 8));
    const unsigned int n = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
    for (unsigned int e = 0; e < n; ++e) {
      const unsigned char* d = entries + e * 0x48;
      if (ERQuickdata::S16(d, 0) == 1)
        object->m_spriteSlots.emplace_back(reinterpret_cast<SlotDescriptor*>(const_cast<unsigned char*>(d)), object);
    }
    break;
  }
}

// 80142F8C
SpriteSlot::~SpriteSlot() {}

// 80142FC0
// The sprite and its frames (played backwards: from the last); the renderer takes it on its next update.
void SpriteSlot::SetSprite(unsigned int, int sprite, int frames, bool reverse) {
  m_sprite = sprite;
  m_frames = frames;
  if (!reverse) {
    m_frame = 0;
    m_frameStep = 1;
  } else {
    m_frame = frames - 1;
    m_frameStep = -1;
  }
  EGlobal::UpdateSpriteRenderer(this);
  m_overlay = -1;
  m_balloon = -1;
}

// 8014308C
void SpriteSlot::ChangeToSprite(int sprite) {
  m_sprite = sprite;
  EGlobal::UpdateSpriteRenderer(this);
}

// 801430BC
void SpriteSlot::AlternateWithSprite(int sprite, int ms) {
  m_altSprite = sprite;
  m_altPeriod = ms;
  m_options |= kAlternate;
}

// 801430D4
// One simulator tick: the time left runs down (at 0 the slot is reset); the sprite alternates; it rises
// or sinks 0.01 a tick (popping ones after 10 ticks), pops (grows to the pop size over 7 ticks, back to
// 1 by tick 10), fades (from tick 20 to 70; then off with kEndWithFade); a frame every 12 ticks. True
// when the frame changed.
int SpriteSlot::Tick() {
  if (m_ticks > 0) --m_ticks;
  if (m_ticks == 0) {
    Reset();
    return 0;
  }
  unsigned int t;
  if (m_ticks == -1) t = static_cast<unsigned int>(m_tickCount++);
  else t = static_cast<unsigned int>(m_tickCount - m_ticks);
  const float age = static_cast<float>(static_cast<int>(t));
  if (m_options & kAlternate) {
    m_altTime += static_cast<int>(_dt * 1000.0f);
    if (m_altPeriod <= m_altTime) {
      std::swap(m_sprite, m_altSprite);
      ChangeToSprite(m_sprite);
      m_altTime = 0;
    }
  }
  if (!(m_options & kPop) || age > 10.0f) {
    if (m_options & kRise) m_offset[2] += 0.01f;
    else if (m_options & kSink) m_offset[2] -= 0.01f;
  }
  if (m_options & kPop) {
    float s;
    if (age <= 7.0f) s = m_popSize * age / 7.0f;
    else if (age < 10.0f) s = m_popSize - (m_popSize - 1.0f) * (age - 7.0f) / 3.0f;
    else s = 1.0f;
    m_sizeX = m_sizeY = s;
  }
  if ((m_options & kFade) && age > 20.0f) {
    if (age < 70.0f) {
      m_alpha = (70.0f - age) / 70.0f;
    } else {
      m_priority = 0;
      m_alpha = 0.0f;
      if (m_options & kEndWithFade) {
        m_ticks = 0;
        Reset();
        return 0;
      }
    }
  }
  if (++m_frameTicks != 12) return 0;
  m_frame += m_frameStep;
  while (m_frame < 0) m_frame += m_frames;
  m_frameTicks = 0;
  m_frame %= m_frames;
  return 1;
}

// 80143398
// Shown for `ticks` (-1 for ever). GC: a slot that was off with no sprite tells its object first
// (cXObject vtable 0x36C, the selector; nothing done with it).
void SpriteSlot::ActivateForTicks(int ticks) {
  m_ticks = ticks;
  m_frameTicks = 0;
  m_tickCount = ticks == -1 ? 0 : ticks;
}

// 80143438
void SpriteSlot::ActivateForLoops(int loops) { ActivateForTicks(m_frames * 12 * loops); }

// 80143464
void SpriteSlot::Reset() {
  m_ticks = 0;
  m_popSize = 1.0f;
  m_offset[2] = m_baseZ;
  m_sprite = 0;
  m_altSprite = 0;
  m_options = 0;
  m_sizeX = m_sizeY = 1.0f;
  m_alpha = 1.0f;
}

// 80145390
void SpriteSlot::Deactivate() { ActivateForTicks(0); }
// 801453B4
int SpriteSlot::IsActive() const { return m_ticks != 0 ? 1 : 0; }

// ---------------------------------------------------------------------------------------------------
// IconGroupImpl

// 800BCA10
IconGroupImpl::IconGroupImpl() {}

// 800BCA34
void IconGroupImpl::Init(int group) {
  for (const auto& e : kIDMaps)
    if (e[1] == group) {
      m_group = group;
      m_map = &e[0];
      return;
    }
}

// 800BCA8C
IconGroupImpl::~IconGroupImpl() {}

// 800BCB04
// The group's first sprite id plus the icon; group 0 (no first id) has a table of its own.
int IconGroupImpl::GetSpriteID(int icon) {
  if (m_map && m_map[2] != -1) return static_cast<short>(m_map[2] + icon);
  if (m_group == 0 && icon >= 0 && icon < 0x13) return kOldStyleSpriteIDs[icon];
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// EGlobal's sprite calls

// 80041694
void EGlobal::AllocSpriteRenderer(cXObject* object) { ESpriteRenderMan::Instance()->AddSprite(object); }

// 800416B8
void EGlobal::FreeSpriteRenderer(cXObject* object) {
  if (s_spriteMan) s_spriteMan->MarkSprite(object);
}

// 800416E4
void EGlobal::UpdateSpriteRenderer(SpriteSlot* slot) { ESpriteRenderMan::Instance()->SetSprite(slot); }

// 80041708
// The "SpriteIdToResIdNode" row of `sprite` (rows of 12 bytes sorted by id: id, shader id, size data
// {w, h, x, y, z}): FindRes<SpriteIdToResIdNode> (8039E4FC), a binary search.
const void* ConvertSpriteIdToResIdNode(unsigned int sprite) {
  if (!s_spriteTable) s_spriteTable = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0x0c33db41u, nullptr, 0));
  const void* table = s_spriteTable ? s_spriteTable->getTable("SpriteIdToResIdNode") : nullptr;
  if (!table) return nullptr;
  const unsigned char* rows = static_cast<const unsigned char*>(ERQuickdata::TableRows(table));
  int lo = 0, n = static_cast<int>(ERQuickdata::TableRowCount(table));
  while (n > 0) {
    if (n == 1) return ERQuickdata::U32(rows + lo * 12) == sprite ? rows + lo * 12 : nullptr;
    const int half = n / 2;
    const unsigned int id = ERQuickdata::U32(rows + (lo + half) * 12);
    if (id == sprite) return rows + (lo + half) * 12;
    if (static_cast<int>(sprite - id) > 0) {
      lo += half + 1;
      n -= half + 1;
    } else {
      n = half;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------------------------------
// ESpriteRender

namespace {
// A size row into the sprite's data (SetSprite's common part): shader id, w, h, x, y, z.
unsigned int ReadSpriteNode(const void* node, ESpriteRender::SpriteData& d) {
  const void* size = ERQuickdata::Ptr(node, 8);
  if (size) {
    d.m_w = ERQuickdata::F32(size, 0);
    d.m_h = ERQuickdata::F32(size, 4);
    d.m_off[0] = ERQuickdata::F32(size, 8);
    d.m_off[1] = ERQuickdata::F32(size, 0xc);
    d.m_off[2] = ERQuickdata::F32(size, 0x10);
  }
  return ERQuickdata::U32(node, 4);
}
}  // namespace

// 80035460
ESpriteRender::ESpriteRender() {
  m_otd.m_callback = SpriteRenderOrderTableCallback;
}

// 80035530
ESpriteRender::~ESpriteRender() {
  DeleteShaders();
  m_object = nullptr;
}

// 80035588
void ESpriteRender::DeleteShaders() {
  DelShader(m_main);
  m_mainId = 0;
  DelShader(m_balloon);
  m_balloonId = 0;
  DelShader(m_overlay);
  m_overlayId = 0;
  DelShader(m_digit1);
  m_digit1Id = 0;
  DelShader(m_digit10);
  m_digit10Id = 0;
}

// 800356C8
// GC: the asynchronous shader loads are picked up here; then a person's plumb bob is told whether a
// sprite shows (ESim::SetPlumbBobState 0 / 10). PC: the shaders load at once; the plumb bob part is
// not ported.
void ESpriteRender::Update() {}

// The point the sprites hang from: a person's head bone (SAnimator2::GetBonePos 1) plus the slot's
// height, else the middle of the object's bounds at the slot's height; 1/8 towards the camera.
bool ESpriteRender::Anchor(EVec3& out, SpriteSlot*& slot) {
  if (m_marked) return false;
  slot = SlotOf(m_object);
  if (!slot || slot->m_ticks == 0) return false;
  cXObjectImpl* impl = Impl(m_object);
  ISimInstance* inst = impl ? impl->m_instance : nullptr;
  if (!inst) return false;
  if (auto* person = dynamic_cast<cXPersonImpl*>(impl)) {
    SAnimator2* anim = person->m_animator;
    if (!anim) return false;
    anim->GetBonePos(1, out);
    out.z += slot->m_offset[2];
  } else {
    out.x = (inst->m_bounds[0] + inst->m_bounds[3]) * 0.5f;
    out.y = (inst->m_bounds[1] + inst->m_bounds[4]) * 0.5f;
    out.z = slot->m_offset[2];
  }
  if (auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow)) {
    out.x += win->m_lookAtPos.m[2][0] * 0.125f;
    out.y += win->m_lookAtPos.m[2][1] * 0.125f;
    out.z += win->m_lookAtPos.m[2][2] * 0.125f;
  }
  return true;
}

// 80035890
// Into the house's order table at its anchor, or the depth-of-field foreground list for people in a
// close-up.
void ESpriteRender::DrawIntoOrderTable() {
  EHouse* house = _globals.m_house;
  if (!house) return;
  SpriteSlot* slot = nullptr;
  if (!Anchor(m_pos, slot)) return;
  static const bool trace = std::getenv("SIMMY_SPRITE_TRACE") != nullptr;  // PC dev aid
  static int traced = 0;
  if (trace && traced++ % 120 == 0) std::printf("[sprite] anchor %.2f %.2f %.2f\n", m_pos.x, m_pos.y, m_pos.z);
  m_otd.m_pos = &m_pos.x;
  m_otd.m_lights = &s_spriteLights;
  m_otd.m_matrix = &s_identity;
  // A selected or socialising sim's sprites stay in front of the depth of field (when flagged).
  bool simInFront = false;
  if (auto* person = dynamic_cast<cXPersonImpl*>(Impl(m_object))) {
    // GC: cXPersonImpl::IsSelected (8012953C); PC: a player's selected person.
    const bool selected = _globals.GetSelectedPerson(0) == person || _globals.GetSelectedPerson(1) == person;
    simInFront = selected || person->IsInSocialMode();
  }
  FrameEffectsManager* fx = _globals.m_cameraDirector ? _globals.m_cameraDirector->GetFrameEffectsManager() : nullptr;
  const bool dofOn = fx && fx->DepthOfFieldObject() && fx->DepthOfFieldObject()->m_state != kFrameEffectOff;
  if (!m_dofForeground || !simInFront || !dofOn) house->InsertHouseObject(&m_otd);
  else house->m_level->InsertInDOFForegroundObjectList(m_otd);
}

// 80035C08
void ESpriteRender::SpriteRenderOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  static_cast<ESpriteRender*>(static_cast<void*>(otd->m_dl))->DrawDirectly(data.m_rc);
  data.m_curLights = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
  data.m_curShader = nullptr;
}

// 80035C50
// The balloon behind (its size by the slot's size multipliers), then the sprite (or the two digits of
// a number), then the overlay; each at the anchor plus its offset, faded by the slot's alpha.
void ESpriteRender::DrawDirectly(ERC* rc) {
  SpriteSlot* slot = nullptr;
  EVec3 at;
  if (!Anchor(at, slot)) return;
  rc->ModelMatrixId();
  const float sx = slot->m_sizeX, sy = slot->m_sizeY;
  bool ok = true;
  if (m_balloon) {
    m_balloonData.m_color[3] = slot->m_alpha;
    SelectShader(m_balloon, rc);
    EVec3 p{m_balloonData.m_off[0] + at.x, m_balloonData.m_off[1] + at.y, m_balloonData.m_off[2] + at.z};
    ok = SetUpRect(rc, p, m_balloonData.m_w * sx, m_balloonData.m_h * sy, m_balloonData);
  }
  EVec3 p{m_mainData.m_off[0] + at.x, m_mainData.m_off[1] + at.y, m_mainData.m_off[2] + at.z};
  if (!m_main) {
    if (m_digit1 && m_digit10) {
      m_mainData.m_color[3] = slot->m_alpha;
      SetUpDoubleRect(rc, p, m_mainData.m_w * sx, m_mainData.m_h * sy, m_digit10, m_digit1, m_mainData);
    }
  } else if (ok) {
    m_mainData.m_color[3] = slot->m_alpha;
    SelectShader(m_main, rc);
    SetUpRect(rc, p, m_mainData.m_w * sx, m_mainData.m_h * sy, m_mainData);
    if (m_overlay) {
      SelectShader(m_overlay, rc);
      EVec3 q{m_overlayData.m_off[0] + at.x, m_overlayData.m_off[1] + at.y, m_overlayData.m_off[2] + at.z};
      SetUpRect(rc, q, m_overlayData.m_w * sx, m_overlayData.m_h * sy, m_overlayData);
    }
  }
}

// 80036178
// The shaders for the slot's sprite: numbers 1000..1099 as two digits (gValueIndexShaders), the
// selector's thumbnail for -1 (people: their portrait; objects: the catalogue icon of the object or
// its master), else the sprite's row (EGlobal::ConvertSpriteIdToResId); the balloon and the overlay
// from their rows too. GC: skill meters (option 0x10) drive the person's plumb bob instead.
void ESpriteRender::SetSprite() {
  DeleteShaders();
  SpriteSlot* slot = SlotOf(m_object);
  m_markedAsNew = 0;
  if (!slot) return;
  for (SpriteData* d : {&m_mainData, &m_balloonData, &m_overlayData}) *d = SpriteData();
  if (slot->m_options & SpriteSlot::kSkillMeter) {
    // The person's plumb bob turns into the skill meter: the sprite is the value in tenths (10..19:
    // the second ten, the meter's second colour); no ticks left: back to the plumb bob.
    cXObjectImpl* impl = m_object ? m_object->GetObjectImplementation() : nullptr;
    auto* person = impl && impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
    ESim* sim = person ? dynamic_cast<ESim*>(person->m_instance) : nullptr;
    if (sim) {
      if (slot->m_ticks == 0) {
        sim->SetPlumbBobState(0);
      } else if (slot->m_sprite < 10) {
        sim->m_skillValue = static_cast<float>(slot->m_sprite) / 10.0f;
        sim->SetPlumbBobState(1);
      } else {
        sim->m_skillValue = (static_cast<float>(slot->m_sprite) - 10.0f) / 10.0f;
        sim->SetPlumbBobState(2);
      }
    }
  } else if (static_cast<unsigned int>(slot->m_sprite - 1000) < 100) {
    // Two digits (gValueIndexShaders), a small pair high over the object.
    static const unsigned int kValueIndexShaders[10] = {0xc3fc3f43u, 0xb4fb0fd5u, 0x2df25e6fu, 0x5af56ef9u, 0xc491fb5au,
                                                         0xb396cbccu, 0x2a9f9a76u, 0x5d98aae0u, 0xcd27b771u, 0xba2087e7u};
    const int v = std::min(slot->m_sprite - 1000, 99);
    m_digit10Id = kValueIndexShaders[v / 10];
    m_digit1Id = kValueIndexShaders[v % 10];
    m_digit10 = AddShader(m_digit10Id);
    m_digit1 = AddShader(m_digit1Id);
    m_mainData.m_off[0] = 0.0f, m_mainData.m_off[2] = 0.8f, m_mainData.m_off[1] = 0.0f;
    m_mainData.m_w = 0.03f, m_mainData.m_h = 0.0875f;
  } else if (slot->m_sprite == -1) {
    // The slot's selector's picture: a person's portrait (ObjSelector::GetThumbnail), an object's
    // catalogue icon (its definition's resource data +0x1C, or another part's of its group); none: the
    // "no sprite" shader. Shown on a tinted balloon.
    ObjSelector* sel = slot->m_selector;
    if (sel && sel->GetIsPerson()) {
      // ObjSelector::GetThumbnail: the sim's portrait (a reference for us), else the "no sprite" shader.
      ERShader* portrait = nullptr;
      if (sel->GetThumbnail(&portrait) && portrait) {
        m_mainId = 0;
        m_main = portrait;
      } else {
        m_mainId = kNoSprite;
        m_main = AddShader(m_mainId);
      }
    } else {
      m_mainId = kNoSprite;
      const unsigned char* res = sel && sel->m_def ? objdef::ResData(sel->m_def) : nullptr;
      if (res) {
        if (const unsigned int icon = ERQuickdata::U32(res, 0x1c)) m_mainId = icon;
      } else if (sel && Globs_pObjectFolder) {
        auto* master = reinterpret_cast<ObjSelector*>(sel->GetMasterSelector());
        for (ObjSelector* o = Globs_pObjectFolder->GetNextSelector(nullptr); o; o = Globs_pObjectFolder->GetNextSelector(o)) {
          if (o == sel || reinterpret_cast<ObjSelector*>(o->GetMasterSelector()) != master || !o->m_def) continue;
          const unsigned char* r = objdef::ResData(o->m_def);
          if (r && ERQuickdata::U32(r, 0x1c)) {
            m_mainId = ERQuickdata::U32(r, 0x1c);
            break;
          }
        }
      }
      m_main = AddShader(m_mainId);
    }
    m_mainData.m_off[0] = 0.0f, m_mainData.m_off[2] = 0.65f, m_mainData.m_off[1] = 0.0f;
    m_mainData.m_w = 0.2f, m_mainData.m_h = 0.2f;
    m_balloonData.m_color[0] = 0.5294118f, m_balloonData.m_color[1] = 0.5764706f;
    m_balloonData.m_color[2] = 0.7764706f, m_balloonData.m_color[3] = 1.0f;
  } else if (const void* node = ConvertSpriteIdToResIdNode(static_cast<unsigned int>(slot->m_sprite))) {
    m_mainId = ReadSpriteNode(node, m_mainData);
    if (!m_mainId) m_mainId = kNoSprite;
    m_main = AddShader(m_mainId);
  }
  if (const void* node = ConvertSpriteIdToResIdNode(static_cast<unsigned int>(slot->m_balloon))) {
    m_balloonId = ReadSpriteNode(node, m_balloonData);
    if (slot->m_balloon != -1 && m_balloonId) m_balloon = AddShader(m_balloonId);
  }
  if (const void* node = ConvertSpriteIdToResIdNode(static_cast<unsigned int>(slot->m_overlay))) {
    m_overlayId = ReadSpriteNode(node, m_overlayData);
    if (slot->m_overlay != -1 && m_overlayId) m_overlay = AddShader(m_overlayId);
  }
  static const bool trace = std::getenv("SIMMY_SPRITE_TRACE") != nullptr;  // PC dev aid
  if (trace)
    std::printf("[sprite] renderer: sprite %d -> shader %08x (%s) %.2fx%.2f at %.2f,%.2f,%.2f; balloon %d -> %08x (%s); overlay %d -> %08x\n",
                slot->m_sprite, m_mainId, m_main ? "loaded" : "none", m_mainData.m_w, m_mainData.m_h, m_mainData.m_off[0],
                m_mainData.m_off[1], m_mainData.m_off[2], slot->m_balloon, m_balloonId, m_balloon ? "loaded" : "none",
                slot->m_overlay, m_overlayId);
}

// 80036734
// A camera-facing quad: its corners are the point plus and minus (camera right * w + camera up * h).
// GC: one two-point SpriteList (ERC vtable 0xB4; uv (1,1)-(0,0), colours x128, alpha x255). PC: the
// same quad as a triangle strip (ERC::SpriteList is not ported).
bool ESpriteRender::SetUpRect(ERC* rc, EVec3& pos, float w, float h, SpriteData& data) {
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!win) return false;
  const EMat4& cam = win->m_lookAtPos;
  const float rx = cam.m[0][0] * w, ry = cam.m[0][1] * w, rz = cam.m[0][2] * w;
  const float ux = cam.m[1][0] * h, uy = cam.m[1][1] * h, uz = cam.m[1][2] * h;
  EGEVert v[4];
  const int r = static_cast<int>(data.m_color[0] * 128.0f), g = static_cast<int>(data.m_color[1] * 128.0f),
            b = static_cast<int>(data.m_color[2] * 128.0f), a = static_cast<int>(data.m_color[3] * 255.0f);
  const float sx[4] = {-1.0f, 1.0f, -1.0f, 1.0f}, sy[4] = {1.0f, 1.0f, -1.0f, -1.0f};
  for (int i = 0; i < 4; ++i) {
    v[i].pos[0] = pos.x + rx * sx[i] + ux * sy[i];
    v[i].pos[1] = pos.y + ry * sx[i] + uy * sy[i];
    v[i].pos[2] = pos.z + rz * sx[i] + uz * sy[i];
    v[i].uv[0] = sx[i] > 0.0f ? 1.0f : 0.0f;
    v[i].uv[1] = sy[i] > 0.0f ? 1.0f : 0.0f;
    v[i].normal[2] = 0x7f;
    v[i].color[0] = r, v[i].color[1] = g, v[i].color[2] = b, v[i].color[3] = a;
  }
  rc->TriStrip(v, 4);
  return true;
}

// 800369A0
// Two quads side by side (the tens digit left of the units digit), each `data`'s width apart.
bool ESpriteRender::SetUpDoubleRect(ERC* rc, EVec3& pos, float w, float h, ERShader* a, ERShader* b,
                                    SpriteData& data) {
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!win) return false;
  const EMat4& cam = win->m_lookAtPos;
  const float d = -data.m_w;
  EVec3 left{pos.x + cam.m[0][0] * d, pos.y + cam.m[0][1] * d, pos.z + cam.m[0][2] * d};
  EVec3 right{pos.x - cam.m[0][0] * d, pos.y - cam.m[0][1] * d, pos.z - cam.m[0][2] * d};
  SelectShader(a, rc);
  SetUpRect(rc, left, w, h, data);
  SelectShader(b, rc);
  SetUpRect(rc, right, w, h, data);
  return true;
}

// 80036E70
cXObject* ESpriteRender::GetObject() { return m_object; }
// 80036E78
int ESpriteRender::GetMarked() { return m_marked; }
// 80036E80
int ESpriteRender::GetMarkedAsNew() { return m_markedAsNew; }
// 80036E88
void ESpriteRender::Mark() { m_marked = 1; }
// 80036E94
void ESpriteRender::MarkAsNew() { m_markedAsNew = 1; }

// ---------------------------------------------------------------------------------------------------
// ESpriteRenderMan

// 80036EA0
ESpriteRenderMan::ESpriteRenderMan() {}

// 80036EBC
ESpriteRenderMan::~ESpriteRenderMan() {
  if (m_owns)
    for (ESpriteRender* r : m_list) delete r;
  m_list.clear();
}

ESpriteRenderMan* ESpriteRenderMan::Instance() {
  if (!s_spriteMan) s_spriteMan = new ESpriteRenderMan;
  return s_spriteMan;
}

// 80036F64
void ESpriteRenderMan::Update() {
  for (ESpriteRender* r : m_list) r->Update();
  RemoveMarkedSprites();
}

// 80036FB4
// Marked renderers are deleted; those whose slot changed set up their sprite.
void ESpriteRenderMan::RemoveMarkedSprites() {
  for (size_t i = 0; i < m_list.size();) {
    ESpriteRender* r = m_list[i];
    if (r->m_marked) {
      m_list.erase(m_list.begin() + static_cast<long>(i));
      delete r;
      continue;
    }
    if (r->m_markedAsNew) r->SetSprite();
    ++i;
  }
}

// 80037048
void ESpriteRenderMan::DrawIntoOrderTable() {
  for (ESpriteRender* r : m_list) r->DrawIntoOrderTable();
}

// 8003708C
void ESpriteRenderMan::DrawDirectly(ERC* rc) {
  for (ESpriteRender* r : m_list) r->DrawDirectly(rc);
}

// 800370D8
// The first renderer of the slot's object takes the change on its next update.
void ESpriteRenderMan::SetSprite(SpriteSlot* slot) {
  for (ESpriteRender* r : m_list)
    if (r->m_object == slot->m_object) {
      r->m_markedAsNew = 1;
      return;
    }
}

// 80037130
ESpriteRender* ESpriteRenderMan::AddSprite(cXObject* object) {
  auto* r = new ESpriteRender;  // GC: 0xEC bytes, 16-aligned, from the main heap
  r->m_object = object;
  r->m_otd.m_dl = reinterpret_cast<EDL*>(r);  // PC: the callback's way back (GC: +0xBC)
  m_list.push_back(r);
  return r;
}

// 80037190
void ESpriteRenderMan::MarkSprite(cXObject* object) {
  for (ESpriteRender* r : m_list)
    if (r->m_object == object) r->m_marked = 1;
}

// ---------------------------------------------------------------------------------------------------
// The order table's house objects

// 80025464 EHouse::InsertHouseObject and 802379A0 ERLevel::InsertHouseObject: interactorvisualizer.cpp

// ---------------------------------------------------------------------------------------------------
// The users

// 800DE780
RET_UNKNOWN cXObjectImpl::GetSpriteSlot() {
  return m_spriteSlots.empty() ? 0 : reinterpret_cast<RET_UNKNOWN>(&m_spriteSlots[0]);
}

// 800FBA00
// A balloon over me (or, operand +0 bit 0, the stack object): operand +2 the icon group (high byte; it
// sets the priority: 4 -> 2, 10 -> 0, else 1) and icon (low byte; flag 0x1000 adds my data 7), +4 the
// ticks (0: off, 0xFFFF: for ever; flag 0x800: loops), +6 flags (0x400 frames backwards, 0x2000
// alternate with nothing, 0x200 the overlay, 0x100 over inactive people) and +7 the balloon type. A
// slot showing something more important is left alone (asked off: it ends with its fade). Group 7 is an
// object's picture: 0 the stack object's (master) selector, 1 neighbour <stack object>'s, 2 a local's
// object's; group 9 icons under 0x14 are skill meters; groups 2 (icons 0x46-0x47), 6 (0x0F-0x12) and 9
// rise, pop and fade.
int cXObjectImpl::TrySetBalloon(StackElem* frame, const unsigned char* params) {
  auto be = [&](int i) { return static_cast<unsigned short>(simmy::be16(params + 2 * i)); };
  cXObjectImpl* target = this;
  if (be(0) & 1) {
    cXObject* o = GetObjectFromID(frame->m_stackObject);
    target = o ? o->GetObjectImplementation() : nullptr;
    if (!target) {
      if (m_treeSim) m_treeSim->SetError(0x15);
      Error(0x15);
      return -1;
    }
  }
  if (target->m_spriteSlots.empty()) return 1;
  SpriteSlot* slot = &target->m_spriteSlots[0];
  const int group = be(1) >> 8;
  int priority = group == 4 ? 2 : group == 10 ? 0 : 1;
  unsigned int icon = be(1) & 0xff;
  const unsigned short flags = be(3);
  if (slot->m_ticks != 0 && slot->m_priority > priority) {
    if (be(2) == 0) slot->m_options |= SpriteSlot::kEndWithFade;
    return 1;
  }
  slot->m_priority = priority;
  if (be(2) == 0) {
    slot->ActivateForTicks(0);
    return 1;
  }
  if (flags & 0x1000) icon += static_cast<unsigned int>(m_data[7]);
  slot->Reset();
  slot->m_options = SpriteSlot::kPop;
  slot->m_popSize = 1.2f;
  bool popUp = false;
  if (group == 9) {
    if (static_cast<int>(icon) < 0x14) slot->m_options = SpriteSlot::kSkillMeter;
    else {
      slot->m_priority = 2;
      slot->m_options = SpriteSlot::kRise | SpriteSlot::kPop | SpriteSlot::kFade;
    }
    popUp = true;
  } else if (group == 2 || group == 6) {
    if ((group == 2 && icon - 0x46 <= 1) || (group == 6 && icon - 0xf <= 3))
      slot->m_options = SpriteSlot::kRise | SpriteSlot::kPop | SpriteSlot::kFade;
    popUp = true;
  } else if (group == 10 || group == 11) {
    slot->m_options = 0;
  }
  if (popUp) slot->m_popSize = 1.5f;
  if (group == 7) {
    ObjSelector* selector = nullptr;
    const int which = be(1) & 0xff;
    if (which == 0 || which == 2) {
      short id = frame->m_stackObject;
      if (which == 2) {
        const unsigned int local = (be(0) >> 1) & 0x3f0;
        if (frame->m_numLocals <= local) {
          if (m_treeSim) m_treeSim->SetError(0x34);
          Error(0x34);
          return -1;
        }
        id = frame->GetLocals()[local];
      }
      cXObject* o = GetObjectFromID(id);
      cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
      selector = impl ? impl->m_selector : nullptr;  // GC: its master selector
    } else if (which == 1) {
      Neighbor* n = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(frame->m_stackObject) : nullptr;
      selector = n ? n->m_selector : nullptr;
    }
    if (!selector) return 1;
    slot->m_selector = selector;  // SpriteSlot::SetSprite(ObjSelector*) 80143030
    slot->m_sprite = -1;
    slot->m_frame = 0;
    slot->m_frameStep = 1;
    slot->m_frames = 1;
    EGlobal::UpdateSpriteRenderer(slot);
    slot->m_overlay = slot->m_balloon = -1;
  } else {
    int sprite = static_cast<int>(icon);
    if (group != 9 || icon > 0x13) {
      IconGroupImpl icons;
      icons.Init(group);
      sprite = icons.GetSpriteID(static_cast<int>(icon));
      if (!sprite) return 1;
    }
    slot->SetSprite(1, sprite, 1, (flags >> 10) & 1);
  }
  if (flags & 0x2000) {
    SIMMY_STUB("cXObjectImpl::TrySetBalloon: the alternation period (cXObject vtable 0x274)", 0x800FBA00);
    slot->AlternateWithSprite(0, 500);
  }
  // IconGroup::GetBalloonSpriteID (800BCC68): types 0..2 are group 1's icons 0..2.
  {
    IconGroupImpl balloons;
    balloons.Init(1);
    const int type = params[7];
    slot->m_balloon = type <= 2 ? balloons.GetSpriteID(type) : 0;
    slot->m_overlay = (flags & 0x200) ? balloons.GetSpriteID(3) : 0;
  }
  slot->m_overInactive = (flags >> 8) & 1;
  if (be(2) == 0xffff) slot->ActivateForTicks(-1);
  else if (flags & 0x800) slot->ActivateForLoops(be(2));
  else slot->ActivateForTicks(be(2));
  static const bool trace = std::getenv("SIMMY_SPRITE_TRACE") != nullptr;  // PC dev aid
  if (trace) std::printf("[sprite] %s: balloon group %d icon %u for %d (balloon %d)\n", target->DebugName(), group, icon, be(2), slot->m_balloon);
  return 1;
}

// 80125834
// The reward's balloon over the sim's head, unless a more important sprite shows: popping up, rising,
// fading (priority 2, options 0x2D, pop size 1.5), icon 0x22 (+200 and more), 0x23 (up to 200), 0x24
// (losses), 0x25 (-200 and less) of icon group 6, for 20 ticks; the object is told its sprite changed
// (cXObject vtable 0x254; not ported).
void cXPersonImpl::SpawnAspirationBalloon(int reward) {
  SpriteSlot* slot = reinterpret_cast<SpriteSlot*>(GetSpriteSlot());
  if (!slot || (slot->m_ticks != 0 && slot->m_priority >= 3)) return;
  slot->m_priority = 2;
  slot->m_options = SpriteSlot::kRise | SpriteSlot::kPop | SpriteSlot::kFade | SpriteSlot::kEndWithFade;
  slot->m_popSize = 1.5f;
  IconGroupImpl group;
  group.Init(6);
  const int icon = reward < -200 ? 0x25 : reward < 0 ? 0x24 : reward < 200 ? 0x23 : 0x22;
  const int sprite = group.GetSpriteID(icon);
  if (!sprite) return;
  slot->SetSprite(1, sprite, 1, false);
  slot->m_overInactive = 1;
  slot->m_balloon = 3;
  slot->m_overlay = 0;
  slot->ActivateForTicks(0x14);
  // PC dev aid: SIMMY_SPRITE_HOLD=1 keeps the balloon up (no fade) for screenshots.
  if (std::getenv("SIMMY_SPRITE_HOLD")) {
    slot->m_options &= ~(SpriteSlot::kFade | SpriteSlot::kRise | SpriteSlot::kEndWithFade);
    slot->ActivateForTicks(-1);
  }
  std::printf("[sprite] %s: aspiration balloon (reward %d, sprite %d)\n", DebugName(), reward, sprite);
}
