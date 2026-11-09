// Port of Apt's built-in objects: _constructBuiltInObjects (AptValue.cpp 802B10A4) with Object,
// Function, Array (AptArray.cpp), String (AptString.cpp), Math, Key, Stage (AptMiscObjects.cpp),
// Color (AptScriptColour.cpp), TextFormat (AptTextFormat.cpp), XML (AptXml*.cpp), the global
// functions, and the MovieClip / TextField members (AptCharacterInst::objectMemberLookup/Set and
// the sMethod_* natives of AptCharacter.cpp).
#include "simmy/types_gen.hpp"
#include "simmy/detmath.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
using Args = std::vector<AptVar>;
constexpr int kAptScriptDepthBase = 0x4000;  // see createEmptyMovieClip
AptObject* s_objectProto = nullptr;
AptObject* s_functionProto = nullptr;
AptObject* s_arrayProto = nullptr;
AptObject* s_registered = nullptr;  // Object.registerClass table

AptVar Arg(const Args& a, size_t i) { return i < a.size() ? a[i] : AptVar::Undefined(); }

AptNativeFunction* Native(AptNativeFn fn, const char* name) {
  auto* f = new AptNativeFunction(fn, name);
  f->m_proto = s_functionProto;
  return f;
}
void Method(AptObject* o, const char* name, AptNativeFn fn) { o->Put(name, AptVar::Obj(Native(fn, name)), 1); }

// A constructor with its prototype, published on _global.
AptNativeFunction* Class(const char* name, AptNativeFn ctor, AptObject* proto) {
  AptNativeFunction* f = Native(ctor, name);
  f->Put("prototype", AptVar::Obj(proto), 1);
  proto->Put("constructor", AptVar::Obj(f), 1);
  gpGlobalGlobalObject->Put(name, AptVar::Obj(f), 1);
  return f;
}

AptObject* NewObject(AptObject* proto = nullptr) {
  auto* o = new AptObject;
  o->m_proto = proto ? proto : s_objectProto;
  return o;
}
AptArray* NewArray() {
  auto* a = new AptArray;
  a->m_proto = s_arrayProto;
  return a;
}

AptCIH* ThisClip(AptActionInterpreter& in, const AptVar& thisv) {
  if (AptCIH* c = thisv.AsCIH()) return c;
  return in.m_ctx ? in.m_ctx->target : nullptr;
}
std::string ThisString(const AptVar& thisv) {
  if (AptObject* o = thisv.AsObject())
    if (AptProperty* p = o->Find("__value__")) return p->value.ToString();
  return thisv.ToString();
}

// ---- Object / Function ------------------------------------------------------------------------------

AptVar Object_ctor(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  if (Arg(a, 0).AsObject()) return a[0];
  return AptVar::Obj(NewObject());
}
// 802B0E4C _gObjRegistrationFunc: Object.registerClass(name, constructor)
AptVar Object_registerClass(AptActionInterpreter&, const AptVar&, const Args& a) {
  const std::string name = Arg(a, 0).ToString();
  if (AptObject* ctor = Arg(a, 1).AsObject()) s_registered->Put(name, AptVar::Obj(ctor));
  else s_registered->Delete(name);
  return AptVar::Bool(true);
}
AptVar Object_addProperty(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptObject* o = thisv.AsObject();
  if (!o || !Arg(a, 1).AsFunction()) return AptVar::Bool(false);
  const std::string name = Arg(a, 0).ToString();
  o->Put(name, AptVar::Undefined());
  AptProperty* p = o->Find(name);
  p->getter = a[1];
  p->setter = Arg(a, 2).AsFunction() ? a[2] : AptVar::Undefined();
  return AptVar::Bool(true);
}
AptVar Object_hasOwnProperty(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptObject* o = thisv.AsObject();
  return AptVar::Bool(o && o->Find(Arg(a, 0).ToString()));
}
AptVar Object_toString(AptActionInterpreter&, const AptVar& thisv, const Args&) {
  return AptVar::Str(thisv.AsObject() ? thisv.AsObject()->DefaultString() : thisv.ToString());
}
AptVar Object_valueOf(AptActionInterpreter&, const AptVar& thisv, const Args&) { return thisv; }

AptVar Function_ctor(AptActionInterpreter&, const AptVar&, const Args&) { return AptVar::Undefined(); }
AptVar Function_call(AptActionInterpreter& in, const AptVar& thisv, const Args& a) {
  Args rest(a.size() > 1 ? a.begin() + 1 : a.end(), a.end());
  return in.callFunction(thisv, Arg(a, 0), rest);
}
AptVar Function_apply(AptActionInterpreter& in, const AptVar& thisv, const Args& a) {
  Args rest;
  if (AptObject* o = Arg(a, 1).AsObject())
    if (o->m_kind == AptValue::kArray) rest = static_cast<AptArray*>(o)->m_items;
  return in.callFunction(thisv, Arg(a, 0), rest);
}

// 802B1xxx ASSetPropFlags(object, names | null, set, clear)
AptVar ASSetPropFlags(AptActionInterpreter&, const AptVar&, const Args& a) {
  AptObject* o = Arg(a, 0).AsObject();
  if (!o) return AptVar::Undefined();
  std::vector<std::string> names;
  if (Arg(a, 1).IsNullish()) {
    for (auto& p : o->m_props) names.push_back(p.first);
  } else if (AptObject* arr = a[1].AsObject(); arr && arr->m_kind == AptValue::kArray) {
    for (const AptVar& v : static_cast<AptArray*>(arr)->m_items) names.push_back(v.ToString());
  } else {
    std::string s = a[1].ToString();
    size_t i = 0;
    while (i <= s.size()) {
      size_t j = s.find(',', i);
      if (j == std::string::npos) j = s.size();
      names.push_back(s.substr(i, j - i));
      i = j + 1;
    }
  }
  const unsigned int set = static_cast<unsigned int>(Arg(a, 2).ToInteger());
  const unsigned int clear = static_cast<unsigned int>(Arg(a, 3).ToInteger());
  for (const std::string& n : names)
    if (AptProperty* p = o->Find(n)) p->flags = (p->flags & ~clear) | set;
  return AptVar::Undefined();
}

// ---- Array (AptArray.cpp) --------------------------------------------------------------------------

