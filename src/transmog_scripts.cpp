/*
5.0
Transmogrification 3.3.5a - Gossip menu
By Rochet2

ScriptName for NPC:
Creature_Transmogrify

TODO:
Make DB saving even better (Deleting)? What about coding?

Fix the cost formula
-- Too much data handling, use default costs

Are the qualities right?
Blizzard might have changed the quality requirements.
(TC handles it with stat checks)

Cant transmogrify rediculus items // Foereaper: would be fun to stab people with a fish
-- Cant think of any good way to handle this easily, could rip flagged items from cata DB
*/

#include "Transmogrification.h"
#include "ScriptedCreature.h"

#define sT  sTransmogrification
#define GTS session->GetAcoreString // dropped translation support, no one using?

class npc_transmogrifier : public CreatureScript
{
public:
    npc_transmogrifier() : CreatureScript("npc_transmogrifier") { }

    struct npc_transmogrifierAI : ScriptedAI
    {
        npc_transmogrifierAI(Creature* creature) : ScriptedAI(creature) { };

        bool CanBeSeen(Player const* player) override
        {
            Player* target = ObjectAccessor::FindConnectedPlayer(player->GetGUID());
            return sTransmogrification->IsEnabled() && !target->GetPlayerSetting("mod-transmog", SETTING_HIDE_TRANSMOG).value;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_transmogrifierAI(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        WorldSession* session = player->GetSession();
        if (sT->GetEnableTransmogInfo())
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Book_11:30:30:-18:0|tHow does transmogrification work?", EQUIPMENT_SLOT_END + 9, 0);
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (const char* slotName = sT->GetSlotName(slot, session))
            {
                Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                uint32 entry = newItem ? sT->GetFakeEntry(newItem->GetGUID()) : 0;
                std::string icon = entry ? sT->GetItemIcon(entry, 30, 30, -18, 0) : sT->GetSlotIcon(slot, 30, 30, -18, 0);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, icon + std::string(slotName), EQUIPMENT_SLOT_END, slot);
            }
        }
#ifdef PRESETS
        if (sT->GetEnableSets())
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/RAIDFRAME/UI-RAIDFRAME-MAINASSIST:30:30:-18:0|tManage sets", EQUIPMENT_SLOT_END + 4, 0);
#endif
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Enchant_Disenchant:30:30:-18:0|tRemove all transmogrifications", EQUIPMENT_SLOT_END + 2, 0, "Remove transmogrifications from all equipped items?", 0, false);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:30:30:-18:0|tUpdate menu", EQUIPMENT_SLOT_END + 1, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();
        WorldSession* session = player->GetSession();
        // Next page
        if (sender > EQUIPMENT_SLOT_END + 10)
        {
            ShowTransmogItems(player, creature, action, sender);
            return true;
        }
        switch (sender)
        {
            case EQUIPMENT_SLOT_END: // Show items you can use
                ShowTransmogItems(player, creature, action, sender);
                break;
            case EQUIPMENT_SLOT_END + 1: // Main menu
                OnGossipHello(player, creature);
                break;
            case EQUIPMENT_SLOT_END + 2: // Remove Transmogrifications
            {
                bool removed = false;
                auto trans = CharacterDatabase.BeginTransaction();
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        if (!sT->GetFakeEntry(newItem->GetGUID()))
                            continue;
                        sT->DeleteFakeEntry(player, slot, newItem, &trans);
                        removed = true;
                    }
                }
                if (removed)
                {
                    session->SendAreaTriggerMessage("%s", GTS(LANG_ERR_UNTRANSMOG_OK));
                    CharacterDatabase.CommitTransaction(trans);
                }
                else
                    session->SendNotification(LANG_ERR_UNTRANSMOG_NO_TRANSMOGS);
                OnGossipHello(player, creature);
            } break;
            case EQUIPMENT_SLOT_END + 3: // Remove Transmogrification from single item
            {
                if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, action))
                {
                    if (sT->GetFakeEntry(newItem->GetGUID()))
                    {
                        sT->DeleteFakeEntry(player, action, newItem);
                        session->SendAreaTriggerMessage("%s", GTS(LANG_ERR_UNTRANSMOG_OK));
                    }
                    else
                        session->SendNotification(LANG_ERR_UNTRANSMOG_NO_TRANSMOGS);
                }
                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END, action);
            } break;
    #ifdef PRESETS
            case EQUIPMENT_SLOT_END + 4: // Presets menu
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                if (sT->GetEnableSetInfo())
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Book_11:30:30:-18:0|tHow do sets work?", EQUIPMENT_SLOT_END + 10, 0);
                for (Transmogrification::presetIdMap::const_iterator it = sT->presetByName[player->GetGUID()].begin(); it != sT->presetByName[player->GetGUID()].end(); ++it)
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Statue_02:30:30:-18:0|t" + it->second, EQUIPMENT_SLOT_END + 6, it->first);

                if (sT->presetByName[player->GetGUID()].size() < sT->GetMaxSets())
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/GuildBankFrame/UI-GuildBankFrame-NewTab:30:30:-18:0|tSave set", EQUIPMENT_SLOT_END + 8, 0);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_Spy:30:30:-18:0|tBack...", EQUIPMENT_SLOT_END + 1, 0);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            } break;
            case EQUIPMENT_SLOT_END + 5: // Use preset
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                // action = presetID
                for (Transmogrification::slotMap::const_iterator it = sT->presetById[player->GetGUID()][action].begin(); it != sT->presetById[player->GetGUID()][action].end(); ++it)
                {
                    if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, it->first))
                        sT->PresetTransmog(player, item, it->second, it->first);
                }
                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END + 6, action);
            } break;
            case EQUIPMENT_SLOT_END + 6: // view preset
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                // action = presetID
                for (Transmogrification::slotMap::const_iterator it = sT->presetById[player->GetGUID()][action].begin(); it != sT->presetById[player->GetGUID()][action].end(); ++it)
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(it->second, 30, 30, -18, 0) + sT->GetItemLink(it->second, session), sender, action);

                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Statue_02:30:30:-18:0|tUse this set", EQUIPMENT_SLOT_END + 5, action, "Using this set for transmogrify will bind transmogrified items to you and make them non-refundable and non-tradeable.\nDo you wish to continue?\n\n" + sT->presetByName[player->GetGUID()][action], 0, false);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-LeaveItem-Opaque:30:30:-18:0|tDelete set", EQUIPMENT_SLOT_END + 7, action, "Are you sure you want to delete " + sT->presetByName[player->GetGUID()][action] + "?", 0, false);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_Spy:30:30:-18:0|tBack...", EQUIPMENT_SLOT_END + 4, 0);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            } break;
            case EQUIPMENT_SLOT_END + 7: // Delete preset
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                // action = presetID
                CharacterDatabase.Execute("DELETE FROM `custom_transmogrification_sets` WHERE Owner = {} AND PresetID = {}", player->GetGUID().GetCounter(), action);
                sT->presetById[player->GetGUID()][action].clear();
                sT->presetById[player->GetGUID()].erase(action);
                sT->presetByName[player->GetGUID()].erase(action);

                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END + 4, 0);
            } break;
            case EQUIPMENT_SLOT_END + 8: // Save preset
            {
                if (!sT->GetEnableSets() || sT->presetByName[player->GetGUID()].size() >= sT->GetMaxSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                uint32 cost = 0;
                bool canSave = false;
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (!sT->GetSlotName(slot, session))
                        continue;
                    if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        uint32 entry = sT->GetFakeEntry(newItem->GetGUID());
                        if (!entry)
                            continue;
                        const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
                        if (!temp)
                            continue;
                        if (!sT->SuitableForTransmogrification(player, temp)) // no need to check?
                            continue;
                        cost += sT->GetSpecialPrice(temp);
                        canSave = true;
                        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(entry, 30, 30, -18, 0) + sT->GetItemLink(entry, session), EQUIPMENT_SLOT_END + 8, 0);
                    }
                }
                if (canSave)
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/GuildBankFrame/UI-GuildBankFrame-NewTab:30:30:-18:0|tSave set", 0, 0, "Insert set name", cost*sT->GetSetCostModifier() + sT->GetSetCopperCost(), true);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:30:30:-18:0|tUpdate menu", sender, action);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_Spy:30:30:-18:0|tBack...", EQUIPMENT_SLOT_END + 4, 0);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            } break;
            case EQUIPMENT_SLOT_END + 10: // Set info
            {
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_Spy:30:30:-18:0|tBack...", EQUIPMENT_SLOT_END + 4, 0);
                SendGossipMenuFor(player, sT->GetSetNpcText(), creature->GetGUID());
            } break;
    #endif
            case EQUIPMENT_SLOT_END + 9: // Transmog info
            {
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_Spy:30:30:-18:0|tBack...", EQUIPMENT_SLOT_END + 1, 0);
                SendGossipMenuFor(player, sT->GetTransmogNpcText(), creature->GetGUID());
            } break;
            default: // Transmogrify
            {
                if (!sender && !action)
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                // sender = slot, action = display
                if (sT->GetUseCollectionSystem())
                {
                    TransmogAcoreStrings res = sT->Transmogrify(player, action, sender);
                    if (res == LANG_ERR_TRANSMOG_OK)
                        session->SendAreaTriggerMessage("%s",GTS(LANG_ERR_TRANSMOG_OK));
                    else
                        session->SendNotification(res);
                }
                else
                {
                    TransmogAcoreStrings res = sT->Transmogrify(player, ObjectGuid::Create<HighGuid::Item>(action), sender);
                    if (res == LANG_ERR_TRANSMOG_OK)
                        session->SendAreaTriggerMessage("%s",GTS(LANG_ERR_TRANSMOG_OK));
                    else
                        session->SendNotification(res);
                }
                // OnGossipSelect(player, creature, EQUIPMENT_SLOT_END, sender);
                // ShowTransmogItems(player, creature, sender);
                CloseGossipMenuFor(player); // Wait for SetMoney to get fixed, issue #10053
            } break;
        }
        return true;
    }

