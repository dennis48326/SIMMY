// @class AptRenderingContext after AptValue AptObject AptMatrix AptCXForm AptVar
// @class AptDisplayList after AptRenderingContext
// @class AptCIH after AptObject AptDisplayList AptFileImage
// @class AptActionInterpreter after AptCIH AptScriptFunctionBase AptFrameStack
// @class AptAnimationPoolData after AptActionInterpreter
// @class AptUserFunctions after AptCXForm AptMatrix
// @class AptFile after AptFileImage
// @class AptIntervalTimer after AptCIH
// @class AptAllocateStringParameters after AptRect
// The Apt player: character instances (AptCIH), display lists, timelines, rendering, the action
// interpreter and the pool of levels (libaptz: Apt.cpp, AptCIH.cpp, AptDisplayList.cpp,
// AptMovie.cpp, AptAnimation.cpp, AptActionInterpreter.cpp, AptRenderingContext.cpp).
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct AptCIH;
struct AptAnimationPoolData;

// Matrix and colour transform stacks while rendering (AptRenderingContext). The current matrix and
// colour go to the aux layer through setVertexMatrix / setColourTransform.
struct AptRenderingContext {
  void pushVertexMatrix();  // 802ABA60 (address approximate, see aptplayer.cpp)
  void appendVertexMatrix(const AptMatrix& m);
  void popVertexMatrix();
  void pushColourTransform();
  void appendColourTransform(const AptCXForm& c);
  void popColourTransform();
  const AptMatrix& getVertexMatrix() const { return m_matrix.back(); }
  const AptCXForm& getColourTransform() const { return m_colour.back(); }
  void Reset();

  std::vector<AptMatrix> m_matrix{AptMatrix{}};
  std::vector<AptCXForm> m_colour{AptCXForm{}};
};

// The children of a sprite or level, kept in depth order.
struct AptDisplayList {
  AptCIH* find(int depth) const;
  AptCIH* findByName(const std::string& name) const;
  void insert(AptCIH* inst);
  void remove(AptCIH* inst);
  void clear();
  void tick();  // 8029B7BC
  void render(AptRenderingContext* rc, int maskOp);  // 8029B4A4

  std::vector<AptCIH*> m_list;  // sorted by depth

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  RET_UNKNOWN _addToSetCaches(AptCIH *, int);
  static void placeObject(AptPseudoCIH_t *, AptCIH *);
  void deallocAssetStringRecursive();
  void _getBoundingRect(AptRenderingContext *, AptRect *);
  void PreDestroy();
  RET_UNKNOWN getState();
  void useState(AptDisplayListState *);
  static void ReplaceDisplyListItem(AptNativeHash *, AptCIH *, AptPseudoCIH_t *, AptCIH *);
  RET_UNKNOWN mergeState(AptPseudoDisplayList *, AptNativeHash *, bool);
  void validate(AptCIH *);
};

// A character instance on a display list (AptCIH + its AptCharacterInst on GC): sprites and levels
// (with a timeline and children), shapes, images, edit text, static text, morphs, buttons.
struct AptCIH : AptObject {
  AptCIH(AptCharacter* character, AptCIH* parent, AptFileImage* file);
  void RegisterReferences() const override;
  bool GetOwn(AptActionInterpreter& in, const std::string& name, AptVar& out) override;
  bool SetOwn(AptActionInterpreter& in, const std::string& name, const AptVar& value) override;
  void Enumerate(std::vector<std::string>& names) override;
  std::string DefaultString() override { return GetTargetPath(); }

  bool IsSprite() const;  // sprite, movie or level: has a timeline and children
  bool IsText() const { return m_character && m_character->type == AptCharacter::kEditText; }
  const AptMovie* Timeline() const;
  AptCharacterAnimation* Animation() const;  // the file's movie, for character ids
  AptCIH* GetRoot();                         // _root: the level this instance belongs to
  std::string GetTargetPath();               // "_level0.a.b"

