// PC only: the determinism test harness for lockstep netplay studies (no GameCube counterpart).
// Everything here is off unless one of its variables is set.
//
// SIMMY_DET=1             deterministic timing: the game's clock (EClock, the Apt timer, the renderer's
//                         retrace count, the sound systems' millisecond clocks) is a virtual clock that
//                         advances exactly one 60 Hz retrace per game frame, so every frame is one fixed
//                         step of game time whatever the wall time (render speed, vsync, jitter) was.
//                         The real-time pacing still runs. (SIMMY_DET_SEED: the simulation's rand seed,
//                         default 1, in every mode.)
// SIMMY_DET_HASH=<file>   once the live mode runs a house ("tick 1" is the first frame it updates), writes
//                         "tick hash rng sim obj per ngh inst" per frame: a 64-bit FNV-1a of the
//                         simulation state (see Hash below) and of its parts.
// SIMMY_DET_DUMP=<file>   with SIMMY_DET_DUMP_TICKS=<from>-<to>: every hashed field of those ticks, one
//                         line per object, to diff two runs and find the first differing field.
// SIMMY_DET_RECORD=<file> writes the four pads as the game reads them each tick (after scripted/soak input),
//                         and the keyboard-and-mouse record (pc_kbm.cpp: cursor point, camera) when non-zero.
// SIMMY_DET_REPLAY=<file> from tick 1 on, the pads are the file's (live pads and scripts are ignored; neutral
//                         pads after its end); before tick 1 (front end, loading) the usual pads/scripts
//                         drive the game.
// SIMMY_DET_BOOT=1        ticks count from the first game frame instead of the house's first frame: record,
//                         replay and hashes cover the front end and the loading too (lockstep from boot).
// SIMMY_DET_SPEED=<n>     the simulator speed (0, -1, -2, -3) set at the house's first tick (a deterministic
//                         SIMMY_SIM_SPEED).
// SIMMY_DET_JITTER=<ms>   sleeps a random 0..ms milliseconds every frame (wall-clock random): render-pacing noise.
// SIMMY_DET_NOPACE=1      no 60 Hz pacing (run as fast as possible).
// SIMMY_DET_QUIT=<tick>   quits after that tick.
//
// PC mod: network play (pc_net.cpp) runs a session the same way: det_net_enable turns on the virtual
// timing and boot ticks, det_pads takes both PCs' agreed input from net_pads, det_frame_end hands every
// hash_every-th tick's hash and dump to net_frame_end, and the record holds the session's input.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace simmy {
void platform_request_quit();  // pc_main.cpp

