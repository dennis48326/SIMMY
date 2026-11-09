// @class GameData after EResource Player
// Global game state (game/gamedata.cpp, the object _gd): players, the current family/house, the
// copyright notice shown at boot and assorted UI flags. Most fields are still only known by offset.
#pragma once
struct NeighborhoodCustomChars;  // neighborcustomchar.hpp

struct GameData {
    using eCtrlDisableBits = ::GameData__eCtrlDisableBits;
    GameData();  // 8003AFD4
    ~GameData();  // 8003B0FC
    void GamePlayShutdown();  // 8003B1E4
    void GamePlayReset();  // 8003B22C
    static void LoadDefaultNeighborhood();  // 8003B2B0
    static void LoadSavedNeighborhood();  // 8003B5A0
    void PrepareSimData(int, int);  // 8003B638
    void ReplaceSimData(int, bool, int, int);  // 8003B744
    void LoadCreateASim(int, int);  // 8003B878
    static void SaveCreateASim();  // 8003B8A0
    void PlayerEnterHouse();  // 8003B8F8
    void PrepCreateAFamilyData(int);  // 8003BBCC
    int CreateAFamilyAddFamilyMember(Family* family);  // 8003C074 (the new member's GUID)
    RET_UNKNOWN StoreCreateAFamilyData();  // 8003C114
    RET_UNKNOWN SaveCreateAFamily();  // 8003C728
    RET_UNKNOWN SystemPreUpdate();  // 8003C798
    RET_UNKNOWN SystemPreDraw(ERC *);  // 8003CE34
    void SystemPostUpdate();  // 8003CE78
    RET_UNKNOWN IsDrawCtrlMessageDisabled();  // 8003CE7C
    void SetCtrlDrawMessageDisable(GameData__eCtrlDisableBits);  // 8003CE94
    void ClearCtrlDrawMessageDisable(GameData__eCtrlDisableBits);  // 8003CEAC
    void SystemPostDraw(ERC *);  // 8003CEC4
    void SetTwoPlayerMode(bool);  // 8003D138
    bool IsPlayerConnected(unsigned char);  // 8003D19C
    static void GotoLiveMode();  // 8003D208
    int CopyrightUpdate(float dt);  // 8003D248
    void CopyrightDraw(ERC *);  // 8003D344
    void VersionInfoDraw(ERC *);  // 8003D45C
    static RET_UNKNOWN GetLocalizedSims2Logo();  // 8003D630
    static RET_UNKNOWN GetLocalizedEAMovie();  // 8003D6A0
    static RET_UNKNOWN GetLocalizedSimsIntroMovie();  // 8003D788
    static RET_UNKNOWN GetLocalizedSimsEndingMovie();  // 8003D7A8
    static void SetBlackGraphicsBackgroundColor();  // 8003D7C8
    static void SetUglyGraphicsBackgroundColor();  // 8003D824
    static void UseConfigOptions();  // 8003D880
    void StageInitTiming(float *, int);  // 8003D8A8
    static void StageStartFrame();  // 8003DA5C
    void StageEndFrame(char *, int, int);  // 8003DAA0
    void StageEnd(char *, char *, bool);  // 8003DBE8
    void StageOutputActualTimes(char *);  // 8003DC38
    RET_UNKNOWN GotoXamMode(int, int, int, int);  // 8003DC3C
    int GetBuildBuyPlayer() const;  // 8003DDA0
    int SetBuildBuyPlayer(int player);  // 8003DDA8
    void GetSelectedPlayerFamily();  // 8003DDD4

