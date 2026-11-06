// Port of libaptz/Apt.cpp (initialisation, loading, AptCallFunction, internal variables) and the
// parts of AptLoad.cpp / AptAnimation.cpp that attach a loaded movie (AptLoader::CompleteLoad,
// AptCharacterAnimation::Resolve/Fixup/Link, AptLinker::Notify).
#include "simmy/types_gen.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_set>

AptGlobal* gpGlobalGlobalObject = nullptr;
int gAptLoadsSinceCollect = 0;
int gAptUnloadsSinceCollect = 0;

namespace {
std::vector<AptFile*> s_files;  // AptLoader's list
// "_levelN" names a level; "_level0.clip" is a clip path.
bool IsLevelName(const std::string& t, int* level = nullptr) {
  if (t.compare(0, 6, "_level") != 0 || t.size() == 6) return false;
  for (size_t i = 6; i < t.size(); ++i)
    if (t[i] < '0' || t[i] > '9') return false;
  if (level) *level = std::atoi(t.c_str() + 6);
  return true;
}
std::string BaseName(const std::string& path) {
  size_t s = path.find_last_of("/\\");
  std::string n = s == std::string::npos ? path : path.substr(s + 1);
  const size_t dot = n.rfind('.');
  if (dot != std::string::npos) n = n.substr(0, dot);
  return n;
}
void Attach(AptFile* file);
// A clip that is not removed shows the movie.
bool IsShown(const AptFileImage* image) {
  struct Query {
    const AptFileImage* image;
    bool shown;
  } q{image, false};
  AptGC::ForEachLive(
      [](AptValue* v, void* user) {
        auto* q = static_cast<Query*>(user);
        if (v->m_kind == AptValue::kCIH && static_cast<AptCIH*>(v)->m_file == q->image && !static_cast<AptCIH*>(v)->m_removed)
          q->shown = true;
      },
      &q);
  return q.shown;
}
}  // namespace

// 80268998
// Creates the pool, the global object and the built-in classes.
void AptInitialize(void*, bool) {
  if (gpPool) return;
  gpPool = new AptAnimationPoolData;
  gpGlobalGlobalObject = new AptGlobal;
  AptGC::AddRoot(gpGlobalGlobalObject);
  gpRenderingContext = new AptRenderingContext;
  AptRegisterBuiltins();
}

// 80268DD8
void AptShutdown(int) {
  if (!gpPool) return;
  for (AptCIH* l : gpPool->m_levels)
    if (l) l->Remove();
  gpPool->m_levels.clear();
  gpPool->m_actions.clear();
  gpPool->m_timers.clear();
  AptGC::CleanUnreachable();
}

// 802693E8
// Asks the game to load the movie (AptLinker::Load -> the loadAnimation callback, which completes
// at once on PC through AptCompleteAnimationAsyncLoad) and attaches it to `target`.
void AptLoadAnimation(const char* path, const char* target) {
  if (!path || !*path) {
    // an empty name unloads the target level
    int level = 0;
    if (target && IsLevelName(target, &level) && gpPool) {
      if (AptCIH* l = gpPool->getLevel(level)) {
        l->Remove();
        gpPool->m_levels[static_cast<size_t>(level)] = nullptr;
      }
    }
    return;
  }
  ++gAptLoadsSinceCollect;
  auto* file = new AptFile;
  file->name = BaseName(path);
  file->target = target ? target : "_level0";
  file->state = 1;
  // AptLinker::Load (802A040C): a movie already loaded (AptLoader::IsLoaded) is shared, not loaded
  // again (GC: an AptSharedPtr to the same AptFile; here another AptFile with the same image).
  // (PC: only into clips, and only while a clip shows the movie: a movie loaded again after its
  // clips went is loaded afresh, so its #initclip blocks run again as on GC, where the file had gone
  // with its last reference. A level target loads its own copy, which AptLinkerUpdate links.)
  for (AptFile* loaded : s_files) {
    if (IsLevelName(file->target) || !gpPool || !gpPool->getLevel(0)) break;
    if (loaded->name != file->name || !loaded->image || loaded->state < 4 || !IsShown(loaded->image)) continue;
    file->image = loaded->image;
    file->levelInfo = loaded->levelInfo;
    file->state = 4;
    s_files.push_back(file);
    Attach(file);
    // PC: the movie hears of it as of a load (the aux layer's handleSWFLoadComplete).
    const char* args[1] = {path};
    AptCallFunction("handleSWFLoadComplete", nullptr, nullptr, 1, args);
    return;
  }
  s_files.push_back(file);
  if (gAptFuncs.loadAnimation) gAptFuncs.loadAnimation(const_cast<char*>(path), file);
}

