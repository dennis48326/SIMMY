// @class AptVar after AptFileImage AptCharacter AptMatrix AptCXForm AptRect
// @class AptValue after AptVar
// @class AptString after AptValue
// @class AptObject after AptValue
// @class AptArray after AptObject
// @class AptNativeFunction after AptObject
// @class AptScriptFunctionBase after AptObject
// @class AptExtern after AptObject
// @class AptGlobal after AptObject
// @class AptFrameStack after AptObject
// @class AptFunctionValue after AptObject
// @class AptGC after AptObject
// The Apt ActionScript runtime (libaptz: AptValue.cpp, AptObject.cpp, AptArray.cpp, AptString.cpp,
// AptScriptFunction.cpp, AptGC.cpp).
//
// PC design: Apt kept every value, numbers included, as a refcounted pool object with a small
// vtable index (AptVirtualFunctionTable_Indices) and collected cycles with a mark/sweep pass
// (AptGC::CleanUnreachable). Here primitives (undefined, null, boolean, integer, float) and
// references live in a tagged AptVar; strings, objects, arrays, functions and character instances
// are AptValue heap objects owned by one mark/sweep collector (AptGC), run between player ticks
// from the roots AptGC::MarkRoots knows (globals, levels, the action queue, interval timers).
// Conversions follow AptValue::toInteger/toFloat/toBool/toString (SWF version 7: integers and
// 32-bit floats are separate types, undefined converts to "" and 0).
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct AptValue;
struct AptString;
struct AptObject;
struct AptFunctionValue;
struct AptCIH;
struct AptActionInterpreter;

// A value on the interpreter stack, in a register or in a property.
struct AptVar {
  enum Type : unsigned char { kUndefined, kNull, kBoolean, kInteger, kFloat, kString, kObject };
  Type type = kUndefined;
  // A name's byte sum (the UI targets' variable hash; game/targets_tsc3.cpp).
  static int Hash(const char* name);  // 801E97D0
  union {
    int i;
    float f;
    AptValue* p;  // kString: AptString; kObject: any other AptValue
  };
  AptVar() : i(0) {}
  static AptVar Undefined() { return AptVar(); }
  static AptVar Null() {
    AptVar v;
    v.type = kNull;
    return v;
  }
  static AptVar Bool(bool b) {
    AptVar v;
    v.type = kBoolean;
    v.i = b ? 1 : 0;
    return v;
  }
  static AptVar Int(int n) {
    AptVar v;
    v.type = kInteger;
    v.i = n;
    return v;
  }
  static AptVar Float(float x) {
    AptVar v;
    v.type = kFloat;
    v.f = x;
    return v;
  }
  // A number result: integral values that fit become integers, as Apt's arithmetic does for ints.
  static AptVar Number(double x);
  static AptVar Str(const std::string& s);
  static AptVar Str(AptString* s);
  static AptVar Obj(AptValue* o);

  bool IsUndefined() const { return type == kUndefined; }
  bool IsNullish() const { return type == kUndefined || type == kNull; }
  bool IsNumber() const { return type == kInteger || type == kFloat; }
  bool IsString() const { return type == kString; }
  bool IsObject() const { return type == kObject && p; }
  AptString* AsString() const { return type == kString ? reinterpret_cast<AptString*>(p) : nullptr; }
  AptObject* AsObject() const;  // objects, arrays, functions, character instances
  AptCIH* AsCIH() const;
  AptFunctionValue* AsFunction() const;

  int ToInteger() const;       // 802B04B8 AptValue::toInteger
  float ToFloat() const;       // 802B05B8 AptValue::toFloat
  bool ToBool() const;         // 802B06AC AptValue::toBool
  std::string ToString() const;  // 802B08BC AptValue::toString
  const char* TypeOf() const;  // the strings of _FunctionAptActionTypeOf
  // The "not a number" test Apt inlines in isNaN, ToNumber, LessThan2 and Equals2: numbers never
  // are, strings unless numeric (IsNumericString), undefined only for SWF 7 (never on GC, see ToBool),
  // everything else (booleans, null, objects) always is.
  bool IsNaNLike() const;
  static bool IsNumericString(const std::string& s);
};

