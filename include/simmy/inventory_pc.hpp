// BBI::InventoryItems (game/inventory.cpp) on the player family's slots. PC: PlayerFamilyImpl keeps the
// streamed items (m_haveItem / m_items: byte 0 the category, 1 the count, 2 the colour index, 3 the
// subcategory, then the GUID word) instead of InventoryItem objects; these functions are the GC
// members over that layout (src/game/inventory.cpp).
#pragma once

namespace inventory {
constexpr int kContainerSize = 0x24;  // BBI::InventoryItems::GetContainerSize (8004AA5C)
int GetItemIndexByGuid(PlayerFamilyImpl* inv, int guid, int color);                          // 8004AFEC
PlayerFamilyImpl::InventoryItem* GetItemByGuid(PlayerFamilyImpl* inv, int guid, int color);   // 8004A9A4
PlayerFamilyImpl::InventoryItem* GetItemAtIndex(PlayerFamilyImpl* inv, int i);                // 8004AA64
int ItemCount(const PlayerFamilyImpl::InventoryItem* item);                                   // 8004A298
void SetItemCount(PlayerFamilyImpl::InventoryItem* item, int count);                          // 8004A248
void DeleteItem(PlayerFamilyImpl* inv, int index);                                            // 8004A59C
int RemoveItemsByGuid(PlayerFamilyImpl* inv, int guid, int count, int color);                 // 8004A524
int AddItemsByGuid(PlayerFamilyImpl* inv, int guid, int count, int color);                    // 8004A468
int Clear(PlayerFamilyImpl* inv);                                                             // 8004A720
int CountItemsByGuid(PlayerFamilyImpl* inv, int guid);                                        // 8004A800
int IsItemInInventory(PlayerFamilyImpl* inv, int guid);                                       // 8004A9E8
int GetNewItemIndex(PlayerFamilyImpl* inv);                                                   // 8004AFA8
// The player's family's inventory (GC: Neighborhood::GetFamily(DAT_80475C5C) -> GetInventory), or null.
PlayerFamilyImpl* PlayerInventory();
}  // namespace inventory
