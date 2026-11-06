// GENERATED stubs for unported functions of engine/e_rtquantize4d.cpp. Do not edit.
#include "simmy/types_gen.hpp"

ERTQuantize4D::ERTQuantize4D() { SIMMY_STUB("ERTQuantize4D::ERTQuantize4D(void)", 0x8035EAFC); }
ERTQuantize4D::~ERTQuantize4D() { SIMMY_STUB("ERTQuantize4D::~ERTQuantize4D(void)", 0x8035EB34); }
void ERTQuantize4D::Deallocate() { SIMMY_STUB("ERTQuantize4D::Deallocate(void)", 0x8035EB84); }
RET_UNKNOWN ERTQuantize4D::Init(unsigned int, unsigned int, void *(*)(unsigned int), void (*)(void *), bool) { SIMMY_STUB("ERTQuantize4D::Init(unsigned int, unsigned int, void *(*)(unsigned int), void (*)(void *), bool)", 0x8035EBD4); return {}; }
void ERTQuantize4D::DefaultAlloc(unsigned int) { SIMMY_STUB("ERTQuantize4D::DefaultAlloc(unsigned int)", 0x8035EE2C); }
void ERTQuantize4D::DefaultFree(void *) { SIMMY_STUB("ERTQuantize4D::DefaultFree(void *)", 0x8035EE64); }
void ERTQuantize4D::TextureAlloc(unsigned int) { SIMMY_STUB("ERTQuantize4D::TextureAlloc(unsigned int)", 0x8035EE98); }
void ERTQuantize4D::TextureFree(void *) { SIMMY_STUB("ERTQuantize4D::TextureFree(void *)", 0x8035EED0); }
void ERTQuantize4D::InitializeCube() { SIMMY_STUB("ERTQuantize4D::InitializeCube(void)", 0x8035EF04); }
void ERTQuantize4D::InitializeNode(unsigned char, unsigned int, ERTQ4Node *, EVec4 &) { SIMMY_STUB("ERTQuantize4D::InitializeNode(unsigned char, unsigned int, ERTQ4Node *, EVec4 &)", 0x8035EFF4); }
RET_UNKNOWN ERTQuantize4D::AddPixel(unsigned char *) { SIMMY_STUB("ERTQuantize4D::AddPixel(unsigned char *)", 0x8035F15C); return {}; }
void ERTQuantize4D::FlushAdd(ERTQ4CacheNode &) { SIMMY_STUB("ERTQuantize4D::FlushAdd(ERTQ4CacheNode &)", 0x8035F220); }
void ERTQuantize4D::TransformToYuva(unsigned char *, EVec4 &) { SIMMY_STUB("ERTQuantize4D::TransformToYuva(unsigned char *, EVec4 &)", 0x8035F290); }
void ERTQuantize4D::TransformFromYUVA(EVec4 &, unsigned char *) { SIMMY_STUB("ERTQuantize4D::TransformFromYUVA(EVec4 &, unsigned char *)", 0x8035F484); }
void ERTQuantize4D::Classify(EVec4 &, int) { SIMMY_STUB("ERTQuantize4D::Classify(EVec4 &, int)", 0x8035F6E0); }
void ERTQuantize4D::PruneLevel(ERTQ4Node *) { SIMMY_STUB("ERTQuantize4D::PruneLevel(ERTQ4Node *)", 0x8035FAD8); }
void ERTQuantize4D::PruneChild(ERTQ4Node *) { SIMMY_STUB("ERTQuantize4D::PruneChild(ERTQ4Node *)", 0x8035FB7C); }
void ERTQuantize4D::Compute() { SIMMY_STUB("ERTQuantize4D::Compute(void)", 0x8035FC50); }
void ERTQuantize4D::Reduction() { SIMMY_STUB("ERTQuantize4D::Reduction(void)", 0x8035FCE8); }
void ERTQuantize4D::Reduce(ERTQ4Node *) { SIMMY_STUB("ERTQuantize4D::Reduce(ERTQ4Node *)", 0x8035FD78); }
void ERTQuantize4D::MColormap(ERTQ4Node *) { SIMMY_STUB("ERTQuantize4D::MColormap(ERTQ4Node *)", 0x8035FE54); }
RET_UNKNOWN ERTQuantize4D::GetPaletteSize() { SIMMY_STUB("ERTQuantize4D::GetPaletteSize(void)", 0x8035FF48); return {}; }
void ERTQuantize4D::GetPaletteEntry(int, unsigned char *) { SIMMY_STUB("ERTQuantize4D::GetPaletteEntry(int, unsigned char *)", 0x8035FF50); }
RET_UNKNOWN ERTQuantize4D::GetClosestColor(unsigned char *) { SIMMY_STUB("ERTQuantize4D::GetClosestColor(unsigned char *)", 0x8035FF7C); return {}; }
void ERTQuantize4D::ClosestColor(ERTQ4Node *) { SIMMY_STUB("ERTQuantize4D::ClosestColor(ERTQ4Node *)", 0x803602BC); }
RET_UNKNOWN ERTQuantize4D::AllocNode() { SIMMY_STUB("ERTQuantize4D::AllocNode(void)", 0x80360450); return {}; }
void ERTQuantize4D::FreeNode(unsigned short) { SIMMY_STUB("ERTQuantize4D::FreeNode(unsigned short)", 0x8036047C); }
RET_UNKNOWN ERTQuantize4D::IndexToNode(unsigned short) { SIMMY_STUB("ERTQuantize4D::IndexToNode(unsigned short)", 0x8036049C); return {}; }
RET_UNKNOWN ERTQuantize4D::NodeToIndex(ERTQ4Node *) { SIMMY_STUB("ERTQuantize4D::NodeToIndex(ERTQ4Node *)", 0x803604BC); return {}; }
void ERTQuantize4D::EVecToFVec(EVec4 &, unsigned short *) { SIMMY_STUB("ERTQuantize4D::EVecToFVec(EVec4 &, unsigned short *)", 0x803604E8); }
void ERTQuantize4D::FVecToEVec(unsigned short *, EVec4 &) { SIMMY_STUB("ERTQuantize4D::FVecToEVec(unsigned short *, EVec4 &)", 0x80360568); }
