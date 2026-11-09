// Port of game/flashpimenu.cpp: FlashPiMenu, the player's interaction menu builder, and the menu's
// name/icon helpers; plus the PC stand-ins for the simulator side it calls (CollectInteractionsForObject
// with ObjTestSim::AppendInteractionsForMenu, and starting the chosen interaction). See actionmenu.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/influencemap.hpp"
#include "simmy/classes/interaction.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
void PlaySound(unsigned int id) { UIAUDIO::Instance()->PlaySound(id); }

std::u16string FromLoc(const unsigned short* s) {
  std::u16string out;
  for (; s && *s; ++s) out += static_cast<char16_t>(*s);
  return out;
}
std::string Narrow(const std::u16string& s) {
  std::string out;
  for (char16_t c : s) out += c < 0x80 ? static_cast<char>(c) : '?';
  return out;
}

// The object is still one of the module's (GC: compares pointers along the object list; the object
// itself is not read, it may be gone: the go-here object of a game quit to the main menu).
bool InModule(cXObjectImpl* object) {
  if (!object || !Globs_pObjectModule) return false;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
    if (o == object) return true;
  // PC: the lot's people by id too.
  for (int id = 1; id <= Globs_pObjectModule->GetNumObjects(); ++id) {
    cXObject* x = Globs_pObjectModule->GetObjectFromID(static_cast<short>(id));
    if (x && x->GetObjectImplementation() == object) return true;
  }
  return false;
}

// The player's selected person (DAT_80475E88[player]) as the simulator's object.
cXPersonImpl* SelectedPersonImpl(int player) {
  cXPerson* selected = _globals.GetSelectedPerson(player);
  cXObject* self = selected ? selected->AsObject() : nullptr;
  return self ? dynamic_cast<cXPersonImpl*>(self->GetObjectImplementation()) : nullptr;
}

// ObjSelector::GetCatalogShortName (801109A8) through its CatalogResource (8011148C,
// CatalogResourceImpl::Load 800B1CC8): the object file's catalogue entry (ResFile +0x1C, 0x10 bytes:
// +0 its id, the definition's +0x52; +4 name, +8 description, +0xC short name).
std::u16string CatalogShortName(ObjSelector* selector) {
  if (!selector || !selector->m_def) return {};
  const unsigned char* file = SelectorResFile(selector);
  const auto* list = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x1c)) : nullptr;
  const unsigned int n = list ? ERQuickdata::U32(list - 4, 0) : 0;
  const short id = ERQuickdata::S16(selector->m_def, 0x52);
  const unsigned char* entry = nullptr;
  for (unsigned int i = 0; i < n && !entry; ++i)
    if (ERQuickdata::S16(list + i * 0x10, 2) == id)
      entry = list + i * 0x10;
  if (!entry && n) entry = list;  // FindRes<CatalogData>
  return entry ? FromLoc(ERQuickdata::LocString(entry, 0xc)) : std::u16string();
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// FlashPiMenu

// 80039D70
FlashPiMenu::FlashPiMenu(int player) : m_player(player) {}

// 80039D94
FlashPiMenu::~FlashPiMenu() { CleanUpAllMenus(); }

// 80039DE0
void FlashPiMenu::Update() {}

// 80039DE4
void FlashPiMenu::CleanUpAllMenus() {}

// 80039DE8
// The "go here" object at the interactor and what the sim can do with it ("Go Here"): with `immediate`
// (no object was near) the first is chosen at once and the object goes, else they make the menu.
// 1: in the menu.
int FlashPiMenu::CreateMenuForGoHere(bool immediate) {
  if (!SelectedPersonImpl(m_player)) return 0;
  CreateGoHereObjectForMenu();
  if (!m_destination) return 0;
  MenuInteractionList list;
  CollectInteractionsForObject(m_destination, list, m_player, 10000);  // ObjTestSim::AppendInteractionsForMenu
  if (!list.empty()) {
    if (immediate) {
      ChooseAction(m_destination, &list[0], false);
      CleanupDestinationObject();
      return 0;
    }
    CreateInteractionMenu(m_destination, list);
  }
  return 1;
}

