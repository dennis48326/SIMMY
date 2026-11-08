// Port of engine/e_animcontroller.cpp (partial): set-up, main-track playback (SetTrackAnim, Update,
// UpdateTrack), sampling (Compute, Animate, AnimateSpecial, BlendNodeArrays, Blend) and the node
// matrices (ComputeMatrices, CalcMatrix), cross-fades (the blend list, SetTrackBlend*,
// BlendTrackIntensity). Still stubbed: animation events
// (AddEventRange, ProcessEvents), phase-locked slave tracks (GetSlavePos) and layered tracks (Layer).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstring>

namespace {
constexpr int kMaxNodes = 64;
// GC: tempNodeStates (two node arrays the blend list ping-pongs through) and DAT_804AF7A0 (a static-
// blend track's states).
EACNodeState s_tempNodeStates[2][kMaxNodes];
EACNodeState s_staticBlendStates[kMaxNodes];

float kBlendZRotationCutoff = 1.4835298f;  // 8050005C (85 degrees, set by the static initializer)
const float kEACDefaultBlendTime = 0.2f;    // 804FF0B4

EVec3 Sub(const EVec3& a, const EVec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
}  // namespace

// 802E76A8
EACTrack::EACTrack() {}

// 802E77A4
EAnimController::EAnimController() {}

// 802E7884
EAnimController::~EAnimController() {
  Deallocate();
  if (m_character) {
    m_character->DelRef();
    m_character = nullptr;
  }
}

// 802E78E4
void EAnimController::Deallocate() {
  StopAllTracks();
  DeallocateNodes();
  delete[] m_states;
  m_states = nullptr;
  for (EACTrack& t : m_tracks)
    if (t.m_anim) {
      t.m_anim->DelRef();
      t.m_anim = nullptr;
    }
  m_tracks.clear();
  m_blendList.clear();
  m_blendCount = 0;
  m_mainTrack = -1;
  m_mainCount = m_secondaryCount = m_trackCount = 0;
  if (m_character) {
    m_character->DelRef();
    m_character = nullptr;
  }
}

// 802E79F4
// GC: waits for the GPU to finish with the matrices (_pGfx vtable 0x44) before freeing them.
void EAnimController::DeallocateNodes() {
  if (m_matrixBuffers[0] || m_matrixBuffers[1]) {
    for (EMat4*& m : m_matrixBuffers) {
      delete[] m;
      m = nullptr;
    }
  }
  delete[] m_frozen;
  m_frozen = nullptr;
  m_matrices = nullptr;
  m_matricesAt = -1;
}

// 803C357C
// Secondary track index -> its flag.
int EAnimController::GetNonMainTrackFlagFromIndex(int index) {
  switch (index) {
    case 6: return 0x100;
    case 7: return 0x200;
    case 8: return 0x400;
    case 9: return 0x800;
    case 10: return 0x1000;
    case 11: return 0x2000;
    case 12: return 0x4000;
    case 13: return 0x8000;
    default: return 0;
  }
}

// 802E7AC8
// Takes the character, allocates two node matrix buffers (identity) and the node states, and makes
// the tracks: three main ones (one for type 5), or six main and eight secondary ones for people
// (type 0); main tracks after the first can be in the blend list.
void EAnimController::Init(unsigned int character, eAnimatedObjectType type, unsigned int flags) {
  Deallocate();
  auto* chr = static_cast<ERCharacter*>(_characterman.EResourceManager::AddRef(character, nullptr, 0));
  if (m_character) m_character->DelRef();
  m_character = chr;
  m_flags = flags | (m_flags & 2);
  const int nodes = NodeCount();
  if (nodes > 0) {
    for (EMat4*& buf : m_matrixBuffers) {
      buf = new EMat4[nodes];
      for (int i = 0; i < nodes; ++i) buf[i].Id();
    }
    m_matrices = m_matrixBuffers[0];
    m_states = new EACNodeState[nodes];
  }
  int mainCount, secondaryCount;
  switch (static_cast<int>(type)) {
    case 1: case 2: case 3: case 4: mainCount = 3, secondaryCount = 0; break;
    case 5: mainCount = 1, secondaryCount = 0; break;
    default: mainCount = 6, secondaryCount = 8; break;
  }
  m_mainCount = mainCount;
  m_secondaryCount = secondaryCount;
  m_trackCount = mainCount + secondaryCount;
  m_tracks.assign(m_trackCount, EACTrack{});
  m_blendList.assign(mainCount - 1 > 0 ? mainCount - 1 : 0, static_cast<signed char>(-1));
  for (int i = 0; i < m_trackCount; ++i) {
    m_tracks[i].m_index = i;
    if (i > m_mainCount - 1) m_tracks[i].m_flags = static_cast<unsigned int>(GetNonMainTrackFlagFromIndex(i));
  }
}

// 802E7EF4
void EAnimController::Init(char* name, eAnimatedObjectType type, unsigned int flags) {
  Init(EResourceManager::CalcId(name), type, flags);
}

// 802E7F40
void EAnimController::Shutdown() { Deallocate(); }

EACTrack* EAnimController::FindTrack(eTrackFlags flags, bool mainNeedsFlag) {
  const int f = flags;
  int index;
  if (f & eTrackFlags::kMainMask) {
    if (m_mainTrack == -1) return nullptr;
    const unsigned int trackFlags = m_tracks[m_mainTrack].m_flags;
    if (!(trackFlags & static_cast<unsigned int>(mainNeedsFlag ? eTrackFlags::kMain : f))) return nullptr;
    index = m_mainTrack;
  } else {
    switch (f) {
      case 0x100: index = m_mainCount; break;
      case 0x200: index = m_mainCount + 1; break;
      case 0x400: index = m_mainCount + 2; break;
      case 0x800: index = m_mainCount + 3; break;
      case 0x1000: index = m_mainCount + 4; break;
      case 0x2000: index = m_mainCount + 5; break;
      case 0x4000: index = m_mainCount + 6; break;
      case 0x8000: index = m_mainCount + 7; break;
      default: return nullptr;
    }
  }
  if (index < 0 || index >= m_trackCount) return nullptr;
  return &m_tracks[index];
}

// 802E7F60
bool EAnimController::IsTrackValid(eTrackFlags flags) {
  EACTrack* t = FindTrack(flags, false);
  return t && t->m_active == 1;
}

// 802E83EC
// Advances every active track (unlocked ones first, then the phase-locked ones), collecting the root
// translation (`pos`, scaled and, with `rot`, turned by half the rotation step plus `rot`) and
// rotation (`rot`) the tracks moved by this frame.
void EAnimController::Update(EVec3* pos, EVec3* rot, EVec3 scale) {
  m_updatedAt = _framecount;
  if (!(m_flags & 2)) return;
  EVec3 rootDelta, rotDelta;
  m_nodeFlags = 0;
  bool locked = false;
  for (EACTrack& t : m_tracks) {
    if (t.m_active != 1) continue;
    if (t.m_slaveOf == -1) UpdateTrack(&t, rootDelta, rotDelta);
    else locked = true;
  }
  if (locked)
    for (EACTrack& t : m_tracks)
      if (t.m_active == 1 && t.m_slaveOf != -1) UpdateTrack(&t, rootDelta, rotDelta);
  if (pos) {
    rootDelta.x *= scale.x, rootDelta.y *= scale.y, rootDelta.z *= scale.z;
    if (rot) {
      const EVec3 half{rotDelta.x * 0.5f + rot->x, rotDelta.y * 0.5f + rot->y, rotDelta.z * 0.5f + rot->z};
      auto turn = [&](EMat4& m) {
        const EVec3 v = rootDelta;
        rootDelta.x = v.z * m.m[2][0] + v.x * m.m[0][0] + v.y * m.m[1][0] + m.m[3][0];
        rootDelta.y = v.z * m.m[2][1] + v.x * m.m[0][1] + v.y * m.m[1][1] + m.m[3][1];
        rootDelta.z = v.z * m.m[2][2] + v.x * m.m[0][2] + v.y * m.m[1][2] + m.m[3][2];
      };
      EMat4 m;
      if (half.x != 0.0f) {
        m.Id();
        m.RotateX(half.x);
        turn(m);
      }
      if (half.y != 0.0f) {
        m.Id();
        m.RotateY(half.y);
        turn(m);
      }
      if (half.z != 0.0f) {
        m.Id();
        m.RotateZ(half.z);
        turn(m);
      }
    }
    pos->x += rootDelta.x, pos->y += rootDelta.y, pos->z += rootDelta.z;
  }
  if (rot) rot->x += rotDelta.x, rot->y += rotDelta.y, rot->z += rotDelta.z;
}

// 802E8A58
// Advances the track's frame by _dt * fps * speed (unless the frame was set this frame), handling the
// end (loop: wrap and carry the root motion over; hold: clamp; stop: deactivate) and adds the root
// translation moved, weighted by the track's intensity, into `rootDelta` (axes the track does not
// accumulate are blended towards zero instead). Procedural tracks call their update function.
void EAnimController::UpdateTrack(EACTrack* track, EVec3& rootDelta, EVec3& /*rotDelta*/) {
  if (!track->m_active) return;
  track->m_flags &= ~static_cast<unsigned int>(eTrackFlags::kStarted);
  if (!track->m_anim) {
    if (track->m_procUpdate) track->m_procUpdate(track->m_procData, track, m_character, rootDelta, rootDelta);
    return;
  }
  if (track->m_blending) {
    BlendTrackIntensity(track);
    if (!track->m_active) return;
  }
  const float weight = (track->m_flags & eTrackFlags::kMain) ? 1.0f : track->m_intensity;
  const bool events = weight > 0.0f && !track->m_anim->m_events.empty();
  EVec3 moved;
  if (track->m_prevPos == -1.0f) track->m_prevPos = track->m_pos;
  const bool noRoot = (track->m_rootFlags & 7) == 0;
  EVec3 root = noRoot ? EVec3{} : GetAnimRootNodeTrans(track);
  const float last = static_cast<float>(track->m_anim->m_frameCount - 1);
  bool stop = false;
  if (track->m_slaveOf == -1) {
    if (track->m_frameSetAt != _framecount) {
      const float from = track->m_pos;
      track->m_pos = _dt * m_speed * track->m_speed * track->m_fps + from;
      if (track->m_pos > last) {
        track->m_complete = 1;
        if (track->m_endMode == 1) {
          track->m_pos = last;
        } else if (track->m_endMode == 0) {
          if (events) SIMMY_STUB("EAnimController::UpdateTrack: animation events (AddEventRange)", 0x802E89EC);
          if (!noRoot) {
            const float pos = track->m_pos;
            track->m_pos = last;
            moved = Sub(GetAnimRootNodeTrans(track), root);
            track->m_pos = 0.0f;
            root = GetAnimRootNodeTrans(track);
            track->m_pos = pos;
          }
          while (track->m_pos > last) track->m_pos -= last;
          if (track->m_pos < 0.0f) track->m_pos = 0.0f;
        } else if (track->m_endMode == 2) {
          track->m_pos = last;
          stop = true;
        }
      } else if (track->m_pos < 0.0f) {
        track->m_complete = 1;
        if (track->m_endMode == 1) {
          track->m_pos = 0.0f;
        } else if (track->m_endMode == 0) {
          if (!noRoot) {
            const float pos = track->m_pos;
            track->m_pos = 0.0f;
            moved = Sub(GetAnimRootNodeTrans(track), root);
            track->m_pos = last;
            root = GetAnimRootNodeTrans(track);
            track->m_pos = pos;
          }
          while (track->m_pos < 0.0f) track->m_pos += last;
          if (track->m_pos >= last) track->m_pos = last;
        } else if (track->m_endMode == 2) {
          track->m_pos = 0.0f;
        }
      } else if (events) {
        SIMMY_STUB("EAnimController::UpdateTrack: animation events (AddEventRange)", 0x802E89EC);
      }
    }
  } else {
    // Phase-locked: the master's phase (plus the offset) gives the frame; across the end the shorter
    // way round wraps, carrying the root motion over the wrap.
    const float from = track->m_pos;
    const float to = static_cast<float>(GetSlavePos(track, &m_tracks[static_cast<size_t>(track->m_slaveOf)]));
    if (to != from) {
      float forward, backward;
      if (to <= from) {
        forward = (last - from) + to;
        backward = from - to;
      } else {
        backward = (last - to) + from;
        forward = to - from;
      }
      if (events) SIMMY_STUB("EAnimController::UpdateTrack: animation events (AddEventRange)", 0x802E89EC);
      if (backward <= forward) {
        if (track->m_pos < to && !noRoot) {
          // Backwards through frame 0.
          track->m_pos = 0.0f;
          moved = Sub(GetAnimRootNodeTrans(track), root);
          track->m_pos = last;
          root = GetAnimRootNodeTrans(track);
        }
      } else if (to < track->m_pos && !noRoot) {
        // Forwards through the end.
        track->m_pos = last;
        moved = Sub(GetAnimRootNodeTrans(track), root);
        track->m_pos = 0.0f;
        root = GetAnimRootNodeTrans(track);
      }
      track->m_pos = to;
    }
  }
  if (stop) {
    DeactivateTrack(track);
    return;
  }
  track->m_prevPos = track->m_pos;
  float w = weight;
  if (track->m_blendFactors) w *= track->m_blendFactors[0];
  if (noRoot) {
    if (!(track->m_rootFlags & 0x100)) {
      rootDelta.x += (0.0f - rootDelta.x) * w;
      rootDelta.y += (0.0f - rootDelta.y) * w;
      rootDelta.z += (0.0f - rootDelta.z) * w;
    }
  } else {
    const EVec3 now = GetAnimRootNodeTrans(track);
    moved = EVec3{moved.x + now.x - root.x, moved.y + now.y - root.y, moved.z + now.z - root.z};
    for (int i = 0; i < 3; ++i) {
      if (track->m_rootFlags & (1u << i)) {
        rootDelta[i] = (track->m_rootFlags & 0x100) ? w * moved[i] + rootDelta[i]
                                                    : w * (moved[i] - rootDelta[i]) + rootDelta[i];
      } else if (!(track->m_rootFlags & 0x100)) {
        rootDelta[i] = w * (0.0f - rootDelta[i]) + rootDelta[i];
      }
    }
  }
}

// 802E9B68
int EAnimController::GetFirstRelevantTrack() {
  if (m_mainTrack != -1 && m_tracks[m_mainTrack].m_anim) return m_mainTrack;
  for (unsigned int i = 0; i < m_blendCount; ++i) {
    const int index = m_blendList[i];
    if (m_tracks[index].m_active && m_tracks[index].m_anim) return index;
  }
  return -1;
}

// 802E9C24
// Samples an active track with an animation and some intensity into `states`; procedural tracks call
// their animate function. Returns whether the track was sampled.
int EAnimController::AnimateTrack(EACTrack* track, EACNodeState* states, EMat4* root) {
  if (track->m_flags & eTrackFlags::kSuspended) return 0;
  if (track->m_active == 1 && track->m_anim && track->m_intensity > 0.0f) {
    Animate(track, states);
    return 1;
  }
  if (track->m_procAnimate) {
    EMat4 id;
    id.Id();
    track->m_procAnimate(track->m_procData, track, root ? *root : id, m_character, m_states);
  }
  return 0;
}

// 802E9CDC
// Once per frame (or when forced): the first relevant main track straight into the node states (reset
// first unless it fully covers them), the outgoing main tracks blended in from the oldest, then each
// active secondary track blended on top; then the matrices and the callback.
void EAnimController::Compute(EMat4& root, int frame) {
  if (m_computedAt == frame && !m_forceCompute) return;
  m_computedAt = frame;
  if (!(m_flags & 2) || !m_computeEnabled) return;
  int i = 0;
  while (i < m_trackCount && m_tracks[i].m_active != 1) ++i;
  if (i < m_trackCount) {
    int first = GetFirstRelevantTrack();
    EACTrack* track = first == -1 ? &m_tracks[0] : &m_tracks[first];
    const int nodes = NodeCount();
    if (nodes && (first == -1 || !track->m_anim || track->m_intensity != 1.0f || track->m_blendFactors))
      for (int n = 0; n < nodes; ++n) m_states[n] = EACNodeState{};
    m_nonUniformScale = 0;
    AnimateTrack(track, m_states, nullptr);
    EACTrack* staticBlend = nullptr;
    for (EACTrack* s : track->m_slaves)
      if (s->m_flags & eTrackFlags::kStaticBlend) staticBlend = s;
    if (staticBlend) {
      AnimateTrack(staticBlend, s_staticBlendStates, nullptr);
      BlendNodeArrays(staticBlend, s_staticBlendStates, m_states);
    }
    int buffer = 0;
    EACTrack* prev = nullptr;
    EACNodeState* prevStates = nullptr;
    for (int b = static_cast<int>(m_blendCount) - 1; b >= 0; --b) {
      EACNodeState* states = s_tempNodeStates[buffer];
      EACTrack* t = &m_tracks[m_blendList[b]];
      EACTrack* tStatic = nullptr;
      for (EACTrack* s : t->m_slaves)
        if (s->m_flags & eTrackFlags::kStaticBlend) tStatic = s;
      if (AnimateTrack(t, states, nullptr)) {
        if (tStatic) {
          AnimateTrack(tStatic, s_staticBlendStates, nullptr);
          BlendNodeArrays(tStatic, s_staticBlendStates, states);
        }
        if (prev) BlendNodeArrays(prev, prevStates, states);
      }
      buffer = (buffer + 1) % 2;
      prev = t;
      prevStates = states;
    }
    if (prev) BlendNodeArrays(prev, prevStates, m_states);
    for (int s = m_mainCount; s < m_trackCount; ++s) {
      if (AnimateTrack(&m_tracks[s], s_tempNodeStates[0], &root))
        BlendNodeArrays(&m_tracks[s], s_tempNodeStates[0], m_states);
    }
  }
  ComputeMatrices(root);
  if (m_computeCallback) m_computeCallback(m_computeCallbackData, root, m_character, m_matrices);
}

// 802EA128
// GC: the matrices of the frame being drawn alternate between two buffers (_evenodd) so the GPU can
// still read last frame's; a controller without buffers takes them from a frame allocator.
void EAnimController::AllocMatrices() {
  if (m_matricesAt == _framecount) return;
  m_matricesAt = _framecount;
  m_matrices = m_matrixBuffers[_evenodd & 1];
}

// 802EA698
// The root's translation stays at its start on the axes the track accumulates (Update moves the
// object instead).
void EAnimController::FixAccumulationForRootNode(EACTrack* track, EACNodeState& state) {
  for (int i = 0; i < 3; ++i)
    if (track->m_rootFlags & (1u << i)) state.m_trans[i] = track->m_rootStart[i];
}

// 802EA6DC
// The special channels: visibility (unless the node keeps its own), the vector and the three scalars,
// each constant (from the animation's constants), streamed, or absent (default value, flag clear).
void EAnimController::AnimateSpecial(ERAnim* anim, float frame, EACTrackStreams* streams, EACNodeState* stored,
                                     EAnimNodeDataPos* node, EACNodeState& state) {
  const int* d = reinterpret_cast<const int*>(node->m_data);  // [3] vis, [4] vector, [5..7] scalars
  const float* constants = anim->m_constants.data();
  if (d[3] != 0 && !(stored->m_flags & EACNodeState::kKeepVisibility)) {
    bool visible;
    if (d[3] < 1) visible = streams->m_vis ? streams->m_vis->GetFrame(frame) : true;
    else visible = constants[d[3]] != 0.0f;
    if (visible) state.m_flags &= ~static_cast<unsigned int>(EACNodeState::kHidden);
    else state.m_flags |= EACNodeState::kHidden;
  }
  if (d[4] == 0) {
    state.m_vector = EVec3{1.0f, 1.0f, 1.0f};
  } else {
    state.m_flags |= EACNodeState::kHasVector;
    if (d[4] < 1) {
      if (streams->m_vector) {
        state.m_vector = streams->m_vector->GetFrame(frame);
      } else {
        state.m_vector = EVec3{1.0f, 1.0f, 1.0f};
        state.m_flags &= ~static_cast<unsigned int>(EACNodeState::kHasVector);
      }
    } else {
      state.m_vector = EVec3{constants[d[4]], constants[d[4] + 1], constants[d[4] + 2]};
    }
  }
  struct Scalar {
    int index;
    unsigned int flag;
    float def;
    float EACNodeState::*field;
  };
  const Scalar scalars[3] = {{d[5], EACNodeState::kHasScalar0, 1.0f, &EACNodeState::m_scalar0},
                             {d[6], EACNodeState::kHasScalar1, 10.0f, &EACNodeState::m_scalar1},
                             {d[7], EACNodeState::kHasScalar2, 0.0f, &EACNodeState::m_scalar2}};
  for (int i = 0; i < 3; ++i) {
    const Scalar& s = scalars[i];
    if (s.index == 0) {
      state.*s.field = s.def;
      continue;
    }
    state.m_flags |= s.flag;
    if (s.index > 0) {
      state.*s.field = constants[s.index];
    } else if (streams->m_scalar[i]) {
      state.*s.field = streams->m_scalar[i]->GetFrame(frame);
    } else {
      state.*s.field = s.def;
      state.m_flags &= ~s.flag;
    }
  }
}

// 802EA9E0
// Blends a track's node states (`from`) into `to` by its intensity (and per-node factors): copy at
// full weight, layer for layered tracks, else blend; for the two root nodes a blend keeps `to`'s
// translation when they are far apart, and its rotation about z when the blend would turn it by more
// than kBlendZRotationCutoff.
void EAnimController::BlendNodeArrays(EACTrack* track, EACNodeState* from, EACNodeState* to) {
  const unsigned int rootFlags = track->m_rootFlags;
  const float intensity = track->m_intensity;
  const int nodes = NodeCount();
  for (int i = 0; i < nodes; ++i, ++from, ++to) {
    float w = intensity;
    if (track->m_blendFactors) w *= track->m_blendFactors[i];
    if (w == 0.0f) continue;
    if (rootFlags & 0x100) {
      Layer(w, *to, *from);
      continue;
    }
    if (w == 1.0f) {
      *to = *from;
      continue;
    }
    EVec3 zAxis{0.0f, 0.0f, 1.0f};
    const EVec3 trans = to->m_trans;
    const float zRot = to->m_rot.ExtractAxisRotation(zAxis);
    Blend(w, *to, *from);
    if (i >= 2) continue;
    const float dx = trans.x - from->m_trans.x, dy = trans.y - from->m_trans.y;
    if (0.1225f < dx * dx + dy * dy) to->m_trans.x = trans.x, to->m_trans.y = trans.y;
    if (0.35f < std::fabs(trans.z - from->m_trans.z)) to->m_trans.z = trans.z;
    float diff = std::fabs(zRot - from->m_rot.ExtractAxisRotation(zAxis));
    while (3.1415927f < diff) diff -= 6.2831855f;
    if (kBlendZRotationCutoff < diff) {
      EVec3 xAxis{1.0f, 0.0f, 0.0f}, yAxis{0.0f, 1.0f, 0.0f};
      const float xRot = to->m_rot.ExtractAxisRotation(xAxis);
      const float yRot = to->m_rot.ExtractAxisRotation(yAxis);
      to->m_rot.Set(xRot, yRot, zRot);
    }
  }
}

// 802EAD18
// Samples every node of the track at its frame: rotation, translation (main-track flags 2 and 0x40
// drop y or x of the two root nodes) and scale, each constant, streamed or absent, then the special
// channels; the root keeps its accumulated axes fixed.
void EAnimController::Animate(EACTrack* track, EACNodeState* states) {
  ERAnim* anim = track->m_anim;
  const float frame = track->m_pos;
  EACTrackStreams* streams = track->m_streams;
  const int nodes = NodeCount();
  if (!streams) return;
  const float* constants = anim->m_constants.data();
  for (int i = 0; i < nodes; ++i, ++states, ++streams) {
    EAnimNodeDataPos* node = &anim->m_nodes[i];
    const int* d = reinterpret_cast<const int*>(node->m_data);  // [0] rot, [1] scale, [2] trans
    float w = track->m_intensity;
    if (track->m_blendFactors) w *= track->m_blendFactors[i];
    if (w == 0.0f) continue;
    EACNodeState s;
    s.m_flags = (states->m_flags & EACNodeState::kKeepVisibility) ? states->m_flags & 0x18 : 0;
    if (d[0] != 0) {
      if (d[0] < 1) {
        if (streams->m_rot) {
          s.m_rot = streams->m_rot->GetFrame(frame);
          s.m_flags |= EACNodeState::kAnimated;
        }
      } else {
        s.m_rot = EQuat{constants[d[0]], constants[d[0] + 1], constants[d[0] + 2], constants[d[0] + 3]};
        s.m_flags |= EACNodeState::kAnimated;
      }
    }
    if (d[2] != 0) {
      if (d[2] < 1) {
        if (streams->m_trans) {
          s.m_trans = streams->m_trans->GetFrame(frame);
          s.m_flags |= EACNodeState::kAnimated;
        }
      } else {
        s.m_trans = EVec3{constants[d[2]], constants[d[2] + 1], constants[d[2] + 2]};
        s.m_flags |= EACNodeState::kAnimated;
      }
      if (i < 2) {
        if (track->m_flags & 2) s.m_trans.y = 0.0f;
        else if (track->m_flags & 0x40) s.m_trans.x = 0.0f;
      }
    }
    if (d[1] != 0) {
      if (d[1] < 1) {
        if (streams->m_scale) {
          s.m_scale = streams->m_scale->GetFrame(frame);
          s.m_flags |= EACNodeState::kAnimated | EACNodeState::kScaled;
        }
      } else {
        s.m_flags |= EACNodeState::kAnimated | EACNodeState::kScaled;
        s.m_scale = EVec3{constants[d[1]], constants[d[1] + 1], constants[d[1] + 2]};
      }
      if (s.m_scale.x != s.m_scale.y || s.m_scale.x != s.m_scale.z) {
        m_nonUniformScale = 1;
        s.m_flags |= EACNodeState::kNonUniformScale;
      }
    }
    // PC: the special channels start at their defaults (the original left them as found on the
    // stack when AnimateSpecial was not called).
    if (d[3] || d[6] || d[5] || d[4] || d[7]) AnimateSpecial(anim, frame, streams, states, node, s);
    if (i == 0) FixAccumulationForRootNode(track, s);
    *states = s;
    m_nodeFlags |= s.m_flags;
  }
}

// 802EB1B0
void EAnimController::Blend(float t, EACNodeState& to, EACNodeState& from) {
  to.m_flags |= from.m_flags;
  to.m_rot.Slerp(t, to.m_rot, from.m_rot);
  for (int i = 0; i < 3; ++i) {
    to.m_trans[i] = to.m_trans[i] + (from.m_trans[i] - to.m_trans[i]) * t;
    to.m_scale[i] = to.m_scale[i] + (from.m_scale[i] - to.m_scale[i]) * t;
  }
}

// The local matrix of an animated node: rotation (and scale) about the pivot, then the translation.
// A non-uniformly scaled node that is not flagged (node +0xB0) is scaled in its bind frame (node
// +0x70) instead, with the rotation taken relative to the bind orientation, so the scale follows the bone.
static void NodeLocalMatrix(ECharacterNode& node, EACNodeState& s, bool bindFrameScale, EMat4& local) {
  EVec3 back{-node.m_pivot.x, -node.m_pivot.y, -node.m_pivot.z};
  EVec3 to{node.m_pivot.x + s.m_trans.x, node.m_pivot.y + s.m_trans.y, node.m_pivot.z + s.m_trans.z};
  if (bindFrameScale) {
    EMat4 bind = node.m_mat70;
    bind.PreTranslate(back);
    const EQuat& q = s.m_rot;
    const EQuat& b = node.m_orient;
    EQuat qb;  // q * b
    qb.x = q.x * b.w + q.y * b.z + (q.w * b.x - q.z * b.y);
    qb.y = q.y * b.w + ((q.z * b.x + q.w * b.y) - q.x * b.z);
    qb.z = q.z * b.w + q.w * b.z + -q.y * b.x + q.x * b.y;
    qb.w = q.w * b.w + ((-q.x * b.x - q.y * b.y) - q.z * b.z);
    EMat4 rs;
    qb.ToMat4(rs);
    rs.PreScale(s.m_scale);
    rs.PostTranslate(to);
    local.Mult4x4(bind, rs);
    return;
  }
  s.m_rot.ToMat4(local);
  if (s.m_flags & EACNodeState::kScaled) local.PreScale(s.m_scale);
  local.PreTranslate(back);
  local.PostTranslate(to);
}

// 802EB570
// Node i's matrix: its local matrix (NodeLocalMatrix) times its parent's (the root's for node 0);
// unanimated nodes take the parent's matrix. The bind-frame scaling is only considered when some node
// is scaled unevenly this frame. The world position goes back into the node state. A controller scale
// other than 1 scales every matrix.
void EAnimController::ComputeMatrices(EMat4& root) {
  const int nodes = NodeCount();
  if (!nodes) return;
  AllocMatrices();
  EMat4* mats = m_matrices;
  for (int i = 0; i < nodes; ++i) {
    ECharacterNode& node = *m_character->GetNode(i);
    EACNodeState& s = m_states[i];
    EMat4& parent = i == 0 ? root : mats[node.m_parent];
    EMat4& out = mats[i];
    if (!(s.m_flags & EACNodeState::kAnimated)) {
      out = parent;
    } else {
      const bool bindFrame = m_nonUniformScale && (s.m_flags & EACNodeState::kScaled) &&
                             (s.m_flags & EACNodeState::kNonUniformScale) && node.m_flagB0 == 0;
      EMat4 local;
      NodeLocalMatrix(node, s, bindFrame, local);
      out.Mult4x4(local, parent);
    }
    s.m_trans = EVec3{out.m[3][0], out.m[3][1], out.m[3][2]};
  }
  if (m_scale != 1.0f) {
    for (int i = 0; i < nodes; ++i) {
      mats[i].PreScale(m_scale);
      m_states[i].m_trans = EVec3{mats[i].m[3][0], mats[i].m[3][1], mats[i].m[3][2]};
    }
  }
}

// 802EBFD8
void EAnimController::StopAllTracks() {
  for (EACTrack& t : m_tracks) DeactivateTrack(&t);
}

// 802EC038
// Releases a track: its phase-locked slaves are freed (a static-blend slave deactivated too), it
// leaves the blend list (deactivating the older entries), its streams and animation are released.
void EAnimController::DeactivateTrack(EACTrack* track) {
  if (!track) return;
  EACTrack* staticBlend = nullptr;
  for (EACTrack* s : track->m_slaves) {
    s->m_slaveOf = -1;
    if (s->m_flags & eTrackFlags::kStaticBlend) staticBlend = s;
  }
  track->m_slaves.clear();
  if (staticBlend) DeactivateTrack(staticBlend);
  if (track->m_slaveOf != -1) SetTrackPhaseLock(track, nullptr, 0.0f);
  if (track->m_flags & eTrackFlags::kInBlendList) {
    const unsigned int slot = track->m_flags >> 28;
    m_blendList[slot] = -1;
    if (static_cast<int>(slot) <= m_mainCount - 3 && m_blendList[slot + 1] != -1)
      DeactivateTrack(&m_tracks[m_blendList[slot + 1]]);
    m_blendCount = slot;
  }
  if (track->m_active == 0) {
    track->m_flags = 0;
    return;
  }
  DestroyStreams(track);
  track->m_animId = 0;
  if (track->m_anim) {
    track->m_anim->DelRef();
    track->m_anim = nullptr;
  }
  track->m_active = 0;
  if (track->m_flags & eTrackFlags::kMain) m_mainTrack = -1;
  track->m_flags = 0;
}

// 802EC198
// A decompressor for every streamed channel of every node (a negative index in the node's data is
// the stream's bit offset), then the root translation at frame 0.
void EAnimController::CreateStreams(EACTrack* track) {
  if (!track || !track->m_anim) return;
  const int nodes = NodeCount();
  track->m_streams = nodes ? new EACTrackStreams[nodes] : nullptr;
  if (!track->m_streams) return;
  ERAnim* anim = track->m_anim;
  EBitArray* bits = &anim->m_bitArray;
  for (int i = 0; i < nodes && i < static_cast<int>(anim->m_nodes.size()); ++i) {
    EACTrackStreams& s = track->m_streams[i];
    const int* d = reinterpret_cast<const int*>(anim->m_nodes[i].m_data);
    if (d[0] < 0) (s.m_rot = new ERotDecomp)->Init(bits, -d[0]);
    if (d[2] < 0) (s.m_trans = new EVec3Decomp)->Init(bits, -d[2]);
    if (d[1] < 0) (s.m_scale = new EVec3Decomp)->Init(bits, -d[1]);
    if (d[3] < 0) (s.m_vis = new EVisInfoDecomp)->Init(bits, -d[3]);
    if (d[4] < 0) (s.m_vector = new EVec3Decomp)->Init(bits, -d[4]);
    for (int k = 0; k < 3; ++k)
      if (d[5 + k] < 0) (s.m_scalar[k] = new ScalarDecomp)->Init(bits, -d[5 + k]);
  }
  const float pos = track->m_pos;
  track->m_pos = 0.0f;
  track->m_rootStart = GetAnimRootNodeTrans(track);
  track->m_pos = pos;
}

// 802EC4B4
void EAnimController::DestroyStreams(EACTrack* track) {
  if (!track || !track->m_streams) return;
  const int nodes = NodeCount();
  for (int i = 0; i < nodes; ++i) {
    EACTrackStreams& s = track->m_streams[i];
    delete s.m_rot;
    delete s.m_scale;
    delete s.m_trans;
    delete s.m_vis;
    delete s.m_vector;
    for (ScalarDecomp* sd : s.m_scalar) delete sd;
  }
  delete[] track->m_streams;
  track->m_streams = nullptr;
}

// 802ECF0C
// Puts an animation on a track. For main-track flags the current main track (unless it only started
// this frame, which is simply reused) fades out: it leaves the main slot, takes the blend type's fade
// to 0 (-1/0: snap, 1: Hermite, 2: smooth over `blendTime` (-1: kEACDefaultBlendTime), 3: the
// animation's own blend, 4: none, 5: a static blend) and goes to the front of the blend list, the
// older outgoing tracks moving back (the oldest drops out when the list is full). The animation goes
// on a free main track (not current, not in the blend list), else the faintest one, which becomes the
// current main track at its default intensity: Compute samples it first and blends the outgoing
// tracks over it. Returns the track; `outgoing` gets the track fading out, if any.
EACTrack* EAnimController::SetTrackAnim(eTrackFlags flags, unsigned int animId, int blendType, float blendTime,
                                       EACTrack** outgoing) {
  if (m_tracks.empty()) return nullptr;
  const int f = static_cast<int>(flags);
  const bool main = (f & eTrackFlags::kMainMask) != 0;
  EACTrack* track = main ? (m_mainTrack < 0 ? nullptr : &m_tracks[m_mainTrack]) : FindTrack(flags, false);
  if (track && track->m_active == 1 && track->m_anim && track->m_anim->m_id == animId && !track->m_complete) {
    if (outgoing) *outgoing = nullptr;
    return track;
  }
  auto* anim = static_cast<ERAnim*>(_animman.AddRef(animId, nullptr, 0));
  EACTrack* fading = nullptr;
  if (main) {
    bool reuse = false;
    if (track) {
      if (track->m_flags & eTrackFlags::kStarted) {
        // Started this frame: no fade, the track is taken over.
        fading = m_blendCount && m_blendList[0] != -1 ? &m_tracks[m_blendList[0]] : nullptr;
        SetTrackActive(track, false);
        reuse = true;
      } else {
        if (blendType == 5) {
          track->m_flags = (track->m_flags & ~static_cast<unsigned int>(eTrackFlags::kMain)) | eTrackFlags::kStaticBlend;
        } else {
          track->m_flags &= ~static_cast<unsigned int>(eTrackFlags::kMain);
          if (blendTime == -1.0f) blendTime = kEACDefaultBlendTime;
          switch (blendType) {
            case 1: SetTrackBlendHermite(track, 0.0f, blendTime, 0.0f, 0.0f); break;
            case 2: SetTrackBlendSmooth(track, 0.0f, blendTime, 0.01f); break;
            case 3: SetTrackBlend(track, 0.0f); break;
            case 4: break;
            case -1:
            default:
              if (track->m_anim && track->m_anim->m_id != animId) {
                if (track->m_rootFlags & 0x400) SetTrackBlend(track, 0.0f);
                else SetTrackBlendSmooth(track, 0.0f, blendTime, 0.01f);
              } else {
                SetTrackIntensity(track, 0.0f, true);
              }
              break;
            case 0: SetTrackIntensity(track, 0.0f, true); break;
          }
          if (track->m_active) track->m_flags |= eTrackFlags::kInBlendList;
          fading = track;
          if (track->m_active == 1) {
            // To the front of the blend list: the others move back one slot.
            for (int i = static_cast<int>(m_blendCount) - 1; i >= 0; --i) {
              EACTrack& t = m_tracks[m_blendList[i]];
              if (!(t.m_flags & eTrackFlags::kInBlendList) || t.m_active != 1) {
                // No longer fading: out of the list, the later entries closing up.
                m_blendList[i] = -1;
                t.m_flags = 0;
                --m_blendCount;
                for (unsigned int j = static_cast<unsigned int>(i); j + 1 < m_blendList.size() && j < m_blendCount; ++j) {
                  const signed char next = m_blendList[j + 1];
                  if (next != -1) {
                    m_blendList[j] = next;
                    EACTrack& n = m_tracks[next];
                    n.m_flags = (n.m_flags & 0x0fffffffu) | (j << 28);
                    m_blendList[j + 1] = -1;
                  }
                }
              } else if (i < m_mainCount - 2) {
                t.m_flags = (t.m_flags & 0x0fffffffu) | (static_cast<unsigned int>(i + 1) << 28);
                m_blendList[i + 1] = static_cast<signed char>(t.m_index);
              } else {
                DeactivateTrack(&t);
              }
            }
            track->m_flags &= 0x0fffffffu;
            m_blendList[0] = static_cast<signed char>(track->m_index);
            ++m_blendCount;
          }
        }
      }
    }
    if (!reuse) {
      // A free main track (not current, not in the blend list), else the faintest one, freed.
      int faintest = -1, pick = -1;
      float lowest = 2.0f;
      for (int i = 0; i < m_mainCount; ++i) {
        if (i == m_mainTrack) continue;
        EACTrack& t = m_tracks[i];
        bool listed = false;
        for (unsigned int b = 0; b < m_blendCount; ++b)
          if (m_blendList[b] == i) listed = true;
        if (listed) {
          if (t.m_intensity < lowest) faintest = i, lowest = t.m_intensity;
          continue;
        }
        if (t.m_active == 0 || !t.m_anim) {
          t.m_active = 0;
          t.m_flags = 0;
          pick = i;
          break;
        }
        if (t.m_intensity < lowest) faintest = i, lowest = t.m_intensity;
      }
      if (pick == -1) {
        if (faintest == -1) faintest = 0;
        DeactivateTrack(&m_tracks[faintest]);
        pick = faintest;
      }
      track = &m_tracks[pick];
    }
  }
  if (!track) {
    if (anim) anim->DelRef();
    return nullptr;
  }
  if (track->m_anim) {
    track->m_anim->DelRef();
    track->m_anim = nullptr;
  }
  track->m_anim = anim;
  if (!anim) {
    DeactivateTrack(track);
    if (outgoing) *outgoing = nullptr;
    return track;
  }
  track->m_active = 0;
  track->m_flags = static_cast<unsigned int>(f) | eTrackFlags::kStarted;
  if (main) {
    track->m_flags |= eTrackFlags::kMain;
    m_mainTrack = track->m_index;
  }
  track->m_fps = anim->m_fps;
  track->m_defaultIntensity = anim->m_field68;
  track->m_rootFlags = anim->m_field6C;
  track->m_field0C = anim->m_field70;
  track->m_endMode = anim->m_field71;
  track->m_field0E = anim->m_field72;
  track->m_field10 = anim->m_field74;
  track->m_field14 = anim->m_field78;
  track->m_field18 = anim->m_field7C;
  track->m_field1C = anim->m_field80;
  track->m_field20 = anim->m_field84;
  track->m_frameSetAt = _framecount - 1;
  track->m_field48 = -1.0f;
  track->m_speed = 1.0f;
  track->m_slaveOf = -1;
  track->m_animId = animId;
  track->m_intensity = 0.0f;
  track->m_procUpdate = nullptr;
  track->m_procAnimate = nullptr;
  track->m_field3C = 0;
  track->m_pos = 0.0f;
  track->m_prevPos = -1.0f;
  track->m_phaseOffset = 0.0f;
  track->m_complete = 0;
  track->m_blending = 0;
  track->m_blendT = 0.0f;
  track->m_blendFactors = nullptr;
  if (f == 2) track->m_endMode = 0;
  if (m_character && main) SetTrackIntensity(track, track->m_defaultIntensity, true);
  if (outgoing) *outgoing = fading && fading->m_active == 1 ? fading : nullptr;
  return track;
}

// 802EE17C
// The animation's own fade (+0x0E): 0 snaps, 1 and 2 blend smoothly over its time and epsilon.
void EAnimController::SetTrackBlend(EACTrack* track, float target) {
  if (!track || track->m_active != 1) return;
  if (track->m_field0E == 0) SetTrackIntensity(track, target, true);
  else if (track->m_field0E < 3) SetTrackBlendSmooth(track, target, track->m_field1C, track->m_field20);
}

// 802EE324
// The intensity eases toward `target` (BlendTrackIntensity), snapping within `epsilon`.
void EAnimController::SetTrackBlendSmooth(EACTrack* track, float target, float time, float epsilon) {
  if (!track || track->m_active != 1 || target == track->m_blendTo) return;
  track->m_blending = 2;
  track->m_blendEpsilon = epsilon;
  track->m_blendTime = time;
  track->m_blendTo = target;
  track->m_field54 = time;
  if (target != 0.0f) SetTrackActive(track, true);
}

// 802EE390
void EAnimController::SetTrackBlendSmooth(eTrackFlags flags, float target, float time, float epsilon) {
  SetTrackBlendSmooth(FindTrack(flags, false), target, time, epsilon);
}

// 802EE640
// A Hermite curve from the current intensity to `target` with tangents t0, t1, over `time` times the
// distance (no distance: snapped).
void EAnimController::SetTrackBlendHermite(EACTrack* track, float target, float time, float t0, float t1) {
  if (!track || track->m_active != 1 || target == track->m_blendTo) return;
  const float dist = std::fabs(track->m_intensity - target);
  track->m_blending = 1;
  track->m_hermite0 = t0;
  track->m_hermite1 = t1;
  track->m_blendFrom = track->m_intensity;
  track->m_blendTo = target;
  track->m_blendT = 0.0f;
  if (time * dist == 0.0f) {
    SetTrackIntensity(track, target, true);
  } else {
    track->m_blendTime = time * dist;
    if (target != 0.0f) SetTrackActive(track, true);
  }
}

// 802EE854
// One frame of the track's blend: Hermite along its curve, smooth a step of the remaining distance;
// done at the target. (A track faded to 0 is deactivated by SetTrackIntensity.)
void EAnimController::BlendTrackIntensity(EACTrack* track) {
  float v = track->m_intensity;
  auto clamp01 = [](float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); };
  if (track->m_blending == 1) {
    const float t = clamp01(track->m_blendT + _dt * m_speed / track->m_blendTime);
    track->m_blendT = t;
    const float a = track->m_hermite0, b = track->m_hermite1;
    const float h = clamp01(a * t + (b - 2.0f + a) * t * t * t + ((3.0f - b) - (a + a)) * t * t);
    v = (track->m_blendTo - track->m_blendFrom) * h + track->m_blendFrom;
  } else if (track->m_blending == 2) {
    const float t = clamp01(track->m_blendT + _dt * m_speed / track->m_blendTime);
    track->m_blendT = t;
    v = clamp01(t * (track->m_blendTo - track->m_intensity) + track->m_intensity);
    if (std::fabs(track->m_blendTo - v) < track->m_blendEpsilon) v = track->m_blendTo;
  }
  if (v == track->m_blendTo) {
    track->m_blending = 0;
    track->m_blendT = 0.0f;
  }
  SetTrackIntensity(track, v, false);
}

