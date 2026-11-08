// Port of engine/e_rcharacter.cpp and engine/e_characterman.cpp: skeletons ('CHRC').
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>

// 80368C44 ERCharacter::GetTypeInfo
// 80368C50 ERCharacter::GetTypeName
// 80368C5C ERCharacter::GetTypeKey
// 80368C68 ERCharacter::GetTypeVersion
// 80368C04 ERCharacter::SafeDelete
// 80368B8C ERCharacter::New
// 80368BB4 ERCharacter::Construct
// 80368BE0 ERCharacter::Destruct
// 80368C74 ERCharacter::GetTypeInfoStatic
// 80368C80 ERCharacter::GetReadVersion
SIMMY_STORABLE_IMPL(ERCharacter, "ERCharacter", 0, EResource)

// 803686E8
ERCharacter::ERCharacter() {}
// 8036872C
ERCharacter::~ERCharacter() = default;

namespace {
void ReadFloats(EFile& file, float* out, int count) {
  for (int i = 0; i < count; ++i) out[i] = simmy::read_float(file);
}
// EFile >> EString: characters up to a NUL.
std::string ReadString(EFile& file) {
  std::string s;
  for (char c; (c = static_cast<char>(simmy::read_u8(file))) != 0;) s += c;
  return s;
}
}  // namespace

// 80368794
// Per node: parent, child indices, pivot, orientation, a flag byte, two matrices and the name; then
// three more fields. (Version-less data starts with the name instead of a header.)
void ERCharacter::Load(EFile& file) {
  EDataHeader header;
  if (!header.Read(file, 0x43485243 /* 'CHRC' */, 9, 9)) ReadString(file);
  const int count = static_cast<int>(simmy::read_u32(file));
  m_nodes.assign(count, ECharacterNode{});
  for (ECharacterNode& n : m_nodes) {
    n.m_parent = static_cast<int>(simmy::read_u32(file));
    n.m_children.resize(simmy::read_u32(file));
    for (int& c : n.m_children) c = static_cast<int>(simmy::read_u32(file));
    ReadFloats(file, &n.m_pivot.x, 3);
    ReadFloats(file, &n.m_orient.x, 4);
    n.m_flagB0 = simmy::read_u8(file) != 0;
    ReadFloats(file, &n.m_mat30.m[0][0], 16);
    ReadFloats(file, &n.m_mat70.m[0][0], 16);
    n.m_name = ReadString(file);
  }
  ReadFloats(file, &m_field20.x, 3);
  m_field2C = simmy::read_u32(file);
  m_field30 = simmy::read_u32(file);
}

// 80368A54
void ERCharacter::Refresh(EFile* file) {
  if (file) Load(*file);
}

// 80368A94
int ERCharacter::FindNode(char* name) {
  for (std::size_t i = 0; i < m_nodes.size(); ++i) {
    const std::string& n = m_nodes[i].m_name;
    std::size_t k = 0;
    while (k < n.size() && name[k] && std::tolower(static_cast<unsigned char>(n[k])) ==
                                          std::tolower(static_cast<unsigned char>(name[k])))
      ++k;
    if (k == n.size() && !name[k]) return static_cast<int>(i);
  }
  return -1;
}

// 80368B18
void ERCharacter::PrintNodes() {}

// 80368D64
ECharacterNode* ERCharacter::GetNode(int index) { return &m_nodes[index]; }

// 80323224
EResource* ECharacterManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* character = new ERCharacter;  // GC: Alloc(0x34, 8) from this manager, then the constructor
  character->m_id = id;
  character->Load(*file);
  return character;
}

// 8032333C
// Skeletons stay cached (in the free list) after their last reference goes, as animations do.
int ECharacterManager::CanCache(EResource*) { return 1; }
