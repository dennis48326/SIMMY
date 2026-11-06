// @class SimImageMaker
// A sim's portrait (game/simimagemaker.cpp): the sim's body parts posed (the gender's skeleton, the
// portrait pose) and drawn into a render surface through a 14 degree camera on the head, over a pink
// background (the transparency key), under three lights; the surface becomes a texture (vertically
// flipped, sepia if asked), then the final image: 32-bit, 16-bit (RGB5A3) or 8-bit (C8 with a quantised
// 32-bit palette; the pink and the image's right and top edges become a transparent last entry; a
// background texture, if given, shows through instead). A 32x32 image is rendered at 64x64 and halved.
// Used for the portraits of sims made in Create-a-Sim (CasSimRenderer::CreateFaceImage, the 'thmb'
// thumbnails) and sims without one (ESim::CreateThumbnail).
//
// PC: the surface's pixels come back from the GPU some frames after it is drawn (aurora's readback;
// ENgcRenderSurface), so CreateImage draws at once as on GC but returns the final texture blank (all
// transparent) and finishes it once the pixels are there: SimImageMaker::ServiceDeferredImages, called
// once a frame (ESimsApp::Update), does the GC's copy, flip, halving and CopyToFinalImage into that same
// texture. The texture is the caller's at once (it can be shown, wrapped in a shader, saved later).
#pragma once

#include <memory>
#include <vector>

// 0x220 bytes.
struct SimImageMaker {
  SimImageMaker();  // 8006E814
  ~SimImageMaker();  // 8006E918
  int Init();  // 8006E964
  void Shutdown();  // 8006E988
  void Reset();  // 8006E9D8
  void SetToDefaultValues();  // 8006EA14
  void SetGender(bool male);  // 8006EBCC
  void SetSkinShader(EShader* shader);  // 8006EBD4
  void SetModel(eBodyPartS2C part, EIStaticModel* model);  // 8006EBDC
  void SetPortraitModels(SimModel* model);  // 8006EBF4
  void OverrideImageSize(unsigned int width, unsigned int height, unsigned int bpp);  // 8006ECAC
  void OverrideLights(ELights& lights);  // 8006ECBC
  void OverridePose(unsigned int anim, EVec3& pos, EVec3& target, EVec3& up, float fov);  // 8006ED18
  void OverrideBackground(unsigned int texture);  // 8006ED78
  ETexture* CreateImage();  // 8006EDE0
  void InitWindow(E3DWindow& window, ERC* rc);  // 8006F25C
  void PoseSim(ERC* rc);  // 8006F408
  ETexture* CopyToFinalImage(ETexture* image);  // 8006F538
  void UpdateRepShaders(int);  // 8006FE40
  ETexture* CreateImage32x32();  // 80070154

  // PC: the deferred half of CreateImage (see above).
  static void ServiceDeferredImages();

  unsigned int m_width = 0x20;    // +0x00
  unsigned int m_height = 0x20;   // +0x04
  unsigned int m_bpp = 8;         // +0x08 the final image's bits per pixel (8, 16 or 32)
  int m_sepia = 0;                // +0x0C
  EAnimController m_anim;         // +0x10
  int m_male = 0;                 // +0x88
  EShader* m_skinShader = nullptr;  // +0x8C
  EIStaticModel* m_models[0x1a] = {};  // +0x90 by eBodyPartS2C
  ELights m_lights;               // +0xF8
  EVec3 m_cameraPos;              // +0x1F0
  EVec3 m_cameraTarget;           // +0x1FC
  EVec3 m_cameraUp;               // +0x208
  float m_fov = 14.0f;            // +0x214
  unsigned int m_poseAnim = 0xfccc90beu;  // +0x218
  ERTexture* m_background = nullptr;  // +0x21C OverrideBackground's texture

  // PC: what CreateImage leaves for ServiceDeferredImages.
  struct Deferred {
    ERenderSurface* surface = nullptr;
    ETexture* final = nullptr;         // the texture CreateImage returned
    unsigned int width = 0, height = 0;  // the rendered size
    unsigned int finalWidth = 0, finalHeight = 0, bpp = 8;
    int sepia = 0;
    bool halve = false;                // CreateImage32x32: rendered at twice the size
    unsigned int background = 0;       // the background texture's id (0: none)
    int frames = 0;
  };
  ETexture* m_finalTarget = nullptr;  // PC: CopyToFinalImage fills this one instead of making one
};
