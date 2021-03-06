/*
Name:       PlayerBot AI Core by Graff (onlysuffering@gmail.com)
Converted:  from original blueboy's Playerbots: https://github.com/blueboy/portal/
Revision:   bbbd930771eb48804f96651f5e15a098b2cecb43
last merge:
09-08-2012 blueBoy's Playerbots: e13fb1bd41111f99ad39aa54a0b28751efb4f2cd
*/
#include "PlayerbotAI.h"
#include "Config.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "PlayerbotDeathKnightAI.h"
#include "PlayerbotDruidAI.h"
#include "PlayerbotHunterAI.h"
#include "PlayerbotMageAI.h"
#include "PlayerbotPaladinAI.h"
#include "PlayerbotPriestAI.h"
#include "PlayerbotRogueAI.h"
#include "PlayerbotShamanAI.h"
#include "PlayerbotWarlockAI.h"
#include "PlayerbotWarriorAI.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "GossipDef.h"
#include "AuctionHouseMgr.h"
#include <iomanip>
#include "GameEventMgr.h"

PlayerbotAI::PlayerbotAI(Player* const master, Player* const bot) :
    m_master(master), m_bot(bot), m_classAI(0), m_ignoreAIUpdatesUntilTime(0),
    m_combatOrder(ORDERS_NONE), m_ScenarioType(SCENARIO_PVEEASY),
    m_TimeDoneEating(0), m_TimeDoneDrinking(0),
    m_CurrentlyCastingSpellId(0), m_spellIdCommand(0),
    m_targetGuidCommand(0), m_taxiMaster(0)
{

    // load config variables
    m_changeFaction = ConfigMgr::GetBoolDefault("Bot.ChangeFaction", true);

    m_confMaxNumBots = ConfigMgr::GetIntDefault("Bot.MaxNumBots", 9);
    m_confDebugWhisper = ConfigMgr::GetBoolDefault("Bot.DebugWhisper", false);
    m_confFollowDistance[0] = ConfigMgr::GetFloatDefault("Bot.FollowDistanceMin", 0.5f);
    m_confFollowDistance[1] = ConfigMgr::GetFloatDefault("Bot.FollowDistanceMax", 1.0f);
    m_confCollectCombat = ConfigMgr::GetBoolDefault("Bot.Collect.Combat", true);
    m_confCollectQuest = ConfigMgr::GetBoolDefault("Bot.Collect.Quest", true);
    m_confCollectProfession = ConfigMgr::GetBoolDefault("Bot.Collect.Profession", true);
    m_confCollectLoot = ConfigMgr::GetBoolDefault("Bot.Collect.Loot", true);
    m_confCollectSkin = ConfigMgr::GetBoolDefault("Bot.Collect.Skin", true);
    m_confCollectObjects = ConfigMgr::GetBoolDefault("Bot.Collect.Objects", true);
    gConfigSellLevelDiff = ConfigMgr::GetIntDefault("PlayerbotAI.SellAll.LevelDiff", 10);
    m_confCollectDistanceMax = ConfigMgr::GetIntDefault("Bot.Collect.DistanceMax", 50);
    if (m_confCollectDistanceMax > 100)
    {
        //sLog->outError("Playerbot: Bot.Collect.DistanceMax higher than allowed. Using 100");
        m_confCollectDistanceMax = 100;
    }
    m_confCollectDistance = ConfigMgr::GetIntDefault("Bot.Collect.Distance", 25);
    if (m_confCollectDistance > m_confCollectDistanceMax)
    {
        //sLog->outError("Playerbot: Bot.Collect.Distance higher than Bot.Collect.DistanceMax. Using DistanceMax value");
        m_confCollectDistance = m_confCollectDistanceMax;
    }
    m_confSellGarbage = ConfigMgr::GetBoolDefault("Bot.SellGarbage", true);
    // set bot state
    m_botState = BOTSTATE_NORMAL;

    // reset some pointers
    m_targetChanged = false;
    m_targetType = TARGET_NORMAL;
    m_targetCombat = 0;
    m_targetAssist = 0;
    m_targetProtect = 0;

    // set collection options
    m_collectionFlags = 0;
    //m_collectDist = m_confCollectDistance;
    if (m_confCollectCombat)
        SetCollectFlag(COLLECT_FLAG_COMBAT);
    if (m_confCollectQuest)
        SetCollectFlag(COLLECT_FLAG_QUEST);
    if (m_confCollectProfession)
        SetCollectFlag(COLLECT_FLAG_PROFESSION);
    if (m_confCollectLoot)
        SetCollectFlag(COLLECT_FLAG_LOOT);
    if (m_confCollectSkin && m_bot->HasSkill(SKILL_SKINNING))
        SetCollectFlag(COLLECT_FLAG_SKIN);
    if (m_confCollectObjects)
        SetCollectFlag(COLLECT_FLAG_NEAROBJECT);

    // set needed item list
    SetQuestNeedItems();
    SetQuestNeedCreatures();

    // start following master (will also teleport bot to master)
    SetMovementOrder(MOVEMENT_FOLLOW, m_master);

    //add bot to group
    ChatHandler ch(master->GetSession());
    if (Group *group = master->GetGroup())
    {
        if (!group->IsMember(m_bot->GetGUID()))
        {
            if (!group->IsFull())
            {
                if (!group->AddMember(m_bot))
                {
                    ch.PSendSysMessage("Playerbot %u is not added cuz group is full! #1", m_bot->GetGUIDLow());
                    return;
                }
            }
            else if (!group->isRaidGroup())
            {
                group->ConvertToRaid();
                if (!group->AddMember(m_bot))
                {
                    ch.PSendSysMessage("Playerbot %u is not added! #2", m_bot->GetGUIDLow());
                    //master->GetSession()->LogoutPlayerBot(m_bot->GetGUID());
                    return;
                }
            }
            else//raid group is full
            {
                ch.PSendSysMessage("Playerbot %u is not added cuz group is full! #2", m_bot->GetGUIDLow());
                //master->GetSession()->LogoutPlayerBot(m_bot->GetGUID());
                return;
            }
        }
    }
    else
    {
        group = new Group;
        if (!group->Create(master))
        {
            delete group;
            return;
        }

        //critical part
        sGroupMgr->AddGroup(group);

        if (!group->AddMember(m_bot))
        {
            ch.PSendSysMessage("Playerbot %u is not added! #2", m_bot->GetGUIDLow());
            //master->GetSession()->LogoutPlayerBot(m_bot->GetGUID());
            return;
        }
    }

    // get class specific ai
    switch (m_bot->getClass())
    {
        case CLASS_PRIEST:
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotPriestAI(m_master, m_bot, this);
            break;
        case CLASS_MAGE:
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotMageAI(m_master, m_bot, this);
            break;
        case CLASS_WARLOCK:
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotWarlockAI(m_master, m_bot, this);
            break;
        case CLASS_WARRIOR:
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotWarriorAI(m_master, m_bot, this);
            break;
        case CLASS_SHAMAN:
            if (m_bot->GetSpec() == SHAMAN_SPEC_ENHANCEMENT)
                m_combatStyle = COMBAT_MELEE;
            else
                m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotShamanAI(m_master, m_bot, this);
            break;
        case CLASS_PALADIN:
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotPaladinAI(m_master, m_bot, this);
            break;
        case CLASS_ROGUE:
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotRogueAI(m_master, m_bot, this);
            break;
        case CLASS_DRUID:
            if (m_bot->GetSpec() == DRUID_SPEC_FERAL)
                m_combatStyle = COMBAT_MELEE;
            else
                m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotDruidAI(m_master, m_bot, this);
            break;
        case CLASS_HUNTER:
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotHunterAI(m_master, m_bot, this);
            ASPECT_OF_THE_MONKEY = initSpell(ASPECT_OF_THE_MONKEY_1);
            break;
        case CLASS_DEATH_KNIGHT:
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotDeathKnightAI(m_master, m_bot, this);
            break;
    }

    HERB_GATHERING      = initSpell(HERB_GATHERING_1);
    MINING              = initSpell(MINING_1);
    SKINNING            = initSpell(SKINNING_1);

    ClearActiveTalentSpec();
}

PlayerbotAI::~PlayerbotAI()
{
    if (m_classAI) delete m_classAI;
}

// finds spell ID for matching substring args
// in priority of full text match, spells not taking reagents, and highest rank
uint32 PlayerbotAI::getSpellId(const char* args, bool master) const
{
    if (!*args)
        return 0;

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return 0;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    int loc = 0;
    if (master)
        loc = m_master->GetSession()->GetSessionDbcLocale();
    else
        loc = m_bot->GetSession()->GetSessionDbcLocale();

    uint32 foundSpellId = 0;
    bool foundExactMatch = false;
    bool foundMatchUsesNoReagents = false;

    for (PlayerSpellMap::iterator itr = m_bot->GetSpellMap().begin(); itr != m_bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

        if (itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled)
            continue;

        const SpellInfo* pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!pSpellInfo || pSpellInfo->IsPassive())
            continue;

        const std::string name = pSpellInfo->SpellName[loc];
        if (name.empty() || !Utf8FitTo(name, wnamepart))
            continue;

        bool isExactMatch = (name.length() == wnamepart.length());
        bool usesNoReagents = (pSpellInfo->Reagent[0] <= 0);

        // if we already found a spell
        bool useThisSpell = true;
        if (foundSpellId > 0)
        {
            if (isExactMatch && !foundExactMatch) {}
            else if (usesNoReagents && !foundMatchUsesNoReagents) {}
            else if (spellId > foundSpellId) {}
            else
                useThisSpell = false;
        }
        if (useThisSpell)
        {
            foundSpellId = spellId;
            foundExactMatch = isExactMatch;
            foundMatchUsesNoReagents = usesNoReagents;
        }
    }

    return foundSpellId;
}

uint32 PlayerbotAI::getPetSpellId(const char* args) const
{
    if (!*args)
        return 0;

    Pet* pet = m_bot->GetPet();
    if (!pet)
        return 0;

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return 0;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    int loc = m_master->GetSession()->GetSessionDbcLocale();

    uint32 foundSpellId = 0;
    bool foundExactMatch = false;
    bool foundMatchUsesNoReagents = false;

    for (PetSpellMap::iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        uint32 spellId = itr->first;

        if (itr->second.state == PETSPELL_REMOVED || sSpellMgr->GetSpellInfo(spellId)->IsPassive())
            continue;

        const SpellInfo* pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!pSpellInfo)
            continue;

        const std::string name = pSpellInfo->SpellName[loc];
        if (name.empty() || !Utf8FitTo(name, wnamepart))
            continue;

        bool isExactMatch = (name.length() == wnamepart.length()) ? true : false;
        bool usesNoReagents = (pSpellInfo->Reagent[0] <= 0) ? true : false;

        // if we already found a spell
        bool useThisSpell = true;
        if (foundSpellId > 0)
        {
            if (isExactMatch && !foundExactMatch) {}
            else if (usesNoReagents && !foundMatchUsesNoReagents) {}
            else if (spellId > foundSpellId) {}
            else
                useThisSpell = false;
        }
        if (useThisSpell)
        {
            foundSpellId = spellId;
            foundExactMatch = isExactMatch;
            foundMatchUsesNoReagents = usesNoReagents;
        }
    }

    return foundSpellId;
}

uint32 PlayerbotAI::initSpell(uint32 spellId) const
{
    // Check if bot knows this spell
    if (!m_bot->HasSpell(spellId))
        return 0;

    uint32 next = 0;
    SpellChainNode const *Node = sSpellMgr->GetSpellChainNode(spellId);
    next = Node && Node->next ? Node->next->Id : 0;
    //const SpellInfo *const spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (next == 0 || !m_bot->HasSpell(next))
    {
        //// Add spell to spellrange map
        //Spell *spell = new Spell(m_bot, spellInfo, TRIGGERED_NONE);
        //float range = spellInfo->GetMaxRange(spellInfo->IsPositive());
        //m_bot->ApplySpellMod(spellId, SPELLMOD_RANGE, range, spell);
        //m_spellRangeMap.insert(std::pair<uint32, float>(spellId, range));
        //delete spell;
        ////sLog->outBasic("Pbot::InitSpell() proceed spell %u (%s): returned %u (%s)", spellId, spellInfo->SpellName[0], spellId, spellInfo->SpellName[0]);
        return spellId;
    }
    else
    {
        //sLog->outBasic("Pbot::InitSpell() proceed spell %u (%s): forwarding to %u (%s)", spellId, spellInfo->SpellName[0], next, sSpellMgr->GetSpellInfo(next)->SpellName[0]);
        return initSpell(next);
    }
    //return (next == 0) ? spellId : next;
}

// Pet spells do not form chains like player spells.
// One of the options to initialize a spell is to use spell icon id
uint32 PlayerbotAI::initPetSpell(uint32 spellIconId)
{
    Pet * pet = m_bot->GetPet();

    if (!pet)
        return 0;

    for (PetSpellMap::iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        const uint32 spellId = itr->first;

        if (itr->second.state == PETSPELL_REMOVED || sSpellMgr->GetSpellInfo(spellId)->IsPassive())
            continue;

        const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!pSpellInfo)
            continue;

        if (pSpellInfo->SpellIconID == spellIconId)
            return spellId;
    }

    // Nothing found
    return 0;
}

/*
 * Send list of the equipment in bot's inventory not currently equipped.
 * This is called when the master is inspecting the bot.
 */
void PlayerbotAI::SendNotEquipList(Player& /*player*/)
{
    // find all unequipped items and put them in
    // a vector of dynamically created lists where the vector index is from 0-18
    // and the list contains Item* that can be equipped to that slot
    // Note: each dynamically created list in the vector must be deleted at end
    // so NO EARLY RETURNS!
    // see enum EquipmentSlots in Player.h to see what equipment slot each index in vector
    // is assigned to. (The first is EQUIPMENT_SLOT_HEAD=0, and last is EQUIPMENT_SLOT_TABARD=18)
    std::list<Item*>* equip[19];
    for (uint8 i = 0; i < 19; ++i)
        equip[i] = NULL;

    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!pItem)
            continue;

        uint16 dest;
        uint8 msg = m_bot->CanEquipItem(NULL_SLOT, dest, pItem, !pItem->IsBag());
        if (msg != EQUIP_ERR_OK)
            continue;

        // the dest looks like it includes the old loc in the 8 higher bits
        // so casting it to a uint8 strips them
        int8 equipSlot = uint8(dest);
        if (!(equipSlot >= 0 && equipSlot < 19))
            continue;

        // create a list if one doesn't already exist
        if (equip[equipSlot] == NULL)
            equip[equipSlot] = new std::list<Item*>;

        std::list<Item*>* itemListForEqSlot = equip[equipSlot];
        itemListForEqSlot->push_back(pItem);
    }

    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (!pItem)
                    continue;

                uint16 dest;
                uint8 msg = m_bot->CanEquipItem(NULL_SLOT, dest, pItem, !pItem->IsBag());
                if (msg != EQUIP_ERR_OK)
                    continue;

                int8 equipSlot = uint8(dest);
                if (!(equipSlot >= 0 && equipSlot < 19))
                    continue;

                // create a list if one doesn't already exist
                if (equip[equipSlot] == NULL)
                    equip[equipSlot] = new std::list<Item*>;

                std::list<Item*>* itemListForEqSlot = equip[equipSlot];
                itemListForEqSlot->push_back(pItem);
            }
    }

    ChatHandler ch(m_master->GetSession());
    bool bAnyEquippable = false;

    const std::string descr[] = { "head", "neck", "shoulders", "body", "chest",
                                  "waist", "legs", "feet", "wrists", "hands", "finger1", "finger2",
                                  "trinket1", "trinket2", "back", "mainhand", "offhand", "ranged",
                                  "tabard" };

    // now send client all items that can be equipped by slot
    for (uint8 equipSlot = 0; equipSlot < 19; ++equipSlot)
    {
        if (equip[equipSlot] == NULL)
            continue;

        if (!bAnyEquippable)
        {
            TellMaster("Here's all the items in my inventory that I can equip:");
            bAnyEquippable = true;
        }

        std::list<Item*>* itemListForEqSlot = equip[equipSlot];
        std::ostringstream out;
        out << descr[equipSlot] << ": ";
        for (std::list<Item*>::iterator it = itemListForEqSlot->begin(); it != itemListForEqSlot->end(); ++it)
        {
            if ((*it))
                MakeItemLink((*it), out, true);
            //const ItemTemplate* const pItemProto = (*it)->GetTemplate();
            //std::string itemName = pItemProto->Name1;
            //ItemLocalization(itemName, pItemProto->ItemId);
            //out << " |cffffffff|Hitem:" << pItemProto->ItemId << ":0:0:0:0:0:0:0" << "|h[" << itemName << "]|h|r";
        }
        ch.SendSysMessage(out.str().c_str());

        delete itemListForEqSlot; // delete list of Item*
    }

    if (!bAnyEquippable)
        TellMaster("There are no items in my inventory that I can equip.");
}

void PlayerbotAI::FollowAutoReset(Player& /*player*/)
{
    if (FollowAutoGo != 0)
    {
        FollowAutoGo = 3;
        SetMovementOrder(MOVEMENT_FOLLOW, m_master);
    }
}

void PlayerbotAI::AutoUpgradeEquipment(Player& /*player*/) // test for autoequip
{
    ChatHandler ch(m_master->GetSession());
    std::ostringstream out;
    std::ostringstream msg;
    uint32 calc = .10;
    if (AutoEquipPlug != 1)
        if (AutoEquipPlug == 2)
            AutoEquipPlug = 0;
        else
            return;
    // check equipped items for anything that is worn and UNequip them first if possible
    for (uint8 eqslot = EQUIPMENT_SLOT_START; eqslot < EQUIPMENT_SLOT_END; eqslot++)
    {
        Item* const eqitem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, eqslot);
        if (!eqitem)
            continue;
        // if item durability is less than 10% of max durability, UNequip it.
        if (eqitem->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && eqitem->GetUInt32Value(ITEM_FIELD_DURABILITY) <= (calc * eqitem->GetUInt32Value(ITEM_FIELD_MAXDURABILITY)))
        {
            ItemPosCountVec sDest;
            InventoryResult msg = m_bot->CanStoreItem( NULL_BAG, NULL_SLOT, sDest, eqitem, false );
            if(msg == EQUIP_ERR_OK)
            {
                m_bot->RemoveItem(INVENTORY_SLOT_BAG_0, eqslot, true);
                m_bot->StoreItem( sDest, eqitem, true );
            }
            else
            {
                m_bot->SendEquipError(msg, eqitem, NULL);
            }
        }
    }
    // Find equippable items in main backpack one at a time
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!pItem)
            continue;
        // if item durability is less than 10% of max durability, ignore it..
        if (pItem->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && pItem->GetUInt32Value(ITEM_FIELD_DURABILITY) <= (calc * pItem->GetUInt32Value(ITEM_FIELD_MAXDURABILITY)))
        {
            MakeItemLink(pItem, out, true);
            continue;
        }
        uint32 spellId = 0;
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (pItem->GetTemplate()->Spells[i].SpellId > 0)
            {
                spellId = pItem->GetTemplate()->Spells[i].SpellId;
                break;
            }
        }
        if (pItem->GetTemplate()->Flags & ITEM_PROTO_FLAG_OPENABLE && spellId == 0)
        {
            std::string oops = "Oh.. Look!! Theres something Inside this!!!";
            m_bot->Say(oops, LANG_UNIVERSAL);
            UseItem(pItem);
            continue;
        }
        if (uint32 questid = pItem->GetTemplate()->StartQuest)
        {
            Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
            if (m_bot->GetQuestStatus(questid) == QUEST_STATUS_COMPLETE)
                continue;
            else if (!m_bot->CanTakeQuest(qInfo, false))
            {
                std::string oops = "Great..more junk..can I get rid of this please?";
                m_bot->Say(oops, LANG_UNIVERSAL);
                continue;
            }
            UseItem(pItem);
        }
        uint16 dest;
        uint8 msg = m_bot->CanEquipItem(NULL_SLOT, dest, pItem, !pItem->IsBag());
        if (msg != EQUIP_ERR_OK)
            continue;
        int8 equipSlot = uint8(dest);
        if (!(equipSlot >= 0 && equipSlot < 19))
            continue;
        Item* const pItem2 = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot); // do we have anything equipped of this type?
        if (!pItem2)// no item to compare to see if has stats useful for this bots class/style so check for stats and equip if possible
        {
            ItemTemplate const *pProto2 = pItem->GetTemplate();
            if (!ItemStatComparison(pProto2, pProto2))
                continue;
            EquipItem(pItem); //no item equipped so equip new one and go to next item.
            continue;
        }
        // we have an equippable item, ..now lets send it to the comparison function to see if its better than we have on.
        AutoEquipComparison(pItem, pItem2); //pItem is new item, pItem2 is equipped item.
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (!pItem)
                    continue;
                // if item durability is less than 10% of max durability, ignore it..
                if (pItem->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && pItem->GetUInt32Value(ITEM_FIELD_DURABILITY) <= (calc * pItem->GetUInt32Value(ITEM_FIELD_MAXDURABILITY)))
                {
                    MakeItemLink(pItem, out, true);
                    continue;
                }
                uint16 dest;
                uint8 msg = m_bot->CanEquipItem(NULL_SLOT, dest, pItem, !pItem->IsBag());
                if (msg != EQUIP_ERR_OK)
                    continue;
                int8 equipSlot = uint8(dest);
                if (!(equipSlot >= 0 && equipSlot < 19))
                    continue;
                Item* const pItem2 = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot); // do we have anything equipped of this type?
                if (!pItem2)
                {
                    ItemTemplate const *pProto2 = pItem->GetTemplate();
                    if (!ItemStatComparison(pProto2, pProto2))
                        continue;
                    EquipItem(pItem); //no item equipped so equip new one if useable stats and go to next item.
                    continue;
                }
                // we have an equippable item, but something else is equipped..now lets send it to the comparison function to see if its better than we have on.
                AutoEquipComparison(pItem, pItem2); //pItem is new item, pItem2 is equipped item.
            }
    }
    if (out.str().size() != 0)
    {
        std::ostringstream tmp;
        tmp << "|h|cff00ffff _______________________________________ ";
        ch.SendSysMessage(tmp.str().c_str());
        ch.SendSysMessage(out.str().c_str());
        ch.SendSysMessage(tmp.str().c_str());
        TellMaster("These items are worn too badly to use.");// check inventory items.. we'll deal with equipped items elsewhere
    }
    InspectUpdate();
}
void PlayerbotAI::AutoEquipComparison(Item *pItem, Item *pItem2)
{
    const static uint32 item_armor_skills[MAX_ITEM_SUBCLASS_ARMOR] =
    {
        0, SKILL_CLOTH, SKILL_LEATHER, SKILL_MAIL, SKILL_PLATE_MAIL, 0, SKILL_SHIELD, 0, 0, 0, 0
    };
    ItemTemplate const *pProto = pItem2->GetTemplate(); // equipped item if any
    ItemTemplate const *pProto2 = pItem->GetTemplate(); // new item to compare
    // DEBUG_LOG("Item Class (%s)",(pProto->Class == ITEM_CLASS_WEAPON ? "Weapon" : "Not Weapon"));
    switch (pProto->Class)
    {
    case ITEM_CLASS_WEAPON:
        {
            // DEBUG_LOG("Current Item DPS (%f) Equippable Item DPS (%f)",pProto->getDPS(),pProto2->getDPS());
            // m_bot->GetSkillValue(pProto->RequiredSkill) < m_bot->GetSkillValue(pProto2->RequiredSkill)
            if (pProto->getDPS() < pProto2->getDPS())   // if new item has a better DPS
            {
                EquipItem(pItem);
                pProto = pProto2; // ensure that the item with the highest DPS is equipped
            }
            break;
        }
    case ITEM_CLASS_ARMOR:
        {
            if (pProto->ItemLevel < pProto2->ItemLevel && pProto->Armor <= pProto2->Armor && m_bot->HasSkill(item_armor_skills[pProto2->SubClass]) &&
                !m_bot->HasSkill(item_armor_skills[pProto2->SubClass + 1])) // itemlevel + armour + armour class
            {
                // First check to see if this item has stats, and if the bot REALLY wants to lose its old item
                if (pProto2->StatsCount > 0)
                {
                    if (!ItemStatComparison(pProto, pProto2))
                        return; // stats on equipped item are better, OR stats are not useful for this bots class/style
                }
                EquipItem(pItem);
                break;
            }
            // now in case they are same itemlevel, but one is better than the other..
            if (pProto->ItemLevel == pProto2->ItemLevel && pProto->Quality < pProto2->Quality && pProto->Armor <= pProto2->Armor &&
                m_bot->HasSkill(item_armor_skills[pProto2->SubClass]) && !m_bot->HasSkill(item_armor_skills[pProto2->SubClass + 1])) // itemlevel + armour + armour class
            {
                // First check to see if this item has stats, and if the bot REALLY wants to lose its old item
                if (pProto2->StatsCount > 0)
                {
                    if (!ItemStatComparison(pProto, pProto2))
                        return; // stats on equipped item are better, OR stats are not useful for this bots class/style
                }
                EquipItem(pItem);
                break;
            }
            if (pProto->ItemLevel <= pProto2->ItemLevel && pProto->Quality < pProto2->Quality && pProto->Armor > pProto2->Armor &&
                m_bot->HasSkill(item_armor_skills[pProto2->SubClass]) && !m_bot->HasSkill(item_armor_skills[pProto2->SubClass + 1])) // itemlevel + armour + armour class
            {
                // First check to see if this item has stats, and if the bot REALLY wants to lose its old item
                if (pProto2->StatsCount > 0)
                {
                    if (!ItemStatComparison(pProto, pProto2))
                        return; // stats on equipped item are better, OR stats are not useful for this bots class/style
                }
                EquipItem(pItem);
                break;
            }
        }
    }
    InspectUpdate();
}
bool PlayerbotAI::ItemStatComparison(const ItemTemplate *pProto, const ItemTemplate *pProto2)
{
    uint8 isclass = 0; // 1= caster 2 = hybrid 3 = melee
    uint8 ishybrid = 0;
    uint8 olditemscore = 0;
    uint8 newitemscore = 0;
    // get class and style to make it easier to compare later
    switch (m_bot->getClass())
    {
    case CLASS_SHAMAN:
        {
            isclass = 2;
            ishybrid = 1; // hybrid caster
            break;
        }
    case CLASS_PRIEST:
        {
            isclass = 1;
            break;
        }
    case CLASS_MAGE:
        {
            isclass = 1;
            break;
        }
    case CLASS_WARLOCK:
        {
            isclass = 1;
            break;
        }
    case CLASS_DRUID:
        {
            ishybrid = 1;
            isclass = 2; // caster
            break;
        }
    }
    switch (m_bot->getClass())
    {
    case CLASS_WARRIOR:
    case CLASS_ROGUE:
        isclass = 3; // melee
        break;
    }
    switch (m_bot->getClass())
    {
    case CLASS_HUNTER:
        isclass = 2;
        ishybrid = 2;
    case CLASS_PALADIN:
    case CLASS_DEATH_KNIGHT:
        isclass = 2; // hybrid melee
        ishybrid = 1;
        break;
    }
    for (int i = 0; i < MAX_ITEM_PROTO_STATS; ++i) // item can only have 10 stats. We check each stat slot available for stat and type.
    {
        uint32 itemmod = pProto->ItemStat[i].ItemStatType; // equipped item stats if any
        uint32 itemmod2 = pProto2->ItemStat[i].ItemStatType; // newitem stats
        //if (!itemmod) // if no stat type in this slot, continue to next slot
        //   continue;
        // caster stats
        if (itemmod == ITEM_MOD_MANA || itemmod == ITEM_MOD_INTELLECT || itemmod == ITEM_MOD_SPIRIT || itemmod == ITEM_MOD_HIT_SPELL_RATING ||
            itemmod == ITEM_MOD_CRIT_SPELL_RATING || itemmod == ITEM_MOD_HASTE_SPELL_RATING || itemmod == ITEM_MOD_SPELL_DAMAGE_DONE ||
            itemmod == ITEM_MOD_MANA_REGENERATION || itemmod == ITEM_MOD_SPELL_POWER || itemmod == ITEM_MOD_SPELL_PENETRATION ||
            itemmod2 == ITEM_MOD_MANA || itemmod2 == ITEM_MOD_INTELLECT || itemmod2 == ITEM_MOD_SPIRIT || itemmod2 == ITEM_MOD_HIT_SPELL_RATING ||
            itemmod2 == ITEM_MOD_CRIT_SPELL_RATING || itemmod2 == ITEM_MOD_HASTE_SPELL_RATING || itemmod2 == ITEM_MOD_SPELL_DAMAGE_DONE ||
            itemmod2 == ITEM_MOD_MANA_REGENERATION || itemmod2 == ITEM_MOD_SPELL_POWER || itemmod2 == ITEM_MOD_SPELL_PENETRATION)
        {
            switch (isclass) // 1 caster, 2 hybrid, 3 melee
            {
            case 1:
                {
                    uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                    uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                    if (itemmod == itemmod2) //same stat type
                    {
                        if (itemmodval < itemmodval2) // which one has the most
                        {
                            if (olditemscore > 0)
                                olditemscore = (olditemscore - 1);
                            newitemscore = (newitemscore + 1);
                        }
                        else
                        {
                            if (newitemscore > 0)
                                newitemscore = (newitemscore - 1);
                            olditemscore = (olditemscore + 1);
                        }
                    }
                    else
                    {
                        if (itemmod)
                            olditemscore = (olditemscore + 1);
                        if (itemmod2)
                            newitemscore = (newitemscore + 1);
                    }

                    break;
                }
            case 2:
                {
                    uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                    uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                    if (ishybrid != 2) //not a hunter
                    {
                        if (itemmod == itemmod2) //same stat type
                        {
                            if (itemmodval < itemmodval2) // which one has the most
                            {
                                if (olditemscore > 0)
                                    olditemscore = (olditemscore - 1);
                                newitemscore = (newitemscore + 1);
                            }
                            else
                            {
                                if (newitemscore > 0)
                                    newitemscore = (newitemscore - 1);
                                olditemscore = (olditemscore + 1);
                            }
                        }
                        else
                        {
                            if (itemmod)
                                olditemscore = (olditemscore + 1);
                            if (itemmod2)
                                newitemscore = (newitemscore + 1);
                        }
                    }
                    else //is a hunter
                    {
                        if (itemmod)
                        {
                            if (olditemscore > 0) //we dont want any negative returns
                                olditemscore = (olditemscore - 1);
                        }
                        if (itemmod2)
                        {
                            if (newitemscore > 0) //we dont want any negative returns
                                newitemscore = (newitemscore - 1);
                        }
                    }
                    break;
                }  // pure melee need nothing from this list.
            case 3:
                {
                    if (itemmod)
                    {
                        if (olditemscore > 0) //we dont want any negative returns
                            olditemscore = (olditemscore - 1);
                    }
                    if (itemmod2)
                    {
                        if (newitemscore > 0) //we dont want any negative returns
                            newitemscore = (newitemscore - 1);
                    }
                    break;
                }
            default:
                break;
            }
        }
        // melee only stats (warrior/rogue) or stats that only apply to melee style combat
        if (itemmod == ITEM_MOD_HEALTH || itemmod == ITEM_MOD_AGILITY || itemmod == ITEM_MOD_STRENGTH ||
            itemmod == ITEM_MOD_DEFENSE_SKILL_RATING || itemmod == ITEM_MOD_DODGE_RATING || itemmod == ITEM_MOD_PARRY_RATING ||
            itemmod == ITEM_MOD_BLOCK_RATING ||	itemmod == ITEM_MOD_HIT_MELEE_RATING || itemmod == ITEM_MOD_CRIT_MELEE_RATING ||
            itemmod == ITEM_MOD_HIT_TAKEN_MELEE_RATING || itemmod == ITEM_MOD_HIT_TAKEN_RANGED_RATING ||itemmod == ITEM_MOD_HIT_TAKEN_SPELL_RATING ||
            itemmod == ITEM_MOD_CRIT_TAKEN_MELEE_RATING || itemmod == ITEM_MOD_CRIT_TAKEN_RANGED_RATING ||
            itemmod == ITEM_MOD_CRIT_TAKEN_SPELL_RATING || itemmod == ITEM_MOD_HASTE_MELEE_RATING ||
            itemmod == ITEM_MOD_HIT_TAKEN_RATING || itemmod == ITEM_MOD_CRIT_TAKEN_RATING || itemmod == ITEM_MOD_ATTACK_POWER ||
            itemmod == ITEM_MOD_BLOCK_VALUE || itemmod2 == ITEM_MOD_HEALTH || itemmod2 == ITEM_MOD_AGILITY || itemmod2 == ITEM_MOD_STRENGTH ||
            itemmod2 == ITEM_MOD_DEFENSE_SKILL_RATING || itemmod2 == ITEM_MOD_DODGE_RATING || itemmod2 == ITEM_MOD_PARRY_RATING ||
            itemmod2 == ITEM_MOD_BLOCK_RATING ||	itemmod2 == ITEM_MOD_HIT_MELEE_RATING || itemmod2 == ITEM_MOD_CRIT_MELEE_RATING ||
            itemmod2 == ITEM_MOD_HIT_TAKEN_MELEE_RATING || itemmod2 == ITEM_MOD_HIT_TAKEN_RANGED_RATING ||itemmod2 == ITEM_MOD_HIT_TAKEN_SPELL_RATING ||
            itemmod2 == ITEM_MOD_CRIT_TAKEN_MELEE_RATING || itemmod2 == ITEM_MOD_CRIT_TAKEN_RANGED_RATING ||
            itemmod2 == ITEM_MOD_CRIT_TAKEN_SPELL_RATING || itemmod2 == ITEM_MOD_HASTE_MELEE_RATING ||
            itemmod2 == ITEM_MOD_HIT_TAKEN_RATING || itemmod2 == ITEM_MOD_CRIT_TAKEN_RATING || itemmod2 == ITEM_MOD_ATTACK_POWER ||
            itemmod2 == ITEM_MOD_BLOCK_VALUE)
        {
            switch (isclass) // 1 caster, 2 hybrid, 3 melee
            {
            case 1:
                {
                    if (itemmod)
                    {
                        if (olditemscore > 0) //we dont want any negative returns
                            olditemscore = (olditemscore - 1);
                    }
                    if (itemmod2)
                    {
                        if (newitemscore > 0) //we dont want any negative returns
                            newitemscore = (newitemscore - 1);
                    }
                    break;
                }
            case 2:
                {
                    uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                    uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                    if (itemmod == itemmod2) //same stat type
                    {
                        if (itemmodval < itemmodval2) // which one has the most
                        {
                            if (olditemscore > 0)
                                olditemscore = (olditemscore - 1);
                            newitemscore = (newitemscore + 1);
                        }
                        else
                        {
                            if (newitemscore > 0)
                                newitemscore = (newitemscore - 1);
                            olditemscore = (olditemscore + 1);
                        }
                    }
                    else
                    {
                        if (itemmod)
                            olditemscore = (olditemscore + 1);
                        if (itemmod2)
                            newitemscore = (newitemscore + 1);
                    }
                    break;
                }
            case 3:
                {
                    uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                    uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                    if (itemmod == itemmod2) //same stat type
                    {
                        if (itemmodval < itemmodval2) // which one has the most
                        {
                            if (olditemscore > 0)
                                olditemscore = (olditemscore - 1);
                            newitemscore = (newitemscore + 1);
                        }
                        else
                        {
                            if (newitemscore > 0)
                                newitemscore = (newitemscore - 1);
                            olditemscore = (olditemscore + 1);
                        }
                    }
                    else
                    {
                        if (itemmod)
                            olditemscore = (olditemscore + 1);
                        if (itemmod2)
                            newitemscore = (newitemscore + 1);
                    }

                    break;
                }
            default:
                break;
            }
        }
        // stats which aren't strictly caster or melee (hybrid perhaps or style dependant)
        if (itemmod == ITEM_MOD_HIT_RATING || itemmod == ITEM_MOD_CRIT_RATING ||
            itemmod == ITEM_MOD_RESILIENCE_RATING || itemmod == ITEM_MOD_HASTE_RATING || itemmod == ITEM_MOD_EXPERTISE_RATING ||
            itemmod == ITEM_MOD_ARMOR_PENETRATION_RATING || itemmod == ITEM_MOD_HEALTH_REGEN ||	itemmod == ITEM_MOD_STAMINA ||
            itemmod2 == ITEM_MOD_HIT_RATING || itemmod2 == ITEM_MOD_CRIT_RATING || itemmod2 == ITEM_MOD_RESILIENCE_RATING ||
            itemmod2 == ITEM_MOD_HASTE_RATING || itemmod2 == ITEM_MOD_EXPERTISE_RATING || itemmod2 == ITEM_MOD_ARMOR_PENETRATION_RATING ||
            itemmod2 == ITEM_MOD_HEALTH_REGEN || itemmod2 == ITEM_MOD_STAMINA)
        {
            switch (isclass) // 1 caster, 2 hybrid, 3 melee
            {
            case 1:
                {
                    uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                    uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                    if (itemmod == itemmod2) //same stat type
                    {
                        if (itemmodval < itemmodval2) // which one has the most
                        {
                            if (olditemscore > 0)
                                olditemscore = (olditemscore - 1);
                            newitemscore = (newitemscore + 1);
                        }
                        else
                        {
                            if (newitemscore > 0)
                                newitemscore = (newitemscore - 1);
                            olditemscore = (olditemscore + 1);
                        }
                    }
                    else
                    {
                        if (itemmod)
                            olditemscore = (olditemscore + 1);
                        if (itemmod2)
                            newitemscore = (newitemscore + 1);
                    }
                    break;
                }
            case 2:
                {
                    uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                    uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                    if (itemmod == itemmod2) //same stat type
                    {
                        if (itemmodval < itemmodval2) // which one has the most
                        {
                            if (olditemscore > 0)
                                olditemscore = (olditemscore - 1);
                            newitemscore = (newitemscore + 1);
                        }
                        else
                        {
                            if (newitemscore > 0)
                                newitemscore = (newitemscore - 1);
                            olditemscore = (olditemscore + 1);
                        }
                    }
                    else
                    {
                        if (itemmod)
                            olditemscore = (olditemscore + 1);
                        if (itemmod2)
                            newitemscore = (newitemscore + 1);
                    }
                    break;
                }
            case 3:
                {
                    uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                    uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                    if (itemmod == itemmod2) //same stat type
                    {
                        if (itemmodval < itemmodval2) // which one has the most
                        {
                            if (olditemscore > 0)
                                olditemscore = (olditemscore - 1);
                            newitemscore = (newitemscore + 1);
                        }
                        else
                        {
                            if (newitemscore > 0)
                                newitemscore = (newitemscore - 1);
                            olditemscore = (olditemscore + 1);
                        }
                    }
                    else
                    {
                        if (itemmod)
                            olditemscore = (olditemscore + 1);
                        if (itemmod2)
                            newitemscore = (newitemscore + 1);
                    }
                    break;
                }
            default:
                break;
            }
            }
            // stats relating to ranged only
            if (itemmod == ITEM_MOD_HIT_RANGED_RATING || itemmod == ITEM_MOD_CRIT_RANGED_RATING || itemmod == ITEM_MOD_HASTE_RANGED_RATING ||
                itemmod == ITEM_MOD_RANGED_ATTACK_POWER || itemmod2 == ITEM_MOD_HIT_RANGED_RATING || itemmod2 == ITEM_MOD_CRIT_RANGED_RATING ||
                itemmod2 == ITEM_MOD_HASTE_RANGED_RATING || itemmod2 == ITEM_MOD_RANGED_ATTACK_POWER)
            {
                switch (isclass) // 1 caster, 2 hybrid, 3 melee
                {
                case 1:
                {
                    if (itemmod)
                    {
                        if (olditemscore > 0) //we dont want any negative returns
                            olditemscore = (olditemscore - 1);
                    }
                    if (itemmod2)
                    {
                        if (newitemscore > 0) //we dont want any negative returns
                            newitemscore = (newitemscore - 1);
                    }
                    break;
                }
            case 2:
                {
                    if (ishybrid != 2) //not a hunter
                    {
                        if (itemmod)
                        {
                            if (olditemscore > 0) //we dont want any negative returns
                                olditemscore = (olditemscore - 1);
                        }
                        if (itemmod2)
                        {
                            if (newitemscore > 0) //we dont want any negative returns
                                newitemscore = (newitemscore - 1);
                        }
                    }
                    else //is a hunter
                    {
                        uint32 itemmodval = pProto->ItemStat[i].ItemStatValue; // equipped item stats if any
                        uint32 itemmodval2 = pProto2->ItemStat[i].ItemStatValue;  // newitem stats
                        if (itemmod == itemmod2) //same stat type
                        {
                            if (itemmodval < itemmodval2) // which one has the most
                            {
                                if (olditemscore > 0)
                                    olditemscore = (olditemscore - 1);
                                newitemscore = (newitemscore + 1);
                            }
                            else
                            {
                                if (newitemscore > 0)
                                    newitemscore = (newitemscore - 1);
                                olditemscore = (olditemscore + 1);
                            }
                        }
                        else
                        {
                            if (itemmod)
                                olditemscore = (olditemscore + 1);
                            if (itemmod2)
                                newitemscore = (newitemscore + 1);
                        }
                    }
                    break;
                }
            case 3:
                {
                    if (itemmod)
                    {
                        if (olditemscore > 0) //we dont want any negative returns
                            olditemscore = (olditemscore - 1);
                    }
                    if (itemmod2)
                    {
                        if (newitemscore > 0) //we dont want any negative returns
                            newitemscore = (newitemscore - 1);
                    }
                    break;
                }
            default:
                break;
            }
        }
    }
    if (olditemscore <= newitemscore)
        return true;
    else
        return false;
}

void PlayerbotAI::SendQuestNeedList()
{
    std::ostringstream out;

    for (BotNeedItem::iterator itr = m_needItemList.begin(); itr != m_needItemList.end(); ++itr)
    {
        ItemTemplate const* pItemProto = sObjectMgr->GetItemTemplate(itr->first);
        if (pItemProto)
        {
            std::string itemName = pItemProto->Name1;
            ItemLocalization(itemName, pItemProto->ItemId);

            out << " " << itr->second << "x|cffffffff|Hitem:" << pItemProto->ItemId
                << ":0:0:0:0:0:0:0" << "|h[" << itemName
                << "]|h|r";
        }
    }

    for (BotNeedItem::iterator itr = m_needCreatureOrGOList.begin(); itr != m_needCreatureOrGOList.end(); ++itr)
    {
        CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(itr->first);
        if (cInfo)
        {
            std::string creatureName = cInfo->Name;
            CreatureLocalization(creatureName, cInfo->Entry);
            out << " " << itr->second << "x|cFFFFFF00|Hcreature_entry:" << itr->first << "|h[" << creatureName << "]|h|r";
        }

        if (m_bot->HasQuestForGO(itr->first))
        {
            GameObjectTemplate const* gInfo = sObjectMgr->GetGameObjectTemplate(itr->first);
            if (gInfo)
            {
                std::string gameobjectName = gInfo->name;
                GameObjectLocalization(gameobjectName, gInfo->entry);
                out << " " << itr->second << "x|cFFFFFF00|Hgameobject_entry:" << itr->first << "|h[" << gameobjectName << "]|h|r";
            }
        }
    }

    TellMaster("Here's a list of all things needed for quests:");
    if (!out.str().empty())
        TellMaster(out.str().c_str());
}

bool PlayerbotAI::IsItemUseful(uint32 itemid)
{
    const static uint32 item_weapon_skills[MAX_ITEM_SUBCLASS_WEAPON] =
    {
        SKILL_AXES,     SKILL_2H_AXES,  SKILL_BOWS,          SKILL_GUNS,      SKILL_MACES,
        SKILL_2H_MACES, SKILL_POLEARMS, SKILL_SWORDS,        SKILL_2H_SWORDS, 0,
        SKILL_STAVES,   0,              0,                   SKILL_UNARMED,   0,
        SKILL_DAGGERS,  SKILL_THROWN,   SKILL_ASSASSINATION, SKILL_CROSSBOWS, SKILL_WANDS,
        SKILL_FISHING
    };

    const static uint32 item_armor_skills[MAX_ITEM_SUBCLASS_ARMOR] =
    {
        0, SKILL_CLOTH, SKILL_LEATHER, SKILL_MAIL, SKILL_PLATE_MAIL, 0, SKILL_SHIELD, 0, 0, 0, 0
    };

    ItemTemplate const *pProto = sObjectMgr->GetItemTemplate(itemid);
    if (!pProto || pProto->Quality < ITEM_QUALITY_NORMAL)
        return false;

    // do we already have the max allowed of item if more than zero?
    if (pProto->MaxCount > 0 && m_bot->HasItemCount(itemid, pProto->MaxCount, true))
        return false;

    // quest related items
    if (pProto->StartQuest > 0 && HasCollectFlag(COLLECT_FLAG_QUEST))
        return true;

    switch (pProto->Class)
    {
        case ITEM_CLASS_WEAPON:
            if (pProto->SubClass >= MAX_ITEM_SUBCLASS_WEAPON)
                return false;
            else
                return m_bot->HasSkill(item_weapon_skills[pProto->SubClass]);
            break;
        case ITEM_CLASS_ARMOR:
            if (pProto->SubClass >= MAX_ITEM_SUBCLASS_ARMOR)
                return false;
            else
                return (m_bot->HasSkill(item_armor_skills[pProto->SubClass]) && !m_bot->HasSkill(item_armor_skills[pProto->SubClass + 1]));
            break;
        case ITEM_CLASS_QUEST:
            if (!HasCollectFlag(COLLECT_FLAG_QUEST))
                break;
        case ITEM_CLASS_KEY:
            return true;
        case ITEM_CLASS_GEM:
            if ((m_bot->HasSkill(SKILL_BLACKSMITHING) ||
                 m_bot->HasSkill(SKILL_ENGINEERING) ||
                 m_bot->HasSkill(SKILL_JEWELCRAFTING)))
                return true;
            break;
        case ITEM_CLASS_TRADE_GOODS:
            if (!HasCollectFlag(COLLECT_FLAG_PROFESSION))
                break;

            switch (pProto->SubClass)
            {
                case ITEM_SUBCLASS_PARTS:
                case ITEM_SUBCLASS_EXPLOSIVES:
                case ITEM_SUBCLASS_DEVICES:
                    if (m_bot->HasSkill(SKILL_ENGINEERING))
                        return true;
                    break;
                case ITEM_SUBCLASS_JEWELCRAFTING:
                    if (m_bot->HasSkill(SKILL_JEWELCRAFTING))
                        return true;
                    break;
                case ITEM_SUBCLASS_CLOTH:
                    if (m_bot->HasSkill(SKILL_TAILORING))
                        return true;
                    break;
                case ITEM_SUBCLASS_LEATHER:
                    if (m_bot->HasSkill(SKILL_LEATHERWORKING))
                        return true;
                    break;
                case ITEM_SUBCLASS_METAL_STONE:
                    if ((m_bot->HasSkill(SKILL_BLACKSMITHING) ||
                         m_bot->HasSkill(SKILL_ENGINEERING) ||
                         m_bot->HasSkill(SKILL_MINING)))
                        return true;
                    break;
                case ITEM_SUBCLASS_MEAT:
                    if (m_bot->HasSkill(SKILL_COOKING))
                        return true;
                    break;
                case ITEM_SUBCLASS_HERB:
                    if ((m_bot->HasSkill(SKILL_HERBALISM) ||
                         m_bot->HasSkill(SKILL_ALCHEMY) ||
                         m_bot->HasSkill(SKILL_INSCRIPTION)))
                        return true;
                    break;
                case ITEM_SUBCLASS_ELEMENTAL:
                    return true;    // pretty much every profession uses these a bit
                case ITEM_SUBCLASS_ENCHANTING:
                    if (m_bot->HasSkill(SKILL_ENCHANTING))
                        return true;
                    break;
                default:
                    break;
            }
            break;
        case ITEM_CLASS_RECIPE:
        {
            if (!HasCollectFlag(COLLECT_FLAG_PROFESSION))
                break;

            // skip recipes that we have
            if (m_bot->HasSpell(pProto->Spells[2].SpellId))
                break;

            switch (pProto->SubClass)
            {
                case ITEM_SUBCLASS_LEATHERWORKING_PATTERN:
                    if (m_bot->HasSkill(SKILL_LEATHERWORKING))
                        return true;
                    break;
                case ITEM_SUBCLASS_TAILORING_PATTERN:
                    if (m_bot->HasSkill(SKILL_TAILORING))
                        return true;
                    break;
                case ITEM_SUBCLASS_ENGINEERING_SCHEMATIC:
                    if (m_bot->HasSkill(SKILL_ENGINEERING))
                        return true;
                    break;
                case ITEM_SUBCLASS_BLACKSMITHING:
                    if (m_bot->HasSkill(SKILL_BLACKSMITHING))
                        return true;
                    break;
                case ITEM_SUBCLASS_COOKING_RECIPE:
                    if (m_bot->HasSkill(SKILL_COOKING))
                        return true;
                    break;
                case ITEM_SUBCLASS_ALCHEMY_RECIPE:
                    if (m_bot->HasSkill(SKILL_ALCHEMY))
                        return true;
                    break;
                case ITEM_SUBCLASS_FIRST_AID_MANUAL:
                    if (m_bot->HasSkill(SKILL_FIRST_AID))
                        return true;
                    break;
                case ITEM_SUBCLASS_ENCHANTING_FORMULA:
                    if (m_bot->HasSkill(SKILL_ENCHANTING))
                        return true;
                    break;
                case ITEM_SUBCLASS_FISHING_MANUAL:
                    if (m_bot->HasSkill(SKILL_FISHING))
                        return true;
                    break;
                case ITEM_SUBCLASS_JEWELCRAFTING_RECIPE:
                    if (m_bot->HasSkill(SKILL_JEWELCRAFTING))
                        return true;
                    break;
                default:
                    break;
            }
        }
        default:
            break;
    }

    return false;
}

void PlayerbotAI::ReloadAI()
{
    switch (m_bot->getClass())
    {
        case CLASS_PRIEST:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotPriestAI(m_master, m_bot, this);
            break;
        case CLASS_MAGE:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotMageAI(m_master, m_bot, this);
            break;
        case CLASS_WARLOCK:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotWarlockAI(m_master, m_bot, this);
            break;
        case CLASS_WARRIOR:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotWarriorAI(m_master, m_bot, this);
            break;
        case CLASS_SHAMAN:
            if (m_classAI) delete m_classAI;
            if (m_bot->GetSpec() == SHAMAN_SPEC_ENHANCEMENT)
                m_combatStyle = COMBAT_MELEE;
            else
                m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotShamanAI(m_master, m_bot, this);
            break;
        case CLASS_PALADIN:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotPaladinAI(m_master, m_bot, this);
            break;
        case CLASS_ROGUE:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotRogueAI(m_master, m_bot, this);
            break;
        case CLASS_DRUID:
            if (m_classAI) delete m_classAI;
            if (m_bot->GetSpec() == DRUID_SPEC_FERAL)
                m_combatStyle = COMBAT_MELEE;
            else
                m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotDruidAI(m_master, m_bot, this);
            break;
        case CLASS_HUNTER:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_RANGED;
            m_classAI = (PlayerbotClassAI *) new PlayerbotHunterAI(m_master, m_bot, this);
            break;
        case CLASS_DEATH_KNIGHT:
            if (m_classAI) delete m_classAI;
            m_combatStyle = COMBAT_MELEE;
            m_classAI = (PlayerbotClassAI *) new PlayerbotDeathKnightAI(m_master, m_bot, this);
            break;
    }

    HERB_GATHERING      = initSpell(HERB_GATHERING_1);
    MINING              = initSpell(MINING_1);
    SKINNING            = initSpell(SKINNING_1);
}

void PlayerbotAI::SendOrders(Player& /*player*/)
{
    std::ostringstream out;

    if (!m_combatOrder)
        out << "Got no combat orders!";
    else if (m_combatOrder & ORDERS_TANK)
        out << "I TANK";
    else if (m_combatOrder & ORDERS_ASSIST)
        out << "I ASSIST " << (m_targetAssist ? m_targetAssist->GetName() : "unknown");
    else if (m_combatOrder & ORDERS_HEAL)
        out << "I HEAL and DISPEL";
    else if (m_combatOrder & ORDERS_NODISPEL)
        out << "I HEAL and WON'T DISPEL";
    else if (m_combatOrder & ORDERS_PASSIVE)
        out << "I'M PASSIVE";
    if ((m_combatOrder & ORDERS_PRIMARY) && (m_combatOrder & ORDERS_SECONDARY))
        out << " and ";
    if (m_combatOrder & ORDERS_PROTECT)
        out << "I PROTECT " << (m_targetProtect ? m_targetProtect->GetName() : "unknown");
    out << ".";

    if (m_confDebugWhisper)
    {
        out << " " << (IsInCombat() ? "I'm in COMBAT! " : "Not in combat. ");
        out << "Current state is ";
        if (m_botState == BOTSTATE_NORMAL)
            out << "NORMAL";
        else if (m_botState == BOTSTATE_COMBAT)
            out << "COMBAT";
        else if (m_botState == BOTSTATE_TAME)
            out << "TAMING";
        else if (m_botState == BOTSTATE_DEAD)
            out << "DEAD";
        else if (m_botState == BOTSTATE_DEADRELEASED)
            out << "RELEASED";
        else if (m_botState == BOTSTATE_LOOTING)
            out << "LOOTING";
        else if (m_botState == BOTSTATE_FLYING)
            out << "FLYING";
        out << ". Movement order is ";
        if (m_movementOrder == MOVEMENT_NONE)
            out << "NONE";
        else if (m_movementOrder == MOVEMENT_FOLLOW)
            out << "FOLLOW " << (m_followTarget ? m_followTarget->GetName() : "unknown");
        else if (m_movementOrder == MOVEMENT_STAY)
            out << "STAY";
        out << ". Got " << m_attackerInfo.size() << " attacker(s) in list.";
        out << " Next action in " << (m_ignoreAIUpdatesUntilTime - time(NULL)) << "sec.";
    }

    TellMaster(out.str().c_str());
}

// handle outgoing packets the server would send to the client
void PlayerbotAI::HandleBotOutgoingPacket(const WorldPacket& packet)
{
    switch (packet.GetOpcode())
    {
        case MSG_MOVE_TELEPORT_ACK:
        {
            HandleTeleportAck();
            return;
        }

        case SMSG_DUEL_WINNER:
        {
            m_bot->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD);
            return;
        }
        case SMSG_DUEL_COMPLETE:
        {
            m_ignoreAIUpdatesUntilTime = time(NULL) + 4;
            m_ScenarioType = SCENARIO_PVEEASY;
            ReloadAI();
            m_bot->GetMotionMaster()->Clear(true);
            return;
        }
        case SMSG_DUEL_OUTOFBOUNDS:
        {
            m_bot->HandleEmoteCommand(EMOTE_ONESHOT_CHICKEN);
            return;
        }
        case SMSG_DUEL_REQUESTED:
        {
            m_ignoreAIUpdatesUntilTime = 0;
            WorldPacket p(packet);
            uint64 flagGuid;
            p >> flagGuid;
            uint64 playerGuid;
            p >> playerGuid;
            Player* pPlayer = sObjectAccessor->FindPlayer(playerGuid);
            if (canObeyCommandFrom(*pPlayer))
            {
                m_bot->GetMotionMaster()->Clear(true);
                WorldPacket* const packet = new WorldPacket(CMSG_DUEL_ACCEPTED, 8);
                *packet << flagGuid;
                m_bot->GetSession()->QueuePacket(packet); // queue the packet to get around race condition

                // follow target in casting range
                float angle = frand(0, float(M_PI));
                float dist = frand(4, 10);

                m_bot->GetMotionMaster()->Clear(true);
                m_bot->GetMotionMaster()->MoveFollow(pPlayer, dist, angle);

                m_bot->SetSelection(playerGuid);
                m_ignoreAIUpdatesUntilTime = time(NULL) + 4;
                m_ScenarioType = SCENARIO_DUEL;
            }
            return;
        }

        case SMSG_PET_TAME_FAILURE:
        {
            // sLog->outDebug(LOG_FILTER_NONE, "SMSG_PET_TAME_FAILURE");
            WorldPacket p(packet);
            uint8 reason;
            p >> reason;

            switch (reason)
            {
                case PETTAME_INVALIDCREATURE:           // = 1,
                    //sLog->outDebug(LOG_FILTER_NONE, "Invalid Creature");
                    break;
                case PETTAME_TOOMANY:                   // = 2,
                    //sLog->outDebug(LOG_FILTER_NONE, "Too many Creature");
                    break;
                case PETTAME_CREATUREALREADYOWNED:      // = 3,
                    //sLog->outDebug(LOG_FILTER_NONE, "Creature already owned");
                    break;
                case PETTAME_NOTTAMEABLE:               // = 4,
                    //sLog->outDebug(LOG_FILTER_NONE, "Creature not tameable");
                    break;
                case PETTAME_ANOTHERSUMMONACTIVE:       // = 5,
                    //sLog->outDebug(LOG_FILTER_NONE, "Another summon active");
                    break;
                case PETTAME_UNITSCANTTAME:             // = 6,
                    //sLog->outDebug(LOG_FILTER_NONE, "Unit cant tame");
                    break;
                case PETTAME_NOPETAVAILABLE:            // = 7,    // not used in taming
                    //sLog->outDebug(LOG_FILTER_NONE, "No pet available");
                    break;
                case PETTAME_INTERNALERROR:             // = 8,
                    //sLog->outDebug(LOG_FILTER_NONE, "Internal error");
                    break;
                case PETTAME_TOOHIGHLEVEL:              // = 9,
                    //sLog->outDebug(LOG_FILTER_NONE, "Creature level too high");
                    break;
                case PETTAME_DEAD:                      // = 10,   // not used in taming
                    //sLog->outDebug(LOG_FILTER_NONE, "Creature dead");
                    break;
                case PETTAME_NOTDEAD:                   // = 11,   // not used in taming
                    //sLog->outDebug(LOG_FILTER_NONE, "Creature not dead");
                    break;
                case PETTAME_CANTCONTROLEXOTIC:         // = 12,   // 3.x
                    //sLog->outDebug(LOG_FILTER_NONE, "Creature exotic");
                    break;
                case PETTAME_UNKNOWNERROR:              // = 13
                    //sLog->outDebug(LOG_FILTER_NONE, "Unknown error");
                    break;
            }
            return;
        }

        case SMSG_BUY_FAILED:
        {
            WorldPacket p(packet); // 8+4+4+1
            uint64 vendorguid;
            p >> vendorguid;
            uint32 itemid;
            p >> itemid;
            uint8 msg;
            p >> msg; // error msg
            p.resize(13);

            switch (msg)
            {
                case BUY_ERR_CANT_FIND_ITEM:
                    break;
                case BUY_ERR_ITEM_ALREADY_SOLD:
                    break;
                case BUY_ERR_NOT_ENOUGHT_MONEY:
                {
                    Announce(CANT_AFFORD);
                    break;
                }
                case BUY_ERR_SELLER_DONT_LIKE_YOU:
                    break;
                case BUY_ERR_DISTANCE_TOO_FAR:
                    break;
                case BUY_ERR_ITEM_SOLD_OUT:
                    break;
                case BUY_ERR_CANT_CARRY_MORE:
                {
                    Announce(INVENTORY_FULL);
                    break;
                }
                case BUY_ERR_RANK_REQUIRE:
                    break;
                case BUY_ERR_REPUTATION_REQUIRE:
                    break;
            }
            return;
        }

        case SMSG_AUCTION_COMMAND_RESULT:
        {
            uint32 auctionId, Action, ErrorCode;
            std::string action[3] = {"Creating", "Cancelling", "Bidding"};
            std::ostringstream out;

            WorldPacket p(packet);
            p >> auctionId;
            p >> Action;
            p >> ErrorCode;
            p.resize(12);

            switch (ErrorCode)
            {
                case AUCTION_OK:
                {
                    out << "|cff1eff00|h" << action[Action] << " was successful|h|r";
                    break;
                }
                case AUCTION_INTERNAL_ERROR:
                {
                    out << "|cffff0000|hWhile" << action[Action] << ", an internal error occured|h|r";
                    break;
                }
                case AUCTION_NOT_ENOUGHT_MONEY:
                {
                    out << "|cffff0000|hWhile " << action[Action] << ", I didn't have enough money|h|r";
                    break;
                }
                case AUCTION_ITEM_NOT_FOUND:
                {
                    out << "|cffff0000|hItem was not found!|h|r";
                    break;
                }
                case CANNOT_BID_YOUR_AUCTION_ERROR:
                {
                    out << "|cffff0000|hI cannot bid on my own auctions!|h|r";
                    break;
                }
            }
            TellMaster(out.str().c_str());
            return;
        }

        case SMSG_INVENTORY_CHANGE_FAILURE:
        {
            WorldPacket p(packet);
            uint8 err;
            p >> err;

            if (err != EQUIP_ERR_OK)
            {
                switch (err)
                {
                    case EQUIP_ERR_CANT_CARRY_MORE_OF_THIS:
                        TellMaster("I can't carry anymore of those.");
                        return;
                    case EQUIP_ERR_MISSING_REAGENT:
                        TellMaster("I'm missing some reagents for that.");
                        return;
                    case EQUIP_ERR_ITEM_LOCKED:
                        TellMaster("That item is locked.");
                        return;
                    case EQUIP_ERR_ALREADY_LOOTED:
                        TellMaster("That is already looted.");
                        return;
                    case EQUIP_ERR_INVENTORY_FULL:
                    {
                        if (m_inventory_full)
                            return;

                        TellMaster("My inventory is full.");
                        m_inventory_full = true;
                        return;
                    }
                    case EQUIP_ERR_NOT_IN_COMBAT:
                        TellMaster("I can't use that in combat.");
                        return;
                    case EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW:
                        TellMaster("I can't get that now.");
                        return;
                    case EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE:
                        TellMaster("I can only have one of those equipped.");
                        return;
                    case EQUIP_ERR_BANK_FULL:
                        TellMaster("My bank is full.");
                        return;
                    case EQUIP_ERR_ITEM_NOT_FOUND:
                        TellMaster("I can't find the item.");
                        return;
                    case EQUIP_ERR_TOO_FAR_AWAY_FROM_BANK:
                        TellMaster("I'm too far from the bank.");
                        return;
                    case EQUIP_ERR_NONE:
                        TellMaster("I can't use it on that");
                        return;
                    default:
                        TellMaster("I can't use that.");
                        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: HandleBotOutgoingPacket - SMSG_INVENTORY_CHANGE_FAILURE: %u", err);
                        return;
                }
            }
        }

        case SMSG_CAST_FAILED:
        {
            WorldPacket p(packet);
            uint8 castCount;
            uint32 spellId;
            uint8 result;
            std::ostringstream out;

            p >> castCount >> spellId >> result;

            if (result != SPELL_CAST_OK)
            {
                switch (result)
                {
                    case SPELL_FAILED_INTERRUPTED:  // 40
                        //sLog->outDebug(LOG_FILTER_NONE, "spell interrupted (%u)",result);
                        return;

                    case SPELL_FAILED_BAD_TARGETS:  // 12
                    {
                        // sLog->outDebug(LOG_FILTER_NONE, "[%s]bad target (%u) for spellId (%u) & m_CurrentlyCastingSpellId (%u)",m_bot->GetName(),result,spellId,m_CurrentlyCastingSpellId);
                        Spell* const pSpell = GetCurrentSpell();
                        if (pSpell)
                            pSpell->cancel();
                        return;
                    }
                    case SPELL_FAILED_REQUIRES_SPELL_FOCUS: // 102
                    {
                        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
                        if (!spellInfo)
                            return;

                        switch (spellInfo->RequiresSpellFocus) // SpellFocusObject.dbc id
                        {
                            case 1:  // need an anvil
                                out << "|cffff0000I require an anvil.";
                                break;
                            case 2:  // need a loom
                                out << "|cffff0000I require a loom.";
                                break;
                            case 3:  // need forge
                                out << "|cffff0000I require a forge.";
                                break;
                            case 4:  // need cooking fire
                                out << "|cffff0000I require a cooking fire.";
                                break;
                            default:
                                out << "|cffff0000I Require Spell Focus on " << spellInfo->RequiresSpellFocus;
                        }
                        break;
                    }
                    case SPELL_FAILED_CANT_BE_DISENCHANTED:  // 14
                    {
                        out << "|cffff0000Item cannot be disenchanted.";
                        break;
                    }
                    case SPELL_FAILED_CANT_BE_MILLED:  // 16
                    {
                        out << "|cffff0000I cannot mill that.";
                        break;
                    }
                    case SPELL_FAILED_CANT_BE_PROSPECTED:  // 17
                    {
                        out << "|cffff0000There are no gems in this.";
                        break;
                    }
                    case SPELL_FAILED_EQUIPPED_ITEM_CLASS:  // 29
                    {
                        out << "|cffff0000That item is not a valid target.";
                        break;
                    }
                    case SPELL_FAILED_NEED_MORE_ITEMS:  // 55
                    {
                        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
                        if (!spellInfo)
                            return;

                        ItemTemplate const *pProto = sObjectMgr->GetItemTemplate(m_itemTarget);
                        if (!pProto)
                            return;

                        out << "|cffff0000Requires 5 " << pProto->Name1 << ".";
                        m_itemTarget = 0;
                        break;
                    }
                    case SPELL_FAILED_REAGENTS:
                    {
                        out << "|cffff0000I don't have the reagents";
                        break;
                    }
                    default:
                        //sLog->outDebug(LOG_FILTER_NONE, "[%s] SMSG_CAST_FAIL: unknown (%u)", m_bot->GetName(), result);
                        return;
                }
            }
            TellMaster(out.str().c_str());
            return;
        }

        case SMSG_SPELL_FAILURE:
        {
            WorldPacket p(packet);
            uint8 castCount;
            uint32 spellId;
            uint64 casterGuid;

            //packetfix
            p >> casterGuid;
            if (casterGuid != m_bot->GetGUID())
                return;

            p >> castCount >> spellId;
            if (m_CurrentlyCastingSpellId == spellId)
            {
                m_ignoreAIUpdatesUntilTime = time(NULL);
                m_CurrentlyCastingSpellId = 0;
            }
            return;
        }

        // if a change in speed was detected for the master
        // make sure we have the same mount status
        //case SMSG_FORCE_RUN_SPEED_CHANGE:
        //{
        //    WorldPacket p(packet);
        //    uint64 guid;
        //    //guid = extractGuid(p);

        //    p >> guid;

        //    if (guid != m_master->GetGUID())
        //        return;
        //    if (m_master->IsMounted() && !m_bot->IsMounted())
        //    {
        //        //Player Part
        //        Unit::AuraEffectList const& AuraList = m_master->GetAuraEffectsByType(SPELL_AURA_MOUNTED);
        //        if (!AuraList.empty())
        //        {
        //            int32 master_speed1 = AuraList.front()->GetSpellInfo()->Effects[0].BasePoints;
        //            int32 master_speed2 = AuraList.front()->GetSpellInfo()->Effects[1].BasePoints;
        //            int32 master_speed3 = AuraList.front()->GetSpellInfo()->Effects[2].BasePoints;

        //            //Bot Part
        //            uint32 spellMount = 0;
        //            for (PlayerSpellMap::iterator itr = m_bot->GetSpellMap().begin(); itr != m_bot->GetSpellMap().end(); ++itr)
        //            {
        //                if (itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled)
        //                    continue;
        //                uint32 spellId = itr->first;
        //                SpellInfo const *pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
        //                if (!pSpellInfo || pSpellInfo->IsPassive())
        //                    continue;

        //                for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
        //                {
        //                    if (pSpellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOUNTED)
        //                    {
        //                        int32 points = pSpellInfo->Effects[i].BasePoints;
        //                        if (points == master_speed1 || 
        //                            points == master_speed2 || 
        //                            points == master_speed3)
        //                        {
        //                            spellMount = spellId;
        //                            break;
        //                        }
        //                    }
        //                }
        //            }
        //            if (spellMount)
        //                m_bot->CastSpell(m_bot, spellMount, true);
        //            else
        //                SendWhisper("Cannot find approriate mount!", *m_master);
        //        }
        //    }
        //    else if (!m_master->IsMounted() && m_bot->IsMounted())
        //    {
        //        WorldPacket emptyPacket;
        //        m_bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);  //updated code
        //    }
        //    return;
        //}

        // handle flying acknowledgement
        //case SMSG_MOVE_SET_CAN_FLY:
        //{
        //    WorldPacket p(packet);
        //    uint64 guid;
        //    //packetfix
        //    //guid = extractGuid(p);

        //    p >> guid;
        //    if (guid != m_bot->GetGUID())
        //        return;
        //    m_bot->m_movementInfo.AddMovementFlag(MOVEMENTFLAG_FLYING);
        //    //m_bot->SetSpeed(MOVE_RUN, m_master->GetSpeed(MOVE_FLIGHT) +0.1f, true);
        //    return;
        //}

        // handle dismount flying acknowledgement
        //case SMSG_MOVE_UNSET_CAN_FLY:
        //{
        //    WorldPacket p(packet);
        //    uint64 guid;
        //    //packetfix
        //    //guid = extractGuid(p);

        //    p >> guid;
        //    if (guid != m_bot->GetGUID())
        //        return;
        //    m_bot->m_movementInfo.RemoveMovementFlag(MOVEMENTFLAG_FLYING);
        //    //m_bot->SetSpeed(MOVE_RUN,m_master->GetSpeedRate(MOVE_RUN),true);
        //    return;
        //}

        // If the leader role was given to the bot automatically give it to the master
        // if the master is in the group, otherwise leave group
        case SMSG_GROUP_SET_LEADER:
        {
            WorldPacket p(packet);
            std::string name;
            p >> name;
            if (m_bot->GetGroup() && name == m_bot->GetName())
            {
                if (m_bot->GetGroup()->IsMember(m_master->GetGUID()))
                {
                    p.resize(8);
                    p << m_master->GetGUID();
                    m_bot->GetSession()->HandleGroupSetLeaderOpcode(p);
                }
                else
                {
                    p.clear(); // not really needed
                    m_bot->GetSession()->HandleGroupDisbandOpcode(p); // packet not used updated code
                }
            }
            return;
        }

        // If the master leaves the group, then the bot leaves too
        case SMSG_PARTY_COMMAND_RESULT:
        {
            WorldPacket p(packet);
            uint32 operation;
            p >> operation;
            std::string member;
            p >> member;
            uint32 result;
            p >> result;
            p.clear();
            if (operation == PARTY_OP_LEAVE)
                if (member == m_master->GetName())
                    m_bot->GetSession()->HandleGroupDisbandOpcode(p);  // packet not used updated code
            return;
        }

        // Handle Group invites (auto accept if master is in group, otherwise decline & send message
        case SMSG_GROUP_INVITE:
        {
            //if (const Group* const grp = m_bot->GetGroupInvite())
            //{
            //    Player* inviter = sObjectAccessor->FindPlayer(grp->GetLeaderGUID());
            //    if (!inviter)
            //        return;

            //    WorldPacket p;
            //    if (!canObeyCommandFrom(*inviter))
            //    {
            //        std::string buf = "I can't accept your invite unless you first invite my master ";
            //        buf += m_master->GetName();
            //        buf += ".";
            //        SendWhisper(buf, *inviter);
            //        m_bot->GetSession()->HandleGroupDeclineOpcode(p); // packet not used
            //    }
            //    else
            //        m_bot->GetSession()->HandleGroupAcceptOpcode(p);  // packet not used
            //}
            return;
        }

        // Handle when another player opens the trade window with the bot
        // also sends list of tradable items bot can trade if bot is allowed to obey commands from
        case SMSG_TRADE_STATUS:
        {
            if (m_bot->GetTrader() == NULL)
                break;

            WorldPacket p(packet);
            uint32 status;
            p >> status;
            p.resize(4);

            if (status == TRADE_STATUS_TRADE_ACCEPT)
            {
                m_bot->GetSession()->HandleAcceptTradeOpcode(p);  // packet not used
                SetQuestNeedItems();
            }

            else if (status == TRADE_STATUS_BEGIN_TRADE)
            {
                m_bot->GetSession()->HandleBeginTradeOpcode(p); // packet not used

                if (!canObeyCommandFrom(*(m_bot->GetTrader())))
                {
                    // TODO: Really? What if I give a bot all my junk so it's inventory is full when a nice green/blue/purple comes along?
                    SendWhisper("I'm not allowed to trade you any of my items, but you are free to give me money or items.", *(m_bot->GetTrader()));
                    return;
                }

                // list out items available for trade
                std::ostringstream out;
                std::list<std::string> lsItemsTradable;
                std::list<std::string> lsItemsUntradable;

                // list out items in main backpack
                for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
                {
                    const Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                    if (pItem)
                    {
                        MakeItemLink(pItem, out, true);
                        if (pItem->CanBeTraded())
                            lsItemsTradable.push_back(out.str());
                        else
                            lsItemsUntradable.push_back(out.str());
                        out.str("");
                    }
                }

                // list out items in other removable backpacks
                for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
                {
                    const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
                    if (pBag)
                        // Very cool, but unnecessary
                        //const ItemTemplate* const pBagProto = pBag->GetTemplate();
                        //std::string bagName = pBagProto->Name1;
                        //ItemLocalization(bagName, pBagProto->ItemId);

                        for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
                        {
                            const Item* const pItem = m_bot->GetItemByPos(bag, slot);
                            if (pItem)
                            {
                                MakeItemLink(pItem, out, true);
                                if (pItem->CanBeTraded())
                                    lsItemsTradable.push_back(out.str());
                                else
                                    lsItemsUntradable.push_back(out.str());
                                out.str("");
                            }
                        }
                }

                ChatHandler ch(m_bot->GetTrader()->GetSession());
                out.str("");
                out << "Items I have but cannot trade:";
                uint32 count = 0;
                for (std::list<std::string>::iterator iter = lsItemsUntradable.begin(); iter != lsItemsUntradable.end(); iter++)
                {
                    out << (*iter);
                    // Why this roundabout way of posting max 20 items per whisper? To keep the list scrollable.
                    count++;
                    if (count % 20 == 0)
                    {
                        ch.SendSysMessage(out.str().c_str());
                        out.str("");
                    }
                }
                if (count > 0)
                    ch.SendSysMessage(out.str().c_str());

                out.str("");
                out << "I could give you:";
                count = 0;
                for (std::list<std::string>::iterator iter = lsItemsTradable.begin(); iter != lsItemsTradable.end(); iter++)
                {
                    out << (*iter);
                    // Why this roundabout way of posting max 20 items per whisper? To keep the list scrollable.
                    count++;
                    if (count % 20 == 0)
                    {
                        ch.SendSysMessage(out.str().c_str());
                        out.str("");
                    }
                }
                if (count > 0)
                    ch.SendSysMessage(out.str().c_str());
                else
                    ch.SendSysMessage("I have nothing to give you.");

                // calculate how much money bot has
                // send bot the message
                uint32 copper = m_bot->GetMoney();
                out.str("");
                out << "I have |cff00ff00" << Cash(copper) << "|r";
                SendWhisper(out.str().c_str(), *(m_bot->GetTrader()));
            }
            return;
        }

        case SMSG_SPELL_START:
        {
            WorldPacket p(packet);

            //packetfix
            uint64 castItemGuid;
            uint64 casterGuid;
            p >> castItemGuid;
            p >> casterGuid;
            if (casterGuid != m_bot->GetGUID())
                return;

            uint8 castCount;
            p >> castCount;
            uint32 spellId;
            p >> spellId;
            uint32 castFlags;
            p >> castFlags;
            uint32 msTime;
            p >> msTime;

            const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!pSpellInfo)
                return;

            if (pSpellInfo->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
                return;

            m_ignoreAIUpdatesUntilTime = time(NULL) + (msTime / 1000) + 1;

            return;
        }

        case SMSG_SPELL_GO:
        {
            WorldPacket p(packet);

            //packetfix
            uint64 castItemGuid;
            uint64 casterGuid;
            p >> castItemGuid;
            p >> casterGuid;
            if (casterGuid != m_bot->GetGUID())
                return;

            uint8 castCount;
            p >> castCount;
            uint32 spellId;
            p >> spellId;
            uint32 castFlags;
            p >> castFlags;
            uint32 msTime;
            p >> msTime;

            return;
        }

        // if someone tries to resurrect, then accept
        case SMSG_RESURRECT_REQUEST:
        {
            if (!m_bot->isAlive())
            {
                WorldPacket p(packet);
                uint64 guid;
                p >> guid;

                WorldPacket* const packet = new WorldPacket(CMSG_RESURRECT_RESPONSE, 8 + 1);
                *packet << guid;
                *packet << uint8(1);                        // accept
                m_bot->GetSession()->QueuePacket(packet);   // queue the packet to get around race condition

                // set back to normal
                SetState(BOTSTATE_NORMAL);
                SetIgnoreUpdateTime(0);
            }
            return;
        }

        case SMSG_LOOT_RESPONSE:
        {
            WorldPacket p(packet); // (8+1+4+1+1+4+4+4+4+4+1)
            uint64 guid;
            uint8 loot_type;
            uint32 gold;
            uint8 items;

            p >> guid;      // 8 corpse guid
            p >> loot_type; // 1 loot type
            p >> gold;      // 4 money on corpse
            p >> items;     // 1 number of items on corpse

            if (gold > 0)
            {
                WorldPacket* const packet = new WorldPacket(CMSG_LOOT_MONEY, 0);
                m_bot->GetSession()->QueuePacket(packet);
            }

            for (uint8 i = 0; i < items; ++i)
            {
                uint32 itemid;
                uint32 itemcount;
                uint8 lootslot_type;
                uint8 itemindex;

                p >> itemindex;         // 1 counter
                p >> itemid;            // 4 itemid
                p >> itemcount;         // 4 item stack count
                p.read_skip<uint32>();  // 4 item model
                p.read_skip<uint32>();  // 4 randomSuffix
                p.read_skip<uint32>();  // 4 randomPropertyId
                p >> lootslot_type;     // 1 LootSlotType

                if (lootslot_type != LOOT_SLOT_TYPE_ALLOW_LOOT && lootslot_type != LOOT_SLOT_TYPE_OWNER)
                    continue;

                // skinning or collect loot flag = just auto loot everything for getting object
                // corpse = run checks
                if (loot_type == LOOT_SKINNING || HasCollectFlag(COLLECT_FLAG_LOOT) ||
                    (loot_type == LOOT_CORPSE && (IsInQuestItemList(itemid) || IsItemUseful(itemid))))
                {
                    WorldPacket* const packet = new WorldPacket(CMSG_AUTOSTORE_LOOT_ITEM, 1);
                    *packet << itemindex;
                    m_bot->GetSession()->QueuePacket(packet);
                }
            }

            // release loot
            WorldPacket* const packet = new WorldPacket(CMSG_LOOT_RELEASE, 8);
            *packet << guid;
            m_bot->GetSession()->QueuePacket(packet);

            return;
        }

        case SMSG_LOOT_RELEASE_RESPONSE:
        {
            WorldPacket p(packet);
            uint64 guid;

            p >> guid;

            if (guid == m_lootCurrent)
            {
                Creature *c = m_bot->GetMap()->GetCreature(m_lootCurrent);

                if (c && c->GetCreatureTemplate()->SkinLootId && c->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE)/*!c->lootForSkin*/)
                {
                    uint32 reqSkill = c->GetCreatureTemplate()->GetRequiredLootSkill();
                    // check if it is a leather skin and if it is to be collected (could be ore or herb)
                    if (m_bot->HasSkill(reqSkill) && ((reqSkill != SKILL_SKINNING) ||
                        (HasCollectFlag(COLLECT_FLAG_SKIN) && reqSkill == SKILL_SKINNING)))
                    {
                        // calculate skill requirement
                        uint32 skillValue = m_bot->GetPureSkillValue(reqSkill);
                        uint32 targetLevel = c->getLevel();
                        uint32 reqSkillValue = targetLevel < 10 ? 0 : targetLevel < 20 ? (targetLevel - 10) * 10 : targetLevel * 5;
                        if (skillValue >= reqSkillValue)
                        {
                            if (m_lootCurrent != m_lootPrev)    // if this wasn't previous loot try again
                            {
                                m_lootPrev = m_lootCurrent;
                                SetIgnoreUpdateTime(0);
                                return; // so that the DoLoot function is called again to get skin
                            }
                        }
                        else
                            TellMaster("My skill is %u but it requires %u", skillValue, reqSkillValue);
                    }
                }

                // if previous is current, clear
                if (m_lootPrev == m_lootCurrent)
                    m_lootPrev = 0;
                // clear current target
                m_lootCurrent = 0;
                // clear movement
                m_bot->GetMotionMaster()->Clear();
                m_bot->GetMotionMaster()->MoveIdle();
                SetIgnoreUpdateTime(0);
            }

            return;
        }

        case SMSG_BUY_ITEM:
        {
            WorldPacket p(packet);  // (8+4+4+4
            uint64 vguid;
            p >> vguid;
            uint32 vendorslot;
            p >> vendorslot;
            p.resize(20);

            vendorslot = vendorslot - 1;
            Creature *pCreature = m_bot->GetNPCIfCanInteractWith(vguid, UNIT_NPC_FLAG_VENDOR);
            if (!pCreature)
                return;

            VendorItemData const* vItems = pCreature->GetVendorItems();
            if (!vItems || vItems->Empty())
                return;

            uint32 vCount = vItems ? vItems->GetItemCount() : 0;

            if (vendorslot >= vCount)
                return;

            VendorItem const* crItem = vendorslot < vCount ? vItems->GetItem(vendorslot) : NULL;
            if (!crItem)
                return;

            ItemTemplate const *pProto = sObjectMgr->GetItemTemplate(crItem->item);
            if (pProto)
            {
                std::ostringstream out;
                out << "|cff009900" << "I received item: |r";
                MakeItemLink(pProto, out);
                TellMaster(out.str().c_str());
            }
            return;
        }

        case SMSG_ITEM_PUSH_RESULT:
        {
            WorldPacket p(packet);  // (8+4+4+4+1+4+4+4+4+4+4)
            uint64 guid;

            p >> guid;              // 8 player guid
            if (m_bot->GetGUID() != guid)
                return;

            uint8 bagslot;
            uint32 itemslot, itemid, count, totalcount, received, created;

            p >> received;          // 4 0=looted, 1=from npc
            p >> created;           // 4 0=received, 1=created
            p.read_skip<uint32>();  // 4 IsShowChatMessage
            p >> bagslot;           // 1 bagslot
            p >> itemslot;          // 4 item slot, but when added to stack: 0xFFFFFFFF
            p >> itemid;            // 4 item entry id
            p.read_skip<uint32>();  // 4 SuffixFactor
            p.read_skip<uint32>();  // 4 random item property id
            p >> count;             // 4 count of items
            p >> totalcount;        // 4 count of items in inventory

            ItemTemplate const *pProto = sObjectMgr->GetItemTemplate(itemid);
            if (pProto)
            {
                std::ostringstream out;
                if (received == 1)
                {
                    if (created == 1)
                        out << "|cff009900" << "I created: |r";
                    else
                        out << "|cff009900" << "I received: |r";
                    MakeItemLink(pProto, out);
                    TellMaster(out.str().c_str());
                    Player* const bot = GetPlayerBot();
                    AutoUpgradeEquipment(*bot);
                }
            }

            if (IsInQuestItemList(itemid))
            {
                m_needItemList[itemid] = (m_needItemList[itemid] - count);
                if (m_needItemList[itemid] <= 0)
                    m_needItemList.erase(itemid);
            }

            return;
        }

            /* uncomment this and your bots will tell you all their outgoing packet opcode names
               case SMSG_MONSTER_MOVE:
               case SMSG_UPDATE_WORLD_STATE:
               case SMSG_COMPRESSED_UPDATE_OBJECT:
               case MSG_MOVE_SET_FACING:
               case MSG_MOVE_STOP:
               case MSG_MOVE_HEARTBEAT:
               case MSG_MOVE_STOP_STRAFE:
               case MSG_MOVE_START_STRAFE_LEFT:
               case SMSG_UPDATE_OBJECT:
               case MSG_MOVE_START_FORWARD:
               case MSG_MOVE_START_STRAFE_RIGHT:
               case SMSG_DESTROY_OBJECT:
               case MSG_MOVE_START_BACKWARD:
               case SMSG_AURA_UPDATE_ALL:
               case MSG_MOVE_FALL_LAND:
               case MSG_MOVE_JUMP:
                return;

               default:
               {
                const char* oc = LookupOpcodeName(packet.GetOpcode());

                std::ostringstream out;
                out << "botout: " << oc;
                //sLog->outError(out.str().c_str());

                //TellMaster(oc);
               }
             */
    }
}

uint8 PlayerbotAI::GetHealthPercent(const Unit& target) const
{
    return (static_cast<float> (target.GetHealth()) / target.GetMaxHealth()) * 100;
}

uint8 PlayerbotAI::GetHealthPercent() const
{
    return GetHealthPercent(*m_bot);
}

uint8 PlayerbotAI::GetManaPercent(const Unit& target) const
{
    return (static_cast<float> (target.GetPower(POWER_MANA)) / target.GetMaxPower(POWER_MANA)) * 100;
}

uint8 PlayerbotAI::GetManaPercent() const
{
    return GetManaPercent(*m_bot);
}

uint8 PlayerbotAI::GetBaseManaPercent(const Unit& target) const
{
    if (target.GetPower(POWER_MANA) >= target.GetCreateMana())
        return (100);
    else
        return (static_cast<float> (target.GetPower(POWER_MANA)) / target.GetCreateMana()) * 100;
}

uint8 PlayerbotAI::GetBaseManaPercent() const
{
    return GetBaseManaPercent(*m_bot);
}

uint8 PlayerbotAI::GetRageAmount(const Unit& target) const
{
    return (static_cast<float> (target.GetPower(POWER_RAGE)));
}

uint8 PlayerbotAI::GetRageAmount() const
{
    return GetRageAmount(*m_bot);
}

uint8 PlayerbotAI::GetEnergyAmount(const Unit& target) const
{
    return (static_cast<float> (target.GetPower(POWER_ENERGY)));
}

uint8 PlayerbotAI::GetEnergyAmount() const
{
    return GetEnergyAmount(*m_bot);
}

uint8 PlayerbotAI::GetRunicPower(const Unit& target) const
{
    return (static_cast<float>(target.GetPower(POWER_RUNIC_POWER)));
}

uint8 PlayerbotAI::GetRunicPower() const
{
    return GetRunicPower(*m_bot);
}

bool PlayerbotAI::HasAura(uint32 spellId, const Unit& player) const
{
    if (spellId <= 0)
        return false;
    return player.HasAura(spellId);
}

bool PlayerbotAI::HasAura(const char* spellName) const
{
    return HasAura(spellName, *m_bot);
}

bool PlayerbotAI::HasAura(const char* spellName, const Unit& player) const
{
    uint32 spellId = getSpellId(spellName);
    return (spellId) ? HasAura(spellId, player) : false;
}

Item* PlayerbotAI::FindFood() const
{
    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();
            if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                continue;

            if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                //TellMaster("Found Consumable %s in backpack, slot %u", !pItemProto->Name1.empty() ? pItemProto->Name1.c_str() : NULL, slot);
                for (uint8 i = 0; i != 5; ++i)
                {
                    if (pItemProto->Spells[i].SpellCategory == SPELL_CATEGORY_FOOD)
                    {
                        //TellMaster("It's a food! (spell: %u)", i);
                        return pItem;
                    }
                }
            }
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();

                    if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                        continue;

                    if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_FOOD)
                    {
                        //TellMaster("Found Consumable %s in bag %u, slot %u", !pItemProto->Name1.empty() ? pItemProto->Name1.c_str() : NULL, bag, slot);
                        for (uint8 i = 0; i != 5; ++i)
                        {
                            if (pItemProto->Spells[i].SpellCategory == SPELL_CATEGORY_FOOD)
                            {
                                //TellMaster("It's a food! (spell: %u)", i);
                                return pItem;
                            }
                        }
                    }
                }
            }
    }
    return NULL;
}

Item* PlayerbotAI::FindDrink() const
{
    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        Item *pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();

            if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                continue;

            if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                //TellMaster("Found Consumable %s in backpack, slot %u", !pItemProto->Name1.empty() ? pItemProto->Name1.c_str() : NULL, slot);
                for (uint8 i = 0; i != MAX_ITEM_PROTO_SPELLS; ++i)
                {
                    if (pItemProto->Spells[i].SpellCategory == SPELL_CATEGORY_DRINK)
                    {
                        //TellMaster("It's a drink! (spell: %u)", i);
                        return pItem;
                    }
                }
            }
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();

                    if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                        continue;

                    if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_FOOD)
                    {
                        //TellMaster("Found Consumable %s in bag %u, slot %u", !pItemProto->Name1.empty() ? pItemProto->Name1.c_str() : NULL, bag, slot);
                        for (uint8 i = 0; i != MAX_ITEM_PROTO_SPELLS; ++i)
                        {
                            if (pItemProto->Spells[i].SpellCategory == SPELL_CATEGORY_DRINK)
                            {
                                //TellMaster("It's a drink! (spell: %u)", i);
                                return pItem;
                            }
                        }
                    }
                }
            }
    }
    return NULL;
}

Item* PlayerbotAI::FindBandage() const
{
    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();

            if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                continue;

            if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_BANDAGE)
                return pItem;
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();

                    if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                        continue;

                    if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_BANDAGE)
                        return pItem;
                }
            }
    }
    return NULL;
}
//Find Poison ...Natsukawa
Item* PlayerbotAI::FindPoison() const
{
    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();

            if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                continue;

            if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_ITEM_ENHANCEMENT)
                return pItem;
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();

                    if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                        continue;

                    if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->SubClass == ITEM_SUBCLASS_ITEM_ENHANCEMENT)
                        return pItem;
                }
            }
    }
    return NULL;
}

Item* PlayerbotAI::FindConsumable(uint32 displayId) const
{
    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();

            if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                continue;

            if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->DisplayInfoID == displayId)
                return pItem;
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();

                    if (!pItemProto || m_bot->CanUseItem(pItemProto) != EQUIP_ERR_OK)
                        continue;

                    if (pItemProto->Class == ITEM_CLASS_CONSUMABLE && pItemProto->DisplayInfoID == displayId)
                        return pItem;
                }
            }
    }
    return NULL;
}

void PlayerbotAI::InterruptCurrentCastingSpell()
{
    //TellMaster("I'm interrupting my current spell!");
    WorldPacket* const packet = new WorldPacket(CMSG_CANCEL_CAST, 5);  //changed from thetourist suggestion
    *packet << m_CurrentlyCastingSpellId;
    *packet << m_targetGuidCommand;   //changed from thetourist suggestion
    m_CurrentlyCastingSpellId = 0;
    m_bot->GetSession()->QueuePacket(packet);
}

void PlayerbotAI::Feast()
{
    // stand up if we are done feasting
    if (!(m_bot->GetHealth() < m_bot->GetMaxHealth() || (m_bot->getPowerType() == POWER_MANA && m_bot->GetPower(POWER_MANA) < m_bot->GetMaxPower(POWER_MANA))))
    {
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);
        return;
    }

    // wait 3 seconds before checking if we need to drink more or eat more
    time_t currentTime = time(NULL);
    m_ignoreAIUpdatesUntilTime = currentTime + 3;

    // should we drink another
    if (m_bot->getPowerType() == POWER_MANA && currentTime > m_TimeDoneDrinking
        && ((static_cast<float> (m_bot->GetPower(POWER_MANA)) / m_bot->GetMaxPower(POWER_MANA)) < 0.8))
    {
        Item* pItem = FindDrink();
        if (pItem != NULL)
        {
            UseItem(pItem);
            m_TimeDoneDrinking = currentTime + 30;
            return;
        }
        TellMaster("I need water.");
    }

    // should we eat another
    if (currentTime > m_TimeDoneEating && ((static_cast<float> (m_bot->GetHealth()) / m_bot->GetMaxHealth()) < 0.8))
    {
        Item* pItem = FindFood();
        if (pItem != NULL)
        {
            //TellMaster("eating now...");
            UseItem(pItem);
            m_TimeDoneEating = currentTime + 30;
            return;
        }
        TellMaster("I need food.");
    }

    // if we are no longer eating or drinking
    // because we are out of items or we are above 80% in both stats
    if (currentTime > m_TimeDoneEating && currentTime > m_TimeDoneDrinking)
    {
        TellMaster("done feasting!");
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);
    }
}

// intelligently sets a reasonable combat order for this bot
// based on its class / level / etc
void PlayerbotAI::GetCombatTarget(Unit* forcedTarget)
{
    // set combat state, and clear looting, etc...
    if (m_botState != BOTSTATE_COMBAT)
    {
        SetState(BOTSTATE_COMBAT);
        // m_lootCurrent = ObjectGuid(); This was clearing loot target, causing bots to leave corpses unlooted if interupted by combat. Needs testing.
        // using this caused bot to remove current loot target, and add this new threat to the loot list.  Now it remembers the loot target and adds a new one.
        // Bot will still clear the target if the master gets too far away from it.
        m_targetCombat = 0;
    }

    // update attacker info now
    UpdateAttackerInfo();

    // check for attackers on protected unit, and make it a forcedTarget if any
    if (!forcedTarget && (m_combatOrder & ORDERS_PROTECT) && m_targetProtect != 0)
    {
        Unit *newTarget = FindAttacker((ATTACKERINFOTYPE) (AIT_VICTIMNOTSELF | AIT_HIGHESTTHREAT), m_targetProtect);
        if (newTarget && newTarget != m_targetCombat)
        {
            forcedTarget = newTarget;
            m_targetType = TARGET_THREATEN;
            if (m_confDebugWhisper)
                TellMaster("Changing target to %s to protect %s", forcedTarget->GetName().c_str(), m_targetProtect->GetName().c_str());
        }
    }
    else if (forcedTarget)
    {
        if (m_confDebugWhisper)
            TellMaster("Changing target to %s by force!", forcedTarget->GetName().c_str());
        m_targetType = (m_combatOrder == ORDERS_TANK ? TARGET_THREATEN : TARGET_NORMAL);
    }

    // we already have a target and we are not forced to change it
    if (m_targetCombat && !forcedTarget)
        return;

    // are we forced on a target?
    if (forcedTarget)
    {
        m_targetCombat = forcedTarget;
        m_targetChanged = true;
    }
    // do we have to assist someone?
    if (!m_targetCombat && (m_combatOrder & ORDERS_ASSIST) && m_targetAssist != 0)
    {
        m_targetCombat = FindAttacker((ATTACKERINFOTYPE) (AIT_VICTIMNOTSELF | AIT_LOWESTTHREAT), m_targetAssist);
        if (m_confDebugWhisper && m_targetCombat)
            TellMaster("Attacking %s to assist %s", m_targetCombat->GetName().c_str(), m_targetAssist->GetName().c_str());
        m_targetType = (m_combatOrder == ORDERS_TANK ? TARGET_THREATEN : TARGET_NORMAL);
        m_targetChanged = true;
    }
    // are there any other attackers?
    if (!m_targetCombat)
    {
        m_targetCombat = FindAttacker();
        m_targetType = (m_combatOrder == ORDERS_TANK ? TARGET_THREATEN : TARGET_NORMAL);
        m_targetChanged = true;
    }
    // no attacker found anyway
    if (!m_targetCombat || !m_targetCombat->IsVisible() || !m_targetCombat->isTargetableForAttack())
    {
        m_targetType = TARGET_NORMAL;
        m_targetChanged = false;
        return;
    }

    // if thing to attack is in a duel, then ignore and don't call updateAI for 6 seconds
    // this method never gets called when the bot is in a duel and this code
    // prevents bot from helping
    if (m_targetCombat->GetTypeId() == TYPEID_PLAYER && m_targetCombat->ToPlayer()->duel)
    {
        m_ignoreAIUpdatesUntilTime = time(NULL) + 6;
        return;
    }

    m_bot->SetSelection(m_targetCombat->GetGUID());
    m_ignoreAIUpdatesUntilTime = time(NULL) + 1;

    if (m_bot->getStandState() != UNIT_STAND_STATE_STAND)
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);

    m_bot->Attack(m_targetCombat, true);
    //temp
    if (m_targetCombat->GetMapId() == m_bot->GetMapId())
    {
        m_bot->GetMotionMaster()->MoveChase(m_targetCombat);
        GetClassAI()->DoNextCombatManeuver(m_targetCombat);
    }
    m_targetCombatGUID = m_targetCombat->GetGUID();

    // add thingToAttack to loot list
    if (Creature *cre = m_targetCombat->ToCreature())
        if (cre->GetCreatureTemplate()->lootid != 0)
            m_lootTargets.push_back(m_targetCombat->GetGUID());

    return;
}

void PlayerbotAI::GetDuelTarget(Unit* forcedTarget)
{
    // set combat state, and clear looting, etc...
    if (m_botState != BOTSTATE_COMBAT)
    {
        SetState(BOTSTATE_COMBAT);
        m_targetChanged = true;
        m_targetCombat = forcedTarget;
        m_targetType = TARGET_THREATEN;
        m_combatStyle = COMBAT_MELEE;
    }
    m_bot->Attack(m_targetCombat, true);
}

void PlayerbotAI::DoNextCombatManeuver()
{
    if (m_combatOrder == ORDERS_PASSIVE)
        return;

    // check for new targets
    if (m_ScenarioType == SCENARIO_DUEL)
        GetDuelTarget(m_master);
    else if (Unit *u = m_master->getVictim())
        GetCombatTarget(u);
    else
        GetCombatTarget();

    // check if we have a target - fixes crash reported by rrtn (kill hunter's pet bug)
    // if current target for attacks doesn't make sense anymore
    // clear our orders so we can get orders in next update
    m_targetCombat = sObjectAccessor->FindUnit(m_targetCombatGUID);
    if (!m_targetCombat || 
        m_targetCombat->isDead() || 
        !m_targetCombat->IsInWorld() || 
        !m_targetCombat->IsVisible() || 
        !m_targetCombat->isTargetableForAttack() || 
        //!m_bot->IsHostileTo(m_targetCombat) || 
        !m_bot->IsInMap(m_targetCombat))
    {
        m_bot->AttackStop();
        m_bot->SetSelection(0);
        MovementReset();
        m_bot->InterruptNonMeleeSpells(true);
        m_targetCombat = 0;
        m_targetChanged = false;
        m_targetType = TARGET_NORMAL;
        SetQuestNeedCreatures();
        return;
    }

    // do opening moves, if we changed target
    if (m_targetChanged)
    {
        if (m_classAI)
            m_targetChanged = m_classAI->DoFirstCombatManeuver(m_targetCombat);
        else
            m_targetChanged = false;
    }

    // do normal combat movement
    DoCombatMovement();

    if (m_classAI && !m_targetChanged)
        m_classAI->DoNextCombatManeuver(m_targetCombat);
}

void PlayerbotAI::DoCombatMovement()
{
    if (!m_targetCombat) return;

    float targetDist = m_classAI->GetCombatDistance(m_targetCombat);
    //float radius = m_targetCombat->GetFloatValue(UNIT_FIELD_COMBATREACH)/* + m_bot->GetFloatValue(UNIT_FIELD_COMBATREACH)*/;
    //float dx = m_bot->GetPositionX() - m_targetCombat->GetPositionX();
    //float dy = m_bot->GetPositionY() - m_targetCombat->GetPositionY();
    //float dz = m_bot->GetPositionZ() - m_targetCombat->GetPositionZ();
    //float targetDist = sqrt((dx*dx) + (dy*dy) + (dz*dz)) - radius;
    targetDist > 0 ? targetDist : 0;

    m_bot->SetFacingTo(m_bot->GetAngle(m_targetCombat));

    if (m_combatStyle == COMBAT_MELEE && !m_bot->HasUnitState(UNIT_STATE_CHASE) && ((m_movementOrder == MOVEMENT_STAY && targetDist < ATTACK_DISTANCE) || (m_movementOrder != MOVEMENT_STAY)))
        // melee combat - chase target if in range or if we are not forced to stay
        m_bot->GetMotionMaster()->MoveChase(m_targetCombat);
    else if (m_combatStyle == COMBAT_RANGED && m_movementOrder != MOVEMENT_STAY)
    {
        // ranged combat - just move within spell range
        // TODO: just follow in spell range! how to determine bots spell range?
        if (targetDist > 20.0f)
            m_bot->GetMotionMaster()->MoveChase(m_targetCombat);
        else
            MovementClear();
    }
}

void PlayerbotAI::SetQuestNeedCreatures()
{
    // reset values first
    m_needCreatureOrGOList.clear();

    // run through accepted quests, get quest info and data
    for (int qs = 0; qs < MAX_QUEST_LOG_SIZE; ++qs)
    {
        uint32 questid = m_bot->GetQuestSlotQuestId(qs);
        if (questid == 0)
            continue;

        QuestStatusData &qData = m_bot->getQuestStatusMap()[questid];
        // only check quest if it is incomplete
        if (qData.Status != QUEST_STATUS_INCOMPLETE)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        // All creature/GO slain/casted (not required, but otherwise it will display "Creature slain 0/10")
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
        {
            if (!qInfo->RequiredNpcOrGoCount[i] || (qInfo->RequiredNpcOrGoCount[i] - qData.CreatureOrGOCount[i]) <= 0)
                continue;
            m_needCreatureOrGOList[qInfo->RequiredNpcOrGo[i]] = (qInfo->RequiredNpcOrGoCount[i] - qData.CreatureOrGOCount[i]);
        }
    }
}

void PlayerbotAI::SetQuestNeedItems()
{
    // reset values first
    m_needItemList.clear();

    // run through accepted quests, get quest info and data
    for (int qs = 0; qs < MAX_QUEST_LOG_SIZE; ++qs)
    {
        uint32 questid = m_bot->GetQuestSlotQuestId(qs);
        if (questid == 0)
            continue;

        QuestStatusData &qData = m_bot->getQuestStatusMap()[questid];
        // only check quest if it is incomplete
        if (qData.Status != QUEST_STATUS_INCOMPLETE)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        // check for items we not have enough of
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
        {
            if (!qInfo->RequiredItemCount[i] || (qInfo->RequiredItemCount[i] - qData.ItemCount[i]) <= 0)
                continue;
            m_needItemList[qInfo->RequiredItemId[i]] = (qInfo->RequiredItemCount[i] - qData.ItemCount[i]);

            // collect flags not set to gather quest objects skip remaining section
            if (!HasCollectFlag(COLLECT_FLAG_NEAROBJECT) && !HasCollectFlag(COLLECT_FLAG_QUEST))
                continue;

            // TODO: find faster way to handle this look up instead of using SQL lookup for each item
            QueryResult result;
            // determine if GOs are needed
            result = WorldDatabase.PQuery("SELECT entry FROM gameobject_template WHERE questItem1='%u' "
                "OR questItem2='%u' OR questItem3='%u' OR questItem4='%u' OR questItem5='%u' OR questItem6='%u'",
                qInfo->RequiredItemId[i], qInfo->RequiredItemId[i], qInfo->RequiredItemId[i], qInfo->RequiredItemId[i],
                qInfo->RequiredItemId[i], qInfo->RequiredItemId[i]);

            if (result)
            {
                do
                {
                    Field *fields = result->Fetch();
                    uint32 entry = fields[0].GetUInt32();

                    GameObjectTemplate const * gInfo = sObjectMgr->GetGameObjectTemplate(entry);
                    if (!gInfo)
                        continue;

                    // add this GO to our collection list if is chest/ore/herb
                    if (gInfo->type == GAMEOBJECT_TYPE_CHEST)
                    {
                        m_collectObjects.push_back(entry);
                        m_collectObjects.sort();
                        m_collectObjects.unique();
                    }
                } while (result->NextRow());

                ////delete result;
            }
        }
    }
}

void PlayerbotAI::SetState(BotState state)
{
    // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: SetState - %s switch state %d to %d", m_bot->GetName(), m_botState, state );
    m_botState = state;
}

uint8 PlayerbotAI::GetFreeBagSpace() const
{
    uint8 space = 0;
    for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item *pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!pItem)
            ++space;
    }
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pBag && pBag->GetTemplate()->BagFamily == BAG_FAMILY_MASK_NONE)
            space += pBag->GetFreeSlots();
    }
    return space;
}

void PlayerbotAI::DoFlight()
{
    //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: DoFlight - %s : %u", m_bot->GetName(), m_taxiMaster);

    Creature *npc = m_bot->GetNPCIfCanInteractWith(m_taxiMaster, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: DoFlight - %u not found or you can't interact with it.");
        return;
    }

    m_bot->ActivateTaxiPathTo(m_taxiNodes, npc);
}

void PlayerbotAI::DoLoot()
{
    // clear BOTSTATE_LOOTING if no more loot targets
    if (!m_lootCurrent && m_lootTargets.empty())
    {
        // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: DoLoot - %s is going back to idle", m_bot->GetName() );
        SetState(BOTSTATE_NORMAL);
        m_bot->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);
        m_inventory_full = false;
        return;
    }

    // set first in list to current
    if (!m_lootCurrent)
    {
        m_lootCurrent = m_lootTargets.front();
        m_lootTargets.pop_front();
    }

    WorldObject *wo = //sObjectAccessor->GetObjectInMap(m_lootCurrent, m_bot->GetMap(), (WorldObject*)NULL);
    (WorldObject*)sObjectAccessor->GetObjectByTypeMask(*m_bot, m_lootCurrent, TYPEMASK_UNIT|TYPEMASK_GAMEOBJECT);

    // clear invalid object or object that is too far from master
    if (!wo || m_master->GetDistance(wo) > float(m_confCollectDistanceMax))
    {
        m_lootCurrent = 0;
        return;
    }

    Creature *c = m_bot->GetMap()->GetCreature(m_lootCurrent);
    GameObject *go = m_bot->GetMap()->GetGameObject(m_lootCurrent);

    // clear creature or object that is not spawned or if not creature or object
    if ((c && !m_bot->canSeeOrDetect(c)) || (go && !go->isSpawned()) || (!c && !go))
    {
        m_lootCurrent = 0;
        return;
    }

    uint32 skillId = 0;

    if (c)
    {
        if (c->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE))
            skillId = c->GetCreatureTemplate()->GetRequiredLootSkill();

        // not a lootable creature, clear it
        if (!c->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE) &&
            (!c->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE) ||
             (c->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE) && !m_bot->HasSkill(skillId))))
        {
            m_lootCurrent = 0;
            // clear movement target, take next target on next update
            m_bot->GetMotionMaster()->Clear();
            m_bot->GetMotionMaster()->MoveIdle();
            return;
        }
    }

    if (m_bot->GetDistance(wo) > CONTACT_DISTANCE + wo->GetObjectSize())
    {
        float x, y, z;
        wo->GetContactPoint(m_bot, x, y, z, 0.1f);
        m_bot->GetMotionMaster()->MovePoint(wo->GetMapId(), x, y, z);
        // give time to move to point before trying again
        SetIgnoreUpdateTime(1);
    }

    if (m_bot->GetDistance(wo) < INTERACTION_DISTANCE)
    {
        uint32 reqSkillValue = 0;
        uint32 SkillValue = 0;
        bool keyFailed = false;
        bool skillFailed = false;
        bool forceFailed = false;

        if (c)  // creature
        {
            if (c->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE))
            {
                // loot the creature
                WorldPacket* const packet = new WorldPacket(CMSG_LOOT, 8);
                *packet << m_lootCurrent;
                m_bot->GetSession()->QueuePacket(packet);
                return; // no further processing is needed
                // m_lootCurrent is reset in SMSG_LOOT_RELEASE_RESPONSE after checking for skinloot
            }
            else if (c->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE))
                // not all creature skins are leather, some are ore or herb
                if (m_bot->HasSkill(skillId) && ((skillId != SKILL_SKINNING) ||
                    (HasCollectFlag(COLLECT_FLAG_SKIN) && skillId == SKILL_SKINNING)))
                {
                    // calculate skinning skill requirement
                    uint32 targetLevel = c->getLevel();
                    reqSkillValue = targetLevel < 10 ? 0 : targetLevel < 20 ? (targetLevel - 10) * 10 : targetLevel * 5;
                }

            // creatures cannot be unlocked or forced open
            keyFailed = true;
            forceFailed = true;
        }

        if (go) // object
        {
            // add this GO to our collection list if active and is chest/ore/herb
            if (go && HasCollectFlag(COLLECT_FLAG_NEAROBJECT) && go->GetGoType() == GAMEOBJECT_TYPE_CHEST)
            {
                m_collectObjects.push_back(go->GetEntry());
                m_collectObjects.sort();
                m_collectObjects.unique();
            }

            uint32 reqItem = 0;

            // check skill or lock on object
            uint32 lockId = go->GetGOInfo()->GetLockId();
            LockEntry const *lockInfo = sLockStore.LookupEntry(lockId);
            if (lockInfo)
                for (int i = 0; i < 8; ++i)
                {
                    if (lockInfo->Type[i] == LOCK_KEY_ITEM)
                    {
                        if (lockInfo->Index[i] > 0)
                        {
                            reqItem = lockInfo->Index[i];
                            if (m_bot->HasItemCount(reqItem, 1))
                                break;
                            continue;
                        }
                    }
                    else if (lockInfo->Type[i] == LOCK_KEY_SKILL)
                    {
                        switch (LockType(lockInfo->Index[i]))
                        {
                            case LOCKTYPE_OPEN:
                                if (CastSpell(3365))    // Opening
                                    return;
                                break;
                            case LOCKTYPE_CLOSE:
                                if (CastSpell(6233))    // Closing
                                    return;
                                break;
                            case LOCKTYPE_QUICK_OPEN:
                                if (CastSpell(6247))    // Opening
                                    return;
                                break;
                            case LOCKTYPE_QUICK_CLOSE:
                                if (CastSpell(6247))    // Closing
                                    return;
                                break;
                            case LOCKTYPE_OPEN_TINKERING:
                                if (CastSpell(6477))    // Opening
                                    return;
                                break;
                            case LOCKTYPE_OPEN_KNEELING:
                                if (CastSpell(6478))    // Opening; listed with 17667 and 22810
                                    return;
                                break;
                            case LOCKTYPE_OPEN_ATTACKING:
                                if (CastSpell(8386))    // Attacking
                                    return;
                                break;
                            case LOCKTYPE_SLOW_OPEN:
                                if (CastSpell(21651))   // Opening; also had 26868
                                    return;
                                break;
                            case LOCKTYPE_SLOW_CLOSE:
                                if (CastSpell(21652))   // Closing
                                    return;
                                break;
                            case LOCKTYPE_OPEN_FROM_VEHICLE:
                                if (CastSpell(61437))   // Opening
                                    return;
                                break;
                            default:
                                if (SkillByLockType(LockType(lockInfo->Index[i])) > 0)
                                {
                                    skillId = SkillByLockType(LockType(lockInfo->Index[i]));
                                    reqSkillValue = lockInfo->Skill[i];
                                }
                        }
                    }
                }

            // use key on object if available
            if (reqItem > 0 && m_bot->HasItemCount(reqItem, 1))
            {
                UseItem(m_bot->GetItemByEntry(reqItem), TARGET_FLAG_GAMEOBJECT, m_lootCurrent);
                m_lootCurrent = 0;
                return;
            }
            else
                keyFailed = true;
        }

        // determine bot's skill value for object's required skill
        if (skillId != SKILL_NONE)
            SkillValue = uint32(m_bot->GetSkillValue(skillId));

        // bot has the specific skill or object requires no skill at all
        if ((m_bot->HasSkill(skillId) && skillId != SKILL_NONE) || (skillId == SKILL_NONE && go))
        {
            if (SkillValue >= reqSkillValue)
            {
                switch (skillId)
                {
                    case SKILL_MINING:
                        if (HasTool(TC_MINING_PICK) && CastSpell(MINING))
                            return;
                        else
                            skillFailed = true;
                        break;
                    case SKILL_HERBALISM:
                        if (CastSpell(HERB_GATHERING))
                            return;
                        else
                            skillFailed = true;
                        break;
                    case SKILL_SKINNING:
                        if (c && HasCollectFlag(COLLECT_FLAG_SKIN) &&
                            HasTool(TC_SKINNING_KNIFE) && CastSpell(SKINNING, *c))
                            return;
                        else
                            skillFailed = true;
                        break;
                    case SKILL_LOCKPICKING:
                        if (CastSpell(PICK_LOCK_1))
                            return;
                        else
                            skillFailed = true;
                        break;
                    case SKILL_NONE:
                        if (CastSpell(3365)) //Spell 3365 = Opening?
                            return;
                        else
                            skillFailed = true;
                        break;
                    default:
                        TellMaster("I'm not sure how to get that.");
                        skillFailed = true;
                        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]:DoLoot Skill %u is not implemented", skillId);
                        break;
                }
            }
            else
            {
                TellMaster("My skill is not high enough. It requires %u, but mine is %u.",
                           reqSkillValue, SkillValue);
                skillFailed = true;
            }
        }
        else
        {
            TellMaster("I do not have the required skill.");
            skillFailed = true;
        }

        if (go) // only go's can be forced
        {
            // if pickable, check if a forcible item is available for the bot
            if (skillId == SKILL_LOCKPICKING && 
                (m_bot->HasSkill(SKILL_BLACKSMITHING) || m_bot->HasSkill(SKILL_ENGINEERING)))
            {
                // check for skeleton keys appropriate for lock value
                if (m_bot->HasSkill(SKILL_BLACKSMITHING))
                {
                    Item *kItem = FindKeyForLockValue(reqSkillValue);
                    if (kItem)
                    {
                        TellMaster("I have a skeleton key that can open it!");
                        UseItem(kItem, TARGET_FLAG_GAMEOBJECT, m_lootCurrent);
                        return;
                    }
                    else
                    {
                        TellMaster("I have no skeleton keys that can open that lock.");
                        forceFailed = true;
                    }
                }

                // check for a charge that can blast it open
                if (m_bot->HasSkill(SKILL_ENGINEERING))
                {
                    Item *bItem = FindBombForLockValue(reqSkillValue);
                    if (bItem)
                    {
                        TellMaster("I can blast it open!");
                        UseItem(bItem, TARGET_FLAG_GAMEOBJECT, m_lootCurrent);
                        return;
                    }
                    else
                    {
                        TellMaster("I have nothing to blast it open with.");
                        forceFailed = true;
                    }
                }
            }
            else
                forceFailed = true;
        }

        // if all attempts failed in some way then clear because it won't get SMSG_LOOT_RESPONSE
        if (keyFailed && skillFailed && forceFailed)
        {
            //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: DoLoot attempts failed on [%s]", go ? go->GetGOInfo()->name : c->GetCreatureTemplate()->Name);
            m_lootCurrent = 0;

            // remove this GO from our list using the same settings that it was added with earlier
            if (go && HasCollectFlag(COLLECT_FLAG_NEAROBJECT) && go->GetGoType() == GAMEOBJECT_TYPE_CHEST)
                m_collectObjects.remove(go->GetEntry());

            // clear movement target, take next target on next update
            m_bot->GetMotionMaster()->Clear();
            m_bot->GetMotionMaster()->MoveIdle();
        }
    }
}

void PlayerbotAI::AcceptQuest(Quest const *qInfo, Player *pGiver)
{
    if (!qInfo || !pGiver)
        return;

    uint32 quest = qInfo->GetQuestId();

    if (!pGiver->CanShareQuest(qInfo->GetQuestId()))
    {
        // giver can't share quest
        m_bot->SetDivider(0);
        return;
    }

    if (!m_bot->CanTakeQuest(qInfo, false))
    {
        // can't take quest
        m_bot->SetDivider(0);
        return;
    }

    if (m_bot->GetDivider())
    {
        // send msg to quest giving player
        pGiver->SendPushToPartyResponse(m_bot, QUEST_PARTY_MSG_ACCEPT_QUEST);
        m_bot->SetDivider(0);
    }

    if (m_bot->CanAddQuest(qInfo, false))
    {
        m_bot->AddQuest(qInfo, pGiver);

        if (m_bot->CanCompleteQuest(quest))
            m_bot->CompleteQuest(quest);

        // build needed items if quest contains any
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
            if (qInfo->RequiredItemCount[i] > 0)
            {
                SetQuestNeedItems();
                break;
            }

        // build needed creatures if quest contains any
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
            if (qInfo->RequiredNpcOrGoCount[i] > 0)
            {
                SetQuestNeedCreatures();
                break;
            }

        // Runsttren: did not add typeid switch from WorldSession::HandleQuestgiverAcceptQuestOpcode!
        // I think it's not needed, cause typeid should be TYPEID_PLAYER - and this one is not handled
        // there and there is no default case also.

        if (qInfo->GetSrcSpell() > 0)
            m_bot->CastSpell(m_bot, qInfo->GetSrcSpell(), true);
    }
}

void PlayerbotAI::TurnInQuests(WorldObject *questgiver)
{
    uint64 giverGUID = questgiver->GetGUID();

    if (!m_bot->IsInMap(questgiver))
        TellMaster("hey you are turning in quests without me!");
    else
    {
        m_bot->SetSelection(giverGUID);

        // auto complete every completed quest this NPC has
        m_bot->PrepareQuestMenu(giverGUID);
        QuestMenu& questMenu = m_bot->PlayerTalkClass->GetQuestMenu();
        for (uint32 iI = 0; iI < questMenu.GetMenuItemCount(); ++iI)
        {
            QuestMenuItem const& qItem = questMenu.GetItem(iI);
            uint32 questID = qItem.QuestId;
            Quest const* pQuest = sObjectMgr->GetQuestTemplate(questID);

            std::ostringstream out;
            std::string questTitle  = pQuest->GetTitle();
            QuestLocalization(questTitle, questID);

            QuestStatus status = m_bot->GetQuestStatus(questID);

            // if quest is complete, turn it in
            if (status == QUEST_STATUS_COMPLETE)
            {
                // if bot hasn't already turned quest in
                if (!m_bot->GetQuestRewardStatus(questID))
                {
                    // auto reward quest if no choice in reward
                    if (pQuest->GetRewChoiceItemsCount() == 0)
                    {
                        if (m_bot->CanRewardQuest(pQuest, false))
                        {
                            m_bot->RewardQuest(pQuest, 0, questgiver, false);
                            out << "Quest complete: |cff808080|Hquest:" << questID << ':' << pQuest->GetQuestLevel() << "|h[" << questTitle << "]|h|r";
                        }
                        else
                            out << "|cffff0000Unable to turn quest in:|r |cff808080|Hquest:" << questID << ':' << pQuest->GetQuestLevel() << "|h[" << questTitle << "]|h|r";
                    }

                    // auto reward quest if one item as reward
                    else if (pQuest->GetRewChoiceItemsCount() == 1)
                    {
                        int rewardIdx = 0;
                        ItemTemplate const *pRewardItem = sObjectMgr->GetItemTemplate(pQuest->RewardChoiceItemId[rewardIdx]);
                        std::string itemName = pRewardItem->Name1;
                        ItemLocalization(itemName, pRewardItem->ItemId);
                        if (m_bot->CanRewardQuest(pQuest, rewardIdx, false))
                        {
                            m_bot->RewardQuest(pQuest, rewardIdx, questgiver, true);

                            std::string itemName = pRewardItem->Name1;
                            ItemLocalization(itemName, pRewardItem->ItemId);

                            out << "Quest complete: "
                                << " |cff808080|Hquest:" << questID << ':' << pQuest->GetQuestLevel()
                                << "|h[" << questTitle << "]|h|r reward: |cffffffff|Hitem:"
                                << pRewardItem->ItemId << ":0:0:0:0:0:0:0" << "|h[" << itemName << "]|h|r";
                        }
                        else
                            out << "|cffff0000Unable to turn quest in:|r "
                                << "|cff808080|Hquest:" << questID << ':'
                                << pQuest->GetQuestLevel() << "|h[" << questTitle << "]|h|r"
                                << " reward: |cffffffff|Hitem:"
                                << pRewardItem->ItemId << ":0:0:0:0:0:0:0" << "|h[" << itemName << "]|h|r";
                    }

                    // else multiple rewards - let master pick
                    else
                    {
                        out << "What reward should I take for |cff808080|Hquest:" << questID << ':' << pQuest->GetQuestLevel()
                            << "|h[" << questTitle << "]|h|r? ";
                        for (uint8 i = 0; i < pQuest->GetRewChoiceItemsCount(); ++i)
                        {
                            ItemTemplate const * const pRewardItem = sObjectMgr->GetItemTemplate(pQuest->RewardChoiceItemId[i]);
                            std::string itemName = pRewardItem->Name1;
                            ItemLocalization(itemName, pRewardItem->ItemId);
                            out << "|cffffffff|Hitem:" << pRewardItem->ItemId << ":0:0:0:0:0:0:0" << "|h[" << itemName << "]|h|r";
                        }
                    }
                }
            }

            else if (status == QUEST_STATUS_INCOMPLETE)
                out << "|cffff0000Quest incomplete:|r "
                    << " |cff808080|Hquest:" << questID << ':' << pQuest->GetQuestLevel() << "|h[" << questTitle << "]|h|r";

            else if (status == QUEST_STATUS_NONE && m_bot->CanTakeQuest(pQuest, false))
                out << "|cff00ff00Quest available:|r "
                    << " |cff808080|Hquest:" << questID << ':' << pQuest->GetQuestLevel() << "|h[" << questTitle << "]|h|r";

            if (!out.str().empty())
                TellMaster(out.str());
        }
        AutoUpgradeEquipment(*m_bot);
    }
}

bool PlayerbotAI::IsInCombat()
{
    Pet *pet;
    bool inCombat = false;
    inCombat |= m_bot->isInCombat();
    pet = m_bot->GetPet();
    if (pet)
        inCombat |= pet->isInCombat();
    inCombat |= m_master->isInCombat();
    if (m_bot->GetGroup())
    {
        GroupReference *ref = m_bot->GetGroup()->GetFirstMember();
        while (ref)
        {
            inCombat |= ref->getSource()->isInCombat();
            pet = ref->getSource()->GetPet();
            if (pet)
                inCombat |= pet->isInCombat();
            ref = ref->next();
        }
    }
    return inCombat;
}

void PlayerbotAI::UpdateAttackersForTarget(Unit *victim)
{
    HostileReference *ref = victim->getHostileRefManager().getFirst();
    while (ref)
    {
        ThreatManager *target = ref->getSource();
        uint64 guid = target->getOwner()->GetGUID();
        m_attackerInfo[guid].attacker = target->getOwner();
        m_attackerInfo[guid].victim = target->getOwner()->getVictim();
        m_attackerInfo[guid].threat = target->getThreat(victim);
        m_attackerInfo[guid].count = 1;
        //m_attackerInfo[guid].source = 1; // source is not used so far.
        ref = ref->next();
    }
}

void PlayerbotAI::UpdateAttackerInfo()
{
    // clear old list
    m_attackerInfo.clear();

    // check own attackers
    UpdateAttackersForTarget(m_bot);
    Pet *pet = m_bot->GetPet();
    if (pet)
        UpdateAttackersForTarget(pet);

    // check master's attackers
    UpdateAttackersForTarget(m_master);
    pet = m_master->GetPet();
    if (pet)
        UpdateAttackersForTarget(pet);

    // check all group members now
    if (m_bot->GetGroup())
    {
        GroupReference *gref = m_bot->GetGroup()->GetFirstMember();
        while (gref)
        {
            if (gref->getSource() == m_bot || gref->getSource() == m_master)
            {
                gref = gref->next();
                continue;
            }

            UpdateAttackersForTarget(gref->getSource());
            pet = gref->getSource()->GetPet();
            if (pet)
                UpdateAttackersForTarget(pet);

            gref = gref->next();
        }
    }

    // get highest threat not caused by bot for every entry in AttackerInfoList...
    for (AttackerInfoList::iterator itr = m_attackerInfo.begin(); itr != m_attackerInfo.end(); ++itr)
    {
        if (!itr->second.attacker)
            continue;
        Unit *a = itr->second.attacker;
        float t = 0.00;
        std::list<HostileReference*>::const_iterator i = a->getThreatManager().getThreatList().begin();
        for (; i != a->getThreatManager().getThreatList().end(); ++i)
        {
            if ((*i)->getThreat() > t && (*i)->getTarget() != m_bot)
                t = (*i)->getThreat();
        }
        m_attackerInfo[itr->first].threat2 = t;
    }

    // DEBUG: output attacker info
    //sLog->outBasic( "[PlayerbotAI]: %s m_attackerInfo = {", m_bot->GetName() );
    //for( AttackerInfoList::iterator i=m_attackerInfo.begin(); i!=m_attackerInfo.end(); ++i )
    //    //sLog->outBasic( "[PlayerbotAI]:     [%016I64X] { %08X, %08X, %.2f, %.2f, %d, %d }",
    //        i->first,
    //        (i->second.attacker?i->second.attacker->GetGUIDLow():0),
    //        (i->second.victim?i->second.victim->GetGUIDLow():0),
    //        i->second.threat,
    //        i->second.threat2,
    //        i->second.count,
    //        i->second.source );
    //sLog->outBasic( "[PlayerbotAI]: };" );
}

uint32 PlayerbotAI::EstRepairAll()
{
    uint32 TotalCost = 0;
    // equipped, backpack, bags itself
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        TotalCost += EstRepair(((INVENTORY_SLOT_BAG_0 << 8) | i));

    // bank, buyback and keys not repaired

    // items in inventory bags
    for (int j = INVENTORY_SLOT_BAG_START; j < INVENTORY_SLOT_BAG_END; ++j)
        for (int i = 0; i < MAX_BAG_SIZE; ++i)
            TotalCost += EstRepair(((j << 8) | i));
    return TotalCost;
}

uint32 PlayerbotAI::EstRepair(uint16 pos)
{
    Item* item = m_bot->GetItemByPos(pos);

    uint32 TotalCost = 0;
    if (!item)
        return TotalCost;

    uint32 maxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    if (!maxDurability)
        return TotalCost;

    uint32 curDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);

    uint32 LostDurability = maxDurability - curDurability;
    if (LostDurability > 0)
    {
        ItemTemplate const *ditemProto = item->GetTemplate();

        DurabilityCostsEntry const *dcost = sDurabilityCostsStore.LookupEntry(ditemProto->ItemLevel);
        if (!dcost)
        {
            //sLog->outError("RepairDurability: Wrong item lvl %u", ditemProto->ItemLevel);
            return TotalCost;
        }

        uint32 dQualitymodEntryId = (ditemProto->Quality + 1) * 2;
        DurabilityQualityEntry const *dQualitymodEntry = sDurabilityQualityStore.LookupEntry(dQualitymodEntryId);
        if (!dQualitymodEntry)
        {
            //sLog->outError("RepairDurability: Wrong dQualityModEntry %u", dQualitymodEntryId);
            return TotalCost;
        }

        uint32 dmultiplier = dcost->multiplier[ItemSubClassToDurabilityMultiplierId(ditemProto->Class, ditemProto->SubClass)];
        uint32 costs = uint32(LostDurability * dmultiplier * double(dQualitymodEntry->quality_mod));

        if (costs == 0)                                 //fix for ITEM_QUALITY_ARTIFACT
            costs = 1;

        TotalCost = costs;
    }
    return TotalCost;
}

Unit* PlayerbotAI::FindAttacker(ATTACKERINFOTYPE ait, Unit *victim)
{
    // list empty? why are we here?
    if (m_attackerInfo.empty())
        return 0;

    // not searching something specific - return first in list
    if (!ait)
        return (m_attackerInfo.begin())->second.attacker;

    float t = ((ait & AIT_HIGHESTTHREAT) ? 0.00 : 9999.00);
    Unit *a = 0;
    AttackerInfoList::iterator itr = m_attackerInfo.begin();
    for (; itr != m_attackerInfo.end(); ++itr)
    {
        if ((ait & AIT_VICTIMSELF) && !(ait & AIT_VICTIMNOTSELF) && itr->second.victim != m_bot)
            continue;

        if (!(ait & AIT_VICTIMSELF) && (ait & AIT_VICTIMNOTSELF) && itr->second.victim == m_bot)
            continue;

        if ((ait & AIT_VICTIMNOTSELF) && victim && itr->second.victim != victim)
            continue;

        if (!(ait & (AIT_LOWESTTHREAT | AIT_HIGHESTTHREAT)))
        {
            a = itr->second.attacker;
            itr = m_attackerInfo.end();
        }
        else
        {
            if ((ait & AIT_HIGHESTTHREAT) && /*(itr->second.victim==m_bot) &&*/ itr->second.threat >= t)
            {
                t = itr->second.threat;
                a = itr->second.attacker;
            }
            else if ((ait & AIT_LOWESTTHREAT) && /*(itr->second.victim==m_bot) &&*/ itr->second.threat <= t)
            {
                t = itr->second.threat;
                a = itr->second.attacker;
            }
        }
    }
    return a;
}

void PlayerbotAI::SetCombatOrderByStr(std::string str, Unit *target)
{
    CombatOrderType co;
    if (str == "tank") co = ORDERS_TANK;
    else if (str == "assist") co = ORDERS_ASSIST;
    else if (str == "heal") co = ORDERS_HEAL;
    else if (str == "protect") co = ORDERS_PROTECT;
    else if (str == "passive") co = ORDERS_PASSIVE;
    else if (str == "nodispel") co = ORDERS_NODISPEL;
    else if (str == "resistfrost") {
        co = ORDERS_RESIST;
        m_resistType = SCHOOL_FROST;
    }
    else if (str == "resistnature") {
        co = ORDERS_RESIST;
        m_resistType = SCHOOL_NATURE;
    }
    else if (str == "resistfire") {
        co = ORDERS_RESIST;
        m_resistType = SCHOOL_FIRE;
    }
    else if (str == "resistshadow") {
        co = ORDERS_RESIST;
        m_resistType = SCHOOL_SHADOW;
    }
    else
        co = ORDERS_RESET;
    SetCombatOrder(co, target);
    if (FollowAutoGo != 0)
        FollowAutoGo = 1;
}

void PlayerbotAI::SetCombatOrder(CombatOrderType co, Unit *target)
{
    // reset m_combatOrder after ORDERS_PASSIVE
    if (m_combatOrder == ORDERS_PASSIVE)
    {
        m_combatOrder = ORDERS_NONE;
        m_targetAssist = 0;
        m_targetProtect = 0;
        m_resistType = SCHOOL_NONE;
    }

    if ((co == ORDERS_ASSIST || co == ORDERS_PROTECT) && !target) {
        TellMaster("Erf, you forget to target assist/protect characters!");
        return;
    }
    if (co == ORDERS_RESET) {
        m_combatOrder = ORDERS_NONE;
        m_targetAssist = 0;
        m_targetProtect = 0;
        TellMaster("Orders are cleaned!");
        return;
    }
    if (co == ORDERS_PASSIVE)
    {
        m_combatOrder = ORDERS_PASSIVE;
        SendOrders(*m_master);
        return;
    }
    if (co == ORDERS_PROTECT)
        m_targetProtect = target;
    else if (co == ORDERS_ASSIST)
        m_targetAssist = target;
    if ((co & ORDERS_PRIMARY))
        m_combatOrder = (CombatOrderType) (((uint32) m_combatOrder & (uint32) ORDERS_SECONDARY) | (uint32) co);
    else
        m_combatOrder = (CombatOrderType) (((uint32) m_combatOrder & (uint32) ORDERS_PRIMARY) | (uint32) co);
    SendOrders(*m_master);
}

void PlayerbotAI::SetMovementOrder(MovementOrderType mo, Unit *followTarget)
{
    m_movementOrder = mo;
    m_followTarget = followTarget;
    MovementReset();
}

void PlayerbotAI::MovementReset()
{
    //TellMaster("Debug: MovementReset()");
    // stop moving...
    MovementClear();

    if (m_movementOrder == MOVEMENT_FOLLOW)
    {
        // nothing to follow
        if (!m_followTarget)
            return;
        // don't follow while casting
        if (m_bot->HasUnitState(UNIT_STATE_CASTING))
            return;
        // don't follow while in combat
        if (m_targetCombat)
            return;
        // new check bot
        if (!m_bot->isAlive() || m_bot->IsBeingTeleported() || m_bot->isInFlight())
            return;

        WorldObject* distTarget = m_followTarget;   // target to distance check

        // target is player ?
        Player const* pTarget = m_followTarget->ToPlayer();

        if (pTarget)
        {
            // check player for follow situations
            if (pTarget->IsBeingTeleported() || pTarget->isInFlight())
                return;

            // use player's corpse as distance check target
            if (pTarget->GetCorpse())
                distTarget = pTarget->GetCorpse();
        }

        // is bot too far from the follow target
        if (m_bot->GetMap() != distTarget->GetMap() || m_bot->GetDistance2d(distTarget) > 50)
        {
            //DoTeleport(*m_followTarget);
            m_ignoreAIUpdatesUntilTime = time(NULL) + 2;
            PlayerbotChatHandler ch(m_master);
            if (!ch.teleport(*m_bot, *distTarget))
            {
                TellMaster("I cannot be teleported...");
                // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: DoTeleport - %s failed to teleport", m_bot->GetName() );
                //return;
            }
            m_bot->UpdatePosition(*distTarget, true);
            return;
        }

        if (distTarget == m_followTarget)
        {
            float angle = frand(0, float(M_PI));
            float dist = frand(m_confFollowDistance[0], m_confFollowDistance[1]);
        
            m_bot->GetMotionMaster()->MoveFollow(m_followTarget, dist, angle);
            //m_ignoreAIUpdatesUntilTime = time(NULL) + 1;
        }
        if (FollowAutoGo == 5)
            FollowAutoGo = 1;
    }
}

void PlayerbotAI::MovementClear()
{
    //TellMaster("Debug: MovementClear()");
    // stop...
    m_bot->GetMotionMaster()->Clear(true);
    m_bot->ClearUnitState(UNIT_STATE_CHASE);
    m_bot->ClearUnitState(UNIT_STATE_FOLLOW);

    // stand up...
    if (!m_bot->IsStandState())
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);
}

void PlayerbotAI::BotPlaySound(uint32 soundid)
{
    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << soundid;
    m_master->GetSession()->SendPacket(&data);
}

// BotPlaySound data from SoundEntries.dbc
void PlayerbotAI::Announce(AnnounceFlags msg)
{
    switch (m_bot->getRace())
    {
        case RACE_HUMAN:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1908) : BotPlaySound(2032); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1875) : BotPlaySound(1999); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1924) : BotPlaySound(2048); break;
                default: break;
            }
            break;
        case RACE_ORC:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2319) : BotPlaySound(2374); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2284) : BotPlaySound(2341); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2335) : BotPlaySound(2390); break;
                default: break;
            }
            break;
        case RACE_DWARF:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1630) : BotPlaySound(1686); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1581) : BotPlaySound(1654); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1636) : BotPlaySound(1702); break;
                default: break;
            }
            break;
        case RACE_NIGHTELF:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2151) : BotPlaySound(2262); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2118) : BotPlaySound(2229); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2167) : BotPlaySound(2278); break;
                default: break;
            }
            break;
        case RACE_UNDEAD_PLAYER:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2096) : BotPlaySound(2207); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2054) : BotPlaySound(2173); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2112) : BotPlaySound(2223); break;
                default: break;
            }
            break;
        case RACE_TAUREN:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2463) : BotPlaySound(2462); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2396) : BotPlaySound(2397); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(2495) : BotPlaySound(2494); break;
                default: break;
            }
            break;
        case RACE_GNOME:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1743) : BotPlaySound(1798); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1708) : BotPlaySound(1709); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1759) : BotPlaySound(1814); break;
                default: break;
            }
            break;
        case RACE_TROLL:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1853) : BotPlaySound(1963); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1820) : BotPlaySound(1930); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(1869) : BotPlaySound(1993); break;
                default: break;
            }
            break;
        case RACE_BLOODELF:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(9583) : BotPlaySound(9584); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(9549) : BotPlaySound(9550); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(9565) : BotPlaySound(9566); break;
                default: break;
            }
            break;
        case RACE_DRAENEI:
            switch (msg)
            {
                case CANT_AFFORD: m_bot->getGender() == GENDER_MALE ? BotPlaySound(9498) : BotPlaySound(9499); break;
                case INVENTORY_FULL: m_bot->getGender() == GENDER_MALE ? BotPlaySound(9465) : BotPlaySound(9466); break;
                case CANT_USE_TOO_FAR: m_bot->getGender() == GENDER_MALE ? BotPlaySound(9481) : BotPlaySound(9482); break;
                default: break;
            }
            break;
        default:
            break;
    }
}

bool PlayerbotAI::IsMoving()
{
    //return m_bot->isMoving();
    return (m_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE ? false : true);
}

// some possible things to use in AI
// GetRandomContactPoint
// GetPower, GetMaxPower
// HasSpellCooldown
// IsAffectedBySpellmod
// isMoving
// HasUnitState(FLAG) FLAG like: UNIT_STATE_ROOT, UNIT_STATE_CONFUSED, UNIT_STATE_STUNNED
// hasAuraType

void PlayerbotAI::UpdateAI(const uint32 /*p_time*/)
{
    if (m_bot->IsBeingTeleported() || m_bot->GetTrader())
        return;

    if (m_changeFaction && !m_master->GetCharmer())
    {
        //new
        uint32 masterteam = m_master->GetTeam();
        if (m_bot->GetTeam() != masterteam)
            m_bot->SetBotTeam(Team(masterteam));
        //new
        uint32 masterfaction = m_master->getFaction();
        if (m_bot->getFaction() != masterfaction || !m_bot->IsFriendlyTo(m_master))
            m_bot->setFaction(masterfaction);
        for (uint8 i = 0; i != m_bot->GetMaxNpcBots(); ++i)
        {
            Creature *cre = m_bot->GetBotMap()[i]._Guid() != 0 ? sObjectAccessor->GetObjectInWorld(m_bot->GetBotMap()[i]._Guid(), (Creature*)NULL) : NULL;
            if (!cre) continue;
            if (cre->getFaction() != masterfaction || !cre->IsFriendlyTo(m_master))
                cre->setFaction(masterfaction);
        }
    }
    if(m_master->GetBotTankGuid() != m_bot->GetBotTankGuid())
        m_bot->SetBotTank(m_master->GetBotTankGuid());

    time_t currentTime = time(NULL);
    if (currentTime < m_ignoreAIUpdatesUntilTime)
        return;

    // default updates occur every two seconds
    m_ignoreAIUpdatesUntilTime = time(NULL) + 1;
    if (FollowAutoGo == 1)
    {
        if (m_combatOrder & ORDERS_TANK)
            DistOverRide = 1;
        else if (m_combatOrder & ORDERS_ASSIST)
            DistOverRide = 3;
        else
            DistOverRide = 4;
        FollowAutoGo = 2;
        SetMovementOrder(MOVEMENT_FOLLOW, m_master);
    }
    if (!m_bot->isAlive())
    {
        if (m_botState != BOTSTATE_DEAD && m_botState != BOTSTATE_DEADRELEASED)
        {
            // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: UpdateAI - %s died and is not in correct state...", m_bot->GetName() );
            // clear loot list on death
            m_lootTargets.clear();
            m_lootCurrent = 0;
            // clear combat orders
            m_bot->SetSelection(0);
            m_bot->GetMotionMaster()->Clear(true);
            // set state to dead
            SetState(BOTSTATE_DEAD);
            // wait 30sec
            m_ignoreAIUpdatesUntilTime = time(NULL) + 30;
        }
        else if (m_botState == BOTSTATE_DEAD)
        {
            // become ghost
            if (m_bot->GetCorpse())
            {
                // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: UpdateAI - %s already has a corpse...", m_bot->GetName() );
                SetState(BOTSTATE_DEADRELEASED);
                return;
            }
            m_bot->SetBotDeathTimer();
            m_bot->BuildPlayerRepop();
            // relocate ghost
            //Position loc;
            if (Corpse *corpse = m_bot->GetCorpse())
            {
                m_bot->TeleportTo(corpse->GetMapId(), corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ(), m_bot->GetOrientation());
                //m_bot->TeleportTo(*corpse);
            }
            // set state to released
            SetState(BOTSTATE_DEADRELEASED);
        }
        else if (m_botState == BOTSTATE_DEADRELEASED)
        {
            // get bot's corpse
            Corpse *corpse = m_bot->GetCorpse();
            if (!corpse)
                // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: UpdateAI - %s has no corpse!", m_bot->GetName() );
                return;
            // teleport ghost from graveyard to corpse
            // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: UpdateAI - Teleport %s to corpse...", m_bot->GetName() );
            //DoTeleport(*corpse);
            m_ignoreAIUpdatesUntilTime = time(NULL) + 1;
            PlayerbotChatHandler ch(m_master);
            if (!ch.teleport(*m_bot, *corpse))
            {
                ch.sysmessage(".. could not be teleported ..");
                // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: DoTeleport - %s failed to teleport", m_bot->GetName() );
            }
            // check if we are allowed to resurrect now
            if ((corpse->GetGhostTime() + m_bot->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP)) > time(NULL))
            {
                m_ignoreAIUpdatesUntilTime = corpse->GetGhostTime() + m_bot->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP);
                // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: UpdateAI - %s has to wait for %d seconds to revive...", m_bot->GetName(), m_ignoreAIUpdatesUntilTime-time(NULL) );
                return;
            }
            // resurrect now
            // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: UpdateAI - Reviving %s to corpse...", m_bot->GetName() );
            m_ignoreAIUpdatesUntilTime = time(NULL) + 6;
            //PlayerbotChatHandler ch(m_master);
            if (!ch.revive(*m_bot))
            {
                ch.sysmessage(".. could not be revived ..");
                return;
            }
            // set back to normal
            SetState(BOTSTATE_NORMAL);
        }
    }
    else//if bot is alive
    {
        if (!m_findNPC.empty())
            findNearbyCreature();

        // if we are casting a spell then interrupt it
        // make sure any actions that cast a spell set a proper m_ignoreAIUpdatesUntilTime!
        //DEBUG
        Spell* const pSpell = GetCurrentSpell();
        //if (pSpell && !pSpell->IsChannelActive() && !pSpell->IsAutoRepeat())
        //{
        //    TellMaster("UpdateAI(): wrong m_ignoreAIUpdatesUntilTime! Interrupting spell! (%s - %u)", pSpell->GetSpellInfo()->SpellName[0], pSpell->GetSpellInfo()->Id);
        //    InterruptCurrentCastingSpell();
        //}
        //else 
        //DEBUG
        if (m_botState == BOTSTATE_TAME)
        {
            Unit* pTarget = sObjectAccessor->GetUnit(*m_bot, m_targetGuidCommand);
            if (!pTarget)
                return;

            m_bot->SetSelection(m_targetGuidCommand);

            if (!IsInRange(pTarget, TAME_BEAST_1))
                m_bot->ClearUnitState(UNIT_STATE_CHASE);

            if (!m_bot->HasUnitState(UNIT_STATE_CHASE))
            {
                m_bot->GetMotionMaster()->MoveChase(pTarget);
                return;
            }

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(TAME_BEAST_1);
            if (!spellInfo)
                return;

            Spell *spell = new Spell(m_bot, spellInfo, TRIGGERED_NONE);
            if (!spell)
                return;

            if (m_bot->GetPetGUID() || spell->CheckCast(true) != SPELL_CAST_OK || !pTarget ||
                pTarget->isDead() || !m_bot->IsInMap(pTarget) ||
                !(pTarget->ToCreature() && (((Creature *)pTarget)->GetCreatureTemplate()->type_flags & CREATURE_TYPEFLAGS_TAMEABLE)))
            {
                MovementReset();
                m_bot->SetSelection(0);
                SetState(BOTSTATE_NORMAL);
                SetIgnoreUpdateTime(0);
            }
            else if (!m_bot->HasAura(TAME_BEAST_1, 0, 0, 1))
            {
                m_bot->SetFacingTo(m_bot->GetAngle(pTarget));
                SpellCastTargets targets;
                targets.SetUnitTarget(pTarget);
                spell->prepare(&targets);
                SetIgnoreUpdateTime(10);
            }
            return;
        }

        // direct cast command from master
        else if (m_spellIdCommand != 0)
        {
            Unit* pTarget = sObjectAccessor->GetUnit(*m_bot, m_targetGuidCommand);
            if (pTarget)
                CastSpell(m_spellIdCommand, *pTarget);
            m_spellIdCommand = 0;
            m_targetGuidCommand = 0;
        }

        else if (m_botState == BOTSTATE_ENCHANT)
        {
            SetState(BOTSTATE_NORMAL);
            InspectUpdate();
        }

        else if (m_botState == BOTSTATE_CRAFT)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(m_CurrentlyCastingSpellId);
            if (!spellInfo)
                return;

            Spell *spell = new Spell(m_bot, spellInfo, TRIGGERED_NONE);
            if (!spell)
                return;

            if (GetSpellCharges(m_CurrentlyCastingSpellId) == 0 || spell->CheckCast(true) != SPELL_CAST_OK)
            {
                SetState(BOTSTATE_NORMAL);
                SetIgnoreUpdateTime(0);
            }
            else
            {
                SpellCastTargets targets;
                spell->prepare(&targets);
                SetIgnoreUpdateTime(3);
            }
        }

        else if (m_master->IsMounted() && !m_bot->IsMounted() && !m_bot->HasUnitState(UNIT_STATE_CASTING))
        {
            //Player Part
            Unit::AuraEffectList const& AuraList = m_master->GetAuraEffectsByType(SPELL_AURA_MOUNTED);
            if (!AuraList.empty())
            {
                SpellInfo const *pSpellInfo = AuraList.front()->GetSpellInfo();

                //Bot Part
                uint32 spellMount = 0;
                //cheap check if we know this spell
                for (PlayerSpellMap::iterator itr = m_bot->GetSpellMap().begin(); itr != m_bot->GetSpellMap().end(); ++itr)
                {
                    if (itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled)
                        continue;
                    uint32 spellId = itr->first;
                    if (pSpellInfo->Id == spellId)
                    {
                        spellMount = spellId;
                        break;
                    }
                }
                if (!spellMount)
                {
                    //analyze and find proper mount spell
                    for (PlayerSpellMap::iterator itr = m_bot->GetSpellMap().begin(); itr != m_bot->GetSpellMap().end(); ++itr)
                    {
                        if (itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled)
                            continue;
                        uint32 spellId = itr->first;
                        SpellInfo const *bSpellInfo = sSpellMgr->GetSpellInfo(spellId);
                        if (!bSpellInfo || bSpellInfo->IsPassive())
                            continue;

                        for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
                        {
                            if (bSpellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOUNTED)
                            {
                                //arrange values
                                int8 j = i-1, k = i+1;
                                if (j < 0)// i == 0
                                    j = k+1;//2
                                else if (k >= MAX_SPELL_EFFECTS)// i == 2
                                    k = j-1;//0

                                if (bSpellInfo->Effects[j].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)
                                {
                                    for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
                                    {
                                        if (pSpellInfo->Effects[i].BasePoints == bSpellInfo->Effects[j].BasePoints)
                                        {
                                            spellMount = spellId;
                                            break;
                                        }
                                    }
                                    if (spellMount)
                                        break;
                                }
                                else if (bSpellInfo->Effects[k].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)
                                {
                                    for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
                                    {
                                        if (pSpellInfo->Effects[i].BasePoints == bSpellInfo->Effects[k].BasePoints)
                                        {
                                            spellMount = spellId;
                                            break;
                                        }
                                    }
                                    if (spellMount)
                                        break;
                                }
                                else if (bSpellInfo->Effects[j].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED)
                                {
                                    for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
                                    {
                                        if (pSpellInfo->Effects[i].BasePoints == bSpellInfo->Effects[j].BasePoints)
                                        {
                                            spellMount = spellId;
                                            break;
                                        }
                                    }
                                    if (spellMount)
                                        break;
                                }
                                else if (bSpellInfo->Effects[k].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED)
                                {
                                    for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
                                    {
                                        if (pSpellInfo->Effects[i].BasePoints == bSpellInfo->Effects[k].BasePoints)
                                        {
                                            spellMount = spellId;
                                            break;
                                        }
                                    }
                                    if (spellMount)
                                        break;
                                }
                            }
                        }
                        if (spellMount)
                            break;
                    }
                }
                if (spellMount)
                    CastSpell(spellMount);
                else
                    SendWhisper("Cannot find approriate mount!", *m_master);
            }
        }

        //if master is unmounted, unmount the bot
        else if (!m_master->IsMounted() && m_bot->IsMounted())
        {
            WorldPacket emptyPacket;
            m_bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);  //updated code
        }

        // handle combat (either self/master/group in combat, or combat state and valid target)
        else if (IsInCombat() || (m_botState == BOTSTATE_COMBAT && m_targetCombat) ||  m_ScenarioType == SCENARIO_DUEL)
        {
            //check if the bot is Mounted
            if (!m_bot->IsMounted())
            {
                if (!pSpell || !pSpell->IsChannelActive())
                    DoNextCombatManeuver();
                else
                    SetIgnoreUpdateTime(1);  // It's better to update AI more frequently during combat
            }
        }
        // bot was in combat recently - loot now
        else if (m_botState == BOTSTATE_COMBAT)
        {
            SetState(BOTSTATE_LOOTING);
            m_attackerInfo.clear();
            if (HasCollectFlag(COLLECT_FLAG_COMBAT))
                m_lootTargets.unique();
            else
                m_lootTargets.clear();
            SetIgnoreUpdateTime(0);
        }
        else if (m_botState == BOTSTATE_LOOTING)
        {
            DoLoot();
        }
        else if (m_botState == BOTSTATE_FLYING)
        {
            /* std::ostringstream out;
               out << "Taxi: " << m_bot->GetName() << m_ignoreAIUpdatesUntilTime;
               TellMaster(out.str().c_str()); */
            DoFlight();
            SetState(BOTSTATE_NORMAL);
            SetIgnoreUpdateTime(0);
        }
        // if commanded to follow master and not already following master then follow master
        else if (!m_bot->isInCombat() && !IsMoving())
        {
            //TellMaster("UpdateAI():I am not in combat and not moving - reset movement");
            MovementReset();
        }

        // do class specific non combat actions
        else if (m_classAI && !m_bot->IsMounted())
        {
            (m_classAI)->DoNonCombatActions();

            // have we been told to collect GOs
            if (HasCollectFlag(COLLECT_FLAG_NEAROBJECT))
            {
                findNearbyGO();
                // start looting if have targets
                if (!m_lootTargets.empty())
                    SetState(BOTSTATE_LOOTING);
            }
        }
        //debug
        if (!m_bot->HasUnitState(UNIT_STATE_CASTING))
            m_bot->RemoveAura(SPELL_ROOT);
        //end debug
        // debug
        if (m_botState == BOTSTATE_NORMAL)
        {
            if (m_master->getStandState() == UNIT_STAND_STATE_SIT)
                m_bot->SetStandState(UNIT_STAND_STATE_SIT);
            else if (m_bot->getStandState() == UNIT_STAND_STATE_SIT && m_TimeDoneDrinking < time(0) && m_TimeDoneEating < time(0)) //Do no interrupt if bot is eating/drinking
                m_bot->SetStandState(UNIT_STAND_STATE_STAND);
            m_bot->SendUpdateToPlayer(m_master);
        }
        // end debug
    }//end is alive
}

Spell* PlayerbotAI::GetCurrentSpell() const
{
    if (m_CurrentlyCastingSpellId == 0)
        return NULL;

    return m_bot->FindCurrentSpellBySpellId(m_CurrentlyCastingSpellId);
}

void PlayerbotAI::TellMaster(std::string const& text) const
{
    SendWhisper(text, *m_master);
}

void PlayerbotAI::TellMaster(const char *fmt, ...) const
{
    char temp_buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(temp_buf, 2048, fmt, ap);
    va_end(ap);
    std::string str = temp_buf;
    TellMaster(str);
}

void PlayerbotAI::SendWhisper(std::string const& text, Player& player) const
{
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    m_bot->BuildPlayerChat(&data, CHAT_MSG_WHISPER, text, LANG_UNIVERSAL);
    player.GetSession()->SendPacket(&data);
}

bool PlayerbotAI::canObeyCommandFrom(const Player& player) const
{
    return player.GetSession()->GetAccountId() == m_master->GetSession()->GetAccountId();
}

bool PlayerbotAI::IsInRange(Unit* Target, uint32 spellId)
{
    const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!pSpellInfo)
        return false;

    bool positive = (Target->ToPlayer() && Target->ToPlayer()->GetGroup() == m_bot->GetGroup()) || Target->IsFriendlyTo(m_bot);
    return m_bot->IsInRange(Target, pSpellInfo->GetMinRange(positive), pSpellInfo->GetMaxRange(positive));

    //SpellRangeEntry const* TempRange = GetSpellRangeStore()->LookupEntry(pSpellInfo->rangeIndex);

    ////Spell has invalid range store so we can't use it
    //if (!TempRange)
    //    return false;

    //if ((TempRange->minRange == 0.0f) && (TempRange->maxRange == 0.0f))
    //    return true;

    ////Unit is out of range for this spell
    //if (!m_bot->IsInRange(Target, TempRange->minRange, TempRange->maxRange))
    //    return false;

    //return true;
}

bool PlayerbotAI::CastSpell(const char* args)
{
    uint32 spellId = getSpellId(args);
    return (spellId) ? CastSpell(spellId) : false;
}

bool PlayerbotAI::CastSpell(uint32 spellId, Unit& target)
{
    uint64 oldSel = m_bot->GetSelection();
    m_bot->SetSelection(target.GetGUID());
    bool rv = CastSpell(spellId);
    m_bot->SetSelection(oldSel);
    return rv;
}

bool PlayerbotAI::CastSpell(uint32 spellId)
{
    // some AIs don't check if the bot doesn't have spell before using it
    // so just return false when this happens
    if (spellId == 0)
        return false;
    //debug
    //prevent interrupting
    if (m_bot->IsNonMeleeSpellCasted(false))
        return false;
    //debug

    // check spell cooldown
    if (m_bot->HasSpellCooldown(spellId))
        return false;

    // see Creature.cpp 1738 for reference
    // don't allow bot to cast damage spells on friends
    SpellInfo const *pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!pSpellInfo)
    {
        TellMaster("Missing spell entry in CastSpell for spellid %u.", spellId);
        return false;
    }

    // set target
    Unit* pTarget = m_bot->GetSelection() ? sObjectAccessor->GetUnit(*m_bot, m_bot->GetSelection()) : m_bot;

    if (!pTarget)
        pTarget = m_bot;

    if (pSpellInfo->IsPositive())
    {
        if (pTarget && !m_bot->IsFriendlyTo(pTarget))
            pTarget = m_bot;
    }
    else
    {
        if (pTarget && m_bot->IsFriendlyTo(pTarget))
            return false;

        m_bot->SetFacingTo(m_bot->GetAngle(pTarget));
    }

    float CastTime = 0.0f;

    // stop movement to prevent cancel spell casting
    SpellCastTimesEntry const * castTimeEntry = pSpellInfo->CastTimeEntry;
    if (castTimeEntry && castTimeEntry->CastTime)
    {
        CastTime = (castTimeEntry->CastTime / 1000);
        //TellMaster("CastSpell - movement reset for casting %s (%u, cast time: %f)", pSpellInfo->SpellName[0], spellId, CastTime);
        m_bot->StopMoving();
        //debug See also UpdateAI
        m_bot->AddAura(SPELL_ROOT, m_bot);
        //TellMaster("CastSpell - Applying root (%s - %u, cast time: %f)", pSpellInfo->SpellName[0], spellId, CastTime);
        //end debug
    }

    uint32 target_type = TARGET_FLAG_UNIT;

    if (pSpellInfo->Effects[0].Effect == SPELL_EFFECT_OPEN_LOCK)
        target_type = TARGET_FLAG_GAMEOBJECT;

    m_CurrentlyCastingSpellId = spellId;

    if (pSpellInfo->Effects[0].Effect == SPELL_EFFECT_OPEN_LOCK ||
        pSpellInfo->Effects[0].Effect == SPELL_EFFECT_SKINNING)
    {
        if (m_lootCurrent)
        {
            WorldPacket* const packet = new WorldPacket(CMSG_CAST_SPELL, 1 + 4 + 1 + 4 + 8);
            *packet << uint8(0);                            // spells cast count;
            *packet << spellId;
            *packet << uint8(0);                            // unk_flags
            *packet << uint32(target_type);
            *packet << m_lootCurrent;//.WriteAsPacked();
            m_bot->GetSession()->QueuePacket(packet);       // queue the packet to get around race condition

            if (target_type == TARGET_FLAG_GAMEOBJECT)
            {
                WorldPacket* const packetgouse = new WorldPacket(CMSG_GAMEOBJ_REPORT_USE, 8);
                *packetgouse << m_lootCurrent;
                m_bot->GetSession()->QueuePacket(packetgouse);  // queue the packet to get around race condition

                GameObject *obj = m_bot->GetMap()->GetGameObject(m_lootCurrent);
                if (!obj)
                    return false;

                // add other go types here, i.e.:
                // GAMEOBJECT_TYPE_CHEST - loot quest items of chest
                if (obj->GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
                {
                    TurnInQuests(obj);

                    // auto accept every available quest this NPC has
                    m_bot->PrepareQuestMenu(m_lootCurrent);
                    QuestMenu& questMenu = m_bot->PlayerTalkClass->GetQuestMenu();
                    for (uint32 iI = 0; iI < questMenu.GetMenuItemCount(); ++iI)
                    {
                        QuestMenuItem const& qItem = questMenu.GetItem(iI);
                        uint32 questID = qItem.QuestId;
                        if (!AddQuest(questID, obj))
                            TellMaster("Couldn't take quest");
                    }
                    m_lootCurrent = 0;
                    m_bot->GetMotionMaster()->Clear();
                    m_bot->GetMotionMaster()->MoveIdle();
                }
            }
        }
        else
            return false;
    }
    else
    {
        // Check spell range
        if (!IsInRange(pTarget, spellId))
        {
            //TellMaster("CastSpell - i'm not in range! (spell: %s, target: %s)", pSpellInfo->SpellName[0], pTarget->GetName());
            return false;
        }

        // Check line of sight
        if (!m_bot->IsWithinLOSInMap(pTarget))
        {
            //TellMaster("CastSpell - i'm not wothin LOS! (spell: %s, target: %s)", pSpellInfo->SpellName[0], pTarget->GetName());
            return false;
        }

        ////temp
        //Spell *spell = new Spell(m_bot, pSpellInfo, TRIGGERED_NONE);
        //if (!spell->CheckCast(true))
        //    TellMaster("CastSpell:CheckCast() cannot cast spell %u (%s), target: %s)", pSpellInfo->Id, pSpellInfo->SpellName[0], pTarget->GetName());

        m_bot->CastSpell(pTarget, pSpellInfo, false/*true*/);       // actually cast spell
        //TellMaster("CastSpell - processing spell %u (%s), target: %s)", pSpellInfo->Id, pSpellInfo->SpellName[0], pTarget->GetName());
    }

    //DEBUG
    //if (pSpellInfo->IsChanneled())
    //    m_ignoreAIUpdatesUntilTime = time(NULL) + CastTime + 0.1f;
    //else
    //    m_ignoreAIUpdatesUntilTime = time(NULL) + 2;
    //DEBUG

    //m_CurrentlyCastingSpellId = 0;
    m_CurrentlyCastingSpellId = pSpellInfo->Id;

    // if this caused the caster to move (blink) update the position
    // I think this is normally done on the client
    // this should be done on spell success
    /*
       if (name == "Blink") {
       float x,y,z;
       m_bot->GetPosition(x,y,z);
       m_bot->GetNearPoint(m_bot, x, y, z, 1, 5, 0);
       m_bot->Relocate(x,y,z);
       m_bot->SendHeartBeat();

       }
     */

    return true;
}

bool PlayerbotAI::CastPetSpell(uint32 spellId, Unit* target)
{
    if (spellId == 0)
        return false;

    Pet* pet = m_bot->GetPet();
    if (!pet)
        return false;

    if (pet->HasSpellCooldown(spellId))
        return false;

    const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!pSpellInfo)
    {
        TellMaster("Missing spell entry in CastPetSpell()");
        return false;
    }

    // set target
    Unit* pTarget;
    if (!target)
    {
        uint64 targetGUID = m_bot->GetSelection();
        pTarget = sObjectAccessor->GetUnit(*m_bot, targetGUID);
    }
    else
        pTarget = target;

    if (sSpellMgr->GetSpellInfo(spellId)->IsPositive())
    {
        if (pTarget && !m_bot->IsFriendlyTo(pTarget))
            pTarget = m_bot;
    }
    else
    {
        if (pTarget && m_bot->IsFriendlyTo(pTarget))
            return false;

        if (!pet->isInFrontInMap(pTarget, 10)) // distance probably should be calculated
            pet->SetFacingTo(pet->GetAngle(pTarget));
    }

    pet->CastSpell(pTarget, pSpellInfo, false);

    Spell* const pSpell = pet->FindCurrentSpellBySpellId(spellId);
    if (!pSpell)
        return false;

    return true;
}

// Perform sanity checks and cast spell
bool PlayerbotAI::Buff(uint32 spellId, Unit* target, void (*beforeCast)(Player *))
{
    if (spellId == 0)
        return false;

    SpellInfo const * spellProto = sSpellMgr->GetSpellInfo(spellId);

    if (!spellProto)
        return false;

    if (!target)
        return false;

    // Select appropriate spell rank for target's level
    spellProto = spellProto->GetAuraRankForLevel(target->getLevel());
    if (!spellProto)
        return false;

    // Check if spell will boost one of already existent auras
    bool willBenefitFromSpell = false;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellProto->Effects[i].ApplyAuraName == SPELL_AURA_NONE)
            break;

        bool sameOrBetterAuraFound = false;
        int32 bonus = m_bot->CalculateSpellDamage(target, spellProto, i);
        Unit::AuraEffectList const& auras = target->GetAuraEffectsByType(AuraType(spellProto->Effects[i].ApplyAuraName));
        for (Unit::AuraEffectList::const_iterator it = auras.begin(); it != auras.end(); ++it)
        {
            if ((*it)->GetMiscValue() == spellProto->Effects[i].MiscValue && (*it)->GetAmount() >= bonus)
            {
                sameOrBetterAuraFound = true;
                break;
            }
        }
        willBenefitFromSpell = willBenefitFromSpell || !sameOrBetterAuraFound;
    }

    if (!willBenefitFromSpell)
        return false;

    // Druids may need to shapeshift before casting
    if (beforeCast)
        (*beforeCast)(m_bot);

    return CastSpell(spellProto->Id, *target);
}

// Can be used for personal buffs like Mage Armor and Inner Fire
bool PlayerbotAI::SelfBuff(uint32 spellId)
{
    if (spellId == 0)
        return false;

    if (m_bot->HasAura(spellId))
        return false;

    return CastSpell(spellId, *m_bot);
}

// Checks if spell is single per target per caster and will make any effect on target
bool PlayerbotAI::CanReceiveSpecificSpell(uint8 spec, Unit* target) const
{
    if (IsSingleFromSpellSpecificPerTargetPerCaster(SpellSpecific(spec), SpellSpecific(spec)))
    {
        Unit::AuraMap const &holders = target->GetOwnedAuras();
        Unit::AuraMap::const_iterator it;
        for (it = holders.begin(); it != holders.end(); ++it)
            if ((*it).second->GetCasterGUID() == m_bot->GetGUID() && (*it).second->GetSpellInfo()->GetSpellSpecific() == SpellSpecificType(spec))
                return false;
    }
    return true;
}

bool PlayerbotAI::IsSingleFromSpellSpecificPerTargetPerCaster(SpellSpecific spellSpec1, SpellSpecific spellSpec2) const
{
    switch (spellSpec1)
    {
        case SPELL_BLESSING:
        case SPELL_AURA:
        case SPELL_STING:
        case SPELL_CURSE:
        case SPELL_ASPECT:
        case SPELL_POSITIVE_SHOUT:
        case SPELL_JUDGEMENT:
        case SPELL_HAND:
        case SPELL_UA_IMMOLATE:
            return spellSpec1==spellSpec2;
        default:
            return false;
    }
}

uint8 PlayerbotAI::_findItemSlot(Item* target)
{
    // list out items equipped & in main backpack
    //INVENTORY_SLOT_ITEM_START = 23
    //INVENTORY_SLOT_ITEM_END = 39

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: FindItem - [%s's]backpack slot = %u",m_bot->GetName(),slot); // 23 to 38 = 16
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);  // 255, 23 to 38
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();
            if (!pItemProto)
                continue;

            if (pItemProto->ItemId == target->GetTemplate()->ItemId)   // have required item
                return slot;
        }
    }
    // list out items in other removable backpacks
    //INVENTORY_SLOT_BAG_START = 19
    //INVENTORY_SLOT_BAG_END = 23

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)  // 20 to 23 = 4
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);   // 255, 20 to 23
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: FindItem - [%s's]bag[%u] slot = %u", m_bot->GetName(), bag, slot);  // 1 to bagsize = ?
                Item* const pItem = m_bot->GetItemByPos(bag, slot); // 20 to 23, 1 to bagsize
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();
                    if (!pItemProto)
                        continue;

                    if (pItemProto->ItemId == target->GetTemplate()->ItemId)        // have required item
                        return slot;
                }
            }
    }
    return 0;
}

Item* PlayerbotAI::FindItem(uint32 ItemId)
{
    // list out items equipped & in main backpack
    //INVENTORY_SLOT_ITEM_START = 23
    //INVENTORY_SLOT_ITEM_END = 39

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: FindItem - [%s's]backpack slot = %u",m_bot->GetName(),slot); // 23 to 38 = 16
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);  // 255, 23 to 38
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();
            if (!pItemProto)
                continue;

            if (pItemProto->ItemId == ItemId)   // have required item
                return pItem;
        }
    }
    // list out items in other removable backpacks
    //INVENTORY_SLOT_BAG_START = 19
    //INVENTORY_SLOT_BAG_END = 23

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)  // 20 to 23 = 4
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);   // 255, 20 to 23
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: FindItem - [%s's]bag[%u] slot = %u", m_bot->GetName(), bag, slot);  // 1 to bagsize = ?
                Item* const pItem = m_bot->GetItemByPos(bag, slot); // 20 to 23, 1 to bagsize
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();
                    if (!pItemProto)
                        continue;

                    if (pItemProto->ItemId == ItemId)        // have required item
                        return pItem;
                }
            }
    }
    return NULL;
}

Item* PlayerbotAI::FindItemInBank(uint32 ItemId)
{
    // list out items in bank item slots

    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; slot++)
    {
        // sLog->outDebug("[%s's]backpack slot = %u",m_bot->GetName(),slot);
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
        {
            const ItemTemplate* const pItemProto = pItem->GetTemplate();
            if (!pItemProto)
                continue;

            if (pItemProto->ItemId == ItemId)   // have required item
                return pItem;
        }
    }
    // list out items in bank bag slots

    for (uint8 bag = BANK_SLOT_BAG_START; bag < BANK_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                // sLog->outDebug("[%s's]bag[%u] slot = %u", m_bot->GetName(), bag, slot);
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem)
                {
                    const ItemTemplate* const pItemProto = pItem->GetTemplate();
                    if (!pItemProto)
                        continue;

                    if (pItemProto->ItemId == ItemId)        // have required item
                        return pItem;
                }
            }
    }
    return NULL;
}

Item* PlayerbotAI::FindKeyForLockValue(uint32 reqSkillValue)
{
    if (reqSkillValue <= 25 && m_bot->HasItemCount(SILVER_SKELETON_KEY, 1))
        return m_bot->GetItemByEntry(SILVER_SKELETON_KEY);
    if (reqSkillValue <= 125 && m_bot->HasItemCount(GOLDEN_SKELETON_KEY, 1))
        return m_bot->GetItemByEntry(GOLDEN_SKELETON_KEY);
    if (reqSkillValue <= 200 && m_bot->HasItemCount(TRUESILVER_SKELETON_KEY, 1))
        return m_bot->GetItemByEntry(TRUESILVER_SKELETON_KEY);
    if (reqSkillValue <= 300 && m_bot->HasItemCount(ARCANITE_SKELETON_KEY, 1))
        return m_bot->GetItemByEntry(ARCANITE_SKELETON_KEY);
    if (reqSkillValue <= 375 && m_bot->HasItemCount(TITANIUM_SKELETON_KEY, 1))
        return m_bot->GetItemByEntry(TITANIUM_SKELETON_KEY);
    if (reqSkillValue <= 400 && m_bot->HasItemCount(COBALT_SKELETON_KEY, 1))
        return m_bot->GetItemByEntry(COBALT_SKELETON_KEY);

    return NULL;
}

Item* PlayerbotAI::FindBombForLockValue(uint32 reqSkillValue)
{
    if (reqSkillValue <= 150 && m_bot->HasItemCount(SMALL_SEAFORIUM_CHARGE, 1))
        return m_bot->GetItemByEntry(SMALL_SEAFORIUM_CHARGE);
    if (reqSkillValue <= 250 && m_bot->HasItemCount(LARGE_SEAFORIUM_CHARGE, 1))
        return m_bot->GetItemByEntry(LARGE_SEAFORIUM_CHARGE);
    if (reqSkillValue <= 300 && m_bot->HasItemCount(POWERFUL_SEAFORIUM_CHARGE, 1))
        return m_bot->GetItemByEntry(POWERFUL_SEAFORIUM_CHARGE);
    if (reqSkillValue <= 350 && m_bot->HasItemCount(ELEMENTAL_SEAFORIUM_CHARGE, 1))
        return m_bot->GetItemByEntry(ELEMENTAL_SEAFORIUM_CHARGE);

    return NULL;
}

bool PlayerbotAI::HasTool(uint32 TC)
{
    std::ostringstream out;

    switch (TC)
    {
        case TC_MINING_PICK:                //  = 165

            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a MINING PICK!";
            break;

        case TC_ARCLIGHT_SPANNER:          //  = 14

            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have an ARCLIGHT SPANNER!";
            break;

        case TC_BLACKSMITH_HAMMER:         //  = 162

            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a BLACKSMITH's HAMMER!";
            break;

        case TC_SKINNING_KNIFE:            //  = 166

            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a SKINNING KNIFE!";
            break;

        case TC_COPPER_ROD:                //  = 6,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED COPPER ROD!";
            break;

        case TC_SILVER_ROD:                //  = 7,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED SILVER ROD!";
            break;

        case TC_GOLDEN_ROD:                //  = 8,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED GOLDEN ROD!";
            break;

        case TC_TRUESILVER_ROD:            //  = 9,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED TRUESILVER ROD!";
            break;

        case TC_ARCANITE_ROD:              //  = 10,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED ARCANITE ROD!";
            break;

        case TC_FEL_IRON_ROD:              //  = 41,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED FEL IRON ROD!";
            break;

        case TC_ADAMANTITE_ROD:            //  = 62,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED ADAMANTITE ROD!";
            break;

        case TC_ETERNIUM_ROD:              //  = 63,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED ETERNIUM ROD!";
            break;

        case TC_RUNED_AZURITE_ROD:         //  = 101,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED AZURITE ROD!";
            break;

        case TC_VIRTUOSO_INKING_SET:       //  = 121,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a VIRTUOSO INKING SET!";
            break;

        case TC_RUNED_COBALT_ROD:          //  = 189,
            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED COBALT ROD!";
            break;

        case TC_RUNED_TITANIUM_ROD:        //  = 190,

            if (m_bot->HasItemTotemCategory(TC))
                return true;
            else
                out << "|cffff0000I do not have a RUNED TITANIUM ROD!";
            break;
        default:
            out << "|cffffffffI do not know what tool that needs! TC (" << TC << ")";
    }
    TellMaster(out.str().c_str());
    return false;
}

bool PlayerbotAI::HasSpellReagents(uint32 spellId)
{
    const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!pSpellInfo)
        return false;

    if (m_bot->CanNoReagentCast(pSpellInfo))
        return true;

    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (pSpellInfo->Reagent[i] <= 0)
            continue;

        uint32 itemid = pSpellInfo->Reagent[i];
        uint32 count = pSpellInfo->ReagentCount[i];

        if (!m_bot->HasItemCount(itemid, count))
            return false;
    }

    return true;
}

uint32 PlayerbotAI::GetSpellCharges(uint32 spellId)
{
    const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!pSpellInfo)
        return 0;

    if (m_bot->CanNoReagentCast(pSpellInfo))
        return 0;

    uint32 charges = 0;
    std::list<uint32> chargeList;
    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (pSpellInfo->Reagent[i] <= 0)
            continue;

        uint32 totalcount = 0;
        uint32 itemid = pSpellInfo->Reagent[i];
        uint32 count = pSpellInfo->ReagentCount[i];
        ItemCountInInv(itemid, totalcount);
        chargeList.push_back((totalcount / count));
    }

    for (uint32 i = 0; i < 3; ++i)
    {
        if (pSpellInfo->TotemCategory[i] == 0)
            continue;

        if (!m_bot->HasItemTotemCategory(pSpellInfo->TotemCategory[i]))
        {
            m_noToolList.push_back(pSpellInfo->TotemCategory[i]);
            return 0;
        }
    }

    if (!chargeList.empty())
    {
        charges = chargeList.front();
        chargeList.pop_front();
        for (std::list<uint32>::iterator it = chargeList.begin(); it != chargeList.end(); ++it)
            if (*it < charges)
                charges = *it;
    }
    return charges;
}

void PlayerbotAI::ItemCountInInv(uint32 itemid, uint32 &count)
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item *pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == itemid && !pItem->IsInTrade())
            count += pItem->GetCount();
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag * pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                Item* pItem = m_bot->GetItemByPos(i, j);
                if (pItem && pItem->GetEntry() == itemid && !pItem->IsInTrade())
                    count += pItem->GetCount();
            }
    }
}

// extracts all item ids in format below
// I decided to roll my own extractor rather then use the one in ChatHandler
// because this one works on a const string, and it handles multiple links
// |color|linkType:key:something1:...:somethingN|h[name]|h|r
void PlayerbotAI::extractItemIds(const std::string& text, std::list<uint32>& itemIds) const
{
    uint8 pos = 0;
    while (true)
    {
        int i = text.find("Hitem:", pos);
        if (i == -1)
            break;
        pos = i + 6;
        int endPos = text.find(':', pos);
        if (endPos == -1)
            break;
        std::string idC = text.substr(pos, endPos - pos);
        uint32 id = atol(idC.c_str());
        pos = endPos;
        if (id)
            itemIds.push_back(id);
    }
}

void PlayerbotAI::extractMailIds(const std::string& text, std::list<uint32>& mailIds) const
{
    uint8 pos = 0;
    while (true)
    {
        int i = text.find("Hmail:", pos);
        if (i == -1)
            break;
        pos = i + 6;
        int endPos = text.find('|', pos);
        if (endPos == -1)
            break;
        std::string idC = text.substr(pos, endPos - pos);
        uint32 id = atol(idC.c_str());
        pos = endPos;
        if (id)
            mailIds.push_back(id);
    }
}

/**
 * Checks whether the TalentSpec database contains any obvious errors
 *
 * return 0  -> all ok
 * return x  -> return the talentspec_id of the first talentspec that errors out
 */

// TODO: the way this is built is just begging for a memory leak (by adding a return case and forgetting to delete result)
uint32 PlayerbotAI::TalentSpecDBContainsError()
{
    //QueryResult result = CharacterDatabase.PQuery("SELECT * FROM playerbot_talentspec ORDER BY class ASC");

    //if (!result)
    //{
    //    // Do you really need a progress bar? No, but all the other kids jumped off the bridge too...
    //    //BarGoLink bar(1);

    //    //bar.step();

    //    //sLog->outString();
    //    //sLog->outString(">> Loaded `playerbot_talentspec`, table is empty.");

    //    return 0;   // Because, well, no specs means none contain errors...
    //}

    ////BarGoLink bar(result->GetRowCount());

    //do
    //{
    //    //bar.step();

    //    /* 0            talentspec_id
    //       1            name
    //       2            class
    //       3            purpose
    //       4 to 74        talent_10 to 71
    //       75 to 80        major_glyph_15, 30, 80, minor_glyph_15, 50, 70
    //     */
    //    Field* fields = result->Fetch();

    //    uint32 ts_id = fields[0].GetUInt32();
    //    if (!ts_id)    // Nice bit of paranoia: ts_id is a non-zero NOT NULL AUTO_INCREMENT value
    //        continue;  // Of course, if the impossible ever does happen, we can't very well identify a TalentSpec without an ID...

    //    std::string ts_name = fields[1].GetCString();
    //    /*    Commented out? Because it's only required if you assume only players (not the server) pick talentspecs
    //       if (0 == ts_name.size())
    //       {
    //       TellMaster("TalentSpec ID: %u does not have a name.", ts_id);

    //       //delete result;
    //       return ts_id;
    //       }
    //     */

    //    long ts_class = fields[2].GetInt32();
    //    if (ts_class != CLASS_DEATH_KNIGHT && ts_class != CLASS_DRUID && ts_class != CLASS_HUNTER && ts_class != CLASS_MAGE && ts_class != CLASS_PALADIN && ts_class != CLASS_PRIEST && ts_class != CLASS_ROGUE && ts_class != CLASS_SHAMAN && ts_class != CLASS_WARLOCK && ts_class != CLASS_WARRIOR &&
    //        ts_class != CLASS_PET_CUNNING && ts_class != CLASS_PET_FEROCITY && ts_class != CLASS_PET_TENACITY)
    //    {
    //        TellMaster("TalentSpec: %u. \"%s\" contains an invalid class: %i.", ts_id, ts_name.c_str(), ts_class);

    //        ////delete result;
    //        return ts_id;    // invalid class
    //    }

    //    // Can't really be error checked, can it?
    //    // uint32 ts_purpose = fields[3].GetUInt32();

    //    // check all talents
    //    for (uint8 i = 0; i < 71; i++)
    //    {
    //        uint8 fieldLoc = i + 4;
    //        if (fields[fieldLoc].GetUInt16() == 0)
    //        {
    //            for (uint8 j = (i + 1); j < 71; j++)
    //            {
    //                fieldLoc = j + 4;
    //                if (fields[fieldLoc].GetUInt16() != 0)
    //                {
    //                    TellMaster("TalentSpec: %u. \"%s\" contains an empty talent for level: %u while a talent for level: %u exists.", ts_id, ts_name.c_str(), (i + 10), (j + 10));

    //                    ////delete result;
    //                    return ts_id;
    //                }
    //            }
    //            break;
    //        }
    //        else if (!ValidateTalent(fields[fieldLoc].GetUInt16(), ts_class))
    //        {
    //            TellMaster("TalentSpec: %u. \"%s\" (class: %i) contains an invalid talent for level %u: %u", ts_id, ts_name.c_str(), ts_class, (i + 10), fields[fieldLoc].GetUInt16());

    //            ////delete result;
    //            return ts_id;    // invalid talent
    //        }
    //    }

    //    for (uint8 i = 75; i < 78; i++)  // as in, the 3 major glyphs
    //    {
    //        if (fields[i].GetUInt16() != 0 && !ValidateMajorGlyph(fields[i].GetUInt16(), ts_class))
    //        {
    //            TellMaster("TalentSpec: %u. \"%s\" contains an invalid Major glyph %u: %u", ts_id, ts_name.c_str(), (i - 74), fields[i].GetUInt16());
    //            if (!ValidateGlyph(fields[i].GetUInt16(), ts_class))
    //                TellMaster("In fact, according to our records, it's no glyph at all");

    //            ////delete result;
    //            return ts_id;
    //        }
    //    }
    //    for (uint8 i = 78; i < 81; i++)  // as in, the 3 minor glyphs
    //    {
    //        if (fields[i].GetUInt16() != 0 && !ValidateMinorGlyph(fields[i].GetUInt16(), ts_class))
    //        {
    //            TellMaster("TalentSpec: %u. \"%s\" contains an invalid Minor glyph %u: %u", ts_id, ts_name.c_str(), (i - 77), fields[i].GetUInt16());
    //            if (!ValidateGlyph(fields[i].GetUInt16(), ts_class))
    //                TellMaster("In fact, according to our records, it's no glyph at all");

    //            ////delete result;
    //            return ts_id;
    //        }
    //    }
    //} while (result->NextRow());

    ////delete result;
    return 0;
}

uint32 PlayerbotAI::GetTalentSpecsAmount()
{
    //QueryResult result = CharacterDatabase.PQuery("SELECT COUNT(*) FROM playerbot_talentspec");

    //if (!result)
    //{
    //    //sLog->outString();
    //    //sLog->outString(">> Loaded `playerbot_talentspec`, table is empty.");

        return 0;
    //}

    //Field* fields = result->Fetch();

    //uint32 count = fields[0].GetUInt32();

    ////delete result;
    //return count;
}

uint32 PlayerbotAI::GetTalentSpecsAmount(long /*specClass*/)
{
    //QueryResult result = CharacterDatabase.PQuery("SELECT COUNT(*) FROM playerbot_talentspec WHERE class = '%li'", specClass);

    //if (!result)
    //{
    //    //sLog->outString();
    //    //sLog->outString(">> Loaded `playerbot_talentspec`, found no talentspecs for class %li.", specClass);

        return 0;
    //}

    //Field* fields = result->Fetch();

    //uint32 count = fields[0].GetUInt32();

    ////delete result;
    //return count;
}

/**
 * GetTalentSpecs queries DB for the talentspecs (for a class), returning them in a list of TS structures
 *
 * *** for the most part, GetTalentSpecs assumes ALL SPECS ARE VALID ***
 */
std::list<TalentSpec> PlayerbotAI::GetTalentSpecs(long /*specClass*/)
{
    TalentSpec ts;
    std::list<TalentSpec> tsList;
    tsList.clear();
    return tsList;

    //QueryResult result = CharacterDatabase.PQuery("SELECT * FROM playerbot_talentspec WHERE class = %li ORDER BY talentspec_id ASC", specClass);

    //if (!result)
    //{
    //    //sLog->outString();
    //    //sLog->outString(">> Loaded `playerbot_talentspec`, found no talentspecs for class %li.", specClass);

    //    return tsList; // empty
    //}

    //do
    //{
    //    /* 0            talentspec_id
    //       1            name
    //       2            class
    //       3            purpose
    //       4 to 74        talent_10 to 71
    //       75 to 80        major_glyph_15, 30, 80, minor_glyph_15, 50, 70
    //     */
    //    Field* fields = result->Fetch();

    //    /* ts_id = fields[0].GetUInt32(); // not used
    //       if (!ts_id)    // Nice bit of paranoia: ts_id is an AUTO_INCREMENT value
    //       continue;  // Of course, if the impossible ever does happen, we can't very well identify a TalentSpec without an ID...
    //     */

    //    ts.specName = fields[1].GetCString();
    //    ts.specClass = fields[2].GetInt16();
    //    if (ts.specClass != CLASS_DEATH_KNIGHT && ts.specClass != CLASS_DRUID && ts.specClass != CLASS_HUNTER && ts.specClass != CLASS_MAGE && ts.specClass != CLASS_PALADIN && ts.specClass != CLASS_PRIEST && ts.specClass != CLASS_ROGUE && ts.specClass != CLASS_SHAMAN && ts.specClass != CLASS_WARLOCK && ts.specClass != CLASS_WARRIOR &&
    //        ts.specClass != CLASS_PET_CUNNING && ts.specClass != CLASS_PET_FEROCITY && ts.specClass != CLASS_PET_TENACITY)
    //    {
    //        TellMaster("TalentSpec: %u. \"%s\" contains an invalid class.", fields[0].GetUInt32(), ts.specName.c_str());

    //        continue;    // this spec is clearly broken, the next may or may not be
    //    }

    //    ts.specPurpose = (TalentSpecPurpose) fields[3].GetUInt32();

    //    // check all talents
    //    for (uint8 i = 0; i < 71; i++)
    //    {
    //        ts.talentId[i] = fields[i + 4].GetUInt16();
    //    }

    //    for (uint8 i = 0; i < 3; i++)  // as in, the 3 major glyphs
    //    {
    //        ts.glyphIdMajor[i] = fields[i + 75].GetUInt16();
    //    }
    //    for (uint8 i = 0; i < 3; i++)  // as in, the 3 minor glyphs
    //    {
    //        ts.glyphIdMajor[i] = fields[i + 78].GetUInt16();
    //    }

    //    tsList.push_back(ts);
    //} while (result->NextRow());

    ////delete result;
    //return tsList;
}

/**
 * GetTalentSpec queries DB for a talentspec given a class and a choice.
 * The choice applies to the results for that class only, and is volatile.
 *
 * *** for the most part, GetTalentSpec assumes ALL SPECS ARE VALID ***
 */
TalentSpec PlayerbotAI::GetTalentSpec(long /*specClass*/, long /*choice*/)
{
    TalentSpec ts;
    // Let's zero it out to be safe
    ts.specName = "";
    ts.specClass = 0;
    ts.specPurpose = TSP_NONE;
    for (int i = 0; i < 71; i++) ts.talentId[i] = 0;
    for (int i = 0; i < 3; i++) ts.glyphIdMajor[i] = 0;
    for (int i = 0; i < 3; i++) ts.glyphIdMinor[i] = 0;
    return ts;

    //// Weed out invalid choice - ts has been zero'd out anyway
    //if (0 >= choice || (long) GetTalentSpecsAmount(specClass) < choice) return ts;

    //QueryResult result = CharacterDatabase.PQuery("SELECT * FROM playerbot_talentspec WHERE class = %li ORDER BY talentspec_id ASC", specClass);

    //if (!result)
    //{
    //    //sLog->outString();
    //    //sLog->outString(">> Loaded `playerbot_talentspec`, found no talentspecs for class %li.", specClass);

    //    //delete result;
    //    return ts; // empty
    //}

    //for (int i = 1; i <= (int) GetTalentSpecsAmount(specClass); i++)
    //{

    //    if (i == choice)
    //    {
    //        /*
    //           0            talentspec_id
    //           1            name
    //           2            class
    //           3            purpose
    //           4 to 74    talent_10 to 71
    //           75 to 80    major_glyph_15, 30, 80, minor_glyph_15, 50, 70
    //         */
    //        Field* fields = result->Fetch();

    //        /* ts_id = fields[0].GetUInt32(); // not used
    //           if (!ts_id)    // Nice bit of paranoia: ts_id is an AUTO_INCREMENT value
    //           continue;  // Of course, if the impossible ever does happen, we can't very well identify a TalentSpec without an ID...
    //         */

    //        ts.specName = fields[1].GetCString();
    //        ts.specClass = fields[2].GetInt16();
    //        if (ts.specClass != CLASS_DEATH_KNIGHT && ts.specClass != CLASS_DRUID && ts.specClass != CLASS_HUNTER && ts.specClass != CLASS_MAGE && ts.specClass != CLASS_PALADIN && ts.specClass != CLASS_PRIEST && ts.specClass != CLASS_ROGUE && ts.specClass != CLASS_SHAMAN && ts.specClass != CLASS_WARLOCK && ts.specClass != CLASS_WARRIOR &&
    //            ts.specClass != CLASS_PET_CUNNING && ts.specClass != CLASS_PET_FEROCITY && ts.specClass != CLASS_PET_TENACITY)
    //        {
    //            TellMaster("TalentSpec: %u. \"%s\" contains an invalid class.", fields[0].GetUInt32(), ts.specName.c_str());

    //            ts.specName = "";
    //            ts.specClass = 0;
    //            //delete result;
    //            return ts;
    //        }

    //        ts.specPurpose = (TalentSpecPurpose) fields[3].GetUInt32();

    //        // check all talents
    //        for (uint8 i = 0; i < 71; i++)
    //        {
    //            ts.talentId[i] = fields[i + 4].GetUInt16();
    //        }

    //        for (uint8 i = 0; i < 3; i++)  // as in, the 3 major glyphs
    //        {
    //            ts.glyphIdMajor[i] = fields[i + 75].GetUInt16();
    //        }
    //        for (uint8 i = 0; i < 3; i++)  // as in, the 3 minor glyphs
    //        {
    //            ts.glyphIdMajor[i] = fields[i + 78].GetUInt16();
    //        }

    //        //delete result;
    //        return ts;
    //    }

    //    // TODO: okay, this won't bog down the system, but it's still a waste. Figure out a better way.
    //    result->NextRow();
    //}

    ////delete result;
    //return ts;
}

/**
 * ApplyActiveTalentSpec takes the active talent spec and attempts to apply it
 *
 * return true  -> ok, talentspec applied as fully as possible
 * return false -> talentspec was not or only partially applied
 */
bool PlayerbotAI::ApplyActiveTalentSpec()
{
    //DISABLED
    //// empty talent spec -> nothing to apply -> fully applied
    //if (m_activeTalentSpec.specClass == 0 || m_activeTalentSpec.specPurpose == TSP_NONE)
    //    return true;

    //// Some basic error checking just in case
    //if (m_activeTalentSpec.specClass != m_bot->getClass())
    //    return false;

    //std::vector<uint16> talentsToLearn;
    //talentsToLearn.reserve(71);
    //for (int i = 0; i < 71; i++)
    //{
    //    if (m_activeTalentSpec.talentId[i] != 0)
    //        talentsToLearn.push_back(m_activeTalentSpec.talentId[i]);
    //}

    //PlayerTalentMap *ptm = m_bot->GetTalents(m_bot->GetActiveSpec());
    //// First do a check as to whether all known talents are in the talent spec
    //for (PlayerTalentMap::iterator iter = ptm->begin(); iter != ptm->end(); iter++)
    //{
    //    PlayerTalent* talent = (*iter).second;

    //    // WARNING: There may be more than 71 'talents' in the PTM - unlearned talents are simply set as disabled - not removed
    //    if (talent.state == PLAYERSPELL_REMOVED)
    //        continue;

    //    // currentRank = 0 to (MAX_RANK-1) not 1 to MAX_RANK
    //    for (int i = 0; i <= (int) talent.currentRank; i++)
    //    {
    //        int j = 0; // why 0 and not -1? Because if talentsToCheck (no TalentSpec) is empty and talents have been learned -> NOK
    //        for (std::vector<uint16>::iterator it = talentsToLearn.begin(); it != talentsToLearn.end(); it++)
    //        {
    //            if (talentsToLearn.at(j) == talent.talentEntry->TalentID)
    //            {
    //                talentsToLearn.erase(it);
    //                j = -1; // So j = -1 -> learned talent found in talentspec
    //                break;
    //            }
    //            j++;
    //        }

    //        // j == -1 signifies talent has been found in talent spec
    //        if (-1 != j)
    //        {
    //            TellMaster("I've learned talents that are not in my talent spec. If you want me to learn the talent spec anyway you should have me reset my talents.");
    //            return false;
    //        }
    //    }
    //}

    //int x = 0;
    //for (std::vector<uint16>::iterator iter = talentsToLearn.begin(); iter != talentsToLearn.end(); iter++)
    //{
    //    // find current talent rank
    //    uint32 learnTalentRank = 0;
    //    if (PlayerTalent const* talent = m_bot->GetKnownTalentById(talentsToLearn.at(x)))
    //        learnTalentRank = talent->currentRank + 1;
    //    // else -> not known -> to learn = 0

    //    // check if we have enough talent points
    //    uint32 freeTalentPointsBefore = m_bot->GetFreeTalentPoints();
    //    if (0 == freeTalentPointsBefore)
    //        return true;

    //    m_bot->LearnTalent(talentsToLearn.at(x), learnTalentRank);
    //    if (freeTalentPointsBefore == m_bot->GetFreeTalentPoints())
    //    {
    //        // Do not tell master - error is logged server side, master gets generic failure warning from calling function.
    //        //TellMaster("Failed to learn talent - Class: %i; TalentId: %i; TalentRank: %i. This error has been logged.", m_bot->getClass(), talentsToLearn.at(x), learnTalentRank);
    //        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: ApplyActiveTalentSpec failure - Class: %i; TalentId: %i; TalentRank: %i.", m_bot->getClass(), talentsToLearn.at(x), learnTalentRank);
    //        return false;
    //    }

    //    x++;
    //}

    return true;
}

/**
 * ValidateTalent tests a talent against class to see if it belongs to that class
 *
 * uint16 talent:        talent ID
 * long charClass:    member of the Classes enum or ClassesCombatPets enum
 *
 * return true  -> ok
 * return false -> not a valid talent for that class
 */
bool PlayerbotAI::ValidateTalent(uint16 talent, long charClass)
{
    if (charClass == CLASS_DEATH_KNIGHT)
    {
        // this looong 'if' is to see if any talent is not a Death Knight talent when the class clearly is
        if (DEATHKNIGHT_BUTCHERY != talent && DEATHKNIGHT_SUBVERSION != talent && DEATHKNIGHT_BLADE_BARRIER != talent && DEATHKNIGHT_BLADED_ARMOR != talent && DEATHKNIGHT_SCENT_OF_BLOOD != talent && DEATHKNIGHT_TWOHANDED_WEAPON_SPECIALIZATION != talent && DEATHKNIGHT_RUNE_TAP != talent && DEATHKNIGHT_DARK_CONVICTION != talent && DEATHKNIGHT_DEATH_RUNE_MASTERY != talent && DEATHKNIGHT_IMPROVED_RUNE_TAP != talent && DEATHKNIGHT_SPELL_DEFLECTION != talent && DEATHKNIGHT_VENDETTA != talent && DEATHKNIGHT_BLOODY_STRIKES != talent && DEATHKNIGHT_VETERAN_OF_THE_THIRD_WAR != talent && DEATHKNIGHT_MARK_OF_BLOOD != talent && DEATHKNIGHT_BLOODY_VENGEANCE != talent && DEATHKNIGHT_ABOMINATIONS_MIGHT != talent && DEATHKNIGHT_BLOOD_WORMS != talent && DEATHKNIGHT_HYSTERIA != talent && DEATHKNIGHT_IMPROVED_BLOOD_PRESENCE != talent && DEATHKNIGHT_IMPROVED_DEATH_STRIKE != talent && DEATHKNIGHT_SUDDEN_DOOM != talent && DEATHKNIGHT_VAMPIRIC_BLOOD != talent && DEATHKNIGHT_WILL_OF_THE_NECROPOLIS != talent && DEATHKNIGHT_HEART_STRIKE != talent && DEATHKNIGHT_MIGHT_OF_MOGRAINE != talent && DEATHKNIGHT_BLOOD_GORGED != talent && DEATHKNIGHT_DANCING_RUNE_WEAPON != talent && DEATHKNIGHT_IMPROVED_ICY_TOUCH != talent && DEATHKNIGHT_RUNIC_POWER_MASTERY != talent && DEATHKNIGHT_TOUGHNESS != talent && DEATHKNIGHT_ICY_REACH != talent && DEATHKNIGHT_BLACK_ICE != talent && DEATHKNIGHT_NERVES_OF_COLD_STEEL != talent && DEATHKNIGHT_ICY_TALONS != talent && DEATHKNIGHT_LICHBORNE != talent && DEATHKNIGHT_ANNIHILATION != talent && DEATHKNIGHT_KILLING_MACHINE != talent && DEATHKNIGHT_CHILL_OF_THE_GRAVE != talent && DEATHKNIGHT_ENDLESS_WINTER != talent && DEATHKNIGHT_FRIGID_DREADPLATE != talent && DEATHKNIGHT_GLACIER_ROT != talent && DEATHKNIGHT_DEATHCHILL != talent && DEATHKNIGHT_IMPROVED_ICY_TALONS != talent && DEATHKNIGHT_MERCILESS_COMBAT != talent && DEATHKNIGHT_RIME != talent && DEATHKNIGHT_CHILLBLAINS != talent && DEATHKNIGHT_HUNGERING_COLD != talent && DEATHKNIGHT_IMPROVED_FROST_PRESENCE != talent && DEATHKNIGHT_THREAT_OF_THASSARIAN != talent && DEATHKNIGHT_BLOOD_OF_THE_NORTH != talent && DEATHKNIGHT_UNBREAKABLE_ARMOR != talent && DEATHKNIGHT_ACCLIMATION != talent && DEATHKNIGHT_FROST_STRIKE != talent && DEATHKNIGHT_GUILE_OF_GOREFIEND != talent && DEATHKNIGHT_TUNDRA_STALKER != talent && DEATHKNIGHT_HOWLING_BLAST != talent && DEATHKNIGHT_VICIOUS_STRIKES != talent && DEATHKNIGHT_VIRULENCE != talent && DEATHKNIGHT_ANTICIPATION != talent && DEATHKNIGHT_EPIDEMIC != talent && DEATHKNIGHT_MORBIDITY != talent && DEATHKNIGHT_UNHOLY_COMMAND != talent && DEATHKNIGHT_RAVENOUS_DEAD != talent && DEATHKNIGHT_OUTBREAK != talent && DEATHKNIGHT_NECROSIS != talent && DEATHKNIGHT_CORPSE_EXPLOSION != talent && DEATHKNIGHT_ON_A_PALE_HORSE != talent && DEATHKNIGHT_BLOODCAKED_BLADE != talent && DEATHKNIGHT_NIGHT_OF_THE_DEAD != talent && DEATHKNIGHT_UNHOLY_BLIGHT != talent && DEATHKNIGHT_IMPURITY != talent && DEATHKNIGHT_DIRGE != talent && DEATHKNIGHT_DESECRATION != talent && DEATHKNIGHT_MAGIC_SUPPRESSION != talent && DEATHKNIGHT_REAPING != talent && DEATHKNIGHT_MASTER_OF_GHOULS != talent && DEATHKNIGHT_DESOLATION != talent && DEATHKNIGHT_ANTIMAGIC_ZONE != talent && DEATHKNIGHT_IMPROVED_UNHOLY_PRESENCE != talent && DEATHKNIGHT_GHOUL_FRENZY != talent && DEATHKNIGHT_CRYPT_FEVER != talent && DEATHKNIGHT_BONE_SHIELD != talent && DEATHKNIGHT_WANDERING_PLAGUE != talent && DEATHKNIGHT_EBON_PLAGUEBRINGER != talent && DEATHKNIGHT_SCOURGE_STRIKE != talent && DEATHKNIGHT_RAGE_OF_RIVENDARE != talent && DEATHKNIGHT_SUMMON_GARGOYLE != talent)
            return false;
    }
    else if (charClass == CLASS_DRUID)
    {
        if (DRUID_FEROCITY != talent && DRUID_FERAL_AGGRESSION != talent && DRUID_FERAL_INSTINCT != talent && DRUID_SAVAGE_FURY != talent && DRUID_THICK_HIDE != talent && DRUID_FERAL_SWIFTNESS != talent && DRUID_SURVIVAL_INSTINCTS != talent && DRUID_SHARPENED_CLAWS != talent && DRUID_SHREDDING_ATTACKS != talent && DRUID_PREDATORY_STRIKES != talent && DRUID_PRIMAL_FURY != talent && DRUID_PRIMAL_PRECISION != talent && DRUID_BRUTAL_IMPACT != talent && DRUID_FERAL_CHARGE != talent && DRUID_NURTURING_INSTINCT != talent && DRUID_NATURAL_REACTION != talent && DRUID_HEART_OF_THE_WILD != talent && DRUID_SURVIVAL_OF_THE_FITTEST != talent && DRUID_LEADER_OF_THE_PACK != talent && DRUID_IMPROVED_LEADER_OF_THE_PACK != talent && DRUID_PRIMAL_TENACITY != talent && DRUID_PROTECTOR_OF_THE_PACK != talent && DRUID_PREDATORY_INSTINCTS != talent && DRUID_INFECTED_WOUNDS != talent && DRUID_KING_OF_THE_JUNGLE != talent && DRUID_MANGLE != talent && DRUID_IMPROVED_MANGLE != talent && DRUID_REND_AND_TEAR != talent && DRUID_PRIMAL_GORE != talent && DRUID_BERSERK != talent && DRUID_IMPROVED_MARK_OF_THE_WILD != talent && DRUID_NATURES_FOCUS != talent && DRUID_FUROR != talent && DRUID_NATURALIST != talent && DRUID_SUBTLETY != talent && DRUID_NATURAL_SHAPESHIFTER != talent && DRUID_INTENSITY != talent && DRUID_OMEN_OF_CLARITY != talent && DRUID_MASTER_SHAPESHIFTER != talent && DRUID_TRANQUIL_SPIRIT != talent && DRUID_IMPROVED_REJUVENATION != talent && DRUID_NATURES_SWIFTNESS != talent && DRUID_GIFT_OF_NATURE != talent && DRUID_IMPROVED_TRANQUILITY != talent && DRUID_EMPOWERED_TOUCH != talent && DRUID_NATURES_BOUNTY != talent && DRUID_LIVING_SPIRIT != talent && DRUID_SWIFTMEND != talent && DRUID_NATURAL_PERFECTION != talent && DRUID_EMPOWERED_REJUVENATION != talent && DRUID_LIVING_SEED != talent && DRUID_REVITALIZE != talent && DRUID_TREE_OF_LIFE != talent && DRUID_IMPROVED_TREE_OF_LIFE != talent && DRUID_IMPROVED_BARKSKIN != talent && DRUID_GIFT_OF_THE_EARTHMOTHER != talent && DRUID_WILD_GROWTH != talent && DRUID_STARLIGHT_WRATH != talent && DRUID_GENESIS != talent && DRUID_MOONGLOW != talent && DRUID_NATURES_MAJESTY != talent && DRUID_IMPROVED_MOONFIRE != talent && DRUID_BRAMBLES != talent && DRUID_NATURES_GRACE != talent && DRUID_NATURES_SPLENDOR_A != talent && DRUID_NATURES_REACH != talent && DRUID_VENGEANCE != talent && DRUID_CELESTIAL_FOCUS != talent && DRUID_LUNAR_GUIDANCE != talent && DRUID_INSECT_SWARM != talent && DRUID_IMPROVED_INSECT_SWARM != talent && DRUID_DREAMSTATE != talent && DRUID_MOONFURY != talent && DRUID_BALANCE_OF_POWER != talent && DRUID_MOONKIN_FORM != talent && DRUID_IMPROVED_MOONKIN_FORM != talent && DRUID_IMPROVED_FAERIE_FIRE != talent && DRUID_OWLKIN_FRENZY != talent && DRUID_WRATH_OF_CENARIUS != talent && DRUID_ECLIPSE != talent && DRUID_TYPHOON != talent && DRUID_FORCE_OF_NATURE != talent && DRUID_GALE_WINDS != talent && DRUID_EARTH_AND_MOON != talent && DRUID_STARFALL != talent)
            return false;
    }
    else if (charClass == CLASS_HUNTER)
    {
        if (HUNTER_IMPROVED_ASPECT_OF_THE_HAWK != talent && HUNTER_ENDURANCE_TRAINING != talent && HUNTER_FOCUSED_FIRE != talent && HUNTER_IMPROVED_ASPECT_OF_THE_MONKEY != talent && HUNTER_THICK_HIDE != talent && HUNTER_IMPROVED_REVIVE_PET != talent && HUNTER_PATHFINDING != talent && HUNTER_ASPECT_MASTERY != talent && HUNTER_UNLEASHED_FURY != talent && HUNTER_IMPROVED_MEND_PET != talent && HUNTER_FEROCITY != talent && HUNTER_SPIRIT_BOND != talent && HUNTER_INTIMIDATION != talent && HUNTER_BESTIAL_DISCIPLINE != talent && HUNTER_ANIMAL_HANDLER != talent && HUNTER_FRENZY != talent && HUNTER_FEROCIOUS_INSPIRATION != talent && HUNTER_BESTIAL_WRATH != talent && HUNTER_CATLIKE_REFLEXES != talent && HUNTER_INVIGORATION != talent && HUNTER_SERPENTS_SWIFTNESS != talent && HUNTER_LONGEVITY != talent && HUNTER_THE_BEAST_WITHIN != talent && HUNTER_COBRA_STRIKES != talent && HUNTER_KINDRED_SPIRITS != talent && HUNTER_BEAST_MASTERY != talent && HUNTER_IMPROVED_TRACKING != talent && HUNTER_HAWK_EYE != talent && HUNTER_SAVAGE_STRIKES != talent && HUNTER_SUREFOOTED != talent && HUNTER_ENTRAPMENT != talent && HUNTER_TRAP_MASTERY != talent && HUNTER_SURVIVAL_INSTINCTS != talent && HUNTER_SURVIVALIST != talent && HUNTER_SCATTER_SHOT != talent && HUNTER_DEFLECTION != talent && HUNTER_SURVIVAL_TACTICS != talent && HUNTER_TNT != talent && HUNTER_LOCK_AND_LOAD != talent && HUNTER_HUNTER_VS_WILD != talent && HUNTER_KILLER_INSTINCT != talent && HUNTER_COUNTERATTACK != talent && HUNTER_LIGHTNING_REFLEXES != talent && HUNTER_RESOURCEFULNESS != talent && HUNTER_EXPOSE_WEAKNESS != talent && HUNTER_WYVERN_STING != talent && HUNTER_THRILL_OF_THE_HUNT != talent && HUNTER_MASTER_TACTICIAN != talent && HUNTER_NOXIOUS_STINGS != talent && HUNTER_POINT_OF_NO_ESCAPE != talent && HUNTER_BLACK_ARROW != talent && HUNTER_SNIPER_TRAINING != talent && HUNTER_HUNTING_PARTY != talent && HUNTER_EXPLOSIVE_SHOT != talent && HUNTER_IMPROVED_CONCUSSIVE_SHOT != talent && HUNTER_FOCUSED_AIM != talent && HUNTER_LETHAL_SHOTS != talent && HUNTER_CAREFUL_AIM != talent && HUNTER_IMPROVED_HUNTERS_MARK != talent && HUNTER_MORTAL_SHOTS != talent && HUNTER_GO_FOR_THE_THROAT != talent && HUNTER_IMPROVED_ARCANE_SHOT != talent && HUNTER_AIMED_SHOT != talent && HUNTER_RAPID_KILLING != talent && HUNTER_IMPROVED_STINGS != talent && HUNTER_EFFICIENCY != talent && HUNTER_CONCUSSIVE_BARRAGE != talent && HUNTER_READINESS != talent && HUNTER_BARRAGE != talent && HUNTER_COMBAT_EXPERIENCE != talent && HUNTER_RANGED_WEAPON_SPECIALIZATION != talent && HUNTER_PIERCING_SHOTS != talent && HUNTER_TRUESHOT_AURA != talent && HUNTER_IMPROVED_BARRAGE != talent && HUNTER_MASTER_MARKSMAN != talent && HUNTER_RAPID_RECUPERATION != talent && HUNTER_WILD_QUIVER != talent && HUNTER_SILENCING_SHOT != talent && HUNTER_IMPROVED_STEADY_SHOT != talent && HUNTER_MARKED_FOR_DEATH != talent && HUNTER_CHIMERA_SHOT != talent)
            return false;
    }
    else if (charClass == CLASS_MAGE)
    {
        if (MAGE_IMPROVED_FIRE_BLAST != talent && MAGE_INCINERATION != talent && MAGE_IMPROVED_FIREBALL != talent && MAGE_IGNITE != talent && MAGE_BURNING_DETERMINATION != talent && MAGE_WORLD_IN_FLAMES != talent && MAGE_FLAME_THROWING != talent && MAGE_IMPACT != talent && MAGE_PYROBLAST != talent && MAGE_BURNING_SOUL != talent && MAGE_IMPROVED_SCORCH != talent && MAGE_MOLTEN_SHIELDS != talent && MAGE_MASTER_OF_ELEMENTS != talent && MAGE_PLAYING_WITH_FIRE != talent && MAGE_CRITICAL_MASS != talent && MAGE_BLAST_WAVE != talent && MAGE_BLAZING_SPEED != talent && MAGE_FIRE_POWER != talent && MAGE_PYROMANIAC != talent && MAGE_COMBUSTION != talent && MAGE_MOLTEN_FURY != talent && MAGE_FIERY_PAYBACK != talent && MAGE_EMPOWERED_FIRE != talent && MAGE_FIRESTARTER != talent && MAGE_DRAGONS_BREATH != talent && MAGE_HOT_STREAK != talent && MAGE_BURNOUT != talent && MAGE_LIVING_BOMB != talent && MAGE_FROSTBITE != talent && MAGE_IMPROVED_FROSTBOLT != talent && MAGE_ICE_FLOES != talent && MAGE_ICE_SHARDS != talent && MAGE_FROST_WARDING != talent && MAGE_PRECISION != talent && MAGE_PERMAFROST != talent && MAGE_PIERCING_ICE != talent && MAGE_ICY_VEINS != talent && MAGE_IMPROVED_BLIZZARD != talent && MAGE_ARCTIC_REACH != talent && MAGE_FROST_CHANNELING != talent && MAGE_SHATTER != talent && MAGE_COLD_SNAP != talent && MAGE_IMPROVED_CONE_OF_COLD != talent && MAGE_FROZEN_CORE != talent && MAGE_COLD_AS_ICE != talent && MAGE_WINTERS_CHILL != talent && MAGE_SHATTERED_BARRIER != talent && MAGE_ICE_BARRIER != talent && MAGE_ARCTIC_WINDS != talent && MAGE_EMPOWERED_FROSTBOLT != talent && MAGE_FINGERS_OF_FROST != talent && MAGE_BRAIN_FREEZE != talent && MAGE_SUMMON_WATER_ELEMENTAL != talent && MAGE_ENDURING_WINTER != talent && MAGE_CHILLD_TO_THE_BONE != talent && MAGE_DEEP_FREEZE != talent && MAGE_ARCANE_SUBTLETY != talent && MAGE_ARCANE_FOCUS != talent && MAGE_ARCANE_STABILITY != talent && MAGE_ARCANE_FORTITUDE != talent && MAGE_MAGIC_ABSORPTION != talent && MAGE_ARCANE_CONCENTRATION != talent && MAGE_MAGIC_ATTUNEMENT != talent && MAGE_SPELL_IMPACT != talent && MAGE_STUDENT_OF_THE_MIND != talent && MAGE_FOCUS_MAGIC != talent && MAGE_ARCANE_SHIELDING != talent && MAGE_IMPROVED_COUNTERSPELL != talent && MAGE_ARCANE_MEDITATION != talent && MAGE_TORMENT_THE_WEAK != talent && MAGE_IMPROVED_BLINK != talent && MAGE_PRESENCE_OF_MIND != talent && MAGE_ARCANE_MIND != talent && MAGE_PRISMATIC_CLOAK != talent && MAGE_ARCANE_INSTABILITY != talent && MAGE_ARCANE_POTENCY != talent && MAGE_ARCANE_EMPOWERMENT != talent && MAGE_ARCANE_POWER != talent && MAGE_INCANTERS_ABSORPTION != talent && MAGE_ARCANE_FLOWS != talent && MAGE_MIND_MASTERY != talent && MAGE_SLOW != talent && MAGE_MISSILE_BARRAGE != talent && MAGE_NETHERWIND_PRESENCE != talent && MAGE_SPELL_POWER != talent && MAGE_ARCANE_BARRAGE != talent)
            return false;
    }
    else if (charClass == CLASS_PALADIN)
    {
        if (PALADIN_DEFLECTION != talent && PALADIN_BENEDICTION != talent && PALADIN_IMPROVED_JUDGEMENTS != talent && PALADIN_HEART_OF_THE_CRUSADER != talent && PALADIN_IMPROVED_BLESSING_OF_MIGHT != talent && PALADIN_VINDICATION != talent && PALADIN_CONVICTION != talent && PALADIN_SEAL_OF_COMMAND != talent && PALADIN_PURSUIT_OF_JUSTICE != talent && PALADIN_EYE_FOR_AN_EYE != talent && PALADIN_SANCTITY_OF_BATTLE != talent && PALADIN_CRUSADE != talent && PALADIN_TWOHANDED_WEAPON_SPECIALIZATION != talent && PALADIN_SANCTIFIED_RETRIBUTION != talent && PALADIN_VENGEANCE != talent && PALADIN_DIVINE_PURPOSE != talent && PALADIN_THE_ART_OF_WAR != talent && PALADIN_REPENTANCE != talent && PALADIN_JUDGEMENTS_OF_THE_WISE != talent && PALADIN_FANATICISM != talent && PALADIN_SANCTIFIED_WRATH != talent && PALADIN_SWIFT_RETRIBUTION != talent && PALADIN_CRUSADER_STRIKE != talent && PALADIN_SHEATH_OF_LIGHT != talent && PALADIN_RIGHTEOUS_VENGEANCE != talent && PALADIN_DIVINE_STORM != talent && PALADIN_SPIRITUAL_FOCUS != talent && PALADIN_SEALS_OF_THE_PURE != talent && PALADIN_HEALING_LIGHT != talent && PALADIN_DIVINE_INTELLECT != talent && PALADIN_UNYIELDING_FAITH != talent && PALADIN_AURA_MASTERY != talent && PALADIN_ILLUMINATION != talent && PALADIN_IMPROVED_LAY_ON_HANDS != talent && PALADIN_IMPROVED_CONCENTRATION_AURA != talent && PALADIN_IMPROVED_BLESSING_OF_WISDOM != talent && PALADIN_BLESSED_HANDS != talent && PALADIN_PURE_OF_HEART != talent && PALADIN_DIVINE_FAVOR != talent && PALADIN_SANCTIFIED_LIGHT != talent && PALADIN_PURIFYING_POWER != talent && PALADIN_HOLY_POWER != talent && PALADIN_LIGHTS_GRACE != talent && PALADIN_HOLY_SHOCK != talent && PALADIN_BLESSED_LIFE != talent && PALADIN_SACRED_CLEANSING != talent && PALADIN_HOLY_GUIDANCE != talent && PALADIN_DIVINE_ILLUMINATION != talent && PALADIN_JUDGEMENTS_OF_THE_PURE != talent && PALADIN_INFUSION_OF_LIGHT != talent && PALADIN_ENLIGHTENED_JUDGEMENTS != talent && PALADIN_BEACON_OF_LIGHT != talent && PALADIN_DIVINITY != talent && PALADIN_DIVINE_STRENGTH != talent && PALADIN_STOICISM != talent && PALADIN_GUARDIANS_FAVOR != talent && PALADIN_ANTICIPATION != talent && PALADIN_DIVINE_SACRIFICE != talent && PALADIN_IMPROVED_RIGHTEOUS_FURY != talent && PALADIN_TOUGHNESS != talent && PALADIN_DIVINE_GUARDIAN != talent && PALADIN_IMPROVED_HAMMER_OF_JUSTICE != talent && PALADIN_IMPROVED_DEVOTION_AURA != talent && PALADIN_BLESSING_OF_SANCTUARY != talent && PALADIN_RECKONING != talent && PALADIN_SACRED_DUTY != talent && PALADIN_ONEHANDED_WEAPON_SPECIALIZATION != talent && PALADIN_SPIRITUAL_ATTUNEMENT != talent && PALADIN_HOLY_SHIELD != talent && PALADIN_ARDENT_DEFENDER != talent && PALADIN_REDOUBT != talent && PALADIN_COMBAT_EXPERTISE != talent && PALADIN_TOUCHER_BY_THE_LIGHT != talent && PALADIN_AVENGERS_SHIELD != talent && PALADIN_GUARDED_BY_THE_LIGHT != talent && PALADIN_SHIELD_OF_THE_TEMPLAR != talent && PALADIN_JUDGEMENT_OF_THE_JUST != talent && PALADIN_HAMMER_OF_THE_RIGHTEOUS != talent)
            return false;
    }
    else if (charClass == CLASS_PRIEST)
    {
        if (PRIEST_UNBREAKABLE_WILL != talent && PRIEST_TWIN_DISCIPLINES != talent && PRIEST_SILENT_RESOLVE != talent && PRIEST_IMPROVED_INNER_FIRE != talent && PRIEST_IMPROVED_POWER_WORD_FORTITUDE != talent && PRIEST_MARTYRDOM != talent && PRIEST_MEDITATION != talent && PRIEST_INNER_FOCUS != talent && PRIEST_IMPROVED_POWER_WORD_SHIELD != talent && PRIEST_ABSOLUTION != talent && PRIEST_MENTAL_AGILITY != talent && PRIEST_IMPROVED_MANA_BURN != talent && PRIEST_REFLECTIVE_SHIELD != talent && PRIEST_MENTAL_STRENGTH != talent && PRIEST_SOUL_WARDING != talent && PRIEST_FOCUSED_POWER != talent && PRIEST_ENLIGHTENMENT != talent && PRIEST_FOCUSED_WILL != talent && PRIEST_POWER_INFUSION != talent && PRIEST_IMPROVED_FLASH_HEAL != talent && PRIEST_RENEWED_HOPE != talent && PRIEST_RAPTURE != talent && PRIEST_ASPIRATION != talent && PRIEST_DIVINE_AEGIS != talent && PRIEST_PAIN_SUPPRESSION != talent && PRIEST_GRACE != talent && PRIEST_BORROWED_TIME != talent && PRIEST_PENANCE != talent && PRIEST_HEALING_FOCUS != talent && PRIEST_IMPROVED_RENEW != talent && PRIEST_HOLY_SPECIALIZATION != talent && PRIEST_SPELL_WARDING != talent && PRIEST_DIVINE_FURY != talent && PRIEST_DESPERATE_PRAYER != talent && PRIEST_BLESSED_RECOVERY != talent && PRIEST_INSPIRATION != talent && PRIEST_HOLY_REACH != talent && PRIEST_IMPROVED_HEALIN != talent && PRIEST_SEARING_LIGHT != talent && PRIEST_HEALING_PRAYERS != talent && PRIEST_SPIRIT_OF_REDEMPTION != talent && PRIEST_SPIRITUAL_GUIDANCE != talent && PRIEST_SURGE_OF_LIGHT != talent && PRIEST_SPIRITUAL_HEALING != talent && PRIEST_HOLY_CONCENTRATION != talent && PRIEST_LIGHTWELL != talent && PRIEST_BLESSED_RESILIENCE != talent && PRIEST_BODY_AND_SOUL != talent && PRIEST_EMPOWERED_HEALING != talent && PRIEST_SERENDIPITY != talent && PRIEST_EMPOWERED_RENEW != talent && PRIEST_CIRCLE_OF_HEALING != talent && PRIEST_TEST_OF_FAITH != talent && PRIEST_DIVINE_PROVIDENCE != talent && PRIEST_GUARDIAN_SPIRIT != talent && PRIEST_SPIRIT_TAP != talent && PRIEST_IMPROVED_SPIRIT_TAP != talent && PRIEST_DARKNESS != talent && PRIEST_SHADOW_AFFINITY != talent && PRIEST_IMPROVED_SHADOW_WORD_PAIN != talent && PRIEST_SHADOW_FOCUS != talent && PRIEST_IMPROVED_PSYCHIC_SCREAM != talent && PRIEST_IMPROVED_MIND_BLAST != talent && PRIEST_MIND_FLAY != talent && PRIEST_VEILED_SHADOWS != talent && PRIEST_SHADOW_REACH != talent && PRIEST_SHADOW_WEAVING != talent && PRIEST_SILENCE != talent && PRIEST_VAMPIRIC_EMBRACE != talent && PRIEST_IMPROVED_VAMPIRIC_EMBRACE != talent && PRIEST_FOCUSED_MIND != talent && PRIEST_MIND_MELT != talent && PRIEST_IMPROVED_DEVOURING_PLAGUE != talent && PRIEST_SHADOWFORM != talent && PRIEST_SHADOW_POWER != talent && PRIEST_IMPROVED_SHADOWFORM != talent && PRIEST_MISERY != talent && PRIEST_PSYCHIC_HORROR != talent && PRIEST_VAMPIRIC_TOUCH != talent && PRIEST_PAIN_AND_SUFFERING != talent && PRIEST_TWISTED_FAITH != talent && PRIEST_DISPERSION != talent)
            return false;
    }
    else if (charClass == CLASS_ROGUE)
    {
        if (ROGUE_IMPROVED_GOUGE != talent && ROGUE_IMPROVED_SINISTER_STRIKE != talent && ROGUE_DUAL_WIELD_SPECIALIZATION != talent && ROGUE_IMPROVED_SLICE_AND_DICE != talent && ROGUE_DEFLECTION != talent && ROGUE_PRECISION != talent && ROGUE_ENDURANCE != talent && ROGUE_RIPOSTE != talent && ROGUE_CLOSE_QUARTERS_COMBAT != talent && ROGUE_IMPROVED_KICK != talent && ROGUE_IMPROVED_SPRINT != talent && ROGUE_LIGHTNING_REFLEXES != talent && ROGUE_AGGRESSION != talent && ROGUE_MACE_SPECIALIZATION != talent && ROGUE_BLADE_FLURRY != talent && ROGUE_HACK_AND_SLASH != talent && ROGUE_WEAPON_EXPERTISE != talent && ROGUE_BLADE_TWISTING != talent && ROGUE_VITALITY != talent && ROGUE_ADRENALINE_RUSH != talent && ROGUE_NERVES_OF_STEEL != talent && ROGUE_THROWING_SPECIALIZATION != talent && ROGUE_COMBAT_POTENCY != talent && ROGUE_UNFAIR_ADVANTAGE != talent && ROGUE_SURPRISE_ATTACKS != talent && ROGUE_SAVAGE_COMBAT != talent && ROGUE_PREY_ON_THE_WEAK != talent && ROGUE_KILLING_SPREE != talent && ROGUE_IMPROVED_EVISCERATE != talent && ROGUE_REMORSELESS_ATTACKS != talent && ROGUE_MALICE != talent && ROGUE_RUTHLESSNESS != talent && ROGUE_BLOOD_SPATTER != talent && ROGUE_PUNCTURING_WOUNDS != talent && ROGUE_VIGOR != talent && ROGUE_IMPROVED_EXPOSE_ARMOR != talent && ROGUE_LETHALITY != talent && ROGUE_VILE_POISONS != talent && ROGUE_IMPROVED_POISONS != talent && ROGUE_FLEET_FOOTED != talent && ROGUE_COLD_BLOOD != talent && ROGUE_IMPROVED_KIDNEY_SHOT != talent && ROGUE_QUICK_RECOVERY != talent && ROGUE_SEAL_FATE != talent && ROGUE_MURDER != talent && ROGUE_DEADLY_BREW != talent && ROGUE_OVERKILL != talent && ROGUE_DEADENED_NERVES != talent && ROGUE_FOCUSED_ATTACKS != talent && ROGUE_FIND_WEAKNESS != talent && ROGUE_MASTER_POISONER != talent && ROGUE_MUTILATE != talent && ROGUE_TURN_THE_TABLES != talent && ROGUE_CUT_TO_THE_CHASE != talent && ROGUE_HUNGER_FOR_BLOOD != talent && ROGUE_RELENTLESS_STRIKES != talent && ROGUE_MASTER_OF_DECEPTION != talent && ROGUE_OPPORTUNITY != talent && ROGUE_SLEIGHT_OF_HAND != talent && ROGUE_DIRTY_TRICKS != talent && ROGUE_CAMOUFLAGE != talent && ROGUE_ELUSIVENESS != talent && ROGUE_GHOSTLY_STRIKE != talent && ROGUE_SERRATED_BLADES != talent && ROGUE_SETUP != talent && ROGUE_INITIATIVE != talent && ROGUE_IMPROVED_AMBUSH != talent && ROGUE_HEIGHTENED_SENSES != talent && ROGUE_PREPARATION != talent && ROGUE_DIRTY_DEEDS != talent && ROGUE_HEMORRHAGE != talent && ROGUE_MASTER_OF_SUBTLETY != talent && ROGUE_DEADLINESS != talent && ROGUE_ENVELOPING_SHADOWS != talent && ROGUE_PREMEDITATION != talent && ROGUE_CHEAT_DEATH != talent && ROGUE_SINISTER_CALLING != talent && ROGUE_WAYLAY != talent && ROGUE_HONOR_AMONG_THIEVES != talent && ROGUE_SHADOWSTEP != talent && ROGUE_FILTHY_TRICKS != talent && ROGUE_SLAUGHTER_FROM_THE_SHADOWS != talent && ROGUE_SHADOW_DANCE != talent)
            return false;
    }
    else if (charClass == CLASS_SHAMAN)
    {
        if (SHAMAN_CONVECTION != talent && SHAMAN_CONCUSSION != talent && SHAMAN_CALL_OF_FLAME != talent && SHAMAN_ELEMENTAL_WARDING != talent && SHAMAN_ELEMENTAL_DEVASTATION != talent && SHAMAN_REVERBERATION != talent && SHAMAN_ELEMENTAL_FOCUS != talent && SHAMAN_ELEMENTAL_FURY != talent && SHAMAN_IMPROVED_FIRE_NOVA != talent && SHAMAN_EYE_OF_THE_STORM != talent && SHAMAN_ELEMENTAL_REACH != talent && SHAMAN_CALL_OF_THUNDER != talent && SHAMAN_UNRELENTING_STORM != talent && SHAMAN_ELEMENTAL_PRECISION != talent && SHAMAN_LIGHTNING_MASTERY != talent && SHAMAN_ELEMENTAL_MASTERY != talent && SHAMAN_STORM_EARTH_AND_FIRE != talent && SHAMAN_BOOMING_ECHOES != talent && SHAMAN_ELEMENTAL_OATH != talent && SHAMAN_LIGHTNING_OVERLOAD != talent && SHAMAN_ASTRAL_SHIFT != talent && SHAMAN_TOTEM_OF_WRATH != talent && SHAMAN_LAVA_FLOWS != talent && SHAMAN_SHAMANISM != talent && SHAMAN_THUNDERSTORM != talent && SHAMAN_IMPROVED_HEALING_WAVE != talent && SHAMAN_TOTEMIC_FOCUS != talent && SHAMAN_IMPROVED_REINCARNATION != talent && SHAMAN_HEALING_GRACE != talent && SHAMAN_TIDAL_FOCUS != talent && SHAMAN_IMPROVED_WATER_SHIELD != talent && SHAMAN_HEALING_FOCUS != talent && SHAMAN_TIDAL_FORCE != talent && SHAMAN_ANCESTRAL_HEALING != talent && SHAMAN_RESTORATIVE_TOTEMS != talent && SHAMAN_TIDAL_MASTERY != talent && SHAMAN_HEALING_WAY != talent && SHAMAN_NATURES_SWIFTNESS != talent && SHAMAN_FOCUSED_MIND != talent && SHAMAN_PURIFICATION != talent && SHAMAN_NATURES_GUARDIAN != talent && SHAMAN_MANA_TIDE_TOTEM != talent && SHAMAN_CLEANSE_SPIRIT != talent && SHAMAN_BLESSING_OF_THE_ETERNALS != talent && SHAMAN_IMPROVED_CHAIN_HEAL != talent && SHAMAN_NATURES_BLESSING != talent && SHAMAN_ANCESTRAL_AWAKENING != talent && SHAMAN_EARTH_SHIELD != talent && SHAMAN_IMPROVED_EARTH_SHIELD != talent && SHAMAN_TIDAL_WAVES != talent && SHAMAN_RIPTIDE != talent && SHAMAN_ENHANCING_TOTEMS != talent && SHAMAN_EARTHS_GRASP != talent && SHAMAN_ANCESTRAL_KNOWLEDGE != talent && SHAMAN_GUARDIAN_TOTEMS != talent && SHAMAN_THUNDERING_STRIKES != talent && SHAMAN_IMPROVED_GHOST_WOLF != talent && SHAMAN_IMPROVED_SHIELDS != talent && SHAMAN_ELEMENTAL_WEAPONS != talent && SHAMAN_SHAMANISTIC_FOCUS != talent && SHAMAN_ANTICIPATION != talent && SHAMAN_FLURRY != talent && SHAMAN_TOUGHNESS != talent && SHAMAN_IMPROVED_WINDFURY_TOTEM != talent && SHAMAN_SPIRIT_WEAPONS != talent && SHAMAN_MENTAL_DEXTERITY != talent && SHAMAN_UNLEASHED_RAGE != talent && SHAMAN_WEAPON_MASTERY != talent && SHAMAN_FROZEN_POWER != talent && SHAMAN_DUAL_WIELD_SPECIALIZATION != talent && SHAMAN_DUAL_WIELD != talent && SHAMAN_STORMSTRIKE != talent && SHAMAN_STATIC_SHOCK != talent && SHAMAN_LAVA_LASH != talent && SHAMAN_IMPROVED_STORMSTRIKE != talent && SHAMAN_MENTAL_QUICKNESS != talent && SHAMAN_SHAMANISTIC_RAGE != talent && SHAMAN_EARTHEN_POWER != talent && SHAMAN_MAELSTROM_WEAPON != talent && SHAMAN_FERAL_SPIRIT != talent)
            return false;
    }
    else if (charClass == CLASS_WARLOCK)
    {
        if (WARLOCK_IMPROVED_SHADOW_BOLT != talent && WARLOCK_BANE != talent && WARLOCK_AFTERMATH != talent && WARLOCK_MOLTEN_SKIN != talent && WARLOCK_CATACLYSM != talent && WARLOCK_DEMONIC_POWER != talent && WARLOCK_SHADOWBURN != talent && WARLOCK_RUIN != talent && WARLOCK_INTENSITY != talent && WARLOCK_DESTRUCTIVE_REACH != talent && WARLOCK_IMPROVED_SEARING_PAIN != talent && WARLOCK_BACKLASH != talent && WARLOCK_IMPROVED_IMMOLATE != talent && WARLOCK_DEVASTATION != talent && WARLOCK_NETHER_PROTECTION != talent && WARLOCK_EMBERSTORM != talent && WARLOCK_CONFLAGRATE != talent && WARLOCK_SOUL_LEECH != talent && WARLOCK_PYROCLASM != talent && WARLOCK_SHADOW_AND_FLAME != talent && WARLOCK_IMPROVED_SOUL_LEECH != talent && WARLOCK_BACKDRAFT != talent && WARLOCK_SHADOWFURY != talent && WARLOCK_EMPOWERED_IMP != talent && WARLOCK_FIRE_AND_BRIMSTONE != talent && WARLOCK_CHAOS_BOLT != talent && WARLOCK_IMPROVED_CURSE_OF_AGONY != talent && WARLOCK_SUPPRESSION != talent && WARLOCK_IMPROVED_CORRUPTION != talent && WARLOCK_IMPROVED_CURSE_OF_WEAKNESS != talent && WARLOCK_IMPROVED_DRAIN_SOUL != talent && WARLOCK_IMPROVED_LIFE_TAP != talent && WARLOCK_SOUL_SIPHON != talent && WARLOCK_IMPROVED_FEAR != talent && WARLOCK_FEL_CONCENTRATION != talent && WARLOCK_AMPLIFY_CURSE != talent && WARLOCK_GRIM_REACH != talent && WARLOCK_NIGHTFALL != talent && WARLOCK_EMPOWERED_CORRUPTION != talent && WARLOCK_SHADOW_EMBRACE != talent && WARLOCK_SIPHON_LIFE != talent && WARLOCK_CURSE_OF_EXHAUSTION != talent && WARLOCK_IMPROVED_FELHUNTER != talent && WARLOCK_SHADOW_MASTERY != talent && WARLOCK_ERADICATION != talent && WARLOCK_CONTAGION != talent && WARLOCK_DARK_PACT != talent && WARLOCK_IMPROVED_HOWL_OF_TERROR != talent && WARLOCK_MALEDICTION != talent && WARLOCK_DEATHS_EMBRACE != talent && WARLOCK_UNSTABLE_AFFLICTION != talent && WARLOCK_PANDEMIC != talent && WARLOCK_EVERLASTING_AFFLICTION != talent && WARLOCK_HAUNT != talent && WARLOCK_IMPROVED_HEALTHSTONE != talent && WARLOCK_IMPROVED_IMP != talent && WARLOCK_DEMONIC_EMBRACE != talent && WARLOCK_FEL_SYNERGY != talent && WARLOCK_IMPROVED_HEALTH_FUNNEL != talent && WARLOCK_DEMONIC_BRUTALITY != talent && WARLOCK_FEL_VITALITY != talent && WARLOCK_IMPROVED_SUCCUBUS != talent && WARLOCK_SOUL_LINK != talent && WARLOCK_FEL_DOMINATION != talent && WARLOCK_DEMONIC_AEGIS != talent && WARLOCK_UNHOLY_POWER != talent && WARLOCK_MASTER_SUMMONER != talent && WARLOCK_MANA_FEED != talent && WARLOCK_MASTER_CONJURER != talent && WARLOCK_MASTER_DEMONOLOGIST != talent && WARLOCK_MOLTEN_CORE != talent && WARLOCK_DEMONIC_RESILIENCE != talent && WARLOCK_DEMONIC_EMPOWERMENT != talent && WARLOCK_DEMONIC_KNOWLEDGE != talent && WARLOCK_DEMONIC_TACTICS != talent && WARLOCK_DECIMATION != talent && WARLOCK_IMPROVED_DEMONIC_TACTICS != talent && WARLOCK_SUMMON_FELGUARD != talent && WARLOCK_NEMESIS != talent && WARLOCK_DEMONIC_PACT != talent && WARLOCK_METAMORPHOSIS != talent)
            return false;
    }
    else if (charClass == CLASS_WARRIOR)
    {
        if (WARRIOR_IMPROVED_HEROIC_STRIKE != talent && WARRIOR_DEFLECTION != talent && WARRIOR_IMPROVED_REND != talent && WARRIOR_IMPROVED_CHARGE != talent && WARRIOR_IRON_WILL != talent && WARRIOR_TACTICAL_MASTERY != talent && WARRIOR_IMPROVED_OVERPOWER != talent && WARRIOR_ANGER_MANAGEMENT != talent && WARRIOR_IMPALE != talent && WARRIOR_DEEP_WOUNDS != talent && WARRIOR_TWOHANDED_WEAPON_SPECIALIZATION != talent && WARRIOR_TASTE_FOR_BLOOD != talent && WARRIOR_POLEAXE_SPECIALIZATION != talent && WARRIOR_SWEEPING_STRIKES != talent && WARRIOR_MACE_SPECIALIZATION != talent && WARRIOR_SWORD_SPECIALIZATION != talent && WARRIOR_WEAPON_MASTERY != talent && WARRIOR_IMPROVED_HAMSTRING != talent && WARRIOR_TRAUMA != talent && WARRIOR_SECOND_WIND != talent && WARRIOR_MORTAL_STRIKE != talent && WARRIOR_STRENGTH_OF_ARMS != talent && WARRIOR_IMPROVED_SLAM != talent && WARRIOR_JUGGERNAUT != talent && WARRIOR_IMPROVED_MORTAL_STRIKE != talent && WARRIOR_UNRELENTING_ASSAULT != talent && WARRIOR_SUDDEN_DEATH != talent && WARRIOR_ENDLESS_RAGE != talent && WARRIOR_BLOOD_FRENZY != talent && WARRIOR_WRECKING_CREW != talent && WARRIOR_BLADESTORM != talent && WARRIOR_IMPROVED_BLOODRAGE != talent && WARRIOR_SHIELD_SPECIALIZATION != talent && WARRIOR_IMPROVED_THUNDER_CLAP != talent && WARRIOR_INCITE != talent && WARRIOR_ANTICIPATION != talent && WARRIOR_LAST_STAND != talent && WARRIOR_IMPROVED_REVENGE != talent && WARRIOR_SHIELD_MASTERY != talent && WARRIOR_TOUGHNESS != talent && WARRIOR_IMPROVED_SPELL_REFLECTION != talent && WARRIOR_IMPROVED_DISARM != talent && WARRIOR_PUNCTURE != talent && WARRIOR_IMPROVED_DISCIPLINES != talent && WARRIOR_CONCUSSION_BLOW != talent && WARRIOR_GAG_ORDER != talent && WARRIOR_ONEHANDED_WEAPON_SPECIALIZATION != talent && WARRIOR_IMPROVED_DEFENSIVE_STANCE != talent && WARRIOR_VIGILANCE != talent && WARRIOR_FOCUSED_RAGE != talent && WARRIOR_VITALITY != talent && WARRIOR_SAFEGUARD != talent && WARRIOR_WARBRINGER != talent && WARRIOR_DEVASTATE != talent && WARRIOR_CRITICAL_BLOCK != talent && WARRIOR_SWORD_AND_BOARD != talent && WARRIOR_DAMAGE_SHIELD != talent && WARRIOR_SHOCKWAVE != talent && WARRIOR_ARMORED_TO_THE_TEETH != talent && WARRIOR_BOOMING_VOICE != talent && WARRIOR_CRUELTY != talent && WARRIOR_IMPROVED_DEMORALIZING_SHOUT != talent && WARRIOR_UNBRIDLED_WRATH != talent && WARRIOR_IMPROVED_CLEAVE != talent && WARRIOR_PIERCING_HOWL != talent && WARRIOR_BLOOD_CRAZE != talent && WARRIOR_COMMANDING_PRESENCE != talent && WARRIOR_DUAL_WIELD_SPECIALIZATION != talent && WARRIOR_IMPROVED_EXECUTE != talent && WARRIOR_ENRAGE != talent && WARRIOR_PRECISION != talent && WARRIOR_DEATH_WISH != talent && WARRIOR_IMPROVED_INTERCEPT != talent && WARRIOR_IMPROVED_BERSERKER_RAGE != talent && WARRIOR_FLURRY != talent && WARRIOR_INTENSIFY_RAGE != talent && WARRIOR_BLOODTHIRST != talent && WARRIOR_IMPROVED_WHIRLWIND != talent && WARRIOR_FURIOUS_ATTACKS != talent && WARRIOR_IMPROVED_BERSERKER_STANCE != talent && WARRIOR_HEROIC_FURY != talent && WARRIOR_RAMPAGE != talent && WARRIOR_BLOODSURGE != talent && WARRIOR_UNENDING_FURY != talent && WARRIOR_TITANS_GRIP != talent)
            return false;
    }
    else if (charClass == CLASS_PET_CUNNING)
    {
        if (PET_CUNNING_COBRA_REFLEXES != talent && PET_CUNNING_DASHDIVE1 != talent && PET_CUNNING_DASHDIVE2 != talent && PET_CUNNING_GREAT_STAMINA != talent && PET_CUNNING_NATURAL_ARMOR != talent && PET_CUNNING_BOARS_SPEED != talent && PET_CUNNING_MOBILITY1 != talent && PET_CUNNING_MOBILITY2 != talent && PET_CUNNING_OWLS_FOCUS != talent && PET_CUNNING_SPIKED_COLLAR != talent && PET_CUNNING_CULLING_THE_HERD != talent && PET_CUNNING_LIONHEARTED != talent && PET_CUNNING_CARRION_FEEDER != talent && PET_CUNNING_GREAT_RESISTANCE != talent && PET_CUNNING_CORNERED != talent && PET_CUNNING_FEEDING_FRENZY != talent && PET_CUNNING_WOLVERINE_BITE != talent && PET_CUNNING_ROAR_OF_RECOVERY != talent && PET_CUNNING_BULLHEADED != talent && PET_CUNNING_GRACE_OF_THE_MANTIS != talent && PET_CUNNING_WILD_HUNT != talent && PET_CUNNING_ROAR_OF_SACRIFICE != talent)
            return false;
    }
    else if (charClass == CLASS_PET_FEROCITY)
    {
        if (PET_FEROCITY_COBRA_REFLEXES != talent && PET_FEROCITY_DASHDIVE1 != talent && PET_FEROCITY_DASHDIVE2 != talent && PET_FEROCITY_GREAT_STAMINA != talent && PET_FEROCITY_NATURAL_ARMOR != talent && PET_FEROCITY_IMPROVED_COWER != talent && PET_FEROCITY_BLOODTHIRSTY != talent && PET_FEROCITY_SPIKED_COLLAR != talent && PET_FEROCITY_BOARS_SPEED != talent && PET_FEROCITY_CULLING_THE_HERD != talent && PET_FEROCITY_LIONHEARTED != talent && PET_FEROCITY_CHARGESWOOP1 != talent && PET_FEROCITY_CHARGESWOOP2 != talent && PET_FEROCITY_HEART_OF_THE_PHOENIX != talent && PET_FEROCITY_SPIDERS_BITE != talent && PET_FEROCITY_GREAT_RESISTANCE != talent && PET_FEROCITY_RABID != talent && PET_FEROCITY_LICK_YOUR_WOUNDS != talent && PET_FEROCITY_CALL_OF_THE_WILD != talent && PET_FEROCITY_SHARK_ATTACK != talent && PET_FEROCITY_WILD_HUNT != talent)
            return false;
    }
    else if (charClass == CLASS_PET_TENACITY)
    {
        if (PET_TENACITY_COBRA_REFLEXES != talent && PET_TENACITY_CHARGE != talent && PET_TENACITY_GREAT_STAMINA != talent && PET_TENACITY_NATURAL_ARMOR != talent && PET_TENACITY_SPIKED_COLLAR != talent && PET_TENACITY_BOARS_SPEED != talent && PET_TENACITY_BLOOD_OF_THE_RHINO != talent && PET_TENACITY_PET_BARDING != talent && PET_TENACITY_CULLING_THE_HERD != talent && PET_TENACITY_GUARD_DOG != talent && PET_TENACITY_LIONHEARTED != talent && PET_TENACITY_THUNDERSTOMP != talent && PET_TENACITY_GRACE_OF_THE_MANTIS != talent && PET_TENACITY_GREAT_RESISTANCE != talent && PET_TENACITY_LAST_STAND != talent && PET_TENACITY_TAUNT != talent && PET_TENACITY_ROAR_OF_SACRIFICE != talent && PET_TENACITY_INTERVENE != talent && PET_TENACITY_SILVERBACK != talent && PET_TENACITY_WILD_HUNT != talent)
            return false;
    }
    else // charClass unknown
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: Someone was naughty and supplied an invalid class to ValidateTalent: %u", (uint32) charClass);
        return false;
    }

    return true;
}

/**
 * ValidateGlyph tests a glyph against class to see if it belongs to that class - accepts both Major and Minor glyphs
 *
 * uint16 glyph:        glyph ID
 * long charClass:    member of the Classes enum or ClassesCombatPets enum
 *
 * return true  -> ok
 * return false -> not a valid glyph for that class
 */
bool PlayerbotAI::ValidateGlyph(uint16 glyph, long charClass)
{
    // XOR the two helper functions. Both true (supposedly impossible) or both false -> false
    return ValidateMajorGlyph(glyph, charClass) ^ ValidateMinorGlyph(glyph, charClass);
}

/**
 * ValidateMajorGlyph tests a glyph against class to see if it belongs to that class - only accepts Major glyphs
 *
 * uint16 glyph:        glyph ID
 * long charClass:    member of the Classes enum or ClassesCombatPets enum
 *
 * return true  -> ok
 * return false -> not a valid major glyph for that class
 */
bool PlayerbotAI::ValidateMajorGlyph(uint16 glyph, long charClass)
{
    if (charClass == CLASS_DEATH_KNIGHT)
    {
        // this looong 'if' is to see if any glyph is not a Death Knight glyph when the class clearly is
        if (DEATH_KNIGHT_MAJOR_GLYPH_OF_DARK_COMMAND != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_ANTIMAGIC_SHELL != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_HEART_STRIKE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_BONE_SHIELD != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_CHAINS_OF_ICE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_DEATH_GRIP != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_DEATH_AND_DECAY != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_FROST_STRIKE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_ICEBOUND_FORTITUDE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_ICY_TOUCH != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_OBLITERATE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_PLAGUE_STRIKE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_THE_GHOUL != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_RUNE_STRIKE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_SCOURGE_STRIKE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_STRANGULATE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_UNBREAKABLE_ARMOR != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_VAMPIRIC_BLOOD != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_RUNE_TAP != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_BLOOD_STRIKE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_DEATH_STRIKE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_DANCING_RUNE_WEAPON != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_HUNGERING_COLD != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_UNHOLY_BLIGHT != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_DARK_DEATH != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_DISEASE != glyph && DEATH_KNIGHT_MAJOR_GLYPH_OF_HOWLING_BLAST != glyph)
            return false;
    }
    else if (charClass == CLASS_DRUID)
    {
        if (DRUID_MAJOR_GLYPH_OF_FRENZIED_REGENERATION != glyph && DRUID_MAJOR_GLYPH_OF_MAUL != glyph && DRUID_MAJOR_GLYPH_OF_MANGLE != glyph && DRUID_MAJOR_GLYPH_OF_SHRED != glyph && DRUID_MAJOR_GLYPH_OF_RIP != glyph && DRUID_MAJOR_GLYPH_OF_RAKE != glyph && DRUID_MAJOR_GLYPH_OF_SWIFTMEND != glyph && DRUID_MAJOR_GLYPH_OF_INNERVATE != glyph && DRUID_MAJOR_GLYPH_OF_REBIRTH != glyph && DRUID_MAJOR_GLYPH_OF_REGROWTH != glyph && DRUID_MAJOR_GLYPH_OF_REJUVENATION != glyph && DRUID_MAJOR_GLYPH_OF_HEALING_TOUCH != glyph && DRUID_MAJOR_GLYPH_OF_LIFEBLOOM != glyph && DRUID_MAJOR_GLYPH_OF_STARFIRE != glyph && DRUID_MAJOR_GLYPH_OF_INSECT_SWARM != glyph && DRUID_MAJOR_GLYPH_OF_HURRICANE != glyph && DRUID_MAJOR_GLYPH_OF_STARFALL != glyph && DRUID_MAJOR_GLYPH_OF_WRATH != glyph && DRUID_MAJOR_GLYPH_OF_MOONFIRE != glyph && DRUID_MAJOR_GLYPH_OF_ENTANGLING_ROOTS != glyph && DRUID_MAJOR_GLYPH_OF_FOCUS != glyph && DRUID_MAJOR_GLYPH_OF_BERSERK != glyph && DRUID_MAJOR_GLYPH_OF_WILD_GROWTH != glyph && DRUID_MAJOR_GLYPH_OF_NOURISH != glyph && DRUID_MAJOR_GLYPH_OF_SAVAGE_ROAR != glyph && DRUID_MAJOR_GLYPH_OF_MONSOON != glyph && DRUID_MAJOR_GLYPH_OF_BARKSKIN != glyph && DRUID_MAJOR_GLYPH_OF_SURVIVAL_INSTINCTS != glyph && DRUID_MAJOR_GLYPH_OF_CLAW != glyph && DRUID_MAJOR_GLYPH_OF_RAPID_REJUVENATION != glyph)
            return false;
    }
    else if (charClass == CLASS_HUNTER)
    {
        if (HUNTER_MAJOR_GLYPH_OF_AIMED_SHOT != glyph && HUNTER_MAJOR_GLYPH_OF_ARCANE_SHOT != glyph && HUNTER_MAJOR_GLYPH_OF_THE_BEAST != glyph && HUNTER_MAJOR_GLYPH_OF_MENDING != glyph && HUNTER_MAJOR_GLYPH_OF_ASPECT_OF_THE_VIPER != glyph && HUNTER_MAJOR_GLYPH_OF_BESTIAL_WRATH != glyph && HUNTER_MAJOR_GLYPH_OF_DETERRENCE != glyph && HUNTER_MAJOR_GLYPH_OF_DISENGAGE != glyph && HUNTER_MAJOR_GLYPH_OF_FREEZING_TRAP != glyph && HUNTER_MAJOR_GLYPH_OF_FROST_TRAP != glyph && HUNTER_MAJOR_GLYPH_OF_HUNTERS_MARK != glyph && HUNTER_MAJOR_GLYPH_OF_IMMOLATION_TRAP != glyph && HUNTER_MAJOR_GLYPH_OF_MULTISHOT != glyph && HUNTER_MAJOR_GLYPH_OF_RAPID_FIRE != glyph && HUNTER_MAJOR_GLYPH_OF_SERPENT_STING != glyph && HUNTER_MAJOR_GLYPH_OF_SNAKE_TRAP != glyph && HUNTER_MAJOR_GLYPH_OF_STEADY_SHOT != glyph && HUNTER_MAJOR_GLYPH_OF_TRUESHOT_AURA != glyph && HUNTER_MAJOR_GLYPH_OF_VOLLEY != glyph && HUNTER_MAJOR_GLYPH_OF_WYVERN_STING != glyph && HUNTER_MAJOR_GLYPH_OF_CHIMERA_SHOT != glyph && HUNTER_MAJOR_GLYPH_OF_EXPLOSIVE_SHOT != glyph && HUNTER_MAJOR_GLYPH_OF_KILL_SHOT != glyph && HUNTER_MAJOR_GLYPH_OF_EXPLOSIVE_TRAP != glyph && HUNTER_MAJOR_GLYPH_OF_SCATTER_SHOT != glyph && HUNTER_MAJOR_GLYPH_OF_RAPTOR_STRIKE != glyph)
            return false;
    }
    else if (charClass == CLASS_MAGE)
    {
        if (MAGE_MAJOR_GLYPH_OF_ARCANE_EXPLOSION != glyph && MAGE_MAJOR_GLYPH_OF_ARCANE_MISSILES != glyph && MAGE_MAJOR_GLYPH_OF_ARCANE_POWER != glyph && MAGE_MAJOR_GLYPH_OF_BLINK != glyph && MAGE_MAJOR_GLYPH_OF_EVOCATION != glyph && MAGE_MAJOR_GLYPH_OF_FIREBALL != glyph && MAGE_MAJOR_GLYPH_OF_FIRE_BLAST != glyph && MAGE_MAJOR_GLYPH_OF_FROST_NOVA != glyph && MAGE_MAJOR_GLYPH_OF_FROSTBOLT != glyph && MAGE_MAJOR_GLYPH_OF_ICE_ARMOR != glyph && MAGE_MAJOR_GLYPH_OF_ICE_BLOCK != glyph && MAGE_MAJOR_GLYPH_OF_ICE_LANCE != glyph && MAGE_MAJOR_GLYPH_OF_ICY_VEINS != glyph && MAGE_MAJOR_GLYPH_OF_SCORCH != glyph && MAGE_MAJOR_GLYPH_OF_INVISIBILITY != glyph && MAGE_MAJOR_GLYPH_OF_MAGE_ARMOR != glyph && MAGE_MAJOR_GLYPH_OF_MANA_GEM != glyph && MAGE_MAJOR_GLYPH_OF_MOLTEN_ARMOR != glyph && MAGE_MAJOR_GLYPH_OF_POLYMORPH != glyph && MAGE_MAJOR_GLYPH_OF_REMOVE_CURSE != glyph && MAGE_MAJOR_GLYPH_OF_WATER_ELEMENTAL != glyph && MAGE_MAJOR_GLYPH_OF_FROSTFIRE != glyph && MAGE_MAJOR_GLYPH_OF_ARCANE_BLAST != glyph && MAGE_MAJOR_GLYPH_OF_DEEP_FREEZE != glyph && MAGE_MAJOR_GLYPH_OF_LIVING_BOMB != glyph && MAGE_MAJOR_GLYPH_OF_ARCANE_BARRAGE != glyph && MAGE_MAJOR_GLYPH_OF_MIRROR_IMAGE != glyph && MAGE_MAJOR_GLYPH_OF_ICE_BARRIER != glyph && MAGE_MAJOR_GLYPH_OF_ETERNAL_WATER != glyph)
            return false;
    }
    else if (charClass == CLASS_PALADIN)
    {
        if (PALADIN_MAJOR_GLYPH_OF_JUDGEMENT != glyph && PALADIN_MAJOR_GLYPH_OF_SEAL_OF_COMMAND != glyph && PALADIN_MAJOR_GLYPH_OF_HAMMER_OF_JUSTICE != glyph && PALADIN_MAJOR_GLYPH_OF_SPIRITUAL_ATTUNEMENT != glyph && PALADIN_MAJOR_GLYPH_OF_HAMMER_OF_WRATH != glyph && PALADIN_MAJOR_GLYPH_OF_CRUSADER_STRIKE != glyph && PALADIN_MAJOR_GLYPH_OF_CONSECRATION != glyph && PALADIN_MAJOR_GLYPH_OF_RIGHTEOUS_DEFENSE != glyph && PALADIN_MAJOR_GLYPH_OF_AVENGERS_SHIELD != glyph && PALADIN_MAJOR_GLYPH_OF_TURN_EVIL != glyph && PALADIN_MAJOR_GLYPH_OF_EXORCISM != glyph && PALADIN_MAJOR_GLYPH_OF_CLEANSING != glyph && PALADIN_MAJOR_GLYPH_OF_FLASH_OF_LIGHT != glyph && PALADIN_MAJOR_GLYPH_OF_HOLY_LIGHT != glyph && PALADIN_MAJOR_GLYPH_OF_AVENGING_WRATH != glyph && PALADIN_MAJOR_GLYPH_OF_DIVINITY != glyph && PALADIN_MAJOR_GLYPH_OF_SEAL_OF_WISDOM != glyph && PALADIN_MAJOR_GLYPH_OF_SEAL_OF_LIGHT != glyph && PALADIN_MAJOR_GLYPH_OF_HOLY_WRATH != glyph && PALADIN_MAJOR_GLYPH_OF_SEAL_OF_RIGHTEOUSNESS != glyph && PALADIN_MAJOR_GLYPH_OF_SEAL_OF_VENGEANCE != glyph && PALADIN_MAJOR_GLYPH_OF_BEACON_OF_LIGHT != glyph && PALADIN_MAJOR_GLYPH_OF_HAMMER_OF_THE_RIGHTEOUS != glyph && PALADIN_MAJOR_GLYPH_OF_DIVINE_STORM != glyph && PALADIN_MAJOR_GLYPH_OF_SHIELD_OF_RIGHTEOUSNESS != glyph && PALADIN_MAJOR_GLYPH_OF_DIVINE_PLEA != glyph && PALADIN_MAJOR_GLYPH_OF_HOLY_SHOCK != glyph && PALADIN_MAJOR_GLYPH_OF_SALVATION != glyph)
            return false;
    }
    else if (charClass == CLASS_PRIEST)
    {
        if (PRIEST_MAJOR_GLYPH_OF_CIRCLE_OF_HEALING != glyph && PRIEST_MAJOR_GLYPH_OF_DISPEL_MAGIC != glyph && PRIEST_MAJOR_GLYPH_OF_FADE != glyph && PRIEST_MAJOR_GLYPH_OF_FEAR_WARD != glyph && PRIEST_MAJOR_GLYPH_OF_FLASH_HEAL != glyph && PRIEST_MAJOR_GLYPH_OF_HOLY_NOVA != glyph && PRIEST_MAJOR_GLYPH_OF_INNER_FIRE != glyph && PRIEST_MAJOR_GLYPH_OF_LIGHTWELL != glyph && PRIEST_MAJOR_GLYPH_OF_MASS_DISPEL != glyph && PRIEST_MAJOR_GLYPH_OF_MIND_CONTROL != glyph && PRIEST_MAJOR_GLYPH_OF_SHADOW_WORD_PAIN != glyph && PRIEST_MAJOR_GLYPH_OF_SHADOW != glyph && PRIEST_MAJOR_GLYPH_OF_POWER_WORD_SHIELD != glyph && PRIEST_MAJOR_GLYPH_OF_PRAYER_OF_HEALING != glyph && PRIEST_MAJOR_GLYPH_OF_PSYCHIC_SCREAM != glyph && PRIEST_MAJOR_GLYPH_OF_RENEW != glyph && PRIEST_MAJOR_GLYPH_OF_SCOURGE_IMPRISONMENT != glyph && PRIEST_MAJOR_GLYPH_OF_SHADOW_WORD_DEATH != glyph && PRIEST_MAJOR_GLYPH_OF_MIND_FLAY != glyph && PRIEST_MAJOR_GLYPH_OF_SMITE != glyph && PRIEST_MAJOR_GLYPH_OF_SPIRIT_OF_REDEMPTION != glyph && PRIEST_MAJOR_GLYPH_OF_DISPERSION != glyph && PRIEST_MAJOR_GLYPH_OF_GUARDIAN_SPIRIT != glyph && PRIEST_MAJOR_GLYPH_OF_PENANCE != glyph && PRIEST_MAJOR_GLYPH_OF_MIND_SEAR != glyph && PRIEST_MAJOR_GLYPH_OF_HYMN_OF_HOPE != glyph && PRIEST_MAJOR_GLYPH_OF_PAIN_SUPPRESSION != glyph)
            return false;
    }
    else if (charClass == CLASS_ROGUE)
    {
        if (ROGUE_MAJOR_GLYPH_OF_ADRENALINE_RUSH != glyph && ROGUE_MAJOR_GLYPH_OF_AMBUSH != glyph && ROGUE_MAJOR_GLYPH_OF_BACKSTAB != glyph && ROGUE_MAJOR_GLYPH_OF_BLADE_FLURRY != glyph && ROGUE_MAJOR_GLYPH_OF_CRIPPLING_POISON != glyph && ROGUE_MAJOR_GLYPH_OF_DEADLY_THROW != glyph && ROGUE_MAJOR_GLYPH_OF_EVASION != glyph && ROGUE_MAJOR_GLYPH_OF_EVISCERATE != glyph && ROGUE_MAJOR_GLYPH_OF_EXPOSE_ARMOR != glyph && ROGUE_MAJOR_GLYPH_OF_FEINT != glyph && ROGUE_MAJOR_GLYPH_OF_GARROTE != glyph && ROGUE_MAJOR_GLYPH_OF_GHOSTLY_STRIKE != glyph && ROGUE_MAJOR_GLYPH_OF_GOUGE != glyph && ROGUE_MAJOR_GLYPH_OF_HEMORRHAGE != glyph && ROGUE_MAJOR_GLYPH_OF_PREPARATION != glyph && ROGUE_MAJOR_GLYPH_OF_RUPTURE != glyph && ROGUE_MAJOR_GLYPH_OF_SAP != glyph && ROGUE_MAJOR_GLYPH_OF_VIGOR != glyph && ROGUE_MAJOR_GLYPH_OF_SINISTER_STRIKE != glyph && ROGUE_MAJOR_GLYPH_OF_SLICE_AND_DICE != glyph && ROGUE_MAJOR_GLYPH_OF_SPRINT != glyph && ROGUE_MAJOR_GLYPH_OF_HUNGER_FOR_BLOOD != glyph && ROGUE_MAJOR_GLYPH_OF_KILLING_SPREE != glyph && ROGUE_MAJOR_GLYPH_OF_SHADOW_DANCE != glyph && ROGUE_MAJOR_GLYPH_OF_FAN_OF_KNIVES != glyph && ROGUE_MAJOR_GLYPH_OF_TRICKS_OF_THE_TRADE != glyph && ROGUE_MAJOR_GLYPH_OF_MUTILATE != glyph && ROGUE_MAJOR_GLYPH_OF_CLOAK_OF_SHADOWS != glyph)
            return false;
    }
    else if (charClass == CLASS_SHAMAN)
    {
        if (SHAMAN_MAJOR_GLYPH_OF_WATER_MASTERY != glyph && SHAMAN_MAJOR_GLYPH_OF_CHAIN_HEAL != glyph && SHAMAN_MAJOR_GLYPH_OF_CHAIN_LIGHTNING != glyph && SHAMAN_MAJOR_GLYPH_OF_LAVA != glyph && SHAMAN_MAJOR_GLYPH_OF_SHOCKING != glyph && SHAMAN_MAJOR_GLYPH_OF_EARTHLIVING_WEAPON != glyph && SHAMAN_MAJOR_GLYPH_OF_FIRE_ELEMENTAL_TOTEM != glyph && SHAMAN_MAJOR_GLYPH_OF_FIRE_NOVA != glyph && SHAMAN_MAJOR_GLYPH_OF_FLAME_SHOCK != glyph && SHAMAN_MAJOR_GLYPH_OF_FLAMETONGUE_WEAPON != glyph && SHAMAN_MAJOR_GLYPH_OF_FROST_SHOCK != glyph && SHAMAN_MAJOR_GLYPH_OF_HEALING_STREAM_TOTEM != glyph && SHAMAN_MAJOR_GLYPH_OF_HEALING_WAVE != glyph && SHAMAN_MAJOR_GLYPH_OF_LESSER_HEALING_WAVE != glyph && SHAMAN_MAJOR_GLYPH_OF_LIGHTNING_SHIELD != glyph && SHAMAN_MAJOR_GLYPH_OF_LIGHTNING_BOLT != glyph && SHAMAN_MAJOR_GLYPH_OF_STORMSTRIKE != glyph && SHAMAN_MAJOR_GLYPH_OF_LAVA_LASH != glyph && SHAMAN_MAJOR_GLYPH_OF_ELEMENTAL_MASTERY != glyph && SHAMAN_MAJOR_GLYPH_OF_WINDFURY_WEAPON != glyph && SHAMAN_MAJOR_GLYPH_OF_THUNDER != glyph && SHAMAN_MAJOR_GLYPH_OF_FERAL_SPIRIT != glyph && SHAMAN_MAJOR_GLYPH_OF_RIPTIDE != glyph && SHAMAN_MAJOR_GLYPH_OF_EARTH_SHIELD != glyph && SHAMAN_MAJOR_GLYPH_OF_TOTEM_OF_WRATH != glyph && SHAMAN_MAJOR_GLYPH_OF_HEX != glyph && SHAMAN_MAJOR_GLYPH_OF_STONECLAW_TOTEM != glyph)
            return false;
    }
    else if (charClass == CLASS_WARLOCK)
    {
        if (WARLOCK_MAJOR_GLYPH_OF_INCINERATE != glyph && WARLOCK_MAJOR_GLYPH_OF_CONFLAGRATE != glyph && WARLOCK_MAJOR_GLYPH_OF_CORRUPTION != glyph && WARLOCK_MAJOR_GLYPH_OF_CURSE_OF_AGONY != glyph && WARLOCK_MAJOR_GLYPH_OF_DEATH_COIL != glyph && WARLOCK_MAJOR_GLYPH_OF_FEAR != glyph && WARLOCK_MAJOR_GLYPH_OF_FELGUARD != glyph && WARLOCK_MAJOR_GLYPH_OF_FELHUNTER != glyph && WARLOCK_MAJOR_GLYPH_OF_HEALTH_FUNNEL != glyph && WARLOCK_MAJOR_GLYPH_OF_HEALTHSTONE != glyph && WARLOCK_MAJOR_GLYPH_OF_HOWL_OF_TERROR != glyph && WARLOCK_MAJOR_GLYPH_OF_IMMOLATE != glyph && WARLOCK_MAJOR_GLYPH_OF_IMP != glyph && WARLOCK_MAJOR_GLYPH_OF_SEARING_PAIN != glyph && WARLOCK_MAJOR_GLYPH_OF_SHADOW_BOLT != glyph && WARLOCK_MAJOR_GLYPH_OF_SHADOWBURN != glyph && WARLOCK_MAJOR_GLYPH_OF_SIPHON_LIFE != glyph && WARLOCK_MAJOR_GLYPH_OF_SOULSTONE != glyph && WARLOCK_MAJOR_GLYPH_OF_SUCCUBUS != glyph && WARLOCK_MAJOR_GLYPH_OF_UNSTABLE_AFFLICTION != glyph && WARLOCK_MAJOR_GLYPH_OF_VOIDWALKER != glyph && WARLOCK_MAJOR_GLYPH_OF_HAUNT != glyph && WARLOCK_MAJOR_GLYPH_OF_METAMORPHOSIS != glyph && WARLOCK_MAJOR_GLYPH_OF_CHAOS_BOLT != glyph && WARLOCK_MAJOR_GLYPH_OF_DEMONIC_CIRCLE != glyph && WARLOCK_MAJOR_GLYPH_OF_SHADOWFLAME != glyph && WARLOCK_MAJOR_GLYPH_OF_LIFE_TAP != glyph && WARLOCK_MAJOR_GLYPH_OF_SOUL_LINK != glyph && WARLOCK_MAJOR_GLYPH_OF_QUICK_DECAY != glyph)
            return false;
    }
    else if (charClass == CLASS_WARRIOR)
    {
        if (WARRIOR_MAJOR_GLYPH_OF_MORTAL_STRIKE != glyph && WARRIOR_MAJOR_GLYPH_OF_BLOODTHIRST != glyph && WARRIOR_MAJOR_GLYPH_OF_RAPID_CHARGE != glyph && WARRIOR_MAJOR_GLYPH_OF_CLEAVING != glyph && WARRIOR_MAJOR_GLYPH_OF_DEVASTATE != glyph && WARRIOR_MAJOR_GLYPH_OF_EXECUTION != glyph && WARRIOR_MAJOR_GLYPH_OF_HAMSTRING != glyph && WARRIOR_MAJOR_GLYPH_OF_HEROIC_STRIKE != glyph && WARRIOR_MAJOR_GLYPH_OF_INTERVENE != glyph && WARRIOR_MAJOR_GLYPH_OF_BARBARIC_INSULTS != glyph && WARRIOR_MAJOR_GLYPH_OF_OVERPOWER != glyph && WARRIOR_MAJOR_GLYPH_OF_RENDING != glyph && WARRIOR_MAJOR_GLYPH_OF_REVENGE != glyph && WARRIOR_MAJOR_GLYPH_OF_BLOCKING != glyph && WARRIOR_MAJOR_GLYPH_OF_LAST_STAND != glyph && WARRIOR_MAJOR_GLYPH_OF_SUNDER_ARMOR != glyph && WARRIOR_MAJOR_GLYPH_OF_SWEEPING_STRIKES != glyph && WARRIOR_MAJOR_GLYPH_OF_TAUNT != glyph && WARRIOR_MAJOR_GLYPH_OF_RESONATING_POWER != glyph && WARRIOR_MAJOR_GLYPH_OF_VICTORY_RUSH != glyph && WARRIOR_MAJOR_GLYPH_OF_WHIRLWIND != glyph && WARRIOR_MAJOR_GLYPH_OF_BLADESTORM != glyph && WARRIOR_MAJOR_GLYPH_OF_SHOCKWAVE != glyph && WARRIOR_MAJOR_GLYPH_OF_VIGILANCE != glyph && WARRIOR_MAJOR_GLYPH_OF_ENRAGED_REGENERATION != glyph && WARRIOR_MAJOR_GLYPH_OF_SPELL_REFLECTION != glyph && WARRIOR_MAJOR_GLYPH_OF_SHIELD_WALL != glyph)
            return false;
    }
    // pets don't have glyphs... yet
    else if (charClass == CLASS_PET_CUNNING || charClass == CLASS_PET_FEROCITY || charClass == CLASS_PET_TENACITY)
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: Someone tried to validate a glyph for a pet... ValidateMajorGlyph: %u", (uint32) charClass);
        return false;
    }
    else // charClass unknown
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: Someone was naughty and supplied an invalid class to ValidateMajorGlyph: %u", (uint32) charClass);
        return false;
    }

    return true;
}

/**
 * ValidateMinorGlyph tests a glyph against class to see if it belongs to that class - only accepts Minor glyphs
 *
 * uint16 glyph:        glyph ID
 * long charClass:    member of the Classes enum or ClassesCombatPets enum
 *
 * return true  -> ok
 * return false -> not a valid minor glyph for that class
 */
bool PlayerbotAI::ValidateMinorGlyph(uint16 glyph, long charClass)
{
    if (charClass == CLASS_DEATH_KNIGHT)
    {
        // this looong 'if' is to see if any glyph is not a Death Knight glyph when the class clearly is
        if (DEATH_KNIGHT_MINOR_GLYPH_OF_BLOOD_TAP != glyph && DEATH_KNIGHT_MINOR_GLYPH_OF_DEATHS_EMBRACE != glyph && DEATH_KNIGHT_MINOR_GLYPH_OF_HORN_OF_WINTER != glyph && DEATH_KNIGHT_MINOR_GLYPH_OF_PESTILENCE != glyph && DEATH_KNIGHT_MINOR_GLYPH_OF_CORPSE_EXPLOSION != glyph && DEATH_KNIGHT_MINOR_GLYPH_OF_RAISE_DEAD != glyph && DEATH_KNIGHT_MINOR_GLYPH_OF_RAISE_DEAD2 != glyph)
            return false;
    }
    else if (charClass == CLASS_DRUID)
    {
        if (DRUID_MINOR_GLYPH_OF_AQUATIC_FORM != glyph && DRUID_MINOR_GLYPH_OF_CHALLENGING_ROAR != glyph && DRUID_MINOR_GLYPH_OF_THE_WILD != glyph && DRUID_MINOR_GLYPH_OF_UNBURDENED_REBIRTH != glyph && DRUID_MINOR_GLYPH_OF_THORNS != glyph && DRUID_MINOR_GLYPH_OF_DASH != glyph && DRUID_MINOR_GLYPH_OF_TYPHOON != glyph)
            return false;
    }
    else if (charClass == CLASS_HUNTER)
    {
        if (HUNTER_MINOR_GLYPH_OF_REVIVE_PET != glyph && HUNTER_MINOR_GLYPH_OF_MEND_PET != glyph && HUNTER_MINOR_GLYPH_OF_FEIGN_DEATH != glyph && HUNTER_MINOR_GLYPH_OF_SCARE_BEAST != glyph && HUNTER_MINOR_GLYPH_OF_THE_PACK != glyph && HUNTER_MINOR_GLYPH_OF_POSSESSED_STRENGTH != glyph)
            return false;
    }
    else if (charClass == CLASS_MAGE)
    {
        if (MAGE_MINOR_GLYPH_OF_ARCANE_INTELLECT != glyph && MAGE_MINOR_GLYPH_OF_BLAST_WAVE != glyph && MAGE_MINOR_GLYPH_OF_FIRE_WARD != glyph && MAGE_MINOR_GLYPH_OF_FROST_WARD != glyph && MAGE_MINOR_GLYPH_OF_FROST_ARMOR != glyph && MAGE_MINOR_GLYPH_OF_THE_PENGUIN != glyph && MAGE_MINOR_GLYPH_OF_SLOW_FALL != glyph)
            return false;
    }
    else if (charClass == CLASS_PALADIN)
    {
        if (PALADIN_MINOR_GLYPH_OF_BLESSING_OF_KINGS != glyph && PALADIN_MINOR_GLYPH_OF_BLESSING_OF_MIGHT != glyph && PALADIN_MINOR_GLYPH_OF_BLESSING_OF_WISDOM != glyph && PALADIN_MINOR_GLYPH_OF_LAY_ON_HANDS != glyph && PALADIN_MINOR_GLYPH_OF_SENSE_UNDEAD != glyph && PALADIN_MINOR_GLYPH_OF_THE_WISE != glyph)
            return false;
    }
    else if (charClass == CLASS_PRIEST)
    {
        if (PRIEST_MINOR_GLYPH_OF_FADING != glyph && PRIEST_MINOR_GLYPH_OF_LEVITATE != glyph && PRIEST_MINOR_GLYPH_OF_FORTITUDE != glyph && PRIEST_MINOR_GLYPH_OF_SHACKLE_UNDEAD != glyph && PRIEST_MINOR_GLYPH_OF_SHADOW_PROTECTION != glyph && PRIEST_MINOR_GLYPH_OF_SHADOWFIEND != glyph)
            return false;
    }
    else if (charClass == CLASS_ROGUE)
    {
        if (ROGUE_MINOR_GLYPH_OF_DISTRACT != glyph && ROGUE_MINOR_GLYPH_OF_PICK_LOCK != glyph && ROGUE_MINOR_GLYPH_OF_PICK_POCKET != glyph && ROGUE_MINOR_GLYPH_OF_SAFE_FALL != glyph && ROGUE_MINOR_GLYPH_OF_BLURRED_SPEED != glyph && ROGUE_MINOR_GLYPH_OF_VANISH != glyph)
            return false;
    }
    else if (charClass == CLASS_SHAMAN)
    {
        if (SHAMAN_MINOR_GLYPH_OF_ASTRAL_RECALL != glyph && SHAMAN_MINOR_GLYPH_OF_RENEWED_LIFE != glyph && SHAMAN_MINOR_GLYPH_OF_WATER_BREATHING != glyph && SHAMAN_MINOR_GLYPH_OF_WATER_SHIELD != glyph && SHAMAN_MINOR_GLYPH_OF_WATER_WALKING != glyph && SHAMAN_MINOR_GLYPH_OF_GHOST_WOLF != glyph && SHAMAN_MINOR_GLYPH_OF_THUNDERSTORM != glyph)
            return false;
    }
    else if (charClass == CLASS_WARLOCK)
    {
        if (WARLOCK_MINOR_GLYPH_OF_UNENDING_BREATH != glyph && WARLOCK_MINOR_GLYPH_OF_DRAIN_SOUL != glyph && WARLOCK_MINOR_GLYPH_OF_KILROGG != glyph && WARLOCK_MINOR_GLYPH_OF_ENSLAVE_DEMON != glyph && WARLOCK_MINOR_GLYPH_OF_SOULS != glyph)
            return false;
    }
    else if (charClass == CLASS_WARRIOR)
    {
        if (WARRIOR_MINOR_GLYPH_OF_BATTLE != glyph && WARRIOR_MINOR_GLYPH_OF_BLOODRAGE != glyph && WARRIOR_MINOR_GLYPH_OF_CHARGE != glyph && WARRIOR_MINOR_GLYPH_OF_MOCKING_BLOW != glyph && WARRIOR_MINOR_GLYPH_OF_THUNDER_CLAP != glyph && WARRIOR_MINOR_GLYPH_OF_ENDURING_VICTORY != glyph && WARRIOR_MINOR_GLYPH_OF_COMMAND != glyph)
            return false;
    }
    // pets don't have glyphs... yet
    else if (charClass == CLASS_PET_CUNNING || charClass == CLASS_PET_FEROCITY || charClass == CLASS_PET_TENACITY)
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: Someone tried to validate a glyph for a pet... ValidateMinorGlyph: %u", (uint32) charClass);
        return false;
    }
    else // charClass unknown
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: Someone was naughty and supplied an invalid class to ValidateMinorGlyph: %u", (uint32) charClass);
        return false;
    }

    return true;
}

void PlayerbotAI::extractQuestIds(const std::string& text, std::list<uint32>& questIds) const
{
    uint8 pos = 0;
    while (true)
    {
        int i = text.find("Hquest:", pos);
        if (i == -1)
            break;
        pos = i + 7;
        int endPos = text.find(':', pos);
        if (endPos == -1)
            break;
        std::string idC = text.substr(pos, endPos - pos);
        uint32 id = atol(idC.c_str());
        pos = endPos;
        if (id)
            questIds.push_back(id);
    }
}

// Build an hlink for Weapon skills in Aqua
void PlayerbotAI::MakeWeaponSkillLink(const SpellEntry *sInfo, std::ostringstream &out, uint32 skillid)
{
    int loc = m_master->GetSession()->GetSessionDbcLocale();
    out << "|cff00ffff|Hspell:" << sInfo->Id << "|h[" << sInfo->SpellName[loc] << " : " << m_bot->GetSkillValue(skillid) << " /" << m_bot->GetMaxSkillValue(skillid) << "]|h|r";
}

// Build an hlink for spells in White
void PlayerbotAI::MakeSpellLink(const SpellEntry *sInfo, std::ostringstream &out)
{
    int loc = m_master->GetSession()->GetSessionDbcLocale();
    out << "|cffffffff|Hspell:" << sInfo->Id << "|h[" << sInfo->SpellName[loc] << "]|h|r";
}

// Builds a hlink for an item, but since its
// only a ItemTemplate, we cant fill in everything
void PlayerbotAI::MakeItemLink(const ItemTemplate *item, std::ostringstream &out)
{
    // Color
    out << "|c";
    switch (item->Quality)
    {
        case ITEM_QUALITY_POOR:     out << "ff9d9d9d"; break;  //GREY
        case ITEM_QUALITY_NORMAL:   out << "ffffffff"; break;  //WHITE
        case ITEM_QUALITY_UNCOMMON: out << "ff1eff00"; break;  //GREEN
        case ITEM_QUALITY_RARE:     out << "ff0070dd"; break;  //BLUE
        case ITEM_QUALITY_EPIC:     out << "ffa335ee"; break;  //PURPLE
        case ITEM_QUALITY_LEGENDARY: out << "ffff8000"; break;  //ORANGE
        case ITEM_QUALITY_ARTIFACT: out << "ffe6cc80"; break;  //LIGHT YELLOW
        case ITEM_QUALITY_HEIRLOOM: out << "ffe6cc80"; break;  //LIGHT YELLOW
        default:                    out << "ffff0000"; break;  //Don't know color, so red?
    }
    out << "|Hitem:";

    // Item Id
    out << item->ItemId << ":";

    // Permanent enchantment, gems, 4 unknowns, and reporter_level
    // ->new items wont have enchantments or gems so..
    out << "0:0:0:0:0:0:0:0:0";

    // Name
    std::string name = item->Name1;
    ItemLocalization(name, item->ItemId);
    out << "|h[" << name << "]|h|r";

    // Stacked items
    if (item->BuyCount > 1)
        out << "|cff009900x" << item->BuyCount << ".|r";
    else
        out << "|cff009900.|r";
}

// Builds a hlink for an item, includes everything
// |color|Hitem:item_id:perm_ench_id:gem1:gem2:gem3:0:0:0:0:reporter_level|h[name]|h|r
void PlayerbotAI::MakeItemLink(const Item *item, std::ostringstream &out, bool IncludeQuantity /*= true*/)
{
    const ItemTemplate *proto = item->GetTemplate();
    // Color
    out << "|c";
    switch (proto->Quality)
    {
        case ITEM_QUALITY_POOR:     out << "ff9d9d9d"; break;  //GREY
        case ITEM_QUALITY_NORMAL:   out << "ffffffff"; break;  //WHITE
        case ITEM_QUALITY_UNCOMMON: out << "ff1eff00"; break;  //GREEN
        case ITEM_QUALITY_RARE:     out << "ff0070dd"; break;  //BLUE
        case ITEM_QUALITY_EPIC:     out << "ffa335ee"; break;  //PURPLE
        case ITEM_QUALITY_LEGENDARY: out << "ffff8000"; break;  //ORANGE
        case ITEM_QUALITY_ARTIFACT: out << "ffe6cc80"; break;  //LIGHT YELLOW
        case ITEM_QUALITY_HEIRLOOM: out << "ffe6cc80"; break;  //LIGHT YELLOW
        default:                    out << "ffff0000"; break;  //Don't know color, so red?
    }
    out << "|Hitem:";

    // Item Id
    out << proto->ItemId << ":";

    // Permanent enchantment
    out << item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) << ":";

    // Gems
    uint32 g1 = 0, g2 = 0, g3 = 0;
    for (uint32 slot = SOCK_ENCHANTMENT_SLOT; slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++slot)
    {
        uint32 eId = item->GetEnchantmentId(EnchantmentSlot(slot));
        if (!eId) continue;

        SpellItemEnchantmentEntry const* entry = sSpellItemEnchantmentStore.LookupEntry(eId);
        if (!entry) continue;

        switch (slot - SOCK_ENCHANTMENT_SLOT)
        {
            case 1: g1 = entry->GemID; break;
            case 2: g2 = entry->GemID; break;
            case 3: g3 = entry->GemID; break;
        }
    }
    out << g1 << ":" << g2 << ":" << g3 << ":";

    // Temp enchantment, Bonus Enchantment, Prismatic Enchantment?
    // Other stuff, don't know what it is
    out << "0:0:0:0:";

    // Reporter Level
    out << "0";

    // Name
    std::string name = proto->Name1;
    ItemLocalization(name, proto->ItemId);
    out << "|h[" << name << "]|h|r";

    // Stacked items
    if (item->GetCount() > 1 && IncludeQuantity)
        out << "x" << item->GetCount() << ' ';
}

// Builds a string for an item   |color[name]|r
void PlayerbotAI::MakeItemText(const Item *item, std::ostringstream &out, bool IncludeQuantity /*= true*/)
{
    const ItemTemplate *proto = item->GetTemplate();
    // Color
    out << "|c";
    switch (proto->Quality)
    {
        case ITEM_QUALITY_POOR:     out << "ff9d9d9d"; break;  //GREY
        case ITEM_QUALITY_NORMAL:   out << "ffffffff"; break;  //WHITE
        case ITEM_QUALITY_UNCOMMON: out << "ff1eff00"; break;  //GREEN
        case ITEM_QUALITY_RARE:     out << "ff0070dd"; break;  //BLUE
        case ITEM_QUALITY_EPIC:     out << "ffa335ee"; break;  //PURPLE
        case ITEM_QUALITY_LEGENDARY: out << "ffff8000"; break;  //ORANGE
        case ITEM_QUALITY_ARTIFACT: out << "ffe6cc80"; break;  //LIGHT YELLOW
        case ITEM_QUALITY_HEIRLOOM: out << "ffe6cc80"; break;  //LIGHT YELLOW
        default:                    out << "ffff0000"; break;  //Don't know color, so red?
    }

    // Name
    std::string name = proto->Name1;
    ItemLocalization(name, proto->ItemId);
    out << "[" << name << "]|r";

    // Stacked items
    if (item->GetCount() > 1 && IncludeQuantity)
        out << "x" << item->GetCount() << ' ';
}

void PlayerbotAI::extractAuctionIds(const std::string& text, std::list<uint32>& auctionIds) const
{
    uint8 pos = 0;
    while (true)
    {
        int i = text.find("Htitle:", pos);
        if (i == -1)
            break;
        pos = i + 7;
        int endPos = text.find('|', pos);
        if (endPos == -1)
            break;
        std::string idC = text.substr(pos, endPos - pos);
        uint32 id = atol(idC.c_str());
        pos = endPos;
        if (id)
            auctionIds.push_back(id);
    }
}

void PlayerbotAI::extractSpellId(const std::string& text, uint32 &spellId) const
{

    //   Link format
    //   |cffffffff|Hspell:" << spellId << ":" << "|h[" << pSpellInfo->SpellName[loc] << "]|h|r";
    //   cast |cff71d5ff|Hspell:686|h[Shadow Bolt]|h|r";
    //   012345678901234567890123456
    //        base = 16 >|  +7 >|

    uint8 pos = 0;

    int i = text.find("Hspell:", pos);
    if (i == -1)
        return;

    // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: extractSpellId - first pos %u i %u",pos,i);
    pos = i + 7;     // start of window in text 16 + 7 = 23
    int endPos = text.find('|', pos);
    if (endPos == -1)
        return;

    // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: extractSpellId - second endpos : %u pos : %u",endPos,pos);
    std::string idC = text.substr(pos, endPos - pos);     // 26 - 23
    spellId = atol(idC.c_str());
    pos = endPos;     // end
}

void PlayerbotAI::extractSpellIdList(const std::string& text, BotEntryList& m_spellsToLearn) const
{

    //   Link format
    //   |cffffffff|Hspell:" << spellId << ":" << "|h[" << pSpellInfo->SpellName[loc] << "]|h|r";
    //   cast |cff71d5ff|Hspell:686|h[Shadow Bolt]|h|r";
    //   012345678901234567890123456
    //        base = 16 >|  +7 >|

    uint8 pos = 0;
    while (true)
    {
        int i = text.find("Hspell:", pos);
        if (i == -1)
            break;

        // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: extractSpellIdList - first pos %u i %u",pos,i);
        pos = i + 7;     // start of window in text 16 + 7 = 23
        int endPos = text.find('|', pos);
        if (endPos == -1)
            break;

        // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: extractSpellIdList - second endpos : %u pos : %u",endPos,pos);
        std::string idC = text.substr(pos, endPos - pos);     // 26 - 23
        uint32 spellId = atol(idC.c_str());
        pos = endPos;     // end

        if (spellId)
            m_spellsToLearn.push_back(spellId);
    }
}

void PlayerbotAI::extractTalentIds(const std::string &text, std::list<talentPair> &talentIds) const
{
    // Link format:
    // |color|Htalent:talent_id:rank|h[name]|h|r
    // |cff4e96f7|Htalent:1396:4|h[Unleashed Fury]|h|r

    uint8 pos = 0;
    while (true)
    {
        int i = text.find("Htalent:", pos);
        if (i == -1)
            break;
        pos = i + 8;
        // sLog->outDebug(LOG_FILTER_NONE, "extractTalentIds first pos %u i %u",pos,i);
        // extract talent_id
        int endPos = text.find(':', pos);
        if (endPos == -1)
            break;
        // sLog->outDebug(LOG_FILTER_NONE, "extractTalentId second endpos : %u pos : %u",endPos,pos);
        std::string idC = text.substr(pos, endPos - pos);
        uint32 id = atol(idC.c_str());
        pos = endPos + 1;
        // extract rank
        endPos = text.find('|', pos);
        if (endPos == -1)
            break;
        // sLog->outDebug(LOG_FILTER_NONE, "extractTalentId third endpos : %u pos : %u",endPos,pos);
        std::string rankC = text.substr(pos, endPos - pos);
        uint32 rank = atol(rankC.c_str());
        pos = endPos + 1;

        // sLog->outDebug(LOG_FILTER_NONE, "extractTalentId second id : %u  rank : %u",id,rank);

        if (id)
            talentIds.push_back(std::pair<uint32, uint32>(id, rank));
    }
}

void PlayerbotAI::extractGOinfo(const std::string& text, BotObjectList& m_lootTargets) const
{

    //    Link format
    //    |cFFFFFF00|Hfound:" << guid << ':'  << entry << ':'  <<  "|h[" << gInfo->name << "]|h|r";
    //    |cFFFFFF00|Hfound:9582:1731|h[Copper Vein]|h|r

    uint8 pos = 0;
    while (true)
    {
        // extract GO guid
        int i = text.find("Hfound:", pos);     // base H = 11
        if (i == -1)     // break if error
            break;

        pos = i + 7;     //start of window in text 11 + 7 = 18
        int endPos = text.find(':', pos);     // end of window in text 22
        if (endPos == -1)     //break if error
            break;
        std::string guidC = text.substr(pos, endPos - pos);     // get string within window i.e guid 22 - 18 =  4
        uint32 guid = atol(guidC.c_str());     // convert ascii to long int

        // extract GO entry
        pos = endPos + 1;
        endPos = text.find(':', pos);     // end of window in text
        if (endPos == -1)     //break if error
            break;

        std::string entryC = text.substr(pos, endPos - pos);     // get string within window i.e entry
        //uint32 entry = atol(entryC.c_str());     // convert ascii to float

        //uint64 lootCurrent = uint64(HIGHGUID_GAMEOBJECT, entry, guid);
        uint64 lootCurrent = MAKE_NEW_GUID(guid, 0, HIGHGUID_GAMEOBJECT);

        if (guid)
            m_lootTargets.push_back(lootCurrent);
    }
}

// extracts currency in #g#s#c format
uint32 PlayerbotAI::extractMoney(const std::string& text) const
{
    // if user specified money in ##g##s##c format
    std::string acum = "";
    uint32 copper = 0;
    for (uint8 i = 0; i < text.length(); i++)
    {
        if (text[i] == 'g')
        {
            copper += (atol(acum.c_str()) * 100 * 100);
            acum = "";
        }
        else if (text[i] == 'c')
        {
            copper += atol(acum.c_str());
            acum = "";
        }
        else if (text[i] == 's')
        {
            copper += (atol(acum.c_str()) * 100);
            acum = "";
        }
        else if (text[i] == ' ')
            break;
        else if (text[i] >= 48 && text[i] <= 57)
            acum += text[i];
        else
        {
            copper = 0;
            break;
        }
    }
    return copper;
}

// finds items in equipment and adds Item* to foundItemList
// also removes found item IDs from itemIdSearchList when found
void PlayerbotAI::findItemsInEquip(std::list<uint32>& itemIdSearchList, std::list<Item*>& foundItemList) const
{
    for (uint8 slot = EQUIPMENT_SLOT_START; itemIdSearchList.size() > 0 && slot < EQUIPMENT_SLOT_END; slot++)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!pItem)
            continue;

        for (std::list<uint32>::iterator it = itemIdSearchList.begin(); it != itemIdSearchList.end(); ++it)
        {
            if (pItem->GetTemplate()->ItemId != *it)
                continue;

            foundItemList.push_back(pItem);
            itemIdSearchList.erase(it);
            break;
        }
    }
}

// finds items in inventory and adds Item* to foundItemList
// also removes found item IDs from itemIdSearchList when found
void PlayerbotAI::findItemsInInv(std::list<uint32>& itemIdSearchList, std::list<Item*>& foundItemList) const
{

    // look for items in main bag
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; itemIdSearchList.size() > 0 && slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!pItem)
            continue;

        for (std::list<uint32>::iterator it = itemIdSearchList.begin(); it != itemIdSearchList.end(); ++it)
        {
            if (pItem->GetTemplate()->ItemId != *it)
                continue;

            if (m_bot->GetTrader() && m_bot->GetTradeData()->HasItem(pItem->GetGUID()))
                continue;

            foundItemList.push_back(pItem);
            itemIdSearchList.erase(it);
            break;
        }
    }

    // for all for items in other bags
    for (uint8 bag = INVENTORY_SLOT_BAG_START; itemIdSearchList.size() > 0 && bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            continue;

        for (uint8 slot = 0; itemIdSearchList.size() > 0 && slot < pBag->GetBagSize(); ++slot)
        {
            Item* const pItem = m_bot->GetItemByPos(bag, slot);
            if (!pItem)
                continue;

            for (std::list<uint32>::iterator it = itemIdSearchList.begin(); it != itemIdSearchList.end(); ++it)
            {
                if (pItem->GetTemplate()->ItemId != *it)
                    continue;

                if (m_bot->GetTrader() && m_bot->GetTradeData()->HasItem(pItem->GetGUID()))
                    continue;

                foundItemList.push_back(pItem);
                itemIdSearchList.erase(it);
                break;
            }
        }
    }
}

void PlayerbotAI::findNearbyGO()
{
    if (m_collectObjects.empty())
        return;

    std::list<GameObject*> tempTargetGOList;

    for (BotEntryList::iterator itr = m_collectObjects.begin(); itr != m_collectObjects.end(); itr++)
    {
        uint32 entry = *(itr);
        GameObjectTemplate const * gInfo = sObjectMgr->GetGameObjectTemplate(entry);
        bool questGO = false;
        uint8 needCount = 0;

        for (uint32 i = 0; i < 6; ++i)
        {
            if (gInfo->questItems[i] != 0)  // check whether the gameobject contains quest items
            {
                questGO = true;
                if (IsInQuestItemList(gInfo->questItems[i]))    // quest item needed
                    needCount++;
            }
        }

        if (questGO && needCount == 0)
        {
            m_collectObjects.remove(entry); // remove gameobject from collect list
            return;
        }

        // search for GOs with entry, within range of m_bot
        Trinity::GameObjectInRangeCheck go_check(m_bot->GetPositionX(), m_bot->GetPositionY(), m_bot->GetPositionZ(), float(m_confCollectDistance), entry);
        Trinity::GameObjectListSearcher<Trinity::GameObjectInRangeCheck> checker(m_bot, tempTargetGOList, go_check);
        m_bot->VisitNearbyWorldObject(m_confCollectDistance, checker);
        //VisitGridObjects(m_bot, checker, float(m_collectDist));

        // no objects found, continue to next entry
        if (tempTargetGOList.empty())
            continue;

        // add any objects found to our lootTargets
        for (std::list<GameObject*>::iterator iter = tempTargetGOList.begin(); iter != tempTargetGOList.end(); iter++)
        {
            GameObject* go = (*iter);
            if (go->isSpawned())
                m_lootTargets.push_back(go->GetGUID());
        }
    }
}

void PlayerbotAI::findNearbyCreature()
{
    std::list<Creature*> creatureList;
    float radius = INTERACTION_DISTANCE;

    CellCoord pair(Trinity::ComputeCellCoord(m_bot->GetPositionX(), m_bot->GetPositionY()));
    Cell cell(pair);

    Trinity::AnyUnitInObjectRangeCheck go_check(m_bot, radius);
    Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck> go_search(m_bot, creatureList, go_check);
    TypeContainerVisitor<Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>, GridTypeMapContainer> go_visit(go_search);

    // Get Creatures
    cell.Visit(pair, go_visit, *(m_bot->GetMap()), *(m_bot), radius);

    // if (!creatureList.empty())
    //    TellMaster("Found %i Creatures.", creatureList.size());

    for (std::list<Creature*>::iterator iter = creatureList.begin(); iter != creatureList.end(); iter++)
    {
        Creature* currCreature = *iter;

        for (std::list<enum NPCFlags>::iterator itr = m_findNPC.begin(); itr != m_findNPC.end(); itr = m_findNPC.erase(itr))
        {
            uint32 npcflags = currCreature->GetUInt32Value(UNIT_NPC_FLAGS);

            if (!(*itr & npcflags))
                continue;

            if ((*itr == UNIT_NPC_FLAG_TRAINER_CLASS) && !currCreature->isCanTrainingAndResetTalentsOf(m_bot))
                continue;

            //WorldObject *wo = sObjectAccessor->GetObjectInMap(currCreature->GetGUID(), m_bot->GetMap(), (WorldObject*)NULL);
            Creature *wo = sObjectAccessor->GetCreature(*m_bot, currCreature->GetGUID());
            if (!wo) continue;
            if (m_bot->GetDistance(wo) > CONTACT_DISTANCE + wo->GetObjectSize())
            {
                float x, y, z;
                wo->GetContactPoint(m_bot, x, y, z, 1.0f);
                m_bot->GetMotionMaster()->MovePoint(wo->GetMapId(), x, y, z);
                // give time to move to point before trying again
                SetIgnoreUpdateTime(1);
            }

            if (m_bot->GetDistance(wo) < INTERACTION_DISTANCE)
            {

                // sLog->outDebug(LOG_FILTER_NONE, "%s is interacting with (%s)",m_bot->GetName(),currCreature->GetCreatureTemplate()->Name);
                GossipMenuItemsMapBounds pMenuItemBounds = sObjectMgr->GetGossipMenuItemsMapBounds(currCreature->GetCreatureTemplate()->GossipMenuId);

                // prepares quest menu when true
                bool canSeeQuests = currCreature->GetCreatureTemplate()->GossipMenuId == m_bot->GetDefaultGossipMenuForSource(wo);

                // if canSeeQuests (the default, top level menu) and no menu options exist for this, use options from default options
                if (pMenuItemBounds.first == pMenuItemBounds.second && canSeeQuests)
                    pMenuItemBounds = sObjectMgr->GetGossipMenuItemsMapBounds(0);

                for (GossipMenuItemsContainer::const_iterator it = pMenuItemBounds.first; it != pMenuItemBounds.second; it++)
                {
                    if (!(it->second.OptionNpcflag & npcflags))
                        continue;

                    switch (it->second.OptionType)
                    {
                        case GOSSIP_OPTION_BANKER:
                        {
                            // Manage banking actions
                            if (!m_tasks.empty())
                                for (std::list<taskPair>::iterator ait = m_tasks.begin(); ait != m_tasks.end(); ait = m_tasks.erase(ait))
                                {
                                    switch (ait->first)
                                    {
                                        // withdraw items
                                        case BANK_WITHDRAW:
                                        {
                                            // TellMaster("Withdraw items");
                                            if (!Withdraw(ait->second))
                                                //sLog->outDebug(LOG_FILTER_NONE, "Withdraw: Couldn't withdraw (%u)", ait->second);
                                            break;
                                        }
                                        // deposit items
                                        case BANK_DEPOSIT:
                                        {
                                            // TellMaster("Deposit items");
                                            if (!Deposit(ait->second))
                                                //sLog->outDebug(LOG_FILTER_NONE, "Deposit: Couldn't deposit (%u)", ait->second);
                                            break;
                                        }
                                        default:
                                            break;
                                    }
                                }
                            BankBalance();
                            break;
                        }
                        case GOSSIP_OPTION_TAXIVENDOR:
                        case GOSSIP_OPTION_GOSSIP:
                        case GOSSIP_OPTION_INNKEEPER:
                        case GOSSIP_OPTION_TRAINER:
                        case GOSSIP_OPTION_QUESTGIVER:
                        case GOSSIP_OPTION_VENDOR:
                        case GOSSIP_OPTION_UNLEARNTALENTS:
                        {
                            // Manage questgiver, trainer, innkeeper & vendor actions
                            if (!m_tasks.empty())
                                for (std::list<taskPair>::iterator ait = m_tasks.begin(); ait != m_tasks.end(); ait = m_tasks.erase(ait))
                                {
                                    switch (ait->first)
                                    {
                                        // reset talents
                                        case RESET_TALENTS:
                                        {
                                            // TellMaster("Reset all talents");
                                            if (Talent(currCreature))
                                                InspectUpdate();
                                            break;
                                        }
                                        // take new quests
                                        case TAKE_QUEST:
                                        {
                                            // TellMaster("Accepting quest");
                                            if (!AddQuest(ait->second, wo))
                                                //sLog->outDebug(LOG_FILTER_NONE, "AddQuest: Couldn't add quest (%u)", ait->second);
                                            break;
                                        }
                                        // list npc quests
                                        case LIST_QUEST:
                                        {
                                            // TellMaster("Show available npc quests");
                                            ListQuests(wo);
                                            break;
                                        }
                                        // end quests
                                        case END_QUEST:
                                        {
                                            // TellMaster("Turn in available quests");
                                            TurnInQuests(wo);
                                            break;
                                        }
                                        // sell items
                                        case SELL_ITEMS:
                                        {
                                            // TellMaster("Selling items");
                                            Sell(ait->second);
                                            break;
                                        }
                                        // repair items
                                        //case REPAIR_ITEMS:
                                        //{
                                            // TellMaster("Repairing items");
                                            //Repair(ait->second, currCreature);
                                            //break;
                                        //}
                                        default:
                                            break;
                                    }
                                }
                            break;
                        }
                        case GOSSIP_OPTION_AUCTIONEER:
                        {
                            // Manage auctioneer actions
                            if (!m_tasks.empty())
                                for (std::list<taskPair>::iterator ait = m_tasks.begin(); ait != m_tasks.end(); ait = m_tasks.erase(ait))
                                {
                                    switch (ait->first)
                                    {
                                        // add new auction item
                                        case ADD_AUCTION:
                                        {
                                            // TellMaster("Creating auction");
                                            AddAuction(ait->second, currCreature);
                                            break;
                                        }
                                        // cancel active auction
                                        case REMOVE_AUCTION:
                                        {
                                            // TellMaster("Cancelling auction");
                                            if (!RemoveAuction(ait->second))
                                                //sLog->outDebug(LOG_FILTER_NONE, "RemoveAuction: Couldn't remove auction (%u)", ait->second);
                                            break;
                                        }
                                        default:
                                            break;
                                    }
                                }
                            ListAuctions();
                            break;
                        }
                        default:
                            break;
                    }
                    m_bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                }
            }
            m_bot->GetMotionMaster()->Clear();
            m_bot->GetMotionMaster()->MoveIdle();
        }
    }
}

/**
 * GiveLevel sets the bot's level to 'level'
 * Not the clearest of function names, we're just mirroring Player.cpp's function name
 */
void PlayerbotAI::GiveLevel(uint32 /*level*/)
{
    // Talent function in Player::GetLevel take care of resetting talents in case level < getLevel()
    ApplyActiveTalentSpec();
}

bool PlayerbotAI::CanStore()
{
    uint32 totalused = 0;
    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        const Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
            totalused++;
    }
    uint32 totalfree = 16 - totalused;
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
        {
            ItemTemplate const* pBagProto = pBag->GetTemplate();
            if (pBagProto->Class == ITEM_CLASS_CONTAINER && pBagProto->SubClass == ITEM_SUBCLASS_CONTAINER)
                totalfree =  totalfree + pBag->GetFreeSlots();
        }
    }
    return totalfree;
}

// use item on self
void PlayerbotAI::UseItem(Item *item)
{
    UseItem(item, TARGET_FLAG_NONE, 0);
}

// use item on equipped item
void PlayerbotAI::UseItem(Item *item, uint8 targetInventorySlot)
{
    if (targetInventorySlot >= EQUIPMENT_SLOT_END)
        return;

    Item* const targetItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, targetInventorySlot);
    if (!targetItem)
        return;

    UseItem(item, TARGET_FLAG_ITEM, targetItem->GetGUID());
}

// use item on unit
void PlayerbotAI::UseItem(Item *item, Unit *target)
{
    if (!target)
        return;

    UseItem(item, TARGET_FLAG_UNIT, target->GetGUID());
}

// generic item use method
void PlayerbotAI::UseItem(Item *item, uint32 targetFlag, uint64 targetGUID)
{
    if (!item)
        return;

    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();
    uint8 cast_count = 1;
    uint64 item_guid = item->GetGUID();
    uint32 glyphIndex = 0;
    uint8 unk_flags = 0;

    if (uint32 questid = item->GetTemplate()->StartQuest)
    {
        std::ostringstream report;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (qInfo)
        {
            m_bot->GetMotionMaster()->Clear(true);
            WorldPacket* const packet = new WorldPacket(CMSG_QUESTGIVER_ACCEPT_QUEST, 8 + 4 + 4);
            *packet << item_guid;
            *packet << questid;
            *packet << uint32(0);
            m_bot->GetSession()->QueuePacket(packet); // queue the packet to get around race condition
            report << "|cffffff00Quest taken |r" << qInfo->GetTitle();
            TellMaster(report.str());
        }
        return;
    }

    uint32 spellId = 0;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (item->GetTemplate()->Spells[i].SpellId > 0)
        {
            spellId = item->GetTemplate()->Spells[i].SpellId;
            break;
        }
    }

    if (item->GetTemplate()->Flags & ITEM_FLAG_UNLOCKED && spellId == 0)
    {
        // Open quest item in inventory, containing related items (e.g Gnarlpine necklace, containing Tallonkai's Jewel)
        WorldPacket* const packet = new WorldPacket(CMSG_OPEN_ITEM, 2);
        *packet << item->GetBagSlot();
        *packet << item->GetSlot();
        m_bot->GetSession()->QueuePacket(packet); // queue the packet to get around race condition
        return;
    }

    WorldPacket *packet = new WorldPacket(CMSG_USE_ITEM, 28);
    *packet << bagIndex << slot << cast_count << spellId << item_guid
            << glyphIndex << unk_flags << targetFlag;

    if (targetFlag & (TARGET_FLAG_UNIT | TARGET_FLAG_ITEM | TARGET_FLAG_GAMEOBJECT))
        *packet << targetGUID;//.WriteAsPacked();

    m_bot->GetSession()->QueuePacket(packet);

    SpellInfo const * spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        TellMaster("Can't find spell entry for spell %u on item %u", spellId, item->GetEntry());
        return;
    }

    SpellCastTimesEntry const * castingTimeEntry = spellInfo->CastTimeEntry;
    if (!castingTimeEntry)
    {
        TellMaster("Can't find casting time entry for spell %u with index %u", spellId, spellInfo->CastTimeEntry);
        return;
    }

    uint8 duration, castTime;
    castTime = (uint8) ((float) castingTimeEntry->CastTime / 1000.0f);

    if (item->GetTemplate()->Class == ITEM_CLASS_CONSUMABLE && item->GetTemplate()->SubClass == ITEM_SUBCLASS_FOOD)
    {
        duration = (uint8) ((float)spellInfo->GetDuration() / 1000.0f);
        SetIgnoreUpdateTime(castTime + duration);
    }
    else
        SetIgnoreUpdateTime(castTime);
}

// submits packet to use an item
void PlayerbotAI::EquipItem(Item* src_Item)
{
    uint8 src_bagIndex = src_Item->GetBagSlot();
    uint8 src_slot = src_Item->GetSlot();

    // sLog->outDebug(LOG_FILTER_NONE, "PlayerbotAI::EquipItem: %s in srcbag = %u, srcslot = %u", src_Item->GetTemplate()->Name1, src_bagIndex, src_slot);

    uint16 dest;
    InventoryResult msg = m_bot->CanEquipItem(NULL_SLOT, dest, src_Item, !src_Item->IsBag());
    if (msg != EQUIP_ERR_OK)
    {
        m_bot->SendEquipError(msg, src_Item, NULL);
        return;
    }

    uint16 src = src_Item->GetPos();
    if (dest == src)                                        // prevent equip in same slot, only at cheat
        return;

    Item *dest_Item = m_bot->GetItemByPos(dest);
    if (!dest_Item)                                          // empty slot, simple case
    {
        m_bot->RemoveItem(src_bagIndex, src_slot, true);
        m_bot->EquipItem(dest, src_Item, true);
        m_bot->AutoUnequipOffhandIfNeed();
    }
    else                                                    // have currently equipped item, not simple case
    {
        uint8 dest_bagIndex = dest_Item->GetBagSlot();
        uint8 dest_slot = dest_Item->GetSlot();

        msg = m_bot->CanUnequipItem(dest, false);
        if (msg != EQUIP_ERR_OK)
        {
            m_bot->SendEquipError(msg, dest_Item, NULL);
            return;
        }

        // check dest->src move possibility
        ItemPosCountVec sSrc;
        if (m_bot->IsInventoryPos(src))
        {
            msg = m_bot->CanStoreItem(src_bagIndex, src_slot, sSrc, dest_Item, true);
            if (msg != EQUIP_ERR_OK)
                msg = m_bot->CanStoreItem(src_bagIndex, NULL_SLOT, sSrc, dest_Item, true);
            if (msg != EQUIP_ERR_OK)
                msg = m_bot->CanStoreItem(NULL_BAG, NULL_SLOT, sSrc, dest_Item, true);
        }

        if (msg != EQUIP_ERR_OK)
        {
            m_bot->SendEquipError(msg, dest_Item, src_Item);
            return;
        }

        // now do moves, remove...
        m_bot->RemoveItem(dest_bagIndex, dest_slot, false);
        m_bot->RemoveItem(src_bagIndex, src_slot, false);

        // add to dest
        m_bot->EquipItem(dest, src_Item, true);

        // add to src
        if (m_bot->IsInventoryPos(src))
            m_bot->StoreItem(sSrc, dest_Item, true);

        m_bot->AutoUnequipOffhandIfNeed();
    }
}

// submits packet to trade an item (trade window must already be open)
// default slot is -1 which means trade slots 0 to 5. if slot is set
// to TRADE_SLOT_NONTRADED (which is slot 6) item will be shown in the
// 'Will not be traded' slot.
bool PlayerbotAI::TradeItem(const Item& item, int8 slot)
{
    //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: TradeItem - slot=%d, hasTrader=%d, itemInTrade=%d, itemTradeable=%d",
               //slot,
               //(m_bot->GetTrader() ? 1 : 0),
               //(item.IsInTrade() ? 1 : 0),
               //(item.CanBeTraded() ? 1 : 0)
               //);

    if (!m_bot->GetTrader() || item.IsInTrade() || (!item.CanBeTraded() && slot != TRADE_SLOT_NONTRADED))
        return false;

    int8 tradeSlot = -1;

    TradeData* pTrade = m_bot->GetTradeData();
    if ((slot >= 0 && slot < TRADE_SLOT_COUNT) && pTrade->GetItem(TradeSlots(slot)) == NULL)
        tradeSlot = slot;
    else
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot == -1; i++)
        {
            if (pTrade->GetItem(TradeSlots(i)) == NULL)
            {
                tradeSlot = i;
                // reserve trade slot to allow multiple items to be traded
                pTrade->SetItem(TradeSlots(i), const_cast<Item*>(&item));
            }
        }

    if (tradeSlot == -1) return false;

    WorldPacket* const packet = new WorldPacket(CMSG_SET_TRADE_ITEM, 3);
    *packet << (uint8) tradeSlot << (uint8) item.GetBagSlot()
            << (uint8) item.GetSlot();
    m_bot->GetSession()->QueuePacket(packet);
    return true;
}

// submits packet to trade copper (trade window must be open)
bool PlayerbotAI::TradeCopper(uint32 copper)
{
    if (copper > 0)
    {
        WorldPacket* const packet = new WorldPacket(CMSG_SET_TRADE_GOLD, 4);
        *packet << copper;
        m_bot->GetSession()->QueuePacket(packet);
        return true;
    }
    return false;
}

//bool PlayerbotAI::DoTeleport(WorldObject &obj)
//{
//    m_ignoreAIUpdatesUntilTime = time(NULL) + 6;
//    Player *mstr = m_master;
//    PlayerbotChatHandler ch(mstr);
//    if (!ch.teleport(*m_bot, obj))
//    {
//        ch.sysmessage(".. could not be teleported ..");
//        // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: DoTeleport - %s failed to teleport", m_bot->GetName() );
//        return false;
//    }
//    return true;
//}

void PlayerbotAI::HandleTeleportAck()
{
    //TellMaster("Debug: HandleTeleportAck()");
    m_ignoreAIUpdatesUntilTime = time(NULL) + 1;
    m_bot->GetMotionMaster()->Clear(true);
    if (m_bot->IsBeingTeleportedNear())
    {
        WorldPacket p = WorldPacket(MSG_MOVE_TELEPORT_ACK, 8 + 4 + 4);
        p.appendPackGUID(m_bot->GetGUID());
        p << (uint32) 0; // supposed to be flags? not used currently
        p << (uint32) time(NULL); // time - not currently used
        m_bot->GetSession()->HandleMoveTeleportAck(p);
    }
    else if (m_bot->IsBeingTeleportedFar())
        m_bot->GetSession()->HandleMoveWorldportAckOpcode();
}

// Localization support
void PlayerbotAI::ItemLocalization(std::string& itemName, const uint32 itemID) const
{
    uint32 loc = m_master->GetSession()->GetSessionDbLocaleIndex();
    std::wstring wnamepart;

    ItemLocale const *pItemInfo = sObjectMgr->GetItemLocale(itemID);
    if (pItemInfo)
        if (pItemInfo->Name.size() > loc && !pItemInfo->Name[loc].empty())
        {
            const std::string name = pItemInfo->Name[loc];
            if (Utf8FitTo(name, wnamepart))
                itemName = name.c_str();
        }
}

void PlayerbotAI::QuestLocalization(std::string& questTitle, const uint32 questID) const
{
    uint32 loc = m_master->GetSession()->GetSessionDbLocaleIndex();
    std::wstring wnamepart;

    QuestLocale const *pQuestInfo = sObjectMgr->GetQuestLocale(questID);
    if (pQuestInfo)
        if (pQuestInfo->Title.size() > loc && !pQuestInfo->Title[loc].empty())
        {
            const std::string title = pQuestInfo->Title[loc];
            if (Utf8FitTo(title, wnamepart))
                questTitle = title.c_str();
        }
}

void PlayerbotAI::CreatureLocalization(std::string& creatureName, const uint32 entry) const
{
    uint32 loc = m_master->GetSession()->GetSessionDbLocaleIndex();
    std::wstring wnamepart;

    CreatureLocale const *pCreatureInfo = sObjectMgr->GetCreatureLocale(entry);
    if (pCreatureInfo)
        if (pCreatureInfo->Name.size() > loc && !pCreatureInfo->Name[loc].empty())
        {
            const std::string title = pCreatureInfo->Name[loc];
            if (Utf8FitTo(title, wnamepart))
                creatureName = title.c_str();
        }
}

void PlayerbotAI::GameObjectLocalization(std::string& gameobjectName, const uint32 entry) const
{
    uint32 loc = m_master->GetSession()->GetSessionDbLocaleIndex();
    std::wstring wnamepart;

    GameObjectLocale const *pGameObjectInfo = sObjectMgr->GetGameObjectLocale(entry);
    if (pGameObjectInfo)
        if (pGameObjectInfo->Name.size() > loc && !pGameObjectInfo->Name[loc].empty())
        {
            const std::string title = pGameObjectInfo->Name[loc];
            if (Utf8FitTo(title, wnamepart))
                gameobjectName = title.c_str();
        }
}

// Helper function for automatically selling poor quality items to the vendor
void PlayerbotAI::_doSellItem(Item *item, std::ostringstream &report, std::ostringstream &canSell, uint32 &TotalCost, uint32 &TotalSold)
{
    if (!item)
        return;

    uint8 autosell = 0;

    std::ostringstream mout;
    if (item->CanBeTraded() && item->GetTemplate()->Quality == ITEM_QUALITY_POOR) // trash sells automatically.
        autosell = 1;
    if (SellWhite == 1) // set this with the command 'sell all'
    {
        // here we'll do some checks for other items that are safe to automatically sell such as
        // white items that are a number of levels lower than anything we could possibly use.
        // We'll check to make sure its not a tradeskill tool, quest item etc, things that we don't want to lose.
        if (item->GetTemplate()->SellPrice > 0 && (item->GetTemplate()->Quality == ITEM_QUALITY_NORMAL || item->GetTemplate()->Quality == ITEM_QUALITY_UNCOMMON) && item->GetTemplate()->SubClass != ITEM_SUBCLASS_QUEST)
        {
            ItemTemplate const *pProto = item->GetTemplate();
            if (pProto->RequiredLevel < (m_bot->getLevel() - gConfigSellLevelDiff) && pProto->SubClass != ITEM_SUBCLASS_WEAPON_MISC && pProto->FoodType == 0)
            {
                if (pProto->Class == ITEM_CLASS_WEAPON)
                    autosell = 1;
                if (pProto->Class == ITEM_CLASS_ARMOR)
                    autosell = 1;
            }
            if (pProto->SubClass == ITEM_SUBCLASS_FOOD && (pProto->RequiredLevel < (m_bot->getLevel() - gConfigSellLevelDiff)))
            {
                autosell = 1;
            }
        }
    }

    if (autosell == 1) // set this switch above and this item gets sold automatically. Only set this for automatic sales e.g junk etc.
    {

        uint32 cost = item->GetCount() * item->GetTemplate()->SellPrice;
        m_bot->ModifyMoney(cost);
        m_bot->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
        m_bot->AddItemToBuyBackSlot(item);

        ++TotalSold;
        TotalCost += cost;

        report << "Sold ";
        MakeItemLink(item, report, true);
        report << " for ";

        report << Cash(cost);
    }
    else if (item->GetTemplate()->SellPrice > 0)
        MakeItemLink(item, canSell, true);
}

bool PlayerbotAI::Withdraw(const uint32 itemid)
{
    Item* pItem = FindItemInBank(itemid);
    if (pItem)
    {
        std::ostringstream report;

        ItemPosCountVec dest;
        InventoryResult msg = m_bot->CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            m_bot->SendEquipError(msg, pItem, NULL);
            return false;
        }

        m_bot->RemoveItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
        m_bot->StoreItem(dest, pItem, true);

        report << "Withdrawn ";
        MakeItemLink(pItem, report, true);

        TellMaster(report.str());
    }
    return true;
}

bool PlayerbotAI::Deposit(const uint32 itemid)
{
    Item* pItem = FindItem(itemid);
    if (pItem)
    {
        std::ostringstream report;

        ItemPosCountVec dest;
        InventoryResult msg = m_bot->CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            m_bot->SendEquipError(msg, pItem, NULL);
            return false;
        }

        m_bot->RemoveItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
        m_bot->BankItem(dest, pItem, true);

        report << "Deposited ";
        MakeItemLink(pItem, report, true);

        TellMaster(report.str());
    }
    return true;
}

void PlayerbotAI::BankBalance()
{
    std::ostringstream report;

    report << "In my bank\n ";
    report << "My item slots: ";

    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
    {
        Item* const item = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item)
            MakeItemLink(item, report, true);
    }
    TellMaster(report.str());

    // and each of my bank bags
    for (uint8 bag = BANK_SLOT_BAG_START; bag < BANK_SLOT_BAG_END; ++bag)
    {
        std::ostringstream goods;
        const Bag* const pBag = static_cast<Bag *>(m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag));
        if (pBag)
        {
            goods << "\nMy ";
            const ItemTemplate* const pBagProto = pBag->GetTemplate();
            std::string bagName = pBagProto->Name1;
            ItemLocalization(bagName, pBagProto->ItemId);
            goods << bagName << " slot: ";

            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const item = m_bot->GetItemByPos(bag, slot);
                if (item)
                    MakeItemLink(item, goods, true);
            }
            TellMaster(goods.str());
        }
    }
}

bool PlayerbotAI::Talent(Creature* trainer)
{
    if (!(m_bot->resetTalents()))
    {
        WorldPacket* const packet = new WorldPacket(MSG_TALENT_WIPE_CONFIRM, 8 + 4);    //you do not have any talent
        *packet << uint64(0);
        *packet << uint32(0);
        m_bot->GetSession()->QueuePacket(packet);
        return false;
    }

    m_bot->SendTalentsInfoData(false);
    trainer->CastSpell(m_bot, 14867, true);                  //spell: "Untalent Visual Effect"
    return true;
}

void PlayerbotAI::InspectUpdate()
{
    WorldPacket packet(SMSG_INSPECT_TALENT, 50);
    packet.append(m_bot->GetPackGUID());
    m_bot->BuildPlayerTalentsInfoData(&packet);
    m_bot->BuildEnchantmentsInfoData(&packet);
    m_master->GetSession()->SendPacket(&packet);
}

void PlayerbotAI::Repair(const uint32 itemid, Creature* rCreature)
{
    Item* rItem = FindItem(itemid); // if item equipped or in bags
    uint8 IsInGuild = (m_bot->GetGuildId() != 0) ? uint8(1) : uint8(0);
    uint64 itemGuid = (rItem) ? rItem->GetGUID() : 0;

    WorldPacket* const packet = new WorldPacket(CMSG_REPAIR_ITEM, 8 + 8 + 1);
    *packet << rCreature->GetGUID();  // repair npc guid
    *packet << itemGuid; // if item specified then repair this, else repair all
    *packet << IsInGuild;  // guildbank yes=1 no=0
    m_bot->GetSession()->QueuePacket(packet);  // queue the packet to get around race condition
}

bool PlayerbotAI::RemoveAuction(const uint32 auctionid)
{
    QueryResult result = CharacterDatabase.PQuery(
        //"SELECT houseid,itemguid,item_template,itemowner,buyoutprice,time,buyguid,lastbid,startbid,deposit FROM auction WHERE id = '%u'", auctionid);
        "SELECT auctioneerguid,itemguid,itemowner,buyoutprice,time,buyguid,lastbid,startbid,deposit FROM auctionhouse WHERE id = '%u'", auctionid);

    AuctionEntry *auction;

    if (result)
    {
        Field *fields = result->Fetch();

        auction = new AuctionEntry;
        auction->Id = auctionid;
        auction->auctioneer = fields[0].GetUInt32();
        auction->item_guidlow = fields[1].GetUInt32();
        auction->item_template = fields[2].GetUInt32();
        auction->owner = fields[3].GetUInt32();
        auction->buyout = fields[4].GetUInt32();
        auction->expire_time = fields[5].GetUInt32();
        auction->bidder = fields[6].GetUInt32();
        auction->bid = fields[7].GetUInt32();
        auction->startbid = fields[8].GetUInt32();
        auction->deposit = fields[9].GetUInt32();
        auction->auctionHouseEntry = NULL;                  // init later

        // check if sold item exists for guid
        // and item_template in fact (GetAItem will fail if problematic in result check in AuctionHouseMgr::LoadAuctionItems)
        Item* pItem = sAuctionMgr->GetAItem(auction->item_guidlow);
        if (!pItem)
        {
            SQLTransaction trans = CharacterDatabase.BeginTransaction();
            auction->DeleteFromDB(trans);
            CharacterDatabase.CommitTransaction(trans);
            //sLog->outError("Auction %u has not a existing item : %u, deleted", auction->Id, auction->item_guidlow);
            delete auction;
            //delete result;
            return false;
        }

        auction->auctionHouseEntry = sAuctionHouseStore.LookupEntry(auction->auctioneer);

        // Attempt send item back to owner
        std::ostringstream msgAuctionCanceledOwner;
        msgAuctionCanceledOwner << auction->item_template << ":0:" << AUCTION_CANCELED << ":0:0";

        // item will deleted or added to received mail list
        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        MailDraft(msgAuctionCanceledOwner.str(), "")    // TODO: fix body
        .AddItem(pItem)
        .SendMailTo(trans, MailReceiver(auction->owner), auction, MAIL_CHECK_MASK_COPIED);
        

        if (sAuctionMgr->RemoveAItem(auction->item_guidlow))
            m_bot->GetSession()->SendAuctionCommandResult(auction->Id, AUCTION_CANCEL, AUCTION_OK);

        auction->DeleteFromDB(trans);

        CharacterDatabase.CommitTransaction(trans);

        delete auction;
        //delete result;
    }
    return true;
}

// Subject - 9360:0:2
// Subject - item:0:MailAuctionAnswer
// Body - 0:2650:2650:120:132
// Body - 0:High Bid:Buyout:Deposit:AuctionHouse Cut

std::string PlayerbotAI::AuctionResult(std::string subject, std::string body)
{
    std::ostringstream out;
    std::string winner;
    int pos;

    subject.append(":");
    if (body.size() > 0)
    {
        pos = body.find_first_not_of(" ");
        subject.append(body, pos, body.size() - pos);
        subject.append(":");
    }

    //sLog->outDebug(LOG_FILTER_NONE, "Auctions string (%s)", subject.c_str());
    pos = 0;
    int sublen = subject.size() / 2;
    uint32 a_info[15];
    for (int i = 0; i < sublen; i++)
    {
        int endpos = subject.find(':', pos);
        std::string idc = subject.substr(pos, endpos - pos);
        a_info[i] = atol(idc.c_str());
        //sLog->outDebug(LOG_FILTER_NONE, "a_info[%d] = (%u)", i, a_info[i]);
        pos = endpos + 1;
    }

    if (a_info[4] != a_info[5])
        winner =  "High Bidder";
    else
        winner =  "Buyout";

    ItemTemplate const *pProto = sObjectMgr->GetItemTemplate(a_info[0]);
    if (!pProto)
        return out.str();

    switch (a_info[2])
    {
        case AUCTION_OUTBIDDED:           //= 0,
            out << "Subject: Outbid on: " << pProto->Name1;
            break;
        case AUCTION_WON:                 //= 1,
            out << "Subject: Auction won: " << pProto->Name1 << "\n";
            out << "Item Purchased: " << pProto->Name1 << "\n";
            break;
        case AUCTION_SUCCESSFUL:          //= 2,
        {
            out << "Subject: Auction successful: " << pProto->Name1 << "\n";
            out << "Item Sold: " << pProto->Name1 << "\n";
            out << "\n[" << winner << " Sale: " << Cash(a_info[4]) << "]";
            out << "\n( |cff1eff00Deposit:|cffccffff " << Cash(a_info[6]) << " |cffff0000- Tax:|cffccffff " << Cash(a_info[7]) << " ) |cff1eff00+|cffccffff";
            break;
        }
        case AUCTION_EXPIRED:             //= 3,
            out << "Subject: Auction expired: " << pProto->Name1;
            break;
        case AUCTION_CANCELLED_TO_BIDDER: //= 4,
            out << "Subject: Auction cancelled to bidder: " << pProto->Name1;
            break;
        case AUCTION_CANCELED:            //= 5,
            out << "Subject: Auction cancelled: " << pProto->Name1;
            break;
        case AUCTION_SALE_PENDING:        //= 6
            out << "Subject: Auction sale pending: " << pProto->Name1;
            break;
    }
    return out.str();
}

std::string PlayerbotAI::Cash(uint32 copper)
{
    using namespace std;
    std::ostringstream change;

    uint32 gold = uint32(copper / 10000);
    copper -= (gold * 10000);
    uint32 silver = uint32(copper / 100);
    copper -= (silver * 100);

    if (gold > 0)
        change << gold <<  " |TInterface\\Icons\\INV_Misc_Coin_01:8|t";
    if (silver > 0)
        change << std::setfill(' ') << std::setw(2) << silver << " |TInterface\\Icons\\INV_Misc_Coin_03:8|t";
    change << std::setfill(' ') << std::setw(2) << copper << " |TInterface\\Icons\\INV_Misc_Coin_05:8|t";

    return change.str();
}

void PlayerbotAI::ListQuests(WorldObject * questgiver)
{
    if (!questgiver)
        return;

    // list all bot quests this NPC has
    m_bot->PrepareQuestMenu(questgiver->GetGUID());
    QuestMenu& questMenu = m_bot->PlayerTalkClass->GetQuestMenu();
    std::ostringstream out;
    for (uint32 iI = 0; iI < questMenu.GetMenuItemCount(); ++iI)
    {
        QuestMenuItem const& qItem = questMenu.GetItem(iI);
        uint32 questID = qItem.QuestId;
        Quest const* pQuest = sObjectMgr->GetQuestTemplate(questID);

        std::string questTitle  = pQuest->GetTitle();
        QuestLocalization(questTitle, questID);

        if (m_bot->SatisfyQuestStatus(pQuest, false))
            out << "|cff808080|Hquest:" << questID << ':' << pQuest->GetQuestLevel() << "|h[" << questTitle << "]|h|r";
    }
    if (!out.str().empty())
        TellMaster(out.str());
}

bool PlayerbotAI::AddQuest(const uint32 entry, WorldObject * questgiver)
{
    std::ostringstream out;

    Quest const* qInfo = sObjectMgr->GetQuestTemplate(entry);
    if (!qInfo)
    {
        ChatHandler(m_master->GetSession()).PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        return false;
    }

    if (m_bot->GetQuestStatus(entry) == QUEST_STATUS_COMPLETE)
    {
        TellMaster("I already completed that quest.");
        return false;
    }
    else if (!m_bot->CanTakeQuest(qInfo, false))
    {
        if (!m_bot->SatisfyQuestStatus(qInfo, false))
            TellMaster("I already have that quest.");
        else
            TellMaster("I can't take that quest.");
        return false;
    }
    else if (!m_bot->SatisfyQuestLog(false))
    {
        TellMaster("My quest log is full.");
        return false;
    }
    else if (m_bot->CanAddQuest(qInfo, false))
    {
        m_bot->AddQuest(qInfo, questgiver);

        std::string questTitle  = qInfo->GetTitle();
        QuestLocalization(questTitle, entry);

        out << "|cffffff00Quest taken " << "|cff808080|Hquest:" << entry << ':' << qInfo->GetQuestLevel() << "|h[" << questTitle << "]|h|r";

        if (m_bot->CanCompleteQuest(entry))
            m_bot->CompleteQuest(entry);

        // build needed items if quest contains any
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
            if (qInfo->RequiredItemCount[i] > 0)
            {
                SetQuestNeedItems();
                break;
            }

        // build needed creatures if quest contains any
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
            if (qInfo->RequiredNpcOrGoCount[i] > 0)
            {
                SetQuestNeedCreatures();
                break;
            }

        TellMaster(out.str());
        return true;
    }
    return false;
}

void PlayerbotAI::ListAuctions()
{
    std::ostringstream report;

    QueryResult result = CharacterDatabase.PQuery(
        //"SELECT id,itemguid,item_template,time,buyguid,lastbid FROM auction WHERE itemowner = '%u'", m_bot->GetGUIDLow());
        "SELECT id,itemguid,time,buyguid,lastbid FROM auctionhouse WHERE itemowner = '%u'", m_bot->GetGUIDLow());
    if (result)
    {
        report << "My active auctions are: \n";
        do
        {
            Field *fields = result->Fetch();

            uint32 Id = fields[0].GetUInt32();
            uint32 itemGuidLow = fields[1].GetUInt32();
            AuctionEntry *auction = AuctionHouseObject().GetAuction(Id);
            uint32 itemTemplate = auction->item_template;//fields[2].GetUInt32();
            time_t expireTime = fields[2].GetUInt32();
            uint32 bidder = fields[3].GetUInt32();
            uint32 bid = fields[4].GetUInt32();

            // current time
            time_t currtime = time(NULL);
            time_t remtime = expireTime - currtime;

            tm* aTm = gmtime(&remtime);

            if (expireTime > currtime)
            {
                Item* aItem = sAuctionMgr->GetAItem(itemGuidLow);
                if (aItem)
                {
                    // Name
                    uint32 count = aItem->GetCount();
                    std::string name = aItem->GetTemplate()->Name1;
                    ItemLocalization(name, itemTemplate);
                    report << "\n|cffffffff|Htitle:" << Id << "|h[" << name;
                    if (count > 1)
                        report << "|cff00ff00x" << count << "|cffffffff" << "]|h|r";
                    else
                        report << "]|h|r";
                }

                if (bidder)
                {
                    uint64 guid = bidder;
                    std::string bidder_name;
                    if (sObjectMgr->GetPlayerNameByGUID(guid, bidder_name))
                        report << " " << bidder_name << ": ";

                    report << Cash(bid);
                }
                if (aItem)
                    report << " ends: " << aTm->tm_hour << "|cff0070dd|hH|h|r " << aTm->tm_min << "|cff0070dd|hmin|h|r";
            }
        } while (result->NextRow());

        //delete result;
        TellMaster(report.str().c_str());
    }
}

void PlayerbotAI::AddAuction(const uint32 itemid, Creature* aCreature)
{
    Item* aItem = FindItem(itemid);
    if (aItem)
    {
        std::ostringstream out;
        srand(time(NULL));
        uint32 duration[3] = { 720, 1440, 2880 };  // 720 = 12hrs, 1440 = 24hrs, 2880 = 48hrs
        uint32 etime = duration[rand() % 3];

        uint32 min = urand(aItem->GetTemplate()->SellPrice * aItem->GetCount(), aItem->GetTemplate()->BuyPrice * aItem->GetCount()) * (aItem->GetTemplate()->Quality + 1);
        uint32 max = urand(aItem->GetTemplate()->SellPrice * aItem->GetCount(), aItem->GetTemplate()->BuyPrice * aItem->GetCount()) * (aItem->GetTemplate()->Quality + 1);

        out << "Auctioning ";
        MakeItemLink(aItem, out, true);
        out << " with " << aCreature->GetCreatureTemplate()->Name;
        TellMaster(out.str().c_str());

        WorldPacket* const packet = new WorldPacket(CMSG_AUCTION_SELL_ITEM, 8 + 4 + 8 + 4 + 4 + 4 + 4);
        *packet << aCreature->GetGUID();     // auctioneer guid
        *packet << uint32(1);                      // const 1
        *packet << aItem->GetGUID();         // item guid
        *packet << aItem->GetCount();      // stacksize
        *packet << uint32((min < max) ? min : max);  // starting bid
        *packet << uint32((max > min) ? max : min);  // buyout
        *packet << uint32(etime);  // auction duration

        m_bot->GetSession()->QueuePacket(packet);  // queue the packet to get around race condition
    }
}

void PlayerbotAI::Buy(uint64 vendorguid, const uint32 itemid)
{
    Creature *pCreature = m_bot->GetNPCIfCanInteractWith(vendorguid, UNIT_NPC_FLAG_VENDOR);

    if (!pCreature)
        return;

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorItems();

    uint8 customitems = vItems ? vItems->GetItemCount() : 0;
    uint8 numitems = customitems + (tItems ? tItems->GetItemCount() : 0);

    for (uint8 vendorslot = 0; vendorslot < numitems; ++vendorslot)
    {
        VendorItem const* crItem = vendorslot < customitems ? vItems->GetItem(vendorslot) : tItems->GetItem(vendorslot - customitems);

        if (crItem)
        {
            if (itemid != crItem->item)
                continue;

            ItemTemplate const *pProto = sObjectMgr->GetItemTemplate(itemid);
            if (pProto)
            {
                // class wrong item skip only for bindable case
                if ((pProto->AllowableClass & m_bot->getClassMask()) == 0 && pProto->Bonding == BIND_WHEN_PICKED_UP)
                    continue;

                // race wrong item skip always
                if ((pProto->Flags2 & 1) && m_bot->GetTeam() != HORDE)//Horde only
                    continue;

                if ((pProto->Flags2 & 2) && m_bot->GetTeam() != ALLIANCE)//Alliance only
                    continue;

                if ((pProto->AllowableRace & m_bot->getRaceMask()) == 0)
                    continue;

                //// possible item coverting for BoA case
                //if (pProto->Flags == ITEM_PROTO_FLAG_BIND_TO_ACCOUNT)
                //    // convert if can use and then buy
                //    if (pProto->RequiredReputationFaction && uint32(m_bot->GetReputationRank(pProto->RequiredReputationFaction)) >= pProto->RequiredReputationRank)
                //        // checked at convert data loading as existed
                //        if (uint32 newItemId = sObjectMgr->GetItemConvert(itemid, m_bot->getRaceMask()))
                //            pProto = sObjectMgr->GetItemTemplate(newItemId);
                m_bot->BuyItemFromVendorSlot(vendorguid, vendorslot, itemid, 1, NULL_BAG, NULL_SLOT);
                return;
            }
        }
    }
}

void PlayerbotAI::Sell(const uint32 itemid)
{
    Item* pItem = FindItem(itemid);
    if (pItem)
    {
        std::ostringstream report;

        uint32 cost = pItem->GetCount() * pItem->GetTemplate()->SellPrice;
        m_bot->ModifyMoney(cost);
        m_bot->MoveItemFromInventory(pItem->GetBagSlot(), pItem->GetSlot(), true);
        m_bot->AddItemToBuyBackSlot(pItem);

        report << "Sold ";
        MakeItemLink(pItem, report, true);
        report << " for ";

        report << Cash(cost);

        TellMaster(report.str());
    }
}

void PlayerbotAI::SellGarbage(Player & /*plyer*/, bool /*bListNonTrash*/, bool bDetailTrashSold, bool bVerbose)
{
    uint32 SoldCost = 0;
    uint32 SoldQuantity = 0;
    std::ostringstream report, goods;
    ChatHandler ch(m_master->GetSession());

    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item* const item = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item)
            _doSellItem(item, report, goods, SoldCost, SoldQuantity);
    }

    uint8 notempty = 0;
    if (goods.str().size() != 0)
    {
        notempty = 1;
        TellMaster("Heres a list of items I can sell:");
    }
    // and each of our other packs
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag) // check for extra bags
    {
        // we want to output the item list to links one bag at a time and clear it, to prevent the list from overloading
        if (goods.str().size() != 0) // This will be one bag behind in the check. if the previous bag listed anything, llist that now and clear the list
        {
            if (notempty == 0)
            {
                TellMaster("Heres a list of items I can sell:");
                notempty = 1; // at least one bag must have had something in it, used at end of this function
            }
            else
            {
                ch.SendSysMessage(goods.str().c_str()); // previous bags list contents, including main backpack first.
                goods.str(""); // clear the list for next bag
            }
        }

        const Bag* const pBag = static_cast<Bag *>(m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag));
        if (pBag)
        {
            // Very nice, but who cares what bag it's in?
            //const ItemTemplate* const pBagProto = pBag->GetTemplate();
            //std::string bagName = pBagProto->Name1;
            //ItemLocalization(bagName, pBagProto->ItemId);
            //goods << "\nIn my " << bagName << ":";

            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* const item = m_bot->GetItemByPos(bag, slot);
                if (item)
                    _doSellItem(item, report, goods, SoldCost, SoldQuantity);
            }
        }
    }

    if (goods.str().size() != 0) // This will make sure items in the last bag were output to links
    {
        ch.SendSysMessage(goods.str().c_str());
        goods.str(""); // clear the list
        notempty = 1; // at least one bag must have had something in it, used at end of this function
    }
    if (notempty == 1)
        TellMaster("All of the above items could be sold"); // links are complete, notify master

    if (!bDetailTrashSold) // no trash got sold
        report.str("");  // clear ostringstream

    if (SoldCost > 0)
    {
        if (bDetailTrashSold)
            report << "Sold total " << SoldQuantity << " item(s) for ";
        else
            report << "Sold " << SoldQuantity << " trash item(s) for ";
        report << Cash(SoldCost);

        if (bVerbose)
            TellMaster(report.str());
        if (SellWhite == 1)
            SellWhite = 0;
    }

    // For all bags, non-gray sellable items
    if (bVerbose)
    {
        if (SoldQuantity == 0 && notempty == 0)
            TellMaster("No items to sell, trash or otherwise.");
    }
}

std::string PlayerbotAI::DropItem(const uint32 itemid)
{
    Item* pItem = FindItem(itemid);
    if (pItem)
    {
        std::ostringstream report;

        // Yea, that's right, get the item info BEFORE you destroy it :)
        MakeItemText(pItem, report, true);

        m_bot->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);

        return report.str();
    }

    return "";
}

void PlayerbotAI::GetTaxi(uint64 guid, BotTaxiNode& nodes)
{
    // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: GetTaxi - %s node[0] %d node[1] %d", m_bot->GetName(), nodes[0], nodes[1]);

    Creature *unit = m_bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!unit)
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: GetTaxi - %u not found or you can't interact with it.", guid);
        return;
    }

    if (m_bot->m_taxi.IsTaximaskNodeKnown(nodes[0]) ? 0 : 1)
        return;

    if (m_bot->m_taxi.IsTaximaskNodeKnown(nodes[nodes.size() - 1]) ? 0 : 1)
        return;

    if (m_bot->GetPlayerbotAI()->GetMovementOrder() != MOVEMENT_STAY)
    {
        m_taxiNodes = nodes;
        m_taxiMaster = guid;
        SetState(BOTSTATE_FLYING);
    }
}

// handle commands sent through chat channels
void PlayerbotAI::HandleCommand(const std::string& text, Player& fromPlayer)
{
    // prevent bot task spam
    m_inventory_full = false;
    m_tasks.unique();
    m_findNPC.unique();

    // sLog->outDebug(LOG_FILTER_NONE, "chat(%s)",text.c_str());

    // ignore any messages from Addons
    if (text.empty() ||
        text.find("X-Perl") != std::wstring::npos ||
        text.find("HealBot") != std::wstring::npos ||
        text.find("LOOT_OPENED") != std::wstring::npos ||
        text.find("CTRA") != std::wstring::npos ||
        text.find("GathX") == 0) // Gatherer
        return;

    // if message is not from a player in the masters account auto reply and ignore
    if (!canObeyCommandFrom(fromPlayer))
    {
        // only tell the player once instead of endlessly nagging them
        if (m_ignorePlayersChat.find(fromPlayer.GetGUID()) == m_ignorePlayersChat.end())
        {
            std::string msg = "I can't talk to you. Please speak to my master ";
            msg += m_master->GetName();
            SendWhisper(msg, fromPlayer);
            m_bot->HandleEmoteCommand(EMOTE_ONESHOT_NO);
            m_ignorePlayersChat.insert(fromPlayer.GetGUID());
        }
        return;
    }

    // Passed along to ExtractCommand, if (sub)command is found "input" will only contain the rest of the string (or "")
    std::string input = text.c_str();

    // if in the middle of a trade, and player asks for an item/money
    // WARNING: This makes it so you can't use any other commands during a trade!
    if (m_bot->GetTrader() && m_bot->GetTrader()->GetGUID() == fromPlayer.GetGUID())
    {
        uint32 copper = extractMoney(text);
        if (copper > 0)
            TradeCopper(copper);

        std::list<uint32> itemIds;
        extractItemIds(text, itemIds);
        if (itemIds.size() == 0)
            SendWhisper("Show me what item you want by shift clicking the item in the chat window.", fromPlayer);
        else if (!strncmp(text.c_str(), "nt ", 3))
        {
            if (itemIds.size() > 1)
                SendWhisper("There is only one 'Will not be traded' slot. Shift-click just one item, please!", fromPlayer);
            else
            {
                std::list<Item*> itemList;
                findItemsInEquip(itemIds, itemList);
                findItemsInInv(itemIds, itemList);
                if (itemList.size() > 0)
                    TradeItem((**itemList.begin()), TRADE_SLOT_NONTRADED);
                else
                    SendWhisper("I do not have this item equipped or in my bags!", fromPlayer);
            }
        }
        else
        {
            std::list<Item*> itemList;
            findItemsInInv(itemIds, itemList);
            for (std::list<Item*>::iterator it = itemList.begin(); it != itemList.end(); ++it)
                TradeItem(**it);
        }
    }

    else if (ExtractCommand("help", input))
        _HandleCommandHelp(input, fromPlayer);

    else if (fromPlayer.GetSession()->GetSecurity() > SEC_PLAYER && ExtractCommand("gm", input))
        _HandleCommandGM(input, fromPlayer);

    else if (ExtractCommand("reset", input))
        _HandleCommandReset(input, fromPlayer);
    else if (ExtractCommand("orders", input))
        _HandleCommandOrders(input, fromPlayer);
    else if (ExtractCommand("follow", input) || ExtractCommand("come", input))
        _HandleCommandFollow(input, fromPlayer);
    else if (ExtractCommand("stay", input) || ExtractCommand("stop", input))
        _HandleCommandStay(input, fromPlayer);
    else if (ExtractCommand("attack", input))
        _HandleCommandAttack(input, fromPlayer);

    else if (ExtractCommand("cast", input, true)) // true -> "cast" OR "c"
        _HandleCommandCast(input, fromPlayer);

    else if (ExtractCommand("sell", input))
        _HandleCommandSell(input, fromPlayer);

    else if (ExtractCommand("buy", input))
        _HandleCommandBuy(input, fromPlayer);

    else if (ExtractCommand("drop", input))
        _HandleCommandDrop(input, fromPlayer);

    else if (ExtractCommand("repair", input))
        _HandleCommandRepair(input, fromPlayer);

    else if (ExtractCommand("auction", input))
        _HandleCommandAuction(input, fromPlayer);

    else if (ExtractCommand("mail", input))
        _HandleCommandMail(input, fromPlayer);

    else if (ExtractCommand("bank", input))
        _HandleCommandBank(input, fromPlayer);

    else if (ExtractCommand("talent", input))
        _HandleCommandTalent(input, fromPlayer);

    else if (ExtractCommand("use", input, true)) // true -> "use" OR "u"
        _HandleCommandUse(input, fromPlayer);

    else if (ExtractCommand("equip", input, true)) // true -> "equip" OR "e"
        _HandleCommandEquip(input, fromPlayer);

    else if (ExtractCommand("autoequip", input, true)) // switches autoequip on or off if on already
        _HandleCommandAutoEquip(input, fromPlayer);

    // find project: 20:50 02/12/10 rev.4 item in world and wait until ordered to follow
    else if (ExtractCommand("find", input, true)) // true -> "find" OR "f"
        _HandleCommandFind(input, fromPlayer);

    // get project: 20:50 02/12/10 rev.4 compact edition, handles multiple linked gameobject & improves visuals
    else if (ExtractCommand("get", input, true)) // true -> "get" OR "g"
        _HandleCommandGet(input, fromPlayer);

    // Handle all collection related commands here
    else if (ExtractCommand("collect", input))
        _HandleCommandCollect(input, fromPlayer);

    else if (ExtractCommand("quest", input))
        _HandleCommandQuest(input, fromPlayer);

    else if (ExtractCommand("craft", input))
        _HandleCommandCraft(input, fromPlayer);

    else if (ExtractCommand("enchant", input))
        _HandleCommandEnchant(input, fromPlayer);

    else if (ExtractCommand("process", input))
        _HandleCommandProcess(input, fromPlayer);

    else if (ExtractCommand("pet", input))
        _HandleCommandPet(input, fromPlayer);

    else if (ExtractCommand("spells", input))
        _HandleCommandSpells(input, fromPlayer);

    // survey project: 20:50 02/12/10 rev.4 compact edition
    else if (ExtractCommand("survey", input))
        _HandleCommandSurvey(input, fromPlayer);

    // Handle class & professions training:
    else if (ExtractCommand("skill", input))
        _HandleCommandSkill(input, fromPlayer);

    // stats project: 11:30 15/12/10 rev.2 display bot statistics
    else if (ExtractCommand("stats", input))
        _HandleCommandStats(input, fromPlayer);

    else
    {
        // if this looks like an item link, reward item it completed quest and talking to NPC
        std::list<uint32> itemIds;
        extractItemIds(text, itemIds);
        if (!itemIds.empty()) {
            uint32 itemId = itemIds.front();
            bool wasRewarded = false;
            uint64 questRewarderGUID = m_bot->GetSelection();
            Object* pNpc = sObjectAccessor->GetObjectByTypeMask(*m_bot, questRewarderGUID, TYPEMASK_UNIT|TYPEMASK_GAMEOBJECT);
            if (!pNpc)
                return;

            QuestMenu& questMenu = m_bot->PlayerTalkClass->GetQuestMenu();
            for (uint32 iI = 0; !wasRewarded && iI < questMenu.GetMenuItemCount(); ++iI)
            {
                QuestMenuItem const& qItem = questMenu.GetItem(iI);

                uint32 questID = qItem.QuestId;
                Quest const* pQuest = sObjectMgr->GetQuestTemplate(questID);
                QuestStatus status = m_bot->GetQuestStatus(questID);

                // if quest is complete, turn it in
                if (status == QUEST_STATUS_COMPLETE &&
                    !m_bot->GetQuestRewardStatus(questID) &&
                    pQuest->GetRewChoiceItemsCount() > 1 &&
                    m_bot->CanRewardQuest(pQuest, false))
                    for (uint8 rewardIdx = 0; !wasRewarded && rewardIdx < pQuest->GetRewChoiceItemsCount(); ++rewardIdx)
                    {
                        ItemTemplate const * const pRewardItem = sObjectMgr->GetItemTemplate(pQuest->RewardChoiceItemId[rewardIdx]);
                        if (itemId == pRewardItem->ItemId)
                        {
                            m_bot->RewardQuest(pQuest, rewardIdx, pNpc, false);

                            std::string questTitle  = pQuest->GetTitle();
                            QuestLocalization(questTitle, questID);
                            std::string itemName = pRewardItem->Name1;
                            ItemLocalization(itemName, pRewardItem->ItemId);

                            std::ostringstream out;
                            out << "|cffffffff|Hitem:" << pRewardItem->ItemId << ":0:0:0:0:0:0:0" << "|h[" << itemName << "]|h|r rewarded";
                            SendWhisper(out.str(), fromPlayer);
                            wasRewarded = true;
                        }
                    }
            }
        }
        else
        {
            // TODO: make this only in response to direct whispers (chatting in party chat can in fact be between humans)
            std::string msg = "What? For a list of commands, ask for 'help'.";
            SendWhisper(msg, fromPlayer);
            m_bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
        }
    }
}

/**
 * ExtractCommand looks for a command in a text string
 * sLookingFor       - string you're looking for (e.g. "help")
 * text              - string which may or may not start with sLookingFor
 * bUseShort         - does this command accept the shorthand command? If true, "help" would ALSO look for "h"
 *
 * returns true if the string has been found
 * returns false if the string has not been found
 */
bool PlayerbotAI::ExtractCommand(const std::string sLookingFor, std::string &text, bool bUseShort)
{
    // ("help" + " ") < "help X"  AND  text's start (as big as sLookingFor) == sLookingFor
    // Recommend AGAINST adapting this for non-space situations (thinking MangosZero)
    // - unknown would risk being (short for "use") 'u' + "nknown"
    if (sLookingFor.size() + 1 < text.size() && text.at(sLookingFor.size()) == ' '
        && 0 == text.substr(0, sLookingFor.size()).compare(sLookingFor))
    {
        text = text.substr(sLookingFor.size() + 1);
        return true;
    }

    if (0 == text.compare(sLookingFor))
    {
        text = "";
        return true;
    }

    if (bUseShort)
    {
        if (text.size() > 1 && sLookingFor.at(0) == text.at(0) && text.at(1) == ' ')
        {
            text = text.substr(2);
            return true;
        }
        else if (text.size() == 1 && sLookingFor.at(0) == text.at(0))
        {
            text = "";
            return true;
        }
    }

    return false;
}

void PlayerbotAI::_HandleCommandReset(std::string &text, Player &fromPlayer)
{
    if (text != "")
    {
        SendWhisper("reset does not have a subcommand.", fromPlayer);
        return;
    }
    SetState(BOTSTATE_NORMAL);
    MovementReset();
    SetQuestNeedItems();
    SetQuestNeedCreatures();
    UpdateAttackerInfo();
    m_lootTargets.clear();
    m_lootCurrent = 0;
    m_targetCombat = 0;
    ClearActiveTalentSpec();
}

void PlayerbotAI::_HandleCommandOrders(std::string &text, Player &fromPlayer)
{
    if (text != "")
    {
        SendWhisper("orders cannot have a subcommand.", fromPlayer);
        return;
    }
    SendOrders(*m_master);
}

void PlayerbotAI::_HandleCommandFollow(std::string &text, Player &fromPlayer)
{
    if (ExtractCommand("auto", text)) // switch to automatic follow distance
    {
        if (text != "")
        {
            SendWhisper("Invalid subcommand for 'follow'", fromPlayer);
            return;
        }
        DistOverRide = 0; // this resets follow distance to config default
        IsUpOrDown = 0;
        std::ostringstream msg;
        gTempDist = 1;
        gTempDist2 = 2;

        if (FollowAutoGo != 2)
        {
            FollowAutoGo = 1;
            msg << "Automatic Follow Distance is now ON";
            SendWhisper(msg.str(),fromPlayer);
            return;
        }
        else
        {
            FollowAutoGo = 0;
            msg << "Automatic Follow Distance is now OFF";
            SendWhisper(msg.str(),fromPlayer);
        }
        SetMovementOrder(MOVEMENT_FOLLOW, m_master);
        return;
    }
    if (ExtractCommand("reset", text)) // switch to reset follow distance
    {
        if (text != "")
        {
            SendWhisper("Invalid subcommand for 'follow'", fromPlayer);
            return;
        }
        DistOverRide = 0; // this resets follow distance to config default
        IsUpOrDown = 0;
        std::ostringstream msg;
        gTempDist = 1;
        gTempDist2 = 2;
        msg << "Bit crowded isn't it?";
        SendWhisper(msg.str(),fromPlayer);
        SetMovementOrder(MOVEMENT_FOLLOW, m_master);
        return;
    }
    if (ExtractCommand("far", text)) // switch to increment follow distance
    {
        if (text != "")
        {
            SendWhisper("Invalid subcommand for 'follow'", fromPlayer);
            return;
        }
        DistOverRide = (DistOverRide + 1); // this increments follow distance
        std::ostringstream msg;
        msg << "Increasing My follow distance";
        SendWhisper(msg.str(),fromPlayer);
        SetMovementOrder(MOVEMENT_FOLLOW, m_master);
        return;
    }
    if (ExtractCommand("near", text)) // switch to increment follow distance
    {
        if (text != "")
        {
            SendWhisper("Invalid subcommand for 'follow'", fromPlayer);
            return;
        }
        if (DistOverRide > 0)
            DistOverRide = (DistOverRide - 1); // this increments follow distance,

        std::ostringstream msg;
        if (DistOverRide == 0)
        {
            IsUpOrDown = 0;
            DistOverRide = 0;
            gTempDist = 1;
            gTempDist2 = 2;
            msg << "I'm NOT getting any closer than this";
        }
        if (DistOverRide != 0)
            msg << "Decreasing My follow distance";
        SendWhisper(msg.str(),fromPlayer);
        SetMovementOrder(MOVEMENT_FOLLOW, m_master);
        return;
    }
    if (text != "")
    {
        SendWhisper("see help for details on using follow.", fromPlayer);
        return;
    }

    SetMovementOrder(MOVEMENT_FOLLOW, m_master);
}

void PlayerbotAI::_HandleCommandStay(std::string &text, Player &fromPlayer)
{
    if (text != "")
    {
        SendWhisper("stay cannot have a subcommand.", fromPlayer);
        return;
    }
    SetMovementOrder(MOVEMENT_STAY);
}

void PlayerbotAI::_HandleCommandAttack(std::string &text, Player &fromPlayer)
{
    if (text != "")
    {
        SendWhisper("attack cannot have a subcommand.", fromPlayer);
        return;
    }
    uint64 attackOnGuid = fromPlayer.GetSelection();
    if (attackOnGuid)
    {
        if (Unit * thingToAttack = sObjectAccessor->GetUnit(*m_bot, attackOnGuid))
            if (!m_bot->IsFriendlyTo(thingToAttack) && m_bot->IsWithinLOSInMap(thingToAttack))
                GetCombatTarget(thingToAttack);
    }
    else
    {
        SendWhisper("No target is selected.", fromPlayer);
        m_bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
    }
}

void PlayerbotAI::_HandleCommandCast(std::string &text, Player &fromPlayer)
{
    if (text == "")
    {
        SendWhisper("cast must be used with a single spell link (shift + click the spell).", fromPlayer);
        return;
    }

    std::string spellStr = text;
    uint32 spellId = uint32(atol(spellStr.c_str()));

    // try and get spell ID by name
    if (spellId == 0)
    {
        spellId = getSpellId(spellStr.c_str(), true);

        // try link if text NOT (spellid OR spellname)
        if (spellId == 0)
            extractSpellId(text, spellId);
    }

    if (m_bot->HasAura(spellId))
    {
        m_bot->RemoveAurasDueToSpell(spellId, m_bot->GetGUID());
        return;
    }

    uint64 castOnGuid = fromPlayer.GetSelection();
    if (spellId != 0 && m_bot->HasSpell(spellId))
    {
        m_spellIdCommand = spellId;
        if (castOnGuid)
            m_targetGuidCommand = castOnGuid;
        else
            m_targetGuidCommand = m_bot->GetGUID();
    }
}

// _HandleCommandSell: Handle selling items
// sell [Item Link][Item Link] .. -- Sells bot(s) items from inventory
void PlayerbotAI::_HandleCommandSell(std::string &text, Player &fromPlayer)
{
    if (ExtractCommand("all", text)) // switch to auto sell low level white items
    {
        if (text != "")
        {
            SendWhisper("Invalid subcommand for 'sell all'", fromPlayer);
            return;
        }
        SellWhite = 1; // this gets reset once sale is complete.  for testing purposes
        std::ostringstream msg;
        msg << "I will sell all my low level normal items the next time you sell.";
        SendWhisper(msg.str(),fromPlayer);
        return;
    }
    if (text == "")
    {
        SendWhisper("sell must be used with one or more item links (shift + click the item).", fromPlayer);
        return;
    }

    enum NPCFlags VENDOR_MASK = (enum NPCFlags) (UNIT_NPC_FLAG_VENDOR
                                                 | UNIT_NPC_FLAG_VENDOR_AMMO
                                                 | UNIT_NPC_FLAG_VENDOR_FOOD
                                                 | UNIT_NPC_FLAG_VENDOR_POISON
                                                 | UNIT_NPC_FLAG_VENDOR_REAGENT);

    std::list<uint32> itemIds;
    extractItemIds(text, itemIds);
    for (std::list<uint32>::iterator it = itemIds.begin(); it != itemIds.end(); ++it)
        m_tasks.push_back(std::pair<enum TaskFlags, uint32>(SELL_ITEMS, *it));
    m_findNPC.push_back(VENDOR_MASK);
}

// _HandleCommandBuy: Handle buying items
// buy [Item Link][Item Link] .. -- Buys items from vendor
void PlayerbotAI::_HandleCommandBuy(std::string &text, Player &fromPlayer)
{
    if (text == "")
    {
        SendWhisper("buy must be used with one or more item links (shift + click the item).", fromPlayer);
        return;
    }

    uint64 vendorguid = fromPlayer.GetSelection();
    if (!vendorguid)
    {
        SendWhisper("No vendor is selected.", fromPlayer);
        m_bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
        return;
    }

    std::list<uint32> itemIds;
    extractItemIds(text, itemIds);
    for (std::list<uint32>::iterator it = itemIds.begin(); it != itemIds.end(); ++it)
        Buy(vendorguid, *it);
}

// _HandleCommandDrop: Handle dropping items
// drop [Item Link][Item Link] .. -- Drops item(s) from bot's inventory
void PlayerbotAI::_HandleCommandDrop(std::string &text, Player &fromPlayer)
{
    if (text == "")
    {
        SendWhisper("drop must be used with one or more item links (shift + click the item).", fromPlayer);
        return;
    }

    std::ostringstream report;
    std::list<uint32> itemIds;
    extractItemIds(text, itemIds);
    report << "Dropped ";
    for (std::list<uint32>::iterator it = itemIds.begin(); it != itemIds.end(); ++it)
        report << DropItem(*it);

    if (report.str() == "Dropped ")
    {
        SendWhisper("No items were dropped. It would appear something has gone hinky.", fromPlayer);
        return;
    }

    report << ".";
    SendWhisper(report.str(), fromPlayer);
}

// _HandleCommandRepair: Handle repair items
// repair  all                      -- repair all bot(s) items
// repair [Item Link][Item Link] .. -- repair select bot(s) items
void PlayerbotAI::_HandleCommandRepair(std::string &text, Player &fromPlayer)
{
    if (ExtractCommand("all", text))
    {
        if (text != "")
        {
            SendWhisper("Invalid subcommand for 'repair all'", fromPlayer);
            return;
        }
        m_tasks.push_back(std::pair<enum TaskFlags, uint32>(REPAIR_ITEMS, 0));
        m_findNPC.push_back(UNIT_NPC_FLAG_REPAIR);
        return;
    }

    std::list<uint32> itemIds;
    extractItemIds(text, itemIds);

    for (std::list<uint32>::iterator it = itemIds.begin(); it != itemIds.end(); it++)
    {
        m_tasks.push_back(std::pair<enum TaskFlags, uint32>(REPAIR_ITEMS, *it));
        m_findNPC.push_back(UNIT_NPC_FLAG_REPAIR);
    }
}

// _HandleCommandAuction: Handle auctions:
// auction                                        -- Lists bot(s) active auctions.
// auction add [Item Link][Item Link] ..          -- Create bot(s) active auction.
// auction remove [Auction Link][Auction Link] .. -- Cancel bot(s) active auction. ([Auction Link] from auction)
void PlayerbotAI::_HandleCommandAuction(std::string &text, Player &fromPlayer)
{
    if (text == "")
        m_findNPC.push_back(UNIT_NPC_FLAG_AUCTIONEER); // list all bot auctions
    else if (ExtractCommand("add", text))
    {
        std::list<uint32> itemIds;
        extractItemIds(text, itemIds);
        for (std::list<uint32>::iterator it = itemIds.begin(); it != itemIds.end(); ++it)
            m_tasks.push_back(std::pair<enum TaskFlags, uint32>(ADD_AUCTION, *it));
        m_findNPC.push_back(UNIT_NPC_FLAG_AUCTIONEER);
    }
    else if (ExtractCommand("remove", text))
    {
        std::list<uint32> auctionIds;
        extractAuctionIds(text, auctionIds);
        for (std::list<uint32>::iterator it = auctionIds.begin(); it != auctionIds.end(); ++it)
            m_tasks.push_back(std::pair<enum TaskFlags, uint32>(REMOVE_AUCTION, *it));
        m_findNPC.push_back(UNIT_NPC_FLAG_AUCTIONEER);
    }
    else
        SendWhisper("I don't understand what you're trying to do", fromPlayer);
}

void PlayerbotAI::_HandleCommandMail(std::string &text, Player &fromPlayer)
{
    ChatHandler ch(fromPlayer.GetSession());

    if (text == "")
    {
        ch.SendSysMessage("Syntax: mail <inbox [Mailbox] | getcash [mailid].. | getitem [mailid].. | delete [mailid]..>");
        return;
    }
    else if (ExtractCommand("inbox", text))
    {
        uint32 mail_count = 0;
        extractGOinfo(text, m_lootTargets);

        if (m_lootTargets.empty())
        {
            ch.SendSysMessage("Syntax: mail <inbox [Mailbox]>");
            return;
        }

        uint64 m_mailboxGuid = m_lootTargets.front();
        m_lootTargets.pop_front();
        m_lootTargets.clear();

        if (!m_bot->GetGameObjectIfCanInteractWith(m_mailboxGuid, GAMEOBJECT_TYPE_MAILBOX))
        {
            Announce(CANT_USE_TOO_FAR);
            return;
        }

        TellMaster("Inbox:\n");

        for (PlayerMails::iterator itr = m_bot->GetMailBegin(); itr != m_bot->GetMailEnd(); ++itr)
        {
            std::ostringstream msg;
            ++mail_count;

            msg << "|cffffcccc|Hmail:" << (*itr)->messageID << "|h[" << (*itr)->messageID << "]|h|r ";

            switch ((*itr)->messageType)
            {
                case MAIL_NORMAL:
                {
                    msg << "|cffffffff"; // white
                    if ((*itr)->subject != "")
                        msg << "Subject: " << (*itr)->subject << "\n";

                    if ((*itr)->body != "")
                        msg << (*itr)->body << "\n";
                    break;
                }
                case MAIL_CREATURE:
                    msg << "|cffccffccMAIL_CREATURE\n"; // green
                    break;
                case MAIL_GAMEOBJECT:
                    msg << "|cffccffccMAIL_GAMEOBJECT\n"; // green
                    break;
                case MAIL_AUCTION:
                {
                    msg << "|cffccffff"; // blue
                    msg << AuctionResult((*itr)->subject, (*itr)->body) << "\n";
                    break;
                }
                case MAIL_ITEM:
                    msg << "|cffccffccMAIL_ITEM\n"; // green
                    break;
            }

            if ((*itr)->money)
                msg << "[To Collect: " << Cash((*itr)->money) << " ]\n";

            uint8 item_count = (*itr)->items.size(); // max count is MAX_MAIL_ITEMS (12)
            if (item_count > 0)
            {
                msg << "Items: ";
                for (uint8 i = 0; i < item_count; ++i)
                {
                    Item *item = m_bot->GetMItem((*itr)->items[i].item_guid);
                    if (item)
                        MakeItemLink(item, msg, true);
                }
            }
            msg << "\n";
            ch.SendSysMessage(msg.str().c_str());
        }

        if (mail_count == 0)
            ch.SendSysMessage("|cff009900My inbox is empty.");
    }
    else if (ExtractCommand("getcash", text))
    {
        std::ostringstream msg;
        std::list<uint32> mailIds;
        extractMailIds(text, mailIds);
        uint32 total = 0;

        if (mailIds.empty())
        {
            ch.SendSysMessage("Syntax: mail <getcash [mailId]..>");
            return;
        }

        for (std::list<uint32>::iterator it = mailIds.begin(); it != mailIds.end(); ++it)
        {
            Mail* m = m_bot->GetMail(*it);
            if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
            {
                m_bot->SendMailResult(*it, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
                return;
            }

            m_bot->SendMailResult(*it, MAIL_MONEY_TAKEN, MAIL_OK);
            m_bot->ModifyMoney(m->money);
            total += m->money;
            m->money = 0;
            m->state = MAIL_STATE_CHANGED;
            m_bot->m_mailsUpdated = true;
            m_bot->UpdateMail();
        }
        if (total > 0)
        {
            msg << "|cff009900" << "I received: |r" << Cash(total);
            ch.SendSysMessage(msg.str().c_str());
        }
    }
    else if (ExtractCommand("getitem", text))
    {
        std::list<uint32> mailIds;
        extractMailIds(text, mailIds);

        if (mailIds.empty())
        {
            ch.SendSysMessage("Syntax: mail <getitem [mailId]..>");
            return;
        }

        for (std::list<uint32>::iterator it = mailIds.begin(); it != mailIds.end(); it++)
        {
            Mail* m = m_bot->GetMail(*it);
            if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
            {
                m_bot->SendMailResult(*it, MAIL_ITEM_TAKEN, MAIL_ERR_INTERNAL_ERROR);
                return;
            }

            // prevent cheating with skip client money check
            if (m_bot->GetMoney() < m->COD)
            {
                m_bot->SendMailResult(*it, MAIL_ITEM_TAKEN, MAIL_ERR_NOT_ENOUGH_MONEY);
                return;
            }

            if (m->HasItems())
            {
                bool has_items = true;
                std::ostringstream msg;

                msg << "|cff009900" << "I received item: |r";
                for (MailItemInfoVec::const_iterator itr = m->items.begin(); itr != m->items.end(); )
                {
                    has_items = true;
                    Item *item = m_bot->GetMItem(itr->item_guid);
                    if (!item)
                    {
                        ch.SendSysMessage("item not found");
                        return;
                    }

                    ItemPosCountVec dest;

                    InventoryResult res = m_bot->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
                    if (res == EQUIP_ERR_OK)
                    {
                        m->removedItems.push_back(itr->item_guid);

                        if (m->COD > 0)  // if there is COD, take COD money from player and send them to sender by mail
                        {
                            uint64 sender_guid = MAKE_NEW_GUID(m->sender, 0, HIGHGUID_PLAYER);
                            Player *sender = sObjectAccessor->FindPlayer(sender_guid);

                            uint32 sender_accId = 0;

                            if (m_master->GetSession()->GetSecurity() > SEC_PLAYER && sWorld->getBoolConfig(CONFIG_GM_LOG_TRADE))
                            {
                                std::string sender_name;
                                if (sender)
                                {
                                    sender_accId = sender->GetSession()->GetAccountId();
                                    sender_name = sender->GetName();
                                }
                                else if (sender_guid)
                                {
                                    // can be calculated early
                                    sender_accId = sObjectMgr->GetPlayerAccountIdByGUID(sender_guid);

                                    if (!sObjectMgr->GetPlayerNameByGUID(sender_guid, sender_name))
                                        sender_name = sObjectMgr->GetTrinityStringForDBCLocale(LANG_UNKNOWN);
                                }
                                //sLog->outCommand(m_master->GetSession()->GetAccountId(), "GM %s (Account: %u) receive mail item: %s (Entry: %u Count: %u) and send COD money: %u to player: %s (Account: %u)",
                                                //m_master->GetSession()->GetPlayerName(), m_master->GetSession()->GetAccountId(), item->GetTemplate()->Name1, item->GetEntry(), item->GetCount(), m->COD, sender_name.c_str(), sender_accId);
                            }
                            else if (!sender)
                                sender_accId = sObjectMgr->GetPlayerAccountIdByGUID(sender_guid);

                            // check player existence
                            SQLTransaction trans = CharacterDatabase.BeginTransaction();
                            if (sender || sender_accId)
                                MailDraft(m->subject, "")
                                .AddMoney(m->COD)
                                .SendMailTo(trans, MailReceiver(sender, sender_guid), m_bot, MAIL_CHECK_MASK_COD_PAYMENT);
                            CharacterDatabase.CommitTransaction(trans);
                            m_bot->ModifyMoney(-int32(m->COD));
                        }
                        m->COD = 0;
                        m->state = MAIL_STATE_CHANGED;
                        m_bot->m_mailsUpdated = true;
                        m_bot->RemoveMItem(item->GetGUIDLow());

                        uint32 count = item->GetCount(); // save counts before store and possible merge with deleting
                        m_bot->MoveItemToInventory(dest, item, true);
                        m_bot->UpdateMail();
                        m_bot->SendMailResult(*it, MAIL_ITEM_TAKEN, MAIL_OK, 0, itr->item_guid, count);
                        if (m->RemoveItem(itr->item_guid))
                        {
                            MakeItemLink(item, msg, true);
                            has_items = false;
                        }
                    }
                    else
                        m_bot->SendMailResult(*it, MAIL_ITEM_TAKEN, MAIL_ERR_EQUIP_ERROR, res);
                }

                if (!has_items)
                {
                    //SQLTransaction trans = CharacterDatabase.BeginTransaction();
                    CharacterDatabase.PExecute("UPDATE mail SET has_items = 0 WHERE id = %u", *it);
                    //CharacterDatabase.CommitTransaction(trans);
                }
                msg << "\n";
                ch.SendSysMessage(msg.str().c_str());
            }
        }
    }
    else if (ExtractCommand("delete", text))
    {
        std::ostringstream msg;
        std::list<uint32> mailIds;
        extractMailIds(text, mailIds);

        if (mailIds.empty())
        {
            ch.SendSysMessage("Syntax: mail <delete [mailId]..>");
            return;
        }

        msg << "|cff009900Mail ";
        for (std::list<uint32>::iterator it = mailIds.begin(); it != mailIds.end(); ++it)
        {
            m_bot->m_mailsUpdated = true;

            if (Mail * m = m_bot->GetMail(*it))
            {
                // delete shouldn't show up for COD mails
                if (m->COD)
                {
                    m_bot->SendMailResult(*it, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
                    return;
                }
                m->state = MAIL_STATE_DELETED;
            }

            m_bot->SendMailResult(*it, MAIL_DELETED, MAIL_OK);
            //CharacterDatabase.BeginTransaction();
            CharacterDatabase.PExecute("DELETE FROM mail WHERE id = '%u'", *it);
            CharacterDatabase.PExecute("DELETE FROM mail_items WHERE mail_id = '%u'", *it);
            //CharacterDatabase.CommitTransaction();
            m_bot->RemoveMail(*it);
            msg << "|cffffcccc|h[" << *it << "]|h|r";
        }
        msg << "|cff009900 has been deleted..";
        ch.SendSysMessage(msg.str().c_str());
    }
}

// _HandleCommandBank: Handle bank:
// bank                                        -- Lists bot(s) bank balance.
// bank deposit [Item Link][Item Link] ..      -- Deposit item(s) in bank.
// bank withdraw [Item Link][Item Link] ..     -- Withdraw item(s) from bank. ([Item Link] from bank)
void PlayerbotAI::_HandleCommandBank(std::string &text, Player &fromPlayer)
{
    if (text == "")
        m_findNPC.push_back(UNIT_NPC_FLAG_BANKER); // list all bot balance
    else if (ExtractCommand("deposit", text))
    {
        std::list<uint32> itemIds;
        extractItemIds(text, itemIds);
        for (std::list<uint32>::iterator it = itemIds.begin(); it != itemIds.end(); ++it)
            m_tasks.push_back(std::pair<enum TaskFlags, uint32>(BANK_DEPOSIT, *it));
        m_findNPC.push_back(UNIT_NPC_FLAG_BANKER);
    }
    else if (ExtractCommand("withdraw", text))
    {
        std::list<uint32> itemIds;
        extractItemIds(text, itemIds);
        for (std::list<uint32>::iterator it = itemIds.begin(); it != itemIds.end(); ++it)
            m_tasks.push_back(std::pair<enum TaskFlags, uint32>(BANK_WITHDRAW, *it));
        m_findNPC.push_back(UNIT_NPC_FLAG_BANKER);
    }
    else
        SendWhisper("I don't understand what you're trying to do", fromPlayer);
}

// _HandleCommandTalent: Handle talents & glyphs:
// talent                           -- Lists bot(s) active talents [TALENT LINK] & glyphs [GLYPH LINK], unspent points & cost to reset
// talent learn [TALENT LINK] ..    -- Learn selected talent from bot client 'inspect' dialog -> 'talent' tab or from talent command (shift click icon/link)
// talent reset                     -- Resets all talents
// talent spec                      -- Lists various talentspecs for this bot's class
// talent spec #                    -- Sets talent spec # as active talentspec
void PlayerbotAI::_HandleCommandTalent(std::string &text, Player &fromPlayer)
{
    std::ostringstream out;
    if (ExtractCommand("learn", text))
    {
        std::list<talentPair>talents;
        extractTalentIds(text, talents);

        for (std::list<talentPair>::iterator itr = talents.begin(); itr != talents.end(); ++itr)
        {
            uint32 talentid = itr->first;
            uint32 rank = itr->second;

            m_bot->LearnTalent(talentid, ++rank);
            m_bot->SendTalentsInfoData(false);
            InspectUpdate();
        }

        m_bot->MakeTalentGlyphLink(out);
        SendWhisper(out.str(), fromPlayer);
    }
    else if (ExtractCommand("reset", text))
    {
        m_tasks.push_back(std::pair<enum TaskFlags, uint32>(RESET_TALENTS, 0));
        m_findNPC.push_back(UNIT_NPC_FLAG_TRAINER_CLASS);
    }
    else if (ExtractCommand("spec", text))
    {
        if (0 == GetTalentSpecsAmount())
        {
            //SendWhisper("Database does not contain any Talent Specs (for any classes).", fromPlayer);
            SendWhisper("spec subcommand is disabled. Sorry", fromPlayer);
            return;
        }
        if (text.size() == 0) // no spec chosen nor other subcommand
        {
            std::list<TalentSpec> classSpecs = GetTalentSpecs(long(m_bot->getClass()));
            std::list<TalentSpec>::iterator it;
            int count = 0;

            SendWhisper("Please select a talent spec to activate (reply 'talent spec #'):", fromPlayer);
            for (it = classSpecs.begin(); it != classSpecs.end(); it++)
            {
                count++;

                std::ostringstream oss;
                oss << count << ". " << it->specName;
                SendWhisper(oss.str(), fromPlayer);
            }
            if (count == 0)
            {
                std::ostringstream oss;
                oss << "Error: No TalentSpecs listed. Specs retrieved from DB for this class: %u" << m_bot->getClass();
                SendWhisper(oss.str(), fromPlayer);
            }
        }
        else
        {
            uint32 chosenSpec = strtoul(text.c_str(), NULL, 0); // non-int returns 0; too big returns UINT MAX (or somesuch)

            // Warning: also catches non-int sub2command's - e.g. 'talent spec foobar'
            if (0 == chosenSpec)
            {
                ClearActiveTalentSpec();
                SendWhisper("The talent spec has been cleared.", fromPlayer);
            }
            else if (chosenSpec > GetTalentSpecsAmount(long(m_bot->getClass())))
                SendWhisper("The talent spec you have chosen is invalid. Please select one from the valid range (reply 'talent spec' for options).", fromPlayer);
            else
            {
                TalentSpec ts = GetTalentSpec(long(m_bot->getClass()), chosenSpec);

                // no use setting it to an invalid (and probably - hopefully - empty) TalentSpec
                if (0 != ts.specClass && TSP_NONE != ts.specPurpose)
                {
                    out << "Activated talent spec: " << chosenSpec << ". " << ts.specName;
                    SendWhisper(out.str(), fromPlayer);
                    SetActiveTalentSpec(ts);
                    if (!ApplyActiveTalentSpec())
                        SendWhisper("The talent spec has been set active but could not be applied. It appears something has gone awry.", fromPlayer);
                    //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: Could set TalentSpec but could not apply it - 'talent spec #': Class: %i; chosenSpec: %li", (long)m_bot->getClass(), chosenSpec);
                    InspectUpdate();
                }
                else
                {
                    SendWhisper("An error has occured. Please let a Game Master know. This error has been logged.", fromPlayer);
                    //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: Could not GetTalentSpec to set & apply - 'talent spec #': Class: %i; chosenSpec: %li", (long) m_bot->getClass(), chosenSpec);
                }
            }
        }
    }
    // no valid subcommand found for command 'talent'
    else
    {
        uint32 gold = uint32(m_bot->resetTalentsCost() / 10000);

        if (gold > 0)
            out << "Cost to reset all Talents is " << gold << " |TInterface\\Icons\\INV_Misc_Coin_01:8|t";

        m_bot->MakeTalentGlyphLink(out);
        SendWhisper(out.str(), fromPlayer);
    }
}

void PlayerbotAI::_HandleCommandProcess(std::string &text, Player &fromPlayer)
{
    uint32 spellId;

    if (ExtractCommand("disenchant", text, true)) // true -> "process disenchant" OR "process d"
    {
        if (m_bot->HasSkill(SKILL_ENCHANTING))
            spellId = DISENCHANTING_1;
        else
        {
            SendWhisper("|cffff0000I can't disenchant, I don't have the skill.", fromPlayer);
            return;
        }
    }
    else if (ExtractCommand("mill", text, true)) // true -> "process mill" OR "process m"
    {
        if (m_bot->HasSkill(SKILL_INSCRIPTION))
            spellId = MILLING_1;
        else
        {
            SendWhisper("|cffff0000I can't mill, I don't have the skill.", fromPlayer);
            return;
        }
    }
    else if (ExtractCommand("prospect", text, true)) // true -> "process prospect" OR "process p"
    {
        if (m_bot->HasSkill(SKILL_JEWELCRAFTING) && m_bot->GetPureSkillValue(SKILL_JEWELCRAFTING) >= 20)
            spellId = PROSPECTING_1;
        else
        {
            SendWhisper("|cffff0000I can't prospect, I don't have the skill.", fromPlayer);
            return;
        }
    }
    else
        return;

    std::list<uint32> itemIds;
    std::list<Item*> itemList;
    extractItemIds(text, itemIds);
    findItemsInInv(itemIds, itemList);

    if (itemList.empty())
    {
        SendWhisper("|cffff0000I can't process that!", fromPlayer);
        return;
    }

    Item* reagent = itemList.back();
    itemList.pop_back();

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    if (reagent)
    {
        SpellCastTargets targets;
        m_itemTarget = reagent->GetTemplate()->ItemId;
        targets.SetItemTarget(reagent);
        Spell *spell = new Spell(m_bot, spellInfo, TRIGGERED_NONE);
        spell->prepare(&targets);
    }
}

void PlayerbotAI::_HandleCommandUse(std::string &text, Player &fromPlayer)
{
    std::list<uint32> itemIds;
    std::list<Item*> itemList;
    extractItemIds(text, itemIds);
    findItemsInInv(itemIds, itemList);

    if (itemList.empty())
    {
        SendWhisper("|cffff0000I can't use that!", fromPlayer);
        return;
    }

    Item* tool = itemList.back();
    itemList.pop_back();
    if (tool)
    {
        // set target
        Unit* unit = sObjectAccessor->GetUnit(*m_bot, fromPlayer.GetSelection());
        findItemsInEquip(itemIds, itemList);
        extractGOinfo(text, m_lootTargets);
        // sLog->outDebug(LOG_FILTER_NONE, "tool (%s)",tool->GetTemplate()->Name1);

        if (!itemList.empty())
        {
            Item* itarget = itemList.back();
            if (itarget)
            {
                // sLog->outDebug(LOG_FILTER_NONE, "target (%s)",itarget->GetTemplate()->Name1);
                UseItem(tool, _findItemSlot(itarget)); // on equipped item
                SetState(BOTSTATE_ENCHANT);
                SetIgnoreUpdateTime(1);
            }
        }
        else if (!m_lootTargets.empty())
        {
            uint64 gotarget = m_lootTargets.front();
            m_lootTargets.pop_front();

            GameObject *go = m_bot->GetMap()->GetGameObject(gotarget);
            if (go)
                // sLog->outDebug(LOG_FILTER_NONE, "tool (%s) on target gameobject (%s)",tool->GetTemplate()->Name1,go->GetGOInfo()->name);
                UseItem(tool, TARGET_FLAG_GAMEOBJECT, gotarget); // on gameobject
        }
        else if (unit)
            // sLog->outDebug(LOG_FILTER_NONE, "tool (%s) on selected target unit",tool->GetTemplate()->Name1);
            UseItem(tool, unit); // on unit
        else
            // sLog->outDebug(LOG_FILTER_NONE, "tool (%s) on self",tool->GetTemplate()->Name1);
            UseItem(tool); // on self
    }
    return;
}

void PlayerbotAI::_HandleCommandAutoEquip(std::string &text, Player &fromPlayer)
{
    std::ostringstream msg;
    if (ExtractCommand("now", text, true)) // run autoequip cycle right now
    {
        msg << "Running Auto Equip cycle One time. My current setting is" << (AutoEquipPlug ? "ON" : "OFF");
        SendWhisper(msg.str(),fromPlayer);
        if (AutoEquipPlug == 0)
            AutoEquipPlug = 2;
        Player* const bot = GetPlayerBot();
        AutoUpgradeEquipment(*bot);
        return;
    }
    else if (ExtractCommand("on", text, true)) // true -> "autoequip on"
    {
        AutoEquipPlug = 1;
        msg << "AutoEquip is now ON";
        SendWhisper(msg.str(),fromPlayer);
        return;
    }
    else if (ExtractCommand("off", text, true)) // true -> "autoequip off"
    {
        AutoEquipPlug = 0;
        msg << "AutoEquip is now OFF";
        SendWhisper(msg.str(),fromPlayer);
        return;
    }
    if (AutoEquipPlug != 1)
        AutoEquipPlug = 1;
    else
        AutoEquipPlug = 0;
    msg << "AutoEquip is now " << (AutoEquipPlug ? "ON" : "OFF");
    SendWhisper(msg.str(),fromPlayer);
}

void PlayerbotAI::_HandleCommandEquip(std::string &text, Player & /*fromPlayer*/)
{
    std::list<uint32> itemIds;
    std::list<Item*> itemList;
    extractItemIds(text, itemIds);
    findItemsInInv(itemIds, itemList);
    for (std::list<Item*>::iterator it = itemList.begin(); it != itemList.end(); ++it)
        EquipItem(*it);
    InspectUpdate();
    SendNotEquipList(*m_bot);
}

void PlayerbotAI::_HandleCommandFind(std::string &text, Player & /*fromPlayer*/)
{
    if (text == "")
        return;
    extractGOinfo(text, m_lootTargets);

    if (m_lootTargets.empty())
        return;

    m_lootCurrent = m_lootTargets.front();
    m_lootTargets.pop_front();

    GameObject *go = m_bot->GetMap()->GetGameObject(m_lootCurrent);
    if (!go)
    {
        m_lootTargets.clear();
        m_lootCurrent = 0;
        return;
    }

    SetMovementOrder(MOVEMENT_STAY);
    m_bot->GetMotionMaster()->MovePoint(go->GetMapId(), go->GetPositionX(), go->GetPositionY(), go->GetPositionZ());
    m_lootTargets.clear();
    m_lootCurrent = 0;
}

void PlayerbotAI::_HandleCommandGet(std::string &text, Player &fromPlayer)
{
    if (text != "")
    {
        extractGOinfo(text, m_lootTargets);
        SetState(BOTSTATE_LOOTING);
        return;
    }

    // get a selected lootable corpse
    uint64 getOnGuid = fromPlayer.GetSelection();
    if (getOnGuid)
    {
        Creature *c = m_bot->GetMap()->GetCreature(getOnGuid);
        if (!c)
            return;

        uint32 skillId = 0;
        if (c->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE))
            skillId = c->GetCreatureTemplate()->GetRequiredLootSkill();

        if (c->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE) ||
            (c->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE) && m_bot->HasSkill(skillId)))
        {
            m_lootTargets.push_back(getOnGuid);
            SetState(BOTSTATE_LOOTING);
        }
        else
            SendWhisper("Target is not lootable by me.", fromPlayer);
    }
    else
    {
        SendWhisper("No target is selected.", fromPlayer);
        m_bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
    }
}

void PlayerbotAI::_HandleCommandCollect(std::string &text, Player &fromPlayer)
{
    while (text.size() > 0)
    {
        if (ExtractCommand("combat", text))
            SetCollectFlag(COLLECT_FLAG_COMBAT);
        else if (ExtractCommand("loot", text))
            SetCollectFlag(COLLECT_FLAG_LOOT);
        else if (ExtractCommand("quest", text))
            SetCollectFlag(COLLECT_FLAG_QUEST);
        else if (ExtractCommand("profession", text) || ExtractCommand("skill", text))
            SetCollectFlag(COLLECT_FLAG_PROFESSION);
        else if (ExtractCommand("skin", text) && m_bot->HasSkill(SKILL_SKINNING)) // removes skin even if bot does not have skill
            SetCollectFlag(COLLECT_FLAG_SKIN);
        else if (ExtractCommand("objects", text) || ExtractCommand("nearby", text))
        {
            SetCollectFlag(COLLECT_FLAG_NEAROBJECT);
            if (!HasCollectFlag(COLLECT_FLAG_NEAROBJECT))
                m_collectObjects.clear();
        }
        else if (ExtractCommand("distance:", text))
        {
            uint32 distance;
            sscanf(text.c_str(), "distance:%u", &distance);
            if (distance > 0 && distance <= m_confCollectDistanceMax)
            {
                m_confCollectDistance = distance;
                std::ostringstream oss;
                oss << "I will now collect items within " << m_confCollectDistance << " yards.";
                SendWhisper(oss.str(), fromPlayer);
            }
            else
            {
                m_confCollectDistance = m_confCollectDistanceMax;
                std::stringstream oss;
                oss << "I will now collect items within " << m_confCollectDistanceMax << " yards. " << distance << " yards is just too far away.",
                SendWhisper(oss.str(), fromPlayer);
            }
        }
        else if (ExtractCommand("none", text) || ExtractCommand("nothing", text))
        {
            m_collectionFlags = 0;
            m_collectObjects.clear();
            break;  // because none is an exclusive choice
        }
        else
        {
            std::ostringstream oss;
            oss << "Collect <collectable(s)>: none | distance:<1-" << m_confCollectDistanceMax << ">, combat, loot, quest, profession, objects";
            if (m_bot->HasSkill(SKILL_SKINNING))
                oss << ", skin";
            // TODO: perhaps change the command syntax, this way may be lacking in ease of use
            SendWhisper(oss.str(), fromPlayer);
            break;
        }
    }

    std::string collset = "";
    if (HasCollectFlag(COLLECT_FLAG_LOOT))
        collset += ", all loot";
    if (HasCollectFlag(COLLECT_FLAG_PROFESSION))
        collset += ", profession";
    if (HasCollectFlag(COLLECT_FLAG_QUEST))
        collset += ", quest";
    if (HasCollectFlag(COLLECT_FLAG_SKIN))
        collset += ", skin";
    if (collset.length() > 1)
    {
        if (HasCollectFlag(COLLECT_FLAG_COMBAT))
            collset += " items after combat";
        else
            collset += " items";
    }

    if (HasCollectFlag(COLLECT_FLAG_NEAROBJECT))
    {
        if (collset.length() > 1)
            collset += " and ";
        else
            collset += " ";    // padding for substr
        collset += "nearby objects (";
        if (!m_collectObjects.empty())
        {
            std::string strobjects = "";
            for (BotEntryList::iterator itr = m_collectObjects.begin(); itr != m_collectObjects.end(); ++itr)
            {
                uint32 objectentry = *(itr);
                GameObjectTemplate const * gInfo = sObjectMgr->GetGameObjectTemplate(objectentry);
                strobjects += ", ";
                strobjects += gInfo->name;
            }
            collset += strobjects.substr(2);
        }
        else
            collset += "use survey and get to set";
        collset += ")";
    }

    if (collset.length() > 1)
        SendWhisper("I'm collecting " + collset.substr(2), fromPlayer);
    else
        SendWhisper("I'm collecting nothing.", fromPlayer);
}

void PlayerbotAI::_HandleCommandEnchant(std::string &text, Player &fromPlayer)
{
    // sLog->outDebug(LOG_FILTER_NONE, "Enchant (%s)",text.c_str());

    if (!m_bot->HasSkill(SKILL_ENCHANTING))
    {
        SendWhisper("|cffff0000I can't enchant, I don't have the skill.", fromPlayer);
        return;
    }

    if (text.size() > 0)
    {
        uint32 spellId;
        extractSpellId(text, spellId);

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;

        std::list<uint32> itemIds;
        std::list<Item*> itemList;
        extractItemIds(text, itemIds);
        findItemsInEquip(itemIds, itemList);
        findItemsInInv(itemIds, itemList);

        if (itemList.empty())
        {
            SendWhisper("|cffff0000I can't enchant that!", fromPlayer);
            return;
        }

        Item* iTarget = itemList.back();
        itemList.pop_back();

        if (iTarget)
        {
            SpellCastTargets targets;
            targets.SetItemTarget(iTarget);
            Spell *spell = new Spell(m_bot, spellInfo, TRIGGERED_NONE);
            spell->prepare(&targets);
            SetState(BOTSTATE_ENCHANT);
            SetIgnoreUpdateTime(1);
        }
        return;
    }
    else
    {
        std::ostringstream msg;
        uint32 charges;
        uint32 linkcount = 0;

        m_spellsToLearn.clear();
        m_bot->skill(m_spellsToLearn);
        SendWhisper("I can enchant:\n", fromPlayer);
        ChatHandler ch(fromPlayer.GetSession());
        for (std::list<uint32>::iterator it = m_spellsToLearn.begin(); it != m_spellsToLearn.end(); ++it)
        {
            SkillLineEntry const *SkillLine = sSkillLineStore.LookupEntry(*it);

            if (SkillLine->categoryId == SKILL_CATEGORY_PROFESSION && *it == SKILL_ENCHANTING)
                for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
                {
                    SkillLineAbilityEntry const *SkillAbility = sSkillLineAbilityStore.LookupEntry(j);
                    if (!SkillAbility)
                        continue;

                    SpellEntry const* spellInfo = sSpellStore.LookupEntry(SkillAbility->spellId);
                    if (!spellInfo)
                        continue;

                    if (IsPrimaryProfessionSkill(*it) && spellInfo->Effect[0] != SPELL_EFFECT_ENCHANT_ITEM)
                        continue;

                    if (SkillAbility->skillId == *it && m_bot->HasSpell(SkillAbility->spellId) && SkillAbility->forward_spellid == 0 && ((SkillAbility->classmask & m_bot->getClassMask()) == 0))
                    {
                        MakeSpellLink(spellInfo, msg);
                        ++linkcount;
                        if ((charges = GetSpellCharges(SkillAbility->spellId)) > 0)
                            msg << "[" << charges << "]";
                        if (linkcount >= 10)
                        {
                            ch.SendSysMessage(msg.str().c_str());
                            linkcount = 0;
                            msg.str("");
                        }
                    }
                }
        }
        m_noToolList.unique();
        for (std::list<uint32>::iterator it = m_noToolList.begin(); it != m_noToolList.end(); it++)
            HasTool(*it);
        ch.SendSysMessage(msg.str().c_str());
        m_noToolList.clear();
        m_spellsToLearn.clear();
    }
}

void PlayerbotAI::_HandleCommandCraft(std::string &text, Player &fromPlayer)
{
    // sLog->outDebug(LOG_FILTER_NONE, "Craft (%s)",text.c_str());

    std::ostringstream msg;
    uint32 charges;
    uint32 skill;
    int32 category;
    uint32 linkcount = 0;

    if (ExtractCommand("alchemy", text, true)) // true -> "craft alchemy" OR "craft a"
    {
        if (m_bot->HasSkill(SKILL_ALCHEMY))
        {
            skill = SKILL_ALCHEMY;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("blacksmithing", text, true)) // true -> "craft blacksmithing" OR "craft b"
    {
        if (m_bot->HasSkill(SKILL_BLACKSMITHING))
        {
            skill = SKILL_BLACKSMITHING;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("cooking", text, true)) // true -> "craft cooking" OR "craft c"
    {
        if (m_bot->HasSkill(SKILL_COOKING))
        {
            skill = SKILL_COOKING;
            category = SKILL_CATEGORY_SECONDARY;
        }
        else
            return;
    }
    else if (ExtractCommand("engineering", text, true)) // true -> "craft engineering" OR "craft e"
    {
        if (m_bot->HasSkill(SKILL_ENGINEERING))
        {
            skill = SKILL_ENGINEERING;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("firstaid", text, true)) // true -> "craft firstaid" OR "craft f"
    {
        if (m_bot->HasSkill(SKILL_FIRST_AID))
        {
            skill = SKILL_FIRST_AID;
            category = SKILL_CATEGORY_SECONDARY;
        }
        else
            return;
    }
    else if (ExtractCommand("inscription", text, true)) // true -> "craft inscription" OR "craft i"
    {
        if (m_bot->HasSkill(SKILL_INSCRIPTION))
        {
            skill = SKILL_INSCRIPTION;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("jewelcrafting", text, true)) // true -> "craft jewelcrafting" OR "craft j"
    {
        if (m_bot->HasSkill(SKILL_JEWELCRAFTING))
        {
            skill = SKILL_JEWELCRAFTING;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("leatherworking", text, true)) // true -> "craft leatherworking" OR "craft l"
    {
        if (m_bot->HasSkill(SKILL_LEATHERWORKING))
        {
            skill = SKILL_LEATHERWORKING;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("magic", text, true)) // true -> "craft magic" OR "craft m"
    {
        if (m_bot->HasSkill(SKILL_ENCHANTING))
        {
            skill = SKILL_ENCHANTING;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("smelting", text, true)) // true -> "craft smelting" OR "craft s"
    {
        if (m_bot->HasSkill(SKILL_MINING))
        {
            skill = SKILL_MINING;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else if (ExtractCommand("tailoring", text, true)) // true -> "craft tailoring" OR "craft t"
    {
        if (m_bot->HasSkill(SKILL_TAILORING))
        {
            skill = SKILL_TAILORING;
            category = SKILL_CATEGORY_PROFESSION;
        }
        else
            return;
    }
    else
    {
        uint32 spellId;
        extractSpellId(text, spellId);

        if (!m_bot->HasSpell(spellId))
        {
            SendWhisper("|cffff0000I don't have that spell.", fromPlayer);
            return;
        }

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;

        SpellCastTargets targets;
        Spell *spell = new Spell(m_bot, spellInfo, TRIGGERED_NONE);

        if (text.find("all", 0) != std::string::npos)
        {
            SpellCastResult result = spell->CheckCast(true);

            if (result != SPELL_CAST_OK)
                spell->SendCastResult(result);
            else
            {
                spell->prepare(&targets);
                m_CurrentlyCastingSpellId = spellId;
                SetState(BOTSTATE_CRAFT);
            }
        }
        else
            spell->prepare(&targets);
        return;
    }

    m_spellsToLearn.clear();
    m_bot->skill(m_spellsToLearn);
    SendWhisper("I can create:\n", fromPlayer);
    ChatHandler ch(fromPlayer.GetSession());
    for (std::list<uint32>::iterator it = m_spellsToLearn.begin(); it != m_spellsToLearn.end(); ++it)
    {
        SkillLineEntry const *SkillLine = sSkillLineStore.LookupEntry(*it);

        if (SkillLine->categoryId == category && *it == skill)
            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
            {
                SkillLineAbilityEntry const *SkillAbility = sSkillLineAbilityStore.LookupEntry(j);
                if (!SkillAbility)
                    continue;

                SpellEntry const* spellInfo = sSpellStore.LookupEntry(SkillAbility->spellId);
                if (!spellInfo)
                    continue;

                if (IsPrimaryProfessionSkill(*it) && spellInfo->Effect[0] != SPELL_EFFECT_CREATE_ITEM)
                    continue;

                if (SkillAbility->skillId == *it && m_bot->HasSpell(SkillAbility->spellId) && SkillAbility->forward_spellid == 0 && ((SkillAbility->classmask & m_bot->getClassMask()) == 0))
                {
                    MakeSpellLink(spellInfo, msg);
                    ++linkcount;
                    if ((charges = GetSpellCharges(SkillAbility->spellId)) > 0)
                        msg << "[" << charges << "]";
                    if (linkcount >= 10)
                    {
                        ch.SendSysMessage(msg.str().c_str());
                        linkcount = 0;
                        msg.str("");
                    }
                }
            }
    }
    m_noToolList.unique();
    for (std::list<uint32>::iterator it = m_noToolList.begin(); it != m_noToolList.end(); it++)
        HasTool(*it);
    ch.SendSysMessage(msg.str().c_str());
    m_noToolList.clear();
    m_spellsToLearn.clear();
}

void PlayerbotAI::_HandleCommandQuest(std::string &text, Player &fromPlayer)
{
    std::ostringstream msg;

    if (ExtractCommand("add", text, true)) // true -> "quest add" OR "quest a"
    {
        std::list<uint32> questIds;
        extractQuestIds(text, questIds);
        for (std::list<uint32>::iterator it = questIds.begin(); it != questIds.end(); it++)
            m_tasks.push_back(std::pair<enum TaskFlags, uint32>(TAKE_QUEST, *it));
        m_findNPC.push_back(UNIT_NPC_FLAG_QUESTGIVER);
    }
    else if (ExtractCommand("drop", text, true)) // true -> "quest drop" OR "quest d"
    {
        fromPlayer.SetSelection(m_bot->GetGUID());
        PlayerbotChatHandler ch(m_master);
        int8 linkStart = text.find("|");
        if (text.find("|") != std::string::npos)
        {
            if (!ch.dropQuest((char *) text.substr(linkStart).c_str()))
                ch.sysmessage("ERROR: could not drop quest");
            else
            {
                SetQuestNeedItems();
                SetQuestNeedCreatures();
            }
        }
    }
    else if (ExtractCommand("list", text, true)) // true -> "quest list" OR "quest l"
    {
        m_tasks.push_back(std::pair<enum TaskFlags, uint32>(LIST_QUEST, 0));
        m_findNPC.push_back(UNIT_NPC_FLAG_QUESTGIVER);
    }
    else if (ExtractCommand("report", text))
        SendQuestNeedList();
    else if (ExtractCommand("end", text, true)) // true -> "quest end" OR "quest e"
    {
        m_tasks.push_back(std::pair<enum TaskFlags, uint32>(END_QUEST, 0));
        m_findNPC.push_back(UNIT_NPC_FLAG_QUESTGIVER);
    }
    else
    {
        bool hasIncompleteQuests = false;
        std::ostringstream incomout;
        incomout << "my incomplete quests are:";
        bool hasCompleteQuests = false;
        std::ostringstream comout;
        comout << "my complete quests are:";
        for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            if (uint32 questId = m_bot->GetQuestSlotQuestId(slot))
            {
                Quest const* pQuest = sObjectMgr->GetQuestTemplate(questId);

                std::string questTitle  = pQuest->GetTitle();
                QuestLocalization(questTitle, questId);

                if (m_bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
                {
                    hasCompleteQuests = true;
                    comout << " |cFFFFFF00|Hquest:" << questId << ':' << pQuest->GetQuestLevel() << "|h[" << questTitle << "]|h|r";
                }
                else
                {
                    Item* qitem = FindItem(pQuest->GetSrcItemId());
                    if (qitem)
                        incomout << " use " << "|cffffffff|Hitem:" << qitem->GetTemplate()->ItemId << ":0:0:0:0:0:0:0" << "|h[" << qitem->GetTemplate()->Name1 << "]|h|r" << " on ";
                    hasIncompleteQuests = true;
                    incomout << " |cFFFFFF00|Hquest:" << questId << ':' << pQuest->GetQuestLevel() << "|h[" <<  questTitle << "]|h|r";
                }
            }
        }
        if (hasCompleteQuests)
            SendWhisper(comout.str(), fromPlayer);
        if (hasIncompleteQuests)
            SendWhisper(incomout.str(), fromPlayer);
        if (!hasCompleteQuests && !hasIncompleteQuests)
            SendWhisper("I have no quests.", fromPlayer);
    }
}

void PlayerbotAI::_HandleCommandPet(std::string &text, Player &fromPlayer)
{
    if (ExtractCommand("tame", text))
    {
        if (m_bot->GetPetGUID())
        {
            SendWhisper("I already have a pet!", fromPlayer);
            return;
        }

        uint64 castOnGuid = fromPlayer.GetSelection();
        if (castOnGuid && m_bot->HasSpell(TAME_BEAST_1))
        {
            if (ASPECT_OF_THE_MONKEY > 0 && !m_bot->HasAura(ASPECT_OF_THE_MONKEY, 0))
                CastSpell(ASPECT_OF_THE_MONKEY, *m_bot);
            m_targetGuidCommand = castOnGuid;
            SetState(BOTSTATE_TAME);
        }
        else
            SendWhisper("I can't tame that!", fromPlayer);
        return;
    }

    Pet * pet = m_bot->GetPet();
    if (!pet)
    {
        SendWhisper("I have no pet.", fromPlayer);
        return;
    }

    if (ExtractCommand("abandon", text))
    {
        // abandon pet
        WorldPacket* const packet = new WorldPacket(CMSG_PET_ABANDON, 8);
        *packet << pet->GetGUID();
        m_bot->GetSession()->QueuePacket(packet);

    }
    else if (ExtractCommand("react", text))
    {
        if (ExtractCommand("aggressive", text, true))
            pet->SetReactState(REACT_AGGRESSIVE);
        else if (ExtractCommand("defensive", text, true))
            pet->SetReactState(REACT_DEFENSIVE);
        else if (ExtractCommand("passive", text, true))
            pet->SetReactState(REACT_PASSIVE);
        else
            _HandleCommandHelp("pet react", fromPlayer);
    }
    else if (ExtractCommand("state", text))
    {
        if (text != "")
        {
            SendWhisper("'pet state' does not support subcommands.", fromPlayer);
            return;
        }

        std::string state;
        switch (pet->GetReactState())
        {
            case REACT_AGGRESSIVE:
                SendWhisper("My pet is aggressive.", fromPlayer);
                break;
            case REACT_DEFENSIVE:
                SendWhisper("My pet is defensive.", fromPlayer);
                break;
            case REACT_PASSIVE:
                SendWhisper("My pet is passive.", fromPlayer);
        }
    }
    else if (ExtractCommand("cast", text))
    {
        if (text == "")
        {
            _HandleCommandHelp("pet cast", fromPlayer);
            return;
        }

        uint32 spellId = (uint32) atol(text.c_str());

        if (spellId == 0)
        {
            spellId = getPetSpellId(text.c_str());
            if (spellId == 0)
                extractSpellId(text, spellId);
        }

        if (spellId != 0 && pet->HasSpell(spellId))
        {
            if (pet->HasAura(spellId))
            {
                pet->RemoveAurasDueToSpell(spellId, pet->GetGUID());
                return;
            }

            uint64 castOnGuid = fromPlayer.GetSelection();
            Unit* pTarget = sObjectAccessor->GetUnit(*m_bot, castOnGuid);
            CastPetSpell(spellId, pTarget);
        }
    }
    else if (ExtractCommand("toggle", text))
    {
        if (text == "")
        {
            _HandleCommandHelp("pet toggle", fromPlayer);
            return;
        }

        uint32 spellId = (uint32) atol(text.c_str());

        if (spellId == 0)
        {
            spellId = getPetSpellId(text.c_str());
            if (spellId == 0)
                extractSpellId(text, spellId);
        }

        if (spellId != 0 && pet->HasSpell(spellId))
        {
            PetSpellMap::iterator itr = pet->m_spells.find(spellId);
            if (itr != pet->m_spells.end())
            {
                if (itr->second.active == ACT_ENABLED)
                {
                    pet->ToggleAutocast(sSpellMgr->GetSpellInfo(spellId), false);
                    if (pet->HasAura(spellId))
                        pet->RemoveAurasDueToSpell(spellId, pet->GetGUID());
                }
                else
                    pet->ToggleAutocast(sSpellMgr->GetSpellInfo(spellId), true);
            }
        }
    }
    else if (ExtractCommand("spells", text))
    {
        if (text != "")
        {
            SendWhisper("'pet spells' does not support subcommands.", fromPlayer);
            return;
        }

        int loc = m_master->GetSession()->GetSessionDbcLocale();

        std::ostringstream posOut;
        std::ostringstream negOut;

        for (PetSpellMap::iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            const uint32 spellId = itr->first;

            if (itr->second.state == PETSPELL_REMOVED || sSpellMgr->GetSpellInfo(spellId)->IsPassive())
                continue;

            const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!pSpellInfo)
                continue;

            std::string color;
            switch (itr->second.active)
            {
                case ACT_ENABLED:
                    color = "cff35d22d"; // Some flavor of green
                    break;
                default:
                    color = "cffffffff";
            }

            if (sSpellMgr->GetSpellInfo(spellId)->IsPositive())
                posOut << " |" << color << "|Hspell:" << spellId << "|h["
                       << pSpellInfo->SpellName[loc] << "]|h|r";
            else
                negOut << " |" << color << "|Hspell:" << spellId << "|h["
                       << pSpellInfo->SpellName[loc] << "]|h|r";
        }

        ChatHandler ch(fromPlayer.GetSession());
        SendWhisper("Here's my pet's non-attack spells:", fromPlayer);
        ch.SendSysMessage(posOut.str().c_str());
        SendWhisper("and here's my pet's attack spells:", fromPlayer);
        ch.SendSysMessage(negOut.str().c_str());
    }
}

void PlayerbotAI::_HandleCommandSpells(std::string & /*text*/, Player &fromPlayer)
{
    int loc = m_master->GetSession()->GetSessionDbcLocale();

    std::ostringstream posOut;
    std::ostringstream negOut;

    typedef std::map<std::string, uint32> spellMap;

    spellMap posSpells, negSpells;
    std::string spellName;

    uint32 ignoredSpells[] = {1843, 5019, 2479, 6603, 3365, 8386, 21651, 21652, 6233, 6246, 6247,
                              61437, 22810, 22027, 45927, 7266, 7267, 6477, 6478, 7355, 68398};
    uint32 ignoredSpellsCount = sizeof(ignoredSpells) / sizeof(uint32);

    for (PlayerSpellMap::iterator itr = m_bot->GetSpellMap().begin(); itr != m_bot->GetSpellMap().end(); ++itr)
    {
        const uint32 spellId = itr->first;

        if (itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled || sSpellMgr->GetSpellInfo(spellId)->IsPassive())
            continue;

        const SpellInfo * pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!pSpellInfo)
            continue;

        spellName = pSpellInfo->SpellName[loc];

        SkillLineAbilityMapBounds const bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);

        bool isProfessionOrRidingSpell = false;
        for (SkillLineAbilityMap::const_iterator skillIter = bounds.first; skillIter != bounds.second; ++skillIter)
        {
            if (IsProfessionOrRidingSkill(skillIter->second->skillId) && skillIter->first == spellId) {
                isProfessionOrRidingSpell = true;
                break;
            }
        }
        if (isProfessionOrRidingSpell)
            continue;

        bool isIgnoredSpell = false;
        for (uint8 i = 0; i < ignoredSpellsCount; ++i)
        {
            if (spellId == ignoredSpells[i]) {
                isIgnoredSpell = true;
                break;
            }
        }
        if (isIgnoredSpell)
            continue;

        if (sSpellMgr->GetSpellInfo(spellId)->IsPositive()) {
            if (posSpells.find(spellName) == posSpells.end())
                posSpells[spellName] = spellId;
            else if (posSpells[spellName] < spellId)
                posSpells[spellName] = spellId;
        }
        else
        {
            if (negSpells.find(spellName) == negSpells.end())
                negSpells[spellName] = spellId;
            else if (negSpells[spellName] < spellId)
                negSpells[spellName] = spellId;
        }
    }

    for (spellMap::const_iterator iter = posSpells.begin(); iter != posSpells.end(); ++iter)
    {
        posOut << " |cffffffff|Hspell:" << iter->second << "|h[" << iter->first << "]|h|r";
    }

    for (spellMap::const_iterator iter = negSpells.begin(); iter != negSpells.end(); ++iter)
    {
        negOut << " |cffffffff|Hspell:" << iter->second << "|h[" << iter->first << "]|h|r";
    }

    ChatHandler ch(fromPlayer.GetSession());
    SendWhisper("here's my non-attack spells:", fromPlayer);
    ch.SendSysMessage(posOut.str().c_str());
    SendWhisper("and here's my attack spells:", fromPlayer);
    ch.SendSysMessage(negOut.str().c_str());
}

void PlayerbotAI::_HandleCommandSurvey(std::string & /*text*/, Player &fromPlayer)
{
    uint32 count = 0;
    std::ostringstream detectout;
    QueryResult result;
    GameEventMgr::ActiveEvents const& activeEventsList = sGameEventMgr->GetActiveEventList();
    std::ostringstream eventFilter;
    eventFilter << " AND (eventEntry IS NULL ";
    bool initString = true;

    for (GameEventMgr::ActiveEvents::const_iterator itr = activeEventsList.begin(); itr != activeEventsList.end(); ++itr)
    {
        if (initString)
        {
            eventFilter <<  "OR eventEntry IN (" << *itr;
            initString = false;
        }
        else
            eventFilter << "," << *itr;
    }

    if (!initString)
        eventFilter << "))";
    else
        eventFilter << ")";

    result = WorldDatabase.PQuery("SELECT gameobject.guid, id, position_x, position_y, position_z, map, "
                                  "(POW(position_x - %f, 2) + POW(position_y - %f, 2) + POW(position_z - %f, 2)) AS order_ FROM gameobject "
                                  "LEFT OUTER JOIN game_event_gameobject on gameobject.guid=game_event_gameobject.guid WHERE map = '%i' %s ORDER BY order_ ASC LIMIT 10",
                                  m_bot->GetPositionX(), m_bot->GetPositionY(), m_bot->GetPositionZ(), m_bot->GetMapId(), eventFilter.str().c_str());

    if (result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();

            GameObject *go = m_bot->GetMap()->GetGameObject(MAKE_NEW_GUID(guid, 0, HIGHGUID_GAMEOBJECT));
            if (!go)
                continue;

            if (!go->isSpawned())
                continue;

            detectout << "|cFFFFFF00|Hfound:" << guid << ":" << entry  << ":" <<  "|h[" << go->GetGOInfo()->name << "]|h|r";
            ++count;
        } while (result->NextRow());

        //delete result;
    }
    SendWhisper(detectout.str().c_str(), fromPlayer);
}

// _HandleCommandSkill: Handle class & professions training:
// skill                           -- Lists bot(s) Primary profession skills & weapon skills
// skill learn                     -- List available class or profession (Primary or Secondary) skills, spells & abilities from selected trainer.
// skill learn [HLINK][HLINK] ..   -- Learn selected skill and spells, from selected trainer ([HLINK] from skill learn).
// skill unlearn [HLINK][HLINK] .. -- Unlearn selected primary profession skill(s) and all associated spells ([HLINK] from skill)
void PlayerbotAI::_HandleCommandSkill(std::string &text, Player &fromPlayer)
{
    uint32 rank[8] = {0, 75, 150, 225, 300, 375, 450, 525};

    std::ostringstream msg;

    if (ExtractCommand("learn", text))
    {
        uint32 totalCost = 0;

        Unit* unit = sObjectAccessor->GetUnit(*m_bot, fromPlayer.GetSelection());
        if (!unit)
        {
            SendWhisper("Please select the trainer!", fromPlayer);
            return;
        }

        if (!unit->isTrainer())
        {
            SendWhisper("This is not a trainer!", fromPlayer);
            return;
        }

        Creature *creature =  m_bot->GetMap()->GetCreature(fromPlayer.GetSelection());
        if (!creature)
            return;

        if (!creature->isCanTrainingOf(m_bot, false))
        {
            SendWhisper("This trainer can not teach me anything!", fromPlayer);
            return;
        }

        // check present spell in trainer spell list
        TrainerSpellData const* cSpells = creature->GetTrainerSpells();
        //TrainerSpellData const* tSpells = creature->GetTrainerTemplateSpells();
        if (!cSpells)
        {
            SendWhisper("No spells can be learnt from this trainer", fromPlayer);
            return;
        }

        // reputation discount
        float fDiscountMod =  m_bot->GetReputationPriceDiscount(creature);

        // Handle: Learning class or profession (primary or secondary) skill & spell(s) for selected trainer, skill learn [HLINK][HLINK][HLINK].. ([HLINK] from skill train)
        if (text.size() > 0)
        {
            msg << "I have learned the following spells:\r";
            uint32 totalSpellLearnt = 0;
            bool visuals = true;
            m_spellsToLearn.clear();
            extractSpellIdList(text, m_spellsToLearn);
            for (std::list<uint32>::iterator it = m_spellsToLearn.begin(); it != m_spellsToLearn.end(); it++)
            {
                uint32 spellId = *it;

                if (!spellId)
                    break;

                TrainerSpell const* trainer_spell = cSpells->Find(spellId);
                if (!trainer_spell)
                    continue;

                uint32 reqLevel = 0;
                if (!trainer_spell->learnedSpell[0] && !m_bot->IsSpellFitByClassAndRace(trainer_spell->learnedSpell[0]))
                    continue;

                if (sSpellMgr->GetSpellInfo(trainer_spell->learnedSpell[0])->IsPrimaryProfession() && m_bot->HasSpell(trainer_spell->learnedSpell[0]))
                    continue;

                reqLevel = trainer_spell->reqLevel ? trainer_spell->reqLevel : std::max(reqLevel, trainer_spell->reqLevel);

                TrainerSpellState state =  m_bot->GetTrainerSpellState(trainer_spell);
                if (state != TRAINER_SPELL_GREEN)
                    continue;

                // apply reputation discount
                uint32 cost = uint32(floor(trainer_spell->spellCost * fDiscountMod));
                // check money requirement
                if (m_bot->GetMoney() < cost)
                {
                    Announce(CANT_AFFORD);
                    continue;
                }

                m_bot->ModifyMoney(-int32(cost));
                // learn explicitly or cast explicitly
                if (trainer_spell->IsCastable())
                    m_bot->CastSpell(m_bot, trainer_spell->spell, true);
                else
                    m_bot->learnSpell(spellId, false);
                ++totalSpellLearnt;
                totalCost += cost;
                const SpellEntry *const pSpellInfo = sSpellStore.LookupEntry(spellId);
                if (!pSpellInfo)
                    continue;

                if (visuals)
                {
                    visuals = false;
                    WorldPacket data(SMSG_PLAY_SPELL_VISUAL, 12);           // visual effect on trainer
                    data << uint64(fromPlayer.GetSelection());
                    data << uint32(0xB3);                                   // index from SpellVisualKit.dbc
                    m_master->GetSession()->SendPacket(&data);

                    data.Initialize(SMSG_PLAY_SPELL_IMPACT, 12);            // visual effect on player
                    data << m_bot->GetGUID();
                    data << uint32(0x016A);                                 // index from SpellVisualKit.dbc
                    m_master->GetSession()->SendPacket(&data);
                }

                WorldPacket data(SMSG_TRAINER_BUY_SUCCEEDED, 12);
                data << uint64(fromPlayer.GetSelection());
                data << uint32(spellId);                                // should be same as in packet from client
                m_master->GetSession()->SendPacket(&data);

                MakeSpellLink(pSpellInfo, msg);
                msg << " ";
                msg << Cash(cost) << "\n";
            }
            ReloadAI();
            msg << "Total of " << totalSpellLearnt << " spell";
            if (totalSpellLearnt != 1) msg << "s";
            msg << " learnt, ";
            msg << Cash(totalCost) << " spent.";
        }
        // Handle: List class or profession skills, spells & abilities for selected trainer
        else
        {
            msg << "The spells I can learn and their cost:\r";

            for (TrainerSpellMap::const_iterator itr =  cSpells->spellList.begin(); itr !=  cSpells->spellList.end(); ++itr)
            {
                TrainerSpell const* tSpell = &itr->second;

                if (!tSpell)
                    break;

                uint32 reqLevel = 0;
                if (!tSpell->learnedSpell[0] && !m_bot->IsSpellFitByClassAndRace(tSpell->learnedSpell[0]))
                    continue;

                if (sSpellMgr->GetSpellInfo(tSpell->learnedSpell[0])->IsPrimaryProfession() && m_bot->HasSpell(tSpell->learnedSpell[0]))
                    continue;

                reqLevel = tSpell->reqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);

                TrainerSpellState state =  m_bot->GetTrainerSpellState(tSpell);
                if (state != TRAINER_SPELL_GREEN)
                    continue;

                uint32 spellId = tSpell->spell;
                const SpellEntry *const pSpellInfo =  sSpellStore.LookupEntry(spellId);
                if (!pSpellInfo)
                    continue;
                uint32 cost = uint32(floor(tSpell->spellCost *  fDiscountMod));
                totalCost += cost;
                MakeSpellLink(pSpellInfo, msg);
                msg << " ";
                msg << Cash(cost) << "\n";
            }
            int32 moneyDiff = m_bot->GetMoney() - totalCost;
            if (moneyDiff >= 0)
            {
                // calculate how much money bot has
                msg << " ";
                msg << Cash(moneyDiff) << " left.";
            }
            else
            {
                Announce(CANT_AFFORD);
                moneyDiff *= -1;
                msg << "I need ";
                msg << Cash(moneyDiff) << " more to learn all the spells!";
            }
        }
    }
    // Handle: Unlearning selected primary profession skill(s) and all associated spells, skill unlearn [HLINK][HLINK].. ([HLINK] from skill)
    else if (ExtractCommand("unlearn", text))
    {
        m_spellsToLearn.clear();
        extractSpellIdList(text, m_spellsToLearn);
        for (std::list<uint32>::iterator it = m_spellsToLearn.begin(); it != m_spellsToLearn.end(); ++it)
        {
            if (sSpellMgr->GetSpellInfo(*it)->IsPrimaryProfession())
            {
                SpellLearnSkillNode const* spellLearnSkill = sSpellMgr->GetSpellLearnSkill(*it);

                uint32 prev_spell = sSpellMgr->GetPrevSpellInChain(*it);
                if (!prev_spell)                                    // first rank, remove skill
                    GetPlayer()->SetSkill(spellLearnSkill->skill, GetPlayer()->GetSkillStep(spellLearnSkill->skill), 0, 0);
                else
                {
                    // search prev. skill setting by spell ranks chain
                    SpellLearnSkillNode const* prevSkill = sSpellMgr->GetSpellLearnSkill(prev_spell);
                    while (!prevSkill && prev_spell)
                    {
                        prev_spell = sSpellMgr->GetPrevSpellInChain(prev_spell);
                        prevSkill = sSpellMgr->GetSpellLearnSkill(sSpellMgr->GetFirstSpellInChain(prev_spell));
                    }
                    if (!prevSkill)                                 // not found prev skill setting, remove skill
                        GetPlayer()->SetSkill(spellLearnSkill->skill, GetPlayer()->GetSkillStep(spellLearnSkill->skill), 0, 0);
                }
            }
        }
    }
    // Handle: Lists bot(s) primary profession skills & weapon skills.
    else
    {
        m_spellsToLearn.clear();
        m_bot->skill(m_spellsToLearn);
        msg << "My Primary Professions: ";
        for (std::list<uint32>::iterator it = m_spellsToLearn.begin(); it != m_spellsToLearn.end(); ++it)
        {
            if (IsPrimaryProfessionSkill(*it))
                for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
                {
                    SkillLineAbilityEntry const *skillLine = sSkillLineAbilityStore.LookupEntry(j);
                    if (!skillLine)
                        continue;

                    // has skill
                    if (skillLine->skillId == *it && skillLine->learnOnGetSkill == 0)
                    {
                        SpellEntry const* spellInfo = sSpellStore.LookupEntry(skillLine->spellId);
                        if (!spellInfo)
                            continue;

                        if (m_bot->GetSkillValue(*it) <= rank[sSpellMgr->GetSpellRank(skillLine->spellId)] && m_bot->HasSpell(skillLine->spellId))
                        {
                            // sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: HandleCommand - skill (%u)(%u)(%u):",skillLine->spellId, rank[sSpellMgr->GetSpellRank(skillLine->spellId)], m_bot->GetSkillValue(*it));
                            MakeSpellLink(spellInfo, msg);
                            break;
                        }
                    }
                }
        }

        msg << "\nMy Weapon skills: ";
        for (std::list<uint32>::iterator it = m_spellsToLearn.begin(); it != m_spellsToLearn.end(); ++it)
        {
            SkillLineEntry const *SkillLine = sSkillLineStore.LookupEntry(*it);
            // has weapon skill
            if (SkillLine->categoryId == SKILL_CATEGORY_WEAPON)
                for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
                {
                    SkillLineAbilityEntry const *skillLine = sSkillLineAbilityStore.LookupEntry(j);
                    if (!skillLine)
                        continue;

                    SpellEntry const* spellInfo = sSpellStore.LookupEntry(skillLine->spellId);
                    if (!spellInfo)
                        continue;

                    if (skillLine->skillId == *it && spellInfo->Effect[0] == SPELL_EFFECT_WEAPON)
                        MakeWeaponSkillLink(spellInfo, msg, *it);
                }
        }
    }
    SendWhisper(msg.str(), fromPlayer);
    m_spellsToLearn.clear();
    //m_bot->GetPlayerbotAI()->GetClassAI();
}

void PlayerbotAI::_HandleCommandStats(std::string &text, Player &fromPlayer)
{
    if (text != "")
    {
        SendWhisper("'stats' does not have subcommands", fromPlayer);
        return;
    }

    std::ostringstream out;

    uint32 totalused = 0;
    // list out items in main backpack
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++)
    {
        const Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem)
            totalused++;
    }
    uint32 totalfree = 16 - totalused;
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* const pBag = (Bag *) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
        {
            ItemTemplate const* pBagProto = pBag->GetTemplate();
            if (pBagProto->Class == ITEM_CLASS_CONTAINER && pBagProto->SubClass == ITEM_SUBCLASS_CONTAINER)
                totalfree =  totalfree + pBag->GetFreeSlots();
        }

    }

    // estimate how much item damage the bot has
    out << "|cffffffff[|h|cff00ffff" << m_bot->GetName() << "|h|cffffffff] has |cff00ff00";
    out << totalfree << " |h|cffffffff bag slots,|h" << " |cff00ff00";
    out << Cash(EstRepairAll());

    // calculate how much money bot has
    uint32 copper = m_bot->GetMoney();
    out << "|h|cffffffff item damage & has " << "|r|cff00ff00";
    out << Cash(copper);
    ChatHandler ch(fromPlayer.GetSession());
    ch.SendSysMessage(out.str().c_str());
}

void PlayerbotAI::_HandleCommandGM(std::string &text, Player &fromPlayer)
{
    // Check should happen OUTSIDE this function, but this is account security we're talking about, so let's be doubly sure
    if (fromPlayer.GetSession()->GetSecurity() <= SEC_PLAYER)
        return;  // no excuses, no warning

    if (text == "")
    {
        SendWhisper("gm must have a subcommand.", fromPlayer);
        return;
    }
    else if (ExtractCommand("check", text))
    {
        if (ExtractCommand("talent", text))
        {
            if (ExtractCommand("spec", text))
            {
                uint32 tsDBError = TalentSpecDBContainsError();
                if (0 != tsDBError)
                {
                    std::ostringstream oss;
                    oss << "Error found in TalentSpec: " << tsDBError;
                    SendWhisper(oss.str(), fromPlayer);
                }
                else
                    SendWhisper("No errors found. High five!", fromPlayer);
            }
        }
        else
            SendWhisper("'gm check' does not have that subcommand.", fromPlayer);
    }
    else
        SendWhisper("'gm' does not have that subcommand.", fromPlayer);
}

void PlayerbotAI::_HandleCommandHelp(std::string &text, Player &fromPlayer)
{
    ChatHandler ch(fromPlayer.GetSession());

    // "help help"? Seriously?
    if (ExtractCommand("help", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("help", "Lists all the things you can order me to do... But it's up to me whether to follow your orders... Or not.").c_str());
        return;
    }

    bool bMainHelp = (text == "") ? true : false;
    const std::string sInvalidSubcommand = "That's not a valid subcommand.";
    std::string msg = "";
    // All of these must containt the 'bMainHelp' clause -> help lists all major commands
    // Further indented 'ExtractCommand("subcommand")' conditionals make sure these aren't printed for basic "help"
    if (bMainHelp || ExtractCommand("attack", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("attack", "Attack the selected target. Which would, of course, require a valid target.", HL_TARGET).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("follow", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("follow", "I will follow you - this also revives me if dead and teleports me if I'm far away.").c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("follow far", "I will follow at a father distance away from you.").c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("follow near", "I will follow at a closer distance to you.").c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("follow reset", "I will reset my follow distance to its original state.").c_str());
        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("stay", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("stay", "I will stay put until told otherwise.").c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("autoequip", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("autoequip", "Used with no parameter: Toggles Auto Equipping for one or all bots to ON or OFF depending on their current setting.").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("autoequip < on >", "Turns Auto equipping ON for one, or all bots in group").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("autoequip < off >", "Turns Auto equipping OFF for one, or all bots in group").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("autoequip < now >", "Ignores current autoequip setting, Runs the auto equip cycle ONCE for one or all bots (/t or /p)").c_str());
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("assist", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("assist", "I will assist the character listed, attacking as they attack.", HL_NAME).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("spells", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("spells", "I will list all the spells I know.").c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("craft", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("craft", "I will create a single specified recipe", HL_RECIPE).c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("craft [RECIPE] all", "I will create all specified recipes").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < alchemy | a >", "List all learnt alchemy recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < blacksmithing | b >", "List all learnt blacksmith recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < cooking | c >", "List all learnt cooking recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < engineering | e >", "List all learnt engineering recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < firstaid | f >", "List all learnt firstaid recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < inscription | i >", "List all learnt inscription recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < jewelcrafting | j >", "List all learnt jewelcrafting recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < leatherworking | l >", "List all learnt leatherworking recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < magic | m >", "List all learnt enchanting recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < smelting | s >", "List all learnt mining recipes").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("craft < tailoring | t >", "List all learnt tailoring recipes").c_str());
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("process", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("process < disenchant | d >", "Disenchants a green coloured [ITEM] or better", HL_ITEM).c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("process < mill | m >", "Grinds 5 herbs [ITEM] to produce pigments", HL_ITEM).c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("process < prospect | p >", "Searches 5 metal ore [ITEM] for precious gems", HL_ITEM).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("enchant", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("enchant", "Lists all enchantments [SPELL] learnt by the bot").c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("enchant [SPELL]", "Enchants selected tradable [ITEM] either equipped or in bag", HL_ITEM).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("cast", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("cast", "I will cast the spell or ability listed.", HL_SPELL).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("use", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("use", "I will use the linked item.", HL_ITEM).c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("use [ITEM]", "I will use the first linked item on a selected TARGET.", HL_TARGET).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("use [ITEM]", "I will use the first linked item on an equipped linked item.", HL_ITEM).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("use [ITEM]", "I will use the first linked item on a linked gameobject.", HL_GAMEOBJECT).c_str());

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("equip", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("equip", "I will equip the linked item(s).", HL_ITEM, true).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("reset", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("reset", "I will reset all my states, orders, loot list, talent spec, ... Hey, that's kind of like memory loss.").c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("stats", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("stats", "This will inform you of my wealth, free bag slots and estimated equipment repair costs.").c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("survey", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("survey", "Lists all available game objects near me.").c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("find", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("find", "I will find said game object, walk right up to it, and wait.", HL_GAMEOBJECT).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("get", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("get", "I will get said game object and return to your side.", HL_GAMEOBJECT).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("quest", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("quest", "Lists my current quests.").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("quest add", "Adds this quest to my quest log.", HL_QUEST).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("quest drop", "Removes this quest from my quest log.", HL_QUEST).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("quest end", "Turns in my completed quests.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("quest list", "Lists the quests offered to me by this target.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("quest report", "This will give you a full report of all the items, creatures or gameobjects I still need to finish my quests.", HL_QUEST).c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("add", text, true)) {}
            else if (ExtractCommand("drop", text, true)) {}
            else if (ExtractCommand("end", text, true)) {}
            else if (ExtractCommand("list", text, true)) {}
            else if (ExtractCommand("report", text, true)) {}

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("orders", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("orders", "Shows you my orders. Free will is overrated, right?").c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("pet", text))
    {
        if (bMainHelp)
            ch.SendSysMessage(_HandleCommandHelpHelper("pet", "Helps command my pet. Must always be used with a subcommand.").c_str());
        else if (text == "") // not "help" AND "help pet"
            ch.SendSysMessage(_HandleCommandHelpHelper("pet", "This by itself is not a valid command. Just so you know. To be used with a subcommand, such as...").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("pet abandon", "Abandons active hunter pet.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("pet cast", "Has my pet cast this spell. May require a treat. Or at least ask nicely.", HL_SPELL).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("pet react", "Sets my pet's aggro mode.", HL_PETAGGRO).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("pet spells", "Shows you the spells my pet knows.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("pet state", "Shows my pet's aggro mode.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("pet tame", "Allows a hunter to acquire a pet.", HL_TARGET).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("pet toggle", "Toggles autocast for this spell.", HL_SPELL).c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("spells", text)) {}
            else if (ExtractCommand("tame", text)) {}
            else if (ExtractCommand("abandon", text)) {}
            else if (ExtractCommand("cast", text)) {}
            else if (ExtractCommand("toggle", text)) {}
            else if (ExtractCommand("state", text)) {}
            else if (ExtractCommand("react", text))
            {
                ch.SendSysMessage(_HandleCommandHelpHelper("pet react", "has three modes.").c_str());
                ch.SendSysMessage(_HandleCommandHelpHelper("aggressive", "sets it so my precious attacks everything in sight.", HL_NONE, false, true).c_str());
                ch.SendSysMessage(_HandleCommandHelpHelper("defensive", "sets it so it automatically attacks anything that attacks me, or anything I attack.", HL_NONE, false, true).c_str());
                ch.SendSysMessage(_HandleCommandHelpHelper("passive", "makes it so my pet won't attack anything unless directly told to.", HL_NONE, false, true).c_str());

                // Catches all valid subcommands, also placeholders for potential future sub-subcommands
                if (ExtractCommand("aggressive", text, true)) {}
                else if (ExtractCommand("defensive", text, true)) {}
                else if (ExtractCommand("passive", text, true)) {}
                if (text != "")
                    ch.SendSysMessage(sInvalidSubcommand.c_str());
            }

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("collect", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("collect", "Tells you what my current collect status is. Also lists possible options.").c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("collect", "Sets what I collect. Obviously the 'none' option should be used alone, but all the others can be mixed.", HL_OPTION, true).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("sell", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("sell", "Adds this to my 'for sale' list.", HL_ITEM, true).c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("sell all", "The next time you sell, I'll sell all my low level white items.").c_str());
        ch.SendSysMessage(_HandleCommandHelpHelper("sell all", "This command must be called each time before you sell, OR I won't auto sell white items.").c_str());
        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("buy", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("buy", "Adds this to my 'purchase' list.", HL_ITEM, true).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("drop", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("drop", "Drops the linked item(s). Permanently.", HL_ITEM, true).c_str());

        if (!bMainHelp)
        {
            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("auction", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("auction", "Lists all my active auctions. With pretty little links and such. Hi hi hi... I'm gonna be sooo rich!").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("auction add", "Adds the item to my 'auction off later' list. I have a lot of lists, you see...", HL_ITEM).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("auction remove", "Adds the item to my 'Don't auction after all' list. Hope it hasn't sold by then!", HL_AUCTION).c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("add", text, true)) {}
            else if (ExtractCommand("remove", text, true)) {}

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("repair", text))
    {
        if (!bMainHelp && text == "")
            ch.SendSysMessage(_HandleCommandHelpHelper("repair", "This by itself is not a valid command. Just so you know. To be used with a subcommand, such as...").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("repair", "Has me find an armorer and repair the items you listed.", HL_ITEM).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("repair all", "Has me find an armorer and repair all my items, be they equipped or just taking up bagspace.").c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("all", text)) {}

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("talent", text))
    {
        msg = _HandleCommandHelpHelper("talent", "Lists my talents, glyphs, unspent talent points and the cost to reset all talents.");
        ch.SendSysMessage(msg.c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("talent learn", "Has me learn the linked talent.", HL_TALENT).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("talent reset", "Resets my talents. Assuming I have the appropriate amount of sparkly gold, shiny silver, and... unrusted copper.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("talent spec", "Lists all talent specs I can use.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("talent spec #", "I will follow this talent spec. Well, I will if you picked a talent spec that exists.").c_str());

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
        if (!bMainHelp) return;
    }
    if (bMainHelp || ExtractCommand("bank", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("bank", "Gives you my bank balance. I thought that was private.").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("bank deposit", "Deposits the listed items in my bank.", HL_ITEM, true).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("bank withdraw", "Withdraw the listed items from my bank.", HL_ITEM, true).c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("deposit", text)) {}
            else if (ExtractCommand("withdraw", text)) {}

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("skill", text))
    {
        msg = _HandleCommandHelpHelper("skill", "Lists my primary professions & weapon skills.");
        ch.SendSysMessage(msg.c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("skill learn", "Lists the things this trainer can teach me. If you've targeted a trainer, that is.").c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("skill learn", "Have me learn this skill from the selected trainer.", HL_SKILL).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("skill unlearn", "Unlearn the linked (primary) profession and everything that goes with it.", HL_PROFESSION).c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("learn", text)) {}
            else if (ExtractCommand("unlearn", text)) {}

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (fromPlayer.GetSession()->GetSecurity() > SEC_PLAYER && (bMainHelp || ExtractCommand("gm", text)))
    {
        msg = _HandleCommandHelpHelper("gm", "Lists actions available to GM account level and up.");
        ch.SendSysMessage(msg.c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("gm check", "Lists the things you can run a check on.").c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("check", text))
            {
                ch.SendSysMessage(_HandleCommandHelpHelper("gm check talent", "Lists talent mechanics you can run a check on.").c_str());

                if (ExtractCommand("talent", text))
                {
                    ch.SendSysMessage(_HandleCommandHelpHelper("gm check talent spec", "Checks the talent spec database for various errors. Only the first error (if any) is returned.").c_str());

                    if (ExtractCommand("spec", text)) {}

                    if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
                    return;
                }

                if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
                return;
            }

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }
    if (bMainHelp || ExtractCommand("mail", text))
    {
        ch.SendSysMessage(_HandleCommandHelpHelper("mail inbox |cFFFFFF00|h[Mailbox]|h|r", "Lists all bot mail from selected [Mailbox]").c_str());

        if (!bMainHelp)
        {
            ch.SendSysMessage(_HandleCommandHelpHelper("mail getcash", "Gets money from all selected [Mailid]..", HL_MAIL, true).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("mail getitem", "Gets items from all selected [Mailid]..", HL_MAIL, true).c_str());
            ch.SendSysMessage(_HandleCommandHelpHelper("mail delete", "Delete all selected [Mailid]..", HL_MAIL, true).c_str());

            // Catches all valid subcommands, also placeholders for potential future sub-subcommands
            if (ExtractCommand("inbox", text, true)) {}
            else if (ExtractCommand("getcash", text, true)) {}
            else if (ExtractCommand("getitem", text, true)) {}
            else if (ExtractCommand("delete", text, true)) {}

            if (text != "") ch.SendSysMessage(sInvalidSubcommand.c_str());
            return;
        }
    }

    if (bMainHelp)
        ch.SendSysMessage(_HandleCommandHelpHelper("help", "Gives you this listing of main commands... But then, you know that already don't you.").c_str());

    if (text != "")
        ch.SendSysMessage("Either that is not a valid command, or someone forgot to add it to my help journal. I mean seriously, they can't expect me to remember *all* this stuff, can they?");
}

std::string PlayerbotAI::_HandleCommandHelpHelper(std::string sCommand, std::string sExplain, HELPERLINKABLES reqLink, bool bReqLinkMultiples, bool bCommandShort)
{
    if (sCommand == "")
    {
        //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI] _HandleCommandHelpHelper called with an empty sCommand. Ignoring call.");
        return "";
    }

    std::ostringstream oss;
    oss << "'|cffffffff";
    if (bCommandShort)
        oss << "(" << sCommand.at(0) << ")" << sCommand.substr(1);
    else
        oss << sCommand;

    if (reqLink != HL_NONE)
    {
        if (reqLink == HL_PROFESSION)
        {
            oss << " [PROFESSION]";
            if (bReqLinkMultiples)
                oss << " [PROFESSION] ..";
        }
        else if (reqLink == HL_ITEM)
        {
            oss << " [ITEM]";
            if (bReqLinkMultiples)
                oss << " [ITEM] ..";
        }
        else if (reqLink == HL_TALENT)
        {
            oss << " [TALENT]";
            if (bReqLinkMultiples)
                oss << " [TALENT] ..";
        }
        else if (reqLink == HL_SKILL)
        {
            oss << " [SKILL]";
            if (bReqLinkMultiples)
                oss << " [SKILL] ..";
        }
        else if (reqLink == HL_OPTION)
        {
            oss << " <OPTION>";
            if (bReqLinkMultiples)
                oss << " <OPTION> ..";
        }
        else if (reqLink == HL_PETAGGRO)
        {
            oss << " <(a)ggressive | (d)efensive | (p)assive>";
            //if (bReqLinkMultiples)
                //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI] _HandleCommandHelpHelper: sCommand \"pet\" with bReqLinkMultiples \"true\". ... Why? Bug, surely.");
        }
        else if (reqLink == HL_QUEST)
        {
            oss << " [QUEST]";
            if (bReqLinkMultiples)
                oss << " [QUEST] ..";
        }
        else if (reqLink == HL_GAMEOBJECT)
        {
            oss << " [GAMEOBJECT]";
            if (bReqLinkMultiples)
                oss << " [GAMEOBJECT] ..";
        }
        else if (reqLink == HL_SPELL)
        {
            oss << " <Id# | (part of) name | [SPELL]>";
            if (bReqLinkMultiples)
                oss << " <Id# | (part of) name | [SPELL]> ..";
        }
        else if (reqLink == HL_TARGET)
        {
            oss << " (TARGET)";
            if (bReqLinkMultiples)
                oss << " (TARGET) ..";
        }
        else if (reqLink == HL_NAME)
        {
            oss << " <NAME>";
            if (bReqLinkMultiples)
                oss << " <NAME> ..";
        }
        else if (reqLink == HL_AUCTION)
        {
            oss << " [AUCTION]";
            if (bReqLinkMultiples)
                oss << " [AUCTION] ..";
        }
        else if (reqLink == HL_RECIPE)
        {
            oss << " [RECIPE]";
            if (bReqLinkMultiples)
                oss << " [RECIPE] ..";
        }
        else if (reqLink == HL_MAIL)
        {
            oss << " [MAILID]";
            if (bReqLinkMultiples)
                oss << " [MAILID] ..";
        }
        else
        {
            oss << " {unknown}";
            if (bReqLinkMultiples)
                oss << " {unknown} ..";
            //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: _HandleCommandHelpHelper - Uncaught case");
        }
    }

    oss << "|r': " << sExplain;

    return oss.str();
}

void PlayerbotAI::HandleMasterIncomingPacket(const WorldPacket& packet, WorldSession& session)
{
    //WorldSession *session = m_master->GetSession();
    switch (packet.GetOpcode())
    {
        case CMSG_ACTIVATETAXI:
        {
            WorldPacket p(packet);
            p.rpos(0); // reset reader

            uint64 guid;
            std::vector<uint32> nodes;
            nodes.resize(2);
            uint8 delay = 9;

            p >> guid >> nodes[0] >> nodes[1];

            // DEBUG_LOG ("[PlayerbotAI]: HandleMasterIncomingPacket - Received CMSG_ACTIVATETAXI from %d to %d", nodes[0], nodes[1]);

            delay = delay + 3;

            Group* group = m_bot->GetGroup();
            if (!group)
                return;

            Unit *target = sObjectAccessor->GetUnit(*m_bot, guid);

            SetIgnoreUpdateTime(delay);

            m_bot->GetMotionMaster()->Clear(true);
            m_bot->GetMotionMaster()->MoveFollow(target, INTERACTION_DISTANCE, m_bot->GetOrientation());
            GetTaxi(guid, nodes);
            return;
        }

        case CMSG_ACTIVATETAXIEXPRESS:
        {
            WorldPacket p(packet);
            p.rpos(0); // reset reader

            uint64 guid;
            uint32 node_count;
            uint8 delay = 9;

            p >> guid >> node_count;

            std::vector<uint32> nodes;

            for (uint32 i = 0; i < node_count; ++i)
            {
                uint32 node;
                p >> node;
                nodes.push_back(node);
            }

            if (nodes.empty())
                return;

            // DEBUG_LOG ("[PlayerbotAI]: HandleMasterIncomingPacket - Received CMSG_ACTIVATETAXIEXPRESS from %d to %d", nodes.front(), nodes.back());

            delay = delay + 3;

            Group* group = m_bot->GetGroup();
            if (!group)
                return;

            Unit *target = sObjectAccessor->GetUnit(*m_bot, guid);

            SetIgnoreUpdateTime(delay);

            m_bot->GetMotionMaster()->Clear(true);
            m_bot->GetMotionMaster()->MoveFollow(target, INTERACTION_DISTANCE, m_bot->GetOrientation());
            GetTaxi(guid, nodes);
            return;
        }

        //case CMSG_MOVE_SPLINE_DONE:
        //{
            //// DEBUG_LOG ("[PlayerbotMgr]: HandleMasterIncomingPacket - Received CMSG_MOVE_SPLINE_DONE");

            //WorldPacket p(packet);
            //p.rpos(0); // reset reader

            //uint64 guid = extractGuid(p);                           // used only for proper packet read
            //MovementInfo movementInfo;                              // used only for proper packet read

            //p >> guid;
            //p >> movementInfo;
            //p >> Unused<uint32>();                          // unk

            //for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
            //{

            //    Player* const bot = it->second;
            //    if (!bot)
            //        return;

            //    // in taxi flight packet received in 2 case:
            //    // 1) end taxi path in far (multi-node) flight
            //    // 2) switch from one map to other in case multi-map taxi path
            //    // we need process only (1)
            //    uint32 curDest = bot->m_taxi.GetTaxiDestination();
            //    if (!curDest)
            //        return;

            //    TaxiNodesEntry const* curDestNode = sTaxiNodesStore.LookupEntry(curDest);

            //    // far teleport case
            //    if (curDestNode && curDestNode->map_id != bot->GetMapId())
            //    {
            //        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
            //        {
            //            // short preparations to continue flight
            //            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator *) (bot->GetMotionMaster()->top());

            //            flight->Interrupt(*bot);                // will reset at map landing

            //            flight->SetCurrentNodeAfterTeleport();
            //            TaxiPathNodeEntry const& node = flight->GetPath()[flight->GetCurrentNode()];
            //            flight->SkipCurrentNode();

            //            bot->TeleportTo(curDestNode->map_id, node.x, node.y, node.z, bot->GetOrientation());
            //        }
            //        return;
            //    }

            //    uint32 destinationnode = bot->m_taxi.NextTaxiDestination();
            //    if (destinationnode > 0)                                // if more destinations to go
            //    {
            //        // current source node for next destination
            //        uint32 sourcenode = bot->m_taxi.GetTaxiSource();

            //        // Add to taximask middle hubs in taxicheat mode (to prevent having player with disabled taxicheat and not having back flight path)
            //        if (bot->isTaxiCheater())
            //            if (bot->m_taxi.SetTaximaskNode(sourcenode))
            //            {
            //                WorldPacket data(SMSG_NEW_TAXI_PATH, 0);
            //                bot->GetSession()->SendPacket(&data);
            //            }

            //        // DEBUG_LOG ("[PlayerbotMgr]: HandleMasterIncomingPacket - Received CMSG_MOVE_SPLINE_DONE Taxi has to go from %u to %u", sourcenode, destinationnode);

            //        uint32 mountDisplayId = sObjectMgr.GetTaxiMountDisplayId(sourcenode, bot->GetTeam());

            //        uint32 path, cost;
            //        sObjectMgr.GetTaxiPath(sourcenode, destinationnode, path, cost);

            //        if (path && mountDisplayId)
            //            bot->GetSession()->SendDoFlight(mountDisplayId, path, 1);          // skip start fly node
            //        else
            //            bot->m_taxi.ClearTaxiDestinations();    // clear problematic path and next
            //    }
            //    else
            //        /* std::ostringstream out;
            //           out << "Destination reached" << bot->GetName();
            //           ChatHandler ch(m_master);
            //           ch.SendSysMessage(out.str().c_str()); */
            //        bot->m_taxi.ClearTaxiDestinations();        // Destination, clear source node
            //}
            //return;
        //}

        // if master is logging out, log out all bots
        //case CMSG_LOGOUT_REQUEST:
        //{
        //    LogoutAllBots();
        //    return;
        //}

        // If master inspects one of his bots, give the master useful info in chat window
        // such as inventory that can be equipped
        //case CMSG_INSPECT:
        //{
        //    WorldPacket p(packet);
        //    p.rpos(0); // reset reader
        //    uint64 guid;
        //    p >> guid;
        //    Player* const bot = GetPlayerBot(guid);
        //    if (bot) bot->GetPlayerbotAI()->SendNotEquipList(*bot);
        //    return;
        //}

        case CMSG_REPAIR_ITEM:
        {
            WorldPacket p(packet);
            p.rpos(0); //reset reader
            uint64 npcGUID;
            p >> npcGUID;

            WorldObject *const pNpc = (WorldObject*)sObjectAccessor->GetObjectByTypeMask(*session.GetPlayer(), npcGUID, TYPEMASK_UNIT|TYPEMASK_GAMEOBJECT);
            if (!pNpc)
                return;

            if (m_bot->GetDistance(pNpc) > 20.0f)
                TellMaster("I'm too far away to repair items!");
            else
            {
                TellMaster("Repairing my items.");
                m_bot->DurabilityRepairAll(false, 0.0f, false);
            }
            return;
        }

        // handle emotes from the master
        //case CMSG_EMOTE:
        case CMSG_TEXT_EMOTE:
        {
            WorldPacket p(packet);
            p.rpos(0); // reset reader
            uint32 emoteNum;
            p >> emoteNum;

            /* std::ostringstream out;
               out << "emote is: " << emoteNum;
               ChatHandler ch(m_master);
               ch.SendSysMessage(out.str().c_str()); */

            switch (emoteNum)
            {
                case TEXT_EMOTE_BOW:
                {
                    // Buff anyone who bows before me. Useful for players not in bot's group
                    // How do I get correct target???
                    //Player* const pPlayer = GetPlayerBot(m_master->GetSelection());
                    //if (pPlayer->GetPlayerbotAI()->GetClassAI())
                    //    pPlayer->GetPlayerbotAI()->GetClassAI()->BuffPlayer(pPlayer);
                    return;
                }
                /*case TEXT_EMOTE_SALUTE:
                {
                    if (Player* const bot = session.GetPlayerBot(m_master->GetSelection()))
                        if (PlayerbotAI *ai = bot->GetPlayerbotAI())
                            ai->SendNotEquipList(*m_master);
                    return;
                }*/
                
                   case TEXT_EMOTE_BONK:
                   {
                    Player* const pPlayer = m_master->GetSession()->GetPlayerBot(m_master->GetSelection());
                    if (!pPlayer || !pPlayer->GetPlayerbotAI())
                        return;
                    PlayerbotAI* const pBot = pPlayer->GetPlayerbotAI();

                    ChatHandler ch(m_master->GetSession());
                    {
                        std::ostringstream out;
                        out << "time(0): " << time(0)
                            << " m_ignoreAIUpdatesUntilTime: " << pBot->m_ignoreAIUpdatesUntilTime;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "m_TimeDoneEating: " << pBot->m_TimeDoneEating
                            << " m_TimeDoneDrinking: " << pBot->m_TimeDoneDrinking;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "m_CurrentlyCastingSpellId: " << pBot->m_CurrentlyCastingSpellId;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "IsBeingTeleported() " << pBot->GetPlayer()->IsBeingTeleported();
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        bool tradeActive = (pBot->GetPlayer()->GetTrader()) ? true : false;
                        out << "tradeActive: " << tradeActive;
                        ch.SendSysMessage(out.str().c_str());
                    }
                    {
                        std::ostringstream out;
                        out << "IsCharmed() " << pBot->GetPlayer()->isCharmed();
                        ch.SendSysMessage(out.str().c_str());
                    }
                    return;
                   }
                 

                case TEXT_EMOTE_EAT:
                case TEXT_EMOTE_DRINK:
                {
                    Feast();
                    return;
                }

                // emote to attack selected target
                case TEXT_EMOTE_POINT:
                {
                    uint64 attackOnGuid = m_master->GetSelection();
                    if (!attackOnGuid)
                        return;

                    Unit* thingToAttack = sObjectAccessor->GetUnit(*m_master, attackOnGuid);
                    if (!thingToAttack) return;

                    if (!m_bot->IsFriendlyTo(thingToAttack) && m_bot->IsWithinLOSInMap(thingToAttack))
                        GetCombatTarget(thingToAttack);
                    return;
                }

                // emote to stay
                case TEXT_EMOTE_STAND:
                {
                    uint64 selection = m_master->GetSelection();
                    if (!selection || selection == m_bot->GetGUID())
                        SetMovementOrder(MOVEMENT_STAY);

                    return;
                }

                // 324 is the followme emote (not defined in enum)
                // if master has bot selected then only bot follows, else all bots follow
                case 324:
                case TEXT_EMOTE_WAVE:
                {
                    uint64 selection = m_master->GetSelection();
                    if (!selection || selection == m_bot->GetGUID())
                        SetMovementOrder(MOVEMENT_FOLLOW, m_master);

                    return;
                }
            }
            return;
        } /* EMOTE ends here */

        case CMSG_GAMEOBJ_USE: // not sure if we still need this one
        case CMSG_GAMEOBJ_REPORT_USE:
        {
            WorldPacket p(packet);
            p.rpos(0);     // reset reader
            uint64 objGUID;
            p >> objGUID;


            GameObject *obj = m_master->GetMap()->GetGameObject(objGUID);
            if (!obj)
                return;

            // add other go types here, i.e.:
            // GAMEOBJECT_TYPE_CHEST - loot quest items of chest
            if (obj->GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
            {
                TurnInQuests(obj);

                // auto accept every available quest this NPC has
                m_bot->PrepareQuestMenu(objGUID);
                QuestMenu& questMenu = m_bot->PlayerTalkClass->GetQuestMenu();
                for (uint32 iI = 0; iI < questMenu.GetMenuItemCount(); ++iI)
                {
                    QuestMenuItem const& qItem = questMenu.GetItem(iI);
                    uint32 questID = qItem.QuestId;
                    if (!AddQuest(questID, obj))
                        m_bot->Whisper("Couldn't take quest", LANG_UNIVERSAL, m_master->GetGUID());
                }
            }
        }
        //break;
        return;

        case CMSG_QUESTGIVER_HELLO:
        {
            WorldPacket p(packet);
            p.rpos(0);    // reset reader
            uint64 npcGUID;
            p >> npcGUID;

            WorldObject* pNpc = (WorldObject*)sObjectAccessor->GetObjectByTypeMask(*m_bot, npcGUID, TYPEMASK_UNIT|TYPEMASK_GAMEOBJECT);
            if (!pNpc)
                return;

            TurnInQuests(pNpc);

            return;
        }

        // if master accepts a quest, bots should also try to accept quest
        case CMSG_QUESTGIVER_ACCEPT_QUEST:
        {
            WorldPacket p(packet);
            p.rpos(0);    // reset reader
            uint64 guid;
            uint32 quest;
            uint32 unk1;
            p >> guid >> quest >> unk1;

            // DEBUG_LOG ("[PlayerbotAI]: HandleMasterIncomingPacket - Received CMSG_QUESTGIVER_ACCEPT_QUEST npc = %s, quest = %u, unk1 = %u", guid.GetString().c_str(), quest, unk1);

            Quest const* qInfo = sObjectMgr->GetQuestTemplate(quest);
            if (qInfo)
            {

                if (m_bot->GetQuestStatus(quest) == QUEST_STATUS_COMPLETE)
                    TellMaster("I already completed that quest.");
                else if (!m_bot->CanTakeQuest(qInfo, false))
                {
                    if (!m_bot->SatisfyQuestStatus(qInfo, false))
                        TellMaster("I already have that quest.");
                    else
                        TellMaster("I can't take that quest.");
                }
                else if (!m_bot->SatisfyQuestLog(false))
                    TellMaster("My quest log is full.");
                else if (!m_bot->CanAddQuest(qInfo, false))
                    TellMaster("I can't take that quest because it requires that I take items, but my bags are full!");

                else
                {
                    p.rpos(0);         // reset reader
                    m_bot->GetSession()->HandleQuestgiverAcceptQuestOpcode(p);
                    TellMaster("Got the quest.");

                    // build needed items if quest contains any
                    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
                    {
                        if (qInfo->RequiredItemCount[i] > 0)
                        {
                            SetQuestNeedItems();
                            break;
                        }
                    }

                    // build needed creatures if quest contains any
                    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
                    {
                        if (qInfo->RequiredNpcOrGoCount[i] > 0)
                        {
                            SetQuestNeedCreatures();
                            break;
                        }
                    }
                }
            }
            return;
        }

        case CMSG_AREATRIGGER:
        {
            WorldPacket p(packet);

            if (m_bot->IsWithinDistInMap(m_master, 50))
            {
                p.rpos(0);         // reset reader
                m_bot->GetSession()->HandleAreaTriggerOpcode(p);
            }
            return;
        }

        case CMSG_QUESTGIVER_COMPLETE_QUEST:
        {
            WorldPacket p(packet);
            p.rpos(0);    // reset reader
            uint32 quest;
            uint64 npcGUID;
            p >> npcGUID >> quest;

            // DEBUG_LOG ("[PlayerbotAI]: HandleMasterIncomingPacket - Received CMSG_QUESTGIVER_COMPLETE_QUEST npc = %s, quest = %u", npcGUID.GetString().c_str(), quest);

            Creature* pNpc = sObjectAccessor->GetCreature(*m_master, npcGUID);
            if (!pNpc)
                return;

            TurnInQuests(pNpc);
            return;
        }

        case CMSG_LOOT_ROLL:
        {
            WorldPacket p(packet);    //WorldPacket packet for CMSG_LOOT_ROLL, (8+4+1)
            uint64 Guid;
            uint32 NumberOfPlayers;
            uint8 rollType;
            p.rpos(0);              //reset packet pointer
            p >> Guid;              //guid of the lootable target
            p >> NumberOfPlayers;   //number of players invited to roll
            p >> rollType;          //need,greed or pass on roll

            Creature *c = m_master->GetMap()->GetCreature(Guid);
            GameObject *g = m_master->GetMap()->GetGameObject(Guid);
            if (!c && !g)
                return;

            Loot *loot = &c->loot;
            if (!loot)
                loot = &g->loot;

            LootItem& lootItem = loot->items[NumberOfPlayers];

            uint32 choice = 0;

            Group* group = m_bot->GetGroup();
            if (!group)
                return;

            ItemTemplate const *pProto = sObjectMgr->GetItemTemplate(lootItem.itemid);
            if (!pProto)
                return;

            if (CanStore())
            {
                if (m_bot->CanUseItem(pProto) == EQUIP_ERR_OK && IsItemUseful(lootItem.itemid))
                    choice = 1;  // Need
                else if (m_bot->HasSkill(SKILL_ENCHANTING))
                    choice = 3;  // Disenchant
                else
                    choice = 2;  // Greed
            }
            else
                choice = 0;  // Pass

            group->CountRollVote(m_bot->GetGUID(), Guid, RollVote(choice));

            switch (choice)
            {
                case ROLL_NEED:
                    m_bot->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED, 1);
                    break;
                case ROLL_GREED:
                    m_bot->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED, 1);
                    break;
            }
            return;
        }

        // Handle GOSSIP activate actions, prior to GOSSIP select menu actions
        case CMSG_GOSSIP_HELLO:
        {
            // DEBUG_LOG ("[PlayerbotAI]: HandleMasterIncomingPacket - Received CMSG_GOSSIP_HELLO");

            WorldPacket p(packet);    //WorldPacket packet for CMSG_GOSSIP_HELLO, (8)
            uint64 guid;
            p.rpos(0);                //reset packet pointer
            p >> guid;


            Creature *pCreature = m_bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
            if (!pCreature)
            {
                //sLog->outDebug(LOG_FILTER_NONE, "[PlayerbotAI]: HandleMasterIncomingPacket - Received  CMSG_GOSSIP_HELLO object %s not found or you can't interact with him.", pCreature ? pCreature->GetName() : (char*)guid);
                return;
            }

            GossipMenuItemsMapBounds pMenuItemBounds = sObjectMgr->GetGossipMenuItemsMapBounds(pCreature->GetCreatureTemplate()->GossipMenuId);
            for (GossipMenuItemsContainer::const_iterator itr = pMenuItemBounds.first; itr != pMenuItemBounds.second; ++itr)
            {
                uint32 npcflags = pCreature->GetUInt32Value(UNIT_NPC_FLAGS);

                if (!(itr->second.OptionNpcflag & npcflags))
                    continue;

                switch (itr->second.OptionType)
                {
                    case GOSSIP_OPTION_TAXIVENDOR:
                    {
                        // bot->GetPlayerbotAI()->TellMaster("PlayerbotAI:GOSSIP_OPTION_TAXIVENDOR");
                        m_bot->GetSession()->SendLearnNewTaxiNode(pCreature);
                        break;
                    }
                    case GOSSIP_OPTION_QUESTGIVER:
                    {
                        // TellMaster("PlayerbotAI:GOSSIP_OPTION_QUESTGIVER");
                        TurnInQuests(pCreature);
                        break;
                    }
                    case GOSSIP_OPTION_VENDOR:
                    {
                        // bot->GetPlayerbotAI()->TellMaster("PlayerbotAI:GOSSIP_OPTION_VENDOR");
                        if (!m_confSellGarbage)
                            return;

                        SellGarbage(*m_bot);
                        break;
                    }
                    case GOSSIP_OPTION_STABLEPET:
                    {
                        // TellMaster("PlayerbotAI:GOSSIP_OPTION_STABLEPET");
                        break;
                    }
                    case GOSSIP_OPTION_AUCTIONEER:
                    {
                        // TellMaster("PlayerbotAI:GOSSIP_OPTION_AUCTIONEER");
                        break;
                    }
                    case GOSSIP_OPTION_BANKER:
                    {
                        // TellMaster("PlayerbotAI:GOSSIP_OPTION_BANKER");
                        break;
                    }
                    case GOSSIP_OPTION_INNKEEPER:
                    {
                        // TellMaster("PlayerbotAI:GOSSIP_OPTION_INNKEEPER");
                        break;
                    }
                }
            }
            return;
        }

        case CMSG_SPIRIT_HEALER_ACTIVATE:
        {
            // DEBUG_LOG ("[PlayerbotAI]: HandleMasterIncomingPacket - Received CMSG_SPIRIT_HEALER_ACTIVATE SpiritHealer is resurrecting the Player %s",m_master->GetName());
            Group *grp = m_bot->GetGroup();
            if (grp)
                grp->RemoveMember(m_bot->GetGUID(), GROUP_REMOVEMETHOD_KICK);
            return;
        }

        case CMSG_LIST_INVENTORY:
        {
            if (m_confSellGarbage)
                return;

            WorldPacket p(packet);
            p.rpos(0);  // reset reader
            uint64 npcGUID;
            p >> npcGUID;

            WorldObject* pNpc = (WorldObject*)sObjectAccessor->GetObjectByTypeMask(*m_bot, npcGUID, TYPEMASK_UNIT|TYPEMASK_GAMEOBJECT);

            if (!pNpc)
                return;
            if (!m_bot->IsInMap(pNpc))
            {
                TellMaster("I'm too far away to sell items!");
                return;
            }
            else
                SellGarbage(*m_bot);
            return;
        }

            /*
               case CMSG_NAME_QUERY:
               case MSG_MOVE_START_FORWARD:
               case MSG_MOVE_STOP:
               case MSG_MOVE_SET_FACING:
               case MSG_MOVE_START_STRAFE_LEFT:
               case MSG_MOVE_START_STRAFE_RIGHT:
               case MSG_MOVE_STOP_STRAFE:
               case MSG_MOVE_START_BACKWARD:
               case MSG_MOVE_HEARTBEAT:
               case CMSG_STANDSTATECHANGE:
               case CMSG_QUERY_TIME:
               case CMSG_CREATURE_QUERY:
               case CMSG_GAMEOBJECT_QUERY:
               case MSG_MOVE_JUMP:
               case MSG_MOVE_FALL_LAND:
                return;

               default:
               {
                const char* oc = LookupOpcodeName(packet.GetOpcode());
                // ChatHandler ch(m_master);
                // ch.SendSysMessage(oc);

                std::ostringstream out;
                out << "masterin: " << oc;
                //sLog->outError(out.str().c_str());
               }
             */
    }
}

float PlayerbotClassAI::GetCombatDistance(const Unit* target) const
{
    float dist = target->GetCombatReach();
    if (dist < 0.1f)
        dist = DEFAULT_COMBAT_REACH;
    return dist;
    //float radius = target->GetCombatReach() + m_bot->GetCombatReach();
    //float dx = m_bot->GetPositionX() - target->GetPositionX();
    //float dy = m_bot->GetPositionY() - target->GetPositionY();
    //float dz = m_bot->GetPositionZ() - target->GetPositionZ();
    //float dist = sqrt((dx*dx) + (dy*dy) + (dz*dz)) - radius;
    //return ( dist > 0 ? dist : 0);
}

bool PlayerbotClassAI::HasAuraName(Unit *unit, uint32 spellId, uint64 casterGuid) const
{
    const SpellInfo *const pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!pSpellInfo)
        return false;
    uint8 loc = m_master->GetSession()->GetSessionDbcLocale();
    const std::string  name = pSpellInfo->SpellName[loc];
    if (name.length() == 0)
        return false;
    return HasAuraName(unit, name, casterGuid);
}

bool PlayerbotClassAI::HasAuraName(Unit *target, std::string spell, uint64 casterGuid) const
{
    uint8 loc = m_master->GetSession()->GetSessionDbcLocale();

    Unit::AuraMap const &vAuras = target->GetOwnedAuras();
    for (Unit::AuraMap::const_iterator itr = vAuras.begin(); itr != vAuras.end(); ++itr)
    {
        SpellInfo const *spellInfo = itr->second->GetSpellInfo();
        const std::string name = spellInfo->SpellName[loc];
        if (spell == name)
            if (casterGuid == 0 || (casterGuid != 0 && casterGuid == itr->second->GetCasterGUID())) //only if correct caster casted it
                return true;
    }
    return false;
};

//See MainSpec enum in PlayerbotAI.h for details on class return values
uint32 Player::GetSpec()
{
    uint32 row = 0, spec = 0;//disabled

    //Iterate through the 3 talent trees
    for (uint32 i = 0; i < 3; ++i)
    {
        for (PlayerTalentMap::iterator iter = m_talents[m_activeSpec]->begin(); iter != m_talents[m_activeSpec]->end(); ++iter)
        {
            TalentEntry const *talentId = sTalentStore.LookupEntry((*iter).first);
            if (!talentId)
                continue;

            //If current talent is deeper into a tree, that is our new max talent
            if (talentId->Row > row)
            {
                row = talentId->Row;

                //Set the tree the deepest talent is on
                spec = talentId->TalentTab;
            }
        }
    }

    //Return the tree with the deepest talent
    return spec;
}

void Player::UpdateMail()
{
    // save money,items and mail to prevent cheating
    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    //SaveGoldToDB(trans);
    SaveInventoryAndGoldToDB(trans);
    _SaveMail(trans);
    CharacterDatabase.CommitTransaction(trans);
}

bool Player::requiredQuests(const char* pQuestIdString)
{
    if (pQuestIdString != NULL)
    {
        unsigned int pos = 0;
        unsigned int id;
        std::string confString(pQuestIdString);
        chompAndTrim(confString);
        while (getNextQuestId(confString, pos, id))
        {
            QuestStatus status = GetQuestStatus(id);
            if (status == QUEST_STATUS_COMPLETE)
                return true;
        }
    }
    return false;
}

void Player::chompAndTrim(std::string& str)
{
    while (str.length() > 0)
    {
        char lc = str[str.length() - 1];
        if (lc == '\r' || lc == '\n' || lc == ' ' || lc == '"' || lc == '\'')
            str = str.substr(0, str.length() - 1);
        else
            break;
    }

    while (str.length() > 0)
    {
        char lc = str[0];
        if (lc == ' ' || lc == '"' || lc == '\'')
            str = str.substr(1, str.length() - 1);
        else
            break;
    }
}

bool Player::getNextQuestId(const std::string& pString, unsigned int& pStartPos, unsigned int& pId)
{
    bool result = false;
    unsigned int i;
    for (i = pStartPos; i < pString.size(); ++i)
    {
        if (pString[i] == ',')
            break;
    }
    if (i > pStartPos)
    {
        std::string idString = pString.substr(pStartPos, i - pStartPos);
        pStartPos = i + 1;
        chompAndTrim(idString);
        pId = atoi(idString.c_str());
        result = true;
    }
    return(result);
}

void Player::skill(std::list<uint32>& m_spellsToLearn)
{
    for (SkillStatusMap::const_iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
        if (itr->second.uState != SKILL_DELETED)
            m_spellsToLearn.push_back(itr->first);
}

void Player::MakeTalentGlyphLink(std::ostringstream &out)
{
    // |cff4e96f7|Htalent:1396:4|h[Unleashed Fury]|h|r
    // |cff66bbff|Hglyph:23:460|h[Glyph of Fortitude]|h|r

    if (m_specsCount)
        // loop through all specs (only 1 for now)
        for (uint8 specIdx = 0; specIdx < m_specsCount; ++specIdx)
        {
            // find class talent tabs (all players have 3 talent tabs)
            uint32 const* talentTabIds = GetTalentTabPages(getClass());

            out << "\n" << "Active Talents ";

            for (uint8 i = 0; i < 3; ++i)
            {
                uint32 talentTabId = talentTabIds[i];
                for (PlayerTalentMap::iterator iter = m_talents[specIdx]->begin(); iter != m_talents[specIdx]->end(); ++iter)
                {
                    PlayerTalent* talent = (*iter).second;
                    TalentEntry const *talentId = sTalentStore.LookupEntry((*iter).first);
                    if (!talentId)
                        continue;

                    if (talent->state == PLAYERSPELL_REMOVED)
                        continue;

                    // skip another tab talents
                    if (talentId->TalentTab != talentTabId)
                        continue;

                    SpellInfo const *_spellEntry;
                    int8 rank = 0;
                    for (rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
                    {
                        if (talentId->RankID[rank] == 0)
                            continue;
                        _spellEntry = sSpellMgr->GetSpellInfo(talentId->RankID[rank]);
                        if (!_spellEntry)
                            continue;
                        if (m_bot->HasSpell(talentId->RankID[rank]))
                            break;
                    }

                    if (rank == 0)
                        continue;

                    _spellEntry = sSpellMgr->GetSpellInfo(talentId->RankID[rank]);

                    out << "|cff4e96f7|Htalent:" << talentId->TalentID << ":" << rank
                        << " |h[" << _spellEntry->SpellName[GetSession()->GetSessionDbcLocale()] << "]|h|r";
                }
            }

            uint32 freepoints = GetFreeTalentPoints();

            out << " Unspent points : ";
            out << "|h|cffff0000" << freepoints << "|h|r";
            out << "\n" << "Active Glyphs ";

            // GlyphProperties.dbc
            for (uint8 i = 0; i < MAX_GLYPH_SLOT_INDEX; ++i)
            {
                GlyphPropertiesEntry const* glyph = sGlyphPropertiesStore.LookupEntry(m_Glyphs[specIdx][i]);
                if (!glyph)
                    continue;

                SpellInfo const *spell_entry = sSpellMgr->GetSpellInfo(glyph->SpellId);

                out << "|cff66bbff|Hglyph:" << GetGlyphSlot(i) << ":" << m_Glyphs[specIdx][i]
                    << " |h[" << spell_entry->SpellName[GetSession()->GetSessionDbcLocale()] << "]|h|r";

            }
        }
}
