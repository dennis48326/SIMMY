// PC widescreen: where the Apt (Flash) UI goes in a frame wider than 4:3 (platform.hpp, "Widescreen").
//
// hud=centered: the 640-wide stage is the frame's 4:3 centre (display_map_viewport puts every 2D
// draw there), as a Flash player shows a 4:3 movie at "showAll" in a wider window. Only shapes made
// to cover the screen are let out: untextured ones across the whole stage (fades) are stretched to
// the frame's width, ones drawn wider than the stage (the pause menu's dimmer) are not clipped, and
// textured ones over the whole stage have their edges extended over the side bars.
// hud=edges: the stage is still drawn unscaled, but the elements that lie in its left or right half
// are moved out to the frame's edges, as a movie laid out for Stage.align would place them. Apt has no
// stage resize events to let the movies lay themselves out (and this game's movies do not), so the
// anchor is chosen here from the instance tree while it is drawn, top down:
//   - an instance whose visible bounds lie in the stage's left half goes to the left edge, one in
//     the right half to the right edge, and everything inside it goes with it;
//   - a compact instance across the middle (a dialog, a menu) stays centred, with everything in it;
//   - an untextured shape across the whole stage (a dimmer, a fade, a bar) is stretched to the full
//     frame width; textured art over the whole stage (a backdrop, the credits' dimmer) keeps its
//     scale and its outer texel columns are drawn over the side bars ("extend", in the Apt aux
//     layer's draws: aptauxpceorgl.cpp); a shape drawn wider than the stage on both sides (the pause menu's dimmer is
//     1280 wide) is drawn at the stage's scale without the clip to the 4:3 centre ("bleed");
//   - anything else as wide as the stage (the root, full-screen containers) is a container: if all
//     of its children that have a side have the same one, the group goes there (a bar whose text
//     field box is wider than the text); otherwise its children are looked at one by one.
// Only the stage-to-frame mapping changes (the viewport of what is drawn); the movies, their
// coordinates and the interpreter are untouched, so hit tests and script layout stay as on the GC.
//
// Co-op split=windows (pc_splitscreen.cpp, "Two windows"): the two-player HUD has each player's part
// in that player's half of the stage. Each instance is first given a window, top down: one whose
// visible bounds lie in the stage's left half goes to player 1's window, the right half to player
// 2's; a dialog, a dimmer or a fade across the middle (the pause menu) goes to both windows: it is
// drawn twice, once into each (simmy_apt_hud_enter returns 2), laid out over the whole stage; a
// container as wide as the stage decides nothing and its children are looked at. Inside a window the
// anchor is then chosen as above, over the player's half of the stage (or the whole stage for the
// dialogs): hud=centered keeps the half's layout in the middle of the window, hud=edges moves what
// sits in the half's left or right part to the window's edges.
//
// Co-op private screens (platform.hpp; food creation and what it opens): a screen at the movie's root
// that one player has up on their own goes to that player's window only (whole stage, fitted), and
// in a one-window split frame (gc, 4x3, wide) to that player's half of the frame; whatever else lies
// in that player's window / half (the rest of their HUD) is not drawn meanwhile.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern ENgcRenderer _ngcRenderer;

