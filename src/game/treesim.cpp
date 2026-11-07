// Port of game/behavior.cpp (Behavior, with ResourceBehavior's tree sets), game/treesim.cpp (TreeStack,
// TreeSimImpl) and game/motive.cpp (Motives, MotiveConstantsClient): the behaviour VM's core and the
// motive decay. See treesim.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {
const unsigned char* GlobalFileRow() {
  if (!Globs_pObjectFolder || !Globs_pObjectFolder->m_objects) return nullptr;
  const ERQuickdata* qd = Globs_pObjectFolder->m_objects;
  const void* globals = qd->getTable("GlobalResFile");
  const void* row = globals ? qd->getRow(globals, "global") : nullptr;
  return row ? static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 0)) : nullptr;
}

EREdithTreeSet* AddRefTreeSet(const unsigned char* resFile) {
  const unsigned int id = resFile ? ERQuickdata::U32(resFile, 0xc) : 0;
  return id ? _edithtreesetman.AddRef(id, nullptr, 0) : nullptr;
}

std::map<ObjSelector*, Behavior*>& Behaviors() {
  static std::map<ObjSelector*, Behavior*> map;
  return map;
}
}  // namespace

// A selector's object file: its "ResFile" row (GC: ObjSelector +0x54; a user selector's is the template
// person's, by its row index +0x04).
const unsigned char* SelectorResFile(ObjSelector* selector) {
  if (!selector) return nullptr;
  if (selector->m_resFile) return static_cast<const unsigned char*>(selector->m_resFile);
  if (selector->m_row < 0 || !Globs_pObjectFolder || !Globs_pObjectFolder->m_objects) return nullptr;
  const void* table = Globs_pObjectFolder->m_objects->getTable("ResFile");
  return table && static_cast<unsigned int>(selector->m_row) < ERQuickdata::TableRowCount(table)
             ? static_cast<const unsigned char*>(ERQuickdata::TableRow(table, static_cast<unsigned int>(selector->m_row)))
             : nullptr;
}

// The ResFile row whose tree table (+0x24) is the selector's: its own, else its semi-global file's
// (ResFile +0x34; people's interactions live in person_sg). GC: ObjectFolderImpl::GetTreeTable
// (800F17A8) looks the TTAB id up in the object's namespace, then in its semi-global one.
const unsigned char* SelectorTreeTabResFile(ObjSelector* selector) {
  const unsigned char* file = SelectorResFile(selector);
  if (!file) return nullptr;
  const auto* tables = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24));
  if (tables && ERQuickdata::U32(tables - 4, 0) != 0) return file;
  const auto* semi = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x34));
  const auto* semiTables = semi ? static_cast<const unsigned char*>(ERQuickdata::Ptr(semi, 0x24)) : nullptr;
  return semiTables && ERQuickdata::U32(semiTables - 4, 0) != 0 ? semi : file;
}

// ---------------------------------------------------------------------------------------------------
// Behavior

// 800AB300
Behavior::Behavior() {}

// 800AB314
// GC: also the shared tree cache's reference count.
Behavior::~Behavior() {
  for (EREdithTreeSet* set : {m_private, m_semiGlobal, m_global})
    if (set) set->DelRef();
}

// 800AD118 Behavior::GetOwner (inline in the header)

Behavior* Behavior::For(ObjSelector* selector) {
  if (!selector) return nullptr;
  Behavior*& b = Behaviors()[selector];
  if (!b) {
    b = new Behavior;
    b->Init(nullptr, selector);
  }
  return b;
}

// 800AC650
// The owner's trees: its object file's tree set (ResFile +0x0C), its semi-global file's (+0x34) and the
// global file's, as ResourceBehavior::Init (800AC90C) adds them.
void Behavior::Init(Language* language, ObjSelector* owner) {
  m_language = language;
  m_owner = owner;
  m_resFile = SelectorResFile(owner);
  m_sgResFile = m_resFile ? static_cast<const unsigned char*>(ERQuickdata::Ptr(m_resFile, 0x34)) : nullptr;
  m_globalResFile = GlobalFileRow();
  m_private = AddRefTreeSet(m_resFile);
  m_semiGlobal = AddRefTreeSet(m_sgResFile);
  m_global = AddRefTreeSet(m_globalResFile);
}

