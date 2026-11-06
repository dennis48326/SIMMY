// Port of the Apt player: AptCIH.cpp, AptDisplayList.cpp, AptMovie.cpp, AptAnimation.cpp (pool and
// timelines), AptRenderingContext.cpp and the update/render entry points of Apt.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

AptAnimationPoolData* gpPool = nullptr;
AptRenderingContext* gpRenderingContext = nullptr;
AptUserFunctions gAptFuncs;

AptObject* AptGetRegisteredClass(const std::string& name);  // aptbuiltins.cpp

namespace {
int s_instanceCount = 0;
AptMatrix Concat(const AptMatrix& p, const AptMatrix& l) {
  AptMatrix m;
  m.a = p.a * l.a + p.c * l.b;
  m.b = p.b * l.a + p.d * l.b;
  m.c = p.a * l.c + p.c * l.d;
  m.d = p.b * l.c + p.d * l.d;
  m.tx = p.a * l.tx + p.c * l.ty + p.tx;
  m.ty = p.b * l.tx + p.d * l.ty + p.ty;
  return m;
}

AptCXForm Concat(const AptCXForm& p, const AptCXForm& c) {
  AptCXForm r;
  r.mulA = p.mulA * c.mulA;
  r.mulR = p.mulR * c.mulR;
  r.mulG = p.mulG * c.mulG;
  r.mulB = p.mulB * c.mulB;
  r.addA = p.mulA * c.addA + p.addA;
  r.addR = p.mulR * c.addR + p.addR;
  r.addG = p.mulG * c.addG + p.addG;
  r.addB = p.mulB * c.addB + p.addB;
  return r;
}

// _convertToCXForm (8029A1A4): packed ARGB bytes, scaled by 1/255.
AptCXForm UnpackCXForm(unsigned int mul, unsigned int add) {
  AptCXForm c;
  c.mulA = static_cast<float>(mul >> 24) / 255.0f;
  c.mulR = static_cast<float>(mul >> 16 & 0xff) / 255.0f;
  c.mulG = static_cast<float>(mul >> 8 & 0xff) / 255.0f;
  c.mulB = static_cast<float>(mul & 0xff) / 255.0f;
  c.addA = static_cast<float>(add >> 24) / 255.0f;
  c.addR = static_cast<float>(add >> 16 & 0xff) / 255.0f;
  c.addG = static_cast<float>(add >> 8 & 0xff) / 255.0f;
  c.addB = static_cast<float>(add & 0xff) / 255.0f;
  return c;
}

void ExpandRect(AptRect& r, bool& empty, const AptMatrix& m, const AptRect& local) {
  const float xs[2] = {local.left, local.right}, ys[2] = {local.top, local.bottom};
  for (float x : xs)
    for (float y : ys) {
      const float px = m.a * x + m.c * y + m.tx, py = m.b * x + m.d * y + m.ty;
      if (empty) {
        r = {px, py, px, py};
        empty = false;
      } else {
        r.left = std::min(r.left, px);
        r.top = std::min(r.top, py);
        r.right = std::max(r.right, px);
        r.bottom = std::max(r.bottom, py);
      }
    }
}

// The bounds of an instance's content in the space `m` maps it to (AptCIH::_getBoundingRect).
void ContentBounds(AptCIH* inst, const AptMatrix& m, AptRect& r, bool& empty) {
  AptCharacter* ch = inst->m_character;
  if (!ch) return;
  switch (ch->type) {
    case AptCharacter::kShape: ExpandRect(r, empty, m, static_cast<AptCharacterShape*>(ch)->bounds); break;
    case AptCharacter::kEditText: ExpandRect(r, empty, m, inst->m_textBounds); break;
    case AptCharacter::kStaticText: ExpandRect(r, empty, m, static_cast<AptCharacterStaticText*>(ch)->bounds); break;
    default:
      for (AptCIH* child : inst->m_children.m_list)
        if (child->m_clipDepth < 0) ContentBounds(child, Concat(m, child->m_matrix), r, empty);
      break;
  }
}

float Length(float x, float y) { return std::sqrt(x * x + y * y); }
}  // namespace

// ---- AptRenderingContext ------------------------------------------------------------------------------

void AptRenderingContext::Reset() {
  m_matrix.assign(1, AptMatrix{});
  m_colour.assign(1, AptCXForm{});
}
void AptRenderingContext::pushVertexMatrix() { m_matrix.push_back(m_matrix.back()); }
void AptRenderingContext::appendVertexMatrix(const AptMatrix& m) { m_matrix.back() = Concat(m_matrix.back(), m); }
void AptRenderingContext::popVertexMatrix() {
  if (m_matrix.size() > 1) m_matrix.pop_back();
}
void AptRenderingContext::pushColourTransform() { m_colour.push_back(m_colour.back()); }
void AptRenderingContext::appendColourTransform(const AptCXForm& c) { m_colour.back() = Concat(m_colour.back(), c); }
void AptRenderingContext::popColourTransform() {
  if (m_colour.size() > 1) m_colour.pop_back();
}

// ---- AptDisplayList -----------------------------------------------------------------------------------

