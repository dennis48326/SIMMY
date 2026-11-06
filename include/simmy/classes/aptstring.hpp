// @class AptFormat after EVec2 EVec3 EVec4 EMat4
// @class AptWord after AptFormat
// @class AptShaderWord after AptWord
// @class AptLine after AptWord
// @class AptParagraph after AptLine
// Edit-text layout (game/aptstring.cpp): an AptParagraph is a list of AptLines, each a list of
// AptWords placed by their centres in the text field's pixel space. Build splits the text into words
// at white space, '<' and '>' (embedded "<TAG_hex>" codes become formats of the following word and
// are removed), '|' (a hyphenation point, shown as '-'), then fills lines to the field's width.
// A word may carry two AptFormats: one applied before it is drawn (colour, size, blink, pulse, rock,
// font effects, outline, searchlight, particles) and one after it (restoring what the first changed).
// AptShaderWord is a pad-button picture ("<BUTTON_nn>"). GC vtables sat after the members.
#pragma once

struct ERC;

// One emitter of a text particle effect (GC: a 0x54-byte list node; see aptstring_particles.cpp).
struct AptTextParticle {
  AptTextParticle* m_next = nullptr;  // +0x00
  Emitter* m_emitter = nullptr;       // +0x04 an EmitterSpr3d in the text particle system
  RParticle* m_particle = nullptr;    // +0x08
  unsigned int m_id = 0;              // +0x0C the 'PART' resource
  EVec3 m_translate{0, 0, 0};         // +0x10 the emitter's place in the effect's little scene
  EVec3 m_scale{1, 1, 1};             // +0x1C
  EVec3 m_rotate{0, 0, 0};            // +0x28 radians about x, y, z
  EVec3 m_color{-1, -1, -1};          // +0x34 the colour keys' rgb (r < 0: the description's)
  float m_savedAlpha[4] = {};         // +0x40 the colour keys' alpha while scaled for a draw
  float m_alpha = 1.0f;               // +0x50
};

struct AptFormat {
  explicit AptFormat(AptWord* word);  // 8000AF88
  virtual ~AptFormat();  // 8000B1B0
  void Draw(EMat4* matrix, EVec4* scale, EVec4* translate);  // 8000B344
  void InitForParticles(int type);  // 8000BD74
  void ProcessParticles(ERC* rc, EMat4* matrix, EVec2& pos, EVec2& size, float alphaScale, float alphaAdd);  // 8000C628
  int CheckAsyncLoads();  // 8000CB00
  void CopyForward(AptFormat* from);  // 8000CD18
  // PC: the particle part of the destructor (8000B1B0): the view, the emitters and their descriptions.
  void ReleaseParticles();

  int m_fontSizeOn = 0;      // +0x00 use m_fontSize
  int m_colorOn = 0;         // +0x04 use m_color
  int m_blinkOn = 0;         // +0x08
  int m_pulseOn = 0;         // +0x0C
  int m_matrixFx = 0;        // +0x10 1/2 rock (font fx 8/7), 3 rotate, 4 restore the matrix
  int m_field14 = 0;         // +0x14
  int m_outlineOn = 0;       // +0x18
  int m_searchlightOn = 0;   // +0x1C
  int m_enabled = 1;         // +0x20 (AptParagraph::SetFormattingEnable)
  float m_fontSize = 12.0f;  // +0x24
  float m_startTime = 0;     // +0x28 when the matrix effect started
  float m_blink[4] = {0.4f, 0.5f, 0.6f, 0.7f};  // +0x2C fade out from [0] to [1], off to [2], in to [3] (s)
  float m_pulse[4] = {0.4f, 0.5f, 0.6f, 0.7f};  // +0x30 shrink from [0] to [1], grow from [2] to [3]
  float m_pulseSize = 0;     // +0x34 the smallest size
  float m_fxSpeed = 0;       // +0x38
  float m_fxAmount = 0;      // +0x3C
  EVec4 m_color{0, 0, 0, 0};  // +0x40
  int m_field44 = 0;         // +0x44
  E3DWindow* m_particles = nullptr;  // +0x48 the particle effect's own 3D view (aptstring_particles.cpp)
  int m_particleRefs = 0;        // +0x4C the emitters' descriptions were asked for
  AptTextParticle* m_particleList = nullptr;  // +0x50
  float m_field54 = 1.0f;    // +0x54 the effect's time scale
  float m_field58 = 0.01f;   // +0x58 the word's width (pixels) to the effect view's
  float m_field5C = 0.01f;   // +0x5C the same for the height
  int m_particlesLoaded = 0;   // +0x60 the emitters are made
  unsigned char* m_fx = nullptr;  // +0x64 a fontFXcommand: type, then three effect bytes
  float m_fxStart = 0;       // +0x68
  float m_outline[3] = {1.0f, 0, 0};  // +0x6C
  float m_searchAngle = 0;   // +0x78
  float m_searchlight[3] = {0, 0, 0};  // +0x7C
  AptWord* m_word;           // +0x88
};

