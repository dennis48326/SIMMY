// SimmyLauncher: drawing (see canvas.hpp).
#include "canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wextra"
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// The embedded fonts (generated from fonts/*.ttf by embed_file.cmake).
#include "launcher_font_bold.inc"
#include "launcher_font_medium.inc"

namespace launcher {

namespace {
struct SlotSpec {
  int file;     // 0 bold, 1 medium
  float size;   // em size in logical pixels
  bool latin1;  // also the Latin-1 letters and typographic punctuation (disc banners, messages)
  bool upper;   // only ASCII 32..95 (the wordmark)
};
const SlotSpec kSpecs[kFontCount] = {
    {0, 100.0f, false, true},  // kFontWord
    {0, 32.0f, true, false},   // kFontTitle
    {0, 27.0f, true, false},   // kFontButtonBig
    {0, 19.0f, true, false},   // kFontButton
    {0, 17.0f, true, false},   // kFontHeading
    {1, 15.0f, true, false},   // kFontBody
    {1, 13.5f, true, false},   // kFontSmall
    {1, 12.0f, true, false},   // kFontTiny
    {0, 11.5f, true, false},   // kFontLabel
};
const int kExtraCodepoints[] = {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x00D7};

const float kPi = 3.14159265358979f;

Vec2 Sub(Vec2 a, Vec2 b) { return Vec2{a.x - b.x, a.y - b.y}; }
Vec2 Add(Vec2 a, Vec2 b) { return Vec2{a.x + b.x, a.y + b.y}; }
Vec2 Mul(Vec2 a, float s) { return Vec2{a.x * s, a.y * s}; }
Vec2 Norm(Vec2 a) {
  const float l = std::sqrt(a.x * a.x + a.y * a.y);
  return l > 1e-6f ? Vec2{a.x / l, a.y / l} : Vec2{0, 0};
}
float SignedArea(const Vec2* p, int n) {
  float a = 0;
  for (int i = 0; i < n; ++i) {
    const Vec2& u = p[i];
    const Vec2& v = p[(i + 1) % n];
    a += u.x * v.y - v.x * u.y;
  }
  return a * 0.5f;
}
// Outward unit normals at each vertex of a closed path (any orientation), miter-scaled.
void VertexNormals(const Vec2* p, int n, std::vector<Vec2>& out) {
  out.resize(static_cast<size_t>(n));
  const float sign = SignedArea(p, n) >= 0 ? 1.0f : -1.0f;
  for (int i = 0; i < n; ++i) {
    const Vec2 a = p[(i + n - 1) % n], b = p[i], c = p[(i + 1) % n];
    const Vec2 e0 = Norm(Sub(b, a)), e1 = Norm(Sub(c, b));
    // For a positive-area path in y-down screen space the outward normal of edge e is (e.y, -e.x).
    const Vec2 n0{e0.y * sign, -e0.x * sign}, n1{e1.y * sign, -e1.x * sign};
    Vec2 m = Norm(Add(n0, n1));
    if (m.x == 0 && m.y == 0) m = n1;
    const float d = m.x * n1.x + m.y * n1.y;
    out[static_cast<size_t>(i)] = Mul(m, 1.0f / std::max(d, 0.35f));
  }
}
}  // namespace

uint32_t next_codepoint(const std::string& s, size_t& i) {
  const unsigned char c = static_cast<unsigned char>(s[i++]);
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
  if (extra < 0) return 0xFFFD;
  uint32_t cp = c & (0x3F >> extra);
  for (; extra > 0; --extra) {
    if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) return 0xFFFD;
    cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
  }
  return cp;
}

Canvas::~Canvas() {
  if (atlas_) SDL_DestroyTexture(atlas_);
  for (void* f : fontInfo_) delete static_cast<stbtt_fontinfo*>(f);
}

