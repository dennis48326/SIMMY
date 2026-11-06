// SimmyLauncher: 2D drawing on an SDL_Renderer: anti-aliased convex shapes with per-vertex colours
// (gradients), rounded panels, soft glows, and text from the embedded Poppins fonts (stb_truetype).
//
// Coordinates are logical (the 1100x680 layout); the canvas multiplies them by the UI scale (the
// window's pixel density / display scale) and every edge gets a one-pixel feather in device pixels.
#pragma once
#include <SDL3/SDL_render.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace launcher {

struct Color {
  float r = 0, g = 0, b = 0, a = 1;
};
inline Color rgb(uint32_t hex, float a = 1.0f) {
  return Color{static_cast<float>((hex >> 16) & 255) / 255.0f, static_cast<float>((hex >> 8) & 255) / 255.0f,
               static_cast<float>(hex & 255) / 255.0f, a};
}
inline Color withAlpha(Color c, float a) { return Color{c.r, c.g, c.b, c.a * a}; }
inline Color mix(Color a, Color b, float t) {
  return Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

struct Vec2 {
  float x = 0, y = 0;
};

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  float cx() const { return x + w * 0.5f; }
  float cy() const { return y + h * 0.5f; }
  Rect inset(float d) const { return Rect{x + d, y + d, w - 2 * d, h - 2 * d}; }
};

enum FontId { kFontWord, kFontTitle, kFontButtonBig, kFontButton, kFontHeading, kFontBody, kFontSmall, kFontTiny,
              kFontLabel, kFontCount };
enum class Align { Left, Center, Right };

class Canvas {
 public:
  ~Canvas();
  // (Re)builds the font atlas for `scale` device pixels per logical unit (`fonts` false: shapes only,
  // e.g. for the window icon).
  bool init(SDL_Renderer* renderer, float scale, bool fonts = true);
  float scale() const { return scale_; }
  SDL_Renderer* renderer() const { return renderer_; }

  void flush();

  // Global opacity (page transitions) and a transform stack (hover / press zoom around a point).
  float alpha = 1.0f;
  void pushZoom(float cx, float cy, float s);
  void pushOffset(float dx, float dy);
  void pop();

  void fillConvex(const Vec2* points, const Color* colors, int count);
  void fillConvex(const Vec2* points, int count, Color color);
  void rect(Rect r, Color top, Color bottom);
  void roundRect(Rect r, float radius, Color top, Color bottom);
  void roundRect(Rect r, float radius, Color c) { roundRect(r, radius, c, c); }
  void roundRectStroke(Rect r, float radius, float width, Color c);
  // A soft shadow or glow around a rounded rect: `c` at its edge fading out over `spread`.
  void roundRectGlow(Rect r, float radius, float spread, Color c);
  void circle(Vec2 center, float radius, Color c);
  void circleGradient(Vec2 center, float radius, Color top, Color bottom);
  void ring(Vec2 center, float radius, float width, Color c);
  // `c` in the middle, fading to transparent at `radius` (clouds, glows).
  void softCircle(Vec2 center, float radius, Color c) { ellipseSoft(center, radius, radius, c); }
  void ellipseSoft(Vec2 center, float rx, float ry, Color c);
  void line(Vec2 a, Vec2 b, float width, Color c);
  // A chevron "<" (dir -1) or ">" (dir +1), or "v" (dir 0) centred on c.
  void chevron(Vec2 c, float size, int dir, float width, Color col);
  void check(Vec2 c, float size, float width, Color col);

  // Text: `y` is the baseline. textMid centres the capital letters on `midY`.
  float textWidth(FontId f, const std::string& s) const;
  float ascent(FontId f) const;
  float capHeight(FontId f) const;
  float lineHeight(FontId f) const;
  void text(FontId f, float x, float y, const std::string& s, Color c, Align align = Align::Left);
  void textGradient(FontId f, float x, float y, const std::string& s, Color top, Color bottom, Align align = Align::Left);
  void textMid(FontId f, float x, float midY, const std::string& s, Color c, Align align = Align::Left) {
    text(f, x, midY + capHeight(f) * 0.5f, s, c, align);
  }
  // Text with a soft drop shadow (for white text on the sky / glossy buttons).
  void textShadow(FontId f, float x, float y, const std::string& s, Color c, Color shadow, Align align = Align::Left,
                  float dy = 1.5f);
  // Word-wrapped into `width`; returns the height used. `maxLines` > 0 cuts with an ellipsis.
  float textWrapped(FontId f, float x, float y, float width, const std::string& s, Color c, float lineGap = 1.0f,
                    int maxLines = 0, Align align = Align::Left);
  // Shortens `s` with "..." to fit `width`.
  std::string ellipsize(FontId f, const std::string& s, float width) const;

  void image(SDL_Texture* tex, Rect dst, Color tint = Color{1, 1, 1, 1});

 private:
  struct Glyph {
    float x0, y0, x1, y1;  // quad relative to the pen, device pixels
    float u0, v0, u1, v1;
    float advance;         // device pixels
  };
  struct FontSlot {
    std::unordered_map<uint32_t, Glyph> glyphs;
    float ascent = 0, descent = 0, lineGap = 0, cap = 0;  // device pixels
    int file = 0;
    float pxScale = 0;  // stbtt scale for kerning
  };
  struct Xform {
    float s = 1, tx = 0, ty = 0;
  };
  Vec2 map(Vec2 p) const;
  void emit(Vec2 p, Color c, float u = 0, float v = 0);
  void useTexture(SDL_Texture* t);
  const Glyph* glyph(const FontSlot& slot, uint32_t cp) const;
  void drawText(FontId f, float x, float y, const std::string& s, Color top, Color bottom, Align align);
  void roundRectPath(Rect r, float radius, std::vector<Vec2>& out) const;
  void strokePath(const std::vector<Vec2>& path, float inner, float outer, Color cin, Color cout);

  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* atlas_ = nullptr;
  SDL_Texture* batchTex_ = nullptr;
  float scale_ = 1.0f;
  FontSlot fonts_[kFontCount];
  std::vector<Xform> stack_{Xform{}};
  std::vector<SDL_Vertex> verts_;
  std::vector<int> indices_;
  void* fontInfo_[2] = {nullptr, nullptr};  // stbtt_fontinfo
};

// UTF-8 decoding for the text functions (invalid bytes become U+FFFD).
uint32_t next_codepoint(const std::string& s, size_t& i);

}  // namespace launcher