namespace {
const char* Env(const char* n) {
  const char* v = std::getenv(n);
  return v && *v ? v : nullptr;
}

struct DetState {
  bool on = Env("SIMMY_DET") && std::strcmp(Env("SIMMY_DET"), "0") != 0;
  unsigned long long frames = 0;  // virtual retraces so far
  int tick = 0;                   // 0 before the live mode first ran a house
  bool liveRan = false;           // the live mode updated a house this frame
  // SIMMY_DET_BOOT=1: ticks count from the first game frame (boot), not from the house: the record /
  // replay and the hashes cover the front end and the loading too (lockstep from process start).
  bool boot = Env("SIMMY_DET_BOOT") != nullptr;
  int liveTick = 0;               // the tick the live mode first ran a house
  FILE* hash = nullptr;
  FILE* dump = nullptr;
  FILE* lines = nullptr;  // SIMMY_DET_LINES: every tick, each dump line's running category hash
  int dumpFrom = 0, dumpTo = -1;
  // SIMMY_DET_REF=<hash file>: the reference run's hashes; at the first tick that differs, that tick and
  // the one before (and SIMMY_DET_REF_AFTER more, default 2) are dumped in full and "[det] DIVERGED" logged.
  std::vector<unsigned long long> ref;
  int refAfter = Env("SIMMY_DET_REF_AFTER") ? std::atoi(Env("SIMMY_DET_REF_AFTER")) : 2;
  int divergedAt = 0;
  std::string prevDump, curDump;
  FILE* record = nullptr;
  FILE* replay = nullptr;
  bool replayDone = false;
  int jitter = Env("SIMMY_DET_JITTER") ? std::atoi(Env("SIMMY_DET_JITTER")) : 0;
  bool noPace = Env("SIMMY_DET_NOPACE") != nullptr;
  int quitTick = Env("SIMMY_DET_QUIT") ? std::atoi(Env("SIMMY_DET_QUIT")) : 0;
  std::mt19937 jitterRng{static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())};
  DetState() {
    if (const char* f = Env("SIMMY_DET_HASH")) hash = std::fopen(f, "w");
    if (const char* f = Env("SIMMY_DET_DUMP")) dump = std::fopen(f, "w");
    if (const char* f = Env("SIMMY_DET_LINES")) lines = std::fopen(f, "w");
    if (const char* t = Env("SIMMY_DET_DUMP_TICKS")) {
      dumpFrom = std::atoi(t);
      const char* dash = std::strchr(t, '-');
      dumpTo = dash ? std::atoi(dash + 1) : dumpFrom;
    }
    if (const char* f = Env("SIMMY_DET_REF")) {
      if (FILE* r = std::fopen(f, "r")) {
        char buf[512];
        while (std::fgets(buf, sizeof buf, r)) {
          char* p = buf;
          const long t = std::strtol(p, &p, 10);
          if (t != static_cast<long>(ref.size()) + 1) break;
          ref.push_back(std::strtoull(p, nullptr, 16));
        }
        std::fclose(r);
      }
      std::fprintf(stderr, "[det] reference: %d ticks\n", static_cast<int>(ref.size()));
    }
    if (const char* f = Env("SIMMY_DET_RECORD")) record = std::fopen(f, "w");
    if (const char* f = Env("SIMMY_DET_REPLAY")) {
      replay = std::fopen(f, "r");
      if (!replay) std::fprintf(stderr, "[det] cannot read %s\n", f);
    }
    if (on || hash || record || replay)
      std::fprintf(stderr, "[det] timing %s, hash %s, record %s, replay %s, jitter %d ms\n", on ? "virtual" : "wall clock",
                   hash ? "on" : "off", record ? "on" : "off", replay ? "on" : "off", jitter);
  }
};
DetState& St() {
  static DetState s;
  return s;
}

// ---- the state hash -------------------------------------------------------------------------------
// FNV-1a over 64-bit words; with a dump line open, every value is also written as name=value.
struct Hash {
  unsigned long long h = 1469598103934665603ull;
  std::string* line = nullptr;
  void word(unsigned long long v) {
    for (int i = 0; i < 8; ++i) h = (h ^ ((v >> (8 * i)) & 0xff)) * 1099511628211ull;
  }
  void i(const char* name, long long v) {
    word(static_cast<unsigned long long>(v));
    if (line) {
      char buf[64];
      std::snprintf(buf, sizeof buf, " %s=%lld", name, v);
      *line += buf;
    }
  }
  void f(const char* name, float v) {
    unsigned int bits;
    std::memcpy(&bits, &v, 4);
    word(bits);
    if (line) {
      char buf[64];
      std::snprintf(buf, sizeof buf, " %s=%.9g", name, static_cast<double>(v));
      *line += buf;
    }
  }
  void shorts(const char* name, const short* v, int n) {
    for (int k = 0; k < n; ++k) word(static_cast<unsigned short>(v[k]));
    if (line) {
      *line += ' ';
      *line += name;
      *line += '=';
      char buf[16];
      for (int k = 0; k < n; ++k) {
        std::snprintf(buf, sizeof buf, k ? ",%d" : "%d", v[k]);
        *line += buf;
      }
    }
  }
  void floats(const char* name, const float* v, int n) {
    for (int k = 0; k < n; ++k) {
      unsigned int bits;
      std::memcpy(&bits, &v[k], 4);
      word(bits);
    }
    if (line) {
      *line += ' ';
      *line += name;
      *line += '=';
      char buf[32];
      for (int k = 0; k < n; ++k) {
        std::snprintf(buf, sizeof buf, k ? ",%.9g" : "%.9g", static_cast<double>(v[k]));
        *line += buf;
      }
    }
  }
};

int ObjId(cXObject* o) {
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  return impl ? impl->m_id : 0;
}

void HashRelations(Hash& h, RelMatrixImpl* rel) {
  if (!rel) return h.i("rel", -1);
  h.i("rels", static_cast<long long>(rel->m_arrays.size()));
  for (RelArray* a : rel->m_arrays) {
    if (!a) continue;
    h.i("rk", a->m_key);
    for (int v : a->m_values) h.i("rv", v);
  }
}

