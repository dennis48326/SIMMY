// @class EACNodeState after ERAnim ECharacterNode EQuat EMat4 EVec3
// @class eTrackFlags
// @class eAnimatedObjectType
// @class EACTrackStreams
// @class EACTrack
// @class EAnimController
// Skeletal animation playback (engine/e_animcontroller.cpp). A controller animates one character
// (ERCharacter): it has a few "main" tracks that cross-fade into each other (one is current, the
// others are in a blend list) and, for people, secondary tracks layered or blended on top (head,
// hands, ...). Update advances every active track's frame (looping, holding or stopping at the end,
// accumulating root motion); Compute samples each track into per-node states (rotation, translation,
// scale and "special" channels: visibility, a vector and three scalars), blends them, and builds the
// node matrices from the skeleton.
#pragma once
#include <cstdint>
#include <vector>

struct eTrackFlags {
  SIMMY_ENUM_BODY(eTrackFlags)
  // 0x01..0x80 and 0x10000 name main tracks; 0x100..0x8000 the secondary ones (one bit each).
  static constexpr int kMainMask = 0x100ff;
  static constexpr int kMain = 0x10000;          // EACTrack::m_flags: the current main track
  static constexpr int kStarted = 0x20000;       // set by SetTrackAnim
  static constexpr int kInBlendList = 0x40000;   // an outgoing main track still fading
  static constexpr int kStaticBlend = 0x80000;
  static constexpr int kReverse = 0x4000000;
  static constexpr int kSuspended = 0x8000000;
};

struct eAnimatedObjectType {
  SIMMY_ENUM_BODY(eAnimatedObjectType)
};

// The state one track gives a node (0x44 bytes).
struct EACNodeState {
  enum : unsigned int {
    kAnimated = 1, kScaled = 2, kNonUniformScale = 4, kHidden = 8, kKeepVisibility = 0x10,
    kHasVector = 0x20, kHasScalar0 = 0x40, kHasScalar1 = 0x80, kHasScalar2 = 0x100
  };
  unsigned int m_flags = 0;        // +0x00
  EQuat m_rot;                     // +0x04
  EVec3 m_trans;                   // +0x14 (after ComputeMatrices: the node's world position)
  EVec3 m_scale{1, 1, 1};          // +0x20
  EVec3 m_vector{1, 1, 1};         // +0x2C special channel (e.g. a colour)
  float m_scalar0 = 1.0f;          // +0x38
  float m_scalar1 = 10.0f;         // +0x3C
  float m_scalar2 = 0.0f;          // +0x40 (cameras: the field of view)
};

// One node's decompressors (0x20 bytes): rotation, translation, scale, visibility, the vector channel
// and the three scalar channels; null where the channel is constant.
struct EACTrackStreams {
  ERotDecomp* m_rot = nullptr;        // +0x00
  EVec3Decomp* m_trans = nullptr;     // +0x04
  EVec3Decomp* m_scale = nullptr;     // +0x08
  EVisInfoDecomp* m_vis = nullptr;    // +0x0C
  EVec3Decomp* m_vector = nullptr;    // +0x10
  ScalarDecomp* m_scalar[3] = {};     // +0x14
};

// 0xB0 bytes. The first 0x24 bytes are copied from the animation (ERAnim +0x64) when one is set.
struct EACTrack {
  EACTrack();  // 802E76A8

  // PC: the callbacks' data is pointer-sized (GC: an unsigned int holding the owner's address).
  using UpdateFn = void (*)(uintptr_t, EACTrack*, ERCharacter*, EVec3&, EVec3&);
  using AnimateFn = void (*)(uintptr_t, EACTrack*, EMat4&, ERCharacter*, EACNodeState*);