AptArray* ThisArray(const AptVar& thisv) {
  AptObject* o = thisv.AsObject();
  return o && o->m_kind == AptValue::kArray ? static_cast<AptArray*>(o) : nullptr;
}
AptVar Array_ctor(AptActionInterpreter&, const AptVar&, const Args& a) {
  AptArray* arr = NewArray();
  if (a.size() == 1 && a[0].IsNumber()) arr->m_items.resize(static_cast<size_t>(std::max(0, a[0].ToInteger())));
  else arr->m_items = a;
  return AptVar::Obj(arr);
}
AptVar Array_push(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptArray* arr = ThisArray(thisv);
  if (!arr) return AptVar::Undefined();
  for (const AptVar& v : a) arr->m_items.push_back(v);
  return AptVar::Int(static_cast<int>(arr->m_items.size()));
}
AptVar Array_pop(AptActionInterpreter&, const AptVar& thisv, const Args&) {
  AptArray* arr = ThisArray(thisv);
  if (!arr || arr->m_items.empty()) return AptVar::Undefined();
  AptVar v = arr->m_items.back();
  arr->m_items.pop_back();
  return v;
}
AptVar Array_shift(AptActionInterpreter&, const AptVar& thisv, const Args&) {
  AptArray* arr = ThisArray(thisv);
  if (!arr || arr->m_items.empty()) return AptVar::Undefined();
  AptVar v = arr->m_items.front();
  arr->m_items.erase(arr->m_items.begin());
  return v;
}
AptVar Array_unshift(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptArray* arr = ThisArray(thisv);
  if (!arr) return AptVar::Undefined();
  arr->m_items.insert(arr->m_items.begin(), a.begin(), a.end());
  return AptVar::Int(static_cast<int>(arr->m_items.size()));
}
AptVar Array_splice(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptArray* arr = ThisArray(thisv);
  AptArray* removed = NewArray();
  if (!arr) return AptVar::Obj(removed);
  const int size = static_cast<int>(arr->m_items.size());
  int start = Arg(a, 0).ToInteger();
  if (start < 0) start = std::max(0, size + start);
  start = std::min(start, size);
  int count = a.size() > 1 ? Arg(a, 1).ToInteger() : size - start;
  count = std::max(0, std::min(count, size - start));
  removed->m_items.assign(arr->m_items.begin() + start, arr->m_items.begin() + start + count);
  arr->m_items.erase(arr->m_items.begin() + start, arr->m_items.begin() + start + count);
  if (a.size() > 2) arr->m_items.insert(arr->m_items.begin() + start, a.begin() + 2, a.end());
  return AptVar::Obj(removed);
}
AptVar Array_slice(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptArray* arr = ThisArray(thisv);
  AptArray* out = NewArray();
  if (!arr) return AptVar::Obj(out);
  const int size = static_cast<int>(arr->m_items.size());
  int s = a.empty() ? 0 : Arg(a, 0).ToInteger(), e = a.size() > 1 ? Arg(a, 1).ToInteger() : size;
  if (s < 0) s = std::max(0, size + s);
  if (e < 0) e = std::max(0, size + e);
  s = std::min(s, size);
  e = std::min(e, size);
  if (s < e) out->m_items.assign(arr->m_items.begin() + s, arr->m_items.begin() + e);
  return AptVar::Obj(out);
}
AptVar Array_join(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptArray* arr = ThisArray(thisv);
  if (!arr) return AptVar::Str("");
  const std::string sep = a.empty() ? "," : a[0].ToString();
  std::string s;
  for (size_t i = 0; i < arr->m_items.size(); ++i) {
    if (i) s += sep;
    s += arr->m_items[i].ToString();
  }
  return AptVar::Str(s);
}
AptVar Array_concat(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptArray* arr = ThisArray(thisv);
  AptArray* out = NewArray();
  if (arr) out->m_items = arr->m_items;
  for (const AptVar& v : a) {
    if (AptArray* other = ThisArray(v)) out->m_items.insert(out->m_items.end(), other->m_items.begin(), other->m_items.end());
    else out->m_items.push_back(v);
  }
  return AptVar::Obj(out);
}
AptVar Array_reverse(AptActionInterpreter&, const AptVar& thisv, const Args&) {
  if (AptArray* arr = ThisArray(thisv)) std::reverse(arr->m_items.begin(), arr->m_items.end());
  return thisv;
}
AptVar Array_sort(AptActionInterpreter& in, const AptVar& thisv, const Args& a) {
  AptArray* arr = ThisArray(thisv);
  if (!arr) return thisv;
  if (Arg(a, 0).AsFunction()) {
    std::stable_sort(arr->m_items.begin(), arr->m_items.end(), [&](const AptVar& x, const AptVar& y) {
      return in.callFunction(a[0], AptVar::Undefined(), {x, y}).ToInteger() < 0;
    });
  } else {
    std::stable_sort(arr->m_items.begin(), arr->m_items.end(),
                     [](const AptVar& x, const AptVar& y) { return x.ToString() < y.ToString(); });
  }
  return thisv;
}
AptVar Array_toString(AptActionInterpreter& in, const AptVar& thisv, const Args&) {
  return Array_join(in, thisv, {});
}

// ---- String (AptString.cpp) -----------------------------------------------------------------------

AptVar String_ctor(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = a.empty() ? "" : a[0].ToString();
  if (thisv.IsUndefined()) {  // new String(s)
    AptObject* o = NewObject();
    o->Put("__value__", AptVar::Str(s), 1);
    return AptVar::Obj(o);
  }
  return AptVar::Str(s);
}
AptVar String_charAt(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  const int i = Arg(a, 0).ToInteger();
  return AptVar::Str(i >= 0 && i < static_cast<int>(s.size()) ? s.substr(static_cast<size_t>(i), 1) : "");
}
AptVar String_charCodeAt(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  const int i = Arg(a, 0).ToInteger();
  return AptVar::Int(i >= 0 && i < static_cast<int>(s.size()) ? static_cast<unsigned char>(s[static_cast<size_t>(i)]) : 0);
}
AptVar String_indexOf(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  const size_t from = static_cast<size_t>(std::max(0, Arg(a, 1).ToInteger()));
  const size_t pos = s.find(Arg(a, 0).ToString(), from);
  return AptVar::Int(pos == std::string::npos ? -1 : static_cast<int>(pos));
}
AptVar String_lastIndexOf(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  const size_t pos = s.rfind(Arg(a, 0).ToString(), a.size() > 1 ? static_cast<size_t>(std::max(0, a[1].ToInteger())) : std::string::npos);
  return AptVar::Int(pos == std::string::npos ? -1 : static_cast<int>(pos));
}
AptVar String_substr(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  const int size = static_cast<int>(s.size());
  int start = Arg(a, 0).ToInteger();
  if (start < 0) start = std::max(0, size + start);
  if (start >= size) return AptVar::Str("");
  int len = a.size() > 1 ? Arg(a, 1).ToInteger() : size - start;
  len = std::max(0, std::min(len, size - start));
  return AptVar::Str(s.substr(static_cast<size_t>(start), static_cast<size_t>(len)));
}
AptVar String_substring(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  const int size = static_cast<int>(s.size());
  int b = std::max(0, std::min(Arg(a, 0).ToInteger(), size));
  int e = a.size() > 1 ? std::max(0, std::min(a[1].ToInteger(), size)) : size;
  if (b > e) std::swap(b, e);
  return AptVar::Str(s.substr(static_cast<size_t>(b), static_cast<size_t>(e - b)));
}
AptVar String_slice(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  const int size = static_cast<int>(s.size());
  int b = Arg(a, 0).ToInteger(), e = a.size() > 1 ? a[1].ToInteger() : size;
  if (b < 0) b = std::max(0, size + b);
  if (e < 0) e = std::max(0, size + e);
  b = std::min(b, size);
  e = std::min(e, size);
  return AptVar::Str(b < e ? s.substr(static_cast<size_t>(b), static_cast<size_t>(e - b)) : "");
}
AptVar String_split(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  const std::string s = ThisString(thisv);
  AptArray* out = NewArray();
  if (a.empty() || a[0].IsUndefined()) {
    out->m_items.push_back(AptVar::Str(s));
    return AptVar::Obj(out);
  }
  const std::string sep = a[0].ToString();
  if (sep.empty()) {
    for (char ch : s) out->m_items.push_back(AptVar::Str(std::string(1, ch)));
    return AptVar::Obj(out);
  }
  size_t i = 0;
  while (true) {
    const size_t j = s.find(sep, i);
    out->m_items.push_back(AptVar::Str(s.substr(i, j == std::string::npos ? std::string::npos : j - i)));
    if (j == std::string::npos) break;
    i = j + sep.size();
  }
  return AptVar::Obj(out);
}
AptVar String_toLowerCase(AptActionInterpreter&, const AptVar& thisv, const Args&) {
  std::string s = ThisString(thisv);
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return AptVar::Str(s);
}
AptVar String_toUpperCase(AptActionInterpreter&, const AptVar& thisv, const Args&) {
  std::string s = ThisString(thisv);
  for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return AptVar::Str(s);
}
AptVar String_toString(AptActionInterpreter&, const AptVar& thisv, const Args&) { return AptVar::Str(ThisString(thisv)); }
AptVar String_concat(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  std::string s = ThisString(thisv);
  for (const AptVar& v : a) s += v.ToString();
  return AptVar::Str(s);
}
AptVar String_fromCharCode(AptActionInterpreter&, const AptVar&, const Args& a) {
  std::string s;
  for (const AptVar& v : a) s += static_cast<char>(v.ToInteger());
  return AptVar::Str(s);
}