  void tick();                                // 80293128
  void jumpToFrame(int frame);                // 80292994
  void doFrameControls(int frame);            // AptMovie::doFrameControls 802A94AC
  void queueFrameActions(int frame, int marker = 0);  // AptMovie::queueFrameActions 802A97AC
  void placeObject(const AptControlPlaceObject2* c);  // AptDisplayList::placeObject 8029A9E0
  void removeObject(int depth);               // AptDisplayList::removeObject 8029AEC4
  void render(AptRenderingContext* rc, int maskOp);   // 80290C60
  void queueClipEvent(unsigned int event);    // queueClipEvents 80292C94
  void runInitActions(int sprite);            // AptCharacterAnimation::ExecuteInitActions 80284880
  float GetProperty(int index);               // 80291760
  void SetProperty(int index, float value);   // 80291B30
  void Remove();                              // 8028FEC8 (onUnload, detach)
  int labelToFrame(const std::string& label) const;  // AptMovie::labelToFrame 802A9848
  void UpdateText();                          // AptCharacterTextInst::UpdateText 80288700
  void ReleaseText();

  AptCharacter* m_character;
  // PC: a character made by script for this instance (createTextField's field), shared with its
  // duplicates (duplicateMovieClip) and freed with the last of them.
  std::shared_ptr<AptCharacter> m_ownedCharacter;
  AptCIH* m_parent;
  AptFileImage* m_file;          // file of the character (bytecode, character table)
  int m_depth = 0;
  std::string m_name;
  AptMatrix m_matrix;
  AptCXForm m_cxform;
  float m_ratio = 0;
  int m_clipDepth = -1;
  bool m_visible = true;
  bool m_removed = false;
  bool m_placedByScript = false;   // attachMovie/createEmptyMovieClip/duplicate: kept by gotos
  bool m_scriptChanged = false;    // properties set by script: timeline moves leave it (CIH +0x5C 0x8000)
  bool m_loaded = false;           // onLoad queued
  int m_level = -1;                // >= 0 for _levelN
  const AptClipActions* m_clipActions = nullptr;
  // Timeline (sprites and levels)
  int m_frame = 0;                 // current frame (0-based)
  bool m_playing = true;
  AptDisplayList m_children;
  // Edit text
  std::string m_text;              // current text (UTF-8)
  std::string m_variable;          // bound variable path
  unsigned int m_textColour = 0;   // ARGB
  float m_textSize = 0;
  int m_textAlign = 0;             // 0 left, 1 right, 2 centre (AptCharacterEditText alignment)
  bool m_wordWrap = false, m_multiline = false, m_border = false, m_selectable = false;
  unsigned int m_borderColour = 0;
  std::string m_autoSize = "none";
  AptRect m_textBounds;
  void* m_textHandle = nullptr;    // allocateString's result (aux layer)
  bool m_textDirty = true;
  AptObject* m_textFormat = nullptr;  // TextFormat applied by setTextFormat
  // Instances made by script
  AptObject* m_hitArea = nullptr;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  virtual void PreDestroy();
  virtual void DestroyGCPointers();
  RET_UNKNOWN getNativeHash() const;
  virtual RET_UNKNOWN GetNativeHashVirtual();
  virtual RET_UNKNOWN ContainsNativeHashVirtual() const;
  void deallocAssetStringRecursive();
  RET_UNKNOWN FindAndSetEvents();
  void gotoState(AptCharacterButtonRecordState);
  void SetEventHandler(int);
  void RemoveEventHandler(int);
  RET_UNKNOWN HasEvent(int);
  RET_UNKNOWN checkIfHigher(AptCIH *);
  RET_UNKNOWN getParentCount();
  RET_UNKNOWN getDepthOfParentAt(int);
  RET_UNKNOWN isVisiable();
  RET_UNKNOWN isParent(AptCIH *);
  void HasMouseEvent();
  void decZombieCount();
  RET_UNKNOWN hasRenderData();
  void GetMovieclipInfo(AptMovieclipInformation *);
  virtual void setHasClass(int);
  virtual RET_UNKNOWN getHasClass() const;
  void setASChanged(int);
  RET_UNKNOWN getASChanged() const;
  void setZombieSate(int);
  RET_UNKNOWN getZombieState() const;
  void incZombieCount();
  RET_UNKNOWN getZombieCount();
  RET_UNKNOWN getSpriteInst() const;
  RET_UNKNOWN getSpriteInstBase() const;
  RET_UNKNOWN getTextInst() const;
  RET_UNKNOWN getStaticTextInst() const;
  RET_UNKNOWN getMorphInst() const;
  RET_UNKNOWN getButtonInst() const;
  RET_UNKNOWN getAnimationInst() const;
  RET_UNKNOWN getShapeInst() const;
  RET_UNKNOWN getCharacterInst() const;
  bool isCharacterInst() const;
  RET_UNKNOWN isButtonInst(bool) const;
  bool isShapeInst() const;
  bool isStaticTextInst() const;
  bool isMorphInst() const;
  RET_UNKNOWN isAnimationInst(bool) const;
  bool isLevelInst() const;
  static double GetCosAngle(AptMatrix *);
  static double GetVectorLength(AptMatrix *);
  RET_UNKNOWN IsInCtor() const;
  void SetInCtor(unsigned int);
};

