// Port of game/global.cpp (partial): EGlobal construction and the resources loaded before the intro.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <cstdio>
#include <cstring>

namespace {
// The config block holds C strings and numbers at fixed offsets from +0x168.
void PutString(unsigned char* block, unsigned int at, const char* s) { std::memcpy(block + (at - 0x168), s, std::strlen(s)); }
void PutInt(unsigned char* block, unsigned int at, int v) { std::memcpy(block + (at - 0x168), &v, 4); }
void PutFloat(unsigned char* block, unsigned int at, float v) { std::memcpy(block + (at - 0x168), &v, 4); }
}  // namespace

// 8003E218
// The config block starts with the disc path of the executable, the build number and the title code
// (GC: written as big-endian words; the same bytes here).
EGlobal::EGlobal() {
  unsigned char* c = m_config;
  PutString(c, 0x168, "cdrom0:\\SLUS_212.56;1");
  PutString(c, 0x188, "1.00");
  PutString(c, 0x190, "NTSC");
  PutString(c, 0x198, "NICHDD");
  PutString(c, 0x324, "1.05");
  c[0x32c - 0x168] = 0xff;
  c[0x32d - 0x168] = 0xff;
  c[0x384 - 0x168] = 0xff;
  PutInt(c, 0x33c, 1);
  PutInt(c, 0x380, 1);
  PutInt(c, 0x388, 1);
  PutInt(c, 0x38c, 1);
  PutFloat(c, 0x3ac, 60.0f);
  PutFloat(c, 0x3b0, 540.0f);
  PutInt(c, 0x404, 1);
  PutString(c, 0x408, "default");
  PutInt(c, 0x434, 1);
  PutInt(c, 0x438, 1);
  m_field45C = 9;
}

// 8003E1F0
EGlobal::~EGlobal() = default;

// 8003E5B4
RET_UNKNOWN EGlobal::GetGameFontID() { return 0x67f664fb; }