#ifdef PRESETS
    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code) override
    {
        player->PlayerTalkClass->ClearMenus();
        if (sender || action)
            return true; // should never happen
        if (!sT->GetEnableSets())
        {
            OnGossipHello(player, creature);
            return true;
        }
        std::string name(code);
        if (name.find('"') != std::string::npos || name.find('\\') != std::string::npos)
            player->GetSession()->SendNotification(LANG_PRESET_ERR_INVALID_NAME);
        else
        {
            for (uint8 presetID = 0; presetID < sT->GetMaxSets(); ++presetID) // should never reach over max
            {
                if (sT->presetByName[player->GetGUID()].find(presetID) != sT->presetByName[player->GetGUID()].end())
                    continue; // Just remember never to use presetByName[pGUID][presetID] when finding etc!

                int32 cost = 0;
                std::map<uint8, uint32> items;
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (!sT->GetSlotName(slot, player->GetSession()))
                        continue;
                    if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        uint32 entry = sT->GetFakeEntry(newItem->GetGUID());
                        if (!entry)
                            continue;
                        const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
                        if (!temp)
                            continue;
                        if (!sT->SuitableForTransmogrification(player, temp))
                            continue;
                        cost += sT->GetSpecialPrice(temp);
                        items[slot] = entry;
                    }
                }
                if (items.empty())
                    break; // no transmogrified items were found to be saved
                cost *= sT->GetSetCostModifier();
                cost += sT->GetSetCopperCost();
                if (!player->HasEnoughMoney(cost))
                {
                    player->GetSession()->SendNotification(LANG_ERR_TRANSMOG_NOT_ENOUGH_MONEY);
                    break;
                }

                std::ostringstream ss;
                for (std::map<uint8, uint32>::iterator it = items.begin(); it != items.end(); ++it)
                {
                    ss << uint32(it->first) << ' ' << it->second << ' ';
                    sT->presetById[player->GetGUID()][presetID][it->first] = it->second;
                }
                sT->presetByName[player->GetGUID()][presetID] = name; // Make sure code doesnt mess up SQL!
                CharacterDatabase.Execute("REPLACE INTO `custom_transmogrification_sets` (`Owner`, `PresetID`, `SetName`, `SetData`) VALUES ({}, {}, \"{}\", \"{}\")", player->GetGUID().GetCounter(), uint32(presetID), name, ss.str());
                if (cost)
                    player->ModifyMoney(-cost);
                break;
            }
        }
        //OnGossipSelect(player, creature, EQUIPMENT_SLOT_END+4, 0);
        CloseGossipMenuFor(player); // Wait for SetMoney to get fixed, issue #10053
        return true;
    }
