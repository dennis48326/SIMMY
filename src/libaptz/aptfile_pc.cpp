// PC: reads an Apt movie (<name>.apt + <name>.const, big-endian, 32-bit file offsets) into the native
// structures of aptfile.hpp. This replaces the in-place offset fixups of the GC loader
// (AptCharacterAnimation::Resolve/Fixup 802826C8/802821A8, AptMovie::resolve 802A8C44 and
// AptAuxPCEorGL's _loadGeometry), which cannot store 64-bit pointers in 32-bit fields. It only
// translates layout; the runtime side of those functions (aux callbacks for textures, shapes and
// sounds, label hashes, constant AptValues, imports) is done by their ports on top of this.
#include "simmy/types_gen.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
// Reads fields of one image; every offset is relative to the image start, 0 = null.
struct AptImageReader {
  const unsigned char* base;
  unsigned int size;
  AptFileImage* owner;
  bool ok = true;

  unsigned int U32(unsigned int off) {
    if (off + 4 > size) {
      ok = false;
      return 0;
    }
    const unsigned char* p = base + off;
    return static_cast<unsigned int>(p[0]) << 24 | static_cast<unsigned int>(p[1]) << 16 |
           static_cast<unsigned int>(p[2]) << 8 | p[3];
  }
  int S32(unsigned int off) { return static_cast<int>(U32(off)); }
  float F32(unsigned int off) {
    const unsigned int v = U32(off);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
  }
  short S16(unsigned int off) {
    if (off + 2 > size) {
      ok = false;
      return 0;
    }
    return static_cast<short>(base[off] << 8 | base[off + 1]);
  }
  const char* Str(unsigned int off) {
    if (!off) return nullptr;
    if (off >= size) {
      ok = false;
      return nullptr;
    }
    return reinterpret_cast<const char*>(base + off);
  }
  const unsigned char* Bytes(unsigned int off) {
    if (!off) return nullptr;
    if (off >= size) {
      ok = false;
      return nullptr;
    }
    return base + off;
  }
  template <class T>
  T* New(int count = 1) {
    if (count <= 0) return nullptr;
    void* mem = std::calloc(static_cast<size_t>(count), sizeof(T));
    owner->m_blocks.push_back(mem);
    T* t = static_cast<T*>(mem);
    for (int i = 0; i < count; ++i) new (t + i) T;
    return t;
  }
  AptRect Rect(unsigned int off) { return {F32(off), F32(off + 4), F32(off + 8), F32(off + 12)}; }
  AptMatrix Matrix(unsigned int off) {
    return {F32(off), F32(off + 4), F32(off + 8), F32(off + 12), F32(off + 16), F32(off + 20)};
  }

  AptControl* Control(unsigned int off);
  void Timeline(unsigned int off, AptMovie& movie);
  AptCharacter* Character(unsigned int off);
};

AptControl* AptImageReader::Control(unsigned int off) {
  const int type = S32(off);
  switch (type) {
    case AptControl::kAction: {
      auto* c = New<AptControlAction>();
      c->stream = Bytes(U32(off + 4));
      c->type = type;
      return c;
    }
    case AptControl::kFrameLabel: {
      auto* c = New<AptControlFrameLabel>();
      c->label = Str(U32(off + 4));
      c->type = type;
      return c;
    }
    case AptControl::kPlaceObject: {
      auto* c = New<AptControlPlaceObject2>();
      c->type = type;
      c->flags = U32(off + 0x04);
      c->depth = S32(off + 0x08);
      c->character = S32(off + 0x0C);
      c->matrix = Matrix(off + 0x10);
      c->colourMul = U32(off + 0x28);
      c->colourAdd = U32(off + 0x2C);
      c->ratio = F32(off + 0x30);
      c->name = Str(U32(off + 0x34));
      c->clipDepth = S32(off + 0x38);
      if (const unsigned int ca = U32(off + 0x3C)) {
        auto* actions = New<AptClipActions>();
        actions->count = S32(ca);
        actions->actions = New<AptClipAction>(actions->count);
        const unsigned int list = U32(ca + 4);
        for (int i = 0; i < actions->count; ++i) {
          actions->actions[i].events = U32(list + 12 * i);
          actions->actions[i].keyCode = U32(list + 12 * i + 4);
          actions->actions[i].stream = Bytes(U32(list + 12 * i + 8));
        }
        c->clipActions = actions;
      }
      return c;
    }
    case AptControl::kRemoveObject: {
      auto* c = New<AptControlRemoveObject2>();
      c->type = type;
      c->depth = S32(off + 4);
      return c;
    }
    case AptControl::kBackgroundColour: {
      auto* c = New<AptControlBackgroundColour>();
      c->type = type;
      c->colour = U32(off + 4);
      return c;
    }
    case AptControl::kStartSound:
    case AptControl::kStartSoundStream: {
      auto* c = New<AptControlStartSound>();
      c->type = type;
      c->character = S32(off + 4);
      return c;
    }
    case AptControl::kInitAction: {
      auto* c = New<AptControlInitAction>();
      c->type = type;
      c->sprite = S32(off + 4);
      c->stream = Bytes(U32(off + 8));
      return c;
    }
    default:
      std::fprintf(stderr, "[apt] unknown frame control type %d at 0x%X\n", type, off);
      ok = false;
      return nullptr;
  }
}