// 8003E6B4
// The fallback background shader, the game font, the two quickdata string/data images, the options
// and CAS costume lists, the cheats, then the Apt viewer with its command tables, UI targets and the
// "s2c_root" movie (loaded and run until it reports ready; UI update and drawing stay off until a
// state turns them on).
void EGlobal::LoadIntroRequirements() {
  if (!m_globalShader) m_globalShader = static_cast<ERShader*>(_shaderman.AddRef(0x1a18ca65u, nullptr, 0));
  if (!m_font) {
    m_font = static_cast<ERFont*>(_fontman.AddRef(static_cast<unsigned int>(GetGameFontID()), nullptr, 0));
    if (m_font) m_font->LoadFont();
  }
  if (!m_quickdataA) m_quickdataA = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0xa173a1eeu, nullptr, 0));
  if (!m_strings) m_strings = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0x37a8cfb9u, nullptr, 0));
  if (!m_optionsRecon) m_optionsRecon = new OptionsRecon;
  if (!m_casCostumes) {
    auto* costumes = new CasCostumes;
    m_casCostumes = costumes;
    costumes->LoadCostumesList();
  }
  LoadPreGlobalRequirements();
  if (!m_aptViewer) {
    auto* viewer = CreateAptViewer();
    m_aptViewer = viewer;
    if (viewer) {
      viewer->Init(nullptr);
      viewer->m_field3A8 = 0;
      GetVariableCommandTable::Instance();
      SetVariableCommandTable::Instance();
      GetLocalizableCommandTable::Instance();
      UIUpdateTable::Instance();
      UIDrawTable::Instance();
      UIDB::UIDBInit();
      if (!m_uiRouter) m_uiRouter = new UIRouter;
      if (!m_uiQDTarget) m_uiQDTarget = new UIQDTarget;
      if (!m_uiDBTarget) m_uiDBTarget = new UIDBTarget;
      UI2D::Instance();
      UIAUDIO::Instance();
      if (!m_uiScreenManager) m_uiScreenManager = new UIScreenManager;
      if (!m_g2dTarget) m_g2dTarget = new G2DTarget;
      if (!m_uiAudioTarget) m_uiAudioTarget = new UIAUDIOTarget;
      if (!m_uiAnalog) m_uiAnalog = new UIAnalog;
      UI3D::Instance();
      SIMMY_STUB("EGlobal::LoadIntroRequirements: UIUserDrawCB table, "
                 "EdithDialogPrimitive",
                 0x8003E6B4);
      UIButtonImages::Startup();
      viewer->Load(const_cast<char*>("s2c_root"), true);
      gbUpdateUI = 0;
      gbDrawUI = 0;
      // the GlobalShaders the widget screens' rectangles use
      static const struct {
        const char* name;
        unsigned int id;
      } kShaders[] = {{"tile_pane_decor_titleboxcap", 0x0e5c4bea}, {"tile_fx_tintsquare", 0xac11814a},
                      {"ui-tab_back", 0x1cf6e154}, {"ui-tab_front", 0x115f4afc},
                      {"tile_pane_bevel00_corner", 0xa5b12bed}, {"tile_pane_bevel00_side", 0x2ffc53ed},
                      {"tile_pane_bevel00_fill", 0xf93ec6ec}, {"tile_pane_bevel01_corner", 0x691b2b73},
                      {"tile_pane_bevel01_side", 0xe4a08048}, {"tile_pane_bevel01_fill", 0x32621549},
                      {"tile_pane_bevel02_corner", 0xe7942c90}, {"tile_pane_bevel02_side", 0x6234f2e6},
                      {"tile_pane_bevel02_fill", 0xb4f667e7}, {"tile_pane_bevel03_corner", 0x2b3e2c0e},
                      {"tile_pane_bevel03_side", 0xa9682143}, {"tile_pane_bevel03_fill", 0x7faab442},
                      {"tile_pane_bevel04_corner", 0x21fb2517}, {"tile_pane_bevel04_side", 0xb46d11fb},
                      {"tile_pane_bevel04_fill", 0x62af84fa}};
      for (const auto& e : kShaders)
        UI2D::Instance()->InstallEntry(const_cast<char*>(e.name), e.id, const_cast<char*>("GlobalShaders"), 0);
    }
  }
}

// 8003F0CC
// Empty on GC (the release build's scratch heap is set up elsewhere).
void EGlobal::SetupScratchHeap() {}

// 8003F0D0
void EGlobal::FreeScratchHeap() {}

// 8003ECF4
// Run on the Background thread before the main menu: resets the house-selection fields, then makes
// what the game keeps for its whole life. Ported: the field resets and the two camera directors (one
// per player; the main menu's world map runs its camera through player 0's).
void EGlobal::SetDefaults() {
  m_cam = nullptr;
  m_fieldA0 = 0;
  m_selectedPerson[0] = nullptr;
  m_selectedPerson[1] = nullptr;
  m_floorSet = nullptr;
  m_wallSet = nullptr;
  m_fenceSet = nullptr;
  float half = 0.5f;
  std::memcpy(&m_fieldA8, &half, 4);
  std::memcpy(&m_fieldA4, &half, 4);
  SIMMY_STUB("EGlobal::SetDefaults: PresetHouseGameStatus", 0x80104E58);
  m_house = nullptr;
  // PC: the tables are looked up again when the quickdata is already held. GC cleared them above and
  // only set them on the first call, so a second SetDefaults (back at the main menu after quitting a
  // game) left the wall, floor and fence sets null for the next house.
  if (!m_buildSets)
    m_buildSets = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0x19a16f2du, nullptr, 0));
  {
    if (m_buildSets) {
      if (const void* t = m_buildSets->getTable("WallSet")) m_wallSet = ERQuickdata::TableRows(t);
      if (const void* t = m_buildSets->getTable("FloorSet")) m_floorSet = ERQuickdata::TableRows(t);
      if (const void* t = m_buildSets->getTable("FenceSet")) m_fenceSet = ERQuickdata::TableRows(t);
    }
  }
  SIMMY_STUB("EGlobal::SetDefaults: 0xc33db41/0xc8c09e94 quickdata, shader 0xea090184, "
             "font, ISimsObjectModel white shader, ESpriteRenderMan", 0x8003ED40);
  for (CameraDirector*& director : m_cameraDirectors)
    if (!director) director = new CameraDirector();
  SIMMY_STUB("EGlobal::SetDefaults: WantFearManager", 0x8003EE9C);
  // The rumble switch, both ports turned on (PC: made once; GC made a new one on every call).
  if (!m_vibrate) m_vibrate = new EVibrate();
  m_vibrate->Enable();
  m_vibrate->TurnOn(0);
  m_vibrate->TurnOn(1);
  ESims3DHead::InitShaders();
  Globs::Startup();
  SIMMY_STUB("EGlobal::SetDefaults: substitution strings", 0x8003EE9C);
  GoalUnlock::SetupObjectUnlockInfo();
  InitPlayerCheats();
  // the neighbourhood custom characters (CAS's family slots); a config flag (+0x418) makes slot 0 a default male
  if (!_gd.m_customChars) {
    _gd.m_customChars = new NeighborhoodCustomChars;
    if (ConfigInt(0x418)) _gd.m_customChars->m_chars[0].m_desc = CasSimDescriptionS2C(true);
  }
  m_field160 = 0;
  m_field454 = 0;
  m_field450 = 0;
}