AptCIH* AptDisplayList::find(int depth) const {
  for (AptCIH* c : m_list)
    if (c->m_depth == depth) return c;
  return nullptr;
}
AptCIH* AptDisplayList::findByName(const std::string& name) const {
  for (AptCIH* c : m_list)
    if (c->m_name == name) return c;
  return nullptr;
}
void AptDisplayList::insert(AptCIH* inst) {
  auto it = std::lower_bound(m_list.begin(), m_list.end(), inst,
                             [](const AptCIH* a, const AptCIH* b) { return a->m_depth < b->m_depth; });
  m_list.insert(it, inst);
}
void AptDisplayList::remove(AptCIH* inst) {
  auto it = std::find(m_list.begin(), m_list.end(), inst);
  if (it != m_list.end()) m_list.erase(it);
}
void AptDisplayList::clear() {
  for (AptCIH* c : m_list) c->Remove();
  m_list.clear();
}

// 8029B7BC
// Ticks the sprites and buttons of the list (a copy: ticking may change the list).
void AptDisplayList::tick() {
  std::vector<AptCIH*> list = m_list;
  for (AptCIH* c : list)
    if (!c->m_removed) c->tick();
}

// 8029B4A4
// Draws in depth order. A mask (clip depth >= 0) is drawn into the stencil (operation 1) and
// drawn again with -1 once an instance above its clip depth is reached (or at the end).
void AptDisplayList::render(AptRenderingContext* rc, int maskOp) {
  std::vector<AptCIH*> masks;
  auto draw = [&](AptCIH* inst, int op) {
    rc->pushColourTransform();
    rc->appendColourTransform(inst->m_cxform);
    rc->pushVertexMatrix();
    rc->appendVertexMatrix(inst->m_matrix);
    // PC: widescreen HUD anchoring; in the two-window co-op mode a dialog over the whole stage is drawn
    // into each window (platform.hpp).
    const int passes = simmy_apt_hud_enter(inst, rc->getVertexMatrix());
    for (int pass = 0; pass < passes; ++pass) {
      if (pass) simmy_apt_hud_next_pass();
      inst->render(rc, op);
    }
    simmy_apt_hud_leave();
    rc->popVertexMatrix();
    rc->popColourTransform();
  };
  for (AptCIH* inst : m_list) {
    if (inst->m_clipDepth < 0) {
      while (!masks.empty() && masks.back()->m_clipDepth < inst->m_depth) {
        draw(masks.back(), -1);
        masks.pop_back();
      }
      draw(inst, maskOp);
    } else {
      masks.push_back(inst);
      std::sort(masks.begin(), masks.end(), [](AptCIH* a, AptCIH* b) { return a->m_clipDepth > b->m_clipDepth; });
      draw(inst, 1);
    }
  }
  while (!masks.empty()) {
    draw(masks.back(), -1);
    masks.pop_back();
  }
}

// ---- AptCIH -------------------------------------------------------------------------------------------

AptCIH::AptCIH(AptCharacter* character, AptCIH* parent, AptFileImage* file)
    : AptObject(kCIH), m_character(character), m_parent(parent), m_file(file) {}

void AptCIH::RegisterReferences() const {
  AptObject::RegisterReferences();
  for (AptCIH* c : m_children.m_list) AptGC::Mark(c);
  AptGC::Mark(m_parent);
  AptGC::Mark(m_textFormat);
  AptGC::Mark(m_hitArea);
}

bool AptCIH::IsSprite() const {
  return m_character && (m_character->type == AptCharacter::kSprite || m_character->type == AptCharacter::kMovie);
}
const AptMovie* AptCIH::Timeline() const {
  if (!m_character) return nullptr;
  if (m_character->type == AptCharacter::kSprite) return &static_cast<AptCharacterSprite*>(m_character)->timeline;
  if (m_character->type == AptCharacter::kMovie) return &static_cast<AptCharacterMovie*>(m_character)->animation.timeline;
  return nullptr;
}
AptCharacterAnimation* AptCIH::Animation() const {
  if (!m_character || !m_character->movie) return nullptr;
  return &static_cast<AptCharacterMovie*>(m_character->movie)->animation;
}
AptCIH* AptCIH::GetRoot() {
  AptCIH* c = this;
  while (c->m_parent && c->m_level < 0) c = c->m_parent;
  return c;
}

// AptActionInterpreter::getName (8026F824): "_level0.a.b".
std::string AptCIH::GetTargetPath() {
  if (m_level >= 0 || !m_parent) return "_level" + std::to_string(std::max(0, m_level));
  return m_parent->GetTargetPath() + "." + m_name;
}

// AptMovie::labelToFrame (802A9848): frame labels (case-sensitive).
int AptCIH::labelToFrame(const std::string& label) const {
  const AptMovie* tl = Timeline();
  if (!tl) return -1;
  for (int f = 0; f < tl->frameCount; ++f)
    for (int i = 0; i < tl->frames[f].controlCount; ++i) {
      const AptControl* c = tl->frames[f].controls[i];
      if (c->type == AptControl::kFrameLabel && static_cast<const AptControlFrameLabel*>(c)->label &&
          label == static_cast<const AptControlFrameLabel*>(c)->label)
        return f;
    }
  return -1;
}

// AptCharacterAnimation::ExecuteInitActions (80284880): runs a sprite's #initclip blocks once,
// before its first instance is placed.
void AptCIH::runInitActions(int sprite) {
  AptCharacterAnimation* anim = Animation();
  if (!anim) return;
  for (int f = 0; f < anim->timeline.frameCount; ++f)
    for (int i = 0; i < anim->timeline.frames[f].controlCount; ++i) {
      AptControl* c = anim->timeline.frames[f].controls[i];
      if (c->type != AptControl::kInitAction) continue;
      auto* ia = static_cast<AptControlInitAction*>(c);
      if (ia->sprite == sprite && ia->sprite >= 0) {
        ia->sprite = -ia->sprite - 1;  // GC negated the id (id 0 never has init actions)
        gAptActionInterpreter.runStream(ia->stream, m_file, GetRoot());
      }
    }
}

