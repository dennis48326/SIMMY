// Port of game/interactorvisualizer.cpp (the classic, direct and social-mode interactors' parts, the draw
// callback and the resource sets' bookkeeping) and game/interactorresourceset.cpp (models, shaders and
// draw records; textures are not used by the ported interactors), plus EHouse::InsertHouseObject /
// ERLevel::InsertHouseObject. See siminteractor.hpp.
// Also the build tools' cursors and previews (the floor rectangle or room, the wall line and the walls it
// deletes, the wallpaper on the sides it covers or round the room). The object manipulator's part is in
// placemanipulator.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/siminteractor.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/classes/buildtools.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

// PC: the build tools' part of SetupDrawCallback / InteractorOrderTableCallback (the tools are held by
// buildtools.cpp, outside the manager stand-in).
bool BuildToolPreDraw(int player, InteractorModule__InteractorVisualizer& v);
bool BuildToolDraw(int player, EOrderTableData* otd, InteractorModule__InteractorVisualizer& v);
unsigned short InteractorGetRoomIdFromPoint(CTilePt& tile);  // 802131F4 (wallpainter.cpp)

namespace {
// EHouse +0x64: the house's own instance, which house objects' draw records point at (full alpha).
// PC: the house's layout past +0x58 is not kept, so it is a stand-alone instance.
EInstance& HouseInstance() {
  static EInstance instance = [] {  // PC: EHouse +0x64, at full alpha
    EInstance i;
    i.m_alpha[0] = i.m_alpha[1] = 1.0f;
    i.m_fadeMode = 0;
    return i;
  }();
  return instance;
}
// What the order table needs before the callback sets its own (a model matrix and lights).
EMat4& IdentityMatrix() {
  static EMat4 m = [] {
    EMat4 r;
    r.Id();
    return r;
  }();
  return m;
}
EVec3 PlayerColor(int player) {
  const float* c = g_plumbBobParms.m_colorScale + (player & 1) * 3;
  return EVec3{c[0], c[1], c[2]};
}
}  // namespace

// 80025464
// Into the house's level's list of house objects (drawn with the order table), as the house's instance.
void EHouse::InsertHouseObject(EOrderTableData* otd) {
  if (!m_level || !otd) return;
  otd->m_instance = &HouseInstance();
  m_level->InsertHouseObject(otd);
}

// 802379A0
void ERLevel::InsertHouseObject(EOrderTableData* otd) {
  otd->m_next = m_houseObjects;
  m_houseObjects = otd;
}

// ---------------------------------------------------------------------------------------------------
// InteractorModule::InteractorResourceSet

// 8020C09C
InteractorModule__InteractorResourceSet::InteractorModule__InteractorResourceSet() {}
// 8020C14C
InteractorModule__InteractorResourceSet::~InteractorModule__InteractorResourceSet() { ClearSet(); }

namespace {
template <typename T>
typename std::vector<std::pair<unsigned int, T*>>::iterator Find(std::vector<std::pair<unsigned int, T*>>& v, unsigned int id) {
  auto it = std::lower_bound(v.begin(), v.end(), id, [](const std::pair<unsigned int, T*>& e, unsigned int i) { return e.first < i; });
  return it != v.end() && it->first == id ? it : v.end();
}
template <typename T>
void Insert(std::vector<std::pair<unsigned int, T*>>& v, unsigned int id, T* r) {
  auto it = std::lower_bound(v.begin(), v.end(), id, [](const std::pair<unsigned int, T*>& e, unsigned int i) { return e.first < i; });
  v.insert(it, std::make_pair(id, r));
}
}  // namespace

// 8020C354
// The model held (EResourceManager::AddRef) and listed by id; one already listed is returned.
ERModel* InteractorModule__InteractorResourceSet::AddModel(unsigned int id) {
  auto it = Find(m_models, id);
  if (it != m_models.end()) return it->second;
  auto* model = static_cast<ERModel*>(_modelman.AddRef(id, nullptr, 0));
  if (!model) return nullptr;
  Insert(m_models, id, model);
  return model;
}
// 8020CCE0
int InteractorModule__InteractorResourceSet::RemoveModel(unsigned int id) {
  auto it = Find(m_models, id);
  if (it == m_models.end()) return 0;
  if (it->second) it->second->DelRef();
  m_models.erase(it);
  return 1;
}
// 8020CE44
// A listed model; else the manager's copy if it is loaded (GetRef: not held), listed from then on.
ERModel* InteractorModule__InteractorResourceSet::GetModel(unsigned int id) {
  auto it = Find(m_models, id);
  if (it != m_models.end()) return it->second;
  auto* model = static_cast<ERModel*>(_modelman.GetRef(id));
  if (!model) return nullptr;
  model->AddRef();  // PC: the set releases what it lists (GC's GetModel lists it without a reference)
  Insert(m_models, id, model);
  return model;
}
// 8020D6A4
ERShader* InteractorModule__InteractorResourceSet::AddShader(unsigned int id) {
  auto it = Find(m_shaders, id);
  if (it != m_shaders.end()) return it->second;
  auto* shader = static_cast<ERShader*>(_shaderman.AddRef(id, nullptr, 0));
  if (!shader) return nullptr;
  Insert(m_shaders, id, shader);
  return shader;
}
// 8020E038
int InteractorModule__InteractorResourceSet::RemoveShader(unsigned int id) {
  auto it = Find(m_shaders, id);
  if (it == m_shaders.end()) return 0;
  if (it->second) it->second->DelRef();
  m_shaders.erase(it);
  return 1;
}
// 8020E19C
ERShader* InteractorModule__InteractorResourceSet::GetShader(unsigned int id) {
  auto it = Find(m_shaders, id);
  if (it != m_shaders.end()) return it->second;
  auto* shader = static_cast<ERShader*>(_shaderman.GetRef(id));
  if (!shader) return nullptr;
  shader->AddRef();
  Insert(m_shaders, id, shader);
  return shader;
}
// 8020FD74
// Every model and shader let go, the textures and draw records forgotten.
void InteractorModule__InteractorResourceSet::ClearSet() {
  for (auto& m : m_models)
    if (m.second) m.second->DelRef();
  m_models.clear();
  for (auto& s : m_shaders)
    if (s.second) s.second->DelRef();
  m_shaders.clear();
  m_textures.clear();
  m_otds.clear();
}
// 8020FEF4
void InteractorModule__InteractorResourceSet::AddOrderTableData(EOrderTableData& data) { m_otds.push_back(data); }
// 80210208
EOrderTableData* InteractorModule__InteractorResourceSet::GetOrderTableData(unsigned int index) {
  return index < m_otds.size() ? &m_otds[index] : nullptr;
}
// 80210240
int InteractorModule__InteractorResourceSet::GetOrderTableDataCount() { return static_cast<int>(m_otds.size()); }