// ---- Number, Boolean and global functions ------------------------------------------------------------

AptVar Number_ctor(AptActionInterpreter&, const AptVar&, const Args& a) {
  return a.empty() ? AptVar::Int(0) : (a[0].IsNumber() ? a[0] : AptVar::Number(a[0].ToFloat()));
}
AptVar Number_toString(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  if (!a.empty() && a[0].ToInteger() == 16) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%x", static_cast<unsigned int>(thisv.ToInteger()));
    return AptVar::Str(buf);
  }
  return AptVar::Str(thisv.ToString());
}
// 8026EE5C cbCallMethod_boolean: no argument is undefined.
AptVar Boolean_ctor(AptActionInterpreter&, const AptVar&, const Args& a) {
  return a.empty() ? AptVar::Undefined() : AptVar::Bool(a[0].ToBool());
}
AptVar parseIntFn(AptActionInterpreter&, const AptVar&, const Args& a) {
  const std::string s = Arg(a, 0).ToString();
  const int radix = a.size() > 1 ? a[1].ToInteger() : 0;
  return AptVar::Int(static_cast<int>(std::strtol(s.c_str(), nullptr, radix)));
}
AptVar parseFloatFn(AptActionInterpreter&, const AptVar&, const Args& a) {
  return AptVar::Number(std::atof(Arg(a, 0).ToString().c_str()));
}
// 8026E69C cbCallMethod_isNaN
// No argument is NaN; numbers never are (Apt does not test the float); undefined is not in SWF 6.
AptVar isNaNFn(AptActionInterpreter&, const AptVar&, const Args& a) {
  if (a.empty()) return AptVar::Bool(true);
  return AptVar::Bool(a[0].IsNaNLike());
}
// 8026ECE8 / 8026EB48 escape / unescape
AptVar escapeFn(AptActionInterpreter&, const AptVar&, const Args& a) {
  std::string out;
  for (unsigned char ch : Arg(a, 0).ToString()) {
    if (std::isalnum(ch)) out += static_cast<char>(ch);
    else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", ch);
      out += buf;
    }
  }
  return AptVar::Str(out);
}
AptVar unescapeFn(AptActionInterpreter&, const AptVar&, const Args& a) {
  const std::string s = Arg(a, 0).ToString();
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
      i += 2;
    } else {
      out += s[i];
    }
  }
  return AptVar::Str(out);
}
// 8026E048 cbCallMethod_setInterval: (function, ms, args...) or (object, "method", ms, args...)
AptVar setIntervalFn(AptActionInterpreter&, const AptVar&, const Args& a) {
  AptIntervalTimer t;
  size_t next;
  if (Arg(a, 0).AsFunction()) {
    t.function = a[0];
    t.interval = Arg(a, 1).ToInteger();
    next = 2;
  } else {
    t.target = Arg(a, 0);
    t.function = Arg(a, 1);
    t.interval = Arg(a, 2).ToInteger();
    next = 3;
  }
  for (size_t i = next; i < a.size(); ++i) t.args.push_back(a[i]);
  t.id = gpPool->m_nextTimerId++;
  gpPool->m_timers.push_back(t);
  return AptVar::Int(t.id);
}
// 8026E3E4 cbCallMethod_clearInterval
AptVar clearIntervalFn(AptActionInterpreter&, const AptVar&, const Args& a) {
  const int id = Arg(a, 0).ToInteger();
  for (AptIntervalTimer& t : gpPool->m_timers)
    if (t.id == id) t.dead = true;
  return AptVar::Undefined();
}

// ---- Math (AptMathObj) -------------------------------------------------------------------------------

#define MATH1(name, expr)                                                            \
  AptVar Math_##name(AptActionInterpreter&, const AptVar&, const Args& a) {          \
    const double x = Arg(a, 0).ToFloat();                                           \
    return AptVar::Number(expr);                                                     \
  }
MATH1(abs, std::fabs(x))
MATH1(ceil, std::ceil(x))
MATH1(floor, std::floor(x))
MATH1(round, std::floor(x + 0.5))
MATH1(sqrt, x >= 0 ? std::sqrt(x) : 0.0)
MATH1(sin, simmy::dm::sin(static_cast<float>(x)))
MATH1(cos, simmy::dm::cos(static_cast<float>(x)))
MATH1(tan, simmy::dm::tan(static_cast<float>(x)))
MATH1(asin, simmy::dm::asin(static_cast<float>(x)))
MATH1(acos, simmy::dm::acos(static_cast<float>(x)))
MATH1(atan, simmy::dm::atan(static_cast<float>(x)))
MATH1(exp, simmy::dm::exp(static_cast<float>(x)))
MATH1(log, x > 0 ? simmy::dm::log(static_cast<float>(x)) : 0.0)
#undef MATH1
AptVar Math_atan2(AptActionInterpreter&, const AptVar&, const Args& a) {
  return AptVar::Number(simmy::dm::atan2(Arg(a, 0).ToFloat(), Arg(a, 1).ToFloat()));
}
AptVar Math_pow(AptActionInterpreter&, const AptVar&, const Args& a) {
  return AptVar::Number(simmy::dm::pow(Arg(a, 0).ToFloat(), Arg(a, 1).ToFloat()));
}
AptVar Math_min(AptActionInterpreter&, const AptVar&, const Args& a) {
  const AptVar x = Arg(a, 0), y = Arg(a, 1);
  return x.ToFloat() <= y.ToFloat() ? (x.IsNumber() ? x : AptVar::Number(x.ToFloat())) : (y.IsNumber() ? y : AptVar::Number(y.ToFloat()));
}
AptVar Math_max(AptActionInterpreter&, const AptVar&, const Args& a) {
  const AptVar x = Arg(a, 0), y = Arg(a, 1);
  return x.ToFloat() >= y.ToFloat() ? (x.IsNumber() ? x : AptVar::Number(x.ToFloat())) : (y.IsNumber() ? y : AptVar::Number(y.ToFloat()));
}
AptVar Math_random(AptActionInterpreter&, const AptVar&, const Args&) {
  return AptVar::Float(static_cast<float>(simmy::ui_rand()) / 2147483648.0f);
}

// ---- Key (AptKey) ------------------------------------------------------------------------------------

