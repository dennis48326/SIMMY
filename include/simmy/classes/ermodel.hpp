// @class ESMSStrip after EResource ERShader EDL EMat4 EVec3 EVec2
// @class ESMBuildDisplayListData
// @class ESubModelShader
// @class ESubModel
// @class ERModel
// Models (engine/e_rmodel.cpp, e_submodel.cpp, e_submodelshader.cpp). An ERModel is a list of
// sub-models, each a list of ESubModelShaders: one shader (ERShader) and the triangle strips drawn
// with it. At load time each ESubModelShader records its strips into a deferred display list (EDL);
// drawing selects the shader and calls that list.
//
// The MODL data is a byte stream the original read field by field with memcpy (there are no pointer
// fixups), so on PC the loader reads the same stream with big-endian readers. The vertex arrays and
// the GX display lists of indexed strips are the exception: they are kept exactly as stored, big-
// endian, because the renderer hands them to GX (aurora reads arrays declared big-endian and display
// lists as the GameCube's big-endian command stream). Anything the CPU reads from them goes through
// simmy::be16/be32.
#pragma once

#include <vector>

// One triangle strip (0x34 bytes on the GameCube). Vertex arrays: positions are s16 x, y, z, pad
// (fixed point, ESubModelShader flag 0x10) or f32 x, y, z, w; uvs s16 or f32 pairs (two sets when flag
// 0x40); normals s8 x, y, z, pad; colours RGBA8; weights four u8 per vertex.
struct ESMSStrip {
  void* m_positions = nullptr;          // +0x00
  void* m_uvs = nullptr;                // +0x04
  signed char* m_normals = nullptr;     // +0x08
  unsigned char* m_colors = nullptr;    // +0x0C
  unsigned char* m_weights = nullptr;   // +0x10
  unsigned char* m_indices = nullptr;   // +0x14 the first vertex's indices inside m_gxDL
  int m_vertCount = 0;                  // +0x18 entries in each array
  int m_gxDLSize = 0;                   // +0x1C
  unsigned char* m_gxDL = nullptr;      // +0x20 indexed strips (flag 0x20): a GX display list
  unsigned char m_matIndex[4] = {};     // +0x24 model matrix of each skinning slot
  unsigned short m_stripVerts = 0;      // +0x28 vertices the strip draws
  unsigned char m_skinned = 0;          // +0x2A
  unsigned char m_numMatIndices = 0;    // +0x2B
  int m_hasMatIndices = 0;              // +0x2C
};

// What ERModel::FinishLoad passes to every ESubModelShader::BuildDisplayList.
struct ESMBuildDisplayListData {
  const char* m_name = nullptr;  // +0x00 display-list name (debug)
  ERModel* m_model = nullptr;    // +0x04 GC: the model's flags block at +0x14
  float m_minX = 1e8f;           // +0x08 wall models: the x range the cut-away weights span
  float m_maxX = -1e8f;          // +0x0C
  int m_hasBlend = 0;            // +0x10 set when a shader blends with the frame buffer
};

// The vertices of one shader that one lattice deforms (ESubModelShader::CreateLatticeList; 0xC bytes on
// the GC): the lattice, and per vertex (0x14 bytes) its spline parameters, strip << 16 | vertex, and
// the lattice cell (BSplineVolume::ParameterizeVert).
struct ELatticeVertParam {
  EVec3 m_params;             // +0x00
  unsigned int m_vertex = 0;  // +0x0C
  unsigned int m_cell = 0;    // +0x10
};
struct ELatticeVerts {
  BSplineVolume* m_volume = nullptr;     // +0x00
  int m_count = 0;                       // +0x04
  std::vector<ELatticeVertParam> m_params;  // +0x08
};

struct ESubModelShader {
  // Vertex-format flags (m_flags).
  enum : unsigned int {
    kSingleStrip = 1, kUVs = 2, kColors = 4, kNormals = 8, kFixedPoint = 0x10, kIndexed = 0x20, kTwoUVs = 0x40
  };

