// GENERATED stubs for unported functions of game/slots.cpp. Do not edit.
#include "simmy/types_gen.hpp"

Slot::Slot() { SIMMY_STUB("Slot::Slot(void)", 0x80142968); }
Slot::Slot(SlotDescriptor *) { SIMMY_STUB("Slot::Slot(SlotDescriptor *)", 0x8014297C); }
void Slot::WriteSlot(SlotDescriptor *) { SIMMY_STUB("Slot::WriteSlot(SlotDescriptor *)", 0x80142990); }
void RoutingSlot::WriteSlot(SlotDescriptor *) { SIMMY_STUB("RoutingSlot::WriteSlot(SlotDescriptor *)", 0x80142AD8); }
ObjectSlot::ObjectSlot() { SIMMY_STUB("ObjectSlot::ObjectSlot(void)", 0x80142C40); }
ObjectSlot::ObjectSlot(SlotDescriptor *) { SIMMY_STUB("ObjectSlot::ObjectSlot(SlotDescriptor *)", 0x80142CAC); }
void ObjectSlot::WriteSlot(SlotDescriptor *) { SIMMY_STUB("ObjectSlot::WriteSlot(SlotDescriptor *)", 0x80142D28); }
void ObjectSlot::SetHeight(StdHeight) { SIMMY_STUB("ObjectSlot::SetHeight(StdHeight)", 0x80142D64); }
void SpriteSlot::WriteSlot(SlotDescriptor *) { SIMMY_STUB("SpriteSlot::WriteSlot(SlotDescriptor *)", 0x80142F60); }
RET_UNKNOWN SpriteSlot::SetSprite(ObjSelector *) { SIMMY_STUB("SpriteSlot::SetSprite(ObjSelector *)", 0x80143030); return {}; }
SlotDescriptor::SlotDescriptor() { SIMMY_STUB("SlotDescriptor::SlotDescriptor(void)", 0x8014349C); }
void SlotDescriptor::DoStream(ReconBuffer *, int) { SIMMY_STUB("SlotDescriptor::DoStream(ReconBuffer *, int)", 0x80143560); }
SlotLoader::SlotLoader(unsigned int) { SIMMY_STUB("SlotLoader::SlotLoader(unsigned int)", 0x80143814); }
RET_UNKNOWN SlotLoader::Save(short, vector<ObjectSlot, allocator<ObjectSlot>> *, vector<RoutingSlot, allocator<RoutingSlot>> *, vector<SpriteSlot, allocator<SpriteSlot>> *, cXObject *) { SIMMY_STUB("SlotLoader::Save(short, vector<ObjectSlot, allocator<ObjectSlot> > *, vector<RoutingSlot, allocator<RoutingSlot> > *, vector<SpriteSlot, allocator<SpriteSlot> > *, cXObject *)", 0x801445C4); return {}; }
void SlotLoader::GetSlotName(Slot *, BString &, int) { SIMMY_STUB("SlotLoader::GetSlotName(Slot *, BString &, int)", 0x801445CC); }
SlotLoader::~SlotLoader() { SIMMY_STUB("SlotLoader::~SlotLoader(void)", 0x801445D0); }
ObjectDataSlotDescList::ObjectDataSlotDescList() { SIMMY_STUB("ObjectDataSlotDescList::ObjectDataSlotDescList(void)", 0x801445F8); }
ObjectDataSlotDescList::~ObjectDataSlotDescList() { SIMMY_STUB("ObjectDataSlotDescList::~ObjectDataSlotDescList(void)", 0x8014466C); }
bool ObjectDataSlotDescList::GetResourceName(StringBuffer &) const { SIMMY_STUB("ObjectDataSlotDescList::GetResourceName(StringBuffer &) const", 0x801446A0); return {}; }
RET_UNKNOWN ObjectDataSlotDescList::SetResourceName(StringBuffer &) { SIMMY_STUB("ObjectDataSlotDescList::SetResourceName(StringBuffer &)", 0x801446F4); return {}; }
bool QuickDataSlotDescList::LoadFromDataID(ObjectDataID &) { SIMMY_STUB("QuickDataSlotDescList::LoadFromDataID(ObjectDataID &)", 0x8014471C); return {}; }
RET_UNKNOWN QuickDataSlotDescList::LoadFromIndex(unsigned int, int) { SIMMY_STUB("QuickDataSlotDescList::LoadFromIndex(unsigned int, int)", 0x801447BC); return {}; }
RET_UNKNOWN QuickDataSlotDescList::QueryInterface(unsigned int, void **) { SIMMY_STUB("QuickDataSlotDescList::QueryInterface(unsigned int, void **)", 0x80144880); return {}; }
void QuickDataSlotDescList::AddRef() { SIMMY_STUB("QuickDataSlotDescList::AddRef(void)", 0x801448F0); }
RET_UNKNOWN QuickDataSlotDescList::Release() { SIMMY_STUB("QuickDataSlotDescList::Release(void)", 0x80144904); return {}; }
RET_UNKNOWN IFFSlotDescList::AddSlot(SlotDescriptor &) { SIMMY_STUB("IFFSlotDescList::AddSlot(SlotDescriptor &)", 0x80144960); return {}; }
RET_UNKNOWN IFFSlotDescList::RemoveSlot(int) { SIMMY_STUB("IFFSlotDescList::RemoveSlot(int)", 0x80144C20); return {}; }
bool IFFSlotDescList::LoadFromDataID(ObjectDataID &) { SIMMY_STUB("IFFSlotDescList::LoadFromDataID(ObjectDataID &)", 0x80144D24); return {}; }
RET_UNKNOWN IFFSlotDescList::LoadFromIndex(unsigned int, int) { SIMMY_STUB("IFFSlotDescList::LoadFromIndex(unsigned int, int)", 0x80144DF8); return {}; }
RET_UNKNOWN IFFSlotDescList::LoadOnlyNameAndIDFromIndex(unsigned int, int) { SIMMY_STUB("IFFSlotDescList::LoadOnlyNameAndIDFromIndex(unsigned int, int)", 0x80144F00); return {}; }
RET_UNKNOWN IFFSlotDescList::SaveDataByID(ObjectDataID &) { SIMMY_STUB("IFFSlotDescList::SaveDataByID(ObjectDataID &)", 0x80144F34); return {}; }
RET_UNKNOWN IFFSlotDescList::QueryInterface(unsigned int, void **) { SIMMY_STUB("IFFSlotDescList::QueryInterface(unsigned int, void **)", 0x80144FA4); return {}; }
void IFFSlotDescList::AddRef() { SIMMY_STUB("IFFSlotDescList::AddRef(void)", 0x80145014); }
RET_UNKNOWN IFFSlotDescList::Release() { SIMMY_STUB("IFFSlotDescList::Release(void)", 0x80145028); return {}; }
RET_UNKNOWN ObjectDataSlotDescList::GetDataID() const { SIMMY_STUB("ObjectDataSlotDescList::GetDataID(void) const", 0x80145084); return {}; }
QuickDataSlotDescList::QuickDataSlotDescList() { SIMMY_STUB("QuickDataSlotDescList::QuickDataSlotDescList(void)", 0x801450B8); }
QuickDataSlotDescList::~QuickDataSlotDescList() { SIMMY_STUB("QuickDataSlotDescList::~QuickDataSlotDescList(void)", 0x801450FC); }
RET_UNKNOWN QuickDataSlotDescList::AddSlot(SlotDescriptor &) { SIMMY_STUB("QuickDataSlotDescList::AddSlot(SlotDescriptor &)", 0x8014512C); return {}; }
RET_UNKNOWN QuickDataSlotDescList::RemoveSlot(int) { SIMMY_STUB("QuickDataSlotDescList::RemoveSlot(int)", 0x80145134); return {}; }
RET_UNKNOWN QuickDataSlotDescList::GetNumSlots() { SIMMY_STUB("QuickDataSlotDescList::GetNumSlots(void)", 0x8014513C); return {}; }
RET_UNKNOWN QuickDataSlotDescList::GetSlot(int) { SIMMY_STUB("QuickDataSlotDescList::GetSlot(int)", 0x80145154); return {}; }
RET_UNKNOWN QuickDataSlotDescList::GetStringSetID() const { SIMMY_STUB("QuickDataSlotDescList::GetStringSetID(void) const", 0x80145164); return {}; }
void QuickDataSlotDescList::SetStringSetID(int) { SIMMY_STUB("QuickDataSlotDescList::SetStringSetID(int)", 0x8014516C); }
RET_UNKNOWN QuickDataSlotDescList::SaveDataByID(ObjectDataID &) { SIMMY_STUB("QuickDataSlotDescList::SaveDataByID(ObjectDataID &)", 0x80145170); return {}; }
RET_UNKNOWN QuickDataSlotDescList::GetDataSourceType() const { SIMMY_STUB("QuickDataSlotDescList::GetDataSourceType(void) const", 0x80145178); return {}; }
RET_UNKNOWN QuickDataSlotDescList::LoadOnlyNameAndIDFromIndex(unsigned int, int) { SIMMY_STUB("QuickDataSlotDescList::LoadOnlyNameAndIDFromIndex(unsigned int, int)", 0x80145180); return {}; }
IFFSlotDescList::IFFSlotDescList() { SIMMY_STUB("IFFSlotDescList::IFFSlotDescList(void)", 0x80145188); }
IFFSlotDescList::~IFFSlotDescList() { SIMMY_STUB("IFFSlotDescList::~IFFSlotDescList(void)", 0x801451DC); }
RET_UNKNOWN IFFSlotDescList::GetNumSlots() { SIMMY_STUB("IFFSlotDescList::GetNumSlots(void)", 0x8014527C); return {}; }
RET_UNKNOWN IFFSlotDescList::GetSlot(int) { SIMMY_STUB("IFFSlotDescList::GetSlot(int)", 0x8014529C); return {}; }
RET_UNKNOWN IFFSlotDescList::GetStringSetID() const { SIMMY_STUB("IFFSlotDescList::GetStringSetID(void) const", 0x801452AC); return {}; }
void IFFSlotDescList::SetStringSetID(int) { SIMMY_STUB("IFFSlotDescList::SetStringSetID(int)", 0x801452B4); }
RET_UNKNOWN IFFSlotDescList::GetDataSourceType() const { SIMMY_STUB("IFFSlotDescList::GetDataSourceType(void) const", 0x801452B8); return {}; }
Slot::~Slot() { SIMMY_STUB("Slot::~Slot(void)", 0x801452C0); }
ObjectSlot::~ObjectSlot() { SIMMY_STUB("ObjectSlot::~ObjectSlot(void)", 0x801452F4); }
RET_UNKNOWN ObjectSlot::GetHeight() { SIMMY_STUB("ObjectSlot::GetHeight(void)", 0x80145328); return {}; }
RET_UNKNOWN ObjectSlot::IsSurface() { SIMMY_STUB("ObjectSlot::IsSurface(void)", 0x80145330); return {}; }
RET_UNKNOWN ObjectSlot::IsFreePos() { SIMMY_STUB("ObjectSlot::IsFreePos(void)", 0x80145348); return {}; }
RET_UNKNOWN ObjectSlot::GetHeightMask(StdHeight) { SIMMY_STUB("ObjectSlot::GetHeightMask(StdHeight)", 0x80145354); return {}; }
RET_UNKNOWN SpriteSlot::GetTicksPerFrame() { SIMMY_STUB("SpriteSlot::GetTicksPerFrame(void)", 0x80145364); return {}; }
void SpriteSlot::ActivateForever() { SIMMY_STUB("SpriteSlot::ActivateForever(void)", 0x8014536C); }
void SpriteSlot::UseBalloonSprite(int) { SIMMY_STUB("SpriteSlot::UseBalloonSprite(int)", 0x801453CC); }
void SpriteSlot::UseOverlaySprite(int) { SIMMY_STUB("SpriteSlot::UseOverlaySprite(int)", 0x801453D4); }
void SpriteSlot::SetOptions(int) { SIMMY_STUB("SpriteSlot::SetOptions(int)", 0x801453DC); }
void SpriteSlot::SetPopSize(float) { SIMMY_STUB("SpriteSlot::SetPopSize(float)", 0x801453E4); }
RET_UNKNOWN SpriteSlot::GetSelector() const { SIMMY_STUB("SpriteSlot::GetSelector(void) const", 0x801453EC); return {}; }
RET_UNKNOWN SpriteSlot::GetSpriteID() const { SIMMY_STUB("SpriteSlot::GetSpriteID(void) const", 0x80145408); return {}; }
RET_UNKNOWN SpriteSlot::GetBalloonSpriteID() const { SIMMY_STUB("SpriteSlot::GetBalloonSpriteID(void) const", 0x80145410); return {}; }
RET_UNKNOWN SpriteSlot::GetOverlaySpriteID() const { SIMMY_STUB("SpriteSlot::GetOverlaySpriteID(void) const", 0x80145418); return {}; }
RET_UNKNOWN SpriteSlot::GetCurrentFrame() const { SIMMY_STUB("SpriteSlot::GetCurrentFrame(void) const", 0x80145420); return {}; }
RET_UNKNOWN SpriteSlot::GetPPerson() { SIMMY_STUB("SpriteSlot::GetPPerson(void)", 0x80145428); return {}; }
void SpriteSlot::SetPriority(int) { SIMMY_STUB("SpriteSlot::SetPriority(int)", 0x80145430); }
RET_UNKNOWN SpriteSlot::GetPriority() const { SIMMY_STUB("SpriteSlot::GetPriority(void) const", 0x80145438); return {}; }
RET_UNKNOWN SpriteSlot::GetOptions() const { SIMMY_STUB("SpriteSlot::GetOptions(void) const", 0x80145440); return {}; }
RET_UNKNOWN SpriteSlot::GetIsSkillMeter() const { SIMMY_STUB("SpriteSlot::GetIsSkillMeter(void) const", 0x80145448); return {}; }
RET_UNKNOWN SpriteSlot::GetShowOverInactivePeople() const { SIMMY_STUB("SpriteSlot::GetShowOverInactivePeople(void) const", 0x80145454); return {}; }
void SpriteSlot::SetShowOverInactivePeople(bool) { SIMMY_STUB("SpriteSlot::SetShowOverInactivePeople(bool)", 0x8014545C); }
double SpriteSlot::GetAltOffset() const { SIMMY_STUB("SpriteSlot::GetAltOffset(void) const", 0x80145464); return {}; }
void SpriteSlot::SetAltOffset(float) { SIMMY_STUB("SpriteSlot::SetAltOffset(float)", 0x8014546C); }
double SpriteSlot::GetSizeMultiplierX() const { SIMMY_STUB("SpriteSlot::GetSizeMultiplierX(void) const", 0x80145474); return {}; }
double SpriteSlot::GetSizeMultiplierY() const { SIMMY_STUB("SpriteSlot::GetSizeMultiplierY(void) const", 0x8014547C); return {}; }
double SpriteSlot::GetAlpha() const { SIMMY_STUB("SpriteSlot::GetAlpha(void) const", 0x80145484); return {}; }
RET_UNKNOWN RoutingSlot::IsDirectionAllowed(int) const { SIMMY_STUB("RoutingSlot::IsDirectionAllowed(int) const", 0x801454D0); return {}; }
RET_UNKNOWN RoutingSlot::IsAnyRotationAllowed() const { SIMMY_STUB("RoutingSlot::IsAnyRotationAllowed(void) const", 0x801454EC); return {}; }
bool RoutingSlot::IsAnyFacingAllowed() const { SIMMY_STUB("RoutingSlot::IsAnyFacingAllowed(void) const", 0x80145504); return {}; }
bool RoutingSlot::IsFacingTowardsObject() const { SIMMY_STUB("RoutingSlot::IsFacingTowardsObject(void) const", 0x80145518); return {}; }
bool RoutingSlot::IsFacingAwayFromObject() const { SIMMY_STUB("RoutingSlot::IsFacingAwayFromObject(void) const", 0x8014552C); return {}; }
RET_UNKNOWN RoutingSlot::GetFacingDirection() const { SIMMY_STUB("RoutingSlot::GetFacingDirection(void) const", 0x80145540); return {}; }
RET_UNKNOWN RoutingSlot::GetIgnoreRooms() const { SIMMY_STUB("RoutingSlot::GetIgnoreRooms(void) const", 0x80145548); return {}; }
void RoutingSlot::SetIgnoreRooms(bool) { SIMMY_STUB("RoutingSlot::SetIgnoreRooms(bool)", 0x80145560); }
RET_UNKNOWN RoutingSlot::GetMinDist() const { SIMMY_STUB("RoutingSlot::GetMinDist(void) const", 0x80145580); return {}; }
RET_UNKNOWN RoutingSlot::GetMaxDist() const { SIMMY_STUB("RoutingSlot::GetMaxDist(void) const", 0x80145588); return {}; }
RET_UNKNOWN RoutingSlot::GetOptimalDist() const { SIMMY_STUB("RoutingSlot::GetOptimalDist(void) const", 0x80145590); return {}; }
RET_UNKNOWN RoutingSlot::GetResolution() const { SIMMY_STUB("RoutingSlot::GetResolution(void) const", 0x80145598); return {}; }
double RoutingSlot::GetGradient() const { SIMMY_STUB("RoutingSlot::GetGradient(void) const", 0x801455A0); return {}; }
RET_UNKNOWN RoutingSlot::GetMultiplier(RoutingSlot__VerticalPosition) const { SIMMY_STUB("RoutingSlot::GetMultiplier(RoutingSlot::VerticalPosition) const", 0x801455A8); return {}; }
RET_UNKNOWN RoutingSlot::GetSnapTargetSlot() const { SIMMY_STUB("RoutingSlot::GetSnapTargetSlot(void) const", 0x801455B8); return {}; }
RET_UNKNOWN RoutingSlot::SnapsToDirection() const { SIMMY_STUB("RoutingSlot::SnapsToDirection(void) const", 0x801455C0); return {}; }
RET_UNKNOWN RoutingSlot::GetSnapDirection() const { SIMMY_STUB("RoutingSlot::GetSnapDirection(void) const", 0x801455D8); return {}; }
RET_UNKNOWN RoutingSlot::Absolute() const { SIMMY_STUB("RoutingSlot::Absolute(void) const", 0x80145604); return {}; }
RET_UNKNOWN RoutingSlot::HasRandomScoring() const { SIMMY_STUB("RoutingSlot::HasRandomScoring(void) const", 0x80145610); return {}; }
void RoutingSlot::SetHasRandomScoring(bool) { SIMMY_STUB("RoutingSlot::SetHasRandomScoring(bool)", 0x8014561C); }
void RoutingSlot::SetAllowFailureTrees(bool) { SIMMY_STUB("RoutingSlot::SetAllowFailureTrees(bool)", 0x8014563C); }
RET_UNKNOWN RoutingSlot::GetAllowFailureTrees() const { SIMMY_STUB("RoutingSlot::GetAllowFailureTrees(void) const", 0x8014565C); return {}; }
void RoutingSlot::SetAllowDifferentAlts(bool) { SIMMY_STUB("RoutingSlot::SetAllowDifferentAlts(bool)", 0x80145668); }
RET_UNKNOWN RoutingSlot::GetAllowDifferentAlts() const { SIMMY_STUB("RoutingSlot::GetAllowDifferentAlts(void) const", 0x80145688); return {}; }
void RoutingSlot::SetUseAverageObjectLocation(bool) { SIMMY_STUB("RoutingSlot::SetUseAverageObjectLocation(bool)", 0x80145694); }
RET_UNKNOWN RoutingSlot::GetUseAverageObjectLocation() const { SIMMY_STUB("RoutingSlot::GetUseAverageObjectLocation(void) const", 0x801456B4); return {}; }
void RoutingSlot::SetResolution(int) { SIMMY_STUB("RoutingSlot::SetResolution(int)", 0x801456C0); }
double RoutingSlot::GetXOffset() const { SIMMY_STUB("RoutingSlot::GetXOffset(void) const", 0x801456C8); return {}; }
double RoutingSlot::GetYOffset() const { SIMMY_STUB("RoutingSlot::GetYOffset(void) const", 0x801456D0); return {}; }
double RoutingSlot::GetAltOffset() const { SIMMY_STUB("RoutingSlot::GetAltOffset(void) const", 0x801456D8); return {}; }