// 80039F14
// The objects' interactions into the player's menu (with one object, its interactions at the top;
// with several, one sub menu per object), then the menu shown. 1: shown.
int FlashPiMenu::CreateObjectMenuFromObjList(const std::vector<cXObjectImpl*>& objects) {
  if (!SelectedPersonImpl(m_player)) return 0;
  const bool empty = objects.empty();
  const bool single = objects.size() == 1;
  for (cXObjectImpl* o : objects) AddObjectInteractionsToMenu(o, single);
  if (!single && !CreateMenuForGoHere(empty)) return 0;
  HUDTarget* hud = _gd.m_hudTarget;
  return hud ? hud->ShowMenu(m_player) : 0;
}

// 80039FF4
void FlashPiMenu::AddObjectInteractionsToMenu(cXObjectImpl* object, bool single) {
  MenuInteractionList list;
  CollectInteractionsForObject(object, list, m_player, 10000);  // MAX_INTERACTIONS (80500600)
  if (list.empty()) return;
  ERShader* thumbnail = nullptr;
  const unsigned int shader = GetShaderForObject(object, &thumbnail);
  HUDTarget* hud = _gd.m_hudTarget;
  if (!single && hud) hud->AddMenu(m_player, object, GetObjectMenuName(object), shader, thumbnail);
  if (thumbnail) thumbnail->DelRef();  // GetShaderForObject's reference (the menu row holds its own)
  CreateInteractionMenu(object, list);
  if (!single && hud) hud->CompleteMenu(m_player);
}

namespace {
// 8003A104
// Drops "<...>" formatting tags.
[[maybe_unused]] void StripFormatting(std::u16string& s) {
  size_t at = 0;
  for (;;) {
    at = s.find(u'<', at);
    if (at == std::u16string::npos) return;
    const size_t end = s.find(u'>', at + 1);
    if (end == std::u16string::npos) return;
    s.erase(at, end - at + 1);
  }
}

// 8003A180
// A coloured interaction: spaces become '_' and a "<COLOR_FFrrggbb>" tag goes in front (colours 1..3:
// 1AAF2B, F5F500, 911A12; 4: 808080).
void ColorStringForFlash(std::u16string& s, int color) {
  if (color == 0) return;
  static const unsigned char kColors[12] = {0x1a, 0xaf, 0x2b, 0xf5, 0xf5, 0x00, 0x91, 0x1a, 0x12, 0x80, 0x80, 0x80};
  const unsigned char* c = kColors + (color - 1) * 3;
  char tag[32];
  std::snprintf(tag, sizeof tag, "<COLOR_FF%02X%02X%02X>", c[0], c[1], c[2]);
  for (char16_t& ch : s)
    if (ch == u' ') ch = u'_';
  std::u16string out;
  for (const char* p = tag; *p; ++p) out += static_cast<char16_t>(*p);
  s = out + s;
}

// 8003A298
// 4 first, then 0, then the higher number.
bool ColorIsHigherPriority(int a, int b) {
  if (a == b) return false;
  if (a == 4) return true;
  if (b == 4) return false;
  if (a == 0) return true;
  if (b == 0) return false;
  return b < a;
}
}  // namespace

// 8003A2E0
// The interactions as rows; names "<group>/<name>" go under a sub menu "<group>" (coloured by its
// highest-priority member's colour), each row coloured by its interaction. A row is shown disabled
// when the interaction is (flag 0x800).
int FlashPiMenu::CreateInteractionMenu(cXObjectImpl* object, MenuInteractionList& list) {
  ERShader* thumbnail = nullptr;
  const unsigned int objectShader = GetShaderForObject(object, &thumbnail);
  HUDTarget* hud = _gd.m_hudTarget;
  while (!list.empty()) {
    const std::u16string& firstName = list.front().name;
    const size_t slash = firstName.find(u'/', 1);
    const std::u16string group = firstName.substr(0, slash);
    int color = list.front().GetTextColor();
    for (const MenuInteraction& i : list) {
      const int c = i.GetTextColor();
      if (ColorIsHigherPriority(color, c) && i.name.find(u'/', 1) == slash && !i.name.compare(0, slash, group, 0, slash))
        color = c;
    }
    const bool noGroup = slash == std::u16string::npos;
    std::u16string groupName = group;
    ColorStringForFlash(groupName, color);
    if (!noGroup && hud) hud->AddMenu(m_player, object, groupName, objectShader, thumbnail);
    for (size_t k = 0; k < list.size();) {
      const MenuInteraction& i = list[k];
      if (i.name.find(u'/', 1) != slash || (!noGroup && i.name.compare(0, slash, group, 0, slash))) {
        ++k;
        continue;
      }
      ERShader* iconThumb = nullptr;
      const unsigned int iconShader = GetShaderForObject(i.icon, &iconThumb);
      std::u16string row = noGroup ? i.name : i.name.substr(slash + 1);
      ColorStringForFlash(row, i.GetTextColor());
      if (hud) hud->AddMenuItem(m_player, object, &i, row, iconShader, iconThumb, 0, (i.flags >> 0xb & 1) != 0);
      if (iconThumb) iconThumb->DelRef();
      list.erase(list.begin() + static_cast<long>(k));
    }
    if (!noGroup && hud) hud->CompleteMenu(m_player);
  }
  if (thumbnail) thumbnail->DelRef();
  return 1;
}