bool Canvas::init(SDL_Renderer* renderer, float scale, bool fonts) {
  renderer_ = renderer;
  scale_ = scale;
  stack_.assign(1, Xform{scale, 0, 0});
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
  if (!fonts) return true;
  const unsigned char* files[2] = {launcher_font_bold, launcher_font_medium};
  for (int i = 0; i < 2; ++i) {
    if (!fontInfo_[i]) {
      auto* info = new stbtt_fontinfo;
      if (!stbtt_InitFont(info, files[i], stbtt_GetFontOffsetForIndex(files[i], 0))) {
        delete info;
        return false;
      }
      fontInfo_[i] = info;
    }
  }

  std::vector<int> latin;
  for (int c = 0xA0; c <= 0xFF; ++c) latin.push_back(c);
  latin.insert(latin.end(), std::begin(kExtraCodepoints), std::end(kExtraCodepoints));

  for (int size = 1024; size <= 8192; size *= 2) {
    std::vector<unsigned char> pixels(static_cast<size_t>(size) * size, 0);
    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, pixels.data(), size, size, 0, 1, nullptr)) return false;
    bool ok = true;
    std::vector<stbtt_packedchar> ascii[kFontCount], extra[kFontCount];
    for (int f = 0; f < kFontCount && ok; ++f) {
      const SlotSpec& spec = kSpecs[f];
      const float px = spec.size * scale;
      stbtt_PackSetOversampling(&pc, px < 40.0f ? 2 : 1, 1);
      stbtt_pack_range ranges[2] = {};
      const int asciiCount = spec.upper ? 64 : 95;
      ascii[f].resize(static_cast<size_t>(asciiCount));
      ranges[0].font_size = STBTT_POINT_SIZE(px);
      ranges[0].first_unicode_codepoint_in_range = 32;
      ranges[0].num_chars = asciiCount;
      ranges[0].chardata_for_range = ascii[f].data();
      int rangeCount = 1;
      if (spec.latin1) {
        extra[f].resize(latin.size());
        ranges[1].font_size = STBTT_POINT_SIZE(px);
        ranges[1].array_of_unicode_codepoints = latin.data();
        ranges[1].num_chars = static_cast<int>(latin.size());
        ranges[1].chardata_for_range = extra[f].data();
        rangeCount = 2;
      }
      ok = stbtt_PackFontRanges(&pc, files[spec.file], 0, ranges, rangeCount) != 0;
    }
    stbtt_PackEnd(&pc);
    if (!ok) continue;

    for (int f = 0; f < kFontCount; ++f) {
      FontSlot& slot = fonts_[f];
      const SlotSpec& spec = kSpecs[f];
      const auto* info = static_cast<const stbtt_fontinfo*>(fontInfo_[spec.file]);
      slot.glyphs.clear();
      slot.file = spec.file;
      slot.pxScale = stbtt_ScaleForMappingEmToPixels(info, spec.size * scale);
      int asc = 0, desc = 0, gap = 0;
      stbtt_GetFontVMetrics(info, &asc, &desc, &gap);
      slot.ascent = static_cast<float>(asc) * slot.pxScale;
      slot.descent = static_cast<float>(-desc) * slot.pxScale;
      slot.lineGap = static_cast<float>(gap) * slot.pxScale;
      int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
      stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1);
      slot.cap = static_cast<float>(y1) * slot.pxScale;
      auto add = [&](const std::vector<stbtt_packedchar>& data, int index, uint32_t cp) {
        float xpos = 0, ypos = 0;
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(data.data(), size, size, index, &xpos, &ypos, &q, 0);
        slot.glyphs[cp] = Glyph{q.x0, q.y0, q.x1, q.y1, q.s0, q.t0, q.s1, q.t1, xpos};
      };
      for (int i = 0; i < static_cast<int>(ascii[f].size()); ++i) add(ascii[f], i, static_cast<uint32_t>(32 + i));
      for (int i = 0; i < static_cast<int>(extra[f].size()); ++i) add(extra[f], i, static_cast<uint32_t>(latin[static_cast<size_t>(i)]));
    }

    std::vector<unsigned char> rgba(static_cast<size_t>(size) * size * 4);
    for (size_t i = 0; i < pixels.size(); ++i) {
      rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
      rgba[i * 4 + 3] = pixels[i];
    }
    if (atlas_) SDL_DestroyTexture(atlas_);
    atlas_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, size, size);
    if (!atlas_) return false;
    SDL_UpdateTexture(atlas_, nullptr, rgba.data(), size * 4);
    SDL_SetTextureBlendMode(atlas_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas_, SDL_SCALEMODE_LINEAR);
    return true;
  }
  return false;
}

