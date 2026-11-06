// @class InteractorModule__Interactor__CallbackData
// @class InteractorModule__WallManipulator after InteractorModule__Interactor EVec3 InteractorModule__Interactor__CallbackData
// @class InteractorModule__WallPainter after InteractorModule__Interactor EVec3 InteractorModule__Interactor__CallbackData
// @class InteractorModule__FloorPainter after InteractorModule__Interactor EVec3 InteractorModule__Interactor__CallbackData
// Build mode's tools (game/wallmanipulator.cpp, game/wallpainter.cpp, game/floorpainter.cpp and their
// helpers in game/interactorutils.cpp): the player's interactors 3 (walls and fences), 4 (wallpaper) and
// 5 (floor tiles), started by the catalogue (PCTTarget::enter_wall_mode / enter_wallpaper_mode /
// enter_floor_mode) and reporting to it through their callback (CallbackData).
//
// Each tool is a cursor on the lot's grid (the Interactor's +0x08, snapped at +0x20) that A anchors; a
// second A builds (or paints, or removes) the run from the anchor to the cursor, B lets go of the anchor
// (or leaves the tool), the L/R trigger held makes the selection a room (a rectangle), Y swaps to the
// next tool (walls -> wallpaper -> floors -> walls). A change is made in the world (cFixedWorldImpl:
// SetWall, SetFloor), checked (ERoom::PreviewWallBuild, EIFloor::TestCreateFloors: undone from a copy of
// the world's array when it fails), then the rooms are computed again and the house is rebuilt
// (EGlobal::RecalcHouse). The callback is told 5 while the selection's count changes (the catalogue's
// price and selling pig), 0 when something was bought or sold (the catalogue takes or gives the money),
// 3 when the tool is left, 4 to swap tools.
//
// PC stand-ins: the interactor manager and the input manager are not ported. A player's tool runs from
// UpdatePlaceManipulator (placemanipulator.cpp) with the pad read through the GC's button and stick
// semantics (803FE0F8 / 803FE118: pad commands 5 -> 0 (A), 6 -> 4 (B), 0x2F -> 1 (Y), 0x17 -> 2 (the
// triggers); the sticks -> 7/8, 0xB/0xC). The InteractorVisualizer (the cursor, the line and room
// previews) is not ported (PreDraw/Draw draw nothing).
#pragma once
#include <vector>

struct PCTTarget;

// GC InteractorModule::FenceSellCount / PatternSellCount / InvStock (8 bytes): a style, pattern or floor
// pattern and how many of it.
struct BuildSellCount {
  int id = 0;
  int count = 0;
};

// GC InteractorModule::Interactor::CallbackData and the tools' subclasses (WallManipulator::CallbackData
// 0x44, WallPainter::CallbackData, the floor painter's).
struct InteractorModule__Interactor__CallbackData {
  int m_state = 0;               // +0x00 ClientNotificationState: 0 done, 3 cancelled, 4 swap, 5 count
  PCTTarget* m_user = nullptr;   // +0x04 the params' user data
                                 // +0x08 vtable
  int m_value = 0;               // +0x0C walls: the style (+0x100); wallpaper: its index; floors: remove mode
  int m_count = 0;               // +0x10 walls / wallpaper: the stock used (or, for 5, the selection's count)
  std::vector<BuildSellCount> m_sells;      // +0x14 walls: FenceSellCount; wallpaper: PatternSellCount
  std::vector<BuildSellCount> m_patterns;   // +0x24 walls: PatternSellCount (their wallpaper)
  std::vector<BuildSellCount> m_inventory;  // +0x34 walls: InvStock
  std::vector<BuildSellCount>* m_stock = nullptr;  // floors +0x14: the painter's stock changes
};

