// Original methods of the libaptz classes whose PC counterparts are organised differently (see
// aptruntime.hpp / aptplayer.hpp). Each line names the PC function that does the original's work;
// the generator then emits no stub for it. Original methods with no PC counterpart are not listed
// here: they are declared in the class headers and stubbed. Grouped by original unit.

// -- libaptz/Apt.cpp.cpp
// 802688E4 AptUserFunctions::AptUserFunctions()  ->  the PC AptUserFunctions constructor
// -- libaptz/AptActionInterpreter.cpp.cpp
// 8026D6C0 AptActionInterpreter::getObject(AptValue *, AptValue *, EAStringC *)  ->  AptActionInterpreter::resolvePath (aptactioninterpreter.cpp)
// 8026D7FC AptActionInterpreter::getContext(AptValue *, AptValue *, EAStringC *, AptValue **, EAStringC &)  ->  AptActionInterpreter::resolvePath (aptactioninterpreter.cpp)
// 8026D958 AptActionInterpreter::getContext(AptValue *, AptValue *, EAStringC *, AptValue **, char *)  ->  AptActionInterpreter::resolvePath (aptactioninterpreter.cpp)
// 8026E518 AptActionInterpreter::cbCallMethod_hitTest(AptCIH *, int)  ->  MC_hitTest (aptbuiltins.cpp)
// 8026EB48 AptActionInterpreter::cbCallMethod_unescape(AptValue *, int)  ->  unescapeFn (aptbuiltins.cpp)
// 8026EE5C AptActionInterpreter::cbCallMethod_boolean(AptValue *, int)  ->  Boolean_ctor (aptbuiltins.cpp)
// 8026F4B8 AptActionInterpreter::cbCallMethod_ASSetPropFlags(AptValue *, int)  ->  ASSetPropFlags (aptbuiltins.cpp)
// 8026F824 AptActionInterpreter::getName(AptCIH *, EAStringC &)  ->  AptCIH::GetTargetPath (aptplayer.cpp)
// 8026F8DC AptActionInterpreter::_doCloneSprite(AptCIH *, AptValue *, AptValue *, AptValue *, int, AptValue *)  ->  AptActionInterpreter::run, case 0x24 / MC_duplicateMovieClip (aptbuiltins.cpp)
// 8026FF94 AptActionInterpreter::valueToObject(AptValue *, AptValue *, AptValue *, AptValue **)  ->  AptActionInterpreter::toObject (aptactioninterpreter.cpp)
// 80270930 AptActionInterpreter::_createObject(AptValue *, AptValue *, EAStringC *, int, bool)  ->  AptActionInterpreter::newObject (aptactioninterpreter.cpp)
// 80271BB8 AptActionInterpreter::_doEnumerate(AptValue *, AptValue *)  ->  AptActionInterpreter::run, case 0x46/0x55
// 80272938 AptActionInterpreter::isFSCommand(char *)  ->  AptGetURL (apt.cpp)
// 80272984 AptActionInterpreter::doFSCommand(char *, char *)  ->  AptGetURL (apt.cpp)
// 802729D4 AptActionInterpreter::_FunctionAptActionEnd(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run (aptactioninterpreter.cpp)
// 802729D8 AptActionInterpreter::_FunctionAptActionNextFrame(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x04
// 80272A24 AptActionInterpreter::_FunctionAptActionPrevFrame(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x05
// 80272A70 AptActionInterpreter::_FunctionAptActionPlay(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x06
// 80272B44 AptActionInterpreter::_FunctionAptActionStop(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x07
// 80272BBC AptActionInterpreter::_FunctionAptActionToggleQuality(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x08
// 80272BC0 AptActionInterpreter::_FunctionAptActionStopSounds(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x09
// 80272BC4 AptActionInterpreter::_FunctionAptActionAdd(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x0A
// 80272E74 AptActionInterpreter::_FunctionAptActionSubtract(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x0A
// 80273124 AptActionInterpreter::_FunctionAptActionMultiply(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x0A
// 802733D4 AptActionInterpreter::_FunctionAptActionDivide(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x0D
// 802735C4 AptActionInterpreter::_FunctionAptActionEquals(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x0E
// 80273840 AptActionInterpreter::_FunctionAptActionLessThan(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x0F
// 80273AAC AptActionInterpreter::_FunctionAptActionAnd(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x10
// 80273D40 AptActionInterpreter::_FunctionAptActionOr(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x11
// 80273FD0 AptActionInterpreter::_FunctionAptActionNot(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x12
// 80274138 AptActionInterpreter::_FunctionAptActionStringEquals(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x13
// 80274494 AptActionInterpreter::_FunctionAptActionStringLength(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x14
// 8027465C AptActionInterpreter::_FunctionAptActionSubString(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x15
// 802749BC AptActionInterpreter::_FunctionAptActionPop(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x17
// 80274A60 AptActionInterpreter::_FunctionAptActionToInteger(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x18
// 80274BF4 AptActionInterpreter::_FunctionAptActionGetVariable(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x1C
// 80274D4C AptActionInterpreter::_FunctionAptActionSetVariable(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x1D
// 80274E98 AptActionInterpreter::_FunctionAptActionSetTarget2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x20
// 80275014 AptActionInterpreter::_FunctionAptActionStringAdd(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x21
// 80275210 AptActionInterpreter::_FunctionAptActionGetProperty(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x22
// 80275424 AptActionInterpreter::_FunctionAptActionSetProperty(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x23
// 8027554C AptActionInterpreter::_FunctionAptActionCloneSprite(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x24
// 80275638 AptActionInterpreter::_FunctionAptActionRemoveSprite(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x25
// 80275728 AptActionInterpreter::_FunctionAptActionTrace(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x26
// 80275978 AptActionInterpreter::_FunctionAptActionStartDragMovie(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x27
// 80275CB8 AptActionInterpreter::_FunctionAptActionStopDragMovie(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x28
// 80275D1C AptActionInterpreter::_FunctionAptActionStringLessThan(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x29
// 80275D20 AptActionInterpreter::_FunctionAptActionRandom(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x30
// 80275EB0 AptActionInterpreter::_FunctionAptActionMBLength(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x31
// 80275EB4 AptActionInterpreter::_FunctionAptActionCharToAscii(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x32
// 80275EB8 AptActionInterpreter::_FunctionAptActionAsciiToChar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x33
// 80276138 AptActionInterpreter::_FunctionAptActionGetTimer(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x34
// 80276234 AptActionInterpreter::_FunctionAptActionMBSubString(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x35
// 80276238 AptActionInterpreter::_FunctionAptActionMBCharToAscii(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x36
// 8027623C AptActionInterpreter::_FunctionAptActionMBAsciiToChar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x37
// 80276240 AptActionInterpreter::_FunctionAptActionDelete(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x3A
// 80276464 AptActionInterpreter::_FunctionAptActionDelete2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x3B
// 80276640 AptActionInterpreter::_FunctionAptActionDefineLocal(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x3C
// 8027677C AptActionInterpreter::_FunctionAptActionCallFunction(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x3D
// 802769BC AptActionInterpreter::_FunctionAptActionReturn(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x3E
// 802769C8 AptActionInterpreter::_FunctionAptActionModulo(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x3F
// 80276BC0 AptActionInterpreter::_FunctionAptActionNewObject(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x40
// 80276DB0 AptActionInterpreter::_FunctionAptActionDefineLocal2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x41
// 80276F24 AptActionInterpreter::_FunctionAptActionInitArray(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x42
// 802770D0 AptActionInterpreter::_FunctionAptActionInitObject(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x43
// 802773E4 AptActionInterpreter::_FunctionAptActionTypeOf(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x44
// 802777DC AptActionInterpreter::_FunctionAptActionTargetPath(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x45
// 80277B34 AptActionInterpreter::_FunctionAptActionEnumerate(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x46
// 80277B5C AptActionInterpreter::_FunctionAptActionAdd2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x47
// 80278310 AptActionInterpreter::_FunctionAptActionLessThan2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x48
// 80278E44 AptActionInterpreter::_FunctionAptActionEquals2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x49
// 8027A020 AptActionInterpreter::_FunctionAptActionToNumber(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x4A
// 8027A738 AptActionInterpreter::_FunctionAptActionToString(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x4B
// 8027AAD4 AptActionInterpreter::_FunctionAptActionPushDuplicate(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x4C
// 8027AB3C AptActionInterpreter::_FunctionAptActionStackSwap(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x4D
// 8027ABAC AptActionInterpreter::_FunctionAptActionGetMember(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x4E
// 8027AFD0 AptActionInterpreter::_FunctionAptActionSetMember(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x4F
// 8027B338 AptActionInterpreter::_FunctionAptActionIncrement(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x50
// 8027B594 AptActionInterpreter::_FunctionAptActionDecrement(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x51
// 8027B7F0 AptActionInterpreter::_FunctionAptActionCallMethod(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x52
// 8027C324 AptActionInterpreter::_FunctionAptActionNewMethod(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x53
// 8027C590 AptActionInterpreter::_FunctionAptActionEnumerate2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x55
// 8027C5B8 AptActionInterpreter::_FunctionAptActionBitAnd(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x60
// 8027C784 AptActionInterpreter::_FunctionAptActionBitOr(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x61
// 8027C950 AptActionInterpreter::_FunctionAptActionBitXor(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x62
// 8027CB1C AptActionInterpreter::_FunctionAptActionBitLShift(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x63
// 8027CCE8 AptActionInterpreter::_FunctionAptActionBitRShift(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x64
// 8027CEB4 AptActionInterpreter::_FunctionAptActionBitURShift(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x65
// 8027CEB8 AptActionInterpreter::_FunctionAptActionStrictEquals(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x66
// 8027D43C AptActionInterpreter::_FunctionAptActionGreater(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x67
// 8027D7D8 AptActionInterpreter::_FunctionAptActionGotoFrame(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x81
// 8027D8C0 AptActionInterpreter::_FunctionAptActionGetUrl(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x83
// 8027DA60 AptActionInterpreter::_FunctionAptActionStoreRegister(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x87
// 8027DAAC AptActionInterpreter::_FunctionAptActionDefineDictionary(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x88
// 8027DAD4 AptActionInterpreter::_FunctionAptActionWaitForFrame(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x8A
// 8027DAD8 AptActionInterpreter::_FunctionAptActionSetTarget(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x8B
// 8027DC4C AptActionInterpreter::_FunctionAptActionGotoLabel(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x8C
// 8027DD30 AptActionInterpreter::_FunctionAptActionWith(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x94
// 8027DE44 AptActionInterpreter::_FunctionAptActionPush(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x96
// 8027DF4C AptActionInterpreter::_FunctionAptActionGetUrl2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x9A
// 8027E3D4 AptActionInterpreter::_FunctionAptActionDefineFunction(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x9B
// 8027E580 AptActionInterpreter::_FunctionAptActionDefineFunction2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x8E
// 8027E6B4 AptActionInterpreter::_FunctionAptActionBranchIfTrue(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x9D
// 8027E798 AptActionInterpreter::_FunctionAptActionCallFrame(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x9E
// 8027E940 AptActionInterpreter::_FunctionAptActionGotoFrame2(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x9F
// 8027EC04 AptActionInterpreter::_FunctionAptActionBranchAlways(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x99
// 8027EC64 AptActionInterpreter::_FunctionAptActionPushThis(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x56
// 8027EDA8 AptActionInterpreter::_FunctionAptActionPushGlobal(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x58
// 8027EEEC AptActionInterpreter::_FunctionAptActionPush0(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x59
// 8027EFE8 AptActionInterpreter::_FunctionAptActionPush1(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x5A
// 8027F0E4 AptActionInterpreter::_FunctionAptActionPushTrue(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x73
// 8027F1E0 AptActionInterpreter::_FunctionAptActionPushFalse(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x74
// 8027F2DC AptActionInterpreter::_FunctionAptActionPushNULL(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x75
// 8027F340 AptActionInterpreter::_FunctionAptActionPushUndefined(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x76
// 8027F3A4 AptActionInterpreter::_FunctionAptActionCallFuncAndPop(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x5B
// 8027F444 AptActionInterpreter::_FunctionAptActionCallFuncSetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x5C
// 8027F4A0 AptActionInterpreter::_FunctionAptActionCallMethodPop(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x5D
// 8027F540 AptActionInterpreter::_FunctionAptActionCallMethodSetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x5E
// 8027F59C AptActionInterpreter::_FunctionAptActionPushThisVariable(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x70
// 8027F62C AptActionInterpreter::_FunctionAptActionPushGlobalVariable(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x71
// 8027F690 AptActionInterpreter::_FunctionAptActionPushZeroSetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x72
// 8027F79C AptActionInterpreter::_FunctionAptActionPushString(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xA1
// 8027F924 AptActionInterpreter::_FunctionAptActionPushStringDictByte(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xA2
// 8027F9A4 AptActionInterpreter::_FunctionAptActionPushStringDictWord(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xA3
// 8027FA30 AptActionInterpreter::_FunctionAptActionPushStringGetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xA4
// 8027FB4C AptActionInterpreter::_FunctionAptActionPushStringGetMember(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xA5
// 8027FCE4 AptActionInterpreter::_FunctionAptActionPushStringSetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xA6
// 8027FE7C AptActionInterpreter::_FunctionAptActionPushStringSetMember(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xA7
// 80280014 AptActionInterpreter::_FunctionAptActionStringDictByteGetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xAE
// 802800D4 AptActionInterpreter::_FunctionAptActionStringDictByteGetMember(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xAF
// 8028016C AptActionInterpreter::_FunctionAptActionDictCallFuncPop(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB0
// 80280274 AptActionInterpreter::_FunctionAptActionDictCallFuncSetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB1
// 80280338 AptActionInterpreter::_FunctionAptActionDictCallMethodPop(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB2
// 80280440 AptActionInterpreter::_FunctionAptActionDictCallMethodSetVar(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB3
// 80280504 AptActionInterpreter::_FunctionAptActionPushFloat(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB4
// 8028062C AptActionInterpreter::_FunctionAptActionPushByte(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB5
// 80280738 AptActionInterpreter::_FunctionAptActionPushWord(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB6
// 80280850 AptActionInterpreter::_FunctionAptActionPushDWord(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB7
// 80280978 AptActionInterpreter::_FunctionAptActionBranchIfFalse(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0xB8
// 80280A5C AptActionInterpreter::_FunctionAptActionExtends(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x69
// 80280FBC AptActionInterpreter::_FunctionAptActionInstanceOf(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x54
// 802811EC AptActionInterpreter::_FunctionAptActionCastOp(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x2B
// 80281424 AptActionInterpreter::_FunctionAptActionImplementsOp(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x2C
// 802817C8 AptActionInterpreter::_FunctionAptActionTry(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x8F
// 80281B14 AptActionInterpreter::_FunctionAptActionThrow(AptActionInterpreter *, AptActionInterpreter__LocalContextT *)  ->  AptActionInterpreter::run, case 0x2A
// 80281BD4 AptActionInterpreter::~AptActionInterpreter()  ->  the PC AptActionInterpreter destructor
// -- libaptz/AptAnimation.cpp.cpp
// 802832F4 AptAnimationPoolData::AptAnimationPoolData(AptInitParmsT &)  ->  the PC AptAnimationPoolData constructor
// 80283518 AptAnimationPoolData::~AptAnimationPoolData()  ->  the PC AptAnimationPoolData destructor
// 80284344 AptAnimationPoolData::addInput(unsigned int)  ->  AptAnimationPoolData::addInput (aptplayer.cpp)
// 802843D8 AptAnimationPoolData::addInput(AptInputType, AptInputState, AptInputController)  ->  AptAddToInputQueue (apt.cpp) -> AptAnimationPoolData::addInput
// 80284410 AptAnimationPoolData::removeTimerFunctions(AptCIH *)  ->  clearIntervalFn (aptbuiltins.cpp) / AptAnimationPoolData::tickIntervalTimers
// -- libaptz/AptArray.cpp.cpp
// 80285A2C AptArray::AptArray()  ->  AptArray::AptArray (aptruntime.hpp)
// 80285AA8 AptArray::~AptArray()  ->  the PC AptArray destructor
// 80285EB4 AptArray::RegisterReferences() const  ->  AptArray::RegisterReferences (aptvalue.cpp)
// 80285FEC AptArray::set(int, AptValue *)  ->  AptArray::SetOwn (aptvalue.cpp)
// 802860AC AptArray::get(int)  ->  AptArray::GetOwn (aptvalue.cpp)
// 802860E4 AptArray::toString(char *, char *)  ->  AptArray::DefaultString (aptvalue.cpp) / Array_toString (aptbuiltins.cpp)
// 80286168 AptArray::toString(EAStringC &, char *)  ->  AptArray::DefaultString (aptvalue.cpp) / Array_toString (aptbuiltins.cpp)
// 802862C8 AptArray::objectMemberLookup(AptValue *, EAStringC *) const  ->  AptArray::GetOwn (aptvalue.cpp)
// 80286C94 AptArray::objectMemberSet(AptValue *, EAStringC *, AptValue *)  ->  AptArray::SetOwn (aptvalue.cpp)
// 80286D08 AptArray::sMethod_concat(AptValue *, int)  ->  Array_concat (aptbuiltins.cpp)
// 80286E60 AptArray::sMethod_join(AptValue *, int)  ->  Array_join (aptbuiltins.cpp)
// 80287054 AptArray::sMethod_pop(AptValue *, int)  ->  Array_pop (aptbuiltins.cpp)
// 802870D4 AptArray::sMethod_push(AptValue *, int)  ->  Array_push (aptbuiltins.cpp)
// 802871F8 AptArray::sMethod_shift(AptValue *, int)  ->  Array_shift (aptbuiltins.cpp)
// 8028729C AptArray::sMethod_unshift(AptValue *, int)  ->  Array_unshift (aptbuiltins.cpp)
// 80287410 AptArray::defaultSortCompareFunc(void *, void *)  ->  Array_sort (aptbuiltins.cpp)
// 802874E0 AptArray::scriptFunctionSortFunc(void *, void *)  ->  Array_sort (aptbuiltins.cpp)
// 80287650 AptArray::sMethod_sort(AptValue *, int)  ->  Array_sort (aptbuiltins.cpp)
// 80287918 AptArray::sMethod_reverse(AptValue *, int)  ->  Array_reverse (aptbuiltins.cpp)
// 802879B8 AptArray::sMethod_splice(AptValue *, int)  ->  Array_splice (aptbuiltins.cpp)
// 80287C0C AptArray::sMethod_slice(AptValue *, int)  ->  Array_slice (aptbuiltins.cpp)
// 80287DD8 AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// 80287E40 AptArray::length()  ->  AptArray::GetOwn "length" (aptvalue.cpp)
// 80287E48 AptArray::GetAt(int) const  ->  AptArray::GetOwn (aptvalue.cpp)
// 80287E58 AptArray::SetAt(int, AptValue *)  ->  AptArray::SetOwn (aptvalue.cpp)
// -- libaptz/AptCIH.cpp.cpp
// 8028FE08 AptCIH::~AptCIH()  ->  the PC AptCIH destructor
// 802900D8 AptCIH::ClearCIH(bool)  ->  AptCIH::Remove (aptplayer.cpp)
// 802905D8 AptCIH::RegisterReferences() const  ->  AptCIH::RegisterReferences (aptplayer.cpp)
// 80290790 AptCIH::objectMemberLookup(AptValue *, EAStringC *) const  ->  AptCIH::GetOwn (aptbuiltins.cpp)
// 802907B8 AptCIH::objectMemberSet(AptValue *, EAStringC *, AptValue *)  ->  AptCIH::SetOwn (aptbuiltins.cpp)
// 802907E4 AptCIH::Release(char *, char *, int)  ->  AptCIH::Remove (aptplayer.cpp)
// 80290824 AptCIH::ensureStringAllocated(AptCIH *)  ->  AptCIH::render (aptplayer.cpp)
// 80291390 AptCIH::_getBoundingRect(AptRenderingContext *, AptRect *)  ->  ContentBounds (aptplayer.cpp)
// 80291550 AptCIH::getBoundingRect(AptRect *)  ->  MC_getBounds / ContentBounds (aptbuiltins.cpp, aptplayer.cpp)
// 802915FC AptCIH::getGlobalBoundingRect(AptRect *)  ->  MC_getBounds (aptbuiltins.cpp)
// 802916B4 AptCIH::getGlobalTranslation(float *, float *)  ->  MC_localToGlobal (aptbuiltins.cpp)
// 80291760 AptCIH::GetProceduralProperty(AptProceduralProperty)  ->  AptCIH::GetProperty (aptplayer.cpp)
// 80291B30 AptCIH::setProceduralProperty(AptProceduralProperty, float, bool)  ->  AptCIH::SetProperty (aptplayer.cpp)
// 802922FC AptCIH::associateInstToClass()  ->  AptCreateInstance (aptplayer.cpp)
// 80292C94 AptCIH::queueClipEvents(int, unsigned int, int)  ->  AptCIH::queueClipEvent (aptplayer.cpp)
// 80293528 AptCIH::getRootAnimation()  ->  AptCIH::GetRoot (aptplayer.cpp)
// 802939E8 AptCIH::AptCIH(AptVirtualFunctionTable_Indices, AptCharacterInst *, AptCIH *)  ->  AptCIH::AptCIH (aptplayer.cpp)
// 80293B50 AptCIH::isSpriteInst(bool) const  ->  AptCIH::IsSprite (aptplayer.hpp)
// 80293CB0 AptCIH::isTextInst() const  ->  AptCIH::IsText (aptplayer.hpp)
// 80293E3C AptCIH::isSpriteInstBase(bool) const  ->  AptCIH::IsSprite (aptplayer.hpp)
// -- libaptz/AptCharacter.cpp.cpp
// 8028F9B4 AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// -- libaptz/AptDate.cpp.cpp
// 80298BC8 AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// -- libaptz/AptDisplayList.cpp.cpp
// 80299970 AptDisplayList::instantiateCharacter(int, AptCharacter *, EAStringC *, AptCIH *, int, int, AptCIH **, int *)  ->  AptCreateInstance (aptplayer.cpp)
// 8029A2E4 AptDisplayList::placeObjectNCXForm(AptCIH *, int, AptCharacter *, EAStringC *, AptCIH *, int, int, AptnCXForm *, AptMatrix *, AptEventActionSet *, float)  ->  AptCIH::placeObject (aptplayer.cpp)
// 8029A6B0 AptDisplayList::placeObject(AptCIH *, int, AptCharacter *, EAStringC *, AptCIH *, int, int, AptCXForm *, AptMatrix *, AptEventActionSet *, float, AptValue *)  ->  AptCIH::placeObject (aptplayer.cpp)
// 8029A9E0 AptDisplayList::placeObject(AptControlPlaceObject2 *, AptCIH *)  ->  AptCIH::placeObject (aptplayer.cpp)
// 8029AE18 AptDisplayList::removeObject(AptCIH *)  ->  AptDisplayList::remove (aptplayer.cpp)
// 8029AEC4 AptDisplayList::removeObject(int)  ->  AptCIH::removeObject (aptplayer.cpp)
// 8029AF0C AptDisplayList::removeObject(AptControlRemoveObject2 *)  ->  AptCIH::doFrameControls -> AptCIH::removeObject (aptplayer.cpp)
// 8029AF30 AptDisplayList::removeClonedObject(AptCIH *)  ->  MC_removeMovieClip (aptbuiltins.cpp)
// 8029AF8C AptDisplayList::AptDisplayList()  ->  the PC AptDisplayList constructor
// 8029AFCC AptDisplayList::~AptDisplayList()  ->  the PC AptDisplayList destructor
// 8029B89C AptDisplayList::clear(bool)  ->  AptDisplayList::clear (aptplayer.cpp)
// 8029B9FC AptDisplayList::RemoveFromDisplayList(AptNativeHash *, AptCIH *)  ->  AptDisplayList::remove (aptplayer.cpp)
// 8029BA20 AptDisplayList::AddToDisplayList(AptNativeHash *, AptPseudoCIH_t *, AptCIH *)  ->  AptDisplayList::insert (aptplayer.cpp)
// -- libaptz/AptGC.cpp.cpp
// 8029C654 AptGC::sReferenceRegistrationCb(AptValue *, AptValue *, char *)  ->  AptGC::Mark (aptvalue.cpp)
// 8029C69C AptGC::CleanUnreachable()  ->  AptGC::CleanUnreachable (aptvalue.cpp)
// -- libaptz/AptGlobalObject.cpp.cpp
// 802B5B6C AptGlobal::objectMemberLookup(AptValue *, EAStringC *) const  ->  AptObject::GetOwn (aptvalue.cpp)
// 802B5BC8 AptGlobal::objectMemberSet(AptValue *, EAStringC *, AptValue *)  ->  AptObject::SetOwn (aptvalue.cpp)
// 802B5C2C AptGlobal::AptGlobal()  ->  AptGlobal::AptGlobal (aptruntime.hpp)
// 802B5CA4 AptGlobal::~AptGlobal()  ->  the PC AptGlobal destructor
// 802B5D0C AptGlobal::Set(EAStringC *, AptValue *)  ->  AptObject::Put (aptvalue.cpp)
// 802B5D30 AptGlobal::Lookup(EAStringC *) const  ->  AptObject::Find (aptvalue.cpp)
// -- libaptz/AptInput.cpp.cpp
// 8029D428 AptAnimationPoolData::ProcessInputSet(AptInputType, AptInputState, unsigned int, AptInputController, bool)  ->  AptAnimationPoolData::ProcessInputSet (aptplayer.cpp)
// 8029D75C AptAnimationPoolData::ProcessListenerEvents(AptInputType, AptInputState, unsigned int, AptInputController)  ->  AptAnimationPoolData::ProcessListenerEvents (aptplayer.cpp)
// 8029D838 AptAnimationPoolData::AddListenerToQueue(AptValue *, int, unsigned int)  ->  AptAnimationPoolData::ProcessListenerEvents (aptplayer.cpp)
// 8029E150 AptAnimationPoolData::ProcessAptInput(unsigned int, bool)  ->  AptAnimationPoolData::ProcessAptInput (aptplayer.cpp)
// 8029E1F8 AptAnimationPoolData::ProcessInputs()  ->  AptAnimationPoolData::ProcessInputs (aptplayer.cpp)
// -- libaptz/AptMiscObjects.cpp.cpp
// 802A8664 AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// -- libaptz/AptObject.cpp.cpp
// 802B5E14 AptObject::objectMemberLookup(AptValue *, EAStringC *) const  ->  AptObject::GetOwn (aptvalue.cpp)
// 802B5E54 AptObject::RegisterReferences() const  ->  AptObject::RegisterReferences (aptvalue.cpp)
// 802B5E94 AptObject::SetImplementedObjects(AptArray *, int)  ->  AptActionInterpreter::run, case 0x2C
// 802B5F18 AptObject::GetImplementedObjects(int *) const  ->  AptActionInterpreter::instanceOf (aptactioninterpreter.cpp)
// 802B5FB0 AptObject::DoesImplementObject(AptValue *) const  ->  AptActionInterpreter::instanceOf (aptactioninterpreter.cpp)
// 802B6320 AptObject::AptObject(AptVirtualFunctionTable_Indices, int)  ->  AptObject::AptObject (aptvalue.cpp)
// 802B63E4 AptObject::Set(EAStringC *, AptValue *)  ->  AptObject::Put (aptvalue.cpp)
// 802B6408 AptObject::Lookup(EAStringC *) const  ->  AptObject::Find (aptvalue.cpp)
// 802B655C AptObject::~AptObject()  ->  the PC AptObject destructor
// -- libaptz/AptRenderingContext.cpp.cpp
// 802AB188 AptRenderingContext::AptRenderingContext()  ->  the PC AptRenderingContext constructor
// 802AB1E4 AptRenderingContext::pushColourTransform()  ->  AptRenderingContext::pushColourTransform (aptplayer.cpp)
// 802AB25C AptRenderingContext::popColourTransform()  ->  AptRenderingContext::popColourTransform (aptplayer.cpp)
// 802AB2D0 AptRenderingContext::appendColourTransform(AptCXForm *)  ->  AptRenderingContext::appendColourTransform (aptplayer.cpp)
// 802AB380 AptRenderingContext::getVertexMatrix(AptMatrix *)  ->  AptRenderingContext::getVertexMatrix (aptplayer.hpp)
// 802AB400 AptRenderingContext::pushVertexMatrix()  ->  AptRenderingContext::pushVertexMatrix (aptplayer.cpp)
// 802AB450 AptRenderingContext::popVertexMatrix()  ->  AptRenderingContext::popVertexMatrix (aptplayer.cpp)
// 802AB4D0 AptRenderingContext::multMatrix(AptMatrix *, AptMatrix *, AptMatrix *)  ->  Concat (aptplayer.cpp)
// 802AB5CC AptRenderingContext::appendVertexMatrix(AptMatrix *)  ->  AptRenderingContext::appendVertexMatrix (aptplayer.cpp)
// 802AB614 AptRenderingContext::expandBoundingRect(AptRect *, AptRect *)  ->  ExpandRect (aptplayer.cpp)
// -- libaptz/AptScriptColour.cpp.cpp
// 802B78B8 AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// -- libaptz/AptScriptFunction.cpp.cpp
// 802B7A74 AptScriptFunctionBase::AptScriptFunctionBase(AptVirtualFunctionTable_Indices, AptScriptFunctionBase *, AptCIH *, bool)  ->  the PC AptScriptFunctionBase constructor
// 802B7D54 AptScriptFunctionBase::AptScriptFunctionBase(AptVirtualFunctionTable_Indices, AptScriptFunctionBase *, AptCIH *)  ->  the PC AptScriptFunctionBase constructor
// 802B7FA8 AptScriptFunctionBase::~AptScriptFunctionBase()  ->  the PC AptScriptFunctionBase destructor
// 802B820C AptScriptFunctionBase::RegisterReferences() const  ->  AptScriptFunctionBase::RegisterReferences (aptvalue.cpp)
// 802B837C AptScriptFunctionBase::SetupBeforeExecution(_AptScriptFunctionState *, AptValue *)  ->  AptActionInterpreter::runFunction (aptactioninterpreter.cpp)
// 802B8390 AptScriptFunctionBase::CleanupAfterExecution(_AptScriptFunctionState *)  ->  AptActionInterpreter::runFunction (aptactioninterpreter.cpp)
// 802B8420 AptScriptFunctionBase::GetRegisterValue(int)  ->  AptActionInterpreter::run, case 0x96 (register push)
// 802B8430 AptScriptFunctionBase::SetRegisterValue(int, AptValue *)  ->  AptActionInterpreter::run, case 0x87
// 802B8804 AptScriptFunctionBase::ExistsInLocalScope(EAStringC *)  ->  AptActionInterpreter::getVariable (aptactioninterpreter.cpp)
// 802B885C AptScriptFunctionBase::SetInLocalScope(EAStringC *, AptValue *)  ->  AptActionInterpreter::run, case 0x3C/0x41 (DefineLocal)
// 802B88AC AptScriptFunctionBase::SetWhereExistsInScopeChain(EAStringC *, AptValue *)  ->  AptActionInterpreter::setVariable (aptactioninterpreter.cpp)
// 802B8980 AptScriptFunctionBase::GetInScopeChain(EAStringC *)  ->  AptActionInterpreter::getVariable (aptactioninterpreter.cpp)
// 802B8A18 AptScriptFunctionBase::CreateFrameStack()  ->  AptActionInterpreter::runFunction (aptactioninterpreter.cpp)
// 802B8B70 AptScriptFunctionBase::CreatingNestedFunction()  ->  AptActionInterpreter::run, case 0x9B/0x8E
// 802B9284 AptFrameStack::RegisterReferences() const  ->  AptFrameStack::RegisterReferences (aptvalue.cpp)
// 802B92D4 AptFrameStack::~AptFrameStack()  ->  the PC AptFrameStack destructor
// -- libaptz/AptSound.cpp.cpp
// 802ABDCC AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// -- libaptz/AptString.cpp.cpp
// 802AC2E4 AptString::AptString()  ->  AptString::AptString (aptruntime.hpp)
// 802AC344 AptString::~AptString()  ->  the PC AptString destructor
// 802AC3D0 AptString::AptString(char *)  ->  AptString::AptString (aptruntime.hpp)
// 802AC7EC AptString::sMethod_charAt(AptValue *, int)  ->  String_charAt (aptbuiltins.cpp)
// 802AC960 AptString::sMethod_charCodeAt(AptValue *, int)  ->  String_charCodeAt (aptbuiltins.cpp)
// 802ACB50 AptString::sMethod_concat(AptValue *, int)  ->  String_concat (aptbuiltins.cpp)
// 802ACD2C AptString::sMethod_fromCharCode(AptValue *, int)  ->  String_fromCharCode (aptbuiltins.cpp)
// 802ACEC4 AptString::sMethod_indexOf(AptValue *, int)  ->  String_indexOf (aptbuiltins.cpp)
// 802AD0D4 AptString::sMethod_lastIndexOf(AptValue *, int)  ->  String_lastIndexOf (aptbuiltins.cpp)
// 802AD0DC AptString::sMethod_slice(AptValue *, int)  ->  String_slice (aptbuiltins.cpp)
// 802AD3C4 AptString::sMethod_split(AptValue *, int)  ->  String_split (aptbuiltins.cpp)
// 802AD964 AptString::sMethod_substr(AptValue *, int)  ->  String_substr (aptbuiltins.cpp)
// 802ADB94 AptString::sMethod_substring(AptValue *, int)  ->  String_substring (aptbuiltins.cpp)
// 802ADDF0 AptString::sMethod_toLowerCase(AptValue *, int)  ->  String_toLowerCase (aptbuiltins.cpp)
// 802ADF44 AptString::sMethod_toUpperCase(AptValue *, int)  ->  String_toUpperCase (aptbuiltins.cpp)
// 802AE098 AptString::objectMemberLookup(AptValue *, EAStringC *) const  ->  AptActionInterpreter::getMember, string "length" (aptactioninterpreter.cpp)
// 802AEDAC AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// -- libaptz/AptValue.cpp.cpp
// 802B0838 AptValue::toString(char *) const  ->  AptVar::ToString (aptvalue.cpp)
// 802B43EC AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// 802B453C AptExtern::~AptExtern()  ->  the PC AptExtern destructor
// 802B45FC AptValue::AptValue(AptVirtualFunctionTable_Indices)  ->  AptValue::AptValue (aptvalue.cpp)
// 802B46D0 AptValue::AptValue(AptVirtualFunctionTable_Indices, AptValue__CIH_ONLY)  ->  AptValue::AptValue (aptvalue.cpp)
// 802B4770 AptValue::~AptValue()  ->  AptValue::~AptValue (aptvalue.cpp)
// -- libaptz/AptXml.cpp.cpp
// 802BA224 AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
// -- libaptz/AptXmlNode.cpp.cpp
// 802BB6A0 AptNativeFunction::~AptNativeFunction()  ->  the PC AptNativeFunction destructor
