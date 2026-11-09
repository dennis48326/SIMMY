// PC: the in-game options overlay (F1). See pc_overlay.hpp.
//
// Drawing: aurora owns the Dear ImGui context when there is a window (lib/aurora.cpp begin_frame runs
// ImGui::NewFrame, end_frame freezes the draw data, and the present pass draws it over the scaled
// picture). overlay_frame runs between the two (platform_end_frame, before aurora_end_frame) and only
// adds windows while the overlay is open. AuroraConfig::imGuiInitCallback (overlay_imgui_init) loads
// the font and the theme when aurora creates the context.
//
// Input: keyboard and mouse reach ImGui through aurora's input router (its ImGui layer); gamepads
// through ImGui's SDL3 backend (all gamepads, ImGuiConfigFlags_NavEnableGamepad). The game's pads are
// read as usual and neutralised before the game sees them (overlay_filter_pads); their B/Start/L/R
// presses are also used here (B: back, Start: close, L/R: page), so the game's own button layout
// works whatever the controller.
#include "pc_overlay.hpp"
#include "pc_net.hpp"
#include "pc_kbm.hpp"
#include "pc_window2.hpp"
#include "pc_settings.hpp"
#include "simmy/platform.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_stdinc.h>
#include <aurora/aurora.h>
#include <aurora/imgui.h>
#include <dolphin/gx/GXAurora.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_overlay_font.inc"

namespace simmy {
namespace {

// ---- Look ----------------------------------------------------------------------------------------
// The game's UI colours: navy text on light blue panels, blue borders, green for the selected item.
ImVec4 Rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
const ImVec4 kNavy = Rgb(0x0a, 0x0a, 0x6e);
const ImVec4 kPanel = Rgb(0xdc, 0xe7, 0xff);
const ImVec4 kPanelDeep = Rgb(0xc4, 0xd4, 0xfa);
const ImVec4 kBorder = Rgb(0x4a, 0x57, 0xc0);
const ImVec4 kGreen = Rgb(0x3f, 0xb0, 0x55);
const ImVec4 kGreenDark = Rgb(0x2f, 0x8f, 0x44);
const ImVec4 kNote = Rgb(0x4a, 0x57, 0xc0);    // secondary text
const ImVec4 kWarn = Rgb(0xb0, 0x3a, 0x10);    // "restart to apply"
constexpr float kBaseFont = 16.0f;             // the size the style's metrics are written for

// Font sizes in the atlas (pixels). The overlay picks the one nearest above height / 30, so text is
// rasterised near its final size from 640x480 (16 px) to 4K (72 px).
const float kFontSizes[] = {13.0f, 16.0f, 20.0f, 24.0f, 30.0f, 36.0f, 48.0f, 60.0f, 72.0f};
constexpr int kFontCount = static_cast<int>(sizeof kFontSizes / sizeof kFontSizes[0]);
const ImWchar kGlyphRanges[] = {0x0020, 0x007E, 0x00A0, 0x00FF, 0x2013, 0x2014, 0x2018, 0x201D,
                                0x2022, 0x2022, 0x2026, 0x2026, 0};

enum Page { PageGraphics, PageControls, PageQuit, PageCount };
const char* const kPageNames[PageCount] = {"Graphics", "Controls", "Quit"};
const char* const kConfirmName = "Quit the game?##simmy_confirm";

struct DrawList {
  std::vector<ImDrawVert> vtx;
  std::vector<ImDrawIdx> idx;
  std::vector<ImDrawCmd> cmd;
};

struct TestStep {
  unsigned frame;
  std::string action;
};

struct Overlay {
  bool open = false;
  bool offscreen = false;
  bool justOpened = false;
  int page = PageGraphics;
  int pageRequest = -1;      // select this tab
  bool focusFirst = false;   // move the nav cursor to the page's first item
  bool confirmQuit = false;  // the quit confirmation is up
  bool confirmFocus = false;
  int backCooldown = 0;      // frames: one back/close per press (pad and ImGui may both see it)
  int tabCooldown = 0;
  std::string status;
  int statusFrames = 0;
  unsigned startMsaa = 1;
  int startBackend = 0;
  int backendInUse = 0;
  bool cursorWasVisible = true;
  bool gamepadModeSet = false;
  bool dropFocus = false;    // after closing: take ImGui's keyboard focus away from the hidden window
  float minHeight = 0.0f;    // the window keeps its tallest height, so pages do not jump around
  ImVec2 minHeightFor;       // ... for this display size
  // Pads: edges of the game's buttons, noticed in overlay_filter_pads, used by the next frame.
  PADStatus pads[4] = {};
  u16 prevButtons[4] = {};
  bool latch[4] = {};  // after closing: the pad stays blocked until released
  bool padBack = false, padClose = false, padPrevTab = false, padNextTab = false;
  bool keyBack = false;  // Escape
  // Fonts and style.
  ImFont* fonts[kFontCount] = {};
  ImGuiStyle baseStyle;
  float styleScale = 0.0f;
  // Test path (SIMMY_OVERLAY_TEST, offscreen).
  bool test = false;
  bool ownContext = false;
  std::vector<TestStep> steps;
  u16 injectButtons = 0;
  int injectFrames = 0;
  unsigned char* atlas = nullptr;
  int atlasW = 0, atlasH = 0;
  std::vector<DrawList> lastDraw, shotDraw;
  ImVec2 lastSize, shotSize;
};
Overlay& O() {
  static Overlay o;
  return o;
}

void ApplyTheme() {
  ImGuiStyle& s = ImGui::GetStyle();
  s = ImGuiStyle();
  s.WindowRounding = 12.0f;
  s.ChildRounding = 9.0f;
  s.FrameRounding = 8.0f;
  s.PopupRounding = 11.0f;
  s.GrabRounding = 7.0f;
  s.TabRounding = 8.0f;
  s.ScrollbarRounding = 7.0f;
  s.WindowBorderSize = 3.0f;
  s.ChildBorderSize = 0.0f;
  s.PopupBorderSize = 3.0f;
  s.FrameBorderSize = 1.5f;
  s.TabBorderSize = 1.5f;
  s.TabBarBorderSize = 2.0f;
  s.TabBarOverlineSize = 3.0f;
  s.WindowPadding = ImVec2(14.0f, 10.0f);
  s.FramePadding = ImVec2(9.0f, 3.0f);
  s.ItemSpacing = ImVec2(9.0f, 5.0f);
  s.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
  s.CellPadding = ImVec2(7.0f, 1.0f);
  s.ScrollbarSize = 13.0f;
  s.SelectableTextAlign = ImVec2(0.0f, 0.5f);
  s.ButtonTextAlign = ImVec2(0.5f, 0.5f);
  s.WindowTitleAlign = ImVec2(0.5f, 0.5f);
  s.DisplaySafeAreaPadding = ImVec2(0.0f, 0.0f);

  ImVec4* c = s.Colors;
  for (int i = 0; i < ImGuiCol_COUNT; ++i) c[i] = kPanelDeep;
  c[ImGuiCol_Text] = kNavy;
  c[ImGuiCol_TextDisabled] = Rgb(0x6a, 0x72, 0xb0);
  c[ImGuiCol_WindowBg] = kPanel;
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_PopupBg] = kPanel;
  c[ImGuiCol_Border] = kBorder;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = Rgb(0xf4, 0xf7, 0xff);
  c[ImGuiCol_FrameBgHovered] = Rgb(0xb6, 0xe3, 0xc0);
  c[ImGuiCol_FrameBgActive] = kGreen;
  c[ImGuiCol_TitleBg] = kBorder;
  c[ImGuiCol_TitleBgActive] = kBorder;
  c[ImGuiCol_TitleBgCollapsed] = kBorder;
  c[ImGuiCol_MenuBarBg] = kPanelDeep;
  c[ImGuiCol_ScrollbarBg] = Rgb(0xc4, 0xd4, 0xfa, 0.6f);
  c[ImGuiCol_ScrollbarGrab] = Rgb(0x8a, 0x96, 0xde);
  c[ImGuiCol_ScrollbarGrabHovered] = kBorder;
  c[ImGuiCol_ScrollbarGrabActive] = kGreen;
  c[ImGuiCol_CheckMark] = kGreen;
  c[ImGuiCol_SliderGrab] = kBorder;
  c[ImGuiCol_SliderGrabActive] = kGreen;
  c[ImGuiCol_Button] = Rgb(0xf4, 0xf7, 0xff);
  c[ImGuiCol_ButtonHovered] = kGreen;
  c[ImGuiCol_ButtonActive] = kGreenDark;
  c[ImGuiCol_Header] = Rgb(0xb6, 0xe3, 0xc0);
  c[ImGuiCol_HeaderHovered] = kGreen;
  c[ImGuiCol_HeaderActive] = kGreenDark;
  c[ImGuiCol_Separator] = Rgb(0x8a, 0x96, 0xde);
  c[ImGuiCol_SeparatorHovered] = kBorder;
  c[ImGuiCol_SeparatorActive] = kGreen;
  c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ResizeGripHovered] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ResizeGripActive] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_Tab] = kPanelDeep;
  c[ImGuiCol_TabHovered] = kGreen;
  c[ImGuiCol_TabSelected] = Rgb(0xf4, 0xf7, 0xff);
  c[ImGuiCol_TabSelectedOverline] = kGreen;
  c[ImGuiCol_TabDimmed] = kPanelDeep;
  c[ImGuiCol_TabDimmedSelected] = Rgb(0xf4, 0xf7, 0xff);
  c[ImGuiCol_TabDimmedSelectedOverline] = kBorder;
  c[ImGuiCol_TableHeaderBg] = kPanelDeep;
  c[ImGuiCol_TableBorderStrong] = kBorder;
  c[ImGuiCol_TableBorderLight] = Rgb(0xa8, 0xb8, 0xee);
  c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_TableRowBgAlt] = Rgb(0xc4, 0xd4, 0xfa, 0.45f);
  c[ImGuiCol_TextLink] = kBorder;
  c[ImGuiCol_TextSelectedBg] = Rgb(0x3f, 0xb0, 0x55, 0.4f);
  c[ImGuiCol_DragDropTarget] = kGreen;
  c[ImGuiCol_NavCursor] = kGreenDark;
  c[ImGuiCol_NavWindowingHighlight] = kGreen;
  c[ImGuiCol_NavWindowingDimBg] = Rgb(0x0a, 0x0a, 0x3c, 0.35f);
  c[ImGuiCol_ModalWindowDimBg] = Rgb(0x0a, 0x0a, 0x3c, 0.45f);
  O().baseStyle = s;
  O().styleScale = 0.0f;
}