// 8004145C
// A (non-person) object's instance in the current house (GC: an object that has one already is
// re-oriented instead), kept as the object's instance.
void EGlobal::AllocInstance(cXObject* object) {
  ObjSelector* selector = object->GetSelector();
  if (!selector || selector->GetIsPerson() || !m_house || !m_house->m_objectMan) return;
  cXObjectImpl* impl = object->GetObjectImplementation();
  if (impl && impl->m_instance) {
    impl->m_instance->SetObjOrient();
    return;
  }
  ISimInstance* instance = m_house->m_objectMan->AddObject(object, m_house->m_level);
  if (impl) impl->m_instance = instance;
}

// 8004079C
cXPerson* EGlobal::GetSelectedPerson(int player) { return m_selectedPerson[player]; }

// 800409D8
// The player's selected sim is in social mode (its vtable 0x334, cXPersonImpl::IsInSocialMode).
RET_UNKNOWN EGlobal::IsPlayerInSocialMode(int player) {
  cXPerson* person = GetSelectedPerson(player);
  if (!person) return 0;
  cXObject* object = person->AsObject();
  auto* impl = object ? dynamic_cast<cXPersonImpl*>(object->GetObjectImplementation()) : nullptr;
  return impl ? impl->IsInSocialMode() : 0;  // PC: GC calls the person's slot directly
}

// 800407DC
// The player's selected sim. GC: the old one loses (the new one gains) the selected flag and player
// control of its animator, the new one grabs the player's plumb bob, the HUD hears of it, and the
// simulator's selected-person global (3 for player 1, 0x20 for player 2) takes its id.
void EGlobal::SetSelectedPerson(int player, cXPerson* person) {
  // The simulator's global 3 (player 2: 0x20) holds the selected sim's object id (the tutorial waits
  // for it).
  const short global = player == 0 ? 3 : 0x20;
  if (!person) {
    m_selectedPerson[player] = nullptr;
    if (Globs_pSimulator) Globs_pSimulator->SetGlobal(global, 0);
  }
  if (m_selectedPerson[player] == person) return;
  if (cXPerson* old = m_selectedPerson[player]) {
    AttachPersonToHUD(old, false);  // GC: its selected flag cleared (SetHilite -> cXPersonImpl::AttachToHUD)
    if (SAnimator2* anim = old->GetSAnimator()) anim->SetPlayerControl(false);
  }
  m_selectedPerson[player] = person;
  if (person) {
    AttachPersonToHUD(person, true);  // GC: its selected flag set (SetHilite -> cXPersonImpl::AttachToHUD)
    if (SAnimator2* anim = person->GetSAnimator()) anim->SetPlayerControl(true);
    SIMMY_STUB("EGlobal::SetSelectedPerson: selected flag, PlumbBobOwner::GrabPlumbBob", 0x800407DC);
  }
  if (_gd.m_h2dTarget) _gd.m_h2dTarget->SelectedPersonChanged(player, person);  // DAT_80475BCC
  cXObject* o = person ? person->AsObject() : nullptr;
  if (Globs_pSimulator) Globs_pSimulator->SetGlobal(global, static_cast<short>(o ? o->GetID() : 0));
}

