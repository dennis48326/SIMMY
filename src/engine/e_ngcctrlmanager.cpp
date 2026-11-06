// Port of engine/e_ngcctrlmanager.cpp (and ENgcController's inline functions): the four GameCube pads,
// read through PAD. On PC, aurora's PAD maps keyboards and SDL gamepads onto those ports.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <cstdio>

namespace simmy {
void dev_pad_gate(PADStatus* pads);  // game/padgate.cpp: SIMMY_PAD_GATE
void dev_pad_soak(PADStatus* pads);  // platform/pc_soak.cpp: SIMMY_SOAK
void overlay_filter_pads(PADStatus* pads);  // platform/pc_overlay.cpp: no input while the F1 menu is open
}

static ENgcControllerManager _ngcctrlman;
EControllerManager* _pCtrlMan = nullptr;           // 804FF184
EController** _ngcCtrlPads = nullptr;              // 804FF180
static int bWasInError_1812 = 0;

// 8032DC10
ENgcControllerManager::ENgcControllerManager() { _pCtrlMan = this; }
// 8032DC50
ENgcControllerManager::~ENgcControllerManager() {}

// 8032DC84
int ENgcControllerManager::Init() {
  PADInit();
  PADRecalibrate(PAD_CHAN0_BIT);
  PADRecalibrate(PAD_CHAN1_BIT);
  PADRecalibrate(PAD_CHAN2_BIT);
  PADRecalibrate(PAD_CHAN3_BIT);
  _ngcCtrlPads = _ctrlPads;
  for (int i = 0; i < 4; ++i) {
    ENgcController* pad = new ENgcController;
    _ngcCtrlPads[i] = pad;
    pad->m_port = i;
    const float now = static_cast<float>(_pClockMan->GetSec(&_sysclock));
    _ctrlLastActivetime[i][0] = _ctrlLastActivetime[i][1] = now;
  }
  return 1;
}

// 8032DD5C
void ENgcControllerManager::Shutdown() {}

// 8032DD60
void ENgcControllerManager::Update() {
  PADRead(m_pads);
  simmy::kbm_pads(m_pads);        // PC: player 1's keyboard and mouse (pc_kbm.cpp)
  simmy::dev_pad_script(m_pads);  // PC: scripted presses for headless tests
  simmy::dev_pad_gate(m_pads);    // PC: ... held until the game is ready (SIMMY_PAD_GATE)
  simmy::dev_pad_soak(m_pads);    // PC: random play for soak tests (SIMMY_SOAK)
  simmy::overlay_filter_pads(m_pads);  // PC: the options overlay takes the pads while it is open
  simmy::det_pads(m_pads);             // PC: SIMMY_DET_RECORD / SIMMY_DET_REPLAY (pc_det.cpp)
  PADClampCircle(m_pads);
  u32 reset = 0;
  if (m_pads[0].err == PAD_ERR_NO_CONTROLLER) reset |= PAD_CHAN0_BIT;
  if (m_pads[1].err == PAD_ERR_NO_CONTROLLER) reset |= PAD_CHAN1_BIT;
  if (m_pads[2].err == PAD_ERR_NO_CONTROLLER) reset |= PAD_CHAN2_BIT;
  if (m_pads[3].err == PAD_ERR_NO_CONTROLLER) reset |= PAD_CHAN3_BIT;
  if (reset) PADReset(reset);
  {
    // PC: log each port connecting or disconnecting (helps when a controller doesn't reach the game).
    static int s_connected[4] = {-1, -1, -1, -1};
    for (int i = 0; i < 4; ++i) {
      const int now = m_pads[i].err == PAD_ERR_NO_CONTROLLER ? 0 : 1;
      if (now != s_connected[i]) {
        if (s_connected[i] != -1 || now) std::fprintf(stderr, "[pad] port %d %s\n", i + 1, now ? "connected" : "disconnected");
        s_connected[i] = now;
      }
    }
  }
  for (int i = 0; i < 4; ++i) {
    PADStatus status = m_pads[i];
    ENgcController* pad = static_cast<ENgcController*>(_ngcCtrlPads[i]);
    pad->UpdateVibration();
    if (status.err == PAD_ERR_NONE) {
      pad->m_status |= 7;
      pad->ResetControllerData(true);
      pad->UpdateButtonData(pad->m_fake ? pad->m_fakeButtons : pad->GetButtons(status));
      for (int stick = 0; stick < 2; ++stick)
        for (int axis = 0; axis < 2; ++axis)
          pad->UpdateStickData(stick, axis, static_cast<float>(pad->GetStick(status, stick, axis)));
    } else if (status.err == PAD_ERR_TRANSFER || status.err == PAD_ERR_NOT_READY) {
      pad->m_status &= ~2u;  // keep the last state, but it is not stable
    } else if (status.err == PAD_ERR_NO_CONTROLLER) {
      pad->m_status &= ~7u;
      pad->ClearAllData();
    } else {
      pad->m_status = 0xFFFFFFF8u;
      pad->ClearAllData();
    }
  }
  EControllerManager::Update();
}

