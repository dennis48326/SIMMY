// @class EIPointLight after EILight EVec3
// @class EIPortalPointLight after EILight EVec3
// @class EISpotLight after EILight EVec3
// Lights with a position (game/e_ipointlight.cpp, e_iportalpointlight.cpp, e_ispotlight.cpp): lamps
// (ISimsObjectModel::InitBulb, model lights) and the window lights that carry light through a portal
// (one on each side of every window, EIObjectMan::AddObject). See src/game/e_ipointlight.cpp.
#pragma once

// A point light (0xD0 bytes): it lights within falloffEnd / 4 (the grid's range).
struct EIPointLight : public EILight {
  EIPointLight();  // 80229DCC
  SIMMY_STORABLE_DECL(EIPointLight)
  void Write(EStream &) override;  // 80229E78
  void Read(EStream &) override;  // 80229F40
  void CalcFullIntensityLightOnPoint(EVec3& point, float& out) override;  // 8022A008
  void LightingParameters(EVec3& dir, float& range, EVec3& pos, EVec3& color) override;  // 8022A0E0
  void SetPosition(EVec3& pos);  // 8022A278
  void Setup() override;  // 8022A354
  static void RegisterType(unsigned short);  // 8022A5A8
  void CreateCopy() const;  // 8022A5FC
  int GetLightType() override;  // 8022A61C
  void GetPosition(EVec3& out) override;  // 8022A624
  double GetFalloffEnd() override;  // 8022A644
  int GetRoomId() { return m_room; }  // 8022A64C

  EVec3 m_pos;                    // +0xAC
  int m_tileX = 0, m_tileY = 0;   // +0xB8 the tile it is on (a move to another tile or room rebuilds the grid)
  int m_room = -1;                // +0xC0
  float m_falloffStart = 10.0f;   // +0xC4
  float m_falloffEnd = 20.0f;     // +0xC8
  int m_useFalloff = 1;           // +0xCC
};

// A window's light on one side (0xD8 bytes): it carries the light from the other side in.
struct EIPortalPointLight : public EIPointLight {
  EIPortalPointLight();  // 8022A740
  SIMMY_STORABLE_DECL(EIPortalPointLight)
  double GetLumina() const;  // 8022A720
  void SetPortal(EInstance* portal, bool side);  // 8022A7E8
  void Write(EStream &) override;  // 8022A824
  void Read(EStream &) override;  // 8022A908
  void LightingParameters(EVec3& dir, float& range, EVec3& pos, EVec3& color) override;  // 8022A9DC
  void Setup() override;  // 8022AB40
  void Update() override;  // 8022AC50
  RET_UNKNOWN IsEnabled() override;  // 8022AE6C
  static void RegisterType(unsigned short);  // 8022B080
  void CreateCopy() const;  // 8022B0D4
  int GetLightType() override;  // 8022B0F4
  EInstance* GetPortal() { return m_portal; }  // 8022B0FC
  virtual RET_UNKNOWN IsPointLight();  // 8022B104

  int m_side = 0;                 // +0xD0 0: the portal's own side, 1: the other
  EInstance* m_portal = nullptr;  // +0xD4
};

// A spot light (0xD8 bytes). The light grid takes no spot lights (its type is none).
struct EISpotLight : public EILight {
  EISpotLight();  // 8022B1E0
  SIMMY_STORABLE_DECL(EISpotLight)
  void Write(EStream &) override;  // 8022B2B8
  void Read(EStream &) override;  // 8022B3D8
  void Setup() override;  // 8022B4E8
  static void RegisterType(unsigned short);  // 8022B860
  void CreateCopy() const;  // 8022B8B4

  EVec3 m_pos;                    // +0xAC
  EVec3 m_dir{0.0f, 0.0f, -1.0f}; // +0xB8
  float m_falloffStart = 10.0f;   // +0xC4
  float m_falloffEnd = 20.0f;     // +0xC8
  float m_innerAngle = 0.5235988f;  // +0xCC
  float m_outerAngle = 0.7853982f;  // +0xD0
  int m_useFalloff = 1;           // +0xD4
  float m_cosInner = 0.0f;        // +0xD8 cos(inner angle / 2) (Setup)
  float m_cosOuter = 0.0f;        // +0xDC cos(outer angle / 2)
};

// Free functions ported with the lights (game/rooms.cpp, game/ifloor.cpp).
RET_UNKNOWN GetLightRoomID(unsigned short id);  // 80138854
void EvaluateFloorLights(ELightGrid& grid, int room, EVec3& ambient, EVec3& diffuse);  // 800461E0