#endif

    void ShowTransmogItems(Player* player, Creature* creature, uint8 slot, uint16 gossipPageNumber) // Only checks bags while can use an item from anywhere in inventory
    {
        WorldSession* session = player->GetSession();
        Item* oldItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        bool sendGossip = true;
        if (oldItem)
        {
            uint32 price = sT->GetSpecialPrice(oldItem->GetTemplate());
            price *= sT->GetScaledCostModifier();
            price += sT->GetCopperCost();
            std::ostringstream ss;
            ss << std::endl;
            if (sT->GetRequireToken())
                ss << std::endl << std::endl << sT->GetTokenAmount() << " x " << sT->GetItemLink(sT->GetTokenEntry(), session);
            std::string lineEnd = ss.str();

            if (sT->GetUseCollectionSystem())
            {
                sendGossip = false;

                std::string query = "SELECT item_template_id FROM custom_unlocked_appearances WHERE account_id = " + std::to_string(player->GetSession()->GetAccountId()) + " ORDER BY item_template_id";
                session->GetQueryProcessor().AddCallback(LoginDatabase.AsyncQuery(query).WithCallback([=](QueryResult result)
                {
                    uint16 pageNumber = 0;
                    uint32 startValue = 0;
                    uint32 endValue = MAX_OPTIONS - 3;
                    bool lastPage = false;
                    if (gossipPageNumber > EQUIPMENT_SLOT_END + 10)
                    {
                        pageNumber = gossipPageNumber - EQUIPMENT_SLOT_END - 10;
                        startValue = (pageNumber * (MAX_OPTIONS - 2));
                        endValue = (pageNumber + 1) * (MAX_OPTIONS - 2) - 1;
                    }
                    if (result)
                    {
                        std::vector<Item*> allowedItems;
                        do {
                            uint32 newItemEntryId = (*result)[0].Get<uint32>();
                            Item* newItem = Item::CreateItem(newItemEntryId, 1, 0);
                            if (!newItem)
                                continue;
                            if (!sT->CanTransmogrifyItemWithItem(player, oldItem->GetTemplate(), newItem->GetTemplate(), slot))
                                continue;
                            if (sT->GetFakeEntry(oldItem->GetGUID()) == newItem->GetEntry())
                                continue;
                            allowedItems.push_back(newItem);
                        } while (result->NextRow());
                        for (uint32 i = startValue; i <= endValue; i++)
                        {
                            if (allowedItems.empty() || i > allowedItems.size() - 1)
                            {
                                lastPage = true;
                                break;
                            }
                            Item* newItem = allowedItems.at(i);
                            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(newItem->GetEntry(), 30, 30, -18, 0) + sT->GetItemLink(newItem, session), slot, newItem->GetEntry(), "Using this item for transmogrify will bind it to you and make it non-refundable and non-tradeable.\nDo you wish to continue?\n\n" + sT->GetItemIcon(newItem->GetEntry(), 40, 40, -15, -10) + sT->GetItemLink(newItem, session) + lineEnd, price, false);
                        }
                    }
                    if (gossipPageNumber == EQUIPMENT_SLOT_END + 11)
                    {
                        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Previous Page", EQUIPMENT_SLOT_END, slot);
                        if (!lastPage)
                        {
                            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next Page", gossipPageNumber + 1, slot);
                        }
                    }
                    else if (gossipPageNumber > EQUIPMENT_SLOT_END + 11)
                    {
                        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Previous Page", gossipPageNumber - 1, slot);
                        if (!lastPage)
                        {
                            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next Page", gossipPageNumber + 1, slot);
                        }
                    }
                    else if (!lastPage)
                    {
                        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next Page", EQUIPMENT_SLOT_END + 11, slot);
                    }

                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Enchant_Disenchant:30:30:-18:0|tRemove transmogrification", EQUIPMENT_SLOT_END + 3, slot, "Remove transmogrification from the slot?", 0, false);
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:30:30:-18:0|tUpdate menu", EQUIPMENT_SLOT_END, slot);
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_Spy:30:30:-18:0|tBack...", EQUIPMENT_SLOT_END + 1, 0);
                    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                }));
            }
            else
            {
                uint32 limit = 0;
                for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
                {
                    if (limit > MAX_OPTIONS)
                        break;
                    Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                    if (!newItem)
                        continue;
                    if (!sT->CanTransmogrifyItemWithItem(player, oldItem->GetTemplate(), newItem->GetTemplate(), slot))
                        continue;
                    if (sT->GetFakeEntry(oldItem->GetGUID()) == newItem->GetEntry())
                        continue;
                    ++limit;
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(newItem->GetEntry(), 30, 30, -18, 0) + sT->GetItemLink(newItem, session), slot, newItem->GetGUID().GetCounter(), "Using this item for transmogrify will bind it to you and make it non-refundable and non-tradeable.\nDo you wish to continue?\n\n" + sT->GetItemIcon(newItem->GetEntry(), 40, 40, -15, -10) + sT->GetItemLink(newItem, session) + lineEnd, price, false);
                }

                for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
                {
                    Bag* bag = player->GetBagByPos(i);
                    if (!bag)
                        continue;
                    for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                    {
                        if (limit > MAX_OPTIONS)
                            break;
                        Item* newItem = player->GetItemByPos(i, j);
                        if (!newItem)
                            continue;
                        if (!sT->CanTransmogrifyItemWithItem(player, oldItem->GetTemplate(), newItem->GetTemplate(), slot))
                            continue;
                        if (sT->GetFakeEntry(oldItem->GetGUID()) == newItem->GetEntry())
                            continue;
                        ++limit;
                        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(newItem->GetEntry(), 30, 30, -18, 0) + sT->GetItemLink(newItem, session), slot, newItem->GetGUID().GetCounter(), "Using this item for transmogrify will bind it to you and make it non-refundable and non-tradeable.\nDo you wish to continue?\n\n" + sT->GetItemIcon(newItem->GetEntry(), 40, 40, -15, -10) + sT->GetItemLink(newItem, session) + ss.str(), price, false);
                    }
                }
            }
        }

        if (sendGossip) {
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Enchant_Disenchant:30:30:-18:0|tRemove transmogrification", EQUIPMENT_SLOT_END + 3, slot, "Remove transmogrification from the slot?", 0, false);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:30:30:-18:0|tUpdate menu", EQUIPMENT_SLOT_END, slot);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_Spy:30:30:-18:0|tBack...", EQUIPMENT_SLOT_END + 1, 0);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        }
    }
};