void LoadFonts() {
  Overlay& o = O();
  ImGuiIO& io = ImGui::GetIO();
  for (int i = 0; i < kFontCount; ++i) {
    ImFontConfig cfg;
    cfg.OversampleH = cfg.OversampleV = 1;  // one atlas size per screen size instead
    cfg.PixelSnapH = true;
    std::snprintf(cfg.Name, sizeof cfg.Name, "Roboto Medium %.0fpx", static_cast<double>(kFontSizes[i]));
    o.fonts[i] = io.Fonts->AddFontFromMemoryCompressedTTF(simmy_overlay_font_compressed_data,
                                                          static_cast<int>(simmy_overlay_font_compressed_size),
                                                          kFontSizes[i], &cfg, kGlyphRanges);
  }
  if (!o.fonts[0]) {
    std::fprintf(stderr, "[overlay] embedded font failed to load; using ImGui's default\n");
    for (ImFont*& f : o.fonts) f = nullptr;
    io.Fonts->AddFontDefault();
  }
}

// ---- Open / close ----------------------------------------------------------------------------------
void Open() {
  Overlay& o = O();
  if (o.open) return;
  o.open = true;
  o.justOpened = true;
  o.focusFirst = true;
  o.confirmQuit = false;
  o.backCooldown = 8;
  o.padBack = o.padClose = o.padPrevTab = o.padNextTab = o.keyBack = false;
  std::fprintf(stderr, "[overlay] opened\n");
  if (aurora_get_window()) {
    o.cursorWasVisible = SDL_CursorVisible();
    SDL_ShowCursor();
  }
  std::fprintf(stderr, "[overlay] open (game paused, pads blocked)\n");
}

void Close() {
  Overlay& o = O();
  if (!o.open) return;
  o.open = false;
  o.confirmQuit = false;
  for (bool& l : o.latch) l = true;  // a button still held (B, Start) must not reach the game
  o.dropFocus = true;
  if (aurora_get_window() && !o.cursorWasVisible) SDL_HideCursor();
  std::fprintf(stderr, "[overlay] closed\n");
}

void Toggle() {
  if (O().open) Close();
  else Open();
}

void SetStatus(const std::string& text) {
  O().status = text;
  O().statusFrames = 300;
}

// ---- Settings rows ---------------------------------------------------------------------------------
struct Choice {
  std::vector<std::string> labels, values;
  void add(const std::string& label, const std::string& value) {
    labels.push_back(label);
    values.push_back(value);
  }
  int find(const std::string& value) {
    for (size_t i = 0; i < values.size(); ++i)
      if (SDL_strcasecmp(values[i].c_str(), value.c_str()) == 0) return static_cast<int>(i);
    add(value, value);  // a value from simmy.ini that the list does not have
    return static_cast<int>(values.size()) - 1;
  }
};