  void* VertexDataAllocation(int size, const char* file, int line, const char* what);  // 802FBA2C
  static void VertexDataDeallocation(void* data, bool free);  // 802FBA64
  void MorphDataAllocation(int, int);  // 802FBAA4
  void MorphDataDeallocation(void *);  // 802FBACC
  ESubModelShader();  // 802FBAF4
  ~ESubModelShader();  // 802FBB70
  ESubModelShader(const ESubModelShader&) = delete;
  ESubModelShader& operator=(const ESubModelShader&) = delete;
  void DelRefSubResources();  // 802FBBC8
  void AddRefSubResources();  // 802FBC08
  int TryIncrementSubResources();  // 802FBC54
  void DeallocateStripData(ESMSStrip* strip);  // 802FBCA0
  void Deallocate();  // 802FBD70
  void FreeStripResource();  // 802FBE3C
  void FreeMorphStripResource();  // 802FBEAC
  void FreeMorphResource();  // 802FBF48
  int CreateRCPrimitive(unsigned int count, ERC* rc, ESMSStrip* strip, bool, bool weights, bool);  // 802FBFEC
  int ReadPositions(const unsigned char* src, ESMSStrip* strip, bool weighted);  // 802FC110
  int ReadTexcoords(const unsigned char* src, ESMSStrip* strip);  // 802FC24C
  int ReadColors(const unsigned char* src, ESMSStrip* strip);  // 802FC2F8
  int ReadNormalsOld(const unsigned char* src, ESMSStrip* strip);  // 802FC3B8
  int ReadNormals(const unsigned char* src, ESMSStrip* strip);  // 802FC4B0
  int ReadWeights(const unsigned char* src, ESMSStrip* strip, bool weighted);  // 802FC564
  static RET_UNKNOWN OptimizeStripsForMemory(ESubModelShader *, ESMSStrip *, int, bool);  // 802FC708
  unsigned int ReadIndices(const unsigned char* src, ESMSStrip* strip, unsigned int& bytes);  // 802FCDA4
  int Read(const unsigned char* src, int version);  // 802FCED4
  void DrawNormals(ERC* rc);  // 802FD24C
  void DrawWireFrame(ERC* rc);  // 802FD250
  void MorphBreakUpStrip(TArray<ESMSStrip, TArrayERModelAllocator> &, TArray<ESMSStrip, TArrayERModelAllocator> &);  // 802FD254
  void AllocateMorph();  // 802FD258
  int RegisterMorphTarget(ESubModelShader* target, int slot);  // 802FD330
  void UnRegisterMorphTarget(int slot);  // 802FD3E8
  void ResetMorph();  // 802FD3FC
  int ApplyMorph();  // 802FD484
  int GetMorphedVertex(EVec3& out, short strip, short vertex);  // 802FD960
  void GenerateMorphTargetDeltas(ESubModelShader* base);  // 802FDAAC
  int Morph(float* weights);  // 802FDDC4
  int CreateLatticeList(std::vector<BSplineVolume*>* lattices);  // 802FE1A4
  void AllocateParameterizeVerts();  // 802FE400
  void ParameterizeVerts(std::vector<BSplineVolume*>* lattices);  // 802FE5CC
  void LatticeDeform();  // 802FE910
  void BuildDisplayList(ESMBuildDisplayListData* data);  // 802FED28
  void BuildDisplayList(bool, char* name, bool weights, float minX, float maxX);  // 802FF260
  void GetMinMaxX(float* minX, float* maxX);  // 802FF6F8
  unsigned int CanColorBeModified();  // 802FF7D0
  unsigned int GetModifiableColor(unsigned int index);  // 802FF810
  void TransformModelUV(EVec2& scale, EVec2& offset, unsigned int shaderId, EVec2& scale2, EVec2& offset2,
                        bool weldable);  // 802FF844

