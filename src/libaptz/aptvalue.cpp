// Port of libaptz AptValue.cpp, AptObject.cpp, AptArray.cpp (storage), AptGC.cpp: values,
// conversions, objects and the collector. See aptruntime.hpp for the PC value model.
#include "simmy/types_gen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
AptValue* s_gcHead = nullptr;
std::vector<AptValue*> s_roots;

// Apt_atoff (8026B7A4): atof that stops at the first character that is not part of a number.
float AtoF(const std::string& s) { return static_cast<float>(std::atof(s.c_str())); }
}  // namespace

size_t AptGC::s_allocatedSinceCollect = 0;
size_t AptGC::s_liveCount = 0;

// ---- AptVar ---------------------------------------------------------------------------------------

// 801E97D0
int AptVar::Hash(const char* name) {
  int sum = 0;
  for (const char* p = name; *p; ++p) sum += *p;
  return sum;
}

AptVar AptVar::Number(double x) {
  if (std::isfinite(x) && x == std::floor(x) && x >= -2147483648.0 && x <= 2147483647.0) return Int(static_cast<int>(x));
  return Float(static_cast<float>(x));
}
AptVar AptVar::Str(const std::string& s) { return Str(new AptString(s)); }
AptVar AptVar::Str(AptString* s) {
  AptVar v;
  v.type = kString;
  v.p = s;
  return v;
}
AptVar AptVar::Obj(AptValue* o) {
  if (!o) return Null();
  if (o->m_kind == AptValue::kString) return Str(static_cast<AptString*>(o));
  AptVar v;
  v.type = kObject;
  v.p = o;
  return v;
}

AptObject* AptVar::AsObject() const {
  return type == kObject && p && p->m_kind != AptValue::kString ? static_cast<AptObject*>(p) : nullptr;
}
AptCIH* AptVar::AsCIH() const {
  return type == kObject && p && p->m_kind == AptValue::kCIH ? static_cast<AptCIH*>(p) : nullptr;
}
AptFunctionValue* AptVar::AsFunction() const {
  return type == kObject && p && p->m_kind == AptValue::kFunction ? static_cast<AptFunctionValue*>(p) : nullptr;
}

// 802B04B8
// Undefined and null are 0; floats truncate; strings are decimal or "0x" hex; other objects are 1.
int AptVar::ToInteger() const {
  switch (type) {
    case kUndefined:
    case kNull: return 0;
    case kBoolean:
    case kInteger: return i;
    case kFloat: return std::isfinite(f) ? static_cast<int>(f) : 0;
    case kString: {
      const std::string& s = AsString()->m_text;
      if (s.size() > 2 && s[0] == '0' && s[1] == 'x') return static_cast<int>(std::strtol(s.c_str(), nullptr, 16));
      return std::atoi(s.c_str());
    }
    default: return 1;
  }
}

// 802B05B8
float AptVar::ToFloat() const {
  switch (type) {
    case kUndefined:
    case kNull: return 0.0f;
    case kBoolean:
    case kInteger: return static_cast<float>(i);
    case kFloat: return f;
    case kString: return AtoF(AsString()->m_text);
    default: return 1.0f;
  }
}

// 802B06AC
// A string is true by the SWF 6 rules: its number ("0x" hex, else Apt_atoff) is not 0. (SWF 7's
// "not empty" applies when AptGetSwfVersion is 7, but the GC never is: the version comes from the
// first movie, AptCharacterAnimationInst::getSwfVersion reads it from the AptFile's +0xC, which is
// the .const image ("Apt constant file", no ':' at +8), so it is always 6. The movies rely on it:
// cmdProc's SetHandlesInput passes Boolean("0") for the HUD widgets that must not take input.)
bool AptVar::ToBool() const {
  switch (type) {
    case kUndefined:
    case kNull: return false;
    case kBoolean:
    case kInteger: return i != 0;
    case kFloat: return f != 0.0f;
    case kString: {
      const std::string& t = AsString()->m_text;
      if (t.size() >= 3 && t[0] == '0' && t[1] == 'x') return std::strtol(t.c_str(), nullptr, 16) != 0;
      return AtoF(t) != 0.0f;
    }
    default: return true;
  }
}

