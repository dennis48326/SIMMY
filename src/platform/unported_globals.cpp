// Temporary home for globals of files that are not ported yet (see simmy/unported_globals.hpp).
#include "simmy/unported_globals.hpp"

EClock _sysclock;
EScratchBuffMan _scratchBuffMan;
int _iVideoMode = 0;
float _retracetime = 1.0f / 60.0f;
int _enable_trace_log = 0;
int _d_retraces = 1;
EMovieMan _movieman;

EShaderManager _shaderman;
ETextureManager _textureman;
EAnimManager _animman;
EModelManager _modelman;
ECharacterManager _characterman;
EDatasetManager _datasetman;
EQuickdataManager _quickdataman;
EFlashManager _flashman;
ParticleManager _particleman;
EffectsAttachmentManager g_effectsAttachmentMan;
EffectsEmitterManager g_effectsEmitterMan;
EffectsSequencerManager g_effectsSequencerMan;
EFontManager _fontman;
EEdithTreeSetMan _edithtreesetman;
EBinaryManager _binaryman;
EAmbientScoreManager g_ambientscoreman;
ESoundTrackDataManager g_soundtrackdataman;
ESoundEventManager g_soundeventman;
EResPrefetch _resPrefetch;
EFrameAllocGroup _frag;
ETweak _tweak;
ELevelManager _levelman;
EClock g_playerCheats;
EGlobal _globals;
int g_quickdatamanLanguage = 0;
NghResFile* Globs_pNghResFile = nullptr;
int SerializeUIGameData_m_bInitialized = 0;
int gbDrawUI = 1;    // 804FEA28 (game/e_rleveltuning.cpp)
int gbUpdateUI = 1;  // 804FEA2C
float _gamedt = 0.0f;  // 804FF074 (engine; set by the game states)
ERC* g_pRC = nullptr;  // 804FEA40 (game/e_rleveltuning.cpp)
