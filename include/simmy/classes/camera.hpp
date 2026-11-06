// @class CameraDirector after E3DWindow EACNodeState EMat4 EVec3 Quad
// @class CameraMotionSystem
// @class NewControlParms
// @class ESimsCam
// @class ESimsCam__CameraMode
// @class ESimsCam__CameraParameters
// @class SimsCameraParameters
// @class CamDirInterpType
// @class anim__PropKind
// The game camera (game/camera.cpp). ESimsCam is a player's camera: a 3D window plus the eye, target
// and up it looks along, a zoom (distance), rotation and tilt with "actual" values that follow the
// targets, and camera noise. Its tuning comes from ESimsCam::s_params (one SimsCameraParameters per
// player). CameraDirector takes a camera over for scripted shots: camera animations (a "cam_base"
// character whose root node is the camera and whose third scalar channel is the field of view),
// cuts and interpolations, then hands the camera back.
#pragma once

#include "simmy/classes/quad.hpp"  // (ESimsCam holds two)

// How CameraDirector interpolates: 0 at the speed the distance needs over the time, 1 over the time.
struct CamDirInterpType {
  SIMMY_ENUM_BODY(CamDirInterpType)
};

// An animation note's kind (anim::PropKind; EAnimNote +0x04): 5 a sound, 6 an event number, 0xD bloom,
// 0xE motion blur, 0xF depth of field, ...
struct anim__PropKind {
  SIMMY_ENUM_BODY(anim__PropKind)
};

// Camera tuning (0x1C8 bytes; GC vtable at +0x1C4).
struct SimsCameraParameters {
  SimsCameraParameters();  // 80015BE8
  virtual ~SimsCameraParameters();  // 80016010
  void Validate();  // 80016044

  int m_index = 0;                  // +0x000 camera number (_camParamNum)
  int m_field04 = 0;            // +0x004
  float m_field008 = 0;           // +0x008
  float m_field00C = 0;           // +0x00C
  float m_field010 = 0;           // +0x010
  float m_field014 = 0;           // +0x014
  float m_field018 = 0;           // +0x018
  float m_field01C = 0;           // +0x01C
  float m_field020 = 0;           // +0x020
  float m_field024 = 0;           // +0x024
  float m_zoomMin = 0;              // +0x028 camera distance range
  float m_zoomMax = 0;              // +0x02C
  float m_field030 = 0;           // +0x030
  float m_field034 = 0;           // +0x034
  float m_field038 = 0;           // +0x038
  float m_field03C = 0;           // +0x03C
  float m_field040 = 0;           // +0x040
  float m_field044 = 0;           // +0x044
  float m_field048 = 0;           // +0x048
  float m_field04C = 0;           // +0x04C
  float m_field050 = 0;           // +0x050
  float m_field054 = 0;           // +0x054
  float m_field058 = 0;           // +0x058
  float m_field05C = 0;           // +0x05C
  float m_fovMin = 0;               // +0x060 field of view (degrees) at the near and far zoom
  float m_fovMax = 0;               // +0x064
  float m_farPlane = 0;             // +0x068
  float m_nearPlane = 0;            // +0x06C
  float m_nearPlaneZoomed = 0;      // +0x070
  float m_field074 = 0;           // +0x074
  float m_field078 = 0;           // +0x078
  float m_field07C = 0;           // +0x07C
  float m_field080 = 0;           // +0x080
  float m_field084 = 0;           // +0x084
  float m_field088 = 0;           // +0x088
  float m_targetHeight = 0;         // +0x08C target z in the free camera
  float m_field090 = 0;           // +0x090
  float m_field094 = 0;           // +0x094
  float m_field098 = 0;           // +0x098
  float m_field09C = 0;           // +0x09C
  float m_field0A0 = 0;           // +0x0A0
  float m_field0A4 = 0;           // +0x0A4
  float m_field0A8 = 0;           // +0x0A8
  float m_field0AC = 0;           // +0x0AC
  float m_field0B0 = 0;           // +0x0B0
  float m_field0B4 = 0;           // +0x0B4 = m_field038
  float m_field0B8 = 0;           // +0x0B8 = m_zoomMin
  EVec3 m_refFrom;                // +0x0BC rotation angles are measured from the direction
  EVec3 m_refTo;                  // +0x0C8 m_refTo - m_refFrom
  float m_field0D4 = 0;           // +0x0D4
  float m_field0D8 = 0;           // +0x0D8
  float m_field0DC = 0;           // +0x0DC
  int m_noiseEnabled[7] = {};     // +0x0E0 camera noise settings (CameraNoiseSetting)
  struct Noise { float amp[3]; float period; };
  Noise m_noise[7] = {};          // +0x0FC amplitude and period of each noise setting
  float m_noiseSpeed[7] = {};     // +0x16C
  int m_noisePriority[7] = {};    // +0x188
  int m_noiseUseAnim[7] = {};     // +0x1A4
  int m_field1C0 = 0;             // +0x1C0
};