  float m_fps = 1.0f;                 // +0x00 frames per second
  float m_defaultIntensity = 1.0f;    // +0x04
  unsigned int m_rootFlags = 0xfb;    // +0x08 bits 0-2: root translation axes moved by the track
                                      //       (accumulated, not animated); 0x100: layered
  unsigned char m_field0C = 3;        // +0x0C
  unsigned char m_endMode = 0;        // +0x0D at the end: 0 loop, 1 hold, 2 stop
  unsigned char m_field0E = 1;        // +0x0E
  unsigned int m_field10 = 0;         // +0x10
  unsigned int m_field14 = 0;         // +0x14
  float m_field18 = 0.5f;             // +0x18
  float m_field1C = 0.08f;            // +0x1C
  float m_field20 = 0.01f;            // +0x20
  UpdateFn m_procUpdate = nullptr;    // +0x24 procedural tracks (no animation)
  AnimateFn m_procAnimate = nullptr;  // +0x28
  uintptr_t m_procData = 0;           // +0x2C
  unsigned int m_flags = 0;           // +0x30 eTrackFlags; bits 28-31: slot in the blend list
  int m_index = 0;                    // +0x34
  int m_frameSetAt = 0;               // +0x38 _framecount when the frame was set explicitly
  int m_field3C = 0;                  // +0x3C
  float m_pos = 0.0f;                 // +0x40 current frame
  float m_prevPos = 0.0f;             // +0x44
  float m_field48 = 0.0f;             // +0x48
  float m_speed = 1.0f;               // +0x4C
  float m_intensity = 1.0f;           // +0x50
  float m_field54 = 0.0f;             // +0x54 a smooth blend's time
  float m_blendEpsilon = 0.0f;        // +0x58 a smooth blend snaps to its target this close
  float m_blendT = 0.0f;              // +0x5C how far the blend is (0..1)
  float m_blendFrom = 0.0f;           // +0x60 the intensity a Hermite blend started at
  float m_blendTo = 0.0f;             // +0x64 the intensity the blend goes to
  float m_blendTime = 0.0f;           // +0x68 seconds the blend takes
  float m_hermite0 = 0.0f;            // +0x6C a Hermite blend's tangents
  float m_hermite1 = 0.0f;            // +0x70
  int m_slaveOf = -1;                 // +0x74 track index this one is phase-locked to
  float m_phaseOffset = 0.0f;         // +0x78 a phase-locked track's offset (fraction of a cycle)
  ERAnim* m_anim = nullptr;           // +0x7C
  EVec3 m_rootStart;                  // +0x80 the root translation at frame 0
  std::vector<EACTrack*> m_slaves;    // +0x8C ERedBlackTree of phase-locked tracks
  EACTrackStreams* m_streams = nullptr;  // +0x98 one per node
  float* m_blendFactors = nullptr;    // +0x9C per node
  unsigned char m_blending = 0;       // +0xA0 0 none, 1 Hermite, 2 smooth
  int m_complete = 0;                 // +0xA4 reached the end
  int m_active = 0;                   // +0xA8
  unsigned int m_animId = 0;          // +0xAC
};

