// PC development aids that use the game's own systems.
//
// --render-test draws, through the ported render context and renderer, the boot shaders ESimsApp::Init
// loads (the background gradient and the localised logo) and three untextured quads with the default
// shader. It exercises shader selection, texture upload/selection, TEV set-up and blending end to end.
// It also draws two lines of text with the game font (ERFont::Draw and the drop-shadowed DrawDs).
//
// --qd-test (or SIMMY_QD_TEST=1) loads the UI quickdatas once and prints their tables and a few strings.
//
// SIMMY_TEST_LOADING=1 shows the loading screen (LoadingScreenStateMachine::StartingLoad) from the start.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <chrono>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern ENgcRenderer _ngcRenderer;

// Apt UI variable sets that changed the variable's value (SetVariableCommandTable::ExecuteCommand); the
// pad sequence counts them as reactions. (Some screens re-send the same value every few frames.)
int simmy_dev_ui_sets = 0;
void simmy_dev_note_ui_set(const char* name, const char* value) {
  static std::map<std::string, std::string> s_last;
  std::string& last = s_last[name ? name : ""];
  const char* v = value ? value : "";
  if (last != v) last = v, ++simmy_dev_ui_sets;
}

namespace simmy {

namespace {
void DrawRect(ERC* rc, float x0, float y0, float x1, float y1, const EVec4& color) {
  // The game's textures are stored bottom row first, so v runs up the screen.
  EVec2 p0{x0, y0}, p1{x1, y1}, uv0{0, 1}, uv1{1, 0};
  EVec4 c = color;
  rc->Rect(p0, p1, uv0, uv1, c, 0.0f);
}

void SelectShader(ERC* rc, ERShader* r) {
  if (EShader* s = r ? r->GetShader() : nullptr) s->Select(rc, 0xffffffffu);
}

// Text through ERFont, in normalised screen coordinates, with the game's font (EGlobal::GetGameFontID).
void DrawText(ERC* rc) {
  static ERFont* font = static_cast<ERFont*>(_fontman.AddRef(0x67f664fbu, nullptr, 0));
  if (!font || !font->m_size) return;
  font->SetSize(24.0f, 1.0f, true);
  font->Select(rc);
  EVec4 white{1, 1, 1, 1}, yellow{1.0f, 0.9f, 0.3f, 1.0f};
  EVec2 pos{0.5f, 0.06f};
  font->SetColor(white);
  font->Draw(rc, const_cast<char*>("The Sims 2 PC port"), pos, EFontAlignX::Center, EFontAlignY::Top, nullptr,
             nullptr, 0, nullptr, true);
  EVec2 ds{0.5f, 0.68f};
  font->SetColor(yellow);
  font->DrawDs(rc, const_cast<char*>("ERFont::DrawDs - drop shadow"), ds, EFontAlignX::Center, EFontAlignY::Top,
               nullptr, 2.0f, 1.0f, true);
  font->SetColor(white);
}

// Quickdata text is host-endian UTF-16 (see erquickdata.hpp).
std::string Utf8(const unsigned short* s) {
  std::string out;
  if (!s) return "(null)";
  for (; *s; ++s) {
    unsigned int c = *s;
    if (c >= 0xD800 && c < 0xDC00 && s[1] >= 0xDC00 && s[1] < 0xE000) c = 0x10000 + ((c - 0xD800) << 10) + (*++s - 0xDC00);
    if (c < 0x80) {
      out += static_cast<char>(c);
    } else if (c < 0x800) {
      out += static_cast<char>(0xC0 | c >> 6);
      out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
      out += static_cast<char>(0xE0 | c >> 12);
      out += static_cast<char>(0x80 | (c >> 6 & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | c >> 18);
      out += static_cast<char>(0x80 | (c >> 12 & 0x3F));
      out += static_cast<char>(0x80 | (c >> 6 & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
  }
  return out;
}

// NewUIStrings rows read the way EGlobal::GetNewUIString reads them, then a Reload in French.
void QuickdataStrings(ERQuickdata* qd);

// --qd-test / SIMMY_QD_TEST: the quickdatas EGlobal::LoadIntroRequirements loads ("simsui" and
// "urbzui") and their tables.
void QuickdataTest() {
  for (unsigned int id : {0xa173a1eeu, 0x37a8cfb9u}) {
    auto* qd = static_cast<ERQuickdata*>(_quickdataman.AddRef(id, nullptr, 0));
    if (!qd || !qd->GetImage()) {
      std::fprintf(stderr, "[qd-test] %08X: not loaded\n", id);
      continue;
    }
    std::fprintf(stderr, "[qd-test] %08X \"%s\": %u bytes, revision %u, language %d, %d tables\n", id,
                 ERQuickdata::String(qd->GetImage(), 4), qd->GetImageSize(), qd->GetRevisionNumber(),
                 _quickdataman.GetCurrentLanguage(), qd->GetTableCount());
    for (int i = 0; i < qd->GetTableCount(); ++i) {
      const void* t = qd->GetTableByIndex(i);
      std::fprintf(stderr, "[qd-test]   %-22s %5u rows x %3u bytes%s\n", ERQuickdata::TableName(t),
                   ERQuickdata::TableRowCount(t), ERQuickdata::TableRowSize(t),
                   ERQuickdata::TableRowNames(t) ? ", named" : "");
    }
    if (id == 0x37a8cfb9u) QuickdataStrings(qd);
    _quickdataman.DelRef(qd, EResourceManager__DelRefMode::Cache);
  }
}

void QuickdataStrings(ERQuickdata* qd) {
  const unsigned int id = qd->m_id;
  const void* table = qd->getTable("NewUIStrings");
  for (const char* name : {"copyright_string", "loading_please_wait", "no_such_row"}) {
    const void* row = qd->getRow(table, name);
    const unsigned short* const* handle = row ? ERQuickdata::LocSlot(row, 0) : nullptr;  // GC: *(wchar_t***)row
    unsigned int index = 0;
    const void* found = row ? qd->findRow(row, &index) : nullptr;
    const char* tableName = nullptr;
    qd->FindTableName(row, &tableName);
    std::fprintf(stderr, "[qd-test]   NewUIStrings.%s = \"%s\" (findRow: %s row %u = %s)\n", name,
                 handle ? Utf8(*handle).c_str() : "(no row)", tableName ? tableName : "-", index,
                 found ? ERQuickdata::TableRowName(found, index) : "-");
  }
  // EQuickdataManager::Reload re-reads the image in place in the current language.
  const int language = _quickdataman.GetCurrentLanguage();
  _quickdataman.SetCurrentLanguage(2);
  _quickdataman.Reload(id);
  const void* row = qd->getRow(table, "loading_please_wait");
  std::fprintf(stderr, "[qd-test]   after Reload in language 2: loading_please_wait = \"%s\"\n",
               Utf8(ERQuickdata::LocString(row, 0)).c_str());
  _quickdataman.SetCurrentLanguage(language);
  _quickdataman.Reload(id);
}

// SIMMY_APT_TEST=<movie>[,<movie>...]: loads each Flash movie from the "flashes" archive, reads it
// into the native Apt structures and prints a summary (characters by type, frames, controls).
void AptTest(const char* list) {
  std::string names(list);
  size_t start = 0;
  while (start <= names.size()) {
    size_t comma = names.find(',', start);
    if (comma == std::string::npos) comma = names.size();
    std::string name = names.substr(start, comma - start);
    start = comma + 1;
    if (name.empty()) continue;
    auto* flash = static_cast<ERFlash*>(_flashman.AddRef(const_cast<char*>(name.c_str()), nullptr, 0));
    if (!flash || !flash->m_big) {
      std::fprintf(stderr, "[apt-test] %s: not loaded\n", name.c_str());
      continue;
    }
    FlashBigFile* big = flash->GetFlashBigData();
    flash->DelRef();
    auto entry = [&](const char* ext, unsigned int& size) -> unsigned char* {
      std::string n = name + ext;
      int index = 0, offset = 0, sz = 0;
      if (!BIG_locateentry(big->m_header, const_cast<char*>(n.c_str()), &index, &offset, &sz)) return nullptr;
      size = static_cast<unsigned int>(sz);
      return static_cast<unsigned char*>(big->m_entries[index]);
    };
    unsigned int aptSize = 0, constSize = 0;
    unsigned char* apt = entry(".apt", aptSize);
    unsigned char* cst = entry(".const", constSize);
    AptFileImage* image = AptFileImage::Build(apt, aptSize, cst, constSize);
    if (!image) {
      std::fprintf(stderr, "[apt-test] %s: could not read the movie\n", name.c_str());
      delete big;
      continue;
    }
    const AptCharacterAnimation* a = image->Animation();
    int byType[11] = {};
    int frames = a->timeline.frameCount, controls = 0, places = 0, actions = 0;
    auto countTimeline = [&](const AptMovie& m) {
      for (int f = 0; f < m.frameCount; ++f) {
        controls += m.frames[f].controlCount;
        for (int i = 0; i < m.frames[f].controlCount; ++i) {
          const int t = m.frames[f].controls[i]->type;
          places += t == AptControl::kPlaceObject;
          actions += t == AptControl::kAction || t == AptControl::kInitAction;
        }
      }
    };
    countTimeline(a->timeline);
    for (int i = 0; i < a->characterCount; ++i) {
      const AptCharacter* ch = a->characters[i];
      if (!ch) continue;
      if (ch->type > 0 && ch->type <= 10) ++byType[ch->type];
      if (ch->type == AptCharacter::kSprite) {
        frames += static_cast<const AptCharacterSprite*>(ch)->timeline.frameCount;
        countTimeline(static_cast<const AptCharacterSprite*>(ch)->timeline);
      }
    }
    std::fprintf(stderr,
                 "[apt-test] %s: %dx%d, %d frames, %d characters (shape %d, text %d, font %d, button %d, sprite %d, "
                 "sound %d, image %d, morph %d, static text %d), %d constants\n",
                 name.c_str(), a->width, a->height, a->timeline.frameCount, a->characterCount, byType[1], byType[2],
                 byType[3], byType[4], byType[5], byType[6], byType[7], byType[8], byType[10],
                 image->m_constFile.itemCount);
    std::fprintf(stderr, "[apt-test]   all timelines: %d frames, %d controls (%d place, %d action blocks)\n", frames,
                 controls, places, actions);
    for (int i = 0; i < a->exportCount; ++i)
      std::fprintf(stderr, "[apt-test]   export %s = character %d\n", a->exports[i].name, a->exports[i].character);
    for (int i = 0; i < a->importCount; ++i)
      std::fprintf(stderr, "[apt-test]   import %s from %s -> character %d\n", a->imports[i].name,
                   a->imports[i].movie, a->imports[i].character);
    for (int f = 0; f < a->timeline.frameCount && f < 2; ++f)
      for (int i = 0; i < a->timeline.frames[f].controlCount; ++i) {
        const AptControl* c = a->timeline.frames[f].controls[i];
        if (c->type != AptControl::kPlaceObject) continue;
        auto* p = static_cast<const AptControlPlaceObject2*>(c);
        std::fprintf(stderr, "[apt-test]   frame %d: place character %d at depth %d (%.1f, %.1f) %s\n", f,
                     p->character, p->depth, p->matrix.tx, p->matrix.ty, p->name ? p->name : "");
      }
    delete image;
    delete big;
  }
}
// SIMMY_AUDIO_TEST=<list>: plays audio through the game's own calls, as the start screen does. Each
// comma-separated entry is a music stream name or id (ENgcAudio::PlayMusic, started once; "loop:"
// in front to loop it) or, with a "sfx:" prefix, a sample name or id (UIAUDIO::PlaySound, one every
// second in turn). Example:
// SIMMY_AUDIO_TEST=music_48_title_track,sfx:0x374BDCEA
void AudioTest(const char* list) {
  static std::vector<unsigned int> samples;
  static bool started = false;
  static int frame = 0;
  static bool wasPlaying = false;
  auto id = [](const std::string& s) {
    return s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')
               ? static_cast<unsigned int>(std::strtoul(s.c_str() + 2, nullptr, 16))
               : EResourceManager::CalcId(const_cast<char*>(s.c_str()));
  };
  if (!started) {
    started = true;
    std::string all(list);
    for (size_t pos = 0; pos <= all.size();) {
      size_t end = all.find(',', pos);
      if (end == std::string::npos) end = all.size();
      const std::string entry = all.substr(pos, end - pos);
      pos = end + 1;
      if (entry.empty()) continue;
      if (entry.rfind("sfx:", 0) == 0) {
        samples.push_back(id(entry.substr(4)));
      } else if (entry.rfind("loop:", 0) == 0) {
        EPMDesc music(id(entry.substr(5)), true);
        _pActualAudio->PlayMusic(music);
        _pActualAudio->SetMusicVolume(1.0f);
        std::fprintf(stderr, "[audio-test] music %08X (looping)\n", music.m_id);
      } else {
        EPMDesc music(id(entry), false);
        _pActualAudio->PlayMusic(music);
        _pActualAudio->SetMusicVolume(1.0f);
        std::fprintf(stderr, "[audio-test] music %08X\n", music.m_id);
      }
    }
  }
  if (!samples.empty() && frame % 60 == 0) {
    const unsigned int sample = samples[static_cast<size_t>(frame / 60) % samples.size()];
    std::fprintf(stderr, "[audio-test] frame %d: sample %08X\n", frame, sample);
    UIAUDIO::Instance()->PlaySound(sample);
  }
  if (_pActualAudio->IsPlayingMusic() != wasPlaying) {
    wasPlaying = !wasPlaying;
    std::fprintf(stderr, "[audio-test] frame %d: music %s\n", frame, wasPlaying ? "playing" : "stopped");
  }
  ++frame;
}
}  // namespace

static void RenderSurfaceTest(ERC* rc) {
  static ERenderSurface* surface = nullptr;
  static int frame = 0;
  if (!surface) {
    ERenderSurfaceDef def;
    def.m_xSize = 64;
    def.m_ySize = 48;
    def.m_flags = 3;
    def.m_background[0] = 1.0f;
    def.m_background[1] = 0.5f;
    def.m_background[2] = 0.0f;
    surface = _pGfx->CreateRenderSurface(def);
    std::fprintf(stderr, "[rs-test] surface %p texture %p\n", static_cast<void*>(surface),
                 surface ? static_cast<void*>(static_cast<ENgcRenderSurface*>(surface)->GetTexture()) : nullptr);
    if (!surface) return;
  }
  rc->RenderSurface(surface, 0);
  rc->RenderSurface(nullptr, 0);
  auto* ngc = static_cast<ENgcRenderSurface*>(surface);
  if (++frame % 30 == 0 && frame <= 120) {
    std::vector<unsigned char> px(64 * 48 * 4);
    ngc->GetImageData(px.data());
    auto at = [&](int x, int y) {
      const unsigned char* p = &px[static_cast<size_t>((y * 64 + x) * 4)];
      std::fprintf(stderr, " (%d,%d)=%02x%02x%02x%02x", x, y, p[0], p[1], p[2], p[3]);
    };
    std::fprintf(stderr, "[rs-test] frame %d:", frame);
    at(0, 0), at(32, 24), at(63, 47);
    std::fprintf(stderr, "\n");
  }
}

// Presses the '+'-joined button/stick names in `names` on pad 1 (pads[0]); a name prefixed "p2:" goes to
// pad 2 (pads[1]), e.g. "lup+p2:lleft" runs player 1 up and player 2 left at once.
static void dev_press_pad(PADStatus* pads, const std::string& names) {
  static const struct {
    const char* name;
    u16 bit;
  } kButtons[] = {{"start", PAD_BUTTON_START}, {"a", PAD_BUTTON_A}, {"b", PAD_BUTTON_B}, {"x", PAD_BUTTON_X},
                  {"y", PAD_BUTTON_Y}, {"up", PAD_BUTTON_UP}, {"down", PAD_BUTTON_DOWN},
                  {"left", PAD_BUTTON_LEFT}, {"right", PAD_BUTTON_RIGHT},
                  {"l", PAD_TRIGGER_L}, {"r", PAD_TRIGGER_R}, {"z", PAD_TRIGGER_Z}};
  pads[0].err = PAD_ERR_NONE;
  size_t pos = 0;
  while (pos <= names.size()) {
    const size_t plus = names.find('+', pos);
    std::string name = names.substr(pos, plus == std::string::npos ? std::string::npos : plus - pos);
    PADStatus* pad = &pads[0];
    if (name.compare(0, 3, "p2:") == 0) {
      pad = &pads[1];
      pad->err = PAD_ERR_NONE;
      name.erase(0, 3);
    }
    for (const auto& b : kButtons)
      if (name == b.name) pad->button |= b.bit;
    // Sticks at full deflection: l* the main stick, c* the C stick.
    if (name == "lup") pad->stickY = 127;
    if (name == "ldown") pad->stickY = -127;
    if (name == "lleft") pad->stickX = -127;
    if (name == "lright") pad->stickX = 127;
    if (name == "cup") pad->substickY = 127;
    if (name == "cdown") pad->substickY = -127;
    if (name == "cleft") pad->substickX = -127;
    if (name == "cright") pad->substickX = 127;
    if (plus == std::string::npos) break;
    pos = plus + 1;
  }
}

// SIMMY_PAD_SEQ="start,a,right,a,up,a,a,left,a,house,cdown/150": presses each step in turn,
// independent of frame timing. The UI state watched is the managed screens, the main menu's state
// and choices and the dialog box's state and selection. A step fires once that state has been
// unchanged for 90 frames (and 1.5 s: Apt runs on the wall clock) and at least 60 frames (1 s) have
// passed since the previous press; a press that changed nothing in 300 frames (5 s) is repeated, at
// most three times (not once in the house, where nothing watched changes). The step "house" waits
// for a house to be loaded and 120 frames more; "name/N" holds the press N frames (default
// SIMMY_PAD_HOLD, else 6). Progress is logged as "[padseq]". SIMMY_PAD_SEQ_UI=1 also watches the UI
// variables the movies set out of a house and Create-a-Sim's screen and item, and does not repeat
// presses in Create-a-Sim (to drive the front end, free play and Create-a-Sim by pad).
static void dev_pad_seq(PADStatus* pads, const char* seq) {
  static std::vector<std::string> steps;
  static bool parsed = false;
  if (!parsed) {
    parsed = true;
    for (const char* p = seq; *p;) {
      const char* end = std::strchr(p, ',');
      steps.emplace_back(p, end ? end : p + std::strlen(p));
      if (!end) break;
      p = end + 1;
    }
  }
  static size_t step = 0;
  static int lastPress = 0, holdUntil = 0, stableSince = 0, houseSince = -1;
  static std::string pressing;
  static unsigned long long hashAtPress = 0;
  static bool reacted = true;
  static int tries = 0;
  // Apt animates on the wall clock, so the waits are in both frames and seconds (the game's clock:
  // SIMMY_DET makes it virtual, pc_det.cpp).
  const double now = static_cast<double>(game_clock_ns()) * 1e-9;
  static double stableSinceSec = 0, lastPressSec = 0;
  static unsigned long long lastHash = 0;
  if (_framecount < holdUntil) {
    dev_press_pad(pads, pressing);
    return;
  }
  unsigned long long hash = 1469598103934665603ull;
  if (auto* mgr = static_cast<UIScreenManager*>(_globals.m_uiScreenManager))
    for (const auto& r : mgr->m_records) hash = (hash ^ (unsigned)(r.id * 8 + r.state)) * 1099511628211ull;
  auto mix = [&](long long v) { hash = (hash ^ (unsigned long long)v) * 1099511628211ull; };
  mix(MMUTarget::m_MMUSingleton != nullptr);
  if (auto* mmu = MMUTarget::m_MMUSingleton)
    mix(mmu->m_state), mix(mmu->m_topLevelChoice), mix(mmu->m_gameplayChoice), mix(mmu->m_playersChoice);
  if (auto* g2d = static_cast<G2DTarget*>(_globals.m_g2dTarget)) {
    mix(g2d->m_spawned), mix(g2d->m_active), mix(g2d->m_visible), mix(g2d->m_state), mix(g2d->m_selection);
  }
  mix(_globals.m_house != nullptr);
  // SIMMY_PAD_SEQ_UI=1: out of a house (front end, free play's screens, Create-a-Sim) any UI variable
  // set that changes a value counts as a reaction, as do Create-a-Sim's screen and item.
  static const bool uiWatch = std::getenv("SIMMY_PAD_SEQ_UI") && *std::getenv("SIMMY_PAD_SEQ_UI") != '0';
  if (uiWatch) {
    if (CASTarget* cas = _gd.m_casTarget) mix(cas->m_screen), mix(cas->m_subnav);
    if (_gd.m_casTarget || !_globals.m_house) mix(simmy_dev_ui_sets);
  }
  if (hash != lastHash) {
    lastHash = hash;
    stableSince = _framecount;
    stableSinceSec = now;
    std::string ids;
    if (auto* mgr = static_cast<UIScreenManager*>(_globals.m_uiScreenManager))
      for (const auto& r : mgr->m_records)
        if (r.id) ids += " " + std::to_string(r.id) + ":" + std::to_string(r.state);
    auto* g2d = static_cast<G2DTarget*>(_globals.m_g2dTarget);
    std::printf("[padseq] frame %d: screens%s mmu %d g2d %d/%d\n", _framecount, ids.c_str(),
                MMUTarget::m_MMUSingleton ? MMUTarget::m_MMUSingleton->m_state : -1, g2d ? g2d->m_active : -1,
                g2d ? g2d->m_state : -1);
  }
  if (_globals.m_house && houseSince < 0) houseSince = _framecount;
  if (!_globals.m_house) houseSince = -1;  // (quit to the main menu: the next `house` waits for the next one)
  if (step >= steps.size()) return;
  std::string names = steps[step];
  if (names == "house") {
    if (houseSince >= 0 && _framecount - houseSince >= 120) {
      std::printf("[padseq] frame %d: house loaded\n", _framecount);
      if (EController* pad = _pCtrlMan ? _pCtrlMan->GetController(0) : nullptr) {
        std::printf("[padseq] load in progress %d; players mapped %d:%d %d:%d; pad 1 filters:",
                    LoadingScreenStateMachine::LoadInProgress() ? 1 : 0, _pCtrlMan->GetPlayerMapped(0),
                    _pCtrlMan->GetPlayerControllerIndex(0), _pCtrlMan->GetPlayerMapped(1), _pCtrlMan->GetPlayerControllerIndex(1));
        for (int i = 0; i <= pad->m_topFilter && i < 32; ++i)
          std::printf(" %u/%x", pad->m_filters[i].m_id, pad->m_filters[i].m_mask);
        std::printf("\n");
      }
      ++step;
      lastPress = _framecount;
      lastPressSec = now;
      hashAtPress = hash;
      reacted = true;
      tries = 0;
    }
    return;
  }
  // A press that changed nothing within 300 frames (it came while the screen ignored input) is repeated,
  // up to three times.
  if (!reacted && hash != hashAtPress) reacted = true, tries = 0;
  // In the house nothing watched changes: presses there are not checked.
  if (houseSince >= 0) reacted = true;
  // SIMMY_PAD_SEQ_UI=1: Create-a-Sim's sliders and lists change only the movie's own state, so presses
  // there are not checked either.
  if (uiWatch && _gd.m_casTarget) reacted = true;
  if (!reacted) {
    if (_framecount - lastPress < 300 || now - lastPressSec < 5.0) return;
    reacted = true;
    if (++tries < 3) {
      --step;
      std::printf("[padseq] frame %d: no reaction, repeating\n", _framecount);
    } else {
      tries = 0;  // (e.g. "up" on a selection already at the top)
      std::printf("[padseq] frame %d: no reaction, going on\n", _framecount);
    }
  }
  if (_framecount - stableSince < 90 || _framecount - lastPress < 60 || now - stableSinceSec < 1.5 ||
      now - lastPressSec < 1.0)
    return;
  names = steps[step];
  static const int defaultHold = std::getenv("SIMMY_PAD_HOLD") ? std::atoi(std::getenv("SIMMY_PAD_HOLD")) : 6;
  int hold = defaultHold;
  if (const size_t slash = names.find('/'); slash != std::string::npos) {
    hold = std::atoi(names.c_str() + slash + 1);
    names.resize(slash);
  }
  std::printf("[padseq] frame %d: %s/%d\n", _framecount, names.c_str(), hold);
  pressing = names;
  hashAtPress = hash;
  reacted = false;
  holdUntil = _framecount + hold;
  lastPress = _framecount + hold;
  lastPressSec = now;
  ++step;
  dev_press_pad(pads, pressing);
}

void dev_pad_script(PADStatus* pads) {
  static const char* seq = std::getenv("SIMMY_PAD_SEQ");
  static const char* script = std::getenv("SIMMY_PAD_SCRIPT");
  // A script that drives pad 2 ("p2:" steps) keeps it plugged in throughout, as pad 1.
  static const bool usesPad2 = (seq && std::strstr(seq, "p2:")) || (script && std::strstr(script, "p2:"));
  if (usesPad2) pads[1].err = PAD_ERR_NONE;
  if (seq) {
    pads[0].err = PAD_ERR_NONE;
    dev_pad_seq(pads, seq);
    // With both, SIMMY_PAD_SCRIPT's presses are added at its frames (quick presses after a SEQ that
    // reached the house, e.g. the button-code cheats, whose presses must come within a second).
  }
  if (!script) return;
  pads[0].err = PAD_ERR_NONE;  // the scripted pad stays plugged in between presses
  for (const char* p = script; *p;) {
    const int frame = std::atoi(p);
    const char* colon = std::strchr(p, ':');
    if (!colon) break;
    const char* end = std::strchr(colon, ',');
    std::string names(colon + 1, end ? end : colon + std::strlen(colon));
    // "name/N": held N frames (default SIMMY_PAD_HOLD, else 6).
    static const int defaultHold = std::getenv("SIMMY_PAD_HOLD") ? std::atoi(std::getenv("SIMMY_PAD_HOLD")) : 6;
    int hold = defaultHold;
    if (const size_t slash = names.find('/'); slash != std::string::npos) {
      hold = std::atoi(names.c_str() + slash + 1);
      names.resize(slash);
    }
    if (_framecount >= frame && _framecount < frame + hold) dev_press_pad(pads, names);
    if (!end) break;
    p = end + 1;
  }
}

void dev_draw_overlay(ERC* rc) {
  if (const char* audio = std::getenv("SIMMY_AUDIO_TEST")) AudioTest(audio);
  // SIMMY_TEST_LOADING=1: bring up the loading screen (plumb bob) as soon as its state machine exists,
  // as code starting a load does; it stays up because no load ever finishes.
  static bool loadingStarted = false;
  if (!loadingStarted && LoadingScreenStateMachine::s_pSM && std::getenv("SIMMY_TEST_LOADING")) {
    loadingStarted = true;
    LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{0});
  }
  static bool quickdataTested = false;
  if (!quickdataTested && (dev_options().quickdataTest || std::getenv("SIMMY_QD_TEST"))) {
    quickdataTested = true;
    QuickdataTest();
  }
  static bool aptTested = false;
  if (!aptTested && std::getenv("SIMMY_APT_TEST")) {
    aptTested = true;
    AptTest(std::getenv("SIMMY_APT_TEST"));
  }
  // SIMMY_TEST_RENDERSURFACE=1: clears a 64x48 offscreen surface to orange every frame and prints its
  // read-back pixels (the render-to-texture path; the read-back lags a few frames).
  if (rc && std::getenv("SIMMY_TEST_RENDERSURFACE")) RenderSurfaceTest(rc);
  if (!dev_options().renderTest || !rc) return;
  static ERShader* gradient = static_cast<ERShader*>(_shaderman.AddRef(0x7f5abc2du, nullptr, 0));  // s2c_bg_gradient
  static ERShader* logo = static_cast<ERShader*>(
      _shaderman.AddRef(static_cast<unsigned int>(GameData::GetLocalizedSims2Logo()), nullptr, 0));
  static bool logged = false;
  if (!logged) {
    logged = true;
    for (ERShader* r : {gradient, logo}) {
      EShader* sh = r ? r->GetShader() : nullptr;
      if (!sh) continue;
      std::fprintf(stderr, "[render-test] shader layers %d geom %08x surface %08x\n", sh->m_layerCount,
                   sh->m_geometryModes, sh->m_surface);
      for (int i = 0; i < sh->m_layerCount; ++i) {
        const EShaderLayer& L = sh->m_layers[i];
        std::fprintf(stderr, "  layer %d: tex %p raster %08x flags %08x blend %d%d%d%d alpha %02x combine %d texgen %d scale %g,%g\n",
                     i, static_cast<void*>(sh->GetTexture(i)), L.m_field0C, L.m_flags, L.m_blend[0], L.m_blend[1],
                     L.m_blend[2], L.m_blend[3], L.m_alpha, L.m_field21, L.m_texGen, L.m_texScale[0], L.m_texScale[1]);
      }
    }
    std::fprintf(stderr, "[render-test] gradient %p (shader %p), logo %p (shader %p)\n", static_cast<void*>(gradient),
                 gradient ? static_cast<void*>(gradient->GetShader()) : nullptr, static_cast<void*>(logo),
                 logo ? static_cast<void*>(logo->GetShader()) : nullptr);
  }
  if (std::getenv("SIMMY_TEST_CLEAR")) GXSetCopyClear(GXColor{40, 40, 90, 255}, GX_MAX_Z24);
  const EVec4 white{1, 1, 1, 1};
  // SIMMY_TEST_MODE: "plain", "gradient", "logo", "font" (default: all), or "rawgx" (one red quad drawn
  // with plain GX calls, bypassing the renderer). SIMMY_TEST_CLEAR: clear to dark blue.
  const char* mode = std::getenv("SIMMY_TEST_MODE");
  const bool all = !mode;
  if (all || !std::strcmp(mode, "gradient")) {
    SelectShader(rc, gradient);
    DrawRect(rc, 0, 0, 1, 1, white);
  }
  if (all || !std::strcmp(mode, "logo")) {
    SelectShader(rc, logo);
    DrawRect(rc, 0.2f, 0.25f, 0.8f, 0.65f, white);
  }
  if (!all && !std::strcmp(mode, "rawgx")) {
    // Bypass the renderer: plain GX (verifies aurora + vertex format 2 on their own).
    GXSetNumChans(1);
    GXSetCullMode(GX_CULL_NONE);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_NRM, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    Mtx44 ortho;
    C_MTXOrtho(ortho, 0, 1, 0, 1, 0, -1.001f);
    GXSetProjection(ortho, GX_ORTHOGRAPHIC);
    Mtx id;
    PSMTXIdentity(id);
    GXLoadPosMtxImm(id, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXBegin(GX_QUADS, GX_VTXFMT2, 4);
    const float v[4][2] = {{0.1f, 0.1f}, {0.1f, 0.4f}, {0.4f, 0.4f}, {0.4f, 0.1f}};
    for (auto& p : v) {
      GXPosition3f32(p[0], p[1], 0);
      GXNormal3s8(0, 0, -1);
      GXColor4u8(255, 0, 0, 255);
      GXTexCoord2f32(0, 0);
    }
    GXEnd();
    // GX was set behind the render state cache's back: it sends everything at its next Apply.
    if (ENgcRenderStateCache* c = ::_ngcRenderer.GetRenderStateCache()) c->m_dirty = 0xffffffffu;
    return;
  }
  if (all || !std::strcmp(mode, "font")) DrawText(rc);
  if (!all && std::strcmp(mode, "plain")) return;

  EShader* plain = static_cast<ENgcGraphics*>(_pGfx)->m_defaultShader;
  if (plain) {
    plain->Select(rc, 0xffffffffu);
    // The default shader's blend is (0 - 0) * k + dst, i.e. it leaves the screen alone until the
    // caller sets a blend mode; use ordinary alpha blending: (src - dst) * srcAlpha + dst.
    rc->SetBlendMode(0, 1, 0, 1, 1.0f, 0);
    DrawRect(rc, 0.10f, 0.75f, 0.30f, 0.90f, EVec4{1, 0, 0, 1});
    DrawRect(rc, 0.40f, 0.75f, 0.60f, 0.90f, EVec4{0, 1, 0, 1});
    DrawRect(rc, 0.70f, 0.75f, 0.90f, 0.90f, EVec4{0, 0, 1, 0.5f});
  }
}

}  // namespace simmy