// 802B08BC
// Undefined is ""; whole floats print as integers, others with %f; objects by type.
std::string AptVar::ToString() const {
  char buf[64];
  switch (type) {
    case kUndefined: return "";
    case kNull: return "null";
    case kBoolean: return i ? "true" : "false";
    case kInteger: std::snprintf(buf, sizeof(buf), "%d", i); return buf;
    case kFloat:
      if (std::fmod(f, 1.0f) == 0.0f) std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(f));
      else std::snprintf(buf, sizeof(buf), "%f", static_cast<double>(f));
      return buf;
    case kString: return AsString()->m_text;
    default: {
      AptObject* o = AsObject();
      return o ? o->DefaultString() : "";
    }
  }
}

// Inlined in 8026E69C isNaN, 8027A020 ToNumber, 80278310 LessThan2, 80278E44 Equals2: not empty;
// "0x" + hex digits only; else the first character is a digit, '.', '+' or '-', the last a digit, '.',
// '+', '-' or 'e', and the rest digits with at most one '.' and an 'e' (not second, nor third after a
// sign) followed by a sign or digit.
bool AptVar::IsNumericString(const std::string& s) {
  const int n = static_cast<int>(s.size());
  if (n == 0) return false;
  auto digit = [](char c) { return c >= '0' && c <= '9'; };
  if (s[0] == '0' && n > 2 && s[1] == 'x') {
    char* end = nullptr;
    std::strtol(s.c_str(), &end, 16);
    if (*end == '\0') return true;
  }
  const char last = s[n - 1];
  if (!(last == '-' || last == '+' || last == 'e' || last == '.' || digit(last))) return false;
  if (!(s[0] == '.' || s[0] == '-' || s[0] == '+' || digit(s[0]))) return false;
  bool dot = false;
  for (int i = 1; i < n; ++i) {
    const char c = s[i];
    if (c == '.' && !dot) {
      dot = true;
    } else if (c == 'e' && i != 1) {
      if (i == 2 && (s[0] == '+' || s[0] == '-')) return false;
      if (i + 1 < n) {
        const char e = s[i + 1];
        if (e != '-' && e != '+' && !digit(e)) return false;
        ++i;
      }
    } else if (!digit(c)) {
      return false;
    }
  }
  return true;
}

bool AptVar::IsNaNLike() const {
  switch (type) {
    case kInteger:
    case kFloat: return false;
    case kString: return !IsNumericString(AsString()->m_text);
    case kUndefined: return false;  // SWF 6 (AptGetSwfVersion() == 7 would make it NaN)
    default: return true;
  }
}

const char* AptVar::TypeOf() const {
  switch (type) {
    case kUndefined: return "undefined";
    case kNull: return "null";
    case kBoolean: return "boolean";
    case kInteger:
    case kFloat: return "number";
    case kString: return "string";
    default:
      if (AsCIH()) return static_cast<AptCIH*>(p)->IsText() ? "object" : "movieclip";
      if (AsFunction()) return "function";
      return "object";
  }
}

// ---- AptValue / AptGC -----------------------------------------------------------------------------

AptValue::AptValue(Kind kind) : m_kind(kind) { AptGC::Register(this); }
AptValue::~AptValue() = default;

void AptGC::Register(AptValue* v) {
  v->m_gcNext = s_gcHead;
  s_gcHead = v;
  ++s_allocatedSinceCollect;
  ++s_liveCount;
}

void AptGC::Mark(AptValue* v) {
  if (!v || v->m_marked) return;
  v->m_marked = true;
  v->RegisterReferences();
}

void AptGC::AddRoot(AptValue* v) { s_roots.push_back(v); }
void AptGC::ForEachLive(void (*fn)(AptValue* v, void* user), void* user) {
  for (AptValue* v = s_gcHead; v; v = v->m_gcNext) fn(v, user);
}
void AptGC::RemoveRoot(AptValue* v) {
  auto it = std::find(s_roots.begin(), s_roots.end(), v);
  if (it != s_roots.end()) s_roots.erase(it);
}

// AptGC::CleanUnreachable: marks from the global object, the pool (levels, queued actions, timers),
// the running interpreter and explicit roots, then deletes everything unmarked. Only called between
// ticks, when no native code holds unrooted values.
void AptGC::MarkRoots() {
  Mark(gpGlobalGlobalObject);
  if (gpPool) gpPool->RegisterReferences();
  for (const AptVar& v : gAptActionInterpreter.m_stack) Mark(v);
  for (AptObject* o : gAptActionInterpreter.m_tempRoots) Mark(o);
  for (AptValue* v : s_roots) Mark(v);
}

