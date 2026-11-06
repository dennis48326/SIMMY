// @class AptFileImage after AptMaskRenderOperation AptNativeHash AptValue
// @class AptRect after AptMaskRenderOperation AptNativeHash AptValue
// @class AptMatrix after AptMaskRenderOperation AptNativeHash AptValue
// @class AptCXForm after AptMaskRenderOperation AptNativeHash AptValue
// @class AptControl after AptMaskRenderOperation AptNativeHash AptValue
// @class AptControlPlaceObject2 after AptMaskRenderOperation AptNativeHash AptValue
// @class AptControlRemoveObject2 after AptMaskRenderOperation AptNativeHash AptValue
// @class AptMovie after AptMaskRenderOperation AptNativeHash AptValue
// @class AptCharacter after AptMaskRenderOperation AptNativeHash AptValue
// @class AptCharacterButton after AptMaskRenderOperation AptNativeHash AptValue
// @class AptCharacterAnimation after AptMaskRenderOperation AptNativeHash AptValue
// @class AptConstFile after AptMaskRenderOperation AptNativeHash AptValue
// Apt movie data as the player reads it (libaptz: AptCharacter, AptMovie, AptControl*, AptConstFile).
// On GC the .apt/.const files were used in place: AptCharacterAnimation::Resolve/Fixup,
// AptMovie::resolve and AptActionInterpreter::resolveStream added the file base to every 32-bit
// offset. On PC (see PORTING.md, "Apt movies on PC") the loader reads the big-endian files into these
// native structs, which keep the GC field order (GC offsets in comments; pointers are 8 bytes here).
// Strings and ActionScript bytecode stay in the retained .apt/.const images and are pointed into;
// bytecode is never patched: the interpreter decodes its big-endian arguments and resolves its
// offsets against AptFileImage on access.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

struct AptRect {
  float left = 0, top = 0, right = 0, bottom = 0;
};

// 2x2 rotation/scale then translation, as stored and as setVertexMatrix receives it.
struct AptMatrix {
  float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
};

// Colour transform as AptRenderingContext and setColourTransform use it: scale then translate for
// alpha, red, green, blue (alpha first; vertex colours are ARGB).
struct AptCXForm {
  float mulA = 1, mulR = 1, mulG = 1, mulB = 1;
  float addA = 0, addR = 0, addG = 0, addB = 0;
};

struct AptCharacter;
struct AptCharacterAnimation;
struct AptFileImage;

// ---- Frame controls (AptMovie frames) ------------------------------------------------------------

struct AptControl {
  enum : int {
    kAction = 1, kFrameLabel = 2, kPlaceObject = 3, kRemoveObject = 4, kBackgroundColour = 5,
    kStartSound = 6, kStartSoundStream = 7, kInitAction = 8,
  };
  int type = 0;  // +0x00
};
struct AptControlAction : AptControl {
  const unsigned char* stream = nullptr;  // +0x04 bytecode in the .apt image
};
struct AptControlFrameLabel : AptControl {
  const char* label = nullptr;  // +0x04
};
// One clip event handler of a placed sprite (12 bytes on GC).
struct AptClipAction {
  unsigned int events = 0;                // +0x00 event mask
  unsigned int keyCode = 0;               // +0x04
  const unsigned char* stream = nullptr;  // +0x08
};
struct AptClipActions {
  int count = 0;                       // +0x00
  AptClipAction* actions = nullptr;    // +0x04
};
// PlaceObject2 (0x40 bytes on GC). Flags: 1 move, 2 has character, 4 matrix, 8 colour transform,
// 0x10 ratio, 0x20 name, 0x40 clip depth, 0x80 clip actions.
struct AptControlPlaceObject2 : AptControl {
  unsigned int flags = 0;                // +0x04
  int depth = 0;                         // +0x08
  int character = -1;                    // +0x0C character index, -1 = none
  AptMatrix matrix;                      // +0x10
  unsigned int colourMul = 0xffffffff;   // +0x28 ARGB multiplier
  unsigned int colourAdd = 0;            // +0x2C ARGB offset
  float ratio = 0;                       // +0x30
  const char* name = nullptr;            // +0x34
  int clipDepth = -1;                    // +0x38
  AptClipActions* clipActions = nullptr; // +0x3C
};
struct AptControlRemoveObject2 : AptControl {
  int depth = 0;  // +0x04
};
struct AptControlBackgroundColour : AptControl {
  unsigned int colour = 0;  // +0x04 ARGB
};
struct AptControlStartSound : AptControl {
  int character = 0;  // +0x04 sound character index
};
// Runs once per file; the runtime negates `sprite` after running it (AptMovie::doFrameControls).
struct AptControlInitAction : AptControl {
  int sprite = 0;                         // +0x04
  const unsigned char* stream = nullptr;  // +0x08
};