void Pick(const char* key, const std::string& value) {
  const GraphicsSettings before = graphics_settings();
  settings_set_value(key, value);
  settings_apply_live(before);
  std::fprintf(stderr, "[overlay] %s -> %s\n", key, settings_value_string(graphics_settings(), key).c_str());
}

bool KeyPressed(ImGuiKey a, ImGuiKey b, ImGuiKey c) {
  return ImGui::IsKeyPressed(a, true) || ImGui::IsKeyPressed(b, true) || ImGui::IsKeyPressed(c, true);
}

// One setting: the whole row is one navigable item (the green highlight); A / click / Enter steps to
// the next value, Left/Right (d-pad, stick, arrow keys, the < > buttons) step either way.
// Returns the picked index, or -1.
int ChoiceRow(const char* label, const char* note, const ImVec4& noteColor, const Choice& choice, int current) {
  Overlay& o = O();
  const int count = static_cast<int>(choice.values.size());
  const float rowH = ImGui::GetFrameHeight();
  ImGui::TableNextRow(ImGuiTableRowFlags_None, rowH);
  ImGui::TableSetColumnIndex(0);
  ImGui::PushID(label);
  if (o.focusFirst) {
    ImGui::SetKeyboardFocusHere();
    o.focusFirst = false;
  }
  const ImVec2 rowPos = ImGui::GetCursorScreenPos();
  const bool activated = ImGui::Selectable("##row", false,
                                           ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                                           ImVec2(0.0f, rowH));
  const bool focused = ImGui::IsItemFocused();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (focused) {
    // The selected setting: green across the row, as the game marks its selected menu item.
    const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
    dl->AddRectFilled(r0, r1, ImGui::GetColorU32(kGreen), ImGui::GetStyle().FrameRounding);
  }
  const float textY = rowPos.y + (rowH - ImGui::GetFontSize()) * 0.5f;
  dl->AddText(ImVec2(rowPos.x + ImGui::GetStyle().FramePadding.x, textY), ImGui::GetColorU32(kNavy), label);
  if (note && *note) {
    const float lw = ImGui::CalcTextSize(label).x;
    dl->AddText(ImVec2(rowPos.x + ImGui::GetStyle().FramePadding.x + lw + ImGui::GetFontSize() * 0.5f, textY),
                ImGui::GetColorU32(noteColor), note);
  }
  int delta = activated ? 1 : 0;
  if (focused) {
    if (KeyPressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft)) delta = -1;
    if (KeyPressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight)) delta = 1;
  }
  // The value: < value >
  ImGui::TableSetColumnIndex(1);
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  const float x0 = ImGui::GetCursorPosX(), cellW = ImGui::GetContentRegionAvail().x;
  const ImVec2 cell = ImGui::GetCursorScreenPos();
  if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) delta = -1;
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::SetCursorPosX(x0 + cellW - rowH);
  if (ImGui::ArrowButton("##next", ImGuiDir_Right)) delta = 1;
  ImGui::PopItemFlag();
  const std::string& text = choice.labels[static_cast<size_t>(std::clamp(current, 0, count - 1))];
  const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
  const float mid = cell.x + cellW * 0.5f;
  if (focused) {
    const float pad = ImGui::GetFontSize() * 0.5f;
    dl->AddRectFilled(ImVec2(mid - ts.x * 0.5f - pad, cell.y + 1.0f), ImVec2(mid + ts.x * 0.5f + pad, cell.y + rowH - 1.0f),
                      ImGui::GetColorU32(Rgb(0xf4, 0xf7, 0xff)), rowH * 0.5f);
  }
  dl->PushClipRect(ImVec2(cell.x + rowH, cell.y), ImVec2(cell.x + cellW - rowH, cell.y + rowH), true);
  dl->AddText(ImVec2(mid - ts.x * 0.5f, cell.y + (rowH - ts.y) * 0.5f), ImGui::GetColorU32(kNavy), text.c_str());
  dl->PopClipRect();
  ImGui::PopID();
  if (!delta || count < 2) return -1;
  return ((current + delta) % count + count) % count;
}

void InfoRow(const char* label, const std::string& value, const char* note) {
  ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x);
  ImGui::TextUnformatted(label);
  if (note && *note) {
    ImGui::SameLine(0.0f, ImGui::GetFontSize() * 0.5f);
    ImGui::TextColored(kNote, "%s", note);
  }
  ImGui::TableSetColumnIndex(1);
  ImGui::AlignTextToFramePadding();
  const float w = ImGui::CalcTextSize(value.c_str()).x, avail = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - w) * 0.5f));
  ImGui::TextUnformatted(value.c_str());
}

std::string BackendLabel(int b) {
  switch (b) {
  case BACKEND_D3D12: return "Direct3D 12";
  case BACKEND_D3D11: return "Direct3D 11";
  case BACKEND_VULKAN: return "Vulkan";
  case BACKEND_METAL: return "Metal";
  case BACKEND_OPENGL: return "OpenGL";
  case BACKEND_OPENGLES: return "OpenGL ES";
  case BACKEND_NULL: return "Null";
  default: return "Auto";
  }
}

bool BeginRows(const char* id, float valueWidth) {
  if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
    return false;
  ImGui::TableSetupColumn("setting", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, valueWidth);
  return true;
}

