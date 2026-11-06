// Port of game/aptstring.cpp: the layout and drawing of Apt edit-text (AptParagraph, AptLine,
// AptWord, AptShaderWord, AptFormat); see aptstring.hpp. Text is UCS-2. Everything is measured and
// drawn with the Apt font (gpFont, aptauxpceorgl.cpp) at the paragraph's size.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstring>
#include <vector>

extern ERFont* gpFont;  // aptauxpceorgl.cpp
void AptTextParticleTest(AptParagraph* paragraph);  // aptstring_particles.cpp

namespace {
size_t Len16(const unsigned short* s) {
  size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
bool Has16(const unsigned short* s, unsigned short c) {
  for (; s && *s; ++s)
    if (*s == c) return true;
  return false;
}
// wcsncmp(s, tag, n) == 0 for an ASCII tag.
bool Starts16(const unsigned short* s, const char* tag, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (s[i] != static_cast<unsigned char>(tag[i])) return false;
    if (!s[i]) return true;
  }
  return true;
}
float Now() { return static_cast<float>(_pClockMan->GetSec(&_sysclock)); }
bool IsSpace(unsigned short c) { return c == ' ' || static_cast<unsigned>(c - 9) < 5; }
fontFXcommand* Fx(unsigned char* fx) { return reinterpret_cast<fontFXcommand*>(fx); }
const unsigned short kSpace[2] = {' ', 0};
const unsigned short kEmpty[1] = {0};

// s_restoreMatrix (804734C8): the text matrix before a rock or rotate effect changed it.
EMat4 s_restoreMatrix;
}  // namespace

// 8000D8DC
// The width of a pixel relative to its height.
float GetPixelXSize() { return 0.93333334f; }

// ---------------------------------------------------------------------------------------------------
// AptFormat

// 8000AF88
AptFormat::AptFormat(AptWord* word) : m_word(word) { m_fxStart = Now(); }

// 8000B1B0
AptFormat::~AptFormat() {
  ReleaseParticles();
  delete[] m_fx;
  m_fx = nullptr;
}

// 8000B344
// Applies the format before (or after) its word: font size, colour, the blink and pulse cycles, the
// rock/rotate effects (which change the matrix, saved for the restoring format), and the font effect
// command's animation.
void AptFormat::Draw(EMat4* matrix, EVec4* scale, EVec4* translate) {
  if (m_fontSizeOn) gpFont->SetSize(m_fontSize, 1.0f, true);
  if (!m_enabled) return;
  if (m_colorOn) {
    float a = m_color.w;
    if (scale && translate) a = a * scale->w + translate->w;
    gpFont->m_color = EVec4{m_color.x, m_color.y, m_color.z, a};
  }
  auto valid = [](const float* p) {
    return p[0] != 0.0f && p[1] != 0.0f && p[2] != 0.0f && p[3] != 0.0f && p[0] < p[1] && p[1] < p[2] && p[2] < p[3];
  };
  if (m_pulseOn && valid(m_pulse)) {
    const float* p = m_pulse;
    const float t = std::fmod(Now(), p[3]);
    if (t < p[0]) gpFont->SetSize(m_fontSize, 1.0f, true);
    else if (t < p[1]) gpFont->SetSize(m_fontSize - (t - p[0]) / (p[1] - p[0]) * (m_fontSize - m_pulseSize), 1.0f, true);
    else if (t >= p[2]) gpFont->SetSize((t - p[2]) / (p[3] - p[2]) * (m_fontSize - m_pulseSize) + m_pulseSize, 1.0f, true);
  }
  if (m_blinkOn && valid(m_blink)) {
    const float* b = m_blink;
    const float t = std::fmod(Now(), b[3]);
    if (t < b[2]) {
      if (t >= b[1]) gpFont->m_color.w = 0.18f;
      else if (t >= b[0]) gpFont->m_color.w = 1.0f - (t - b[0]) / (b[1] - b[0]) * 0.82f;
    } else {
      gpFont->m_color.w = (t - b[2]) / (b[3] - b[2]) * 0.82f + 0.18f;
    }
  }
  if (m_matrixFx && matrix) {
    const float now = Now();
    if (m_startTime == 0.0f) m_startTime = now;
    if (m_matrixFx == 1 || m_matrixFx == 2) {
      const float s = std::sin((now - m_startTime) * m_fxSpeed) * m_fxAmount;
      s_restoreMatrix = *matrix;
      if (m_fx) {  // GC: written through a null pointer when there was none
        m_fx[0] = m_matrixFx == 2 ? 7 : 8;
        const short v = static_cast<short>(static_cast<int>(s * 32767.0f));
        std::memcpy(m_fx + 2, &v, 2);
      }
    } else if (m_matrixFx == 3) {
      float t = std::fmod(now - m_startTime, m_fxAmount);
      s_restoreMatrix = *matrix;
      if (m_fx) {
        if (t < m_fxSpeed) {
          float a = (-std::cos(t / m_fxSpeed * 3.1415927f) * 0.5f + 0.5f) * 6.2831855f;
          if (std::fmod(now - m_startTime, m_fxAmount + m_fxAmount) >= m_fxAmount) a = 6.2831855f - a;
          m_fx[0] = 7;
          const short v = static_cast<short>(static_cast<int>(a * 32767.0f));
          std::memcpy(m_fx + 2, &v, 2);
        } else {
          m_fx[0] = 0;
          m_fx[2] = m_fx[3] = 0;
        }
      }
    } else if (m_matrixFx == 4) {
      *matrix = s_restoreMatrix;
    }
  }
  const int length = m_word && m_word->m_text ? static_cast<int>(Len16(m_word->m_text)) : 1;
  if (!m_fx) return;
  switch (m_fx[0]) {
    case 4: {  // tipsy: frames since the format was made
      const int frames = static_cast<int>((Now() - m_fxStart) * 60.0f);
      m_fx[2] = static_cast<unsigned char>(frames);
      m_fx[3] = static_cast<unsigned char>(static_cast<unsigned>(frames) >> 8);
      return;
    }
    case 2:  // wave: the phase
      m_fx[1] = static_cast<unsigned char>(static_cast<int>(std::fmod((Now() - m_fxStart) * 560.0f, 256.0f)));
      return;
    case 3: {  // bulge: the letter bulging runs back and forth along the word
      const int n2 = length * 2;
      int i = static_cast<int>((Now() - m_fxStart) * 15.0f);
      i -= i / n2 * n2;
      if (length - 1 < i) i = n2 - (i + 1);
      m_fx[3] = static_cast<unsigned char>(i > 0x7f ? 0x7f : i);
      return;
    }
    case 1: {  // rumble: a new random-ish phase m_fx[3] times a second
      const float now = Now();
      if (now - m_fxStart <= 1.0f / static_cast<float>(m_fx[3])) return;
      m_fxStart = now;
      m_fx[1] = static_cast<unsigned char>(static_cast<int>(std::fmod(now, 1.0f) * 256.0f));
      return;
    }
    case 5: {  // reveal: letters appear one after another, then again after a pause
      const float now = Now();
      int i = static_cast<int>((now - m_fxStart) * 5.0f * static_cast<float>(static_cast<signed char>(m_fx[1])) *
                               0.0078125f);
      if (length + m_fx[2] <= i) {
        m_fxStart = now;
        i = 0;
      }
      m_fx[3] = static_cast<unsigned char>(i > 0x7f ? 0x7f : i);
      return;
    }
    default: return;
  }
}