// 800AB968
const unsigned char* Behavior::GetResFileID(short id) {
  if (static_cast<unsigned int>(id - 0x100) >= 0x2229) return nullptr;
  if (id < 0x1000) return m_globalResFile;
  if (id < 0x2000) return m_resFile;
  if (id < 0x2329) return m_sgResFile;
  return nullptr;
}

// 800AB9B4
int Behavior::GetBaseID(short treeClass) {
  switch (treeClass) {
    case 1: return 0x100;
    case 2: return 0x1000;
    case 3: return 0x2000;
    default: return 0;
  }
}

// 800AB9F4
int Behavior::GetMaxID(short treeClass) {
  switch (treeClass) {
    case 1: return 0xfff;
    case 2: return 0x1fff;
    case 3: return 9000;
    default: return 0;
  }
}

// 800ABA34
int Behavior::GetTreeClass(short id) {
  const unsigned int u = static_cast<unsigned short>(id);
  if (u < 0x100) return 0;
  if (u - 0x100 < 0xf00) return 1;
  if (u - 0x2000 <= 0x328) return 3;
  return u - 0x1000 <= 0xfff ? 2 : 0;
}

EREdithTreeSet* Behavior::TreeSetFor(short id) {
  switch (GetTreeClass(id)) {
    case 1: return m_global;
    case 2: return m_private;
    case 3: return m_semiGlobal;
    default: return nullptr;
  }
}

// 800AC384
const unsigned char* Behavior::GetTree(short id) {
  EREdithTreeSet* set = TreeSetFor(id);
  return set ? set->GetBehaviorTree(static_cast<unsigned short>(id)) : nullptr;
}

// 800AC4A4
const char* Behavior::GetTreeName(short id) {
  if (EREdithTreeSet* set = TreeSetFor(id))
    for (const EREdithTreeSet::Tree& t : set->m_trees)
      if (t.m_id == static_cast<unsigned short>(id)) return t.m_name;
  return "";
}

// 800AB5BC
// GC: through a three-tree most-recently-used cache of ResourceBehaviorTrees.
int Behavior::GetNodeRef(short tree, short node, const unsigned char** out) {
  if (!out) return 0;
  *out = nullptr;
  const unsigned char* t = GetTree(tree);
  if (!t || node < 0 || node >= bhav::NumNodes(t)) return 0;
  *out = bhav::Node(t, node);
  return 1;
}

// 800AB560
int Behavior::GetNode(short tree, short node, const unsigned char** out) { return GetNodeRef(tree, node, out); }

// 800AC5A8
// The object file's 'BCON' list (ResFile +0x20: 0x10-byte entries {id, values (8 bytes each: the
// short, padding, a label; count at [-4])},
// sorted by id; QuickDataBehaviorConstants::LoadFromDataID 80159FBC).
bool Behavior::GetConstants(short id, const unsigned char** values, int* count) {
  *values = nullptr;
  *count = 0;
  const unsigned char* file = GetResFileID(id);
  const auto* list = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x20)) : nullptr;
  if (!list) return false;
  const unsigned int n = ERQuickdata::U32(list - 4, 0);
  for (unsigned int i = 0; i < n; ++i) {
    const unsigned char* e = list + i * 0x10;
    if (ERQuickdata::S32(e, 0) != id) continue;
    const auto* v = static_cast<const unsigned char*>(ERQuickdata::Ptr(e, 4));
    *values = v;
    *count = v ? static_cast<int>(ERQuickdata::U32(v - 4, 0)) : 0;
    return v != nullptr;
  }
  return false;
}

// ---------------------------------------------------------------------------------------------------
// StackElem, TreeStack

// 80148B7C StackElem::GetParams (inline in the header)
// 80148B84 StackElem::GetLocals (inline)
// 80149118 StackElem::GetTreeID (inline)
// 80149138 StackElem::SetTreeID (inline)

