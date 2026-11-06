// @class ENgcMemoryCard after EMemoryCard
// GameCube memory cards through the CARD API (engine/e_ngcmemorycard.cpp). On PC, aurora's CARD stores
// each slot as a folder of Dolphin-compatible .gci files.
#pragma once

struct ENgcMemoryCard : public EMemoryCard {
  ENgcMemoryCard();
  ~ENgcMemoryCard() override;
  int IsCardOperationInProgress();
  void BeginCardOperation();
  void EndCardOperation();
  static void SetComments(unsigned short* line1, unsigned short* line2);
  int CheckAndWriteBannerAndIcon(char* name, unsigned int card);
  int GetFreeFilesS(unsigned int card, int& files);
  void IsEnoughFiles(unsigned int card, unsigned int files, bool& enough);

  int InitMemoryCard() override;
  int UnInitMemoryCard() override;
  int OpenForIO(char* name, unsigned int card) override;
  int CloseForIO(char* name, unsigned int card) override;
  int LoadDataChunkS(char* name, unsigned int card, unsigned int size, unsigned int offset, void* dst) override;
  int LoadDataS(char* name, unsigned int card, unsigned int size, void* dst) override;
  int SaveDataS(char* name, unsigned int card, unsigned int size, void* src) override;
  int SaveDataChunkS(char* name, unsigned int card, unsigned int size, unsigned int offset, void* src) override;
  int DeleteDataS(char* name, unsigned int card) override;
  int CreateFileS(char* name, unsigned int card, unsigned int size) override;
  int FormatCardS(unsigned int card) override;
  int UnFormatCardS(unsigned int card) override;
  void IsSpaceAvailable(unsigned int card, unsigned int bytes, bool& available) override;
  int IsSpaceAvailable(unsigned int card, EMC_SaveType type, bool& available) override;
  int IsCardAvailable(unsigned int card) override;
  bool IsWrongDevice(unsigned int card) override;
  int IsCardFormated(unsigned int card, bool& formatted) override;
  void SetupSaveTypes(EMC_SaveType type, unsigned int size) override;
  void SetGameCode(char* code) override;
  int DoesFileExist(char* name, unsigned int card, bool& exists) override;
  int LoadDataA(char*, unsigned int, unsigned int, void*, EMC_Operation*) override;
  int SaveDataA(char*, unsigned int, unsigned int, void*, EMC_Operation*) override;
  int DeleteDataA(char*, unsigned int, unsigned int, EMC_Operation*) override;
  int FormatCardA(unsigned int, EMC_Operation*) override;
  int UnFormatCardA(unsigned int, EMC_Operation*) override;
  int UpdateOperation(EMC_Operation op, bool& done) override;
  int GetFreeSpaceS(unsigned int card, unsigned int& bytes) override;
  int CheckForOverwriteSpace(unsigned int card, unsigned int size, EMC_SaveType type, bool& ok) override;
  bool AnyCardsPresent(int& card) override;
  int GetFileList(unsigned int card, char* filter, unsigned int max, char (*names)[32]) override;
  int GetFileListA(unsigned int card, char* filter, unsigned int max, char (*names)[32], EMC_Operation*) override;
  int GetSectorSize(int card, unsigned int& size) override;

  int m_operations = 0;  // 0x04 card operations in progress
};