  int m_field00 = 0;  // +0x00
  unsigned char m_memoryMeter[0x28] = {};  // +0x04 EMemoryMeterWin (debug)
  int m_field2C = 0;  // +0x2C
  int m_field30 = 0;  // +0x30 player 1 pad unplugged
  int m_field34 = 0;  // +0x34 player 2 pad unplugged
  int m_field38 = 0;  // +0x38
  CASTarget* m_casTarget = nullptr;  // +0x3C the CAS target while Create-a-Sim runs (DAT_80475BC0)
  void* m_field40 = nullptr;  // +0x40 PDATarget (DAT_80475BC4; the in-game targets hold UIObjectBase*)
  HUDTarget* m_hudTarget = nullptr;  // +0x44 the live-mode HUD (DAT_80475BC8)
  H2DTarget* m_h2dTarget = nullptr;  // +0x48 its 2D part (DAT_80475BCC)
  void* m_field4C = nullptr;  // +0x4C (DAT_80475BD0)
  void* m_field50 = nullptr;  // +0x50 OPTTarget (DAT_80475BD4)
  void* m_field54 = nullptr;  // +0x54 M2MTarget (DAT_80475BD8; deleted in the destructor)
  void* m_field58 = nullptr;  // +0x58 E2ETarget (DAT_80475BDC)
  void* m_field5C = nullptr;  // +0x5C WXFTarget (DAT_80475BE0)
  void* m_field60 = nullptr;  // +0x60 PRGTarget (DAT_80475BE4)
  void* m_field64 = nullptr;  // +0x64 O2TTarget (DAT_80475BE8)
  UIObjectBase* m_mainMenuTarget = nullptr;  // +0x68 MMUTarget while the main menu is up (DAT_80475BEC)
  void* m_field6C = nullptr;  // +0x6C INGTarget (DAT_80475BF0; owned)
  void* m_field70 = nullptr;  // +0x70 PAZTarget, the pause menu (DAT_80475BF4; owned)
  void* m_field74 = nullptr;  // +0x74 PCT/CSP/R2L target (DAT_80475BF8; owned)
  NeighborhoodCustomChars* m_customChars = nullptr;  // +0x78 the CAS family slots (DAT_80475BFC, neighborcustomchar.hpp)
  int m_field7C = 0;  // +0x7C
  int m_field80 = 1;  // +0x80
  int m_field84 = 1;  // +0x84
  int m_drawTopmost = 0;  // +0x88 StateMachine::DrawTopmost runs only when set (DAT_80475C0C)
  int m_field8C = 0;  // +0x8C
  int m_field90 = 0;  // +0x90
  int m_field94 = 0;  // +0x94
  int m_field98 = 0;  // +0x98
  int m_field9C = 0;  // +0x9C
  int m_fieldA0 = -1;  // +0xA0
  int m_fieldA4 = 0;  // +0xA4
  int m_fieldA8 = 0;  // +0xA8
  int m_fieldAC = 0;  // +0xAC
  int m_fieldB0 = 0;  // +0xB0
  int m_fieldB4 = 0;  // +0xB4
  int m_fieldB8 = 0;  // +0xB8
  int m_fieldBC = 0;  // +0xBC
  int m_fieldC0 = 0;  // +0xC0
  int m_fieldC4 = 0;  // +0xC4
  int m_fieldC8 = 0;  // +0xC8
  int m_fieldCC = 0;  // +0xCC
  int m_fieldD0 = 0;  // +0xD0
  int m_fieldD4 = 0;  // +0xD4
  int m_fieldD8 = 0;  // +0xD8
  int m_fieldDC = 0;  // +0xDC
  int m_fieldE0 = 0;  // +0xE0
  int m_fieldE4 = 1;  // +0xE4
  int m_fieldE8 = 0;  // +0xE8
  int m_fieldEC = 0;  // +0xEC
  int m_fieldF0 = 0;  // +0xF0
  Player m_players[kMaxPlayers];  // PC mod: 4 players (GC [2]; [2..3]: camera only, mp4.cpp). +0xF4 (DAT_80475C78, DAT_80475C88)
  unsigned int m_copyrightFlags = 0;  // +0x114 1 fading in, 2 fading out, 4 shown, 8 done
  float m_copyrightAlpha = 0;  // +0x118
  float m_copyrightShownAt = 0;  // +0x11C
  float m_copyrightTime = 0;  // +0x120
  int m_field124 = 0;  // +0x124
  int m_buildBuyPlayer = -1;  // +0x128 the player in build/buy mode (-1: none; two-player: only their view is drawn)
};