// 802EF214
void EAnimController::SetTrackBlendFactors(EACTrack* track, float* factors) {
  if (track && track->m_active == 1) track->m_blendFactors = factors;
}

// 802EF62C
// The track's phase (0..1 through the animation); 0 when inactive.
double EAnimController::GetTrackPos(eTrackFlags flags) {
  EACTrack* track = FindTrack(flags, false);
  if (!track || track->m_active != 1 || !track->m_anim) return 0.0;
  return track->m_pos / static_cast<float>(track->m_anim->m_frameCount - 1);
}

// 802EF820
void EAnimController::SetTrackPos(eTrackFlags flags, float phase, bool markSet) {
  SetTrackPos(FindTrack(flags, false), phase, markSet);
}

// 802EDF1C
double EAnimController::GetTrackSpeed(eTrackFlags flags) {
  EACTrack* track = FindTrack(flags, false);
  return track && track->m_active == 1 ? track->m_speed : 0.0;
}

// 802EFD88
void EAnimController::SetTrackSpeed(eTrackFlags flags, float speed) { SetTrackSpeed(FindTrack(flags, false), speed); }

// 802EDB44
void EAnimController::SetTrackIntensity(EACTrack* track, float intensity, bool snap) {
  if (intensity < 0.0f) intensity = 0.0f;
  if (intensity > 1.0f) intensity = 1.0f;
  if (snap) {
    track->m_blendFrom = intensity;
    track->m_blending = 0;
    track->m_blendTo = intensity;
  }
  track->m_intensity = intensity;
  if (intensity == 0.0f) {
    SetTrackActive(track, false);
  } else {
    SetTrackActive(track, true);
    track->m_field48 = -1.0f;
  }
}