// 80041538
// The person's ESim, kept by the house's object manager and put in the house's level, and its shadow
// (in the level's extra list, drawn by the shadow pass); both find their room and hidden state through
// the person's object (ThisSide, Hidden; PC: by the object's id).
void EGlobal::AllocPersonInstance(cXPerson* person) {
  if (!m_house || !m_house->m_objectMan || !m_house->m_level) return;
  ESim* sim = new ESim(person);
  m_house->m_objectMan->AttachObject(sim);
  m_house->m_level->InsertInstance(sim, nullptr);
  cXObject* xob = person ? person->AsObject() : nullptr;
  const unsigned int id = xob ? static_cast<unsigned int>(static_cast<unsigned short>(xob->GetID())) : 0;
  if (EIStaticModel* shadow = sim->GetShadow()) {
    shadow->m_flags |= 0x40000;
    m_house->m_level->InsertInstance(shadow, nullptr);
    shadow->m_hiddenFn = Hidden;
    shadow->m_thisSideFn = ThisSide;
    shadow->m_thisSideData = id;
    shadow->m_hiddenData = id;
  }
  sim->m_thisSideFn = ThisSide;
  sim->m_hiddenData = id;
  sim->m_hiddenFn = Hidden;
  sim->m_thisSideData = id;
  if (cXObject* object = person->AsObject())
    if (cXObjectImpl* impl = object->GetObjectImplementation()) impl->m_instance = sim;
}

// 8003FA24
// The current director (+0xE0); each player's own stays at +0x118.
void EGlobal::SetCameraDirector(CameraDirector* director) { m_cameraDirector = director; }

namespace {
const unsigned short kEmpty[1] = {0};
const unsigned short* const kEmptyHandle = kEmpty;
}  // namespace
// _emptyString (GC: &_pEmptyString), the handle for "no string".
const unsigned short* const* _emptyString = &kEmptyHandle;

// 8003FD34
const unsigned short* const* EGlobal::GetNewUIString(const char* name) {
  if (!name || !m_strings) return _emptyString;
  const void* table = m_strings->getTable(const_cast<char*>("NewUIStrings"));
  const void* row = m_strings->getRow(table, const_cast<char*>(name));
  if (!row) row = m_strings->getRow(table, const_cast<char*>("deprecated_string_message"));
  return row ? ERQuickdata::LocSlot(row, 0) : _emptyString;
}

namespace {
// A row of one of the UI string tables: its text handle, or _emptyString.
const unsigned short* const* TableString(ERQuickdata* strings, const char* tableName, const char* name) {
  if (!name || !strings) return _emptyString;
  const void* table = strings->getTable(const_cast<char*>(tableName));
  const void* row = strings->getRow(table, const_cast<char*>(name));
  return row ? ERQuickdata::LocSlot(row, 0) : _emptyString;
}
}  // namespace

// 8003FB0C
const unsigned short* const* EGlobal::HouseNameText(const char* name) { return TableString(m_strings, "HouseData", name); }
// 8003FB7C
// The "HouseData" row's second text (+0x0C: the name in capitals, the lot's description).
const unsigned short* const* EGlobal::HouseNameCapsText(const char* name) {
  if (!name || !m_strings) return _emptyString;
  const void* table = m_strings->getTable(const_cast<char*>("HouseData"));
  const void* row = m_strings->getRow(table, const_cast<char*>(name));
  return row ? ERQuickdata::LocSlot(row, 0xc) : _emptyString;
}
// 8003FCC4
const unsigned short* const* EGlobal::GetHUDUIString(const char* name) { return TableString(m_strings, "HUDStrings", name); }
namespace {
// The name after its "#": up to 64 characters, ending at a character whose low byte is 0 (GC looked
// at the low byte only); `key` gets the low bytes (the row name).
std::u16string StripKey(const std::u16string& name, std::string* key) {
  std::u16string out;
  for (size_t i = 1; i < name.size() && out.size() < 0x40; ++i) {
    if ((name[i] & 0xff) == 0) break;
    out += name[i];
    if (key) *key += static_cast<char>(name[i] & 0xff);
  }
  return out;
}
}  // namespace