void HashTree(Hash& h, TreeSimImpl* t) {
  if (!t) return h.i("tree", -1);
  h.i("terr", t->m_error);
  h.i("tres", t->m_lastResult);
  h.i("ttr", t->m_lastTransition);
  h.i("tit", t->m_iterations);
  h.i("depth", t->GetStackSize());
  for (int k = 0; k < t->GetStackSize(); ++k) {
    StackElem* e = t->GetNthElem(static_cast<short>(k));
    h.i("tid", e->m_treeId);
    h.i("node", e->m_node);
    h.i("so", e->m_stackObject);
    h.i("ps", e->m_primState);
    h.i("pd", e->m_primData);
    h.shorts("par", e->m_params, 8);
    if (!e->m_locals.empty()) h.shorts("loc", e->m_locals.data(), static_cast<int>(e->m_locals.size()));
  }
}

void HashAnimator(Hash& h, SAnimator2* a) {
  if (!a) return h.i("anim", -1);
  h.i("arout", a->m_routing);
  h.i("aridx", a->m_routeIndex);
  h.i("afol", a->m_followMode);
  h.floats("apos", a->m_pos, 2);
  h.i("ast", a->m_animState);
  h.f("adir", a->m_dir);
  h.i("ades", a->m_desiredState);
  h.f("addir", a->m_desiredDir);
  h.f("azoom", a->m_zoomRatio);
  h.i("aev", a->m_eventFrame);
  h.i("aoev", a->m_overlayEventFrame);
  h.f("aevf", a->m_eventFraction);
  h.i("acur", a->m_curAnim ? static_cast<long long>(AnimRefId(a->m_curAnim)) : -1);
  h.i("aovl", a->m_overlayAnim ? static_cast<long long>(AnimRefId(a->m_overlayAnim)) : -1);
  h.f("atm", a->m_timeMult);
  h.f("apm", a->m_pauseMult);
  h.i("awalk", a->m_walkStyle);
  h.f("awb", a->m_walkBlend);
  h.i("ashuf", a->m_shuffleDir);
  h.i("alic", a->m_longIdleCount);
  h.i("ali", a->m_longIdle);
  h.f("aimp", a->m_impatientTime);
  h.i("acarry", a->m_carryObject);
  h.i("aevn", static_cast<long long>(a->m_animEvents.size()));
  for (int ev : a->m_animEvents) h.i("aevt", ev);
  h.i("askr", a->m_skillRunning);
  h.i("aloop", a->m_loopCount);
  h.i("askill", a->m_skillAnimId);
  h.i("aflags", a->m_flags);
  h.i("aaware", a->m_aware.m_flags);
  h.f("aang", a->m_aware.m_angle);
  h.f("amemt", a->m_aware.m_memoryTime);
  h.i("acat", a->m_aware.m_category);
  h.i("aact", a->m_aware.m_action);
  h.i("aof", Globs_pObjectModule && Globs_pObjectModule->IsValidObject(a->m_aware.m_awareOf)
                 ? a->m_aware.m_awareOf->GetObjectImplementation()->m_id : -1);
  h.f("atgt", a->m_aware.m_targetAngle);
  h.f("apit", a->m_aware.m_passiveTime);
  h.i("aportal", a->m_portalMode);
  h.i("aportset", a->m_portalModeSet);
  h.i("aautorun", a->m_autoRun);
  h.f("ause", a->m_useSpeed);
  h.i("alowm", a->m_lowMotive);
  h.i("amood", a->m_moodAnim);
}