// 802EF040
// Jumps the track to a frame (clamped to the animation); `markSet` keeps Update from advancing it this
// frame.
void EAnimController::SetTrackFrame(eTrackFlags flags, float frame, bool markSet) {
  EACTrack* track = FindTrack(flags, false);
  if (!track || track->m_active != 1) return;
  if (!track->m_anim) {
    track->m_pos = frame;
  } else {
    const float last = static_cast<float>(track->m_anim->m_frameCount - 1);
    track->m_pos = frame < 0.0f ? 0.0f : (frame > last ? last : frame);
  }
  if (markSet) track->m_frameSetAt = _framecount;
}

// 802EF7B8
void EAnimController::SetTrackPos(EACTrack* track, float phase, bool markSet) {
  if (!track || track->m_active != 1) return;
  track->m_pos = phase * static_cast<float>(track->m_anim->m_frameCount - 1);
  if (markSet) track->m_frameSetAt = _framecount;
}

// 802EC990
// The id of the animation the track plays (0 when none).
unsigned int EAnimController::GetTrackAnimId(eTrackFlags flags) {
  EACTrack* track = FindTrack(flags, false);
  if (track && track->m_active == 1 && track->m_anim) return track->m_anim->m_id;
  return 0;
}

// 802EF968
// Whether the track reached its end (an absent or inactive track counts as complete).
int EAnimController::IsTrackAnimComplete(eTrackFlags flags) {
  EACTrack* track = FindTrack(flags, true);
  if (track && (track->m_flags & static_cast<unsigned int>(static_cast<int>(flags))) && track->m_active == 1)
    return track->m_complete;
  return 1;
}