// ---- batching -------------------------------------------------------------------------------------

void Canvas::flush() {
  if (!indices_.empty())
    SDL_RenderGeometry(renderer_, batchTex_, verts_.data(), static_cast<int>(verts_.size()), indices_.data(),
                       static_cast<int>(indices_.size()));
  verts_.clear();
  indices_.clear();
}

void Canvas::useTexture(SDL_Texture* t) {
  if (t != batchTex_) {
    flush();
    batchTex_ = t;
  }
  if (verts_.size() > 60000) flush();
}

void Canvas::emit(Vec2 p, Color c, float u, float v) {
  SDL_Vertex vx;
  vx.position = SDL_FPoint{p.x, p.y};
  vx.color = SDL_FColor{c.r, c.g, c.b, std::clamp(c.a * alpha, 0.0f, 1.0f)};
  vx.tex_coord = SDL_FPoint{u, v};
  verts_.push_back(vx);
}

Vec2 Canvas::map(Vec2 p) const {
  const Xform& x = stack_.back();
  return Vec2{p.x * x.s + x.tx, p.y * x.s + x.ty};
}

void Canvas::pushZoom(float cx, float cy, float s) {
  const Xform& o = stack_.back();
  // p -> ((p - c) * s + c) * o.s + o.t
  stack_.push_back(Xform{s * o.s, (cx * (1 - s)) * o.s + o.tx, (cy * (1 - s)) * o.s + o.ty});
}
void Canvas::pushOffset(float dx, float dy) {
  const Xform& o = stack_.back();
  stack_.push_back(Xform{o.s, dx * o.s + o.tx, dy * o.s + o.ty});
}
void Canvas::pop() {
  if (stack_.size() > 1) stack_.pop_back();
}

// ---- shapes ---------------------------------------------------------------------------------------

void Canvas::fillConvex(const Vec2* points, const Color* colors, int n) {
  if (n < 3) return;
  useTexture(nullptr);
  std::vector<Vec2> dev(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) dev[static_cast<size_t>(i)] = map(points[i]);
  std::vector<Vec2> normals;
  VertexNormals(dev.data(), n, normals);
  const int base = static_cast<int>(verts_.size());
  // Inner ring (half a pixel in), outer ring (half a pixel out, transparent): a one-pixel feather.
  for (int i = 0; i < n; ++i) emit(Sub(dev[static_cast<size_t>(i)], Mul(normals[static_cast<size_t>(i)], 0.5f)), colors[i]);
  for (int i = 0; i < n; ++i)
    emit(Add(dev[static_cast<size_t>(i)], Mul(normals[static_cast<size_t>(i)], 0.5f)), withAlpha(colors[i], 0.0f));
  for (int i = 1; i + 1 < n; ++i) indices_.insert(indices_.end(), {base, base + i, base + i + 1});
  for (int i = 0; i < n; ++i) {
    const int j = (i + 1) % n;
    indices_.insert(indices_.end(), {base + i, base + n + i, base + n + j, base + i, base + n + j, base + j});
  }
}

void Canvas::fillConvex(const Vec2* points, int n, Color c) {
  std::vector<Color> cols(static_cast<size_t>(n), c);
  fillConvex(points, cols.data(), n);
}

void Canvas::rect(Rect r, Color top, Color bottom) {
  const Vec2 p[4] = {{r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}};
  const Color c[4] = {top, top, bottom, bottom};
  fillConvex(p, c, 4);
}

