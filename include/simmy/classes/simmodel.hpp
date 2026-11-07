// @class CasSimDescriptionS2C after DigestGeomShaderPtr EIGameInstance EXPORT_DIRECTORY EACNodeState
// @class CasSimPartsS2C
// @class SimModel
// @class SimRenderer
// @class MorphUtilities
// @class Sim__OuterLayerCollarStyleS2C
// @class Sim__OuterLayerTorsoStyleS2C
// @class Sim__SimPartsS2CParam
// @class eBodyPartS2C
// @class eBodyTypeS2C
// @class eSimPartsMapLocation
// @class eSkinColorType
// A sim's look and its 3D body (game/cassimdescription.cpp, cassimparts.cpp, simmodel.cpp,
// simrenderer.cpp). A CasSimDescriptionS2C says which style (index into the sim parts tables),
// texture and options each body part uses; CasSimPartsS2C reads the "Sim::SimPartsS2C" quickdata row
// of the gender for the parts' model and texture ids; SimModel holds one EIStaticModel per body part
// (built from those ids) and the sim's composited skin; SimRenderer draws the parts, skinned by the
// sim's EAnimController.
#pragma once
#include <string>

// The body parts (eBodyPartS2C), in the order SimModel keeps them.
struct eBodyPartS2C {
  SIMMY_ENUM_BODY(eBodyPartS2C)
  enum : int {
    kGlasses = 0, kHead = 1, kHair0 = 2, kHairHat = 3, kHair1 = 4, kHair2 = 5, kHair3 = 6,
    kInnerTorso = 7, kInnerSleeve = 8, kMidTorso = 9, kMidCollar = 10, kMidSleeve = 11,
    kOuterTorso = 12, kOuterCollar = 13, kOuterSleeve = 14, kLowerBody = 15, kShoes = 16, kBootTop = 17,
    kFacialFeature = 18, kHat = 19, kNecklaceEarring = 20, kArmAccessory = 21, kBelt = 22,
    kMusicPlayer = 23, kBeltExtra = 24, kVestScarf = 25, kCount = 26
  };
};

// The four quadrant maps of the composited skin (upper left/right, lower left/right).
struct eSimPartsMapLocation {
  SIMMY_ENUM_BODY(eSimPartsMapLocation)
  enum : int { kUL = 0, kUR = 1, kLL = 2, kLR = 3, kCount = 4 };
};
// caucasian, asian, brown, darkbrown, pale, alien; 6..10 the same (5: alien) aged.
struct eSkinColorType {
  SIMMY_ENUM_BODY(eSkinColorType)
};
// 2: buff.
struct eBodyTypeS2C {
  SIMMY_ENUM_BODY(eBodyTypeS2C)
};
struct Sim__OuterLayerTorsoStyleS2C {
  SIMMY_ENUM_BODY(Sim__OuterLayerTorsoStyleS2C)
};
struct Sim__OuterLayerCollarStyleS2C {
  SIMMY_ENUM_BODY(Sim__OuterLayerCollarStyleS2C)
};

// A body part's rectangle in its skin map (SimModel::s_SimPartsMap, 10 bytes on GC), in units of the
// game's 128-pixel map.
struct SimPartsMapEntry {
  unsigned char m_part;
  unsigned char m_location;  // eSimPartsMapLocation
  unsigned short m_x0, m_y0, m_x1, m_y1;
};

// What a part's description adds to its style (Sim::SimPartsS2CParam): its texture and up to three
// options (sleeve, collar or torso style, ...).
struct Sim__SimPartsS2CParam {
  signed char m_texture = 0;  // +0x00
  int m_option1 = 0;          // +0x04
  int m_option2 = 0;          // +0x08
  int m_option3 = 0;          // +0x0C
};