// Every heap value: strings, objects and what derives from them. Allocated with new; owned by AptGC.
struct AptValue {
  enum Kind : unsigned char {
    kString, kObject, kArray, kFunction, kCIH, kExtern, kGlobal, kTextFormat, kColour, kXmlNode, kKey, kMath,
    kStage, kDate,
  };
  AptValue() : AptValue(kObject) {}
  explicit AptValue(Kind kind);
  virtual ~AptValue();
  virtual void RegisterReferences() const {}  // marks what this value references (AptGC)

  Kind m_kind;
  bool m_marked = false;
  AptValue* m_gcNext = nullptr;  // AptGC's list of every live value

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  RET_UNKNOWN urlEncode();
  RET_UNKNOWN urlEncodeCustomRender();
  RET_UNKNOWN findChild(EAStringC *, AptValue *);
  RET_UNKNOWN isMCInParentChain() const;
  RET_UNKNOWN CanCreateScriptObject() const;
  virtual RET_UNKNOWN AddRef(char *, char *, int);
  virtual RET_UNKNOWN Release(char *, char *, int);
  virtual void ForceDelete();
  virtual RET_UNKNOWN GetNativeHashVirtual();
  virtual RET_UNKNOWN ContainsNativeHashVirtual() const;
  virtual RET_UNKNOWN getHasClass() const;
  virtual void setHasClass(int);
  virtual RET_UNKNOWN objectMemberLookup(AptValue *, EAStringC *) const;
  virtual RET_UNKNOWN objectMemberSet(AptValue *, EAStringC *, AptValue *);
  void SetAllowDelayedDeletion(bool);
  RET_UNKNOWN GetAllowDelayedDeletion();
  virtual void DeleteThis();
  virtual void PreDestroy();
  virtual void DestroyGCPointers();
  RET_UNKNOWN GetMaxRefCountHit() const;
  void SetMaxRefCountHit(bool);
  RET_UNKNOWN getRefCount() const;
  RET_UNKNOWN getVtblIndex() const;
  RET_UNKNOWN getGCMark() const;
  RET_UNKNOWN getGCRoot() const;
  RET_UNKNOWN getIsDefined() const;
  void setRefCount(unsigned int);
  void setVtblIndex(AptVirtualFunctionTable_Indices);
  void setIsDefined(bool);
  void setGCMark(bool);
  void setGCRoot(unsigned int);
  void incGCRoot();
  void decGCRoot();
  void SetReleaseAtEnd();
  void ClearReleaseAtEnd();
  RET_UNKNOWN IsReleaseAtEnd() const;
  void c_lookup() const;
  void c_integer() const;
  void c_register() const;
  void c_float() const;
  RET_UNKNOWN c_string() const;
  void c_boolean() const;
  void c_scriptfunction() const;
  void c_nativefunction() const;
  void c_cih(bool) const;
  void c_array() const;
  void c_sound() const;
  void c_key() const;
  void c_global() const;
  void c_math() const;
  void c_scriptcolour() const;
  void c_object() const;
  void c_prototype() const;
  void c_date() const;
  void c_textformat() const;
  void c_movieClip() const;
  void c_xmlnode() const;
  void c_xml() const;
  void c_xmlattributes() const;
  void c_loadvars() const;
  void c_stage() const;
  bool isXmlNode() const;
  bool isXml() const;
  bool isXmlAttributes() const;
  bool isLoadVars() const;
  bool isNone() const;
  RET_UNKNOWN isUndefined() const;
  RET_UNKNOWN isLookup() const;
  RET_UNKNOWN isString() const;
  RET_UNKNOWN isBoolean() const;
  RET_UNKNOWN isInteger() const;
  RET_UNKNOWN isRegister() const;
  RET_UNKNOWN isFloat() const;
  RET_UNKNOWN isNativeFunction() const;
  RET_UNKNOWN isScriptFunction() const;
  RET_UNKNOWN isExtern() const;
  RET_UNKNOWN isFrameStack() const;
  RET_UNKNOWN isArray() const;
  RET_UNKNOWN isSound() const;
  RET_UNKNOWN isKey() const;
  RET_UNKNOWN isMath() const;
  RET_UNKNOWN isScriptColour() const;
  RET_UNKNOWN isCIH(bool) const;
  RET_UNKNOWN isObject() const;
  RET_UNKNOWN isPrototype() const;
  RET_UNKNOWN isDate() const;
  RET_UNKNOWN isTextFormat() const;
  RET_UNKNOWN isMovieClip() const;
  RET_UNKNOWN isStage() const;
};