// 80148BE4
void StackElem::Setup(const StackElem& from, const short* params) {
  m_treeId = from.m_treeId;
  m_node = from.m_node;
  m_stackObject = from.m_stackObject;
  m_numLocals = from.m_numLocals;
  m_numParams = from.m_numParams;
  m_primState = from.m_primState;
  m_behavior = from.m_behavior;
  m_locals.assign(m_numLocals, 0);
  for (short& p : m_params) p = 0;
  if (params)
    for (int i = 0; i < m_numParams && i < 8; ++i) m_params[i] = params[i];
}

// 80148B98 StackElem::GetSize (inline in the header)

// 801475A8
void StackElem::ReconStream(ReconBuffer* b, int version, ObjectFolderImpl* finder) {
  if (version > 0x19) b->Recon16(&m_stackObject, 1);
  short tree = static_cast<short>(GetTreeID());
  b->Recon16(&tree, 1);
  SetTreeID(tree);
  b->Recon16(&m_node, 1);
  b->Recon8(reinterpret_cast<signed char*>(&m_numLocals), 1);
  b->Recon8(reinterpret_cast<signed char*>(&m_numParams), 1);
  // PC: the params live in a fixed 8 (GC: in the frame, as many as it has).
  if (m_numParams <= 8) {
    b->Recon16(m_params, m_numParams);
  } else {
    std::vector<short> params(m_params, m_params + 8);
    params.resize(m_numParams, 0);
    b->Recon16(params.data(), m_numParams);
    for (int i = 0; i < 8; ++i) m_params[i] = params[static_cast<size_t>(i)];
  }
  if (b->m_mode == ReconBuffer__Mode::kRead) m_locals.assign(m_numLocals, 0);
  if (m_numLocals) b->Recon16(m_locals.data(), m_numLocals);
  b->Recon32(&m_primState, 1);
  if (finder) finder->ReconBehavior(&m_behavior, b, version);
}

// 80148CCC
int TreeStack::GetMemUsed() {
  int used = 0;
  for (const StackElem& e : m_frames) used += e.GetSize();
  return used;
}

// 80148D28
int TreeStack::GetMemReserved() { return GetMemUsed(); }

// 8014881C
// Read: the frames replace the stack (GC: a buffer of the saved size is allocated first; then the last
// frame's NextFrame, which has no effect). Versions 0xB..0x18 kept {tree, node, stack object, state, the
// object's ..., four params} per frame and pushed them (4 params, no locals).
void TreeStack::ReconStream(ReconBuffer* b, int version, ObjectFolderImpl* finder) {
  if (version <= 10) return;
  const bool reading = b->m_mode == ReconBuffer__Mode::kRead;
  if (version < 0x19) {
    short count = static_cast<short>(GetStackSize());
    b->Recon16(&count, 1);
    if (!reading) {
      SIMMY_STUB("TreeStack::ReconStream: writing saves before version 0x19", 0x8014881C);
      return;
    }
    for (int i = 0; i < count; ++i) {
      StackElem e;
      short tree = 0, unused16 = 0;
      int state = 0;
      b->Recon16(&tree, 1);
      e.SetTreeID(tree);
      b->Recon16(&e.m_node, 1);
      b->Recon16(&e.m_stackObject, 1);
      b->Recon32(&state, 1);
      b->Recon16(&unused16, 1);
      short params[4] = {};
      b->Recon16(params, 4);
      if (finder) finder->ReconBehavior(&e.m_behavior, b, version);
      e.m_primState = state;
      e.m_numLocals = 0;
      e.m_numParams = 4;
      Push(e, params);
    }
    return;
  }
  int count = GetStackSize();
  b->Recon32(&count, 1);
  int reserved = reading ? 0 : GetMemReserved();
  b->Recon32(&reserved, 1);
  if (reading) m_frames.clear();
  for (int i = 0; i < count; ++i) {
    if (reading) m_frames.emplace_back();
    GetNthFrame(i)->ReconStream(b, version, finder);
  }
}

// 80147164 TreeStack::TreeStack (inline)
// 80147198 TreeStack::~TreeStack (inline)
// 801490C4 TreeStack::Reset (inline)
// 80149104 TreeStack::GetStackSize (inline)

