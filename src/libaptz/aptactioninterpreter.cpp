// Port of libaptz/AptActionInterpreter.cpp: the ActionScript (AVM1) interpreter with Apt's
// extensions (opcodes 0x59.. and 0xA1..0xB8: dictionary pushes, fused get/set/call/pop forms,
// immediate numbers). Bytecode is read in place from the .apt image: arguments of the classic
// opcodes are 4-byte aligned big-endian words, offsets point into the image, Push and
// DefineDictionary list constant-table indices (aptfile.hpp, PORTING.md "Apt movies on PC").
//
// The opcode handlers are the _FunctionAptAction* functions (sGlobalTable 8043D344). They are
// written as cases of one switch; each case is marked with its original address.
#include "simmy/types_gen.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

AptActionInterpreter gAptActionInterpreter;

bool s_traceOps = std::getenv("SIMMY_APT_TRACE") != nullptr;  // PC: op trace on stderr
// PC: calls of undefined functions/methods are silent no-ops on GC (Flash semantics). The movies make
// such calls by design (every spatialoptionlist calls option.deselect(), which no tab or grid cell
// defines; PCT's onTabChange refreshes the grid before init() attaches it), so they are logged only
// with SIMMY_APT_WARN=1.
static const bool s_warnCalls = std::getenv("SIMMY_APT_WARN") != nullptr;
namespace {
unsigned int BE32(const unsigned char* p) {
  return static_cast<unsigned int>(p[0]) << 24 | static_cast<unsigned int>(p[1]) << 16 |
         static_cast<unsigned int>(p[2]) << 8 | p[3];
}
const unsigned char* Align4(const unsigned char* p, const unsigned char* base) {
  const size_t off = static_cast<size_t>(p - base);
  return base + ((off + 3) & ~static_cast<size_t>(3));
}

int s_unknownLogged[256];

// One constant-table entry as a value (what resolveStream stored in the stream on GC).
AptVar ConstValue(AptFileImage* file, unsigned int index, const std::vector<AptVar>& pool,
                  const std::vector<AptVar>& regs) {
  if (!file || static_cast<int>(index) >= file->m_constFile.itemCount) return AptVar::Undefined();
  AptConstItem& item = file->m_constFile.items[index];
  switch (item.type) {
    case 1:
      if (!item.resolved) {
        item.resolved = new AptString(item.string ? item.string : "");
        AptGC::AddRoot(item.resolved);
      }
      return AptVar::Str(static_cast<AptString*>(item.resolved));
    case 2: return AptVar::Null();
    case 3: return AptVar::Undefined();
    case 4: return item.value < regs.size() ? regs[item.value] : AptVar::Undefined();  // register
    case 5: return AptVar::Bool(item.value != 0);
    case 6: {
      float f;
      std::memcpy(&f, &item.value, 4);
      return AptVar::Float(f);
    }
    case 7: return AptVar::Int(static_cast<int>(item.value));
    case 8: return item.value < pool.size() ? pool[item.value] : AptVar::Undefined();  // dictionary
    default: return AptVar::Undefined();
  }
}

// Numbers for arithmetic: integer when both sides are integers (Apt's AptInteger fast paths).
bool BothInt(const AptVar& a, const AptVar& b) { return a.type == AptVar::kInteger && b.type == AptVar::kInteger; }

// The comparisons below follow Apt's SWF 6 paths (AptGetSwfVersion() is always 6 on GC, see
// AptVar::ToBool): an undefined operand is not special, it converts like any other value (to 0 / "").
bool Primitive(const AptVar& v) {
  return v.type == AptVar::kInteger || v.type == AptVar::kFloat || v.type == AptVar::kBoolean ||
         v.type == AptVar::kString;
}
bool NearlyEqual(float x, float y) { return std::fabs(x - y) < 0.001f; }
// A numeric operand that compares as a float: a float, or a numeric string with a '.'.
bool FloatLike(const AptVar& v) {
  return v.type == AptVar::kFloat || (v.IsString() && v.AsString()->m_text.find('.') != std::string::npos);
}

// 80278E44 Equals2
bool LooseEquals(AptActionInterpreter&, const AptVar& a0, const AptVar& b0) {
  // null is undefined here (Apt's null type, 0x13, is replaced by the undefined value first)
  const AptVar a = a0.type == AptVar::kNull ? AptVar::Undefined() : a0;
  const AptVar b = b0.type == AptVar::kNull ? AptVar::Undefined() : b0;
  if (!Primitive(a) || !Primitive(b)) {
    if (a.type != b.type) return false;
    if (a.type == AptVar::kUndefined) return true;
    return a.p == b.p;
  }
  if (a.type == AptVar::kFloat && b.type == AptVar::kFloat) return a.f == b.f;
  if (a.IsString() && b.IsString()) return a.AsString()->m_text == b.AsString()->m_text;
  if (a.type == AptVar::kBoolean || b.type == AptVar::kBoolean) return a.ToInteger() == b.ToInteger();
  // a number and a number or string: a string that is not numeric compares as text
  if (a.IsNaNLike() || b.IsNaNLike()) return a.ToString() == b.ToString();
  if (a.type != AptVar::kInteger && b.type != AptVar::kInteger) return NearlyEqual(a.ToFloat(), b.ToFloat());
  if (FloatLike(a) || FloatLike(b)) return NearlyEqual(a.ToFloat(), b.ToFloat());
  return a.ToInteger() == b.ToInteger();
}

// 8027CEB8 StrictEquals: same type; numbers within 0.001 unless both integers; strings compare
// without case (strcasecmp); objects by identity.
bool StrictEq(const AptVar& a0, const AptVar& b0) {
  const AptVar a = a0.type == AptVar::kNull ? AptVar::Undefined() : a0;  // as in Equals2
  const AptVar b = b0.type == AptVar::kNull ? AptVar::Undefined() : b0;
  if (a.IsNumber() && b.IsNumber()) {
    if (BothInt(a, b)) return a.i == b.i;
    return NearlyEqual(a.ToFloat(), b.ToFloat());
  }
  if (a.type != b.type) return false;
  switch (a.type) {
    case AptVar::kUndefined:
    case AptVar::kNull: return true;
    case AptVar::kBoolean: return a.i == b.i;
    case AptVar::kString: {
      const std::string& x = a.AsString()->m_text;
      const std::string& y = b.AsString()->m_text;
      if (x.size() != y.size()) return false;
      for (size_t i = 0; i < x.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(x[i])) != std::tolower(static_cast<unsigned char>(y[i])))
          return false;
      return true;
    }
    default: return a.p == b.p;
  }
}

// 80278310 LessThan2 result: 1 true, 0 false, -1 undefined (an operand is NaN). Two strings compare
// with strcmp; else integers unless either is a float.
int Less(const AptVar& a, const AptVar& b) {
  if (a.IsString() && b.IsString()) return std::strcmp(a.AsString()->m_text.c_str(), b.AsString()->m_text.c_str()) < 0;
  if (b.IsNaNLike() || a.IsNaNLike()) return -1;
  if (a.type != AptVar::kFloat && b.type != AptVar::kFloat) return a.ToInteger() < b.ToInteger() ? 1 : 0;
  return a.ToFloat() < b.ToFloat() ? 1 : 0;
}
// 8027D43C Greater (a > b): as LessThan2 with the operands swapped, but never undefined in SWF 6.
bool Greater(const AptVar& a, const AptVar& b) {
  if (a.IsString() && b.IsString()) return std::strcmp(b.AsString()->m_text.c_str(), a.AsString()->m_text.c_str()) < 0;
  if (a.type != AptVar::kFloat && b.type != AptVar::kFloat) return b.ToInteger() < a.ToInteger();
  return b.ToFloat() < a.ToFloat();
}
}  // namespace