// 802EFD44
void EAnimController::SetTrackSpeed(EACTrack* track, float speed) {
  if (!track || track->m_active != 1) return;
  if (speed >= 0.0f) track->m_flags &= ~static_cast<unsigned int>(eTrackFlags::kReverse);
  else track->m_flags |= eTrackFlags::kReverse;
  track->m_speed = speed;
}

// 802EFED0
// Every track's speed magnitude, keeping reversed tracks reversed.
void EAnimController::SetAllTrackSpeed(float speed) {
  for (EACTrack& t : m_tracks) {
    if (!(t.m_flags & eTrackFlags::kReverse)) t.m_speed = speed > 0.0f ? speed : -speed;
    else t.m_speed = speed > 0.0f ? -speed : speed;
  }
}

// 802F0300
// The root node's translation at the track's frame.
EVec3 EAnimController::GetAnimRootNodeTrans(EACTrack* track) {
  if (NodeCount() && track->m_anim && !track->m_anim->m_nodes.empty()) {
    const int index = static_cast<int>(track->m_anim->m_nodes[0].m_data[2]);
    if (index > 0) {
      const float* c = &track->m_anim->m_constants[index];
      return EVec3{c[0], c[1], c[2]};
    }
    if (index < 0 && track->m_streams && track->m_streams[0].m_trans)
      return track->m_streams[0].m_trans->GetFrame(track->m_pos);
  }
  return EVec3{};
}