  // Weights the wall cut-away fades with: 0..255 across [minX, maxX] (BuildDisplayList).
  void MakeWallWeights(ESMSStrip& strip, float minX, float maxX);
  EShader* GetEShader() const { return m_shader ? m_shader->GetShader() : nullptr; }

  EDL* m_dl = nullptr;                  // +0x00 the strips, recorded by BuildDisplayList
  int m_morph = 0;                      // +0x04 set for sub-models of morphing models
  ERShader* m_shader = nullptr;         // +0x08
  unsigned int m_shaderId = 0;          // +0x0C
  unsigned int m_flags = 0;             // +0x10 vertex format (k* above)
  std::vector<ESMSStrip> m_strips;      // +0x14 TArray<ESMSStrip>
  ESubModel* m_owner = nullptr;         // +0x20
  float m_mipMap = 1.0f;                // +0x24 SetMipMap argument
  int m_deltasMade = 0;                 // +0x28 a morph target's positions are its offsets (once)
  // Morphing: the strips again with their own positions (the other arrays are the base strips'),
  // holding the offsets the morphs add up, then (ApplyMorph) base + offsets, which m_dl then draws.
  std::vector<ESMSStrip> m_morphStrips; // +0x2C
  ESubModelShader* m_targets[12] = {};  // +0x38 the registered morph targets' shaders
  std::vector<ELatticeVerts> m_latticeList;  // +0x68 (GC: pointer +0x68, count +0x6C)

  static int m_vertsLoaded;             // statistics
};

struct ESubModel {
  ESubModel();  // 802FAC50
  ~ESubModel();  // 802FAC88
  ESubModel(const ESubModel&) = delete;
  ESubModel& operator=(const ESubModel&) = delete;
  void DelRefSubResources();  // 802FACD4
  void AddRefSubResources();  // 802FAD30
  int TryIncrementSubResources();  // 802FAD8C
  static unsigned int GetCompositeShaderFlags();  // 802FAE20
  int Read(const unsigned char* src, int version);  // 802FAE2C
  void Draw(ERC* rc);  // 802FAF20
  void DrawGeometry(ERC* rc);  // 802FB004
  void DrawAsShadow(ERC* rc);  // 802FB09C
  void DrawNormals(ERC* rc);  // 802FB17C
  void DrawWireFrame(ERC* rc);  // 802FB1E0
  void BuildDisplayList(bool, char* name, bool weights, float minX, float maxX);  // 802FB244
  void BuildDisplayList(ESMBuildDisplayListData* data);  // 802FB2D8
  void GetMinMaxX(float* minX, float* maxX);  // 802FB33C
  int ApplyMorph();  // 802FB3A8
  void ResetMorph();  // 802FB408
  int Morph(float* weights);  // 802FB464
  void LatticeDeform();  // 802FB4CC
  void RegisterMorphTarget(ESubModel* target, int slot);  // 802FB528
  void UnRegisterMorphTarget(int slot);  // 802FB59C
  void GenerateMorphTargetDeltas(ESubModel* base);  // 802FB600
  void ParameterizeVerts(std::vector<BSplineVolume*>* lattices);  // 802FB678
  void FreeMorphResource();  // 802FB6DC
  void FreeStripResource();  // 802FB738
  void FreeMorphStripResource();  // 802FB794
  int HasModifiableColor();  // 802FB7F0
  unsigned int GetModifiableColor(unsigned int index);  // 802FB860
  void TransformModelUV(EVec2& scale, EVec2& offset, unsigned int shaderId, EVec2& scale2, EVec2& offset2,
                        bool weldable);  // 802FB8E0

  // TArray<ESubModelShader> (+0x00). Elements are not copyable (they own GPU data), so they are held
  // by pointer.
  std::vector<ESubModelShader*> m_shaders;
  int m_field0C = 0;          // +0x0C read from the file (-1 in the models looked at)
  ERModel* m_model = nullptr; // +0x14
};

