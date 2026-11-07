// Globals owned by source files that are not ported yet. Each is defined (null / zero) in
// src/platform/unported_globals.cpp; move the definition into the real file when it is ported.
#pragma once
#include "simmy/types_gen.hpp"

extern EApp* _pApp;                     // 804FF058, set by EApp::EApp (engine/e_app.cpp)
extern EEngine* _pEngine;               // 804FF190, set by ENgcEngine::ENgcEngine
extern ENgcClockMan* _pClockMan;        // 804FF17C, engine/e_ngcclock.cpp
extern ENgcFileSystem _eorFileSys;      // 8049D684, base/e_ngcfilesystem.cpp
extern EGraphics* _pGfx;                // 804FF208, engine/e_ngcgraphics.cpp
extern ENgcScheduler* _pSched;          // 804FF3E8, engine/e_ngcscheduler.cpp

extern EControllerManager* _pCtrlMan;   // 804FF184, engine/e_ngcctrlmanager.cpp
extern EMemoryCard* _pMemoryCard;      // 804FF210, engine/e_ngcmemorycard.cpp
extern ENgcAudio* _pActualAudio;        // 804FF15C, engine/e_ngcaudio.cpp
extern EResourceLoaderImpl* _pResLoader;  // 805000A4, engine/e_resloader.cpp
extern EClock _sysclock;                // system clock started by EEngine::Init
extern EScratchBuffMan _scratchBuffMan;
extern int _iVideoMode;                 // 80501104, 0 = NTSC 60 Hz, 1 = PAL 50 Hz
extern float _retracetime;              // 80500048, seconds per retrace
extern int _enable_trace_log;
extern int _d_retraces;                 // 804FF06C, retraces per frame
extern EMovieMan _movieman;             // 804BA17C, movie resource manager

// Resource managers (each owned by its own, not yet ported, source file).
extern EShaderManager _shaderman;               // 804BBBDC
extern ETextureManager _textureman;
extern EAnimManager _animman;
extern EModelManager _modelman;
extern ECharacterManager _characterman;
extern EDatasetManager _datasetman;
extern EQuickdataManager _quickdataman;
extern EFlashManager _flashman;
extern ParticleManager _particleman;
extern EffectsAttachmentManager g_effectsAttachmentMan;
extern EffectsEmitterManager g_effectsEmitterMan;
extern EffectsSequencerManager g_effectsSequencerMan;
extern EFontManager _fontman;
extern EEdithTreeSetMan _edithtreesetman;
extern EAudioStreamManager _audiostreamman;     // 804B3858, engine/e_audiostreamman.cpp
extern EBinaryManager _binaryman;
extern EAmbientScoreManager g_ambientscoreman;
extern ESoundTrackDataManager g_soundtrackdataman;
extern ESoundEventManager g_soundeventman;
extern EResPrefetch _resPrefetch;               // 804B13A0
extern EAudioSampleManager* _pAudiosampleman;   // 805000F4, engine/e_ngcaudiosampleman.cpp
extern BackgroundImpl* _pBackground;            // 8050003C, engine/background.cpp
extern float _dt;                       // 804FF070, seconds this frame (EEngine::PreFrameUpdate)
extern unsigned int _retracecount;      // 804FF068, retraces since boot (EEngine::RetraceUpdate)
extern unsigned int _lastCtrlUpdate;    // 804FF114, engine/e_ctrlmanager.cpp
extern EFrameAllocGroup _frag;          // 804B0D5C, engine/e_frameallocgroup.cpp
extern ETweak _tweak;                   // 804B0D94, engine/e_tweak.cpp
extern unsigned int _evenodd;           // 804FF060 (engine/e_engine.cpp)
extern int _framecount;                 // 804FF064
extern float _rendtime, _rendtime_m1;   // 804FF08C, render seconds of the last two frames
extern ELevelManager _levelman;          // 80487E4C, game/e_levelman.cpp
extern PlayerCheats g_playerCheats;      // 8046E1E0 (game/appmain.cpp)
extern EGlobal _globals;                 // 80475DCC, game/eglobal.cpp
extern int g_quickdatamanLanguage;       // _quickdataman + 0xD20
extern NghResFile* Globs_pNghResFile;    // 804FDBE8 Globs::pNghResFile
extern int SerializeUIGameData_m_bInitialized;  // 804FDE4C
extern GameData _gd;                     // 80475B84, game/gamedata.cpp
extern ESimsApp _app;                    // game/appmain.cpp (the application object)
extern int gbDrawUI;                     // 804FEA28, draw the Apt UI
extern int gbUpdateUI;                   // 804FEA2C, update the Apt UI
extern float _dtlim20;                   // 80500050, _dt capped at 1/20 s (EEngine)
extern float _dtlim15;                   // 80500054, _dt capped at 1/15 s (EEngine)
extern float _gamedt;                    // 804FF074, game time this frame (0 when paused)
extern ERC* g_pRC;                       // 804FEA40, the context the level is drawing with