// ---------------------------------------------------------------------------------------------------
// InteractorModule::InteractorVisualizer

ERC* InteractorModule__InteractorVisualizer::s_pERC = nullptr;

// 80213D8C
InteractorModule__InteractorVisualizer::InteractorModule__InteractorVisualizer() {}

// 80213DE0
InteractorModule__InteractorVisualizer::~InteractorModule__InteractorVisualizer() {
  for (auto& s : m_sets) delete s.second;
  m_sets.clear();
  Shutdown();
}

// 80213E74
// One draw record per player, drawn by InteractorOrderTableCallback.
void InteractorModule__InteractorVisualizer::Initialize() {
  for (EOrderTableData*& otd : m_otd) {
    if (!otd) otd = new EOrderTableData;
    otd->m_matrix = &IdentityMatrix();  // GC: 0 (the callback sets its own); PC: the order table sets one first
    otd->m_lights = &m_lights;
    otd->m_instance = nullptr;
    otd->m_dl = nullptr;
    otd->m_animController = nullptr;
    otd->m_pos = nullptr;
    otd->m_callback = InteractorOrderTableCallback;
  }
}

// 80213F54
void InteractorModule__InteractorVisualizer::Shutdown() {
  for (EOrderTableData*& otd : m_otd) {
    delete otd;
    otd = nullptr;
  }
}

// 80217D00
// The interactor's set, made (empty) the first time; the sets are kept sorted by interactor.
InteractorModule__InteractorResourceSet* InteractorModule__InteractorVisualizer::CreateResourceSet(InteractorModule__Interactor& in) {
  auto it = std::lower_bound(m_sets.begin(), m_sets.end(), &in,
                             [](const auto& e, InteractorModule__Interactor* i) { return e.first < i; });
  if (it != m_sets.end() && it->first == &in) return it->second;
  auto* set = new InteractorModule__InteractorResourceSet;
  m_sets.insert(it, std::make_pair(&in, set));
  return set;
}

// 80218C44
int InteractorModule__InteractorVisualizer::DestroyResourceSet(InteractorModule__Interactor& in) {
  auto it = std::lower_bound(m_sets.begin(), m_sets.end(), &in,
                             [](const auto& e, InteractorModule__Interactor* i) { return e.first < i; });
  if (it == m_sets.end() || it->first != &in) return 0;
  it->second->ClearSet();
  delete it->second;
  m_sets.erase(it);
  return 1;
}

// 80218DE4
InteractorModule__InteractorResourceSet* InteractorModule__InteractorVisualizer::GetInteractorResourceSet(InteractorModule__Interactor* in) {
  if (!in) return nullptr;
  auto it = std::lower_bound(m_sets.begin(), m_sets.end(), in,
                             [](const auto& e, InteractorModule__Interactor* i) { return e.first < i; });
  return it != m_sets.end() && it->first == in ? it->second : nullptr;
}

// 80216E98
// The classic cursor's beam (0x13187659).
bool InteractorModule__InteractorVisualizer::CreateResources(InteractorModule__SimInteractor& in) {
  InteractorModule__InteractorResourceSet* set = CreateResourceSet(in);
  if (set) set->AddModel(0x13187659);
  return set != nullptr;
}
// 80216ED8
void InteractorModule__InteractorVisualizer::DestroyResources(InteractorModule__SimInteractor& in) { DestroyResourceSet(in); }

// 80216EF8
// The player's draw record at the interactor's position, into the house's order table.
void InteractorModule__InteractorVisualizer::PreDraw(InteractorModule__SimInteractor& in) {
  EOrderTableData* otd = m_otd[in.m_player & 1];
  if (!otd) return;
  otd->m_pos = &in.m_pos.x;  // GC: +0x18 holds the player (PC: the callback finds it by the position)
  if (EHouse* house = _globals.m_house) house->InsertHouseObject(otd);
}

// 80216F54
// The player's plumb bob (in the player's colour; while the sim is in the world and not hidden, data
// 0x22), the cursor's squares, and the beam at the cursor, its height growing in (GetBeamScale).
void InteractorModule__InteractorVisualizer::Draw(InteractorModule__SimInteractor& in) {
  const int player = in.m_player;
  PlumbBob* bob = static_cast<unsigned int>(player) < 2 ? PlumbBob::s_pPlumbBob[player] : nullptr;
  if (bob) {
    cXPerson* person = _globals.GetSelectedPerson(player);
    bob->SetPlayerColor(player);
    cXObjectImpl* impl = person && person->AsObject() ? person->AsObject()->GetObjectImplementation() : nullptr;
    if (impl && ObjIsInWorld(impl) && impl->GetData(0x22) == 0) bob->Draw(s_pERC);
  }
  EVec3 color = PlayerColor(player);
  for (FadeSquare& sq : in.m_squares)
    if (sq.ShouldDraw()) sq.Draw(s_pERC, color);
  s_pERC->SetAlpha(1.0f);
  const float beam = static_cast<float>(in.GetBeamScale());
  EMat4 m;
  m.Id();
  EVec3 at{in.m_pos.x, in.m_pos.y, 0.0f};
  m.Translate(at);
  EVec3 s{1.0f, 1.0f, beam};
  m.PreScale(s);
  DrawResource(in, 0x13187659, color, &m);
}

