// @class WorldMap after DigestGeomShaderPtr CameraDirector EACNodeState ERShader ERModel ERAnim ECharacterNode ISimInstance ObjAnimDef
// The neighbourhood map behind the main menu (WorldMap in game/thesims.cpp): the "worldmap" level
// lit by a fast day/night cycle, an animated scenery object ("scenery_outerlot"), a camera flying the
// "cam_intro_worldmap" animation through the player's CameraDirector, and the Sims 2 logo. The main
// menu state creates it; its level arriving ends the loading screen.
#pragma once

// 0xDC bytes.
struct WorldMap {
  WorldMap();  // 800998C8
  ~WorldMap();  // 80099AA8
  WorldMap(const WorldMap&) = delete;
  WorldMap& operator=(const WorldMap&) = delete;
  void Reset();  // 80099C44
  void Update();  // 80099E00
  void Draw(ERC* rc);  // 8009A1D0

  static constexpr unsigned int kLevel = 0xbafac2db;       // "worldmap"
  static constexpr unsigned int kScenery = 0x695154b9;     // "scenery_outerlot" (model and character)
  static constexpr unsigned int kSceneryAnim = 0x05e6001d; // "o_scenery_outerlot..."
  static constexpr unsigned int kCameraAnim = 0xba7c00f0;  // "cam_intro_worldmap"

  int m_waitingForLevel = 0;            // +0x00 the loading screen is up until the level is in
  ESimsCam* m_cam = nullptr;            // +0x04
  ERLevel* m_level = nullptr;           // +0x08
  ERModel* m_sceneryModel = nullptr;    // +0x0C
  ERAnim* m_sceneryAnim = nullptr;      // +0x10
  ERCharacter* m_sceneryCharacter = nullptr;  // +0x14
  EAnimController m_anim;               // +0x18
  ISimsObjectModel* m_scenery = nullptr;  // +0x90 (also +0x94)
  ERShader* m_logo = nullptr;           // +0x98 the localised Sims 2 logo
  ObjAnimDef m_sceneryAnimDef{kScenery, 0, kSceneryAnim};  // +0x9C the scenery's model and animation
  float m_hour = 6.0f;                  // +0xBC time of day
  float m_hoursPerHour = 1800.0f;       // +0xC0 game hours per real hour (half an hour a second)
  float m_hourStep = 0.0f;              // +0xC4 this frame's
  int m_hourInt = 6;                    // +0xC8
  int m_minute = 0;                     // +0xCC
  int m_fieldD0 = 0;                    // +0xD0
  int m_drawLogo = 0;                   // +0xD4 set every frame by the main menu
  int m_running = 1;                    // +0xD8 time and animations run (stopped in the save menus)
};

extern WorldMap* g_pWorldMap;  // 804FF8F4
