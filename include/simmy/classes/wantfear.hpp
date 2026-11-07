// @class WantFear__Notification
// @class WantFear__Bookmark
// @class WantFearManager after WantFear__Bookmark
// Wants and fears (game/wantfear.cpp, the WantFear part of game/neighbor.cpp). The data is the binary
// resource 33B78948 ("wantdata_gc.bin", big-endian, kept as loaded): events (0x24 bytes: +0 id, +2
// kind (0x80 a fear, 1 platinum), +4 the text set, +6 the "valid" check tree, +8 the "completed" check
// tree, +0xA the target type (4 a sim), +0xB 2: counted, +0xC the icon shader, +0x14 a name), trees
// (+0 id, +2 flags (1 story), +4 a name, +0x24 the node count, +0x28 the first node, +0x2C the child
// list) of nodes (0x20 bytes: +0 id, +2 the event, +4 the text index, +6 the target, +8 the count,
// +0xA the first child, +0xC the branches, +0xE the reward, +0x10 AdData (12 bytes), +0x1C how the
// count compares, +0x1D flags: 1 hidden, 2 the target comes from the parent), the story roots and the
// category shaders. A neighbour holds 20 bookmarks (0..12 wants, 13..19 fears) and 7 active slots
// (0..3 wants, 4..6 fears) of them.
#pragma once
#include <string>
#include <vector>

// What happened (6 bytes): the event, its target (a neighbour id, 0 none) and a count; given to the
// neighbours' wants and fears (Neighbor::Notify).
struct WantFear__Notification {
  short m_event = 0;   // +0x00
  short m_target = 0;  // +0x02
  short m_count = 0;   // +0x04
};

// The events C++ code sends (WantFear:: constants in .sdata, 80500638..80500654); kGenericParam fills a
// target or count the event does not use.
namespace WantFearEvent {
constexpr short kGenericParam = 0;                                      // 80500638
constexpr short kBuildBuyCategory = 0x2ffb;                             // 80500644
constexpr short kBuildBuyWorth = static_cast<short>(0x9fd5);            // 80500646
constexpr short kFamilyFundsOfX = 0x1684;                               // 80500648
constexpr short kFamilyFundsPlat = static_cast<short>(0xb902);          // 8050064A
constexpr short kBuildBuyWorthPlatinum = 0x4c73;                        // 8050064C
constexpr short kAddARoom = 0x1a40;                                     // 8050064E
constexpr short kBuildWalls = 0x03ff;                                   // 80500650
constexpr short kBuyXLights = static_cast<short>(0x96c8);               // 80500652
constexpr short kBuyXChairs = static_cast<short>(0xb53f);               // 80500654
}  // namespace WantFearEvent

// 0x0C bytes. PC: the node is its index in its tree (GC: a WantFear::Node*; saved as the node's id,
// which is its index), -1 for none.
struct WantFear__Bookmark {
  WantFear__Bookmark() { Clear(); }  // 8014AB70
  // `tree`'s node `node` (index), the target kept when the node takes its parent's, a countdown.
  WantFear__Bookmark(unsigned short tree, short target, int node, unsigned short countdown);  // 8014ABA0
  void Clear();  // 8014ABC8
  void ResetCountdown();  // 8014ABE4
  int GetNumBranches() const;  // 8014AC34
  bool IsPlatinum() const;  // 8014AC40
  bool IsFear() const;  // 8014AC74
  bool IsHidden() const;  // 8014ACA8
  bool IsDuplicateNode(const WantFear__Bookmark& other) const;  // 8014ACB8
  bool IsDuplicateEvent(const WantFear__Bookmark& other) const;  // 8014AD30
  // 0 a want, 1 platinum, 4 a story want, 0x80 a fear.
  int GetType() const;  // 8014ADA0
  bool IsInStoryTree() const;  // 8014AE08
  // Its text (the event's string set: the node's line, else the generic one) with $sim, $int,
  // $money(Hundreds) and $me filled in; not `full`: " (+reward)" after it. GC: a BString2.
  std::u16string GetText(Neighbor* who, bool full) const;  // 8014AE3C
  int GetTarget() const;  // 8014B36C
  const unsigned char* GetAdData() const;  // 8014B38C
  unsigned int GetBaseShader(Neighbor* who) const;  // 8014B398
  unsigned int GetOverlayShader(Neighbor* who) const;  // 8014B440
  unsigned int GetBackgroundShader(Neighbor* who) const;  // 8014B44C
  // The notification is its node's event, for its target (if it has one), with the count compared as
  // the node says (+0x1C: 0 any, 1 <, 2 <=, 3 ==, 4 >=, 5 >).
  bool SatisfiedBy(const WantFear__Notification& n) const;  // 8014B4DC
  // Its ad (when `checkAd`) and the event's "valid" check tree (on the wants object, GUID 9E12BF3C)
  // allow it.
  bool IsValid(Neighbor* who, bool checkAd) const;  // 8014B5F4
  // The event's "completed" check tree says it is done already.
  bool IsCompleted(Neighbor* who) const;  // 8014B748
  // The node's reward (+0x0E) to the neighbour's aspiration (its person's, with the balloon), and to
  // the story family's total (for a primary sim or person data 0x4B), unlocking catalogue objects.
  void ApplyReward(Neighbor* who);  // 8014B858
  WantFear__Bookmark GetNextBookmarkAlongBranch(short branch) const;  // 8014BA7C
  RET_UNKNOWN DoStream(ReconBuffer *, int);  // 8014BB4C