// 80217130
int InteractorModule__InteractorVisualizer::CreateResources(InteractorModule__DirectInteractor&) { return 1; }
// 80217138
int InteractorModule__InteractorVisualizer::DestroyResources(InteractorModule__DirectInteractor&) { return 1; }
// 80217140
void InteractorModule__InteractorVisualizer::PreDraw(InteractorModule__DirectInteractor& in) {
  EOrderTableData* otd = m_otd[in.m_player & 1];
  if (!otd) return;
  otd->m_pos = &in.m_pos.x;
  if (EHouse* house = _globals.m_house) house->InsertHouseObject(otd);
}
// 8021719C
// The player's plumb bob over the sim, unless the camera director plays the game camera (8).
void InteractorModule__InteractorVisualizer::Draw(InteractorModule__DirectInteractor& in) {
  const int player = in.m_player;
  PlumbBob* bob = static_cast<unsigned int>(player) < 2 ? PlumbBob::s_pPlumbBob[player] : nullptr;
  cXPerson* person = _globals.GetSelectedPerson(player);
  if (!bob) return;
  bob->SetPlayerColor(player);
  cXObjectImpl* impl = person && person->AsObject() ? person->AsObject()->GetObjectImplementation() : nullptr;
  if (!impl || !ObjIsInWorld(impl) || impl->GetData(0x22) != 0) return;
  CameraDirector* d = _globals.CameraDirectorOf(player);  // DAT_80475EE4
  if (d && d->m_state == CameraDirector::kGame) return;
  bob->Draw(s_pERC);
}

// 802172FC
// The player's interactor draws itself (vtable 0xBC), then the order table forgets its draw state.
void InteractorModule__InteractorVisualizer::InteractorOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  s_pERC = data.m_rc;
  InteractorModule__InteractorVisualizer* v = InteractorVisualizer();
  if (!v || !s_pERC) return;
  for (int player = 0; player < 2; ++player) {
    // GC: the player from the record (+0x18) and InteractorManager::GetPlayerInteractor(player, -1).
    InteractorModule__SimInteractor* sim = PlayerInteractorType(player) == 1 ? PlayerSimInteractor(player) : nullptr;
    InteractorModule__DirectInteractor* direct = PlayerInteractorType(player) == 0 ? PlayerDirectInteractor(player) : nullptr;
    if (sim && otd->m_pos == &sim->m_pos.x) {
      sim->Draw(*v);
      break;
    }
    if (direct && otd->m_pos == &direct->m_pos.x) {
      direct->Draw(*v);
      break;
    }
    if (BuildToolDraw(player, otd, *v)) break;  // PC: the build tools (buildtools.cpp)
    if (PlaceManipulatorDraw(player, otd, *v)) break;  // PC: the buy manipulators (placemanipulator.cpp)
  }
  data.m_curShader = nullptr;
  data.m_curAnim = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curLights = nullptr;
  data.m_fieldB0 = 0;
}

// 80217388
// Before the house draws: the player's interactor puts its draw record in (vtable 0xB4, PreDraw).
void InteractorModule__InteractorVisualizer::SetupDrawCallback(EHouse*, int player) {
  // PC: the build tools (interactors 3, 4, 5) and the buy manipulators (the GC's object manipulators,
  // 2 and 6) are held outside the manager stand-in; their PreDraw is BuildToolPreDraw /
  // PlaceManipulatorPreDraw.
  if (BuildToolPreDraw(player, *this)) return;
  InteractorModule__InteractorInfo info;
  if (PlaceManipulatorInfo(player, info)) {
    PlaceManipulatorPreDraw(player, *this);
    return;
  }
  switch (PlayerInteractorType(player)) {
    case 1:
      if (InteractorModule__SimInteractor* sim = PlayerSimInteractor(player)) sim->PreDraw(*this);
      break;
    case 0:
      if (InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player)) d->PreDraw(*this);
      break;
    default:
      break;
  }
}

// 80217B30
// One of the interactor's models at matrix `m` (scaled to world units), lit in `color` (ambient: colour
// times the plumb bob tuning's 0.3) by a light across the view.
void InteractorModule__InteractorVisualizer::DrawResource(InteractorModule__Interactor& in, unsigned int id, EVec3& color, EMat4* m) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  ERModel* model = set ? set->GetModel(id) : nullptr;
  if (!model) return;
  DrawLitModel(model, color, m);
}

// PC: DrawResource's drawing, for a model the caller holds (the PC build/buy manipulators are not
// InteractorModule::Interactors, so they keep their models themselves).
void InteractorModule__InteractorVisualizer::DrawLitModel(ERModel* model, EVec3& color, EMat4* m) {
  if (!model || !s_pERC) return;
  m_lights.m_numDir = 1;
  m_lights.m_numPoint = 0;
  if (ESimsCam* cam = _globals.GetCam()) {
    const float dx = cam->m_target.x - cam->m_eye.x, dy = cam->m_target.y - cam->m_eye.y, dz = cam->m_target.z - cam->m_eye.z;
    float d[3] = {-dy, dx, dz};
    const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l != 0.0f) d[0] /= l, d[1] /= l, d[2] /= l;
    for (int i = 0; i < 3; ++i) m_lights.m_dir[0].m_dir[i] = d[i];
  }
  const float k = g_plumbBobParms.m_field24;  // DAT_804855C4
  m_lights.m_dir[0].m_color[0] = color.x;
  m_lights.m_dir[0].m_color[1] = color.y;
  m_lights.m_dir[0].m_color[2] = color.z;
  m_lights.m_ambient[0] = color.x * k;
  m_lights.m_ambient[1] = color.y * k;
  m_lights.m_ambient[2] = color.z * k;
  m->PreScale(model->m_scale);
  s_pERC->Lights(&m_lights);
  s_pERC->ModelMatrices(m, 1);
  model->Draw(s_pERC);
}

// 8021728C
RET_UNKNOWN InteractorModule__InteractorVisualizer::CreateResources(InteractorModule__SocialModeInteractor&) { return 1; }
// 80217294
RET_UNKNOWN InteractorModule__InteractorVisualizer::DestroyResources(InteractorModule__SocialModeInteractor&) { return 1; }
// 802172F8
void InteractorModule__InteractorVisualizer::Draw(InteractorModule__SocialModeInteractor&) {}

// ---------------------------------------------------------------------------------------------------
// The build tools: the floor tool (FloorPainter), the wallpaper tool (WallPainter) and the wall tool
// (WallManipulator): their cursor models, and while a selection is open its preview: the floor
// rectangle or room in the pattern, the wall line as an upright strip (and the walls it would delete),
// the wallpaper on the wall sides it would cover or round the room.