class PS_Transmogrification : public PlayerScript
{
private:
    void AddToDatabase(Player* player, ItemTemplate const* itemTemplate)
    {
        if (!sT->GetTrackUnusableItems() && !sT->SuitableForTransmogrification(player, itemTemplate))
            return;
        if (itemTemplate->Class != ITEM_CLASS_ARMOR && itemTemplate->Class != ITEM_CLASS_WEAPON)
            return;
        if (itemTemplate->Class == ITEM_CLASS_ARMOR && (itemTemplate->SubClass > 6 || (itemTemplate->InventoryType == 0 || itemTemplate->InventoryType == 2 || itemTemplate->InventoryType == 11 || itemTemplate->InventoryType == 12 || itemTemplate->InventoryType == 28)))
            return;
        if (!itemTemplate->Quality)
            return;
        uint32 itemId = itemTemplate->ItemId;
        uint32 accountId = player->GetSession()->GetAccountId();
        std::string itemName = itemTemplate -> Name1;
        std::stringstream tempStream;
        tempStream << std::hex << ItemQualityColors[itemTemplate->Quality];
        std::string itemQuality = tempStream.str();
        bool showChatMessage = !(player->GetPlayerSetting("mod-transmog", SETTING_HIDE_TRANSMOG).value);
        std::string query = "SELECT account_id, item_template_id FROM custom_unlocked_appearances WHERE (account_id = 0 or account_id = " + std::to_string(accountId) + ") AND item_template_id = " + std::to_string(itemId);
        player->GetSession()->GetQueryProcessor().AddCallback(LoginDatabase.AsyncQuery(query).WithCallback([=](QueryResult result)
        {
            if (!result)
            {
                if (showChatMessage)
                    ChatHandler(player->GetSession()).SendSysMessage(sT->GetItemLink(itemId, player->GetSession()) + " a rejoint votre collection d'apparences.");
                LoginDatabase.Execute( "INSERT INTO custom_unlocked_appearances (account_id, item_template_id) VALUES ({}, {})", accountId, itemId);
            }
        }));
    }
public:
    PS_Transmogrification() : PlayerScript("Player_Transmogrify") { }