// 803C70FC
ENgcController::~ENgcController() {}
// 803C711C
int ENgcController::HasVibration() const { return 1; }

// 803C7124
int ENgcController::VibrateMotorOne(float strength) {
  const int ok = EController::VibrateMotorOne(strength);
  if (ok) UpdateMotors();
  return ok != 0;
}

// 803C7168
int ENgcController::VibrateMotorTwo(float strength) {
  const int ok = EController::VibrateMotorTwo(strength);
  if (ok) UpdateMotors();
  return ok != 0;
}

// 803C71AC
int ENgcController::StopMotorOne() {
  const int ok = EController::StopMotorOne();
  if (ok) UpdateMotors();
  return ok != 0;
}

// 803C71F0
bool ENgcController::StopMotorTwo() {
  const bool ok = EController::StopMotorTwo();
  if (ok) UpdateMotors();
  return ok;
}

// 803C7234
bool ENgcController::IsMotorOneActive() const { return EController::IsMotorOneActive(); }
// 803C7254
bool ENgcController::IsMotorTwoActive() const { return EController::IsMotorTwoActive(); }

// 803C7274
// SDK pad bits -> EA controller bits. The sticks also count as digital directions:
// 0x10000/0x20000/0x40000/0x80000 = main stick up/right/down/left, 0x100000.. the same for the C stick.
unsigned int ENgcController::GetButtons(PADStatus& pad) {
  const u16 b = pad.button;
  unsigned int out = 0;
  if (b & PAD_BUTTON_A) out |= 0x40;
  if (b & PAD_BUTTON_B) out |= 0x80;
  if (b & PAD_BUTTON_Y) out |= 0x10;
  if (b & PAD_BUTTON_X) out |= 0x20;
  if (b & PAD_BUTTON_START) out |= 0x800;
  if ((b & PAD_TRIGGER_L) || pad.triggerLeft > 10) out |= 0x4;
  if ((b & PAD_TRIGGER_R) || pad.triggerRight > 10) out |= 0x8;
  if (b & PAD_TRIGGER_Z) out |= 0x2;
  if (b & PAD_BUTTON_LEFT) out |= 0x8000;
  if (b & PAD_BUTTON_RIGHT) out |= 0x2000;
  if (b & PAD_BUTTON_DOWN) out |= 0x4000;
  if (b & PAD_BUTTON_UP) out |= 0x1000;
  if (GetStick(pad, 0, 0) > 0.0) out |= 0x20000;
  if (GetStick(pad, 0, 0) < 0.0) out |= 0x80000;
  if (GetStick(pad, 0, 1) > 0.0) out |= 0x10000;
  if (GetStick(pad, 0, 1) < 0.0) out |= 0x40000;
  if (GetStick(pad, 1, 0) > 0.0) out |= 0x200000;
  if (GetStick(pad, 1, 0) < 0.0) out |= 0x800000;
  if (GetStick(pad, 1, 1) > 0.0) out |= 0x100000;
  if (GetStick(pad, 1, 1) < 0.0) out |= 0x400000;
  return out;
}

// 803C7470
// Stick 0 is the main stick (full scale 40), stick 1 the C stick (full scale 28); clamped to -1..1.
double ENgcController::GetStick(PADStatus& pad, int stick, int axis) {
  float raw = 0.0f, scale;
  if (stick == 0) {
    if (axis == 0) raw = pad.stickX;
    else if (axis == 1) raw = pad.stickY;
    scale = 40.0f;
  } else if (stick == 1) {
    if (axis == 0) raw = pad.substickX;
    else if (axis == 1) raw = pad.substickY;
    scale = 28.0f;
  } else {
    return 0.0;
  }
  double v = raw / scale;
  if (v < -1.0) v = -1.0;
  else if (v > 1.0) v = 1.0;
  return v;
}

// 803C7560
// The rumble motor runs only while both "motors" are on and the disc drive is not in an error state
// (END, WAITING and PAUSING keep the previous verdict).
void ENgcController::UpdateMotors() {
  // The GameCube also stopped rumble while the disc drive reported an error (no disc, cover open,
  // wrong disc, retry). The PC build reads files directly and has no drive, so bWasInError_1812
  // stays 0.
  const bool on = !bWasInError_1812 && m_motor[0] > 0.0f && m_motor[1] > 0.0f;
  PADControlMotor(m_port, on ? PAD_MOTOR_RUMBLE : PAD_MOTOR_STOP);
}