// 0x124 bytes (saved as is in the neighbourhood and costume files).
struct CasSimDescriptionS2C {
  CasSimDescriptionS2C();  // 801672FC
  void ClearSim(bool keepIdentity);  // 80167330
  explicit CasSimDescriptionS2C(bool male);  // 80167540
  int GetBodyPartIndex(eBodyPartS2C part) const;  // 801675B0
  void SetBodyPartIndex(eBodyPartS2C, signed char);  // 801676F0
  int GetBodyPartTextureIndex(eBodyPartS2C part) const;  // 801677F0
  void SetBodyPartTextureIndex(eBodyPartS2C, signed char);  // 801678FC
  Sim__SimPartsS2CParam GetBodyPartParam(eBodyPartS2C part) const;  // 801679E4
  void ReadOldDescription(ReconBuffer *);  // 80167B7C
  void DoStream(ReconBuffer* buffer, int version);  // 80167F00
  void AssertWithInfo(char *);  // 80168408
  void Verify();  // 8016850C
  void VerifyBodyPartNoTexture(CasSimPartsS2C &, eBodyPartS2C, signed char &, signed char, bool);  // 80169468
  RET_UNKNOWN VerifyBodyPartWithNoOptions(CasSimPartsS2C &, eBodyPartS2C, signed char &, signed char &, signed char, signed char, bool);  // 80169578
  RET_UNKNOWN VerifyBodyPartWithOneOption(CasSimPartsS2C &, eBodyPartS2C, signed char &, signed char &, signed char &, signed char, signed char, signed char, bool);  // 8016977C
  void SetInnerLayerTorsoStyle(CasSimPartsS2C &);  // 801699FC
  void SwitchInnerLayerTorso(bool);  // 80169A40
  void SwitchMidLayerTorso(bool);  // 80169A5C
  void SetMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C, CasSimPartsS2C &);  // 80169AF8
  bool InnerLayerHalfTorsoRequired(CasSimPartsS2C &);  // 80169B38
  bool SwitchInnerLayerSleeveToFirstAvailableStyle(CasSimPartsS2C &);  // 80169BB4
  bool MidLayerHalfTorsoRequired(CasSimPartsS2C &);  // 80169CB4
  void SwitchMidLayerTorsoToFirstAvailableStyle(CasSimPartsS2C &, bool);  // 80169D30
  void SwitchMidLayerCollarToFirstAvailableStyle(CasSimPartsS2C &);  // 80169E84
  void SwitchMidLayerSleeveToFirstAvailableStyle(CasSimPartsS2C &);  // 80169F24
  void SwitchOuterLayerTorsoToFirstAvailableStyle(CasSimPartsS2C &);  // 80169FFC
  void SwitchOuterLayerCollarToFirstAvailableStyle(CasSimPartsS2C &);  // 8016A09C
  void SwitchOuterLayerSleeveToFirstAvailableStyle(CasSimPartsS2C &);  // 8016A1AC
  void SwitchLowerBodyToFirstAvailableStyle(CasSimPartsS2C &);  // 8016A24C
  void SwitchHatToFirstAvailableStyle(CasSimPartsS2C &);  // 8016A35C
  int ShouldBootTopBeDisplayed(CasSimPartsS2C &);  // 8016A460
  void SetShoeBoottopDisplayedFlag(CasSimPartsS2C &);  // 8016A4E8
  void SetBodyTypeMorphValues();  // 8016A518
  void InitializeToDefaultCostume(bool male);  // 8016A5F8
  unsigned int GetCompositeTextureID(const char* name, eSimPartsMapLocation location) const;  // 8016A670
  std::string GetCompositeTextureName(const char* name, eSimPartsMapLocation location) const;  // 8016A6CC
  int GetHairBodyPart();  // 8016A824
  void WriteSimDescriptionToTextFile(__sFILE *, SimModel *);  // 8016A890

  unsigned short m_magic = 0xfeed;        // +0x00
  unsigned char m_version = 2;            // +0x02
  unsigned short m_firstName[32] = {};    // +0x04
  unsigned short m_lastName[32] = {};     // +0x44
  int m_male = 0;                         // +0x84
  signed char m_bodyType = 0;             // +0x88 eBodyTypeS2C
  signed char m_skinColor = 0;            // +0x89 eSkinColorType
  signed char m_glasses = 0;              // +0x8A part 0
  signed char m_glassesTexture = 0;       // +0x8B
  signed char m_head = 0;                 // +0x8C part 1
  signed char m_hair = 0;                 // +0x8D parts 2..6
  signed char m_hairTexture = 0;          // +0x8E parts 1..6
  int m_hairStyle = 0;                    // +0x90 which hair part (0..3)
  int m_hairUnderHat = 0;                 // +0x94 (part 3)
  signed char m_innerTop = 0;             // +0x98 parts 7, 8
  signed char m_innerTopTexture = 0;      // +0x99
  int m_innerOption1 = 0;                 // +0x9C
  int m_innerOption2 = 3;                 // +0xA0 sleeve style
  signed char m_midTop = 0;               // +0xA4 parts 9..11
  signed char m_midTopTexture = 0;        // +0xA5
  int m_midOption1 = 0;                   // +0xA8 torso style
  int m_midOption2 = 0;                   // +0xAC collar style
  int m_midOption3 = 3;                   // +0xB0 sleeve style
  signed char m_outerTop = 0;             // +0xB4 parts 12..14
  signed char m_outerTopTexture = 0;      // +0xB5
  int m_outerOption1 = 0;                 // +0xB8 torso style
  int m_outerOption2 = 0;                 // +0xBC collar style
  int m_outerOption3 = 3;                 // +0xC0 sleeve style
  signed char m_lowerBody = 0;            // +0xC4 part 15
  signed char m_lowerBodyTexture = 0;     // +0xC5
  int m_lowerBodyOption = 0;              // +0xC8 (part 17 shown)
  signed char m_shoes = 0;                // +0xCC parts 16, 17
  signed char m_shoesTexture = 0;         // +0xCD
  int m_bootTop = 0;                      // +0xD0
  signed char m_facialFeature = 0;        // +0xD4 part 18
  signed char m_hat = 0;                  // +0xD5 part 19
  signed char m_hatTexture = 0;           // +0xD6
  int m_hatOption = 0;                    // +0xD8
  signed char m_belt = 0;                 // +0xDC parts 22..24
  signed char m_beltTexture = 0;          // +0xDD
  int m_musicPlayer = 0;                  // +0xE0 (part 23)
  int m_beltExtra = 0;                    // +0xE4 (part 24)
  signed char m_vestScarf = 0;            // +0xE8 part 25
  signed char m_vestScarfTexture = 0;     // +0xE9
  signed char m_armAccessory = 0;         // +0xEA part 21
  signed char m_armAccessoryTexture = 0;  // +0xEB
  int m_armAccessoryOption = 0;           // +0xEC
  signed char m_necklace = 0;             // +0xF0 part 20
  signed char m_necklaceTexture = 0;      // +0xF1
  unsigned char m_tattoos[3] = {};        // +0xF2
  float m_morphBody = 1.0f;               // +0xF8 body morphs (SetBodyTypeMorphValues)
  float m_morphArms = 1.0f;               // +0xFC
  float m_morphLegs = 1.0f;               // +0x100
  float m_morphHead = 3.0f;               // +0x104
  float m_morph108 = 1.0f;                // +0x108
  float m_morph10C = 1.0f;                // +0x10C
  float m_morph110 = 1.0f;                // +0x110
  float m_morph114 = 1.0f;                // +0x114
  int m_precomposited = 0;                // +0x118 the skin textures are already made (saved sims)
  unsigned char m_personality[5] = {};    // +0x11C
  unsigned char m_zodiac = 0;             // +0x121
};

