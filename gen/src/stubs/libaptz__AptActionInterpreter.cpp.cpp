// GENERATED stubs for unported functions of libaptz/AptActionInterpreter.cpp.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void AptActionInterpreter::initialize(AptInitParmsT &) { SIMMY_STUB("AptActionInterpreter::initialize(AptInitParmsT &)", 0x8026C8BC); }
RET_UNKNOWN AptActionInterpreter::shutdown() { SIMMY_STUB("AptActionInterpreter::shutdown(void)", 0x8026C968); return {}; }
void AptActionInterpreter::_parseStream(unsigned char *, unsigned char *, AptConstFile *, int *) { SIMMY_STUB("AptActionInterpreter::_parseStream(unsigned char *, unsigned char *, AptConstFile *, int *)", 0x8026CA44); }
void AptActionInterpreter::unresolveStream(unsigned char *, unsigned char *, int *) { SIMMY_STUB("AptActionInterpreter::unresolveStream(unsigned char *, unsigned char *, int *)", 0x8026D40C); }
void AptActionInterpreter::resolveStream(unsigned char *, unsigned char *, AptConstFile *, int *) { SIMMY_STUB("AptActionInterpreter::resolveStream(unsigned char *, unsigned char *, AptConstFile *, int *)", 0x8026D434); }
RET_UNKNOWN AptActionInterpreter::loadVariables(AptValue *, AptValue *, EAStringC *) { SIMMY_STUB("AptActionInterpreter::loadVariables(AptValue *, AptValue *, EAStringC *)", 0x8026D454); return {}; }
void _getNameString(AptCIH *, EAStringC &, NameStringType) { SIMMY_STUB("_getNameString(AptCIH *, EAStringC &, NameStringType)", 0x8026F4C0); }
void AptActionInterpreter::getName2(AptCIH *, EAStringC &) { SIMMY_STUB("AptActionInterpreter::getName2(AptCIH *, EAStringC &)", 0x8026F6DC); }
void AptActionInterpreter::PrepareForExecution(char *) { SIMMY_STUB("AptActionInterpreter::PrepareForExecution(char *)", 0x80270748); }
void AptActionInterpreter::PrepareForExecution(AptActionSetup *) { SIMMY_STUB("AptActionInterpreter::PrepareForExecution(AptActionSetup *)", 0x80270768); }
void AptActionInterpreter::CleanupAfterExecution(char *, void *) { SIMMY_STUB("AptActionInterpreter::CleanupAfterExecution(char *, void *)", 0x80270788); }
void AptActionInterpreter::CleanupAfterExecution(void *, AptActionSetup *) { SIMMY_STUB("AptActionInterpreter::CleanupAfterExecution(void *, AptActionSetup *)", 0x8027085C); }
RET_UNKNOWN _concatAsStrings(AptValue *, AptValue *) { SIMMY_STUB("_concatAsStrings(AptValue *, AptValue *)", 0x80271EB0); return {}; }
RET_UNKNOWN _escape(EAStringC &) { SIMMY_STUB("_escape(EAStringC &)", 0x802724C0); return {}; }
RET_UNKNOWN _escape2Char(char, char) { SIMMY_STUB("_escape2Char(char, char)", 0x8027263C); return {}; }
void _unEscape(EAStringC &) { SIMMY_STUB("_unEscape(EAStringC &)", 0x8027269C); }
bool AptActionInterpreter::urlDecode(char *, EAStringC &, EAStringC &) { SIMMY_STUB("AptActionInterpreter::urlDecode(char *, EAStringC &, EAStringC &)", 0x802727F4); return {}; }
RET_UNKNOWN AptValueGC::IsGarbageCollected() const { SIMMY_STUB("AptValueGC::IsGarbageCollected(void) const", 0x80281D1C); return {}; }
RET_UNKNOWN AptValueNoGC::IsGarbageCollected() const { SIMMY_STUB("AptValueNoGC::IsGarbageCollected(void) const", 0x80281D24); return {}; }
void AptValueNoGC::RegisterReferences() const { SIMMY_STUB("AptValueNoGC::RegisterReferences(void) const", 0x80281D2C); }
AptRegister::~AptRegister() { SIMMY_STUB("AptRegister::~AptRegister(void)", 0x80281D30); }
AptLookup::~AptLookup() { SIMMY_STUB("AptLookup::~AptLookup(void)", 0x80281D88); }
AptStringObject::~AptStringObject() { SIMMY_STUB("AptStringObject::~AptStringObject(void)", 0x80281DE0); }
void AptStringObject::setString(AptString *) { SIMMY_STUB("AptStringObject::setString(AptString *)", 0x80281E94); }
RET_UNKNOWN AptStringObject::objectMemberLookup(AptValue *, EAStringC *) const { SIMMY_STUB("AptStringObject::objectMemberLookup(AptValue *, EAStringC *) const", 0x80281EF0); return {}; }
AptMovieClip::~AptMovieClip() { SIMMY_STUB("AptMovieClip::~AptMovieClip(void)", 0x80281F54); }
bool EAStringC::operator==(char *) const { SIMMY_STUB("EAStringC::operator==(char *) const", 0x80282120); return {}; }