// Moves the real camera toward the rig the camera computes (0x5C bytes). Type 0 snaps, 1 rubber-bands
// (each of eye and target eases by the tuning's +0x50 far from it / +0x54 within +0x4C). Nothing in the
// game sets the type, so the camera always snaps.
struct CameraMotionSystem {
  explicit CameraMotionSystem(SimsCameraParameters* params);  // 8001606C
  ~CameraMotionSystem();  // 80016080
  void SetMotionType(int type);  // 800160A8
  int GetMotionType();  // 800160B0
  void SetRig(EVec3& eye, EVec3& target, EVec3& up);  // 800160B8
  void SnapCamera();  // 80016110
  void MoveCamera(float dt);  // 8001621C
  void RubberBandInterpolate(EVec3* value, EVec3 target, float dt);  // 80016258
  void RubberBandMove(float dt);  // 8001639C

  int m_type = 0;                          // +0x00 CameraMotionType
  SimsCameraParameters* m_params = nullptr;  // +0x04
  EVec3 m_rigEye;                          // +0x08 where the camera should be
  EVec3 m_rigTarget;                       // +0x14
  EVec3 m_rigUp;                           // +0x20
  EVec3 m_eye;                             // +0x2C where it is
  EVec3 m_target;                          // +0x38
  EVec3 m_up;                              // +0x44
  EVec3 m_forward;                         // +0x50 normalized m_target - m_eye
};

// The classic cursor's camera tuning (ESimsCam::s_newControlParms, 0xC4 bytes, GC vtable at +0xC0). Only
// the fields the camera reads are kept; the constructor's other values (+0x00..+0x78) are unused.
struct NewControlParms {
  NewControlParms();  // 8001E6DC
  virtual ~NewControlParms();  // 8039DADC
  void Validate();  // 8001E878

  int m_followArea = 1;          // +0x2C 1: the camera follows a cursor that leaves its area (+0x360); 0: the cursor is pinned to it
  float m_followRate = 0.6f;     // +0x7C the share of the cursor's overshoot the target moves per frame
  float m_trap[4] = {};          // +0x80 the area on screen: top y, top width (by zoom: +0x90..+0x94), bottom y, bottom width
  float m_topWidthNear = 0.5f;   // +0x90
  float m_topWidthFar = 0.45f;   // +0x94
  float m_depthNear = 3.0f;      // +0x98 the area's depth on the ground (the top corners), by zoom
  float m_depthFar = 5.0f;       // +0x9C
  int m_centerArea = 1;          // +0xA0 CenterOnSelectedSim: without an interactor, pull the target into +0x390
  int m_fieldA4 = 0;             // +0xA4
  float m_centerPull = 0.3f;     // +0xA8 CenterOnSelectedSim: the share of the overshoot the target moves
  float m_areaScale = 1.0f;      // +0xAC UpdateWin's ground area (+0x390): along the view, front and back offsets
  float m_areaFront = 0.5f;      // +0xB0
  float m_areaFrontWidth = 1.25f;  // +0xB4
  float m_areaBack = -0.5f;      // +0xB8
  float m_areaBackWidth = 1.25f;   // +0xBC
};

// 0x570 bytes (GC vtable at +0x56C).
// 0: the camera follows the player's cursor, 1: the selected sim, 2 (SetMode): the previous mode.
struct ESimsCam__CameraMode {
  SIMMY_ENUM_BODY(ESimsCam__CameraMode)
};