// A queued action: a block of bytecode for a clip, or a function call (AptActionQueueC).
struct AptQueuedAction {
  const unsigned char* stream = nullptr;
  AptFileImage* file = nullptr;
  AptCIH* target = nullptr;
  AptFunctionValue* function = nullptr;
  AptVar thisv;
  std::vector<AptVar> args;
  unsigned int input = 0;  // the input event a listener call answers (Key.getCode while it runs)
  // GC entry +8: the target's frame marker (CIH +0x28) when queued. A frame action queued by the
  // timeline advancing holds -frame and only runs if the clip is still on that frame.
  int frameMarker = 0;
};

// setInterval timers (AptIntervalTimer).
struct AptIntervalTimer {
  int id = 0;
  int interval = 0;       // ms
  int elapsed = 0;
  AptVar target;          // object, for setInterval(obj, "method", ms)
  AptVar function;        // function or method name
  std::vector<AptVar> args;
  bool dead = false;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  void cleanParams();
};

// The interpreter (AptActionInterpreter). runStream executes one block of bytecode in a context.
struct AptActionInterpreter {
  struct Context {
    AptFileImage* file = nullptr;
    const unsigned char* pc = nullptr;
    const unsigned char* end = nullptr;      // function bodies end here
    AptCIH* target = nullptr;                // current target (SetTarget)
    AptCIH* origTarget = nullptr;
    AptVar thisv;
    AptScriptFunctionBase* function = nullptr;
    AptFrameStack* locals = nullptr;
    std::vector<AptVar> registers;
    std::vector<AptVar> pool;                // DefineDictionary
    std::vector<AptObject*> with;
    AptObject* super = nullptr;
    bool returned = false;
    AptVar result;
  };

  // 802720D0: runs bytecode with `target` as this/the timeline.
  AptVar runStream(const unsigned char* stream, AptFileImage* file, AptCIH* target);
  // Runs a function body (AptScriptFunctionBase::Call).
  AptVar runFunction(AptScriptFunctionBase* f, const AptVar& thisv, const std::vector<AptVar>& args, AptObject* super);
  AptVar callFunction(const AptVar& fn, const AptVar& thisv, const std::vector<AptVar>& args, AptObject* super = nullptr);  // 80270070
  AptVar callMethod(const AptVar& obj, const std::string& name, const std::vector<AptVar>& args);

  AptVar getVariable(const std::string& path);           // 8026FB50
  void setVariable(const std::string& path, const AptVar& v);  // 8026DB60
  AptObject* resolvePath(const std::string& path, AptObject* base);  // getContext 8026D958
  AptVar getMember(const AptVar& obj, const std::string& name);
  void setMember(const AptVar& obj, const std::string& name, const AptVar& v);
  AptObject* toObject(const AptVar& v);  // valueToObject 8026FF94 (strings, numbers -> wrappers)
  AptObject* newObject(const AptVar& ctor, const std::vector<AptVar>& args);  // _createObject 80270930
  bool instanceOf(const AptVar& v, AptObject* ctor);  // isObjectOfType 80280E48
  void run(Context& c);

  void push(const AptVar& v) { m_stack.push_back(v); }
  AptVar pop();
  AptVar& top() { return m_stack.back(); }

  std::vector<AptVar> m_stack;
  std::vector<Context*> m_contexts;  // running contexts (GC roots)
  Context* m_ctx = nullptr;
  int m_depth = 0;
  std::vector<AptObject*> m_tempRoots;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  void initialize(AptInitParmsT &);
  RET_UNKNOWN shutdown();
  static void _parseStream(unsigned char *, unsigned char *, AptConstFile *, int *);
  static void unresolveStream(unsigned char *, unsigned char *, int *);
  void resolveStream(unsigned char *, unsigned char *, AptConstFile *, int *);
  static RET_UNKNOWN loadVariables(AptValue *, AptValue *, EAStringC *);
  static void getName2(AptCIH *, EAStringC &);
  void PrepareForExecution(char *);
  void PrepareForExecution(AptActionSetup *);
  void CleanupAfterExecution(char *, void *);
  static void CleanupAfterExecution(void *, AptActionSetup *);
  bool urlDecode(char *, EAStringC &, EAStringC &);
};