// 802F03B4
void EAnimController::Draw(ERC* rc, ERModel* model, EMat4& root) {
  if (!m_computeEnabled) return;
  Compute(root, _framecount);
  model->DrawHierarchical(rc, m_matrices, m_states, m_character ? m_character->GetNodeCount() : 0);
}

// 802F0AE8
// Activating a track (re)creates its streams; deactivating the current main track stops all tracks.
void EAnimController::SetTrackActive(EACTrack* track, bool active) {
  const int a = active ? 1 : 0;
  if (track->m_active == a) {
    if (!active) {
      if (track->m_flags & eTrackFlags::kInBlendList) {
        const unsigned int slot = track->m_flags >> 28;
        m_blendList[slot] = -1;
        if (static_cast<int>(slot) <= m_mainCount - 3 && m_blendList[slot + 1] != -1)
          DeactivateTrack(&m_tracks[m_blendList[slot + 1]]);
        m_blendCount = slot;
      }
      track->m_flags = 0;
    }
    return;
  }
  if (!active) {
    if ((track->m_flags & 0x30000) == eTrackFlags::kMain) StopAllTracks();
    DeactivateTrack(track);
  } else if (track->m_anim) {
    DestroyStreams(track);
    CreateStreams(track);
  }
  track->m_active = a;
}