void AptImageReader::Timeline(unsigned int off, AptMovie& movie) {
  movie.frameCount = S32(off);
  movie.frames = New<AptFrame>(movie.frameCount);
  const unsigned int frames = U32(off + 4);
  for (int f = 0; f < movie.frameCount && ok; ++f) {
    AptFrame& frame = movie.frames[f];
    frame.controlCount = S32(frames + 8 * f);
    frame.controls = New<AptControl*>(frame.controlCount);
    const unsigned int list = U32(frames + 8 * f + 4);
    for (int i = 0; i < frame.controlCount && ok; ++i) frame.controls[i] = Control(U32(list + 4 * i));
  }
  movie.labels = nullptr;  // AptMovie::resolve builds the label hash at run time
}

AptCharacter* AptImageReader::Character(unsigned int off) {
  const int type = S32(off);
  switch (type) {
    case AptCharacter::kShape: {
      auto* c = New<AptCharacterShape>();
      c->bounds = Rect(off + 8);
      c->geometryId = S32(off + 0x18);
      c->type = type;
      return c;
    }
    case AptCharacter::kEditText: {
      auto* c = New<AptCharacterEditText>();
      c->type = type;
      c->bounds = Rect(off + 8);
      c->font = S32(off + 0x18);
      c->alignment = S32(off + 0x1C);
      c->colour = U32(off + 0x20);
      c->fontHeight = F32(off + 0x24);
      c->readOnly = S32(off + 0x28);
      c->multiline = S32(off + 0x2C);
      c->wordWrap = S32(off + 0x30);
      c->text = Str(U32(off + 0x34));
      c->variable = Str(U32(off + 0x38));
      return c;
    }
    case AptCharacter::kFont: {
      auto* c = New<AptCharacterFont>();
      c->type = type;
      c->name = Str(U32(off + 8));
      c->glyphCount = S32(off + 0xC);
      c->glyphIds = New<int>(c->glyphCount);
      c->glyphs = New<AptCharacter*>(c->glyphCount);
      const unsigned int glyphs = U32(off + 0x10);
      for (int i = 0; i < c->glyphCount; ++i) c->glyphIds[i] = S32(glyphs + 4 * i);
      return c;
    }
    case AptCharacter::kButton: {
      auto* c = New<AptCharacterButton>();
      c->type = type;
      c->isMenu = S32(off + 8);
      c->bounds = Rect(off + 0xC);
      c->triangleCount = S32(off + 0x1C);
      c->vertexCount = S32(off + 0x20);
      c->vertices = New<float>(c->vertexCount * 2);
      for (int i = 0; i < c->vertexCount * 2; ++i) c->vertices[i] = F32(U32(off + 0x24) + 4 * i);
      c->triangles = New<unsigned int>(c->triangleCount * 3);
      for (int i = 0; i < c->triangleCount * 3; ++i) c->triangles[i] = U32(U32(off + 0x28) + 4 * i);
      c->recordCount = S32(off + 0x2C);
      c->records = New<AptButtonRecord>(c->recordCount);
      const unsigned int recs = U32(off + 0x30);
      for (int i = 0; i < c->recordCount; ++i) {
        const unsigned int r = recs + 0x44 * i;
        AptButtonRecord& rec = c->records[i];
        rec.flags = U32(r);
        rec.characterId = S32(r + 4);
        rec.depth = S32(r + 8);
        rec.matrix = Matrix(r + 0xC);
        for (int k = 0; k < 4; ++k) rec.colour[k] = F32(r + 0x24 + 4 * k);
        for (int k = 0; k < 4; ++k) rec.unknown[k] = F32(r + 0x34 + 4 * k);
      }
      c->actionCount = S32(off + 0x34);
      c->actions = New<AptButtonAction>(c->actionCount);
      for (int i = 0; i < c->actionCount; ++i) {
        c->actions[i].flags = U32(U32(off + 0x38) + 8 * i);
        c->actions[i].stream = Bytes(U32(U32(off + 0x38) + 8 * i + 4));
      }
      if (const unsigned int snd = U32(off + 0x3C)) {
        c->hasSounds = true;
        for (int k = 0; k < 4; ++k) c->soundIds[k] = S32(snd + 4 * k);
      }
      return c;
    }
    case AptCharacter::kSprite: {
      auto* c = New<AptCharacterSprite>();
      c->type = type;
      Timeline(off + 8, c->timeline);
      return c;
    }
    case AptCharacter::kSound: {
      auto* c = New<AptCharacterSound>();
      c->type = type;
      return c;
    }
    case AptCharacter::kImage: {
      auto* c = New<AptCharacterImage>();
      c->type = type;
      c->texture = S32(off + 8);
      return c;
    }
    case AptCharacter::kMorph: {
      auto* c = New<AptCharacterMorph>();
      c->type = type;
      c->startId = S32(off + 8);
      c->endId = S32(off + 0xC);
      return c;
    }
    case AptCharacter::kStaticText: {
      auto* c = New<AptCharacterStaticText>();
      c->type = type;
      c->bounds = Rect(off + 8);
      c->matrix = Matrix(off + 0x18);
      c->recordCount = S32(off + 0x30);
      c->records = New<AptStaticTextRecord>(c->recordCount);
      const unsigned int recs = U32(off + 0x34);
      for (int i = 0; i < c->recordCount; ++i) {
        const unsigned int r = recs + 0x38 * i;
        AptStaticTextRecord& rec = c->records[i];
        rec.font = S32(r);
        for (int k = 0; k < 8; ++k) rec.colour[k] = F32(r + 4 + 4 * k);
        rec.offsetX = F32(r + 0x24);
        rec.offsetY = F32(r + 0x28);
        rec.height = F32(r + 0x2C);
        rec.glyphCount = S32(r + 0x30);
        rec.glyphs = New<short>(rec.glyphCount * 2);
        for (int k = 0; k < rec.glyphCount * 2; ++k) rec.glyphs[k] = S16(U32(r + 0x34) + 2 * k);
      }
      return c;
    }
    case AptCharacter::kMovie: {
      // Only characters[0], the file's own movie, is a movie; it is read by Build.
      std::fprintf(stderr, "[apt] nested movie character at 0x%X\n", off);
      ok = false;
      return nullptr;
    }
    default:
      std::fprintf(stderr, "[apt] unknown character type %d at 0x%X\n", type, off);
      ok = false;
      return nullptr;
  }
}
}  // namespace

