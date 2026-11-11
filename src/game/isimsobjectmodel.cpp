// Port of game/isiminstance.cpp and game/iobject.cpp (partial): simulation objects' 3D models and
// their animation. Ported: the model and controller set-up, the animation track (ObjAnimDef), the
// animation's script events, the per-frame node computation, lighting and drawing. Still stubbed: the
// simulation side (cXObject queries, the simulator's speed), sounds and particle effects from
// animation events, lamps and light animation, highlights, shadows, multi-tile and wall objects,
// orientation from the object and EStream serialisation.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/epointlight.hpp"
#include "simmy/classes/psystem.hpp"
#include "simmy/roomresolve.hpp"
#include "simmy/platform.hpp"
#include "simmy/detmath.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

// 800568AC ISimInstance::GetTypeInfo
// 800568B8 ISimInstance::GetTypeName
// 800568C4 ISimInstance::GetTypeKey
// 800568D0 ISimInstance::GetTypeVersion
// 8005686C ISimInstance::SafeDelete
// 80056800 ISimInstance::New
// 80056828 ISimInstance::Construct
// 80056848 ISimInstance::Destruct
// 800568DC ISimInstance::GetTypeInfoStatic
// 800568E8 ISimInstance::GetReadVersion
SIMMY_STORABLE_IMPL(ISimInstance, "ISimInstance", 0, EIStaticModel)
// 80053CE0 ISimsObjectModel::GetTypeInfo
// 80053CEC ISimsObjectModel::GetTypeName
// 80053CF8 ISimsObjectModel::GetTypeKey
// 80053D04 ISimsObjectModel::GetTypeVersion
// 80053CA0 ISimsObjectModel::SafeDelete
// 80053C08 ISimsObjectModel::New
// 80053C5C ISimsObjectModel::Construct
// 80053C7C ISimsObjectModel::Destruct
// 80053D10 ISimsObjectModel::GetTypeInfoStatic
// 80053D1C ISimsObjectModel::GetReadVersion
SIMMY_STORABLE_IMPL(ISimsObjectModel, "ISimsObjectModel", 0, ISimInstance)

// ---------------------------------------------------------------------------------------------------
// ISimInstance

// 8005635C
// GC: also (re)initialises the shared placement-error light set (_ERRORLight / _ERRORLightCur:
// ambient 2, 0, 0, no other lights).
ISimInstance::ISimInstance() {}

// 80056424
// The lamps it owns leave their level.
ISimInstance::~ISimInstance() {
  m_xob = nullptr;
  for (EILight** light : {&m_light1, &m_light2}) {
    if (*light) {
      if (ERLevel* level = (*light)->GetLevel()) level->RemoveLight(*light);
      delete *light;
      *light = nullptr;
    }
  }
}

// 800562F4
void ISimInstance::SetPlacementError(bool on) { SetHighlight(kErrorLight, on); }
// 8005631C
void ISimInstance::Create(cXObject*, EHouse*) {}
// 80056320
void ISimInstance::OrentSubObject(cXObject*) {}
// 80056328
void ISimInstance::CreateShadow() {}
// 8005632C
void ISimInstance::InsertSubModelsInHouse(ERLevel*) {}
// 80056330
void ISimInstance::RemoveSubModelsFromHouse(ERLevel*) {}
// 80056334
void ISimInstance::PropigateFlagsToSubModels() {}
// 80056338
void ISimInstance::SetOutOfWorld() {}
// 8005633C
void ISimInstance::SetCarryOrient() {}
// 80056340
void ISimInstance::StartBurp(int) {}
// 80056344
EIStaticModel* ISimInstance::GetShadow() { return nullptr; }
// 8005634C
void ISimInstance::SetXOb(cXObject* xob) { m_xob = xob; }
// 80056354
cXObject* ISimInstance::GetXOb() { return m_xob; }
// 80056628
// Whether the object offers player `player` anything (its interactions for the player's menu, none while
// its data 0x22 is set), looked at again every 10..25 frames. GC: the cached answer while the simulator is
// in a tick (vtable 0x7C).
RET_UNKNOWN ISimInstance::HasInteractions(int player) {
  const int p = player & 1;
  if (m_interactionsFrame[p] <= _framecount) {
    cXObjectImpl* o = m_xob ? m_xob->GetObjectImplementation() : nullptr;
    MenuInteractionList list;
    if (o && o->GetData(0x22) == 0) CollectInteractionsForObject(o, list, player, 1);
    m_hasInteractions[p] = list.empty() ? 0 : 1;
    m_interactionsFrame[p] = _framecount + (simmy::ui_rand() & 0xf) + 10;
  }
  return m_hasInteractions[p];
}

// 800569F0
void ISimInstance::CleanupStuff(ERLevel*) {}
// 800569F4
void ISimInstance::SetCursFlags(unsigned int flags) { m_cursFlags = flags; }
// 800569FC
unsigned int ISimInstance::GetCursFlags() { return m_cursFlags; }
// 8005656C
void ISimInstance::SetHighlight(unsigned int flags, bool on) {
  if (on) m_cursFlags |= flags;
  else m_cursFlags &= ~flags;
}
// 80056A08
RET_UNKNOWN ISimInstance::IsMultiTilePart() { return 0; }
// 80056A10
unsigned int ISimInstance::GetFlags() { return m_cursFlags; }
// 80056A18
int ISimInstance::HasModel() { return m_model ? 1 : 0; }
// 80056A30
EAnimController* ISimInstance::GetAnimController() { return &m_anim; }

