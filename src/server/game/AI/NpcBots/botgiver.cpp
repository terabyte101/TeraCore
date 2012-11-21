#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "ObjectMgr.h"
#include "Config.h"
#include "Group.h"
#include "Player.h"

const uint8 GroupIcons[TARGETICONCOUNT] =
{
    /*STAR        = */0x001,
    /*CIRCLE      = */0x002,
    /*DIAMOND     = */0x004,
    /*TRIANGLE    = */0x008,
    /*MOON        = */0x010,
    /*SQUARE      = */0x020,
    /*CROSS       = */0x040,
    /*SKULL       = */0x080,
};
class script_bot_giver : public CreatureScript
{
public:
    script_bot_giver() : CreatureScript("script_bot_giver") { }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {
        switch (sender)
        {
            case 6006: SendCreateNPCBotMenu(player, creature, action); break;
            case 6001: SendCreateNPCBot(player, creature, action); break;
            case 6002: SendCreatePlayerBotMenu(player, creature, action); break;
            case 6003: SendCreatePlayerBot(player, creature, action); break;

            case 6004: SendRemovePlayerBotMenu(player, creature, action); break;
            case 6005: SendRemovePlayerBot(player, creature, action); break;
            case 6007: SendRemoveNPCBotMenu(player, creature, action); break;
            case 6008: SendRemoveNPCBot(player, creature, action); break;

            case 6009: SendBotHelpWhisper(player, creature, action); break;
        }
        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        WorldSession* session = player->GetSession();
        uint8 count = 0;
        uint8 maxcount = ConfigMgr::GetIntDefault("Bot.MaxPlayerbots", 9);
        bool allowPBots = ConfigMgr::GetBoolDefault("Bot.EnablePlayerBots", true);
        bool allowNBots = ConfigMgr::GetBoolDefault("Bot.EnableNpcBots", true);

        for (PlayerBotMap::const_iterator itr = session->GetPlayerBotsBegin(); itr != session->GetPlayerBotsEnd(); ++itr)
        {
            if (count == 0)
                player->ADD_GOSSIP_ITEM(0, "Abandon my Player", 6004, GOSSIP_ACTION_INFO_DEF + 100);
            ++count;
        }
        if (count < maxcount && allowPBots)
            player->ADD_GOSSIP_ITEM(0, "Recruit a Player", 6002, GOSSIP_ACTION_INFO_DEF + 1);

        maxcount = player->GetMaxNpcBots();
        if (player->HaveBot())
        {
            count = player->GetNpcBotsCount();
            if (count > 0)
                player->ADD_GOSSIP_ITEM(0, "Abandon my Minion", 6007, GOSSIP_ACTION_INFO_DEF + 101);
            if (count < maxcount && allowNBots)
                player->ADD_GOSSIP_ITEM(0, "Recruit a Minion", 6006, GOSSIP_ACTION_INFO_DEF + 2);
        }
        else if (allowNBots)
            player->ADD_GOSSIP_ITEM(0, "Recruit a Minion", 6006, GOSSIP_ACTION_INFO_DEF + 2);

        player->ADD_GOSSIP_ITEM(0, "Tell me about these bots", 6009, GOSSIP_ACTION_INFO_DEF + 200);

        player->PlayerTalkClass->SendGossipMenu(8446, creature->GetGUID());
        return true;
    }

    static void SendCreatePlayerBot(Player* player, Creature*  /*creature*/, uint32 action)
    {
        std::list<std::string>* names = player->GetCharacterList();
        if (names == NULL || names->empty())
        {
            player->CLOSE_GOSSIP_MENU();
            return;
        }

        int8 x = action - GOSSIP_ACTION_INFO_DEF - 1;

        for (std::list<std::string>::iterator iter = names->begin(); iter != names->end(); iter++)
        {
            if (x == 0)
            {
                uint64 guid = sObjectMgr->GetPlayerGUIDByName((*iter).c_str());
                if (player->GetSession()->GetPlayerBot(guid) != NULL)
                    return;
                player->GetSession()->AddPlayerBot(guid);
            }
            else
            {
                if (x == 1)
                {
                    uint64 guid = sObjectMgr->GetPlayerGUIDByName((*iter).c_str());
                    if (player->GetSession()->GetPlayerBot(guid) != NULL)
                        return;
                    player->GetSession()->AddPlayerBot(guid);
                    break;
                }
                --x;
            }
        }
        player->CLOSE_GOSSIP_MENU();
    }

    static void SendCreatePlayerBotMenu(Player* player, Creature* creature, uint32 /*action*/)
    {
        std::list<std::string>* names = player->GetCharacterList();
        if (names == NULL || names->empty())
        {
            player->CLOSE_GOSSIP_MENU();
            return;
        }

        player->PlayerTalkClass->ClearMenus();
        player->ADD_GOSSIP_ITEM(9, "ADD ALL" , 6003, GOSSIP_ACTION_INFO_DEF + 1);
        int8 x = 2;

        for (std::list<std::string>::iterator iter = names->begin(); iter != names->end(); iter++)
        {
            player->ADD_GOSSIP_ITEM(9, (*iter).c_str() , 6003, GOSSIP_ACTION_INFO_DEF + x);
            ++x;
        }
        player->PlayerTalkClass->SendGossipMenu(8446, creature->GetGUID());
    } //end SendCreatePlayerBotMenu

