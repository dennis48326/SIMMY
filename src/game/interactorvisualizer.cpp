// Port of game/interactorvisualizer.cpp (the classic, direct and social-mode interactors' parts, the draw
// callback and the resource sets' bookkeeping) and game/interactorresourceset.cpp (models, shaders and
// draw records; textures are not used by the ported interactors), plus EHouse::InsertHouseObject /
// ERLevel::InsertHouseObject. See siminteractor.hpp.
// Not ported here: the build tools' and the object manipulator's previews (the wall/floor/wallpaper
// rectangles, the placement object's grid and cursor; the PC tools draw nothing of their own).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/siminteractor.hpp"
#include "simmy/classes/placement.hpp"

#include <algorithm>
#include <cmath>

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
    if (PlaceManipulatorDraw(player, otd, *v)) break;  // PC: the build/buy manipulators (placemanipulator.cpp)
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
  // PC: the build/buy manipulators (the GC's object manipulators, interactors 2 and 6) are stand-ins
  // outside the manager; their PreDraw is PlaceManipulatorPreDraw. (The build tools draw nothing yet.)
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