AptVar Key_addListener(AptActionInterpreter&, const AptVar&, const Args& a) {
  if (AptObject* o = Arg(a, 0).AsObject()) {
    auto* list = static_cast<AptArray*>(gpPool->m_keyListeners);
    if (std::none_of(list->m_items.begin(), list->m_items.end(), [o](const AptVar& v) { return v.AsObject() == o; }))
      list->m_items.push_back(a[0]);
  }
  return AptVar::Undefined();
}
AptVar Key_removeListener(AptActionInterpreter&, const AptVar&, const Args& a) {
  auto* list = static_cast<AptArray*>(gpPool->m_keyListeners);
  AptObject* o = Arg(a, 0).AsObject();
  list->m_items.erase(std::remove_if(list->m_items.begin(), list->m_items.end(),
                                     [o](const AptVar& v) { return v.AsObject() == o; }),
                      list->m_items.end());
  return AptVar::Bool(true);
}
// BUTTONACTIONCODE_TO_KEYCODE: the pad's navigation events as Flash key codes.
const int kButtonToKeyCode[20] = {-1, 37, 39, 36, 35, 45, 46, -1, 8, -1, -1, -1, -1, 13, 38, 40, 33, 34, 9, 27};
// 802A5B8C: lower-case letters come as upper case, navigation events through the table.
AptVar Key_getCode(AptActionInterpreter&, const AptVar&, const Args&) {
  unsigned int code = gpPool->m_curInput >> 17;
  if (code - 0x20 < 0x5f) {
    if (code >= 'a' && code <= 'z') code -= 0x20;
  } else if (code < 0x14) {
    return AptVar::Int(kButtonToKeyCode[code]);
  }
  return AptVar::Int(static_cast<int>(code));
}
// 802A5C8C
AptVar Key_getAscii(AptActionInterpreter&, const AptVar&, const Args&) {
  unsigned int code = gpPool->m_curInput >> 17;
  if ((gpPool->m_curInput >> 2 & 0xff) != 0 && code < 0x14) return AptVar::Int(kButtonToKeyCode[code]);
  return AptVar::Int(static_cast<int>(code));
}
// 802A5DA0: the controller the event came from (0 = player 1).
AptVar Key_getController(AptActionInterpreter&, const AptVar&, const Args&) {
  return AptVar::Int(static_cast<int>(gpPool->m_curInput >> 2 & 0xff) - 2);
}
AptVar Key_isDown(AptActionInterpreter&, const AptVar&, const Args&) { return AptVar::Bool(false); }

// ---- Color (AptScriptColour) ----------------------------------------------------------------------

AptCIH* ColourTarget(AptActionInterpreter& in, const AptVar& thisv) {
  AptObject* o = thisv.AsObject();
  if (!o) return nullptr;
  AptProperty* p = o->Find("__target__");
  if (!p) return nullptr;
  if (AptCIH* c = p->value.AsCIH()) return c;
  return in.getVariable(p->value.ToString()).AsCIH();
}
AptVar Color_ctor(AptActionInterpreter& in, const AptVar&, const Args& a) {
  AptObject* o = NewObject();
  o->m_kind = AptValue::kColour;
  o->Put("__target__", Arg(a, 0), 1);
  (void)in;
  return AptVar::Obj(o);
}
AptVar Color_setRGB(AptActionInterpreter& in, const AptVar& thisv, const Args& a) {
  if (AptCIH* c = ColourTarget(in, thisv)) {
    const unsigned int rgb = static_cast<unsigned int>(Arg(a, 0).ToInteger());
    c->m_cxform.mulR = c->m_cxform.mulG = c->m_cxform.mulB = 0;
    c->m_cxform.addR = static_cast<float>(rgb >> 16 & 0xff) / 255.0f;
    c->m_cxform.addG = static_cast<float>(rgb >> 8 & 0xff) / 255.0f;
    c->m_cxform.addB = static_cast<float>(rgb & 0xff) / 255.0f;
    c->m_scriptChanged = true;
  }
  return AptVar::Undefined();
}
AptVar Color_getRGB(AptActionInterpreter& in, const AptVar& thisv, const Args&) {
  AptCIH* c = ColourTarget(in, thisv);
  if (!c) return AptVar::Int(0);
  auto b = [](float f) { return static_cast<int>(std::max(0.0f, std::min(1.0f, f)) * 255.0f + 0.5f); };
  return AptVar::Int(b(c->m_cxform.addR) << 16 | b(c->m_cxform.addG) << 8 | b(c->m_cxform.addB));
}
AptVar Color_setTransform(AptActionInterpreter& in, const AptVar& thisv, const Args& a) {
  AptCIH* c = ColourTarget(in, thisv);
  AptObject* t = Arg(a, 0).AsObject();
  if (!c || !t) return AptVar::Undefined();
  auto get = [&](const char* n, float& f, float scale) {
    AptVar v = t->Get(in, n);
    if (!v.IsUndefined()) f = v.ToFloat() / scale;
  };
  get("ra", c->m_cxform.mulR, 100.0f);
  get("ga", c->m_cxform.mulG, 100.0f);
  get("ba", c->m_cxform.mulB, 100.0f);
  get("aa", c->m_cxform.mulA, 100.0f);
  get("rb", c->m_cxform.addR, 255.0f);
  get("gb", c->m_cxform.addG, 255.0f);
  get("bb", c->m_cxform.addB, 255.0f);
  get("ab", c->m_cxform.addA, 255.0f);
  c->m_scriptChanged = true;
  return AptVar::Undefined();
}
AptVar Color_getTransform(AptActionInterpreter& in, const AptVar& thisv, const Args&) {
  AptObject* o = NewObject();
  if (AptCIH* c = ColourTarget(in, thisv)) {
    o->Put("ra", AptVar::Number(c->m_cxform.mulR * 100.0));
    o->Put("ga", AptVar::Number(c->m_cxform.mulG * 100.0));
    o->Put("ba", AptVar::Number(c->m_cxform.mulB * 100.0));
    o->Put("aa", AptVar::Number(c->m_cxform.mulA * 100.0));
    o->Put("rb", AptVar::Number(c->m_cxform.addR * 255.0));
    o->Put("gb", AptVar::Number(c->m_cxform.addG * 255.0));
    o->Put("bb", AptVar::Number(c->m_cxform.addB * 255.0));
    o->Put("ab", AptVar::Number(c->m_cxform.addA * 255.0));
  }
  return AptVar::Obj(o);
}

// ---- TextFormat (AptTextFormat) -----------------------------------------------------------------------

AptVar TextFormat_ctor(AptActionInterpreter&, const AptVar&, const Args& a) {
  AptObject* o = NewObject();
  o->m_kind = AptValue::kTextFormat;
  static const char* const kFields[] = {"font", "size", "color", "bold", "italic", "underline", "url",
                                        "target", "align", "leftMargin", "rightMargin", "indent", "leading"};
  for (size_t i = 0; i < 13; ++i) o->Put(kFields[i], i < a.size() ? a[i] : AptVar::Null());
  return AptVar::Obj(o);
}

// ---- XML (AptXml, AptXmlNode) ------------------------------------------------------------------------