    static void SendRemovePlayerBotAll(Player* player, Creature* creature)
    {
        for (int8 x = 2; x<=10; x++ )
        {
            SendRemovePlayerBot(player, creature, GOSSIP_ACTION_INFO_DEF + 2);
        }
    }

    static void SendRemoveNPCBot(Player* player, Creature*  /*creature*/, uint32 action)
    {
        int8 x = action - GOSSIP_ACTION_INFO_DEF - 1;
        if (x == 0)
        {
            player->CLOSE_GOSSIP_MENU();
            for (uint8 i = 0; i != player->GetMaxNpcBots(); ++i)
                player->RemoveBot(player->GetBotMap()[i]._Guid(), true);
            return;
        }
        for (uint8 i = 0; i != player->GetMaxNpcBots(); ++i)
        {
            if (x == 1 && player->GetBotMap()[i]._Cre())
            {
                player->RemoveBot(player->GetBotMap()[i]._Guid(), true);
                break;
            }
            --x;
        }
        player->CLOSE_GOSSIP_MENU();
    }

    static void SendRemovePlayerBot(Player* player, Creature* creature, uint32 action)
    {
        int8 x = action - GOSSIP_ACTION_INFO_DEF - 1;

        if (x == 0)
        {
            SendRemovePlayerBotAll(player, creature);
            return;
        }

        WorldSession* session = player->GetSession();
        for (PlayerBotMap::const_iterator itr = session->GetPlayerBotsBegin(); itr != session->GetPlayerBotsEnd(); ++itr)
        {
            if (x == 1 && itr->second)
            {
                session->LogoutPlayerBot(itr->second->GetGUID());
                break;
            }
            --x;
        }
        player->CLOSE_GOSSIP_MENU();
    } //end SendRemovePlayerBot

    static void SendRemovePlayerBotMenu(Player* player, Creature* creature, uint32 /*action*/)
    {
        player->PlayerTalkClass->ClearMenus();
        player->ADD_GOSSIP_ITEM(9, "REMOVE ALL", 6005, GOSSIP_ACTION_INFO_DEF + 1);

        uint8 x = 2;
        WorldSession* session = player->GetSession();
        for (PlayerBotMap::const_iterator itr = session->GetPlayerBotsBegin(); itr != session->GetPlayerBotsEnd(); ++itr)
        {
            Player* bot = itr->second;
            player->ADD_GOSSIP_ITEM(9, bot->GetName(), 6005, GOSSIP_ACTION_INFO_DEF + x);
            ++x;
        }
        player->PlayerTalkClass->SendGossipMenu(8446, creature->GetGUID());
    } //end SendRemovePlayerBotMenu

    static void SendRemoveNPCBotMenu(Player* player, Creature* creature, uint32 /*action*/)
    {
        player->PlayerTalkClass->ClearMenus();
        if (player->GetNpcBotsCount() == 1)
        {
            for (uint8 i = 0; i != player->GetMaxNpcBots(); ++i)
                player->RemoveBot(player->GetBotMap()[i]._Guid(), true);
            player->CLOSE_GOSSIP_MENU();
            return;
        }
        player->ADD_GOSSIP_ITEM(9, "REMOVE ALL", 6008, GOSSIP_ACTION_INFO_DEF + 1);

        uint8 x = 2;
        for (uint8 i = 0; i != player->GetMaxNpcBots(); ++i)
        {
            Creature* bot = player->GetBotMap()[i]._Cre();
            if (!bot) continue;
            player->ADD_GOSSIP_ITEM(9, bot->GetName(), 6008, GOSSIP_ACTION_INFO_DEF + x);
            ++x;
        }
        player->PlayerTalkClass->SendGossipMenu(8446, creature->GetGUID());
    }