namespace {
// ENodeList::RemoveAll on the owning list of EParticleObj (+0x440).
void ClearParticles(std::vector<EParticleObj*>& list) {
  for (EParticleObj* obj : list) delete obj;
  list.clear();
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// ISimsObjectModel

// 8004C500
ISimsObjectModel::ISimsObjectModel() {
  m_flags |= 0x300;
  m_objFlags &= 0x7ffffff;
}

// 8004D598
// A new state's effects (its ObjAnimDef's attachment, +0x04) replace the old ones, unless the object
// is hidden (data 0x22).
void ISimsObjectModel::UpdateParticle(ObjAnimDef* def) {
  // PC development aid: SIMMY_FX_OBJECT=<object name part>=<attachment name> gives matching objects
  // that effects attachment instead of their state's (to look at effects without their triggers).
  ObjAnimDef forced;
  if (const char* fx = std::getenv("SIMMY_FX_OBJECT")) {
    const char* eq = std::strchr(fx, '=');
    cXObjectImpl* self = m_xob ? m_xob->GetObjectImplementation() : nullptr;
    if (eq && self && std::string(self->DebugName()).find(std::string(fx, eq)) != std::string::npos) {
      forced = *def;
      forced.m_field04 = EResourceManager::CalcId(const_cast<char*>(eq + 1));
      def = &forced;
    }
  }
  if (m_xob->GetData(0x22) != 0 || m_curAttach == def->m_field04) return;
  m_curAttach = def->m_field04;
  ClearParticles(m_particles);
  auto* obj = new EParticleObj;
  EMat4 m;
  GetOrient(m);
  if (obj->CreateEffects(m, 0.0f, def)) m_particles.push_back(obj);
  else delete obj;
}

// 8004C9D0
// Its lights leave the house's level: the two it owns (a window's portal lights, +0x3BC / +0x3C0), the
// ones it animates and its bulb; the bulb and animated lights are deleted (the owned two by
// ~ISimInstance). GC: also its separate sub-instance (+0x44C) and the particles; the preloaded
// animation released (PC: in the destructor).
void ISimsObjectModel::CleanupStuff(ERLevel* level) {
  if (!level) return;
  if (m_light1) level->RemoveLight(m_light1);
  if (m_light2) level->RemoveLight(m_light2);
  for (EILight* light : m_animLights) level->RemoveLight(light);
  if (m_bulb) {
    level->RemoveLight(m_bulb);
    delete m_bulb;
    m_bulb = nullptr;
  }
}

// 8004C644
// Its lights out of the house's level (CleanupStuff), its sub-objects deleted, then its references
// released.
ISimsObjectModel::~ISimsObjectModel() {
  if (m_house) CleanupStuff(m_house->m_level);
  for (EInstance* sub : m_subModels) delete sub;  // owned (+0x458); each leaves its level (~EInstance)
  m_subModels.clear();
  for (EILight* light : m_animLights) delete light;  // GC: TArray<EILight*>::~TArray (owning)
  m_animLights.clear();
  if (m_stateShader) {  // PC: released (GC kept the reference)
    m_stateShader->DelRef();
    m_stateShader = nullptr;
  }
  ClearParticles(m_particles);
  if (m_preloadAnim) {
    _animman.DelRefAsync(m_preloadAnim);
    m_preloadAnim = 0;
  }
}

// 8004C91C
void ISimsObjectModel::InsertSubModelsInHouse(ERLevel* level) {
  for (EInstance* sub : m_subModels) level->InsertInstance(sub, nullptr);
}

// 8004C96C
// The sub-objects take its cursor flags (highlight, placement error): a counter's sink lights with it.
void ISimsObjectModel::PropigateFlagsToSubModels() {
  for (EInstance* sub : m_subModels)
    if (auto* model = dynamic_cast<ISimInstance*>(sub)) model->SetCursFlags(m_cursFlags);
}

// 8004E55C
// The sub-models leave the level, with their shadows and lamps.
void ISimsObjectModel::RemoveSubModelsFromHouse(ERLevel* level) {
  for (EInstance* sub : m_subModels) {
    level->RemoveInstance(sub);
    auto* model = dynamic_cast<ISimsObjectModel*>(sub);
    if (!model) continue;
    if (EIStaticModel* shadow = model->GetShadow()) level->RemoveInstance(shadow);
    if (model->m_light1) level->RemoveLight(model->m_light1);
    if (model->m_light2) level->RemoveLight(model->m_light2);
  }
}

// 8004DF34
void ISimsObjectModel::AnimStartImmediate(unsigned int anim) {
  m_anim.SetTrackAnim(eTrackFlags{1}, anim, 0, 0.0f, nullptr);
  m_trackMode = kTracksRunning;
}

// 8004DF84
void ISimsObjectModel::AnimStartBlendIn(unsigned int anim) {
  m_anim.SetTrackAnim(eTrackFlags{1}, anim, 3, -1.0f, nullptr);
  m_trackMode = kTracksBlending;
}

// 8004DFD4
void ISimsObjectModel::KillAllAnimTracks() {
  m_anim.StopAllTracks();
  m_trackMode = kTracksOff;
  if (m_preloadAnim) {
    _animman.DelRefAsync(m_preloadAnim);
    m_preloadAnim = 0;
  }
}

// 8004E028
// Running: the controller is re-enabled while the track is young (under 0.24 s) or loops (track
// field 0x0E 0); otherwise it is left as it is this frame. Blending: until no main track is still
// fading in.
void ISimsObjectModel::UpdateTracks() {
  if (m_trackMode == kTracksRunning) {
    EACTrack* track = m_anim.GetTrackAnimDef(eTrackFlags{eTrackFlags::kMain});
    if (m_trackTime < 0.24f || (track && track->m_field0E == 0)) {
      m_anim.Enable(true, m_orient);
      m_objFlags &= 0x7fffffff;
    } else {
      m_objFlags |= 0x80000000;
    }
    UpdateAnimController();
    m_objFlags &= 0x7fffffff;
  } else if (m_trackMode == kTracksBlending) {
    UpdateAnimController();
    bool fading = false;
    for (int i = 0; i < m_anim.m_mainCount; ++i) {
      const EACTrack& t = m_anim.m_tracks[i];
      if (t.m_active == 1 && (t.m_flags & eTrackFlags::kInBlendList) && t.m_intensity > 0.0f) {
        fading = true;
        break;
      }
    }
    if (!fading) m_trackMode = kTracksRunning;
  }
}

namespace {
// The simulator's speed for object animations: its speed multiplier in mode 1 (live), else 1.
float SimulatorSpeed() {
  if (!Globs_pSimulator || Globs_pSimulator->GetMode() != 1) return 1.0f;
  return static_cast<float>(Globs_pSimulator->GetSpeedMultiplier());
}
}  // namespace

// 8005455C
// A new animation (or a restart): the controller is enabled and the track blended in from the
// current one, started at once, or stopped for no animation. GC: objects whose cXObject is busy
// hand this to the object instead (vtable 0x4E4).
void ISimsObjectModel::StartNewAnimTrack(ObjAnimDef* def, bool restart) {
  const unsigned int anim = def->m_anim;
  if ((m_curAnim == anim && !restart) || m_anim.m_tracks.empty()) return;
  m_animDone = 0;
  m_animMsFraction = 0.0f;
  m_eventsLoop = 1;
  m_scriptEvents.clear();
  m_animMs = 0;
  if (anim && m_xob) {
    SIMMY_STUB("ISimsObjectModel::StartNewAnimTrack: the cXObject's animation hand-off", 0x8005455C);
  }
  m_anim.Enable(true, m_orient);
  m_objFlags &= 0x7fffffff;
  m_trackTime = 0.0f;
  if (m_curAnim && anim && !restart) AnimStartBlendIn(anim);
  else if (!anim) KillAllAnimTracks();
  else AnimStartImmediate(anim);
  m_curAnim = anim;
}

// 8004E15C
// Starts the animation `def` asks for (StartNewAnimTrack, inlined), then runs its time in
// milliseconds at the simulator's speed through the animation's events; at its end it loops (events
// again from the start), holds or stops as the track says.
void ISimsObjectModel::UpdateAnimTrack(ObjAnimDef* def, bool restart) {
  const float speed = m_paused ? 0.0f : SimulatorSpeed();
  StartNewAnimTrack(def, restart);
  ERAnim* anim = def->m_anim ? static_cast<ERAnim*>(_animman.GetRef(def->m_anim)) : nullptr;
  if (anim) {
    const double t = static_cast<double>(_dt * 1000.0f) * speed + m_animMsFraction;
    const unsigned int ms = static_cast<unsigned int>(t);
    m_animMsFraction = static_cast<float>(t) - static_cast<float>(static_cast<int>(ms));
    if (ms) {
      const int last = processAnimEvents(anim, m_animMs, static_cast<int>(ms), false);
      if (m_scriptEvents.empty()) m_animMs += static_cast<int>(ms);
      else m_animMs = last + 1;
      const int length = anim->m_durationMs;
      if (m_eventsLoop && length < m_animMs) {
        EACTrack* track = m_anim.m_mainTrack < 0 ? nullptr : &m_anim.m_tracks[m_anim.m_mainTrack];
        if (track) {
          m_scriptEvents.clear();
          if (track->m_endMode == 1) {
            m_animMs = length;
            m_animDone = 1;
          } else if (track->m_endMode == 2) {
            m_animDone = 1;
          } else {
            m_animMs -= length;
            processAnimEvents(anim, 0, m_animMs, false);
          }
        }
      }
    }
  }
  m_trackTime += _dt;
  m_anim.m_speed = speed;
  UpdateTracks();
}

namespace {
// The highlight pulse (GC: _hlphase[2], _hlfreq 0.2, _hlpulselen 0.7, _hllastframe), shared by every
// object: each player's phase runs once a frame.
float g_hlPhase[2] = {};
const float g_hlFreq = 0.2f;
const float g_hlPulseLen = 0.7f;
int g_hlLastFrame = 0;
// _hlcolor / _hlcolor2 (80485424 / 80485430, set by the unit's static initialiser): player 1's
// highlight (1, 1, 0.65), player 2's (1.1, 0.65, 0.65).
const float g_hlColor[2][3] = {{1.0f, 1.0f, 0.65f}, {1.1f, 0.65f, 0.65f}};
const float g_hlStartAmp = 0.6f;       // _hlstartamp (804FDA60)
const float g_hlStartLen = 0.35f;      // _hlstartlen
const float g_hlPulseAmp = 0.0f;       // _hlpulseamp (0 on the disc: no pulse after the start)
const float g_hlMinScaleStart = 1.3f;  // _hlminscalestart
const float g_hlMinScalePulse = 1.2f;  // _hlminscalepulse
}  // namespace

// DAT_80475E98 (livemode.cpp): the player whose view is being drawn (ELiveMode::DrawMain sets it before
// the house draws); still the last drawn player while the objects update.
extern int g_livemode80475E98;

// 8004DDEC
// Advances the shared pulse phases once a frame; each player's highlight (cursor flag 1 for player 1,
// 8 for player 2) times how long it has been on, starting at the middle of a pulse.
void ISimsObjectModel::UpdateHighlightAnim() {
  if (_framecount != g_hlLastFrame) {
    g_hlLastFrame = _framecount;
    const float step = _dt * g_hlFreq;
    for (float& phase : g_hlPhase) {
      phase += step;
      while (phase > 1.0f) phase -= 1.0f;
    }
  }
  const int g_hlPlayer = g_livemode80475E98;
  if (g_hlPlayer == 0 && (m_cursFlags & kHighlight)) {
    if (m_hlTime[0] == 0.0f) g_hlPhase[0] = g_hlPulseLen * 0.5f;
    m_hlTime[0] += _dt * g_hlFreq;
  } else {
    m_hlTime[0] = 0.0f;
  }
  if (g_hlPlayer == 1 && (m_cursFlags & kHighlight2)) {
    if (m_hlTime[1] == 0.0f) g_hlPhase[1] = g_hlPulseLen * 0.5f;
    m_hlTime[1] += _dt * g_hlFreq;
  } else {
    m_hlTime[1] = 0.0f;
  }
}

// 800544D0
void ISimsObjectModel::UpdateModel(ObjAnimDef* def) {
  if (m_curModel != def->m_model) {
    m_curModel = def->m_model;
    if (m_curModel) SetSOMModel(m_curModel);
  }
}

// 8004E618
// The object's state shows another shader in sub-model shader `slot` (e.g. a TV's screen, a lit lamp).
void ISimsObjectModel::ChageShader(unsigned int slot, unsigned int shader) {
  if (!m_model) return;
  if (m_stateShader) {
    m_stateShader->DelRef();
    m_stateShader = nullptr;
  }
  if (shader) {
    m_stateShader = static_cast<ERShader*>(_shaderman.AddRef(shader, nullptr, 0));
    ChangeShader(slot, shader);
  }
}

// 8005450C
void ISimsObjectModel::UpdateShader(ObjAnimDef* def) {
  if (m_curShader != def->m_shader && def->m_shader) {
    m_curShader = def->m_shader;
    m_curShaderSlot = def->m_shaderSlot;
    ChageShader(def->m_shaderSlot, def->m_shader);
  }
}

// 8004CBDC
// Once a frame (the level's update tree): follows its cXObject. The same state (data 0) keeps its
// animation running (restarted when the simulation tagged the graphics); a new state takes the new
// ObjAnimDef's model, particles, animation, shader and lamp. Then the particle effects follow the
// object's hidden flag, the highlight pulses, and objects in a slot or carried follow their holder
// (objects held by a sim take the sim's hand position: cursor flag 0x40, then CalcOrient /
// CalcDynamicOrient; not ported: an object in another object's slot). Vehicles last: their bounds from
// the pose and their shadow on the car body.
void ISimsObjectModel::Update() {
  if ((m_objFlags & 0x10000000u) || !m_xob) return;
  m_dataDir = m_xob->GetData(1);
  // A vehicle keeps its shadow's previous orientation (EIStaticModel +0xC0) for SlamOrient below.
  const bool vehicle = m_xob->IsVehicle() != 0;
  EMat4 shadowPrev;
  if (vehicle && m_shadow) shadowPrev = m_shadow->m_orient;
  if (m_xob->GetContainer())
    SIMMY_STUB("ISimsObjectModel::Update: carried objects (the holder's hand position)", 0x8004CBDC);
  const int state = m_xob->GetData(0);
  ObjAnimDef* def;
  if (m_state == state) {
    def = GetAnimDef(m_state, true);
    const bool restart = m_xob->HaveGraphicsBeenTagged() && def->m_anim;
    UpdateAnimTrack(def, restart);
  } else {
    m_paused = 0;
    def = GetAnimDef(state, true);
    UpdateModel(def);
    UpdateParticle(def);
    UpdateAnimTrack(def, false);
    UpdateShader(def);
    if (m_bulb) UpdateBulb(def);
    m_state = state;
  }
  // The effects go while the object is hidden (data 0x22) and come back (its state's attachment)
  // once it is shown and not animating.
  if (m_particles.empty()) {
    if ((m_objFlags & 0x40000000u) && (!m_xob || m_xob->GetData(0x22) == 0) && def->m_anim == 0) {
      ClearParticles(m_particles);
      m_objFlags &= ~0x40000000u;
      if (m_curAttach) {
        auto* obj = new EParticleObj;
        EMat4 m;
        GetOrient(m);
        ObjAnimDef cur;
        cur.m_model = m_curModel;
        cur.m_field04 = m_curAttach;
        if (obj->CreateEffects(m, 0.0f, &cur)) m_particles.push_back(obj);
        else delete obj;
      }
    }
  } else if (!(m_objFlags & 0x40000000u) && m_xob && m_xob->GetData(0x22) != 0) {
    m_objFlags |= 0x40000000u;
    ClearParticles(m_particles);
  }
  UpdateHighlightAnim();
  if (m_cursFlags & 0x40) {
    // Carried (cursor flags 0x40, in a holder 0x80): in the holder's hand (data 2), facing as it does
    // plus its own direction; with 0x100 turned with the hand.
    bool dynamic = false;
    cXObjectImpl* self = m_xob->GetObjectImplementation();
    cXObject* holder = self && Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(self->GetData(2)) : nullptr;
    cXObjectImpl* impl = holder ? holder->GetObjectImplementation() : nullptr;
    if (impl && (m_cursFlags & 0x80)) {
      if (impl->GetType() != 2) {
        SIMMY_STUB("ISimsObjectModel::Update: in an object's slot (the container's instance position)", 0x8004CBDC);
      } else if (SAnimator2* animator = static_cast<cXPersonImpl*>(impl)->m_animator) {
        EVec3 pos, dir;
        EMat4 hand;
        animator->GetCarryHandPosAndDir(pos, dir, hand);
        m_position[0] = pos.x, m_position[1] = pos.y, m_position[2] = pos.z;
        EVec3 flip(1.0f, 1.0f, -1.0f);
        hand.PreScale(flip);
        SetOrient(hand);
        m_rot = static_cast<float>(m_xob->GetData(1)) * 3.1415927f * 0.25f + simmy::dm::atan2(dir.y, dir.x);
        if (m_cursFlags & 0x100) dynamic = true;
      }
    }
    if (dynamic) CalcDynamicOrient();
    else CalcOrient();
  }
  if (vehicle) {
    // A vehicle (the carpool, the bus) drives by animation: its bounds are the box around its posed
    // nodes and its sphere the box's; its shadow follows the car body (node 0), moved along the sun's
    // direction on the ground.
    EMat4 orient;
    GetOrient(orient);
    float box[6];
    m_anim.CalcTightBoundBox(orient, *reinterpret_cast<EBound3*>(box), nullptr);
    SetBounds(*reinterpret_cast<EBound3*>(box));
    reinterpret_cast<EBound3*>(box)->CalcBoundSphere(*reinterpret_cast<EBoundSphere*>(m_sphere));
    if (m_shadow) {
      EMat4 body = *m_anim.GetNodeMatrix(0);
      if (ERLevel* level = GetLevel()) {
        body.m[3][0] += level->m_sun.m_dir.x;
        body.m[3][1] += level->m_sun.m_dir.y;
      }
      body.m[3][2] = 0.0f;
      m_shadow->SlamOrient(body, shadowPrev);
    }
  }
}

// 8004C7F0
// The height of an object on a person (data 2 its holder): the holder's bone 0 height (its ESim's
// person's animator, GetBonePos), when above 0.001, at most 1; else 0. None for a sub-object (flag
// 0x10000000). PC: the holder must be a person with an animator (GC reads its instance as an ESim).
double ISimsObjectModel::GetHeightOffset() {
  if (m_objFlags & 0x10000000u) return 0.0;
  cXObjectImpl* self = m_xob ? m_xob->GetObjectImplementation() : nullptr;
  cXObject* holder = self && Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(self->GetData(2)) : nullptr;
  cXObjectImpl* impl = holder ? holder->GetObjectImplementation() : nullptr;
  if (!impl || !impl->m_instance || impl == self) return 0.0;
  auto* person = impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
  SAnimator2* animator = person ? person->m_animator : nullptr;
  if (!animator) return 0.0;
  EVec3 bone(0.0f, 0.0f, 0.0f);
  animator->GetBonePos(0, bone);
  const double z = bone.z;
  if (!(0.0010000000474974513 < z)) return 0.0;
  if (z < -1.0) return -1.0;
  return z <= 1.0 ? z : 1.0;
}

// 80050C38
// A portal's side (ApplyMatrix): `data` is the room there; the second room its lighting group (the
// room's +2) when it is a room of the manager (not 0xFFFB and up).
void PortalRoom(unsigned int data, bool, short& room, short& level) {
  const unsigned short id = static_cast<unsigned short>(data);
  room = level = static_cast<short>(id);
  if (Globs_pRoomManager && id < 0xfffb)
    if (Room* r = Globs_pRoomManager->GetRoom(id)) level = static_cast<short>(r->m_lightingGroup);
}

// 8004E710
// The rooms of an object's instance: its room (data 0x1D) and that room's lighting group, the level's
// room. PC: `data` is the object's id (GC: the cXObject pointer, which does not fit the 32-bit slot).
void ThisSide(unsigned int data, bool, short& room, short& level) {
  cXObject* object = cXObjectImpl::GetObjectFromID(static_cast<short>(data));
  const unsigned short id = object ? static_cast<unsigned short>(object->GetData(0x1D)) : 0;
  room = level = static_cast<short>(id);
  if (Globs_pRoomManager && id < 0xfffb) {
    if (Room* r = Globs_pRoomManager->GetRoom(id)) level = static_cast<short>(r->m_lightingGroup);
  }
}

// 8004E6A0
// The rooms on the other side of a portal's instance: the room behind its wall part and that room's
// lighting group. PC: `data` is the object's id (GC: the instance pointer).
void OtherSide(unsigned int data, bool, short& room, short& level) {
  cXObject* object = cXObjectImpl::GetObjectFromID(static_cast<short>(data));
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  auto* model = impl ? dynamic_cast<ISimsObjectModel*>(impl->m_instance) : nullptr;
  const unsigned short id = model ? static_cast<unsigned short>(model->GetAdjacentRoom(false)) : 0xffff;
  room = level = static_cast<short>(id);
  if (Globs_pRoomManager && id < 0xfffb) {
    if (Room* r = Globs_pRoomManager->GetRoom(id)) level = static_cast<short>(r->m_lightingGroup);
  }
}

// 8004E794
// Hidden while the object's hidden flag (data 0x22) is set. PC: `data` is the object's id.
bool Hidden(unsigned int data) {
  cXObject* object = cXObjectImpl::GetObjectFromID(static_cast<short>(data));
  return object && object->GetData(0x22) != 0;
}

// 8004EA54
// Objects of types 4, 0xC, 0xE and 0xF with model data cast a shadow (their own model, posed by their
// controller) when one of their shaders is not excluded from shadows (surface flags 0x401): the room
// lists them (0x20000) for the level's shadow pass.
void ISimsObjectModel::CreateShadow() {
  m_shadow = nullptr;
  if (!m_model) return;
  bool casts = false;
  for (DigestGeomShaderPtr& d : m_digests) {
    EShader* shader = d.m_shader ? d.m_shader->GetShader() : nullptr;
    if (shader && !(shader->m_surface & 0x401)) {
      casts = true;
      break;
    }
  }
  if (!casts || !m_xob) return;
  const ObjDefinition* def = m_xob->GetDef();
  if (!def) return;
  const short type = objdef::Type(def);
  if ((type == 4 || type == 0xc || type == 0xe || type == 0xf) && objdef::ResData(def)) {
    m_animController = &m_anim;
    m_shadow = this;
    m_flags |= 0x20000;
  }
}

// 80053D9C
EIStaticModel* ISimsObjectModel::GetShadow() { return m_shadow; }

// 8004EB70
// Upright at m_position, turned to m_rot (half a turn more: the model's front). GC: the shadow
// (+0x44C) follows 0.1 higher. PC: the level is told it moved (its bounds tree), as ApplyMatrix does.
void ISimsObjectModel::CalcOrient() {
  EMat4 m;
  m.RotateZ(m_rot);
  EVec3 pos(m_position[0], m_position[1], m_position[2]);
  m.PostTranslate(pos);
  EVec3 flip(-1.0f, -1.0f, 1.0f);
  m.PreScale(flip);
  SetOrient(m);
  if (m_level) m_level->NotifyInstanceMoved(this);
}

// 8004EC78
// The orientation stays as set (the hand's); GC: only the shadow (+0x44C) follows, 0.1 higher. PC:
// the level is told it moved.
void ISimsObjectModel::CalcDynamicOrient() {
  if (m_level) m_level->NotifyInstanceMoved(this);
}

// 8004E538
void ISimsObjectModel::UpdateAnim(ObjAnimDef* def) { UpdateAnimTrack(def, false); }

// 8004FBA0
// Off screen, without a model, or hidden (its cXObject or the hidden callback): 1 (culled).
unsigned int ISimsObjectModel::VisibilityTest(E3DWindow& win) {
  unsigned int vis = win.Test(*reinterpret_cast<EBound3*>(m_bounds));
  if (!(vis & 1)) {
    if (IsVisible() && !(m_hiddenFn && m_hiddenFn(m_hiddenData))) return vis;
    vis = 1;
  }
  return vis;
}

// 8004FC24
// Not without a model, nor while its object's data 0x22 (hidden) is set.
int ISimsObjectModel::IsVisible() {
  if (!m_model) return 0;
  if (m_xob && m_xob->GetData(0x22) != 0) return 0;
  return 1;
}

// 8004FC8C
// GC: objects a sim is animating through SAnimator2 (cXObject type 0xC with animations pending) are
// left to it.
void ISimsObjectModel::UpdateAnimController() {
  if (m_xob) SIMMY_STUB("ISimsObjectModel::UpdateAnimController: SAnimator2 check", 0x8004FC8C);
  m_anim.Update(nullptr, nullptr, EVec3{1.0f, 1.0f, 1.0f});
}

// 8004FD38
// The node matrices for this frame; with hidden nodes, the records of their sub-models are marked.
void ISimsObjectModel::DoAnimation() {
  if (!m_anim.NodeCount()) {
    if (!m_animLights.empty()) AnimateLights();
    return;
  }
  m_anim.Compute(m_orient, _framecount);
  if (!(m_anim.m_nodeFlags & EACNodeState::kHidden)) {
    for (DigestGeomShaderPtr& d : m_digests) d.m_nodeFlags &= 0xfff7;
  } else {
    for (DigestGeomShaderPtr& d : m_digests) {
      unsigned short f = d.m_nodeFlags & 0xfff7;
      if (d.m_node != -1) f |= m_anim.m_states[d.m_node].m_flags & EACNodeState::kHidden;
      d.m_nodeFlags = f;
    }
  }
  if (!m_animLights.empty()) AnimateLights();
}

// 80050288
// 80050288
// The model's lights follow their nodes: a point light at its node's position (and the falloff of
// its scalar 1 channel), a directional light along its node's z axis; colour (vector channel) and
// intensity (scalar 0) when animated; in the object's room, set up again.
void ISimsObjectModel::AnimateLights() {
  EACNodeState* states = m_anim.m_states;
  if (!states || !m_model) return;
  for (size_t i = 0; i < m_animLights.size() && i < m_model->m_lightInfo.size(); ++i) {
    EILight* light = m_animLights[i];
    const unsigned char* li = m_model->m_lightInfo[i].data();
    const short type = static_cast<short>(li[0] << 8 | li[1]);
    const int node = static_cast<int>(static_cast<unsigned int>(li[4]) << 24 | static_cast<unsigned int>(li[5]) << 16 |
                                      static_cast<unsigned int>(li[6]) << 8 | li[7]);
    if (node < 0 || node >= m_anim.NodeCount()) continue;
    EACNodeState& st = states[node];
    if (type == 0) {
      auto* p = static_cast<EIPointLight*>(light);
      p->SetPosition(st.m_trans);
      if (st.m_flags & EACNodeState::kHasScalar1) {
        p->m_falloffStart = st.m_scalar1;
        p->m_falloffEnd = st.m_scalar1 + st.m_scalar1;
      }
    } else if (type == 1) {
      EMat4 m;
      m_anim.CalcNodeOrient(node, m);
      auto* d = static_cast<EIDirLight*>(light);
      m.GetColumn(2, d->m_dir);
    }
    if (st.m_flags & EACNodeState::kHasVector) light->m_color = st.m_vector;
    if (st.m_flags & EACNodeState::kHasScalar0) light->m_intensity = st.m_scalar0;
    light->m_thisSideData = static_cast<unsigned short>(m_xob->GetID());  // PC: the object's id (see ThisSide)
    light->m_thisSideFn = ThisSide;
    light->Init();
  }
}

// 8004F8D8
// The lights at its bound sphere (both sides for portal objects); sky objects take the sky lights.
// Placement errors light it red; highlighted objects blend in the highlight.
void ISimsObjectModel::DoLightingCalculation() {
  if (m_cursFlags & kErrorLight) {
    // ISimInstance::_ERRORLightCur (8048543C, set up in ISimInstance::ISimInstance): an ambient of
    // (2, 0, 0) and no lights, so the model shows solid red.
    ELights error;
    error.m_ambient[0] = 2.0f;
    m_lights = error;
    if (m_shaderFlags) m_otherSideLights = error;
    return;
  }
  if (m_digests.empty() || !(m_digests[0].m_flags & 0x2000)) {
    CalcLights(*reinterpret_cast<EVec3*>(m_sphere), m_lights, false);
    if (m_shaderFlags) CalcLights(*reinterpret_cast<EVec3*>(m_sphere), m_otherSideLights, true);
  } else {
    m_lights = m_level->m_skyLights;
    if (m_shaderFlags) m_otherSideLights = m_level->m_skyLights;
  }
  // GC: DAT_80476124 (never set) would turn the highlights off. Only the drawing player's highlight.
  bool on[2] = {g_livemode80475E98 == 0 && (m_cursFlags & kHighlight) != 0,
                g_livemode80475E98 == 1 && (m_cursFlags & kHighlight2) != 0};
  if (on[0] || on[1]) BlendHighlights(&m_lights, on);
}

namespace {
float Mag3(const float* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
}  // namespace

// 8004F02C
// The highlight: a flash in the player's colour as it comes on (_hlstartamp, fading over _hlstartlen of
// the highlight's time, +0x3F8) or the shared pulse (_hlpulseamp, 0 on the disc), each scaled by the
// lights' total brightness (at least _hlminscale*). The blend (at most 1) takes the ambient toward the
// highlight colour and the directional and point lights toward black.
void ISimsObjectModel::BlendHighlights(ELights* lights, bool* players) {
  ELights out;
  out.m_numDir = lights ? lights->m_numDir : 0;
  out.m_numPoint = lights ? lights->m_numPoint : 0;
  for (int i = 0; i < static_cast<int>(out.m_numDir); ++i)
    for (int k = 0; k < 3; ++k) out.m_dir[i].m_dir[k] = lights->m_dir[i].m_dir[k];
  for (int i = 0; i < static_cast<int>(out.m_numPoint); ++i) {
    for (int k = 0; k < 3; ++k) out.m_point[i].m_pos[k] = lights->m_point[i].m_pos[k];
    out.m_point[i].m_range = lights->m_point[i].m_range;
  }
  float total = Mag3(lights->m_ambient);
  for (int i = 0; i < static_cast<int>(lights->m_numDir); ++i) total += Mag3(lights->m_dir[i].m_color);
  for (int i = 0; i < static_cast<int>(lights->m_numPoint); ++i) total += Mag3(lights->m_point[i].m_color);
  float blend = 0.0f;
  float target[3] = {0.0f, 0.0f, 0.0f};
  for (int p = 0; p < 2; ++p) {
    if (!players[p]) continue;
    const float start = m_hlTime[p] < g_hlStartLen
                            ? std::max(total, g_hlMinScaleStart) * g_hlStartAmp * (1.0f - m_hlTime[p] / g_hlStartLen)
                            : 0.0f;
    const float pulseScale = std::max(total, g_hlMinScalePulse) * g_hlPulseAmp;
    const float pulse = g_hlPhase[p] < g_hlPulseLen
                            ? (pulseScale + pulseScale) * (0.5f - std::fabs(0.5f - g_hlPhase[p] / g_hlPulseLen))
                            : 0.0f;
    const float v = std::max(pulse, start);
    blend += v;
    for (int k = 0; k < 3; ++k) target[k] += g_hlColor[p][k] * v;
  }
  if (blend > 1.0f) blend = 1.0f;
  for (int k = 0; k < 3; ++k) out.m_ambient[k] = lights->m_ambient[k] + (target[k] - lights->m_ambient[k]) * blend;
  for (int i = 0; i < static_cast<int>(out.m_numDir); ++i)
    for (int k = 0; k < 3; ++k) out.m_dir[i].m_color[k] = lights->m_dir[i].m_color[k] + (0.0f - lights->m_dir[i].m_color[k]) * blend;
  for (int i = 0; i < static_cast<int>(out.m_numPoint); ++i)
    for (int k = 0; k < 3; ++k)
      out.m_point[i].m_color[k] = lights->m_point[i].m_color[k] + (0.0f - lights->m_point[i].m_color[k]) * blend;
  *lights = out;
}

// 8004FE28
// Like EIStaticModel::Draw, with records of hidden nodes skipped, the other side's lights for
// portal shaders (0x20), and the controller's matrices while a track runs.
void ISimsObjectModel::Draw(ELevelDrawData& data) {
  if (m_skipInShadowPass && m_level->m_wallFadeState == 2) return;
  const float alpha = GetInstAlpha();
  bool selected = false;
  for (DigestGeomShaderPtr& d : m_digests) {
    if (d.m_node != -1 && (d.m_nodeFlags & 8)) continue;
    ELights* lights = (d.m_flags & 0x20) ? &m_otherSideLights : &m_lights;
    if (alpha < 1.0f || (d.m_flags & 0x8000)) {
      EOrderTableData otd;
      EShader* shader = d.m_shader ? d.m_shader->GetShader() : nullptr;
      otd.m_sortKey = shader ? static_cast<short>(shader->m_field02) : 0;
      otd.m_pos = m_sphere;
      otd.m_lights = lights;
      otd.m_callback = AnimOrderTableCallback;
      otd.m_dl = d.m_dl;
      otd.m_shader = d.m_shader;
      otd.m_instance = this;
      if (m_trackMode == kTracksOff) {
        otd.m_matrix = GetDrawMatrix();
      } else {
        otd.m_animController = &m_anim;
        otd.m_skinned = m_skinned;
      }
      InsertInOrderTable(otd);
      continue;
    }
    if (!selected) {
      selected = true;
      if (m_trackMode == kTracksOff) {
        EMat4* matrix = GetDrawMatrix();
        if (data.m_curMatrix != matrix) {
          data.m_curMatrix = matrix;
          data.m_curAnim = nullptr;
          data.m_rc->ModelMatrices(matrix, 1);
        }
      } else if (data.m_curAnim != &m_anim) {
        data.m_curMatrix = nullptr;
        data.m_curAnim = &m_anim;
        const int count = m_anim.NodeCount();
        if (!m_skinned) data.m_rc->ModelMatrices(m_anim.m_matrices, count);
        else data.m_rc->ModelMatrixList(m_anim.m_matrices, count);
      }
    }
    if (data.m_curLights != lights) {
      data.m_curLights = lights;
      data.m_rc->Lights(lights);
    }
    if (data.m_curShader != d.m_shader) {
      data.m_curShader = d.m_shader;
      if (EShader* shader = d.m_shader ? d.m_shader->GetShader() : nullptr) shader->Select(data.m_rc, 0);
    }
    data.m_rc->DisplayList(d.m_dl);
  }
  if (!m_particles.empty()) {
    EMat4 m;
    GetOrient(m);
    const float rot = static_cast<float>(m_xob ? m_xob->GetData(1) : 0) * 3.1415927f * 0.25f;
    for (EParticleObj* obj : m_particles)
      obj->UpdateEffectPosAndAlphaFade(m_anim, rot, m, reinterpret_cast<EVec3*>(m_sphere), alpha);
  }
}

// 80050630
// The animation's notes from `from` for `count` ms (or to the end when count < 0; backwards from the
// end when reversed); footstep-like notes (type 5) fire 150 ms early. Returns the time of the first
// note that stopped the animation (a script event), else the end of the range.
int ISimsObjectModel::processAnimEvents(ERAnim* anim, int from, int count, bool reverse) {
  if (!anim) return reverse ? from - count : from + count;
  const std::vector<EAnimNote> notes = anim->m_notes;
  const int n = static_cast<int>(notes.size());
  if (!reverse) {
    const int end = from + count;
    int result = end;
    for (int i = 0; i < n; ++i) {
      int t = static_cast<int>(notes[i].m_time) - (notes[i].m_field04 == 5 ? 150 : 0);
      if (t < 0) t = 0;
      if (from <= t && (count < 0 || t < end) && animEventHandler(anim, const_cast<EAnimNote&>(notes[i]), i) &&
          t < result)
        result = static_cast<int>(notes[i].m_time);
    }
    return result;
  }
  const int start = anim->m_durationMs - from;
  const int end = start - count;
  int result = end;
  for (int i = n - 1; i >= 0; --i) {
    int t = static_cast<int>(notes[i].m_time) - (notes[i].m_field04 == 5 ? 150 : 0);
    if (t < 0) t = 0;
    if (t <= start && (count < 0 || end < t) && animEventHandler(anim, const_cast<EAnimNote&>(notes[i]), i) &&
        result < t)
      result = static_cast<int>(notes[i].m_time);
  }
  return result;
}

// 800507E8
// Type 6: a script event (its number queued for the simulation; stops the animation's time there).
// Type 5: a sound; 10: a particle effect attached to the object.
int ISimsObjectModel::animEventHandler(ERAnim*, EAnimNote& note, int) {
  const int type = static_cast<int>(note.m_field04);
  if (type == 6) {
    m_scriptEvents.push_back(std::atoi(note.m_text.c_str()));
    return 1;
  }
  if (type == 5) {
    if (m_animDone == 0 && Globs_pSound)
      Globs_pSound->PlayBySource(const_cast<char*>(note.m_text.c_str()), static_cast<short>(m_xob ? m_xob->GetID() : 0));
  } else if (type == 10) {
    // A particle event toggles its attachment (by name): off when one of the object's effects is
    // from it (and the state's effects stay off), else on.
    char* name = const_cast<char*>(note.m_text.c_str());
    if (EResource* att = g_effectsAttachmentMan.GetRef(name)) {
      for (auto it = m_particles.begin(); it != m_particles.end(); ++it)
        if ((*it)->m_attachment == att) {
          m_objFlags |= 0x40000000u;
          delete *it;
          m_particles.erase(it);
          return 0;
        }
    }
    m_objFlags &= ~0x40000000u;
    auto* obj = new EParticleObj;
    EMat4 m;
    GetOrient(m);
    m_curAttach = g_effectsAttachmentMan.CalcId(name);
    ObjAnimDef cur;
    cur.m_model = m_curModel;
    cur.m_field04 = m_curAttach;
    if (obj->CreateEffects(m, 0.0f, &cur)) m_particles.push_back(obj);
    else delete obj;
  }
  return 0;
}

// 80050CA0
// Without an object (the world map's scenery) the model is posed at once; with one, it is placed
// where the object is and takes the object's state. Then Init when it has a model.
void ISimsObjectModel::Create(cXObject* xob, EHouse* house) {
  m_xob = xob;
  m_house = house;
  if (!xob || !house) {
    DoAnimation();
  } else {
    SetObjOrient();
    SetInitalObjectState();
  }
  if (m_model) Init();
}

// 800504B8
// The ObjAnimDef for `state` in the object's definition (ObjectDefinition::ResData): the one whose key
// (+0x18) is the state, unless the table has no keys (-1: indexed by state); the first when there is
// none. PC: the quickdata row is copied out (host order) and the copy returned.
ObjAnimDef* ISimsObjectModel::GetAnimDef(int state, bool) {
  const unsigned char* defs = objdef::AnimDefs(objdef::ResData(m_xob->GetDef()));
  const int count = static_cast<int>(objdef::AnimDefCount(defs));
  int index = state;
  if (count > 0 && ERQuickdata::S32(defs, 0x18) != -1) {
    index = 0;
    bool found = false;
    for (int i = 0; i < count; ++i)
      if (ERQuickdata::S32(defs, static_cast<unsigned int>(i) * 0x20 + 0x18) == state) {
        index = i;
        found = true;
        break;
      }
    if (!found) index = 0;
  }
  if (index < 0 || index >= count) index = 0;
  m_animDefCopy = ObjAnimDef{};
  if (count > 0) {
    const unsigned char* d = defs + static_cast<unsigned int>(index) * 0x20;
    m_animDefCopy.m_model = ERQuickdata::U32(d, 0x00);
    m_animDefCopy.m_field04 = ERQuickdata::U32(d, 0x04);
    m_animDefCopy.m_anim = ERQuickdata::U32(d, 0x08);
    m_animDefCopy.m_shaderSlot = ERQuickdata::U32(d, 0x0c);
    m_animDefCopy.m_shader = ERQuickdata::U32(d, 0x10);
    for (unsigned int i = 0; i < 3; ++i) m_animDefCopy.m_field14[i] = ERQuickdata::U32(d, 0x14 + 4 * i);
  }
  return &m_animDefCopy;
}

// 8004DD1C
// The model (roofs skip the shadow pass); an animated object's controller re-poses with a new one.
// The controller poses in the model's units (its scale, +0x88).
void ISimsObjectModel::SetSOMModel(unsigned int model) {
  const unsigned int previous = m_modelId;
  SetModel(model, m_xob->IsRoof(), nullptr, nullptr);
  if (m_model) m_anim.m_scale = m_model->m_scale;  // GC +0x38C: the controller's scale
  if (previous != model && m_trackMode != kTracksOff && (m_objFlags & 0x80000000u)) {
    EMat4 orient;
    GetOrient(orient);
    m_anim.Enable(true, orient);
    m_anim.Enable(false, orient);
  }
}

// 80050B00
// The ObjAnimDef of the object's state (data 0): its model, character, lamp, particles, animation and
// shader change, then the pose.
void ISimsObjectModel::SetInitalObjectState() {
  cXObjectImpl* impl = m_xob->GetObjectImplementation();
  const int state = impl ? impl->m_data[0] : 0;
  if (m_state == state) return;
  ObjAnimDef* def = GetAnimDef(state, true);
  m_state = state;
  m_paused = 0;
  if (m_curModel != def->m_model) {
    m_curModel = def->m_model;
    if (m_curModel) SetSOMModel(m_curModel);
  }
  SetupCharacter();
  InitBulb();
  if (m_bulb) UpdateBulb(def);
  UpdateParticle(def);
  UpdateAnimTrack(def, false);
  UpdateShader(def);
  DoAnimation();
}

// 8004D6CC
// The character the object's definition names (ResData +0x20) for the animation controller; then a
// light per light in the model (point or directional, colour and falloff from its SimsLightInfo).
void ISimsObjectModel::SetupCharacter() {
  const unsigned char* res = objdef::ResData(m_xob->GetDef());
  if (res) {
    if (const unsigned int character = ERQuickdata::U32(res, 0x20))
      m_anim.Init(character, eAnimatedObjectType{1}, 0);
  }
  // The model's own lights (SimsLightInfo, 0x1C big-endian bytes each: type, colour, intensity, falloff):
  // point lights (type 0, range twice the falloff) and directional lights (type 1), switched on, in
  // the object's room, kept to be animated.
  if (m_model && !m_model->m_lightInfo.empty()) {
    for (const std::vector<unsigned char>& info : m_model->m_lightInfo) {
      if (info.size() < 0x1c) continue;
      const unsigned char* li = info.data();
      const short type = static_cast<short>(li[0] << 8 | li[1]);
      EILight* light = nullptr;
      if (type == 0) {
        auto* p = new EIPointLight;
        const float falloff = simmy::befloat(li + 0x18);
        p->m_falloffStart = falloff;
        p->m_useFalloff = 1;
        p->m_falloffEnd = falloff + falloff;
        EVec3 zero{0.0f, 0.0f, 0.0f};
        p->SetPosition(zero);
        light = p;
      } else if (type == 1) {
        auto* d = new EIDirLight;
        d->m_dir = EVec3{0.0f, 0.0f, 0.0f};
        light = d;
      }
      if (!light) continue;
      light->m_color = EVec3{simmy::befloat(li + 8), simmy::befloat(li + 0xc), simmy::befloat(li + 0x10)};
      light->m_useIntensityScale = 0;
      light->m_intensity = simmy::befloat(li + 0x14);
      light->m_enabled = 1;
      light->m_field88 = 1;
      light->m_field84 = 0;
      light->m_thisSideData = static_cast<unsigned short>(m_xob->GetID());  // PC: the object's id (see ThisSide)
      light->m_thisSideFn = ThisSide;
      if (m_house && m_house->m_level) m_house->m_level->InsertLight(light);
      m_animLights.push_back(light);
    }
  }
}

namespace {
float _newlightint = 1.1f;  // 804FDA40 a lamp's intensity
}  // namespace

// 8004D8A0
// A lamp (the definition's bulb, ResData +0x34): a point light (type 0) or a spot light (type 1, its
// angles in degrees, its direction turned with the object) at its offset on the object, its colour
// from the bulb's bytes, intensity _newlightint, switched off until its state says on; it follows
// the object when ResData +0x0C says so. In the object's room, in the level.
void ISimsObjectModel::InitBulb() {
  const unsigned char* res = objdef::ResData(m_xob->GetDef());
  const unsigned char* bulb = res ? static_cast<const unsigned char*>(ERQuickdata::Ptr(res, 0x34)) : nullptr;
  if (bulb) {
    EMat4 m;
    GetOrient(m);
    const int type = ERQuickdata::S32(bulb, 0);
    const EVec3 off{ERQuickdata::F32(bulb, 0xc), ERQuickdata::F32(bulb, 0x10), ERQuickdata::F32(bulb, 0x14)};
    if (type == 0) {
      auto* p = new EIPointLight;
      m_bulb = p;
      p->m_falloffStart = ERQuickdata::F32(bulb, 0x2c);
      p->m_falloffEnd = ERQuickdata::F32(bulb, 0x30);
      EVec3 pos{off.z * m.m[2][0] + off.x * m.m[0][0] + off.y * m.m[1][0] + m.m[3][0],
                off.z * m.m[2][1] + off.x * m.m[0][1] + off.y * m.m[1][1] + m.m[3][1],
                off.z * m.m[2][2] + off.x * m.m[0][2] + off.y * m.m[1][2] + m.m[3][2]};
      p->SetPosition(pos);
      p->m_enabled = 1;
      p->m_useFalloff = 1;
      p->m_falloffStart = ERQuickdata::F32(bulb, 0x2c);
      p->m_falloffEnd = ERQuickdata::F32(bulb, 0x30);
    } else if (type == 1) {
      auto* s = new EISpotLight;
      m_bulb = s;
      s->m_innerAngle = ERQuickdata::F32(bulb, 0x24) * 0.017453292f;
      s->m_outerAngle = ERQuickdata::F32(bulb, 0x28) * 0.017453292f;
      s->m_falloffStart = ERQuickdata::F32(bulb, 0x2c);
      s->m_falloffEnd = ERQuickdata::F32(bulb, 0x30);
      const EVec3 dir{ERQuickdata::F32(bulb, 0x18), ERQuickdata::F32(bulb, 0x1c), ERQuickdata::F32(bulb, 0x20)};
      s->m_dir = EVec3{dir.z * m.m[2][0] + dir.x * m.m[0][0] + dir.y * m.m[1][0],
                       dir.z * m.m[2][1] + dir.x * m.m[0][1] + dir.y * m.m[1][1],
                       dir.z * m.m[2][2] + dir.x * m.m[0][2] + dir.y * m.m[1][2]};
      const float len = std::sqrt(s->m_dir.x * s->m_dir.x + s->m_dir.y * s->m_dir.y + s->m_dir.z * s->m_dir.z);
      if (len != 0.0f) s->m_dir = EVec3{s->m_dir.x / len, s->m_dir.y / len, s->m_dir.z / len};
      s->m_pos = EVec3{off.z * m.m[2][0] + off.x * m.m[0][0] + off.y * m.m[1][0] + m.m[3][0],
                       off.z * m.m[2][1] + off.x * m.m[0][1] + off.y * m.m[1][1] + m.m[3][1],
                       off.z * m.m[2][2] + off.x * m.m[0][2] + off.y * m.m[1][2] + m.m[3][2]};
      s->m_enabled = 1;
      s->m_useFalloff = 1;
    }
    if (m_bulb) {
      m_bulb->m_color = EVec3{static_cast<float>(ERQuickdata::U8(bulb, 4)) / 255.0f,
                              static_cast<float>(ERQuickdata::U8(bulb, 5)) / 255.0f,
                              static_cast<float>(ERQuickdata::U8(bulb, 6)) / 255.0f};
      m_bulb->m_field88 = 0;
      m_bulb->m_intensity = _newlightint;
      m_bulb->m_field84 = ERQuickdata::U8(res, 0xc) ? 1 : 0;  // GC: this model (it follows it)
    }
  }
  if (m_bulb) {
    m_bulb->m_thisSideFn = ThisSide;
    m_bulb->m_thisSideData = static_cast<unsigned short>(m_xob->GetID());  // PC: the object's id (see ThisSide)
    if (m_house && m_house->m_level) m_house->m_level->InsertLight(m_bulb);
  }
}

// 8004DCE4
// The state's bulb setting switches the lamp (1: on).
void ISimsObjectModel::UpdateBulb(ObjAnimDef* def) {
  if (!def) return;
  const int state = static_cast<int>(def->m_field14[0]);
  if (m_bulbState == state) return;
  m_bulbState = state;
  m_bulb->m_field88 = state == 1 ? 1 : 0;
}

// (80052E3C / 8005127C) The lamp's light moves with the object: a point light to its offset (its
// grid tile and room re-checked), a spot light's position and direction; then set up again.
void ISimsObjectModel::PlaceBulb(EMat4& m) {
  const unsigned char* res = objdef::ResData(m_xob->GetDef());
  const unsigned char* bulb = res ? static_cast<const unsigned char*>(ERQuickdata::Ptr(res, 0x34)) : nullptr;
  if (!bulb) return;
  const int type = ERQuickdata::S32(bulb, 0);
  const EVec3 off{ERQuickdata::F32(bulb, 0xc), ERQuickdata::F32(bulb, 0x10), ERQuickdata::F32(bulb, 0x14)};
  const EVec3 pos{off.z * m.m[2][0] + off.x * m.m[0][0] + off.y * m.m[1][0] + m.m[3][0],
                  off.z * m.m[2][1] + off.x * m.m[0][1] + off.y * m.m[1][1] + m.m[3][1],
                  off.z * m.m[2][2] + off.x * m.m[0][2] + off.y * m.m[1][2] + m.m[3][2]};
  if (type == 0) {
    auto* p = static_cast<EIPointLight*>(m_bulb);
    EVec3 at = pos;
    p->SetPosition(at);
    p->Init();
  } else if (type == 1) {
    auto* s = static_cast<EISpotLight*>(m_bulb);
    s->m_pos = pos;
    const EVec3 dir{ERQuickdata::F32(bulb, 0x18), ERQuickdata::F32(bulb, 0x1c), ERQuickdata::F32(bulb, 0x20)};
    s->m_dir = EVec3{dir.z * m.m[2][0] + dir.x * m.m[0][0] + dir.y * m.m[1][0],
                     dir.z * m.m[2][1] + dir.x * m.m[0][1] + dir.y * m.m[1][1],
                     dir.z * m.m[2][2] + dir.x * m.m[0][2] + dir.y * m.m[1][2]};
    const float len = std::sqrt(s->m_dir.x * s->m_dir.x + s->m_dir.y * s->m_dir.y + s->m_dir.z * s->m_dir.z);
    if (len != 0.0f) s->m_dir = EVec3{s->m_dir.x / len, s->m_dir.y / len, s->m_dir.z / len};
    s->Init();
  }
}

// 80056324
void ISimInstance::SetObjOrient() {}

// 80056594
RET_UNKNOWN ISimInstance::GetIsPerson() {
  ObjSelector* selector = m_xob ? m_xob->GetSelector() : nullptr;
  return selector && selector->GetIsPerson() ? 1 : 0;
}

// 80050D44
// At its tile (the house's position added), turned by its direction (data 1, eighths of a turn).
void ISimsObjectModel::SetObjOrient() {
  const CTilePt tile = m_xob->GetCTilePt();
  const float angle = static_cast<float>(m_xob->GetData(1)) * 3.1415927f * 0.25f;
  m_rot = angle;
  EVec3 pos(static_cast<float>(tile.GetX()) + m_house->m_posY, static_cast<float>(tile.GetY()) + m_house->m_posX, 0.0f);
  EVec3 scale(1.0f, 1.0f, 1.0f);
  ApplyMatrix(angle, pos, scale);
}

// 80052E3C
// The orientation: scaled, turned by `angle` about z and moved to `pos` in tile space (x along the
// tile's x), then the axes swapped into the world's (x and y exchange). A portal (door, window) first
// takes the rooms half a tile either side of it along its turned y axis (ResolveRoomID) as its two
// sides (PortalRoom); every object notes whether its tile is outside (m_objFlags bit 29: no room, or an
// outside room) and may be drawn where the simulator overrides it (cXObjectImpl::OverrideRenderPosition).
// GC: also the height on a counter (data 4 bit 0x20); the shadow sub-instance follows (+0x44C).
void ISimsObjectModel::ApplyMatrix(float angle, EVec3& pos, EVec3& scale) {
  if (!m_model) return;
  if (IsPortalObject(m_xob)) {  // cXObjectImpl::IsPortal (800EAD18): type 8, a cXPortal
    EMat4 p;
    p.Id();
    p.PostRotateZ(angle);
    p.PostTranslate(pos);
    EVec3 plus(p.m[1][0] * 0.5f + p.m[3][0], p.m[1][1] * 0.5f + p.m[3][1], p.m[1][2] * 0.5f + p.m[3][2]);
    EVec3 minus(p.m[1][0] * -0.5f + p.m[3][0], p.m[1][1] * -0.5f + p.m[3][1], p.m[1][2] * -0.5f + p.m[3][2]);
    const CTilePt a(plus), b(minus);
    // CTilePt::ToFTilePt (800B4C60): y << 4 | 8, x << 4 | 8.
    const int fa[2] = {a.GetY() << 4 | 8, a.GetX() << 4 | 8};
    const int fb[2] = {b.GetY() << 4 | 8, b.GetX() << 4 | 8};
    const unsigned short thisRoom = static_cast<unsigned short>(ResolveRoomID(fa, 1));
    const unsigned short otherRoom = static_cast<unsigned short>(ResolveRoomID(fb, 1));
    m_otherSideData = otherRoom;
    m_thisSideData = thisRoom;
    m_otherSideFn = PortalRoom;
    m_thisSideFn = PortalRoom;
  }
  m_matrixAngle = angle;
  m_scale[0] = scale.x;
  m_scale[1] = scale.y;
  m_scale[2] = scale.z;
  EVec3 at = pos;
  {
    // Outside: the room of its tile (the house's position taken off; level 1; tiles 1..size-1).
    const float hx = m_house ? m_house->m_posY : 0.0f, hy = m_house ? m_house->m_posX : 0.0f;
    CTilePt tile(static_cast<int>(at.x - hx), static_cast<int>(at.y - hy), 1);
    cFixedWorldImpl* w = Globs_pFixedWorld;
    unsigned int roomId = 0;
    if (w && 0 < tile.GetX() && tile.GetX() <= w->GetXSize() - 1 && 0 < tile.GetY() && tile.GetY() <= w->GetYSize() - 1)
      roomId = w->GetRoom(tile);
    Room* room = Globs_pRoomManager ? Globs_pRoomManager->GetRoom(static_cast<unsigned short>(roomId)) : nullptr;
    if (!room) m_objFlags |= 0x20000000u;
    else m_objFlags = (room->IsOutside() ? 0x20000000u : 0u) | (m_objFlags & ~0x20000000u);
  }
  if (cXObjectImpl* impl = m_xob->GetObjectImplementation()) impl->OverrideRenderPosition(at.x, at.y);
  EMat4 m;
  m.Id();
  m.Scale(scale);
  m.PostRotateZ(angle);
  m.PostTranslate(at);
  for (auto& row : m.m) {
    const float t = row[1];
    row[1] = row[0];
    row[0] = t;
  }
  if (m_xob->GetData(4) & 0x20) {
    // Its holder (data 2) with an instance: it and what it holds down their slot 0 (GetContainedObject,
    // vtable 0x2C4) are drawn as carried (cursor flags 0xC0) and oriented again; its height is the
    // holder's (GetHeightOffset). PC guard: not when data 2 names the object itself.
    cXObject* holder = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(m_xob->GetData(2)) : nullptr;
    cXObjectImpl* holderImpl = holder && holder != m_xob ? holder->GetObjectImplementation() : nullptr;
    if (holderImpl && holderImpl->m_instance) {
      SetCursFlags(GetCursFlags() | 0xc0);
      cXObjectImpl* self = m_xob->GetObjectImplementation();
      for (cXObjectImpl* held = self ? self->GetContainedObject(0) : nullptr; held; held = held->GetContainedObject(0)) {
        if (ISimInstance* inst = held->m_instance) {
          inst->SetCursFlags(GetCursFlags() | 0xc0);
          inst->SetObjOrient();
        }
        if (held == self) break;  // PC guard
      }
    }
    m.m[3][2] = static_cast<float>(GetHeightOffset());
  }
  // In a container's slot (not a multi-tile object): the slot's offset (ObjectSlot +0x14 / +0x18, 1/16
  // tiles, in world axes: the descriptor's y along world x) turned by the container's facing, and its
  // height (+0x1C, altitude units: AltToWorld = x 3 / (2 sqrt 6)), summed up the container chain.
  cXObjectImpl* self = m_xob->GetObjectImplementation();
  cXObject* container = m_xob->GetContainer();
  static const bool slotOffsetOff = std::getenv("SIMMY_SLOT_OFFSET_OFF") != nullptr;  // PC dev: before/after shots
  if (container && self && !slotOffsetOff && !(self->m_def && objdef::MasterID(self->m_def) != 0)) {
    EVec3 offset(0.0f, 0.0f, 0.0f);
    float alt = 0.0f;
    cXObjectImpl* child = self;
    cXObjectImpl* parent = container->GetObjectImplementation();
    while (parent) {
      const unsigned char* slot = ObjectSlotDescriptor(parent, child->GetData(3));
      if (slot) {
        offset.y += ERQuickdata::F32(slot, 4) * 0.0625f;
        offset.x += ERQuickdata::F32(slot, 8) * 0.0625f;
        int height = 0;
        float slotAlt = 0.0f;
        ObjectSlotHeight(slot, height, slotAlt);
        alt += slotAlt;
        static const bool traceSlot = std::getenv("SIMMY_SLOT_TRACE") != nullptr;
        if (traceSlot)
          std::printf("[slot]   slot %d of %s: x %.2f y %.2f height %d alt %.3f (descriptor %.3f)\n", child->GetData(3),
                      std::string(parent->DebugName()).c_str(), ERQuickdata::F32(slot, 4), ERQuickdata::F32(slot, 8), height,
                      slotAlt, ERQuickdata::F32(slot, 0xc));
      }
      EMat4 r;
      r.Id();
      r.RotateZ(static_cast<float>(parent->GetData(1)) * 3.1415927f * -0.25f);
      offset = EVec3(offset.z * r.m[2][0] + offset.x * r.m[0][0] + offset.y * r.m[1][0] + r.m[3][0],
                     offset.z * r.m[2][1] + offset.x * r.m[0][1] + offset.y * r.m[1][1] + r.m[3][1],
                     offset.z * r.m[2][2] + offset.x * r.m[0][2] + offset.y * r.m[1][2] + r.m[3][2]);
      cXObject* up = parent->GetContainer();
      child = parent;
      parent = up ? up->GetObjectImplementation() : nullptr;
    }
    offset.z = alt * 3.0f / (2.0f * std::sqrt(6.0f));  // AltToWorld
    m.PostTranslate(offset);
    static const bool trace = std::getenv("SIMMY_SLOT_TRACE") != nullptr;
    if (trace)
      std::printf("[slot] %s in %s slot %d: at %d,%d (container %d,%d), offset %.2f %.2f %.2f\n", std::string(self->DebugName()).c_str(),
                  std::string(container->GetObjectImplementation()->DebugName()).c_str(), self->GetData(3), self->m_location[1] >> 4,
                  self->m_location[0] >> 4, container->GetObjectImplementation()->m_location[1] >> 4,
                  container->GetObjectImplementation()->m_location[0] >> 4, offset.x, offset.y, offset.z);
  }
  SetOrient(m);
  const unsigned char* res = objdef::ResData(m_xob->GetDef());
  if (res && ERQuickdata::U32(res, 0x20)) {
    // Animated models (a character): the bounds of the posed model (EIStaticModel::SetupBounds did
    // EBound3::Compute / CalcBoundSphere / SetBounds from the draw matrix), and the controller
    // re-enabled at the new place.
    m_anim.Enable(true, m);
    m_objFlags &= 0x7fffffff;
  }
  // GC: m_rot from the matrix (EMat4::GetHPR): the same angle.
  m_position[0] = m.m[3][0];
  m_position[1] = m.m[3][1];
  m_position[2] = m.m[3][2];
  if (!m_subModels.empty()) OrientSubObjects();
  if (m_bulb) {
    EMat4 orient;
    GetOrient(orient);
    PlaceBulb(orient);
  }
  if (m_level) m_level->NotifyInstanceMoved(this);
}

// ---------------------------------------------------------------------------------------------------
// ISimsWallObjectModel

// 80053F58 ISimsWallObjectModel::GetTypeInfo
// 80053F64 ISimsWallObjectModel::GetTypeName
// 80053F70 ISimsWallObjectModel::GetTypeKey
// 80053F7C ISimsWallObjectModel::GetTypeVersion
// 80053F18 ISimsWallObjectModel::SafeDelete
// 80053E80 ISimsWallObjectModel::New
// 80053ED4 ISimsWallObjectModel::Construct
// 80053EF4 ISimsWallObjectModel::Destruct
// 80053F88 ISimsWallObjectModel::GetTypeInfoStatic
// 80053F94 ISimsWallObjectModel::GetReadVersion
SIMMY_STORABLE_IMPL(ISimsWallObjectModel, "ISimsWallObjectModel", 0, ISimsObjectModel)

// 80051170
ISimsWallObjectModel::ISimsWallObjectModel() {}
// 800511B8
ISimsWallObjectModel::~ISimsWallObjectModel() {}
// 80051218
void ISimsWallObjectModel::CreateShadow() {}

// 8005121C
// The state first, then the placement.
void ISimsWallObjectModel::Create(cXObject* xob, EHouse* house) {
  m_xob = xob;
  m_house = house;
  SetInitalObjectState();
  SetObjOrient();
  if (m_model) Init();
}

namespace {
// cXMTObjectImpl::IsDynamic (800C71F0) of the object's part (the GC's SetObjOrients call it through
// the part's cXMTObject interface, vtable +0x44).
bool IsDynamicMultiTile(cXObject* xob) {
  auto* mt = xob ? dynamic_cast<cXMTObjectImpl*>(xob->GetObjectImplementation()) : nullptr;
  return mt && mt->IsDynamic();
}

// PC: the tile rectangle's middle of a multi-tile object's group (GC: cXMTObject's lead and next
// links, set as the group is read; EIObjTileBoundRect). The group: the objects of the same object
// file and multi-tile master with the same direction whose tiles are where their definitions' sub
// positions (+0x16: x << 8 | y), turned by the direction, put them from the group's lead tile.
void MultiTileGroupCenter(cXObject* xob, float& x, float& y) {
  const int dir = xob->GetData(1) & 7;
  auto turn = [dir](int dx, int dy, int& ox, int& oy) {
    switch (dir >> 1) {
      case 0: ox = dx, oy = dy; break;
      case 1: ox = -dy, oy = dx; break;
      case 2: ox = -dx, oy = -dy; break;
      default: ox = dy, oy = -dx; break;
    }
  };
  auto sub = [&](cXObject* o, int& ox, int& oy) {
    const unsigned short s = static_cast<unsigned short>(objdef::SubIndex(o->GetDef()));
    turn(static_cast<signed char>(s >> 8), static_cast<signed char>(s & 0xff), ox, oy);
  };
  const CTilePt tile = xob->GetCTilePt();
  int ox = 0, oy = 0;
  sub(xob, ox, oy);
  const int leadX = tile.GetX() - ox, leadY = tile.GetY() - oy;
  float minX = static_cast<float>(tile.GetX()) - 0.5f, maxX = minX + 1.0f;
  float minY = static_cast<float>(tile.GetY()) - 0.5f, maxY = minY + 1.0f;
  ObjSelector* selector = xob->GetSelector();
  const short master = objdef::MasterID(xob->GetDef());
  for (cXObjectImpl* o = Globs_pObjectModule ? Globs_pObjectModule->m_first : nullptr; o; o = o->m_nextObject) {
    if (o == xob || !o->m_selector || !selector || o->m_selector->m_resFile != selector->m_resFile ||
        objdef::MasterID(o->m_def) != master || (o->GetData(1) & 7) != dir)
      continue;
    sub(o, ox, oy);
    const CTilePt t = o->GetCTilePt();
    if (t.GetX() != leadX + ox || t.GetY() != leadY + oy || t.GetLevel() != tile.GetLevel()) continue;
    minX = std::min(minX, static_cast<float>(t.GetX()) - 0.5f);
    maxX = std::max(maxX, static_cast<float>(t.GetX()) + 0.5f);
    minY = std::min(minY, static_cast<float>(t.GetY()) - 0.5f);
    maxY = std::max(maxY, static_cast<float>(t.GetY()) + 0.5f);
  }
  x = (maxX - minX) * 0.5f + minX;
  y = (maxY - minY) * 0.5f + minY;
}
}  // namespace

// 8005127C
// Turned by its direction and pushed 0.45 tiles from the tile's middle toward the wall behind it
// (direction 0: +y, 2: -x, 4: -y, 6: +x); multi-tile wall objects use their tiles' middle (dynamic
// ones their own tile). GC: no push when the simulator overrides the render position; the lamp's light.
void ISimsWallObjectModel::SetObjOrient() {
  const unsigned int dir = static_cast<unsigned int>(m_xob->GetData(1));
  const float angle = static_cast<float>(static_cast<int>(dir)) * 3.1415927f * 0.25f;
  float x, y;
  if (!m_xob->IsMultiTile() || IsDynamicMultiTile(m_xob)) {
    const CTilePt tile = m_xob->GetCTilePt();
    x = static_cast<float>(tile.GetX());
    y = static_cast<float>(tile.GetY());
  } else {
    MultiTileGroupCenter(m_xob, x, y);
  }
  float dx = 0.0f, dy = 0.0f;
  if (dir == 2) dx = -0.45f;
  else if (dir == 4) dy = -0.45f;
  else if (dir == 6) dx = 0.45f;
  else if (dir == 0) dy = 0.45f;
  EMat4 m;
  m.Id();
  m.RotateZ(angle);
  EVec3 pos(x + m_house->m_posY + dx, y + m_house->m_posX + dy, 0.0f);
  m.PostTranslate(pos);
  for (auto& row : m.m) {
    const float t = row[1];
    row[1] = row[0];
    row[0] = t;
  }
  SetOrient(m);
  if (!m_subModels.empty()) OrientSubObjects();
  if (m_bulb) {
    EMat4 orient;
    GetOrient(orient);
    PlaceBulb(orient);
  }
}

// ---------------------------------------------------------------------------------------------------
// ISimsMultiTileObjectModel

// 800540EC ISimsMultiTileObjectModel::GetTypeInfo
// 800540F8 ISimsMultiTileObjectModel::GetTypeName
// 80054104 ISimsMultiTileObjectModel::GetTypeKey
// 80054110 ISimsMultiTileObjectModel::GetTypeVersion
// 800540AC ISimsMultiTileObjectModel::SafeDelete
// 80054014 ISimsMultiTileObjectModel::New
// 80054068 ISimsMultiTileObjectModel::Construct
// 80054088 ISimsMultiTileObjectModel::Destruct
// 8005411C ISimsMultiTileObjectModel::GetTypeInfoStatic
// 80054128 ISimsMultiTileObjectModel::GetReadVersion
SIMMY_STORABLE_IMPL(ISimsMultiTileObjectModel, "ISimsMultiTileObjectModel", 0, ISimsObjectModel)

// 80051A8C
ISimsMultiTileObjectModel::ISimsMultiTileObjectModel() {}
// 80051AD4
ISimsMultiTileObjectModel::~ISimsMultiTileObjectModel() {}

// 80051B34
void ISimsMultiTileObjectModel::Create(cXObject* xob, EHouse* house) {
  m_xob = xob;
  m_house = house;
  SetObjOrient();
  SetInitalObjectState();
  if (m_model) Init();
}

namespace {
bool IsWindowObject(cXObject* xob);
EIWallPart* PortalWallPart(cXObject* xob, EHouse* house);
}  // namespace

// 80051B94
// At the middle of its group's tiles (a dynamic multi-tile object: its own tile), turned by its
// direction: a door or window (two parts, one each side of the wall) is centred on the wall line, as
// its model is. Windows also take their wall (ERoom::GetWallFromTileAndSegment) and place their portal
// lights again.
void ISimsMultiTileObjectModel::SetObjOrient() {
  const float angle = static_cast<float>(m_xob->GetData(1)) * 3.1415927f * 0.25f;
  float x, y;
  if (!IsDynamicMultiTile(m_xob)) {
    MultiTileGroupCenter(m_xob, x, y);
  } else {
    const CTilePt tile = m_xob->GetCTilePt();
    x = static_cast<float>(tile.GetX());
    y = static_cast<float>(tile.GetY());
  }
  EVec3 pos(x + m_house->m_posY, y + m_house->m_posX, 0.0f);
  EVec3 scale(1.0f, 1.0f, 1.0f);
  ApplyMatrix(angle, pos, scale);
  // A window: its wall part again, and its two portal lights placed anew.
  if (IsPortalObject(m_xob) && IsWindowObject(m_xob)) {
    if (EIWallPart* wall = PortalWallPart(m_xob, m_house)) m_portalWall = wall;
    for (EILight* light : {m_light1, m_light2})
      if (light) light->Update();
  }
}

namespace {
// cXObjectImpl::IsWindow (800EAD98): a portal (type 8) without a tree for entry point 0xF (doors have one).
bool IsWindowObject(cXObject* xob) {
  cXObjectImpl* impl = xob ? xob->GetObjectImplementation() : nullptr;
  return impl && IsPortalObject(xob) && impl->GetTreeID(0xf) == 0;
}

// cXObjectImpl::GetRequiredSegment (800E2544; as objectplace.cpp): the wall placement flags (data
// 0xD: 1 the tile's segment 2, 4 its segment 8), turned by the direction (TileWalls::RotateSegment).
unsigned int RequiredSegment(cXObjectImpl* o) {
  const unsigned int f = static_cast<unsigned int>(o->GetData(0xd)) & 5;
  unsigned int seg = f == 1 ? 2 : f == 4 ? 8 : 0;
  const int quarters = (o->GetData(1) / 2) & 3;
  for (int i = 0; i < quarters && seg; ++i) seg = seg << 1 > 8 ? 1 : seg << 1;
  return seg;
}

// The wall part a portal sits in (SetWallForPortal / SetObjOrient): the wall segment it needs, or else
// the first wall of its tile in the portal's wall style, on its tile, unless the tile is out of bounds.
EIWallPart* PortalWallPart(cXObject* xob, EHouse* house) {
  if (!xob || !house || !house->m_room) return nullptr;
  cXObjectImpl* impl = xob->GetObjectImplementation();
  if (!impl) return nullptr;
  int seg = static_cast<int>(RequiredSegment(impl));  // cXObject vtable 0x2A4
  CTilePt tile = xob->GetCTilePt();
  if (!Globs_pFixedWorld || Globs_pFixedWorld->OutOfBounds(tile) == 1) return nullptr;
  if (!seg) {
    // cXPortalImpl::GetWallStyle (801327C8): the definition's +0x5C.
    const ObjDefinition* def = xob->GetDef();
    const int style = def ? ERQuickdata::S16(def, 0x5c) : -1;
    TileWalls walls = Globs_pFixedWorld->GetWall(tile);
    for (int s = walls.First(); s; s = walls.Next(TileWallsSegment{s})) {
      if (style == walls.GetStyle(TileWallsSegment{s})) {
        seg = s;
        break;
      }
    }
  }
  if (!seg) return nullptr;
  return house->m_room->GetWallFromTileAndSegment(TileWallsSegment{seg}, tile);
}
}  // namespace

// 8004E7DC
// The room behind a portal's wall (-1 without one); `find` looks the wall up again.
short ISimsObjectModel::GetAdjacentRoom(bool find) {
  if (find) m_portalWall = nullptr;
  if (!m_xob || !IsPortalObject(m_xob)) return -1;
  if (!m_portalWall) {
    SetWallForPortal();
    if (!m_portalWall) return -1;
  }
  return m_portalWall->m_room2;  // EIWallPart::GetAdjacentRoom (8002B99C)
}

// 8004E868
void ISimsObjectModel::SetWallForPortal() {
  m_portalWall = nullptr;
  if (!m_xob || !IsPortalObject(m_xob) || !m_house || !m_house->m_room) return;
  m_portalWall = PortalWallPart(m_xob, m_house);
}

// ---------------------------------------------------------------------------------------------------
// The animate-object primitive (0x37)

// 8004ED3C
int ISimsObjectModel::TryAnimateObject(short anim, short* event) {
  cXObjectImpl* xob = m_xob ? m_xob->GetObjectImplementation() : nullptr;
  if (m_preloadAnim) {
    if (xob && xob->GetData(0) == anim) {
      if (!_animman.GetRef(m_preloadAnim)) return 2;
      _animman.DelRef(m_preloadAnim, EResourceManager__DelRefMode{1});
    } else {
      _animman.DelRefAsync(m_preloadAnim);
      m_animatePrim = 0;
    }
    m_preloadAnim = 0;
  }
  m_animDone = 0;
  m_eventsLoop = 0;
  if (!m_animatePrim) {
    if (xob) xob->m_data.WriteVar(0, anim);  // GC: cXObject vtable 0x1EC SetData(0, anim)
    m_animMs = 0;
    m_animatePrim = 1;
  }
  if (!event) {
    if (!m_scriptEvents.empty()) m_scriptEvents.erase(m_scriptEvents.begin());
  } else {
    if (!m_scriptEvents.empty()) {
      *event = static_cast<short>(m_scriptEvents.front());
      m_scriptEvents.erase(m_scriptEvents.begin());
      m_paused = 1;
      return 0;
    }
    *event = -1;
  }
  m_paused = 0;
  ObjAnimDef* def = GetAnimDef(anim, false);
  const unsigned int id = def ? def->m_anim : 0;
  if (!id) {
    m_animatePrim = 0;
    return 1;
  }
  EResource* res = _animman.GetRef(id);
  if (!res) {
    m_preloadAnim = id;
    _animman.AddRefAsync(id);
  } else if (static_cast<ERAnim*>(res)->m_durationMs <= m_animMs) {
    m_animatePrim = 0;
    return 1;
  }
  return 2;
}

// 8004EF58
int ISimsObjectModel::PreloadAnimation(short anim) {
  ObjAnimDef* def = GetAnimDef(anim, false);
  const unsigned int id = def ? def->m_anim : 0;
  if (m_preloadAnim) {
    if (m_preloadAnim == id) {
      if (!_animman.GetRef(m_preloadAnim)) return 0;
      _animman.DelRef(m_preloadAnim, EResourceManager__DelRefMode{1});
      m_preloadAnim = 0;
      return 1;
    }
    _animman.DelRefAsync(m_preloadAnim);
    m_preloadAnim = 0;
  }
  if (!id || _animman.GetRef(id)) return 1;
  m_preloadAnim = id;
  _animman.AddRefAsync(id);
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// Sub-objects (a counter's sink)

// 80050EB4
// Each sub-object told of the object (ISimInstance::OrentSubObject(cXObject*), vtable 0x174: empty for
// the plain models a counter's sink is, so the sink stays where its counter's SetObjOrient put it).
void ISimsObjectModel::OrientSubObjects() {
  for (EInstance* sub : m_subModels)
    if (auto* model = dynamic_cast<ISimInstance*>(sub)) model->OrentSubObject(m_xob);
}

// 80050F14
// A sub-object at its own object's tile (the house's position added), turned by that object's
// direction (data 1, eighths of a turn), the axes swapped into the world's.
void ISimsObjectModel::OrentSubObject() {
  const float angle = static_cast<float>(m_xob->GetData(1)) * 3.1415927f * 0.25f;
  const CTilePt tile = m_xob->GetCTilePt();
  EMat4 m;
  m.Id();
  m.RotateZ(angle);
  EVec3 pos(static_cast<float>(tile.GetX()) + m_house->m_posY, static_cast<float>(tile.GetY()) + m_house->m_posX, 0.0f);
  m.PostTranslate(pos);
  for (auto& row : m.m) {
    const float t = row[1];
    row[1] = row[0];
    row[0] = t;
  }
  SetOrient(m);
  Init();
}

// ---------------------------------------------------------------------------------------------------
// ISimsCounterTopObject

// 80054280 ISimsCounterTopObject::GetTypeInfo
// 8005428C ISimsCounterTopObject::GetTypeName
// 80054298 ISimsCounterTopObject::GetTypeKey
// 800542A4 ISimsCounterTopObject::GetTypeVersion
// 80054240 ISimsCounterTopObject::SafeDelete
// 800541A8 ISimsCounterTopObject::New
// 800541FC ISimsCounterTopObject::Construct
// 8005421C ISimsCounterTopObject::Destruct
// 800542B0 ISimsCounterTopObject::GetTypeInfoStatic
// 800542BC ISimsCounterTopObject::GetReadVersion
SIMMY_STORABLE_IMPL(ISimsCounterTopObject, "ISimsCounterTopObject", 0, ISimsObjectModel)

// 800520AC
ISimsCounterTopObject::ISimsCounterTopObject() {}
// 800520F4
ISimsCounterTopObject::~ISimsCounterTopObject() {}

// 80052154
void ISimsCounterTopObject::Create(cXObject* xob, EHouse* house) {
  m_xob = xob;
  m_house = house;
  SetObjOrient();
  SetInitalObjectState();
}

// 800548E0
int ISimsCounterTopObject::IsSinkId(cXObject* xob) {
  const ObjDefinition* def = xob ? xob->GetDef() : nullptr;
  const unsigned char* res = def ? objdef::ResData(def) : nullptr;
  return res && res[7] != 0 ? 1 : 0;
}

const void* GetCounterModelTable();  // 80041758 (sprites.cpp)

namespace {
// PC mod: mods. A counter top at (x, y) on `level` other than `self` (ResData +6, as AllocSimsObjectInstance
// picks ISimsCounterTopObject): its direction (data 1) in `dir`.
bool CounterTopAt(int x, int y, int level, cXObject* self, int& dir) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w || x < 0 || y < 0 || x > w->GetXSize() || y > w->GetYSize()) return false;
  CTilePt tile(x, y, level);
  ObjectIterator it;
  it.init(tile, ObjectIterator__IterateType{0});
  for (; it.m_current; ++it) {
    if (it.m_current == self) continue;
    const ObjDefinition* def = it.m_current->GetDef();
    const unsigned char* res = def ? objdef::ResData(def) : nullptr;
    if (!res || res[6] != 1) continue;
    dir = it.m_current->GetData(1) & 7;
    return true;
  }
  return false;
}

// The tile one step from (x, y) in direction `dir` (0 -y, 2 +x, 4 +y, 6 -x: xrouting.cpp).
void StepTile(int dir, int& x, int& y) {
  switch (dir & 7) {
    case 0: --y; break;
    case 2: ++x; break;
    case 4: ++y; break;
    case 6: --x; break;
    default: break;
  }
}
}  // namespace

// PC mod: mods. The piece a mod counter (mod.ini connect, model_*) shows by its neighbours, as the PC's
// counter globals pick them: facing `dir` (its front toward StepTile(dir); the model's +x side toward
// dir + 2, its back +y toward the wall), a counter in front of it that faces along its side makes it a
// corner (the corner model closes the back and the -x side; one whose front neighbour faces the other
// way is the corner turned a quarter, drawn facing dir - 2); else an island or a counter with end
// pieces closes the sides with no counter next to them. `model` and `drawDir` are left alone when the
// object is no mod counter or shows its plain model.
bool ModCounterPiece(cXObject* xob, unsigned int& model, int& drawDir) {
  ObjSelector* sel = xob ? xob->GetSelector() : nullptr;
  const simmy::ModObject* m = sel ? sel->m_modObject : nullptr;
  if (!m || !m->connect) return false;
  const CTilePt tile = xob->GetCTilePt();
  const int level = tile.GetLevel(), dir = xob->GetData(1) & 7;
  if (dir & 1) return false;  // diagonal: plain
  static const bool trace = std::getenv("SIMMY_COUNTER_TRACE") != nullptr;
  int fx = tile.GetX(), fy = tile.GetY(), frontDir = -1;
  StepTile(dir, fx, fy);
  if (m->pieces[0] && CounterTopAt(fx, fy, level, xob, frontDir)) {
    if (frontDir == ((dir + 2) & 7) || frontDir == ((dir + 6) & 7)) {
      model = m->pieces[0];
      drawDir = frontDir == ((dir + 2) & 7) ? dir : (dir + 6) & 7;
      if (trace) std::printf("[counter] #%d at %d,%d dir %d: corner (front neighbour faces %d), drawn %d\n", xob->GetID(),
                             tile.GetX(), tile.GetY(), dir, frontDir, drawDir);
      return true;
    }
  }
  int px = tile.GetX(), py = tile.GetY(), nx = px, ny = py, unused = 0;
  StepTile(dir + 2, px, py);
  StepTile(dir + 6, nx, ny);
  const bool plus = CounterTopAt(px, py, level, xob, unused), minus = CounterTopAt(nx, ny, level, xob, unused);
  unsigned int piece = 0;
  if (!plus && !minus) piece = m->pieces[3];
  else if (!plus) piece = m->pieces[1];
  else if (!minus) piece = m->pieces[2];
  if (trace)
    std::printf("[counter] #%d at %d,%d dir %d: neighbours +x %d -x %d: %s\n", xob->GetID(), tile.GetX(), tile.GetY(), dir,
                plus, minus, piece ? (!plus && !minus ? "both ends" : !plus ? "end +x" : "end -x") : "plain");
  if (!piece) return false;
  model = piece;
  drawDir = dir;
  return true;
}

// 80052190
// The sub-objects of the last time go (out of the level, deleted). The model of its state's graphic
// (data 0; past the ObjAnimDefs: the first); the counter model table's row for its first ObjAnimDef's
// model. On the lot, the first sink among the objects at its tile (ObjectIterator) gets a plain model
// instance of the row's sink model at the sink's place (ISimsObjectModel::OrentSubObject), kept as a
// sub-object (flag 0x10000000: Update leaves it alone) and put in the level with the counter, and the
// counter takes the row's cut-out model. Then placed as a plain object (ISimsObjectModel::SetObjOrient).
// GC quirk kept: a sink on a counter whose model has no row reads a null row (PC: no cut-out, logged).
void ISimsCounterTopObject::SetObjOrient() {
  if (m_level) RemoveSubModelsFromHouse(m_level);
  for (EInstance* sub : m_subModels) delete sub;  // owned (+0x458)
  m_subModels.clear();
  const unsigned char* defs = objdef::AnimDefs(objdef::ResData(m_xob->GetDef()));
  const int count = static_cast<int>(objdef::AnimDefCount(defs));
  int graphic = m_xob->GetData(0);
  if (count <= graphic) graphic = 0;
  unsigned int model = ERQuickdata::U32(defs, static_cast<unsigned int>(graphic) * 0x20);
  // The table's one row holds the entry array (its count at -4).
  const void* table = GetCounterModelTable();
  const unsigned char* first = table ? ERQuickdata::TableRows(table) : nullptr;
  const unsigned char* rows = first ? static_cast<const unsigned char*>(ERQuickdata::Ptr(first, 0)) : nullptr;
  const int rowCount = rows ? static_cast<int>(ERQuickdata::U32(rows - 4, 0)) : 0;
  const unsigned char* row = nullptr;
  for (int i = 0; i < rowCount; ++i)
    if (ERQuickdata::U32(rows, static_cast<unsigned int>(i) * 0x10) == ERQuickdata::U32(defs, 0)) {
      row = rows + i * 0x10;
      break;
    }
  CTilePt tile = m_xob->GetCTilePt();
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w || tile.GetX() < 0 || tile.GetX() > w->GetXSize() || tile.GetY() < 0 || tile.GetY() > w->GetYSize()) {
    ISimsObjectModel::SetObjOrient();
    return;
  }
  ObjectIterator it;
  it.init(tile, ObjectIterator__IterateType{0});
  for (; it.m_current; ++it) {
    if (!IsSinkId(it.m_current)) continue;
    if (!row) {  // PC: GC reads the row's fields through a null pointer here
      static bool logged = false;
      if (!logged) std::printf("[counter] no ECntrMdlLkupTable row for model 0x%08x\n", ERQuickdata::U32(defs, 0));
      logged = true;
      break;
    }
    auto* sink = new ISimsObjectModel;  // GC: 0x4C0 bytes, 16-aligned, cleared, from the main heap
    sink->m_house = m_house;
    sink->SetXOb(it.m_current);
    sink->m_thisSideData = static_cast<unsigned short>(it.m_current->GetID());  // GC: the cXObject (PC: its id)
    sink->m_thisSideFn = ThisSide;
    sink->SetSOMModel(ERQuickdata::U32(row, 4));
    sink->OrentSubObject();
    m_subModels.push_back(sink);
    sink->m_objFlags |= 0x10000000u;
    if (m_level) InsertSubModelsInHouse(m_level);
    model = ERQuickdata::U32(row, 8);
    static const bool trace = std::getenv("SIMMY_BUILD_TRACE") != nullptr;  // PC dev aid
    if (trace)
      std::printf("[counter] #%d at %d,%d: sink #%d, sink model 0x%08x, cut-out 0x%08x\n", m_xob->GetID(), tile.GetX(),
                  tile.GetY(), it.m_current->GetID(), ERQuickdata::U32(row, 4), model);
    break;
  }
  // PC mod: a mod counter's piece by its neighbours (not with a sink in it: its own model)
  int drawDir = -1;
  if (m_subModels.empty()) ModCounterPiece(m_xob, model, drawDir);
  SetSOMModel(model);
  if (drawDir >= 0 && drawDir != (m_xob->GetData(1) & 7)) {
    const float angle = static_cast<float>(drawDir) * 3.1415927f * 0.25f;
    m_rot = angle;
    EVec3 pos(static_cast<float>(tile.GetX()) + m_house->m_posY, static_cast<float>(tile.GetY()) + m_house->m_posX, 0.0f);
    EVec3 scale(1.0f, 1.0f, 1.0f);
    ApplyMatrix(angle, pos, scale);
  } else {
    ISimsObjectModel::SetObjOrient();
  }
  Init();
}

// ---------------------------------------------------------------------------------------------------
// IShrubObject

// 80054414 IShrubObject::GetTypeInfo
// 80054420 IShrubObject::GetTypeName
// 8005442C IShrubObject::GetTypeKey
// 80054438 IShrubObject::GetTypeVersion
// 800543D4 IShrubObject::SafeDelete
// 8005433C IShrubObject::New
// 80054390 IShrubObject::Construct
// 800543B0 IShrubObject::Destruct
// 80054444 IShrubObject::GetTypeInfoStatic
// 80054450 IShrubObject::GetReadVersion
SIMMY_STORABLE_IMPL(IShrubObject, "IShrubObject", 0, ISimsObjectModel)

// 800525E4
IShrubObject::IShrubObject() {}
// 8005262C
IShrubObject::~IShrubObject() {}

// 8005268C
void IShrubObject::Create(cXObject* xob, EHouse* house) {
  m_xob = xob;
  m_house = house;
  SetObjOrient();
  SetInitalObjectState();
  if (m_model) Init();
}

namespace {
// A boxwood hedge among the objects at (x, y) on level 1: an object whose first ObjAnimDef's model is
// "boxwood_hedge" (0xBFB9EE2A).
bool HedgeAt(int x, int y) {
  CTilePt tile(x, y, 1);
  ObjectIterator it;
  it.init(tile, ObjectIterator__IterateType{0});
  for (; it.m_current; ++it) {
    const ObjDefinition* def = it.m_current->GetDef();
    const unsigned char* res = def ? objdef::ResData(def) : nullptr;
    if (!res) continue;
    const unsigned char* defs = objdef::AnimDefs(res);
    if (defs && objdef::AnimDefCount(defs) > 0 && ERQuickdata::U32(defs, 0) == 0xbfb9ee2au) return true;
  }
  return false;
}
}  // namespace

// 800526EC
// On the lot: stretched 1.32 times across x with a hedge to the north or south (y + 1, y - 1) and
// across y with one to the west or east (x - 1, x + 1), so a row of hedges closes up; then at its tile
// (the house's position added), a quarter turn round (its direction is not used).
void IShrubObject::SetObjOrient() {
  const CTilePt tile = m_xob->GetCTilePt();
  float sx = 1.0f, sy = 1.0f;
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (w && tile.GetX() >= 0 && tile.GetX() <= w->GetXSize() && tile.GetY() >= 0 && tile.GetY() <= w->GetYSize()) {
    // GC: also reads the tile's walls (cFixedWorld vtable 0xAC), unused.
    const int x = tile.GetX(), y = tile.GetY();
    bool alongY = false, alongX = false;
    if (HedgeAt(x - 1, y)) alongX = true;
    if (HedgeAt(x, y + 1)) alongY = true;
    if (HedgeAt(x + 1, y)) alongX = true;
    if (HedgeAt(x, y - 1)) alongY = true;
    if (alongY) sx = 1.32f;
    if (alongX) sy = 1.32f;
  }
  EVec3 pos(static_cast<float>(tile.GetX()) + m_house->m_posY, static_cast<float>(tile.GetY()) + m_house->m_posX, 0.0f);
  EVec3 scale(sx, sy, 1.0f);
  static const bool trace = std::getenv("SIMMY_BUILD_TRACE") != nullptr;  // PC dev aid
  if (trace) std::printf("[shrub] #%d at %d,%d: scale %.2f %.2f\n", m_xob->GetID(), tile.GetX(), tile.GetY(), sx, sy);
  ApplyMatrix(1.5707963705062866f, pos, scale);
}
