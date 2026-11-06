// @class EStorable after ETypeInfo
// Base of everything that can be saved and loaded as part of an object graph (base/e_storable.cpp).
// Read/Write go through an EStream; Load reads from a file (the archives).
#pragma once

struct EStorable {
  EStorable() = default;
  // Virtuals, in vtable order.
  virtual void SafeDelete();
  virtual ETypeInfo* GetTypeInfo() const;
  virtual const char* GetTypeName() const;
  virtual unsigned int GetTypeKey() const;
  virtual unsigned short GetTypeVersion() const;
  virtual ~EStorable();
  virtual void Read(EStream& stream);
  virtual void Write(EStream& stream);
  virtual void Load(EFile& file);

  RET_UNKNOWN CreateCopy() const;
  void AssertValid(ETypeInfo*) const;
  int IsDerivedFrom(ETypeInfo* type) const;
  bool IsExactType(ETypeInfo* type) const;
  const EStorable* DynamicCast(ETypeInfo* type) const;
  EStorable* DynamicCast(ETypeInfo* type);
  static EStorable* New();
  static void Construct(EStorable* p);
  static void Destruct(EStorable* p);

  static ETypeInfo m_typeInfo;
};

// The per-class boilerplate of a ported storable: its type info (registered at static-init time, as in
// the original), the virtual accessors and New/Construct/Destruct. The class declares the members
// with SIMMY_STORABLE_DECL.
#define SIMMY_STORABLE_DECL(Class)                     \
  ETypeInfo* GetTypeInfo() const override;             \
  const char* GetTypeName() const override;            \
  unsigned int GetTypeKey() const override;            \
  unsigned short GetTypeVersion() const override;      \
  void SafeDelete() override;                          \
  static EStorable* New();                             \
  static void Construct(Class* p);                     \
  static void Destruct(Class* p);                      \
  static ETypeInfo* GetTypeInfoStatic();               \
  static unsigned short GetReadVersion();              \
  static ETypeInfo m_typeInfo;

#define SIMMY_STORABLE_IMPL(Class, Name, Version, Parent)                                          \
  ETypeInfo Class::m_typeInfo;                                                                      \
  static const ETypeInfo* const s_register_##Class = Class::m_typeInfo.Register(                    \
      &Class::New, [](void* p) { Class::Construct(static_cast<Class*>(p)); },                       \
      [](void* p) { Class::Destruct(static_cast<Class*>(p)); }, Version, Name, &Parent::m_typeInfo); \
  ETypeInfo* Class::GetTypeInfo() const { return &m_typeInfo; }                                    \
  const char* Class::GetTypeName() const { return m_typeInfo.m_name; }                             \
  unsigned int Class::GetTypeKey() const { return m_typeInfo.m_key; }                              \
  unsigned short Class::GetTypeVersion() const { return m_typeInfo.m_version; }                    \
  void Class::SafeDelete() { delete this; }                                                         \
  EStorable* Class::New() { return new Class; }                                                     \
  void Class::Construct(Class* p) { new (p) Class; }                                                \
  void Class::Destruct(Class* p) { p->~Class(); }                                                   \
  ETypeInfo* Class::GetTypeInfoStatic() { return &m_typeInfo; }                                    \
  unsigned short Class::GetReadVersion() { return m_typeInfo.m_readVersion; }