void GraphicsPage() {
  Overlay& o = O();
  GraphicsSettings& g = graphics_settings();
  const float font = ImGui::GetFontSize();
  if (BeginRows("graphics", font * 11.5f)) {
    int pick;
    {
      Choice c;
      c.add("4:3 (original)", "4:3");
      c.add("16:9 (widescreen)", "16:9");
      c.add("16:10", "16:10");
      c.add("21:9", "21:9");
      if ((pick = ChoiceRow("Aspect ratio", split_mode() != SplitMode::Gc ? "(one player)" : "", kNote, c,
                            c.find(settings_value_string(g, "aspect")))) >= 0)
        Pick("aspect", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      c.add("Centered", "centered");
      c.add("Edges", "edges");
      if ((pick = ChoiceRow("HUD placement", g.wide ? "" : "(16:9 only)", kNote, c, c.find(settings_value_string(g, "hud")))) >= 0)
        Pick("hud", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      c.add("Auto (window)", "auto");
      c.add("480p", "480p");
      c.add("720p", "720p");
      c.add("1080p", "1080p");
      c.add("1440p", "1440p");
      c.add("2160p (4K)", "2160p");
      c.add("2x", "2x");
      c.add("3x", "3x");
      c.add("4x", "4x");
      if ((pick = ChoiceRow("Internal resolution", "", kNote, c, c.find(settings_value_string(g, "resolution")))) >= 0)
        Pick("resolution", c.values[static_cast<size_t>(pick)]);
    }
    {
      // The effective fullscreen state (Alt+Enter may have changed it since start).
      const bool now = settings_fullscreen_now();
      const FullscreenMode shown =
          !now ? FullscreenMode::Off : g.fullscreen == FullscreenMode::Off ? FullscreenMode::Borderless : g.fullscreen;
      Choice c;
      c.add("Off (window)", "off");
      c.add("Borderless", "borderless");
      c.add("Exclusive", "exclusive");
      const int cur = shown == FullscreenMode::Off ? 0 : shown == FullscreenMode::Borderless ? 1 : 2;
      if ((pick = ChoiceRow("Fullscreen", "(Alt+Enter)", kNote, c, cur)) >= 0) {
        g.fullscreen = shown;  // so the change is measured from what is on screen
        Pick("fullscreen", c.values[static_cast<size_t>(pick)]);
      }
    }
    {
      GraphicsSettings def = g;
      def.windowSet = false;
      unsigned dw = 0, dh = 0;
      settings_window_size(def, dw, dh);
      char label[48];
      std::snprintf(label, sizeof label, "Default (%ux%u)", dw, dh);
      Choice c;
      c.add(label, "auto");
      for (const char* s : {"640x480", "960x720", "1280x960", "1440x1080", "1280x720", "1600x900", "1920x1080",
                            "2560x1440", "3840x2160"})
        c.add(s, s);
      const bool exclusive = g.fullscreen == FullscreenMode::Exclusive && settings_fullscreen_now();
      const bool borderless = settings_fullscreen_now() && !exclusive;
      if ((pick = ChoiceRow(exclusive ? "Display mode" : "Window size", borderless ? "(when windowed)" : "", kNote, c,
                            c.find(settings_value_string(g, "window")))) >= 0)
        Pick("window", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      c.add("Off", "off");
      c.add("On", "on");
      if ((pick = ChoiceRow("VSync", "", kNote, c, c.find(settings_value_string(g, "vsync")))) >= 0)
        Pick("vsync", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      for (const char* s : {"60", "30", "20", "15"}) c.add(std::string(s) + " fps", s);
      if ((pick = ChoiceRow("Frame-rate cap", "", kNote, c, c.find(settings_value_string(g, "fps_cap")))) >= 0)
        Pick("fps_cap", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      c.add("Off", "1");
      c.add("2x", "2");
      c.add("4x", "4");
      c.add("8x", "8");
      const bool changed = g.msaa != o.startMsaa;
      if ((pick = ChoiceRow("Anti-aliasing (MSAA)", changed ? "(restart to apply)" : "(applies after restart)",
                            changed ? kWarn : kNote, c, c.find(settings_value_string(g, "msaa")))) >= 0)
        Pick("msaa", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      c.add("Off", "1");
      for (const char* s : {"2", "4", "8", "16"}) c.add(std::string(s) + "x", s);
      if ((pick = ChoiceRow("Anisotropic filtering", "", kNote, c, c.find(settings_value_string(g, "anisotropy")))) >= 0)
        Pick("anisotropy", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      c.add("Bilinear", "bilinear");
      c.add("Area (sharper)", "area");
      if ((pick = ChoiceRow("Scaling filter", "", kNote, c, c.find(settings_value_string(g, "resampler")))) >= 0)
        Pick("resampler", c.values[static_cast<size_t>(pick)]);
    }
    {
      Choice c;
      c.add("Auto", "auto");
      size_t n = 0;
      const AuroraBackend* list = aurora_get_available_backends(&n);
      for (size_t i = 0; list && i < n; ++i)
        if (list[i] != BACKEND_NULL && list[i] != BACKEND_AUTO)
          c.add(BackendLabel(list[i]), settings_backend_name(list[i]));
      const bool changed = g.backend != o.startBackend;
      if ((pick = ChoiceRow("Graphics API", changed ? "(restart to apply)" : "(applies after restart)",
                            changed ? kWarn : kNote, c, c.find(settings_value_string(g, "backend")))) >= 0)
        Pick("backend", c.values[static_cast<size_t>(pick)]);
    }
    InfoRow("Graphics API in use", BackendLabel(o.backendInUse), "");
    {
      // Two-player co-op: how the halves are shown (split=, --split); applies from the next frame.
      Choice c;
      c.add("GameCube (one 4:3 split)", "gc");
      c.add("Two 4:3 halves", "4x3");
      c.add("Widescreen split", "wide");
      c.add("Two windows", "windows");
      if ((pick = ChoiceRow("Co-op display", "", kNote, c, c.find(settings_value_string(g, "split")))) >= 0)
        Pick("split", c.values[static_cast<size_t>(pick)]);
    }
    if (split_mode() == SplitMode::Windows) {
      // The second player's window (pc_window2.cpp): F2 shows / hides it too.
      Choice c;
      c.add("Windowed", "windowed");
      c.add("Fullscreen", "fullscreen");
      c.add("Hidden", "hidden");
      const int cur = !window2_shown() ? 2 : window2_fullscreen() ? 1 : 0;
      if ((pick = ChoiceRow("Player 2 window", "(F2)", kNote, c, cur)) >= 0) {
        if (pick == 2) {
          window2_set_shown(false);
        } else {
          window2_set_shown(true);
          window2_set_fullscreen(pick == 1);
        }
        std::fprintf(stderr, "[overlay] player 2 window -> %s\n", c.values[static_cast<size_t>(pick)].c_str());
      }
    }
    ImGui::EndTable();
  }

  ImGui::Spacing();
  if (ImGui::Button("Save to simmy.ini")) {
    std::string message;
    settings_save(message);
    SetStatus(message);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset to defaults")) {
    const GraphicsSettings before = g;
    GraphicsSettings defaults;
    defaults.sourceFile = g.sourceFile;
    // The co-op display and the second window's place are a layout choice, not a quality setting: kept.
    defaults.split = g.split;
    defaults.window2Set = g.window2Set, defaults.window2Width = g.window2Width, defaults.window2Height = g.window2Height;
    defaults.window2PosSet = g.window2PosSet, defaults.window2X = g.window2X, defaults.window2Y = g.window2Y;
    defaults.window2Display = g.window2Display, defaults.fullscreen2 = g.fullscreen2;
    g = defaults;
    settings_apply_live(before);
    SetStatus("Defaults restored (not saved yet)");
    std::fprintf(stderr, "[overlay] defaults restored\n");
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) Close();
  if (o.statusFrames > 0) {
    --o.statusFrames;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kNote, "%s", o.status.c_str());
    ImGui::PopTextWrapPos();
  } else {
    ImGui::TextColored(kNote, "Changes apply at once; Save keeps them for the next start.");
  }
}

void ControlsPage() {
  Overlay& o = O();
  const float font = ImGui::GetFontSize();
  if (BeginRows("controls", font * 15.0f)) {
    auto row = [](const char* what, const char* keys) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x);
      ImGui::TextUnformatted(what);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(keys);
    };
    row("Open / close this menu", "F1");
    row("Fullscreen on / off", "Alt+Enter");
    row("Move", "Arrow keys, D-pad, stick");
    row("Choose / next value", "Enter, Space, A, click");
    row("Change a value", "Left / Right");
    row("Previous / next page", "L / R, shoulder buttons");
    row("Back / close", "Esc, B, Start");
    ImGui::EndTable();
  }
  ImGui::Spacing();
  ImGui::TextUnformatted("In the game (player 1, keyboard and mouse):");
  if (BeginRows("gamekeys", font * 15.0f)) {
    auto row = [](const char* what, const std::string& keys) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x);
      ImGui::TextUnformatted(what);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(keys.c_str());
    };
    if (graphics_settings().p1Input == 1) {
      row("Keyboard and mouse", "off (p1_input = controller)");
    } else {
      for (int a = 0; a < kbm_action_count(); ++a) row(kbm_action_label(a), kbm_binding_text(a));
      row("Classic cursor", "move the mouse over the room");
      row("A at the cursor / B", "left click / right click");
      row("Turn camera", "drag with the middle or right button");
      row("Zoom (menus: up / down)", "mouse wheel");
    }
    ImGui::EndTable();
  }
  ImGui::Spacing();
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(kNote,
                     "Controllers use the GameCube layout (A, B, X, Y, Z, Start, L, R, both sticks and the D-pad). "
                     "Keys can be changed in the [keys] section of simmy.ini; p1_input chooses keyboard and mouse, "
                     "a controller, or both for player 1. While this menu is open the game is paused and gets no input.");
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  if (o.focusFirst) {
    ImGui::SetKeyboardFocusHere();
    o.focusFirst = false;
  }
  if (ImGui::Button("Close")) Close();
}

void QuitPage() {
  Overlay& o = O();
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted("Leave The Sims 2.");
  ImGui::TextColored(kNote, "Progress since your last save in the game is not kept. The game shuts down the "
                            "normal way: memory card files are closed and the window is closed.");
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  if (o.focusFirst) {
    ImGui::SetKeyboardFocusHere();
    o.focusFirst = false;
  }
  if (ImGui::Button("Quit game")) {
    o.confirmQuit = true;
    o.confirmFocus = true;
    std::fprintf(stderr, "[overlay] quit asked, confirming\n");
  }
  ImGui::SameLine();
  if (ImGui::Button("Back to the game")) Close();
}

void ConfirmQuit() {
  Overlay& o = O();
  if (o.confirmQuit && !ImGui::IsPopupOpen(kConfirmName)) ImGui::OpenPopup(kConfirmName);
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  if (!ImGui::BeginPopupModal(kConfirmName, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoTitleBar))
    return;
  if (!o.confirmQuit) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  ImGui::TextUnformatted("Quit the game now?");
  ImGui::Spacing();
  const float w = ImGui::GetFontSize() * 6.0f;
  bool quit = ImGui::Button("Quit", ImVec2(w, 0.0f));
  ImGui::SameLine();
  if (o.confirmFocus) {
    ImGui::SetKeyboardFocusHere();  // Cancel is the default: one more press is needed to quit
    o.confirmFocus = false;
  }
  if (ImGui::Button("Cancel", ImVec2(w, 0.0f))) o.confirmQuit = false;
  ImGui::EndPopup();
  if (quit) {
    std::fprintf(stderr, "[overlay] quit confirmed: leaving through the game's exit path\n");
    o.confirmQuit = false;
    Close();
    platform_request_quit();
  }
}

// ---- The frame -------------------------------------------------------------------------------------
void Draw() {
  Overlay& o = O();
  ImGuiIO& io = ImGui::GetIO();
  const float fbScale = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
  const float target = std::clamp(io.DisplaySize.y * fbScale / 30.0f, 13.0f, 72.0f);  // pixels
  int fi = kFontCount - 1;
  for (int i = 0; i < kFontCount; ++i)
    if (kFontSizes[i] >= target - 0.5f) {
      fi = i;
      break;
    }
  ImFont* font = o.fonts[fi] ? o.fonts[fi] : io.Fonts->Fonts[0];
  const float fontSize = target / fbScale;  // in ImGui's (logical) units
  io.FontGlobalScale = fontSize / font->FontSize;
  const float scale = fontSize / kBaseFont;
  if (std::fabs(scale - o.styleScale) > 0.001f) {
    ImGui::GetStyle() = o.baseStyle;
    ImGuiStyle& st = ImGui::GetStyle();
    st.ScaleAllSizes(scale);
    // ScaleAllSizes leaves the border widths alone; keep them in proportion at 1440p/4K too.
    const float b = std::max(1.0f, std::floor(scale + 0.25f));
    st.WindowBorderSize = o.baseStyle.WindowBorderSize * b;
    st.PopupBorderSize = o.baseStyle.PopupBorderSize * b;
    st.FrameBorderSize = o.baseStyle.FrameBorderSize * b;
    st.TabBorderSize = o.baseStyle.TabBorderSize * b;
    st.TabBarBorderSize = o.baseStyle.TabBarBorderSize * b;
    st.TabBarOverlineSize = o.baseStyle.TabBarOverlineSize * b;
    o.styleScale = scale;
  }

  // Back (B, Esc, the gamepad's cancel button), close (Start), page (L/R): once per press.
  if (o.backCooldown > 0) --o.backCooldown;
  if (o.tabCooldown > 0) --o.tabCooldown;
  bool back = o.padBack || o.keyBack || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
  bool close = o.padClose || ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false);
  if (o.backCooldown > 0) back = close = false;
  if (back || close) o.backCooldown = 10;
  if (close) {
    Close();
    return;
  }
  if (back) {
    if (o.confirmQuit) {
      o.confirmQuit = false;
    } else {
      Close();
      return;
    }
  }
  bool prev = o.padPrevTab || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false);
  bool next = o.padNextTab || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false);
  if (o.tabCooldown > 0 || o.confirmQuit) prev = next = false;
  if (prev || next) {
    o.tabCooldown = 8;
    o.pageRequest = (o.page + (next ? 1 : PageCount - 1)) % PageCount;
  }

  ImGui::PushFont(font);
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(8, 10, 60, 110));
  const ImVec2 ds = io.DisplaySize;
  const float width = std::min(ds.x * 0.94f, fontSize * 34.0f);
  if (o.minHeightFor.x != ds.x || o.minHeightFor.y != ds.y) o.minHeight = 0.0f, o.minHeightFor = ds;
  ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(width, std::min(o.minHeight, ds.y * 0.98f)), ImVec2(width, ds.y * 0.98f));
  if (o.justOpened) ImGui::SetNextWindowFocus();
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;
  if (ImGui::Begin("Options##simmy_overlay", nullptr, flags)) {
    // Header band.
    const ImGuiStyle& st = ImGui::GetStyle();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const float ww = ImGui::GetWindowWidth();
    const float bandH = fontSize * 1.75f;
    dl->AddRectFilled(wp, ImVec2(wp.x + ww, wp.y + bandH), ImGui::GetColorU32(kBorder), st.WindowRounding,
                      ImDrawFlags_RoundCornersTop);
    dl->AddText(ImVec2(wp.x + st.WindowPadding.x, wp.y + (bandH - fontSize) * 0.5f), IM_COL32_WHITE, "Options");
    const char* hint = "F1 / Start: close";
    const ImVec2 hs = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(wp.x + ww - st.WindowPadding.x - hs.x, wp.y + (bandH - fontSize) * 0.5f),
                IM_COL32(0xdc, 0xe7, 0xff, 0xff), hint);
    ImGui::SetCursorScreenPos(ImVec2(wp.x + st.WindowPadding.x, wp.y + bandH + st.ItemSpacing.y));

    if (ImGui::BeginTabBar("pages", ImGuiTabBarFlags_NoTooltip | ImGuiTabBarFlags_FittingPolicyResizeDown)) {
      for (int i = 0; i < PageCount; ++i) {
        const ImGuiTabItemFlags tf = o.pageRequest == i ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem(kPageNames[i], nullptr, tf)) {
          if (i != o.page) {
            o.page = i;
            o.focusFirst = true;
          }
          if (o.pageRequest == i) o.pageRequest = -1;
          // Each page in its own child: directional navigation stays inside the page (Left/Right on a
          // setting change it instead of jumping to the tabs).
          const float maxH = std::max(fontSize * 6.0f, ds.y * 0.98f - (ImGui::GetCursorScreenPos().y - wp.y) -
                                                           st.WindowPadding.y * 2.0f);
          ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, maxH));
          if (ImGui::BeginChild(kPageNames[i], ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY)) {
            if (o.justOpened || o.focusFirst) ImGui::SetWindowFocus();
            if (i == PageGraphics) GraphicsPage();
            else if (i == PageControls) ControlsPage();
            else QuitPage();
          }
          ImGui::EndChild();
          ImGui::EndTabItem();
        }
      }
      ImGui::EndTabBar();
    }
    o.minHeight = std::max(o.minHeight, ImGui::GetWindowHeight());
  }
  ImGui::End();
  if (o.open) ConfirmQuit();
  ImGui::PopFont();
  o.justOpened = false;
}