namespace {
// AptCharacterAnimation::Fixup (802821A8) and Link (80283024): the aux layer's handles for shapes,
// bitmaps and sounds; fonts point at their glyph shapes.
void FixupAndLink(AptFileImage* image, void* levelInfo) {
  AptCharacterAnimation* a = image->Animation();
  for (int i = 0; i < a->characterCount; ++i) {
    AptCharacter* ch = a->characters[i];
    if (!ch) continue;
    switch (ch->type) {
      case AptCharacter::kShape:
        if (gAptFuncs.loadRenderingUnit)
          static_cast<AptCharacterShape*>(ch)->renderingUnit = gAptFuncs.loadRenderingUnit(levelInfo, i);
        break;
      case AptCharacter::kImage:
        if (gAptFuncs.loadTexture) static_cast<AptCharacterImage*>(ch)->texture = gAptFuncs.loadTexture(levelInfo, i);
        break;
      case AptCharacter::kSound:
        if (gAptFuncs.loadSound) static_cast<AptCharacterSound*>(ch)->sound = gAptFuncs.loadSound(levelInfo, i, nullptr);
        break;
      default: break;
    }
  }
  // Link
  for (int i = 0; i < a->characterCount; ++i) {
    AptCharacter* ch = a->characters[i];
    if (!ch) continue;
    if (ch->type == AptCharacter::kImage && gAptFuncs.bindTexture)
      gAptFuncs.bindTexture(levelInfo, i, static_cast<int>(static_cast<AptCharacterImage*>(ch)->texture));
    if (ch->type == AptCharacter::kFont) {
      auto* f = static_cast<AptCharacterFont*>(ch);
      for (int g = 0; g < f->glyphCount; ++g)
        f->glyphs[g] = f->glyphIds[g] >= 0 && f->glyphIds[g] < a->characterCount ? a->characters[f->glyphIds[g]] : nullptr;
    }
    if (ch->type == AptCharacter::kMorph) {
      auto* m = static_cast<AptCharacterMorph*>(ch);
      if (m->startId >= 0 && m->startId < a->characterCount) m->start = a->characters[m->startId];
      if (m->endId >= 0 && m->endId < a->characterCount) m->end = a->characters[m->endId];
    }
    if (ch->type == AptCharacter::kButton) {
      auto* b = static_cast<AptCharacterButton*>(ch);
      for (int r = 0; r < b->recordCount; ++r) {
        const int id = b->records[r].characterId;
        b->records[r].character = id >= 0 && id < a->characterCount ? a->characters[id] : nullptr;
      }
    }
  }
  if (a->importCount) SIMMY_STUB("AptCharacterAnimation::Link: imports from other movies", 0x80283024);
}

// GlobalNotificationFunction (802A1118) / AptLinker::Notify: put the movie on its level or into
// the clip it was loaded into, and start its first frame.
void Attach(AptFile* file) {
  AptCharacterMovie* movie = file->image->Movie();
  const std::string& target = file->target;
  int level = 0;
  if (IsLevelName(target, &level)) {
    if (level >= static_cast<int>(gpPool->m_levels.size())) gpPool->m_levels.resize(static_cast<size_t>(level) + 1);
    if (AptCIH* old = gpPool->m_levels[static_cast<size_t>(level)]) old->Remove();
    gpPool->m_levels[static_cast<size_t>(level)] = nullptr;
    AptCIH* inst = AptCreateInstance(nullptr, movie, level, "", [&](AptCIH* i) {
      i->m_level = level;
      i->m_file = file->image;
      gpPool->m_levels[static_cast<size_t>(level)] = i;
    });
    file->targetClip = inst;
    return;
  }
  // loadMovie into a clip: the clip takes the movie's timeline in place of its own.
  AptActionInterpreter& in = gAptActionInterpreter;
  AptActionInterpreter::Context ctx;
  ctx.target = gpPool->getLevel(0);
  AptActionInterpreter::Context* saved = in.m_ctx;
  in.m_ctx = &ctx;
  AptCIH* clip = in.getVariable(target).AsCIH();
  in.m_ctx = saved;
  if (!clip) {
    std::fprintf(stderr, "[apt] loadMovie target %s not found\n", target.c_str());
    return;
  }
  clip->m_children.clear();
  clip->m_character = movie;
  clip->m_file = file->image;
  clip->m_frame = 0;
  clip->m_playing = true;
  clip->m_loaded = false;
  clip->doFrameControls(0);
  // PC: the movie's first frame (its function definitions) runs now, so the movie that loaded it can
  // use it in the same frame (as the screens' "onTargetClassReady" does).
  const size_t from = gpPool->m_actions.size();
  clip->queueFrameActions(0);
  std::vector<AptQueuedAction> first(gpPool->m_actions.begin() + static_cast<std::ptrdiff_t>(from), gpPool->m_actions.end());
  gpPool->m_actions.resize(from);
  for (const AptQueuedAction& a : first) {
    if (a.function) gAptActionInterpreter.callFunction(AptVar::Obj(a.function), a.thisv, a.args);
    else gAptActionInterpreter.runStream(a.stream, a.file, a.target);
  }
  file->targetClip = clip;
}
}  // namespace