// 8000BD74 InitForParticles, 8000C628 ProcessParticles, 8000CB00 CheckAsyncLoads: aptstring_particles.cpp.

// 8000CD18
// Takes over the previous word's format (the particles move over rather than being copied).
void AptFormat::CopyForward(AptFormat* from) {
  m_fontSizeOn = from->m_fontSizeOn;
  m_colorOn = from->m_colorOn;
  m_blinkOn = from->m_blinkOn;
  m_pulseOn = from->m_pulseOn;
  m_matrixFx = from->m_matrixFx;
  m_field14 = from->m_field14;
  m_outlineOn = from->m_outlineOn;
  m_searchlightOn = from->m_searchlightOn;
  m_fontSize = from->m_fontSize;
  m_startTime = from->m_startTime;
  std::memcpy(m_blink, from->m_blink, sizeof m_blink);
  std::memcpy(m_pulse, from->m_pulse, sizeof m_pulse);
  m_pulseSize = from->m_pulseSize;
  m_fxSpeed = from->m_fxSpeed;
  m_fxAmount = from->m_fxAmount;
  m_color = from->m_color;
  m_field44 = from->m_field44;
  m_particles = from->m_particles;
  from->m_particles = nullptr;
  m_particleRefs = from->m_particleRefs;
  m_particleList = from->m_particleList;
  from->m_particleList = nullptr;
  m_field54 = from->m_field54;
  m_field58 = from->m_field58;
  m_field5C = from->m_field5C;
  m_particlesLoaded = from->m_particlesLoaded;
  if (from->m_fx) {
    if (!m_fx) m_fx = new unsigned char[4]();
    std::memcpy(m_fx, from->m_fx, 4);
  }
  m_fxStart = from->m_fxStart;
  std::memcpy(m_outline, from->m_outline, sizeof m_outline);
  m_searchAngle = from->m_searchAngle;
  std::memcpy(m_searchlight, from->m_searchlight, sizeof m_searchlight);
}

// ---------------------------------------------------------------------------------------------------
// AptWord

// 8000FDA8
AptWord::AptWord() = default;