// Reads the constant table, then the movie at the offset the .const gives, its character table
// (index 0 is the movie itself; an empty slot is an import, filled in by Link), imports and exports.
// Each character's `movie` points at the movie, as Fixup set it. Returns null on malformed data.
AptFileImage* AptFileImage::Build(const unsigned char* apt, unsigned int aptSize, const unsigned char* constData,
                                  unsigned int constSize) {
  if (!apt || !constData || aptSize < 12 || std::memcmp(apt, "Apt Data:", 9) != 0 ||
      constSize < 0x20 || std::memcmp(constData, "Apt constant file\x1a", 18) != 0)
    return nullptr;
  auto* image = new AptFileImage;
  image->m_apt = apt;
  image->m_aptSize = aptSize;
  image->m_const = constData;
  image->m_constSize = constSize;

  AptImageReader c{constData, constSize, image};
  AptConstFile& cf = image->m_constFile;
  cf.aptDataOffset = c.U32(0x14);
  cf.itemCount = c.S32(0x18);
  cf.items = c.New<AptConstItem>(cf.itemCount);
  const unsigned int items = c.U32(0x1C);
  for (int i = 0; i < cf.itemCount && c.ok; ++i) {
    AptConstItem& item = cf.items[i];
    item.type = c.S32(items + 8 * i);
    item.value = c.U32(items + 8 * i + 4);
    if (item.type == 1) item.string = c.Str(item.value);
  }

  AptImageReader r{apt, aptSize, image};
  const unsigned int mo = cf.aptDataOffset;
  if (c.ok && r.S32(mo) == AptCharacter::kMovie) {
    auto* movie = r.New<AptCharacterMovie>();
    movie->type = AptCharacter::kMovie;
    AptCharacterAnimation& a = movie->animation;
    r.Timeline(mo + 8, a.timeline);
    a.characterCount = r.S32(mo + 0x14);
    a.characters = r.New<AptCharacter*>(a.characterCount);
    a.width = r.S32(mo + 0x1C);
    a.height = r.S32(mo + 0x20);
    a.msPerFrame = r.S32(mo + 0x24);
    a.importCount = r.S32(mo + 0x28);
    a.imports = r.New<AptImport>(a.importCount);
    for (int i = 0; i < a.importCount; ++i) {
      const unsigned int e = r.U32(mo + 0x2C) + 16 * i;
      a.imports[i].movie = r.Str(r.U32(e));
      a.imports[i].name = r.Str(r.U32(e + 4));
      a.imports[i].character = r.S32(e + 8);
    }
    a.exportCount = r.S32(mo + 0x30);
    a.exports = r.New<AptExport>(a.exportCount);
    for (int i = 0; i < a.exportCount; ++i) {
      const unsigned int e = r.U32(mo + 0x34) + 8 * i;
      a.exports[i].name = r.Str(r.U32(e));
      a.exports[i].character = r.S32(e + 4);
    }
    const unsigned int chars = r.U32(mo + 0x18);
    for (int i = 0; i < a.characterCount && r.ok; ++i) {
      const unsigned int off = r.U32(chars + 4 * i);
      if (!off) continue;
      AptCharacter* ch = off == mo ? movie : r.Character(off);
      if (ch) ch->movie = movie;
      a.characters[i] = ch;
    }
    image->m_movie = movie;
  }
  if (!c.ok || !r.ok || !image->m_movie) {
    std::fprintf(stderr, "[apt] malformed movie data\n");
    delete image;
    return nullptr;
  }
  return image;
}

AptFileImage::~AptFileImage() {
  for (void* p : m_blocks) std::free(p);
}