// Global player state (AptAnimationPoolData, gpPool): levels, the action queue, timers, input.
struct AptAnimationPoolData {
  void tickIntervalTimers(int ms);  // 80283AC8
  void runActions();                // 80283FA0
  void addAction(const unsigned char* stream, AptFileImage* file, AptCIH* target, int frameMarker = 0);  // AptActionQueueC::AddActionBack 80284FD8
  void addFunction(AptCIH* target, AptFunctionValue* f, const AptVar& thisv, const std::vector<AptVar>& args);  // AddFunctionBack 80285160
  void RegisterReferences();        // 80284C2C
  AptCIH* getLevel(int level) const;
  // Input (AptInput.cpp). An event is type << 17 | state << 10 | controller << 2 | 1.
  void addInput(unsigned int input);  // 80284344
  void ProcessInputs();               // 8029E1F8
  void ProcessAptInput(unsigned int input);  // 8029E150
  void ProcessInputSet(unsigned int input);  // 8029D428
  void ProcessListenerEvents(unsigned int input);  // 8029D75C

  std::vector<AptCIH*> m_levels;    // by level number
  std::vector<AptQueuedAction> m_actions;
  std::vector<AptIntervalTimer> m_timers;
  int m_nextTimerId = 1;
  int m_tickRemainder = 0;          // ms carried to the next tick (CIH +0x30 of _level0 on GC)
  unsigned int m_curTick = 0;       // gnCurTick
  AptObject* m_keyListeners = nullptr;
  std::vector<unsigned int> m_inputs;  // +0x34/+0x38 this tick's input events
  unsigned int m_curInput = 0;         // DAT_8049C19C: the event being handled (Key.getCode)

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  void PreDestroy();
  void Reset();
  void _tickNewInsts();
  void addAnalogInput(AptAnalogStickInfo);
  void clearBIL();
  void appendButtonToBIL(AptCIH *, AptMatrix *);
  void removeFromBIL(AptCIH *);
  void setInputMask(AptCIH *);
  RET_UNKNOWN isInputMasked(AptCIH *);
  RET_UNKNOWN GetActionPool();
  void _doButtonActions(AptCIH *, int);
  void setValidFocusButton();
  static void _isPointInButtonHitTestRegion(AptCharacterButton *, AptMatrix *, int, int);
  static RET_UNKNOWN _pointHits(AptCIH *, unsigned int);
  RET_UNKNOWN HandleFocusButton(AptInputType, AptInputState, AptCIH **);
  void HandleAutoNav(AptCIH *, AptInputType, AptInputState);
  static RET_UNKNOWN validateBIL();
};

// The callbacks the game supplies (gAptFuncs, AptUserFunctions; AptAuxPCEorGL_Initialize fills it).
struct AptUserFunctions {
  void* (*malloc)(unsigned int) = nullptr;                 // +0x00
  void (*free)(void*) = nullptr;                           // +0x04
  void (*assertFail)(char*, char*, unsigned int) = nullptr;  // +0x0C
  void (*setBackgroundColour)(unsigned int) = nullptr;     // +0x10
  void (*loadAnimation)(char*, AptFile*) = nullptr;        // +0x20
  void (*freeAnimation)(void*) = nullptr;                  // +0x24
  void (*freeConstantTable)(void*) = nullptr;              // +0x28
  void (*command)(char*, char*) = nullptr;                 // +0x30
  AptVar (*loadVariables)(char*) = nullptr;                // +0x34
  void (*setExternVariable)(char*, char*) = nullptr;       // +0x3C
  AptVar (*getExternVariable)(char*) = nullptr;            // +0x40
  void* (*allocateString)(struct AptAllocateStringParameters*) = nullptr;  // +0x48
  void (*deallocateString)(void*, unsigned int) = nullptr; // +0x4C
  void (*drawString)(void*, int) = nullptr;                // +0x50
  void* (*loadSound)(void*, int, char*) = nullptr;         // +0x54
  void (*freeSound)(void*) = nullptr;                      // +0x58
  void (*startSound)(void*, char*) = nullptr;              // +0x5C
  int (*loadTexture)(void*, int) = nullptr;                // +0x64
  void (*freeTexture)(void*) = nullptr;                    // +0x68
  void (*bindTexture)(void*, int, int) = nullptr;          // +0x6C
  void* (*loadRenderingUnit)(void*, int) = nullptr;        // +0x70
  void (*freeRenderingUnit)(void*) = nullptr;              // +0x74
  void (*setVertexMatrix)(const AptMatrix*) = nullptr;     // +0x78
  void (*setColourTransform)(const AptCXForm*) = nullptr;  // +0x7C
  void (*drawRenderingUnit)(void*, int) = nullptr;         // +0x80
  void (*customControlRender)(const char*, const char*, void*, const char*) = nullptr;  // +0x84
  float (*getStageHeight)() = nullptr;                     // +0xA0
  float (*getStageWidth)() = nullptr;                      // +0xA4
};