namespace {
enum Anchor : unsigned char { kUndecided, kCenter, kLeft, kRight, kStretch, kBleed, kExtend };
// One per instance being drawn: its anchor and, with split=windows, its window (-1: not decided)
// and whether it is laid out over the whole stage.
struct Entry {
  Anchor a = kUndecided;
  signed char window = -1;
  bool whole = false;
  signed char side = -1;   // a one-window split frame with a private screen up: the half it lies in
  AptCIH* inst = nullptr;  // split=windows: the instance and its stage matrix
  AptMatrix m;
};
std::vector<Entry> g_stack;
Anchor g_current = kUndecided;
Entry g_currentEntry;
bool g_windowsActive = false;  // the last Use was in split=windows mapping
bool g_halfActive = false;     // the last Use was in a one-window split frame's private screen mapping
float g_stageWidth = 640.0f;
float g_stageHeight = 480.0f;

AptMatrix Concat(const AptMatrix& p, const AptMatrix& c) {
  AptMatrix m;
  m.a = p.a * c.a + p.c * c.b;
  m.b = p.b * c.a + p.d * c.b;
  m.c = p.a * c.c + p.c * c.d;
  m.d = p.b * c.c + p.d * c.d;
  m.tx = p.a * c.tx + p.c * c.ty + p.tx;
  m.ty = p.b * c.tx + p.d * c.ty + p.ty;
  return m;
}

void Expand(AptRect& r, bool& empty, const AptMatrix& m, const AptRect& local) {
  const float xs[2] = {local.left, local.right}, ys[2] = {local.top, local.bottom};
  for (float x : xs)
    for (float y : ys) {
      const float px = m.a * x + m.c * y + m.tx, py = m.b * x + m.d * y + m.ty;
      if (empty) {
        r.left = r.right = px, r.top = r.bottom = py, empty = false;
      } else {
        if (px < r.left) r.left = px;
        if (px > r.right) r.right = px;
        if (py < r.top) r.top = py;
        if (py > r.bottom) r.bottom = py;
      }
    }
}

// The stage-space bounds of what an instance draws (visible content only; masks left out).
void VisibleBounds(AptCIH* inst, const AptMatrix& m, AptRect& r, bool& empty) {
  if (!inst->m_visible || inst->m_removed || !inst->m_character) return;
  AptCharacter* ch = inst->m_character;
  switch (ch->type) {
  case AptCharacter::kShape: Expand(r, empty, m, static_cast<AptCharacterShape*>(ch)->bounds); break;
  case AptCharacter::kEditText: Expand(r, empty, m, inst->m_textBounds); break;
  case AptCharacter::kStaticText: Expand(r, empty, m, static_cast<AptCharacterStaticText*>(ch)->bounds); break;
  case AptCharacter::kSprite:
  case AptCharacter::kMovie:
  case AptCharacter::kButton:
    for (AptCIH* child : inst->m_children.m_list)
      if (child->m_clipDepth < 0) VisibleBounds(child, Concat(m, child->m_matrix), r, empty);
    break;
  default: break;
  }
}

// split=windows: the stage-space bounds of what is drawn with text fields measured by their text (a
// field's box is often much wider than its text: player 1's clock field reaches into player 2's half).
// The text's width is estimated from its length and size; its place from the field's alignment.
void TightBounds(AptCIH* inst, const AptMatrix& m, AptRect& r, bool& empty) {
  if (!inst->m_visible || inst->m_removed || !inst->m_character) return;
  AptCharacter* ch = inst->m_character;
  if (ch->type == AptCharacter::kEditText) {
    size_t longest = 0, line = 0;
    for (unsigned char c : inst->m_text) {
      if (c == '\n' || c == '\r') line = 0;
      else if ((c & 0xC0) != 0x80) longest = std::max(longest, ++line);  // UTF-8 code points
    }
    if (!longest) return;
    AptRect box = inst->m_textBounds;
    const float size = inst->m_textSize > 0.0f ? inst->m_textSize : 12.0f;
    const float width = std::min(box.right - box.left, static_cast<float>(longest) * size * 0.6f);
    if (inst->m_textAlign == 1) box.left = box.right - width;
    else if (inst->m_textAlign == 2) box.left += 0.5f * (box.right - box.left - width), box.right = box.left + width;
    else box.right = box.left + width;
    Expand(r, empty, m, box);
    return;
  }
  if (ch->type == AptCharacter::kSprite || ch->type == AptCharacter::kMovie || ch->type == AptCharacter::kButton) {
    for (AptCIH* child : inst->m_children.m_list)
      if (child->m_clipDepth < 0) TightBounds(child, Concat(m, child->m_matrix), r, empty);
    return;
  }
  VisibleBounds(inst, m, r, empty);
}

// split=windows: is something drawn across the middle of the stage (a panel, a dimmer, a line of
// text), rather than only on its two sides?
bool CrossesMiddle(AptCIH* inst, const AptMatrix& m, float middle, float slack) {
  if (!inst->m_visible || inst->m_removed || !inst->m_character) return false;
  const int type = inst->m_character->type;
  if (type == AptCharacter::kSprite || type == AptCharacter::kMovie || type == AptCharacter::kButton) {
    for (AptCIH* child : inst->m_children.m_list)
      if (child->m_clipDepth < 0 && CrossesMiddle(child, Concat(m, child->m_matrix), middle, slack)) return true;
    return false;
  }
  AptRect r;
  bool empty = true;
  TightBounds(inst, m, r, empty);
  return !empty && r.left < middle - slack && r.right > middle + slack;
}

// split=windows: the window an instance goes to: 0 / 1 for one in the stage's left / right half (that
// player's part of the two-player HUD), 2 for both (something across the middle: a dialog, the pause
// menu, a dimmer; the GameCube showed it to both players), -1 when its children decide (a container
// over the whole stage, or one with parts on both sides and nothing across the middle, such as the
// HUD's clock-and-money bar whose clock is in player 1's half and money in player 2's).
int WindowOf(AptCIH* inst, const AptMatrix& m) {
  AptRect r;
  bool empty = true;
  TightBounds(inst, m, r, empty);
  if (empty) return -1;
  const float w = g_stageWidth, half = 0.5f * w, slack = 0.01f * w;
  if (r.right <= half + slack) return 0;
  if (r.left >= half - slack) return 1;
  const int type = inst->m_character->type;
  const bool container = type == AptCharacter::kSprite || type == AptCharacter::kMovie || type == AptCharacter::kButton;
  if (container && r.right - r.left >= 0.95f * w) return -1;
  if (CrossesMiddle(inst, m, half, slack)) return 2;
  return container ? -1 : 2;
}

// A shape drawn with colours only (no bitmap).
bool SolidShape(AptCIH* inst) {
  if (!inst->m_character || inst->m_character->type != AptCharacter::kShape) return false;
  auto* unit = static_cast<AptAuxShape*>(static_cast<AptCharacterShape*>(inst->m_character)->renderingUnit);
  if (!unit) return false;
  for (const AptAuxUnit* u : unit->units)
    if (u->type == AptAuxUnit::kTriTiled || u->type == AptAuxUnit::kTriTiledClipped) return false;
  return true;
}

bool IsShape(AptCIH* inst) { return inst->m_character && inst->m_character->type == AptCharacter::kShape; }

// A clip the game draws itself with a UI2D shader (custom control "_type" CCR2D: a texture over the
// clip's first shape).
bool TexturedCustomControl(AptCIH* inst) {
  if (!inst->m_character || (inst->m_character->type != AptCharacter::kSprite && inst->m_character->type != AptCharacter::kMovie))
    return false;
  AptProperty* type = inst->Find("_type");
  return type && !type->value.IsUndefined() && type->value.ToString() == "CCR2D";
}

// `x0` and `width`: the part of the stage looked at (split=windows: a player's half); width < 0 is
// the whole stage.
Anchor Classify(AptCIH* inst, const AptMatrix& m, bool edges, int depth = 0, float x0 = 0.0f, float width = -1.0f) {
  const bool custom = TexturedCustomControl(inst);
  if (!edges && !IsShape(inst) && !custom) return kUndecided;  // centered: only full-stage fills are special
  AptRect r;
  bool empty = true;
  VisibleBounds(inst, m, r, empty);
  if (empty) return kUndecided;
  r.left -= x0, r.right -= x0;
  const float w = width < 0.0f ? g_stageWidth : width, half = 0.5f * w, slack = 0.01f * w;
  if (IsShape(inst) && r.left < -slack && r.right > w + slack) return kBleed;  // drawn wider than the stage
  if (r.left <= slack && r.right >= w - slack && SolidShape(inst)) return kStretch;
  // Textured art over the whole stage (a full-screen backdrop or dimmer): drawn unscaled in the
  // centre, its outer texel columns extended over the side bars (simmy_apt_hud_extend).
  if (r.left <= slack && r.right >= w - slack && r.top <= 0.01f * g_stageHeight &&
      r.bottom >= 0.9f * g_stageHeight && (custom || IsShape(inst)))
    return kExtend;
  if (custom && !edges) return kUndecided;
  if (!edges) return kUndecided;
  if (r.right <= half + slack) return kLeft;
  if (r.left >= half - slack) return kRight;
  if (r.right - r.left < 0.9f * w) return kCenter;
  // As wide as the stage, but perhaps only because of an oversized box (a left-aligned text field
  // reaching across the stage): when every child that has a side has the same one and none is
  // centred, the whole group goes to that side.
  if (depth >= 3 || !inst->m_character) return kUndecided;
  const int type = inst->m_character->type;
  if (type != AptCharacter::kSprite && type != AptCharacter::kMovie && type != AptCharacter::kButton) return kUndecided;
  Anchor side = kUndecided;
  for (AptCIH* child : inst->m_children.m_list) {
    if (child->m_clipDepth >= 0 || !child->m_visible || child->m_removed) continue;
    const Anchor c = Classify(child, Concat(m, child->m_matrix), true, depth + 1, x0, width);
    if (c == kCenter || c == kStretch || c == kBleed) return kUndecided;
    if (c == kLeft || c == kRight) {
      if (side != kUndecided && side != c) return kUndecided;
      side = c;
    }
  }
  return side;
}

void Use(const Entry& e) {
  const Anchor a = e.a;
  if (simmy::split_half_frame()) {
    // A one-window split frame with a co-op private screen up: only that screen is mapped (into its
    // player's half, laid out over the whole stage); the rest is drawn as the GC drew it.
    if (g_halfActive && e.window == g_currentEntry.window) return;
    g_halfActive = true;
    g_current = kUndecided;
    g_currentEntry = e;
    simmy::display_set_hud_window(e.window, true, 0);
    if (ENgcRenderStateCache* c = _ngcRenderer.GetRenderStateCache()) c->m_dirty |= 0x80000600;
    return;
  }
  if (g_halfActive) {
    g_halfActive = false;
    simmy::display_set_hud_window(-1, false, 0);
    g_currentEntry = Entry{};
  }
  if (simmy::split_windows_frame()) {
    // split=windows: the window and the anchor in it.
    if (g_windowsActive && a == g_current && e.window == g_currentEntry.window && e.whole == g_currentEntry.whole) return;
    g_windowsActive = true;
    g_current = a;
    g_currentEntry = e;
    const int anchor = a == kStretch ? 1 : a == kBleed || a == kExtend ? 2 : a == kLeft ? 3 : a == kRight ? 4 : 0;
    simmy::display_set_hud_window(e.window, e.whole, anchor);
    if (ENgcRenderStateCache* c = _ngcRenderer.GetRenderStateCache()) c->m_dirty |= 0x80000600;
    return;
  }
  if (g_windowsActive) {
    g_windowsActive = false;
    simmy::display_set_hud_window(-1, false, 0);
    g_current = kUndecided;
    g_currentEntry = Entry{};
  }
  if (a == g_current) return;
  g_current = a;
  const float bar = simmy::display_side_bar();
  simmy::display_set_hud_anchor(a == kLeft ? -bar : a == kRight ? bar : 0.0f, a == kStretch ? 1 : a == kBleed || a == kExtend ? 2 : 0);
  // Apply only sends GX state when something is dirty: make it send the viewport and scissor again.
  if (ENgcRenderStateCache* c = _ngcRenderer.GetRenderStateCache()) c->m_dirty |= 0x80000600;
}
}  // namespace

