// @class UI3D__UI3DRecord after CUnlockDisplay
// @class UI3D after UI3D__UI3DRecord
// The 3D shapes of the Flash UI (game/ui3d.cpp): a pool of 16 records, each a named CUnlockDisplay set
// up for an object definition (character, model, animation) on behalf of a target (its group: the
// target's name). customControlRender draws the record named like a "CCR3D" clip into the clip's box;
// "__3D_UPDATE_" (UIUpdateTable, priority 0x4B) updates every display each frame. The catalogue
// (PCTTarget "PCT_ObjDisp3D") and the reward dialog (RMDTarget "0RMD_model") install them.
#pragma once

// 0x28 bytes (GC: a pool node).
struct UI3D__UI3DRecord {
  UI3D__UI3DRecord* m_prev = nullptr;  // +0x00
  UI3D__UI3DRecord* m_next = nullptr;  // +0x04
  const char* m_name = nullptr;        // +0x08
  unsigned int m_character = 0;        // +0x0C
  unsigned int m_model = 0;            // +0x10
  unsigned int m_anim = 0;             // +0x14
  const char* m_group = nullptr;       // +0x18 the target's name
  CUnlockDisplay* m_display = nullptr; // +0x1C
  int m_colorIndex = 0;                // +0x20
};

struct UI3D {
  using UI3DRecord = ::UI3D__UI3DRecord;
  UI3D();  // 80176880
  virtual ~UI3D();  // 801769E8
  void AddEntry(char* name, unsigned int character, unsigned int model, unsigned int anim, int colorIndex,
                char* group);  // 80176AB4
  void RemoveEntry(UI3D__UI3DRecord* record);  // 80176C68
  bool InstallEntry(char* name, unsigned int character, unsigned int model, unsigned int anim, char* group,
                    int colorIndex);  // 80176D6C
  UI3D__UI3DRecord* FindEntry(char* name);  // 80176E34
  bool UnInstallEntry(char* name);  // 80176EEC
  bool UnInstallAllEntries(char* group);  // 80176F30
  bool ContainsEntry(char* name);  // 80176F90
  void Update3D();  // 80176FC0
  CUnlockDisplay* GetDisplayObject(char* name);  // 80177004
  static UI3D* Instance();  // 80177038
  static void Shutdown();  // 80177074

  UI3D__UI3DRecord* m_head = nullptr;  // +0x00 installed, most recently found first
  UI3D__UI3DRecord* m_tail = nullptr;  // +0x04
  UI3D__UI3DRecord* m_free = nullptr;  // +0x08 the pool
  static UI3D* s_pInstance;
};
