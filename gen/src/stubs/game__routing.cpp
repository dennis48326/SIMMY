// GENERATED stubs for unported functions of game/routing.cpp. Do not edit.
#include "simmy/types_gen.hpp"

PenaltyRect::PenaltyRect(IRect *, int) { SIMMY_STUB("PenaltyRect::PenaltyRect(IRect *, int)", 0x8013896C); }
PenaltyRect::PenaltyRect(int, int, int, int, int) { SIMMY_STUB("PenaltyRect::PenaltyRect(int, int, int, int, int)", 0x80138998); }
RET_UNKNOWN FindIntersectingRect(IRect *, vector<PenaltyRect, allocator<PenaltyRect>> *) { SIMMY_STUB("FindIntersectingRect(IRect *, vector<PenaltyRect, allocator<PenaltyRect> > *)", 0x801389B4); return {}; }
void SpacePartition::FindInterfaceRect(int, int, IRect *) { SIMMY_STUB("SpacePartition::FindInterfaceRect(int, int, IRect *)", 0x80138A3C); }
RET_UNKNOWN SpacePartition::FindInterfaceRect(ASTNode *, ASTNode *, IRect *) { SIMMY_STUB("SpacePartition::FindInterfaceRect(ASTNode *, ASTNode *, IRect *)", 0x80138ABC); return {}; }
void SpacePartition::FindInterfacePoint(ASTNode *, ASTNode *) { SIMMY_STUB("SpacePartition::FindInterfacePoint(ASTNode *, ASTNode *)", 0x80138B98); }
SpacePartition::SpacePartition() { SIMMY_STUB("SpacePartition::SpacePartition(void)", 0x80138C20); }
RET_UNKNOWN SpacePartition::GetIntersectingFreeRect(IRect *) { SIMMY_STUB("SpacePartition::GetIntersectingFreeRect(IRect *)", 0x80138C94); return {}; }
RET_UNKNOWN SpacePartition::GetIntersectingPartitionRect(IRect *) { SIMMY_STUB("SpacePartition::GetIntersectingPartitionRect(IRect *)", 0x80138D18); return {}; }
RET_UNKNOWN SpacePartition::GetNode(int) { SIMMY_STUB("SpacePartition::GetNode(int)", 0x80138E30); return {}; }
RET_UNKNOWN SpacePartition::GetNodeRectangle(int, IRect *) { SIMMY_STUB("SpacePartition::GetNodeRectangle(int, IRect *)", 0x80138E70); return {}; }
bool SpacePartition::IsSpatialNode(int) { SIMMY_STUB("SpacePartition::IsSpatialNode(int)", 0x80138F1C); return {}; }
double SpacePartition::EstimateDistanceToGoal(int) { SIMMY_STUB("SpacePartition::EstimateDistanceToGoal(int)", 0x80138F30); return {}; }
double SpacePartition::MeasureDistance(int, int, IPoint *) { SIMMY_STUB("SpacePartition::MeasureDistance(int, int, IPoint *)", 0x80139070); return {}; }
void SpacePartition::GetTerminals(int *, int *) { SIMMY_STUB("SpacePartition::GetTerminals(int *, int *)", 0x80139250); }
RET_UNKNOWN SpacePartition::CountSuccessors(int) { SIMMY_STUB("SpacePartition::CountSuccessors(int)", 0x80139264); return {}; }
RET_UNKNOWN SpacePartition::GetNthSuccessor(int, int) { SIMMY_STUB("SpacePartition::GetNthSuccessor(int, int)", 0x801397D0); return {}; }
void SpacePartition::Clear() { SIMMY_STUB("SpacePartition::Clear(void)", 0x80139844); }
void SpacePartition::Deallocate() { SIMMY_STUB("SpacePartition::Deallocate(void)", 0x801398BC); }
RET_UNKNOWN IsRectInside(IRect *, IRect *) { SIMMY_STUB("IsRectInside(IRect *, IRect *)", 0x80139C04); return {}; }
RET_UNKNOWN SpacePartition::ExpandRect(PenaltyRect *) { SIMMY_STUB("SpacePartition::ExpandRect(PenaltyRect *)", 0x80139C60); return {}; }
void SpacePartition::BuildSpatialSuccessorList(int) { SIMMY_STUB("SpacePartition::BuildSpatialSuccessorList(int)", 0x8013A130); }
RET_UNKNOWN Path::InitAST() { SIMMY_STUB("Path::InitAST(void)", 0x8013ACAC); return {}; }
RET_UNKNOWN Path::OpenANode() { SIMMY_STUB("Path::OpenANode(void)", 0x8013AE78); return {}; }
bool EvalPoint(IPoint, IPoint, IPoint, IPoint *, float *) { SIMMY_STUB("EvalPoint(IPoint, IPoint, IPoint, IPoint *, float *)", 0x8013BA9C); return {}; }
bool Path::DoOneSmooth() { SIMMY_STUB("Path::DoOneSmooth(void)", 0x8013BC20); return {}; }
void SpacePartition::Init(RoutingParams *) { SIMMY_STUB("SpacePartition::Init(RoutingParams *)", 0x8013C0EC); }
void Path::InitPath(RoutingParams *) { SIMMY_STUB("Path::InitPath(RoutingParams *)", 0x8013D6AC); }
RET_UNKNOWN Path::FindSmallestOpenNode() { SIMMY_STUB("Path::FindSmallestOpenNode(void)", 0x8013D750); return {}; }
void Path::DeallocateAllPathData() { SIMMY_STUB("Path::DeallocateAllPathData(void)", 0x8013D7FC); }