// 8000FE24
AptWord::~AptWord() {
  delete[] m_text;
  if (m_format) {
    delete m_format;
    delete m_endFormat;
    m_endFormat = nullptr;
  }
}

// 8000CEF4
// The word centred on its position through the matrix: its format first; then (by the format) an
// outline (the word twice in the outline colour, 2.5 pixels apart) or a moving searchlight copy;
// otherwise, for light text, a black shadow 2 pixels down and right; then the word and the format
// after it.
void AptWord::Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate) {
  EVec2 pos = m_pos;
  bool rotate = true;
  fontFXcommand* fx = nullptr;
  auto draw = [&](EVec2 at, fontFXcommand* f) {
    gpFont->DoDrawAlign(rc, m_text, true, at, EFontAlignX::Center, EFontAlignY::Center, nullptr, matrix,
                        EFontMatrixType(rotate ? EFontMatrixType::Rotate : EFontMatrixType::Scale), f, false);
  };
  if (m_format) {
    if (m_endFormat && (m_format->m_colorOn || m_format->m_blinkOn)) {
      m_endFormat->m_color = gpFont->m_color;
      m_endFormat->m_colorOn = 1;
    }
    m_format->Draw(matrix, scale, translate);
    m_format->ProcessParticles(rc, matrix, pos, m_size, scale ? scale->w : 1.0f, translate ? translate->w : 0.0f);
    rotate = static_cast<unsigned>(m_format->m_matrixFx - 2) > 1;
    fx = Fx(m_format->m_fx);
  }
  const EVec4 colour = gpFont->m_color;
  const EVec2 offset{2.0f, 2.0f};
  bool shadow = true;
  if (m_format) {
    if (m_format->m_outlineOn) {
      gpFont->m_color = EVec4{m_format->m_outline[0], m_format->m_outline[1], m_format->m_outline[2], colour.w};
      unsigned char outlineFx[4] = {6, 0, 0, 0};
      draw(pos, Fx(outlineFx));
      draw(EVec2{pos.x + offset.x * 1.25f, pos.y}, Fx(outlineFx));
      gpFont->m_color = colour;
    }
    if (m_format->m_searchlightOn) {
      const float a = m_format->m_searchAngle;
      const float vx = std::cos(a) + std::cos(a), vy = std::sin(a);
      EMat4 r;
      r.RotateZ(0.7853982f);
      const float wx = vx * r.m[0][0] + vy * r.m[1][0], wy = vx * r.m[0][1] + vy * r.m[1][1];
      m_format->m_searchAngle += _dt * 4.0f / std::sqrt(wx * wx + wy * wy);
      const float dx = offset.x * wx, dy = offset.y * wy;
      EVec2 at = pos;
      if (!matrix) {
        at.x += dx;
        at.y += dy;
      } else {
        matrix->m[3][0] += dx;
        matrix->m[3][1] += dy;
      }
      gpFont->m_color =
          EVec4{m_format->m_searchlight[0], m_format->m_searchlight[1], m_format->m_searchlight[2], colour.w};
      draw(at, fx);
      gpFont->m_color = colour;
      if (matrix) {
        matrix->m[3][0] -= dx;
        matrix->m[3][1] -= dy;
      }
      shadow = false;
    }
  }
  if (shadow && colour.x > 0.5f && colour.y > 0.5f && colour.z > 0.5f && matrix) {
    gpFont->m_color = EVec4{0.0f, 0.0f, 0.0f, colour.w};  // _BLACK
    const float tx = matrix->m[3][0], ty = matrix->m[3][1];
    matrix->m[3][1] = ty + offset.y;
    matrix->m[3][0] = tx + offset.x;
    draw(pos, fx);
    matrix->m[3][0] = tx;
    matrix->m[3][1] = ty;
    gpFont->m_color = colour;
  }
  draw(pos, fx);
  if (m_format && m_endFormat) m_endFormat->Draw(matrix, scale, translate);
}

// 8000D5BC
// Takes `count` characters of the text and measures them at the current font size.
void AptWord::SetWord(const unsigned short* text, int count, EVec2& pos) {
  m_pos = pos;
  unsigned short* old = m_text;
  m_text = new unsigned short[static_cast<size_t>(count) + 1];
  for (int i = 0; i < count; ++i) m_text[i] = text[i];
  m_text[count] = 0;
  m_size = gpFont->DoGetStringSize(m_text, true, false, nullptr);
  delete[] old;
}

// 8000D674
// Re-measures at the format's size when the format changes the size.
void AptWord::Resize(float fontSize) {
  if (m_format && (m_format->m_fontSizeOn || m_format->m_pulseOn)) {
    gpFont->SetSize(m_format->m_fontSize, 1.0f, true);
    m_size = gpFont->DoGetStringSize(m_text, true, false, nullptr);
    gpFont->SetSize(fontSize, 1.0f, true);
  }
}