// ---- Test path -------------------------------------------------------------------------------------
void ParseTestScript() {
  Overlay& o = O();
  const char* env = std::getenv("SIMMY_OVERLAY_TEST");
  if (!env || !*env) return;
  o.test = true;
  std::string all = env;
  size_t start = 0;
  while (start < all.size()) {
    size_t end = all.find(',', start);
    if (end == std::string::npos) end = all.size();
    const std::string item = all.substr(start, end - start);
    const size_t colon = item.find(':');
    if (colon != std::string::npos)
      o.steps.push_back({static_cast<unsigned>(std::strtoul(item.c_str(), nullptr, 10)), item.substr(colon + 1)});
    start = end + 1;
  }
  std::fprintf(stderr, "[overlay] test script: %zu steps\n", o.steps.size());
}

void RunTestSteps(unsigned frame) {
  Overlay& o = O();
  for (const TestStep& s : o.steps) {
    if (s.frame != frame) continue;
    std::fprintf(stderr, "[overlay] frame %u: test %s\n", frame, s.action.c_str());
    const std::string& a = s.action;
    u16 button = 0;
    if (a == "f1") Toggle();
    else if (a == "open") Open();
    else if (a == "close") Close();
    else if (a == "up") button = PAD_BUTTON_UP;
    else if (a == "down") button = PAD_BUTTON_DOWN;
    else if (a == "left") button = PAD_BUTTON_LEFT;
    else if (a == "right") button = PAD_BUTTON_RIGHT;
    else if (a == "a") button = PAD_BUTTON_A;
    else if (a == "b") button = PAD_BUTTON_B;
    else if (a == "start") button = PAD_BUTTON_START;
    else if (a == "l") button = PAD_TRIGGER_L;
    else if (a == "r") button = PAD_TRIGGER_R;
    else if (a.rfind("set:", 0) == 0) {
      const size_t eq = a.find('=');
      if (eq != std::string::npos) Pick(a.substr(4, eq - 4).c_str(), a.substr(eq + 1));
    } else if (a == "save") {
      std::string message;
      settings_save(message);
      SetStatus(message);
    } else if (a == "sdlquit") {  // exit-path tests: what closing window 1 / Alt+F4 / a logoff sends
      SDL_Event e{};
      e.type = SDL_EVENT_QUIT;
      SDL_PushEvent(&e);
    } else if (a == "close2") {  // exit-path tests: player 2's window closed (it is only hidden)
      std::fprintf(stderr, "[window2] closed by the player (test)\n");
      window2_set_shown(false);
    } else std::fprintf(stderr, "[overlay] unknown test action \"%s\"\n", a.c_str());
    if (button) o.injectButtons = button, o.injectFrames = 3;
  }
}

