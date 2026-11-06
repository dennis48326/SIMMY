// @class ECharacterNode after EResource EMat4 EVec3 EQuat
// @class ERCharacter
// A skeleton (engine/e_rcharacter.cpp, 'CHRC' version 9): a list of nodes, each with its parent, its
// children, the pivot it rotates about, its bind orientation, two matrices and a name. EAnimController
// animates one; cameras and plumb bobs are characters too.
#pragma once
#include <string>
#include <vector>

// 0xB8 bytes on the GameCube.
struct ECharacterNode {
  int m_parent = -1;              // +0x00 node index (-1 for the root)
  std::vector<int> m_children;    // +0x04 TArray<int>
  EVec3 m_pivot;                  // +0x10
  EQuat m_orient;                 // +0x1C bind orientation
  EMat4 m_mat30;                  // +0x30
  EMat4 m_mat70;                  // +0x70 (EAnimController::CalcMatrix: the frame scale is applied in)
  int m_flagB0 = 0;               // +0xB0
  std::string m_name;             // +0xB4 EString
};

// 0x34 bytes.
struct ERCharacter : public EResource {
  ERCharacter();  // 803686E8
  ~ERCharacter() override;  // 8036872C
  SIMMY_STORABLE_DECL(ERCharacter)
  void Load(EFile& file) override;  // 80368794
  void Refresh(EFile* file) override;  // 80368A54
  using EResource::Refresh;
  int FindNode(char* name);  // 80368A94
  static void PrintNodes();  // 80368B18
  static void RegisterType(unsigned short);  // 80368C8C
  void CreateCopy() const;  // 80368CE0
  ECharacterNode* GetNode(int index);  // 80368D64
  int GetNodeCount() const { return static_cast<int>(m_nodes.size()); }  // (GC: m_nodes count at +0x18)

  std::vector<ECharacterNode> m_nodes;  // +0x14 TArray<ECharacterNode>
  EVec3 m_field20;                      // +0x20
  unsigned int m_field2C = 0;           // +0x2C
  unsigned int m_field30 = 0;           // +0x30
};