// 8004032C
void EGlobal::GetNghFamilyName(std::u16string& name) {
  if (name.empty() || name[0] != u'#') return;
  name = StripKey(name, nullptr);
}

// 80040414
void EGlobal::GetNghHouseName(std::u16string& name) {
  if (name.empty() || name[0] != u'#') return;
  const void* table = m_strings ? m_strings->getTable(const_cast<char*>("HouseData")) : nullptr;
  std::string key;
  const std::u16string stripped = StripKey(name, &key);
  const void* row = table ? m_strings->getRow(table, const_cast<char*>(key.c_str())) : nullptr;
  if (!row) {
    name = stripped;
    return;
  }
  const unsigned short* const* text = ERQuickdata::LocSlot(row, 0);
  name.clear();
  if (text && *text)
    for (const unsigned short* t = *text; *t; ++t) name += static_cast<char16_t>(*t);
}

// 80042338
// The cheat bits the trees see (simulator globals 0x2E/0x2F): every bit with the "all cheats" config
// switch (+0x400).
unsigned short EGlobal::GetPlayerCheats() const {
  if (ConfigInt(0x400) == 0) return m_playerCheats;
  return 0xffff;
}

// 80042358
// Every cheat with the "all cheats" config switch (+0x400); else bit `cheat` of +0x9C.
int EGlobal::IsPlayerCheatOn(int cheat) const {
  if (ConfigInt(0x400) != 0) return 1;
  return (static_cast<int>(m_playerCheats) >> (cheat & 0x3f) & 1) ? 1 : 0;
}

// 80042388
// Code 10 only plays a sound (no bit; GC). The others flip their bit, with the "on" or "off" sound
// while the game's sound player exists.
void EGlobal::TogglePlayerCheat(int cheat) {
  if (cheat == 10) {
    UIAUDIO::Instance()->PlaySound(0x3ea0941fu);
    return;
  }
  m_playerCheats = static_cast<unsigned short>(m_playerCheats ^ (1 << cheat));
  const int on = IsPlayerCheatOn(cheat);
  if (Globs_pSound) UIAUDIO::Instance()->PlaySound(on ? 0xce619e32u : 0x1c99b71cu);
  std::printf("[cheat] code %d %s (bits %04x)\n", cheat, on ? "on" : "off", m_playerCheats);
}

// 8004246C
// The eleven codes, as EA button bits (Z 2, L 4, R 8, Y 0x10, X 0x20, A 0x40, B 0x80, d-pad up 0x1000,
// right 0x2000, down 0x4000, left 0x8000), each ending at a 0 or after six; then each code's button
// mask and the mask of all of them.
void EGlobal::InitPlayerCheats() {
  static const unsigned short kCodes[11][6] = {
      {0x0004, 0x0008, 0x1000, 0x0040, 0x0002, 0},       // 0  L R Up A Z
      {0x0008, 0x0004, 0x0002, 0x2000, 0x8000, 0},       // 1  R L Z Right Left
      {0x1000, 0x0020, 0x1000, 0x2000, 0x0002, 0},       // 2  Up X Up Right Z
      {0x0002, 0x0020, 0x4000, 0x8000, 0x1000, 0},       // 3  Z X Down Left Up
      {0x0020, 0x0002, 0x8000, 0x0020, 0x1000, 0x0020},  // 4  X Z Left X Up X
      {0x0080, 0x0002, 0x4000, 0x2000, 0x0080, 0},       // 5  B Z Down Right B
      {0x0020, 0x0080, 0x0004, 0x1000, 0x4000, 0},       // 6  X B L Up Down
      {0x0010, 0x0020, 0x0080, 0x0002, 0x8000, 0},       // 7  Y X B Z Left
      {0x0002, 0x0080, 0x1000, 0x4000, 0x2000, 0x0040},  // 8  Z B Up Down Right A
      {0x2000, 0x4000, 0x2000, 0x4000, 0x2000, 0},       // 9  Right Down Right Down Right
      {0x0008, 0x0004, 0x0008, 0x0004, 0x0010, 0},       // 10 R L R L Y
  };
  std::memcpy(m_cheatCodes, kCodes, sizeof(kCodes));
  unsigned short all = 0;
  for (int i = 0; i < 11; ++i) {
    unsigned short mask = 0;
    for (int j = 0; j < 6; ++j) mask = static_cast<unsigned short>(mask | m_cheatCodes[i][j]);
    all = static_cast<unsigned short>(all | mask);
    m_cheatCodeMask[i] = mask;
  }
  m_cheatButtons = all;
}