// 8003A6AC
// The go-here object goes (when it is still in the module).
void FlashPiMenu::CleanupDestinationObject() {
  if (!m_destination) return;
  if (InModule(m_destination)) {
    const short id = static_cast<short>(m_destination->m_id);  // GC: the object's vtable 0x32C
    m_destination = nullptr;
    if (Globs_pObjectModule) Globs_pObjectModule->KillObject(id);  // ObjectModule vtable 0x5C
  }
  m_destination = nullptr;
}

// 8003A7F8
// A new go-here object (GUID 0x7C4) at the player's interactor rounded to the nearest tile corner, moved
// to the nearest place the player's person can stand (FindGoodLocation; else the tile's centre), placed
// there if it can be (object vtables 0x134 CanPlace / 0x13C Place), else deleted.
void FlashPiMenu::CreateGoHereObjectForMenu() {
  InteractorModule__InteractorInfo info;
  if (!GetPlayerInteractorInfo(m_player, info)) return;  // InteractorManager::GetPlayerInteractor
  ObjectModuleImpl* module = Globs_pObjectModule;
  ObjSelector* selector = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(0x7c4) : nullptr;
  short id = 0;
  if (!selector || !module || !module->MakeNewOutOfWorldObject(selector, &id) || !id) return;
  cXObject* object = module->GetObjectFromID(id);
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  if (!impl) return;
  auto round = [](float v) {
    int i = static_cast<int>(v);
    if (0.5f <= v - static_cast<float>(i)) ++i;
    return i << 4;
  };
  // DAT_80475E70 / DAT_80475E74: floats (0.5) in EGlobal's int fields +0xA4 / +0xA8. (PC fix: they were
  // converted as integers, 0x3F000000, so the go-here object never went anywhere.)
  float offX, offY;
  std::memcpy(&offX, &_globals.m_fieldA4, 4);
  std::memcpy(&offY, &_globals.m_fieldA8, 4);
  int x = round(info.m_x - offX);
  int y = round(info.m_y - offY);
  // The player's selected person's FindGoodLocation (person vtable 0x194; FindGoodLocationParams: from
  // that corner on level 1, no direction, the same room, any tile): the nearest place the person can
  // stand on; failing that the corner's tile centre.
  int at[2] = {x, y};
  cXObjectImpl* person = SelectedPersonImpl(m_player);  // DAT_80475E88[player]
  int found[2];
  if (person && ObjFindGoodLocation(person, at, 1, true, false, found)) {
    at[0] = found[0];
    at[1] = found[1];
  } else {
    at[0] = (x & ~0xf) | 8;
    at[1] = (y & ~0xf) | 8;
  }
  static const bool trace = std::getenv("SIMMY_CTL_TRACE") != nullptr;  // PC dev trace
  if (trace)
    std::printf("[ctl] player %d go here: corner %d,%d -> tile %d,%d\n", m_player + 1, y >> 4, x >> 4, at[1] >> 4,
                at[0] >> 4);
  // CanPlace there (level 1), then Place (its location, room id and model), else it goes.
  if (!ObjCanPlace(impl, at, 1, nullptr, 0)) {
    module->KillObject(id);
    m_destination = nullptr;
    return;
  }
  ObjPlace(impl, at, 1, nullptr, 0);
  m_destination = impl;
}

// 8003A784
void FlashPiMenu::Die() {
  if (!m_dead) return;
  CleanupDestinationObject();
  CleanUpAllMenus();
  // GC: the player's camera back to state 0 (ESimsCam::SetState, not ported: the menu never changed it).
  m_dead = 0;
}

