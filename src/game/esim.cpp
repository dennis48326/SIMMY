// Port of game/esim.cpp (partial): a sim's 3D instance in the house. Ported: construction, the model
// set-up (initModel), the per-frame update, animation, lighting and drawing of the body. Not ported:
// the model cache, outfit changes, the plumb bob and skill meter (models 0x6d1f0956, 0xc1a394fd,
// 0x528d93c8, 0x68de8ae2, 0x3887fccd), the shadow (ESimShadow), carried props, ghosts, the censor blur,
// pets, the cursor highlight and the depth-of-field foreground list.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/siminteractor.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

// The players' sims' models kept between lots (ESim::m_pP1Model / m_pP2Model; guarded by
// g_playerCacheMutex). PC: the cache is not filled yet (ESim::initModel's model cache, not ported), so
// these stay null and DeletePlayerCachedModel has nothing to free.
SimModel* ESim_m_pP1Model = nullptr;
SimModel* ESim_m_pP2Model = nullptr;

// 800312C8
// The cached player models go (before the ending movie and Create-a-Sim). GC: under g_playerCacheMutex.
void ESim::DeletePlayerCachedModel() {
  delete ESim_m_pP1Model;
  ESim_m_pP1Model = nullptr;
  delete ESim_m_pP2Model;
  ESim_m_pP2Model = nullptr;
}

// 800350F0 ESim::GetTypeInfo
// 800350FC ESim::GetTypeName
// 80035108 ESim::GetTypeKey
// 80035114 ESim::GetTypeVersion
// 800350B0 ESim::SafeDelete
// 80035018 ESim::New
// 8003506C ESim::Construct
// 8003508C ESim::Destruct
// 80035120 ESim::GetTypeInfoStatic
// 8003512C ESim::GetReadVersion
SIMMY_STORABLE_IMPL(ESim, "ESim", 0, ISimInstance)

namespace {
// 804754F4 bigPosVal: the censor record's position, as far as can be, so the order table draws it
// first (after the opaque scene, the sim's body included, before anything blended).
float s_bigPosVal[3] = {1.0e10f, 1.0e10f, 1.0e10f};

// The censor record (GC: set by the constructor and again by initModel).
void InitCensorOtd(ESim* sim, EOrderTableData& otd) {
  otd.m_flags = 0;
  otd.m_sortKey = -1;
  otd.m_pos = s_bigPosVal;
  otd.m_matrix = nullptr;
  otd.m_callback = ESim::CensorOrderTableCallback;
  otd.m_instance = sim;
}

// The sim skeletons (characters) by gender, and the standing idle initModel starts.
const unsigned int kMaleSkeleton = 0xffa60350u;
const unsigned int kFemaleSkeleton = 0x1fb80af4u;
const unsigned int kStandIdle = 0xc00a15d1u;
}  // namespace

// 800351AC
ESim::ESim() { m_simModel = new SimModel(); }

// 80030B9C
// The person's instance: its order-table records (the body, props, the censor blur, the skill meter),
// a SimModel for the body, updated each frame (0x100), the controller in the model's units (1/4096).
// GC: also the 3D head (ESims3DHead), the shadow instance, and the model built by the data manager
// (ESimsDataManager::QueueCommand(this, 0) -> tProcessCommand -> initModel). PC: initModel runs on the
// first Update, on the game thread (it composites the skin).
ESim::ESim(cXPerson* person) {
  m_otd.m_instance = this;
  m_otd.m_pos = m_sphere;
  m_otd.m_callback = SimOrderTableCallback;
  m_meterOtd.m_instance = this;
  m_meterOtd.m_pos = m_sphere;
  m_meterOtd.m_callback = SkillMeterOrderTableCallback;
  m_propOtd.m_instance = this;
  m_propOtd.m_pos = m_sphere;
  m_propOtd.m_callback = PropOrderTableCallback;
  InitCensorOtd(this, m_censorOtd);
  m_person = person;
  m_xob = person ? person->AsObject() : nullptr;
  m_simModel = new SimModel();
  m_flags = (m_flags & ~0x200u) | 0x100u;
  m_anim.m_scale = 1.0f / 4096.0f;
  m_head = new ESims3DHead(this);  // GC: 0x3B8 bytes, 16-aligned, zeroed, from the main heap
  if (!m_shadow) {
    // The shadow: a low body (model 0x6ef2f2da) on the sim's skeleton, posed by the sim's controller,
    // 0.1 above the sim. PC: pets' own shadow models (IsDog/IsCat/IsMonkey: the service NPC record's
    // model) are not ported; there are no pets in this build's houses.
    EMat4 orient;
    GetOrient(orient);
    EVec3 up{0.0f, 0.0f, 0.1f};
    orient.PostTranslate(up);
    auto* shadow = new ESimShadow(m_person);  // GC: 0x328 bytes, 16-aligned, from the main heap
    m_shadow = shadow;
    shadow->SetModel(0x6ef2f2da, false, this, &m_anim);
    shadow->SetOrient(orient);
    shadow->m_field0C = 1;
    shadow->Init();
  }
}

// 800310EC
// GC: also hands the model to the cache for the next house (TransferModelToCache) and frees the
// plumb bob models, the head and the shadow.
ESim::~ESim() {
  if (m_shadow) {
    if (m_shadow->m_level) m_shadow->m_level->RemoveInstance(m_shadow);
    delete m_shadow;
    m_shadow = nullptr;
  }
  for (ERModel*& m : m_hlModels)  // PC: released here (GC: the instance's cleanup)
    if (m) {
      m->DelRef();
      m = nullptr;
    }
  if (m_curtainModel) {
    m_curtainModel->DelRef();
    m_curtainModel = nullptr;
  }
  delete m_head;
  m_head = nullptr;
  if (m_curtainModel) m_curtainModel->DelRef();
  m_curtainModel = nullptr;
  delete m_simModel;
  m_simModel = nullptr;
}