AptVar AptActionInterpreter::pop() {
  if (m_stack.empty()) return AptVar::Undefined();
  AptVar v = m_stack.back();
  m_stack.pop_back();
  return v;
}

// ---- Variables and members --------------------------------------------------------------------------

AptObject* AptActionInterpreter::toObject(const AptVar& v) {
  if (AptObject* o = v.AsObject()) return o;
  return nullptr;
}

// Member read with Apt's special cases: strings expose length and String.prototype, arrays index,
// extern asks the game.
AptVar AptActionInterpreter::getMember(const AptVar& obj, const std::string& name) {
  if (AptObject* o = obj.AsObject()) return o->Get(*this, name);
  if (obj.IsString()) {
    if (name == "length") return AptVar::Int(static_cast<int>(obj.AsString()->m_text.size()));
    AptVar proto = gpGlobalGlobalObject->Get(*this, "String");
    if (AptObject* ctor = proto.AsObject())
      if (AptObject* p = ctor->Get(*this, "prototype").AsObject()) return p->Get(*this, name);
  }
  if (obj.IsNumber() || obj.type == AptVar::kBoolean) {
    AptVar ctor = gpGlobalGlobalObject->Get(*this, obj.IsNumber() ? "Number" : "Boolean");
    if (AptObject* c = ctor.AsObject())
      if (AptObject* p = c->Get(*this, "prototype").AsObject()) return p->Get(*this, name);
  }
  return AptVar::Undefined();
}

void AptActionInterpreter::setMember(const AptVar& obj, const std::string& name, const AptVar& v) {
  if (AptObject* o = obj.AsObject()) o->SetOwn(*this, name, v);
}

// getContext (8026D958): resolves a target path ("/a/b", "a.b", "_root.a", "_level0", "..").
AptObject* AptActionInterpreter::resolvePath(const std::string& path, AptObject* base) {
  AptObject* cur = base;
  size_t i = 0;
  if (!path.empty() && path[0] == '/') {
    AptCIH* clip = cur && cur->m_kind == AptValue::kCIH ? static_cast<AptCIH*>(cur) : nullptr;
    cur = clip ? clip->GetRoot() : gpPool->getLevel(0);
    i = 1;
  }
  while (i < path.size() && cur) {
    size_t j = path.find_first_of("/.:", i);
    if (j == std::string::npos) j = path.size();
    const std::string part = path.substr(i, j - i);
    i = j + 1;
    if (part.empty()) continue;
    AptVar next;
    if (part == "..") {
      AptCIH* clip = cur->m_kind == AptValue::kCIH ? static_cast<AptCIH*>(cur) : nullptr;
      next = clip && clip->m_parent ? AptVar::Obj(clip->m_parent) : AptVar::Undefined();
    } else if (cur == base && (part == "_root" || part == "_global" || part == "this" || part == "_parent" ||
                               part.rfind("_level", 0) == 0)) {
      next = cur == base && m_ctx ? getVariable(part) : cur->Get(*this, part);
    } else {
      next = cur->Get(*this, part);
    }
    cur = next.AsObject();
  }
  return cur;
}

// 8026FB50
// Plain names look in with() objects, the function's locals (and its enclosing functions'), the
// current target clip, the clip the function was defined in, then _global. "a.b" and "/a/b:c"
// paths resolve their target first.
AptVar AptActionInterpreter::getVariable(const std::string& name) {
  Context* c = m_ctx;
  AptCIH* target = c ? c->target : nullptr;
  if (name == "this") return c ? c->thisv : AptVar::Undefined();
  if (name == "_global") return AptVar::Obj(gpGlobalGlobalObject);
  if (name == "_root") return target ? AptVar::Obj(target->GetRoot()) : AptVar::Obj(gpPool->getLevel(0));
  if (name == "_parent") return target && target->m_parent ? AptVar::Obj(target->m_parent) : AptVar::Undefined();
  if (name == "super" && c && c->super) return AptVar::Obj(c->super);
  if (name.rfind("_level", 0) == 0 && name.size() > 6 &&
      name.find_first_not_of("0123456789", 6) == std::string::npos) {
    AptCIH* level = gpPool->getLevel(std::atoi(name.c_str() + 6));
    return level ? AptVar::Obj(level) : AptVar::Undefined();
  }
  const size_t colon = name.find(':');
  const size_t slash = name.find('/');
  if (colon != std::string::npos || slash != std::string::npos) {
    // "/path:var" or "path:var" or "/path"
    std::string path = name, var;
    if (colon != std::string::npos) {
      path = name.substr(0, colon);
      var = name.substr(colon + 1);
    }
    AptObject* o = resolvePath(path, target);
    if (!o) return AptVar::Undefined();
    return var.empty() ? AptVar::Obj(o) : o->Get(*this, var);
  }
  const size_t dot = name.find('.');
  if (dot != std::string::npos && dot > 0) {
    AptVar base = getVariable(name.substr(0, dot));
    std::string rest = name.substr(dot + 1);
    while (true) {
      const size_t d = rest.find('.');
      const std::string part = rest.substr(0, d);
      base = getMember(base, part);
      if (d == std::string::npos) return base;
      rest = rest.substr(d + 1);
    }
  }
  if (c) {
    for (auto it = c->with.rbegin(); it != c->with.rend(); ++it)
      if ((*it)->Has(*this, name)) return (*it)->Get(*this, name);
    for (AptFrameStack* f = c->locals; f; f = f->m_outer)
      if (AptProperty* p = f->Find(name)) return p->value;
  }
  if (target && target->Has(*this, name)) return target->Get(*this, name);
  if (c && c->function && c->function->m_target && c->function->m_target != target &&
      c->function->m_target->Has(*this, name))
    return c->function->m_target->Get(*this, name);
  if (gpGlobalGlobalObject->Has(*this, name)) return gpGlobalGlobalObject->Get(*this, name);
  return AptVar::Undefined();
}

// 8026DB60
// Sets where the name already exists in the locals chain, else on the target clip.
void AptActionInterpreter::setVariable(const std::string& name, const AptVar& v) {
  Context* c = m_ctx;
  AptCIH* target = c ? c->target : nullptr;
  const size_t colon = name.find(':');
  const size_t slash = name.find('/');
  if (colon != std::string::npos || slash != std::string::npos) {
    std::string path = name, var;
    if (colon != std::string::npos) {
      path = name.substr(0, colon);
      var = name.substr(colon + 1);
    }
    if (AptObject* o = resolvePath(path, target)) o->SetOwn(*this, var, v);
    return;
  }
  const size_t dot = name.rfind('.');
  if (dot != std::string::npos && dot > 0) {
    if (AptObject* o = getVariable(name.substr(0, dot)).AsObject()) o->SetOwn(*this, name.substr(dot + 1), v);
    return;
  }
  // 8026D958 getContext: a plain name's object is the With object (LocalContextT +8) when one is
  // active, else the target (+4). Unlike Flash, which writes into a with object only when it already
  // has the name, Apt writes there unconditionally: the movies rely on it (the buy catalogue's cells do
  // `with (this["PCT_Icon" + num]) { _type = "CCR2D"; ... }` to make the attached shader clip a custom
  // control). 8026DB60 then sets a function's local of that name, else the object's property (GC
  // tries the object's procedural properties, _x etc., before the locals: that only differs for a
  // local named like one).
  AptObject* obj = target;
  if (c && !c->with.empty()) obj = c->with.back();
  if (c) {
    for (AptFrameStack* f = c->locals; f; f = f->m_outer)
      if (AptProperty* p = f->Find(name)) {
        p->value = v;
        return;
      }
  }
  if (obj) obj->SetOwn(*this, name, v);
}