struct EAnimController {
  EAnimController();  // 802E77A4
  ~EAnimController();  // 802E7884
  EAnimController(const EAnimController&) = delete;
  EAnimController& operator=(const EAnimController&) = delete;
  void Deallocate();  // 802E78E4
  void DeallocateNodes();  // 802E79F4
  void Init(unsigned int character, eAnimatedObjectType type, unsigned int flags);  // 802E7AC8
  void Init(char* name, eAnimatedObjectType type, unsigned int flags);  // 802E7EF4
  void Shutdown();  // 802E7F40
  bool IsTrackValid(eTrackFlags);  // 802E7F60
  void SuspendTrack(EACTrack *);  // 802E80A0
  void SuspendTrack(eTrackFlags);  // 802E80C4
  void ResumeTrack(eTrackFlags);  // 802E820C
  static void CalcOrientMatrix(EVec3& pos, EVec3& rot, EVec3& scale, EMat4& out);  // 802E8358
  void Update(EVec3* pos, EVec3* rot, EVec3 scale);  // 802E83EC
  void ProcessEvents(TNodeList<EACEventRange *> &, bool);  // 802E8864
  double AddEventRange(TNodeList<EACEventRange *> &, EACTrack *, float, float);  // 802E89EC
  void UpdateTrack(EACTrack* track, EVec3& rootDelta, EVec3& rotDelta);  // 802E8A58
  // The frame of phase-locked `slave` from its master's phase plus its offset.
  double GetSlavePos(EACTrack* slave, EACTrack* master);  // 802E9410
  void GetAnimDistance(eTrackFlags, EVec3 &);  // 802E94A8
  RET_UNKNOWN GetAnimTrans(eTrackFlags, EVec3 &);  // 802E94D8
  void GetAnimTranslate(eTrackFlags, EVec3 &, EVec3 &);  // 802E96A8
  double GetAnimTime(eTrackFlags);  // 802E9960
  static double GetAnimVelocity(eTrackFlags, EVec3 &);  // 802E9B04
  int GetFirstRelevantTrack();  // 802E9B68
  void PrintTracks();  // 802E9C0C
  int AnimateTrack(EACTrack* track, EACNodeState* states, EMat4* root);  // 802E9C24
  void Compute(EMat4& root, int frame);  // 802E9CDC
  void AllocMatrices();  // 802EA128
  void ComputeInverse(EMat4 &, int);  // 802EA1DC
  void PrepareForCollision(EMat4 &);  // 802EA28C
  void PostMultNodeAndDescendents(int, EMat4 &);  // 802EA2CC
  void PreMultNodeAndDescendents(int, EMat4 &);  // 802EA404
  void CalcTightBoundBox(EMat4 &, EBound3 &, bool *);  // 802EA53C
  void FixAccumulationForRootNode(EACTrack* track, EACNodeState& state);  // 802EA698
  void AnimateSpecial(ERAnim* anim, float frame, EACTrackStreams* streams, EACNodeState* stored,
                      EAnimNodeDataPos* node, EACNodeState& state);  // 802EA6DC
  void BlendNodeArrays(EACTrack* track, EACNodeState* from, EACNodeState* to);  // 802EA9E0
  void Animate(EACTrack* track, EACNodeState* states);  // 802EAD18
  static void Blend(float t, EACNodeState& to, EACNodeState& from);  // 802EB1B0
  static void Layer(float t, EACNodeState& to, EACNodeState& from);  // 802EB340
  static void CallbackIntermediateComputeMatrices(EMat4 &);  // 802EB53C
  void ComputeMatrices(EMat4& root);  // 802EB570
  void CalcMatrix(EMat4& parent, ECharacterNode& node, EACNodeState& state, EMat4& out);  // 802EBBC8
  void StopAllTracks();  // 802EBFD8
  void DeactivateTrack(EACTrack* track);  // 802EC038
  void CreateStreams(EACTrack* track);  // 802EC198
  void DestroyStreams(EACTrack* track);  // 802EC4B4
  void StopTrack(eTrackFlags);  // 802EC5A0
  void TransferTrack(eTrackFlags, eTrackFlags);  // 802EC6E8
  unsigned int GetTrackAnimId(eTrackFlags flags);  // 802EC990
  void SetStaticBlendTrackAnim(eTrackFlags, unsigned int, float, int, float, EACTrack *);  // 802ECAF8
  EACTrack* SetTrackAnim(eTrackFlags flags, unsigned int animId, int blendType, float blendTime,
                         EACTrack** outgoing);  // 802ECF0C
  RET_UNKNOWN SetTrackAnim(eTrackFlags, char *);  // 802ED7EC
  RET_UNKNOWN GetTrackAnimName(eTrackFlags);  // 802ED840
  void SetTrackIntensity(eTrackFlags, float);  // 802ED9AC
  void SetTrackIntensity(EACTrack* track, float intensity, bool snap);  // 802EDB44
  void SetAllTrackIntensities(float);  // 802EDBF0
  double GetTrackBlendTarget(eTrackFlags);  // 802EDC70
  double GetTrackIntensity(eTrackFlags);  // 802EDDCC
  double GetTrackSpeed(eTrackFlags flags);  // 802EDF1C
  void BlendAllOutgoingTracks(float target, float time, bool includeKind4);  // 802EE06C
  void SetTrackBlend(EACTrack* track, float target);  // 802EE17C
  static void SetTrackBlend(eTrackFlags, float);  // 802EE1DC
  void SetTrackBlendSmooth(EACTrack* track, float target, float time, float epsilon);  // 802EE324
  void SetTrackBlendSmooth(eTrackFlags flags, float target, float time, float epsilon);  // 802EE390
  void SetTrackBlendHermiteSafe(EACTrack *, float, float, float, float);  // 802EE4D8
  void SetTrackBlendHermiteSafe(eTrackFlags, float, float, float, float);  // 802EE4F8
  void SetTrackBlendHermite(EACTrack* track, float target, float time, float t0, float t1);  // 802EE640
  RET_UNKNOWN SetTrackBlendHermite(eTrackFlags, float, float, float, float);  // 802EE6F8
  void BlendTrackIntensity(EACTrack* track);  // 802EE854
  void SetTrackPhaseLock(EACTrack* slave, EACTrack* master, float offset);  // 802EEA60
  void SetProceduralTrack(eTrackFlags flags, EACTrack::UpdateFn update, EACTrack::AnimateFn animate, uintptr_t data);  // 802EEAFC
  void RestartTrack(eTrackFlags flags);  // 802EED14
  void SetTrackPhase(eTrackFlags, float);  // 802EEEC0
  void SetTrackFrame(eTrackFlags flags, float frame, bool markSet);  // 802EF040
  void SetTrackBlendFactors(EACTrack* track, float* factors);  // 802EF214
  void SetTrackBlendFactors(eTrackFlags, float *);  // 802EF230
  RET_UNKNOWN GetTrackFrameCount(eTrackFlags);  // 802EF374
  double GetTrackFrame(eTrackFlags);  // 802EF4DC
  double GetTrackPos(eTrackFlags flags);  // 802EF62C
  void SetTrackPos(EACTrack* track, float phase, bool markSet);  // 802EF7B8
  void SetTrackPos(eTrackFlags flags, float phase, bool markSet);  // 802EF820
  int IsTrackAnimComplete(eTrackFlags flags);  // 802EF968
  void ClearTrackAnimComplete(eTrackFlags);  // 802EFAC0
  EACTrack* GetTrackAnimDef(eTrackFlags flags);  // 802EFC08
  void SetTrackSpeed(EACTrack* track, float speed);  // 802EFD44
  void SetTrackSpeed(eTrackFlags flags, float speed);  // 802EFD88
  void SetAllTrackSpeed(float speed);  // 802EFED0
  RET_UNKNOWN VisibilityTest(EMat4 &);  // 802EFF38
  void CalcVisibilitySphere(EMat4 &, EBoundSphere &);  // 802EFFCC
  EVec3 GetAnimRootNodeTrans(EACTrack* track);  // 802F0300
  // `model` posed by this controller at `root` (Compute, then ERModel::DrawHierarchical).
  void Draw(ERC* rc, ERModel* model, EMat4& root);  // 802F03B4
  RET_UNKNOWN CalcNodePos(int);  // 802F0428
  void CalcNodeMatrix(int, EMat4 &);  // 802F055C
  RET_UNKNOWN GetNodeMatrix(int);  // 802F0678
  void CalcNodeOrientWithoutLocalRot(int, EMat4 &);  // 802F0700
  void CalcNodeOrient(int, EMat4 &);  // 802F085C
  void SetNodeIgnoreAnimatedVisibility(int, bool);  // 802F0A30
  void SetNodeVisible(int, bool);  // 802F0A8C
  void SetTrackActive(EACTrack* track, bool active);  // 802F0AE8
  void Enable(bool enable, EMat4& root);  // 802F0BFC
  void CalcBillinearCoeff(float, float, float, float &, float &, float &, float &);  // 802F0CCC
  int GetNonMainTrackFlagFromIndex(int index);  // 803C357C