// 80031790
// The person's (cXPerson vtable 0x204, cXPersonImpl::IsMale: its description, a service NPC by its
// record).
int ESim::IsMale() {
  if (auto* impl = dynamic_cast<cXPersonImpl*>(m_person)) return impl->IsMale();
  CasSimDescriptionS2C* desc = m_person ? m_person->GetSimDescription() : nullptr;
  return desc && desc->m_male ? 1 : 0;
}
// 800317C8
int ESim::IsFemale() { return IsMale() ? 0 : 1; }
// 800317EC
int ESim::IsAdult() { return m_person ? static_cast<int>(static_cast<cXPersonImpl*>(m_person)->IsAdult()) : 1; }

// 8003193C
// The skeleton of the sim's gender with the standing idle; then the body: the parts of its description
// (low resolution), the skin composited under the sim's name ("first_last"; NPCs with precomposited
// skins "<guid>_npc"), welded. GC: first waits for the person to be ready, takes the family's
// information, the "sim_<guid>" dataset, a cached model when there is one, and the plumb bob models;
// pets (the service NPC's model) are not ported. Drawn unless the house is loaded from a save
// (DAT_80475C74); PC: always.
void ESim::initModel() {
  m_field3D4 = 0;
  const bool male = IsMale() != 0;
  m_anim.Init(male ? kMaleSkeleton : kFemaleSkeleton, eAnimatedObjectType{0}, 0);
  m_anim.SetTrackAnim(eTrackFlags{1}, kStandIdle, -1, -1.0f, nullptr);
  m_anim.m_speed = 1.0f;
  SIMMY_STUB("ESim::initModel: the family's information, the sim_<guid> dataset, the model cache", 0x8003193C);
  if (!m_curtainModel) m_curtainModel = static_cast<ERModel*>(_modelman.AddRef(0x3887fccdu, nullptr, 0));  // +0x42C
  CasSimDescriptionS2C* desc = m_person ? m_person->GetSimDescription() : nullptr;
  if (desc && !m_person->GetServiceNPC()) {
    char first[32], last[32];
    for (int i = 0; i < 32; ++i) {
      first[i] = static_cast<char>(desc->m_firstName[i]);
      last[i] = static_cast<char>(desc->m_lastName[i]);
    }
    first[31] = last[31] = 0;
    m_changingOutfit = 0;
    m_freeMorphs = 1;
    m_simModel->Init(desc, false);
    char name[80];
    if (!desc->m_precomposited) {
      std::snprintf(name, sizeof name, "%s_%s", first, last);
      for (char* c = name; *c; ++c)  // EString::MakeLegalSymbolName
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '_')) *c = '_';
    } else {
      std::snprintf(name, sizeof name, "%08x_npc", 0u);
    }
    m_simModel->CreateSkin(name);
    m_simModel->SetAllModels();
    m_simModel->VerifyRequiredModels();
    m_simModel->ApplyAllLatticeMorphing();
    m_simModel->ApplyAllMorphTargets();
    m_simModel->UpdateReweld(true);
    CompositeSkin();
    m_simModel->ApplySkinToModels();
    m_otd.m_instance = this;
    m_otd.m_pos = m_sphere;
    m_otd.m_callback = SimOrderTableCallback;
    m_propOtd.m_pos = m_sphere;
    InitCensorOtd(this, m_censorOtd);
  }
  m_plumbBobState = 0;
  m_skillValue = 0.0f;
  // The cursor highlight's models and the changing curtain.
  for (ERModel*& m : m_hlModels)
    if (m) {
      m->DelRef();
      m = nullptr;
    }
  m_hlModels[0] = static_cast<ERModel*>(_modelman.AddRef(0xc1a394fdu, nullptr, 0));
  m_hlModels[1] = static_cast<ERModel*>(_modelman.AddRef(0x528d93c8u, nullptr, 0));
  m_hlModels[2] = static_cast<ERModel*>(_modelman.AddRef(0x68de8ae2u, nullptr, 0));
  m_changingOutfit = 0;
  if (m_curtainModel) m_curtainModel->DelRef();
  m_curtainModel = static_cast<ERModel*>(_modelman.AddRef(0x3887fccdu, nullptr, 0));
  m_firstDraw = 1;
  DrawSim(true);
  m_modelReady = 1;
}

// 8003461C
// The skin's quadrants composited (GC: the person's data 0x3C takes the skin colour first).
void ESim::CompositeSkin() { m_simModel->CompositeAllSkin(); }

namespace {
ESim* s_changingSim = nullptr;  // 80475538 the sim whose outfit the data manager is making
}  // namespace

// 80034318
void ESim::UpdateShowerCurtain() {
  m_curtainWanted = m_skinChange != -1 ? 1 : 0;
  if (cXPersonImpl* p = m_person ? static_cast<cXPersonImpl*>(m_person) : nullptr)
    if (p->GetPersonData(0x1e) & 0x80) m_curtainWanted = 1;
  const float step = Globs_pSimulator ? static_cast<float>(_dt * Globs_pSimulator->GetSpeedMultiplier()) : _dt;
  if (!m_curtainWanted) {
    if (!m_curtain) return;
    m_curtainAlpha -= step + step;
    if (m_curtainAlpha > 0.0f) return;
    m_curtain = 0;
    m_curtainAlpha = 0.0f;
    return;
  }
  if (!m_curtain) {
    m_curtain = 1;
    m_curtainAlpha = 0.0f;
    return;
  }
  m_curtainAlpha += step + step;
  if (m_curtainAlpha < 1.0f) return;
  m_curtainAlpha = 1.0f;
}