// 8000D724
void AptWord::InsertSpace() {
  const size_t n = Len16(m_text);
  auto* text = new unsigned short[n + 2];
  for (size_t i = 0; i < n; ++i) text[i] = m_text[i];
  text[n] = ' ';
  text[n + 1] = 0;
  delete[] m_text;
  m_text = text;
  if (m_format && (m_format->m_fontSizeOn || m_format->m_pulseOn)) gpFont->SetSize(m_format->m_fontSize, 1.0f, true);
  m_size = gpFont->DoGetStringSize(m_text, true, false, nullptr);
  if (m_endFormat && m_endFormat->m_fontSizeOn) gpFont->SetSize(m_endFormat->m_fontSize, 1.0f, true);
}

// 8000D82C
void AptWord::AddSpace() { ++m_spaces; }
// 8000D83C
void AptWord::AddLineFeed() { ++m_lineFeeds; }

// 8000D84C
// Gives the word its two formats (if it has none) and carries `from`'s formats over to them.
void AptWord::ForceFormat(AptWord* from) {
  if (!m_format) {
    m_format = new AptFormat(this);
    m_endFormat = new AptFormat(this);
  }
  if (from) {
    if (from->m_format) m_format->CopyForward(from->m_format);
    if (from->m_endFormat) m_endFormat->CopyForward(from->m_endFormat);
  }
}

// ---------------------------------------------------------------------------------------------------
// AptShaderWord

// 8000FED4
AptShaderWord::AptShaderWord() = default;
// 8000FF5C
AptShaderWord::~AptShaderWord() = default;

// 8000D8E8
// The button picture, as tall as a line of text (at least its minimum size) and square on screen,
// drawn as one rotated rectangle through the matrix and the 3D window's projection.
void AptShaderWord::Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate) {
  ERShader* r = UIButtonImages::GetButtonShader(m_button);
  if (!r) return;
  if (m_format) m_format->Draw(matrix, scale, translate);
  float alpha = 1.0f;
  if (scale && translate) alpha = alpha * scale->w + translate->w;
  m_size.y = gpFont->DoGetStringSize(const_cast<unsigned short*>(kEmpty), true, false, nullptr).y;
  const float minSize = static_cast<float>(UIButtonImages::GetButtonMinSize(m_button));
  if (m_size.y < minSize) m_size.y = minSize;
  m_size.x = m_size.y / GetPixelXSize();
  if (EShader* sh = r->GetShader()) sh->Select(rc, 0);
  const EMat4& m = *matrix;
  const float x = m_pos.x - m_spaceWidth * 0.5f, y = m_pos.y;
  const float cx = m.m[0][0] * x + m.m[1][0] * y, cy = m.m[0][1] * x + m.m[1][1] * y;
  EMat4 identity;
  const EMat4* p = &identity;  // GC: an uninitialised matrix when no 3D window was current
  if (EWindow::m_pCurrent3DWindow) p = &static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow)->m_normalizedProjection;
  const EMat4& P = *p;
  float rect[16] = {-m_size.x * 0.5f, -m_size.y * 0.5f, m_size.x * 0.5f, m_size.y * 0.5f,
                    0.0f, 1.0f, 1.0f, 0.0f,
                    m.m[0][0] * P.m[0][0], m.m[0][1] * P.m[1][1], (m.m[3][0] + cx) * P.m[0][0] + P.m[3][0], 0.0f,
                    m.m[1][0] * P.m[0][0], m.m[1][1] * P.m[1][1], (m.m[3][1] + cy) * P.m[1][1] + P.m[3][1], 0.0f};
  EVec4 colour{1.0f, 1.0f, 1.0f, alpha};
  rc->RectListRot(1, rect, colour, 0.0f);
  gpFont->Select(rc);
  if (m_format && m_endFormat) m_endFormat->Draw(nullptr, scale, translate);
}

// 8000DC54
void AptShaderWord::Resize(float fontSize) {
  if (m_format && (m_format->m_fontSizeOn || m_format->m_pulseOn)) {
    gpFont->SetSize(m_format->m_fontSize, 1.0f, true);
    m_size.y = gpFont->DoGetStringSize(const_cast<unsigned short*>(kEmpty), true, false, nullptr).y;
    const float minSize = static_cast<float>(UIButtonImages::GetButtonMinSize(m_button));
    if (m_size.y < minSize) m_size.y = minSize;
    m_size.x = m_size.y / GetPixelXSize() + m_spaceWidth;
    gpFont->SetSize(fontSize, 1.0f, true);
  }
}