namespace {
// InteractorModule::white / red / green (80487E04 / 80487E1C / 80487E10).
EVec3 kWhite{1.0f, 1.0f, 1.0f};
EVec3 kRed{1.0f, 0.0f, 0.0f};
EVec3 kGreen{0.0f, 1.0f, 0.0f};
const float kWallToolHeight = 3.0f;                // WallManipulator::m_wallToolHeight (805006B4)
const float kAffectedWallToolHeight = 3.0f;        // m_affectedWallToolHeight
const float kAffectedWallToolFenceHeight = 3.0f;   // m_affectedWallToolFenceHeight
const float kPaperToolHeight = 3.5f;               // WallPainter::m_paperToolHeight (805006C4)
const float kPaperWallHeight = 3.0f;               // WallPainter::m_wallHeight

// An EGEVert at (x, y, z) with normal n (signed bytes: x, y, z, w), uv and colour 0x80 (1.0 after the
// GC's TEV doubling).
EGEVert Vert(float x, float y, float z, int nx, int ny, int nz, int nw, float u, float v) {
  EGEVert e;
  e.pos[0] = x, e.pos[1] = y, e.pos[2] = z, e.pos[3] = 1.0f;
  e.normal[0] = nx, e.normal[1] = ny, e.normal[2] = nz, e.normal[3] = nw;
  e.uv[0] = u, e.uv[1] = v;
  for (int& c : e.color) c = 0x80;
  return e;
}
// The house's offset (EHouse +0x4C, +0x50).
float HouseX() { return _globals.m_house ? _globals.m_house->m_posY : 0.0f; }
float HouseY() { return _globals.m_house ? _globals.m_house->m_posX : 0.0f; }
bool SameVec(const EVec3& a, const EVec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
}  // namespace

// 800595AC
EVec3& Player::GetInteractorColor() {
  static EVec3 s_interactorColor{1.0f, 1.0f, 3.0f};  // 80485594
  return s_interactorColor;
}

// 803C06D4
// White ambient light and no other.
void InteractorModule__InteractorVisualizer::SetDefaultLights() {
  m_lights.m_numPoint = 0;
  m_lights.m_numDir = 0;
  m_lights.m_ambient[0] = kWhite.x, m_lights.m_ambient[1] = kWhite.y, m_lights.m_ambient[2] = kWhite.z;
  s_pERC->Lights(&m_lights);
}

// 80217550
// The interactor's shader `id` selected for drawing; false without it.
bool InteractorModule__InteractorVisualizer::SelectShader(InteractorModule__Interactor& in, unsigned int id) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  ERShader* shader = set ? set->GetShader(id) : nullptr;
  EShader* s = shader ? shader->GetShader() : nullptr;
  if (!s) return false;
  s->Select(s_pERC, 0);
  return true;
}

// 80213F98
// The cursor's two models (0xF01FADBC, 0x01950A92), the remove tool's shader (0x636C94C0) and a draw
// record of its own (flags 0x800, at the preview position, draw state 1).
bool InteractorModule__InteractorVisualizer::CreateResources(InteractorModule__FloorPainter& in) {
  InteractorModule__InteractorResourceSet* set = CreateResourceSet(in);
  if (!set) return false;
  set->AddModel(0xf01fadbc);
  set->AddModel(0x1950a92);
  set->AddShader(0x636c94c0);
  if (set->GetOrderTableDataCount() == 0) {  // PC: the tool is created again on each start
    EOrderTableData otd;
    otd.m_flags = 0x800;
    otd.m_pos = &in.m_previewPos.x;
    otd.m_callback = InteractorOrderTableCallback;
    otd.m_matrix = &IdentityMatrix();  // GC: 0 (the callback sets its own)
    otd.m_lights = &m_lights;
    set->AddOrderTableData(otd);
  }
  return true;
}
// 802140E0
void InteractorModule__InteractorVisualizer::DestroyResources(InteractorModule__FloorPainter& in) { DestroyResourceSet(in); }
// 80214100
void InteractorModule__InteractorVisualizer::PreDraw(InteractorModule__FloorPainter& in) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  EOrderTableData* otd = set ? set->GetOrderTableData(0) : nullptr;
  if (!otd) return;
  otd->m_pos = &in.m_previewPos.x;  // GC: +0x18 the player
  if (EHouse* house = _globals.m_house) house->InsertHouseObject(otd);
}

// 8021414C
// Not anchored: the cursor's two models at the cursor tile in the interactor colour, and with the
// trigger held (a room, state 2) the room under it in the pattern (the remove shader in remove mode),
// lit white (red while the selection is invalid, 0x20). Anchored: the two models at the rectangle's
// corners, half a tile in and turned to the drag, the far one green, and the rectangle in the pattern.
void InteractorModule__InteractorVisualizer::Draw(InteractorModule__FloorPainter& in) {
  if (in.m_drawState == 0) return;
  EVec3& cursor = in.m_savedPos;  // +0x20
  EVec3& anchor = in.m_anchor;    // +0x64
  if (!(in.m_state & 1)) {
    EMat4 m;
    m.Id();
    EVec3 at{cursor.x, cursor.y, 0.0f};
    m.Translate(at);
    EVec3& color = Player::GetInteractorColor();
    EMat4 m2 = m;
    DrawResource(in, 0xf01fadbc, color, &m);
    DrawResource(in, 0x1950a92, color, &m2);
    if (!(in.m_state & 2)) return;
    m_lights.m_numPoint = 0;
    m_lights.m_numDir = 0;
    const EVec3& l = (in.m_state & 0x20) ? kRed : kWhite;
    m_lights.m_ambient[0] = l.x, m_lights.m_ambient[1] = l.y, m_lights.m_ambient[2] = l.z;
    s_pERC->Lights(&m_lights);
    const void* floor = in.GetSelectedFloorData();
    const unsigned int shader = in.IsInPlaceMode() ? (floor ? ERQuickdata::U32(floor, 0xc) : 0u) : 0x636c94c0u;
    if (!SelectShader(in, shader)) return;
    s_pERC->EnableGeometryModes(8);
    DrawFloorRoomPreview(s_pERC, cursor);
    return;
  }
  const EVec3 inset{0.125f, 0.125f, 0.1f};
  EMat4 first, second;
  first.Id();
  second.Id();
  EVec3 c, a;
  ibuild::SnapPositionToTile(cursor, c, 1);
  ibuild::SnapPositionToTile(anchor, a, 1);
  EVec3 d{c.x - a.x, c.y - a.y, c.z - a.z};
  if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f) {
    const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    d.x /= len, d.y /= len, d.z /= len;
  }
  float turn = 0.0f;
  EVec3 t;
  if (d.x == 0.0f && d.y == 0.0f) {
    t = EVec3{anchor.x - inset.x, anchor.y - inset.y, inset.z};
    first.Translate(t);
    t = EVec3{cursor.x + inset.x, cursor.y + inset.y, inset.z};
    second.Translate(t);
  } else if (d.x >= 0.0f && d.y > 0.0f) {
    t = EVec3{anchor.x - inset.x, anchor.y - inset.y, inset.z};
    first.Translate(t);
    t = EVec3{cursor.x + inset.x, cursor.y + inset.y, inset.z};
    second.Translate(t);
  } else if (d.x < 0.0f && d.y >= 0.0f) {
    t = EVec3{anchor.x + inset.x, anchor.y - inset.y, inset.z};
    first.Translate(t);
    t = EVec3{cursor.x - inset.x, cursor.y + inset.y, inset.z};
    second.Translate(t);
    turn = 1.5707964f;
  } else if (d.x <= 0.0f && d.y < 0.0f) {
    t = EVec3{anchor.x + inset.x, anchor.y + inset.y, inset.z};
    first.Translate(t);
    t = EVec3{cursor.x - inset.x, cursor.y - inset.y, inset.z};
    second.Translate(t);
    turn = 3.1415927f;
  } else if (d.x > 0.0f && d.y <= 0.0f) {
    t = EVec3{anchor.x - inset.x, anchor.y + inset.y, inset.z};
    first.Translate(t);
    t = EVec3{cursor.x + inset.x, cursor.y - inset.y, inset.z};
    second.Translate(t);
    turn = 4.712389f;
  }
  if (turn != 0.0f) {
    first.PreRotateZ(turn);
    second.PreRotateZ(turn);
  }
  DrawResource(in, 0xf01fadbc, Player::GetInteractorColor(), &first);
  DrawResource(in, 0x1950a92, kGreen, &second);
  m_lights.m_numPoint = 0;
  m_lights.m_numDir = 0;
  const EVec3& l = (in.m_state & 0x20) ? kRed : kWhite;
  m_lights.m_ambient[0] = l.x, m_lights.m_ambient[1] = l.y, m_lights.m_ambient[2] = l.z;
  s_pERC->Lights(&m_lights);
  if (in.m_state & 4) {
    const void* floor = in.GetSelectedFloorData();
    if (!SelectShader(in, floor ? ERQuickdata::U32(floor, 0xc) : 0u)) return;
  } else if (in.m_state & 8) {
    if (!SelectShader(in, 0x636c94c0)) return;
  }
  DrawFloorRectPreview(s_pERC, anchor, cursor);
}

