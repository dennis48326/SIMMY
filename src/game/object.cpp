// Port of game/object.cpp (cXObjectImpl and EdithVariableSet, partial): a simulation object's state
// as the house stores it. Ported: the object's selector and definition, its stream's header
// (location, level), attributes, temporaries and data variables, and the accessors live mode uses.
// Still stubbed: the rest of the stream (the behaviour tree stack, slots, sprites), behaviour,
// hierarchy (containers and slots), routing, interactions and everything the simulator runs.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <vector>

// ---------------------------------------------------------------------------------------------------
// cXObject

// 800D9484
cXObject::~cXObject() {}

// ---------------------------------------------------------------------------------------------------
// cXObjectImpl

void LoadObjectSpriteSlots(cXObjectImpl* object);  // sprites.cpp

// 800D962C
// GC: also the TreeSim / TreeSimImpl bases (the behaviour stack), the default attribute and data sets
// from the selector's definition, the slots and the object's function table.
cXObjectImpl::cXObjectImpl(int, ObjSelector* selector, ObjectModule*) {
  m_selector = selector;
  m_def = selector ? selector->m_def : nullptr;
  // The definition's attribute count (+0x74; none: 8).
  m_attrCount = m_def && ERQuickdata::S16(m_def, 0x74) ? ERQuickdata::S16(m_def, 0x74) : 8;
  m_attrs = new short[static_cast<unsigned int>(m_attrCount)]();
  m_miscFlags |= 0x200;
  LoadObjectSpriteSlots(this);  // GC: SlotLoader::Load (its sprite slots; object and routing slots: PC elsewhere)
  SIMMY_STUB("cXObjectImpl::cXObjectImpl: TreeSim, default data from the definition, object slots", 0x800D962C);
}

// 800DBC54
cXObjectImpl::~cXObjectImpl() {
  if (Globs_pSound) Globs_pSound->QuietBySourceID(m_id);
  delete[] m_attrs;
  delete[] m_sprites;
  delete m_relations;
  EGlobal::FreeSpriteRenderer(this);  // its sprite renderer goes on the next ESpriteRenderMan::Update
  delete m_treeSim;
}

// 800E2AC4
int cXObjectImpl::GetLevel() const { return m_level; }

// 800E2AD4
// The tile under it (the location is in 1/16 tiles).
CTilePt cXObjectImpl::GetCTilePt() const { return CTilePt(m_location[1] >> 4, m_location[0] >> 4, m_level); }

// 800EB0C4
int cXObjectImpl::GetData(int index) { return m_data[index]; }
// 800EB2CC
const ObjDefinition* cXObjectImpl::GetDef() { return m_def; }
// 800EB2D4
int cXObjectImpl::GetType() { return objdef::Type(m_def); }
// 800EB2E0
int cXObjectImpl::GetID() { return m_id; }
// 800EB328
ObjSelector* cXObjectImpl::GetSelector() { return m_selector; }
// 800EB330
ObjSelector* cXObjectImpl::GetSelector() const { return m_selector; }
// 800EB8DC
cXObjectImpl* cXObjectImpl::GetObjectImplementation() { return this; }
// 800EB1D0
bool cXObjectImpl::IsRoof() { return objdef::Type(GetDef()) == 0xe; }
// 800E3050
bool cXObjectImpl::IsVehicle() { return GetType() == 0xc; }
// 800EB89C
// Once: whether the simulation asked the model to restart its animation.
int cXObjectImpl::HaveGraphicsBeenTagged() {
  if (!m_graphicsTagged) return 0;
  m_graphicsTagged = 0;
  return 1;
}
// 800EB378
int cXObjectImpl::IsMultiTile() { return objdef::MasterID(m_def) != 0 ? 1 : 0; }

// 800DD4C8
// The object holding it, up its slot hierarchy. PC: slots are not read, so none.
// The object it is in (GC: the slot hierarchy's top, HierGetParent, then its container data; PC: the
// container id in data 2, as TrySnap / TryGrab set it).
cXObject* cXObjectImpl::GetContainer() {
  return m_data[2] > 0 && m_data[2] != m_id ? GetObjectFromID(m_data[2]) : nullptr;
}

// 800EB758
// A seat: its function table has a "sit" tree (entry 0x1A).
bool cXObjectImpl::IsChair() { return GetTreeID(0x1a) != 0; }

// 800DB168
// Before the house is saved (people store their state; plain objects have nothing to do).
void cXObjectImpl::PreSave() {}

// 800EB3EC
RelMatrixImpl* cXObjectImpl::GetRelMatrix() {
  if (!m_relations) m_relations = new RelMatrixImpl;  // GC: RelMatrix::CreateInstance in the constructor
  return m_relations;
}