void HashPerson(Hash& h, cXPersonImpl* p) {
  h.shorts("pdata", p->m_personData, 0x100);
  h.floats("mot", p->m_motives.m_values, Motives::kCount);
  h.floats("mold", p->m_motives.m_old, Motives::kCount);
  h.i("mtick", p->m_lastMotiveTick);
  for (const cXPersonImpl::MotiveInc& inc : p->m_motiveDeltas) {
    h.i("dm", inc.m_motive);
    h.f("dd", inc.m_delta);
    h.f("dl", inc.m_limit);
  }
  const ActionQueue& q = p->m_actionQueue;
  h.i("qn", q.Count());
  h.i("qhead", q.m_head % 10);
  for (unsigned k = 0; k < q.Count(); ++k) {
    const Interaction& a = q.At(k);
    h.i("qtype", a.m_type);
    h.i("qso", ObjId(a.m_stackObject));
    h.i("qio", ObjId(a.m_iconObject));
    h.i("qidx", a.m_index);
    h.shorts("qsv", a.m_stackVars, 4);
    if (!a.m_localVars.empty()) h.shorts("qlv", a.m_localVars.data(), static_cast<int>(a.m_localVars.size()));
    h.i("qpri", a.m_priority);
    h.i("qtree", a.m_tree);
    h.f("qatt", a.m_attenuation);
    h.i("qid", a.m_id);
    h.i("qfl", a.m_flags);
  }
  h.i("qcur", p->m_hasCurrentAction);
  h.i("idlec", p->m_idleAnimateCount);
  h.floats("pos", p->m_pos, 2);
  h.i("dir", p->m_dir);
  h.i("routes", static_cast<long long>(p->m_routes.size()));
  for (const XRoute& r : p->m_routes) {
    h.i("rgoals", static_cast<long long>(r.m_goals.size()));
    h.i("rgoal", r.m_goal);
    h.i("rstuck", r.m_stuck);
    h.i("rwait", r.m_waitStart);
    h.i("rportal", ObjId(r.m_portal));
    h.i("rasked", ObjId(r.m_asked));
    h.i("rfail", r.m_failure);
    h.i("rplan", r.m_planned);
  }
  h.i("path", static_cast<long long>(p->m_path.m_points.size()));
  for (const TileListPoint& t : p->m_path.m_points) {
    h.i("p0", t.p[0]);
    h.i("p1", t.p[1]);
  }
  h.i("ignobj", ObjId(p->m_ignoringObject));
  h.i("soc1", p->m_firstInSocialMode);
  h.i("soc2", p->m_secondInSocialMode);
  h.i("socw", p->m_waitingForSocialMode);
  h.i("ngh", p->m_neighbor ? p->m_neighbor->m_id : -1);
  HashAnimator(h, p->m_animator);
}

void HashObject(Hash& h, cXObjectImpl* o) {
  h.i("type", o->GetType());
  if (o->m_attrs && o->m_attrCount > 0) h.shorts("attr", o->m_attrs, o->m_attrCount);
  if (o->m_sprites && o->m_spriteCount > 0) h.shorts("spr", o->m_sprites, o->m_spriteCount);
  h.shorts("tmp", o->m_temps, 8);
  h.i("dk0", o->m_data.m_keys[0]);
  h.i("dk1", o->m_data.m_keys[1]);
  h.i("dk2", o->m_data.m_keys[2]);
  h.shorts("data", o->m_data.m_values, EdithVariableSet::kCount);
  h.i("lx", o->m_location[0]);
  h.i("ly", o->m_location[1]);
  h.i("lvl", o->m_level);
  h.i("misc", o->m_miscFlags);
  if (!o->m_slotContents.empty()) h.shorts("slots", o->m_slotContents.data(), static_cast<int>(o->m_slotContents.size()));
  h.i("errs", o->m_errorRepeats);
  if (auto* mt = dynamic_cast<cXMTObjectImpl*>(o))
    if (!mt->m_portalValues.empty()) h.floats("proute", mt->m_portalValues.data(), static_cast<int>(mt->m_portalValues.size()));
  ObjectModuleImpl* m = Globs_pObjectModule;
  h.i("sf1", m->GetSimFlag(o->m_id, ObjectModule__SimFlag{1}));
  h.i("sf2", m->GetSimFlag(o->m_id, ObjectModule__SimFlag{2}));
  h.i("sf4", m->GetSimFlag(o->m_id, ObjectModule__SimFlag{4}));
  h.i("idle", m->GetIdleStatus(o->m_id));
  HashTree(h, o->m_treeSim);
  HashRelations(h, o->m_relations);
}

