// @class ESim after ISimInstance SimModel cXPerson EOrderTableData ELights ESims3DHead
// @class ESimShadow
// A sim's 3D instance in the house (game/esim.cpp): its SimModel (the body parts built from the
// person's CasSimDescriptionS2C, skinned by the composited skin) posed by the ISimInstance's animation
// controller on the sim skeleton, lit at its bound sphere with an extra light from the camera, drawn
// through SimOrderTableCallback. GC: the person's animator (SAnimator2) drives the controller and its
// position; ESimsDataManager builds the model on its own thread (QueueCommand / tProcessCommand).
// PC: initModel runs on the game thread (ESim::Update).
#pragma once

// 0x648 bytes.
struct ESim : public ISimInstance {
  explicit ESim(cXPerson* person);  // 80030B9C
  ESim();  // 800351AC
  SIMMY_STORABLE_DECL(ESim)
  ~ESim() override;  // 800310EC
  int IsMale();  // 80031790
  int IsFemale();  // 800317C8
  void initModel();  // 8003193C
  static void SimOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 80032C58
  void DrawSimModel(ERC* rc, EMat4* matrices, unsigned int count);  // 80032EF8
  void DoAnimation() override;  // 800334B4
  void DoLightingCalculation() override;  // 800334F8
  void Draw(ELevelDrawData& data) override;  // 800334FC
  void Update() override;  // 80034090
  unsigned int VisibilityTest(E3DWindow& win) override;  // 800341E8
  void CompositeSkin();  // 8003461C
  int HasQueuedOperation();  // 8003489C
  void DrawSim(bool draw);  // 8003494C
  void UpdatePlumbBob();  // 80034AF0
  cXPerson* GetPerson();  // 80035314
  SimModel* GetSimModel();  // 80035338
  int GetIsModelLoaded();  // 8003536C
  void SetXOb(cXObject* xob);  // 80035308
  static void DeletePlayerCachedModel();  // 800312C8
  RET_UNKNOWN ReadModelFromCache();  // 80031360
  void TransferModelToCache();  // 8003154C
  void flushQueuedCostumeModels();  // 8003176C
  int IsAdult();  // 800317EC
  void IsChild();  // 80031824
  void IsDog();  // 8003185C
  void IsCat();  // 80031894
  void IsMonkey();  // 800318CC
  void IsPet();  // 80031904
  // The player's plumb bob over the sim, shrinking into a meter (3, 4) or growing back (7).
  void DrawPlumbBob(ERC* rc);  // 80031F94
  double UpdateSkillMeter(EVec3 &, EVec3 &);  // 800321A0
  void DrawSkillMeter(ERC *);  // 80032488
  // The plumb bob / skill meter state (+0x3EC): 0 plumb bob; 1 skill (2 repair) meter asked for: from
  // the plumb bob it shrinks away first (3 / 4), else it changes over (8 / 9); back to the plumb bob:
  // 5 / 6 shrink the meter, then 7 grows the plumb bob; 10 hidden.
  void SetPlumbBobState(unsigned int state);  // 80032928
  static void PropOrderTableCallback(ELevelDrawData &, EOrderTableData *);  // 80032B84
  static void CensorOrderTableCallback(ELevelDrawData &, EOrderTableData *);  // 80032D68
  static void ServiceNpcOrderTableCallback(ELevelDrawData &, EOrderTableData *);  // 80032F34
  void DrawSimAndNpcCommonItems(ERC *);  // 80032FFC
  static void SkillMeterOrderTableCallback(ELevelDrawData &, EOrderTableData *);  // 8003317C
  void DrawCursorHighLight(ERC *);  // 80033218
  void SetAnim(char *);  // 800342CC
  // The shower curtain fades in (half a second at speed 1) while the outfit changes or the person's data
  // 0x1E has 0x80, else out.
  void UpdateShowerCurtain();  // 80034318
  // An outfit change: once the curtain covers the sim, the new costume's models (ChangeCostume; GC: on
  // the data manager's thread), then done.
  void UpdateSkinChange();  // 80034464
  // Starts an outfit change (none running or queued). 1: started.
  int CreateSkinAsync();  // 800345C4
  void CreateThumbnail();  // 80034690
  void UpdateRepShaders(int);  // 80034740
  void RefreshSkin();  // 800347DC
  void tProcessCommand(unsigned int);  // 80034860
  void UpdateQueuedOperation();  // 800348B8
  double GetScaler();  // 80034908
  virtual RET_UNKNOWN GetObCenter();  // 800349A8
  RET_UNKNOWN GetPlayerIndex();  // 80034AB8
  void UpdateShadow();  // 80034B14
  void UpdateGhostParticleEffect();  // 80034C38
  void ChangeCostume();  // 80034DE4
  static void RegisterType(unsigned short);  // 80035138
  void CreateCopy() const;  // 8003518C
  EIStaticModel* GetShadow() override;  // 8003530C
  RET_UNKNOWN GetModelPart(int);  // 8003531C
  RET_UNKNOWN GetSimHead();  // 80035330
  void SetVanityDraw(bool, unsigned int);  // 80035340
  RET_UNKNOWN UseVanityDraw(unsigned int *);  // 8003534C
  RET_UNKNOWN GetPlumbBobState();  // 8003535C
  void SetSkillmeterValue(float);  // 80035364
  int GetIsChangingOutfit();  // 80035374