// 80034464
void ESim::UpdateSkinChange() {
  switch (m_skinChange) {
    case -1: return;
    case 0:
      if (s_changingSim) return;
      if ((!m_curtain || m_curtainAlpha < 1.0f) && !m_field424) return;
      if (m_person && m_person->GetServiceNPC()) break;
      s_changingSim = this;
      m_skinChange = 1;
      SIMMY_STUB("ESim::UpdateSkinChange: flushing queued data-manager commands and costume models", 0x80034464);
      m_changingSkin = 1;
      // GC: ESimsDataManager::QueueCommand(this, 1) -> tProcessCommand(1) on its thread; PC: at once.
      tProcessCommand(1);
      return;
    case 1:
      if (HasQueuedOperation()) return;
      s_changingSim = nullptr;
      m_skinChange = 2;
      return;
    case 2:
      if (m_curtain && !m_field424 && !m_curtainWanted) return;
      break;
    default: return;
  }
  m_changingOutfit = 0;
  m_skinChange = -1;
  m_field424 = 0;
}

// 800345C4
int ESim::CreateSkinAsync() {
  if (HasQueuedOperation() || m_skinChange != -1) return 0;
  m_skinChange = 0;
  m_changingOutfit = 1;
  return 1;
}

// 80034860
void ESim::tProcessCommand(unsigned int command) {
  if (command == 0) initModel();
  else if (command == 1) ChangeCostume();
}

// 800348B8
// GC: on the app thread, the "changing" mark goes once the data manager has nothing queued.
void ESim::UpdateQueuedOperation() {
  if (m_changingSkin && m_queuedOps < 1) m_changingSkin = 0;
}

// 80034DE4
// The models of the description as it now is (the costume copied in), welded and the skin composited.
void ESim::ChangeCostume() {
  m_simModel->SetAllModels();
  m_simModel->VerifyRequiredModels();
  m_simModel->ApplyAllLatticeMorphing();
  m_simModel->ApplyAllMorphTargets();
  m_simModel->UpdateReweld(true);
  CompositeSkin();
  m_simModel->ApplySkinToModels();  // PC: the composited skin onto the new parts (initModel does the same)
}

// 80035374
int ESim::GetIsChangingOutfit() { return m_changingOutfit; }

// 8003489C
int ESim::HasQueuedOperation() { return m_queuedOps > 0 ? 1 : m_changingSkin; }

// 8003494C
// Drawn when asked, unless the person is invisible.
void ESim::DrawSim(bool draw) { m_drawSim = m_person && m_person->IsInvisible() ? 0 : (draw ? 1 : 0); }

namespace {
// SkillMeterParams (803FD9B0..; vSkillMeterColor / vRepairMeterColor from the unit's static initialiser).
struct SkillMeterParams {
  static float fBobDur, OuterColorScaler, fBobHeight, fRotateZ, fTranslateZ, fpbobtime;
  static float bobrange[2];
  static int pbobS0, pbobS1;
  static const float vSkillMeterColor[3], vRepairMeterColor[3];
};
float SkillMeterParams::fBobDur = 0.5f;
float SkillMeterParams::OuterColorScaler = 1.2f;
float SkillMeterParams::fBobHeight = 0.0f;
float SkillMeterParams::fRotateZ = 0.0f;
float SkillMeterParams::fTranslateZ = 0.3f;
float SkillMeterParams::fpbobtime = 0.0f;
float SkillMeterParams::bobrange[2] = {-0.012f, 0.012f};
int SkillMeterParams::pbobS0 = 0;
int SkillMeterParams::pbobS1 = 1;
const float SkillMeterParams::vSkillMeterColor[3] = {0.12f, 0.12f, 1.0f};
const float SkillMeterParams::vRepairMeterColor[3] = {0.25f, 0.6f, 0.25f};

// PlumbBobOwner::GetPlumbBob (8005B7E8): the player's plumb bob (made on first use: player 2's only in a
// two-player game), when this owner holds it. PC: the owner is the player's selected sim (GrabPlumbBob
// is not ported; the selected sim's ESim stands for the owner at +0x3C8).
PlumbBob* PlayerPlumbBob(ESim* sim) {
  const int player = static_cast<int>(sim->GetPlayerIndex());
  if (static_cast<unsigned int>(player) >= 2) return nullptr;
  if (!PlumbBob::s_pPlumbBob[player] && (player == 0 || _gd.m_fieldEC)) {
    PlumbBob::s_pPlumbBob[player] = new PlumbBob;
    PlumbBob::s_pPlumbBob[player]->SetPlayerColor(player);
  }
  return PlumbBob::s_pPlumbBob[player];
}
}  // namespace

// 80034AB8
// 0 / 1: player 1's / player 2's selected sim; -1 another sim.
RET_UNKNOWN ESim::GetPlayerIndex() {
  if (m_person && m_person == _globals.GetSelectedPerson(0)) return 0;
  if (m_person && m_person == _globals.GetSelectedPerson(1)) return 1;
  return -1;
}

// 80031F94
// Not while the model is being changed. The player's plumb bob: states 3 / 4 shrink it over 0.3 s, then
// it becomes the meter (model 0x4FFAFD38, states 8 / 9); state 7 grows it back (1.5 at 0.21 s, 1 at
// 0.3 s, then 0). Over a player's sim the bob jumps to the head (PlumbBob state 1) at that size.
void ESim::DrawPlumbBob(ERC*) {
  if (HasQueuedOperation()) return;
  PlumbBob* bob = PlayerPlumbBob(this);
  if (!bob) return;
  double scale = 1.0;
  const unsigned int state = static_cast<unsigned int>(m_plumbBobState);
  if (2 < state) {
    if (state < 5) {
      if (0.3f <= m_plumbBobTime) {
        bob->SetModel(0x4ffafd38);
        bob->SetShadow(0);
        if (m_meterModel) {
          m_meterModel->DelRef();
          m_meterModel = nullptr;
        }
        m_meterModel = _modelman.AddRef(0x4ffafd38u, nullptr, 0);
        m_plumbBobState = m_plumbBobState == 3 ? 8 : 9;
        m_plumbBobTime = 0.0f;
        return;
      }
      scale = 1.0 - m_plumbBobTime / 0.3f;
    } else if (state == 7) {
      const float t = m_plumbBobTime;
      if (0.21000001f <= t) {
        if (0.3f <= t) m_plumbBobState = 0;
        else scale = (1.0f - (t - 0.21000001f) / 0.09f) * 0.5f + 1.0f;
      } else {
        scale = (t * 1.5f) / 0.21000001f;
      }
    }
  }
  if (GetPlayerIndex() < 0) return;
  SAnimator2* anim = m_person ? m_person->GetSAnimator() : nullptr;
  if (!anim) return;
  EVec3 head{};
  anim->GetBonePos(1, head);
  bob->SetState(1);
  bob->m_target = head;
  if (bob->m_model) bob->m_model->m_scale = static_cast<float>(scale);
}