    void OnEquip(Player* player, Item* it, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        if (!sT->GetUseCollectionSystem())
            return;
        ItemTemplate const* pProto = it->GetTemplate();
        AddToDatabase(player, pProto);
    }

    void OnLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid /*lootguid*/) override
    {
        if (!sT->GetUseCollectionSystem() || !item)
            return;
        if (item->GetTemplate()->Bonding == ItemBondingType::BIND_WHEN_PICKED_UP || item->IsSoulBound())
        {
            AddToDatabase(player, item->GetTemplate());
        }
    }

    void OnCreateItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if (!sT->GetUseCollectionSystem())
            return;
        if (item->GetTemplate()->Bonding == ItemBondingType::BIND_WHEN_PICKED_UP || item->IsSoulBound())
        {
            AddToDatabase(player, item->GetTemplate());
        }
    }

    void OnAfterStoreOrEquipNewItem(Player* player, uint32 /*vendorslot*/, Item* item, uint8 /*count*/, uint8 /*bag*/, uint8 /*slot*/, ItemTemplate const* /*pProto*/, Creature* /*pVendor*/, VendorItem const* /*crItem*/, bool /*bStore*/) override
    {
        if (!sT->GetUseCollectionSystem())
            return;
        if (item->GetTemplate()->Bonding == ItemBondingType::BIND_WHEN_PICKED_UP || item->IsSoulBound())
        {
            AddToDatabase(player, item->GetTemplate());
        }
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (!sT->GetUseCollectionSystem())
            return;
        for (uint8 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
        {
            uint32 itemId = uint32(quest->RewardChoiceItemId[i]);
            if (!itemId)
                continue;
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            AddToDatabase(player, itemTemplate);
        }
        for (uint8 i = 0; i < QUEST_REWARDS_COUNT; ++i)
        {
            uint32 itemId = uint32(quest->RewardItemId[i]);
            if (!itemId)
                continue;
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            AddToDatabase(player, itemTemplate);
        }
    }

    void OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item *item) override
    {
        if (!item)
            return;

        if (uint32 entry = sT->GetFakeEntry(item->GetGUID()))
        {
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), entry);
        }
    }

    void OnAfterMoveItemFromInventory(Player* /*player*/, Item* it, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        sT->DeleteFakeFromDB(it->GetGUID().GetCounter());
    }

    void OnLogin(Player* player) override
    {
        ObjectGuid playerGUID = player->GetGUID();
        sT->entryMap.erase(playerGUID);
        QueryResult result = CharacterDatabase.Query("SELECT GUID, FakeEntry FROM custom_transmogrification WHERE Owner = {}", player->GetGUID().GetCounter());
        if (result)
        {
            do
            {
                ObjectGuid itemGUID = ObjectGuid::Create<HighGuid::Item>((*result)[0].Get<uint32>());
                uint32 fakeEntry = (*result)[1].Get<uint32>();
                if (sObjectMgr->GetItemTemplate(fakeEntry))
                {
                    sT->dataMap[itemGUID] = playerGUID;
                    sT->entryMap[playerGUID][itemGUID] = fakeEntry;
                }
                else
                {
                    //sLog->outError(LOG_FILTER_SQL, "Item entry (Entry: {}, itemGUID: {}, playerGUID: {}) does not exist, ignoring.", fakeEntry, GUID_LOPART(itemGUID), player->GetGUIDLow());
                    // CharacterDatabase.Execute("DELETE FROM custom_transmogrification WHERE FakeEntry = {}", fakeEntry);
                }
            } while (result->NextRow());

            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    player->SetVisibleItemSlot(slot, item);
            }
        }

