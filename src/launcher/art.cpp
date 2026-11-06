// SimmyLauncher: procedural artwork (see art.hpp).
#include "art.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace launcher {

namespace {
const float kPi = 3.14159265358979f;

struct P3 {
  float x, y, z;
};
P3 Sub3(P3 a, P3 b) { return P3{a.x - b.x, a.y - b.y, a.z - b.z}; }
P3 Cross(P3 a, P3 b) { return P3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float Dot(P3 a, P3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
P3 Normalize(P3 a) {
  const float l = std::sqrt(Dot(a, a));
  return l > 1e-6f ? P3{a.x / l, a.y / l, a.z / l} : P3{0, 0, 0};
}

// 2D convex hull (monotone chain) for the plumbob's silhouette.
std::vector<Vec2> Hull(std::vector<Vec2> p) {
  std::sort(p.begin(), p.end(), [](Vec2 a, Vec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
  auto cross = [](Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
  std::vector<Vec2> h(p.size() * 2);
  size_t k = 0;
  for (size_t i = 0; i < p.size(); ++i) {
    while (k >= 2 && cross(h[k - 2], h[k - 1], p[i]) <= 0) --k;
    h[k++] = p[i];
  }
  for (size_t i = p.size() - 1, t = k + 1; i > 0; --i) {
    while (k >= t && cross(h[k - 2], h[k - 1], p[i - 1]) <= 0) --k;
    h[k++] = p[i - 1];
  }
  h.resize(k > 1 ? k - 1 : k);
  return h;
}

void Cloud(Canvas& c, float x, float y, float s, float alpha) {
  c.ellipseSoft(Vec2{x + 10 * s, y + 4 * s}, 175 * s, 72 * s, rgb(0xFFFFFF, 0.28f * alpha));
  struct Puff {
    float dx, dy, r;
  };
  static const Puff kPuffs[] = {{-92, 16, 26}, {-58, 4, 36}, {-20, -14, 46}, {26, -24, 52}, {70, -2, 40}, {104, 14, 26}};
  const Color top = rgb(0xFFFFFF, alpha), bottom = rgb(0xD3E9FB, alpha);
  c.roundRect(Rect{x - 112 * s, y + 4 * s, 240 * s, 38 * s}, 19 * s, mix(top, bottom, 0.55f), bottom);
  for (const Puff& p : kPuffs) {
    const float r = p.r * s;
    const float cy = y + p.dy * s;
    // The gradient runs over the cloud's height, so the puffs blend into one body.
    const float t0 = std::clamp((cy - r - (y - 76 * s)) / (118 * s), 0.0f, 1.0f);
    const float t1 = std::clamp((cy + r - (y - 76 * s)) / (118 * s), 0.0f, 1.0f);
    c.circleGradient(Vec2{x + p.dx * s, cy}, r, mix(top, bottom, t0), mix(top, bottom, t1));
  }
}
}  // namespace

void draw_sky(Canvas& c, float t, float w, float h) {
  c.rect(Rect{0, 0, w, h * 0.58f}, rgb(0x1A63C4), rgb(0x56A9EE));
  c.rect(Rect{0, h * 0.58f - 0.5f, w, h * 0.42f + 1}, rgb(0x56A9EE), rgb(0xB9E2FF));
  // The sun's glow, top right.
  c.softCircle(Vec2{w * 0.86f, h * 0.06f}, 330, rgb(0xFFFFFF, 0.30f));
  c.softCircle(Vec2{w * 0.86f, h * 0.06f}, 120, rgb(0xFFF7DA, 0.45f));

  struct CloudSpec {
    float x, y, s, speed, alpha;
  };
  static const CloudSpec kClouds[] = {
      {80, 120, 0.42f, 5.0f, 0.55f},  {520, 70, 0.36f, 4.0f, 0.5f},   {900, 160, 0.40f, 4.5f, 0.5f},
      {300, 300, 0.62f, 8.0f, 0.75f}, {760, 360, 0.70f, 9.0f, 0.8f},  {1180, 250, 0.55f, 7.0f, 0.7f},
      {-40, 470, 0.95f, 13.0f, 0.95f}, {640, 540, 1.05f, 15.0f, 0.95f},
  };
  const float span = w + 520;
  for (const CloudSpec& k : kClouds) {
    float x = std::fmod(k.x + k.speed * t, span);
    if (x < 0) x += span;
    Cloud(c, x - 240, k.y, k.s, k.alpha);
  }

  // Rolling hills along the bottom (the neighbourhood is just over there).
  c.circleGradient(Vec2{w * 0.80f, h + 760}, 900, rgb(0x93D7F2), rgb(0x7CC6EA));
  c.circleGradient(Vec2{w * 0.15f, h + 640}, 720, rgb(0x8EDB62), rgb(0x4E9E3A));
  c.circleGradient(Vec2{w * 0.78f, h + 780}, 860, rgb(0x76C94E), rgb(0x3F8E2E));
}

void draw_plumbob(Canvas& c, float cx, float cy, float h, float angle, float glow, bool shadow) {
  const int kSides = 6;
  const float radius = h * 0.29f, top = -h * 0.40f, bottom = h * 0.60f;
  const float tilt = 0.30f, ct = std::cos(tilt), st = std::sin(tilt);
  if (shadow) c.ellipseSoft(Vec2{cx, cy + h * 0.98f}, h * 0.34f, h * 0.075f, rgb(0x0B2B5A, 0.30f));
  if (glow > 0) {
    c.softCircle(Vec2{cx, cy + h * 0.08f}, h * 0.95f, rgb(0x9DFF62, 0.30f * glow));
    c.softCircle(Vec2{cx, cy + h * 0.05f}, h * 0.52f, rgb(0xE6FFC8, 0.30f * glow));
  }
  auto view = [&](P3 p) { return P3{p.x, p.y * ct - p.z * st, p.y * st + p.z * ct}; };
  const P3 apexTop = view(P3{0, top, 0}), apexBottom = view(P3{0, bottom, 0});
  P3 ring[kSides];
  for (int i = 0; i < kSides; ++i) {
    const float a = angle + 2 * kPi * static_cast<float>(i) / kSides;
    ring[i] = view(P3{radius * std::cos(a), 0, radius * std::sin(a)});
  }
  auto screen = [&](P3 p) { return Vec2{cx + p.x, cy + p.y}; };

  // The silhouette first (so the facets' anti-aliased seams show green, not sky).
  std::vector<Vec2> pts = {screen(apexTop), screen(apexBottom)};
  for (const P3& p : ring) pts.push_back(screen(p));
  const std::vector<Vec2> hull = Hull(pts);
  c.fillConvex(hull.data(), static_cast<int>(hull.size()), rgb(0x1E8A22));

  const P3 light = Normalize(P3{-0.5f, -0.75f, -0.55f});
  const P3 half = Normalize(P3{light.x, light.y, light.z - 1.0f});
  const Color dark = rgb(0x0A5A16), mid = rgb(0x39C42A), bright = rgb(0xC9FF8F);
  auto shade = [&](float b) {
    b = std::clamp(b, 0.0f, 1.0f);
    return b < 0.6f ? mix(dark, mid, b / 0.6f) : mix(mid, bright, (b - 0.6f) / 0.4f);
  };
  struct Edge {
    Vec2 a, b;
  };
  std::vector<Edge> edges;
  for (int half2 = 0; half2 < 2; ++half2) {
    const P3 apex = half2 == 0 ? apexTop : apexBottom;
    for (int i = 0; i < kSides; ++i) {
      const P3 a = ring[i], b = ring[(i + 1) % kSides];
      P3 n = Normalize(Cross(Sub3(a, apex), Sub3(b, apex)));
      const P3 centroid{(apex.x + a.x + b.x) / 3, (apex.y + a.y + b.y) / 3, (apex.z + a.z + b.z) / 3};
      if (Dot(n, centroid) < 0) n = P3{-n.x, -n.y, -n.z};
      if (n.z >= -0.02f) continue;  // facing away
      const float diffuse = std::max(0.0f, Dot(n, light));
      const float spec = std::pow(std::max(0.0f, Dot(n, half)), 18.0f);
      const float b0 = 0.30f + 0.62f * diffuse + (half2 == 0 ? 0.08f : -0.06f);
      Color face = shade(b0);
      face = mix(face, rgb(0xFFFFFF), spec * 0.55f);
      const Color apexCol = half2 == 0 ? mix(face, rgb(0xEFFFD8), 0.28f) : mix(face, dark, 0.35f);
      const Vec2 tri[3] = {screen(apex), screen(a), screen(b)};
      const Color cols[3] = {apexCol, face, mix(face, dark, 0.08f)};
      c.fillConvex(tri, cols, 3);
      edges.push_back(Edge{screen(a), screen(b)});
      edges.push_back(Edge{screen(apex), screen(a)});
    }
  }
  // Facet edges: a thin light line catches the light.
  const float lw = std::max(0.8f, h * 0.008f);
  for (const Edge& e : edges) c.line(e.a, e.b, lw, rgb(0xEFFFE0, 0.22f));
  // A sparkle that twinkles as it turns.
  const float tw = 0.5f + 0.5f * std::sin(angle * 3.0f);
  const Vec2 sp{cx - h * 0.10f, cy - h * 0.12f};
  c.softCircle(sp, h * 0.16f, rgb(0xFFFFFF, 0.45f * tw));
  const float r1 = h * 0.13f * (0.6f + 0.4f * tw), r2 = h * 0.018f;
  const Vec2 v1[4] = {{sp.x, sp.y - r1}, {sp.x + r2, sp.y}, {sp.x, sp.y + r1}, {sp.x - r2, sp.y}};
  const Vec2 v2[4] = {{sp.x - r1, sp.y}, {sp.x, sp.y - r2}, {sp.x + r1, sp.y}, {sp.x, sp.y + r2}};
  c.fillConvex(v1, 4, rgb(0xFFFFFF, 0.85f * tw));
  c.fillConvex(v2, 4, rgb(0xFFFFFF, 0.85f * tw));
}

void draw_wordmark(Canvas& c, float x, float y, float alpha) {
  const float saved = c.alpha;
  c.alpha *= alpha;
  const std::string word = "SIMMY";
  c.text(kFontWord, x, y + 9, word, rgb(0x08244F, 0.35f), Align::Center);  // drop shadow
  const Color outline = rgb(0x0F3D86);
  for (int i = 0; i < 16; ++i) {
    const float a = 2 * kPi * static_cast<float>(i) / 16.0f;
    c.text(kFontWord, x + std::cos(a) * 5.5f, y + std::sin(a) * 5.5f, word, outline, Align::Center);
  }
  c.textGradient(kFontWord, x, y, word, rgb(0xFFFFFF), rgb(0xA9DBFF), Align::Center);
  c.alpha = saved;
}

void draw_disc(Canvas& c, float cx, float cy, float r, float angle, bool present, Color hole) {
  if (!present) {
    // An empty slot: a dashed circle and a plus.
    const int dashes = 28;
    for (int i = 0; i < dashes; ++i) {
      const float a0 = angle * 0.3f + 2 * kPi * static_cast<float>(i) / dashes, a1 = a0 + 2 * kPi / dashes * 0.55f;
      c.line(Vec2{cx + std::cos(a0) * r, cy + std::sin(a0) * r}, Vec2{cx + std::cos(a1) * r, cy + std::sin(a1) * r}, 3.0f,
             rgb(0xFFFFFF, 0.55f));
    }
    c.line(Vec2{cx - r * 0.22f, cy}, Vec2{cx + r * 0.22f, cy}, 5.0f, rgb(0xFFFFFF, 0.8f));
    c.line(Vec2{cx, cy - r * 0.22f}, Vec2{cx, cy + r * 0.22f}, 5.0f, rgb(0xFFFFFF, 0.8f));
    return;
  }
  c.ellipseSoft(Vec2{cx + r * 0.06f, cy + r * 0.12f}, r * 1.12f, r * 1.12f, rgb(0x061F48, 0.35f));
  c.circleGradient(Vec2{cx, cy}, r, rgb(0xF6FAFF), rgb(0xA7B9CF));
  // Rainbow sheen wedges turning with the disc.
  const Color tints[3] = {rgb(0xFF9AD5, 0.55f), rgb(0x8FF0FF, 0.6f), rgb(0xFFF08A, 0.55f)};
  for (int w = 0; w < 3; ++w)
    for (int mirror = 0; mirror < 2; ++mirror) {
      const float base = angle + static_cast<float>(w) * 0.55f + static_cast<float>(mirror) * kPi;
      const int seg = 8;
      Vec2 pts[seg + 2];
      Color cols[seg + 2];
      pts[0] = Vec2{cx, cy};
      cols[0] = withAlpha(tints[w], 0.0f);
      for (int i = 0; i <= seg; ++i) {
        const float a = base + 0.42f * static_cast<float>(i) / seg;
        pts[i + 1] = Vec2{cx + std::cos(a) * r * 0.97f, cy + std::sin(a) * r * 0.97f};
        cols[i + 1] = tints[w];
      }
      c.fillConvex(pts, cols, seg + 2);
    }
  c.ring(Vec2{cx, cy}, r * 0.97f, 1.5f, rgb(0xFFFFFF, 0.7f));
  c.circleGradient(Vec2{cx, cy}, r * 0.40f, rgb(0xEAF1F8), rgb(0xC3D1E0));
  c.circle(Vec2{cx, cy}, r * 0.29f, rgb(0xFFFFFF, 0.55f));
  c.circle(Vec2{cx, cy}, r * 0.15f, hole);
  c.ring(Vec2{cx, cy}, r * 0.40f, 1.0f, rgb(0x8296AE, 0.6f));
}

void draw_controller(Canvas& c, float cx, float cy, float w, bool connected, Color accent) {
  const float h = w * 0.62f;
  const Color top = connected ? rgb(0xFFFFFF) : rgb(0xFFFFFF, 0.16f);
  const Color bottom = connected ? rgb(0xC7D5E8) : rgb(0xFFFFFF, 0.10f);
  if (connected) c.ellipseSoft(Vec2{cx, cy + h * 0.55f}, w * 0.55f, h * 0.16f, rgb(0x061F48, 0.35f));
  // Grips, then the body over them.
  c.circleGradient(Vec2{cx - w * 0.30f, cy + h * 0.10f}, h * 0.36f, mix(top, bottom, 0.3f), bottom);
  c.circleGradient(Vec2{cx + w * 0.30f, cy + h * 0.10f}, h * 0.36f, mix(top, bottom, 0.3f), bottom);
  c.roundRect(Rect{cx - w * 0.42f, cy - h * 0.36f, w * 0.84f, h * 0.56f}, h * 0.26f, top, mix(top, bottom, 0.7f));
  if (!connected) {
    c.roundRectStroke(Rect{cx - w * 0.42f, cy - h * 0.36f, w * 0.84f, h * 0.56f}, h * 0.26f, 1.5f, rgb(0xFFFFFF, 0.35f));
    return;
  }
  // Stick, d-pad, the big green button and friends, a yellow little stick.
  c.circle(Vec2{cx - w * 0.24f, cy - h * 0.08f}, h * 0.13f, rgb(0x55657C));
  c.circle(Vec2{cx - w * 0.24f, cy - h * 0.10f}, h * 0.08f, rgb(0x8796AB));
  const float dx = cx - w * 0.10f, dy = cy + h * 0.13f, dl = h * 0.11f, dw = h * 0.045f;
  c.roundRect(Rect{dx - dl, dy - dw, dl * 2, dw * 2}, dw * 0.6f, rgb(0x55657C));
  c.roundRect(Rect{dx - dw, dy - dl, dw * 2, dl * 2}, dw * 0.6f, rgb(0x55657C));
  c.circleGradient(Vec2{cx + w * 0.22f, cy - h * 0.06f}, h * 0.12f, rgb(0x6CE36A), rgb(0x23A133));
  c.circleGradient(Vec2{cx + w * 0.09f, cy + h * 0.07f}, h * 0.065f, rgb(0xFF7C7C), rgb(0xD13A3A));
  c.circle(Vec2{cx + w * 0.33f, cy - h * 0.15f}, h * 0.05f, rgb(0x9AA8BA));
  c.circle(Vec2{cx + w * 0.30f, cy + h * 0.06f}, h * 0.05f, rgb(0x9AA8BA));
  c.circleGradient(Vec2{cx + w * 0.10f, cy + h * 0.22f}, h * 0.075f, rgb(0xFFE36B), rgb(0xE0A91C));
  // The player's colour as a light strip.
  c.roundRect(Rect{cx - w * 0.07f, cy - h * 0.28f, w * 0.14f, h * 0.06f}, h * 0.03f, accent);
}

namespace {
void Monitor(Canvas& c, Rect screen, int halves, bool halvesAre4x3, const char* const* labels, float highlight) {
  const Color bezel = mix(rgb(0x173463), rgb(0x2B5FA8), highlight);
  c.roundRectGlow(Rect{screen.x - 4, screen.y - 4, screen.w + 8, screen.h + 8}, 6, 8, rgb(0x061F48, 0.25f));
  c.roundRect(Rect{screen.x - 4, screen.y - 4, screen.w + 8, screen.h + 8}, 6, bezel);
  // Stand.
  c.roundRect(Rect{screen.cx() - 6, screen.y + screen.h + 4, 12, 7}, 2, bezel);
  c.roundRect(Rect{screen.cx() - 18, screen.y + screen.h + 10, 36, 4}, 2, bezel);
  c.rect(screen, rgb(0x0B1E3D), rgb(0x0B1E3D));
  const float gap = 2.0f;
  for (int i = 0; i < halves; ++i) {
    Rect v = screen;
    if (halves == 2) {
      v.w = (screen.w - gap) * 0.5f;
      v.x = screen.x + static_cast<float>(i) * (v.w + gap);
    }
    if (halvesAre4x3) {  // letterboxed 4:3 pictures
      const float hh = v.w * 0.75f;
      v.y = screen.y + (screen.h - hh) * 0.5f;
      v.h = hh;
    }
    c.rect(Rect{v.x, v.y, v.w, v.h * 0.68f}, rgb(0x5FB3F2), rgb(0xBFE6FF));
    c.rect(Rect{v.x, v.y + v.h * 0.68f, v.w, v.h * 0.32f}, rgb(0x7DCB55), rgb(0x4F9E39));
    // A tiny plumbob over a tiny Sim.
    const float px = v.cx(), py = v.y + v.h * 0.46f, ph = std::min(v.h * 0.26f, 14.0f);
    const Vec2 diamond[4] = {{px, py - ph * 0.5f}, {px + ph * 0.28f, py}, {px, py + ph * 0.6f}, {px - ph * 0.28f, py}};
    c.fillConvex(diamond, 4, rgb(0x46D633));
    if (labels && labels[i]) {
      const Vec2 b{v.x + 9, v.y + 9};
      c.circle(b, 7, i == 0 ? rgb(0x2F86E8) : rgb(0xF08A24));
      c.textMid(kFontLabel, b.x, b.y, labels[i], rgb(0xFFFFFF), Align::Center);
    }
  }
}
}  // namespace

void draw_layout_picture(Canvas& c, Rect area, int split, float highlight) {
  static const char* const kOne[] = {"1", "2"};
  const float cy = area.cy() - 5;
  switch (split) {
  case 3: {  // two windows (two monitors)
    const float w = std::min(area.w * 0.42f, (area.h - 20) * 16.0f / 9.0f), h = w * 9.0f / 16.0f;
    const char* one[] = {"1"};
    const char* two[] = {"2"};
    Monitor(c, Rect{area.cx() - w - 7, cy - h * 0.5f, w, h}, 1, false, one, highlight);
    Monitor(c, Rect{area.cx() + 7, cy - h * 0.5f, w, h}, 1, false, two, highlight);
    break;
  }
  case 2: {  // the window's aspect cut in two
    const float w = std::min(area.w * 0.7f, (area.h - 20) * 16.0f / 9.0f), h = w * 9.0f / 16.0f;
    Monitor(c, Rect{area.cx() - w * 0.5f, cy - h * 0.5f, w, h}, 2, false, kOne, highlight);
    break;
  }
  case 1: {  // two 4:3 halves side by side
    const float w = std::min(area.w * 0.82f, (area.h - 20) * 16.0f / 9.0f), h = w * 9.0f / 16.0f;
    Monitor(c, Rect{area.cx() - w * 0.5f, cy - h * 0.5f, w, h}, 2, true, kOne, highlight);
    break;
  }
  default: {  // the GameCube's: one 4:3 picture cut in two
    const float h = area.h - 24, w = h * 4.0f / 3.0f;
    Monitor(c, Rect{area.cx() - w * 0.5f, cy - h * 0.5f, w, h}, 2, false, kOne, highlight);
    break;
  }
  }
}

void draw_icon(Canvas& c, Icon icon, Vec2 p, float s, Color col) {
  const float h = s * 0.5f;
  switch (icon) {
  case Icon::Play: {
    const Vec2 tri[3] = {{p.x - h * 0.55f, p.y - h * 0.8f}, {p.x + h * 0.85f, p.y}, {p.x - h * 0.55f, p.y + h * 0.8f}};
    c.fillConvex(tri, 3, col);
    break;
  }
  case Icon::People: {
    c.circle(Vec2{p.x - h * 0.42f, p.y - h * 0.42f}, h * 0.30f, col);
    c.roundRect(Rect{p.x - h * 0.92f, p.y - h * 0.02f, h * 1.0f, h * 0.85f}, h * 0.42f, col);
    c.circle(Vec2{p.x + h * 0.45f, p.y - h * 0.30f}, h * 0.27f, withAlpha(col, 0.85f));
    c.roundRect(Rect{p.x + 0.0f, p.y + h * 0.08f, h * 0.92f, h * 0.77f}, h * 0.38f, withAlpha(col, 0.85f));
    break;
  }
  case Icon::Gear: {
    for (int i = 0; i < 8; ++i) {
      const float a = kPi * 2 * static_cast<float>(i) / 8;
      c.line(Vec2{p.x + std::cos(a) * h * 0.5f, p.y + std::sin(a) * h * 0.5f},
             Vec2{p.x + std::cos(a) * h * 0.88f, p.y + std::sin(a) * h * 0.88f}, h * 0.32f, col);
    }
    c.ring(p, h * 0.48f, h * 0.30f, col);
    break;
  }
  case Icon::Disc: {
    c.ring(p, h * 0.72f, h * 0.22f, col);
    c.circle(p, h * 0.18f, col);
    break;
  }
  case Icon::Back: {
    c.chevron(Vec2{p.x - h * 0.15f, p.y}, h * 1.3f, -1, h * 0.26f, col);
    break;
  }
  case Icon::Power: {
    const int seg = 14;
    for (int i = 0; i < seg; ++i) {
      const float a0 = -kPi * 0.5f + 0.7f + (2 * kPi - 1.4f) * static_cast<float>(i) / seg;
      const float a1 = -kPi * 0.5f + 0.7f + (2 * kPi - 1.4f) * static_cast<float>(i + 1) / seg;
      c.line(Vec2{p.x + std::cos(a0) * h * 0.7f, p.y + std::sin(a0) * h * 0.7f},
             Vec2{p.x + std::cos(a1) * h * 0.7f, p.y + std::sin(a1) * h * 0.7f}, h * 0.2f, col);
    }
    c.line(Vec2{p.x, p.y - h * 0.85f}, Vec2{p.x, p.y - h * 0.1f}, h * 0.22f, col);
    break;
  }
  case Icon::Folder: {
    c.roundRect(Rect{p.x - h * 0.85f, p.y - h * 0.62f, h * 0.8f, h * 0.4f}, h * 0.12f, col);
    c.roundRect(Rect{p.x - h * 0.85f, p.y - h * 0.4f, h * 1.7f, h * 1.1f}, h * 0.16f, col);
    break;
  }
  case Icon::Stop: {
    c.roundRect(Rect{p.x - h * 0.6f, p.y - h * 0.6f, h * 1.2f, h * 1.2f}, h * 0.2f, col);
    break;
  }
  case Icon::None: break;
  }
}

}  // namespace launcher