// PC: InteractorModule::Interactor::InteractorParams for the tools (WallManipulator 0x28, WallPainter
// 0x24, FloorPainter 0x28 bytes), as PCTTarget::enter_*_mode fill them.
struct BuildToolParams {
  int id = 0;                    // +0x00 the interactor: 3 walls, 4 wallpaper, 5 floors
  int player = 0;                // +0x04
  PCTTarget* user = nullptr;     // +0x08
  int (*callback)(InteractorModule__Interactor__CallbackData*) = nullptr;  // +0x0C
  // walls: +0x14 fence (1) or wall (0), +0x18 the sell (destruct) tool, +0x1C the FenceData, +0x20 the
  //   style's cell id (1: walls), +0x24 the stock (funds / price; -1 unlimited)
  // wallpaper: +0x14 the sell tool, +0x18 the WallTile row, +0x1C its cell id, +0x20 the stock
  // floors: +0x14 the FloorTile row, +0x18 its pattern index, +0x1C the stock, +0x20 the stock vector,
  //   +0x24 place (1) or remove (0)
  int fence = 0;
  int sell = 0;
  const void* data = nullptr;    // FenceData (PC: see PCTFenceData), WallTile or FloorTile row
  int index = 0;
  int stock = 0;
  int place = 0;
};

struct PCTFenceData;  // pcttarget.hpp

// 0x108 bytes.
struct InteractorModule__WallManipulator : public InteractorModule__Interactor {
  using CallbackData = ::InteractorModule__Interactor__CallbackData;
  InteractorModule__WallManipulator();  // 8021F598
  virtual ~InteractorModule__WallManipulator();  // 8021F648
  virtual void OnCreate();  // 8021F7BC
  virtual void OnDestroy();  // 8021F808
  void Start(BuildToolParams& params);  // 8021F83C OnStart
  virtual void OnStop();  // 8021FB18
  void CancelSession();  // 8021FB80
  void CommittTransaction(int state);  // 8021FBA4
  int FinalizePlacement();  // 802202C0
  int FinalizeWallDel();  // 80220570
  void CountWallsInRoomSelection();  // 80220940
  int FinalizeRoom();  // 80220AFC
  void HandleFinalizeRequest();  // 80221028
  void HandleSwapRequest();  // 802212DC
  void HandleExitRequest();  // 80221344
  void AdjustCursorPosition();  // 80221394
  void ValidateWallSegment();  // 802213C0
  void UpdateStickState();  // 802216B8
  void ClearWallFadeOffList();  // 8022170C
  virtual void Update(float);  // 8022176C
  void OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float) override;  // 802219B4
  void OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand) override;  // 80221A84
  void OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand, float) override;  // 80221B08
  int GetTexGenMode();  // 80221B54
  virtual void PreDraw(InteractorModule__InteractorVisualizer &);  // 80221B7C
  virtual void Draw(InteractorModule__InteractorVisualizer &);  // 80221BA8
  int GetWallLineCost(EVec3& from, EVec3& to, int& any, bool, bool remove);  // 80221BD4
  bool SubmitLine(EVec3& from, EVec3& to, int& count, bool, bool remove);  // 80221DE8
  int DoesNotConflictWithExistingArchitecture(CTilePt& tile, TileWallsSegment segment);  // 80221F78
  int CanChangeTileAdd(CTilePt& tile, TileWallsSegment segment);  // 8022237C
  int CanChangeTileDelete(CTilePt& tile, TileWallsSegment segment);  // 802224C8
  int HandleDeleteLine(CTilePt& from, CTilePt& to, TilePtDir& dir, int);  // 802229E4
  int HandleAddLine(CTilePt from, CTilePt to, TilePtDir& dir, int);  // 80222DF0
  int LegalWallTile(CTilePt& tile, TileWallsSegment segment);  // 80223088
  bool AddWallAtTile(CTilePt& tile, TileWalls& walls, TileWallsSegment segment);  // 80223354
  bool PreviewNRooms();  // 802234D4
  void IncrementSellCountForStyle(WallStyle style);  // 80223584
  int CheckForAffectedWalls(EVec3& from, EVec3& to);  // 802237A0
  void SendBuildItemCountChangeEvent(int count);  // 80223ED0
  int IsAnchored();  // 80223F70
  int IsValidWallSegment();  // 80223F88
  int IsRoomPreview();  // 80223F94
  int IsBuyMode();  // 80223FA0
  int IsSellMode();  // 80223FAC
  int IsFenceMode();  // 80223FB8
  int IsDestructMode();  // 80223FC4
  unsigned int GetFenceShaderID();  // 80223FD0
  void SetAnchorPoint(EVec3 &);  // 80223FF0
  void SetAnchorPointDrawPos(EVec3 &);  // 80224010
  EVec3& GetAnchorPos();  // 80224030
  EVec3& GetAnchorDrawPos();  // 80224038
  static double GetWallHeight();  // 80224040
  double GetAffectedWallHeight();  // 80224048

  // Flags (+0xC4): 1 anchored, 2 anchor at the cursor next update, 4 add, 8 delete, 0x10 room, 0x20
  // finalize, 0x40 exit, 0x80 the selection is valid, 0x100 fences, 0x200 the destruct (sell) tool,
  // 0x400 / 0x800 the stick's first push (one tile), 0x1000 swap tools.
  EVec3 m_anchor;                           // +0x60
  EVec3 m_anchorDraw;                       // +0x6C
  std::vector<BuildSellCount> m_fenceSells;    // +0x78 FenceSellCount
  std::vector<BuildSellCount> m_patternSells;  // +0x88 PatternSellCount
  std::vector<BuildSellCount> m_inventory;     // +0x98 InvStock
  std::vector<EIWallPart*> m_fadeOff;          // +0xA8 the walls shown while the selection deletes them
  EVec3 m_sideOffset;                       // +0xB8 towards the wall's visible side (GetVisibleSideOfWall)
  unsigned int m_flags = 0;                 // +0xC4
  int m_stock = 0;                          // +0xC8 what may still be built (-1 unlimited)
  int m_affected = 0;                       // +0xCC CheckForAffectedWalls' answer
  int m_count = 0;                          // +0xD0 the selection's segments
  int m_style = 1;                          // +0xD4 WallStyle (1 walls, a fence's style)
  const PCTFenceData* m_fence = nullptr;    // +0xD8
  int m_fenceIndex = 0;                     // +0xDC
  BuildToolParams m_params;                 // +0xE0
  int m_startStock = 0;                     // +0x104 the stock at the last commit
};