// PC: `fn` for every value reachable from the roots now (marks cleared again; nothing is freed).
void AptGC::ForEachReachable(void (*fn)(AptValue* v, void* user), void* user) {
  MarkRoots();
  for (AptValue* v = s_gcHead; v; v = v->m_gcNext)
    if (v->m_marked) {
      v->m_marked = false;
      fn(v, user);
    }
}

void AptGC::CleanUnreachable() {
  MarkRoots();
  AptValue** link = &s_gcHead;
  while (AptValue* v = *link) {
    if (v->m_marked) {
      v->m_marked = false;
      link = &v->m_gcNext;
    } else {
      *link = v->m_gcNext;
      --s_liveCount;
      delete v;
    }
  }
  s_allocatedSinceCollect = 0;
  // PC: the movies nothing alive uses any more are freed (GC: AptFile reference counts).
  AptReleaseUnusedFiles();
}

// ---- AptObject ------------------------------------------------------------------------------------

AptObject::AptObject(Kind kind) : AptValue(kind) {}

void AptObject::RegisterReferences() const {
  for (const auto& p : m_props) {
    AptGC::Mark(p.second.value);
    AptGC::Mark(p.second.getter);
    AptGC::Mark(p.second.setter);
  }
  AptGC::Mark(m_proto);
  for (AptObject* o : m_interfaces) AptGC::Mark(o);
}

AptProperty* AptObject::Find(const std::string& name) {
  auto it = m_index.find(name);
  return it == m_index.end() ? nullptr : &m_props[it->second].second;
}

void AptObject::Put(const std::string& name, const AptVar& value, unsigned int flags) {
  if (AptProperty* p = Find(name)) {
    p->value = value;
    return;
  }
  m_index[name] = m_props.size();
  AptProperty prop;
  prop.value = value;
  prop.flags = flags;
  m_props.emplace_back(name, prop);
}

bool AptObject::GetOwn(AptActionInterpreter& in, const std::string& name, AptVar& out) {
  if (name == "__proto__") {
    out = m_proto ? AptVar::Obj(m_proto) : AptVar::Undefined();
    return true;
  }
  AptProperty* p = Find(name);
  if (!p) return false;
  if (!p->getter.IsUndefined()) {
    out = in.callFunction(p->getter, AptVar::Obj(this), {});
    return true;
  }
  out = p->value;
  return true;
}

bool AptObject::SetOwn(AptActionInterpreter& in, const std::string& name, const AptVar& value) {
  if (name == "__proto__") {
    m_proto = value.AsObject();
    return true;
  }
  AptProperty* p = Find(name);
  if (p) {
    if (!p->setter.IsUndefined()) {
      in.callFunction(p->setter, AptVar::Obj(this), {value});
      return true;
    }
    if (p->flags & 4) return true;  // read-only
    p->value = value;
    return true;
  }
  // A setter inherited through the prototype chain (addProperty on a class prototype).
  for (AptObject* o = m_proto; o; o = o->m_proto) {
    if (AptProperty* q = o->Find(name)) {
      if (!q->setter.IsUndefined()) {
        in.callFunction(q->setter, AptVar::Obj(this), {value});
        return true;
      }
      break;
    }
  }
  Put(name, value);
  return true;
}

bool AptObject::Delete(const std::string& name) {
  auto it = m_index.find(name);
  if (it == m_index.end()) return false;
  if (m_props[it->second].second.flags & 2) return false;
  m_props.erase(m_props.begin() + static_cast<long>(it->second));
  m_index.clear();
  for (size_t i = 0; i < m_props.size(); ++i) m_index[m_props[i].first] = i;
  return true;
}

void AptObject::Enumerate(std::vector<std::string>& names) {
  for (const auto& p : m_props)
    if (!(p.second.flags & 1)) names.push_back(p.first);
}

AptVar AptObject::Get(AptActionInterpreter& in, const std::string& name) {
  AptVar out;
  int guard = 0;
  for (AptObject* o = this; o && guard < 64; o = o->m_proto, ++guard) {
    if (o == this ? o->GetOwn(in, name, out) : [&] {
          // Inherited plain properties and getters run with `this` as the receiver.
          AptProperty* p = o->Find(name);
          if (!p) return false;
          out = p->getter.IsUndefined() ? p->value : in.callFunction(p->getter, AptVar::Obj(this), {});
          return true;
        }())
      return out;
  }
  return AptVar::Undefined();
}