// 80042614
// Maps the player to the first pad pressing start that no player has; returns its index, or -1.
int EGlobal::TryAutoMapPlayer(int player) {
  if (_pCtrlMan->GetPlayerMapped(static_cast<unsigned int>(player)))
    return _pCtrlMan->GetPlayerMapped(static_cast<unsigned int>(player));
  // PC p1_input=keyboard_mouse (pc_kbm.cpp): the keyboard is pad 1 and is player 1's, whichever pad pressed;
  // the controllers (from pad 2 on) are for player 2.
  const bool keyboardP1 = simmy::kbm_p1_input() == 2;
  for (unsigned int i = 0; i < 4; ++i) {
    EController* pad = _pCtrlMan->GetController(i);
    if (!pad || !pad->GetCmdResult(5, pad->GetBtnPressedMask(0xffffffffu), pad->GetBtnDownMask(0xffffffffu), false))
      continue;
    if (keyboardP1 && player == 0) i = 0;
    else if (keyboardP1 && i == 0) continue;
    bool taken = false;
    for (unsigned int p = 0; p < 2; ++p)
      if (_pCtrlMan->GetPlayerMapped(p) && _pCtrlMan->GetPlayerControllerIndex(p) == static_cast<int>(i)) taken = true;
    if (!taken) {
      _pCtrlMan->MapPlayerToController(static_cast<unsigned int>(player), i);
      return static_cast<int>(i);
    }
  }
  return -1;
}

// 800402BC
const unsigned short* const* EGlobal::GetMemCardString(const char* name) {
  return TableString(m_strings, "MemCardStrings", name);
}
// 8003FDC8
const unsigned short* const* EGlobal::GetPDAString(const char* name) { return TableString(m_strings, "PDAStrings", name); }

const unsigned short* EGlobal::NewUIText(const char* name) {
  const unsigned short* const* handle = _globals.GetNewUIString(name);
  return handle && *handle ? *handle : kEmpty;
}

// 80042718
// Saving is allowed unless the HUD is in a job mode (+0xC8) or a player's camera director holds a
// shot (state 1 with +0x234 set); in two-player mode both directors count.
RET_UNKNOWN EGlobal::IsSaveGameEnabled() {
  const HUDTarget* hud = _gd.m_hudTarget;  // DAT_80475BC8
  const CameraDirector* d0 = _globals.CameraDirectorOf(0);
  int enabled = 0;
  // PC: GC dereferences the HUD and the directors (live mode always has them).
  if ((!hud || hud->m_mode == 0) && (!d0 || d0->m_state != 1 || d0->m_field234 == 0)) enabled = 1;
  if (!_gd.m_fieldEC) return enabled;  // DAT_80475C70
  if (!enabled) return 0;
  const CameraDirector* d1 = _globals.CameraDirectorOf(1);
  if (d1 && d1->m_state == 1 && d1->m_field234 != 0) return 0;
  return 1;
}