// ---- Calls -------------------------------------------------------------------------------------------

// 80270070
AptVar AptActionInterpreter::callFunction(const AptVar& fn, const AptVar& thisv, const std::vector<AptVar>& args,
                                          AptObject* super) {
  AptFunctionValue* f = fn.AsFunction();
  if (!f) return AptVar::Undefined();
  if (m_depth > 200) {
    std::fprintf(stderr, "[apt] call depth exceeded\n");
    return AptVar::Undefined();
  }
  ++m_depth;
  AptVar r = f->Call(*this, thisv, args, super);
  --m_depth;
  return r;
}

AptVar AptActionInterpreter::callMethod(const AptVar& obj, const std::string& name, const std::vector<AptVar>& args) {
  return callFunction(getMember(obj, name), obj, args);
}

// AptScriptFunction1/2::SetupBeforeExecution + runStream + CleanupAfterExecution.
AptVar AptActionInterpreter::runFunction(AptScriptFunctionBase* f, const AptVar& thisv, const std::vector<AptVar>& args,
                                         AptObject* super) {
  Context c;
  c.file = f->m_file;
  c.pc = f->m_body;
  c.end = f->m_body + f->m_bodySize;
  c.function = f;
  c.target = f->m_target;
  if (AptCIH* clip = thisv.AsCIH()) c.target = clip;  // timeline functions act on the receiver clip
  c.origTarget = c.target;
  c.thisv = thisv;
  c.pool = f->m_constantPool;
  c.with = f->m_withScope;
  c.locals = new AptFrameStack(f->m_scope);
  c.super = super;
  c.registers.assign(static_cast<size_t>(std::max(4, f->m_registerCount)), AptVar::Undefined());
  if (f->m_version == 2) {
    // AptScriptFunction2::SetupBeforeExecution (802B84CC): preload registers in order this, arguments, super, _root, _parent, _global.
    int reg = 1;
    const unsigned int flags = f->m_flags;
    auto set = [&](const AptVar& v) {
      if (reg < static_cast<int>(c.registers.size())) c.registers[static_cast<size_t>(reg)] = v;
      ++reg;
    };
    if (flags & 0x01) set(thisv);
    if (flags & 0x04) {
      AptArray* a = new AptArray;
      a->m_items = args;
      set(AptVar::Obj(a));
    }
    if (flags & 0x10) set(super ? AptVar::Obj(super) : AptVar::Undefined());
    Context* saved = m_ctx;
    m_ctx = &c;
    if (flags & 0x40) set(getVariable("_root"));
    if (flags & 0x80) set(getVariable("_parent"));
    if (flags & 0x100) set(AptVar::Obj(gpGlobalGlobalObject));
    m_ctx = saved;
  }
  // AptScriptFunction1/2::SetArgument (802B8D20 / 802B8F64): a register or a local.
  for (size_t i = 0; i < f->m_args.size(); ++i) {
    const AptVar v = i < args.size() ? args[i] : AptVar::Undefined();
    const int reg = f->m_args[i].first;
    if (f->m_version == 2 && reg > 0 && reg < static_cast<int>(c.registers.size()))
      c.registers[static_cast<size_t>(reg)] = v;
    else
      c.locals->Put(f->m_args[i].second, v);
  }
  if (f->m_version == 1 && !(f->m_flags & 0x08)) {
    AptArray* a = new AptArray;
    a->m_items = args;
    c.locals->Put("arguments", AptVar::Obj(a));
  }
  const size_t stackBase = m_stack.size();
  run(c);
  if (m_stack.size() > stackBase) m_stack.resize(stackBase);
  return c.returned ? c.result : AptVar::Undefined();
}

// 802720D0
AptVar AptActionInterpreter::runStream(const unsigned char* stream, AptFileImage* file, AptCIH* target) {
  if (!stream || !file) return AptVar::Undefined();
  Context c;
  c.file = file;
  c.pc = stream;
  c.target = c.origTarget = target;
  c.thisv = target ? AptVar::Obj(target) : AptVar::Undefined();
  c.registers.assign(4, AptVar::Undefined());
  const size_t stackBase = m_stack.size();
  run(c);
  if (m_stack.size() > stackBase) m_stack.resize(stackBase);
  return c.result;
}

// _createObject (80270930): `new ctor(args)`. Natives with their own construction (Array, Color,
// XML...) build the object; script classes get an Object whose __proto__ is ctor.prototype.
AptObject* AptActionInterpreter::newObject(const AptVar& ctorv, const std::vector<AptVar>& args) {
  AptFunctionValue* ctor = ctorv.AsFunction();
  if (!ctor) return new AptObject;
  if (ctor->m_kind == AptValue::kFunction) {
    if (auto* native = dynamic_cast<AptNativeFunction*>(ctor)) {
      AptVar r = native->m_fn(*this, AptVar(), args);  // natives construct when this is undefined
      if (AptObject* o = r.AsObject()) {
        if (AptObject* proto = ctor->Get(*this, "prototype").AsObject()) o->m_proto = proto;
        return o;
      }
    }
  }
  AptObject* o = new AptObject;
  o->m_proto = ctor->Get(*this, "prototype").AsObject();
  o->Put("__constructor__", ctorv, 1);
  m_tempRoots.push_back(o);
  AptProperty* sc = o->m_proto ? o->m_proto->Find("__constructor__") : nullptr;
  callFunction(ctorv, AptVar::Obj(o), args, sc ? sc->value.AsObject() : nullptr);
  m_tempRoots.pop_back();
  return o;
}

// 80280E48
bool AptActionInterpreter::instanceOf(const AptVar& v, AptObject* ctor) {
  AptObject* o = v.AsObject();
  if (!o || !ctor) return false;
  AptObject* proto = ctor->Get(*this, "prototype").AsObject();
  for (AptObject* p = o->m_proto; p; p = p->m_proto) {
    if (p == proto) return true;
    for (AptObject* i : p->m_interfaces)
      if (i == ctor) return true;
  }
  return false;
}

// ---- The loop ----------------------------------------------------------------------------------------

namespace {
std::vector<AptVar> PopArgs(AptActionInterpreter& in, int count) {
  std::vector<AptVar> args;
  count = std::max(0, std::min(count, 256));
  args.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) args.push_back(in.pop());
  return args;
}

// The super object of a method call: the prototype above the one holding the method.
AptObject* SuperFor(AptActionInterpreter& in, AptObject* obj, const std::string& name) {
  if (!obj) return nullptr;
  (void)in;
  for (AptObject* p = obj; p; p = p->m_proto)
    if (p->Find(name)) {
      AptProperty* sc = p->Find("__constructor__");
      return sc ? sc->value.AsObject() : nullptr;
    }
  return nullptr;
}

// CallMethod/NewMethod on `super`: the super constructor's prototype holds the method.
}  // namespace