// 802690FC (AptLoader::CompleteLoad 8029EA94): the game hands over the .apt and .const blocks.
void AptCompleteAnimationAsyncLoad(AptFile* file, void* apt, unsigned int aptSize, void* constData,
                                   unsigned int constSize, void* levelInfo) {
  if (!file) return;
  file->levelInfo = levelInfo;  // (PC: also for a movie that could not be read, so its level goes back)
  if (!apt) return;
  file->image = AptFileImage::Build(static_cast<const unsigned char*>(apt), aptSize,
                                    static_cast<const unsigned char*>(constData), constSize);
  if (!file->image) {
    std::fprintf(stderr, "[apt] %s: could not read the movie\n", file->name.c_str());
    return;
  }
  // GC: loaded (3); AptLinker::Update links and attaches it after the tick's actions. PC: a movie
  // loaded into a clip of a running movie is attached at once.
  file->state = 3;
  int level = 0;
  if (gpPool && gpPool->getLevel(0) && !IsLevelName(file->target, &level)) {
    FixupAndLink(file->image, levelInfo);
    file->state = 4;
    Attach(file);
  }
}

// AptLinker::Update: the loaded movies are linked and attached (their first frame's actions run in
// the next tick, before the movie that loaded them gets to use them).
// GC (8029F2A8) also runs the queued actions: the input handlers the tick queued (which may load
// movies), then the new movies' first frames.
void AptLinkerUpdate() {
  if (gpPool) gpPool->runActions();
  bool attached = false;
  for (size_t i = 0; i < s_files.size(); ++i) {
    AptFile* file = s_files[i];
    if (file->state != 3 || !file->image) continue;
    FixupAndLink(file->image, file->levelInfo);
    file->state = 4;
    Attach(file);
    attached = true;
  }
  if (attached && gpPool) gpPool->runActions();
}