// 800E04CC
// Its location and the box around it (1/16 tiles), its level, (old saves) its id.
void cXObjectImpl::ReconHeader(ReconBuffer* b, int version) {
  b->Recon32(&m_field70[2], 1);  // +0x78
  b->Recon32(&m_field70[3], 1);  // +0x7C
  b->Recon32(&m_field70[0], 1);  // +0x70
  b->Recon32(&m_field70[1], 1);  // +0x74
  b->Recon32(&m_location[1], 1);  // +0x6C
  b->Recon32(&m_location[0], 1);  // +0x68
  if (version > 0x18) b->ReconInt(&m_level, 1);
  if (version < 0x2f) b->Recon16(&m_id, 1);
  short one = 1;
  b->Recon16(&one, 1);
  if (version == 0x37) b->ReconInt(&m_miscFlags, 1);
}

// 800E05BC
// The header, the attributes (resized to the saved count when `resize`, else as many as it has kept),
// the temporaries and the data variables; a 0; the behaviour stack (TreeStack, its behaviours by the
// object folder); its object relationships (RelMatrix +0x60, from version 5); its slots' contents
// (ReconSlots); for versions 0x42..0x56 an unused word; the sprite visibility (from 0x1B). Old saves
// get data 9/10 (before 0xD), 0x10/0x11 (before 0x22) and 0x43 (before 0x34) cleared or set. Read with
// `resize`: data 1 cleared. GC: last the model gets the object's colour (ISimInstance vtable 0x124 with
// GetColorIndex), PC: AllocInstance after the stream does.
void cXObjectImpl::ReconStream(ReconBuffer* b, int version, bool resize) {
  const bool reading = b->m_mode == ReconBuffer__Mode::kRead;
  ReconHeader(b, version);
  if (version < 1) {
    b->Recon16(m_attrs, 8);
    SIMMY_STUB("cXObjectImpl::ReconStream: version 0 saves (32 temporaries)", 0x800E05BC);
    return;
  }
  if (version < 0xb) {
    b->Recon16(m_attrs, 8);
    b->Recon16(m_temps, 8);
  } else {
    if (version < 0x18) {
      b->Recon16(m_attrs, 8);
    } else {
      short count = static_cast<short>(m_attrCount);
      b->Recon16(&count, 1);
      if (m_attrCount < count) {
        if (!resize) {
          std::vector<short> saved(static_cast<unsigned int>(count));
          b->Recon16(saved.data(), count);
          for (int i = 0; i < m_attrCount; ++i) m_attrs[i] = saved[static_cast<unsigned int>(i)];
        } else {
          delete[] m_attrs;
          m_attrCount = count;
          m_attrs = new short[static_cast<unsigned int>(count)]();
          b->Recon16(m_attrs, count);
        }
      } else {
        b->Recon16(m_attrs, count);
      }
    }
    b->Recon16(m_temps, 8);
    if (version < 0x53) {
      SIMMY_STUB("cXObjectImpl::ReconStream: old data variables (EdithVariableSet::ReconOldFdata)", 0x800E05BC);
      return;
    }
    if (version > 0x53) m_data.ReconStream(b, version);
  }
  short zero = 0;
  b->Recon16(&zero, 1);
  // GC: the TreeSim base's stack (+0x0C), with the folder's BehaviorFinder (GetFolder()->vtable 0x16C).
  if (!m_treeSim) {
    m_treeSim = new TreeSimImpl(this);
    m_treeSim->Initialize(0, m_temps);
  }
  m_treeSim->m_stack.ReconStream(b, version, Globs_pObjectFolder);
  if (version > 4) {
    if (!m_relations) m_relations = new RelMatrixImpl;
    m_relations->DoStream(b, version < 7 ? 0 : version);
  }
  if (version <= 0xc && reading) {
    m_data.WriteVar(9, 0);
    m_data.WriteVar(10, 0);
  }
  if (version < 0x22) {
    m_data.WriteVar(0x10, 0);
    m_data.WriteVar(0x11, 0);
  }
  ReconSlots(b, version);  // GC: vtable 0x4FC
  if (static_cast<unsigned int>(version - 0x42) < 0x15) {
    int unused = 0;
    b->Recon32(&unused, 1);
  }
  if (version < 0x34) m_data.WriteVar(0x43, static_cast<short>(static_cast<short>(m_slotContents.size()) - 1));
  if (version > 0x1a) {
    short count = m_spriteCount;
    b->Recon16(&count, 1);
    if (m_spriteCount < count && resize) {
      delete[] m_sprites;
      m_spriteCount = count;
      m_sprites = new short[static_cast<unsigned int>(count)]();
    }
    // GC reads the saved count into the array it has (an object type that lost sprites overruns it);
    // PC: the extra ones are dropped.
    if (count <= m_spriteCount) {
      b->Recon16(m_sprites, count);
    } else {
      std::vector<short> saved(static_cast<unsigned int>(count));
      for (int i = 0; i < m_spriteCount; ++i) saved[static_cast<unsigned int>(i)] = m_sprites[i];
      b->Recon16(saved.data(), count);
      for (int i = 0; i < m_spriteCount; ++i) m_sprites[i] = saved[static_cast<unsigned int>(i)];
    }
  }
  if (resize) m_data.WriteVar(1, 0);
}

