// @class Behavior after EREdithTreeSet ObjSelector ReconBuffer
// @class StackElem
// @class TreeStack
// @class TreeSimImpl
// @class Motives
// @class MotiveConstantsClient
// The behaviour VM (SimAntics, from The Sims): game/behavior.cpp (Behavior: an object's trees),
// game/treesim.cpp (TreeSimImpl: a stack of tree frames run node by node), game/motive.cpp (Motives).
//
// A tree (an IFF 'BHAV' chunk, big-endian, read in place) is a 12-byte header {signature 0x8000-0x8002,
// node count, type, params, locals, flags, version} and 12-byte nodes {opcode, true target, false
// target, 8 operand bytes}. Opcodes below 0x100 are primitives (cXObjectImpl::TryElement); others call
// a tree: 0x100-0xFFF global (the "global" object file), 0x1000-0x1FFF the object's own file,
// 0x2000-0x2328 its semi-global file. Targets 0xFD error, 0xFE return true, 0xFF return false.
// A primitive's result: 0 false, 1 true, 2 not done (it runs again next tick), 3 it pushed a frame
// (carry on there), -1 error.
#pragma once
#include <deque>
#include <vector>

// The trees an object's selector can call (GC: Behavior, with the tree-set lookup of its subclass
// ResourceBehavior; trees are found through the SmartDataFactory's ResourceBehaviorTree cache, PC: in
// the tree sets directly).
struct Behavior {
  Behavior();  // 800AB300
  virtual ~Behavior();  // 800AB314
  RET_UNKNOWN CountNodes(short);  // 800AB4A0
  RET_UNKNOWN GetNamespaceID();  // 800AB540
  RET_UNKNOWN GetSemiGlobalNamespaceID();  // 800AB54C
  static RET_UNKNOWN GetGlobNamespaceID();  // 800AB558
  // The node (its 12 bytes) of tree `tree`. 1: there is one.
  int GetNode(short tree, short node, const unsigned char** out);  // 800AB560
  int GetNodeRef(short tree, short node, const unsigned char** out);  // 800AB5BC
  RET_UNKNOWN IsNodeReachable(short, int);  // 800AB7B4
  void GetNodeText(short, BehaviorNode *, StringBuffer &);  // 800AB8C0
  void GetNodeText(short, short, StringBuffer &);  // 800AB8C4
  RET_UNKNOWN CountPrimitives();  // 800AB930
  // The object file (ResFile row) that holds tree or constants `id`, or null.
  virtual const unsigned char* GetResFileID(short id);  // 800AB968
  static int GetBaseID(short treeClass);  // 800AB9B4
  static int GetMaxID(short treeClass);  // 800AB9F4
  static int GetTreeClass(short id);  // 800ABA34 (0 primitive, 1 global, 2 private, 3 semi-global)
  static void GetClassName(short, StringBuffer &);  // 800ABA88
  RET_UNKNOWN IsSingleExit(short, short);  // 800ABB14
  RET_UNKNOWN GetCumulativeTreeVersion(short);  // 800ABD30
  // Tree `id`'s data (header, then nodes), or null.
  const unsigned char* GetTree(short id);  // 800AC384
  static void GetTreeIDByNameFast(char *);  // 800AC470
  // Tree `id`'s name ("" for none).
  const char* GetTreeName(short id);  // 800AC4A4
  // The constants (BCON) `id`: big-endian shorts. 1: there are some.
  bool GetConstants(short id, const unsigned char** values, int* count);  // 800AC5A8
  virtual void Init(Language* language, ObjSelector* owner);  // 800AC650
  ObjSelector* GetOwner() { return m_owner; }  // 800AD118
  RET_UNKNOWN GetLanguage();  // 800AD120
  void GetResFileIDByClass(short);  // 800AD128
  static bool IsDefaultParam(BehaviorNodeParam *);  // 800AD17C
  static void SetDefaultParam(BehaviorNodeParam *);  // 800AD1A4