    static void SendCreateNPCBot(Player* player, Creature*  /*creature*/, uint32 action)
    {
        uint8 bot_class = 0;
        if (action == GOSSIP_ACTION_INFO_DEF + 1)//"Back"
        {
            player->CLOSE_GOSSIP_MENU();
            return;
        }
        else if (action == GOSSIP_ACTION_INFO_DEF + 2)
            bot_class = CLASS_WARRIOR;
        //else if (action == GOSSIP_ACTION_INFO_DEF + 3)
        //    bot_class = CLASS_HUNTER;
        else if (action == GOSSIP_ACTION_INFO_DEF + 4)
            bot_class = CLASS_PALADIN;
        //else if (action == GOSSIP_ACTION_INFO_DEF + 5)
        //    bot_class = CLASS_SHAMAN;
        else if (action == GOSSIP_ACTION_INFO_DEF + 6)
            bot_class = CLASS_ROGUE;
        else if (action == GOSSIP_ACTION_INFO_DEF + 7)
            bot_class = CLASS_DRUID;
        else if (action == GOSSIP_ACTION_INFO_DEF + 8)
            bot_class = CLASS_MAGE;
        else if (action == GOSSIP_ACTION_INFO_DEF + 9)
            bot_class = CLASS_PRIEST;
        else if (action == GOSSIP_ACTION_INFO_DEF + 10)
            bot_class = CLASS_WARLOCK;
        //else if (action == GOSSIP_ACTION_INFO_DEF + 11)
        //    bot_class = CLASS_DEATH_KNIGHT;

        if (bot_class != 0)
            player->CreateNPCBot(bot_class);
        player->CLOSE_GOSSIP_MENU();
        return;
    }

    static void SendCreateNPCBotMenu(Player* player, Creature* creature, uint32 /*action*/)
    {
        player->PlayerTalkClass->ClearMenus();
        player->ADD_GOSSIP_ITEM(9, "Recruit a Warrior", 6001, GOSSIP_ACTION_INFO_DEF + 2);
        //player->ADD_GOSSIP_ITEM(9, "Recruit a Hunter", 6001, GOSSIP_ACTION_INFO_DEF + 3);
        player->ADD_GOSSIP_ITEM(9, "Recruit a Paladin", 6001, GOSSIP_ACTION_INFO_DEF + 4);
        //player->ADD_GOSSIP_ITEM(9, "Recruit a Shaman", 6001, GOSSIP_ACTION_INFO_DEF + 5);
        player->ADD_GOSSIP_ITEM(9, "Recruit a Rogue", 6001, GOSSIP_ACTION_INFO_DEF + 6);
        player->ADD_GOSSIP_ITEM(3, "Recruit a Druid", 6001, GOSSIP_ACTION_INFO_DEF + 7);
        player->ADD_GOSSIP_ITEM(3, "Recruit a Mage", 6001, GOSSIP_ACTION_INFO_DEF + 8);
        player->ADD_GOSSIP_ITEM(3, "Recruit a Priest", 6001, GOSSIP_ACTION_INFO_DEF + 9);
        player->ADD_GOSSIP_ITEM(3, "Recruit a Warlock", 6001, GOSSIP_ACTION_INFO_DEF + 10);
        //player->ADD_GOSSIP_ITEM(9, "Recruit a Death Knight", 1, GOSSIP_ACTION_INFO_DEF + 11);
        player->PlayerTalkClass->SendGossipMenu(8446, creature->GetGUID());
    }

    static void SendBotHelpWhisper(Player* player, Creature* creature, uint32 /*action*/)
    {
        player->CLOSE_GOSSIP_MENU();
        //Basic
        std::string msg1 = "To see list of Playerbot commands whisper 'help' to one of your playerbots";
        std::string msg2 = "To see list of available npcbot commands type .npcbot or .npcb";
        std::string msg3 = "You can also use .maintank (or .mt or .main) command on any party member (even npcbot) so your bots will stick to your plan";
        creature->MonsterWhisper(msg1.c_str(), player->GetGUID());
        creature->MonsterWhisper(msg2.c_str(), player->GetGUID());
        creature->MonsterWhisper(msg3.c_str(), player->GetGUID());
        //Heal Icons
        uint8 mask = ConfigMgr::GetIntDefault("Bot.HealTargetIconsMask", 8);
        std::string msg4 = "";
        if (mask == 255)
        {
            msg4 = "If you want your npcbots to heal someone out of your party set any raid target icon on them";
            creature->MonsterWhisper(msg4.c_str(), player->GetGUID());
        }
        else if (mask != 0)
        {
            msg4 = "If you want your npcbots to heal someone out of your party set proper raid target icon on them, one of these: ";
            std::string iconrow = "";
            uint8 count = 0;
            for (uint8 i = 0; i != TARGETICONCOUNT; ++i)
            {
                if (mask & GroupIcons[i])
                {
                    if (count != 0)
                        iconrow += ", ";
                    ++count;
                    switch (i)
                    {
                        case 0: iconrow += "star"; break;
                        case 1: iconrow += "circle"; break;
                        case 2: iconrow += "diamond"; break;
                        case 3: iconrow += "triangle"; break;
                        case 4: iconrow += "moon"; break;
                        case 5: iconrow += "square"; break;
                        case 6: iconrow += "cross"; break;
                        case 7: iconrow += "skull"; break;
                        //debug
                        default: iconrow += "unknown icon"; break;
                    }
                }
            }
            msg4 += iconrow;
            creature->MonsterWhisper(msg4.c_str(), player->GetGUID());
        }
        //End
    }
};

//This function is called when the player clicks an option on the gossip menu
void AddSC_script_bot_giver()
{
    new script_bot_giver();
}