// 80148EC8
void TreeStack::Push(const StackElem& frame, const short* params) {
  m_frames.emplace_back();
  m_frames.back().Setup(frame, params);
}

// 80149048
void TreeStack::Pop() {
  if (m_frames.empty()) {
    m_owner->SetError(0x3e9);
    if (m_owner->m_object) m_owner->m_object->Error(0x3e9);
    return;
  }
  m_frames.pop_back();
}

// 801490D0
StackElem* TreeStack::GetNthFrame(int n) {
  return n >= 0 && n < static_cast<int>(m_frames.size()) ? &m_frames[static_cast<size_t>(n)] : nullptr;
}

// ---------------------------------------------------------------------------------------------------
// TreeSimImpl

int TreeSimImpl::sMaxIterations = 25000;   // TreeSim::sMaxIterations
int TreeSimImpl::sInMainSim = 1;           // TreeSim::sInMainSim

// 8014772C
TreeSimImpl::TreeSimImpl(cXObjectImpl* object) : m_object(object), m_stack(*this) {}

// 80147258
TreeSimImpl::~TreeSimImpl() {}

// 80147434 TreeSimImpl::SetError (inline)
// 8014743C TreeSimImpl::GetError (inline)
// 80147444 TreeSimImpl::ClearError (inline)
// 8014853C TreeSimImpl::GetLastTransition (inline)
// 801486CC TreeSimImpl::GetCurElem (inline)
// 801487CC TreeSimImpl::GetNthElem (inline)
// 801487F0 TreeSimImpl::GetStackSize (inline)
// 80148818 TreeSimImpl::StackJustPopped (inline)
// 80149294 TreeSimImpl::GetIterations (inline)
// 8014929C TreeSimImpl::GetLastResult (inline)
// 801492A4 TreeSimImpl::HasMainTreeSim (inline)
// 801492B4 TreeSimImpl::SetNoMainTreeSim (inline)

// 80147928
void TreeSimImpl::Initialize(int, short* defaultParams) { m_defaultParams = defaultParams; }

// 80147960
void TreeSimImpl::GetCurrentNode(short* tree, short* node) {
  StackElem* e = GetCurElem();
  *tree = e ? static_cast<short>(e->GetTreeID()) : -1;
  *node = e ? e->m_node : -1;
}

// 801479BC
int TreeSimImpl::GetCurrentPrimitive() {
  StackElem* e = GetCurElem();
  const unsigned char* node = nullptr;
  if (!e || !e->m_behavior || !e->m_behavior->GetNode(static_cast<short>(e->GetTreeID()), e->m_node, &node)) return -1;
  return bhav::Opcode(node);
}

// 80147A34
void TreeSimImpl::Reset(Behavior* behavior, short tree) {
  m_stack.Reset();
  if (tree != 0) {
    const short params[4] = {0, 0, 0, 0};
    Gosub(behavior, params, tree);
    m_error = 0;
  }
}