AptObject* s_xmlNodeProto = nullptr;
struct XmlParser {
  const std::string& s;
  size_t i = 0;
  AptObject* Node(int type, const std::string& name, const std::string& value) {
    AptObject* n = NewObject(s_xmlNodeProto);
    n->m_kind = AptValue::kXmlNode;
    n->Put("nodeType", AptVar::Int(type));
    n->Put("nodeName", type == 1 ? AptVar::Str(name) : AptVar::Null());
    n->Put("nodeValue", type == 3 ? AptVar::Str(value) : AptVar::Null());
    n->Put("attributes", AptVar::Obj(NewObject()));
    n->Put("childNodes", AptVar::Obj(NewArray()));
    n->Put("firstChild", AptVar::Null());
    n->Put("lastChild", AptVar::Null());
    n->Put("nextSibling", AptVar::Null());
    n->Put("previousSibling", AptVar::Null());
    n->Put("parentNode", AptVar::Null());
    return n;
  }
  void Append(AptObject* parent, AptObject* child) {
    auto* kids = static_cast<AptArray*>(parent->Find("childNodes")->value.AsObject());
    if (!kids->m_items.empty()) {
      AptObject* prev = kids->m_items.back().AsObject();
      prev->Put("nextSibling", AptVar::Obj(child));
      child->Put("previousSibling", AptVar::Obj(prev));
    } else {
      parent->Put("firstChild", AptVar::Obj(child));
    }
    kids->m_items.push_back(AptVar::Obj(child));
    parent->Put("lastChild", AptVar::Obj(child));
    child->Put("parentNode", AptVar::Obj(parent));
  }
  static std::string Unescape(const std::string& t) {
    std::string out;
    for (size_t k = 0; k < t.size(); ++k) {
      if (t[k] == '&') {
        const size_t e = t.find(';', k);
        if (e != std::string::npos) {
          const std::string ent = t.substr(k + 1, e - k - 1);
          const char* rep = ent == "lt" ? "<" : ent == "gt" ? ">" : ent == "amp" ? "&" : ent == "quot" ? "\"" : ent == "apos" ? "'" : nullptr;
          if (rep) {
            out += rep;
            k = e;
            continue;
          }
        }
      }
      out += t[k];
    }
    return out;
  }
  void Parse(AptObject* root) {
    std::vector<AptObject*> stack{root};
    while (i < s.size()) {
      if (s[i] == '<') {
        if (s.compare(i, 4, "<!--") == 0) {
          const size_t e = s.find("-->", i);
          i = e == std::string::npos ? s.size() : e + 3;
          continue;
        }
        if (s[i + 1] == '?' || s[i + 1] == '!') {
          const size_t e = s.find('>', i);
          i = e == std::string::npos ? s.size() : e + 1;
          continue;
        }
        if (s[i + 1] == '/') {
          const size_t e = s.find('>', i);
          i = e == std::string::npos ? s.size() : e + 1;
          if (stack.size() > 1) stack.pop_back();
          continue;
        }
        size_t j = i + 1;
        while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j])) && s[j] != '>' && s[j] != '/') ++j;
        AptObject* el = Node(1, s.substr(i + 1, j - i - 1), "");
        AptObject* attrs = el->Find("attributes")->value.AsObject();
        i = j;
        bool selfClose = false;
        while (i < s.size() && s[i] != '>') {
          if (s[i] == '/') {
            selfClose = true;
            ++i;
            continue;
          }
          if (std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
            continue;
          }
          size_t eq = s.find('=', i);
          if (eq == std::string::npos) break;
          std::string an = s.substr(i, eq - i);
          while (!an.empty() && std::isspace(static_cast<unsigned char>(an.back()))) an.pop_back();
          size_t q = s.find_first_of("\"'", eq);
          if (q == std::string::npos) break;
          const size_t qe = s.find(s[q], q + 1);
          attrs->Put(an, AptVar::Str(Unescape(s.substr(q + 1, qe - q - 1))));
          i = qe + 1;
        }
        ++i;
        Append(stack.back(), el);
        if (!selfClose) stack.push_back(el);
      } else {
        const size_t e = s.find('<', i);
        const std::string text = s.substr(i, e == std::string::npos ? std::string::npos : e - i);
        i = e == std::string::npos ? s.size() : e;
        bool blank = std::all_of(text.begin(), text.end(), [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
        if (!blank) Append(stack.back(), Node(3, "", Unescape(text)));
      }
    }
  }
};
AptVar XML_ctor(AptActionInterpreter&, const AptVar&, const Args& a) {
  std::string src = a.empty() ? std::string() : a[0].ToString();
  XmlParser parser{src};
  AptObject* doc = parser.Node(1, "", "");
  doc->Put("nodeName", AptVar::Null());
  if (!src.empty()) parser.Parse(doc);
  return AptVar::Obj(doc);
}
AptVar XML_parseXML(AptActionInterpreter&, const AptVar& thisv, const Args& a) {
  AptObject* doc = thisv.AsObject();
  if (!doc) return AptVar::Undefined();
  std::string src = Arg(a, 0).ToString();
  XmlParser parser{src};
  static_cast<AptArray*>(doc->Find("childNodes")->value.AsObject())->m_items.clear();
  parser.Parse(doc);
  return AptVar::Undefined();
}
AptVar XMLNode_toString(AptActionInterpreter&, const AptVar& thisv, const Args&) {
  AptObject* n = thisv.AsObject();
  if (!n) return AptVar::Str("");
  AptProperty* v = n->Find("nodeValue");
  return AptVar::Str(v ? v->value.ToString() : "");
}

// ---- MovieClip (AptCharacterInst sMethod_*) -------------------------------------------------------------