// 802147E4
// The tiles from the anchor's to the cursor's as one quad just above the floor (0.05), the pattern
// repeating once a tile. (GC: the anchor less the house's offset, the cursor taken as it is.)
bool InteractorModule__InteractorVisualizer::DrawFloorRectPreview(ERC* rc, EVec3& anchor, EVec3& cursor) {
  const float hx = HouseX(), hy = HouseY();
  const int ax = static_cast<int>(anchor.x - hx), ay = static_cast<int>(anchor.y - hy);
  const int cx = static_cast<int>(cursor.x), cy = static_cast<int>(cursor.y);
  const float x0 = (static_cast<float>(std::min(ax, cx)) - 0.5f) + hx, x1 = static_cast<float>(std::max(ax, cx)) + 0.5f + hx;
  const float y0 = (static_cast<float>(std::min(ay, cy)) - 0.5f) + hy, y1 = static_cast<float>(std::max(ay, cy)) + 0.5f + hy;
  rc->ModelMatrixId();
  const float w = x1 - x0 <= 1.0f ? 1.0f : x1 - x0;
  const float h = y1 - y0 <= 1.0f ? 1.0f : y1 - y0;
  EGEVert v[4] = {Vert(x1, y0, 0.05f, 0, 0, 0x7f, 0xff, w, 0.0f), Vert(x0, y0, 0.05f, 0, 0, 0x7f, 0xff, 0.0f, 0.0f),
                  Vert(x1, y1, 0.05f, 0, 0, 0x7f, 0xff, w, h), Vert(x0, y1, 0.05f, 0, 0, 0x7f, 0xff, 0.0f, h)};
  rc->TriStrip(v, 4);
  return true;
}

// 80214B04
// Each tile of the room under the cursor (a diagonal tile: its first room) as a quad just above the
// floor.
int InteractorModule__InteractorVisualizer::DrawFloorRoomPreview(ERC* rc, EVec3& cursor) {
  CTilePt tile(static_cast<int>(cursor.y), static_cast<int>(cursor.x), 1);
  cFixedWorldImpl* world = Globs_pFixedWorld;
  if (!world) return 0;
  unsigned int id = world->GetRoom(tile);
  if (id == 0xfffb && Globs_pRoomManager) {
    unsigned short r1 = 0, r2 = 0;
    Room__Sides s1, s2;
    Globs_pRoomManager->ResolveDiagonal(tile, &r1, &r2, &s1, &s2);
    id = r1;
  }
  Room* room = Globs_pRoomManager ? Globs_pRoomManager->GetRoom(static_cast<unsigned short>(id)) : nullptr;
  if (!room || id == 0 || room->m_tiles.empty()) return 0;
  rc->SetMipMap(0.2f, 0);
  rc->ModelMatrixId();
  const float hx = HouseX(), hy = HouseY();
  for (CTilePt& t : room->m_tiles) {
    const float ty = static_cast<float>(t.GetY()), tx = static_cast<float>(t.GetX());
    const float x0 = (ty - 0.5f) + hx, x1 = (ty + 0.5f) + hx;
    const float y0 = (tx - 0.5f) + hy, y1 = (tx + 0.5f) + hy;
    EGEVert v[4] = {Vert(x1, y0, 0.05f, 0, 0, 0x7f, 0xff, 1.0f, 0.0f), Vert(x0, y0, 0.05f, 0, 0, 0x7f, 0xff, 0.0f, 0.0f),
                    Vert(x1, y1, 0.05f, 0, 0, 0x7f, 0xff, 1.0f, 1.0f), Vert(x0, y1, 0.05f, 0, 0, 0x7f, 0xff, 0.0f, 1.0f)};
    rc->TriStrip(v, 4);
  }
  return 1;
}

