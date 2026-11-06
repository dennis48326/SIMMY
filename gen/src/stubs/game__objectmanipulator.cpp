// GENERATED stubs for unported functions of game/objectmanipulator.cpp. Do not edit.
#include "simmy/types_gen.hpp"

InteractorModule__ObjectManipulator::InteractorModule__ObjectManipulator() { SIMMY_STUB("InteractorModule::ObjectManipulator::ObjectManipulator(void)", 0x80218F3C); }
InteractorModule__ObjectManipulator::~InteractorModule__ObjectManipulator() { SIMMY_STUB("InteractorModule::ObjectManipulator::~ObjectManipulator(void)", 0x80218FAC); }
void InteractorModule__ObjectManipulator::OnCreate() { SIMMY_STUB("InteractorModule::ObjectManipulator::OnCreate(void)", 0x8021900C); }
void InteractorModule__ObjectManipulator::OnDestroy() { SIMMY_STUB("InteractorModule::ObjectManipulator::OnDestroy(void)", 0x80219040); }
void InteractorModule__ObjectManipulator::OnStart(InteractorModule__Interactor__InteractorParams *) { SIMMY_STUB("InteractorModule::ObjectManipulator::OnStart(InteractorModule::Interactor::InteractorParams *)", 0x80219074); }
void InteractorModule__ObjectManipulator::OnStop() { SIMMY_STUB("InteractorModule::ObjectManipulator::OnStop(void)", 0x802191F0); }
void InteractorModule__ObjectManipulator::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float) { SIMMY_STUB("InteractorModule::ObjectManipulator::OnCommandPressed(InteractorModule::InteractorInputManager::InteractorCommand, float)", 0x8021922C); }
void InteractorModule__PlaceManipulator::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float) { SIMMY_STUB("InteractorModule::PlaceManipulator::OnCommandPressed(InteractorModule::InteractorInputManager::InteractorCommand, float)", 0x80219230); }
void InteractorModule__GrabManipulator::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float) { SIMMY_STUB("InteractorModule::GrabManipulator::OnCommandPressed(InteractorModule::InteractorInputManager::InteractorCommand, float)", 0x802192E8); }
void InteractorModule__ObjectManipulator::OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand, float) { SIMMY_STUB("InteractorModule::ObjectManipulator::OnCommandUpdate(InteractorModule::InteractorInputManager::InteractorCommand, float)", 0x802193D0); }
void InteractorModule__ObjectManipulator::OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand) { SIMMY_STUB("InteractorModule::ObjectManipulator::OnCommandReleased(InteractorModule::InteractorInputManager::InteractorCommand)", 0x8021941C); }
void InteractorModule__ObjectManipulator::PreDraw(InteractorModule__InteractorVisualizer &) { SIMMY_STUB("InteractorModule::ObjectManipulator::PreDraw(InteractorModule::InteractorVisualizer &)", 0x80219488); }
void InteractorModule__ObjectManipulator::Draw(InteractorModule__InteractorVisualizer &) { SIMMY_STUB("InteractorModule::ObjectManipulator::Draw(InteractorModule::InteractorVisualizer &)", 0x802194B4); }
void InteractorModule__ObjectManipulator::Update(float) { SIMMY_STUB("InteractorModule::ObjectManipulator::Update(float)", 0x802194E0); }
void InteractorModule__ObjectManipulator::UpdateObjectHighlights(EIObjectMan *) { SIMMY_STUB("InteractorModule::ObjectManipulator::UpdateObjectHighlights(EIObjectMan *)", 0x80219C74); }
void InteractorModule__ObjectManipulator::UpdateOverlapIntersection(InteractorModule__Interactor__OverlapData *) { SIMMY_STUB("InteractorModule::ObjectManipulator::UpdateOverlapIntersection(InteractorModule::Interactor::OverlapData *)", 0x80219EF8); }
void InteractorModule__ObjectManipulator::SetUIDebugName(cXObject *) { SIMMY_STUB("InteractorModule::ObjectManipulator::SetUIDebugName(cXObject *)", 0x8021A030); }
void InteractorModule__ObjectManipulator::SetIsCursorModelEnabled(bool) { SIMMY_STUB("InteractorModule::ObjectManipulator::SetIsCursorModelEnabled(bool)", 0x8021A034); }
RET_UNKNOWN InteractorModule__ObjectManipulator::GetIsCursorModelEnabled() const { SIMMY_STUB("InteractorModule::ObjectManipulator::GetIsCursorModelEnabled(void) const", 0x8021A03C); return {}; }
void InteractorModule__GrabManipulator::OnCreate() { SIMMY_STUB("InteractorModule::GrabManipulator::OnCreate(void)", 0x8021A044); }
void InteractorModule__GrabManipulator::OnStart(InteractorModule__Interactor__InteractorParams *) { SIMMY_STUB("InteractorModule::GrabManipulator::OnStart(InteractorModule::Interactor::InteractorParams *)", 0x8021A07C); }
void InteractorModule__PlaceManipulator::OnCreate() { SIMMY_STUB("InteractorModule::PlaceManipulator::OnCreate(void)", 0x8021A188); }
void InteractorModule__PlaceManipulator::OnStart(InteractorModule__Interactor__InteractorParams *) { SIMMY_STUB("InteractorModule::PlaceManipulator::OnStart(InteractorModule::Interactor::InteractorParams *)", 0x8021A1C0); }
void InteractorModule__PlaceManipulator::StartPlacement(InteractorModule__PlaceManipulator__PlaceManipulatorParams *) { SIMMY_STUB("InteractorModule::PlaceManipulator::StartPlacement(InteractorModule::PlaceManipulator::PlaceManipulatorParams *)", 0x8021A1FC); }
void InteractorModule__GrabManipulator::TryGrabbingWorldObject() { SIMMY_STUB("InteractorModule::GrabManipulator::TryGrabbingWorldObject(void)", 0x8021A348); }
void InteractorModule__GrabManipulator::TryGrabbingWorldObjectFromId(short) { SIMMY_STUB("InteractorModule::GrabManipulator::TryGrabbingWorldObjectFromId(short)", 0x8021A438); }
void InteractorModule__GrabManipulator::DestroyObjectInHand() { SIMMY_STUB("InteractorModule::GrabManipulator::DestroyObjectInHand(void)", 0x8021A50C); }
void InteractorModule__GrabManipulator::DropCurrentObject() { SIMMY_STUB("InteractorModule::GrabManipulator::DropCurrentObject(void)", 0x8021A564); }
void InteractorModule__GrabManipulator::TryRemoveCurrentObjectFromWorld() { SIMMY_STUB("InteractorModule::GrabManipulator::TryRemoveCurrentObjectFromWorld(void)", 0x8021A620); }
void InteractorModule__GrabManipulator::CancelCurrentGrab() { SIMMY_STUB("InteractorModule::GrabManipulator::CancelCurrentGrab(void)", 0x8021A7BC); }
void InteractorModule__GrabManipulator::CancelSession() { SIMMY_STUB("InteractorModule::GrabManipulator::CancelSession(void)", 0x8021A854); }
RET_UNKNOWN InteractorModule__ObjectManipulator::GetPlacementObject() { SIMMY_STUB("InteractorModule::ObjectManipulator::GetPlacementObject(void)", 0x8021A8D0); return {}; }
void InteractorModule__PlaceManipulator::TryPlacingCurrentObject() { SIMMY_STUB("InteractorModule::PlaceManipulator::TryPlacingCurrentObject(void)", 0x8021A8D8); }
void InteractorModule__PlaceManipulator::CancelSession() { SIMMY_STUB("InteractorModule::PlaceManipulator::CancelSession(void)", 0x8021A9EC); }
InteractorModule__PlacementObject::InteractorModule__PlacementObject() { SIMMY_STUB("InteractorModule::PlacementObject::PlacementObject(void)", 0x8021AAB8); }
InteractorModule__PlacementObject::~InteractorModule__PlacementObject() { SIMMY_STUB("InteractorModule::PlacementObject::~PlacementObject(void)", 0x8021AAF0); }
void InteractorModule__PlacementObject::CreateGridTile(ObjSelector *, cXMTObjectImpl *, cXObjectImpl *) { SIMMY_STUB("InteractorModule::PlacementObject::CreateGridTile(ObjSelector *, cXMTObjectImpl *, cXObjectImpl *)", 0x8021B27C); }
bool InteractorModule__PlacementObject::InitializeGridObject(cXMTObjectImpl *) { SIMMY_STUB("InteractorModule::PlacementObject::InitializeGridObject(cXMTObjectImpl *)", 0x8021BC44); return {}; }
void InteractorModule__PlacementObject::AssignOffsetsToGridObject(cXMTObjectImpl *, cXObjectImpl *) { SIMMY_STUB("InteractorModule::PlacementObject::AssignOffsetsToGridObject(cXMTObjectImpl *, cXObjectImpl *)", 0x8021BCA8); }
void InteractorModule__PlacementObject::SetDirection(int) { SIMMY_STUB("InteractorModule::PlacementObject::SetDirection(int)", 0x8021C384); }
InteractorModule__GrabManipulator::InteractorModule__GrabManipulator() { SIMMY_STUB("InteractorModule::GrabManipulator::GrabManipulator(void)", 0x8021CD14); }
InteractorModule__GrabManipulator::~InteractorModule__GrabManipulator() { SIMMY_STUB("InteractorModule::GrabManipulator::~GrabManipulator(void)", 0x8021CD90); }
InteractorModule__PlaceManipulator::~InteractorModule__PlaceManipulator() { SIMMY_STUB("InteractorModule::PlaceManipulator::~PlaceManipulator(void)", 0x8021CDC8); }