#ifdef PRESETS
        if (sT->GetEnableSets())
            sT->LoadPlayerSets(playerGUID);
#endif
    }

    void OnLogout(Player* player) override
    {
        ObjectGuid pGUID = player->GetGUID();
        for (Transmogrification::transmog2Data::const_iterator it = sT->entryMap[pGUID].begin(); it != sT->entryMap[pGUID].end(); ++it)
            sT->dataMap.erase(it->first);
        sT->entryMap.erase(pGUID);

#ifdef PRESETS
        if (sT->GetEnableSets())
            sT->UnloadPlayerSets(pGUID);
#endif
    }
};

class WS_Transmogrification : public WorldScript
{
public:
    WS_Transmogrification() : WorldScript("WS_Transmogrification") { }

    void OnAfterConfigLoad(bool reload) override
    {
        sT->LoadConfig(reload);
    }

    void OnStartup() override
    {
        sT->LoadConfig(false);
        //sLog->outInfo(LOG_FILTER_SERVER_LOADING, "Deleting non-existing transmogrification entries...");
        CharacterDatabase.Execute("DELETE FROM custom_transmogrification WHERE NOT EXISTS (SELECT 1 FROM item_instance WHERE item_instance.guid = custom_transmogrification.GUID)");

#ifdef PRESETS
        // Clean even if disabled
        // Dont delete even if player has more presets than should
        CharacterDatabase.Execute("DELETE FROM `custom_transmogrification_sets` WHERE NOT EXISTS(SELECT 1 FROM characters WHERE characters.guid = custom_transmogrification_sets.Owner)");
#endif
    }
};