// 80147AA0
// A frame for `tree` on top of the stack: its stack object is the caller's, its params `params`
// (null: the default ones). A tree of type 4 starts at the node its first param names.
int TreeSimImpl::Gosub(Behavior* behavior, const short* params, short tree) {
  StackElem* caller = m_stack.GetNthFrame(m_stack.GetStackSize() - 1);
  if (!behavior && caller) behavior = caller->m_behavior;
  if (!behavior) return 0;
  const unsigned char* t = behavior->GetTree(tree);
  if (!t) return 0;
  StackElem frame;
  frame.m_primState = 0;
  frame.m_behavior = behavior;
  frame.SetTreeID(tree);
  frame.m_stackObject = caller ? caller->m_stackObject : 0;
  if (!params) params = m_defaultParams;
  if (bhav::Type(t) == 4) {
    frame.m_node = params ? params[0] : 0;
    if (frame.m_node >= bhav::NumNodes(t)) frame.m_node = static_cast<short>(bhav::NumNodes(t) - 1);
  } else {
    frame.m_node = 0;
  }
  if (frame.m_node < 0 || frame.m_node >= bhav::NumNodes(t)) return 0;
  // PC development aid: SIMMY_TUTORIAL_FROM=<tree hex> starts the tutorial's level tree ("Tutorial L01",
  // 0x1001) at its node calling that tree (e.g. 1009 "Meet a sim"), skipping the stages before it.
  static const char* from = std::getenv("SIMMY_TUTORIAL_FROM");
  if (from && tree == 0x1001 && behavior->GetOwner() && behavior->GetOwner()->m_fileName &&
      std::strcmp(behavior->GetOwner()->m_fileName, "tutorial") == 0) {
    const int target = static_cast<int>(std::strtol(from, nullptr, 16));
    for (int i = 0; i < bhav::NumNodes(t); ++i)
      if (bhav::Opcode(bhav::Node(t, i)) == target) {
        std::printf("[story] SIMMY_TUTORIAL_FROM: tutorial L01 starts at node %d (tree %04x)\n", i, target);
        frame.m_node = static_cast<short>(i);
        break;
      }
  }
  frame.m_numLocals = static_cast<unsigned char>(bhav::NumLocals(t));
  frame.m_numParams = static_cast<unsigned char>(bhav::NumParams(t));
  m_stack.Push(frame, params);
  return 1;
}

// 80147C70
// On a marker frame (tree -1, waiting) so the check tree's return stops there; run outside the main
// sim (TreeSim::sInMainSim 0), counting its own iterations. A tree that does not finish in one go is an
// error (0x44F). The result: the last transition was "return true".
int TreeSimImpl::RunCheckTree(Behavior* behavior, short tree, short stackObject, const short* params) {
  if (m_error != 0) return 0;
  StackElem marker;
  marker.m_primState = 1;
  marker.SetTreeID(-1);
  marker.m_node = 0;
  marker.m_numLocals = 0;
  marker.m_numParams = 0;
  m_stack.Push(marker, nullptr);
  if (!Gosub(behavior, params, tree)) {
    m_stack.Pop();
    StackJustPopped();
    return 0;
  }
  GetCurElem()->m_stackObject = stackObject;
  const int savedInMain = sInMainSim;
  sInMainSim = 0;
  const int savedIterations = m_iterations, saved08 = m_field08;
  if (m_object && m_object->TreeSimError()) m_object->ClearTreeSimError();
  m_iterations = 0;
  m_field08 = 0;
  for (;;) {
    StackElem* e = GetCurElem();
    if (!e || e->GetTreeID() == -1) {
      m_stack.Pop();
      StackJustPopped();
      m_field08 = saved08;
      m_iterations = savedIterations;
      sInMainSim = savedInMain;
      return GetLastTransition() ? 1 : 0;
    }
    const int r = DoNodeAction(e);
    if (r == 1 || r == 2) break;
  }
  sInMainSim = savedInMain;
  m_iterations = savedIterations;
  m_field08 = saved08;
  if (m_error != 0) {
    if (m_object) m_object->HandleError();
    return 0;
  }
  m_error = 0x44f;
  if (m_object) m_object->Error(0x44f);
  return 0;
}

// 80147F58
// GC: through the TreeSim's vtable (0xA4), RunCheckTree.
int TreeSimImpl::RunOneTickTree(Behavior* behavior, short tree, short stackObject, const short* params) {
  return RunCheckTree(behavior, tree, stackObject, params);
}

