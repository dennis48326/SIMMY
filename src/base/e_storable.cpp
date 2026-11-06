// Port of base/e_storable.cpp and EStorable's inline members.
#include "simmy/types_gen.hpp"

ETypeInfo EStorable::m_typeInfo;  // 804FCBF8
static const ETypeInfo* const s_registerEStorable = EStorable::m_typeInfo.Register(
    &EStorable::New, [](void* p) { EStorable::Construct(static_cast<EStorable*>(p)); },
    [](void* p) { EStorable::Destruct(static_cast<EStorable*>(p)); }, 0, "EStorable", &EStorable::m_typeInfo);

// 803C13E0
void EStorable::SafeDelete() { delete this; }
// 803C1420
ETypeInfo* EStorable::GetTypeInfo() const { return &m_typeInfo; }
// 803C142C
const char* EStorable::GetTypeName() const { return m_typeInfo.m_name; }
// 803C1438
unsigned int EStorable::GetTypeKey() const { return m_typeInfo.m_key; }
// 803C1444
unsigned short EStorable::GetTypeVersion() const { return m_typeInfo.m_version; }
// 803C1450
EStorable::~EStorable() {}
// 803C1484
void EStorable::Read(EStream&) {}
// 803C1488
void EStorable::Write(EStream&) {}
// 803C148C
void EStorable::Load(EFile&) {}
// 803C2448
EStorable* EStorable::New() { return new EStorable; }
// 803C247C
void EStorable::Construct(EStorable* p) { new (p) EStorable; }
// 803C248C
void EStorable::Destruct(EStorable* p) { p->~EStorable(); }

// 802D1420
void EStorable::AssertValid(ETypeInfo*) const {}

// 802D1440
int EStorable::IsDerivedFrom(ETypeInfo* type) const { return this ? GetTypeInfo()->IsDerivedFrom(type) : 0; }

// 802D1498
bool EStorable::IsExactType(ETypeInfo* type) const { return this && GetTypeInfo() == type; }

// 802D14F4
const EStorable* EStorable::DynamicCast(ETypeInfo* type) const { return this && IsDerivedFrom(type) ? this : nullptr; }

// 802D1540
EStorable* EStorable::DynamicCast(ETypeInfo* type) { return this && IsDerivedFrom(type) ? this : nullptr; }