struct AptFrame {
  int controlCount = 0;              // +0x00
  AptControl** controls = nullptr;   // +0x04
};

// The timeline of a sprite or movie (AptMovie, at +8 of the character on GC).
struct AptMovie {
  int frameCount = 0;              // +0x00
  AptFrame* frames = nullptr;      // +0x04
  AptNativeHash* labels = nullptr; // +0x08 label -> frame, built by AptMovie::resolve (runtime)

  void resolve(unsigned char*, AptConstFile*, int*);  // 802A8C44
  void unresolve(unsigned char*, int*);  // 802A8FC8
  void DoTemporaryFrameControls(AptPseudoDisplayList*, int);  // 802A9284
  void doFrameControls(AptDisplayList*, AptCIH*, int);  // 802A94AC
  void runFrameActions(AptCIH*, int);  // 802A96C4
  void queueFrameActions(AptCIH*, int);  // 802A97AC
  static RET_UNKNOWN labelToFrame(EAStringC*);  // 802A9848
};

// ---- Characters ----------------------------------------------------------------------------------

struct AptCharacter {
  enum : int {
    kShape = 1, kEditText = 2, kFont = 3, kButton = 4, kSprite = 5, kSound = 6, kImage = 7, kMorph = 8,
    kMovie = 9, kStaticText = 10,
  };
  int type = 0;                     // +0x00
  AptCharacter* movie = nullptr;    // +0x04 the file's movie (characters[0]); 0x09876543 on disk

  void render(AptRenderingContext*, AptMaskRenderOperation, AptMatrix*);  // 80288090
  void _getBoundingRect(AptRenderingContext*, AptRect*, AptMatrix*);  // 80288120
};
struct AptCharacterShape : AptCharacter {
  AptRect bounds;                  // +0x08
  int geometryId = 0;              // +0x18 on disk the character index;
  void* renderingUnit = nullptr;   //       after Fixup loadRenderingUnit's result
};
struct AptCharacterEditText : AptCharacter {
  AptRect bounds;                  // +0x08
  int font = 0;                    // +0x18 font character index
  int alignment = 0;               // +0x1C
  unsigned int colour = 0;         // +0x20 RGBA
  float fontHeight = 0;            // +0x24
  int readOnly = 0;                // +0x28
  int multiline = 0;               // +0x2C
  int wordWrap = 0;                // +0x30
  const char* text = nullptr;      // +0x34 initial text
  const char* variable = nullptr;  // +0x38 bound variable name
};
struct AptCharacterFont : AptCharacter {
  const char* name = nullptr;          // +0x08
  int glyphCount = 0;                  // +0x0C
  int* glyphIds = nullptr;             // +0x10 shape character indices
  AptCharacter** glyphs = nullptr;     //       PC: the shapes, set by AptCharacterAnimation::Link
};
// One state record of a button (0x44 bytes on GC).
struct AptButtonRecord {
  unsigned int flags = 0;               // +0x00 states (up/over/down/hit)
  int characterId = 0;                  // +0x04 index on disk, pointer after Link
  AptCharacter* character = nullptr;
  int depth = 0;                        // +0x08
  AptMatrix matrix;                     // +0x0C
  float colour[4] = {};                 // +0x24
  float unknown[4] = {};                // +0x34
};
struct AptButtonAction {
  unsigned int flags = 0;                 // +0x00 conditions
  const unsigned char* stream = nullptr;  // +0x04
};
struct AptCharacterButton : AptCharacter {
  int isMenu = 0;                          // +0x08
  AptRect bounds;                          // +0x0C
  int triangleCount = 0;                   // +0x1C hit area
  int vertexCount = 0;                     // +0x20
  float* vertices = nullptr;               // +0x24 x,y pairs
  unsigned int* triangles = nullptr;       // +0x28 raw (format not checked yet)
  int recordCount = 0;                     // +0x2C
  AptButtonRecord* records = nullptr;      // +0x30
  int actionCount = 0;                     // +0x34
  AptButtonAction* actions = nullptr;      // +0x38
  int soundIds[4] = {};                    // +0x3C sound characters per state (pointer on disk)
  bool hasSounds = false;
};
struct AptCharacterSprite : AptCharacter {
  AptMovie timeline;  // +0x08
};
struct AptCharacterSound : AptCharacter {
  void* sound = nullptr;  // +0x08 loadSound's handle
};
struct AptCharacterImage : AptCharacter {
  intptr_t texture = 0;  // +0x08 loadTexture's slot (on disk the character index)
};
struct AptCharacterMorph : AptCharacter {
  int startId = 0, endId = 0;            // +0x08, +0x0C shape character indices
  AptCharacter* start = nullptr;         //   after Link
  AptCharacter* end = nullptr;
};
// One run of glyphs in static text (0x38 bytes on GC).
struct AptStaticTextRecord {
  int font = 0;                       // +0x00 font character index
  float colour[8] = {};               // +0x04 colour transform
  float offsetX = 0, offsetY = 0;     // +0x24, +0x28
  float height = 0;                   // +0x2C
  int glyphCount = 0;                 // +0x30
  short* glyphs = nullptr;            // +0x34 pairs of (glyph index, advance in twips)
};
struct AptCharacterStaticText : AptCharacter {
  AptRect bounds;                        // +0x08
  AptMatrix matrix;                      // +0x18
  int recordCount = 0;                   // +0x30
  AptStaticTextRecord* records = nullptr;// +0x34
};