  // PC: the behaviour of `selector`'s object file, made on first use (GC: ObjSelector +0x0C).
  static Behavior* For(ObjSelector* selector);
  // The tree set `id`'s trees (PC: by its tree id's class).
  EREdithTreeSet* TreeSetFor(short id);

  Language* m_language = nullptr;              // +0x04
  ObjSelector* m_owner = nullptr;              // +0x08
  const unsigned char* m_resFile = nullptr;    // the selector's ResFile row (GC: its namespace id +0x0C)
  const unsigned char* m_sgResFile = nullptr;  // its semi-global ResFile (+0x34)
  const unsigned char* m_globalResFile = nullptr;
  EREdithTreeSet* m_private = nullptr;         // +0x10 (ResourceBehavior)
  EREdithTreeSet* m_semiGlobal = nullptr;      // +0x14
  EREdithTreeSet* m_global = nullptr;          // +0x18
};

// A selector's object file (its "ResFile" row in the SimsObjects quickdata), or null.
const unsigned char* SelectorResFile(ObjSelector* selector);
// The object file holding the selector's tree table (its own or its semi-global file's).
const unsigned char* SelectorTreeTabResFile(ObjSelector* selector);

// Tree data (the BHAV chunk) fields.
namespace bhav {
inline int Signature(const unsigned char* t) { return static_cast<short>(simmy::be16(t)); }
inline int NumNodes(const unsigned char* t) { return simmy::be16(t + 2); }
inline int Type(const unsigned char* t) { return t[4]; }
// ObjectDataBehaviorTree::GetNumParams / GetNumLocals (8015AB2C / 8015ABA0): old signatures
// (0x8000, 0x8001) have at most four params, and 0x8000 trees no locals.
inline int NumParams(const unsigned char* t) {
  const int sig = Signature(t);
  if (sig < -0x7ffe) return sig == -0x7fff && t[5] <= 4 ? t[5] : 4;
  return t[5];
}
inline int NumLocals(const unsigned char* t) {
  const int sig = Signature(t);
  if (sig < -0x7ffe) return sig == -0x7fff ? (t[6] > 4 ? 4 : t[6]) : 0;
  return t[6];
}
inline const unsigned char* Node(const unsigned char* t, int i) { return t + 0xc + i * 0xc; }
inline int Opcode(const unsigned char* node) { return simmy::be16(node) & 0x7fff; }
inline int Operand16(const unsigned char* node, int i) { return static_cast<short>(simmy::be16(node + 4 + 2 * i)); }
}  // namespace bhav

// A frame of the tree stack (GC: variable length, 0x14 bytes then params and locals).
struct StackElem {
  void GetTreeName(StringBuffer &);  // 8014754C
  // Its stack object, tree, node, sizes, params, locals, primitive state and behaviour (by the
  // behaviour finder: the owner's save id). PC: the finder is the object folder (GC: its BehaviorFinder
  // interface, vtable 0x0C ReconBehavior).
  void ReconStream(ReconBuffer* buffer, int version, ObjectFolderImpl* finder);  // 801475A8
  short* GetParams() { return m_params; }  // 80148B7C
  short* GetLocals() { return m_locals.empty() ? nullptr : m_locals.data(); }  // 80148B84
  // Its bytes in the GC stack buffer: 0x14 then params and locals (rounded to 8 bytes).
  int GetSize() const { return ((m_numLocals + m_numParams + 3) * 2 & 0x7f8) + 0x14; }  // 80148B98
  RET_UNKNOWN NextFrame() const;  // 80148BB4
  // A copy of `from` with `params` (null: none copied).
  void Setup(const StackElem& from, const short* params);  // 80148BE4
  int GetTreeID() const { return m_treeId == -1 ? -1 : m_treeId & 0x7fff; }  // 80149118
  void SetTreeID(short id) { m_treeId = id == -1 ? -1 : static_cast<short>((id & 0x7fff) | (m_treeId & 0x8000)); }  // 80149138
  RET_UNKNOWN GetBreak() const;  // 8014915C
  void SetBreak(bool);  // 8014917C
  RET_UNKNOWN GetIgnoreDataBreakPoint() const;  // 801491AC
  void SetIgnoreDataBreakPoint(bool);  // 801491B4

