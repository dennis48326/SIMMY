// @class EResource after EStorable
// Base of every loadable resource (engine/e_resource.cpp). EStorable's type/serialisation virtuals
// come first, as in the original vtable.
#pragma once

struct EResource : public EStorable {
  EResource();
  SIMMY_STORABLE_DECL(EResource)
  virtual ~EResource();
  virtual void Read(EStream &);  // 80311EB8
  virtual void Write(EStream &);  // 80311E64
  virtual void Load(EFile &);  // 80311F14
  virtual int IsSafeToDelete();
  virtual void Init();
  virtual void Refresh(EFile* file);
  virtual void DelRefSubResources();
  virtual void AddRefSubResources();
  virtual int TryIncrementSubResources();
  virtual void DetachClone();

  void DelRef();
  void Detach();
  void AddRef();
  void Refresh();
  static void RegisterType(unsigned short);  // 803121E0
  void CreateCopy() const;  // 80312234
  int IsManaged() const;
  unsigned int GetResId() const;
  void SetResId(unsigned int id);
  int GetNumRefs();
  void SetResSize(unsigned int size);
  unsigned int GetResSize();
  void SetResName(char* name);
  const char* GetResName();
  void SetCheckPoint(unsigned int);
  unsigned int GetCheckPoint();

  EResourceManager* m_manager = nullptr;  // +0x04, null for unmanaged resources
  unsigned int m_id = 0;                  // +0x08, EChecksum::ComputeSymbol of the name
  short m_refs = 1;                       // +0x0C
  short m_loading = 0;                    // +0x0E, in-flight sub-resource loads
  unsigned int m_flags = 0x80;            // +0x10, size << 8 | flags (0x80 = cacheable)
};