// 8003A7EC
void FlashPiMenu::Kill() { m_dead = 1; }

// 8003AAB0
// The chosen interaction (none: the menu closed) to the player's interactor, if its person and
// object are still there; the accept or refuse sound. With `die`, the menu is done.
int FlashPiMenu::ChooseAction(cXObjectImpl* object, const MenuInteraction* interaction, bool die) {
  if (!object) {
    m_dead = 1;
    Die();
    return 1;
  }
  if (!interaction || !Globs_pObjectModule) return 0;
  int r = 0;
  // (PC: the lot's people are not in the object module's list; the player's selected person counts.)
  const bool personThere = InModule(interaction->person) || interaction->person == SelectedPersonImpl(m_player);
  if (InModule(interaction->object) && personThere) {
    // GC: InteractorManager::GetPlayerInteractor(player, -1) vtable 0xAC, DirectInteractor::ChooseAction
    // (80205BB0) -> the person's vtable 0xB4 (the action queue).
    // A player in social mode: the social-mode interactor takes the choice (socialmode.cpp).
    r = SocialModeChooseAction(m_player & 1, interaction) ? 1 : StartMenuInteraction(*interaction);
  }
  PlaySound(r ? 0x374bdcea : 0x6e2778b7);
  if (r && m_destination == object) m_destination = nullptr;
  if (die) {
    m_dead = 1;
    Die();
  }
  return r;
}

// 8003ACA0
// GC: the player's interactor's ChooseAction(null).
void FlashPiMenu::BackedOutOfMenu() { SocialModeChooseAction(m_player & 1, nullptr); }

// 8003AFCC
int FlashPiMenu::GetPlayerId() { return m_player & 0xff; }

// ---------------------------------------------------------------------------------------------------
// Names and icons

// 8003ACFC
std::u16string GetObjectMenuName(cXObjectImpl* object) {
  ObjSelector* selector = object ? object->m_selector : nullptr;
  if (!selector) return {};
  if (selector->GetIsPerson()) return selector->GetUserName();
  // GC: a multi-tile part (vtable 0x394) names its master selector's entry; PC: the parts share it.
  return CatalogShortName(selector);
}

// 8003ADC4
// The go-here icon, a person's NPC icon (else its portrait), or the definition's ResData icon.
unsigned int GetShaderForObject(cXObjectImpl* object, ERShader** thumbnail) {
  if (thumbnail) *thumbnail = nullptr;
  if (!object || !object->m_def) return 0;
  if (objdef::GUID(object->m_def) == 0x7c4) return 0xfe47c11a;
  ObjSelector* selector = object->m_selector;
  if (selector && selector->GetIsPerson()) {
    // A story sim's portrait shader; else the sim's own thumbnail as a resource.
    const unsigned int id = static_cast<unsigned int>(GetNpcShaderId(static_cast<int>(selector->GetGUID()), true));
    if (id == 0 && thumbnail) selector->GetBigThumbnail(thumbnail);
    return id;
  }
  // GC: a multi-tile part takes the first part of its group with a ResData; PC: its own.
  const unsigned char* res = objdef::ResData(object->m_def);
  return res ? ERQuickdata::U32(res, 0x1c) : 0;
}

// ---------------------------------------------------------------------------------------------------
// PC stand-ins for the simulator side

// 80041B9C
// GC: (cXObject*, InteractionList&, cXPerson*, int) 80041BE0 builds an ObjTestSim for the object (a
// multi-tile object's lead parts) and AppendInteractionsForMenu (8011271C) tests each tree table entry
// (TestInteraction: the check tree on the person's stack, age availability, autonomy flags) and adds
// the available ones. PC: the object's tree table (ResFile +0x24, 0x24 bytes an entry: +0 check tree,
// +2 action tree, +0x10 flags (0x80 debug), +0x16 attenuation code (> 99 not offered), +0x1C name) with
// each check tree run on the person's stack, in table order, as the A-button stand-in did.
bool PlayerUsesDirectInteractor(int player);  // directinteractor.cpp