void HashNeighbor(Hash& h, Neighbor* n) {
  h.i("guid", n->m_guid);
  h.shorts("pers", n->m_pers, 0x50);
  h.floats("nmot", n->m_motives, 0x10);
  h.i("fr", n->m_friendCount);
  h.i("en", n->m_enemyCount);
  h.i("rep", n->m_reputation);
  for (const Neighbor::Bookmark& b : n->m_bookmarks) {
    h.i("bt", b.m_tree);
    h.i("bg", b.m_field02);
    h.i("bn", b.m_node);
    h.i("bc", b.m_field08);
  }
  for (int a : n->m_active) h.i("act", a);
  HashRelations(h, n->m_relations);
}

// Writes the tick's hash line (and dump lines).
// PC mod: network play: `net` (not null) also gets the parts and the tick's dump.
void HashTick(DetState& s, NetHash* net = nullptr) {
  const bool window = s.dump && s.tick >= s.dumpFrom && s.tick <= s.dumpTo;
  const bool fileDump = window || (s.dump && !s.ref.empty() && (!s.divergedAt || s.tick <= s.divergedAt + s.refAfter));
  const bool dumping = fileDump || net;
  std::string line;
  std::string& out = s.curDump;
  out.clear();
  const char* lineWhat = "";
  int lineId = 0;
  auto begin = [&](Hash& h, const char* what, int id) {
    lineWhat = what, lineId = id;
    if (!dumping) return;
    line.clear();
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d %s %d:", s.tick, what, id);
    line = buf;
    h.line = &line;
  };
  auto end = [&](Hash& h) {
    if (s.lines) std::fprintf(s.lines, "%d %s %d %016llx\n", s.tick, lineWhat, lineId, h.h);
    if (!dumping) return;
    out += line;
    out += '\n';
    h.line = nullptr;
  };
  Hash rng, sim, obj, per, ngh, inst;
  begin(rng, "rng", 0);
  rng.i("srand", GetSRandSeed());  // the tree RNG outside ticks (the outer seed)
  rng.i("tick", Globs_pSimulator ? Globs_pSimulator->m_randSeed : 0);  // the ticks' own tree RNG
  rng.i("simrand", sim_rand_state());
  end(rng);
  if (s.lines)
    std::fprintf(s.lines, "%d rngv srand=%u tick=%u simrand=%u ui=%u\n", s.tick, GetSRandSeed(),
                 Globs_pSimulator ? Globs_pSimulator->m_randSeed : 0u, sim_rand_state(), ui_rand_state());
  if (dumping || s.lines) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%d uirng 0: ui=%u psys=%u (not hashed)\n", s.tick, ui_rand_state(), psystem_rand_state());
    out += buf;
    // The UI's state (not hashed): frame, Apt time, dt, the managed screens, the dialog, the pause count.
    char ui[512];
    int n = std::snprintf(ui, sizeof ui, "%d ui 0: frame=%d apt=%d dt=%.9g pause=%d screens", s.tick, _framecount,
                          Timer_GetTotalTicks(), static_cast<double>(_dt), H2DTarget::m_nPauseRefCount);
    if (auto* mgr = static_cast<UIScreenManager*>(_globals.m_uiScreenManager))
      for (const auto& r : mgr->m_records)
        if (r.id && n < 400) n += std::snprintf(ui + n, sizeof ui - n, " %d:%d", r.id, r.state);
    if (auto* g2d = static_cast<G2DTarget*>(_globals.m_g2dTarget))
      std::snprintf(ui + n, sizeof ui - n, " g2d=%d/%d/%d/%d/%d t=%.9g", g2d->m_spawned, g2d->m_active, g2d->m_visible,
                    g2d->m_state, g2d->m_selection, static_cast<double>(g2d->m_timer));
    if (dumping) {
      out += ui;
      out += '\n';
    }
    if (s.lines) std::fprintf(s.lines, "%s\n", ui);
  }
  if (cSimulatorImpl* m = Globs_pSimulator) {
    begin(sim, "sim", 0);
    sim.shorts("glob", m->m_globals, cSimulatorImpl::kCount);
    sim.i("funds", m->m_funds);
    sim.i("pend", m->m_pendingFunds);
    sim.i("ticks", m->m_ticks);
    sim.i("sadj", m->m_secondsAdjust);
    sim.i("lot", m->m_lotValue);
    sim.i("ov", m->m_objectsValue);
    sim.i("av", m->m_archValue);
    sim.i("stall", m->m_stalled);
    sim.f("acc", m->m_msAccumulated);
    sim.f("spd", m->m_speedMultiplier);
    sim.f("bt", m->m_boostTime);
    sim.i("tobj", m->m_tickObject);
    end(sim);
  }
  if (ObjectModuleImpl* module = Globs_pObjectModule) {
    begin(obj, "objs", 0);
    obj.i("n", module->GetNumObjects());
    end(obj);
    for (cXObjectImpl* o = module->m_first; o; o = o->m_nextObject) {
      begin(obj, "obj", o->m_id);
      obj.i("id", o->m_id);
      HashObject(obj, o);
      end(obj);
      if (auto* p = o->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(o) : nullptr) {
        begin(per, "per", o->m_id);
        per.i("id", o->m_id);
        HashPerson(per, p);
        end(per);
      }
      if (o->m_instance) {
        begin(inst, "inst", o->m_id);
        inst.i("id", o->m_id);
        inst.floats("m", &o->m_instance->m_orient.m[0][0], 16);
        end(inst);
      }
    }
  }
  if (NeighborhoodImpl* n = Globs_pNeighborhood) {
    for (Neighbor* nb : n->m_neighbors) {
      if (!nb) continue;
      begin(ngh, "ngh", nb->m_id);
      HashNeighbor(ngh, nb);
      end(ngh);
    }
  }
  Hash all;
  all.word(rng.h), all.word(sim.h), all.word(obj.h), all.word(per.h), all.word(ngh.h), all.word(inst.h);
  if (!s.ref.empty() && !s.divergedAt && s.tick <= static_cast<int>(s.ref.size()) &&
      s.ref[static_cast<size_t>(s.tick - 1)] != all.h) {
    s.divergedAt = s.tick;
    std::fprintf(stderr, "[det] DIVERGED from the reference at tick %d\n", s.tick);
    if (fileDump && !window) std::fputs(s.prevDump.c_str(), s.dump);
    // SIMMY_DET_REF_QUIT=1: stop SIMMY_DET_REF_AFTER ticks later.
    if (Env("SIMMY_DET_REF_QUIT")) s.quitTick = s.tick + s.refAfter;
  }
  if (net) {
    net->total = all.h;
    const unsigned long long parts[6] = {rng.h, sim.h, obj.h, per.h, ngh.h, inst.h};
    for (int k = 0; k < 6; ++k) net->part[k] = parts[k];
    net->dump = out;
  }
  if (fileDump) {
    if (window || s.divergedAt) std::fputs(out.c_str(), s.dump);
    s.prevDump.swap(s.curDump);
  }
  if (s.hash)
    std::fprintf(s.hash, "%d %016llx rng=%016llx sim=%016llx obj=%016llx per=%016llx ngh=%016llx inst=%016llx\n", s.tick,
                 all.h, rng.h, sim.h, obj.h, per.h, ngh.h, inst.h);
}