void CreateTestContext() {
  Overlay& o = O();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  overlay_imgui_init(nullptr);
  io.Fonts->GetTexDataAsRGBA32(&o.atlas, &o.atlasW, &o.atlasH);
  io.Fonts->SetTexID(static_cast<ImTextureID>(1));
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  o.ownContext = true;
  std::fprintf(stderr, "[overlay] test context (font atlas %dx%d)\n", o.atlasW, o.atlasH);
}

// The test context has no SDL backend: the pads (as the game would have read them) are its gamepad.
void FeedPads(ImGuiIO& io) {
  Overlay& o = O();
  u16 b = 0;
  int sx = 0, sy = 0;
  for (const PADStatus& p : o.pads) {
    if (p.err != PAD_ERR_NONE) continue;
    b |= p.button;
    if (std::abs(p.stickX) > std::abs(sx)) sx = p.stickX;
    if (std::abs(p.stickY) > std::abs(sy)) sy = p.stickY;
  }
  io.AddKeyEvent(ImGuiKey_GamepadFaceDown, (b & PAD_BUTTON_A) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceRight, (b & PAD_BUTTON_B) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceUp, (b & PAD_BUTTON_Y) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadStart, (b & PAD_BUTTON_START) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadUp, (b & PAD_BUTTON_UP) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadDown, (b & PAD_BUTTON_DOWN) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, (b & PAD_BUTTON_LEFT) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadRight, (b & PAD_BUTTON_RIGHT) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL1, (b & PAD_TRIGGER_L) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR1, (b & PAD_TRIGGER_R) != 0);
  const float fx = sx / 127.0f, fy = sy / 127.0f;
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, fx < -0.3f, std::max(0.0f, -fx));
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, fx > 0.3f, std::max(0.0f, fx));
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, fy > 0.3f, std::max(0.0f, fy));
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, fy < -0.3f, std::max(0.0f, -fy));
}