int simmy_apt_hud_enter(AptCIH* inst, const AptMatrix& m) {
  if (g_stack.empty()) {
    int w = 0, h = 0;
    AptGetAnimationSize(&w, &h);
    g_stageWidth = w > 0 ? static_cast<float>(w) : 640.0f;
    g_stageHeight = h > 0 ? static_cast<float>(h) : 480.0f;
  }
  Entry e = g_stack.empty() ? Entry{} : g_stack.back();
  int passes = 1;
  if (simmy::split_windows_frame()) {
    // split=windows: the window first (whose half of the stage, or both for a dialog), then the anchor
    // in it.
    const char* how = nullptr;
    // A co-op private screen (a screen at the movie's root: food creation, its recipe book, the
    // dialog box while it is theirs): its player's window only, laid out over the whole stage.
    const int owner = g_stack.size() == 1 && e.window < 0 ? simmy::coop_private_screen_owner(inst->m_name.c_str()) : -1;
    if (owner >= 0) {
      e.window = static_cast<signed char>(owner), e.whole = true, e.a = kUndecided, how = "private";
    } else if (e.window < 0) {
      const int window = WindowOf(inst, m);
      if (window == 0 || window == 1) {
        e.window = static_cast<signed char>(window), e.whole = false, e.a = kUndecided, how = "half";
        // The rest of the HUD of a player with a private screen up is not drawn in their window (the
        // screen covers it on the GC; the shared clock-and-money bar stays up for the other player).
        if (window == simmy::coop_private_player()) passes = 0, how = "hidden under a private screen";
      } else if (window == 2) {
        e.window = 0, e.whole = true, e.a = kUndecided, passes = 2, how = "both windows";
      }
    }
    if (e.window >= 0 && e.a == kUndecided) {
      const bool edges = simmy::display_hud_edges();
      const float x0 = 0.5f * g_stageWidth * static_cast<float>(e.window), w = 0.5f * g_stageWidth;
      e.a = e.whole ? Classify(inst, m, edges) : Classify(inst, m, edges, 0, x0, w);
      // hud=edges, a part of a compact group whose parts go to both windows (the clock-and-money bar:
      // player 1 gets the clock, player 2 the money): placed with its siblings in the same window, so
      // the money box keeps its icon. (Not for the HUD's containers as wide as most of the stage.)
      const Entry& parent = g_stack.empty() ? Entry{} : g_stack.back();
      AptRect pr;
      bool pempty = true;
      if (edges && how && !e.whole && parent.window < 0 && parent.inst && parent.inst == inst->m_parent)
        TightBounds(parent.inst, parent.m, pr, pempty);
      if (!pempty && pr.right - pr.left < 0.75f * g_stageWidth && (e.a == kLeft || e.a == kRight || e.a == kCenter)) {
        AptRect g;
        bool none = true;
        for (AptCIH* sibling : parent.inst->m_children.m_list) {
          if (sibling->m_clipDepth >= 0) continue;
          const AptMatrix sm = Concat(parent.m, sibling->m_matrix);
          if (WindowOf(sibling, sm) == e.window) TightBounds(sibling, sm, g, none);
        }
        if (!none) {
          const float l = g.left - x0, r = g.right - x0, slack = 0.01f * g_stageWidth;
          e.a = r <= 0.5f * w + slack ? kLeft : l >= 0.5f * w - slack ? kRight : kCenter;
        }
      }
    }
    e.inst = inst, e.m = m;
    static const bool traceAny = std::getenv("SIMMY_HUD_TRACE") != nullptr;
    if (traceAny && simmy::dev_trace_frame("SIMMY_HUD_TRACE")) {
      AptRect r;
      bool empty = true;
      VisibleBounds(inst, m, r, empty);
      static const char* names[] = {"undecided", "center", "left", "right", "stretch", "bleed", "extend"};
      std::fprintf(stderr, "[hud] %*s%s %.0f..%.0f x %.0f..%.0f -> window %d%s%s, %s\n", static_cast<int>(g_stack.size() * 2), "",
                   inst->m_name.c_str(), r.left, r.right, r.top, r.bottom, e.window, e.whole ? " (whole stage)" : "",
                   how ? (std::string(" [") + how + "]").c_str() : "", names[e.a]);
    }
    g_stack.push_back(e);
    Use(e);
    return passes;
  }
  if (simmy::split_half_frame()) {
    // One window, both halves: a co-op private screen goes into its player's half; the rest of that
    // player's HUD (what lies in their half of the stage) is not drawn.
    int passes = 1;
    if (g_stack.size() == 1 && e.window < 0) {
      const int owner = simmy::coop_private_screen_owner(inst->m_name.c_str());
      if (owner >= 0) e.window = static_cast<signed char>(owner), e.whole = true;
    }
    if (e.window < 0 && e.side < 0) {
      const int side = WindowOf(inst, m);
      if (side == 0 || side == 1) e.side = static_cast<signed char>(side);
      if (side == 2) e.side = 2;
    }
    if (e.window < 0 && e.side == simmy::coop_private_player()) passes = 0;
    e.a = kUndecided;
    e.inst = inst, e.m = m;
    g_stack.push_back(e);
    Use(e);
    return passes;
  }
  Anchor a = e.a;
  if (a == kUndecided && simmy::g_displayMap2D) {
    a = Classify(inst, m, simmy::display_hud_edges());
    // With SIMMY_HUD_TRACE set: each instance name extended over the side bars, once.
    static const bool traceExtend = std::getenv("SIMMY_HUD_TRACE") != nullptr;
    static std::vector<std::string> extended;
    if (traceExtend && a == kExtend && std::find(extended.begin(), extended.end(), inst->m_name) == extended.end()) {
      extended.push_back(inst->m_name);
      std::fprintf(stderr, "[hud] frame %d: extend %s\n", _framecount, inst->m_name.c_str());
    }
    // SIMMY_HUD_TRACE=<frame>: log each decision made in that game frame.
    static const bool traceAny = std::getenv("SIMMY_HUD_TRACE") != nullptr;
    if (traceAny && simmy::dev_trace_frame("SIMMY_HUD_TRACE")) {
      AptRect r;
      bool empty = true;
      VisibleBounds(inst, m, r, empty);
      static const char* names[] = {"undecided", "center", "left", "right", "stretch", "bleed", "extend"};
      std::string units;
      if (inst->m_character && inst->m_character->type == AptCharacter::kShape) {
        auto* u = static_cast<AptAuxShape*>(static_cast<AptCharacterShape*>(inst->m_character)->renderingUnit);
        units = u ? " units" : " no units";
        if (u)
          for (const AptAuxUnit* x : u->units) units += " " + std::to_string(x->type);
      }
      std::fprintf(stderr, "[hud] %*s%s (type %d%s) %.0f..%.0f x %.0f..%.0f -> %s\n", static_cast<int>(g_stack.size() * 2), "",
                   inst->m_name.c_str(), inst->m_character ? inst->m_character->type : -1, units.c_str(), r.left, r.right,
                   r.top, r.bottom, names[a]);
    }
  }
  e.a = a;
  g_stack.push_back(e);
  Use(e);
  return 1;
}

// split=windows, a dialog over the whole stage: its second drawing goes into player 2's window.
void simmy_apt_hud_next_pass() {
  if (g_stack.empty()) return;
  g_stack.back().window = 1;
  Use(g_stack.back());
}

void simmy_apt_hud_leave() {
  if (!g_stack.empty()) g_stack.pop_back();
  Use(g_stack.empty() ? Entry{} : g_stack.back());
}

// The stage width to add on each side of the full-stage textured art being drawn (0 for anything
// else): the side bar's width (display_side_bar, 640x480 units at the centre's scale) in stage units.
float simmy_apt_hud_extend() {
  if (g_current != kExtend) return 0.0f;
  if (g_windowsActive) return simmy::display_window_extend(g_stageWidth);
  const float bar = simmy::display_side_bar();
  if (bar <= 0.0f) return 0.0f;
  const float centre = 640.0f - 2.0f * bar;  // the 4:3 centre's width in 640x480 units
  return centre > 0.0f ? bar * g_stageWidth / centre : 0.0f;
}