// ---- input --------------------------------------------------------------------------------------
void WritePads(DetState& s, int tick, const PADStatus* pads) {
  std::fprintf(s.record, "%d", tick);
  for (int p = 0; p < 4; ++p) {
    const PADStatus& q = pads[p];
    std::fprintf(s.record, " %d,%x,%d,%d,%d,%d,%u,%u,%u,%u,%x", q.err, q.button, q.stickX, q.stickY, q.substickX, q.substickY,
                 q.triggerLeft, q.triggerRight, q.analogA, q.analogB, static_cast<unsigned>(q.extButton));
  }
  // The keyboard-and-mouse record of the frame (pc_kbm.cpp), when there is one: " m flags,cx,cy,rot,zoom,
  // panx,pany", the floats in hexadecimal (%a), exact.
  const KbmTick& m = kbm_tick();
  if (!(m == KbmTick{}))
    std::fprintf(s.record, " m %x,%a,%a,%a,%a,%a,%a", m.flags, static_cast<double>(m.cx), static_cast<double>(m.cy),
                 static_cast<double>(m.rot), static_cast<double>(m.zoom), static_cast<double>(m.panX), static_cast<double>(m.panY));
  // PC mod: network play: pad port 2's (the joining PC's keyboard and mouse), " n ...".
  const KbmTick& m1 = kbm_tick_port(1);
  if (!(m1 == KbmTick{}))
    std::fprintf(s.record, " n %x,%a,%a,%a,%a,%a,%a", m1.flags, static_cast<double>(m1.cx), static_cast<double>(m1.cy),
                 static_cast<double>(m1.rot), static_cast<double>(m1.zoom), static_cast<double>(m1.panX), static_cast<double>(m1.panY));
  std::fprintf(s.record, "\n");
}