struct AptWord {
  AptWord();  // 8000FDA8
  virtual ~AptWord();  // 8000FE24
  virtual void Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate);  // 8000CEF4
  virtual void InsertSpace();  // 8000D724
  virtual void Resize(float fontSize);  // 8000D674
  void SetWord(const unsigned short* text, int count, EVec2& pos);  // 8000D5BC
  void AddSpace();  // 8000D82C
  void AddLineFeed();  // 8000D83C
  void ForceFormat(AptWord* from);  // 8000D84C

  AptWord* m_next = nullptr;          // +0x00
  AptLine* m_line = nullptr;          // +0x04
  unsigned short* m_text = nullptr;   // +0x08
  AptFormat* m_format = nullptr;      // +0x0C applied before the word
  AptFormat* m_endFormat = nullptr;   // +0x10 applied after it
  int m_lineFeeds = 0;                // +0x14 after the word
  int m_spaces = 0;                   // +0x18 after the word
  EVec2 m_pos{0.5f, 0.5f};            // +0x1C the centre (field pixels)
  EVec2 m_size{0, 0};                 // +0x24
};

struct AptShaderWord : AptWord {
  AptShaderWord();  // 8000FED4
  ~AptShaderWord() override;  // 8000FF5C
  void Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate) override;  // 8000D8E8
  void InsertSpace() override;  // 8000DDDC
  void Resize(float fontSize) override;  // 8000DC54
  void SetShader(int button);  // 8000DD44

  float m_spaceWidth = 0;  // +0x30
  int m_button = 0;        // +0x34 UIButtonImages index
};

struct AptLine {
  AptLine() = default;
  virtual ~AptLine();  // 8039D468
  void AddWord(AptWord* word);  // 8000DF70
  AptWord* UnlinkWord(AptWord* word);  // 8000DFA8
  void Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate);  // 8000DFF8

  AptLine* m_next = nullptr;          // +0x00
  AptWord* m_first = nullptr;         // +0x04
  AptParagraph* m_paragraph = nullptr;  // +0x08
};

struct AptParagraph {
  AptParagraph() = default;
  virtual ~AptParagraph();  // 8039D3D0
  void Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate);  // 8000E06C
  void AddLine(AptLine* line);  // 8000E0D4
  void Build(EVec2& topLeft, EVec2& bottomRight, float fontSize, EVec4& color, int alignment,
             const unsigned short* text);  // 8000E10C
  float GetParagraphHeight();  // 8000FC68
  float GetPrintWidth();  // 8000FC70
  int GetWideHexValue(const unsigned short* text, int at, int count);  // 8000FC78
  void SetFormattingEnable(int on);  // 8000FD38

  AptLine* m_first = nullptr;     // +0x00
  EVec2 m_centre{0.5f, 0.5f};     // +0x04
  EVec4 m_color{0, 0, 0, 0};      // +0x0C (GC: a pointer to it)
  EVec2 m_topLeft{0, 0};          // +0x10
  EVec2 m_bottomRight{0, 0};      // +0x18
  float m_fontSize = 0;           // +0x20
  float m_printWidth = 0;         // +0x24 the widest line while filling
  int m_alignment = 0;            // +0x28 0 left, 1 right, 2 centred
  float m_width = 0;              // +0x2C the widest line
  float m_height = 0;             // +0x30
};