struct AptImport {
  const char* movie = nullptr;  // +0x00 file name
  const char* name = nullptr;   // +0x04 exported name
  int character = 0;            // +0x08 index it fills in this file
  AptFile* file = nullptr;      // +0x0C AptSharedPtr<AptFile>, set when loaded
};
struct AptExport {
  const char* name = nullptr;  // +0x00
  int character = 0;           // +0x04
};

// The data after the movie character's header (Ghidra's `this` for AptCharacterAnimation is the
// character + 8, so its offsets are 8 less than the character's).
struct AptCharacterAnimation {
  AptMovie timeline;                    // +0x00 (character +0x08)
  int characterCount = 0;               // +0x0C
  AptCharacter** characters = nullptr;  // +0x10
  int width = 0;                        // +0x14 stage size (640 x 448)
  int height = 0;                       // +0x18
  int msPerFrame = 0;                   // +0x1C player tick of _level0 in ms (16 in s2c_root)
  int importCount = 0;                  // +0x20
  AptImport* imports = nullptr;         // +0x24
  int exportCount = 0;                  // +0x28
  AptExport* exports = nullptr;         // +0x2C
  int valueCount = 0;                   // +0x30 constants resolved for this file (resolveStream)

  void Fixup(void*, AptConstFile*, void*);  // 802821A8
  static void Resolve(void*, AptConstFile*, void*);  // 802826C8
  RET_UNKNOWN UnmapCharacter(AptCharacter*);  // 80282730
  RET_UNKNOWN IsImport(int);  // 80282774
  void Unresolve(void*);  // 802827BC
  void Link(AptCharacter*, void*);  // 80283024
  RET_UNKNOWN GetIDFromImportFile(int);  // 802847E4
  void ExecuteInitActions(AptCIH*, int);  // 80284880
  void ExecuteInitAction(AptCIH*, int);  // 802849A0
  void ExportClassDefinitionAssets(AptCIH*);  // 80284AB4
};
struct AptCharacterMovie : AptCharacter {
  AptCharacterAnimation animation;  // +0x08
};

// ---- Constant file -------------------------------------------------------------------------------

// An entry of <name>.const: type 1 = string (offset into the .const image), 4..8 = numbers and
// registers as the interpreter's constant pool reads them.
struct AptConstItem {
  int type = 0;
  unsigned int value = 0;          // raw (big-endian word converted)
  const char* string = nullptr;    // type 1
  AptValue* resolved = nullptr;    // PC: the AptValue resolveStream would have stored in the stream
};

struct AptConstFile {
  unsigned int aptDataOffset = 0;  // +0x14 where the movie character sits in the .apt image
  int itemCount = 0;               // +0x18
  AptConstItem* items = nullptr;   // +0x1C
};

// ---- PC: a loaded movie --------------------------------------------------------------------------

// Owns the native structures built from one .apt/.const pair and keeps the images (strings and
// bytecode point into them). Built by AptFileImage::Build in place of the GC in-place fixups.
struct AptFileImage {
  static AptFileImage* Build(const unsigned char* apt, unsigned int aptSize, const unsigned char* constData,
                             unsigned int constSize);
  ~AptFileImage();

  AptCharacterMovie* Movie() const { return m_movie; }
  AptCharacterAnimation* Animation() const { return m_movie ? &m_movie->animation : nullptr; }
  // Offsets inside bytecode resolve against the .apt image (strings, arrays, function bodies).
  const unsigned char* AptAt(unsigned int offset) const { return offset ? m_apt + offset : nullptr; }
  unsigned int ReadU32(const unsigned char* p) const {
    return static_cast<unsigned int>(p[0]) << 24 | static_cast<unsigned int>(p[1]) << 16 |
           static_cast<unsigned int>(p[2]) << 8 | p[3];
  }

  const unsigned char* m_apt = nullptr;   // the .apt image (owned by the FlashBigFile)
  unsigned int m_aptSize = 0;
  const unsigned char* m_const = nullptr; // the .const image
  unsigned int m_constSize = 0;
  AptConstFile m_constFile;
  AptCharacterMovie* m_movie = nullptr;
  std::vector<void*> m_blocks;            // everything below, freed together
};