bool ReadPads(DetState& s, int tick, PADStatus* pads) {
  char buf[512];
  while (!s.replayDone) {
    if (!std::fgets(buf, sizeof buf, s.replay)) {
      s.replayDone = true;
      std::fprintf(stderr, "[det] replay file ends before tick %d\n", tick);
      break;
    }
    char* p = buf;
    const int t = static_cast<int>(std::strtol(p, &p, 10));
    if (t < tick) continue;
    if (t > tick) {
      std::fprintf(stderr, "[det] replay file has no tick %d\n", tick);
      return false;
    }
    for (int k = 0; k < 4; ++k) {
      int v[11] = {};
      for (int j = 0; j < 11; ++j) {
        while (*p == ' ' || *p == ',') ++p;
        v[j] = static_cast<int>(std::strtol(p, &p, (j == 1 || j == 10) ? 16 : 10));
      }
      PADStatus& q = pads[k];
      q.err = static_cast<s8>(v[0]);
      q.button = static_cast<u16>(v[1]);
      q.stickX = static_cast<s8>(v[2]);
      q.stickY = static_cast<s8>(v[3]);
      q.substickX = static_cast<s8>(v[4]);
      q.substickY = static_cast<s8>(v[5]);
      q.triggerLeft = static_cast<u8>(v[6]);
      q.triggerRight = static_cast<u8>(v[7]);
      q.analogA = static_cast<u8>(v[8]);
      q.analogB = static_cast<u8>(v[9]);
      q.extButton = static_cast<u32>(v[10]);
    }
    kbm_tick_port_for_det(0) = KbmTick{};
    kbm_tick_port_for_det(1) = KbmTick{};
    for (int port = 0; port < 2; ++port) {
      while (*p == ' ') ++p;
      if (*p != (port == 0 ? 'm' : 'n')) continue;  // " m" port 1's record, " n" port 2's (network play)
      KbmTick& m = kbm_tick_port_for_det(port);
      ++p;
      m.flags = static_cast<unsigned>(std::strtoul(p, &p, 16));
      float* f[6] = {&m.cx, &m.cy, &m.rot, &m.zoom, &m.panX, &m.panY};
      for (float* x : f) {
        while (*p == ' ' || *p == ',') ++p;
        *x = static_cast<float>(std::strtod(p, &p));
      }
    }
    return true;
  }
  return false;
}
}  // namespace

bool det_timing() { return St().on; }

// The virtual clock in nanoseconds: whole retraces of 1/60 s (1e9 / 60 = 50,000,000 / 3).
unsigned long long det_clock_ns() { return St().frames * 50000000ull / 3ull; }