void KeepDrawData() {
  Overlay& o = O();
  o.lastDraw.clear();
  const ImDrawData* dd = ImGui::GetDrawData();
  o.lastSize = ImGui::GetIO().DisplaySize;
  if (!dd) return;
  for (int i = 0; i < dd->CmdListsCount; ++i) {
    const ImDrawList* src = dd->CmdLists[i];
    DrawList l;
    l.vtx.assign(src->VtxBuffer.Data, src->VtxBuffer.Data + src->VtxBuffer.Size);
    l.idx.assign(src->IdxBuffer.Data, src->IdxBuffer.Data + src->IdxBuffer.Size);
    l.cmd.assign(src->CmdBuffer.Data, src->CmdBuffer.Data + src->CmdBuffer.Size);
    o.lastDraw.push_back(std::move(l));
  }
}

// Rasterises ImGui's triangles (font atlas texture, vertex colours, clip rectangles) over an RGBA image.
void Rasterise(const std::vector<DrawList>& lists, ImVec2 size, unsigned char* rgba, unsigned W, unsigned H) {
  const Overlay& o = O();
  if (!o.atlas || size.x <= 0.0f || size.y <= 0.0f) return;
  const float kx = static_cast<float>(W) / size.x, ky = static_cast<float>(H) / size.y;
  auto sample = [&o](float u, float v, float out[4]) {
    const float x = u * static_cast<float>(o.atlasW) - 0.5f, y = v * static_cast<float>(o.atlasH) - 0.5f;
    const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    for (int c = 0; c < 4; ++c) out[c] = 0.0f;
    for (int j = 0; j < 2; ++j)
      for (int i = 0; i < 2; ++i) {
        const int tx = std::clamp(x0 + i, 0, o.atlasW - 1), ty = std::clamp(y0 + j, 0, o.atlasH - 1);
        const float w = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
        const unsigned char* t = o.atlas + (static_cast<size_t>(ty) * static_cast<size_t>(o.atlasW) + static_cast<size_t>(tx)) * 4;
        for (int c = 0; c < 4; ++c) out[c] += w * static_cast<float>(t[c]) / 255.0f;
      }
  };
  for (const DrawList& l : lists) {
    for (const ImDrawCmd& cmd : l.cmd) {
      if (cmd.UserCallback) continue;
      const int cx0 = std::max(0, static_cast<int>(std::floor(cmd.ClipRect.x * kx)));
      const int cy0 = std::max(0, static_cast<int>(std::floor(cmd.ClipRect.y * ky)));
      const int cx1 = std::min(static_cast<int>(W), static_cast<int>(std::ceil(cmd.ClipRect.z * kx)));
      const int cy1 = std::min(static_cast<int>(H), static_cast<int>(std::ceil(cmd.ClipRect.w * ky)));
      if (cx0 >= cx1 || cy0 >= cy1) continue;
      for (unsigned t = 0; t + 2 < cmd.ElemCount; t += 3) {
        const ImDrawVert* v[3];
        for (int k = 0; k < 3; ++k) v[k] = &l.vtx[cmd.VtxOffset + l.idx[cmd.IdxOffset + t + static_cast<unsigned>(k)]];
        float px[3], py[3];
        for (int k = 0; k < 3; ++k) px[k] = v[k]->pos.x * kx, py[k] = v[k]->pos.y * ky;
        const float area = (px[1] - px[0]) * (py[2] - py[0]) - (py[1] - py[0]) * (px[2] - px[0]);
        if (std::fabs(area) < 1e-6f) continue;
        const int bx0 = std::max(cx0, static_cast<int>(std::floor(std::min({px[0], px[1], px[2]}))));
        const int by0 = std::max(cy0, static_cast<int>(std::floor(std::min({py[0], py[1], py[2]}))));
        const int bx1 = std::min(cx1, static_cast<int>(std::ceil(std::max({px[0], px[1], px[2]}))));
        const int by1 = std::min(cy1, static_cast<int>(std::ceil(std::max({py[0], py[1], py[2]}))));
        float col[3][4];
        for (int k = 0; k < 3; ++k)
          for (int c = 0; c < 4; ++c) col[k][c] = static_cast<float>((v[k]->col >> (8 * c)) & 0xFF) / 255.0f;
        for (int y = by0; y < by1; ++y)
          for (int x = bx0; x < bx1; ++x) {
            const float qx = static_cast<float>(x) + 0.5f, qy = static_cast<float>(y) + 0.5f;
            const float w0 = ((px[1] - qx) * (py[2] - qy) - (py[1] - qy) * (px[2] - qx)) / area;
            const float w1 = ((px[2] - qx) * (py[0] - qy) - (py[2] - qy) * (px[0] - qx)) / area;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            float tex[4];
            sample(w0 * v[0]->uv.x + w1 * v[1]->uv.x + w2 * v[2]->uv.x, w0 * v[0]->uv.y + w1 * v[1]->uv.y + w2 * v[2]->uv.y,
                   tex);
            float src[4];
            for (int c = 0; c < 4; ++c) src[c] = (w0 * col[0][c] + w1 * col[1][c] + w2 * col[2][c]) * tex[c];
            unsigned char* d = rgba + (static_cast<size_t>(y) * W + static_cast<size_t>(x)) * 4;
            for (int c = 0; c < 3; ++c)
              d[c] = static_cast<unsigned char>(
                  std::lround(std::clamp(src[c] * src[3] * 255.0f + static_cast<float>(d[c]) * (1.0f - src[3]), 0.0f, 255.0f)));
          }
      }
    }
  }
}

void Neutral(PADStatus& p) {
  p.button = 0;
  p.stickX = p.stickY = p.substickX = p.substickY = 0;
  p.triggerLeft = p.triggerRight = 0;
  p.analogA = p.analogB = 0;
#ifdef TARGET_PC
  p.extButton = 0;
#endif
}
}  // namespace

// ---- Entry points ----------------------------------------------------------------------------------

void overlay_imgui_init(const AuroraWindowSize*) {
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigNavCursorVisibleAlways = true;  // the green highlight is the cursor, as in the game's menus
  LoadFonts();
  ApplyTheme();
}

