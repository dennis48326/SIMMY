// GENERATED stubs for unported functions of engine/imageprocmanager.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void ImageProcessingManager::Open(int) { SIMMY_STUB("ImageProcessingManager::Open(int)", 0x803293F8); }
void ImageProcessingManager::Close() { SIMMY_STUB("ImageProcessingManager::Close(void)", 0x80329460); }
void ImageProcessingManager::SetStageFilter(int, ImageProcessingManager__FilterDescriptor *) { SIMMY_STUB("ImageProcessingManager::SetStageFilter(int, ImageProcessingManager::FilterDescriptor *)", 0x8032949C); }
void ImageProcessingManager::SetStageCallback(int, void (*)(ETexture *)) { SIMMY_STUB("ImageProcessingManager::SetStageCallback(int, void (*)(ETexture *))", 0x80329508); }
void ImageProcessingManager::ExecuteAsync(ETexture *, ETexture *, void (*)(ETexture *)) { SIMMY_STUB("ImageProcessingManager::ExecuteAsync(ETexture *, ETexture *, void (*)(ETexture *))", 0x8032951C); }
void ImageProcessingManager::Execute(ETexture *, ETexture *) { SIMMY_STUB("ImageProcessingManager::Execute(ETexture *, ETexture *)", 0x80329558); }
RET_UNKNOWN ImageProcessingManager::HasBaseColorParam(ImageProcessingManager__eFilter) { SIMMY_STUB("ImageProcessingManager::HasBaseColorParam(ImageProcessingManager::eFilter)", 0x803297C4); return {}; }
RET_UNKNOWN ImageProcessingManager::HasHSVParam(ImageProcessingManager__eFilter) { SIMMY_STUB("ImageProcessingManager::HasHSVParam(ImageProcessingManager::eFilter)", 0x803297DC); return {}; }
RET_UNKNOWN ImageProcessingManager::HasThreshParam(ImageProcessingManager__eFilter) { SIMMY_STUB("ImageProcessingManager::HasThreshParam(ImageProcessingManager::eFilter)", 0x803297F4); return {}; }
RET_UNKNOWN ImageProcessingManager::HasBlendParam(ImageProcessingManager__eFilter) { SIMMY_STUB("ImageProcessingManager::HasBlendParam(ImageProcessingManager::eFilter)", 0x8032980C); return {}; }
void ImageProcessingManager__FilterParams::Reset() { SIMMY_STUB("ImageProcessingManager::FilterParams::Reset(void)", 0x80329824); }
RET_UNKNOWN calcWeightSum(int *, int) { SIMMY_STUB("calcWeightSum(int *, int)", 0x80329868); return {}; }
void ApplyConvolution(short *, short *, int, int, int *, int, int, int, ImageProcessingManager__BaseColor) { SIMMY_STUB("ApplyConvolution(short *, short *, int, int, int *, int, int, int, ImageProcessingManager::BaseColor)", 0x8032989C); }
RET_UNKNOWN Sharpen(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Sharpen(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x80329BE0); return {}; }
RET_UNKNOWN Sharpen_MeanRemoval(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Sharpen_MeanRemoval(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x80329C64); return {}; }
RET_UNKNOWN GaussianBlur(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("GaussianBlur(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x80329CE8); return {}; }
RET_UNKNOWN Emboss(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Emboss(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x80329DD4); return {}; }
RET_UNKNOWN Emboss2(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Emboss2(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x80329EBC); return {}; }
void Laplacian(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Laplacian(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032A024); }
RET_UNKNOWN MergeEdges_Color(short *, short *, short *, int) { SIMMY_STUB("MergeEdges_Color(short *, short *, short *, int)", 0x8032A10C); return {}; }
RET_UNKNOWN ImageProcessingManager::LinkTexture(unsigned int, unsigned int) { SIMMY_STUB("ImageProcessingManager::LinkTexture(unsigned int, unsigned int)", 0x8032A290); return {}; }
void ImageProcessingManager::DummyFree(void *) { SIMMY_STUB("ImageProcessingManager::DummyFree(void *)", 0x8032A298); }
void ImageProcessingManager::CompositeEdges(short *, short *, int, int, ImageProcessingManager__BlendParams *) { SIMMY_STUB("ImageProcessingManager::CompositeEdges(short *, short *, int, int, ImageProcessingManager::BlendParams *)", 0x8032A29C); }
RET_UNKNOWN EdgeDetect_Prewitt(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("EdgeDetect_Prewitt(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032A804); return {}; }
RET_UNKNOWN EdgeDetect_Sobel(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("EdgeDetect_Sobel(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032AA68); return {}; }
RET_UNKNOWN EdgeDetect_OnePass(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("EdgeDetect_OnePass(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032AC80); return {}; }
RET_UNKNOWN EdgeDetect_NonConvolved(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("EdgeDetect_NonConvolved(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032AD68); return {}; }
void Posterize(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Posterize(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032AEC0); }
void Solarize(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Solarize(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032AF98); }
void InverseSolarize(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("InverseSolarize(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032B004); }
void sort3IntPtrs(int **) { SIMMY_STUB("sort3IntPtrs(int **)", 0x8032B070); }
void Saturate(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("Saturate(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032B0D4); }
RET_UNKNOWN CustomConvolution(short *, short *, int, int, ImageProcessingManager__FilterParams *) { SIMMY_STUB("CustomConvolution(short *, short *, int, int, ImageProcessingManager::FilterParams *)", 0x8032B554); return {}; }