// 80214E80
// The anchor and cursor models (0xD5D17E48, 0xC167C449, the brush 0x6591A8D0) and the sell, valid and
// invalid strip shaders (0x84853612, 0x3B494D6C, 0xEA1905EB).
bool InteractorModule__InteractorVisualizer::CreateResources(InteractorModule__WallPainter& in) {
  InteractorModule__InteractorResourceSet* set = CreateResourceSet(in);
  if (!set) return false;
  set->AddModel(0xd5d17e48);
  set->AddModel(0xc167c449);
  set->AddModel(0x6591a8d0);
  set->AddShader(0x84853612);
  set->AddShader(0x3b494d6c);
  set->AddShader(0xea1905eb);
  return true;
}
// 80214F18
void InteractorModule__InteractorVisualizer::DestroyResources(InteractorModule__WallPainter& in) { DestroyResourceSet(in); }
// 80214F38
// The player's draw record at the tool's position.
void InteractorModule__InteractorVisualizer::PreDraw(InteractorModule__WallPainter& in) {
  EOrderTableData* otd = m_otd[in.m_player & 1];
  if (!otd) return;
  otd->m_pos = &in.m_pos.x;  // GC: +0x18 the player
  if (EHouse* house = _globals.m_house) house->InsertHouseObject(otd);
}

// 80214F94
// The anchor model at the anchor's draw position. At the cursor: unanchored, the cursor model; dragging,
// the brush turned along the drag, green over the wallpaper preview on the sides it would cover, red
// (with the red strip along the drag) while invalid; a room (0x10): the room's preview and the cursor
// model green or red.
void InteractorModule__InteractorVisualizer::Draw(InteractorModule__WallPainter& in) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  if (!set) return;
  EMat4 m;
  m.Id();
  m.Translate(in.m_anchorDraw);
  DrawResource(in, 0xd5d17e48, Player::GetInteractorColor(), &m);
  m.Id();
  m.Translate(in.m_savedPos);
  if (!(in.m_flags & 0x10)) {
    if (!(in.m_flags & 1)) {
      DrawResource(in, 0xc167c449, Player::GetInteractorColor(), &m);
      return;
    }
    const bool moved = !SameVec(in.m_savedPos, in.m_anchor);
    if (moved) {
      const float a = std::atan2(in.m_savedPos.y - in.m_anchorDraw.y, in.m_savedPos.x - in.m_anchorDraw.x);
      m.PreRotateZ(a + in.m_cursorRotation);
    }
    EVec3* color;
    if (!(in.m_flags & 0x80)) {
      if (ERShader* strip = set->GetShader(0xea1905eb)) DrawVerticalRect(strip, in.m_anchor, in.m_savedPos, kPaperToolHeight, 0, 0.0f);
      color = &kRed;
    } else {
      if (moved) DrawWallpaperPreviewOnAffectedWalls(in);
      color = &kGreen;
    }
    DrawResource(in, 0x6591a8d0, *color, &m);
    return;
  }
  DrawWallpaperRoomPreview(in);
  EVec3 color = (in.m_flags & 0x80) ? kGreen : kRed;
  DrawResource(in, 0xc167c449, color, &m);
}

// 80215218
// The paper on every wall run of the room the anchor is in (ERoom::DrawWallpaperPreview), on the
// cursor's side, lit white.
void InteractorModule__InteractorVisualizer::DrawWallpaperRoomPreview(InteractorModule__WallPainter& in) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  if (!set) return;
  ERoom* room = _globals.m_house ? _globals.m_house->m_room : nullptr;
  if (!room) return;
  ERShader* shader = set->GetShader(in.GetShaderID());
  EShader* s = shader ? shader->GetShader() : nullptr;
  if (!s) return;
  s->Select(s_pERC, 0);
  s_pERC->EnableGeometryModes(8);
  SetDefaultLights();
  EVec3 at{in.m_anchor.y, in.m_anchor.x, in.m_anchor.z};
  CTilePt tile(at);
  const unsigned short id = InteractorGetRoomIdFromPoint(tile);
  EVec2 cursor{in.m_savedPos.x, in.m_savedPos.y};
  room->DrawWallpaperPreview(s_pERC, id, cursor);
}

// 802153F4
// The paper (the sell shader with the sell tool, 8) on the wall sides along the drag, on the side the
// camera sees; then the strip along the drag in the valid shader, or the invalid one when no side was
// covered. Returns the sides covered.
int InteractorModule__InteractorVisualizer::DrawWallpaperPreviewOnAffectedWalls(InteractorModule__WallPainter& in) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  if (!set) return 0;
  ERShader* shader = set->GetShader((in.m_flags & 8) ? 0x84853612u : in.GetShaderID());
  EShader* s = shader ? shader->GetShader() : nullptr;
  if (!s) return 0;
  s->Select(s_pERC, 0);
  EVec3 a = in.m_anchor, b = in.m_savedPos, side;
  ibuild::GetVisibleSideOfWall(_gd.m_players[in.m_player & 1].m_cam, a, b, &side);
  a.x += side.x, a.y += side.y, a.z += side.z;
  b.x += side.x, b.y += side.y, b.z += side.z;
  const int n = DrawWallpaperRectPreview(shader, a, b, kPaperWallHeight);
  ERShader* strip = set->GetShader(n == 0 ? 0xea1905ebu : 0x3b494d6cu);
  DrawVerticalRect(strip, in.m_anchor, in.m_savedPos, kPaperToolHeight, 0, 0.0f);
  return n;
}

// 802155E8
// The anchor, cursor and line-end models (0x05072619, 0xC29189E2, 0x153D4C3A), the strip shaders
// (valid walls, add, sell, delete, invalid) and a draw record of its own.
bool InteractorModule__InteractorVisualizer::CreateResources(InteractorModule__WallManipulator& in) {
  InteractorModule__InteractorResourceSet* set = CreateResourceSet(in);
  if (!set) return false;
  set->AddModel(0x5072619);
  set->AddModel(0xc29189e2);
  set->AddModel(0x153d4c3a);
  set->AddShader(0x899ba3eb);
  set->AddShader(0x3b494d6c);
  set->AddShader(0x84853612);
  set->AddShader(0x71ba2e9a);
  set->AddShader(0x7604c163);
  if (set->GetOrderTableDataCount() == 0) {  // PC: the tool is created again on each start
    EOrderTableData otd;
    otd.m_flags = 0x800;
    otd.m_pos = &in.m_previewPos.x;
    otd.m_callback = InteractorOrderTableCallback;
    otd.m_matrix = &IdentityMatrix();
    otd.m_lights = &m_lights;
    set->AddOrderTableData(otd);
  }
  return true;
}