// AptMovie::doFrameControls (802A94AC): init actions of the frame first, then removes, places,
// sounds and the background colour.
void AptCIH::doFrameControls(int frame) {
  const AptMovie* tl = Timeline();
  if (!tl || frame < 0 || frame >= tl->frameCount) return;
  const AptFrame& fr = tl->frames[frame];
  for (int i = 0; i < fr.controlCount; ++i) {
    auto* c = fr.controls[i];
    if (c->type == AptControl::kInitAction) {
      auto* ia = static_cast<AptControlInitAction*>(c);
      if (ia->sprite >= 0) {
        ia->sprite = -ia->sprite - 1;
        gAptActionInterpreter.runStream(ia->stream, m_file, GetRoot());
      }
    }
  }
  static bool s_backgroundSet = false;
  for (int i = 0; i < fr.controlCount && !m_removed; ++i) {
    const AptControl* c = fr.controls[i];
    switch (c->type) {
      case AptControl::kRemoveObject: removeObject(static_cast<const AptControlRemoveObject2*>(c)->depth); break;
      case AptControl::kPlaceObject: {
        auto* p = static_cast<const AptControlPlaceObject2*>(c);
        if (p->flags & 2) runInitActions(p->character);
        placeObject(p);
        break;
      }
      case AptControl::kBackgroundColour:
        if (!s_backgroundSet && gAptFuncs.setBackgroundColour) {
          gAptFuncs.setBackgroundColour(static_cast<const AptControlBackgroundColour*>(c)->colour);
          s_backgroundSet = true;
        }
        break;
      default: break;
    }
  }
}

// AptMovie::queueFrameActions (802A97AC); `marker` is the CIH's +0x28 frame marker the queue
// entries take (-frame when the timeline advances in tick, else the frame or 0).
void AptCIH::queueFrameActions(int frame, int marker) {
  const AptMovie* tl = Timeline();
  if (!tl || frame < 0 || frame >= tl->frameCount) return;
  const AptFrame& fr = tl->frames[frame];
  for (int i = 0; i < fr.controlCount; ++i)
    if (fr.controls[i]->type == AptControl::kAction)
      gpPool->addAction(static_cast<const AptControlAction*>(fr.controls[i])->stream, m_file, this, marker);
}

namespace {
// What class an exported character was registered with (Object.registerClass).
AptObject* RegisteredClass(AptCIH* inst) {
  AptCharacterAnimation* anim = inst->Animation();
  if (!anim) return nullptr;
  for (int i = 0; i < anim->exportCount; ++i) {
    if (anim->exports[i].character < 0 || anim->exports[i].character >= anim->characterCount) continue;
    if (anim->characters[anim->exports[i].character] != inst->m_character) continue;

    if (AptObject* cls = AptGetRegisteredClass(anim->exports[i].name)) return cls;
  }
  return nullptr;
}
}  // namespace


// The prototype an instance starts with: MovieClip, TextField or a registered class.
void AptAssignPrototype(AptCIH* inst) {
  AptActionInterpreter& in = gAptActionInterpreter;
  const char* cls = inst->IsText() ? "TextField" : "MovieClip";
  if (AptObject* ctor = gpGlobalGlobalObject->Get(in, cls).AsObject()) inst->m_proto = ctor->Get(in, "prototype").AsObject();
}

// AptDisplayList::instantiateCharacter (80299970): makes the instance, names it, sets it up by
// type, inserts it, associates a registered class (AptCIH::associateInstToClass 802922FC) and runs
// the new timeline's first frame.
AptCIH* AptCreateInstance(AptCIH* parent, AptCharacter* ch, int depth, const std::string& name,
                          const std::function<void(AptCIH*)>& setup) {
  auto* inst = new AptCIH(ch, parent, parent ? parent->m_file : nullptr);
  inst->m_depth = depth;
  inst->m_name = name;
  if (setup) setup(inst);
  AptAssignPrototype(inst);
  if (ch && ch->type == AptCharacter::kEditText) {
    auto* t = static_cast<AptCharacterEditText*>(ch);
    inst->m_textBounds = t->bounds;
    inst->m_textColour = t->colour;
    inst->m_textSize = t->fontHeight;
    inst->m_textAlign = t->alignment;
    inst->m_multiline = t->multiline != 0;
    inst->m_wordWrap = t->wordWrap != 0;
    inst->m_variable = t->variable ? t->variable : "";
    inst->m_text = t->text ? t->text : "";
  }
  if (parent) {
    if (AptCIH* old = parent->m_children.find(depth)) parent->removeObject(depth), (void)old;
    parent->m_children.insert(inst);
  }
  if (inst->IsText() && !inst->m_variable.empty() && inst->m_variable[0] != '$') {
    // AptCharacterTextInst::SetText (802883AC): bind to the variable; an undefined variable takes
    // the field's initial text.
    AptCIH* owner = parent;
    while (owner && !owner->IsSprite()) owner = owner->m_parent;
    AptActionInterpreter& in = gAptActionInterpreter;
    AptActionInterpreter::Context ctx;
    ctx.target = owner;
    AptActionInterpreter::Context* saved = in.m_ctx;
    in.m_ctx = &ctx;
    AptVar v = in.getVariable(inst->m_variable);
    if (v.IsUndefined()) in.setVariable(inst->m_variable, AptVar::Str(inst->m_text));
    else inst->m_text = v.ToString();
    in.m_ctx = saved;
  }
  // AptDisplayList::_addToSetCaches (8029A630): initialize and construct clip events (component
  // parameters) run before the class constructor.
  inst->queueClipEvent(0x200);
  inst->queueClipEvent(0x40000);
  if (AptObject* cls = RegisteredClass(inst)) {
    AptActionInterpreter& in = gAptActionInterpreter;
    inst->m_proto = cls->Get(in, "prototype").AsObject();
    in.callFunction(AptVar::Obj(cls), AptVar::Obj(inst), {});
  }
  if (inst->IsSprite()) {
    inst->m_frame = 0;
    inst->doFrameControls(0);
    inst->queueFrameActions(0);
  }
  inst->queueClipEvent(1);  // load
  return inst;
}