struct InteractorModule__WallPainter : public InteractorModule__Interactor {
  using CallbackData = ::InteractorModule__Interactor__CallbackData;
  InteractorModule__WallPainter();  // 80224064
  virtual ~InteractorModule__WallPainter();  // 802240D4
  virtual void OnCreate();  // 80224180
  virtual void OnDestroy();  // 802241CC
  void Start(BuildToolParams& params);  // 80224200 OnStart
  virtual void OnStop();  // 8022448C
  void CancelSession();  // 802244C8
  void CommittTransaction(int state);  // 802244EC
  void HandleFinalizeRequest();  // 80224838
  void HandleSwapRequest();  // 802249C4
  void HandleExitRequest();  // 80224A3C
  void AdjustCursorPosition();  // 80224AAC
  void ValidateWallSegment();  // 80224BB8
  void UpdateStickState();  // 80225044
  virtual void Update(float);  // 80225098
  void OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float) override;  // 8022544C
  void OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand) override;  // 80225594
  void OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand, float) override;  // 80225658
  virtual void PreDraw(InteractorModule__InteractorVisualizer &);  // 802256A4
  virtual void Draw(InteractorModule__InteractorVisualizer &);  // 802256D0
  void CountWallsInRoomSelection();  // 802256FC
  int FinalizePaperForRoom();  // 80225D70
  int FinalizePaperForLine();  // 802262B8
  int FinalizeSellPaperForLine();  // 80226390
  int SubmitPaperLine(EVec3& from, EVec3& to, int pattern, int side);  // 80226458
  unsigned int GetShaderID();  // 80226830
  void SendBuildItemCountChangeEvent(int count);  // 8022683C
  int IsAnchored();  // 80226948
  int IsValidWallSegment();  // 80226960
  int IsRoomPreview();  // 8022696C
  int IsBuyMode();  // 80226978
  int IsSellMode();  // 80226984
  double GetCursorRotationAdjustment();  // 80226990
  void SetAnchorPoint(EVec3 &);  // 80226998
  void SetAnchorPointDrawPos(EVec3 &);  // 802269B8
  void SetHoldPos(EVec3 &);  // 802269D8
  EVec3& GetAnchorPos();  // 802269F8
  EVec3& GetAnchorDrawPos();  // 80226A00
  EVec3& GetHoldPos();  // 80226A08
  static double GetPaperToolHeight();  // 80226A10
  static double GetWallHeight();  // 80226A18
  void SaveInHoldPos(EVec3 &);  // 80226A20
  void RestoreFromHoldPos(EVec3 &);  // 80226A58

  // Flags (+0x88): 1 anchored, 2 anchor next update, 4 buy (paint), 8 sell (strip), 0x10 room, 0x20
  // finalize, 0x40 exit, 0x80 valid, 0x100 / 0x200 the stick's first push, 0x400 / 0x800 / 0x1000 the
  // anchor held while the room preview is up, 0x2000 swap tools.
  EVec3 m_anchor;                           // +0x60
  EVec3 m_anchorDraw;                       // +0x6C
  EVec3 m_hold;                             // +0x78
  float m_cursorRotation = 0.0f;            // +0x84
  unsigned int m_flags = 0;                 // +0x88
  int m_field8C = 0;                        // +0x8C
  const void* m_paper = nullptr;            // +0x90 the WallTile row
  int m_stock = 0;                          // +0x94
  int m_count = 0;                          // +0x98 the selection's wall sides
  std::vector<BuildSellCount> m_patternSells;  // +0x9C PatternSellCount
  BuildToolParams m_params;                 // +0xAC
  int m_startStock = 0;                     // +0xCC
};

