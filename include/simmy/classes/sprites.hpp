// @class IconGroupImpl
// @class ESpriteRender__SpriteData
// @class ESpriteRender after ESpriteRender__SpriteData EOrderTableData
// @class ESpriteRenderMan
// Sprites over objects and people (game/slots.cpp SpriteSlot, game/icongroup.cpp, game/espriterender.cpp,
// game/espriterenderman.cpp): a SpriteSlot says which sprite (an icon id, or an object's thumbnail), with
// which balloon behind it and overlay over it, shows over its object, for how many ticks, rising, popping
// up and fading out (options); ESpriteRender (one per object, kept by EGlobal's ESpriteRenderMan) holds
// the shaders for it (EGlobal::ConvertSpriteIdToResId: the "SpriteIdToResIdNode" table of quickdata
// 0x0c33db41 gives a sprite's shader and size) and draws it as camera-facing quads into the house's
// order table. IconGroupImpl maps an icon group's icons to sprite ids (sIDMaps).
#pragma once
#include <vector>


// 0x10 bytes.
struct IconGroupImpl {
  IconGroupImpl();  // 800BCA10
  virtual void Init(int group);  // 800BCA34
  virtual ~IconGroupImpl();  // 800BCA8C
  virtual int GetSpriteID(int icon);  // 800BCB04
  virtual void GetLabel(StringBuffer &);  // 800BCB68
  static void LoadStrings();  // 800BCB6C
  virtual void GetIconLabel(int, StringBuffer &);  // 800BCB70
  virtual RET_UNKNOWN CountIconLabels();  // 800BCC0C
  int m_group = 0;               // +0x04
  void* m_strings = nullptr;     // +0x08 (labels: not ported)
  const short* m_map = nullptr;  // +0x0C the group's sIDMaps entry (+4: the first sprite id)
};

// A sprite's quad (ESpriteRender::SpriteData): its half size, its offset from the anchor, its colour.
struct ESpriteRender__SpriteData {
  float m_w = 0.0f, m_h = 0.0f;           // +0x00
  float m_off[3] = {};                    // +0x08
  float m_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // +0x14 r, g, b (x128 on GC), a
};

// 0xEC bytes.
struct ESpriteRender {
  using SpriteData = ESpriteRender__SpriteData;
  ESpriteRender();  // 80035460
  virtual ~ESpriteRender();  // 80035530
  void DeleteShaders();  // 80035588
  void Update();  // 800356C8
  void DrawIntoOrderTable();  // 80035890
  static void SpriteRenderOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 80035C08
  void DrawDirectly(ERC* rc);  // 80035C50
  void SetSprite();  // 80036178
  bool SetUpRect(ERC* rc, EVec3& pos, float w, float h, SpriteData& data);  // 80036734
  bool SetUpDoubleRect(ERC* rc, EVec3& pos, float w, float h, ERShader* a, ERShader* b, SpriteData& data);  // 800369A0
  cXObject* GetObject();  // 80036E70
  int GetMarked();  // 80036E78
  int GetMarkedAsNew();  // 80036E80
  void Mark();  // 80036E88
  void MarkAsNew();  // 80036E94
  bool Anchor(EVec3& out, SpriteSlot*& slot);  // PC: DrawIntoOrderTable's / DrawDirectly's common part

  int m_marked = 0;              // +0x00 to be deleted
  int m_markedAsNew = 0;         // +0x04 the slot changed: SetSprite on the next Update
  cXObject* m_object = nullptr;  // +0x08
  unsigned int m_mainId = 0;     // +0x0C
  ERShader* m_main = nullptr;    // +0x10
  unsigned int m_balloonId = 0;  // +0x14
  ERShader* m_balloon = nullptr; // +0x18
  unsigned int m_overlayId = 0;  // +0x1C
  ERShader* m_overlay = nullptr; // +0x20
  ERShader* m_digit1 = nullptr;  // +0x24 numbers 1000..1099: the units digit
  unsigned int m_digit1Id = 0;   // +0x28
  ERShader* m_digit10 = nullptr; // +0x2C the tens digit
  unsigned int m_digit10Id = 0;  // +0x30
  SpriteData m_mainData;         // +0x38
  SpriteData m_balloonData;      // +0x5C
  SpriteData m_overlayData;      // +0x80
  EOrderTableData m_otd;         // +0xA4
  int m_dofForeground = 0;       // +0xC8
  EVec3 m_pos;                   // +0xDC the anchor (the order table sorts by it)
};

// EGlobal +0x10C (PC: sprites.cpp keeps it).
struct ESpriteRenderMan {
  ESpriteRenderMan();  // 80036EA0
  ~ESpriteRenderMan();  // 80036EBC
  void Update();  // 80036F64
  void RemoveMarkedSprites();  // 80036FB4
  void DrawIntoOrderTable();  // 80037048
  void DrawDirectly(ERC* rc);  // 8003708C
  void SetSprite(SpriteSlot* slot);  // 800370D8
  ESpriteRender* AddSprite(cXObject* object);  // 80037130
  void MarkSprite(cXObject* object);  // 80037190
  static ESpriteRenderMan* Instance();  // PC: EGlobal +0x10C, made on first use

  std::vector<ESpriteRender*> m_list;  // +0x00 ENodeList
  int m_owns = 1;                      // +0x08 deletes its renderers
};