// AptDisplayList::placeObject (8029A9E0): a new instance (flag 2) or changes to the one at the
// depth (flag 1). Instances changed by script keep their properties.
void AptCIH::placeObject(const AptControlPlaceObject2* p) {
  AptCharacterAnimation* anim = Animation();
  if (p->flags & 2) {
    if (!anim || p->character < 0 || p->character >= anim->characterCount) return;
    AptCharacter* ch = anim->characters[p->character];
    if (!ch) return;
    AptCIH* existing = m_children.find(p->depth);
    if ((p->flags & 1) && existing && existing->m_character == ch) {
      // replace with the same character: keep the instance
    } else {
      std::string name = p->name ? p->name : "";
      if (name.empty()) name = "instance" + std::to_string(++s_instanceCount);
      AptCreateInstance(this, ch, p->depth, name, [&](AptCIH* inst) {
        inst->m_matrix = p->matrix;
        if (p->flags & 8) inst->m_cxform = UnpackCXForm(p->colourMul, p->colourAdd);
        inst->m_ratio = p->ratio;
        inst->m_clipDepth = (p->flags & 0x40) ? p->clipDepth : -1;
        inst->m_clipActions = (p->flags & 0x80) ? p->clipActions : nullptr;
      });
      return;
    }
  }
  AptCIH* inst = m_children.find(p->depth);
  if (!inst || inst->m_scriptChanged) return;
  if (p->flags & 4) inst->m_matrix = p->matrix;
  if (p->flags & 8) inst->m_cxform = UnpackCXForm(p->colourMul, p->colourAdd);
  if (p->flags & 0x10) inst->m_ratio = p->ratio;
  if (p->flags & 0x40) inst->m_clipDepth = p->clipDepth;
}

// AptDisplayList::removeObject (8029AEC4)
void AptCIH::removeObject(int depth) {
  AptCIH* inst = m_children.find(depth);
  if (!inst) return;
  m_children.remove(inst);
  inst->Remove();
}

// 8028FEC8: unload event, then the instance and its children leave the stage.
void AptCIH::Remove() {
  if (m_removed) return;
  extern bool s_traceOps;
  if (s_traceOps) std::fprintf(stderr, "[apt] remove %s depth %d\n", GetTargetPath().c_str(), m_depth);
  queueClipEvent(4);  // unload (SWF clip event bit 2)
  m_removed = true;
  // PC: a loaded movie's root clip leaving (a level, or a clip showing another file than its parent's):
  // AptUpdate collects at the end of the tick, so the movie gives its aux level back before the next
  // screen loads its own (GC: the AptFile went with its last reference at once).
  if (m_file && (m_level >= 0 || !m_parent || m_parent->m_file != m_file)) ++gAptUnloadsSinceCollect;
  for (AptCIH* c : m_children.m_list) c->Remove();
  ReleaseText();
}

void AptCIH::ReleaseText() {
  if (m_textHandle && gAptFuncs.deallocateString) gAptFuncs.deallocateString(m_textHandle, 0);
  m_textHandle = nullptr;
}