  // PC: drawn as a ghost (the person's IsGhost; dev aid SIMMY_GHOST_TEST: ghost.cpp).
  bool IsGhostSim();
  // PC: where the sim stands in the world (GC: callers ask the animator, SAnimator2 vtable 0xE4).
  EVec3 GetSimPos();

  cXPerson* m_person = nullptr;           // +0x3D0
  int m_field3D4 = 0;                     // +0x3D4
  int m_changingOutfit = 0;               // +0x3D8 an outfit change is wanted or running
  EIStaticModel* m_shadow = nullptr;      // +0x3E0 the shadow (model 0x6ef2f2da)
  int m_plumbBobState = 0;                // +0x3EC
  float m_skillValue = 0.0f;              // +0x3F0 the skill meter's fill (0..1)
  float m_plumbBobTime = 0.0f;            // +0x3F4
  EResource* m_ghostEffects = nullptr;    // +0x404 the ghost effect's attachment while a ghost (0x503FE21C)
  EResource* m_meterModel = nullptr;      // +0x3E8 the plumb bob's or skill meter's model
  int m_field420 = 1;                     // +0x420
  int m_field424 = 0;                     // +0x424 (UpdateSkinChange: change at once)
  int m_drawSim = 0;                      // +0x428 drawn (DrawSim)
  ERModel* m_curtainModel = nullptr;      // +0x42C the shower curtain (model 0x3887fccd, initModel)
  int m_curtain = 0;                      // +0x430 the shower curtain is up
  int m_curtainWanted = 0;                // +0x434
  float m_curtainAlpha = 0.0f;            // +0x438
  int m_queuedOps = 0;                    // +0x43C operations waiting for the data manager
  EOrderTableData m_otd;                  // +0x440 the sim's draw record
  EOrderTableData m_censorOtd;            // +0x478 its censor mosaic (CensorOrderTableCallback; sorted first)
  EOrderTableData m_meterOtd;             // +0x4B0 its plumb bob / skill meter record (SkillMeterOrderTableCallback)
  EOrderTableData m_propOtd;              // +0x4E8 the props it wears (PropOrderTableCallback)
  int m_changingSkin = 0;                 // +0x520
  int m_drawModel = 0;                    // +0x528 the model is drawn this frame
  int m_freeMorphs = 0;                   // +0x52C the morph data can go after the first draw
  ELights m_simLights;                    // +0x530 the lights before the sim's own adjustments
  ESims3DHead* m_head = nullptr;          // +0x628 the head in the HUD's mood widget
  int m_skinChange = -1;                  // +0x630 outfit change: -1 none, 0 asked, 1 making, 2 made
  int m_firstDraw = 1;                    // +0x634
  int m_modelReady = 0;                   // +0x638 initModel has run
  SimModel* m_simModel = nullptr;         // +0x63C
  int m_wasGhost = 0;                     // +0x640
  // The cursor highlight and the changing curtain (DrawSimAndNpcCommonItems). m_field420 (+0x420) is
  // the highlight pulse's other scale index.
  ERModel* m_hlModels[3] = {};            // +0x408 the highlight's rings (0xc1a394fd, 0x528d93c8), 0x68de8ae2
  float m_hlTime = 0.0f;                  // +0x418 into the highlight's pulse (0.4 s)
  int m_hlFrom = 0;                       // +0x41C the pulse's scale index (1.1 / 1.6), swapped each pulse
};

// The shadow under a sim (an EIStaticModel, 0x328 bytes).
struct ESimShadow : public EIStaticModel {
  ESimShadow(cXPerson *);  // 80034F78
  ~ESimShadow() override;  // 80034FBC
  void DrawShadow(ELevelDrawData &) override;  // 80032E34
  void GetShadowCenter(EVec3 &) const override;  // 80032E88
  cXPerson* m_person = nullptr;           // +0x320
};