// 8000DD44
void AptShaderWord::SetShader(int button) {
  m_button = button;
  m_size.y = gpFont->DoGetStringSize(const_cast<unsigned short*>(kEmpty), true, false, nullptr).y;
  const float minSize = static_cast<float>(UIButtonImages::GetButtonMinSize(m_button));
  if (m_size.y < minSize) m_size.y = minSize;
  m_size.x = m_size.y / GetPixelXSize() + m_spaceWidth;
}

// 8000DDDC
// A space after a button is the width of a space in the font.
void AptShaderWord::InsertSpace() {
  if (m_format && (m_format->m_fontSizeOn || m_format->m_pulseOn)) gpFont->SetSize(m_format->m_fontSize, 1.0f, true);
  AptWord space;
  EVec2 at{0.0f, 0.0f};
  space.SetWord(kSpace, 1, at);
  m_spaceWidth = space.m_size.x;
  m_size.x += space.m_size.x;
  if (m_endFormat) gpFont->SetSize(m_endFormat->m_fontSize, 1.0f, true);
}

// ---------------------------------------------------------------------------------------------------
// AptLine

// 8039D468
AptLine::~AptLine() {
  for (AptWord* w = m_first; w;) {
    AptWord* next = w->m_next;
    delete w;
    w = next;
  }
}

// 8000DF70
void AptLine::AddWord(AptWord* word) {
  if (!m_first) {
    m_first = word;
  } else {
    AptWord* w = m_first;
    while (w->m_next) w = w->m_next;
    w->m_next = word;
  }
  word->m_line = this;
}

// 8000DFA8
AptWord* AptLine::UnlinkWord(AptWord* word) {
  AptWord* prev = nullptr;
  for (AptWord* w = m_first; w; prev = w, w = w->m_next) {
    if (w != word) continue;
    if (prev) prev->m_next = w->m_next;
    else m_first = w->m_next;
    return w;
  }
  return nullptr;
}

// 8000DFF8
void AptLine::Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate) {
  for (AptWord* w = m_first; w;) {
    AptWord* next = w->m_next;
    w->Draw(rc, matrix, scale, translate);
    w = next;
  }
}

// ---------------------------------------------------------------------------------------------------
// AptParagraph

// 8039D3D0
AptParagraph::~AptParagraph() {
  for (AptLine* l = m_first; l;) {
    AptLine* next = l->m_next;
    delete l;
    l = next;
  }
}

// 8000E06C
void AptParagraph::Draw(ERC* rc, EMat4* matrix, EVec4* scale, EVec4* translate) {
  for (AptLine* l = m_first; l;) {
    AptLine* next = l->m_next;
    l->Draw(rc, matrix, scale, translate);
    l = next;
  }
}

// 8000E0D4
void AptParagraph::AddLine(AptLine* line) {
  if (!m_first) {
    m_first = line;
  } else {
    AptLine* l = m_first;
    while (l->m_next) l = l->m_next;
    l->m_next = line;
  }
  line->m_paragraph = this;
}