// Where a camera looks (ESimsCam::CameraParameters, 0x18 bytes): target, distance, angles (degrees).
struct ESimsCam__CameraParameters {
  EVec3 m_target;
  float m_zoom = 0;
  float m_rotAngle = 0;
  float m_tilt = 0;
};

struct ESimsCam {
  using CameraMode = ::ESimsCam__CameraMode;
  using CameraParameters = ::ESimsCam__CameraParameters;
  explicit ESimsCam(int player);  // 800164D4
  virtual ~ESimsCam();  // 8001669C
  ESimsCam(const ESimsCam&) = delete;
  ESimsCam& operator=(const ESimsCam&) = delete;
  float GetCurZoomRatio();  // 80016728
  float GetNearPlane();  // 80016770
  float GetFarPlane();  // 80016990
  float GetFov();  // 8001699C
  void SetFov(float fov);  // 800169E8
  void ForceFullScreenViewport();  // 80016A1C
  void Init();  // 80016A8C
  void Reset();  // 80016AB8
  void SetState(int panelState);  // 80016B54 (Panelstateman::Panelstate)
  void SetMode(ESimsCam__CameraMode);  // 80016BD4
  void FollowPlayerInteractor();  // 80016C34
  virtual void Update();  // 80016DB8
  void ApplyPan(float);  // 80016FB8
  void ApplyZoom(float delta);  // 80016FFC
  void SetZoom(float zoom);  // 80017070
  int HandleRotation();  // 8001709C
  int HandleZoom();  // 800172E8
  void PullTargetToCursor(float rate);  // PC helper: the cursor pull shared by HandleRotation / HandleZoom
  int PcKeyboardMouseCamera();  // PC: the mouse's turn and zoom, the pan keys (pc_kbm.cpp)
  void UpdateWin();  // 80017490
  double CalcTiltFromZoom(float);  // 80017A90
  void CalcEyePosition(EVec3& eye, ESimsCam__CameraParameters& params);  // 80017ABC
  void ResetPos(float);  // 80017DC0
  void FollowSimStiff();  // 80017EEC
  bool IsInDeadZone(EVec3 &) const;  // 800183F4
  float CalcTranslationStiffness() const;  // 800184A0
  float CalcRotationStiffness() const;  // 800184D8
  float CalcZoomStiffness() const;  // 80018510
  void InterpolateActualCameraParamsToTarget(float);  // 80018548
  void SnapActualCameraParamsToTarget();  // 800187E8
  void CenterOnSelectedSim();  // 80018824
  void GetPos(EVec3& eye, EVec3& target, EVec3& up);  // 80018E1C
  void SetPosAndReset(EVec3 &, EVec3 &, EVec3 &);  // 80018E74
  void SetPos(EVec3& eye, EVec3& target, EVec3& up);  // 80018FE4
  void ForcePosition(EVec3 &, EVec3 &, EVec3 &);  // 8001916C
  void ForceTarget(EVec3 &);  // 800192FC
  int PointOffScreen(EVec3& p);  // 80019364
  int PointOutsideScreenRect(EVec3& p, float left, float top, float right, float bottom);  // 80019398
  void ValidateCursorPosition(int player, EVec3* pos);  // 8001942C
  unsigned short CalRoomId(EVec3& pos);  // 80019628
  unsigned short GetContainingRoomId(EVec3& pos);  // 8001976C
  RET_UNKNOWN CursorMoved(int, EVec3 &);  // 80019A00
  void SetWinPos(E3DWindow& win);  // 80019A04
  static float CalcPitch(EVec3& dir);  // 80019DFC
  static float CalcZAxisTheta(EVec3& dir);  // 80019EA8
  EVec3 GetCursorPos();  // 80019F6C
  double GetCursorSpeed();  // 8001A00C
  float GetCursorLimitTopWidth();  // 8001A03C
  float GetCursorLimitDepth();  // 8001A074
  void GetObjectPosition(EVec3 *, cXObject *);  // 8001A0AC
  void GetObjectOrientation(EVec3 *, cXObject *);  // 8001A168
  void SetAnimationMode(CameraAnimationMode, cXObject *, cXObject *, bool);  // 8001A22C
  RET_UNKNOWN DrawDebug(ERC *);  // 8001A24C
  int CursorNotActive();  // 8001A2B8
  void SetActiveNoiseSetting(int setting);  // 8001A328
  EVec3 ApplyNoise(EVec3 eye, EVec3 target, EVec3 up);  // 8001A424
  void DrawCameraInfo(ERC *);  // 8001AA74
  RET_UNKNOWN SetControllerFilterId(unsigned int);  // 8001AD94
  RET_UNKNOWN GetControllerFilterId() const;  // 8001ADA0
  RET_UNKNOWN ClearControllerFilterId();  // 8001ADA8
  float ReadControllerZoom();  // 8001ADB8
  float ReadControllerRotation();  // 8001AE0C
  ESimsCam();  // 8001E944
  RET_UNKNOWN GetEye() const;  // 8001EA2C
  RET_UNKNOWN GetTarget() const;  // 8001EA34
  RET_UNKNOWN GetUp() const;  // 8001EA3C
  RET_UNKNOWN GetForward() const;  // 8001EA44
  void SetTarget(EVec3 &);  // 8001EA4C
  E3DWindow* GetWin();  // 8001EA6C
  double GetZoom() const;  // 8001EA74
  double GetTilt() const;  // 8001EA7C
  void SetTilt(float);  // 8001EA84
  RET_UNKNOWN GetbMoved() const;  // 8001EA8C
  double GetRotAng() const;  // 8001EA94
  RET_UNKNOWN GetMode() const;  // 8001EA9C
  int GetPlayerId();  // 8001EAA4
  void ForceFullScreen(bool);  // 8001EAAC
  RET_UNKNOWN IsForceFullScreen();  // 8001EAB4
  void SetCenterNew(bool);  // 8001EABC
  RET_UNKNOWN GetRoomId();  // 8001EAC4
  RET_UNKNOWN GetRoomWallsOpaque();  // 8001EACC
  void SetInterpolatingToOrFromSocialModeCameraFlag(bool);  // 8001EAD4
  void SetInterpolatingFromLotIntroCameraFlag(bool);  // 8001EADC

