// @class EMemoryCard after EMC_SaveType EMC_Operation
// The platform-neutral memory card interface (engine/e_memorycard.h). Results: 1 = OK, 0 = failed,
// -1 = no card, -3 = no space, -4 = card broken, -5 = canceled, -6 = encoding, -7 = no such file,
// -8 = I/O error (card dead), -9 = wrong device.
#pragma once

struct EMemoryCard {
  // Virtuals, in vtable order. The pure ones are implemented by ENgcMemoryCard.
  virtual ~EMemoryCard();
  virtual int InitMemoryCard() = 0;
  virtual int UnInitMemoryCard() = 0;
  virtual int OpenForIO(char* name, unsigned int card);
  virtual int CloseForIO(char* name, unsigned int card);
  virtual int GetFileRefCount(char* name, unsigned int card);
  virtual int LoadDataChunkS(char* name, unsigned int card, unsigned int size, unsigned int offset, void* dst);
  virtual int LoadDataS(char* name, unsigned int card, unsigned int size, void* dst) = 0;
  virtual int SaveDataS(char* name, unsigned int card, unsigned int size, void* src) = 0;
  virtual int SaveDataChunkS(char* name, unsigned int card, unsigned int size, unsigned int offset, void* src);
  virtual int DeleteDataS(char* name, unsigned int card) = 0;
  virtual int CreateFileS(char* name, unsigned int card, unsigned int size);
  virtual int FormatCardS(unsigned int card) = 0;
  virtual int UnFormatCardS(unsigned int card) = 0;
  virtual void SetMemCardCopyProtection(unsigned int card);
  virtual void IsSpaceAvailable(unsigned int card, unsigned int bytes, bool& available) = 0;
  virtual int IsSpaceAvailable(unsigned int card, EMC_SaveType type, bool& available) = 0;
  virtual int PollCards(bool force);
  virtual int IsPolledCardAvailable(unsigned int card);
  virtual int IsCardAvailable(unsigned int card) = 0;
  virtual bool IsWrongDevice(unsigned int card);
  virtual int IsCardFormated(unsigned int card, bool& formatted) = 0;
  virtual void SetupSaveTypes(EMC_SaveType type, unsigned int size) = 0;
  virtual void SetGameCode(char* code) = 0;
  virtual int DoesFileExist(char* name, unsigned int card, bool& exists) = 0;
  virtual int LoadDataA(char*, unsigned int, unsigned int, void*, EMC_Operation*) = 0;
  virtual int SaveDataA(char*, unsigned int, unsigned int, void*, EMC_Operation*) = 0;
  virtual int DeleteDataA(char*, unsigned int, unsigned int, EMC_Operation*) = 0;
  virtual int FormatCardA(unsigned int, EMC_Operation*) = 0;
  virtual int UnFormatCardA(unsigned int, EMC_Operation*) = 0;
  virtual int UpdateOperation(EMC_Operation op, bool& done) = 0;
  virtual int GetFreeSpaceS(unsigned int card, unsigned int& bytes) = 0;
  virtual int CheckForOverwriteSpace(unsigned int card, unsigned int size, EMC_SaveType type, bool& ok) = 0;
  virtual bool AnyCardsPresent(int& card) = 0;
  virtual int GetFileList(unsigned int card, char* filter, unsigned int max, char (*names)[32]) = 0;
  virtual int GetFileListA(unsigned int card, char* filter, unsigned int max, char (*names)[32], EMC_Operation*) = 0;
  virtual int GetSectorSize(int card, unsigned int& size) = 0;
};