// 80288B98 _gotoAndX: labels or 1-based frame numbers.
AptVar GotoAndX(AptActionInterpreter& in, const AptVar& thisv, const Args& a, bool play) {
  AptCIH* c = ThisClip(in, thisv);
  if (!c || a.empty()) return AptVar::Undefined();
  int frame;
  if (a[0].IsString()) {
    frame = c->labelToFrame(a[0].AsString()->m_text) + 1;
    if (frame == 0 && !a[0].AsString()->m_text.empty() && std::isdigit(static_cast<unsigned char>(a[0].AsString()->m_text[0])))
      frame = a[0].ToInteger();
  } else {
    frame = a[0].ToInteger();
  }
  if (frame - 1 >= 0) {
    c->jumpToFrame(frame - 1);
    c->m_playing = play;
  }
  return AptVar::Undefined();
}
// 802893BC / 80289398
AptVar MC_gotoAndPlay(AptActionInterpreter& in, const AptVar& t, const Args& a) { return GotoAndX(in, t, a, true); }
AptVar MC_gotoAndStop(AptActionInterpreter& in, const AptVar& t, const Args& a) { return GotoAndX(in, t, a, false); }
// 8028AF08 / 8028AE68 / 8028AFA8 / 8028AFF4
AptVar MC_play(AptActionInterpreter& in, const AptVar& t, const Args&) {
  if (AptCIH* c = ThisClip(in, t)) c->m_playing = true;
  return AptVar::Undefined();
}
AptVar MC_stop(AptActionInterpreter& in, const AptVar& t, const Args&) {
  if (AptCIH* c = ThisClip(in, t)) c->m_playing = false;
  return AptVar::Undefined();
}
AptVar MC_nextFrame(AptActionInterpreter& in, const AptVar& t, const Args&) {
  if (AptCIH* c = ThisClip(in, t)) c->jumpToFrame(c->m_frame + 1), c->m_playing = false;
  return AptVar::Undefined();
}
AptVar MC_prevFrame(AptActionInterpreter& in, const AptVar& t, const Args&) {
  if (AptCIH* c = ThisClip(in, t)) c->jumpToFrame(c->m_frame - 1), c->m_playing = false;
  return AptVar::Undefined();
}
void CopyInitObject(AptActionInterpreter& in, AptCIH* c, const AptVar& init) {
  if (AptObject* o = init.AsObject()) {
    std::vector<std::string> names;
    o->Enumerate(names);
    for (const std::string& n : names) c->SetOwn(in, n, o->Get(in, n));
  }
}
// 802893E0 attachMovie(exportName, newName, depth[, initObject])
AptVar MC_attachMovie(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  if (!c) return AptVar::Undefined();
  const std::string id = Arg(a, 0).ToString();
  AptCharacterAnimation* anim = c->Animation();
  AptCharacter* ch = nullptr;
  for (int i = 0; anim && i < anim->exportCount; ++i)
    if (id == anim->exports[i].name && anim->exports[i].character >= 0 && anim->exports[i].character < anim->characterCount)
      ch = anim->characters[anim->exports[i].character];
  if (!ch) {
    std::fprintf(stderr, "[apt] attachMovie: no export %s\n", id.c_str());
    return AptVar::Undefined();
  }
  for (int i = 0; anim && i < anim->exportCount; ++i)
    if (id == anim->exports[i].name) c->runInitActions(anim->exports[i].character);
  AptCIH* inst = AptCreateInstance(c, ch, Arg(a, 2).ToInteger() + kAptScriptDepthBase, Arg(a, 1).ToString(), [&](AptCIH* i) {
    i->m_placedByScript = true;
    CopyInitObject(in, i, Arg(a, 3));
  });
  return AptVar::Obj(inst);
}
// 8028ABFC createEmptyMovieClip(name, depth): an instance of an empty sprite.
// Depths given by script are offset by 0x4000 in the display list (attachMovie, duplicateMovieClip,
// createTextField, createEmptyMovieClip, swapDepths; getDepth subtracts it), so they never collide
// with the timeline's own placements (Flash keeps those 16384 below the script range too).
AptCharacterSprite* EmptySprite() {
  static AptCharacterSprite* s = [] {
    auto* sp = new AptCharacterSprite;
    sp->type = AptCharacter::kSprite;
    sp->timeline.frameCount = 0;
    return sp;
  }();
  return s;
}
AptVar MC_createEmptyMovieClip(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  if (!c) return AptVar::Undefined();
  AptCharacterSprite* empty = EmptySprite();
  empty->movie = c->m_character ? c->m_character->movie : nullptr;
  AptCIH* inst = AptCreateInstance(c, empty, Arg(a, 1).ToInteger() + kAptScriptDepthBase, Arg(a, 0).ToString(),
                                   [&](AptCIH* i) { i->m_placedByScript = true; });
  return AptVar::Obj(inst);
}
// 80289B00 createTextField(name, depth, x, y, width, height)
AptVar MC_createTextField(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  if (!c) return AptVar::Undefined();
  // The field's own character, freed with the instance (and its duplicates). It was kept for the program's
  // life: about 50 fields (8 KB) more each time the main menu came back.
  auto owned = std::make_shared<AptCharacterEditText>();
  AptCharacterEditText* ch = owned.get();
  ch->type = AptCharacter::kEditText;
  ch->movie = c->m_character ? c->m_character->movie : nullptr;
  ch->bounds = {0, 0, Arg(a, 4).ToFloat(), Arg(a, 5).ToFloat()};
  ch->fontHeight = 12.0f;
  ch->colour = 0xff000000;
  AptCIH* inst = AptCreateInstance(c, ch, Arg(a, 1).ToInteger() + kAptScriptDepthBase, Arg(a, 0).ToString(), [&](AptCIH* i) {
    i->m_placedByScript = true;
    i->m_ownedCharacter = owned;
    i->m_matrix.tx = Arg(a, 2).ToFloat();
    i->m_matrix.ty = Arg(a, 3).ToFloat();
  });
  return AptVar::Obj(inst);
}
// 80289974 duplicateMovieClip(name, depth)
AptVar MC_duplicateMovieClip(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  if (!c || !c->m_parent) return AptVar::Undefined();
  AptCIH* inst = AptCreateInstance(c->m_parent, c->m_character, Arg(a, 1).ToInteger() + kAptScriptDepthBase, Arg(a, 0).ToString(), [&](AptCIH* i) {
    i->m_placedByScript = true;
    i->m_matrix = c->m_matrix;
    i->m_cxform = c->m_cxform;
    i->m_clipActions = c->m_clipActions;
    i->m_ownedCharacter = c->m_ownedCharacter;
  });
  return AptVar::Obj(inst);
}
// 80289A10 removeMovieClip (script-made clips only), 80289A88 removeTextField
AptVar MC_removeMovieClip(AptActionInterpreter& in, const AptVar& t, const Args&) {
  AptCIH* c = ThisClip(in, t);
  if (c && c->m_parent && c->m_placedByScript) c->m_parent->removeObject(c->m_depth);
  return AptVar::Undefined();
}
// 80289E34
AptVar MC_getDepth(AptActionInterpreter& in, const AptVar& t, const Args&) {
  AptCIH* c = ThisClip(in, t);
  return c ? AptVar::Int(c->m_depth - kAptScriptDepthBase) : AptVar::Undefined();
}
// 80289F18 swapDepths(depth | clip)
AptVar MC_swapDepths(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  if (!c || !c->m_parent) return AptVar::Undefined();
  AptDisplayList& dl = c->m_parent->m_children;
  int depth;
  if (AptCIH* other = Arg(a, 0).AsCIH()) depth = other->m_depth;
  else depth = Arg(a, 0).ToInteger() + kAptScriptDepthBase;
  AptCIH* other = dl.find(depth);
  if (other == c) {  // already there (swapping with itself)
    c->m_placedByScript = true;
    return AptVar::Undefined();
  }
  dl.remove(c);
  if (other) {
    dl.remove(other);
    other->m_depth = c->m_depth;
    other->m_placedByScript = true;
    dl.insert(other);
  }
  c->m_depth = depth;
  c->m_placedByScript = true;
  dl.insert(c);
  return AptVar::Undefined();
}
AptVar MC_getNextHighestDepth(AptActionInterpreter& in, const AptVar& t, const Args&) {
  AptCIH* c = ThisClip(in, t);
  int d = 0;
  if (c)
    for (AptCIH* k : c->m_children.m_list) d = std::max(d, k->m_depth - kAptScriptDepthBase + 1);
  return AptVar::Int(d);
}
// 8028A318 getBounds(target)
AptVar MC_getBounds(AptActionInterpreter& in, const AptVar& t, const Args&) {
  AptCIH* c = ThisClip(in, t);
  AptObject* o = NewObject();
  const float w = c ? c->GetProperty(8) : 0, h = c ? c->GetProperty(9) : 0;
  o->Put("xMin", AptVar::Number(c ? c->m_matrix.tx : 0));
  o->Put("yMin", AptVar::Number(c ? c->m_matrix.ty : 0));
  o->Put("xMax", AptVar::Number((c ? c->m_matrix.tx : 0) + w));
  o->Put("yMax", AptVar::Number((c ? c->m_matrix.ty : 0) + h));
  return AptVar::Obj(o);
}
// 8028F73C localToGlobal(point)
AptVar MC_localToGlobal(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  AptObject* p = Arg(a, 0).AsObject();
  if (!c || !p) return AptVar::Undefined();
  float x = p->Get(in, "x").ToFloat(), y = p->Get(in, "y").ToFloat();
  for (AptCIH* k = c; k; k = k->m_parent) {
    const AptMatrix& m = k->m_matrix;
    const float nx = m.a * x + m.c * y + m.tx, ny = m.b * x + m.d * y + m.ty;
    x = nx;
    y = ny;
  }
  p->Put("x", AptVar::Number(x));
  p->Put("y", AptVar::Number(y));
  return AptVar::Undefined();
}
AptVar MC_hitTest(AptActionInterpreter&, const AptVar&, const Args&) { return AptVar::Bool(false); }
// 8028A244 setMask(clip): not used by the menus yet.
AptVar MC_setMask(AptActionInterpreter&, const AptVar&, const Args&) {
  SIMMY_STUB("AptCharacterInst::sMethod_setMask (script masks)", 0x8028A244);
  return AptVar::Undefined();
}
// 802896B0 loadMovie(url): the movie replaces the clip's timeline.
AptVar MC_loadMovie(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  if (c) AptLoadAnimation(Arg(a, 0).ToString().c_str(), c->GetTargetPath().c_str());
  return AptVar::Undefined();
}
// 80289898 unloadMovie
AptVar MC_unloadMovie(AptActionInterpreter& in, const AptVar& t, const Args&) {
  if (AptCIH* c = ThisClip(in, t)) c->m_children.clear();
  return AptVar::Undefined();
}
AptVar MC_getBytes(AptActionInterpreter&, const AptVar&, const Args&) { return AptVar::Int(100); }
AptVar MC_noop(AptActionInterpreter&, const AptVar&, const Args&) { return AptVar::Undefined(); }

// ---- TextField ----------------------------------------------------------------------------------------