// 0x10 bytes.
struct CasSimPartsS2C {
  CasSimPartsS2C();  // 8016A894
  ~CasSimPartsS2C();  // 8016A8F8
  CasSimPartsS2C(const CasSimPartsS2C&) = delete;
  CasSimPartsS2C& operator=(const CasSimPartsS2C&) = delete;
  void SetGender(bool male);  // 8016A950
  unsigned int GetNumBodyModels(eBodyPartS2C part) const;  // 8016A9FC
  unsigned int GetNumBodyTextures(eBodyPartS2C, unsigned int) const;  // 8016AB48
  unsigned int GetNumTattooTextures(eTattooTextureTypeS2C) const;  // 8016AD14
  unsigned int GetModelID(eBodyPartS2C part, unsigned int index, Sim__SimPartsS2CParam param) const;  // 8016AD5C
  unsigned int GetSkinTextureID(eSkinColorType skin, eBodyTypeS2C body, eSimPartsMapLocation location);  // 8016B5E0
  unsigned int GetSkinTexturesDatasetID(eSkinColorType skin, eBodyTypeS2C body);  // 8016B768
  RET_UNKNOWN GetTextureIDFromVector(VECTOR<unsigned int> &, int, bool, unsigned char, char *) const;  // 8016B8BC
  unsigned int GetTextureID(eBodyPartS2C part, CasSimDescriptionS2C* desc, bool reflection) const;  // 8016B910
  unsigned int GetReflectionTextureID(eBodyPartS2C part, CasSimDescriptionS2C* desc) const;  // 8016C82C
  unsigned int GetTattooTextureID(eTattooTextureTypeS2C type, unsigned int index) const;  // 8016C850
  RET_UNKNOWN IsGamecubeOnly(eBodyPartS2C, unsigned int) const;  // 8016C8E8
  unsigned int GetIconTextureId(eBodyPartS2C, unsigned int) const;  // 8016CAC4
  unsigned int GetIconTattooTextureId(eTattooTextureTypeS2C, unsigned int) const;  // 8016CC9C
  int OuterLayerTorsoRequiresFullTorsoMeshInMidLayer(unsigned int) const;  // 8016CD30
  int OuterLayerTorsoHidesMusicPlayer(unsigned int index) const;  // 8016CD58
  int LongSleeveDoesNotHideArmAccessory(eBodyPartS2C part, unsigned int index) const;  // 8016CD80
  int OuterLayerCollarIsAHood(unsigned int index, Sim__OuterLayerCollarStyleS2C style) const;  // 8016CDD8
  int OuterLayerTorsoIsAHood(unsigned int index, Sim__OuterLayerTorsoStyleS2C style) const;  // 8016CE80
  int CanBeWornSleeveless(eBodyPartS2C, unsigned int, Sim__SimPartsS2CParam) const;  // 8016CED8
  RET_UNKNOWN MidLayerTorsoHidesBelt(unsigned int, Sim__MidLayerTorsoStyleS2C, unsigned int) const;  // 8016CF78
  RET_UNKNOWN OuterLayerTorsoHidesBelt(unsigned int, Sim__OuterLayerTorsoStyleS2C, unsigned int) const;  // 8016CFF8
  int GetUnlockGroupNum(eBodyPartS2C, unsigned int) const;  // 8016D078
  int GetUnlockGroupNum(eTattooTextureTypeS2C, unsigned int) const;  // 8016D294
  int GetTattoosForLocation(eTattooTextureTypeS2C, unsigned char *) const;  // 8016D388
  int ConvertTattooTextureTypeToLocation(eTattooTextureTypeS2C) const;  // 8016D41C
  void GetIndexFromResId(eBodyPartS2C, int, Sim__SimPartsS2CParam, int *);  // 8016D460
  unsigned int GetGameStateModelID(unsigned int id) const;  // 8016D508
  unsigned int GetGameStateTextureID(unsigned int id) const;  // 8016D534
  RET_UNKNOWN GetWeldableBodyPartConnections(eBodyPartS2C, unsigned int *, unsigned int);  // 8016D560
  RET_UNKNOWN GetNumBodyPartOptions(eBodyPartS2C);  // 8016D730
  RET_UNKNOWN GetBodyPartName(eBodyPartS2C);  // 8016D820
  void VerifyBodyPartsData();  // 8016D8B4