// Wall clock or, with SIMMY_DET, the virtual clock (nanoseconds since an arbitrary start).
unsigned long long game_clock_ns() {
  if (St().on) return det_clock_ns();
  return static_cast<unsigned long long>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

// EEngine::PreFrameUpdate, first thing: a new frame of game time.
void det_frame_begin() {
  DetState& s = St();
  if (s.on) ++s.frames;
}

// The live mode updated a house this frame (ELiveMode::Update).
void det_live_update() { St().liveRan = true; }

// ENgcControllerManager::Update, after the scripts: record or replace the tick's pads.
void det_pads(PADStatus* pads) {
  DetState& s = St();
  if (!s.tick && !s.boot) return;
  const int tick = s.tick + 1;  // the frame starting now
  if (net_pads(tick, pads)) {
    // PC mod: network play: both PCs' input of this tick (pc_net.cpp).
  } else if (s.replay) {
    if (s.replayDone || !ReadPads(s, tick, pads)) {
      for (int k = 0; k < 4; ++k) {
        pads[k] = PADStatus{};
        pads[k].err = k == 0 ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER;
      }
      kbm_tick_port_for_det(0) = KbmTick{};
      kbm_tick_port_for_det(1) = KbmTick{};
    }
  }
  if (s.record) WritePads(s, tick, pads);
}

// PC mod: network play (pc_net.cpp): a session runs as a SIMMY_DET_BOOT replay does (virtual timing,
// ticks from boot) and records its input (the agreed input of both PCs: a replay file for the session).
void det_net_enable(FILE* record, const std::string& recordPath) {
  DetState& s = St();
  s.on = true;
  s.boot = true;
  if (!s.record && record) s.record = record;
  else if (record) std::fclose(record);  // (SIMMY_DET_RECORD is already open)
  std::fprintf(stderr, "[det] network play: virtual timing from boot, input recorded to %s\n",
               record ? recordPath.c_str() : "SIMMY_DET_RECORD's file, if any");
}

bool det_replaying() { return St().replay != nullptr && St().tick > 0; }

// platform_frame_tick, before the pacing: count the tick, hash, jitter. False: pace as usual.
bool det_frame_end() {
  DetState& s = St();
  if (s.liveRan || s.tick || s.boot) {
    ++s.tick;
    if (s.liveRan && !s.liveTick) {
      s.liveTick = s.tick;
      std::fprintf(stderr, "[det] tick %d (the house's first) at frame %d\n", s.tick, _framecount);
      if (const char* sp = Env("SIMMY_DET_SPEED"); sp && Globs_pSimulator) Globs_pSimulator->SetSpeed(SimSpeed{std::atoi(sp)});
    }
    s.liveRan = false;
    // PC mod: network play: every hash_every ticks the state hash (and its dump) goes to the partner.
    const bool netHash = net_wants_hash(s.tick);
    NetHash nh;
    if (s.hash || s.dump || s.lines || netHash) HashTick(s, netHash ? &nh : nullptr);
    net_frame_end(s.tick, netHash ? &nh : nullptr);
    if (s.tick % 3000 == 0) {
      if (s.hash) std::fflush(s.hash);
      if (s.record) std::fflush(s.record);
      std::fprintf(stderr, "[det] tick %d\n", s.tick);
    }
    if (s.quitTick && s.tick == s.quitTick) {
      if (s.hash) std::fflush(s.hash);
      if (s.record) std::fflush(s.record);
      if (s.dump) std::fflush(s.dump);
      if (s.lines) std::fflush(s.lines);
      platform_request_quit();
    }
  }
  if (s.jitter > 0) std::this_thread::sleep_for(std::chrono::milliseconds(s.jitterRng() % static_cast<unsigned>(s.jitter + 1)));
  return s.noPace;
}

// ---- the game's rand streams --------------------------------------------------------------------
// GC: sn/libc rand.obj (srand 802432A4, rand 802432B0): state = state * 1103515245 + 12345, the result
// its low 31 bits (RAND_MAX 0x7FFFFFFF). On GC one stream served everything (the particle system,
// the UI, the simulation) and ParticleSysOpen (803037DC) seeded it from the clock. The port uses the
// GC algorithm (the host CRT's rand differs between glibc and MSVC, MSVC's having RAND_MAX 32767) in
// separate streams, so that the simulation's draws do not depend on how much the UI, the sound
// director or the particles drew (which differs between players' machines): the simulation's
// (autonomy, routing, animation choices, wants), and the UI's (Apt, Create-a-Sim, the sound
// director, the unlock display, the menus' highlight refresh). The particle system keeps its own
// (psystem.cpp). The simulation's stream starts at SIMMY_DET_SEED (default 1), as the port's unseeded
// host rand did (GC seeded its one stream from the clock); a netplay host would send the seed.
namespace {
unsigned int& SimRandState() {
  static unsigned int s = Env("SIMMY_DET_SEED") ? static_cast<unsigned int>(std::strtoul(Env("SIMMY_DET_SEED"), nullptr, 0)) : 1u;
  return s;
}
unsigned int g_uiRand = 1;
}  // namespace

int sim_rand() {
  unsigned int& s = SimRandState();
  s = s * 1103515245u + 12345u;
  return static_cast<int>(s & 0x7fffffffu);
}
unsigned int sim_rand_state() { return SimRandState(); }
void set_sim_rand_state(unsigned int state) { SimRandState() = state; }
int ui_rand() {
  g_uiRand = g_uiRand * 1103515245u + 12345u;
  return static_cast<int>(g_uiRand & 0x7fffffffu);
}
unsigned int ui_rand_state() { return g_uiRand; }

}  // namespace simmy