void AptActionInterpreter::run(Context& c) {
  Context* saved = m_ctx;
  m_ctx = &c;
  m_contexts.push_back(&c);
  ++m_depth;
  const unsigned char* base = c.file->m_apt;
  struct WithEntry {
    AptObject* obj;
    const unsigned char* end;
  };
  std::vector<WithEntry> withEnds;
  const size_t baseWith = c.with.size();
  int guard = 0;
  while (!c.returned && c.pc && (!c.end || c.pc < c.end)) {
    while (!withEnds.empty() && c.pc >= withEnds.back().end) {
      withEnds.pop_back();
      if (c.with.size() > baseWith) c.with.pop_back();
    }
    if (++guard > 2000000) {
      std::fprintf(stderr, "[apt] script runs too long, stopped\n");
      break;
    }
    if (s_traceOps) {
      const unsigned char o = *c.pc;
      std::string pooled;
      if ((o == 0xA2 || o == 0xAE || o == 0xAF || o == 0xB2 || o == 0xB3) && c.pc[1] < c.pool.size()) pooled = c.pool[c.pc[1]].ToString();
      std::fprintf(stderr, "[apt op] %05x %02X stack %zu top %s %s\n", static_cast<unsigned>(c.pc - base), o, m_stack.size(),
                   m_stack.empty() ? "-" : (m_stack.back().IsObject() ? "<obj>" : m_stack.back().ToString().c_str()),
                   pooled.c_str());
    }
    const unsigned char op = *c.pc++;
    if (op == 0) break;  // 802729D4 End
    const unsigned char* arg = Align4(c.pc, base);
    switch (op) {
      // ---- timeline
      case 0x04:  // 802729D8 NextFrame
        if (c.target) c.target->jumpToFrame(c.target->m_frame + 1), c.target->m_playing = false;
        break;
      case 0x05:  // 80272A24 PrevFrame
        if (c.target) c.target->jumpToFrame(c.target->m_frame - 1), c.target->m_playing = false;
        break;
      case 0x06:  // 80272A70 Play
        if (c.target) c.target->m_playing = true;
        break;
      case 0x07:  // 80272B44 Stop
        if (c.target) c.target->m_playing = false;
        break;
      case 0x08:  // 80272BBC ToggleQuality
      case 0x09:  // 80272BC0 StopSounds
        break;
      case 0x81: {  // 8027D7D8 GotoFrame
        const int frame = static_cast<int>(BE32(arg));
        c.pc = arg + 4;
        if (c.target) c.target->jumpToFrame(frame), c.target->m_playing = false;
        break;
      }
      case 0x8C: {  // 8027DC4C GotoLabel
        const char* label = reinterpret_cast<const char*>(c.file->AptAt(BE32(arg)));
        c.pc = arg + 4;
        if (c.target && label) {
          const int frame = c.target->labelToFrame(label);
          if (frame >= 0) c.target->jumpToFrame(frame), c.target->m_playing = false;
        }
        break;
      }
      case 0x9F: {  // 8027E940 GotoFrame2: integer frames as given (already 0-based), labels by name
        const bool play = BE32(arg) != 0;
        c.pc = arg + 4;
        AptVar f = pop();
        AptCIH* clip = c.target;
        int frame = -1;
        if (f.IsString()) {
          std::string s = f.AsString()->m_text;
          const size_t colon = s.find(':');
          if (colon != std::string::npos) {
            clip = resolvePath(s.substr(0, colon), c.target) ? AptVar::Obj(resolvePath(s.substr(0, colon), c.target)).AsCIH()
                                                             : nullptr;
            s = s.substr(colon + 1);
          }
          if (clip) frame = clip->labelToFrame(s);
          if (frame < 0 && !s.empty() && std::isdigit(static_cast<unsigned char>(s[0]))) frame = std::atoi(s.c_str()) - 1;
        } else if (f.type == AptVar::kInteger) {
          frame = f.i;
        }
        if (clip && frame != -1) {
          clip->jumpToFrame(frame);
          clip->m_playing = play;
        }
        break;
      }
      case 0x8B: {  // 8027DAD8 SetTarget
        const char* path = reinterpret_cast<const char*>(c.file->AptAt(BE32(arg)));
        c.pc = arg + 4;
        if (!path || !*path) c.target = c.origTarget;
        else if (AptCIH* t = AptVar::Obj(resolvePath(path, c.origTarget)).AsCIH()) c.target = t;
        break;
      }
      case 0x20: {  // 80274E98 SetTarget2
        AptVar t = pop();
        if (AptCIH* clip = t.AsCIH()) c.target = clip;
        else {
          const std::string path = t.ToString();
          if (path.empty()) c.target = c.origTarget;
          else if (AptCIH* clip2 = AptVar::Obj(resolvePath(path, c.origTarget)).AsCIH()) c.target = clip2;
        }
        break;
      }
      case 0x9E: {  // 8027E798 CallFrame
        AptVar f = pop();
        if (!c.target) break;
        const int frame = f.IsString() ? c.target->labelToFrame(f.AsString()->m_text) : f.ToInteger();
        const AptMovie* tl = c.target->Timeline();
        if (tl && frame >= 0 && frame < tl->frameCount)
          for (int i = 0; i < tl->frames[frame].controlCount; ++i) {
            const AptControl* ctl = tl->frames[frame].controls[i];
            if (ctl->type == AptControl::kAction)
              runStream(static_cast<const AptControlAction*>(ctl)->stream, c.target->m_file, c.target);
          }
        break;
      }
      // ---- arithmetic and logic (classic SWF4 forms)
      case 0x0A: case 0x0B: case 0x0C: {  // 80272BC4 Add, 80272E74 Subtract, 80273124 Multiply
        AptVar b = pop(), a = pop();
        if (BothInt(a, b)) {
          const long long x = a.i, y = b.i;
          push(AptVar::Int(static_cast<int>(op == 0x0A ? x + y : op == 0x0B ? x - y : x * y)));
        } else {
          const float x = a.ToFloat(), y = b.ToFloat();
          push(AptVar::Float(op == 0x0A ? x + y : op == 0x0B ? x - y : x * y));
        }
        break;
      }
      case 0x0D: {  // 802733D4 Divide (by 0: undefined)
        AptVar b = pop(), a = pop();
        const float y = b.ToFloat();
        push(y == 0.0f ? AptVar::Undefined() : AptVar::Float(a.ToFloat() / y));
        break;
      }
      case 0x3F: {  // 802769C8 Modulo: fmodf (by 0: undefined)
        AptVar b = pop(), a = pop();
        const float y = b.ToFloat();
        push(y == 0.0f ? AptVar::Undefined() : AptVar::Float(std::fmod(a.ToFloat(), y)));
        break;
      }
      case 0x0E: {  // 802735C4 Equals (within 0.001)
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(BothInt(a, b) ? a.i == b.i : NearlyEqual(a.ToFloat(), b.ToFloat())));
        break;
      }
      case 0x0F: {  // 80273840 LessThan
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(a.ToFloat() < b.ToFloat()));
        break;
      }
      case 0x10: {  // 80273AAC And
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(a.ToBool() && b.ToBool()));
        break;
      }
      case 0x11: {  // 80273D40 Or
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(a.ToBool() || b.ToBool()));
        break;
      }
      case 0x12:  // 80273FD0 Not
        push(AptVar::Bool(!pop().ToBool()));
        break;
      case 0x13: {  // 80274138 StringEquals
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(a.ToString() == b.ToString()));
        break;
      }
      case 0x14:  // 80274494 StringLength
        push(AptVar::Int(static_cast<int>(pop().ToString().size())));
        break;
      case 0x15: {  // 8027465C SubString (1-based index)
        const int count = pop().ToInteger();
        const int index = pop().ToInteger() - 1;
        const std::string s = pop().ToString();
        if (index < 0 || index >= static_cast<int>(s.size()) || count <= 0) push(AptVar::Str(""));
        else push(AptVar::Str(s.substr(static_cast<size_t>(index), static_cast<size_t>(count))));
        break;
      }
      case 0x21: {  // 80275014 StringAdd
        AptVar b = pop(), a = pop();
        push(AptVar::Str(a.ToString() + b.ToString()));
        break;
      }
      case 0x29:  // 80275D1C StringLessThan (empty in Apt)
      case 0x31:  // 80275EB0 MBLength (empty)
      case 0x32:  // 80275EB4 CharToAscii (empty)
      case 0x35:  // 80276234 MBSubString (empty)
      case 0x36:  // 80276238 MBCharToAscii (empty)
      case 0x37:  // 8027623C MBAsciiToChar (empty)
        break;
      case 0x33: {  // 80275EB8 AsciiToChar
        const int code = pop().ToInteger();
        push(AptVar::Str(std::string(1, static_cast<char>(code))));
        break;
      }
      case 0x17:  // 802749BC Pop
        pop();
        break;
      case 0x18:  // 80274A60 ToInteger
        push(AptVar::Int(pop().ToInteger()));
        break;
      case 0x30: {  // 80275D20 Random
        const int max = pop().ToInteger();
        push(AptVar::Int(max > 0 ? simmy::ui_rand() % max : 0));
        break;
      }
      case 0x34: {  // 80276138 GetTimer
        // PC: on the game's clock (SIMMY_DET makes it virtual).
        static const unsigned long long start = simmy::game_clock_ns();
        push(AptVar::Int(static_cast<int>((simmy::game_clock_ns() - start) / 1000000ull)));
        break;
      }
      case 0x26: {  // 80275728 Trace
        std::fprintf(stderr, "[apt trace] %s\n", pop().ToString().c_str());
        break;
      }
      // ---- variables
      case 0x1C: {  // 80274BF4 GetVariable
        push(getVariable(pop().ToString()));
        break;
      }
      case 0x1D: {  // 80274D4C SetVariable
        AptVar v = pop();
        setVariable(pop().ToString(), v);
        break;
      }
      case 0x3C: {  // 80276640 DefineLocal
        AptVar v = pop();
        const std::string name = pop().ToString();
        if (c.locals) c.locals->Put(name, v);
        else if (c.target) c.target->SetOwn(*this, name, v);
        break;
      }
      case 0x41: {  // 80276DB0 DefineLocal2
        const std::string name = pop().ToString();
        if (c.locals) {
          if (!c.locals->Find(name)) c.locals->Put(name, AptVar::Undefined());
        } else if (c.target && !c.target->Find(name)) {
          c.target->Put(name, AptVar::Undefined());
        }
        break;
      }
      case 0x3A: {  // 80276240 Delete
        const std::string name = pop().ToString();
        AptObject* o = pop().AsObject();
        push(AptVar::Bool(o && o->Delete(name)));
        break;
      }
      case 0x3B: {  // 80276464 Delete2
        const std::string name = pop().ToString();
        bool done = false;
        for (AptFrameStack* f = c.locals; f && !done; f = f->m_outer) done = f->Delete(name);
        if (!done && c.target) done = c.target->Delete(name);
        push(AptVar::Bool(done));
        break;
      }
      case 0x22: {  // 80275210 GetProperty
        const int index = pop().ToInteger();
        AptVar t = pop();
        AptCIH* clip = t.AsCIH();
        if (!clip) {
          const std::string path = t.ToString();
          clip = path.empty() ? c.target : AptVar::Obj(resolvePath(path, c.target)).AsCIH();
        }
        if (!clip) push(AptVar::Undefined());
        else if (index == 11) push(AptVar::Str(clip->GetTargetPath()));
        else if (index == 13) push(AptVar::Str(clip->m_name));
        else push(AptVar::Number(clip->GetProperty(index)));
        break;
      }
      case 0x23: {  // 80275424 SetProperty
        AptVar v = pop();
        const int index = pop().ToInteger();
        AptVar t = pop();
        AptCIH* clip = t.AsCIH();
        if (!clip) {
          const std::string path = t.ToString();
          clip = path.empty() ? c.target : AptVar::Obj(resolvePath(path, c.target)).AsCIH();
        }
        if (clip) {
          if (index == 13) clip->m_name = v.ToString();
          else clip->SetProperty(index, v.ToFloat());
        }
        break;
      }
      case 0x24: {  // 8027554C CloneSprite (duplicateMovieClip)
        const int depth = pop().ToInteger();
        const std::string name = pop().ToString();
        AptVar src = pop();
        AptCIH* clip = src.AsCIH() ? src.AsCIH() : AptVar::Obj(resolvePath(src.ToString(), c.target)).AsCIH();
        if (clip && clip->m_parent) {
          AptVar dup = callMethod(AptVar::Obj(clip), "duplicateMovieClip", {AptVar::Str(name), AptVar::Int(depth)});
          (void)dup;
        }
        break;
      }
      case 0x25: {  // 80275638 RemoveSprite
        AptVar t = pop();
        AptCIH* clip = t.AsCIH() ? t.AsCIH() : AptVar::Obj(resolvePath(t.ToString(), c.target)).AsCIH();
        if (clip && clip->m_parent) clip->m_parent->removeObject(clip->m_depth);
        break;
      }
      case 0x27:  // 80275978 StartDragMovie (no pointer on the console)
        pop();
        if (pop().ToBool()) PopArgs(*this, 4);
        break;
      case 0x28:  // 80275CB8 StopDragMovie
        break;
      // ---- functions and objects
      case 0x3D:  // 8027677C CallFunction
      case 0x5B:  // 8027F3A4 CallFuncAndPop
      case 0x5C:  // 8027F444 CallFuncSetVar
      case 0xB0:  // 8028016C DictCallFuncPop
      case 0xB1: {  // 80280274 DictCallFuncSetVar
        std::string name;
        if (op >= 0xB0) {
          const unsigned char idx = *c.pc++;
          name = idx < c.pool.size() ? c.pool[idx].ToString() : "";
        } else {
          name = pop().ToString();
        }
        const int nargs = pop().ToInteger();
        std::vector<AptVar> args = PopArgs(*this, nargs);
        AptVar fn = getVariable(name);
        // getContext (8026D958): a plain name's context is the innermost with() object, else the
        // current target; it becomes the called function's this (and so its timeline).
        AptVar thisv = c.target ? AptVar::Obj(c.target) : AptVar::Undefined();
        if (!withEnds.empty() && name.find_first_of("/:.") == std::string::npos)
          thisv = AptVar::Obj(withEnds.back().obj);
        AptVar r;
        if (fn.AsFunction()) r = callFunction(fn, thisv, args);
        else if (s_warnCalls && s_unknownLogged[op] < 20) {
          ++s_unknownLogged[op];
          std::fprintf(stderr, "[apt] call of undefined function %s\n", name.c_str());
        }
        if (op == 0x3D) push(r);
        else if (op == 0x5C || op == 0xB1) {
          push(r);
          AptVar v = pop();
          setVariable(pop().ToString(), v);
        }
        break;
      }
      case 0x52:  // 8027B7F0 CallMethod
      case 0x5D:  // 8027F4A0 CallMethodPop
      case 0x5E:  // 8027F540 CallMethodSetVar
      case 0xB2:  // 80280338 DictCallMethodPop
      case 0xB3: {  // 80280440 DictCallMethodSetVar
        AptVar namev;
        if (op >= 0xB2) {
          const unsigned char idx = *c.pc++;
          namev = idx < c.pool.size() ? c.pool[idx] : AptVar::Undefined();
        } else {
          namev = pop();
        }
        AptVar obj = pop();
        const int nargs = pop().ToInteger();
        std::vector<AptVar> args = PopArgs(*this, nargs);
        AptVar r;
        const std::string name = namev.ToString();
        if (namev.IsUndefined() || name.empty()) {
          // calling the object itself (super(...) or a function value)
          if (c.super && obj.AsObject() == c.super) {
            // super(...): the super constructor on this object, with its own super above it
            AptObject* sp = c.super->Get(*this, "prototype").AsObject();
            AptProperty* sc = sp ? sp->Find("__constructor__") : nullptr;
            AptObject* above = sc ? sc->value.AsObject() : nullptr;
            r = callFunction(obj, c.thisv, args, above);
          } else {
            r = callFunction(obj, AptVar::Undefined(), args);
          }
        } else if (c.super && obj.AsObject() == c.super) {
          // super.method(): the method from the super prototype, with the current this
          AptObject* proto = c.super->Get(*this, "prototype").AsObject();
          AptVar fn = proto ? proto->Get(*this, name) : AptVar::Undefined();
          r = callFunction(fn, c.thisv, args, proto ? SuperFor(*this, proto, name) : nullptr);
        } else {
          AptVar fn = getMember(obj, name);
          if (fn.AsFunction()) {
            AptObject* o = obj.AsObject();
            r = callFunction(fn, obj, args, o ? SuperFor(*this, o->m_proto, name) : nullptr);
          } else if (s_warnCalls && s_unknownLogged[op] < 40) {
            ++s_unknownLogged[op];
            std::fprintf(stderr, "[apt] call of undefined method %s on %s\n", name.c_str(), obj.ToString().c_str());
          }
        }
        if (op == 0x52) push(r);
        else if (op == 0x5E || op == 0xB3) {
          push(r);
          AptVar v = pop();
          setVariable(pop().ToString(), v);
        }
        break;
      }
      case 0x40: {  // 80276BC0 NewObject
        const std::string name = pop().ToString();
        const int nargs = pop().ToInteger();
        std::vector<AptVar> args = PopArgs(*this, nargs);
        push(AptVar::Obj(newObject(getVariable(name), args)));
        break;
      }
      case 0x53: {  // 8027C324 NewMethod
        AptVar namev = pop();
        AptVar obj = pop();
        const int nargs = pop().ToInteger();
        std::vector<AptVar> args = PopArgs(*this, nargs);
        const std::string name = namev.ToString();
        AptVar ctor = name.empty() ? obj : getMember(obj, name);
        push(AptVar::Obj(newObject(ctor, args)));
        break;
      }
      case 0x3E: {  // 802769BC Return
        c.result = pop();
        c.returned = true;
        break;
      }
      case 0x42: {  // 80276F24 InitArray
        const int n = pop().ToInteger();
        AptArray* a = new AptArray;
        AptVar ctor = gpGlobalGlobalObject->Get(*this, "Array");
        if (AptObject* co = ctor.AsObject()) a->m_proto = co->Get(*this, "prototype").AsObject();
        a->m_items = PopArgs(*this, n);
        push(AptVar::Obj(a));
        break;
      }
      case 0x43: {  // 802770D0 InitObject
        const int n = pop().ToInteger();
        AptObject* o = new AptObject;
        AptVar ctor = gpGlobalGlobalObject->Get(*this, "Object");
        if (AptObject* co = ctor.AsObject()) o->m_proto = co->Get(*this, "prototype").AsObject();
        for (int i = 0; i < n; ++i) {
          AptVar v = pop();
          o->Put(pop().ToString(), v);
        }
        push(AptVar::Obj(o));
        break;
      }
      case 0x44:  // 802773E4 TypeOf
        push(AptVar::Str(pop().TypeOf()));
        break;
      case 0x45: {  // 802777DC TargetPath
        AptVar v = pop();
        push(v.AsCIH() ? AptVar::Str(v.AsCIH()->GetTargetPath()) : AptVar::Undefined());
        break;
      }
      case 0x46:    // 80277B34 Enumerate
      case 0x55: {  // 8027C590 Enumerate2
        AptVar v = pop();
        AptObject* o = op == 0x46 ? getVariable(v.ToString()).AsObject() : v.AsObject();
        push(AptVar::Null());  // the end marker (null, as in AVM1)
        if (o) {
          std::vector<std::string> names;
          o->Enumerate(names);
          for (AptObject* p = o->m_proto; p; p = p->m_proto) p->Enumerate(names);
          std::vector<std::string> seen;
          for (const std::string& n : names)
            if (std::find(seen.begin(), seen.end(), n) == seen.end()) {
              seen.push_back(n);
              push(AptVar::Str(n));
            }
        }
        break;
      }
      case 0x47: {  // 80277B5C Add2: string concatenation when either side is a string; else an
                    // integer sum when either side is an integer and neither a float
        AptVar b = pop(), a = pop();
        if (a.IsString() || b.IsString()) push(AptVar::Str(a.ToString() + b.ToString()));
        else if ((a.type == AptVar::kInteger || b.type == AptVar::kInteger) && a.type != AptVar::kFloat &&
                 b.type != AptVar::kFloat)
          push(AptVar::Int(static_cast<int>(static_cast<unsigned int>(a.ToInteger()) + static_cast<unsigned int>(b.ToInteger()))));
        else push(AptVar::Float(a.ToFloat() + b.ToFloat()));
        break;
      }
      case 0x48: {  // 80278310 LessThan2
        AptVar b = pop(), a = pop();
        const int r = Less(a, b);
        push(r < 0 ? AptVar::Undefined() : AptVar::Bool(r == 1));
        break;
      }
      case 0x67: {  // 8027D43C Greater
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(Greater(a, b)));
        break;
      }
      case 0x49: {  // 80278E44 Equals2
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(LooseEquals(*this, a, b)));
        break;
      }
      case 0x66: {  // 8027CEB8 StrictEquals
        AptVar b = pop(), a = pop();
        push(AptVar::Bool(StrictEq(a, b)));
        break;
      }
      case 0x4A: {  // 8027A020 ToNumber
        AptVar v = pop();
        // numbers stay; NaN (booleans, null, objects, non-numeric strings) is undefined; else an
        // integer, or a float when the string form has a '.'
        if (v.IsNumber()) push(v);
        else if (v.IsNaNLike()) push(AptVar::Undefined());
        else if (v.ToString().find('.') == std::string::npos) push(AptVar::Int(v.ToInteger()));
        else push(AptVar::Float(v.ToFloat()));
        break;
      }
      case 0x4B:  // 8027A738 ToString
        push(AptVar::Str(pop().ToString()));
        break;
      case 0x4C:  // 8027AAD4 PushDuplicate
        push(m_stack.empty() ? AptVar::Undefined() : top());
        break;
      case 0x4D: {  // 8027AB3C StackSwap
        AptVar b = pop(), a = pop();
        push(b);
        push(a);
        break;
      }
      case 0x4E:    // 8027ABAC GetMember
      case 0xA5:    // 8027FB4C PushStringGetMember
      case 0xAF: {  // 802800D4 StringDictByteGetMember
        AptVar namev;
        if (op == 0xA5) {
          namev = AptVar::Str(reinterpret_cast<const char*>(c.file->AptAt(BE32(arg))));
          c.pc = arg + 4;
        } else if (op == 0xAF) {
          const unsigned char idx = *c.pc++;
          namev = idx < c.pool.size() ? c.pool[idx] : AptVar::Undefined();
        } else {
          namev = pop();
        }
        AptVar obj = pop();
        if (AptArray* a = obj.AsObject() && obj.p->m_kind == AptValue::kArray ? static_cast<AptArray*>(obj.p) : nullptr;
            a && namev.IsNumber()) {
          const int i = namev.ToInteger();
          push(i >= 0 && i < static_cast<int>(a->m_items.size()) ? a->m_items[static_cast<size_t>(i)] : AptVar::Undefined());
        } else {
          push(getMember(obj, namev.ToString()));
        }
        break;
      }
      case 0x4F:    // 8027AFD0 SetMember
      case 0xA7: {  // 8027FE7C PushStringSetMember: pushes the string as the value, then SetMember
        AptVar v;
        if (op == 0xA7) {
          const unsigned char* str = c.file->AptAt(BE32(arg));
          v = AptVar::Str(str ? reinterpret_cast<const char*>(str) : "");
          c.pc = arg + 4;
        } else {
          v = pop();
        }
        AptVar namev = pop();
        AptVar obj = pop();
        if (AptArray* a = obj.AsObject() && obj.p->m_kind == AptValue::kArray ? static_cast<AptArray*>(obj.p) : nullptr;
            a && namev.IsNumber()) {
          const int i = namev.ToInteger();
          if (i >= 0 && i < 100000) {
            if (i >= static_cast<int>(a->m_items.size())) a->m_items.resize(static_cast<size_t>(i) + 1);
            a->m_items[static_cast<size_t>(i)] = v;
          }
        } else {
          setMember(obj, namev.ToString(), v);
        }
        break;
      }
      case 0x50:  // 8027B338 Increment
      case 0x51: {  // 8027B594 Decrement
        AptVar v = pop();
        const int d = op == 0x50 ? 1 : -1;
        if (v.type == AptVar::kInteger) push(AptVar::Int(v.i + d));
        else push(AptVar::Number(static_cast<double>(v.ToFloat()) + d));
        break;
      }
      case 0x54: {  // 80280FBC InstanceOf
        AptObject* ctor = pop().AsObject();
        AptVar v = pop();
        push(AptVar::Bool(instanceOf(v, ctor)));
        break;
      }
      case 0x2B: {  // 802811EC CastOp
        AptVar v = pop();
        AptObject* ctor = pop().AsObject();
        push(instanceOf(v, ctor) ? v : AptVar::Null());
        break;
      }
      case 0x2C: {  // 80281424 ImplementsOp
        AptObject* ctor = pop().AsObject();
        const int n = pop().ToInteger();
        std::vector<AptVar> ifaces = PopArgs(*this, n);
        if (ctor)
          if (AptObject* proto = ctor->Get(*this, "prototype").AsObject())
            for (const AptVar& i : ifaces)
              if (AptObject* o = i.AsObject()) proto->m_interfaces.push_back(o);
        break;
      }
      case 0x69: {  // 80280A5C Extends
        AptObject* superCtor = pop().AsObject();
        AptObject* sub = pop().AsObject();
        if (sub && superCtor) {
          AptObject* proto = new AptObject;
          proto->m_proto = superCtor->Get(*this, "prototype").AsObject();
          proto->Put("__constructor__", AptVar::Obj(superCtor), 1);
          sub->Put("prototype", AptVar::Obj(proto), 1);
        }
        break;
      }
      case 0x2A: {  // 80281B14 Throw
        std::fprintf(stderr, "[apt] throw %s\n", pop().ToString().c_str());
        c.returned = true;
        break;
      }
      case 0x8F: {  // 802817C8 Try: run the try block; catch/finally are skipped unless thrown (unsupported)
        const unsigned int trySize = BE32(arg), catchSize = BE32(arg + 4), finallySize = BE32(arg + 8);
        (void)trySize;
        (void)catchSize;
        (void)finallySize;
        c.pc = arg + 20;
        break;
      }
      case 0x94: {  // 8027DD30 With
        const int size = static_cast<int>(BE32(arg));
        c.pc = arg + 4;
        if (AptObject* o = pop().AsObject()) {
          c.with.push_back(o);
          withEnds.push_back({o, c.pc + size});
        } else {
          c.pc += size;
        }
        break;
      }
      // ---- pushes
      case 0x96: {  // 8027DE44 Push
        const unsigned int count = BE32(arg), list = BE32(arg + 4);
        c.pc = arg + 8;
        const unsigned char* l = c.file->AptAt(list);
        for (unsigned int k = 0; k < count && l; ++k) push(ConstValue(c.file, BE32(l + 4 * k), c.pool, c.registers));
        break;
      }
      case 0x88: {  // 8027DAAC DefineDictionary
        const unsigned int count = BE32(arg), list = BE32(arg + 4);
        c.pc = arg + 8;
        c.pool.clear();
        const unsigned char* l = c.file->AptAt(list);
        for (unsigned int k = 0; k < count && l; ++k) c.pool.push_back(ConstValue(c.file, BE32(l + 4 * k), {}, {}));
        break;
      }
      case 0x87: {  // 8027DA60 StoreRegister
        const unsigned int reg = BE32(arg);
        c.pc = arg + 4;
        if (reg >= c.registers.size()) c.registers.resize(reg + 1);
        c.registers[reg] = m_stack.empty() ? AptVar::Undefined() : top();
        break;
      }
      case 0x56:  // 8027EC64 PushThis
        push(c.thisv);
        break;
      case 0x58:  // 8027EDA8 PushGlobal
      case 0x71:  // 8027F62C PushGlobalVariable
        push(AptVar::Obj(gpGlobalGlobalObject));
        break;
      case 0x70:  // 8027F59C PushThisVariable
        push(getVariable("this"));
        break;
      case 0x59: push(AptVar::Int(0)); break;  // 8027EEEC Push0
      case 0x5A: push(AptVar::Int(1)); break;  // 8027EFE8 Push1
      case 0x73: push(AptVar::Bool(true)); break;   // 8027F0E4 PushTrue
      case 0x74: push(AptVar::Bool(false)); break;  // 8027F1E0 PushFalse
      case 0x75: push(AptVar::Null()); break;       // 8027F2DC PushNULL
      case 0x76: push(AptVar::Undefined()); break;  // 8027F340 PushUndefined
      case 0x72: {  // 8027F690 PushZeroSetVar
        const std::string name = pop().ToString();
        setVariable(name, AptVar::Int(0));
        break;
      }
      case 0xA1: {  // 8027F79C PushString
        push(AptVar::Str(reinterpret_cast<const char*>(c.file->AptAt(BE32(arg)) ? c.file->AptAt(BE32(arg)) : reinterpret_cast<const unsigned char*>(""))));
        c.pc = arg + 4;
        break;
      }
      case 0xA2: {  // 8027F924 PushStringDictByte
        const unsigned char idx = *c.pc++;
        push(idx < c.pool.size() ? c.pool[idx] : AptVar::Undefined());
        break;
      }
      case 0xA3: {  // 8027F9A4 PushStringDictWord
        const unsigned int idx = static_cast<unsigned int>(c.pc[0] << 8 | c.pc[1]);
        c.pc += 2;
        push(idx < c.pool.size() ? c.pool[idx] : AptVar::Undefined());
        break;
      }
      case 0xA4: {  // 8027FA30 PushStringGetVar
        const char* name = reinterpret_cast<const char*>(c.file->AptAt(BE32(arg)));
        c.pc = arg + 4;
        push(getVariable(name ? name : ""));
        break;
      }
      case 0xA6: {  // 8027FCE4 PushStringSetVar: pushes the string as the value, then SetVariable
        const char* str = reinterpret_cast<const char*>(c.file->AptAt(BE32(arg)));
        c.pc = arg + 4;
        const std::string name = pop().ToString();
        setVariable(name, AptVar::Str(str ? str : ""));
        break;
      }
      case 0xAE: {  // 80280014 StringDictByteGetVar
        const unsigned char idx = *c.pc++;
        push(getVariable(idx < c.pool.size() ? c.pool[idx].ToString() : ""));
        break;
      }
      case 0xB4: {  // 80280504 PushFloat (unaligned big-endian)
        const unsigned int u = static_cast<unsigned int>(c.pc[0]) << 24 | static_cast<unsigned int>(c.pc[1]) << 16 |
                               static_cast<unsigned int>(c.pc[2]) << 8 | c.pc[3];
        c.pc += 4;
        float f;
        std::memcpy(&f, &u, 4);
        push(AptVar::Float(f));
        break;
      }
      case 0xB5:  // 8028062C PushByte
        push(AptVar::Int(static_cast<signed char>(*c.pc++)));
        break;
      case 0xB6: {  // 80280738 PushWord
        push(AptVar::Int(static_cast<short>(c.pc[0] << 8 | c.pc[1])));
        c.pc += 2;
        break;
      }
      case 0xB7: {  // 80280850 PushDWord
        const unsigned int u = static_cast<unsigned int>(c.pc[0]) << 24 | static_cast<unsigned int>(c.pc[1]) << 16 |
                               static_cast<unsigned int>(c.pc[2]) << 8 | c.pc[3];
        c.pc += 4;
        push(AptVar::Int(static_cast<int>(u)));
        break;
      }
      // ---- branches
      case 0x99: {  // 8027EC04 BranchAlways
        const int rel = static_cast<int>(BE32(arg));
        c.pc = arg + 4 + rel;
        break;
      }
      case 0x9D:    // 8027E6B4 BranchIfTrue
      case 0xB8: {  // 80280978 BranchIfFalse
        const int rel = static_cast<int>(BE32(arg));
        c.pc = arg + 4;
        const bool cond = pop().ToBool();
        if (cond == (op == 0x9D)) c.pc += rel;
        break;
      }
      // ---- bit operations
      case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: {  // 8027C5B8.. BitAnd/Or/Xor/LShift/RShift
        const int b = pop().ToInteger(), a = pop().ToInteger();
        int r = 0;
        switch (op) {
          case 0x60: r = a & b; break;
          case 0x61: r = a | b; break;
          case 0x62: r = a ^ b; break;
          case 0x63: r = static_cast<int>(static_cast<unsigned int>(a) << (b & 31)); break;
          default: r = a >> (b & 31); break;
        }
        push(AptVar::Int(r));
        break;
      }
      case 0x65:  // 8027CEB4 BitURShift (empty in Apt)
        break;
      // ---- functions
      case 0x9B:    // 8027E3D4 DefineFunction
      case 0x8E: {  // 8027E580 DefineFunction2
        auto* f = new AptScriptFunctionBase;
        f->m_file = c.file;
        f->m_target = c.target;
        f->m_scope = c.locals;
        f->m_withScope = c.with;
        f->m_constantPool = c.pool;
        f->m_name = reinterpret_cast<const char*>(c.file->AptAt(BE32(arg)) ? c.file->AptAt(BE32(arg)) : reinterpret_cast<const unsigned char*>(""));
        const unsigned int argc = BE32(arg + 4);
        if (op == 0x9B) {
          f->m_version = 1;
          const unsigned char* args = c.file->AptAt(BE32(arg + 8));
          for (unsigned int k = 0; k < argc && args; ++k)
            f->m_args.emplace_back(0, reinterpret_cast<const char*>(c.file->AptAt(BE32(args + 4 * k))));
          f->m_bodySize = static_cast<int>(BE32(arg + 12));
          c.pc = arg + 24;
        } else {
          f->m_version = 2;
          f->m_registerCount = arg[9];
          f->m_flags = static_cast<unsigned int>(arg[10] << 8 | arg[11]);
          const unsigned char* args = c.file->AptAt(BE32(arg + 12));
          for (unsigned int k = 0; k < argc && args; ++k)
            f->m_args.emplace_back(static_cast<int>(BE32(args + 8 * k)),
                                   reinterpret_cast<const char*>(c.file->AptAt(BE32(args + 8 * k + 4))));
          f->m_bodySize = static_cast<int>(BE32(arg + 16));
          c.pc = arg + 28;
        }
        f->m_body = c.pc;
        if (s_traceOps) {
          std::fprintf(stderr, "[apt def] %s body %05x (", f->m_name.c_str(), static_cast<unsigned>(c.pc - base));
          for (const auto& a : f->m_args) std::fprintf(stderr, "%d:%s ", a.first, a.second.c_str());
          std::fprintf(stderr, ")\n");
        }
        c.pc += f->m_bodySize;
        // A function value gets a prototype object whose constructor is the function.
        AptObject* proto = new AptObject;
        AptVar objCtor = gpGlobalGlobalObject->Get(*this, "Object");
        if (AptObject* oc = objCtor.AsObject()) proto->m_proto = oc->Get(*this, "prototype").AsObject();
        proto->Put("constructor", AptVar::Obj(f), 1);
        f->Put("prototype", AptVar::Obj(proto), 1);
        AptVar fnCtor = gpGlobalGlobalObject->Get(*this, "Function");
        if (AptObject* fc = fnCtor.AsObject()) f->m_proto = fc->Get(*this, "prototype").AsObject();
        if (f->m_name.empty()) push(AptVar::Obj(f));
        else setVariable(f->m_name, AptVar::Obj(f));
        break;
      }
      case 0x83: {  // 8027D8C0 GetUrl
        const char* url = reinterpret_cast<const char*>(c.file->AptAt(BE32(arg)));
        const char* target = reinterpret_cast<const char*>(c.file->AptAt(BE32(arg + 4)));
        c.pc = arg + 8;
        extern void AptGetURL(const std::string& url, const std::string& target, AptCIH* clip);
        AptGetURL(url ? url : "", target ? target : "", c.target);
        break;
      }
      case 0x9A: {  // 8027DF4C GetUrl2
        const std::string target = pop().ToString();
        const std::string url = pop().ToString();
        extern void AptGetURL(const std::string& url, const std::string& target, AptCIH* clip);
        AptGetURL(url, target, c.target);
        break;
      }
      case 0x8A:  // 8027DAD4 WaitForFrame (empty)
        break;
      default:
        if (s_unknownLogged[op]++ == 0) std::fprintf(stderr, "[apt] unhandled opcode 0x%02X\n", op);
        break;
    }
  }
  --m_depth;
  m_contexts.pop_back();
  m_ctx = saved;
}