// 8000E10C
// Lays the text out in the box from topLeft to bottomRight (pixels) at `fontSize`. Text without
// embedded codes, line feeds or hyphenation points that fits is one word; otherwise it is split into
// words, the codes are turned into formats, and the words are filled into lines (one pixel apart;
// further line feeds add whole lines) aligned left, right or centred. Words not followed by a space
// count 3 pixels wider. A first line raised to a larger font is moved up to stay centred on the box's
// first line.
void AptParagraph::Build(EVec2& topLeft, EVec2& bottomRight, float fontSize, EVec4& color, int alignment,
                         const unsigned short* text) {
  m_topLeft = topLeft;
  m_bottomRight = bottomRight;
  m_alignment = alignment;
  EVec2 centre{(topLeft.x + bottomRight.x) * 0.5f, (topLeft.y + bottomRight.y) * 0.5f};
  m_centre = centre;
  m_color = color;
  m_fontSize = fontSize;
  const float boxWidth = bottomRight.x - topLeft.x;
  bottomRight.y = topLeft.y;  // (sic)
  gpFont->SetSize(m_fontSize, 1.0f, true);
  const EVec2 full = gpFont->DoGetStringSize(const_cast<unsigned short*>(text), true, false, nullptr);
  m_height = topLeft.y;
  const bool special = Has16(text, '<') || Has16(text, '\n') || Has16(text, '|');

  if (!special && !(boxWidth < full.x)) {
    auto* line = new AptLine;
    AddLine(line);
    auto* w = new AptWord;
    line->AddWord(w);
    float x;
    if (m_alignment == 0) x = topLeft.x;
    else if (m_alignment == 1) x = bottomRight.x - full.x;
    else x = (boxWidth * 0.5f + topLeft.x) - full.x * 0.5f;
    EVec2 pos{full.x * 0.5f + x, full.y * 0.5f + topLeft.y};
    w->SetWord(text, static_cast<int>(Len16(text)), pos);
    m_width = w->m_size.x;
    m_printWidth = full.x;
    m_height = w->m_size.y;
    return;
  }

  // Split into words (kept on a scratch line).
  auto* words = new AptLine;
  words->m_paragraph = this;
  const unsigned short* p = text;
  const unsigned short* const end = text + Len16(text);
  while (p < end) {
    bool atEnd = false, space = false, bar = false, lineFeed = false, open = false, close = false;
    const unsigned short* q = p;
    do {
      ++q;
      const unsigned short c = *q;
      if (c == '\n') lineFeed = true;
      if (IsSpace(c)) space = true;
      else if (c == '|') bar = true;
      else if (c == '<') open = true;
      else if (c == '>') close = true;
      else if (q == end) atEnd = true;
    } while (!bar && !space && !atEnd && !open && !close);
    auto* w = new AptWord;
    words->AddWord(w);
    int n;
    if (space || lineFeed || open) {
      n = static_cast<int>(q - p);
      w->SetWord(p, n, centre);
    } else if (close) {
      n = static_cast<int>(q - p) + 1;
      w->SetWord(p, n, centre);
    } else if (!bar) {
      n = static_cast<int>(Len16(p));
      w->SetWord(p, n, centre);
    } else {  // a hyphenation point: the word ends in '-'
      n = static_cast<int>(q - p) + 1;
      std::vector<unsigned short> hyphen(p, p + n);
      hyphen[static_cast<size_t>(n - 1)] = '-';
      w->SetWord(hyphen.data(), n, centre);
    }
    if (lineFeed) w->AddLineFeed();
    for (p += n; IsSpace(*p); ++p) {
      if (*p == '\n') w->AddLineFeed();
      else if (*p == ' ') w->AddSpace();
    }
  }

  // Embedded codes: each "<TAG_hex>" word sets up the formats of the word after it and is removed.
  auto hex = [&](AptWord* w, int at) { return GetWideHexValue(w->m_text, at, 2); };
  auto hex255 = [&](AptWord* w, int at) { return static_cast<float>(hex(w, at)) / 255.0f; };
  auto fxOf = [](AptWord* w) {
    if (!w->m_format->m_fx) w->m_format->m_fx = new unsigned char[4]();
    return w->m_format->m_fx;
  };
  // BLINK_/PULSE_ timings: four values, or two each followed by 0.01; scaled by 2.56 and replaced by
  // the defaults unless they rise.
  auto cycle = [&](AptWord* w, int first, bool four, float out[4]) {
    if (four) {
      for (int i = 0; i < 4; ++i) out[i] = hex255(w, first + 2 * i);
    } else {
      out[0] = hex255(w, first);
      out[1] = out[0] + 0.01f;
      out[2] = hex255(w, first + 2);
      out[3] = out[2] + 0.01f;
    }
    for (int i = 0; i < 4; ++i) out[i] *= 2.56f;
    if (out[0] == 0.0f || out[1] == 0.0f || out[2] == 0.0f || out[3] == 0.0f || out[1] <= out[0] ||
        out[2] <= out[1] || out[3] <= out[2]) {
      out[0] = 0.4f, out[1] = 0.5f, out[2] = 0.6f, out[3] = 0.7f;
    }
  };
  AptWord* prev = words->m_first;
  for (AptWord* cur = words->m_first; cur;) {
    AptWord* next = cur->m_next;
    AptWord* keep = cur;
    const unsigned short* t = cur->m_text;
    if (Has16(t, '<')) {
      const size_t len = Len16(t);
      bool remove = true;
      if (Starts16(t, "<COLOR_", 7) && len == 0x10 && next) {
        const float a = hex255(cur, 7), r = hex255(cur, 9), g = hex255(cur, 11), b = hex255(cur, 13);
        next->ForceFormat(cur);
        next->m_format->m_colorOn = 1;
        next->m_endFormat->m_colorOn = 1;
        next->m_format->m_color = EVec4{r, g, b, a};
        next->m_endFormat->m_color = m_color;
      } else if (Starts16(t, "<FONT_", 5) && len == 9 && next) {
        float s = hex255(cur, 6) * 2.56f;
        if (s < 0.25f) s = 0.25f;
        next->ForceFormat(cur);
        next->m_format->m_fontSizeOn = 1;
        next->m_endFormat->m_fontSizeOn = 1;
        next->m_format->m_fontSize = m_fontSize * s;
        next->m_endFormat->m_fontSize = m_fontSize;
      } else if (Starts16(t, "<PULSE_", 7) && (len == 0x12 || len == 0xe) && next) {
        float s = hex255(cur, 7) * 2.55f;
        if (s < 0.25f) s = 0.25f;
        float c[4];
        cycle(cur, 9, len == 0x12, c);
        next->ForceFormat(cur);
        next->m_format->m_pulseOn = 1;
        std::memcpy(next->m_format->m_pulse, c, sizeof c);
        next->m_format->m_fontSizeOn = 0;
        next->m_endFormat->m_fontSizeOn = 1;
        next->m_format->m_fontSize = m_fontSize * s;
        next->m_endFormat->m_fontSize = m_fontSize;
        next->m_format->m_pulseSize = m_fontSize;
      } else if (Starts16(t, "<BUTTON_", 8) && len == 0xb) {
        const int button = hex(cur, 8);
        if (button < 0xe) {  // replaced by the picture
          auto* b = new AptShaderWord;
          b->SetShader(button);
          b->m_next = next;
          b->m_spaces = cur->m_spaces;
          b->m_lineFeeds = cur->m_lineFeeds;
          if (cur->m_format) {
            b->ForceFormat(cur);
            if (b->m_format->m_fontSizeOn || b->m_format->m_pulseOn) b->Resize(m_fontSize);
          }
          if (words->m_first == cur) words->m_first = b;
          else prev->m_next = b;
          b->m_line = words;
          delete cur;
          keep = b;
        }
        remove = false;
      } else if (Starts16(t, "<ROCK_", 6) && len == 0xd && next) {
        const float speed = hex255(cur, 8) * 35.0f;
        const float amount = static_cast<float>(hex(cur, 10)) * 3.1415927f / 180.0f;
        next->ForceFormat(cur);
        unsigned char* fx = fxOf(next);
        fx[1] = 0;
        fx[2] = fx[3] = 0;
        if (t[6] == 'W') {
          fx[0] = 8;
          next->m_format->m_matrixFx = 1;
        } else {
          fx[0] = 7;
          next->m_format->m_matrixFx = 2;
        }
        next->m_format->m_fxSpeed = speed;
        next->m_format->m_fxAmount = amount;
        next->m_endFormat->m_matrixFx = 4;
      } else if (Starts16(t, "<ROTATE_", 8) && len == 0xd && next) {
        float speed = hex255(cur, 8) * 8.0f;
        float amount = hex255(cur, 10) * 8.0f;
        if (speed < 0.05f) speed = 0.05f;
        if (amount < speed) amount = speed;
        next->ForceFormat(cur);
        next->m_format->m_matrixFx = 3;
        next->m_format->m_fxSpeed = speed;
        next->m_format->m_fxAmount = amount;
        next->m_endFormat->m_matrixFx = 4;
      } else if (Starts16(t, "<BLINK_", 7) && (len == 0x10 || len == 0xc) && next) {
        float c[4];
        cycle(cur, 7, len == 0x10, c);
        next->ForceFormat(cur);
        next->m_format->m_blinkOn = 1;
        std::memcpy(next->m_format->m_blink, c, sizeof c);
        next->m_format->m_fontSizeOn = 0;
        next->m_endFormat->m_fontSizeOn = 0;
      } else if (Starts16(t, "<PARTICLE_", 10) && len == 0xd && next) {
        int type = hex(cur, 10);
        if (type > 8) type = 0;
        next->ForceFormat(cur);
        next->m_format->InitForParticles(type);
      } else if (Starts16(t, "<RUMBLE_", 8) && len == 0xd && next) {
        int rate = hex(cur, 8);
        const int amount = hex(cur, 10);
        if (rate < 1) rate = 10;
        next->ForceFormat(cur);
        unsigned char* fx = fxOf(next);
        fx[0] = 1;
        fx[2] = static_cast<unsigned char>(amount);
        fx[3] = static_cast<unsigned char>(rate);
      } else if (Starts16(t, "<WAVE_", 6) && len == 0xb && next) {
        const int a = hex(cur, 6), b = hex(cur, 8);
        next->ForceFormat(cur);
        unsigned char* fx = fxOf(next);
        fx[0] = 2;
        fx[2] = static_cast<unsigned char>(a);
        fx[3] = static_cast<unsigned char>(b);
      } else if (Starts16(t, "<OUTLINE_", 9) && len == 0x10 && next) {
        const float r = hex255(cur, 9), g = hex255(cur, 11), b = hex255(cur, 13);
        next->ForceFormat(cur);
        next->m_format->m_outlineOn = 1;
        next->m_format->m_outline[0] = r;
        next->m_format->m_outline[1] = g;
        next->m_format->m_outline[2] = b;
      } else if (Starts16(t, "<SEARCHLIGHT_", 0xd) && len == 0x14 && next) {
        next->ForceFormat(cur);
        next->m_format->m_searchlightOn = 1;
        next->m_format->m_searchlight[0] = hex255(cur, 0xd);
        next->m_format->m_searchlight[1] = hex255(cur, 0xf);
        next->m_format->m_searchlight[2] = hex255(cur, 0x11);
      } else if (Starts16(t, "<BULGE_", 7) && len == 0xc && next) {
        next->ForceFormat(cur);
        unsigned char* fx = fxOf(next);
        fx[0] = 3;
        fx[1] = static_cast<unsigned char>(hex(cur, 7));
        fx[2] = static_cast<unsigned char>(hex(cur, 9));
      } else if (Starts16(t, "<TIPSY>", 7) && next) {
        next->ForceFormat(cur);
        unsigned char* fx = fxOf(next);
        fx[0] = 4;
        fx[1] = 0x17;
      } else if (Starts16(t, "<REVEAL_", 8) && len == 0xd && next) {
        next->ForceFormat(cur);
        unsigned char* fx = fxOf(next);
        fx[0] = 5;
        fx[1] = static_cast<unsigned char>(hex(cur, 8));
        fx[2] = static_cast<unsigned char>(hex(cur, 10));
      } else {
        remove = false;  // not a code: shown as text
      }
      if (remove) {
        if (prev == cur) prev = next;
        if (AptWord* w = words->UnlinkWord(cur)) delete w;
        keep = prev;
      }
    }
    prev = keep;
    cur = next;
  }

  // Fill the lines.
  float y = topLeft.y;
  do {
    auto* line = new AptLine;
    AddLine(line);
    int lineFeeds = 0;
    bool larger = false, full_ = false;
    float lineHeight = 0.0f, lineWidth = 0.0f;
    for (AptWord* w = words->m_first; w && !full_; w = words->m_first) {
      w->Resize(m_fontSize);
      if (lineWidth == 0.0f || lineWidth + w->m_size.x < boxWidth) {
        words->m_first = w->m_next;
        if (words->m_first) {
          if (w->m_spaces) w->InsertSpace();
          if (w->m_lineFeeds) full_ = true;
        }
        line->AddWord(w);
        w->m_next = nullptr;
        const float h = w->m_size.y;
        lineWidth += w->m_size.x;
        lineFeeds = w->m_lineFeeds;
        if (lineHeight < h) {
          lineHeight = h;
          larger = w->m_format && w->m_format->m_fontSizeOn && m_fontSize < w->m_format->m_fontSize;
        }
        if (w->m_spaces == 0) {
          lineWidth += 3.0f;
          w->m_size.x += 3.0f;
        }
        if (m_printWidth < lineWidth) m_printWidth = lineWidth;
      } else {
        full_ = true;
      }
    }
    if (y == topLeft.y && larger) {
      y = y - (lineHeight - full.y) * 0.5f;
      m_height = y;
    }
    for (AptWord* w = line->m_first; w; w = w->m_next) w->m_pos.y = lineHeight * 0.5f + y;
    float width = 0.0f;
    for (AptWord* w = line->m_first; w; w = w->m_next) width += w->m_size.x;
    if (m_width < width) m_width = width;
    float x;
    if (m_alignment == 0) {
      x = topLeft.x;
    } else {
      if (m_alignment == 1) {
        x = bottomRight.x;
      } else {
        width *= 0.5f;
        x = boxWidth * 0.5f + topLeft.x;
      }
      x -= width;
    }
    for (AptWord* w = line->m_first; w; w = w->m_next) {
      w->m_pos.x = w->m_size.x * 0.5f + x;
      x += w->m_size.x;
    }
    y = y + lineHeight + 1.0f;
    if (lineFeeds > 0) {
      if (lineFeeds > 20) lineFeeds = 20;
      for (; lineFeeds > 1; --lineFeeds) y += full.y;
    }
  } while (words->m_first);
  m_height = y - m_height - 1.0f;
  delete words;
  AptTextParticleTest(this);  // PC development aid (aptstring_particles.cpp)
}

// 8000FC68
float AptParagraph::GetParagraphHeight() { return m_height; }
// 8000FC70
float AptParagraph::GetPrintWidth() { return m_printWidth; }

// 8000FC78
// `count` hex digits at `at` (other characters count as 0).
int AptParagraph::GetWideHexValue(const unsigned short* text, int at, int count) {
  int value = 0, place = 1;
  for (int i = count - 1; i >= 0; --i) {
    const unsigned c = text[at + i];
    if (c - '0' < 10) value += static_cast<int>(c - '0') * place;
    else if (c - 'A' < 6) value += static_cast<int>(c - 'A' + 10) * place;
    else if (c - 'a' < 6) value += static_cast<int>(c - 'a' + 10) * place;
    place <<= 4;
  }
  return value;
}

// 8000FD38
// GC: the inner loop never advanced, so this hung on any line with a word; it has no callers.
void AptParagraph::SetFormattingEnable(int on) {
  for (AptLine* l = m_first; l; l = l->m_next)
    for (AptWord* w = l->m_first; w; w = w->m_next) {
      if (w->m_format) w->m_format->m_enabled = on != 0;
      if (w->m_endFormat) w->m_endFormat->m_enabled = on != 0;
    }
}
