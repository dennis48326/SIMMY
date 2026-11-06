// @class AptViewer after E3DWindow EMutex UIObjectBase ECTRL_CMD
// The Flash UI player (game/aptviewer.cpp): an orthographic 3D window over the Apt stage that loads
// the root movie, advances Apt by the frame timer and draws it after the game. _globals.m_aptViewer.
#pragma once

struct AptViewer : E3DWindow {
  AptViewer();  // 800101F0
  ~AptViewer();  // 800102E0
  void CallFunction(char* function, char* unused, char* target, int argc, ...);  // 80010358
  void NewCallFunction(char* function, char* unused, char* target, int argc, ...);  // 80010394
  void NewCallFunction2(char* function, char* unused, char* target, int argc, char** argv);  // 800104B0
  void Shutdown();  // 800105B4
  void Init(UIObjectBase* handler);  // 800105D8
  static void Update();  // 80010640
  void UpdateAll();  // 80010644
  void Draw(ERC* rc);  // 80010778
  void Load(char* name, bool wait);  // 80010C14
  void LoadActionScript(char *, bool);  // 80010C94
  void UnLoad();  // 80010CF0
  int ReadController(int, bool, int, bool);  // 80010D68
  void RepeatClear(int player, int direction);  // 80010D70
  bool RepeatCheck(int player, int direction);  // 80010DAC
  int NewReadController(int, bool, int, bool);  // 80010E7C
  int OnePlayerReadController(int player, int controller);  // 80010F40
  unsigned int PushAptButtonFilter(int player, char* name);  // 80011A48
  void PopAptButtonFilter(int player, unsigned int id);  // 80011B4C
  void AddCmdToAptButtonFilter(int player, unsigned int id, int command);  // 80011C44
  void RemoveCmdFromAptButtonFilter(int player, unsigned int id, int command);  // 80011CA8
  unsigned int GetCurrentAptButtonFilter(int player);  // 80011D0C
  EController* GetPlayersController(int player);  // 80011D50
  void NewSetMessageHandler(UIObjectBase* handler);  // 80011DCC
  void NewRemoveMessageHandler(UIObjectBase* handler);  // 80011DF0
  static int UIOn();  // 80011E14
  bool AllowControllerReading(bool allow);  // 80011E1C (returns the previous setting)
  bool IsWaitingForPlayerInput();  // 80011E2C
  bool IsReadControllerActive();  // 80011E58

  int m_loaded = 0;              // +0x350 a movie is loaded (Load)
  int m_projectionSet = 0;       // +0x354 the stage projection was set up (first Draw)
  EMutex m_mutex;                // +0x358 held around update and draw
  EClock* m_clock = nullptr;     // +0x374
  int m_field378 = 0;            // +0x378
  int m_field37C = 0;            // +0x37C
  char m_movieName[0x20] = {};   // +0x380
  int m_started = 0;             // +0x3A0 StartItAll ran
  int m_readController = 1;      // +0x3A4 Apt may read the pads (AllowControllerReading)
  int m_field3A8 = 1;            // +0x3A8
  int m_field3AC = -1;           // +0x3AC
  struct ButtonFilter {
    unsigned int id = 0;
    const char* name = nullptr;
  };
  ButtonFilter m_filters[2][10];  // +0x3B0 per player: a stack of EController filters
  int m_filterTop[2] = {};        // +0x450
  int m_stageRect[4] = {};       // +0x458 x, y, w, h of the stage on screen
};