  static SimsCameraParameters s_params[2];  // 804735D8
  static int m_modeDef;  // 804FD974 the mode cameras start in
  static NewControlParms s_newControlParms;  // 80473514

  SimsCameraParameters* m_params = nullptr;  // +0x000
  int m_player = 0;                    // +0x004
  int m_moved = 0;                     // +0x008 rotated or zoomed this frame
  E3DWindow m_win;                     // +0x010
  Quad m_cursorLimit;                  // +0x360 the ground under the screen area the classic cursor moves in
                                       //        without moving the camera (ValidateCursorPosition)
  Quad m_cursorQuad;                   // +0x390 a ground area around the target (UpdateWin; CenterOnSelectedSim)
  int m_mode = 1;                      // +0x3CC ESimsCam::CameraMode (0 follow the cursor, 1 the sim)
  int m_prevMode = 1;                  // +0x3D0
  int m_field3D4 = 0;                  // +0x3D4
  EVec3 m_eye;                         // +0x418
  EVec3 m_up;                          // +0x424
  EVec3 m_field430;                    // +0x430
  EVec3 m_target;                      // +0x43C
  float m_zoom = 0;                    // +0x448 distance
  float m_rotAngle = 0;                // +0x44C degrees about z
  float m_tilt = 0;                    // +0x450 degrees
  EVec3 m_actualTarget;                // +0x454 the values the camera is at, following the above
  float m_actualZoom = 0;              // +0x460
  float m_actualRotAngle = 0;          // +0x464
  float m_actualTilt = 0;              // +0x468
  int m_field46C = 0;                  // +0x46C
  float m_field470 = 0;                // +0x470
  int m_field474 = 1;                  // +0x474
  int m_snap = 0;                      // +0x478 snap the motion system next time
  EVec3 m_noiseDir;                    // +0x47C
  EVec3 m_noiseStep;                   // +0x488
  EVec3 m_noiseBase;                   // +0x494
  float m_noiseTime = 0;               // +0x4A0
  float m_noiseLen = 0;                // +0x4A4
  int m_noiseSetting = -1;             // +0x4A8 active CameraNoiseSetting
  unsigned int m_noiseAnims[7] = {0x880340c4, 0xff047052, 0x660d21e8, 0x110a117e,
                                  0x6ffe7e2e, 0x6f8552f4, 0xad04db0a};  // +0x4AC
  EAnimController m_noiseAnim;         // +0x4C8 animated noise ("cam_base" character)
  int m_field540 = 1;                  // +0x540
  unsigned short m_roomId = 0;         // +0x544
  int m_roomWallsOpaque = 0;           // +0x548
  int m_fullScreen = 0;                // +0x54C one camera drawn full screen in two-player mode
  CameraMotionSystem* m_motion = nullptr;  // +0x550
  int m_animMode = 0;                  // +0x554 CameraAnimationMode
  cXObject* m_animObject = nullptr;    // +0x558
  cXObject* m_animObject2 = nullptr;   // +0x55C
  unsigned int m_filterId = 0;         // +0x560
  int m_field564 = 0;                  // +0x564
  int m_field568 = 0;                  // +0x568
};