bool AptObject::Has(AptActionInterpreter& in, const std::string& name) {
  AptVar out;
  if (GetOwn(in, name, out)) return true;
  for (AptObject* o = m_proto; o; o = o->m_proto)
    if (o->Find(name)) return true;
  return false;
}

void AptObject::Set(AptActionInterpreter& in, const std::string& name, const AptVar& value) { SetOwn(in, name, value); }

// ---- AptArray -------------------------------------------------------------------------------------

namespace {
bool ArrayIndex(const std::string& name, size_t& index) {
  if (name.empty() || name.size() > 9) return false;
  for (char c : name)
    if (c < '0' || c > '9') return false;
  index = static_cast<size_t>(std::atoi(name.c_str()));
  return true;
}
}  // namespace

void AptArray::RegisterReferences() const {
  AptObject::RegisterReferences();
  for (const AptVar& v : m_items) AptGC::Mark(v);
}

bool AptArray::GetOwn(AptActionInterpreter& in, const std::string& name, AptVar& out) {
  size_t index;
  if (ArrayIndex(name, index)) {
    out = index < m_items.size() ? m_items[index] : AptVar::Undefined();
    return true;
  }
  if (name == "length") {
    out = AptVar::Int(static_cast<int>(m_items.size()));
    return true;
  }
  return AptObject::GetOwn(in, name, out);
}

bool AptArray::SetOwn(AptActionInterpreter& in, const std::string& name, const AptVar& value) {
  size_t index;
  if (ArrayIndex(name, index)) {
    if (index >= m_items.size()) m_items.resize(index + 1);
    m_items[index] = value;
    return true;
  }
  if (name == "length") {
    m_items.resize(static_cast<size_t>(std::max(0, value.ToInteger())));
    return true;
  }
  return AptObject::SetOwn(in, name, value);
}

void AptArray::Enumerate(std::vector<std::string>& names) {
  for (size_t i = 0; i < m_items.size(); ++i) names.push_back(std::to_string(i));
  AptObject::Enumerate(names);
}

// AptArray::toString: the items joined with ",".
std::string AptArray::DefaultString() {
  std::string s;
  for (size_t i = 0; i < m_items.size(); ++i) {
    if (i) s += ',';
    s += m_items[i].ToString();
  }
  return s;
}

// ---- Functions, frame stacks, extern ----------------------------------------------------------------

AptVar AptNativeFunction::Call(AptActionInterpreter& in, const AptVar& thisv, const std::vector<AptVar>& args,
                               AptObject*) {
  return m_fn(in, thisv, args);
}

void AptFrameStack::RegisterReferences() const {
  AptObject::RegisterReferences();
  AptGC::Mark(m_outer);
}

void AptScriptFunctionBase::RegisterReferences() const {
  AptObject::RegisterReferences();
  for (const AptVar& v : m_constantPool) AptGC::Mark(v);
  AptGC::Mark(m_target);
  AptGC::Mark(m_scope);
  for (AptObject* o : m_withScope) AptGC::Mark(o);
}

AptVar AptScriptFunctionBase::Call(AptActionInterpreter& in, const AptVar& thisv, const std::vector<AptVar>& args,
                                   AptObject* super) {
  return in.runFunction(this, thisv, args, super);
}

// GetMember on extern asks the game (getExternVariable); SetMember tells it (setExternVariable,
// AptExtern::objectMemberSet 802B44B4).
bool AptExtern::GetOwn(AptActionInterpreter&, const std::string& name, AptVar& out) {
  out = gAptFuncs.getExternVariable ? gAptFuncs.getExternVariable(const_cast<char*>(name.c_str()))
                                    : AptVar::Undefined();
  return true;
}

// 802B44B4
bool AptExtern::SetOwn(AptActionInterpreter&, const std::string& name, const AptVar& value) {
  if (gAptFuncs.setExternVariable) {
    const std::string s = value.ToString();
    gAptFuncs.setExternVariable(const_cast<char*>(name.c_str()), const_cast<char*>(s.c_str()));
  }
  return true;
}