void CollectInteractionsForObject(cXObjectImpl* object, MenuInteractionList& list, int player, int max) {
  cXPersonImpl* person = SelectedPersonImpl(player);
  if (!object || !person || !person->m_treeSim) return;
  object = object->GetInteractionLeader();  // ObjTestSim::SetStackObject
  const unsigned char* file = SelectorTreeTabResFile(object->m_selector);
  const auto* tables = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24)) : nullptr;
  if (!tables || ERQuickdata::U32(tables - 4, 0) == 0) return;
  const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(tables, 0));
  const unsigned int n = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
  Behavior* behavior = object->GetBehavior();
  // ObjTestSim::AppendInteractionsForMenu: is the person a player's sim under direct control
  // (InteractorManager::GetPlayerInteractor(player, 0))?
  bool direct = false;
  for (int p = 0; p < kMaxPlayers; ++p) {  // PC mod: 4 players (GC: 2)
    cXPerson* sel = _globals.GetSelectedPerson(p);  // DAT_80475E88
    if (sel && sel->AsObject() == person && PlayerUsesDirectInteractor(p)) direct = true;
  }
  InfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetInfluenceMap() : nullptr;
  int added = 0;
  for (unsigned int i = 0; i < n && added < max; ++i) {
    const unsigned char* e = entries + i * 0x24;
    const unsigned short* name = ERQuickdata::LocString(e, 0x1c);
    const bool debug = (ERQuickdata::U32(e, 0x10) & 0x80) != 0 || (name && name[0] == '!');
    const short check = ERQuickdata::S16(e, 0), action = ERQuickdata::S16(e, 2);
    if (debug || !action) continue;  // GC: entry vtable 0xCC (debug, unless the cheat), then RunMenuCheckTree
    // A player's sim under direct control is offered only the entries whose activation area holds it
    // (InfluenceMap::CalculateAffectedArea).
    if (direct && map) {
      int r[4];
      map->CalculateAffectedArea(object, e, r);
      const int* at = person->m_location;
      if (!(at[1] < r[1] && r[3] <= at[1] && at[0] < r[0] && r[2] <= at[0])) continue;
    }
    if (ERQuickdata::U32(e, 0x10) & 0x40) continue;  // ObjTestSim::TestInteraction: not for adults
    // TestInteraction: the check tree runs with {not autonomous, not a pushed type}; the person's data
    // 0x32 it may set hides the entry (1; 2 greys it: SetupInteractionColors).
    person->m_data.WriteVar(0x32, 0);
    short params[4] = {0, 0, 0, 0};
    PcMenuBuild items;  // ObjTestSim::RunMenuCheckTree: the check tree may make menu items
    g_pcMenuBuild = &items;
    const int ok = !check || person->m_treeSim->RunCheckTree(behavior, check, static_cast<short>(object->m_id), params);
    g_pcMenuBuild = nullptr;
    if (!ok) continue;
    if (person->m_data[0x32] == 1) continue;
    for (const PcMenuItem& item : items) {
      MenuInteraction m;
      m.person = person;
      m.object = object;
      m.icon = object;
      m.entry = static_cast<int>(i);
      m.actionTree = action;
      m.name = item.name;
      for (int k = 0; k < 4; ++k) m.vars[k] = item.vars[k];
      list.push_back(m);
      ++added;
    }
    if (!items.empty()) continue;
    MenuInteraction m;
    m.person = person;
    m.object = object;
    m.icon = object;
    m.entry = static_cast<int>(i);
    m.actionTree = action;
    m.name = FromLoc(name);
    list.push_back(m);
    ++added;
  }
}

// GC: cXPersonImpl::AddAction (the person's action queue, then its main loop's TryGosubFoundAction):
// the chosen interaction is queued at the player's priority, so the cancel button can take it back.
int StartMenuInteraction(const MenuInteraction& m) {
  cXPersonImpl* person = m.person;
  if (!person || !m.object || !person->m_treeSim || m.entry < 0) return 0;
  Interaction action(person, m.object, m.entry);
  for (int k = 0; k < 4; ++k) action.m_stackVars[k] = m.vars[k];
  // Its name as the menu showed it (ObjTestSim::MakeNewMenuItem puts a check tree's own item text in the
  // interaction, +0x34; a plain entry's is its tree table name, as GetName would give).
  action.m_name = m.name;
  const int r = person->AddAction(&action);
  const std::string who = person->DebugName();
  std::printf("[sim] %s queues \"%s\" on %s (menu): %s\n", who.c_str(), Narrow(m.name).c_str(), m.object->DebugName(),
              r ? "queued" : "refused");
  return r;
}