// 80147FD8
int TreeSimImpl::DoNodeAction(StackElem* frame) {
  int error = 0;
  if (!frame) {
    if (!HasMainTreeSim()) return 1;
    error = 0x44e;
  } else if (!frame->m_behavior) {
    if (m_stack.GetStackSize() > 1) {
      m_stack.Pop();
      StackJustPopped();
      return 0;
    }
    error = 0x3e9;
  } else {
    const int target = static_cast<unsigned short>(frame->m_node);
    if (target < 0xfb) {
      const unsigned char* node = nullptr;
      if (!frame->m_behavior->GetNodeRef(static_cast<short>(frame->GetTreeID()), frame->m_node, &node)) {
        error = 0x44e;
      } else if (++m_iterations >= sMaxIterations) {
        error = 0x44d;
      } else {
        const int op = bhav::Opcode(node);
        if (op > 0xff) {
          // A tree call: the node's operands are its params unless they are all 0xFF.
          bool noParams = simmy::be32(node + 4) == 0xffffffffu && simmy::be32(node + 8) == 0xffffffffu;
          short params[4];
          for (int i = 0; i < 4; ++i) params[i] = static_cast<short>(bhav::Operand16(node, i));
          if (Gosub(nullptr, noParams ? nullptr : params, static_cast<short>(op))) return 0;
          error = frame->m_behavior->GetTree(static_cast<short>(op)) ? 1000 : 0x44e;
          m_error = static_cast<short>(error);
          if (m_object) {
            static int logged = 0;
            if (logged++ < 20)
              std::printf("[vm] %s: tree %04x node %d calls missing tree %04x\n", m_object->DebugName(),
                          frame->GetTreeID(), frame->m_node, op);
            m_object->Error(static_cast<short>(error));
          }
          return 2;
        }
        const int r = m_object ? m_object->TryElement(frame, static_cast<short>(op), node + 4) : -1;
        if (r == 1 || r == 0) {
          const int dest = node[r == 1 ? 2 : 3];
          if (dest == 0xfd) {
            error = 0x44c;
            m_error = static_cast<short>(error);
            if (m_object) m_object->Error(static_cast<short>(error));
            return 2;
          }
          frame->m_node = static_cast<short>(dest);
          m_lastResult = r;
          m_lastTransition = dest;
          frame->m_primState = 0;
          return 0;
        }
        if (r == 3) return 0;
        if (r == 2) return 1;
        return 2;  // -1 (the primitive reported its error) or 4
      }
    } else if (target == 0xfe || target == 0xff) {
      if (m_stack.GetStackSize() < 2) {
        error = 0x3e9;
      } else {
        m_stack.Pop();
        StackJustPopped();
        StackElem* caller = GetCurElem();
        m_lastResult = target == 0xfe ? 1 : 0;
        if (caller->m_primState != 0) return 0;  // the caller's primitive runs again with the result
        const unsigned char* node = nullptr;
        if (!caller->m_behavior->GetNode(static_cast<short>(caller->GetTreeID()), caller->m_node, &node)) {
          error = 0x44e;
        } else {
          const int dest = node[target == 0xfe ? 2 : 3];
          if (dest == 0xfd) {
            error = 0x44c;
          } else {
            caller->m_node = static_cast<short>(dest);
            m_lastTransition = dest;
            return 0;
          }
        }
      }
    } else {
      error = 0x44c;
    }
  }
  m_error = static_cast<short>(error);
  if (m_object) m_object->Error(static_cast<short>(error));
  return 2;
}

// 80148550
int TreeSimImpl::Simulate() {
  m_iterations = 0;
  m_field08 = 0;
  for (;;) {
    const int r = DoNodeAction(GetCurElem());
    if (r == 1) return 1;
    if (r == 2) return 0;
  }
}

// 80148708
// The first frame of the main sim: the one under the lowest check-tree marker (tree -1), else the top.
StackElem* TreeSimImpl::GetMainSimElem() {
  const int top = m_stack.GetStackSize() - 1;
  int n = top;
  for (int i = 0; i <= top; ++i)
    if (m_stack.GetNthFrame(i)->GetTreeID() == -1) {
      n = i - 1;
      break;
    }
  if (n == -1) return nullptr;
  return m_stack.GetNthFrame(n);
}

// ---------------------------------------------------------------------------------------------------
// Motives (game/motive.cpp)