struct InteractorModule__FloorPainter : public InteractorModule__Interactor {
  InteractorModule__FloorPainter();  // 80206224
  virtual ~InteractorModule__FloorPainter();  // 802062B8
  virtual void OnCreate();  // 80206364
  virtual void OnDestroy();  // 802063D8
  void Start(BuildToolParams& params);  // 80206424 OnStart
  virtual void OnStop();  // 80206684
  void UpdateStickState();  // 802066E0
  virtual void Update(float);  // 80206734
  void SendBuildItemCountChangeEvent(int count);  // 80206D5C
  void SendSuccessEvent(int count);  // 80206DDC
  void HandleActionCmdInRemoveMode();  // 80206E5C
  void UpdateStockChanges(int* counts, int n);  // 8020707C
  void HandleActionCmdInPlaceMode();  // 80207278
  void OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float) override;  // 80207418
  void OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand) override;  // 802078B0
  void OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand, float) override;  // 8020796C
  virtual void PreDraw(InteractorModule__InteractorVisualizer &);  // 802079B8
  virtual void Draw(InteractorModule__InteractorVisualizer &);  // 802079E4
  static int IsValidFloorPlacement(CTilePt& tile, int pattern);  // 80207A10
  int SetFloorTile(CTilePt* tile, int pattern, int* counts);  // 80207AE0
  int SetFloorTile(CTilePt& tile, int pattern, Room* room, int* counts);  // 80207C94
  void RemoveFloorTile(CTilePt* tile, int* counts);  // 80207E7C
  void GetSelectedRectPoints(int& x0, int& y0, int& x1, int& y1);  // 80208080
  int PaintFloor(int* counts);  // 8020812C
  int RemoveFloor(int* counts);  // 802083A4
  int PaintRoom(int* counts);  // 80208654
  int RemoveRoom(int* counts);  // 80208954
  void CheckDiagForRoomContainment(Room *, CTilePt &, TileWalls &, DiagonalSideSelector &, DiagonalSideSelector &);  // 80208C74
  void CancelSession();  // 80208DA0
  void SwapTools();  // 80208E20
  bool CurrentSelectionIsValid(int& count);  // 80208ECC
  int IsInAnchorState();  // 8020913C
  bool IsInFreeState();  // 80209148
  int IsInBuildState();  // 80209158
  int IsInRemoveState();  // 80209164
  int IsInFillState();  // 80209170
  int IsInConfirmedState();  // 8020917C
  int IsInInvalidState();  // 80209188
  EVec3& GetAnchorPos();  // 80209194
  const void* GetSelectedFloorData();  // 8020919C
  int IsInPlaceMode();  // 802091A4
  bool IsInRemoveMode();  // 802091AC

  // State (+0x60): 1 anchored (a rectangle), 2 a room, 4 place, 8 remove, 0x10 busy, 0x20 invalid,
  // 0x40 / 0x80 the stick's first push.
  unsigned char m_state = 0;                // +0x60
  EVec3 m_anchor;                           // +0x64
  int m_field70[3] = {};                    // +0x70
  int m_stock = 0;                          // +0x7C
  BuildToolParams m_params;                 // +0x80
  std::vector<BuildSellCount> m_ownStock;   // +0xA8 (when the params bring none)
  std::vector<int> m_counts;                // +0xB8 per floor pattern
  std::vector<BuildSellCount>* m_stockChanges = nullptr;  // +0xA0
};

