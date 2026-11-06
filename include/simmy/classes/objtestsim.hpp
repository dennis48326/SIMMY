// @class ObjTestSim
// game/objtestsim.cpp: tests a person's interactions with an object (the tree table entry's ghost /
// adult availability, the object's prefix and check trees run on the person's stack) and builds the
// menu / autonomy lists. A check tree may write the person's temps: the first test saves them and the
// destructor puts them back (GC +0x08 the saved temps, +0x18 whose).
#pragma once

struct ObjTestSim {
  ObjTestSim() = default;
  explicit ObjTestSim(cXPerson* person);           // 80112048
  ObjTestSim(cXPerson* person, cXObject* object);  // 80112060
  ~ObjTestSim();                                   // 801120A4
  void SetStackObject(cXObject* object);           // 80112138
  // Sets the action's checked flag and its available flag from the tests: `autonomous` (param 0 of the
  // check tree; the object's data 0x19 "in use" refuses), `prefix` (the tree table's prefix check
  // tree), `check` (the entry's check tree, then the menu colours from the person's data 0x32).
  void TestInteraction(Interaction* action, bool autonomous, bool prefix, bool check, TTabScratchEntry** mod);  // 80112188
  static void SetupInteractionColors(Interaction* action, short mode);  // 801126E0
  void AppendInteractionsForMenu(InteractionList &, int);  // 8011271C
  RET_UNKNOWN RunMenuCheckTree(InteractionList &, Interaction &);  // 80112BA8
  static RET_UNKNOWN IsMenuInProgress();  // 80112C40
  void MakeNewMenuItem(unsigned short *, short *, StackElem *, short);  // 80112C70
  RET_UNKNOWN AppendInteractionsForAuto(InteractionList &);  // 80112D40

  cXPerson* m_person = nullptr;          // +0x00
  cXObject* m_stackObject = nullptr;     // +0x04 the interaction leader (SetStackObject)
  short m_savedTemps[8] = {};            // +0x08
  cXPersonImpl* m_tempsOwner = nullptr;  // +0x18 the person whose temps are saved, or null
};