  short m_treeId = 0;             // +0x00
  short m_node = 0;               // +0x02
  short m_stackObject = 0;        // +0x04 the stack object's id
  unsigned char m_numLocals = 0;  // +0x06
  unsigned char m_numParams = 0;  // +0x07
  int m_primState = 0;            // +0x08 a primitive's state while it runs over several ticks
  int m_primData = 0;             // +0x0C
  Behavior* m_behavior = nullptr; // +0x10
  short m_params[8] = {};         // +0x14
  std::vector<short> m_locals;
};

// GC: a buffer of frames; PC: a deque (frames keep their address while others are pushed).
struct TreeStack {
  TreeStack(TreeSimImpl& owner) : m_owner(&owner) {}  // 80147164
  ~TreeStack() = default;  // 80147198
  static void Initialize(int);  // 80147450
  void MakeNewFrame(unsigned int);  // 80147474
  // Version 0x19 on: the frame count, the memory reserved (GC: the buffer is allocated to it), then
  // each frame. 0xB..0x18: a count and old-style frames pushed. PC: the finder as StackElem's.
  void ReconStream(ReconBuffer* buffer, int version, ObjectFolderImpl* finder);  // 8014881C
  RET_UNKNOWN GetNewFrame();  // 80148C84
  // The bytes the frames take (GC: up to the end of the last frame).
  int GetMemUsed();  // 80148CCC
  // GC: the buffer's size. PC: the frames' bytes (a deque, no buffer).
  int GetMemReserved();  // 80148D28
  void AssignFrames(int);  // 80148D38
  void Push(const StackElem& frame, const short* params);  // 80148EC8
  void Pop();  // 80149048
  void Reset() { m_frames.clear(); }  // 801490C4
  StackElem* GetNthFrame(int n);  // 801490D0
  int GetStackSize() const { return static_cast<int>(m_frames.size()); }  // 80149104

  std::deque<StackElem> m_frames;  // +0x08
  TreeSimImpl* m_owner = nullptr;  // +0x18
};

// An object's behaviour stack (GC 0x44 bytes, a base of cXObjectImpl, with TreeSim's interface).
struct TreeSimImpl {
  explicit TreeSimImpl(cXObjectImpl* object);  // 8014772C
  ~TreeSimImpl();  // 80147258
  void SetError(short error) { m_error = error; }  // 80147434
  int GetError() const { return m_error; }  // 8014743C
  void ClearError() { m_error = 0; }  // 80147444
  RET_UNKNOWN GetISimInstance();  // 801476E0
  // `defaultParams`: a call's params when a node passes none (the object's temps).
  void Initialize(int depth, short* defaultParams);  // 80147928
  void GetCurrentNode(short* tree, short* node);  // 80147960
  int GetCurrentPrimitive();  // 801479BC
  // The stack emptied, then `tree` of `behavior` started (0: left empty).
  void Reset(Behavior* behavior, short tree);  // 80147A34
  // Pushes a frame for `tree` (`behavior` null: the current frame's). 1: pushed.
  int Gosub(Behavior* behavior, const short* params, short tree);  // 80147AA0
  // Runs `tree` to its end in one go (a check tree), on top of the stack. The result: 0 false, 1 true.
  int RunCheckTree(Behavior* behavior, short tree, short stackObject, const short* params);  // 80147C70
  int RunOneTickTree(Behavior* behavior, short tree, short stackObject, const short* params);  // 80147F58
  virtual void HandleBreakpoint(StackElem *, BehaviorNode *);  // 80147F90
  // One node of `frame`: 0 carry on, 1 the tick is over (a primitive waits), 2 stop (an error).
  int DoNodeAction(StackElem* frame);  // 80147FD8
  bool GetLastTransition() const { return m_lastTransition == 0xfe; }  // 8014853C
  // Runs nodes until one waits (1) or fails (0).
  int Simulate();  // 80148550
  void GetHighLevelAction();  // 801485D8
  StackElem* GetCurElem() { return m_stack.GetNthFrame(m_stack.GetStackSize() - 1); }  // 801486CC
  StackElem* GetMainSimElem();  // 80148708
  StackElem* GetNthElem(short n) { return m_stack.GetNthFrame(n); }  // 801487CC
  int GetStackSize() const { return m_stack.GetStackSize(); }  // 801487F0
  virtual void StackJustPopped() {  // 80148818
    if (m_object) m_object->TreeStackPopped();
  }
  int GetIterations() const { return m_iterations; }  // 80149294
  int GetLastResult() const { return m_lastResult; }  // 8014929C
  bool HasMainTreeSim() const { return m_noMainTreeSim == 0; }  // 801492A4
  void SetNoMainTreeSim(bool noMain) { m_noMainTreeSim = noMain ? 1 : 0; }  // 801492B4
  void AsTreeSim();  // 801492BC
  void AsTreeSim() const;  // 801492C0