// 800321A0
// Player 1's sim moves the bob and spin (bobbing between bobrange over fBobDur, three radians a
// second); the meter's height factor: shrinking back (5, 6: then the plumb bob model again, state 7),
// growing in (8, 9: to 1.5 at 0.21 s, back to 1 at 0.3 s, then state 1 / 2), else 1. `inner` the
// meter's colour (skill: blue, repair: green), `outer` it brightened.
double ESim::UpdateSkillMeter(EVec3& inner, EVec3& outer) {
  PlumbBob* bob = PlayerPlumbBob(this);
  if (GetPlayerIndex() == 0) {
    using P = SkillMeterParams;
    P::fpbobtime += _gamedt;
    if (P::fBobDur + P::fBobDur < P::fpbobtime) {
      std::swap(P::pbobS0, P::pbobS1);
      P::fpbobtime = 0.0f;
    }
    P::fRotateZ = _gamedt * 3.0f + P::fRotateZ;
    P::fBobHeight = ((P::fpbobtime * 0.5f) / P::fBobDur) * (P::bobrange[P::pbobS1] - P::bobrange[P::pbobS0]) +
                    P::bobrange[P::pbobS0];
    if (P::fRotateZ < 0.0f) P::fRotateZ = 6.2831855f;
    else if (6.2831855f < P::fRotateZ) P::fRotateZ = 0.0f;
  }
  double scale = 1.0;
  const unsigned int state = static_cast<unsigned int>(m_plumbBobState);
  if (4 < state) {
    if (state < 7) {
      if (0.3f <= m_plumbBobTime) {
        if (bob) {
          bob->SetModel(0x6d1f0956);
          bob->SetShadow(0);
        }
        if (m_meterModel) {
          m_meterModel->DelRef();
          m_meterModel = nullptr;
        }
        m_meterModel = _modelman.AddRef(0x6d1f0956u, nullptr, 0);
        scale = 0.0;
        m_plumbBobState = 7;
        m_plumbBobTime = 0.0f;
      } else {
        scale = 1.0 - m_plumbBobTime / 0.3f;
      }
    } else if (state < 10 && 7 < state) {
      const float t = m_plumbBobTime;
      if (0.21000001f <= t) {
        if (0.3f <= t) m_plumbBobState = m_plumbBobState == 8 ? 1 : 2;
        else scale = (1.0f - (t - 0.21000001f) / 0.09f) * 0.5f + 1.0f;
      } else {
        scale = (t * 1.5f) / 0.21000001f;
      }
    }
  }
  const float* c = (m_plumbBobState == 1 || m_plumbBobState == 5 || m_plumbBobState == 8)
                       ? SkillMeterParams::vSkillMeterColor
                       : SkillMeterParams::vRepairMeterColor;
  inner = EVec3{c[0], c[1], c[2]};
  outer = EVec3{inner.x * SkillMeterParams::OuterColorScaler, inner.y * SkillMeterParams::OuterColorScaler,
                inner.z * SkillMeterParams::OuterColorScaler};
  return scale;
}

// 80032488
// The meter over the sim's head (its head bone, raised by fTranslateZ and the bob): the fill (sub-model
// 0) scaled up by the skill value (+0x3F0) and spun, lit by the meter's colour; the frame (sub-model 1)
// lit by the brightened colour (ambient 0.3 of it). Both lights fall across the view. Smaller (0.6) over
// a sim no player has. Not while the meter is off (state 10) or has shrunk away.
void ESim::DrawSkillMeter(ERC* rc) {
  if (!PlayerPlumbBob(this) && GetPlayerIndex() >= 0) return;
  EVec3 inner, outer;
  const double scale = UpdateSkillMeter(inner, outer);
  if (scale == 0.0 || m_plumbBobState == 10) return;
  auto* model = static_cast<ERModel*>(m_meterModel);
  if (!model || model->m_subModels.size() < 2) return;
  SAnimator2* anim = m_person ? m_person->GetSAnimator() : nullptr;
  if (!anim) return;
  EVec3 head{};
  anim->GetBonePos(1, head);
  ESimsCam* cam = _globals.GetCam();
  EVec3 dir{0.0f, 0.0f, 1.0f};
  if (cam) {
    dir = EVec3{-(cam->m_target.y - cam->m_eye.y), cam->m_target.x - cam->m_eye.x, head.z};
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len != 0.0f) dir = EVec3{dir.x / len, dir.y / len, dir.z / len};
  }
  ELights fill, frame;
  fill.m_ambient[0] = inner.x, fill.m_ambient[1] = inner.y, fill.m_ambient[2] = inner.z;
  fill.m_dir[0].m_color[0] = inner.x, fill.m_dir[0].m_color[1] = inner.y, fill.m_dir[0].m_color[2] = inner.z;
  fill.m_dir[0].m_dir[0] = dir.x, fill.m_dir[0].m_dir[1] = dir.y, fill.m_dir[0].m_dir[2] = dir.z;
  fill.m_numDir = 1;
  frame.m_ambient[0] = outer.x * 0.3f, frame.m_ambient[1] = outer.y * 0.3f, frame.m_ambient[2] = outer.z * 0.3f;
  frame.m_dir[0].m_color[0] = outer.x, frame.m_dir[0].m_color[1] = outer.y, frame.m_dir[0].m_color[2] = outer.z;
  frame.m_dir[0].m_dir[0] = dir.x, frame.m_dir[0].m_dir[1] = dir.y, frame.m_dir[0].m_dir[2] = dir.z;
  frame.m_numDir = 1;
  EMat4 base;
  base.Id();
  base.m[3][0] = head.x;
  base.m[3][1] = head.y;
  base.m[3][2] = head.z + SkillMeterParams::fTranslateZ + SkillMeterParams::fBobHeight;
  float s = model->m_scale;
  if (GetPlayerIndex() < 0) s *= 0.6f;
  EVec3 sv{s, s, static_cast<float>(s * scale)};
  base.PreScale(sv);
  EMat4 fillMatrix = base;
  EVec3 fv{1.0f, 1.0f, m_skillValue};
  fillMatrix.PreScale(fv);
  fillMatrix.PreRotateZ(SkillMeterParams::fRotateZ);
  rc->Lights(&fill);
  rc->ModelMatrix(&fillMatrix);
  model->m_subModels[0]->Draw(rc);
  rc->Lights(&frame);
  rc->ModelMatrix(&base);
  model->m_subModels[1]->Draw(rc);
}