// 0x330 bytes.
struct CameraDirector {
  // (3 a cut to m_animBase, 5 an interpolation to it, then held; 4 one that releases the camera at its end)
  enum State { kIdle = 0, kHold = 1, kHoldTimed = 2, kCut = 3, kInterp = 4, kInterpToGame = 5, kAnim = 6, kCancel = 7, kGame = 8 };
  CameraDirector();  // 8001AE60
  ~CameraDirector();  // 8001AEE4
  CameraDirector(const CameraDirector&) = delete;
  CameraDirector& operator=(const CameraDirector&) = delete;
  void Reset();  // 8001AF60
  void RegisterCurrentCamera(ESimsCam* cam, bool force);  // 8001B038
  void InitSimsCamera();  // 8001B078
  void InitCurrentCamera();  // 8001B198
  void ReleaseCurrentCamera();  // 8001B1FC
  void DisableControls();  // 8001B34C
  void EnableControls();  // 8001B3EC
  // GC: an ENDummy (its first point's matrix: dummy +0x48 -> +0x10); PC: that matrix.
  void AttachDummy(EMat4* dummy, EMat4* mat);  // 8001B478
  void SetCameraCut(ENCamera *);  // 8001B524
  void SetCameraCutAnim(int);  // 8001B7AC
  void SetCameraInterp(ENCamera *, float, CamDirInterpType);  // 8001BDB0
  void SetCameraInterpAnim(int, float, CamDirInterpType, float, bool *);  // 8001C058
  void SetUpInterpCamera(float time, CamDirInterpType type);  // 8001C310
  RET_UNKNOWN ReleaseToGameCamera();  // 8001C540
  RET_UNKNOWN InterpToGameCamera(float, CamDirInterpType);  // 8001C69C
  void SetHoldState(float);  // 8001C830
  void InterpToCancelCamera(float time, CamDirInterpType type, bool again);  // 8001C840
  RET_UNKNOWN CalcSimsCam();  // 8001C8D4
  FrameEffectsManager* GetFrameEffectsManager();  // 8001CAF8
  void CalcCancelCam();  // 8001CB00
  int CheckCancelled();  // 8001CD20
  int Interp();  // 8001CDF4
  void SetFOV(float radians);  // 8001D2DC
  void InitAnim(unsigned int character);  // 8001D33C
  int StartAnim(unsigned int anim, bool flag, bool releaseAfter);  // 8001D368
  bool StartAnim(AnimRef *, bool, bool);  // 8001D5F8
  void Update();  // 8001D644
  void UpdateAnimNoteTrack();  // 8001DB94
  void UpdateCameraPosAndFOV();  // 8001DC68
  void LetterBox(bool);  // 8001DFC8
  void Draw(ERC* rc);  // 8001DFE0 the letterbox
  void ProcessPropertyEventTags();  // 8001E1FC
  RET_UNKNOWN ProcessAnimEvents(AnimRef* ref, int from, int length);  // 8001E2E0 (GC: called with `this`)
  RET_UNKNOWN AnimEventHandler(AnimRef *, EAnimNote &, int);  // 8001E3E0
  void BeginCameraBloomInterp(CameraBloomDataElement *);  // 8001E4C0
  void BloomInterp();  // 8001E540
  void BlurInterp();  // 8001E5F0
  void ForceDisableHud(bool disable);  // 8001E68C
  bool IsForceDisableHud();  // 8001E6B8