namespace {
// The constants (MotiveConstantsClient::UpdateConstants; PC: FloatConstants is not ported, so its
// defaults are used, which the code carries for every value).
int sConstantsUpdated = 0;
float totalEnergyShift = 0, clockDrift = 0, hungerToBladderRatio = 0, hungerDecRatio = 0, socialDecBase = 0,
      socialDecMultiplier = 0, entWakeDec = 0, entSleepMultiplier = 0, hygWakeDec = 0, hygSleepDec = 0,
      bladderWakeDec = 0, bladderSleepDec = 0, sleepinessMultiplier = 0, interestInc = 0, aspirationDec = 0,
      comfortDecBase = 0, comfortDecMultiplier = 0, sleepHours = 0, sleepEnergyDelta = 0, wakeEnergyDelta = 0;
int wakeHours = 0, wakeTime = 0, bedTime = 0;
MotiveConstantsClient sTheClient;
}  // namespace

float kAspirationPointToMeterRatio = 1.0f;  // 804FDC40 (the wants' rewards use it)
float redAspMotiveDecayMult = 0, greenAspMotiveDecayMult = 0, goldAspMotiveDecayMult = 0,
      platinumAspMotiveDecayMult = 0, redAspMotiveSatisfyMult = 0, greenAspMotiveSatisfyMult = 0,
      goldAspMotiveSatisfyMult = 0, platinumAspMotiveSatisfyMult = 0, redAspMotiveCostMult = 0,
      greenAspMotiveCostMult = 0, goldAspMotiveCostMult = 0, platinumAspMotiveCostMult = 0;

// 800C2D1C
MotiveConstantsClient::MotiveConstantsClient() {}

// 800C2010
// The motive tuning: group 1 (MotiveConstantsClient: GlobalConstantsClient(1)) of the global object
// file's float constants (FloatConstants::Load / Get, with the code's defaults).
void MotiveConstantsClient::UpdateConstants() {
  auto get = [](const char* name, float def) { return GlobalFloatConstant(1, name, def); };
  totalEnergyShift = get("energy span", 180.0f);
  wakeHours = static_cast<int>(get("wake hours", 16));
  wakeTime = static_cast<int>(get("wake hour", 7));
  clockDrift = get("energy drift", 0.01f);
  hungerToBladderRatio = get("hunger to bladder multiplier", 0.25f);
  const double comfortActive = get("comfort decrement active", 0.4);
  const double comfortLazy = get("comfort decrement lazy", 0.6);
  hungerDecRatio = get("hunger decrement ratio", 0.001428571f);
  socialDecBase = get("social decrement base", 0.1f);
  socialDecMultiplier = get("social decrement multiplier", 0.0004f);
  entWakeDec = get("ent decrement, awake", 0.25f);
  entSleepMultiplier = get("ent multiplier, asleep", 0.5f);
  hygWakeDec = get("hyg decrement, awake", 0.15f);
  hygSleepDec = get("hyg decrement, asleep", 0.1f);
  bladderWakeDec = get("bladder decrement, awake", 0.3f);
  bladderSleepDec = get("bladder decrement, asleep", 0.2f);
  sleepinessMultiplier = get("sleepiness multiplier", 0.001f);
  interestInc = get("interest increment", 0.7f);
  aspirationDec = get("aspiration decrement", 0.1f);
  redAspMotiveDecayMult = get("red aspiration motive decay multiplier", 1.0f);
  greenAspMotiveDecayMult = get("green aspiration motive decay multiplier", 1.0f);
  goldAspMotiveDecayMult = get("gold aspiration motive decay multiplier", 0.75f);
  platinumAspMotiveDecayMult = get("platinum aspiration motive decay multiplier", 0.25f);
  redAspMotiveSatisfyMult = get("red aspiration motive satisfaction multiplier", 0.75f);
  greenAspMotiveSatisfyMult = get("green aspiration motive satisfaction multiplier", 1.0f);
  goldAspMotiveSatisfyMult = get("gold aspiration motive satisfaction multiplier", 1.25f);
  platinumAspMotiveSatisfyMult = get("platinum aspiration motive satisfaction multiplier", 1.5f);
  redAspMotiveCostMult = get("red aspiration motive cost multiplier", 1.25f);
  greenAspMotiveCostMult = get("green aspiration motive cost multiplier", 1.0f);
  goldAspMotiveCostMult = get("gold aspiration motive cost multiplier", 1.0f);
  platinumAspMotiveCostMult = get("platinum aspiration motive cost multiplier", 0.5f);
  kAspirationPointToMeterRatio = get("aspiration points per meter point", 1.0f);
  sleepHours = static_cast<float>(24 - wakeHours);
  bedTime = wakeTime + wakeHours;
  sConstantsUpdated = 1;
  comfortDecBase = static_cast<float>(comfortLazy);
  sleepEnergyDelta = totalEnergyShift / sleepHours / 30.0f;
  comfortDecMultiplier = static_cast<float>(comfortActive - comfortLazy) / 1000.0f;
  wakeEnergyDelta = -(totalEnergyShift / static_cast<float>(wakeHours)) / 30.0f;
  std::printf("[motive] constants: energy span %g, wake %d h from %d, hunger ratio %g, social base %g, aspiration dec %g\n",
              totalEnergyShift, wakeHours, wakeTime, hungerDecRatio, socialDecBase, aspirationDec);
}