// 8003317C
// The record of a sim's plumb bob or meter: the meter in its states (1, 2, 5, 6, 8, 9), else (0, 3, 4, 7)
// the player's plumb bob's part (DrawPlumbBob); the draw state forgotten after.
void ESim::SkillMeterOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  auto* sim = static_cast<ESim*>(otd->m_instance);
  const unsigned int s = static_cast<unsigned int>(sim->m_plumbBobState);
  const bool meter = s == 1 || s == 2 || s == 5 || s == 6 || s == 8 || s == 9;
  if (meter) sim->DrawSkillMeter(data.m_rc);
  else if (s == 0 || s == 3 || s == 4 || s == 7) sim->DrawPlumbBob(data.m_rc);
  data.m_curLights = nullptr;
  data.m_curShader = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
  data.m_fieldB0 = 0;
}

// 80032928
// The meter's states: 1 a skill meter (3 growing in, 8 from the plumb bob), 2 a repair meter (4, 9),
// 0 back to the plumb bob (5 / 6 shrinking from a meter, 7 from a growing one), 10 none. The player's
// plumb bob takes the model too (PlumbBob::SetModel / SetShadow).
void ESim::SetPlumbBobState(unsigned int state) {
  constexpr unsigned int kMeterModel = 0x4ffafd38u, kPlumbBobModel = 0x6d1f0956u;
  PlumbBob* bob = PlayerPlumbBob(this);
  auto model = [this, bob](unsigned int id, int next) {
    if (bob) {
      bob->SetModel(id);
      bob->SetShadow(0);
    }
    if (m_meterModel) {
      m_meterModel->DelRef();
      m_meterModel = nullptr;
    }
    m_meterModel = _modelman.AddRef(id, nullptr, 0);
    m_plumbBobTime = 0.0f;
    m_plumbBobState = next;
  };
  int next;
  switch (state) {
    case 1:
      if (m_plumbBobState == 1 || m_plumbBobState == 3 || m_plumbBobState == 8) return;
      if (m_plumbBobState != 0) return model(kMeterModel, 8);
      next = 3;
      break;
    case 0:
      if (m_plumbBobState == 1) next = 5;
      else if (m_plumbBobState == 2) next = 6;
      else if (m_plumbBobState == 0 || m_plumbBobState == 5 || m_plumbBobState == 6 || m_plumbBobState == 7) return;
      else return model(kPlumbBobModel, 7);
      break;
    case 2:
      if (m_plumbBobState == 2 || m_plumbBobState == 4 || m_plumbBobState == 9) return;
      if (m_plumbBobState != 0) return model(kMeterModel, 9);
      next = 4;
      break;
    case 10:
      m_plumbBobState = 10;
      return;
    default: return;
  }
  m_plumbBobState = next;
  m_plumbBobTime = 0.0f;
}

// 80034AF0
void ESim::UpdatePlumbBob() {
  if (static_cast<unsigned int>(m_plumbBobState - 3) > 6) return;
  m_plumbBobTime += _dt;
}

// 80035314
cXPerson* ESim::GetPerson() { return m_person; }
// 80035338
SimModel* ESim::GetSimModel() { return m_simModel; }
// 8003536C
int ESim::GetIsModelLoaded() { return m_modelReady; }
// 80035308
// GC: nothing (the person's object is set by the constructor).
void ESim::SetXOb(cXObject*) {}

// PC: where the animator stands the sim (its engine-format position, as
// SAnimator2::positionCharacter places it); the person's location before it has one.
EVec3 ESim::GetSimPos() {
  SAnimator2* anim = m_person ? m_person->GetSAnimator() : nullptr;
  if (anim) {
    const EVec3 p = anim->GetEngineFormatPos();
    if (p.x != 0.0f || p.y != 0.0f) return EVec3{p.x, p.y, 0.0f};
  }
  cXPersonImpl* impl = m_person ? static_cast<cXPersonImpl*>(m_person) : nullptr;
  if (!impl) return EVec3{0.0f, 0.0f, 0.0f};
  return EVec3{impl->m_location[0] * 0.0625f, impl->m_location[1] * 0.0625f, 0.0f};
}