  // PC: the node and its event in the manager's data (null for none).
  const unsigned char* Node() const;
  const unsigned char* Event() const;
  bool Empty() const { return Node() == nullptr; }

  unsigned short m_tree = 0;     // +0x00
  short m_field02 = 0;           // +0x02 the target (when the node takes its parent's)
  short m_node = -1;             // +0x04 (GC: WantFear::Node*)
  unsigned short m_field08 = 0;  // +0x08 the countdown
};

// GC: EGlobal +0x120 (DAT_80475EEC), made by EGlobal::SetDefaults. PC: TheWantFearManager().
struct WantFearManager {
  WantFearManager();  // 8014BC6C
  ~WantFearManager();  // 8014BCB8
  void CaptureLog(bool) {}  // 8014BD04
  bool LoadWants();  // 8014BD08
  const unsigned char* GetEventByIndex(int index) const;  // 8014BF20
  const unsigned char* GetEvent(unsigned short id) const;  // 8014BF30
  const unsigned char* GetTreeByIndex(int index) const;  // 8014BFBC
  const unsigned char* GetTree(unsigned short id) const;  // 8014BFCC
  // GetStoryTreeRoot: the root bookmark of the story tree listed for (a, b).
  WantFear__Bookmark StoryTreeRoot(short a, short b) const;  // 8014C064
  unsigned int GetCategoryShader(int type, short target);  // 8014C164
  // A new root bookmark of a want (or fear) tree, by lottery weighted with each root's ad.
  WantFear__Bookmark PickNewRoot(Neighbor* who, bool fear);  // 8014C1FC
  int Lottery(unsigned int* weights, unsigned int count) const;  // 8014C3B8

  // PC helpers over the big-endian data.
  int TreeCount() const;
  const unsigned char* Nodes(const unsigned char* tree) const;
  const unsigned char* NthChildOfNode(const unsigned char* tree, const unsigned char* node, int n) const;  // WantFear::Tree::GetNthChildOfNode 8014AB4C
  int NodeIndex(const unsigned char* tree, const unsigned char* node) const;

  EResource* m_res = nullptr;              // +0x00
  const unsigned char* m_header = nullptr; // +0x04 (version 3: the data's start)
  const unsigned char* m_events = nullptr; // +0x08
  const unsigned char* m_trees = nullptr;  // +0x0C offsets of the trees
  const unsigned char* m_nodes = nullptr;  // +0x10
  const unsigned char* m_story = nullptr;  // +0x14
  const unsigned char* m_categories = nullptr;  // +0x18
};

WantFearManager* TheWantFearManager();
// The person on the lot whose neighbour this is (GC: Neighbor +0x18), else null.
cXPersonImpl* NeighborPerson(Neighbor* n);
// PC: active slot `slot` (0..3 wants, 4..6 fears) of the neighbour for the HUD (GetType, GetBaseShader,
// GetOverlayShader, GetText); false when empty.
bool WantFearSlot(Neighbor* n, int slot, int* type, unsigned int* base, unsigned int* overlay, std::u16string* text);
// The want/fear events of a purchase in buy mode (PCTTarget::PlaceModeCallback's part, 801D745C).
void NotifyBuildBuyPurchase(ObjSelector* master, int price);