void Canvas::roundRectPath(Rect r, float radius, std::vector<Vec2>& out) const {
  radius = std::max(0.0f, std::min(radius, std::min(r.w, r.h) * 0.5f));
  const float px = radius * stack_.back().s;
  const int seg = std::clamp(static_cast<int>(px / 2.5f), 2, 20);
  out.clear();
  const Vec2 centers[4] = {{r.x + r.w - radius, r.y + radius},
                           {r.x + r.w - radius, r.y + r.h - radius},
                           {r.x + radius, r.y + r.h - radius},
                           {r.x + radius, r.y + radius}};
  for (int c = 0; c < 4; ++c) {
    const float a0 = -kPi * 0.5f + c * kPi * 0.5f;
    for (int i = 0; i <= seg; ++i) {
      const float a = a0 + (kPi * 0.5f) * static_cast<float>(i) / static_cast<float>(seg);
      const Vec2 p{centers[c].x + std::cos(a) * radius, centers[c].y + std::sin(a) * radius};
      // Corners meet without a straight edge on a pill's ends: skip the repeated point (a zero-length
      // edge has no normal).
      if (!out.empty() && std::fabs(out.back().x - p.x) + std::fabs(out.back().y - p.y) < 1e-3f) continue;
      out.push_back(p);
    }
  }
  if (out.size() > 1 && std::fabs(out.back().x - out.front().x) + std::fabs(out.back().y - out.front().y) < 1e-3f)
    out.pop_back();
}

void Canvas::roundRect(Rect r, float radius, Color top, Color bottom) {
  std::vector<Vec2> path;
  roundRectPath(r, radius, path);
  std::vector<Color> cols(path.size());
  for (size_t i = 0; i < path.size(); ++i) cols[i] = mix(top, bottom, r.h > 0 ? (path[i].y - r.y) / r.h : 0.0f);
  fillConvex(path.data(), cols.data(), static_cast<int>(path.size()));
}

void Canvas::strokePath(const std::vector<Vec2>& path, float inner, float outer, Color cin, Color cout) {
  // A band between offsets `inner` and `outer` (device pixels along the outward normal).
  const int n = static_cast<int>(path.size());
  if (n < 3) return;
  useTexture(nullptr);
  std::vector<Vec2> dev(path.size());
  for (int i = 0; i < n; ++i) dev[static_cast<size_t>(i)] = map(path[static_cast<size_t>(i)]);
  std::vector<Vec2> normals;
  VertexNormals(dev.data(), n, normals);
  const int base = static_cast<int>(verts_.size());
  for (int i = 0; i < n; ++i) {
    emit(Add(dev[static_cast<size_t>(i)], Mul(normals[static_cast<size_t>(i)], inner)), cin);
    emit(Add(dev[static_cast<size_t>(i)], Mul(normals[static_cast<size_t>(i)], outer)), cout);
  }
  for (int i = 0; i < n; ++i) {
    const int j = (i + 1) % n;
    const int a = base + 2 * i, b = base + 2 * j;
    indices_.insert(indices_.end(), {a, a + 1, b + 1, a, b + 1, b});
  }
}

void Canvas::roundRectStroke(Rect r, float radius, float width, Color c) {
  std::vector<Vec2> path;
  roundRectPath(r, radius, path);
  const float w = width * stack_.back().s;
  // Centred on the edge: feather, solid band, feather.
  strokePath(path, -w * 0.5f - 0.5f, -w * 0.5f + 0.5f, withAlpha(c, 0), c);
  strokePath(path, -w * 0.5f + 0.5f, w * 0.5f - 0.5f, c, c);
  strokePath(path, w * 0.5f - 0.5f, w * 0.5f + 0.5f, c, withAlpha(c, 0));
}