// 8021577C
// One quad up each wall side (a wall, not a fence) the line from `from` to `to` passes, `height` high,
// the paper once across. Returns how many.
int InteractorModule__InteractorVisualizer::DrawWallpaperRectPreview(ERShader*, EVec3& from, EVec3& to, float height) {
  CTilePt t0, t1;
  EVec3 a{from.x, from.y, 0.0f}, b{to.x, to.y, 0.0f};
  ibuild::ConvertVertsToTiles(a, b, t0, t1);
  const int dir = CTilePt::GetTileDirection(t0, t1);
  if (dir == 8) return 0;
  const int seg = static_cast<int>(TileWalls::DirToWallSeg(TilePtDir{dir}));
  cFixedWorldImpl* world = Globs_pFixedWorld;
  if (!world) return 0;
  int count = 0;
  EVec3 p0{from.x, from.y, 0.0f}, p1 = p0;
  ibuild::GetAdjacentTileCoordinates(dir, 1, p1);
  CTilePt tile(t0);
  CTilePt next = tile + CTilePt::sDirections[dir];
  SetDefaultLights();
  while (!world->OutOfBounds(tile) && tile != t1) {
    TileWalls walls = world->GetWall(tile);
    if (walls.HasWallNotFence(TileWallsSegment{seg})) {
      ++count;
      EGEVert v[4] = {Vert(p0.x, p0.y, height, 0, 0x7f, 0, 0xff, 1.0f, 1.0f), Vert(p1.x, p1.y, height, 0, 0x7f, 0, 0xff, 1.0f, 0.0f),
                      Vert(p0.x, p0.y, 0.05f, 0, 0x7f, 0, 0xff, 0.0f, 1.0f), Vert(p1.x, p1.y, 0.05f, 0, 0x7f, 0, 0xff, 0.0f, 0.0f)};
      s_pERC->ZTest(true, 2, 0, 0);
      s_pERC->ModelMatrixId();
      s_pERC->TriStrip(v, 4);
    }
    tile = next;
    next += CTilePt::sDirections[dir];
    p0 = p1;
    ibuild::GetAdjacentTileCoordinates(dir, 1, p1);
  }
  return count;
}

// 80215C14
int InteractorModule__InteractorVisualizer::DestroyResources(InteractorModule__WallManipulator& in) { return DestroyResourceSet(in); }
// 80215C34
void InteractorModule__InteractorVisualizer::PreDraw(InteractorModule__WallManipulator& in) {
  EOrderTableData* otd = m_otd[in.m_player & 1];
  if (!otd) return;
  otd->m_pos = &in.m_anchorDraw.x;  // GC: +0x18 the player
  if (EHouse* house = _globals.m_house) house->InsertHouseObject(otd);
}

// 80215C90
// The anchor model at the anchor's draw position (turned along the drag while anchored or a room); at the
// cursor its model, or while dragging the line-end model green (with the walls the line would delete
// when deleting, 8) or red, and the line's strip (a room: the four sides).
void InteractorModule__InteractorVisualizer::Draw(InteractorModule__WallManipulator& in) {
  EMat4 m;
  m.Id();
  m.Translate(in.m_anchorDraw);
  float a = 0.0f;
  const bool dragging = (in.m_flags & 1) || (in.m_flags & 0x10);
  if (dragging) {
    a = std::atan2(in.m_savedPos.y - in.m_anchorDraw.y, in.m_savedPos.x - in.m_anchorDraw.x);
    if (a != 0.0f) m.PreRotateZ(a);
  }
  DrawResource(in, 0x5072619, Player::GetInteractorColor(), &m);
  m.Id();
  m.Translate(in.m_savedPos);
  if (!dragging) {
    DrawResource(in, 0xc29189e2, Player::GetInteractorColor(), &m);
    return;
  }
  if (a != 0.0f) m.PreRotateZ(a);
  if (!(in.m_flags & 0x80)) {
    DrawResource(in, 0x153d4c3a, kRed, &m);
  } else {
    DrawResource(in, 0x153d4c3a, kGreen, &m);
    if (in.m_flags & 8) DrawPreviewOnAffectedWalls(in);
  }
  if (!SameVec(in.m_savedPos, in.m_anchor)) {
    if (!(in.m_flags & 0x10)) DrawWallRectPreview(in);
    else DrawRoomPreview(in);
  }
}

// 80215F08
// The line's strip from the anchor to the cursor in the wall tool height: invalid, walls, deleting,
// or fences shader.
void InteractorModule__InteractorVisualizer::DrawWallRectPreview(InteractorModule__WallManipulator& in) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  if (!set) return;
  const unsigned int f = in.m_flags;
  const unsigned int id = !(f & 0x80) ? 0x7604c163u : !(f & 4) ? 0x3b494d6cu : !(f & 0x100) ? 0x899ba3ebu : 0x9b0c648du;
  if (ERShader* shader = set->GetShader(id)) DrawVerticalRect(shader, in.m_anchor, in.m_savedPos, kWallToolHeight, in.GetTexGenMode(), 1.0f);
}