struct AptString : AptValue {
  explicit AptString(std::string text) : AptValue(kString), m_text(std::move(text)) {}
  std::string m_text;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  virtual void DeleteThis();
  virtual void ForceDelete();
  static void CleanNativeFunctions();
  static RET_UNKNOWN printf(char *,...);
  static RET_UNKNOWN Create(char *);
  static void cpy(EAStringC *);
  static void cpy(char *);
  static void cat(EAStringC *);
  static void cat(char *);
  RET_UNKNOWN GetInternalString();
  void SetNext(AptString *);
  RET_UNKNOWN GetNext() const;
  static RET_UNKNOWN Create();
  void Destroy();
};

// One property. Getter/setter pairs come from Object.addProperty.
struct AptProperty {
  AptVar value;
  AptVar getter, setter;
  unsigned int flags = 0;  // ASSetPropFlags: 1 dont-enum, 2 dont-delete, 4 read-only
};

// An ActionScript object: an ordered property table and its __proto__ (AptObject / AptPrototype /
// AptValueWithHash's AptNativeHash on GC).
struct AptObject : AptValue {
  explicit AptObject(Kind kind = kObject);
  void RegisterReferences() const override;

  // Own (and native) members. Get/Set return false when the name is not handled here.
  virtual bool GetOwn(AptActionInterpreter& in, const std::string& name, AptVar& out);
  virtual bool SetOwn(AptActionInterpreter& in, const std::string& name, const AptVar& value);
  virtual bool Delete(const std::string& name);
  virtual void Enumerate(std::vector<std::string>& names);
  virtual std::string DefaultString() { return "[object Object]"; }

  // Lookup through __proto__ (with getters); undefined when absent.
  AptVar Get(AptActionInterpreter& in, const std::string& name);
  bool Has(AptActionInterpreter& in, const std::string& name);
  void Set(AptActionInterpreter& in, const std::string& name, const AptVar& value);
  // Plain storage without natives or setters.
  AptProperty* Find(const std::string& name);
  void Put(const std::string& name, const AptVar& value, unsigned int flags = 0);

  std::vector<std::pair<std::string, AptProperty>> m_props;
  std::unordered_map<std::string, size_t> m_index;
  AptObject* m_proto = nullptr;  // __proto__
  // Interfaces implemented (AptObject::SetImplementedObjects, for instanceof / implements).
  std::vector<AptObject*> m_interfaces;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  virtual void DestroyGCPointers();
  virtual void setHasClass(int);
  virtual RET_UNKNOWN getHasClass() const;
  void setInMainInst(int);
  RET_UNKNOWN getInMainInst() const;
  void SetPrototype(AptValue *);
};

struct AptArray : AptObject {
  AptArray() : AptObject(kArray) {}
  void RegisterReferences() const override;
  bool GetOwn(AptActionInterpreter& in, const std::string& name, AptVar& out) override;
  bool SetOwn(AptActionInterpreter& in, const std::string& name, const AptVar& value) override;
  void Enumerate(std::vector<std::string>& names) override;
  std::string DefaultString() override;
  std::vector<AptVar> m_items;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  AptArray(int, AptValue **);
  static void CleanNativeFunctions();
  virtual void DestroyGCPointers();
  void _reserve(int);
  static RET_UNKNOWN defaultSortOnCompareFunc(void *, void *);
  static RET_UNKNOWN sMethod_sortOn(AptValue *, int);
  void ConvertAptValue(AptValue *);
};

// What can be called: script functions (DefineFunction/DefineFunction2) and natives.
struct AptFunctionValue : AptObject {
  AptFunctionValue() : AptObject(kFunction) {}
  virtual AptVar Call(AptActionInterpreter& in, const AptVar& thisv, const std::vector<AptVar>& args,
                      AptObject* super = nullptr) = 0;
  std::string DefaultString() override { return "[function]"; }
};