namespace {
// The aux layer's handles of the movie's characters go back (AptCharacterAnimation's destructor on
// GC: freeTexture once per bitmap slot, freeRenderingUnit, freeSound), then its level
// (freeAnimation) and the strings its constants made.
void FreeFile(AptFile* file) {
  if (AptFileImage* image = file->image) {
    if (AptCharacterAnimation* a = image->Animation()) {
      std::unordered_set<int> slots;
      for (int i = 0; i < a->characterCount; ++i) {
        AptCharacter* ch = a->characters[i];
        if (!ch) continue;
        if (ch->type == AptCharacter::kImage) {
          const int slot = static_cast<int>(static_cast<AptCharacterImage*>(ch)->texture);
          if (slot >= 0 && slots.insert(slot).second && gAptFuncs.freeTexture)
            gAptFuncs.freeTexture(reinterpret_cast<void*>(static_cast<intptr_t>(slot)));
        } else if (ch->type == AptCharacter::kShape) {
          if (gAptFuncs.freeRenderingUnit) gAptFuncs.freeRenderingUnit(static_cast<AptCharacterShape*>(ch)->renderingUnit);
        } else if (ch->type == AptCharacter::kSound) {
          if (gAptFuncs.freeSound) gAptFuncs.freeSound(static_cast<AptCharacterSound*>(ch)->sound);
        }
      }
    }
    for (int i = 0; i < image->m_constFile.itemCount; ++i)
      if (AptValue* v = image->m_constFile.items[i].resolved) AptGC::RemoveRoot(v);
    if (gAptFuncs.freeConstantTable) gAptFuncs.freeConstantTable(const_cast<unsigned char*>(image->m_const));
    delete image;
    file->image = nullptr;
  }
  if (file->levelInfo && gAptFuncs.freeAnimation) gAptFuncs.freeAnimation(file->levelInfo);
  file->levelInfo = nullptr;
  delete file;
}

void NoteLiveFile(AptValue* v, void* user) {
  auto* used = static_cast<std::unordered_set<const AptFileImage*>*>(user);
  if (v->m_kind == AptValue::kCIH) used->insert(static_cast<AptCIH*>(v)->m_file);
  else if (auto* f = dynamic_cast<AptScriptFunctionBase*>(v)) used->insert(f->m_file);
}
}  // namespace

void AptReleaseUnusedFiles() {
  gAptLoadsSinceCollect = 0;
  gAptUnloadsSinceCollect = 0;
  std::unordered_set<const AptFileImage*> used;
  AptGC::ForEachLive(NoteLiveFile, &used);
  if (gpPool)
    for (const AptQueuedAction& a : gpPool->m_actions) used.insert(a.file);
  for (const AptActionInterpreter::Context* c = gAptActionInterpreter.m_ctx; c; c = nullptr) used.insert(c->file);
  size_t kept = 0;
  std::unordered_set<const AptFileImage*> freed;  // shared movies (AptLoadAnimation) go once
  for (size_t i = 0; i < s_files.size(); ++i) {
    AptFile* file = s_files[i];
    // Loads complete at once on PC: a file still requested or loading failed; a resolved one waits to
    // be attached (AptLinkerUpdate).
    const bool dead = file->state < 3 || (file->state >= 4 && !used.count(file->image));
    if (!dead) {
      s_files[kept++] = file;
    } else if (file->image && !freed.insert(file->image).second) {
      delete file;
    } else {
      FreeFile(file);
    }
  }
  s_files.resize(kept);
}

// PC diagnostic: the movies holding aux levels (name, state, whether anything live still uses them).
void AptDumpFiles() {
  std::unordered_set<const AptFileImage*> used, reachable;
  AptGC::ForEachLive(NoteLiveFile, &used);
  AptGC::ForEachReachable(NoteLiveFile, &reachable);
  std::fprintf(stderr, "[apt] %zu movies loaded:", s_files.size());
  for (const AptFile* f : s_files)
    std::fprintf(stderr, " %s(%d%s%s%s)", f->name.c_str(), f->state, f->levelInfo ? "" : " no level",
                 f->image && used.count(f->image) ? "" : " unused",
                 f->image && used.count(f->image) && !reachable.count(f->image) ? " garbage" : "");
  std::fprintf(stderr, "\n");
}