// 802EBBC8
// One node's matrix from its state (as ComputeMatrices) times `parent`.
void EAnimController::CalcMatrix(EMat4& parent, ECharacterNode& node, EACNodeState& s, EMat4& out) {
  if (!(s.m_flags & EACNodeState::kAnimated)) {
    out = parent;
    return;
  }
  const bool bindFrame = (s.m_flags & EACNodeState::kScaled) && (s.m_flags & EACNodeState::kNonUniformScale) &&
                         node.m_flagB0 == 0;
  EMat4 local;
  NodeLocalMatrix(node, s, bindFrame, local);
  EMat4 r;
  r.Mult4x4(local, parent);
  out = r;
}

// 802EFC08
// The track `flags` names: the current main track if it carries a main-track flag of `flags`, else
// the secondary track of that one flag.
EACTrack* EAnimController::GetTrackAnimDef(eTrackFlags flags) {
  const int f = static_cast<int>(flags);
  int index;
  if (f & eTrackFlags::kMainMask) {
    index = m_mainTrack;
    const unsigned int trackFlags = index == -1 ? 0 : m_tracks[index].m_flags;
    if (!(trackFlags & static_cast<unsigned int>(f))) return nullptr;
  } else {
    switch (f) {
      case 0x100: index = m_mainCount; break;
      case 0x200: index = m_mainCount + 1; break;
      case 0x400: index = m_mainCount + 2; break;
      case 0x800: index = m_mainCount + 3; break;
      case 0x1000: index = m_mainCount + 4; break;
      case 0x2000: index = m_mainCount + 5; break;
      case 0x4000: index = m_mainCount + 6; break;
      case 0x8000: index = m_mainCount + 7; break;
      default: return nullptr;
    }
  }
  return &m_tracks[index];
}