// 800C27FC
void Motives::Init() {
  if (!sConstantsUpdated) sTheClient.UpdateConstants();
  for (float& v : m_values) v = 0.0f;
  for (float& v : m_old) v = 0.0f;
  m_person = nullptr;
}

// 800C2898
void Motives::Sim() {
  cXPersonImpl* p = m_person;
  double mult = 1.0;
  switch (p->GetAspirationStatus()) {
    case 0: mult = redAspMotiveDecayMult; break;
    case 1: mult = greenAspMotiveDecayMult; break;
    case 2: mult = goldAspMotiveDecayMult; break;
    case 3: mult = platinumAspMotiveDecayMult; break;
    default: break;
  }
  if (m_values[kSleepState] < 0.0f) {
    m_values[kEnergy] += sleepEnergyDelta;
    const int hour = Globs_pSimulator ? Globs_pSimulator->GetGlobal(cSimulatorImpl::kHour) : 12;
    if (hour > wakeTime && hour < bedTime) m_values[kEnergy] -= clockDrift;
  } else {
    m_values[kEnergy] += wakeEnergyDelta;
  }
  if (m_values[kHunger] - m_old[kHunger] > 0.0f)
    m_values[kBladder] -= static_cast<float>((m_values[kHunger] - m_old[kHunger]) * hungerToBladderRatio * mult);
  const int activity = p->GetPersonData(3);
  const float hungerDec = (m_values[kHunger] + 100.0f) * hungerDecRatio;
  m_values[kComfort] -= static_cast<float>((comfortDecMultiplier * static_cast<float>(activity) + comfortDecBase) * mult);
  m_values[kHunger] -= static_cast<float>(hungerDec * mult);
  const int outgoing = p->GetPersonData(6);
  if (m_values[kSleepState] >= 0.0f)
    m_values[kSocial] -= static_cast<float>((socialDecMultiplier * static_cast<float>(outgoing) + socialDecBase) * mult);
  if (m_values[kSleepState] < 0.0f) {
    m_values[kFun] *= entSleepMultiplier;
    m_values[kHygiene] -= static_cast<float>(hygSleepDec * mult);
    m_values[kBladder] -= static_cast<float>(bladderSleepDec * mult);
  } else {
    m_values[kFun] -= static_cast<float>(entWakeDec * mult);
    m_values[kHygiene] -= static_cast<float>(hygWakeDec * mult);
    m_values[kBladder] -= static_cast<float>(bladderWakeDec * mult);
  }
  if (m_values[kAspiration] > -80.0f) m_values[kAspiration] -= aspirationDec / 30.0f / 5.0f;
  Cleanup();
  if (p->GetPersonData(0x28) == 0)
    p->IncrementSimSocialInterest(interestInc);  // vtable 0x39C
  p->CalcHappy();
}

// 800C2C10
void Motives::Cleanup() {
  for (int i = 0; i < kCount; ++i) {
    if (m_person && m_person->IsGhost() && i != 0) m_values[i] = 0.0f;
    if (m_values[i] > 100.0f) m_values[i] = 100.0f;
    if (m_values[i] < -100.0f) m_values[i] = -100.0f;
    m_old[i] = m_values[i];
  }
}