// 80292994
// The next frame plays its controls; any other frame rebuilds the display list from the first
// frame (or the current one, going forward) up to the target, keeping instances that are still
// there with the same character, then queues the target frame's actions.
void AptCIH::jumpToFrame(int frame) {
  const AptMovie* tl = Timeline();
  if (!tl || frame < 0 || frame >= tl->frameCount || frame == m_frame) return;
  if (frame == m_frame + 1) {
    m_frame = frame;
    doFrameControls(frame);
  } else {
    // AptMovie::DoTemporaryFrameControls + AptDisplayList::mergeState
    struct Pseudo {
      int character;
      AptMatrix matrix;
      bool hasCX = false;
      unsigned int mul = 0xffffffff, add = 0;
      float ratio = 0;
      const char* name = nullptr;
      int clipDepth = -1;
      const AptClipActions* clip = nullptr;
    };
    std::vector<std::pair<int, Pseudo>> state;
    auto find = [&](int depth) -> Pseudo* {
      for (auto& s : state)
        if (s.first == depth) return &s.second;
      return nullptr;
    };
    int start = 0;
    if (frame > m_frame) {
      start = m_frame + 1;
      for (AptCIH* c : m_children.m_list) {
        if (c->m_placedByScript) continue;
        Pseudo p;
        p.character = -2;  // existing instance
        p.matrix = c->m_matrix;
        p.ratio = c->m_ratio;
        state.emplace_back(c->m_depth, p);
      }
    }
    for (int f = start; f <= frame; ++f) {
      const AptFrame& fr = tl->frames[f];
      for (int i = 0; i < fr.controlCount; ++i) {
        const AptControl* ctl = fr.controls[i];
        if (ctl->type == AptControl::kRemoveObject) {
          const int d = static_cast<const AptControlRemoveObject2*>(ctl)->depth;
          state.erase(std::remove_if(state.begin(), state.end(), [d](auto& s) { return s.first == d; }), state.end());
        } else if (ctl->type == AptControl::kPlaceObject) {
          auto* p = static_cast<const AptControlPlaceObject2*>(ctl);
          Pseudo* s = find(p->depth);
          if (p->flags & 2) {
            if (!s || s->character != p->character) {
              state.erase(std::remove_if(state.begin(), state.end(), [p](auto& e) { return e.first == p->depth; }),
                          state.end());
              Pseudo n;
              n.character = p->character;
              state.emplace_back(p->depth, n);
              s = find(p->depth);
            }
            s->name = p->name;
          }
          if (!s) continue;
          if (p->flags & 4) s->matrix = p->matrix;
          if (p->flags & 8) s->hasCX = true, s->mul = p->colourMul, s->add = p->colourAdd;
          if (p->flags & 0x10) s->ratio = p->ratio;
          if (p->flags & 0x40) s->clipDepth = p->clipDepth;
          if (p->flags & 0x80) s->clip = p->clipActions;
        }
      }
    }
    m_frame = frame;
    // merge: remove timeline instances not in the state or with another character
    std::vector<AptCIH*> current = m_children.m_list;
    AptCharacterAnimation* anim = Animation();
    for (AptCIH* c : current) {
      if (c->m_placedByScript) continue;
      Pseudo* s = find(c->m_depth);
      const bool keep = s && (s->character == -2 ||
                              (anim && s->character >= 0 && s->character < anim->characterCount &&
                               anim->characters[s->character] == c->m_character));
      if (!keep) removeObject(c->m_depth);
    }
    for (auto& [depth, s] : state) {
      AptCIH* c = m_children.find(depth);
      if (c) {
        // an instance moved by script keeps what the script set (as placeObject does); the
        // timeline's moves apply to the others, also to instances kept from before the jump
        if (!c->m_scriptChanged) {
          c->m_matrix = s.matrix;
          if (s.hasCX) c->m_cxform = UnpackCXForm(s.mul, s.add);
          c->m_ratio = s.ratio;
        }
        continue;
      }
      if (s.character < 0) continue;
      AptControlPlaceObject2 p;
      p.type = AptControl::kPlaceObject;
      p.flags = 2 | 4 | (s.hasCX ? 8 : 0) | 0x10 | (s.name ? 0x20 : 0) | (s.clipDepth >= 0 ? 0x40 : 0) |
                (s.clip ? 0x80 : 0);
      p.depth = depth;
      p.character = s.character;
      p.matrix = s.matrix;
      p.colourMul = s.mul;
      p.colourAdd = s.add;
      p.ratio = s.ratio;
      p.name = s.name;
      p.clipDepth = s.clipDepth;
      p.clipActions = const_cast<AptClipActions*>(s.clip);
      runInitActions(p.character);
      placeObject(&p);
    }
  }
  queueFrameActions(m_frame, m_frame);  // +0x28 = the frame: always runs
}

// 80293128
// Playing sprites advance a frame (wrapping to the first), then the enterFrame and load events are
// queued and the children tick.
void AptCIH::tick() {
  if (!IsSprite()) {
    m_children.tick();
    return;
  }
  const AptMovie* tl = Timeline();
  if (m_playing && tl && m_loaded) {
    const int next = m_frame + 1;
    if (tl->frameCount == 1) {
      // a one-frame timeline stays put
    } else if (next >= tl->frameCount) {
      jumpToFrame(0);
    } else {
      m_frame = next;
      doFrameControls(next);
      if (!m_removed) queueFrameActions(next, -next);  // dropped if the clip leaves the frame first
    }
  }
  if (m_removed) return;
  queueClipEvent(2);  // enterFrame
  m_loaded = true;
  m_children.tick();
}

// queueClipEvents (80292C94): onClipEvent handlers placed with the instance, and the event
// methods (onLoad, onEnterFrame, onUnload) set by script.
void AptCIH::queueClipEvent(unsigned int event) {
  if (m_clipActions)
    for (int i = 0; i < m_clipActions->count; ++i)
      if (m_clipActions->actions[i].events & event) {
        // initialize (0x200), unload (4) and construct (0x40000) run at once on GC
        if (event == 0x200 || event == 4 || event == 0x40000)
          gAptActionInterpreter.runStream(m_clipActions->actions[i].stream, m_file, this);
        else
          gpPool->addAction(m_clipActions->actions[i].stream, m_file, this);
      }
  const char* method = event == 1 ? "onLoad" : event == 2 ? "onEnterFrame" : event == 4 ? "onUnload" : nullptr;
  if (!method) return;
  AptActionInterpreter& in = gAptActionInterpreter;
  AptVar fn = Get(in, method);
  if (AptFunctionValue* f = fn.AsFunction()) gpPool->addFunction(this, f, AptVar::Obj(this), {});
}