// 802F0BFC
// Disabling computes the matrices once more and keeps a copy that is drawn from then on; enabling
// lets Compute run again.
void EAnimController::Enable(bool enable, EMat4& root) {
  if (((m_flags >> 1) & 1) == (enable ? 1u : 0u)) return;
  if (enable) {
    m_flags |= 2;
    return;
  }
  const int count = NodeCount();
  Compute(root, _framecount);
  if (!m_frozen) m_frozen = new EMat4[count > 0 ? count : 1];
  for (int i = 0; i < count; ++i) m_frozen[i] = m_matrices[i];
  m_flags &= ~2u;
  m_matrices = m_frozen;
}

// 802E8358
// Scale, then the rotations about x, y and z (where set), then the translation.
void EAnimController::CalcOrientMatrix(EVec3& pos, EVec3& rot, EVec3& scale, EMat4& out) {
  out.Scale(scale);
  if (rot.x != 0.0f) out.PostRotateX(rot.x);
  if (rot.y != 0.0f) out.PostRotateY(rot.y);
  if (rot.z != 0.0f) out.PostRotateZ(rot.z);
  out.PostTranslate(pos);
}

// 802EED14
// An active track starts over (its slaves follow).
void EAnimController::RestartTrack(eTrackFlags flags) {
  EACTrack* track = GetTrackAnimDef(flags);
  if (!track || track->m_active != 1) return;
  track->m_complete = 0;
  track->m_field3C = 0;
  track->m_pos = 0.0f;
  for (EACTrack* slave : track->m_slaves)
    if (slave) slave->m_pos = static_cast<float>(GetSlavePos(slave, track));
}

// 802E80A0
void EAnimController::SuspendTrack(EACTrack* track) {
  if (track && track->m_active == 1) track->m_flags |= eTrackFlags::kSuspended;
}

// 802E80C4
// A suspended track keeps its place but is not sampled (AnimateTrack) until resumed.
void EAnimController::SuspendTrack(eTrackFlags flags) { SuspendTrack(FindTrack(flags, false)); }

// 802E820C
void EAnimController::ResumeTrack(eTrackFlags flags) {
  EACTrack* track = FindTrack(flags, false);
  if (track && track->m_active == 1) track->m_flags &= ~eTrackFlags::kSuspended;
}

// 802F0678
// Node `index`'s matrix this frame; out of range or without matrices a shared identity (GC:
// unavailable_4020, with a one-shot warning flag).
EMat4* EAnimController::GetNodeMatrix(int index) {
  if (m_matrices && index >= 0 && index < NodeCount()) return &m_matrices[index];
  static EMat4 unavailable;
  unavailable.Id();
  return &unavailable;
}

// 802F0428
// Node `index`'s pivot (the controller's scale taken out) through its matrix: where the joint is this
// frame; 0 for an index out of range.
EVec3 EAnimController::CalcNodePos(int index) {
  if (index < 0 || index >= NodeCount()) return EVec3{0.0f, 0.0f, 0.0f};
  const EMat4& m = *GetNodeMatrix(index);
  const float inv = 1.0f / m_scale;
  const EVec3& pivot = m_character->m_nodes[static_cast<std::size_t>(index)].m_pivot;
  const float x = pivot.x * inv, y = pivot.y * inv, z = pivot.z * inv;
  return EVec3{z * m.m[2][0] + x * m.m[0][0] + y * m.m[1][0] + m.m[3][0],
               z * m.m[2][1] + x * m.m[0][1] + y * m.m[1][1] + m.m[3][1],
               z * m.m[2][2] + x * m.m[0][2] + y * m.m[1][2] + m.m[3][2]};
}

// 802EA53C
// Poses the character at `root` (Compute for this frame), then the box (min x y z, max x y z) around
// every node's joint position (only the nodes `use` marks, when given). No node: an empty box at 0.
void EAnimController::CalcTightBoundBox(EMat4& root, EBound3& box, bool* use) {
  bool first = true;
  Compute(root, _framecount);
  float* b = reinterpret_cast<float*>(&box);
  for (int i = 0; i < 6; ++i) b[i] = 0.0f;
  for (int i = 0; i < NodeCount(); ++i) {
    if (use && !use[i]) continue;
    const EVec3 p = CalcNodePos(i);
    if (first) {
      first = false;
      b[0] = b[3] = p.x, b[1] = b[4] = p.y, b[2] = b[5] = p.z;
      continue;
    }
    for (int a = 0; a < 3; ++a) {
      if (b[a] <= p[a]) {
        if (b[3 + a] < p[a]) b[3 + a] = p[a];
      } else {
        b[a] = p[a];
      }
    }
  }
}

// 802EFFCC
// The sphere the posed character fills at `root`: the skeleton's own sphere (ERCharacter +0x20),
// moved towards each animated track's sphere by the track's intensity (clamped to 0..1): the track's
// root translation now (the axes the track accumulates held at its start) with the animation's radius
// (ERAnim +0x40), grown around the running sphere for a layered track (flag 0x100); then offset by the
// root node's pivot, and the radius scaled and the centre carried by `root`.
void EAnimController::CalcVisibilitySphere(EMat4& root, EBoundSphere& out) {
  EBoundSphere sphere;
  if (m_character) {
    sphere.x = m_character->m_field20.x, sphere.y = m_character->m_field20.y, sphere.z = m_character->m_field20.z;
    std::memcpy(&sphere.r, &m_character->m_field2C, sizeof(float));
  }
  if (NodeCount() != 0) {
    for (int i = 0; i < m_trackCount && i < static_cast<int>(m_tracks.size()); ++i) {
      EACTrack& track = m_tracks[static_cast<std::size_t>(i)];
      if (!track.m_anim) continue;
      double w = track.m_intensity;
      if (w < 0.0) w = 0.0;
      if (1.0 < w) w = 1.0;
      if (w < 0.0) w = 0.0;
      if (w == 0.0) continue;
      const EVec3 trans = GetAnimRootNodeTrans(&track);
      EBoundSphere s;
      s.x = trans.x, s.y = trans.y, s.z = trans.z;
      float* c = &s.x;
      for (int a = 0; a < 3; ++a)
        if (track.m_rootFlags & (1u << a)) c[a] = track.m_rootStart[a];
      std::memcpy(&s.r, &track.m_anim->m_field40, sizeof(float));
      if (track.m_rootFlags & 0x100) s.Combine(sphere, s);
      sphere.x += static_cast<float>((s.x - sphere.x) * w);
      sphere.y += static_cast<float>((s.y - sphere.y) * w);
      sphere.z += static_cast<float>((s.z - sphere.z) * w);
      if (w < 0.0) w = -w;
      sphere.r = static_cast<float>(w * static_cast<float>(static_cast<double>(s.r) - sphere.r) + sphere.r);
    }
    const EVec3& pivot = m_character->m_nodes[0].m_pivot;
    sphere.x += pivot.x, sphere.y += pivot.y, sphere.z += pivot.z;
  }
  const float scale = root.GetMaxScale();
  const float* m0 = root.m[0];
  const float* m1 = root.m[1];
  const float* m2 = root.m[2];
  const float* m3 = root.m[3];
  out.r = sphere.r * scale;
  out.x = sphere.z * m2[0] + sphere.x * m0[0] + sphere.y * m1[0] + m3[0];
  out.y = sphere.z * m2[1] + sphere.x * m0[1] + sphere.y * m1[1] + m3[1];
  out.z = sphere.z * m2[2] + sphere.x * m0[2] + sphere.y * m1[2] + m3[2];
}