// 80034090
// Once the model is in, on the game thread: the person's animator runs (it moves and poses the
// sim: SAnimator2::Update), ghosts return to full alpha when they stop being ghosts, the plumb bob and shadow follow.
// GC: also queued data-manager operations, outfit changes, the shower curtain and the ghost effect
// (UpdateGhostParticleEffect: boneparticle.cpp).
void ESim::Update() {
  if (!m_modelReady) {
    initModel();  // PC: ESimsDataManager's job (see the constructor)
    if (!m_modelReady) return;
  }
  m_otd.m_callback = SimOrderTableCallback;  // GC: ServiceNpcOrderTableCallback for service NPCs (pets)
  // PC development aid: SIMMY_SKILL_METER_TEST=<fill 0..1>[,repair] shows the skill (or repair) meter over
  // player 1's sim, as TrySetBalloon's group 9 icons do.
  static const char* meterTest = std::getenv("SIMMY_SKILL_METER_TEST");
  if (meterTest && GetPlayerIndex() == 0 && m_plumbBobState == 0) {
    m_skillValue = static_cast<float>(std::atof(meterTest));
    SetPlumbBobState(std::strstr(meterTest, "repair") ? 2u : 1u);
  }
  UpdateQueuedOperation();
  UpdateSkinChange();
  UpdateShowerCurtain();
  UpdateGhostParticleEffect();
  if (HasQueuedOperation()) {
    if (m_shadow) m_shadow->m_flags |= 0x100000;
    return;
  }
  if (SAnimator2* anim = m_person->GetSAnimator()) anim->Update();
  const int ghost = IsGhostSim() ? 1 : 0;
  if (!ghost && m_wasGhost) m_alpha[1] = m_alpha[0] = 1.0f;
  m_wasGhost = ghost;
  UpdatePlumbBob();
  UpdateShadow();
}

// 80034B14
// The shadow shows only with the sim: not while its object is hidden (data 0x22), before its first draw,
// when it is not drawn, while the data manager has work for it, as a ghost, or (service NPCs) with the
// model 0x9C5FC449.
void ESim::UpdateShadow() {
  if (!m_shadow) return;
  bool show = true;
  if ((m_xob && m_xob->GetData(0x22) != 0) || m_firstDraw == 1 || m_drawSim == 0 || HasQueuedOperation() ||
      IsGhostSim())
    show = false;
  else if (m_model && m_person->GetServiceNPC() && m_model->m_id == 0x9C5FC449u)
    show = false;
  if (show) m_shadow->m_flags &= ~0x100000u;
  else m_shadow->m_flags |= 0x100000u;
}

// 8003530C
EIStaticModel* ESim::GetShadow() { return m_shadow; }

// 80034F78
ESimShadow::ESimShadow(cXPerson* person) : m_person(person) {}

// 80034FBC
ESimShadow::~ESimShadow() { m_person = nullptr; }

// 80032E34
// The shadow model, then the props the sim holds (their models as shadows too).
void ESimShadow::DrawShadow(ELevelDrawData& data) {
  EIStaticModel::DrawShadow(data);
  if (SAnimator2* anim = m_person ? m_person->GetSAnimator() : nullptr) anim->DrawPropsShadow(data.m_rc);
}

// 80032E88
// The middle of the sim on the ground (the animator's node 2).
void ESimShadow::GetShadowCenter(EVec3& out) const {
  SAnimator2* anim = m_person ? m_person->GetSAnimator() : nullptr;
  if (anim) anim->GetBonePos(2, out);
  out.z = 0.0f;
}

// 800334B4
void ESim::DoAnimation() {
  if (!HasQueuedOperation()) m_anim.Compute(m_orient, _framecount);
}

// 800334F8
void ESim::DoLightingCalculation() {}

// 800341E8
// GC: the pose's tight bound box when the controller has matrices, else the bound sphere.
unsigned int ESim::VisibilityTest(E3DWindow& win) {
  if (HasQueuedOperation()) return 0;
  return win.Test(*reinterpret_cast<EBound3*>(m_bounds));
}

namespace {
// 80030AC4
// While depth of field runs (the current director's), the drawing player's sim and the sim it is in
// social mode with stay in front of it.
bool IsPartOfDOFForeground(ESim* sim) {
  CameraDirector* d = _globals.m_cameraDirector;
  FrameEffectsManager* fx = d ? d->GetFrameEffectsManager() : nullptr;
  DepthOfField* dof = fx ? fx->DepthOfFieldObject() : nullptr;
  if (!dof || dof->m_state == kFrameEffectOff) return false;
  cXPerson* person = sim->m_person;
  // GC: DAT_80475E98, the player being drawn; PC: the player whose director is current (DrawMain
  // selects it with the view).
  const int player = d == _globals.CameraDirectorOf(1) ? 1 : 0;
  cXPerson* selected = _globals.GetSelectedPerson(player);
  if (!person || !selected) return false;
  if (selected != person) {
    auto* impl = dynamic_cast<cXPersonImpl*>(selected);
    if (!impl || !impl->IsInSocialMode()) return false;
    auto* target = reinterpret_cast<cXPersonImpl*>(impl->GetSocialModeTarget());
    if (!target || static_cast<cXPerson*>(target) != person) return false;
  }
  return true;
}
}  // namespace