// 8028EA8C setTextFormat([begin, end,] format): the field takes the format's colour, size and align.
void ApplyFormat(AptActionInterpreter& in, AptCIH* c, AptObject* fmt) {
  if (!c || !fmt) return;
  AptVar color = fmt->Get(in, "color");
  if (color.IsNumber()) c->m_textColour = (c->m_textColour & 0xff000000) | (static_cast<unsigned int>(color.ToInteger()) & 0xffffff);
  AptVar size = fmt->Get(in, "size");
  if (size.IsNumber()) c->m_textSize = size.ToFloat();
  AptVar align = fmt->Get(in, "align");
  if (align.IsString()) {
    const std::string& s = align.AsString()->m_text;
    c->m_textAlign = s == "right" ? 1 : s == "center" ? 2 : 0;
  }
  c->m_textFormat = fmt;
  c->m_textDirty = true;
}
AptVar TF_setTextFormat(AptActionInterpreter& in, const AptVar& t, const Args& a) {
  AptCIH* c = ThisClip(in, t);
  ApplyFormat(in, c, a.empty() ? nullptr : a.back().AsObject());
  return AptVar::Undefined();
}
// 8028F178 getTextFormat / 8028ED54 getNewTextFormat
AptVar TF_getTextFormat(AptActionInterpreter& in, const AptVar& t, const Args&) {
  AptCIH* c = ThisClip(in, t);
  AptVar f = TextFormat_ctor(in, AptVar(), {});
  AptObject* o = f.AsObject();
  if (c) {
    o->Put("color", AptVar::Int(static_cast<int>(c->m_textColour & 0xffffff)));
    o->Put("size", AptVar::Number(c->m_textSize));
    o->Put("align", AptVar::Str(c->m_textAlign == 1 ? "right" : c->m_textAlign == 2 ? "center" : "left"));
    o->Put("font", AptVar::Str("Verdana"));
  }
  if (AptObject* ctor = gpGlobalGlobalObject->Get(in, "TextFormat").AsObject()) o->m_proto = ctor->Get(in, "prototype").AsObject();
  return f;
}
}  // namespace

AptObject* AptGetRegisteredClass(const std::string& name) {
  if (!s_registered) return nullptr;
  AptProperty* p = s_registered->Find(name);
  return p ? p->value.AsObject() : nullptr;
}

// ---- Character instance members (AptCharacterInst::objectMemberLookup 8028C524 / Set 8028B44C) ---------

namespace {
int PropertyIndex(const std::string& n) {
  static const char* const kNames[] = {"_x", "_y", "_xscale", "_yscale", "_currentframe", "_totalframes",
                                       "_alpha", "_visible", "_width", "_height", "_rotation", "_target",
                                       "_framesloaded", "_name", "_droptarget", "_url", "_highquality",
                                       "_focusrect", "_soundbuftime", "_quality", "_xmouse", "_ymouse"};
  for (int i = 0; i < 22; ++i)
    if (n == kNames[i]) return i;
  return -1;
}
}  // namespace

bool AptCIH::GetOwn(AptActionInterpreter& in, const std::string& name, AptVar& out) {
  if (!name.empty() && name[0] == '_') {
    const int index = PropertyIndex(name);
    if (index == 11) return out = AptVar::Str(GetTargetPath()), true;
    if (index == 13) return out = AptVar::Str(m_name), true;
    if (index == 7) return out = AptVar::Bool(m_visible), true;
    if (index == 15) return out = AptVar::Str(""), true;
    if (index >= 0) return out = AptVar::Number(GetProperty(index)), true;
    if (name == "_parent") return out = m_parent ? AptVar::Obj(m_parent) : AptVar::Undefined(), true;
    if (name == "_root") return out = AptVar::Obj(GetRoot()), true;
    if (name == "_global") return out = AptVar::Obj(gpGlobalGlobalObject), true;
  }
  if (IsText()) {
    if (name == "text" || name == "htmlText") {
      UpdateText();
      return out = AptVar::Str(m_text), true;
    }
    if (name == "textColor") return out = AptVar::Int(static_cast<int>(m_textColour & 0xffffff)), true;
    if (name == "variable") return out = m_variable.empty() ? AptVar::Null() : AptVar::Str(m_variable), true;
    if (name == "wordWrap") return out = AptVar::Bool(m_wordWrap), true;
    if (name == "multiline") return out = AptVar::Bool(m_multiline), true;
    if (name == "border") return out = AptVar::Bool(m_border), true;
    if (name == "borderColor") return out = AptVar::Int(static_cast<int>(m_borderColour)), true;
    if (name == "selectable") return out = AptVar::Bool(m_selectable), true;
    if (name == "autoSize") return out = AptVar::Str(m_autoSize), true;
    if (name == "length") return out = AptVar::Int(static_cast<int>(m_text.size())), true;
    if (name == "textWidth" || name == "textHeight") {
      // measured by the aux layer when the string is allocated (sizeX/Y, in stage units / 640, 448)
      extern void AptMeasureText(AptCIH* field, float& w, float& h);
      float w = 0, h = 0;
      AptMeasureText(this, w, h);
      return out = AptVar::Number(name == "textWidth" ? w : h), true;
    }
  }
  if (AptObject::GetOwn(in, name, out)) return true;
  if (AptCIH* child = m_children.findByName(name)) return out = AptVar::Obj(child), true;
  return false;
}

bool AptCIH::SetOwn(AptActionInterpreter& in, const std::string& name, const AptVar& value) {
  if (!name.empty() && name[0] == '_') {
    const int index = PropertyIndex(name);
    if (index == 13) return m_name = value.ToString(), true;
    if (index >= 0 && index != 11) {
      SetProperty(index, value.ToFloat());  // setProceduralProperty takes a number (_visible "0" hides)
      return true;
    }
  }
  if (IsText()) {
    if (name == "text" || name == "htmlText") {
      m_text = value.ToString();
      m_textDirty = true;
      if (!m_variable.empty() && m_variable[0] != '$') {
        AptActionInterpreter::Context ctx;
        AptCIH* owner = m_parent;
        while (owner && !owner->IsSprite()) owner = owner->m_parent;
        ctx.target = owner;
        AptActionInterpreter::Context* saved = in.m_ctx;
        in.m_ctx = &ctx;
        in.setVariable(m_variable, AptVar::Str(m_text));
        in.m_ctx = saved;
      }
      return true;
    }
    if (name == "textColor") {
      m_textColour = (m_textColour & 0xff000000) | (static_cast<unsigned int>(value.ToInteger()) & 0xffffff);
      m_textDirty = true;
      return true;
    }
    if (name == "variable") return m_variable = value.IsNullish() ? "" : value.ToString(), m_textDirty = true, true;
    if (name == "wordWrap") return m_wordWrap = value.ToBool(), m_textDirty = true, true;
    if (name == "multiline") return m_multiline = value.ToBool(), m_textDirty = true, true;
    if (name == "border") return m_border = value.ToBool(), true;
    if (name == "borderColor") return m_borderColour = static_cast<unsigned int>(value.ToInteger()), true;
    if (name == "selectable") return m_selectable = value.ToBool(), true;
    if (name == "autoSize") return m_autoSize = value.type == AptVar::kBoolean ? (value.i ? "left" : "none") : value.ToString(), true;
  }
  return AptObject::SetOwn(in, name, value);
}

void AptCIH::Enumerate(std::vector<std::string>& names) {
  AptObject::Enumerate(names);
  for (AptCIH* c : m_children.m_list) names.push_back(c->m_name);
}

// ---- Registration ------------------------------------------------------------------------------------------