// PC glue (see buildtools.cpp). GC: InteractorManager::ChangeActiveInteractor(player, 3/4/5, params) and
// ChangeActiveInteractorToNull.
bool StartBuildTool(BuildToolParams& params);
void StopBuildTool(int player);
// Each frame (from UpdatePlaceManipulator): the player's tool, if any, reads the pad and updates. true:
// a tool is active.
bool UpdateBuildTool(int player, float dt);
// The active tool's cursor as the player's interactor info (the camera follows it). false: none.
bool BuildToolInfo(int player, InteractorModule__InteractorInfo& info);

// game/interactorutils.cpp's helpers (InteractorModule::), in buildtools.cpp.
namespace ibuild {
void SnapPositionToTile(const EVec3& in, EVec3& out, int snap);                       // 802106C4
void ClampPosToWorld(EVec3& pos);                                                     // 80210F7C
void MoveCursorOneTileAlongCameraTargetVector(EVec3& pos, const EVec2& stick, ESimsCam* cam, int snap);  // 80210C58
void MoveCursorAlongCameraTargetVector(float speed, EVec3& pos, EVec2& stick, ESimsCam* cam);  // 8021097C
void ConvertVertsToTiles(const EVec3& from, const EVec3& to, CTilePt& a, CTilePt& b);  // 80212D58
int GetVisibleSideOfWall(ESimsCam* cam, const EVec3& a, const EVec3& b, EVec3* offset);  // 80212AD8
void ForcePointDir(CTilePt& a, CTilePt& b);                                           // 802132D8
void EorGetAdjacentTile(int& segment, int front, int& side, CTilePt& a, CTilePt& b);  // 8021358C
void IncrementSellCountForPattern(int pattern, std::vector<BuildSellCount>& counts);  // 80213828
// PC helpers: the build sets' rows (EGlobal +0xD4 / +0xD8 / +0xDC), undo copies of a world array
// (_c2DArray::CopyFrom), the objects on a tile (ObjectIterator), the rooms in use.
const unsigned char* SetArray(const void* set);
int SetCount(const void* set);
const void* SetRow(const void* set, int i);
std::vector<unsigned char> SaveArray(const _c2DArray* a);
void RestoreArray(_c2DArray* a, const std::vector<unsigned char>& saved);
std::vector<cXObjectImpl*> ObjectsOnTile(const CTilePt& tile);
int CountRoomsInUse();
void SetCameraFilter(int player);    // 8021374C
void ClearCameraFilter(int player);  // 802137C0
}  // namespace ibuild
