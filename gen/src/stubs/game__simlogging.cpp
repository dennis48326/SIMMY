// GENERATED stubs for unported functions of game/simlogging.cpp. Do not edit.
#include "simmy/types_gen.hpp"

LogPersonState::LogPersonState(cXPerson *) { SIMMY_STUB("LogPersonState::LogPersonState(cXPerson *)", 0x8013E470); }
void LogPersonState::SetDeltas(cXPerson *) { SIMMY_STUB("LogPersonState::SetDeltas(cXPerson *)", 0x8013E55C); }
LogInteractionSample::LogInteractionSample() { SIMMY_STUB("LogInteractionSample::LogInteractionSample(void)", 0x8013E624); }
void LogInteractionSample::Print() { SIMMY_STUB("LogInteractionSample::Print(void)", 0x8013E66C); }
void LogInteractionSample::PrintHeader() { SIMMY_STUB("LogInteractionSample::PrintHeader(void)", 0x8013E878); }
bool LogPersonTracker::SameInteraction(Interaction *) const { SIMMY_STUB("LogPersonTracker::SameInteraction(Interaction *) const", 0x8013EA18); return {}; }
void LogPersonTracker::SetInteraction(cXPerson *, Interaction *) { SIMMY_STUB("LogPersonTracker::SetInteraction(cXPerson *, Interaction *)", 0x8013EAD0); }
bool LogPersonTracker::SamePerson(cXPerson *) const { SIMMY_STUB("LogPersonTracker::SamePerson(cXPerson *) const", 0x8013EC20); return {}; }
LogPersonTracker::LogPersonTracker(cXPerson *) { SIMMY_STUB("LogPersonTracker::LogPersonTracker(cXPerson *)", 0x8013ECBC); }
void LogPersonTracker::Print() { SIMMY_STUB("LogPersonTracker::Print(void)", 0x8013EDD0); }
void LogPersonTracker::PrintHeader() { SIMMY_STUB("LogPersonTracker::PrintHeader(void)", 0x8013EDD4); }
LogInteraction::LogInteraction(LogPersonTracker *) { SIMMY_STUB("LogInteraction::LogInteraction(LogPersonTracker *)", 0x8013EDD8); }
bool LogInteraction::SameInteraction(LogPersonTracker *) { SIMMY_STUB("LogInteraction::SameInteraction(LogPersonTracker *)", 0x8013EF1C); return {}; }
void LogInteraction::AddSample(LogInteractionSample &) { SIMMY_STUB("LogInteraction::AddSample(LogInteractionSample &)", 0x8013EF4C); }
void LogInteraction::PrintHeader() { SIMMY_STUB("LogInteraction::PrintHeader(void)", 0x8013F268); }
void LogInteraction::Print() { SIMMY_STUB("LogInteraction::Print(void)", 0x8013F2A8); }
SimLog::SimLog() { SIMMY_STUB("SimLog::SimLog(void)", 0x8013F33C); }
void SimLog::SimTickCompleted(cXPerson *) { SIMMY_STUB("SimLog::SimTickCompleted(cXPerson *)", 0x8013F378); }
RET_UNKNOWN SimLog::GetTracker(cXPerson *) { SIMMY_STUB("SimLog::GetTracker(cXPerson *)", 0x8013F4D4); return {}; }
RET_UNKNOWN SimLog::GetLog(LogPersonTracker *) { SIMMY_STUB("SimLog::GetLog(LogPersonTracker *)", 0x8013F7F0); return {}; }
void SimLog::BeginLogging() { SIMMY_STUB("SimLog::BeginLogging(void)", 0x8013FADC); }
void SimLog::EndLogging() { SIMMY_STUB("SimLog::EndLogging(void)", 0x8013FAE8); }