  // The texture id at entry `texture` of the id array at `field` of `rec` (GetTextureIDFromVector).
  static unsigned int TextureIDFromArray(const unsigned char* rec, unsigned int field, int texture);

  // The `index`-th record (`stride` bytes) of the parts array at `off` in the gender's row, and the
  // array's length (stored before it). PC: quickdata pointers are resolved through ERQuickdata.
  const unsigned char* Record(unsigned int off, unsigned int index, unsigned int stride) const;
  unsigned int ArrayLength(unsigned int off) const;

  ERQuickdata* m_quickdata = nullptr;  // +0x00 0x2a2af469
  const void* m_row = nullptr;         // +0x04 the "Sim::SimPartsS2C" row of the gender
  int m_male = 1;                      // +0x08
  int m_lowRes = 0;                    // +0x0C game (not CAS) models: the "_LOD" ids
};

// 0x168 bytes.
struct SimModel {
  // m_partFlags bits.
  enum : unsigned int {
    kChanged = 2, kShared = 4, kSkinApplied = 8, kHasModel = 0x10, kLoading = 0x20, kLoaded = 0x40
  };

  SimModel();  // 80070580
  ~SimModel();  // 8007068C
  SimModel(const SimModel&) = delete;
  SimModel& operator=(const SimModel&) = delete;
  void Init(CasSimDescriptionS2C* desc, bool highRes);  // 80070718
  void Reset();  // 8007077C
  void DeallocateAllModels();  // 80070808
  void DeallocateModelPart(eBodyPartS2C part);  // 8007084C
  void DeallocateMorphResources();  // 800708F4
  void DeallocateAllSkinTextures();  // 8007096C
  void SetSimDescription(CasSimDescriptionS2C* desc);  // 800709D0
  void SetHighResolution(bool highRes);  // 80070A34
  void DetachSimDescription();  // 80070A48
  RET_UNKNOWN Clone();  // 80070B48
  void SetAllModels();  // 80070CC4
  int SetModelPart(eBodyPartS2C part, unsigned int id);  // 80070E64
  int UpdateModelPart(eBodyPartS2C part, bool lattice, bool composite);  // 80070FB4
  RET_UNKNOWN UpdateQueuedModelPart(eBodyPartS2C, bool, bool);  // 80071034
  int ChangeModelPart(eBodyPartS2C part, unsigned int id, bool lattice, bool composite);  // 8007115C
  RET_UNKNOWN GetChangedModels(int *);  // 800712A4
  RET_UNKNOWN SetModelPartASync(eBodyPartS2C, unsigned int);  // 80071374
  int IsLoadingModels() const;  // 80071434
  void FlushLoadingModels();  // 80071460
  RET_UNKNOWN VerifyRequiredModels() const;  // 800714C4
  void WeldAllSharedVertices();  // 800714CC
  void WeldSharedVertices(eBodyPartS2C, eBodyPartS2C, bool);  // 80071558
  void ApplyAllLatticeMorphing();  // 80071620
  void ApplyLatticeMorphingToModelPart(eBodyPartS2C);  // 8007172C
  void ApplyAllMorphTargets();  // 80071B1C
  void ApplyMorphTargetsToRegion(unsigned int base, unsigned int count, float value, unsigned int* ids);  // 80071E18
  int CreateSkin(char* name);  // 80072064
  static void GetSkinTextureDef(ETextureDef& def, bool highRes);  // 80072354
  void ApplySkinToModels();  // 800723C8
  void ApplySkinToModelPart(eBodyPartS2C part);  // 8007240C
  void TransformAllModelUV();  // 800725D4
  void TransformModelPartUV(eBodyPartS2C);  // 80072618
  void GetModelUVTransformParameters(eBodyPartS2C part, EVec2& scale, EVec2& offset, unsigned int& secondShader,
                                     EVec2& secondScale, EVec2& secondOffset);  // 800726A8
  void CompositeAllSkin();  // 80072B20
  void CompositeSkinPart(unsigned char quadrant);  // 80072BE4
  void UpdateReweld(bool);  // 80072CBC
  void UpdateReweldCAS(bool);  // 80072D3C
  void UpdateSimDescriptionIndices(unsigned int *);  // 80072D94
  void RebuildModified();  // 80072E30
  void LoadAllMorphTargets();  // 80072EB8
  static unsigned int GetGameStateModelID(unsigned int id, bool highRes);  // 8007302C
  static unsigned int GetGameStateTextureID(unsigned int id, bool highRes);  // 80073090
  int UsingHighResolutionModel();  // 800730F4
  void UpdateInnerLayerTorso(bool update, bool composite, bool morph);  // 80073114
  void UpdateInnerLayerSleeve(bool update, bool composite, bool morph);  // 800731F8
  void UpdateMidLayerTorso(bool update, bool composite, bool morph, bool under);  // 800732CC
  void UpdateMidLayerSleeve(bool update, bool composite, bool morph, bool under);  // 800733A4
  void UpdateMidLayerCollar(bool morph);  // 800734AC
  void UpdateOuterLayerTorso(bool update, bool composite, bool morph, bool under);  // 80073544
  void UpdateOuterLayerSleeve(bool update, bool composite, bool morph, bool under);  // 80073698
  void UpdateOuterLayerCollar(bool morph, bool composite);  // 80073778
  void UpdateLowerBody(bool update, bool composite);  // 80073820
  void UpdateHair(bool update, bool composite, bool morph);  // 800738F0
  void UpdateHead(bool update, bool composite, bool morph);  // 80073ACC
  void UpdateHat(bool update, bool composite, bool morph);  // 80073B78
  void UpdateArmAccessory(bool update, bool composite, bool morph);  // 80073C1C
  void UpdateVestScarf(bool update, bool composite, bool morph);  // 80073D60
  void UpdateNecklaceEarring(bool update, bool composite, bool morph);  // 80073E38
  void UpdateBelt(bool update, bool composite, bool morph);  // 80073EDC
  void UpdateShoes(bool update, bool composite, bool morph);  // 80074148
  void UpdateFacialFeature(bool update, bool composite, bool morph);  // 80074220
  void UpdateGlasses(bool update, bool composite, bool morph);  // 800742B0
  RET_UNKNOWN GetModelName(eBodyPartS2C, EString &);  // 80074340
  RET_UNKNOWN GetTextureName(eBodyPartS2C, EString &);  // 800743B0
  RET_UNKNOWN GetTattooTextureName(eTattooTextureTypeS2C, EString &);  // 80074464