  // True while the director owns the camera (ESimsCam::UpdateWin/SetWinPos leave it alone).
  bool IsActive() const { return m_state != kIdle && m_state != kGame; }

  EMat4 m_simsCam;                     // +0x000 the game camera when taken over (LookAtPos)
  EMat4 m_field040;                    // +0x040
  EMat4 m_animBase;                    // +0x080 camera animations play relative to this
  EMat4 m_field0C0;                    // +0x0C0
  EVec3 m_simsCamTarget;               // +0x100
  EVec3 m_simsCamUp;                   // +0x10C
  EMat4* m_attach = nullptr;           // +0x118 matrices the shot is attached to
  EMat4* m_attach2 = nullptr;          // +0x11C
  EMat4 m_field120;                    // +0x120
  int m_state = kIdle;                 // +0x160
  int m_nextState = kIdle;             // +0x164
  float m_holdTime = 0;                // +0x168
  int m_hudDisabled = 0;               // +0x16C
  ESimsCam* m_cam = nullptr;           // +0x170
  float m_savedFov = 0;                // +0x174 radians
  float m_savedFovMin = 0;             // +0x178 the camera tuning the director changes, restored
  float m_savedFovMax = 0;             // +0x17C   by ReleaseCurrentCamera
  float m_savedNearZoomed = 0;         // +0x180
  float m_savedNear = 0;               // +0x184
  float m_savedTilt = 0;               // +0x188
  float m_field18C = 0;                // +0x18C
  float m_field190 = 0;                // +0x190 an interpolation's start time (seconds)
  float m_field194 = 0;                // +0x194 its length (SetUpInterpCamera)
  int m_interpolating = 0;             // +0x198
  float m_startFov = 0;                // +0x19C the field of view an interpolation starts from (radians)
  float m_fov = 0;                     // +0x1A0 radians
  int m_field1A4 = 0;                  // +0x1A4
  EAnimController m_anim;              // +0x1A8
  AnimRef* m_animRef = nullptr;        // +0x220 (StartAnim(AnimRef*): note-track events)
  float m_eventTime = 0;               // +0x224
  float m_eventFrame = 0;              // +0x228
  float m_startFrame = 0;              // +0x22C
  int m_resetBase = 1;                 // +0x230 reset m_animBase when a shot starts
  int m_field234 = 1;                  // +0x234
  int m_cancelMode = 0;                // +0x238 1: the stick cancels the shot; 2: cancelling
  int m_field23C = 0;                  // +0x23C
  unsigned int m_flags = 0;            // +0x240 1: disabled
  unsigned int m_controllerFilter = 0; // +0x244
  int m_filterPlayer = 0;              // +0x248
  int m_transform = 0;                 // +0x24C transform shots by the two matrices below
  EMat4 m_transformA;                  // +0x250
  EMat4 m_transformB;                  // +0x290
  FrameEffectsManager* m_frameEffects = nullptr;  // +0x2D0
  float m_bloomTime = 0;               // +0x2D4
  float m_blurTime = 0;                // +0x2D8
  float m_bloomFrom[4] = {};           // +0x2DC the camera bloom an interpolation starts from (EGraphics)
  float m_bloomTo[4] = {};             // +0x2EC and goes to (BeginCameraBloomInterp)
  float m_blurFrom[4] = {};            // +0x2FC the same for the motion blur parameters
  float m_blurTo[4] = {};              // +0x30C
  unsigned int m_effectFlags = 0;      // +0x31C 1: bloom, 2: blur interpolating
  int m_field320 = 0;                  // +0x320
  int m_field324 = 0;                  // +0x324
  int m_field328 = 0;                  // +0x328
};