// A model resource (0x180 bytes on the GameCube).
struct ERModel : public EResource {
  ERModel();  // 80317DEC
  SIMMY_STORABLE_DECL(ERModel)
  ~ERModel() override;  // 80318014
  void Refresh(EFile* file) override;  // 803180F4
  using EResource::Refresh;
  void DelRefSubResources() override;  // 8031811C
  void AddRefSubResources() override;  // 80318180
  int TryIncrementSubResources() override;  // 803181E4
  int ReadAttachmentVert(const unsigned char* src);  // 80318280
  void LoadModel(EFile* file);  // 80318324
  void FinishLoad(char flags);  // 80318664
  int GetShaderCount();  // 80318778
  EMat4* GetScaleMatrix();  // 803187AC
  void DeallocateScaleMatrix();  // 8031882C
  void CalcOrientedBoundSphere(EMat4 &, EBoundSphere &);  // 80318874
  void DrawGeometry(ERC* rc);  // 80318950
  void DrawAsShadow(ERC* rc);  // 803189BC
  void DrawNormals(ERC* rc);  // 80318A28
  void DrawWireFrame(ERC* rc);  // 80318A94
  int GetLatticeCount();  // 80318B00
  int GetDummyCount();  // 80318B08
  int GetCameraCount();  // 80318B10
  void Draw(ERC* rc);  // 80318B18
  void DrawShadow(ERC *, float);  // 80318B84
  void DrawShadowWithStencilInverted(ERC *);  // 80318DA4
  void DrawHierarchical(ERC *, EMat4 *, EACNodeState *, int);  // 80318FB0
  void GetWeldPos(EWeldVert *);  // 80319214
  void SetWeldPos(EWeldVert *, EVec3 &);  // 803192C0
  void WeldSharedVerts(ERModel *, float);  // 80319350
  static void RebuildAfterWelding();  // 80319E84
  void GenerateMorphTargetDeltas(ERModel *);  // 80319E88
  int RegisterMorphTarget(ERModel* target, int slot);  // 80319F08
  ERModel* RegisterMorphTarget(unsigned int id, int slot);  // 80319FD0
  void UnRegisterMorphTarget(int slot);  // 8031A040
  void UnRegisterMorphTarget(ERModel* target);  // 8031A108
  int SetMorphTargetWeight(unsigned int slot, float weight);  // 8031A174
  void ResetMorph();  // 8031A1AC
  int MorphTargets();  // 8031A21C
  void MorphLattices();  // 8031A2A4
  void ApplyMorph();  // 8031A34C
  void CleanMorphTargets(bool);  // 8031A3C4
  void FreeAllMorphAndLatticeResource(bool keepDirty);  // 8031A41C
  void FreeStripResource();  // 8031A458
  void FreeMorphStripResource();  // 8031A4BC
  void SetLatticeWeight(int lattice, int volume, float weight);  // 8031A520
  void ResetLatticeWeights();  // 8031A56C
  int GetNumMorphLattice(int lattice);  // 8031A5FC
  int GetNumMorphBases();  // 8031A628
  void ParameterizeVerts();  // 8031A630
  void FreeMorphResource();  // 8031A698
  int HasModifiableColor();  // 8031A6FC
  unsigned int GetModifiableColor(unsigned int index);  // 8031A774
  RET_UNKNOWN GetAttachmentID(unsigned char, signed char &);  // 8031A7FC
  RET_UNKNOWN DrawAttachment(ERC *, ERModel *, EMat4 *, int, float);  // 8031A84C
  void GetAttachmentMatrix(EMat4 &, ERModel *, EMat4 *, int, float);  // 8031A8C0
  RET_UNKNOWN GetAttachmentMatrix(EMat4 &, unsigned char, EMat4 *, int, float, float);  // 8031A964
  void TransformModelUV(EVec2& scale, EVec2& offset, unsigned int shaderId, EVec2& scale2, EVec2& offset2);  // 8031B0C0
  static void RegisterType(unsigned short);  // 8031B2EC
  void CreateCopy() const;  // 8031B340
  void CalcOrientedBoundBox(EMat4 &, EBound3 &);  // 8031B3C4
  RET_UNKNOWN GetLattice(int);  // 8031B3F8
  RET_UNKNOWN GetDummy(int);  // 8031B408
  RET_UNKNOWN GetCamera(int);  // 8031B418
  int IsHierarchical();  // 8031B428
  int IsWeldable();  // 8031B430
  int IsMorphTarget();  // 8031B438
  int IsLattice();  // 8031B440
  int AreMatsInDLs();  // 8031B448
  int DontFrontPlaneFade();  // 8031B450
  int DontInterestFade();  // 8031B458
  int DontCull();  // 8031B460
  int IsWall();  // 8031B468
  unsigned char GetAttachmentColor();  // 8031B470
  void ClearAttachmentID();  // 8031B478
  int GetNumAttachmentVerts();  // 8031B484
  int GetNumLightPos();  // 8031B48C
  float* GetLightPos();  // 8031B494