using AptNativeFn = AptVar (*)(AptActionInterpreter& in, const AptVar& thisv, const std::vector<AptVar>& args);

struct AptNativeFunction : AptFunctionValue {
  AptNativeFunction(AptNativeFn fn, const char* name) : m_fn(fn), m_name(name) {}
  AptVar Call(AptActionInterpreter& in, const AptVar& thisv, const std::vector<AptVar>& args,
              AptObject* super) override;
  AptNativeFn m_fn;
  const char* m_name;
};

// Locals of a running script function (AptFrameStack): named variables, chained to the frame the
// function was defined in so nested functions see their outer locals.
struct AptFrameStack : AptObject {
  explicit AptFrameStack(AptFrameStack* outer) : m_outer(outer) {}
  void RegisterReferences() const override;
  AptFrameStack* m_outer;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  virtual void DestroyGCPointers();
};

// A function defined by DefineFunction (1) or DefineFunction2 (2). The body stays in the movie's
// .apt image; arguments and the constant pool in force at definition time are kept here (on GC the
// pool was written into the stream, AptAction_DefineFunction +0x10).
struct AptScriptFunctionBase : AptFunctionValue {
  AptVar Call(AptActionInterpreter& in, const AptVar& thisv, const std::vector<AptVar>& args,
              AptObject* super) override;
  void RegisterReferences() const override;

  int m_version = 1;                      // 1 or 2
  std::string m_name;
  const unsigned char* m_body = nullptr;  // bytecode
  int m_bodySize = 0;
  AptFileImage* m_file = nullptr;         // the .apt image the body and constants belong to
  std::vector<std::pair<int, std::string>> m_args;  // (register or 0, name)
  int m_registerCount = 0;                // DefineFunction2
  unsigned int m_flags = 0;               // DefineFunction2 preload/suppress flags
  std::vector<AptVar> m_constantPool;     // the dictionary at definition time
  AptCIH* m_target = nullptr;             // the clip the function was defined in
  AptFrameStack* m_scope = nullptr;       // enclosing function's locals
  std::vector<AptObject*> m_withScope;    // enclosing with() objects

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  static void InitializeStaticData(AptInitParmsT &);
  static void ShutdownStaticData();
  static void PushStaticData();
  static void PopStaticData(void *);
  virtual void PreDestroy();
  virtual void DestroyGCPointers();
};

// `extern`: member reads and writes go to the game (getExternVariable / setExternVariable).
struct AptExtern : AptObject {
  AptExtern() : AptObject(kExtern) {}
  bool GetOwn(AptActionInterpreter& in, const std::string& name, AptVar& out) override;
  bool SetOwn(AptActionInterpreter& in, const std::string& name, const AptVar& value) override;
  std::string DefaultString() override { return "[Extern]"; }

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  virtual RET_UNKNOWN AddRef(char *, char *, int);
  virtual RET_UNKNOWN Release(char *, char *, int);
};

struct AptGlobal : AptObject {
  AptGlobal() : AptObject(kGlobal) {}
};

// The collector (AptGC.cpp). Values register themselves on construction.
struct AptGC {
  static void Register(AptValue* v);
  static void Mark(AptValue* v);
  static void Mark(const AptVar& v) {
    if ((v.type == AptVar::kObject || v.type == AptVar::kString) && v.p) Mark(v.p);
  }
  static void CleanUnreachable();  // 80268... AptGC::CleanUnreachable (mark from the roots, free the rest)
  static void AddRoot(AptValue* v);
  static void RemoveRoot(AptValue* v);
  // PC: calls `fn` for every value still alive (after CleanUnreachable: the reachable ones).
  static void ForEachLive(void (*fn)(AptValue* v, void* user), void* user);
  static void ForEachReachable(void (*fn)(AptValue* v, void* user), void* user);  // PC
  static void MarkRoots();  // PC: CleanUnreachable's mark phase
  static size_t s_allocatedSinceCollect;
  static size_t s_liveCount;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  static void Initialize();
  static void CleanAll();
};