// 800334FC
// Unless hidden (object data 0x22): the lights at its bound sphere, brightened to the tuning's least
// luminance (ERLevel::s_levelLightingTuning [0]: ambient scale, [1]: scale of the other lights, [3]: the
// floor), with a light from over the camera's shoulder (the camera's frame turned 90 degrees about y,
// tuning [2]); the body is drawn at once (blended or ghosts: through the order table). Close to the
// camera (fade mode 0) it fades out.
void ESim::Draw(ELevelDrawData& data) {
  if (!m_modelReady || !m_xob || m_xob->GetData(0x22)) return;
  if (m_person->IsGreen()) {
    SIMMY_STUB("ESim::Draw: the green ghost light (_ESim_GreenLight)", 0x800334FC);
    return;
  }
  const float* tuning = ERLevel::s_levelLightingTuning.m_field00;
  CalcLights(*reinterpret_cast<EVec3*>(m_sphere), m_lights, false);
  m_simLights = m_lights;
  const bool ghost = IsGhostSim();
  float lightColor;
  if (!ghost) {
    float r = m_lights.m_ambient[0] * tuning[0], g = m_lights.m_ambient[1] * tuning[0], b = m_lights.m_ambient[2] * tuning[0];
    const float lum = b * 0.11f + r * 0.3f + g * 0.59f;
    if (lum < tuning[3]) {
      if (lum >= 1e-06f) {
        const float k = tuning[3] / lum;
        r *= k, g *= k, b *= k;
      } else {
        r = g = b = tuning[3];
      }
    }
    m_lights.m_ambient[0] = r, m_lights.m_ambient[1] = g, m_lights.m_ambient[2] = b;
    for (unsigned int i = 0; i < m_lights.m_numDir; ++i)
      for (float& c : m_lights.m_dir[i].m_color) c *= tuning[1];
    for (unsigned int i = 0; i < m_lights.m_numPoint; ++i)
      for (float& c : m_lights.m_point[i].m_color) c *= tuning[1];
    lightColor = tuning[2];
  } else {
    m_lights.m_ambient[0] = tuning[7] * tuning[4];
    m_lights.m_ambient[1] = tuning[8] * tuning[4];
    m_lights.m_ambient[2] = tuning[9] * tuning[4];
    for (unsigned int i = 0; i < m_lights.m_numDir; ++i) {
      m_lights.m_dir[i].m_color[0] *= tuning[10];
      m_lights.m_dir[i].m_color[1] *= tuning[11];
      m_lights.m_dir[i].m_color[2] *= tuning[12];
    }
    m_lights.m_numPoint = 0;
    lightColor = tuning[5];
  }
  // (0, 0, -1) turned about y (by -90 degrees; ghosts by tuning [6]), then into the world by the
  // camera's frame.
  EMat4 ry;
  ry.RotateY(ghost ? tuning[6] : -1.5707964f);
  const float lx = -ry.m[2][0], ly = -ry.m[2][1], lz = -ry.m[2][2];
  const EMat4& cam = data.m_win->m_lookAtPos;
  if (m_lights.m_numDir == 4) m_lights.m_numDir = 3;
  ELightsDir& extra = m_lights.m_dir[m_lights.m_numDir];
  extra.m_dir[0] = lz * cam.m[2][0] + lx * cam.m[0][0] + ly * cam.m[1][0];
  extra.m_dir[1] = lz * cam.m[2][1] + lx * cam.m[0][1] + ly * cam.m[1][1];
  extra.m_dir[2] = lz * cam.m[2][2] + lx * cam.m[0][2] + ly * cam.m[1][2];
  extra.m_color[0] = extra.m_color[1] = extra.m_color[2] = lightColor;
  ++m_lights.m_numDir;

  m_drawModel = 1;
  if (m_firstDraw == 1 || !m_drawSim || HasQueuedOperation()) {
    m_drawModel = 0;
    m_firstDraw = 0;
  }
  if (m_fadeMode == 0 && _globals.m_cam) {
    // Fade out when the camera comes within reach of the sim's head.
    ESimsCam* cam3d = _globals.m_cam;
    const EVec3 pos = GetSimPos();
    const float nearPlane = cam3d->GetNearPlane();
    const float fov = cam3d->GetFov();
    const float s = std::sin(fov * 0.017453292f);
    const float reach = nearPlane * 4.32f - (s + s);
    const float dx = pos.x - cam3d->m_eye.x, dy = pos.y - cam3d->m_eye.y, dz = 2.0f - cam3d->m_eye.z;
    const float d2 = dx * dx + dy * dy + dz * dz;
    const float alpha = GetInstAlpha();
    if (reach <= d2) {
      if (alpha < 1.0f) SetInstAlpha(_dt * 4.0f + alpha);
    } else if (alpha > 0.0f) {
      SetInstAlpha(alpha - _dt * 4.0f);
    }
  }
  if (ghost) {
    const float alpha = GetInstAlpha();
    if (alpha != 0.0f && alpha < 1.0f) SetInstAlpha(0.0f);
  }
  m_otd.m_lights = &m_lights;
  m_censorOtd.m_lights = &m_lights;
  m_propOtd.m_lights = &m_lights;
  const float alpha = GetInstAlpha();
  if (alpha == 0.0f) return;
  const bool dofForeground = !data.m_fieldA4 && IsPartOfDOFForeground(this);
  if (dofForeground) {
    // While depth of field runs: drawn sharp after the blur (the level's foreground list), its censor
    // first. GC: also its props (+0x4E8) record.
    m_level->InsertInDOFForegroundObjectList(m_censorOtd);
    m_level->InsertInDOFForegroundObjectList(m_otd);
    m_level->InsertInDOFForegroundObjectList(m_propOtd);
  } else if (alpha < 1.0f || ghost) {
    InsertInOrderTable(m_otd);
    InsertInOrderTable(m_propOtd);
  } else {
    if (data.m_curLights != m_otd.m_lights) {
      data.m_curLights = m_otd.m_lights;
      data.m_rc->Lights(m_otd.m_lights);
    }
    m_otd.m_callback(data, &m_otd);
    data.m_fieldB0 = 0;
    // The props: drawn now, or sorted in the order table when one blends (SAnimator2::PropsHaveAlpha).
    SAnimator2* anim = m_person ? m_person->GetSAnimator() : nullptr;
    if (anim && !anim->PropsHaveAlpha()) {
      if (data.m_curLights != m_propOtd.m_lights) {
        data.m_curLights = m_propOtd.m_lights;
        data.m_rc->Lights(m_propOtd.m_lights);
      }
      m_propOtd.m_callback(data, &m_propOtd);
      data.m_fieldB0 = 0;
    } else {
      InsertInOrderTable(m_propOtd);
    }
  }
  if (!dofForeground) InsertInOrderTable(m_censorOtd);
  InsertInOrderTable(m_meterOtd);
}