class global_transmog_script : public GlobalScript
{
public:
    global_transmog_script() : GlobalScript("global_transmog_script") { }

    void OnItemDelFromDB(CharacterDatabaseTransaction trans, ObjectGuid::LowType itemGuid) override
    {
        sT->DeleteFakeFromDB(itemGuid, &trans);
    }

    void OnMirrorImageDisplayItem(const Item *item, uint32 &display) override
    {
        if (uint32 entry = sTransmogrification->GetFakeEntry(item->GetGUID()))
            display=uint32(sObjectMgr->GetItemTemplate(entry)->DisplayInfoID);
    }
};

class unit_transmog_script : public UnitScript
{
public:
    unit_transmog_script() : UnitScript("unit_transmog_script") { }

    bool OnBuildValuesUpdate(Unit const* unit, uint8 /*updateType*/, ByteBuffer& fieldBuffer, Player* target, uint16 index) override
    {
        if (unit->IsPlayer() && index >= PLAYER_VISIBLE_ITEM_1_ENTRYID && index <= PLAYER_VISIBLE_ITEM_19_ENTRYID && (index & 1))
        {
            if (Item* item = unit->ToPlayer()->GetItemByPos(INVENTORY_SLOT_BAG_0, ((index - PLAYER_VISIBLE_ITEM_1_ENTRYID) / 2U)))
            {
                if (!sTransmogrification->IsEnabled() || target->GetPlayerSetting("mod-transmog", SETTING_HIDE_TRANSMOG).value)
                {
                    fieldBuffer << item->GetEntry();
                    return true;
                }
            }
        }

        return false;
    }
};

void AddSC_Transmog()
{
    new global_transmog_script();
    new unit_transmog_script();
    new npc_transmogrifier();
    new PS_Transmogrification();
    new WS_Transmogrification();
}
