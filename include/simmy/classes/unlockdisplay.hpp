// @class CUnlockDisplayObject after EMat4 EVec3 EIStaticModel EAnimController
// @class CUnlockDisplayObjectSim after CUnlockDisplayObject CasSimDescriptionS2C ESim
// @class CUnlockDisplay after E3DWindow ELights CUnlockDisplayObject ObjectDefinition
// @class ObjectDefinition
// The UI's 3D model widget (game/unlockdisplay.cpp, unlockdisplayobject.cpp, unlockdisplayobjectsim.cpp):
// an E3DWindow of its own with a camera, a projection, a box on the screen (fractions of it) and up to
// four directional lights, drawing a chain of display objects: models (CUnlockDisplayObject: a model,
// optionally animated by a character + animation) or sims (CUnlockDisplayObjectSim: an ESim made for the
// person). The catalogue's preview and the reward dialog's model use it through UI3D (ui3d.hpp); the
// promotion (SetupForPromotion) and social (SetupForSocial) set-ups are never called in this build.
// The camera orbits the objects: the first object's rotation (turned by its rotation speed each frame)
// turns the camera's offset and the lights flagged 0, so the model appears to turn in its box.
#pragma once

// What a display shows (GetObjSelector3dDisplayInfo fills it from an object's ResData: +0x20 the
// character, its first ObjAnimDef's model (+0) and animation (+8)).
struct ObjectDefinition {
  unsigned int m_character = 0;  // +0x00
  unsigned int m_model = 0;      // +0x04
  unsigned int m_anim = 0;       // +0x08
  unsigned int m_field0C = 0;    // +0x0C
};

// 0x88 bytes (GC: vtable at +0x84).
struct CUnlockDisplayObject {
  CUnlockDisplayObject();  // 80079078
  virtual ~CUnlockDisplayObject();  // 800791B0
  virtual bool ServiceDataReady();  // 800792B8
  virtual void Setup();  // 80079358
  void Update();  // 800793D8
  virtual void Draw(ERC* rc);  // 80079874
  bool IsActive();  // 800799A0
  ERModel* GetModel();  // 800799C4
  void SetMultiColorIndex(int index);  // 800799CC
  int IsSimple();  // 80079A54
  void SetModelID(unsigned int id);  // 80079A5C
  void SetCharacterID(unsigned int id);  // 80079A64
  void SetAnimationID(unsigned int id);  // 80079A8C
  void SetPos(EVec3& pos);  // 80079AB4
  EVec3 GetPos();  // 80079AD4
  void SetRot(EVec3& rot);  // 80079AF8
  EVec3 GetRot();  // 80079B18
  void SetRotAdd(EVec3& rot);  // 80079B3C
  EVec3 GetRotAdd();  // 80079B5C
  void SetAnimationLoops(unsigned int loops);  // 80079B80
  void SetPlayAndHold(bool hold);  // 80079B88
  void SetOrient(EMat4& m);  // 80079B90
  int IsReady();  // 80079C18
  void SetTrigger();  // 80079C20
  int IsDone();  // 80079C2C
  double GetRadius();  // 80079C34
  EVec3 GetCenter();  // 80079C3C
  int HasDrawn();  // 80079C60
  int GetType();  // 80079C68

  CUnlockDisplayObject* m_next = nullptr;  // +0x00 the display's chain
  int m_type = 1;                     // +0x04 1 model, 2 sim
  int m_state = 0;                    // +0x08 0 load, 1 loading, 2 ready, 3 playing, 4 done
  int m_visible = 0;                  // +0x0C drawn (from state 3)
  EAnimController* m_anim = nullptr;  // +0x10 (a sim: its ESim's)
  ERModel* m_model = nullptr;         // +0x14
  int m_colorIndex = -1;              // +0x18 a multi-colour index still to apply (-1 none)
  EIStaticModel* m_instance = nullptr;  // +0x1C
  ERAnim* m_animRes = nullptr;        // +0x20
  EMat4* m_orient = nullptr;          // +0x24 (16-byte aligned on GC)
  float m_radius = 0;                 // +0x28 half the bound box's diagonal, world units
  unsigned int m_modelId = 0;         // +0x2C
  unsigned int m_characterId = 0;     // +0x30
  unsigned int m_animId = 0;          // +0x34
  unsigned int m_loops = 1;           // +0x38 replays (0: once)
  EVec3 m_pos;                        // +0x3C
  EVec3 m_rot;                        // +0x48 radians about x, y, z
  EVec3 m_rotAdd;                     // +0x54 per 1/60 s
  EVec3 m_center;                     // +0x60 the bound box's centre, world units
  int m_playAndHold = 0;              // +0x6C stop on the last frame
  int m_trigger = 0;                  // +0x70 start playing
  int m_ready = 0;                    // +0x74
  int m_done = 0;                     // +0x78
  int m_hasDrawn = 0;                 // +0x7C
  int m_simple = 1;                   // +0x80 no character or animation: the model alone
};