// AptCIH::GetProceduralProperty (80291760), in SWF property numbering.
float AptCIH::GetProperty(int index) {
  switch (index) {
    case 0: return m_matrix.tx;
    case 1: return m_matrix.ty;
    case 2: return Length(m_matrix.a, m_matrix.b) * 100.0f;
    case 3: return Length(m_matrix.c, m_matrix.d) * 100.0f;
    case 4: return static_cast<float>(m_frame + 1);
    case 5: return Timeline() ? static_cast<float>(Timeline()->frameCount) : 1.0f;
    case 6: return m_cxform.mulA * 100.0f;
    case 7: return m_visible ? 1.0f : 0.0f;
    case 8:
    case 9: {
      AptRect r{};
      bool empty = true;
      ContentBounds(this, m_matrix, r, empty);
      if (empty) return 0.0f;
      return index == 8 ? r.right - r.left : r.bottom - r.top;
    }
    case 10: return std::atan2(m_matrix.b, m_matrix.a) * 180.0f / 3.14159265f;
    case 12: return Timeline() ? static_cast<float>(Timeline()->frameCount) : 1.0f;
    default: return 0.0f;
  }
}

// AptCIH::setProceduralProperty (80291B30)
void AptCIH::SetProperty(int index, float v) {
  m_scriptChanged = true;
  const float sx = Length(m_matrix.a, m_matrix.b), sy = Length(m_matrix.c, m_matrix.d);
  const float rot = std::atan2(m_matrix.b, m_matrix.a);
  auto rebuild = [&](float scaleX, float scaleY, float r) {
    const float skew = std::atan2(-m_matrix.c, m_matrix.d) - rot;  // keep any skew
    m_matrix.a = scaleX * std::cos(r);
    m_matrix.b = scaleX * std::sin(r);
    m_matrix.c = -scaleY * std::sin(r + skew);
    m_matrix.d = scaleY * std::cos(r + skew);
  };
  switch (index) {
    case 0: m_matrix.tx = v; break;
    case 1: m_matrix.ty = v; break;
    case 2: rebuild(v / 100.0f, sy, rot); break;
    case 3: rebuild(sx, v / 100.0f, rot); break;
    case 6: m_cxform.mulA = v / 100.0f; break;
    case 7: m_visible = v != 0.0f; break;
    case 8:
    case 9: {
      if (IsText()) {
        // PC: a text field's _width/_height resize its box (the widget screens size their fields
        // this way; scaling the field stretched the glyphs, unlike the GC's paragraph layout).
        if (v <= 0.0f) break;
        if (index == 8) m_textBounds.right = m_textBounds.left + v / (sx > 0.0f ? sx : 1.0f);
        else m_textBounds.bottom = m_textBounds.top + v / (sy > 0.0f ? sy : 1.0f);
        m_textDirty = true;
        break;
      }
      AptRect r{};
      bool empty = true;
      ContentBounds(this, m_matrix, r, empty);
      const float cur = empty ? 0.0f : (index == 8 ? r.right - r.left : r.bottom - r.top);
      if (cur > 0.0f) {
        if (index == 8) rebuild(sx * v / cur, sy, rot);
        else rebuild(sx, sy * v / cur, rot);
      }
      break;
    }
    case 10: rebuild(sx, sy, v * 3.14159265f / 180.0f); break;
    default: break;
  }
}

// AptCharacterTextInst::UpdateText (80288700): bound fields follow their variable.
void AptCIH::UpdateText() {
  if (m_variable.empty() || m_variable[0] == '$') return;
  AptCIH* owner = m_parent;
  while (owner && !owner->IsSprite()) owner = owner->m_parent;
  AptActionInterpreter& in = gAptActionInterpreter;
  AptActionInterpreter::Context ctx;
  ctx.target = owner;
  AptActionInterpreter::Context* saved = in.m_ctx;
  in.m_ctx = &ctx;
  AptVar v = in.getVariable(m_variable);
  in.m_ctx = saved;
  const std::string text = v.IsUndefined() ? "" : v.ToString();
  if (text != m_text) {
    m_text = text;
    m_textDirty = true;
  }
}