void overlay_init(bool offscreen) {
  Overlay& o = O();
  o.offscreen = offscreen;
  o.startMsaa = graphics_settings().msaa;
  o.startBackend = graphics_settings().backend;
  o.backendInUse = aurora_get_backend();
  ParseTestScript();
}

namespace {
bool g_f1Down = false;           // F1 held at the last poll (overlay_poll_keys)
bool g_f1ToggledByEvent = false; // this frame's F1 key event already toggled
}  // namespace

void overlay_handle_event(const SDL_Event& e) {
  if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat) return;
  const bool f1 = e.key.key == SDLK_F1 || e.key.scancode == SDL_SCANCODE_F1;
  if (f1 && !(e.key.mod & (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
    std::fprintf(stderr, "[overlay] F1 key event\n");
    g_f1ToggledByEvent = true;
    Toggle();
  } else if (e.key.key == SDLK_ESCAPE && O().open) {
    O().keyBack = true;
  }
}

// Once a frame after the events: F1 from the keyboard state too, for when the key event does not
// reach the game (it is routed to aurora's input layer first). One toggle per press.
void overlay_poll_keys() {
  int count = 0;
  const bool* keys = SDL_GetKeyboardState(&count);
  const bool down = keys && count > SDL_SCANCODE_F1 && keys[SDL_SCANCODE_F1];
  if (down && !g_f1Down && !g_f1ToggledByEvent) {
    const SDL_Keymod mod = SDL_GetModState();
    if (!(mod & (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
      std::fprintf(stderr, "[overlay] F1 held (keyboard state)\n");
      Toggle();
    }
  }
  g_f1Down = down;
  g_f1ToggledByEvent = false;
}

bool overlay_is_open() { return O().open; }
// (PC mod: network play: never in a session: the partner's game goes on, so this PC's must too; its
// pads are idle while the options are open.)
bool overlay_pauses_game() { return O().open && !net_active(); }

void overlay_filter_pads(PADStatus* pads) {
  Overlay& o = O();
  if (o.injectFrames > 0) {  // test presses on pad 1
    --o.injectFrames;
    pads[0].button = static_cast<u16>(pads[0].button | o.injectButtons);
    if (o.injectButtons & PAD_TRIGGER_L) pads[0].triggerLeft = 255;
    if (o.injectButtons & PAD_TRIGGER_R) pads[0].triggerRight = 255;
    pads[0].err = PAD_ERR_NONE;
  }
  for (int i = 0; i < 4; ++i) {
    PADStatus& p = pads[i];
    o.pads[i] = p;
    u16 b = p.err == PAD_ERR_NONE ? p.button : static_cast<u16>(0);
    if (p.err == PAD_ERR_NONE && p.triggerLeft > 150) b = static_cast<u16>(b | PAD_TRIGGER_L);
    if (p.err == PAD_ERR_NONE && p.triggerRight > 150) b = static_cast<u16>(b | PAD_TRIGGER_R);
    const u16 pressed = static_cast<u16>(b & ~o.prevButtons[i]);
    o.prevButtons[i] = b;
    if (o.open) {
      if (pressed & PAD_BUTTON_B) o.padBack = true;
      if (pressed & PAD_BUTTON_START) o.padClose = true;
      if (pressed & PAD_TRIGGER_L) o.padPrevTab = true;
      if (pressed & PAD_TRIGGER_R) o.padNextTab = true;
    }
    if (p.err != PAD_ERR_NONE) continue;
    const bool held = b != 0 || std::abs(p.stickX) > 24 || std::abs(p.stickY) > 24 || std::abs(p.substickX) > 24 ||
                      std::abs(p.substickY) > 24 || p.triggerLeft > 40 || p.triggerRight > 40;
    if (o.open || (o.latch[i] && held)) Neutral(p);
    else o.latch[i] = false;
  }
}

// PC mod: network play: a one-line note at the top of the window (waiting for the partner, the
// session's end; pc_net.cpp), no input.
static void DrawNetNotice() {
  const std::string& text = net_notice();
  if (text.empty()) return;
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 12.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(vp->WorkSize.x * 0.9f, vp->WorkSize.y * 0.5f));
  ImGui::SetNextWindowBgAlpha(0.8f);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
  if (ImGui::Begin("##simmy_net_notice", nullptr, flags)) {
    ImGui::PushTextWrapPos(vp->WorkSize.x * 0.85f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::End();
}

void overlay_frame(unsigned devFrame) {
  Overlay& o = O();
  if (o.test) RunTestSteps(devFrame);
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx && o.test && o.offscreen) {
    CreateTestContext();
    ctx = ImGui::GetCurrentContext();
  }
  if (ctx && o.ownContext) {
    ImGuiIO& io = ImGui::GetIO();
    u32 w = 0, h = 0;
    AuroraGetRenderSize(&w, &h);
    if (!w || !h) w = 640, h = 480;
    io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = 1.0f / 60.0f;
    FeedPads(io);
    ImGui::NewFrame();
    if (o.open) Draw();
    ImGui::Render();
    KeepDrawData();
  } else if (o.open && (!ctx || !ctx->WithinFrameScope)) {
    static int s_warned = 0;  // open, but aurora has no ImGui frame this frame: nothing can be drawn
    if (s_warned++ < 5)
      std::fprintf(stderr, "[overlay] open but no ImGui frame (context %s)\n", ctx ? "yes" : "no");
  }
  if (!o.ownContext && ctx && ctx->WithinFrameScope) {
    if (o.dropFocus && !o.open) ImGui::SetWindowFocus(nullptr);  // no keyboard capture once closed
    o.dropFocus = false;
    if (!o.gamepadModeSet) {
      // Any connected gamepad navigates (both players), not only the first one.
      const char* backend = ImGui::GetIO().BackendPlatformName;
      if (backend && std::strstr(backend, "sdl3")) ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);
      o.gamepadModeSet = true;
    }
    if (o.open) Draw();
    DrawNetNotice();
  }
  o.padBack = o.padClose = o.padPrevTab = o.padNextTab = o.keyBack = false;
}

void overlay_shutdown() {
  Overlay& o = O();
  if (o.ownContext && ImGui::GetCurrentContext()) ImGui::DestroyContext();  // before aurora_shutdown looks for one
  o.ownContext = false;
  o.atlas = nullptr;
}

void overlay_shot_requested() {
  Overlay& o = O();
  if (!o.ownContext) return;
  o.shotDraw = o.lastDraw;
  o.shotSize = o.lastSize;
}

void overlay_composite_shot(unsigned char* rgba, unsigned width, unsigned height) {
  Overlay& o = O();
  if (!o.ownContext || o.shotDraw.empty()) return;
  Rasterise(o.shotDraw, o.shotSize, rgba, width, height);
  o.shotDraw.clear();
}

}  // namespace simmy