// 80215FDC
// An upright strip from `from` to `to` (a quad per tile), `height` high, in `shader` lit white. Texture
// coordinates: mode 2 a tile across each quad, stepping `uvStep` along; mode 1 the same turned; else
// the texture's centre. Sent as arrays (positions x, y, z, 0; uvs; normals 0, 0x7f, 0, 0x7f; no
// colours) allocated from the context, through ERC::TriStrip(int, float*, ...).
void InteractorModule__InteractorVisualizer::DrawVerticalRect(ERShader* shader, EVec3& from, EVec3& to, float height, int texGen, float uvStep) {
  EShader* s = shader ? shader->GetShader() : nullptr;
  if (!s) return;
  s->Select(s_pERC, 0);
  s_pERC->ZTest(true, 2, 1, 0);
  ELights lights;
  lights.m_ambient[0] = lights.m_ambient[1] = lights.m_ambient[2] = 1.0f;
  s_pERC->Lights(&lights);  // PC: the renderer copies the set (GC: from the stack too)
  s_pERC->EnableGeometryModes(8);
  s_pERC->ModelMatrixId();
  const float dx = std::fabs(to.x - from.x), dy = std::fabs(to.y - from.y);
  int n = static_cast<int>(dy + 0.5f);
  if (n < static_cast<int>(dx + 0.5f)) n = static_cast<int>(dx + 0.5f);
  if (n == 0) return;
  const unsigned int pairs = static_cast<unsigned int>(n) + 1;
  // One block: positions (8 floats a pair), uvs (4 floats a pair), normals (8 bytes a pair).
  auto* pos = static_cast<float*>(s_pERC->Alloc(pairs * 0x38, 0x20));
  if (!pos) return;
  float* uvs = pos + pairs * 8;
  auto* normals = reinterpret_cast<signed char*>(uvs + pairs * 4);
  const float inv = 1.0f / static_cast<float>(n);
  const float sx = (to.x - from.x) * inv, sy = (to.y - from.y) * inv;
  float u, v, du, dv, su, sv;  // the pair's uv, the top's offset, the step
  if (texGen == 2) {
    u = 0.0f, v = static_cast<float>(-n / 2), du = 1.0f, dv = 0.0f, su = 0.0f, sv = uvStep;
  } else if (texGen == 1) {
    u = static_cast<float>(-n / 2), v = 0.0f, du = 0.0f, dv = 1.0f, su = uvStep, sv = 0.0f;
  } else {
    u = 0.5f, v = 0.5f, du = dv = su = sv = 0.0f;
  }
  float x = from.x, y = from.y;
  float* p = pos;
  float* t = uvs;
  signed char* nm = normals;
  for (unsigned int i = 0; i < pairs; ++i) {
    p[0] = x, p[1] = y, p[2] = 0.0f, p[3] = 0.0f;
    p[4] = x, p[5] = y, p[6] = height, p[7] = 0.0f;
    t[0] = u, t[1] = v, t[2] = u + du, t[3] = v + dv;
    for (int k = 0; k < 2; ++k) nm[k * 4] = 0, nm[k * 4 + 1] = 0x7f, nm[k * 4 + 2] = 0, nm[k * 4 + 3] = 0x7f;
    p += 8, t += 4, nm += 8;
    x += sx, y += sy;
    u += su, v += sv;
  }
  s_pERC->TriStrip(static_cast<int>(pairs * 2), pos, uvs, nullptr, normals, nullptr, false);
}

// 80216488
// The room rectangle's four sides as strips (anchor -> (anchor x, cursor y) -> cursor -> (cursor x,
// anchor y) -> anchor), valid or invalid shader.
void InteractorModule__InteractorVisualizer::DrawRoomPreview(InteractorModule__WallManipulator& in) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  if (!set) return;
  ERShader* shader = set->GetShader((in.m_flags & 0x80) ? 0x899ba3ebu : 0x7604c163u);
  if (!shader) return;
  const EVec3 a = in.m_anchor, c = in.m_savedPos;
  EVec3 p{a.x, a.y, a.z}, q{a.x, c.y, 0.0f};
  DrawVerticalRect(shader, p, q, kWallToolHeight, in.GetTexGenMode(), 1.0f);
  p = q;
  q = EVec3{c.x, c.y, c.z};
  DrawVerticalRect(shader, p, q, kWallToolHeight, in.GetTexGenMode(), 1.0f);
  p = q;
  q = EVec3{c.x, a.y, q.z};
  DrawVerticalRect(shader, p, q, kWallToolHeight, in.GetTexGenMode(), 1.0f);
  p = q;
  q = EVec3{a.x, a.y, a.z};
  DrawVerticalRect(shader, p, q, kWallToolHeight, in.GetTexGenMode(), 1.0f);
}

// 802166D0
// Deleting: a quad (the sell shader) over each wall the line would remove (with the sell tool any wall,
// fences: fences only, else walls only), on the side the camera sees. Returns how many.
int InteractorModule__InteractorVisualizer::DrawPreviewOnAffectedWalls(InteractorModule__WallManipulator& in) {
  InteractorModule__InteractorResourceSet* set = GetInteractorResourceSet(&in);
  ERShader* shader = set ? set->GetShader(0x84853612) : nullptr;
  EShader* s = shader ? shader->GetShader() : nullptr;
  cFixedWorldImpl* world = Globs_pFixedWorld;
  if (!s || !world) return 0;
  SetDefaultLights();
  EVec3 a = in.m_anchor, b = in.m_savedPos, side;
  ibuild::GetVisibleSideOfWall(_gd.m_players[in.m_player & 1].m_cam, a, b, &side);
  a.x += side.x, a.y += side.y, a.z += side.z;
  b.x += side.x, b.y += side.y, b.z += side.z;
  CTilePt t0, t1;
  ibuild::ConvertVertsToTiles(a, b, t0, t1);
  const int dir = CTilePt::GetTileDirection(t0, t1);
  if (dir == 8) return 0;
  const TileWallsSegment seg{static_cast<int>(TileWalls::DirToWallSeg(TilePtDir{dir}))};
  EVec3 p0 = a, p1 = a;
  ibuild::GetAdjacentTileCoordinates(dir, 1, p1);
  CTilePt tile(t0);
  CTilePt next = tile + CTilePt::sDirections[dir];
  s->Select(s_pERC, 0);
  const float h = (in.m_flags & 0x100) ? kAffectedWallToolFenceHeight : kAffectedWallToolHeight;
  int count = 0;
  while (!world->OutOfBounds(tile) && tile != t1) {
    TileWalls walls = world->GetWall(tile);
    bool has;
    if (in.m_flags & 0x200) has = walls.HasWall(seg);
    else if (in.m_flags & 0x100) has = walls.HasFenceNotWall(seg) != 0;
    else has = walls.HasWallNotFence(seg) != 0;
    if (has) {
      ++count;
      EGEVert v[4] = {Vert(p0.x, p0.y, h, 0, 0x7f, 0, 0, 1.0f, 1.0f), Vert(p1.x, p1.y, h, 0, 0x7f, 0, 0, 1.0f, 0.0f),
                      Vert(p0.x, p0.y, 0.05f, 0, 0x7f, 0, 0, 0.0f, 1.0f), Vert(p1.x, p1.y, 0.05f, 0, 0x7f, 0, 0, 0.0f, 0.0f)};
      s_pERC->ZTest(true, 2, 0, 0);
      s_pERC->ModelMatrixId();
      s_pERC->TriStrip(v, 4);
    }
    tile = next;
    next += CTilePt::sDirections[dir];
    p0 = p1;
    ibuild::GetAdjacentTileCoordinates(dir, 1, p1);
  }
  return count;
}