  // The track for `flags`: the current main track for main-track flags, else the secondary slot.
  // (Inlined in every eTrackFlags accessor of the original.)
  EACTrack* FindTrack(eTrackFlags flags, bool mainNeedsFlag);
  int NodeCount() const { return m_character ? m_character->GetNodeCount() : 0; }

  unsigned int m_flags = 2;           // +0x00 2: enabled; the rest from Init
  unsigned int m_nodeFlags = 0;       // +0x04 the union of the node states' flags this frame
  int m_mainTrack = -1;               // +0x08
  EACNodeState* m_states = nullptr;   // +0x0C one per node
  EMat4* m_matrices = nullptr;        // +0x10 this frame's node matrices
  void* m_field14 = nullptr;          // +0x14
  EMat4* m_frozen = nullptr;          // +0x18 disabled (Enable): the matrices it stopped at
  EMat4* m_matrixBuffers[2] = {};     // +0x1C by _evenodd
  ERCharacter* m_character = nullptr; // +0x24
  int m_updatedAt = 0;                // +0x28 _framecount of the last Update
  int m_matricesAt = -1;              // +0x2C of the last AllocMatrices
  int m_computedAt = -1;              // +0x30 of the last Compute
  int m_field34 = -1;                 // +0x34
  int m_mainCount = 0;                // +0x38 main tracks
  int m_secondaryCount = 0;           // +0x3C
  int m_trackCount = 0;               // +0x40
  std::vector<EACTrack> m_tracks;     // +0x44
  int m_field48 = 0;                  // +0x48 TNodeList (event ranges)
  int m_field4C = 0;                  // +0x4C
  int m_field50 = 1;                  // +0x50
  float m_speed = 1.0f;               // +0x54
  float m_scale = 1.0f;               // +0x58
  int m_nonUniformScale = 0;          // +0x5C a node is scaled unevenly this frame
  int m_computeEnabled = 1;           // +0x60
  void (*m_computeCallback)(unsigned int, EMat4&, ERCharacter*, EMat4*) = nullptr;  // +0x64
  unsigned int m_computeCallbackData = 0;  // +0x68
  unsigned int m_blendCount = 0;      // +0x6C outgoing main tracks
  std::vector<signed char> m_blendList;  // +0x70 their track indices (-1 = free)
  int m_forceCompute = 0;             // +0x74
};