// 802EF4DC
// The track's current frame; 0 when it is not active.
double EAnimController::GetTrackFrame(eTrackFlags flags) {
  EACTrack* track = FindTrack(flags, false);
  return track && track->m_active == 1 ? track->m_pos : 0.0;
}

// 802F055C
// Node `index`'s matrix this frame (identity without matrices), the controller's scale taken out.
void EAnimController::CalcNodeMatrix(int index, EMat4& out) {
  if (index < 0 || index >= NodeCount()) {
    out.Id();
    return;
  }
  if (!m_matrices) out.Id();
  else out = m_matrices[index];
  if (m_scale != 1.0f) out.PreScale(1.0f / m_scale);
}

// 802F0700
// Node `index`'s matrix moved to its pivot (the bone's own frame at the joint; its local rotation is
// the node matrix's).
void EAnimController::CalcNodeOrientWithoutLocalRot(int index, EMat4& out) {
  if (index < 0 || index >= NodeCount()) {
    out.Id();
    return;
  }
  EMat4 node, pivot;
  CalcNodeMatrix(index, node);
  ECharacterNode* n = m_character->GetNode(index);
  if (!n) {
    pivot.Id();
  } else if (n->m_flagB0 == 0) {
    pivot.Id();
    pivot.PostTranslate(n->m_pivot);
  } else {
    pivot.Translate(n->m_pivot);
  }
  out.Mult4x4(pivot, node);
}

// 802F085C
// Node `index`'s orientation in the world: its bind matrix (+0x30) moved to its pivot (translated
// after, or before for flag +0xB0), through the node matrix.
void EAnimController::CalcNodeOrient(int index, EMat4& out) {
  if (index < 0 || index >= NodeCount()) {
    out.Id();
    return;
  }
  EMat4 node, local;
  CalcNodeMatrix(index, node);
  ECharacterNode* n = m_character->GetNode(index);
  if (!n) {
    local.Id();
  } else if (n->m_flagB0 == 0) {
    local = n->m_mat30;
    local.PostTranslate(n->m_pivot);
  } else {
    local.Translate(n->m_pivot);
  }
  out.Mult4x4(local, node);
}

// 802EDDCC
double EAnimController::GetTrackIntensity(eTrackFlags flags) {
  EACTrack* track = FindTrack(flags, false);
  return track && track->m_active == 1 ? track->m_intensity : 0.0;
}

// 802ED9AC
// Snapped (no blend); 0 deactivates the track.
void EAnimController::SetTrackIntensity(eTrackFlags flags, float intensity) {
  EACTrack* track = FindTrack(flags, false);
  if (track && track->m_active == 1) SetTrackIntensity(track, intensity, true);
}

// 802E9410
// The master's phase (its frame over its length) plus the offset, wrapped into one cycle, as a frame
// of the slave.
double EAnimController::GetSlavePos(EACTrack* slave, EACTrack* master) {
  float phase = master->m_pos / static_cast<float>(master->m_anim->m_frameCount - 1) + slave->m_phaseOffset;
  while (1.0f < phase) phase -= 1.0f;
  return phase * static_cast<float>(slave->m_anim->m_frameCount - 1);
}

// 802EEA60
// `slave` (active) follows `master`'s phase from now on (null: free again); the master keeps a list of
// its slaves.
void EAnimController::SetTrackPhaseLock(EACTrack* slave, EACTrack* master, float offset) {
  if (!slave || slave->m_active != 1) return;
  if (slave->m_slaveOf != -1) {
    std::vector<EACTrack*>& list = m_tracks[static_cast<size_t>(slave->m_slaveOf)].m_slaves;
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i] == slave) {
        list.erase(list.begin() + static_cast<long>(i));
        break;
      }
  }
  if (!master) {
    slave->m_slaveOf = -1;
  } else {
    // GC: an ERedBlackTree keyed by the slave's index.
    std::vector<EACTrack*>& list = master->m_slaves;
    size_t at = 0;
    while (at < list.size() && list[at]->m_index < slave->m_index) ++at;
    if (at == list.size() || list[at] != slave) list.insert(list.begin() + static_cast<long>(at), slave);
    slave->m_slaveOf = master->m_index;
  }
  slave->m_phaseOffset = offset;
}

// 802EB340
// Adds `from` on top of `to` by `t`: the rotation scaled by t then applied after to's, the
// translation added, the scale multiplied in.
void EAnimController::Layer(float t, EACNodeState& to, EACNodeState& from) {
  to.m_flags |= from.m_flags;
  EQuat q;
  q.Scale(t, from.m_rot);
  const float x = to.m_rot.x, y = to.m_rot.y, z = to.m_rot.z, w = to.m_rot.w;
  to.m_rot.x = q.x * w + q.y * z + (q.w * x - q.z * y);
  to.m_rot.w = q.w * w + ((-q.x * x - q.y * y) - q.z * z);
  to.m_rot.y = q.y * w + ((q.z * x + q.w * y) - q.x * z);
  to.m_rot.z = q.z * w + q.w * z + -q.y * x + q.x * y;
  to.m_trans.x = to.m_trans.x + t * from.m_trans.x;
  to.m_trans.y = to.m_trans.y + t * from.m_trans.y;
  to.m_trans.z = to.m_trans.z + t * from.m_trans.z;
  to.m_scale.x = to.m_scale.x + (to.m_scale.x * from.m_scale.x - to.m_scale.x) * t;
  to.m_scale.z = to.m_scale.z + (to.m_scale.z * from.m_scale.z - to.m_scale.z) * t;
  to.m_scale.y = to.m_scale.y + (to.m_scale.y * from.m_scale.y - to.m_scale.y) * t;
}

// 802EC5A0
void EAnimController::StopTrack(eTrackFlags flags) { DeactivateTrack(FindTrack(flags, false)); }

// 802EF230
// The per-node factors of an active track.
void EAnimController::SetTrackBlendFactors(eTrackFlags flags, float* factors) {
  EACTrack* t = FindTrack(flags, false);
  if (t && t->m_active == 1) t->m_blendFactors = factors;
}

// 802EE06C
// Every outgoing main track still showing fades to `target` over `time` (smoothly), unless it is
// already blending (with `includeKind4`, a track whose blend is of kind 4 is faded too).
void EAnimController::BlendAllOutgoingTracks(float target, float time, bool includeKind4) {
  for (int i = 0; i <= m_mainCount - 1; ++i) {
    EACTrack& t = m_tracks[static_cast<size_t>(i)];
    if (t.m_active != 1 || !(t.m_flags & eTrackFlags::kInBlendList) || !(0.0f < t.m_intensity)) continue;
    if (!includeKind4) {
      if (t.m_blending == 0) SetTrackBlendSmooth(&t, target, time, 0.01f);
    } else if (t.m_blending == 0 || t.m_blending == 4) {
      SetTrackBlendSmooth(&t, target, time, 0.01f);
    }
  }
}

// 802EEAFC
// A track driven by callbacks instead of an animation (`update` each Update, `animate` writing the
// node states each Compute, both given `data`); the same callbacks again only change the data.
void EAnimController::SetProceduralTrack(eTrackFlags flags, EACTrack::UpdateFn update, EACTrack::AnimateFn animate,
                                         uintptr_t data) {
  EACTrack* t = FindTrack(flags, false);
  if (!t) return;
  if (t->m_procUpdate == update && t->m_procAnimate == animate) {
    t->m_procData = data;
    return;
  }
  DestroyStreams(t);
  if (t->m_anim) {
    t->m_anim->DelRef();
    t->m_anim = nullptr;
  }
  t->m_procUpdate = update;
  t->m_procAnimate = animate;
  t->m_procData = data;
  t->m_flags = static_cast<unsigned int>(static_cast<int>(flags));
  t->m_active = 0;
  t->m_anim = nullptr;
  t->m_blending = 0;
  t->m_streams = nullptr;
  t->m_blendFactors = nullptr;
  t->m_animId = 0;
  t->m_slaveOf = -1;
  t->m_phaseOffset = 0.0f;
  t->m_intensity = 1.0f;
  t->m_pos = 0.0f;
  t->m_prevPos = 0.0f;
  t->m_speed = 1.0f;
  t->m_fps = 1.0f;
  t->m_blendT = 0.0f;
  SetTrackActive(t, true);
}