// 0x1B8 bytes.
struct CUnlockDisplayObjectSim : public CUnlockDisplayObject {
  CUnlockDisplayObjectSim();  // 80079C70
  ~CUnlockDisplayObjectSim() override;  // 80079CD0
  void SetPerson(cXPerson* person);  // 80079D48
  bool ServiceDataReady() override;  // 80079E38
  void Setup() override;  // 80079EAC
  void Draw(ERC* rc) override;  // 80079EB0
  int IsMale();  // 80079F78
  static int IsAdult();  // 80079F84

  cXPerson* m_person = nullptr;           // +0x88
  ESim* m_sim = nullptr;                  // +0x8C
  CasSimDescriptionS2C m_desc;            // +0x90 a copy of the person's
  CasSimDescriptionS2C* m_descPtr = nullptr;  // +0x1B4
};

// 0x4F0 bytes (GC: EWindow/E3DWindow first, its own vtable at +0x4E8).
struct CUnlockDisplay : public E3DWindow {
  CUnlockDisplay();  // 800775AC
  ~CUnlockDisplay() override;  // 80077854
  void makeObjects();  // 800778B0
  void destroyObjects();  // 800779A0
  void SetupForObject(ObjectDefinition* def, EVec3& pos, EVec3& rot, EVec3& rotAdd);  // 80077A0C
  void SetupForPromotion(cXPerson* person, void*);  // 80077C3C
  void SetupForSocial(void*);  // 80077DD4
  void Update();  // 80077DFC
  void SetDisplayBoxLeft(float v);  // 80077FA8
  void SetDisplayBoxTop(float v);  // 80077FC0
  void SetDisplayBoxRight(float v);  // 80077FD8
  void SetDisplayBoxBottom(float v);  // 80077FF0
  void SetViewZoomFactor(float zoom);  // 80078008
  void ManualCameraPos(float x, float y, float z);  // 80078010
  void Draw(ERC* rc);  // 80078020
  void SetDirectLight(float r, float g, float b, int index);  // 80078B74
  void SetAmbientLight(float r, float g, float b);  // 80078BD0
  bool HasDrawn();  // 80078C08
  bool ObjectHasMultiColor();  // 80078C44
  void SetMultiColorNumber(int index);  // 80078CAC
  void SetDirectLightDir(EVec3 dir, bool relative, int index);  // 80078CF8
  void RemoveAllDirectLights();  // 80078DAC
  static int GetBedOtherHalfIDs(unsigned int model, unsigned int& otherModel, unsigned int& otherAnim);  // 80078DB8
  int IsDone();  // 80078FDC
  void Hide();  // 80078FE4
  void UnHide();  // 80078FF0
  int DonePreloadObject();  // 80078FFC

  ELights m_lights;                   // +0x350 (m_numDir at +0x440)
  int m_state = 0;                    // +0x448 0 idle, 1 set up, 2 loading, 3 ready, 4 shown, 5 finish
  int m_mode = 0;                     // +0x44C 1 object, 2 promotion, 3 social
  int m_preloaded = 0;                // +0x450 every object loaded and active
  int m_shown = 0;                    // +0x454
  int m_hidden = 0;                   // +0x458
  int m_done = 0;                     // +0x45C
  float m_radius = 0;                 // +0x460 of everything shown (world units)
  CUnlockDisplayObject* m_objects = nullptr;  // +0x464
  float m_boxLeft = 0, m_boxTop = 0, m_boxRight = 0, m_boxBottom = 0;  // +0x468 fractions of the screen
  float m_unused478[6] = {};          // +0x478
  int m_models = 0;                   // +0x490 promotion: models to make
  int m_sims = 0;                     // +0x494 promotion: sims to make
  float m_zoom = 1.0f;                // +0x498
  EVec3 m_cameraPos;                  // +0x49C the camera's direction from the centre (object frame)
  int m_lightRelative[4] = {};        // +0x4A8 1: the light's dir holds rotation angles about the view
  EVec3 m_lightDir[4];                // +0x4B8
};