// 80269614
bool AptIsFileLoaded(const char* name) {
  for (AptFile* f : s_files)
    if (f->name == BaseName(name) && f->state >= 4) return true;
  return false;
}

// 802693BC
void AptSetValidFocusButton() {}
// 8026B808
void AptFlushInputQueue() {
  if (gpPool) gpPool->m_inputs.clear();
}
// 8026AAEC
void AptAddToInputQueue(int type, int state, int controller) {
  if (gpPool)
    gpPool->addInput(static_cast<unsigned int>(type) << 17 | (static_cast<unsigned int>(state) & 0x7f) << 10 |
                     (static_cast<unsigned int>(controller) & 0xff) << 2 | 1);
}

// 8026AE4C
AptVar AptCallFunction(const char* function, const char*, const char* target, int argc, const char* const* argv) {
  if (!gpPool || !function) return AptVar::Undefined();
  AptActionInterpreter& in = gAptActionInterpreter;
  AptCIH* level0 = gpPool->getLevel(0);
  if (!level0) return AptVar::Undefined();
  AptActionInterpreter::Context ctx;
  ctx.target = level0;
  ctx.thisv = AptVar::Obj(level0);
  AptActionInterpreter::Context* saved = in.m_ctx;
  in.m_ctx = &ctx;
  AptVar obj = target && *target ? in.getVariable(target) : AptVar::Obj(level0);
  std::vector<AptVar> args;
  for (int i = 0; i < argc; ++i) args.push_back(AptVar::Str(argv[i] ? argv[i] : ""));
  AptVar fn = in.getMember(obj, function);
  if (!fn.AsFunction()) fn = in.getVariable(function);
  in.m_ctx = saved;
  return in.callFunction(fn, obj, args);
}

// 8026ABBC: sets a variable by path from _level0 (the game's way to poke the movie).
void AptSetInternalVariable(const char* name, const char* value) {
  if (!gpPool || !gpPool->getLevel(0)) return;
  AptActionInterpreter& in = gAptActionInterpreter;
  AptActionInterpreter::Context ctx;
  ctx.target = gpPool->getLevel(0);
  AptActionInterpreter::Context* saved = in.m_ctx;
  in.m_ctx = &ctx;
  in.setVariable(name, AptVar::Str(value ? value : ""));
  in.m_ctx = saved;
}

// 8026AD5C
std::string AptGetInternalVariable(const char* name) {
  if (!gpPool || !gpPool->getLevel(0)) return "";
  AptActionInterpreter& in = gAptActionInterpreter;
  AptActionInterpreter::Context ctx;
  ctx.target = gpPool->getLevel(0);
  AptActionInterpreter::Context* saved = in.m_ctx;
  in.m_ctx = &ctx;
  const std::string v = in.getVariable(name).ToString();
  in.m_ctx = saved;
  return v;
}

// GetUrl/GetUrl2: "FSCommand:" goes to the game's command callback (doFSCommand 80272984); a
// target level or clip loads the movie there (loadMovie).
void AptGetURL(const std::string& url, const std::string& target, AptCIH* clip) {
  if (url.compare(0, 10, "FSCommand:") == 0 || url.compare(0, 10, "fscommand:") == 0) {
    if (gAptFuncs.command) gAptFuncs.command(const_cast<char*>(url.c_str() + 10), const_cast<char*>(target.c_str()));
    return;
  }
  if (target.empty()) return;
  std::string path = target;
  if (!IsLevelName(target) && clip) {
    // a clip path relative to the caller
    AptActionInterpreter& in = gAptActionInterpreter;
    AptActionInterpreter::Context ctx;
    ctx.target = clip;
    AptActionInterpreter::Context* saved = in.m_ctx;
    in.m_ctx = &ctx;
    AptCIH* t = in.getVariable(target).AsCIH();
    in.m_ctx = saved;
    if (t) path = t->GetTargetPath();
  }
  AptLoadAnimation(url.c_str(), path.c_str());
}