  // MdfData (+0x14): flags from the load-flags byte (FinishLoad) and the file.
  int m_hierarchical = 0;        // +0x14 load flag 0x01
  int m_field18 = 0;             // +0x18
  int m_field1C = 0;             // +0x1C
  int m_weldable = 0;            // +0x20 load flag 0x02
  int m_dontFrontPlaneFade = 0;  // +0x24 load flag 0x04
  int m_dontInterestFade = 0;    // +0x28 load flag 0x40
  int m_dontCull = 0;            // +0x2C load flag 0x80
  int m_isWall = 0;              // +0x30
  int m_isLattice = 0;           // +0x34 load flag 0x20
  int m_isMorphTarget = 0;       // +0x38 load flag 0x10 (no display lists are built)
  std::vector<ESubModel*> m_subModels;  // +0x3C TArray<ESubModel> (held by pointer, see ESubModel)
  float m_boundSphere[4] = {};   // +0x48 centre, radius (model units)
  float m_bounds[6] = {};        // +0x58 EBound3: min, max
  float m_bounds2[6] = {};       // +0x70 EBound3
  float m_scale = 0;             // +0x88 model units -> world (fixed-point models: 1/4096)
  int m_matsInDLs = 0;           // +0x8C
  // The lights, dummies and cameras of the model. Their classes are not ported, so each element is
  // kept as its stored big-endian record.
  std::vector<std::vector<unsigned char>> m_lightInfo;  // +0x90 TArray<SimsLightInfo> (0x1C bytes each)
  ERModel* m_morphTargets[12] = {};  // +0x9C registered morph targets (models of the same shape)
  float m_morphWeights[12] = {};     // +0xCC
  int m_morphDirty = 1;          // +0xFC a weight changed: the morphs are to be redone
  int m_hasBlend = 0;            // +0x100 a shader blends with the frame buffer
  std::vector<BSplineVolume*> m_lattices;               // +0x104 TArray<BSplineVolume> (owned)
  std::vector<std::vector<unsigned char>> m_dummies;    // +0x11C TArray<ENDummy>
  std::vector<std::vector<unsigned char>> m_cameras;    // +0x128 TArray<ENCamera>
  unsigned char m_attachmentColor = 0;    // +0x134
  unsigned char m_attachmentId = 0xff;    // +0x135
  int m_numAttachmentVerts = 0;           // +0x138
  EMat4* m_attachmentVerts = nullptr;     // +0x13C
  int m_numLightPos = 0;                  // +0x140
  float m_lightPos[12] = {};              // +0x144
  EMat4* m_scaleMatrix = nullptr;         // +0x174
  unsigned char m_loadFlags = 0;          // +0x178
  int m_uvTransformed = 0;                // +0x17C TransformModelUV is done (once per model)
};