  static int sMaxIterations;  // TreeSim::sMaxIterations
  static int sInMainSim;      // TreeSim::sInMainSim

  cXObjectImpl* m_object = nullptr;    // GC: the TreeSim's object (+0x00)
  int m_iterations = 0;                // +0x04 nodes run this tick
  int m_field08 = 0;                   // +0x08
  TreeStack m_stack;                   // +0x0C
  int m_lastTransition = 0xfd;         // +0x28 the last node target taken
  int m_lastResult = 0;                // +0x2C the last primitive's (or return's) result
  short* m_defaultParams = nullptr;    // +0x30
  short m_error = 0;                   // +0x34
  int m_noMainTreeSim = 0;             // +0x38
};

// A person's motives (game/motive.cpp; GC at cXPersonImpl +0xA8, 0x84 bytes): 16 values from -100 to
// 100 and their values at the last update. Indices: 0 aspiration, 5 energy, 6 comfort, 7 hunger,
// 8 hygiene, 9 bladder, 0xB sleep state (below 0 asleep), 0xE social, 0xF fun.
struct Motives {
  enum : int { kAspiration = 0, kMood = 3, kEnergy = 5, kComfort = 6, kHunger = 7, kHygiene = 8, kBladder = 9,
               kSleepState = 0xb, kSocial = 0xe, kFun = 0xf, kCount = 0x10 };
  void Init();  // 800C27FC
  // One update (every 60 ticks): the decays (scaled by the aspiration level's multiplier), energy
  // from waking/sleeping and the clock, then Cleanup.
  void Sim();  // 800C2898
  // Clamped to -100..100 (a ghost's are 0 but the first), then remembered as the old values.
  void Cleanup();  // 800C2C10

  float m_values[kCount] = {};  // +0x00
  float m_old[kCount] = {};     // +0x40
  cXPersonImpl* m_person = nullptr;  // +0x80
};

// The motive decay constants (FloatConstants "energy span", "hunger decrement ratio", ...).
struct MotiveConstantsClient {
  virtual void UpdateConstants();  // 800C2010
  MotiveConstantsClient();  // 800C2D1C
  virtual ~MotiveConstantsClient() = default;
};

// The aspiration levels' motive multipliers (game/motive.cpp; MotiveConstantsClient sets them).
extern float redAspMotiveDecayMult, greenAspMotiveDecayMult, goldAspMotiveDecayMult, platinumAspMotiveDecayMult;
extern float redAspMotiveSatisfyMult, greenAspMotiveSatisfyMult, goldAspMotiveSatisfyMult, platinumAspMotiveSatisfyMult;
extern float redAspMotiveCostMult, greenAspMotiveCostMult, goldAspMotiveCostMult, platinumAspMotiveCostMult;
