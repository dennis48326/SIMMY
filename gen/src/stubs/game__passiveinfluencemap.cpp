// GENERATED stubs for unported functions of game/passiveinfluencemap.cpp. Do not edit.
#include "simmy/types_gen.hpp"

PassiveInfluenceMap::PassiveInfluenceMap(int, int) { SIMMY_STUB("PassiveInfluenceMap::PassiveInfluenceMap(int, int)", 0x801133E8); }
PassiveInfluenceMap::~PassiveInfluenceMap() { SIMMY_STUB("PassiveInfluenceMap::~PassiveInfluenceMap(void)", 0x80113470); }
void PassiveInfluenceMap::AddItemToMap(PassiveInfluenceItem *) { SIMMY_STUB("PassiveInfluenceMap::AddItemToMap(PassiveInfluenceItem *)", 0x80113520); }
void PassiveInfluenceMap::RemoveItemFromMap(PassiveInfluenceItem *) { SIMMY_STUB("PassiveInfluenceMap::RemoveItemFromMap(PassiveInfluenceItem *)", 0x80113740); }
RET_UNKNOWN PassiveInfluenceMap::GetListForTile(int, int) { SIMMY_STUB("PassiveInfluenceMap::GetListForTile(int, int)", 0x801138F8); return {}; }
void PassiveInfluenceMap::EmptyMap() { SIMMY_STUB("PassiveInfluenceMap::EmptyMap(void)", 0x80113944); }
RET_UNKNOWN PassiveInfluenceMap::NotifySimulator(cXPerson *, int, int) { SIMMY_STUB("PassiveInfluenceMap::NotifySimulator(cXPerson *, int, int)", 0x80113A58); return {}; }
RET_UNKNOWN PassiveInfluenceMap::CheckObjectInfluence(PassiveInfluenceTarget *) { SIMMY_STUB("PassiveInfluenceMap::CheckObjectInfluence(PassiveInfluenceTarget *)", 0x80113A8C); return {}; }
RET_UNKNOWN PassiveInfluenceMap::ApplyInfluence(PassiveInfluenceObject *, PassiveInfluenceTarget *) { SIMMY_STUB("PassiveInfluenceMap::ApplyInfluence(PassiveInfluenceObject *, PassiveInfluenceTarget *)", 0x80113C68); return {}; }
RET_UNKNOWN PassiveInfluenceMap::ValidateMissing(PassiveInfluenceItem *) { SIMMY_STUB("PassiveInfluenceMap::ValidateMissing(PassiveInfluenceItem *)", 0x80113DA8); return {}; }
PassiveInfluenceSystem::PassiveInfluenceSystem() { SIMMY_STUB("PassiveInfluenceSystem::PassiveInfluenceSystem(void)", 0x80113DB0); }
PassiveInfluenceSystem::~PassiveInfluenceSystem() { SIMMY_STUB("PassiveInfluenceSystem::~PassiveInfluenceSystem(void)", 0x80113E48); }
void PassiveInfluenceSystem::FlushSystem() { SIMMY_STUB("PassiveInfluenceSystem::FlushSystem(void)", 0x80113F04); }
void PassiveInfluenceSystem::DoStream(ReconBuffer *, int) { SIMMY_STUB("PassiveInfluenceSystem::DoStream(ReconBuffer *, int)", 0x80113F3C); }
RET_UNKNOWN PassiveInfluenceSystem::AddObjectToMap(cXObject *, signed char, signed char, signed char, signed char) { SIMMY_STUB("PassiveInfluenceSystem::AddObjectToMap(cXObject *, signed char, signed char, signed char, signed char)", 0x80113F40); return {}; }
RET_UNKNOWN PassiveInfluenceSystem::RemoveObjectFromMap(cXObject *) { SIMMY_STUB("PassiveInfluenceSystem::RemoveObjectFromMap(cXObject *)", 0x8011416C); return {}; }
void PassiveInfluenceSystem::MoveObjectInWorld(cXObject *) { SIMMY_STUB("PassiveInfluenceSystem::MoveObjectInWorld(cXObject *)", 0x80114344); }
RET_UNKNOWN PassiveInfluenceSystem::NotifySimulator(cXPerson *, int, int) { SIMMY_STUB("PassiveInfluenceSystem::NotifySimulator(cXPerson *, int, int)", 0x80114704); return {}; }
RET_UNKNOWN PassiveInfluenceSystem::CheckObjects(int) { SIMMY_STUB("PassiveInfluenceSystem::CheckObjects(int)", 0x80114B9C); return {}; }
void PassiveInfluenceSystem::SetInfluenceMotiveDelta(cXObject *, unsigned char, float, short) { SIMMY_STUB("PassiveInfluenceSystem::SetInfluenceMotiveDelta(cXObject *, unsigned char, float, short)", 0x80114E1C); }
RET_UNKNOWN PassiveInfluenceSystem::ClearInfluenceMotiveDeltas(cXObject *) { SIMMY_STUB("PassiveInfluenceSystem::ClearInfluenceMotiveDeltas(cXObject *)", 0x801150A0); return {}; }
bool PassiveInfluenceSystem::SetSkillNameID(cXObject *, AnimRef *) { SIMMY_STUB("PassiveInfluenceSystem::SetSkillNameID(cXObject *, AnimRef *)", 0x8011513C); return {}; }
void PassiveInfluenceMap::CalculateTargetArea(PassiveInfluenceItem *) { SIMMY_STUB("PassiveInfluenceMap::CalculateTargetArea(PassiveInfluenceItem *)", 0x801151C0); }
void PassiveInfluenceMap::FilledArc_Line(int, int, int, PassiveInfluenceItem *) { SIMMY_STUB("PassiveInfluenceMap::FilledArc_Line(int, int, int, PassiveInfluenceItem *)", 0x80115428); }
RET_UNKNOWN PassiveInfluenceMap::FilledArc_Polygon(int, int, int, int, int, int *, vert2 *) { SIMMY_STUB("PassiveInfluenceMap::FilledArc_Polygon(int, int, int, int, int, int *, vert2 *)", 0x80115700); return {}; }
RET_UNKNOWN PassiveInfluenceMap::FilledArc(int, int, int, int, int, PassiveInfluenceItem *) { SIMMY_STUB("PassiveInfluenceMap::FilledArc(int, int, int, int, int, PassiveInfluenceItem *)", 0x80115928); return {}; }
RET_UNKNOWN PassiveInfluenceMap::CalculateAffectedArea(PassiveInfluenceItem *) { SIMMY_STUB("PassiveInfluenceMap::CalculateAffectedArea(PassiveInfluenceItem *)", 0x80115BD8); return {}; }
void PassiveInfluenceMap::Validate() { SIMMY_STUB("PassiveInfluenceMap::Validate(void)", 0x8011623C); }
