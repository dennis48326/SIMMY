// GENERATED stubs for unported functions of game/inventory.cpp. Do not edit.
#include "simmy/types_gen.hpp"

BBI__InventoryItem::BBI__InventoryItem() { SIMMY_STUB("BBI::InventoryItem::InventoryItem(void)", 0x8004A060); }
BBI__InventoryItem::~BBI__InventoryItem() { SIMMY_STUB("BBI::InventoryItem::~InventoryItem(void)", 0x8004A090); }
void BBI__InventoryItem::SetItemCategory(BBI__InventoryItem__eItemCategory) { SIMMY_STUB("BBI::InventoryItem::SetItemCategory(BBI::InventoryItem::eItemCategory)", 0x8004A0C4); }
RET_UNKNOWN BBI__InventoryItem::GetItemCategory() const { SIMMY_STUB("BBI::InventoryItem::GetItemCategory(void) const", 0x8004A0F8); return {}; }
void BBI__InventoryItem::SetItemSubcategory(BBI__InventoryItem__eItemSubcategory) { SIMMY_STUB("BBI::InventoryItem::SetItemSubcategory(BBI::InventoryItem::eItemSubcategory)", 0x8004A104); }
RET_UNKNOWN BBI__InventoryItem::GetItemSubcategory() const { SIMMY_STUB("BBI::InventoryItem::GetItemSubcategory(void) const", 0x8004A138); return {}; }
RET_UNKNOWN BBI__InventoryItem::DoStream(ReconBuffer *, int) { SIMMY_STUB("BBI::InventoryItem::DoStream(ReconBuffer *, int)", 0x8004A144); return {}; }
RET_UNKNOWN BBI__InventoryItem::GetItemGuid() const { SIMMY_STUB("BBI::InventoryItem::GetItemGuid(void) const", 0x8004A20C); return {}; }
void BBI__InventoryItem::SetItemGuid(int) { SIMMY_STUB("BBI::InventoryItem::SetItemGuid(int)", 0x8004A214); }
RET_UNKNOWN BBI__InventoryItem::GetItemCount() const { SIMMY_STUB("BBI::InventoryItem::GetItemCount(void) const", 0x8004A298); return {}; }
void BBI__InventoryItem::SetItemColorIndex(unsigned char) { SIMMY_STUB("BBI::InventoryItem::SetItemColorIndex(unsigned char)", 0x8004A2A4); }
RET_UNKNOWN BBI__InventoryItem::GetItemColorIndex() const { SIMMY_STUB("BBI::InventoryItem::GetItemColorIndex(void) const", 0x8004A2D8); return {}; }
bool BBI__InventoryItem::CopyTo(BBI__InventoryItem *) const { SIMMY_STUB("BBI::InventoryItem::CopyTo(BBI::InventoryItem *) const", 0x8004A2E0); return {}; }
BBI__InventoryItems::BBI__InventoryItems() { SIMMY_STUB("BBI::InventoryItems::InventoryItems(void)", 0x8004A31C); }
BBI__InventoryItems::~BBI__InventoryItems() { SIMMY_STUB("BBI::InventoryItems::~InventoryItems(void)", 0x8004A33C); }
RET_UNKNOWN BBI__InventoryItems::CanAddItemsByGuid(int, int, unsigned char) const { SIMMY_STUB("BBI::InventoryItems::CanAddItemsByGuid(int, int, unsigned char) const", 0x8004A37C); return {}; }
RET_UNKNOWN BBI__InventoryItems::AddItemsByGuid(int, int, unsigned char) { SIMMY_STUB("BBI::InventoryItems::AddItemsByGuid(int, int, unsigned char)", 0x8004A468); return {}; }
RET_UNKNOWN BBI__InventoryItems::Clear() { SIMMY_STUB("BBI::InventoryItems::Clear(void)", 0x8004A720); return {}; }
RET_UNKNOWN BBI__InventoryItems::GetItemCount() const { SIMMY_STUB("BBI::InventoryItems::GetItemCount(void) const", 0x8004A7A0); return {}; }
RET_UNKNOWN BBI__InventoryItems::CountItemsByGuid(int) const { SIMMY_STUB("BBI::InventoryItems::CountItemsByGuid(int) const", 0x8004A800); return {}; }
RET_UNKNOWN BBI__InventoryItems::CountItemsByCategory(BBI__InventoryItem__eItemCategory) const { SIMMY_STUB("BBI::InventoryItems::CountItemsByCategory(BBI::InventoryItem::eItemCategory) const", 0x8004A878); return {}; }
RET_UNKNOWN BBI__InventoryItems::GetItemsByCategory(BBI__InventoryItem__eItemCategory, BBI__InventoryItem **, unsigned int) const { SIMMY_STUB("BBI::InventoryItems::GetItemsByCategory(BBI::InventoryItem::eItemCategory, BBI::InventoryItem **, unsigned int) const", 0x8004A904); return {}; }
RET_UNKNOWN BBI__InventoryItems::IsItemInInventory(int) const { SIMMY_STUB("BBI::InventoryItems::IsItemInInventory(int) const", 0x8004A9E8); return {}; }
RET_UNKNOWN BBI__InventoryItems::GetContainerSize() const { SIMMY_STUB("BBI::InventoryItems::GetContainerSize(void) const", 0x8004AA5C); return {}; }
RET_UNKNOWN BBI__InventoryItems::CanAddBuilderItem(BBI__InventoryItem__eItemSubcategory, int, int) const { SIMMY_STUB("BBI::InventoryItems::CanAddBuilderItem(BBI::InventoryItem::eItemSubcategory, int, int) const", 0x8004AA80); return {}; }
RET_UNKNOWN BBI__InventoryItems::AddBuilderItem(BBI__InventoryItem__eItemSubcategory, int, int) { SIMMY_STUB("BBI::InventoryItems::AddBuilderItem(BBI::InventoryItem::eItemSubcategory, int, int)", 0x8004AB10); return {}; }
RET_UNKNOWN BBI__InventoryItems::RemoveBuilderItem(BBI__InventoryItem__eItemSubcategory, int, int) { SIMMY_STUB("BBI::InventoryItems::RemoveBuilderItem(BBI::InventoryItem::eItemSubcategory, int, int)", 0x8004ABDC); return {}; }
RET_UNKNOWN BBI__InventoryItems::GetBuilderItem(BBI__InventoryItem__eItemSubcategory, int) const { SIMMY_STUB("BBI::InventoryItems::GetBuilderItem(BBI::InventoryItem::eItemSubcategory, int) const", 0x8004AC50); return {}; }
RET_UNKNOWN BBI__InventoryItems::GetBuilderItemIndex(BBI__InventoryItem__eItemSubcategory, int) const { SIMMY_STUB("BBI::InventoryItems::GetBuilderItemIndex(BBI::InventoryItem::eItemSubcategory, int) const", 0x8004AC94); return {}; }
RET_UNKNOWN BBI__InventoryItems::DoStreamWrite(ReconBuffer *, int) { SIMMY_STUB("BBI::InventoryItems::DoStreamWrite(ReconBuffer *, int)", 0x8004AD24); return {}; }
RET_UNKNOWN BBI__InventoryItems::DoStreamRead(ReconBuffer *, int) { SIMMY_STUB("BBI::InventoryItems::DoStreamRead(ReconBuffer *, int)", 0x8004ADDC); return {}; }
RET_UNKNOWN BBI__InventoryItems::DoStream(ReconBuffer *, int) { SIMMY_STUB("BBI::InventoryItems::DoStream(ReconBuffer *, int)", 0x8004AF14); return {}; }
RET_UNKNOWN BBI__InventoryItems::GetNewItemIndex() const { SIMMY_STUB("BBI::InventoryItems::GetNewItemIndex(void) const", 0x8004AFA8); return {}; }
RET_UNKNOWN BBI__InventoryItems::CreateNewItemFromGuid(int) const { SIMMY_STUB("BBI::InventoryItems::CreateNewItemFromGuid(int) const", 0x8004B078); return {}; }
RET_UNKNOWN BBI__InventoryItems::CreateNewBuilderItem(BBI__InventoryItem__eItemSubcategory, int) const { SIMMY_STUB("BBI::InventoryItems::CreateNewBuilderItem(BBI::InventoryItem::eItemSubcategory, int) const", 0x8004B0D4); return {}; }
RET_UNKNOWN BBI__InventoryItems::CreateNewItemFromSelector(ObjSelector *) const { SIMMY_STUB("BBI::InventoryItems::CreateNewItemFromSelector(ObjSelector *) const", 0x8004B148); return {}; }
RET_UNKNOWN BBI__InventoryItems::GetItemCategoryFromObjDefinition(ObjDefinition *) { SIMMY_STUB("BBI::InventoryItems::GetItemCategoryFromObjDefinition(ObjDefinition *)", 0x8004B1E0); return {}; }