// 80032C58
// The body skinned by the controller's matrices (ghosts: RenderGhosted, ghost.cpp); after its first
// draw the morph data goes. Then the common items (the carried prop, the cursor highlight), and the
// draw state is forgotten (the renderer's state is the body's).
void ESim::SimOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  ESim* sim = static_cast<ESim*>(otd->m_instance);
  ERC* rc = data.m_rc;
  if (sim->m_drawModel) {
    const int count = sim->m_anim.NodeCount();
    EMat4* matrices = sim->m_anim.m_matrices;
    if (!matrices || !count) return;
    if (!sim->IsGhostSim())
      SimRenderer::Render(rc, sim->m_simModel->m_parts, nullptr, matrices, static_cast<unsigned int>(count), false);
    else
      SimRenderer::RenderGhosted(rc, sim->m_simModel->m_parts, matrices, static_cast<unsigned int>(count), false);
    if (sim->m_freeMorphs) {
      sim->m_freeMorphs = 0;
      sim->m_simModel->DeallocateMorphResources();
    }
  }
  sim->DrawSimAndNpcCommonItems(rc);
  data.m_curLights = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
  data.m_curShader = nullptr;
}

// 80032B84
// The props the sim wears, skinned by the body's bone matrices (ERC::ModelMatrixList, then
// SAnimator2::DrawProps); the draw state is forgotten.
void ESim::PropOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  ESim* sim = static_cast<ESim*>(otd->m_instance);
  if (sim->m_drawModel) {
    const int count = sim->m_anim.NodeCount();
    EMat4* matrices = sim->m_anim.m_matrices;
    if (!matrices || !count) return;
    data.m_rc->ModelMatrixList(matrices, count);
    if (SAnimator2* anim = sim->m_person ? sim->m_person->GetSAnimator() : nullptr) anim->DrawProps(data.m_rc, false, nullptr);
  }
  data.m_curLights = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
  data.m_curShader = nullptr;
}

// 80032D68
// While the model is drawn, the animator's censor mosaic (at full alpha), then the instance's alpha
// again; the draw state is forgotten.
void ESim::CensorOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  ESim* sim = static_cast<ESim*>(otd->m_instance);
  ERC* rc = data.m_rc;
  if (sim->m_drawModel) {
    rc->SetAlpha(1.0f);
    if (SAnimator2* anim = sim->m_person ? sim->m_person->GetSAnimator() : nullptr) anim->DrawCensor(rc);
    rc->SetAlpha(sim->GetInstAlpha());
  }
  data.m_curShader = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
}

// 80032EF8
void ESim::DrawSimModel(ERC* rc, EMat4* matrices, unsigned int count) {
  SimRenderer::Render(rc, m_simModel->m_parts, nullptr, matrices, count, false);
}

// 80032FFC
// The changing curtain while it is up (rising out of the floor as its alpha goes 0 -> 1); the cursor
// highlight when the drawing player's cursor is on the sim (cursor flags 1 / 8) and its director is not
// in state 8.
void ESim::DrawSimAndNpcCommonItems(ERC* rc) {
  if (m_curtainModel && m_curtain) {
    EMat4 m;
    GetOrient(m);
    m.PreScale(m_curtainModel->m_scale);
    const float t = m_curtainAlpha;
    const float s = t * 0.0f + t * -2.0f * t * t + t * 3.0f * t;  // smoothstep
    m.m[3][2] += (s + s) - 2.0f;
    rc->ModelMatrices(&m, 1);
    m_curtainModel->Draw(rc);
  }
  const int player = EInstance::GetContext();  // GC: DAT_80475E98, the player being drawn
  bool highlight = false;
  if (player == 0 && m_cursFlags == 1) {
    CameraDirector* d = _globals.CameraDirectorOf(0);
    highlight = d && d->m_state != 8;
  }
  if (player == 1 && m_cursFlags == 8) {
    CameraDirector* d = _globals.CameraDirectorOf(1);
    highlight = highlight || (d && d->m_state != 8);
  }
  if (highlight) DrawCursorHighLight(rc);
}

// 80033218
// Two rings under the sim (not the drawing player's own selected sim) in the player's plumb bob
// colour, pulsing between 1.1 and 1.6 times their size every 0.4 s. (GC: DAT_80476124, never set,
// turns it off.)
void ESim::DrawCursorHighLight(ERC* rc) {
  const int player = EInstance::GetContext();
  if (_globals.GetSelectedPerson(player) == m_person || HasQueuedOperation()) return;
  ELights lights;
  for (int i = 0; i < 3; ++i) lights.m_ambient[i] = g_plumbBobParms.m_colorScale[player * 3 + i];
  EMat4 orient;
  GetOrient(orient);
  m_hlTime += _dt;
  static const float kScale[2] = {1.1f, 1.6f};
  if (m_hlTime > 0.4f) {
    std::swap(m_hlFrom, m_field420);
    m_hlTime = 0.0f;
  }
  const float t = m_hlTime / 0.4f;
  const float scale = (t * 0.0f + t * -2.0f * t * t + t * 3.0f * t) * (kScale[m_field420] - kScale[m_hlFrom]) + kScale[m_hlFrom];
  EMat4 m;
  m.Id();
  EVec3 sc(scale, scale, 1.0f);
  m.Scale(sc);
  EVec3 at(orient.m[3][0], orient.m[3][1], 0.01f);
  m.PostTranslate(at);
  rc->ModelMatrices(&m, 1);
  ELights lights2 = lights;
  rc->Lights(&lights2);
  if (m_hlModels[0]) m_hlModels[0]->Draw(rc);
  rc->Lights(&lights);
  if (m_hlModels[1]) m_hlModels[1]->Draw(rc);
}