// 80290C60
void AptCIH::render(AptRenderingContext* rc, int maskOp) {
  if (!m_visible || m_removed) return;
  AptCharacter* ch = m_character;
  if (!ch) return;
  switch (ch->type) {
    case AptCharacter::kSprite:
    case AptCharacter::kMovie: {
      // A clip given a "_type" by script is a custom control (80290D10): the game draws it in the
      // bounds of its first shape (customControlRender with the type and the clip's target path).
      AptProperty* type = Find("_type");
      if (!type || type->value.IsUndefined() || !gAptFuncs.customControlRender) {
        m_children.render(rc, maskOp);
        break;
      }
      AptCIH* shape = nullptr;
      for (AptCIH* c : m_children.m_list)
        if (c->m_character && c->m_character->type == AptCharacter::kShape) {
          shape = c;
          break;
        }
      void* unit = shape ? static_cast<AptCharacterShape*>(shape->m_character)->renderingUnit : nullptr;
      rc->pushVertexMatrix();
      if (shape) rc->appendVertexMatrix(shape->m_matrix);
      if (gAptFuncs.setVertexMatrix) gAptFuncs.setVertexMatrix(&rc->getVertexMatrix());
      if (gAptFuncs.setColourTransform) gAptFuncs.setColourTransform(&rc->getColourTransform());
      const std::string kind = type->value.ToString();
      std::string target = GetTargetPath();
      for (char& c : target)
        if (c == '.') c = '/';
      gAptFuncs.customControlRender(kind.c_str(), target.c_str(), unit, "");
      rc->popVertexMatrix();
      break;
    }
    case AptCharacter::kShape: {
      auto* s = static_cast<AptCharacterShape*>(ch);
      if (!s->renderingUnit) break;
      if (gAptFuncs.setVertexMatrix) gAptFuncs.setVertexMatrix(&rc->getVertexMatrix());
      if (gAptFuncs.setColourTransform) gAptFuncs.setColourTransform(&rc->getColourTransform());
      if (gAptFuncs.drawRenderingUnit) gAptFuncs.drawRenderingUnit(s->renderingUnit, maskOp);
      break;
    }
    case AptCharacter::kEditText: {
      // AptCIH::ensureStringAllocated (80290824) then drawStringEor.
      UpdateText();
      if (m_textDirty) {
        ReleaseText();
        m_textDirty = false;
      }
      if (!m_textHandle && gAptFuncs.allocateString) {
        AptAllocateStringParameters p;
        p.bounds = m_textBounds;
        p.alignment = m_textAlign;
        p.colour = m_textColour;
        p.fontHeight = m_textSize;
        p.text = m_text.c_str();
        m_textHandle = gAptFuncs.allocateString(&p);
      }
      if (m_textHandle && gAptFuncs.drawString) {
        if (gAptFuncs.setVertexMatrix) gAptFuncs.setVertexMatrix(&rc->getVertexMatrix());
        if (gAptFuncs.setColourTransform) gAptFuncs.setColourTransform(&rc->getColourTransform());
        gAptFuncs.drawString(m_textHandle, maskOp);
      }
      break;
    }
    default: break;
  }
}

// ---- AptAnimationPoolData -----------------------------------------------------------------------------

AptCIH* AptAnimationPoolData::getLevel(int level) const {
  return level >= 0 && level < static_cast<int>(m_levels.size()) ? m_levels[static_cast<size_t>(level)] : nullptr;
}

// AptActionQueueC::AddActionBack (80284FD8)
void AptAnimationPoolData::addAction(const unsigned char* stream, AptFileImage* file, AptCIH* target, int frameMarker) {
  AptQueuedAction a;
  a.frameMarker = frameMarker;
  a.stream = stream;
  a.file = file;
  a.target = target;
  m_actions.push_back(a);
}

// AptActionQueueC::AddFunctionBack (80285160)
void AptAnimationPoolData::addFunction(AptCIH* target, AptFunctionValue* f, const AptVar& thisv,
                                       const std::vector<AptVar>& args) {
  AptQueuedAction a;
  a.target = target;
  a.function = f;
  a.thisv = thisv;
  a.args = args;
  m_actions.push_back(a);
}

// 80283FA0
// Runs the queue in order, including what running it adds; actions of removed clips are dropped, and
// a frame action the timeline queued (marker -frame) is skipped once its clip is on another frame
// (e.g. a parent's script sent it elsewhere with gotoAndPlay before the clip's own stop() ran).
void AptAnimationPoolData::runActions() {
  for (size_t i = 0; i < m_actions.size() && i < 100000; ++i) {
    AptQueuedAction a = m_actions[i];
    if (a.target && a.target->m_removed && !a.function) continue;
    if (!a.function && a.frameMarker < 0 && a.target && a.target->Timeline() && -a.frameMarker != a.target->m_frame)
      continue;
    if (a.input) m_curInput = a.input;
    if (a.function) gAptActionInterpreter.callFunction(AptVar::Obj(a.function), a.thisv, a.args);
    else gAptActionInterpreter.runStream(a.stream, a.file, a.target);
  }
  m_actions.clear();
}

// 80284344
// Queued for the next tick; the same event twice in a row counts once.
void AptAnimationPoolData::addInput(unsigned int input) {
  if (m_inputs.size() >= 32) return;
  if (!m_inputs.empty() && m_inputs.back() == input) return;
  m_inputs.push_back(input);
}

// 8029E1F8
void AptAnimationPoolData::ProcessInputs() {
  std::vector<unsigned int> inputs;
  inputs.swap(m_inputs);
  for (unsigned int input : inputs) ProcessAptInput(input);
}

// 8029E150
// GC: then the focused button (HandleFocusButton) or button auto-navigation; the front end's movies
// take all input through Key listeners, so buttons are not ported.
void AptAnimationPoolData::ProcessAptInput(unsigned int input) {
  ProcessInputSet(input);
  ProcessListenerEvents(input);
}

namespace {
void CollectKeyClips(AptCIH* c, std::vector<AptCIH*>& out) {
  if (!c || c->m_removed) return;
  if (c->m_clipActions) out.push_back(c);
  for (AptCIH* child : c->m_children.m_list) CollectKeyClips(child, out);
}
}  // namespace

// 8029D428
// onClipEvent(keyDown) / (keyUp) handlers of the clips on stage (GC kept them in an input set).
void AptAnimationPoolData::ProcessInputSet(unsigned int input) {
  const unsigned int state = input >> 10 & 0x7f;
  const unsigned int event = state == 0 ? 0x40 : state == 1 ? 0x80 : 0;
  if (!event) return;
  std::vector<AptCIH*> clips;
  for (AptCIH* l : m_levels) CollectKeyClips(l, clips);
  for (AptCIH* c : clips)
    for (int i = 0; i < c->m_clipActions->count; ++i)
      if (c->m_clipActions->actions[i].events & event) {
        AptQueuedAction a;
        a.stream = c->m_clipActions->actions[i].stream;
        a.file = c->m_file;
        a.target = c;
        a.input = input;
        m_actions.push_back(a);
      }
}