// 802B10A4 _constructBuiltInObjects
void AptRegisterBuiltins() {
  s_objectProto = new AptObject;
  s_functionProto = new AptObject;
  s_functionProto->m_proto = s_objectProto;
  s_registered = new AptObject;
  AptGC::AddRoot(s_registered);
  AptGlobal* g = gpGlobalGlobalObject;
  g->m_proto = s_objectProto;

  AptNativeFunction* object = Class("Object", Object_ctor, s_objectProto);
  Method(object, "registerClass", Object_registerClass);
  Method(s_objectProto, "addProperty", Object_addProperty);
  Method(s_objectProto, "hasOwnProperty", Object_hasOwnProperty);
  Method(s_objectProto, "toString", Object_toString);
  Method(s_objectProto, "valueOf", Object_valueOf);
  Class("Function", Function_ctor, s_functionProto);
  Method(s_functionProto, "call", Function_call);
  Method(s_functionProto, "apply", Function_apply);

  s_arrayProto = NewObject();
  Class("Array", Array_ctor, s_arrayProto);
  Method(s_arrayProto, "push", Array_push);
  Method(s_arrayProto, "pop", Array_pop);
  Method(s_arrayProto, "shift", Array_shift);
  Method(s_arrayProto, "unshift", Array_unshift);
  Method(s_arrayProto, "splice", Array_splice);
  Method(s_arrayProto, "slice", Array_slice);
  Method(s_arrayProto, "join", Array_join);
  Method(s_arrayProto, "concat", Array_concat);
  Method(s_arrayProto, "reverse", Array_reverse);
  Method(s_arrayProto, "sort", Array_sort);
  Method(s_arrayProto, "toString", Array_toString);

  AptObject* stringProto = NewObject();
  AptNativeFunction* string = Class("String", String_ctor, stringProto);
  Method(string, "fromCharCode", String_fromCharCode);
  Method(stringProto, "charAt", String_charAt);
  Method(stringProto, "charCodeAt", String_charCodeAt);
  Method(stringProto, "indexOf", String_indexOf);
  Method(stringProto, "lastIndexOf", String_lastIndexOf);
  Method(stringProto, "substr", String_substr);
  Method(stringProto, "substring", String_substring);
  Method(stringProto, "slice", String_slice);
  Method(stringProto, "split", String_split);
  Method(stringProto, "toLowerCase", String_toLowerCase);
  Method(stringProto, "toUpperCase", String_toUpperCase);
  Method(stringProto, "toString", String_toString);
  Method(stringProto, "valueOf", String_toString);
  Method(stringProto, "concat", String_concat);

  AptObject* numberProto = NewObject();
  Class("Number", Number_ctor, numberProto);
  Method(numberProto, "toString", Number_toString);
  Class("Boolean", Boolean_ctor, NewObject());

  Method(g, "parseInt", parseIntFn);
  Method(g, "parseFloat", parseFloatFn);
  Method(g, "isNaN", isNaNFn);
  Method(g, "escape", escapeFn);
  Method(g, "unescape", unescapeFn);
  Method(g, "setInterval", setIntervalFn);
  Method(g, "clearInterval", clearIntervalFn);
  Method(g, "ASSetPropFlags", ASSetPropFlags);

  AptObject* math = NewObject();
  math->m_kind = AptValue::kMath;
  g->Put("Math", AptVar::Obj(math), 1);
  Method(math, "abs", Math_abs);
  Method(math, "ceil", Math_ceil);
  Method(math, "floor", Math_floor);
  Method(math, "round", Math_round);
  Method(math, "sqrt", Math_sqrt);
  Method(math, "sin", Math_sin);
  Method(math, "cos", Math_cos);
  Method(math, "tan", Math_tan);
  Method(math, "asin", Math_asin);
  Method(math, "acos", Math_acos);
  Method(math, "atan", Math_atan);
  Method(math, "atan2", Math_atan2);
  Method(math, "exp", Math_exp);
  Method(math, "log", Math_log);
  Method(math, "pow", Math_pow);
  Method(math, "min", Math_min);
  Method(math, "max", Math_max);
  Method(math, "random", Math_random);
  math->Put("PI", AptVar::Float(3.14159265f), 1);
  math->Put("E", AptVar::Float(2.71828183f), 1);
  math->Put("SQRT2", AptVar::Float(1.41421356f), 1);

  AptObject* key = NewObject();
  key->m_kind = AptValue::kKey;
  g->Put("Key", AptVar::Obj(key), 1);
  Method(key, "addListener", Key_addListener);
  Method(key, "removeListener", Key_removeListener);
  Method(key, "getCode", Key_getCode);
  Method(key, "getAscii", Key_getAscii);
  Method(key, "getController", Key_getController);
  Method(key, "isDown", Key_isDown);
  static const std::pair<const char*, int> kKeys[] = {
      {"BACKSPACE", 8}, {"TAB", 9}, {"ENTER", 13}, {"SHIFT", 16}, {"CONTROL", 17}, {"CAPSLOCK", 20}, {"ESCAPE", 27},
      {"SPACE", 32}, {"PGUP", 33}, {"PGDN", 34}, {"END", 35}, {"HOME", 36}, {"LEFT", 37}, {"UP", 38},
      {"RIGHT", 39}, {"DOWN", 40}, {"INSERT", 45}, {"DELETEKEY", 46}};
  for (const auto& k : kKeys) key->Put(k.first, AptVar::Int(k.second), 1);
  gpPool->m_keyListeners = NewArray();

  AptObject* stage = NewObject();
  stage->m_kind = AptValue::kStage;
  stage->Put("width", AptVar::Int(640));
  stage->Put("height", AptVar::Int(448));
  g->Put("Stage", AptVar::Obj(stage), 1);

  auto* ext = new AptExtern;
  g->Put("extern", AptVar::Obj(ext), 1);

  AptObject* colorProto = NewObject();
  Class("Color", Color_ctor, colorProto);
  Method(colorProto, "setRGB", Color_setRGB);
  Method(colorProto, "getRGB", Color_getRGB);
  Method(colorProto, "setTransform", Color_setTransform);
  Method(colorProto, "getTransform", Color_getTransform);

  Class("TextFormat", TextFormat_ctor, NewObject());

  s_xmlNodeProto = NewObject();
  Method(s_xmlNodeProto, "toString", XMLNode_toString);
  AptObject* xmlProto = NewObject(s_xmlNodeProto);
  Class("XML", XML_ctor, xmlProto);
  Class("XMLNode", XML_ctor, s_xmlNodeProto);
  Method(xmlProto, "parseXML", XML_parseXML);

  AptObject* mc = NewObject();
  Class("MovieClip", MC_noop, mc);
  Method(mc, "gotoAndPlay", MC_gotoAndPlay);
  Method(mc, "gotoAndStop", MC_gotoAndStop);
  Method(mc, "play", MC_play);
  Method(mc, "stop", MC_stop);
  Method(mc, "nextFrame", MC_nextFrame);
  Method(mc, "prevFrame", MC_prevFrame);
  Method(mc, "attachMovie", MC_attachMovie);
  Method(mc, "createEmptyMovieClip", MC_createEmptyMovieClip);
  Method(mc, "createTextField", MC_createTextField);
  Method(mc, "duplicateMovieClip", MC_duplicateMovieClip);
  Method(mc, "removeMovieClip", MC_removeMovieClip);
  Method(mc, "getDepth", MC_getDepth);
  Method(mc, "swapDepths", MC_swapDepths);
  Method(mc, "getNextHighestDepth", MC_getNextHighestDepth);
  Method(mc, "getBounds", MC_getBounds);
  Method(mc, "localToGlobal", MC_localToGlobal);
  Method(mc, "hitTest", MC_hitTest);
  Method(mc, "setMask", MC_setMask);
  Method(mc, "loadMovie", MC_loadMovie);
  Method(mc, "unloadMovie", MC_unloadMovie);
  Method(mc, "getBytesLoaded", MC_getBytes);
  Method(mc, "getBytesTotal", MC_getBytes);
  Method(mc, "startDrag", MC_noop);
  Method(mc, "stopDrag", MC_noop);

  AptObject* tf = NewObject();
  Class("TextField", MC_noop, tf);
  Method(tf, "setTextFormat", TF_setTextFormat);
  Method(tf, "setNewTextFormat", TF_setTextFormat);
  Method(tf, "getTextFormat", TF_getTextFormat);
  Method(tf, "getNewTextFormat", TF_getTextFormat);
  Method(tf, "removeTextField", MC_removeMovieClip);
}