  bool HasPart(int part) const { return m_parts[part] && m_parts[part]->m_model; }
  // The outer layer's torso or collar style of the description is a hood (its skin is in map 2).
  int OuterTorsoIsAHood() const;
  int OuterCollarIsAHood() const;

  static const SimPartsMapEntry s_SimPartsMap[eBodyPartS2C::kCount];  // 80485698
  static const char* const s_SimPartsMapNames[eSimPartsMapLocation::kCount];  // "ul", "ur", "ll", "lr"

  unsigned int m_partFlags[eBodyPartS2C::kCount] = {};     // +0x00
  EIStaticModel* m_parts[eBodyPartS2C::kCount] = {};       // +0x68
  unsigned int m_pendingIds[eBodyPartS2C::kCount] = {};    // +0xD0 SetModelPartASync
  CasSimPartsS2C* m_simParts = nullptr;                    // +0x138
  CasSimDescriptionS2C* m_desc = nullptr;                  // +0x13C
  ERTexture* m_skin[4] = {};                               // +0x140 composited skin, per map quadrant
  int m_highRes = 1;                                       // +0x150 CAS (high resolution) models
  int m_ownsDesc = 0;                                      // +0x154
  std::string m_skinNames[4];                              // +0x158 EString
};

struct SimRenderer {
  static void Render(ERC* rc, EIStaticModel** parts, EShader* shader, EMat4* matrices, unsigned int count,
                     bool morphHead);  // 80074B30
  RET_UNKNOWN Render(ERC *, EIStaticModel **, EAnimController *, EShader *, EMat4 &, bool);  // 80074BF4
  static void MorphParts(EIStaticModel** parts, bool morphHead);  // 80074CA8
  static void RenderGhosted(ERC* rc, EIStaticModel** parts, EMat4* matrices, unsigned int count, bool morphHead);  // 80074D48