// 8029D75C (AddListenerToQueue 8029D838)
// Each Key listener's onKeyDown (state 0) or onKeyUp (state 1) is queued with the event.
void AptAnimationPoolData::ProcessListenerEvents(unsigned int input) {
  const unsigned int state = input >> 10 & 0x7f;
  if (state > 1 || !m_keyListeners) return;
  const char* method = state == 0 ? "onKeyDown" : "onKeyUp";
  auto* list = static_cast<AptArray*>(m_keyListeners);
  AptActionInterpreter& in = gAptActionInterpreter;
  for (const AptVar& l : list->m_items) {
    AptObject* o = l.AsObject();
    if (!o) continue;
    AptFunctionValue* f = o->Get(in, method).AsFunction();
    if (!f) continue;
    AptQueuedAction a;
    a.target = o->m_kind == AptValue::kCIH ? static_cast<AptCIH*>(o) : nullptr;
    a.function = f;
    a.thisv = l;
    a.input = input;
    m_actions.push_back(a);
  }
}

// 80283AC8
void AptAnimationPoolData::tickIntervalTimers(int ms) {
  for (size_t i = 0; i < m_timers.size(); ++i) {
    if (m_timers[i].dead) continue;
    m_timers[i].elapsed += ms;
    if (m_timers[i].elapsed < m_timers[i].interval) continue;
    m_timers[i].elapsed = 0;
    AptIntervalTimer t = m_timers[i];
    AptActionInterpreter& in = gAptActionInterpreter;
    if (t.function.AsFunction()) in.callFunction(t.function, t.target, t.args);
    else in.callMethod(t.target, t.function.ToString(), t.args);
  }
  m_timers.erase(std::remove_if(m_timers.begin(), m_timers.end(), [](const AptIntervalTimer& t) { return t.dead; }),
                 m_timers.end());
}

// 80284C2C
void AptAnimationPoolData::RegisterReferences() {
  for (AptCIH* l : m_levels) AptGC::Mark(l);
  for (const AptQueuedAction& a : m_actions) {
    AptGC::Mark(a.target);
    AptGC::Mark(a.function);
    AptGC::Mark(a.thisv);
    for (const AptVar& v : a.args) AptGC::Mark(v);
  }
  for (const AptIntervalTimer& t : m_timers) {
    AptGC::Mark(t.target);
    AptGC::Mark(t.function);
    for (const AptVar& v : t.args) AptGC::Mark(v);
  }
  AptGC::Mark(m_keyListeners);
}

// ---- Update and render ----------------------------------------------------------------------------------

namespace {
// _tick (80269718): one or more player ticks of _level0's frame time (the movie's +0x24).
void Tick(unsigned int ms) {
  AptCIH* level0 = gpPool->getLevel(0);
  if (!level0) return;
  AptCharacterAnimation* anim = level0->Animation();
  const int period = anim && anim->msPerFrame > 0 ? anim->msPerFrame : 33;
  int t = static_cast<int>(ms) + gpPool->m_tickRemainder;
  while (t >= period) {
    t -= period;
    gpPool->tickIntervalTimers(period);
    for (size_t i = 0; i < gpPool->m_levels.size(); ++i)
      if (AptCIH* l = gpPool->m_levels[i]) {
        std::vector<AptCIH*> one{l};
        AptDisplayList dl;
        dl.m_list = one;
        dl.tick();
      }
    gpPool->runActions();
    gpPool->ProcessInputs();
    AptLinkerUpdate();
    gpPool->m_curTick += static_cast<unsigned int>(period);
    if (!gpPool->getLevel(0)) break;
  }
  gpPool->m_tickRemainder = t;
}
}  // namespace

// 8026AAA8 (_AptInternalUpdate 8026A86C)
void AptUpdate(unsigned int ms) {
  if (!gpPool) return;
  // PC: movies the game loaded since the last tick are attached first (on GC the game's loads come
  // through the movie's own frames, inside the tick)
  AptLinkerUpdate();
  if (gpPool->getLevel(0)) Tick(ms);
  // PC: also after a few movie loads, so the movies unloaded since give their aux levels and texture
  // slots back (GC freed each movie when its last reference went).
  if (AptGC::s_allocatedSinceCollect > 20000 || gAptLoadsSinceCollect >= 4 || gAptUnloadsSinceCollect > 0)
    AptGC::CleanUnreachable();
}

// 8026AAC8 (_AptInternalRender 8026AA58)
void AptRender() {
  if (!gpPool) return;

  if (!gpRenderingContext) gpRenderingContext = new AptRenderingContext;
  gpRenderingContext->Reset();
  for (AptCIH* l : gpPool->m_levels)
    if (l) {
      AptDisplayList dl;
      dl.m_list.push_back(l);
      dl.render(gpRenderingContext, 0);
    }
}

// 802692E0
void AptGetAnimationSize(int* width, int* height) {
  AptCIH* level0 = gpPool ? gpPool->getLevel(0) : nullptr;
  AptCharacterAnimation* anim = level0 ? level0->Animation() : nullptr;
  if (width) *width = anim ? anim->width : 0;
  if (height) *height = anim ? anim->height : 0;
}
