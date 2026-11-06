// @class InLevelCreateASimState after CreateASimBaseState
// In-game Create-a-Sim (game/pregamestate.cpp's InLevelCreateASimState, state 14): a dresser's or
// wardrobe's "Change Clothes" (the generic sim call 0x22, cXObjectImpl::TryGenericSimCall) sends the
// "Wardrobe" flow (5). InLevelBaseState then leaves the lot as for the story's last lot: the HUD goes,
// the simulator pauses, the loading screen comes up, the lot is saved into the neighbourhood and the
// memory card, the objects and the live mode go, and this state is called. It runs CAS in its wardrobe
// mode (CAS_mode 1) on the sim's neighbour (CAS_neighborID): the outfit goes into the neighbour
// (CASTarget::SaveSimToFamilySlot -> GameData::ReplaceSimData) and the game is saved; then live mode
// loads the lot again, with the sim's new look, and the state returns to InLevelBaseState.
#pragma once

struct InLevelCreateASimState : public CreateASimBaseState {
  explicit InLevelCreateASimState(int id = -1);  // 800953BC
  void Reset() override;  // 80094DB0
  void Update(float dt) override;  // 80094DF0
};

// Parts of cXObjectImpl::TryGenericSimCall (80102A24, objectsim.cpp) that open UI screens, here with the
// screens they open. The result is the primitive's (1 true, 2 yield and run again, 4 wait, -1 error).
int GenericSimCallWardrobe(cXObjectImpl* self);  // call 0x22: "Wardrobe" (in-game CAS)
int GenericSimCallXamMode(cXObjectImpl* self);   // call 0x27: GameData::GotoXamMode (food creation)