  static const int s_bodyPartRenderOrder[eBodyPartS2C::kCount];  // 803D5730
};

// The body shape and face sliders on a sim's parts (game/morphutilities.cpp): a slider value picks
// one or two lattice volumes (or morph targets) and their weights. Body parts have lattice 0 for the
// body (fat/thin), 1 for the arms (buff) and both for the legs (ApplyMorph's parts); the head parts
// have 7 head-shape volumes (ApplyMorphHead, value 0..7, 3 = none).
struct MorphUtilities {
  MorphUtilities() = default;
  explicit MorphUtilities(EIStaticModel** parts);  // 80057C18
  ~MorphUtilities();  // 80057C20
  void ApplyAllMorphs(CasSimDescriptionS2C& desc);  // 80057C50
  // `what` (CasEventMorphSimS2C::MorphPartS2C): 0 body, 1 arms, 3 legs.
  void ApplyMorph(int what, float value);  // 80057CB4
  void ApplyMorphHead(float value);  // 80057EF4
  void MorphLatticeOnHead(unsigned int part, float value, unsigned int lattice);  // 80058024
  int MorphLatticeOnBodyPart(unsigned int part, float value, unsigned int lattice);  // 80058224
  void GetFacialMorphTargetIndexAndValue(float value, float& w1, int& i1, float& w2, int& i2);  // 800583AC
  void GetBodyLatticeIndexAndValue(float value, float& w1, int& i1, float& w2, int& i2);  // 80058474
  void GetHeadLatticeIndexAndValue(float value, float& w1, int& i1, float& w2, int& i2);  // 800584F8

  EIStaticModel** m_parts = nullptr;  // +0x00 the sim's 26 parts (null entries are skipped)
};