// What allocateString gets for an edit-text field (AptAllocateStringParameters, 0x58 bytes on GC).
struct AptAllocateStringParameters {
  AptRect bounds;               // +0x04
  int alignment = 0;            // +0x14
  int lineCount = 0;            // +0x1C (out)
  unsigned int colour = 0;      // +0x2C ARGB
  float sizeX = 0, sizeY = 0;   // +0x40/+0x44 (out) text size in stage units / 640, 448
  float fontHeight = 0;         // +0x48
  const char* text = nullptr;   // +0x54 UTF-8
};

// A movie file as the loader tracks it (AptFile, 0x18 bytes on GC).
struct AptFile {
  std::string name;                // +0x04
  int state = 0;                   // +0x08 1 requested, 2 loading, 3 resolved, 4 linked
  AptFileImage* image = nullptr;   // +0x0C/+0x10 the movie
  void* levelInfo = nullptr;       // +0x14 the aux layer's AptAuxLayerInfo
  std::string target;              // where the movie goes ("_level0" or a clip path)
  AptCIH* targetClip = nullptr;

  // Original methods the PC runtime has no counterpart for: declared so the generator stubs them
  // (nothing on PC calls them).
  RET_UNKNOWN GetMainCharacter() const;
};

extern AptUserFunctions gAptFuncs;
extern AptAnimationPoolData* gpPool;
extern AptActionInterpreter gAptActionInterpreter;
extern AptGlobal* gpGlobalGlobalObject;
extern AptRenderingContext* gpRenderingContext;

// Apt.cpp API
void AptInitialize(void* parms, bool);                            // 80268998
void AptShutdown(int);                                            // 80268DD8
void AptUpdate(unsigned int ms);                                  // 8026AAA8
void AptRender();                                                 // 8026AAC8
void AptLoadAnimation(const char* path, const char* target);      // 802693E8
void AptLinkerUpdate();  // AptLinker::Update: links and attaches the loaded movies
void AptCompleteAnimationAsyncLoad(AptFile* file, void* apt, unsigned int aptSize, void* constData,
                                   unsigned int constSize, void* levelInfo);  // 802690FC
void AptGetAnimationSize(int* width, int* height);                // 802692E0
void AptSetValidFocusButton();                                    // 802693BC
void AptFlushInputQueue();                                        // 8026B808
// 8026AE4C: calls `function` on `target` (a path from _level0; null = _level0) with string arguments.
AptVar AptCallFunction(const char* function, const char* unused, const char* target, int argc, const char* const* argv);
void AptSetInternalVariable(const char* name, const char* value);  // 8026ABBC
std::string AptGetInternalVariable(const char* name);              // 8026AD5C
void AptAddToInputQueue(int type, int state, int controller);      // 8026AAEC
bool AptIsFileLoaded(const char* name);
// PC (GC: AptFile reference counts, AptCharacterAnimation release -> freeAnimation): frees the movies
// that no live clip, script function or queued action uses any more, with their textures and aux
// level (AptGC::CleanUnreachable calls it after a collection).
void AptReleaseUnusedFiles();
// PC diagnostic: the loaded movies on stderr (SIMMY_APT_LEVELS, or when no aux level is free).
void AptDumpFiles();
// PC: movies loaded since the last collection (AptUpdate collects after a few, so dialogs and
// screens that come and go give their aux levels and texture slots back).
extern int gAptLoadsSinceCollect;
// PC: loaded movies' root clips removed since the last collection (AptUpdate collects after any).
extern int gAptUnloadsSinceCollect;
void AptRegisterBuiltins();
// AptDisplayList::instantiateCharacter (80299970); `setup` sets matrix and colour before the class
// constructor and the first frame run.
AptCIH* AptCreateInstance(AptCIH* parent, AptCharacter* ch, int depth, const std::string& name,
                          const std::function<void(AptCIH*)>& setup = nullptr);  // _constructBuiltInObjects 802B10A4
