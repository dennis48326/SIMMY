// Port of game/inventory.cpp (part): BBI::InventoryItems, the family inventory (food ingredients from the
// garden, the fruit tree and the aquarium, food creation's purchases), over PlayerFamilyImpl's streamed
// slots (see simmy/inventory_pc.hpp). IngredientManager is in ingtarget.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/inventory_pc.hpp"

namespace inventory {

// 8004AFEC
int GetItemIndexByGuid(PlayerFamilyImpl* inv, int guid, int color) {
  for (int i = 0; i < kContainerSize; ++i)
    if (inv->m_haveItem[i] && inv->m_items[i].m_word == guid && static_cast<unsigned char>(inv->m_items[i].m_bytes[2]) == color)
      return i;
  return -1;
}

// 8004A9A4
PlayerFamilyImpl::InventoryItem* GetItemByGuid(PlayerFamilyImpl* inv, int guid, int color) {
  const int i = GetItemIndexByGuid(inv, guid, color);
  return i == -1 ? nullptr : &inv->m_items[i];
}

// 8004AA64
PlayerFamilyImpl::InventoryItem* GetItemAtIndex(PlayerFamilyImpl* inv, int i) {
  return i < kContainerSize && inv->m_haveItem[i] ? &inv->m_items[i] : nullptr;
}

// 8004A298
int ItemCount(const PlayerFamilyImpl::InventoryItem* item) { return item->m_bytes[1]; }

// 8004A248
void SetItemCount(PlayerFamilyImpl::InventoryItem* item, int count) {
  item->m_bytes[1] = static_cast<signed char>(count < 0 ? 0 : count > 99 ? 99 : count);
}

// 8004A59C
// The slot goes; the later ones move down.
void DeleteItem(PlayerFamilyImpl* inv, int index) {
  inv->m_haveItem[index] = false;
  inv->m_items[index] = PlayerFamilyImpl::InventoryItem();
  for (int i = index + 1; i < kContainerSize && inv->m_haveItem[i]; ++i) {
    inv->m_haveItem[i - 1] = true;
    inv->m_items[i - 1] = inv->m_items[i];
    inv->m_haveItem[i] = false;
    inv->m_items[i] = PlayerFamilyImpl::InventoryItem();
  }
}

// 8004A524
// The items removed (all of them when fewer are held: the slot goes).
int RemoveItemsByGuid(PlayerFamilyImpl* inv, int guid, int count, int color) {
  const int index = GetItemIndexByGuid(inv, guid, color);
  if (index == -1) return 0;
  PlayerFamilyImpl::InventoryItem* item = &inv->m_items[index];
  const int had = ItemCount(item);
  if (had - count < 1) {
    DeleteItem(inv, index);
    return had;
  }
  SetItemCount(item, had - count);
  return count;
}

// 8004AFA8
// The first empty slot, -1 when all 36 are used.
int GetNewItemIndex(PlayerFamilyImpl* inv) {
  int i = 0;
  if (!inv->m_haveItem[0]) return 0;
  do {
    if (++i > 0x23) return -1;
  } while (inv->m_haveItem[i]);
  return i;
}

// 8004A468
// `count` more of the GUID (a new slot: category 1, subcategory 6, BBI::InventoryItems::
// CreateNewItemFromGuid 8004B078), at most 99. The count asked, or (GC quirk) what went over 99.
int AddItemsByGuid(PlayerFamilyImpl* inv, int guid, int count, int color) {
  if (guid == 0) return 0;
  if (count == 0) return 0;
  PlayerFamilyImpl::InventoryItem* item = GetItemByGuid(inv, guid, color);
  if (!item) {
    const int index = GetNewItemIndex(inv);
    if (index == -1) return 0;
    PlayerFamilyImpl::InventoryItem made;
    made.m_bytes[0] = 1;  // SetItemCategory
    made.m_bytes[3] = 6;  // SetItemSubcategory
    made.m_word = guid;
    inv->m_haveItem[index] = true;
    inv->m_items[index] = made;
    item = &inv->m_items[index];
  }
  const int total = ItemCount(item) + count;
  int r = count;
  if (total > 99) r = total - 99;
  SetItemCount(item, total);
  return r;
}

// 8004A720
int Clear(PlayerFamilyImpl* inv) {
  for (int i = 0; i < kContainerSize; ++i) {
    inv->m_haveItem[i] = false;
    inv->m_items[i] = PlayerFamilyImpl::InventoryItem();
  }
  return 1;
}

// 8004A800
// The items of the GUID in every slot (any colour).
int CountItemsByGuid(PlayerFamilyImpl* inv, int guid) {
  int n = 0;
  for (int i = 0; i < kContainerSize; ++i)
    if (inv->m_haveItem[i] && inv->m_items[i].m_word == guid) n += ItemCount(&inv->m_items[i]);
  return n;
}

// 8004A9E8
int IsItemInInventory(PlayerFamilyImpl* inv, int guid) {
  for (int i = 0; i < kContainerSize; ++i)
    if (inv->m_haveItem[i] && inv->m_items[i].m_word == guid) return 1;
  return 0;
}

PlayerFamilyImpl* PlayerInventory() {
  FamilyImpl* family = Globs_pNeighborhood ? Globs_pNeighborhood->GetFamily(_gd.m_fieldD8) : nullptr;
  return dynamic_cast<PlayerFamilyImpl*>(family);
}

}  // namespace inventory