// 800E0BA8
// From version 5: the slot count, then per slot a 0 and the id of the object it holds (ObjectSlot +4;
// read: slots are added as needed, PC: m_slotContents).
void cXObjectImpl::ReconSlots(ReconBuffer* b, int version) {
  if (version <= 4) return;
  short count = static_cast<short>(m_slotContents.size());
  b->Recon16(&count, 1);
  for (int i = 0; i < count; ++i) {
    if (static_cast<int>(m_slotContents.size()) <= i) m_slotContents.resize(static_cast<size_t>(i) + 1, 0);
    short zero = 0;
    short held = m_slotContents[static_cast<size_t>(i)];
    b->Recon16(&zero, 1);
    b->Recon16(&held, 1);
    m_slotContents[static_cast<size_t>(i)] = held;
  }
}

// ---------------------------------------------------------------------------------------------------
// EdithVariableSet

// 800E3280
EdithVariableSet::EdithVariableSet() {}
// 800E3FD4
EdithVariableSet::~EdithVariableSet() {}

// 800E4040
// GC: a variable not set reads as a shared 0 (s_InvalidReference).
short& EdithVariableSet::operator[](int index) {
  static short invalid;
  if (index < 0 || index >= kCount) {
    invalid = 0;
    return invalid;
  }
  return m_values[index];
}

// 800E4558
int EdithVariableSet::WriteVar(int index, short value) {
  if (index < 0 || index >= kCount) return 0;
  m_keys[index >> 5] |= 1u << (index & 31);
  m_values[index] = value;
  return 1;
}

// 800E583C
// Read (version 0x57 on): the 72-bit set of the variables present (two words and a byte), then each
// one's value in key order. Versions 0x55 and 0x56 give a count and per variable its key and value.
// GC: writing first stores the eight cached variables.
void EdithVariableSet::ReconStream(ReconBuffer* b, int version) {
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    // Written (and measured) in the current form: the key set, then the values of the keys set.
    // (GC first stored its eight cached variables; the port keeps them in m_values.)
    int keys[2] = {static_cast<int>(m_keys[0]), static_cast<int>(m_keys[1])};
    b->Recon32(&keys[0], 1);
    b->Recon32(&keys[1], 1);
    signed char last = static_cast<signed char>(m_keys[2] & 0xff);
    b->Recon8(&last, 1);
    for (int i = 0; i < kCount; ++i)
      if (m_keys[i >> 5] & (1u << (i & 31))) b->Recon16(&m_values[i], 1);
    return;
  }
  for (unsigned int& k : m_keys) k = 0;
  for (short& v : m_values) v = 0;
  if (version > 0x56) {
    // ReconKeyBitSet (800E7C14)
    unsigned int keys[3] = {};
    b->Recon32(reinterpret_cast<int*>(&keys[0]), 1);
    b->Recon32(reinterpret_cast<int*>(&keys[1]), 1);
    signed char last = 0;
    b->Recon8(&last, 1);
    keys[2] = static_cast<unsigned char>(last);
    for (int i = 0; i < kCount; ++i) {
      if (!(keys[i >> 5] & (1u << (i & 31)))) continue;
      short value = 0;
      b->Recon16(&value, 1);
      WriteVar(i, value);
    }
    return;
  }
  signed char count = 0;
  b->Recon8(&count, 1);
  for (int i = 0; i < count; ++i) {
    if (version < 0x55) {
      signed char bytes[4] = {};
      b->Recon8(bytes, 4);
      WriteVar(static_cast<unsigned char>(bytes[0]), static_cast<short>((bytes[2] & 0xff) << 8 | (bytes[3] & 0xff)));
    } else {
      signed char key = 0;
      short value = 0;
      b->Recon8(&key, 1);
      b->Recon16(&value, 1);
      WriteVar(static_cast<unsigned char>(key), value);
    }
  }
}