void Canvas::roundRectGlow(Rect r, float radius, float spread, Color c) {
  std::vector<Vec2> path;
  roundRectPath(r, radius, path);
  const float s = spread * stack_.back().s;
  strokePath(path, 0.0f, s * 0.35f, c, withAlpha(c, 0.45f));
  strokePath(path, s * 0.35f, s, withAlpha(c, 0.45f), withAlpha(c, 0));
}

void Canvas::circle(Vec2 center, float radius, Color c) { circleGradient(center, radius, c, c); }

void Canvas::circleGradient(Vec2 center, float radius, Color top, Color bottom) {
  const float px = radius * stack_.back().s;
  const int seg = std::clamp(static_cast<int>(px * 0.9f), 12, 120);
  std::vector<Vec2> p(static_cast<size_t>(seg));
  std::vector<Color> c(static_cast<size_t>(seg));
  for (int i = 0; i < seg; ++i) {
    const float a = 2 * kPi * static_cast<float>(i) / static_cast<float>(seg);
    p[static_cast<size_t>(i)] = Vec2{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
    c[static_cast<size_t>(i)] = mix(top, bottom, (std::sin(a) + 1) * 0.5f);
  }
  fillConvex(p.data(), c.data(), seg);
}

void Canvas::ring(Vec2 center, float radius, float width, Color c) {
  const float px = radius * stack_.back().s;
  const int seg = std::clamp(static_cast<int>(px * 0.9f), 12, 120);
  std::vector<Vec2> p(static_cast<size_t>(seg));
  for (int i = 0; i < seg; ++i) {
    const float a = 2 * kPi * static_cast<float>(i) / static_cast<float>(seg);
    p[static_cast<size_t>(i)] = Vec2{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
  }
  const float w = width * stack_.back().s;
  strokePath(p, -w * 0.5f - 0.5f, -w * 0.5f + 0.5f, withAlpha(c, 0), c);
  strokePath(p, -w * 0.5f + 0.5f, w * 0.5f - 0.5f, c, c);
  strokePath(p, w * 0.5f - 0.5f, w * 0.5f + 0.5f, c, withAlpha(c, 0));
}

void Canvas::ellipseSoft(Vec2 center, float rx, float ry, Color c) {
  useTexture(nullptr);
  const float px = std::max(rx, ry) * stack_.back().s;
  const int seg = std::clamp(static_cast<int>(px * 0.5f), 16, 96);
  const int base = static_cast<int>(verts_.size());
  emit(map(center), c);
  // Three rings give a smooth falloff: 1 at the centre, ~0.55 at 55%, ~0.15 at 82%, 0 at the edge.
  const float radii[3] = {0.55f, 0.82f, 1.0f};
  const float alphas[3] = {0.62f, 0.18f, 0.0f};
  for (int r = 0; r < 3; ++r)
    for (int i = 0; i < seg; ++i) {
      const float a = 2 * kPi * static_cast<float>(i) / static_cast<float>(seg);
      emit(map(Vec2{center.x + std::cos(a) * rx * radii[r], center.y + std::sin(a) * ry * radii[r]}),
           withAlpha(c, alphas[r]));
    }
  for (int i = 0; i < seg; ++i) {
    const int j = (i + 1) % seg;
    indices_.insert(indices_.end(), {base, base + 1 + i, base + 1 + j});
    for (int r = 0; r < 2; ++r) {
      const int a = base + 1 + r * seg, b = base + 1 + (r + 1) * seg;
      indices_.insert(indices_.end(), {a + i, b + i, b + j, a + i, b + j, a + j});
    }
  }
}

void Canvas::line(Vec2 a, Vec2 b, float width, Color c) {
  const Vec2 d = Norm(Sub(b, a));
  const Vec2 n{-d.y * width * 0.5f, d.x * width * 0.5f};
  const Vec2 e = Mul(d, width * 0.35f);  // a little past the ends, for rounded-looking joins
  const Vec2 p[4] = {Add(Sub(a, e), n), Sub(Sub(a, e), n), Sub(Add(b, e), n), Add(Add(b, e), n)};
  fillConvex(p, 4, c);
}

void Canvas::chevron(Vec2 c, float size, int dir, float width, Color col) {
  const float h = size * 0.5f;
  if (dir == 0) {
    line(Vec2{c.x - h, c.y - h * 0.5f}, Vec2{c.x, c.y + h * 0.5f}, width, col);
    line(Vec2{c.x, c.y + h * 0.5f}, Vec2{c.x + h, c.y - h * 0.5f}, width, col);
    return;
  }
  const float d = static_cast<float>(dir) * h * 0.5f;
  line(Vec2{c.x - d, c.y - h}, Vec2{c.x + d, c.y}, width, col);
  line(Vec2{c.x + d, c.y}, Vec2{c.x - d, c.y + h}, width, col);
}

void Canvas::check(Vec2 c, float size, float width, Color col) {
  const float h = size * 0.5f;
  line(Vec2{c.x - h, c.y}, Vec2{c.x - h * 0.3f, c.y + h * 0.65f}, width, col);
  line(Vec2{c.x - h * 0.3f, c.y + h * 0.65f}, Vec2{c.x + h, c.y - h * 0.6f}, width, col);
}

void Canvas::image(SDL_Texture* tex, Rect dst, Color tint) {
  if (!tex) return;
  useTexture(tex);
  const int base = static_cast<int>(verts_.size());
  emit(map(Vec2{dst.x, dst.y}), tint, 0, 0);
  emit(map(Vec2{dst.x + dst.w, dst.y}), tint, 1, 0);
  emit(map(Vec2{dst.x + dst.w, dst.y + dst.h}), tint, 1, 1);
  emit(map(Vec2{dst.x, dst.y + dst.h}), tint, 0, 1);
  indices_.insert(indices_.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// ---- text -----------------------------------------------------------------------------------------

const Canvas::Glyph* Canvas::glyph(const FontSlot& slot, uint32_t cp) const {
  auto it = slot.glyphs.find(cp);
  if (it != slot.glyphs.end()) return &it->second;
  if (cp >= 'a' && cp <= 'z') {  // the wordmark slot only has capitals
    it = slot.glyphs.find(cp - 32);
    if (it != slot.glyphs.end()) return &it->second;
  }
  it = slot.glyphs.find('?');
  return it != slot.glyphs.end() ? &it->second : nullptr;
}

float Canvas::textWidth(FontId f, const std::string& s) const {
  const FontSlot& slot = fonts_[f];
  const auto* info = static_cast<const stbtt_fontinfo*>(fontInfo_[slot.file]);
  float w = 0;
  uint32_t prev = 0;
  for (size_t i = 0; i < s.size();) {
    const uint32_t cp = next_codepoint(s, i);
    const Glyph* g = glyph(slot, cp);
    if (!g) continue;
    if (prev) w += static_cast<float>(stbtt_GetCodepointKernAdvance(info, static_cast<int>(prev), static_cast<int>(cp))) * slot.pxScale;
    w += g->advance;
    prev = cp;
  }
  return w / scale_;
}

float Canvas::ascent(FontId f) const { return fonts_[f].ascent / scale_; }
float Canvas::capHeight(FontId f) const { return fonts_[f].cap / scale_; }
float Canvas::lineHeight(FontId f) const { return (fonts_[f].ascent + fonts_[f].descent + fonts_[f].lineGap) / scale_; }

void Canvas::drawText(FontId f, float x, float y, const std::string& s, Color top, Color bottom, Align align) {
  if (s.empty()) return;
  const FontSlot& slot = fonts_[f];
  const auto* info = static_cast<const stbtt_fontinfo*>(fontInfo_[slot.file]);
  if (align != Align::Left) x -= textWidth(f, s) * (align == Align::Center ? 0.5f : 1.0f);
  useTexture(atlas_);
  const Xform& xf = stack_.back();
  const float rel = xf.s / scale_;  // glyphs are baked at the base scale
  Vec2 pen = map(Vec2{x, y});
  pen.y = std::round(pen.y);
  const float capTop = pen.y - slot.cap * rel, capH = std::max(1.0f, slot.cap * rel);
  uint32_t prev = 0;
  for (size_t i = 0; i < s.size();) {
    const uint32_t cp = next_codepoint(s, i);
    const Glyph* g = glyph(slot, cp);
    if (!g) continue;
    if (prev) pen.x += static_cast<float>(stbtt_GetCodepointKernAdvance(info, static_cast<int>(prev), static_cast<int>(cp))) * slot.pxScale * rel;
    prev = cp;
    if (g->x1 > g->x0) {
      const float x0 = pen.x + g->x0 * rel, x1 = pen.x + g->x1 * rel, y0 = pen.y + g->y0 * rel, y1 = pen.y + g->y1 * rel;
      const Color c0 = mix(top, bottom, std::clamp((y0 - capTop) / capH, 0.0f, 1.0f));
      const Color c1 = mix(top, bottom, std::clamp((y1 - capTop) / capH, 0.0f, 1.0f));
      const int base = static_cast<int>(verts_.size());
      emit(Vec2{x0, y0}, c0, g->u0, g->v0);
      emit(Vec2{x1, y0}, c0, g->u1, g->v0);
      emit(Vec2{x1, y1}, c1, g->u1, g->v1);
      emit(Vec2{x0, y1}, c1, g->u0, g->v1);
      indices_.insert(indices_.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    pen.x += g->advance * rel;
  }
}

void Canvas::text(FontId f, float x, float y, const std::string& s, Color c, Align align) { drawText(f, x, y, s, c, c, align); }

void Canvas::textGradient(FontId f, float x, float y, const std::string& s, Color top, Color bottom, Align align) {
  drawText(f, x, y, s, top, bottom, align);
}

void Canvas::textShadow(FontId f, float x, float y, const std::string& s, Color c, Color shadow, Align align, float dy) {
  drawText(f, x, y + dy, s, shadow, shadow, align);
  drawText(f, x, y, s, c, c, align);
}

std::string Canvas::ellipsize(FontId f, const std::string& s, float width) const {
  if (textWidth(f, s) <= width) return s;
  const std::string dots = "\xE2\x80\xA6";
  std::string cut = s;
  while (!cut.empty()) {
    // Drop one UTF-8 character from the end.
    size_t end = cut.size() - 1;
    while (end > 0 && (static_cast<unsigned char>(cut[end]) & 0xC0) == 0x80) --end;
    cut.erase(end);
    if (textWidth(f, cut + dots) <= width) break;
  }
  return cut + dots;
}

float Canvas::textWrapped(FontId f, float x, float y, float width, const std::string& s, Color c, float lineGap,
                          int maxLines, Align align) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flushWord = [&]() {
    if (word.empty()) return;
    const std::string trial = line.empty() ? word : line + " " + word;
    if (!line.empty() && textWidth(f, trial) > width) {
      lines.push_back(line);
      line = word;
    } else {
      line = trial;
    }
    word.clear();
  };
  for (char ch : s) {
    if (ch == ' ') {
      flushWord();
    } else if (ch == '\n') {
      flushWord();
      lines.push_back(line);
      line.clear();
    } else {
      word += ch;
    }
  }
  flushWord();
  if (!line.empty()) lines.push_back(line);
  if (maxLines > 0 && static_cast<int>(lines.size()) > maxLines) {
    lines.resize(static_cast<size_t>(maxLines));
    lines.back() = ellipsize(f, lines.back() + " \xE2\x80\xA6", width);
  }
  const float lh = lineHeight(f) * lineGap;
  float yy = y;
  const float ax = align == Align::Center ? x + width * 0.5f : align == Align::Right ? x + width : x;
  for (const std::string& l : lines) {
    text(f, ax, yy + ascent(f), l, c, align);
    yy += lh;
  }
  return yy - y;
}

}  // namespace launcher
