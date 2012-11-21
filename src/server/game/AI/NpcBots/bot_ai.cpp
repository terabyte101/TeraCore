#include "bot_ai.h"
#include "Config.h"
#include "Chat.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "SpellAuraEffects.h"
/*
NpcBot System by Graff (onlysuffering@gmail.com)
Original patch (npcbot part only) from: LordPsyan https://bitbucket.org/lordpsyan/trinitycore-patches/src/3b8b9072280e/Individual/11185-BOTS-NPCBots.patch
TODO:
Convert doCast events (CD etc.) into SpellHit()- and SpellHitTarget()-based
Implement heal/tank/DD modes
Implement Racial Abilities
Implement Equipment Change (maybe)
I NEED MORE
*/
const uint8 GroupIconsFlags[TARGETICONCOUNT] =
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

bot_minion_ai::bot_minion_ai(Creature* creature): bot_ai(creature)
{
    Potion_cd = 0;
    pvpTrinket_cd = 30000;
    rezz_cd = 0;
    myangle = 0.f;
}
bot_minion_ai::~bot_minion_ai(){}

bot_pet_ai::bot_pet_ai(Creature* creature): bot_ai(creature)
{
    m_creatureOwner = NULL;
    basearmor = 0;
}
bot_pet_ai::~bot_pet_ai(){}

bot_ai::bot_ai(Creature* creature) : ScriptedAI(creature)
{
    master = me->GetBotOwner();
    m_spellpower = 0;
    haste = 0;
    hit = 0.f;
    regen_mp5 = 0.f;
    m_TankGuid = 0;
    tank = NULL;
    extank = NULL;
    info = NULL;
    clear_cd = 2;
    temptimer = 0;
    wait = 15;
    GC_Timer = 0;
    checkAurasTimer = 20;
    cost = 0;
    doHealth = false;
    doMana = false;
    pos.m_positionX = 0.f;
    pos.m_positionY = 0.f;
    pos.m_positionZ = 0.f;
    aftercastTargetGuid = 0;
    currentSpell = 0;
    dmgmult_melee = ConfigMgr::GetFloatDefault("Bot.DamageMult.Melee", 1.0);
    dmgmult_spell = ConfigMgr::GetFloatDefault("Bot.DamageMult.Spell", 1.0);
    dmgmult_melee = std::max(dmgmult_melee, 0.01f);
    dmgmult_spell = std::max(dmgmult_spell, 0.01f);
    dmgmult_melee = std::min(dmgmult_melee, 10.f);
    dmgmult_spell = std::min(dmgmult_spell, 10.f);
    dmgmod_melee = Creature::_GetDamageMod(me->GetCreatureTemplate()->rank);
    dmgmod_spell = me->GetSpellDamageMod(me->GetCreatureTemplate()->rank);
    healTargetIconFlags = ConfigMgr::GetIntDefault("Bot.HealTargetIconsMask", 8);
}
bot_ai::~bot_ai(){}

SpellCastResult bot_ai::checkBotCast(Unit* victim, uint32 spellId, uint8 botclass) const
{
    if (spellId == 0) return SPELL_FAILED_DONT_REPORT;
    if (!CheckImmunities(spellId, victim)) return SPELL_FAILED_DONT_REPORT;
    if (InDuel(victim)) return SPELL_FAILED_DONT_REPORT;

    switch (botclass)
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_DRUID:
        case CLASS_WARLOCK:
        case CLASS_SHAMAN:
            if (Feasting() && !master->isInCombat() && !master->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE))
                return SPELL_FAILED_DONT_REPORT;
            return SPELL_CAST_OK;
        case CLASS_PALADIN:
            //Crusader Strike
            if (spellId != 35395 && spellId != MANAPOTION && spellId != HEALINGPOTION && me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
                return SPELL_FAILED_DONT_REPORT;
            return SPELL_CAST_OK;
        case CLASS_WARRIOR:
            //BladeStorm
            if (me->HasAura(46924/*67541*/))
                return SPELL_FAILED_DONT_REPORT;
            return SPELL_CAST_OK;
        case CLASS_ROGUE:
        case CLASS_HUNTER:
        case CLASS_DEATH_KNIGHT:
        default:
            return SPELL_CAST_OK;
    }
}

bool bot_ai::doCast(Unit* victim, uint32 spellId, bool triggered, uint64 originalCaster)
{
    if (spellId == 0) return false;
    if (me->IsMounted()) return false;
    if (IsCasting()) return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;
    info = spellInfo;

    if (spellId == MANAPOTION)
    {
        value = urand(me->GetMaxPower(POWER_MANA)/4, me->GetMaxPower(POWER_MANA)/2);
        me->CastCustomSpell(victim, spellId, &value, 0, 0, true);
        return true;
    }

    if (me->GetShapeshiftForm() != FORM_NONE && info->CheckShapeshift(me->GetShapeshiftForm()) != SPELL_CAST_OK)
        removeFeralForm(true, true);

    if (spellId != HEALINGPOTION && spellId != MANAPOTION)
        me->SetStandState(UNIT_STAND_STATE_STAND);

    if (!victim->IsWithinLOSInMap(me) && IsInBotParty(victim))
    {
        //std::ostringstream msg;
        //msg << "casting " << spellInfo->SpellName[0] << " on " << victim->GetName();
        //me->MonsterWhisper(msg.str().c_str(), master->GetGUID());
        me->Relocate(victim);
    }

    TriggerCastFlags flags = triggered ? TRIGGERED_FULL_MASK : TRIGGERED_NONE;
    SpellCastTargets targets;
    targets.SetUnitTarget(victim);
    Spell* spell = new Spell(me, info, flags, originalCaster);
    spell->prepare(&targets);//sets current spell if succeed

    bool casted = triggered;//triggered casts are casted immediately
    for (uint8 i = 0; i != CURRENT_MAX_SPELL; ++i)
        if (Spell* curspell = me->GetCurrentSpell(i))
            if (curspell == spell)
                casted = true;

    if (!casted)
    {
        //failed to cast
        //delete spell;//crash due to invalid event added to master's eventmap
        return false;
    }

    currentSpell = spellId;

    switch (me->GetBotClass())
    {
        case CLASS_ROGUE:
        case CAT:
            value = int32(1000.f - 1000.f*(float(haste) / 100.f));
            break;
        default:
            value = int32(1500.f - 1500.f*(float(haste) / 100.f));
            break;
    }
    GC_Timer = std::max<uint32>(value, 500);

    return true;
}
//Follow point calculation
void bot_minion_ai::CalculatePos(Position & pos)
{
    uint8 followdist = master->GetBotFollowDist();
    Unit* followTarget = master;
    float mydist, angle;
    if (master->GetBotTankGuid() == me->GetGUID())
    {
        if (master->GetPlayerbotAI())
            followTarget = master->GetSession()->m_master; 
        mydist = frand(3.5f, 6.5f);
        angle = (M_PI/2.f) / 16.f * frand(-3.f, 3.f);
    }
    else
    {
        switch (me->GetBotClass())
        {
            case CLASS_WARRIOR: case CLASS_DEATH_KNIGHT: case CLASS_PALADIN: case BEAR:
                mydist = frand(0.2f, 1.f);//(1.f, 3.f);//RAND(1.f,1.5f,2.f,2.5f,3.f,3.5f);
                angle = (M_PI/2.f) / 8.f * RAND(frand(5.f, 10.f), frand(-10.f, -5.f));//RAND(6.5f,7.f,7.5f,8.f,8.5f,9.f,9.5f,-6.5f,-7.f,-7.5f,-8.f,-8.5f,-9.f,-9.5f);
                break;
            case CLASS_WARLOCK: case CLASS_PRIEST: case CLASS_MAGE: case CAT:
                mydist = frand(0.15f, 0.8f);//(0.5f, 2.f);//RAND(0.5f,1.f,1.5f,2.f);
                angle = (M_PI/2.f) / 6.f * frand(10.5f, 13.5f);//RAND(10.5f,11.f,11.5f,12.f,12.5f,13.f,13.5f);
                break;
            default:
                mydist = frand(0.3f, 1.2f);//(2.5f, 4.f);//RAND(2.5f,3.f,3.5f,4.f);
                angle = (M_PI/2.f) / 6.f * frand(9.f, 15.f);//RAND(9.f,10.f,11.f,12.f,13.f,14.f,15.f);
                break;
        }
    }
    if (abs(abs(myangle) - abs(angle)) > M_PI/3.f)
        myangle = angle;
    else
        angle = myangle;
    mydist += followdist > 10 ? float(followdist - 10)/4.f : 0.f;
    mydist = std::min<float>(mydist, 35.f);
    angle += followTarget->GetOrientation();
    float x(0),y(0),z(0);
    float size = me->GetObjectSize()/3.f;
    bool over = false;
    for (uint8 i = 0; i != 5 + over; ++i)
    {
        if (over)
        {
            mydist *= 0.2f;
            break;
        }
        followTarget->GetNearPoint(me, x, y, z, size, mydist, angle);
        if (!master->IsWithinLOS(x,y,z))
        {
            mydist *= 0.4f - float(i*0.07f);
            size *= 0.1f;
            if (size < 0.1)
                size = 0.f;
            if (size == 0.f && me->GetPositionZ() < followTarget->GetPositionZ())
                z += 0.25f;
        }
        else
            over = true;
    }
    pos.m_positionX = x;
    pos.m_positionY = y;
    pos.m_positionZ = z;

    //            T   
    //           TTT
    //    mmmmmmmm mmmmmmmm 
    //   mmmmmmm MMM mmmmmmm 
    //   mmmmm rrrrrrr mmmmm 
    //    ddd rrrrrrrrr ddd
    //     ddddddddddddddd
    //       ddddddddddd
    //         
    //MMM - player
    //TTT - bot tank
    //m - melee (warrior, paladin, deathknight)
    //d - default (druid, shaman, rogue, hunter)
    //r - ranged/support (priest, warlock, mage)
}
// Movement set
void bot_minion_ai::SetBotCommandState(CommandStates st, bool force, Position* newpos)
{
    if (me->isDead())
        return;
    if (st == COMMAND_FOLLOW && ((!me->isMoving() && !IsCasting() && master->isAlive()) || force))
    {
        if (CCed(me, true)/* || master->HasUnitState(UNIT_STATE_FLEEING)*/) return;
        if (!newpos)
            CalculatePos(pos);
        else
        {
            pos.m_positionX = newpos->m_positionX;
            pos.m_positionY = newpos->m_positionY;
            pos.m_positionZ = newpos->m_positionZ;
        }
        if (me->getStandState() == UNIT_STAND_STATE_SIT)
            me->SetStandState(UNIT_STAND_STATE_STAND);
        me->GetMotionMaster()->MovePoint(master->GetMapId(), pos);
        //me->GetMotionMaster()->MoveFollow(master, mydist, angle);
    }
    else if (st == COMMAND_STAY)
    {
        me->StopMoving();
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
    }
    else if (st == COMMAND_ATTACK)
    { }
    m_botCommandState = st;
    if (Creature* m_botsPet = me->GetBotsPet())
        m_botsPet->SetBotCommandState(st, force);
}

void bot_pet_ai::SetBotCommandState(CommandStates st, bool force, Position* /*newpos*/)
{
    if (me->isDead())
        return;
    if (st == COMMAND_FOLLOW && ((!me->isMoving() && !IsCasting() && master->isAlive()) || force))
    {
        if (CCed(me, true)) return;
        Unit* followtarget = m_creatureOwner;
        if (CCed(m_creatureOwner))
            followtarget = master;
        if (followtarget == m_creatureOwner)
        {
            if (!me->HasUnitState(UNIT_STATE_FOLLOW) || me->GetDistance(master)*0.75f < me->GetDistance(m_creatureOwner))
                me->GetMotionMaster()->MoveFollow(m_creatureOwner, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
        }
        else
            if (!me->HasUnitState(UNIT_STATE_FOLLOW) || me->GetDistance(m_creatureOwner)*0.75f < me->GetDistance(master))
                me->GetMotionMaster()->MoveFollow(master, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
    }
    else if (st == COMMAND_STAY)//NUY
    {
        me->StopMoving();
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
    }
    else if (st == COMMAND_ATTACK)
    { }
    m_botCommandState = st;
}
// Get Maintank
void bot_ai::FindTank()
{
    if (tank == me)
        extank = me;
    //check group flags in DB
    tank = _GetBotGroupMainTank(master->GetGroup());
    //check if master has set tank
    if (!tank)
        tank = master->GetBotTankGuid() != 0 ? sObjectAccessor->GetObjectInWorld(master->GetBotTankGuid(), (Unit*)NULL) : NULL;
    //check if we have tank flag in master's motmap
    if (!tank)
        tank = master->GetBotTank(me->GetEntry());
    //at last try to find tank by class if master is too lazy to set it
    if (!tank)
    {
        Player* owner = master->GetPlayerbotAI() ? master->GetSession()->m_master : master;
        uint8 Class = owner->getClass();
        if (owner->isAlive() && 
            (Class == CLASS_WARRIOR || Class == CLASS_PALADIN || Class == CLASS_DEATH_KNIGHT))
            tank = owner;
        else if (owner != master && master->isAlive())
        {
            Class = master->getClass();
            if (Class == CLASS_WARRIOR || Class == CLASS_PALADIN || Class == CLASS_DEATH_KNIGHT)
                tank = master;
        }
    }
    //it happens to every bot so they all will know who the tank is
    if (tank != extank)
        me->SetBotTank(tank);
    if (tank == me)
    {
        //if tank set by entry let master get right guid and set tank in botmap
        if (master->GetBotTankGuid() != me->GetGUID())
            master->SetBotTank(me->GetGUID());
    }
}
//Get Group maintank
Unit* bot_ai::_GetBotGroupMainTank(Group* group)
{
    if (!group)
        return NULL;
    QueryResult result = CharacterDatabase.PQuery("SELECT memberGuid, memberFlags FROM `group_member` WHERE `guid`='%u'", group->GetGUID());
    if (!result)
        return NULL;
    Unit* unit = NULL;
    do
    {
        Field* field = result->Fetch();
        uint32 lowGuid = field[0].GetInt32();
        uint8 flags = field[1].GetInt8();
        if (flags & MEMBER_FLAG_MAINTANK)
        {
            Group::MemberSlotList const &members = group->GetMemberSlots();
            for (Group::MemberSlotList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
                if (GUID_LOPART(itr->guid) == lowGuid)
                    unit = sObjectAccessor->FindUnit(itr->guid);
        }
    } while (result->NextRow() && !unit);
    return unit;
}
// Buffs And Heal (really)
void bot_minion_ai::BuffAndHealGroup(Player* gPlayer, const uint32 diff)
{
    if (GC_Timer > diff) return;
    if (me->IsMounted()) return;
    if (IsCasting() || Feasting()) return; // if I'm already casting

    Group* pGroup = gPlayer->GetGroup();
    if (!pGroup)
    {
        HealTarget(master, GetHealthPCT(master), diff);
        BuffTarget(master, diff);
        for (Unit::ControlList::const_iterator itr = master->m_Controlled.begin(); itr != master->m_Controlled.end(); ++itr)
        {
            Unit* u = *itr;
            if (!u || u->isDead()) continue;
            if (HealTarget(u, GetHealthPCT(u), diff))
                return;
            if (Creature* cre = u->ToCreature())
                if (cre->GetIAmABot() || cre->isPet())
                    if (BuffTarget(u, diff))
                        return;
        }
        return;
    }
    bool Bots = false;
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* tPlayer = itr->getSource();
        if (tPlayer == NULL) continue;
        if (me->GetMap() != tPlayer->GetMap()) continue;
        if (!tPlayer->m_Controlled.empty())
            Bots = true;
        if (tPlayer->isDead()) continue;
        if (HealTarget(tPlayer, GetHealthPCT(tPlayer), diff))
            return;
        if (BuffTarget(tPlayer, diff))
            return;
    }
    if (Bots)
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* tPlayer = itr->getSource();
            if (tPlayer == NULL || tPlayer->m_Controlled.empty()) continue;
            if (me->GetMap() != tPlayer->GetMap()) continue;
            for (Unit::ControlList::const_iterator itr = tPlayer->m_Controlled.begin(); itr != tPlayer->m_Controlled.end(); ++itr)
            {
                Unit* u = *itr;
                if (!u || u->isDead()) continue;
                if (HealTarget(u, GetHealthPCT(u), diff))
                    return;
                if (Creature* cre = u->ToCreature())
                    if (cre->GetIAmABot() || cre->isPet())
                        if (BuffTarget(u, diff))
                            return;
            }
        }
    }
    //check if we have pointed heal target
    for (uint8 i = 0; i != TARGETICONCOUNT; ++i)
    {
        if (healTargetIconFlags & GroupIconsFlags[i])
        {
            if (uint64 guid = pGroup->GetTargetIcons()[i])//check this one
            {
                if (Unit* unit = sObjectAccessor->FindUnit(guid))
                {
                    if (unit->isAlive() && me->GetMap() == unit->GetMap() && 
                        master->getVictim() != unit && unit->getVictim() != master && 
                        unit->GetReactionTo(master) >= REP_NEUTRAL)
                    {
                        HealTarget(unit, GetHealthPCT(unit), diff);
                        //CureTarget(unit, getCureSpell(), diff);
                    }
                }
            }
        }
    }
}
// Attempt to resurrect dead players using class spells
// Targets either player or its corpse
void bot_minion_ai::RezGroup(uint32 REZZ, Player* gPlayer)
{
    if (!REZZ || !gPlayer || me->IsMounted()) return;
    if (IsCasting()) return; // if I'm already casting
    if (rezz_cd > 0) return;

    //sLog->outBasic("RezGroup by %s", me->GetName().c_str());
    Group* pGroup = gPlayer->GetGroup();
    if (!pGroup)
    {
        Unit* target = master;
        if (master->isAlive()) return;
        if (master->isRessurectRequested()) return; //ressurected
        if (master->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
            target = (Unit*)master->GetCorpse();
        if (me->GetDistance(target) > 30)
        {
            me->GetMotionMaster()->MovePoint(master->GetMapId(), *target);
            rezz_cd = 3;//6-9 sec reset
            return;
        }
        else if (!target->IsWithinLOSInMap(me))
            me->Relocate(*target);

        if (doCast(target, REZZ))//rezzing it
        {
            me->MonsterWhisper("Rezzing You", master->GetGUID());
            rezz_cd = 60;
        }
        return;
    }
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* tPlayer = itr->getSource();
        Unit* target = tPlayer;
        if (!tPlayer || tPlayer->isAlive()) continue;
        if (tPlayer->isRessurectRequested()) continue; //ressurected
        if (Rand() > 5) continue;
        if (tPlayer->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
            target = (Unit*)tPlayer->GetCorpse();
        if (master->GetMap() != target->GetMap()) continue;
        if (me->GetDistance(target) > 30)
        {
            me->GetMotionMaster()->MovePoint(master->GetMapId(), *target);
            rezz_cd = 3;//6-9 sec reset
            return;
        }
        else if (!target->IsWithinLOSInMap(me))
            me->Relocate(*target);

        if (doCast(target, REZZ))//rezzing it
        {
            me->MonsterWhisper("Rezzing You", tPlayer->GetGUID());
            rezz_cd = 60;
            return;
        }
    }
}
// CURES
//cycle through the group sending members for cure
void bot_minion_ai::CureGroup(Player* pTarget, uint32 cureSpell, const uint32 diff)
{
    if (!cureSpell || GC_Timer > diff) return;
    if (me->getLevel() < 10 || pTarget->getLevel() < 10) return;
    if (me->IsMounted()) return;
    if (IsCasting() || Feasting()) return;
    if (!master->GetMap()->IsRaid() && Rand() > 75) return;
    //sLog->outBasic("%s: CureGroup() on %s", me->GetName().c_str(), pTarget->GetName().c_str());
    Group* pGroup = pTarget->GetGroup();
    if (!pGroup)
    {
        CureTarget(master, cureSpell, diff);
        for (uint8 i = 0; i != master->GetMaxNpcBots(); ++i)
        {
            Creature* cre = master->GetBotMap()[i]._Cre();
            if (!cre || !cre->IsInWorld() || me->GetDistance(cre) > 30) continue;
            if (CureTarget(cre, cureSpell, diff))
                return;
        }
    }
    else
    {
        bool Bots = false;
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* tPlayer = itr->getSource();
            if (!tPlayer || (tPlayer->isDead() && !tPlayer->HaveBot())) continue;
            if (!Bots && tPlayer->HaveBot())
                Bots = true;
            if (me->GetDistance(tPlayer) > 30) continue;
            if (CureTarget(tPlayer, cureSpell, diff))
                return;
        }
        if (!Bots) return;
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* tPlayer = itr->getSource();
            if (tPlayer == NULL || !tPlayer->HaveBot()) continue;
            for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
            {
                Creature* cre = tPlayer->GetBotMap()[i]._Cre();
                if (!cre || !cre->IsInWorld() || me->GetDistance(cre) > 30) continue;
                if (CureTarget(cre, cureSpell, diff))
                    return;
            }
        }
    }
}

bool bot_minion_ai::CureTarget(Unit* target, uint32 cureSpell, const uint32 diff)
{
    return CanCureTarget(target, cureSpell, diff) ? doCast(target, cureSpell) : false;
}
// determines if unit has something to cure
bool bot_minion_ai::CanCureTarget(Unit* target, uint32 cureSpell, const uint32 diff) const
{
    if (!cureSpell || GC_Timer > diff) return false;
    if (!target || target->isDead()) return false;
    if (me->getLevel() < 10 || target->getLevel() < 10) return false;
    if (me->IsMounted()) return false;
    if (IsCasting() || Feasting()) return false;
    if (me->GetDistance(target) > 30) return false;
    if (!IsInBotParty(target)) return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(cureSpell);
    if (!info) return false;

    uint32 dispelMask = 0;
    for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
        if (info->Effects[i].Effect == SPELL_EFFECT_DISPEL)
            dispelMask |= SpellInfo::GetDispelMask(DispelType(info->Effects[i].MiscValue));

    if (dispelMask == 0)
        return false;

    DispelChargesList dispel_list;
    target->GetDispellableAuraList(me, dispelMask, dispel_list);
    if (dispel_list.empty())
        return false;
    return true;
}

bool bot_ai::HasAuraName(Unit* unit, uint32 spellId, uint64 casterGuid, bool exclude) const
{
    if (!spellId) return false;
    SpellInfo const* pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!pSpellInfo) return false;

    uint8 loc = master->GetSession()->GetSessionDbcLocale();
    const std::string name = pSpellInfo->SpellName[loc];
    if (name.length() == 0) return false;

    return HasAuraName(unit, name, casterGuid, exclude);
}

bool bot_ai::HasAuraName(Unit* unit, const std::string spell, uint64 casterGuid, bool exclude) const
{
    if (spell.length() == 0) return false;

    uint8 loc = master->GetSession()->GetSessionDbcLocale();
    if (!unit || unit->isDead()) return false;

    Unit::AuraMap const &vAuras = unit->GetOwnedAuras();
    for (Unit::AuraMap::const_iterator itr = vAuras.begin(); itr != vAuras.end(); ++itr)
    {
        SpellInfo const* spellInfo = itr->second->GetSpellInfo();
        const std::string name = spellInfo->SpellName[loc];
        if (spell == name)
            if (casterGuid == 0 || (casterGuid != 0 && exclude == (casterGuid != itr->second->GetCasterGUID())))
                return true;
    }
    return false;
}
//LIST AURAS
// Debug: Returns bot's info to called player
// Can be used on your own or your playerbots' npcbot
void bot_ai::listAuras(Player* player, Unit* unit) const
{
    if (!IsInBotParty(player)) return;
    if (!IsInBotParty(unit)) return;
    ChatHandler ch(player->GetSession());
    std::ostringstream botstring;
    if (unit->GetTypeId() == TYPEID_PLAYER)
        botstring << "player";
    else if (unit->GetTypeId() == TYPEID_UNIT)
    {
        if (unit->ToCreature()->GetIAmABot())
        {
            botstring << "minion bot, master: ";
            std::string const& ownername = unit->ToCreature()->GetBotOwner()->GetName();
            botstring << ownername;
        }
        else if (unit->ToCreature()->GetIAmABotsPet())
        {
            Player* owner = unit->ToCreature()->GetBotOwner();
            Creature* creowner = unit->ToCreature()->GetBotPetAI()->GetCreatureOwner();
            std::string const& ownername = owner ? owner->GetName() : "none";
            std::string const& creownername = creowner ? creowner->GetName() : "none";
            botstring << "pet bot, master: ";
            botstring << ownername;
            botstring << ", creature owner: ";
            botstring << creownername;
            if (creowner)
                botstring << " (" << creowner->GetGUIDLow() << ')';
        }
    }
    ch.PSendSysMessage("ListAuras for %s, %s", unit->GetName().c_str(), botstring.str().c_str());
    uint8 locale = player->GetSession()->GetSessionDbcLocale();
    Unit::AuraMap const &vAuras = unit->GetOwnedAuras();
    for (Unit::AuraMap::const_iterator itr = vAuras.begin(); itr != vAuras.end(); ++itr)
    {
        SpellInfo const* spellInfo = itr->second->GetSpellInfo();
        if (!spellInfo)
            continue;
        uint32 id = spellInfo->Id;
        SpellInfo const* learnSpellInfo = sSpellMgr->GetSpellInfo(spellInfo->Effects[0].TriggerSpell);
        const std::string name = spellInfo->SpellName[locale];
        std::ostringstream spellmsg;
        spellmsg << id << " - |cffffffff|Hspell:" << id << "|h[" << name;
        spellmsg << ' ' << localeNames[locale] << "]|h|r";
        uint32 talentcost = GetTalentSpellCost(id);
        uint32 rank = 0;
        if (talentcost > 0 && spellInfo->GetNextRankSpell())
            rank = talentcost;
        else if (learnSpellInfo && learnSpellInfo->GetNextRankSpell())
            rank = spellInfo->GetRank();
        if (rank > 0)
            spellmsg << " Rank " << rank;
        if (talentcost > 0)
            spellmsg << " [talent]";
        if (spellInfo->IsPassive())
            spellmsg << " [passive]";
        if (unit->GetTypeId() == TYPEID_PLAYER && unit->ToPlayer()->HasSpell(id))
            spellmsg << " [known]";

        ch.PSendSysMessage(spellmsg.str().c_str());
    }
    for (uint8 i = STAT_STRENGTH; i != MAX_STATS; ++i)
    {
        std::string mystat;
        switch (i)
        {
            case STAT_STRENGTH: mystat = "str"; break;
            case STAT_AGILITY: mystat = "agi"; break;
            case STAT_STAMINA: mystat = "sta"; break;
            case STAT_INTELLECT: mystat = "int"; break;
            case STAT_SPIRIT: mystat = "spi"; break;
            default: mystat = "unk stat"; break;
        }
        ch.PSendSysMessage("%s: %f", mystat.c_str(), unit->GetTotalStatValue(Stats(i)));
    }
    ch.PSendSysMessage("Melee AP: %f", unit->GetTotalAttackPowerValue(BASE_ATTACK));
    ch.PSendSysMessage("Ranged AP: %f", unit->GetTotalAttackPowerValue(RANGED_ATTACK));
    ch.PSendSysMessage("armor: %u", unit->GetArmor());
    ch.PSendSysMessage("crit: %f pct", unit->GetUnitCriticalChance(BASE_ATTACK, me));
    ch.PSendSysMessage("dodge: %f pct", unit->GetUnitDodgeChance());
    ch.PSendSysMessage("parry: %f pct", unit->GetUnitParryChance());
    ch.PSendSysMessage("Damage taken melee: %f", unit->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_SCHOOL_MASK_NORMAL));
    ch.PSendSysMessage("Damage taken spell: %f", unit->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_SCHOOL_MASK_MAGIC));
    ch.PSendSysMessage("Damage range mainhand: min: %f, max: %f", unit->GetFloatValue(UNIT_FIELD_MINDAMAGE), unit->GetFloatValue(UNIT_FIELD_MAXDAMAGE));
    ch.PSendSysMessage("Damage range offhand: min: %f, max: %f", unit->GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE), unit->GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
    ch.PSendSysMessage("Damage range ranged: min: %f, max: %f", unit->GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), unit->GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
    ch.PSendSysMessage("Damage mult mainhand: %f", unit->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_PCT)*unit->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT));
    ch.PSendSysMessage("Damage mult offhand: %f", unit->GetModifierValue(UNIT_MOD_DAMAGE_OFFHAND, BASE_PCT)*unit->GetModifierValue(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT));
    ch.PSendSysMessage("Damage mult ranged: %f", unit->GetModifierValue(UNIT_MOD_DAMAGE_RANGED, BASE_PCT)*unit->GetModifierValue(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT));
    ch.PSendSysMessage("Attack time mainhand: %f", float(unit->GetAttackTime(BASE_ATTACK))/1000.f);
    ch.PSendSysMessage("Attack time offhand: %f", float(unit->GetAttackTime(OFF_ATTACK))/1000.f);
    ch.PSendSysMessage("Attack time ranged: %f", float(unit->GetAttackTime(RANGED_ATTACK))/1000.f);
    if (unit == me)
        ch.PSendSysMessage("melee damage mult: %f", dmgmult_melee);
    ch.PSendSysMessage("base hp: %u", unit->GetCreateHealth());
    ch.PSendSysMessage("total hp: %u", unit->GetMaxHealth());
    ch.PSendSysMessage("base mana: %u", unit->GetCreateMana());
    ch.PSendSysMessage("total mana: %u", unit->GetMaxPower(POWER_MANA));
    //DEBUG1
    //ch.PSendSysMessage("STATS: ");
    //ch.PSendSysMessage("Health");
    //ch.PSendSysMessage("base value: %f", unit->GetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE));
    //ch.PSendSysMessage("base pct: %f", unit->GetModifierValue(UNIT_MOD_HEALTH, BASE_PCT));
    //ch.PSendSysMessage("total value: %f", unit->GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE));
    //ch.PSendSysMessage("total pct: %f", unit->GetModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT));
    //ch.PSendSysMessage("Mana");
    //ch.PSendSysMessage("base value: %f", unit->GetModifierValue(UNIT_MOD_MANA, BASE_VALUE));
    //ch.PSendSysMessage("base pct: %f", unit->GetModifierValue(UNIT_MOD_MANA, BASE_PCT));
    //ch.PSendSysMessage("total value: %f", unit->GetModifierValue(UNIT_MOD_MANA, TOTAL_VALUE));
    //ch.PSendSysMessage("total pct: %f", unit->GetModifierValue(UNIT_MOD_MANA, TOTAL_PCT));
    //ch.PSendSysMessage("Stamina");
    //ch.PSendSysMessage("base value: %f", unit->GetModifierValue(UNIT_MOD_STAT_STAMINA, BASE_VALUE));
    //ch.PSendSysMessage("base pct: %f", unit->GetModifierValue(UNIT_MOD_STAT_STAMINA, BASE_PCT));
    //ch.PSendSysMessage("total value: %f", unit->GetModifierValue(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE));
    //ch.PSendSysMessage("total pct: %f", unit->GetModifierValue(UNIT_MOD_STAT_STAMINA, TOTAL_PCT));
    //ch.PSendSysMessage("Intellect");
    //ch.PSendSysMessage("base value: %f", unit->GetModifierValue(UNIT_MOD_STAT_INTELLECT, BASE_VALUE));
    //ch.PSendSysMessage("base pct: %f", unit->GetModifierValue(UNIT_MOD_STAT_INTELLECT, BASE_PCT));
    //ch.PSendSysMessage("total value: %f", unit->GetModifierValue(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE));
    //ch.PSendSysMessage("total pct: %f", unit->GetModifierValue(UNIT_MOD_STAT_INTELLECT, TOTAL_PCT));
    //ch.PSendSysMessage("Spirit");
    //ch.PSendSysMessage("base value: %f", unit->GetModifierValue(UNIT_MOD_STAT_SPIRIT, BASE_VALUE));
    //ch.PSendSysMessage("base pct: %f", unit->GetModifierValue(UNIT_MOD_STAT_SPIRIT, BASE_PCT));
    //ch.PSendSysMessage("total value: %f", unit->GetModifierValue(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE));
    //ch.PSendSysMessage("total pct: %f", unit->GetModifierValue(UNIT_MOD_STAT_SPIRIT, TOTAL_PCT));
    //END DEBUG1
    if (unit == me)
    {
        ch.PSendSysMessage("spellpower: %u", m_spellpower - m_spellpower % 50);
        ch.PSendSysMessage("spell damage mult: %f", dmgmult_spell);
        ch.PSendSysMessage("mana regen: %f", regen_mp5 - (int32(regen_mp5) % 45));
        ch.PSendSysMessage("haste: %u *10 pct", haste);
        for (uint8 i = SPELL_SCHOOL_HOLY; i != MAX_SPELL_SCHOOL; ++i)
        {
            const char* resist = NULL;
            switch (i)
            {
                case 1: resist = "holy";   break;
                case 2: resist = "fire";   break;
                case 3: resist = "nature"; break;
                case 4: resist = "frost";  break;
                case 5: resist = "shadow"; break;
                case 6: resist = "arcane"; break;
            }
            ch.PSendSysMessage("Resistance %s: %u", resist, me->GetResistance(SpellSchools(i)));
        }
        ch.PSendSysMessage("BotCommandState: %s", m_botCommandState == COMMAND_FOLLOW ? "Follow" : m_botCommandState == COMMAND_ATTACK ? "Attack" : m_botCommandState == COMMAND_STAY ? "Stay" : m_botCommandState == COMMAND_ABANDON ? "Reset" : "none");
        ch.PSendSysMessage("Follow distance: %u", master->GetBotFollowDist());
        //ch.PSendSysMessage("healTargetIconFlags: %u", healTargetIconFlags);
        if (tank != NULL && tank->IsInWorld())
        {
            if (tank == me)
                ch.PSendSysMessage("Is a MainTank!");
            else
                ch.PSendSysMessage("Maintank is %s", tank->GetName().c_str());
        }
        //debug
        //if (IsPetAI()) GetPetAI()->ListSpells(&ch);
    }
}
//SETSTATS
// Health, Armor, Powers, Combat Ratings, and global update setup
void bot_minion_ai::setStats(uint8 myclass, uint8 myrace, uint8 mylevel, bool force)
{
    if (myrace == 0 || myclass == 0) return;
    if (myclass != BEAR && myclass != CAT && master->isDead()) return;
    /*sLog->outBasic("setStats(): Updating bot %s, class: %u, race: %u, level %u, master: %s",
        me->GetName().c_str(), myclass, myrace, mylevel, master->GetName().c_str());*/

    mylevel = std::min<uint8>(mylevel, 80);

    //LEVEL
    if (me->getLevel() != mylevel)
    {
        me->SetLevel(mylevel);
        force = true; //restore powers on lvl update
    }
    if (force)
        InitSpells();

    //FACTION
    //restore charmer (prevent  <(someone else)'s Minion> after master has been charmed)
    //if (!master->GetCharmer())
    //{
    //    if (me->GetCharmerGUID() != master->GetGUID())
    //        me->SetCharmerGUID(master->GetGUID());  //master
    //    Player* owner = master->GetPlayerbotAI() ? master->GetSession()->m_master : master;
    //    if (!owner->GetCharmer() && me->getFaction() != owner->getFaction())
    //        me->setFaction(owner->getFaction()); //owner of all bots
    //}

    //PHASE
    if (!me->InSamePhase(master))
        me->SetPhaseMask(master->GetPhaseMask(), true);
    //INIT STATS
    //partially receive master's stats and get base class stats, we'll need all this later
    uint8 tempclass = myclass == BEAR || myclass == CAT ? CLASS_DRUID : myclass;
    sObjectMgr->GetPlayerClassLevelInfo(tempclass, mylevel, &classinfo);
    const CreatureBaseStats* const classstats = sObjectMgr->GetCreatureBaseStats(mylevel, me->getClass());//use creature class
    float value;
    if (force)
        for (uint8 i = STAT_STAMINA; i < MAX_STATS; i++)
            me->SetCreateStat(Stats(i), master->GetCreateStat(Stats(i)));

    //MAXSTAT
    for (uint8 i = 0; i < MAX_STATS; ++i)
    {
        value = master->GetTotalStatValue(Stats(i));
        if (i == 0 || value > stat)
            stat = value;//Get Hightest stat (on first cycle just set base value)
    }
    stat = std::max(stat - 18.f, 0.f);

    //INIT CLASS MODIFIERS
    switch (myclass)
    {
        case CLASS_WARRIOR:      ap_mod = 1.3f;  spp_mod = 0.0f; armor_mod = 1.4f;  crit_mod = 1.0f; haste_mod = 0.75f; dodge_mod = 0.75f; parry_mod = 1.75f; break;
        case CLASS_DEATH_KNIGHT: ap_mod = 1.2f;  spp_mod = 1.0f; armor_mod = 1.15f; crit_mod = 0.9f; haste_mod = 0.65f; dodge_mod = 0.8f;  parry_mod = 2.0f;  break;//NYI
        case CLASS_PALADIN:      ap_mod = 1.0f;  spp_mod = 0.8f; armor_mod = 1.2f;  crit_mod = 0.8f; haste_mod = 0.85f; dodge_mod = 0.7f;  parry_mod = 1.5f;  break;
        case CLASS_ROGUE:        ap_mod = 1.5f;  spp_mod = 0.0f; armor_mod = 0.7f;  crit_mod = 1.5f; haste_mod = 1.35f; dodge_mod = 1.5f;  parry_mod = 0.8f;  break;//NYI
        case CLASS_HUNTER:       ap_mod = 1.15f; spp_mod = 0.0f; armor_mod = 0.85f; crit_mod = 1.2f; haste_mod = 1.25f; dodge_mod = 1.2f;  parry_mod = 1.2f;  break;//NYI
        case CLASS_SHAMAN:       ap_mod = 0.9f;  spp_mod = 1.0f; armor_mod = 0.9f;  crit_mod = 1.2f; haste_mod = 1.65f; dodge_mod = 0.8f;  parry_mod = 0.5f;  break;//NYI
        case CLASS_DRUID:        ap_mod = 0.0f;  spp_mod = 1.3f; armor_mod = 0.7f;  crit_mod = 0.7f; haste_mod = 1.95f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;
        case CLASS_MAGE:         ap_mod = 0.0f;  spp_mod = 0.8f; armor_mod = 0.5f;  crit_mod = 0.7f; haste_mod = 1.75f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;          
        case CLASS_PRIEST:       ap_mod = 0.0f;  spp_mod = 1.2f; armor_mod = 0.5f;  crit_mod = 0.7f; haste_mod = 1.75f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;
        case CLASS_WARLOCK:      ap_mod = 0.0f;  spp_mod = 1.0f; armor_mod = 0.5f;  crit_mod = 0.7f; haste_mod = 1.75f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;
        case BEAR:               ap_mod = 2.0f;  spp_mod = 1.3f; armor_mod = 2.25f; crit_mod = 1.0f; haste_mod = 0.75f; dodge_mod = 2.5f;  parry_mod = 0.0f;  break;
        case CAT:                ap_mod = 1.5f;  spp_mod = 1.3f; armor_mod = 1.1f;  crit_mod = 1.5f; haste_mod = 2.25f; dodge_mod = 1.35f; parry_mod = 0.0f;  break;
        default:                 ap_mod = 0.0f;  spp_mod = 0.0f; armor_mod = 0.0f;  crit_mod = 0.0f; haste_mod = 0.00f; dodge_mod = 0.0f;  parry_mod = 0.0f;  break;
    }
    if (spp_mod != 0.f && mylevel > 39)
        spp_mod *= (float(mylevel - 39))/41.f;// gain spell power slowly

    //DAMAGE
    _OnMeleeDamageUpdate(myclass);
    
    //ARMOR
    //sLog->outBasic("Unpdating %s's ARMOR: ", me->GetName().c_str());
    //sLog->outBasic("armor mod: %f", armor_mod);
    armor_mod *= (master->GetModifierValue(UNIT_MOD_ARMOR, BASE_PCT) + master->GetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT))/2.f;
    //sLog->outBasic("armor mod * master's modifier: %f", armor_mod);
    value = float(classstats->BaseArmor);
    //sLog->outBasic("base armor: %f", value);
    value += float(master->GetArmor())/5.f;
    //sLog->outBasic("base armor + 1/5 of master's armor: %f", value);
    value *= armor_mod;
    //sLog->outBasic("multiplied by armor mod (total base armor): %f", value);
    me->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, value);
    me->UpdateArmor();//buffs will be took in consideration here

    //RESISTANCES
    //sLog->outBasic("Unpdating %s's RESISTANCES: ", me->GetName().c_str());
    for (uint8 i = SPELL_SCHOOL_HOLY; i != MAX_SPELL_SCHOOL; ++i)
    {
        value = float(master->GetResistance(SpellSchools(i)));
        //sLog->outBasic("master's resistance %u: %f, setting %f (triple) to bot", uint32(UNIT_MOD_RESISTANCE_START + i), value, value*3);
        me->SetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_VALUE, value*2.5f + float(mylevel*2));
        //me->SetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_PCT, 1.f);
        me->UpdateResistances(i);
    }
    //DAMAGE TAKEN
    float directReduction = master->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_SCHOOL_MASK_NORMAL);
    float magicReduction = master->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_SCHOOL_MASK_MAGIC);
    value = (directReduction + magicReduction)/2.f;// average
    if (value > 1.f)
        value -= 1.f;
    else
        value = 1.f - value;//get reduction even if master's is 1.0+
    value = std::min(0.42f, value);
    value/= 0.01f; //here we get percent like 0.42/0.01 = 42% (value * 100.f)
    if (mylevel > 77)
        value += float(mylevel - 77)*6.f;// + 3 stacks for high level
    RefreshAura(DMG_TAKEN, int8(value/6.f));//so max aura count = 10

    //HEALTH
    _OnHealthUpdate(myclass, mylevel);

    //HASTE
    value = 0.f;
    for (uint8 i = CR_HASTE_MELEE; i != CR_HASTE_SPELL + 1; ++i)
        if (float rating = master->GetRatingBonusValue(CombatRating(i)))
            if (rating > value)//master got some haste
                value = rating;//get hightest pct
    for (uint8 i = EQUIPMENT_SLOT_BACK; i < EQUIPMENT_SLOT_END; ++i)
        if (Item* item = master->GetItemByPos(0, i))//inventory weapons
            if (item->GetTemplate()->ItemLevel >= 277)//bears ICC 25H LK items or Wrathful items
                value += 10.f;//only weapons so we can add 1 to 3 stacks (rogue, warr, sham...)
    value *= haste_mod;
    if (isMeleeClass(myclass))
        value *= 0.67f;//nerf melee haste by 1/3
    value = value/10.f + float(mylevel/39);//get bonus at 78
    if (myclass == CAT)//give cat lots of haste
        value += float(mylevel/16);//or 20 (+ 4-5 stacks);
    RefreshAura(HASTE,  uint8(value));//spell haste
    RefreshAura(HASTE2, uint8(value + 1*(myclass == CLASS_ROGUE)));//melee haste
    haste = uint8(value);//for show only

    //HIT
    int32 melee_hit = master->GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE) + master->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
    int32 spell_hit = master->GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE) + master->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT, SPELL_SCHOOL_MASK_SPELL);
    value = float(melee_hit > spell_hit ? melee_hit : spell_hit)*1.5f;//hightest, buff hit chance for bots
    hit = value/3.f;
    RefreshAura(PRECISION,  int8(hit) + mylevel/39);//melee
    RefreshAura(PRECISION2, int8(hit) + mylevel/39);//spell

    //CRIT
    //chose melee or ranged cuz crit rating increases melee/spell, and hunter benefits from agility
    value = master->GetUnitCriticalChance((master->getClass() == CLASS_HUNTER ? RANGED_ATTACK : BASE_ATTACK), me);
    value = value > 5.f ? value - 5.f : 0.f;//remove base chance if can
    value *= crit_mod;
    RefreshAura(CRITS, int8(value/5.f) + mylevel/39);
    if (myclass == CLASS_PRIEST)
        RefreshAura(HOLYCRIT, int8(value/7.f));//add holy crit to healers

    //PARRY
    value = master->GetFloatValue(PLAYER_PARRY_PERCENTAGE);
    value = value > 5.f ? value - 5.f : 0.f;//remove base chance if possible
    value *= parry_mod;
    if (master->GetBotTankGuid() == me->GetGUID() && myclass != CAT && myclass != BEAR)//feral cannot parry so let it be base 5%
        value += 10.f;
    if (value > 55.f)
        value = 55.f;
    float parryAndDodge = value;//set temp value, this is needed to keep total avoidance within 65%
    RefreshAura(PARRY, int8(value/5.f));

    //DODGE
    value = master->GetUnitDodgeChance();
    value = value > 5.f ? value - 5.f : 0.f;//remove base chance if possible
    value *= dodge_mod;
    if (master->GetBotTankGuid() == me->GetGUID())
        value += 10.f;
    if (value > 55.f)
        value = 55.f;
    if (parryAndDodge + value > 55.f)
        value = 55.f - parryAndDodge;//do not allow avoidance to be more than 65% (base 5+5)
    if (myclass == CLASS_ROGUE)
        value += 6.f;
    RefreshAura(DODGE, int8(value/5.f));

    //MANA
    _OnManaUpdate(myclass, mylevel);

    //MANA REGEN
    if (mylevel >= 40 && me->getPowerType() == POWER_MANA)
    {
        regen_mp5 = master->GetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER);
        //regen_mp5 = (master->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + sqrt(master->GetStat(STAT_INTELLECT)) * master->OCTRegenMPPerSpirit()) / 5.f;
        //Unit::AuraEffectList const& regenAura = master->GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
        //for (Unit::AuraEffectList::const_iterator i = regenAura.begin(); i != regenAura.end(); ++i)
        //    regen_mp5 += master->GetStat(Stats((*i)->GetMiscValue())) * (*i)->GetAmount() / 500.f;
        //regen_mp5 *= 0.8f;//custom modifier
        float regen_mp5_a = stat * 0.2f;
        //regen_mp5 += master->GetTotalStatValue(STAT_SPIRIT) * 0.1f;
        regen_mp5 = regen_mp5 > regen_mp5_a ? regen_mp5 : regen_mp5_a;
        if (regen_mp5 >= 45.f)
        {
            me->RemoveAurasDueToSpell(MANAREGEN100);
            me->RemoveAurasDueToSpell(MANAREGEN45);
            if      (regen_mp5 > 200.f)   RefreshAura(MANAREGEN100,int8(regen_mp5/100.f) + mylevel/20);
            else/*if (regen_mp5 > 150.f)*/RefreshAura(MANAREGEN45, int8(regen_mp5/45.f)  + mylevel/20);
        }
    }

    //SPELL POWER
    if (mylevel >= 40 && spp_mod != 0.f)
    {
        //sLog->outBasic("Updating spellpower for %s:", me->GetName().c_str());
        //sLog->outBasic("spp_mod: %f", spp_mod);
        for (uint8 i = SPELL_SCHOOL_HOLY; i != MAX_SPELL_SCHOOL; ++i)
        {
            int32 power = master->SpellBaseDamageBonusDone(SpellSchoolMask(1 << i));
            if (power > sppower || i == SPELL_SCHOOL_HOLY)
                sppower = power;
        }
        //sppower = master->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_MAGIC);//"Spell Power" stat
        //sLog->outBasic("Master's spell power: %i", sppower);
        atpower = master->GetTotalAttackPowerValue(master->getClass() == CLASS_HUNTER ? RANGED_ATTACK : BASE_ATTACK);
        atpower *= 0.67f;
        //sLog->outBasic("Master's 2/3 of attack power: %f", atpower);
        m_spellpower = sppower > atpower ? sppower : atpower;
        //sLog->outBasic("Chosen stat value: %i", m_spellpower);
        m_spellpower = int32(float(m_spellpower)*spp_mod);
        //sLog->outBasic("spellpower * mod: %i", m_spellpower);
        if (myclass == CLASS_MAGE)
            RefreshAura(FIREDAM_86, m_spellpower/4/86 + (mylevel >= 78)*2); //(86,172,258,344,430,516,602,688...) // fire spp, do not touch this
        me->RemoveAurasDueToSpell(SPELL_BONUS_250);
        me->RemoveAurasDueToSpell(SPELL_BONUS_150);
        me->RemoveAurasDueToSpell(SPELL_BONUS_50);
        if      (mylevel < 60) RefreshAura(SPELL_BONUS_50,  m_spellpower/50);
        else if (mylevel < 80) RefreshAura(SPELL_BONUS_150, m_spellpower/150 + 1);
        else                   RefreshAura(SPELL_BONUS_250, m_spellpower/250 + 2);
    }

    if (force)
    {
        me->SetFullHealth();
        me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA));
    }

    //SetStats for pet
    if (Creature* pet = me->GetBotsPet())
        if (bot_pet_ai* petai = pet->GetBotPetAI())
            petai->setStats(mylevel, bot_pet_ai::GetPetType(pet), force);
}
void bot_pet_ai::setStats(uint8 mylevel, uint8 petType, bool force)
{
    if (petType == PET_TYPE_NONE || petType >= MAX_PET_TYPES) return;
    //sLog->outError(LOG_FILTER_PLAYER, "setStats(): Updating pet bot %s, type: %u, level %u, owner: %s, master: %s", me->GetName().c_str(), petType, mylevel, m_creatureOwner->GetName().c_str(), master->GetName().c_str());

    //LEVEL
    if (me->getLevel() != mylevel)
    {
        me->SetLevel(mylevel);
        force = true; //restore powers on lvl update
    }
    if (force)
        InitSpells();

    //FACTION
    //restore charmer (prevent  <(someone else)'s Minion> after master has been charmed)
    //if (!master->GetCharmer())
    //{
    //    if (me->GetCharmerGUID() != master->GetGUID())
    //        me->SetCharmerGUID(master->GetGUID());  //master
    //    Player* owner = master->GetPlayerbotAI() ? master->GetSession()->m_master : master;
    //    if (!owner->GetCharmer() && me->getFaction() != owner->getFaction())
    //        me->setFaction(owner->getFaction()); //owner of all bots
    //}

    //PHASE
    if (!me->InSamePhase(master))
        me->SetPhaseMask(master->GetPhaseMask(), true);

    ////INIT STATS
    uint8 botclass = m_creatureOwner->GetBotClass();
    if (botclass == BEAR || botclass == CAT)
        botclass = CLASS_DRUID;
    //sObjectMgr->GetPlayerClassLevelInfo(botclass, m_creatureOwner->getLevel(), &classinfo);
    //const CreatureBaseStats* const classstats = sObjectMgr->GetCreatureBaseStats(mylevel, me->GetBotClass());//use creature class
    //if (force)
    //    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; i++)
    //        me->SetCreateStat(Stats(i), master->GetCreateStat(Stats(i))*0.5f);

    //MAXSTAT
    float value;
    for (uint8 i = 0; i < MAX_STATS; ++i)
    {
        value = master->GetTotalStatValue(Stats(i));
        if (i == 0 || value > stat)
            stat = value;//Get Hightest stat (on first cycle just set base value)
    }
    stat = std::max(stat - 18.f, 0.f);//remove base

    //INIT CLASS MODIFIERS
    //STAT -- 'mod' -- used stat values to apply
    //WARLOCK
    //Stamina x0.3  -- health
    //Armor   x0.35 -- armor
    //Int     x0.3  -- crit/mana
    //Spd     x0.15 -- spd (if has mana)
    //AP      x0.57 -- attack power (if melee pet)
    //Resist  x0.4  -- resistances
    //MAGE
    //
    //SHAMAN
    //
    //HUNTER
    //Other   x1.0  -- use as default
    switch (petType)
    {
        case PET_TYPE_VOIDWALKER:       ap_mod = 0.57f; spp_mod = 0.15f; crit_mod = 1.0f; break;
        //case PET_TYPE_FELHUNTER:        ap_mod = 0.57f; spp_mod = 0.15f; crit_mod = 1.0f; break;//NYI
        //case PET_TYPE_FELGUARD:         ap_mod = 0.57f; spp_mod = 0.15f; crit_mod = 1.0f; break;//NYI
        //case PET_TYPE_SUCCUBUS:         ap_mod = 0.57f; spp_mod = 0.15f; crit_mod = 1.0f; break;//NYI
        //case PET_TYPE_IMP:              ap_mod = 0.f;   spp_mod = 0.15f; crit_mod = 1.0f; break;//NYI

        //case PET_TYPE_WATER_ELEMENTAL:  ap_mod = 0.0f;  spp_mod = 0.0f; crit_mod = 0.0f; break;//NYI

        //case PET_TYPE_FIRE_ELEMENTAL:   ap_mod = 0.0f;  spp_mod = 0.0f; crit_mod = 0.0f; break;//NYI
        //case PET_TYPE_EARTH_ELEMENTAL:  ap_mod = 0.0f;  spp_mod = 0.0f; crit_mod = 0.0f; break;//NYI

        //case PET_TYPE_VULTURE:          ap_mod = 0.9f;  spp_mod = 1.0f; crit_mod = 1.2f; break;//NYI
        default:                        ap_mod = 0.0f;  spp_mod = 0.0f; crit_mod = 0.0f; break;
    }
    //case CLASS_WARRIOR:      ap_mod = 1.3f;  spp_mod = 0.0f; armor_mod = 1.4f;  crit_mod = 1.0f; haste_mod = 0.75f; dodge_mod = 0.75f; parry_mod = 1.75f; break;
    //case CLASS_DEATH_KNIGHT: ap_mod = 1.2f;  spp_mod = 1.0f; armor_mod = 1.15f; crit_mod = 0.9f; haste_mod = 0.65f; dodge_mod = 0.8f;  parry_mod = 2.0f;  break;//NYI
    //case CLASS_PALADIN:      ap_mod = 1.0f;  spp_mod = 0.8f; armor_mod = 1.2f;  crit_mod = 0.8f; haste_mod = 0.85f; dodge_mod = 0.7f;  parry_mod = 1.5f;  break;
    //case CLASS_ROGUE:        ap_mod = 1.5f;  spp_mod = 0.0f; armor_mod = 0.7f;  crit_mod = 1.5f; haste_mod = 1.35f; dodge_mod = 1.5f;  parry_mod = 0.8f;  break;//NYI
    //case CLASS_HUNTER:       ap_mod = 1.15f; spp_mod = 0.0f; armor_mod = 0.85f; crit_mod = 1.2f; haste_mod = 1.25f; dodge_mod = 1.2f;  parry_mod = 1.2f;  break;//NYI
    //case CLASS_SHAMAN:       ap_mod = 0.9f;  spp_mod = 1.0f; armor_mod = 0.9f;  crit_mod = 1.2f; haste_mod = 1.65f; dodge_mod = 0.8f;  parry_mod = 0.5f;  break;//NYI
    //case CLASS_DRUID:        ap_mod = 0.0f;  spp_mod = 1.3f; armor_mod = 0.7f;  crit_mod = 0.7f; haste_mod = 1.95f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;
    //case CLASS_MAGE:         ap_mod = 0.0f;  spp_mod = 0.8f; armor_mod = 0.5f;  crit_mod = 0.7f; haste_mod = 1.75f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;          
    //case CLASS_PRIEST:       ap_mod = 0.0f;  spp_mod = 1.2f; armor_mod = 0.5f;  crit_mod = 0.7f; haste_mod = 1.75f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;
    //case CLASS_WARLOCK:      ap_mod = 0.0f;  spp_mod = 1.0f; armor_mod = 0.5f;  crit_mod = 0.7f; haste_mod = 1.75f; dodge_mod = 0.5f;  parry_mod = 0.0f;  break;
    //case BEAR:               ap_mod = 2.0f;  spp_mod = 1.3f; armor_mod = 2.25f; crit_mod = 1.0f; haste_mod = 0.75f; dodge_mod = 2.5f;  parry_mod = 0.0f;  break;
    //case CAT:                ap_mod = 1.5f;  spp_mod = 1.3f; armor_mod = 1.1f;  crit_mod = 1.5f; haste_mod = 2.25f; dodge_mod = 1.35f; parry_mod = 0.0f;  break;

    if (spp_mod != 0.f && mylevel > 39)
        spp_mod *= (float(mylevel - 39))/41.f;// gain spell power slowly

    //DAMAGE
    if (ap_mod > 0.f)//do not bother casters
    {
        switch (m_creatureOwner->GetBotClass())
        {
            case CLASS_WARLOCK:
                value = float(m_creatureOwner->GetBotAI()->GetSpellPower());
                break;
            default://some weird class or NYI
                value = 0.f;
                break;
        }
        //Calculate ap
        //set base strength
        me->SetModifierValue(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, me->GetCreateStat(STAT_STRENGTH) - 9.f);
        //calc attack power (strength and minion's spd)
        atpower = me->GetTotalAuraModValue(UNIT_MOD_STAT_STRENGTH)*2.f + value*ap_mod;
        //set value
        me->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, atpower);
        me->UpdateAttackPowerAndDamage();
    }
    
    //ARMOR
    value = float(basearmor);
    //get minion's armor and give 35% to pet (just as for real pets)
    value += m_creatureOwner->GetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE)*0.35f;
    me->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, value);
    me->UpdateArmor();//buffs will be took in consideration here

    //RESISTANCES
    //based on minion's resistances gain x0.4
    for (uint8 i = SPELL_SCHOOL_HOLY; i != MAX_SPELL_SCHOOL; ++i)
    {
        value = float(master->GetResistance(SpellSchools(i)));
        me->SetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_VALUE, 0.4f*(value*2.5f + float(mylevel*2)));
        me->UpdateResistances(i);
    }

    //DAMAGE TAKEN
    //just get minion's reduction and apply to pet
    value = m_creatureOwner->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_SCHOOL_MASK_NORMAL);
    if (value > 1.f)
        value -= 1.f;
    else
        value = 1.f - value;//get reduction even if owner's is 1.0+
    value = std::min(0.42f, value);
    value/= 0.01f; //here we get percent like 0.42/0.01 = 42% (value * 100.f)
    RefreshAura(DMG_TAKEN, int8(value/6.f));//so max aura count = 10

    //HEALTH
    _OnHealthUpdate(petType, mylevel);

//////RATINGS//////
    //ok now, pet receives 100% of its master's ratings

    //HASTE
    haste = m_creatureOwner->GetBotAI()->GetHaste();
    RefreshAura(HASTE,  haste);//spell haste
    RefreshAura(HASTE2, haste);//melee haste

    //HIT
    hit = m_creatureOwner->GetBotAI()->GetHitRating();
    RefreshAura(PRECISION,  int8(hit) + mylevel/39);//melee
    RefreshAura(PRECISION2, int8(hit) + mylevel/39);//spell

    //CRIT
    //chose melee or ranged cuz crit rating increases melee/spell, and hunter benefits from agility
    value = master->GetUnitCriticalChance((master->getClass() == CLASS_HUNTER ? RANGED_ATTACK : BASE_ATTACK), me);
    if (crit_mod != 1.0f)
        value *= crit_mod;
    RefreshAura(CRITS, int8(value/5.f) + mylevel/39);

    //PARRY
    value = master->GetFloatValue(PLAYER_PARRY_PERCENTAGE);
    if (master->GetBotTankGuid() == me->GetGUID())//feral cannot parry so let it be base 5%
        value += 10.f;
    if (value > 65.f)
        value = 65.f;
    float parryAndDodge = value;//set temp value, this is needed to keep total avoidance within 75%
    RefreshAura(PARRY, int8(value/5.f));

    //DODGE
    value = master->GetUnitDodgeChance();
    value = value > 5.f ? value - 5.f : 0.f;//remove base chance if possible
    if (master->GetBotTankGuid() == me->GetGUID())
        value += 10.f;
    if (value > 65.f)
        value = 65.f;
    if (parryAndDodge + value > 65.f)
        value = 65.f - parryAndDodge;//do not allow avoidance to be more than 75% (base 5+5)
    RefreshAura(DODGE, int8(value/5.f));

    //MANA
    _OnManaUpdate(petType, mylevel);

    //MANA REGEN
    if (mylevel >= 40 && me->getPowerType() == POWER_MANA)
    {
        //let regen rate be same as stats rate x0.3
        regen_mp5 = m_creatureOwner->GetBotAI()->GetManaRegen()*0.3f;
        if (regen_mp5 >= 45.f)
        {
            me->RemoveAurasDueToSpell(MANAREGEN100);
            me->RemoveAurasDueToSpell(MANAREGEN45);
            if      (regen_mp5 > 200.f)   RefreshAura(MANAREGEN100,int8(regen_mp5/100.f) + mylevel/20);
            else/*if (regen_mp5 > 150.f)*/RefreshAura(MANAREGEN45, int8(regen_mp5/45.f)  + mylevel/20);
        }
    }

    //SPELL POWER
    if (mylevel >= 40 && spp_mod != 0.f)
    {
        switch (m_creatureOwner->GetBotClass())
        {
            case CLASS_WARLOCK:
                value = float(m_creatureOwner->GetBotAI()->GetSpellPower());
                break;
            default://some weird class or NYI
                value = 0.f;
                break;
        }
        m_spellpower = int32(value*spp_mod);
        me->RemoveAurasDueToSpell(SPELL_BONUS_250);
        me->RemoveAurasDueToSpell(SPELL_BONUS_150);
        me->RemoveAurasDueToSpell(SPELL_BONUS_50);
        if      (mylevel < 60) RefreshAura(SPELL_BONUS_50,  m_spellpower/50);
        else if (mylevel < 80) RefreshAura(SPELL_BONUS_150, m_spellpower/150 + 1);
        else                   RefreshAura(SPELL_BONUS_250, m_spellpower/250 + 2);
    }

    if (force)
    {
        me->SetFullHealth();
        me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA));
    }
}
//Emotion-based action 
void bot_ai::ReceiveEmote(Player* player, uint32 emote)
{
    switch (emote)
    {
        case TEXT_EMOTE_BONK:
            listAuras(player, me);
            break;
        case TEXT_EMOTE_SALUTE:
            listAuras(player, player);
            break;
        case TEXT_EMOTE_STAND:
            if (!IsMinionAI())
                return;
            if (master != player)
            {
                me->HandleEmoteCommand(EMOTE_ONESHOT_RUDE);
                return;
            }
            SetBotCommandState(COMMAND_STAY);
            me->MonsterWhisper("Standing Still.", player->GetGUID());
            break;
        case TEXT_EMOTE_WAVE:
            if (!IsMinionAI())
                return;
            if (master != player)
            {
                me->HandleEmoteCommand(EMOTE_ONESHOT_RUDE);
                return;
            }
            SetBotCommandState(COMMAND_FOLLOW, true);
            me->MonsterWhisper("Following!", player->GetGUID());
            break;
        default:
            break;
    }
}

//ISINBOTPARTY
//Returns group members and your playerbots (and their npcbots too)
//For now all your puppets are in your group automatically
bool bot_ai::IsInBotParty(Unit* unit) const
{
    if (!unit) return false;
    if (unit == me || unit == master) return true;

    //cheap check
    if (Group* gr = master->GetGroup())
    {
        //group member case
        if (gr->IsMember(unit->GetGUID()))
            return true;
        //pointed target case
        for (uint8 i = 0; i != TARGETICONCOUNT; ++i)
            if (healTargetIconFlags & GroupIconsFlags[i])
                if (uint64 guid = gr->GetTargetIcons()[i])//check this one
                    if (guid == unit->GetGUID())
                        if (unit->GetReactionTo(master) >= REP_NEUTRAL && 
                            master->getVictim() != unit && 
                            unit->getVictim() != master)
                            return true;
    }

    //Player-controlled creature case
    if (Creature* cre = unit->ToCreature())
    {
        //npcbot/npcbot's pet case
        if (Player* owner = cre->GetBotOwner())
        {
            if (owner == master || owner->GetSession()->m_master == master)
                return true;
        }
        //pets, minions, guardians etc.
        else
        {
            //first case: constrolled by playerbot (possible without group)
            uint64 ownerGuid = unit->GetOwnerGUID();
            if (ownerGuid == 0 || !IS_PLAYER_GUID(ownerGuid))
                return false;
            uint32 masterId = master->GetSession()->GetAccountId();
            uint32 ownerId = sObjectMgr->GetPlayerAccountIdByGUID(ownerGuid);
            if (masterId == ownerId)
                return true;
            //second case: controlled by group member real player
            if (Group* gr = master->GetGroup())
                if (gr->IsMember(ownerGuid))
                    return true;
        }
    }

    //playerbot case
    if (Player* plr = unit->ToPlayer())
        if (plr->GetSession()->m_master == master)
            return true;

    return false;
}

//REFRESHAURA
//Applies/reapplies aura stacks
bool bot_ai::RefreshAura(uint32 spell, int8 count, Unit* target) const
{
    if (!spell)
        return false;
    if (!target)
        target = me;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell);
    if (!spellInfo)
        return false;
    //if (!spellInfo->IsPassive())
    //{
    //    sLog->outError(LOG_FILTER_PLAYER, "bot_ai::RefreshAura(): %s received spell %u (%s) which is not a passive spell!", target->GetName().c_str(), spell, spellInfo->SpellName[0]);
    //    //return false;
    //}
    if (target->HasAura(spell))
        target->RemoveAurasDueToSpell(spell);
    if (count > 0)
        for (uint8 i = 0; i < count; ++i)
            target->AddAura(spellInfo, MAX_EFFECT_MASK, target);
    return true;
}
//CHECKAURAS
//Updates bot's condition once a while
void bot_minion_ai::CheckAuras(bool force)
{
    if (checkAurasTimer > 0 && !force) return;
    if (checkAurasTimer == 0)
    {
        checkAurasTimer = 10 + master->GetNpcBotsCount()/2;
        if (m_botCommandState != COMMAND_FOLLOW && m_botCommandState != COMMAND_STAY)
        {
            opponent = me->getVictim();
            if (opponent)
            {
                switch (me->GetBotClass())
                {
                    case CLASS_MAGE: case CLASS_DRUID: case CLASS_WARLOCK: case CLASS_PRIEST:/* case CLASS_SHAMAN:*/
                        CalculateAttackPos(opponent, attackpos);
                        if (me->GetDistance(attackpos) > 8)
                            GetInPosition(true, true, opponent, &attackpos);
                        break;
                    default:
                        if (me->GetDistance(opponent) > 1.5f)
                            GetInPosition(true, false);
                        break;
                }
            }
        }
        setStats(me->GetBotClass(), me->getRace(), master->getLevel());
        if (rezz_cd > 0)
            --rezz_cd;
        if (clear_cd > 0)
            --clear_cd;
        else
        {
            FindTank();
            clear_cd = 15;
        }
        return;
    }
    else if (force)
    {
        if (!opponent)
        {
            if (master->isDead())
            {
                //If ghost move to corpse, else move to dead player
                if (master->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                {
                    if (!me->isInCombat() && !me->HasUnitState(UNIT_STATE_MOVING) && !IsCasting() && !CCed(me) && me->GetDistance(master->GetCorpse()) > 5)
                        me->GetMotionMaster()->MovePoint(master->GetCorpse()->GetMapId(), *master->GetCorpse());
                }
                else
                {
                    if (m_botCommandState != COMMAND_FOLLOW || me->GetDistance(master) > 30 - 20 * (!me->IsWithinLOSInMap(master)))
                        Follow(true);
                }
            }
            else if (m_botCommandState != COMMAND_STAY && !IsCasting())
            {
                CalculatePos(pos);
                uint8 followdist = master->GetBotFollowDist();
                if (me->GetExactDist(&pos) > (followdist > 8 ? 4 + followdist/2*(!master->isMoving()) : 8))
                    Follow(true, &pos); // check if doing nothing
            }
        }
        if (!IsCasting())
        {
            if (me->isInCombat())
            {
                if (me->GetSheath() != SHEATH_STATE_MELEE)
                    me->SetSheath(SHEATH_STATE_MELEE);
            }
            else if (me->IsStandState() && me->GetSheath() != SHEATH_STATE_UNARMED && Rand() < 50)
                me->SetSheath(SHEATH_STATE_UNARMED);
        }
        UpdateMountedState();
        UpdateStandState();
        UpdateRations();
    }
}
void bot_pet_ai::CheckAuras(bool /*force*/)
{
    if (checkAurasTimer > 0) return;
    checkAurasTimer = 10 + master->GetNpcBotsCount()/2;
    if (m_botCommandState != COMMAND_FOLLOW && m_botCommandState != COMMAND_STAY)
    {
        opponent = me->getVictim();
        if (opponent)
        {
            switch (GetPetType(me))
            {
                case PET_TYPE_IMP:
                    CalculateAttackPos(opponent, attackpos);
                    if (me->GetDistance(attackpos) > 8)
                        GetInPosition(true, true, opponent, &attackpos);
                    break;
                default:
                    if (me->GetDistance(opponent) > 1.5f)
                        GetInPosition(true, false);
                    break;
            }
        }
    }
    if (clear_cd > 0)
        --clear_cd;
    else
    {
        FindTank();
        clear_cd = 15;
    }
    return;
}

bool bot_ai::CanBotAttack(Unit* target, int8 byspell) const
{
    if (!target) return false;
    uint8 followdist = master->GetBotFollowDist();
    float foldist = _getAttackDistance(float(followdist));
    return
       (target->isAlive() &&
       target->IsVisible() &&
       (master->isDead() || target->GetTypeId() == TYPEID_PLAYER || target->isPet() ||
       (target->GetDistance(master) < foldist && me->GetDistance(master) < followdist)) &&//if master is killed pursue to the end
        target->isTargetableForAttack() &&
        !IsInBotParty(target) &&
        (target->IsHostileTo(master) ||
        (target->GetReactionTo(master) < REP_FRIENDLY && master->getVictim() == target && (master->isInCombat() || target->isInCombat())) ||//master has pointed this target
        target->IsHostileTo(me)) &&//if master is controlled
        //target->IsWithinLOSInMap(me) &&
        (byspell == -1 || !target->IsImmunedToDamage(byspell ? SPELL_SCHOOL_MASK_MAGIC : SPELL_SCHOOL_MASK_NORMAL)));
}
//GETTARGET
//Returns attack target or 'no target'
//uses follow distance if range isn't set
Unit* bot_ai::getTarget(bool byspell, bool ranged, bool &reset) const
{
    //check if no need to change target
    Unit* u = master->GetPlayerbotAI() ? master->GetSession()->m_master->getVictim() : master->getVictim();
    Unit* mytar = me->getVictim();
    if (!mytar && IsMinionAI())
        if (Creature* pet = me->GetBotsPet())
            mytar = pet->getVictim();

    if (u && u == mytar)
    {
        //sLog->outError(LOG_FILTER_PLAYER, "bot %s continues attack common target %s", me->GetName().c_str(), u->GetName().c_str());
        return u;//forced
    }
    //Follow if...
    uint8 followdist = master->GetBotFollowDist();
    float foldist = _getAttackDistance(float(followdist));
    if (!u && master->isAlive() && (me->GetDistance(master) > foldist || (mytar && master->GetDistance(mytar) > foldist && me->GetDistance(master) > foldist)))
    {
        //sLog->outError(LOG_FILTER_PLAYER, "bot %s cannot attack target %s, too far away", me->GetName().c_str(), mytar ? mytar->GetName().c_str() : "");
        return NULL;
    }

    if (u && (master->isInCombat() || u->isInCombat()) && !InDuel(u) && !IsInBotParty(u))
    {
        //sLog->outError(LOG_FILTER_PLAYER, "bot %s starts attack master's target %s", me->GetName().c_str(), u->GetName().c_str());
        return u;
    }

    if (CanBotAttack(mytar, byspell) && !InDuel(mytar))
    {
        //sLog->outError(LOG_FILTER_PLAYER, "bot %s continues attack its target %s", me->GetName().c_str(), mytar->GetName().c_str());
        if (me->GetDistance(mytar) > (ranged ? 20.f : 5.f) && m_botCommandState != COMMAND_STAY && m_botCommandState != COMMAND_FOLLOW)
            reset = true;
        return mytar;
    }

    if (followdist == 0 && master->isAlive())
        return NULL; //do not bother

    //check group
    Group* gr = master->GetGroup();
    if (!gr)
    {
        for (uint8 i = 0; i != master->GetMaxNpcBots(); ++i)
        {
            Creature* bot = master->GetBotMap()[i]._Cre();
            if (!bot || !bot->InSamePhase(me) || bot == me) continue;
            u = bot->getVictim();
            if (u && CanBotAttack(u, byspell) && 
                (bot->isInCombat() || u->isInCombat()) && 
                (master->isDead() || master->GetDistance(u) < foldist))
            {
                //sLog->outError(LOG_FILTER_PLAYER, "bot %s hooked %s's victim %s", me->GetName().c_str(), bot->GetName().c_str(), u->GetName().c_str());
                return u;
            }
            Creature* pet = bot->GetIAmABot() ? bot->GetBotsPet() : NULL;
            if (!pet || !pet->InSamePhase(me)) continue;
            u = pet->getVictim();
            if (u && CanBotAttack(u, byspell) && 
                (pet->isInCombat() || u->isInCombat()) && 
                (master->isDead() || master->GetDistance(u) < foldist))
            {
                //sLog->outError(LOG_FILTER_PLAYER, "bot %s hooked %s's victim %s", me->GetName().c_str(), pet->GetName().c_str(), u->GetName().c_str());
                return u;
            }
        }
    }
    else
    {
        for (GroupReference* ref = gr->GetFirstMember(); ref != NULL; ref = ref->next())
        {
            Player* pl = ref->getSource();
            if (!pl || me->GetMap() != pl->GetMap() || !pl->InSamePhase(me)) continue;
            u = pl->getVictim();
            if (u && pl != master && CanBotAttack(u, byspell) && 
                (pl->isInCombat() || u->isInCombat()) && 
                (master->isDead() || master->GetDistance(u) < foldist))
            {
                //sLog->outError(LOG_FILTER_PLAYER, "bot %s hooked %s's victim %s", me->GetName().c_str(), pl->GetName().c_str(), u->GetName().c_str());
                return u;
            }
            if (!pl->HaveBot()) continue;
            for (uint8 i = 0; i != pl->GetMaxNpcBots(); ++i)
            {
                Creature* bot = pl->GetBotMap()[i]._Cre();
                if (!bot || !bot->InSamePhase(me) || bot == me) continue;
                u = bot->getVictim();
                if (u && CanBotAttack(u, byspell) && 
                    (bot->isInCombat() || u->isInCombat()) && 
                    (master->isDead() || master->GetDistance(u) < foldist))
                {
                    //sLog->outError(LOG_FILTER_PLAYER, "bot %s hooked %s's victim %s", me->GetName().c_str(), bot->GetName().c_str(), u->GetName().c_str());
                    return u;
                }
                Creature* pet = bot->GetIAmABot() ? bot->GetBotsPet() : NULL;
                if (!pet || !pet->InSamePhase(me)) continue;
                u = pet->getVictim();
                if (u && CanBotAttack(u, byspell) && 
                    (pet->isInCombat() || u->isInCombat()) && 
                    (master->isDead() || master->GetDistance(u) < foldist))
                {
                    //sLog->outError(LOG_FILTER_PLAYER, "bot %s hooked %s's victim %s", me->GetName().c_str(), pet->GetName().c_str(), u->GetName().c_str());
                    return u;
                }
            }
        }
    }

    //check targets around
    Unit* t = NULL;
    float maxdist = InitAttackRange(float(followdist), ranged);
    //first cycle we search non-cced target, then, if not found, check all
    for (uint8 i = 0; i != 2; ++i)
    {
        if (!t)
        {
            bool attackCC = i;

            CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
            Cell cell(p);
            cell.SetNoCreate();

            NearestHostileUnitCheck check(me, maxdist, byspell, this, attackCC);
            Trinity::UnitLastSearcher <NearestHostileUnitCheck> searcher(master, t, check);
            me->VisitNearbyWorldObject(maxdist, searcher);

            TypeContainerVisitor<Trinity::UnitLastSearcher <NearestHostileUnitCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
            TypeContainerVisitor<Trinity::UnitLastSearcher <NearestHostileUnitCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);
            cell.Visit(p, world_unit_searcher, *master->GetMap(), *master, maxdist);
            cell.Visit(p, grid_unit_searcher, *master->GetMap(), *master, maxdist);
        }
    }

    if (t && opponent && t != opponent)
    {
        //sLog->outError(LOG_FILTER_PLAYER, "bot %s has Found new target %s", me->GetName().c_str(), t->GetName().c_str());
        reset = true;
    }
    return t;
}
//'CanAttack' function
bool bot_ai::CheckAttackTarget(uint8 botOrPetType)
{
    bool byspell = false, ranged = false, reset = false;
    if (IsMinionAI())
    {
        switch (botOrPetType)
        {
            case CLASS_DRUID:
                byspell = me->GetShapeshiftForm() == FORM_NONE || 
                    me->GetShapeshiftForm() == FORM_TREE || 
                    me->GetShapeshiftForm() == FORM_MOONKIN;
                ranged = byspell;
                break;
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            case CLASS_SHAMAN:
                byspell = true;
                ranged = true;
                break;
            case CLASS_HUNTER:
                ranged = true;
                break;
            default:
                break;
        }
    }
    else
    {
        switch (botOrPetType)
        {
            case PET_TYPE_IMP:
                byspell = true;
                ranged = true;
                break;
            default:
                break;
        }
    }

    opponent = getTarget(byspell, ranged, reset);
    if (!opponent)
    {
        me->AttackStop();
        return false;
    }

    if (reset)
        m_botCommandState = COMMAND_ABANDON;//reset AttackStart()

    me->Attack(opponent, !ranged);
    return true;
}
//POSITION
void bot_ai::CalculateAttackPos(Unit* target, Position& pos) const
{
    uint8 followdist = master->GetBotFollowDist();
    float x(0),y(0),z(0),
        dist = float(6 + urand(followdist/4, followdist/3)),
        angle = target->GetAngle(me);
    dist = std::min(dist, 20.f);
    if (me->GetIAmABotsPet())
        dist *= 0.5f;
    float clockwise = RAND(1.f,-1.f);
    for (uint8 i = 0; i != 5; ++i)
    {
        target->GetNearPoint(me, x, y, z, me->GetObjectSize()/2.f, dist, angle);
        bool toofaraway = master->GetDistance(x,y,z) > (followdist > 30 ? 30.f : followdist < 20 ? 20.f : float(followdist));
        bool outoflos = !target->IsWithinLOS(x,y,z);
        if (toofaraway || outoflos)
        {
            if (toofaraway)
                angle = target->GetAngle(master) + frand(0.f, M_PI*0.5f) * clockwise;
            if (outoflos)
                dist *= 0.5f;
        }
        else
        {
            dist *= 0.75f;
            break;
        }
    }
    pos.m_positionX = x;
    pos.m_positionY = y;
    pos.m_positionZ = z;
}
// Forces bot to chase opponent (if ranged then distance depends on follow distance)
void bot_ai::GetInPosition(bool force, bool ranged, Unit* newtarget, Position* mypos)
{
    if (me->HasUnitState(UNIT_STATE_ROOT)) return;
    if (!newtarget)
        newtarget = me->getVictim();
    if (!newtarget)
        return;
    if ((!newtarget->isInCombat() || m_botCommandState == COMMAND_STAY) && !force)
        return;
    if (IsCasting())
        return;
    uint8 followdist = master->GetBotFollowDist();
    if (ranged)
    {
        if (newtarget->GetTypeId() == TYPEID_PLAYER && 
            me->GetDistance(newtarget) < 6 + urand(followdist/4, followdist/3)) return;//do not allow constant runaway from player
        if (!mypos)
            CalculateAttackPos(newtarget, attackpos);
        else
        {
            attackpos.m_positionX = mypos->m_positionX;
            attackpos.m_positionY = mypos->m_positionY;
            attackpos.m_positionZ = mypos->m_positionZ;
        }
        if (me->GetDistance(attackpos) > 8)
            me->GetMotionMaster()->MovePoint(newtarget->GetMapId(), attackpos);
    }
    else
        me->GetMotionMaster()->MoveChase(newtarget);
    me->Attack(newtarget, !ranged);
}

bool bot_ai::MoveBehind(Unit& target) const
{
    if (me->HasUnitState(UNIT_STATE_ROOT)) return false;
    if (target.IsWithinCombatRange(me, ATTACK_DISTANCE) &&
        target.HasInArc(M_PI, me)                       &&
        tank != me &&
        (me->GetBotClass() == CLASS_ROGUE ? target.getVictim() != me || CCed(&target) : target.getVictim() != me && !CCed(&target)))
    {
        float x(0),y(0),z(0);
        target.GetNearPoint(me, x, y, z, me->GetObjectSize()/3, 0.1f, me->GetAngle(&target));
        me->GetMotionMaster()->MovePoint(target.GetMapId(), x, y, z);
        return true;
    }
    return false;
}
//MOUNT SUPPORT
void bot_minion_ai::UpdateMountedState()
{
    if (master->IsMounted() && me->IsMounted())
    {
        if ((master->HasAuraType(SPELL_AURA_FLY) || master->HasUnitMovementFlag(MOVEMENTFLAG_CAN_FLY) || master->HasUnitMovementFlag(MOVEMENTFLAG_FLYING)))
        {
            //creature don't benefit from mount flight speed, so force it
            if (me->GetSpeed(MOVE_FLIGHT) != master->GetSpeed(MOVE_FLIGHT)/2)
            me->SetSpeed(MOVE_FLIGHT, master->GetSpeed(MOVE_FLIGHT)/2);
        }
        return;
    }
    bool aura = me->HasAuraType(SPELL_AURA_MOUNTED);
    bool mounted = me->IsMounted();
    if ((!master->IsMounted() || aura != mounted || (me->isInCombat() && opponent)) && (aura || mounted))
    {
        me->RemoveAurasByType(SPELL_AURA_MOUNTED);
        me->Dismount();
        return;
    }
    if (me->isInCombat() || IsCasting() || me->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING)) //IsInWater() is too much calculations
        return;
    //fly
    //if ((master->IsMounted() && master->HasAuraType(SPELL_AURA_FLY))/* || master->HasUnitMovementFlag(MOVEMENTFLAG_CAN_FLY) || master->HasUnitMovementFlag(MOVEMENTFLAG_FLYING)*/)
    //{
    //    if (!me->IsMounted() || !me->HasAuraType(SPELL_AURA_FLY))
    //    {
    //        //if (me->GetBotClass() == CLASS_DRUID && InitSpell(FLY_FORM))//TODO
    //        //{
    //        //}
    //        //else
    //        {
    //            uint32 mount = 0;
    //            Unit::AuraEffectList const &mounts = master->GetAuraEffectsByType(SPELL_AURA_MOUNTED);
    //            if (!mounts.empty())
    //                mount = mounts.front()->GetId();
    //            if (mount)
    //            {
    //                if (me->HasAuraType(SPELL_AURA_MOUNTED))
    //                        me->RemoveAurasByType(SPELL_AURA_MOUNTED);
    //                if (doCast(me, mount))
    //                {
    //                    if (Feasting())
    //                    {
    //                        me->RemoveAurasDueToSpell(DRINK);
    //                        me->RemoveAurasDueToSpell(EAT);
    //                    }
    //                }
    //            }
    //        }
    //    }
    //}
    ////ground
    /*else */
    if (master->IsMounted() && !me->IsMounted() && !master->isInCombat() && !me->isInCombat())
    {
        uint32 mount = 0;
        Unit::AuraEffectList const &mounts = master->GetAuraEffectsByType(SPELL_AURA_MOUNTED);
        if (!mounts.empty())
            mount = mounts.front()->GetId();
        if (mount)
        {
            if (me->HasAuraType(SPELL_AURA_MOUNTED))
                me->RemoveAurasByType(SPELL_AURA_MOUNTED);
            if (Feasting())
            {
                me->RemoveAurasDueToSpell(DRINK);
                me->RemoveAurasDueToSpell(EAT);
            }
            if (doCast(me, mount))
            {
                return;
            }
        }
    }
}
//STANDSTATE
void bot_minion_ai::UpdateStandState() const
{
    if (master->getStandState() == UNIT_STAND_STATE_STAND && 
        me->getStandState() == UNIT_STAND_STATE_SIT && 
        !Feasting())
        me->SetStandState(UNIT_STAND_STATE_STAND);
    if ((master->getStandState() == UNIT_STAND_STATE_SIT || Feasting()) && !me->isInCombat() && !me->isMoving() && 
        me->getStandState() == UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_SIT);

}
//RATIONS
void bot_minion_ai::UpdateRations() const
{
    if (me->isInCombat() || CCed(me))
    {
        if (me->HasAura(EAT))   me->RemoveAurasDueToSpell(EAT);
        if (me->HasAura(DRINK)) me->RemoveAurasDueToSpell(DRINK);
    }

    //drink
    if (me->getPowerType() == POWER_MANA && !me->IsMounted() && !CCed(me) && 
        !me->isInCombat() && !IsCasting() && rand()%100 < 30 && GetManaPCT(me) < 80 && 
        !me->HasAura(DRINK))
    {
        me->CastSpell(me, DRINK);
        me->SetStandState(UNIT_STAND_STATE_SIT);
    }
    if (me->GetPower(POWER_MANA) < me->GetMaxPower(POWER_MANA) && me->HasAura(DRINK))
        me->ModifyPower(POWER_MANA, me->GetCreateMana()/20);

    //eat
    if (!me->IsMounted() && !CCed(me) && 
        !me->isInCombat() && !IsCasting() && rand()%100 < 30 && GetHealthPCT(me) < 80 && 
        !me->HasAura(EAT))
    {
        me->CastSpell(me, EAT);
        me->SetStandState(UNIT_STAND_STATE_SIT);
    }
    if (me->GetHealth() < me->GetMaxHealth() && me->HasAura(EAT))
        me->SetHealth(me->GetHealth() + me->GetCreateHealth()/20);

    //check
    if (me->GetHealth() >= me->GetMaxHealth() && me->HasAura(EAT))
        me->RemoveAurasDueToSpell(EAT);
    if (me->getPowerType() == POWER_MANA && 
        me->GetPower(POWER_MANA) >= me->GetMaxPower(POWER_MANA) && 
        me->HasAura(DRINK))
        me->RemoveAurasDueToSpell(DRINK);
}
//PASSIVES
// Used to apply common passives (run once)
void bot_ai::ApplyPassives(uint8 botOrPetType) const
{
    //me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
    //me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);

    //apply +healing taken
    if (master->getLevel() >= 60) RefreshAura(BOR);//+40%
    if (IsMinionAI())
    {
        //apply -threat mod
        switch (botOrPetType)
        {
            case CLASS_WARRIOR:
                RefreshAura(RCP,1);//-27%
                break;
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_ROGUE:
                RefreshAura(RCP,3);//-87%
                break;
            default:
                RefreshAura(RCP,2);//-54%
                break;
        }
    }
    else
    {
        switch (botOrPetType)
        {
            case PET_TYPE_VOIDWALKER:
                RefreshAura(DEFENSIVE_STANCE_PASSIVE,2);
                break;
            default:
                break;
        }
    }
}
//check if our party players are in duel. if so - ignore them, their opponents and any bots they have
bool bot_ai::InDuel(Unit* target) const
{
    if (!target) return false;
    bool isbot = target->GetTypeId() == TYPEID_UNIT && (target->ToCreature()->GetIAmABot() || target->ToCreature()->GetIAmABotsPet());
    Player* player = target->GetTypeId() == TYPEID_PLAYER ? target->ToPlayer() : isbot ? target->ToCreature()->GetBotOwner() : NULL;
    if (!player)
    {
        if (!target->IsControlledByPlayer())
            return false;
        player = target->GetCharmerOrOwnerPlayerOrPlayerItself();
    }
    if (!player) return false;

    if (player->duel)
    {
        if (IsInBotParty(player))
            return true;
        else if (master->duel)
            if (master->duel->opponent == player || player->duel->opponent == master)
                return true;
    }

    return false;
}
//Used to find target for priest's dispels and mage's spellsteal (also shaman's purge in future)
//Returns dispellable/stealable 'Any Hostile Unit Attacking BotParty'
Unit* bot_minion_ai::FindHostileDispelTarget(float dist, bool stealable) const
{
    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    HostileDispelTargetCheck check(me, dist, stealable, this);
    Trinity::UnitLastSearcher <HostileDispelTargetCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <HostileDispelTargetCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <HostileDispelTargetCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
//Finds single target affected by given spell (and given caster if is)
//Can check:
//    hostile targets  (hostile = 0) <default>
//    our party players (hostile = 1)
//    our party members  (hostile = 2)
//    any friendly target (hostile = 3)
//    any target in range  (hostile = any other value)
Unit* bot_minion_ai::FindAffectedTarget(uint32 spellId, uint64 caster, float dist, uint8 hostile) const
{
    if (master->GetMap()->Instanceable())
        dist = DEFAULT_VISIBILITY_INSTANCE;

    CellCoord p(Trinity::ComputeCellCoord(master->GetPositionX(), master->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    AffectedTargetCheck check(caster, dist, spellId, master, hostile);
    Trinity::UnitLastSearcher <AffectedTargetCheck> searcher(master, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <AffectedTargetCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <AffectedTargetCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *master->GetMap(), *master, dist);
    cell.Visit(p, grid_unit_searcher, *master->GetMap(), *master, dist);

    return unit;
}
//Finds target for mage's polymorph (maybe for Hex in future)
Unit* bot_minion_ai::FindPolyTarget(float dist, Unit* currTarget) const
{
    if (!currTarget)
        return NULL;

    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    PolyUnitCheck check(me, dist, currTarget);
    Trinity::UnitLastSearcher <PolyUnitCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <PolyUnitCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <PolyUnitCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
//Finds target for direct fear (warlock)
Unit* bot_minion_ai::FindFearTarget(float dist) const
{
    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    FearUnitCheck check(me, dist);
    Trinity::UnitLastSearcher <FearUnitCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <FearUnitCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <FearUnitCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
//Finds target for paladin's repentance
Unit* bot_minion_ai::FindRepentanceTarget(float dist) const
{
    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    StunUnitCheck check(me, dist);
    Trinity::UnitLastSearcher <StunUnitCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <StunUnitCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <StunUnitCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
//Finds target for priest's shackles
Unit* bot_minion_ai::FindUndeadCCTarget(float dist, uint32 spellId/* = 0*/) const
{
    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    UndeadCCUnitCheck check(me, dist, spellId);
    Trinity::UnitLastSearcher <UndeadCCUnitCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <UndeadCCUnitCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <UndeadCCUnitCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
//Finds target for druid's Entangling Roots
Unit* bot_minion_ai::FindRootTarget(float dist, uint32 spellId) const
{
    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    RootUnitCheck check(me, me->getVictim(), dist, spellId);
    Trinity::UnitLastSearcher <RootUnitCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <RootUnitCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <RootUnitCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
//Finds casting target (friend or enemy)
Unit* bot_minion_ai::FindCastingTarget(float dist, bool isFriend, uint32 spellId) const
{
    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    CastingUnitCheck check(me, dist, isFriend, spellId);
    Trinity::UnitLastSearcher <CastingUnitCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <CastingUnitCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <CastingUnitCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
// Returns target for AOE spell (blizzard, hurricane etc.) based on attackers count
// Cycles through BotParty, first checks player and, if checked, npcbots
// If checked, can return friendly target as target for AOE spell
Unit* bot_minion_ai::FindAOETarget(float dist, bool checkbots, bool targetfriend) const
{
    if (me->isMoving() || IsCasting()) return NULL;//prevent aoe casts while running away
    Unit* unit = NULL;
    Group* pGroup = master->GetGroup();
    if (!pGroup)
    {
        AttackerSet m_attackers = master->getAttackers();
        if (m_attackers.size() > 1)
        {
            uint32 mCount = 0;
            for(AttackerSet::iterator iter = m_attackers.begin(); iter != m_attackers.end(); ++iter)
            {
                if (!(*iter) || (*iter)->isDead()) continue;
                if ((*iter)->isMoving()) continue;
                if ((*iter)->HasBreakableByDamageCrowdControlAura())
                    continue;
                if (me->GetDistance(*iter) < dist)
                    ++mCount;
            }
            if (mCount > 1)
            {
                Unit* u = master->getVictim();
                if (mCount > 3 && targetfriend == true)
                    unit = master;
                else if (u && FindSplashTarget(dist, u))
                    unit = u;
            }//end if
        }//end if
        if (!checkbots)
            return unit;
        for (uint8 i = 0; i != master->GetMaxNpcBots(); ++i)
        {
            Creature* bot = master->GetBotMap()[i]._Cre();
            if (!bot || bot->isDead() || !bot->IsInWorld() || me->GetDistance(bot) > dist) continue;

            AttackerSet b_attackers = bot->getAttackers();
            if (b_attackers.size() > 1)
            {
                uint32 mCount = 0;
                for(AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
                {
                    if (!(*iter) || (*iter)->isDead()) continue;
                    if ((*iter)->isMoving()) continue;
                    if ((*iter)->HasBreakableByDamageCrowdControlAura())
                        continue;
                    if (me->GetDistance(*iter) < dist)
                        ++mCount;
                }
                if (mCount > 1)
                {
                    Unit* u = bot->getVictim();
                    if (mCount > 3 && targetfriend == true)
                        unit = bot;
                    else if (u && FindSplashTarget(dist, u))
                        unit = u;
                }//end if
            }//end if
            if (unit) return unit;
        }//end for
        return unit;
    }
    bool Bots = false;
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* tPlayer = itr->getSource();
        if (!tPlayer) continue;
        if (checkbots && tPlayer->HaveBot())
            Bots = true;
        if (tPlayer->isDead() || master->GetMap() != tPlayer->GetMap()) continue;
        if (me->GetDistance(tPlayer) > 40) continue;

        AttackerSet m_attackers = tPlayer->getAttackers();
        if (m_attackers.size() > 1)
        {
            uint32 mCount = 0;
            for (AttackerSet::iterator iter = m_attackers.begin(); iter != m_attackers.end(); ++iter)
            {
                if (!(*iter) || (*iter)->isDead()) continue;
                if ((*iter)->isMoving()) continue;
                if (me->GetDistance(*iter) < dist)
                    ++mCount;
            }
            if (mCount > 1)
            {
                Unit* u = tPlayer->getVictim();
                if (mCount > 3 && targetfriend == true)
                    unit = tPlayer;
                else if (u && FindSplashTarget(dist, u))
                    unit = u;
            }//end if
        }//end if
        if (unit) return unit;
    }//end for
    if (!Bots) return NULL;
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* tPlayer = itr->getSource();
        if (tPlayer == NULL || !tPlayer->HaveBot()) continue;
        for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
        {
            Creature* bot = tPlayer->GetBotMap()[i]._Cre();
            if (!bot || bot->isDead() || master->GetMap() != bot->GetMap()) continue;
            if (me->GetDistance(bot) > 40) continue;

            AttackerSet b_attackers = bot->getAttackers();
            if (b_attackers.size() > 1)
            {
                uint32 mCount = 0;
                for(AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
                {
                    if (!(*iter) || (*iter)->isDead()) continue;
                    if ((*iter)->isMoving()) continue;
                    if (me->GetDistance(*iter) < dist)
                        ++mCount;
                }
                if (mCount > 1)
                {
                    Unit* u = bot->getVictim();
                    if (mCount > 3 && targetfriend == true)
                        unit = bot;
                    else if (u && FindSplashTarget(dist, u))
                        unit = u;
                }//end if
            }//end if
        }//end for
        if (unit) return unit;
    }//end for
    return unit;
}
// Finds second target for spells like Cleave, Swipe, (maybe Mind Sear) etc.
Unit* bot_minion_ai::FindSplashTarget(float dist, Unit* To) const
{
    if (!To)
        To = me->getVictim();
    if (!To)
        return NULL;

    if (me->GetDistance(To) > dist)
        return NULL;

    CellCoord p(Trinity::ComputeCellCoord(me->GetPositionX(), me->GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* unit = NULL;

    SecondEnemyCheck check(me, dist, To, this);
    Trinity::UnitLastSearcher <SecondEnemyCheck> searcher(me, unit, check);

    TypeContainerVisitor<Trinity::UnitLastSearcher <SecondEnemyCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
    TypeContainerVisitor<Trinity::UnitLastSearcher <SecondEnemyCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, world_unit_searcher, *me->GetMap(), *me, dist);
    cell.Visit(p, grid_unit_searcher, *me->GetMap(), *me, dist);

    return unit;
}
//////////
//Internal
//////////
uint32 bot_ai::InitSpell(Unit* caster, uint32 spell)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell);
    if (!info)
        return 0;//weird spell with no info, disable it

    uint8 lvl = caster->getLevel();
    if (lvl < info->BaseLevel)//only 1st rank spells check
        return 0;//cannot use this spell

    if (SpellInfo const* spInfo = info->GetNextRankSpell())
    {
        if (lvl < spInfo->BaseLevel)
            return spell;//cannot use next rank, use this one
        else
            return InitSpell(caster, spInfo->Id);//can use next rank, forward check
    }

    return spell;//max rank, use this
}
void bot_minion_ai::_OnHealthUpdate(uint8 myclass, uint8 mylevel) const
{
    //sLog->outError(LOG_FILTER_PLAYER, "_OnHealthUpdate(): updating bot %s", me->GetName().c_str());
    float pct = me->GetHealthPct();// needs for regeneration
    uint32 m_basehp = classinfo.basehealth;
    //sLog->outError(LOG_FILTER_PLAYER, "class base health: %u", m_basehp);
    me->SetCreateHealth(m_basehp);
    float stammod;
    switch (myclass)
    {
        case CLASS_WARRIOR: case CLASS_DEATH_KNIGHT: case BEAR:
            switch (master->getClass())
            {
                case CLASS_PRIEST: case CLASS_MAGE: case CLASS_WARLOCK:
                    stammod = 16.f;
                    break;
                case CLASS_DRUID: case CLASS_SHAMAN: case CLASS_HUNTER: case CLASS_ROGUE:
                    stammod = 13.f;
                    break;
                default: stammod = 9.8f; break;
            }
            break;
        case CLASS_PALADIN:
            switch (master->getClass())
            {
                case CLASS_PRIEST: case CLASS_MAGE: case CLASS_WARLOCK:
                    stammod = 15.5f;
                    break;
                case CLASS_DRUID: case CLASS_SHAMAN: case CLASS_HUNTER: case CLASS_ROGUE:
                    stammod = 12.5f;
                    break;
                case CLASS_PALADIN:
                    stammod = 9.8f;
                    break;
                default: stammod = 9.f; break;
            }
            break;
        case CLASS_PRIEST: case CLASS_MAGE: case CLASS_WARLOCK:
            switch (master->getClass())
            {
                case CLASS_PRIEST: case CLASS_MAGE: case CLASS_WARLOCK:
                    stammod = 9.8f;
                    break;
                case CLASS_DRUID: case CLASS_SHAMAN: case CLASS_HUNTER: case CLASS_ROGUE:
                    stammod = 8.f;
                    break;
                default: stammod = 5.f; break;
            }
            break;
        case CLASS_DRUID: case CAT: case CLASS_SHAMAN: case CLASS_HUNTER: case CLASS_ROGUE:
            switch (master->getClass())
            {
                case CLASS_PRIEST: case CLASS_MAGE: case CLASS_WARLOCK:
                    stammod = 12.f;
                    break;
                case CLASS_DRUID: case CLASS_SHAMAN: case CLASS_HUNTER: case CLASS_ROGUE:
                    stammod = 9.8f;
                    break;
                default: stammod = 8.f; break;
            }
            break;
        default: stammod = 10.f;
            break;
    }
    stammod -= 0.3f;
    //sLog->outError(LOG_FILTER_PLAYER, "stammod: %f", stammod);
    
    //manually pick up stamina from bot's buffs
    float stamValue = me->GetTotalStatValue(STAT_STAMINA);
    stamValue = std::max(stamValue - 18.f, 1.f); //remove base stamina (not calculated into health)
    //sLog->outError(LOG_FILTER_PLAYER, "bot's stats to health add: Stamina (%f), value: %f", stamValue, stamValue * 10.f);
    int32 hp_add = int32(stamValue * 10.f);
    //pick up master's stamina from items
    float total_pct = std::max((master->GetModifierValue(UNIT_MOD_STAT_STAMINA, TOTAL_PCT) - 0.1f), 1.f);
    float base_stam = master->GetModifierValue(UNIT_MOD_STAT_STAMINA, BASE_VALUE);
    base_stam = std::max(base_stam - 18.f, 0.f); //remove base stamina (not calculated into health)
    stamValue = base_stam * master->GetModifierValue(UNIT_MOD_STAT_STAMINA, BASE_PCT) * total_pct;
    //sLog->outError(LOG_FILTER_PLAYER, "stat to health add: Stamina (%f), value: %f", stamValue, stamValue*stammod);
    hp_add += int32(stamValue*stammod);
    //float stamstat = stat * 0.5f;
    //if (stamValue > stamstat)
    //{
    //    //sLog->outBasic("selected stat to health add: Stamina (%f), value: %f", stamValue, stamValue*stammod);
    //    hp_add += int32(stamValue * stammod);
    //}
    //else
    //{
    //    //sLog->outBasic("selected stat to health add: stamStat (%f), value: %f", stamstat, stamstat*stammod);
    //    hp_add += int32(stamstat * stammod);
    //}
    //sLog->outBasic("health to add after master's stat mod: %i", hp_add);
    int32 miscVal = me->getGender()*mylevel;
    //sLog->outError(LOG_FILTER_PLAYER, "health to remove from gender mod: %i", -miscVal);
    hp_add -= miscVal;//less hp for females lol
    //sLog->outError(LOG_FILTER_PLAYER, "health to add after gender mod: %i", hp_add);
    //miscVal = myrace*(mylevel/5);
    //sLog->outError(LOG_FILTER_PLAYER, "health to add from race mod: %i", miscVal);
    //hp_add += miscVal;//draenei tanks lol
    //sLog->outError(LOG_FILTER_PLAYER, "health to add after race mod: %i", hp_add);
    miscVal = master->GetNpcBotSlot(me->GetGUID()) * (mylevel/5);
    //sLog->outError(LOG_FILTER_PLAYER, "health to remove from slot mod: %i", -miscVal);
    hp_add -= miscVal;
    //sLog->outError(LOG_FILTER_PLAYER, "health to add after slot mod: %i", hp_add);
    uint32 m_totalhp = m_basehp + hp_add;//m_totalhp = uint32(float(m_basehp + hp_add) * stammod);
    //sLog->outError(LOG_FILTER_PLAYER, "total base health: %u", m_totalhp);
    if (master->GetBotTankGuid() == me->GetGUID())
    {
        m_totalhp = (m_totalhp * 135) / 100;//35% hp bonus for tanks
        //sLog->outBasic("total base health (isTank): %u", m_totalhp);
    }
    me->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, float(m_totalhp));//replaces base 18900 hp at 80 lvl
    me->UpdateMaxHealth();//will use our values we just set (update base health and buffs)
    //sLog->outError(LOG_FILTER_PLAYER, "overall hp: %u", me->GetMaxHealth());
    me->SetHealth(uint32(0.5f + float(me->GetMaxHealth()) * pct / 100.f));//restore pct
    if (!me->isInCombat())
        me->SetHealth(me->GetHealth() + m_basehp / 100);//regenerate
}

void bot_minion_ai::_OnManaUpdate(uint8 myclass, uint8 mylevel) const
{
    if (me->getPowerType() != POWER_MANA)
        return;
    //sLog->outError(LOG_FILTER_PLAYER, "_OnManaUpdate(): updating bot %s", me->GetName().c_str());
    float pct = (float(me->GetPower(POWER_MANA)) * 100.f) / float(me->GetMaxPower(POWER_MANA));
    float m_basemana = classinfo.basemana > 0 ? classinfo.basemana : me->GetCreateMana();
    //sLog->outError(LOG_FILTER_PLAYER, "classinfo base mana = %f", m_basemana);
    me->SetCreateMana(m_basemana);//set base mana, critical
    float manamod = 15.f;//here we set mana multiplier from intellect as we gain mana from MASTER's stats mostly
    switch (myclass)
    {
        case CLASS_PALADIN: case CLASS_HUNTER: manamod =  4.5f; break;
        case CLASS_SHAMAN:                     manamod = 11.5f; break;
        case CLASS_DRUID:                      manamod = 12.5f; break;
        case CLASS_PRIEST:                     manamod = 16.5f; break;
        case CLASS_MAGE: case CLASS_WARLOCK:   manamod = 10.5f; break;
        default:                                                break;
    }
    //manamod += 1.f;//custom
    //manamod *= 0.70f;//custom
    //sLog->outError(LOG_FILTER_PLAYER, "Manamod: %f", manamod);
    float intValue = me->GetTotalStatValue(STAT_INTELLECT);
    intValue = std::max(intValue - 18.f, 1.f); //remove base int (not calculated into mana)
    //sLog->outError(LOG_FILTER_PLAYER, "bot's stats to mana add: Int (%f), value: %f", intValue, intValue * manamod);
    m_basemana += intValue * 15.f;
    //pick up master's intellect from items if master has mana
    if (master->getPowerType() == POWER_MANA)
    {
        float total_pct = std::max((master->GetModifierValue(UNIT_MOD_STAT_INTELLECT, TOTAL_PCT) - 0.1f), 1.f);
        intValue = std::max(master->GetModifierValue(UNIT_MOD_STAT_INTELLECT, BASE_VALUE) - 18.f, 1.f); //remove base int (not calculated into mana)
        intValue = intValue * master->GetModifierValue(UNIT_MOD_STAT_INTELLECT, BASE_PCT) * total_pct;
    }
    else// pick up maxstat
        intValue = stat * 0.5f;
    //sLog->outError(LOG_FILTER_PLAYER, "mana add from master's stat: %f", intValue * manamod);
    m_basemana += intValue * manamod;
    //sLog->outError(LOG_FILTER_PLAYER, "base mana + mana from master's intellect or stat: %f", m_basemana);
    //intValue = me->GetTotalAuraModValue(UNIT_MOD_STAT_INTELLECT);
    //sLog->outBasic("Intellect from buffs: %f", intValue);
    //m_basemana += uint32(intValue) * manamod;
    //sLog->outBasic("base mana + mana from intellect + mana from buffs: %u", m_basemana);
    uint8 otherVal = me->getGender()*3*mylevel;
    //sLog->outError(LOG_FILTER_PLAYER, "mana to add from gender mod: %u", otherVal);
    m_basemana += float(otherVal);//more mana for females lol
    //sLog->outError(LOG_FILTER_PLAYER, "base mana after gender mod: %f", m_basemana);
    otherVal = master->GetNpcBotSlot(me->GetGUID()) * (mylevel/5);// only to make mana unique
    //sLog->outError(LOG_FILTER_PLAYER, "mana to remove from slot mod: %i", -int8(otherVal));
    m_basemana -= otherVal;
    //sLog->outError(LOG_FILTER_PLAYER, "base mana after slot mod: %f", m_basemana);
    float m_totalmana = m_basemana;
    //sLog->outError(LOG_FILTER_PLAYER, "total mana to set: %f", m_totalmana);
    me->SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, m_totalmana);
    me->UpdateMaxPower(POWER_MANA);
    //sLog->outError(LOG_FILTER_PLAYER, "Overall mana to set: %u", me->GetMaxPower(POWER_MANA));
    me->SetPower(POWER_MANA, uint32(0.5f + float(me->GetMaxPower(POWER_MANA)) * pct / 100.f));//restore pct
    //No Regen
}

void bot_minion_ai::_OnMeleeDamageUpdate(uint8 myclass) const
{
    if (ap_mod == 0.f) return; //do not bother casters
    //sLog->outBasic("_OnMeleeDamageUpdate: Updating bot %s", me->GetName().c_str());
    float my_ap_mod = ap_mod;
    float mod = master->getClass() == CLASS_HUNTER ? (master->GetModifierValue(UNIT_MOD_DAMAGE_RANGED, BASE_PCT) + master->GetModifierValue(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT))/2.f : 
        (master->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_PCT) + master->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT))/2.f;
    mod = std::max(mod, 1.f); // x1 is Minimum
    mod = std::min(mod, 2.5f); // x2.5 is Maximum
    //sLog->outBasic("got base damage modifier: %f", mod);
    mod -= (mod - 1.f)*0.33f;//reduce bonus by 33%
    //sLog->outBasic("damage modifier truencated to %f, applying", mod);
    me->SetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_PCT, mod);
    me->SetModifierValue(UNIT_MOD_DAMAGE_OFFHAND, BASE_PCT, mod);
    me->SetModifierValue(UNIT_MOD_DAMAGE_RANGED, BASE_PCT, mod);
    me->SetCanDualWield(myclass == CLASS_ROGUE || myclass == CLASS_SHAMAN);
    //Rogue has mainhand attack speed 1900, other dual-wielders - 2800 or 2600 or 2400
    if (myclass == CLASS_ROGUE)
        me->SetAttackTime(BASE_ATTACK, 1900);
    if (me->CanDualWield())
        me->SetAttackTime(OFF_ATTACK, myclass == CLASS_ROGUE ? 1400 : 1800);
    //me->SetModifierValue(UNIT_MOD_DAMAGE_RANGED, BASE_PCT, mod);//NUY
    mod = (mod - 1.f)*0.5f;
    //sLog->outBasic("reduced damage modifier to gain bonus: %f", mod);
    //sLog->outBasic("base ap modifier is %f", my_ap_mod);
    my_ap_mod *= 0.5f;
    //sLog->outBasic("ap modifier multiplied to %f", my_ap_mod);
    my_ap_mod += my_ap_mod > 0.f ? mod : 0.f; //add reduced master's multiplier if can have damage
    //sLog->outBasic("ap modifier + mod = %f", my_ap_mod);
    me->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_PCT, my_ap_mod);
    me->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_PCT, my_ap_mod);

    int32 sppower = 0;
    for (uint8 i = SPELL_SCHOOL_HOLY; i != MAX_SPELL_SCHOOL; ++i)
    {
        int32 power = master->SpellBaseDamageBonusDone(SpellSchoolMask(1 << i));
        if (power > sppower)
            sppower = power;
    }
    //sLog->outBasic("master's spellpower is %i, multiplying...", sppower);
    sppower *= 1.5f;
    //sLog->outBasic("got spellpower of %i", sppower);
    //atpower = float(master->GetInt32Value(master->getClass() == CLASS_HUNTER ? UNIT_FIELD_RANGED_ATTACK_POWER : UNIT_FIELD_ATTACK_POWER));
    float atpower = master->GetTotalAttackPowerValue(master->getClass() == CLASS_HUNTER ? RANGED_ATTACK : BASE_ATTACK);
    //sLog->outBasic("master's base attack power is %f", atpower);
    atpower = sppower > atpower ? sppower : atpower;//highest stat is used (either 1.5x spellpower or attack power)
    //sLog->outBasic("chosen attack power stat value: %f", atpower);
    //sLog->outBasic("expected attack power: %f", atpower*ap_mod);

    me->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, atpower);
    if (myclass == CLASS_HUNTER || myclass == CLASS_ROGUE)
    {
        me->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, atpower);
        me->UpdateAttackPowerAndDamage(true);
    }
    me->UpdateAttackPowerAndDamage();
    //sLog->outBasic("listing stats: ");
    //sLog->outBasic("attack power main hand: %f", me->GetTotalAttackPowerValue(BASE_ATTACK));
    //sLog->outBasic("attack power off hand: %f", me->GetTotalAttackPowerValue(OFF_ATTACK));
    //sLog->outBasic("attack power ranged: %f", me->GetTotalAttackPowerValue(RANGED_ATTACK));
    //sLog->outBasic("damage multiplier main hand: %f", me->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_PCT) * me->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT));
    //sLog->outBasic("damage multiplier off hand: %f", me->GetModifierValue(UNIT_MOD_DAMAGE_OFFHAND, BASE_PCT) * me->GetModifierValue(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT));
    //sLog->outBasic("damage multiplier ranged: %f", me->GetModifierValue(UNIT_MOD_DAMAGE_RANGED, BASE_PCT) * me->GetModifierValue(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT));
    //sLog->outBasic("Damage range main hand: min: %f, max: %f", me->GetFloatValue(UNIT_FIELD_MINDAMAGE), me->GetFloatValue(UNIT_FIELD_MAXDAMAGE));
    //sLog->outBasic("Damage range off hand: min: %f, max: %f", me->GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE), me->GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
    //sLog->outBasic("Damage range ranged: min: %f, max: %f", me->GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), me->GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
}

void bot_pet_ai::_OnHealthUpdate(uint8 /*petType*/, uint8 mylevel) const
{
    float hp_mult = 10.f;
    switch (GetPetType(me))
    {
        case PET_TYPE_VOIDWALKER:
            hp_mult = 11.f;
            break;
        default:
            break;
    }
    float pct = me->GetHealthPct();// needs for regeneration
    //Use simple checks and calcs
    //0.3 hp for bots (inaccurate but cheap)
    uint32 m_basehp = me->GetCreateHealth()/2;
    //pick up stamina from buffs
    float stamValue = me->GetTotalStatValue(STAT_STAMINA);
    stamValue = std::max(stamValue - 18.f, 1.f); //remove base stamina (not calculated into health)
    uint32 hp_add = uint32(stamValue*hp_mult);
    hp_add += (m_creatureOwner->GetMaxHealth() - m_creatureOwner->GetCreateHealth())*0.3f;
    uint8 miscVal = GetPetType(me)*mylevel;
    hp_add -= miscVal;
    uint32 m_totalhp = m_basehp + hp_add;
    if (master->GetBotTankGuid() == me->GetGUID())
        m_totalhp = (m_totalhp*135) / 100;//35% hp bonus for tanks
    me->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, float(m_totalhp));
    me->UpdateMaxHealth();//will use values set (update base health and buffs)
    me->SetHealth(uint32(0.5f + float(me->GetMaxHealth())*pct / 100.f));//restore pct
    if (!me->isInCombat())
        me->SetHealth(me->GetHealth() + m_basehp / 100);//regenerate
}

void bot_pet_ai::_OnManaUpdate(uint8 /*petType*/, uint8 mylevel) const
{
    if (me->getPowerType() != POWER_MANA)
        return;

    float mana_mult = 15.f;
    switch (GetPetType(me))
    {
        case PET_TYPE_VOIDWALKER:
            mana_mult = 11.5f;
            break;
        default:
            break;
    }
    float pct = (float(me->GetPower(POWER_MANA)) * 100.f) / float(me->GetMaxPower(POWER_MANA));
    //Use simple checks and calcs
    //0.3 mana for bots (inaccurate but cheap)
    float m_basemana = float(me->GetCreateMana());
    m_basemana += (std::max<float>(me->GetTotalStatValue(STAT_INTELLECT) - 18.f, 1.f))*mana_mult; //remove base stamina (not calculated into mana)
    m_basemana += float(m_creatureOwner->GetMaxPower(POWER_MANA) - m_creatureOwner->GetCreateMana())*0.3f;
    m_basemana -= float(GetPetType(me)*mylevel);
    me->SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, m_basemana);
    me->UpdateMaxPower(POWER_MANA);
    me->SetPower(POWER_MANA, uint32(0.5f + float(me->GetMaxPower(POWER_MANA))*pct / 100.f));//restore pct
}
//emulates evade mode, removes buggy bots' threat from party, so no 'stuck in combat' bugs form bot mod
//optionally interrupts casted spell if target is dead for bot and it's pet
void bot_minion_ai::_OnEvade()
{
    if (me->HasUnitState(UNIT_STATE_CASTING))
        for (uint8 i = CURRENT_FIRST_NON_MELEE_SPELL; i != CURRENT_AUTOREPEAT_SPELL; ++i)
            if (Spell* spell = me->GetCurrentSpell(CurrentSpellTypes(i)))
                if (!spell->GetSpellInfo()->IsChanneled())
                    if (Unit* u = spell->m_targets.GetUnitTarget())
                        if (u->isDead() && !IsInBotParty(u))
                            me->InterruptSpell(CurrentSpellTypes(i), false, false);

    Creature* m_botsPet = me->GetBotsPet();
    if (m_botsPet && m_botsPet->HasUnitState(UNIT_STATE_CASTING))
        for (uint8 i = CURRENT_FIRST_NON_MELEE_SPELL; i != CURRENT_AUTOREPEAT_SPELL; ++i)
            if (Spell* spell = m_botsPet->GetCurrentSpell(CurrentSpellTypes(i)))
                if (!spell->GetSpellInfo()->IsChanneled())
                    if (Unit* u = spell->m_targets.GetUnitTarget())
                        if (u->isDead() && !IsInBotParty(u))
                            m_botsPet->InterruptSpell(CurrentSpellTypes(i), false, false);

    if (Rand() > 10) return;
    if (!master->isInCombat() && !me->isInCombat() && (!m_botsPet || !m_botsPet->isInCombat())) return;
    if (CheckAttackTarget(GetBotClassForCreature(me)))
        return;
    //ChatHandler ch(master->GetPlayerbotAI() ? master->GetSession()->m_master : master);
    //ch.PSendSysMessage("_OnEvade() by bot %s", me->GetName().c_str());
    if (master->isInCombat())
    {
        HostileRefManager& mgr = master->getHostileRefManager();
        if (!mgr.isEmpty())
        {
            std::set<Unit*> Set;
            HostileReference* ref = mgr.getFirst();
            while (ref)
            {
                if (ref->getSource() && ref->getSource()->getOwner())
                    Set.insert(ref->getSource()->getOwner());
                ref = ref->next();
            }
            for (std::set<Unit*>::const_iterator i = Set.begin(); i != Set.end(); ++i)
            {
                Unit* unit = (*i);
                if (/*unit->IsFriendlyTo(master)*/IsInBotParty(unit) || !unit->isInCombat())
                {
                    //ch.PSendSysMessage("_OnEvade(): %s's hostile reference is removed from %s!", unit->GetName().c_str(), master->GetName().c_str());
                    mgr.deleteReference(unit);
                }
            }
        }
        return;
    }
    else if (master->getHostileRefManager().isEmpty())
    {
        for (uint8 i = 0; i != master->GetMaxNpcBots(); ++i)
        {
            Creature* cre = master->GetBotMap()[i]._Cre();
            if (!cre) continue;
            if (cre->isInCombat())
            {
                cre->DeleteThreatList();
                HostileRefManager& mgr = cre->getHostileRefManager();
                if (!mgr.isEmpty())
                {
                    std::set<Unit*> Set;
                    HostileReference* ref = mgr.getFirst();
                    while (ref)
                    {
                        if (ref->getSource() && ref->getSource()->getOwner())
                            Set.insert(ref->getSource()->getOwner());
                        ref = ref->next();
                    }
                    for (std::set<Unit*>::const_iterator i = Set.begin(); i != Set.end(); ++i)
                    {
                        Unit* unit = (*i);
                        if (!unit->InSamePhase(me)) continue;
                        if (/*unit->IsFriendlyTo(master)*/IsInBotParty(unit) || !unit->isInCombat())
                        {
                            //ch.PSendSysMessage("_OnEvade(): %s's hostile reference is removed from %s!", unit->GetName().c_str(), cre->GetName().c_str());
                            mgr.deleteReference(unit);
                        }
                    }
                }
                if (mgr.isEmpty())// has empty threat list and no hostile refs - we have all rights to stop combat
                {
                    if (cre->isInCombat())
                    {
                        //ch.PSendSysMessage("_OnEvade(): %s's HostileRef is empty! Combatstop!", cre->GetName().c_str());
                        cre->CombatStop();
                    }
                }
            }

            Creature* m_botsPet = cre->GetBotsPet();
            if (!m_botsPet || !m_botsPet->isInCombat()) continue;
            m_botsPet->DeleteThreatList();
            HostileRefManager& mgr = m_botsPet->getHostileRefManager();
            if (!mgr.isEmpty())
            {
                std::set<Unit*> Set;
                HostileReference* ref = mgr.getFirst();
                while (ref)
                {
                    if (ref->getSource() && ref->getSource()->getOwner())
                        Set.insert(ref->getSource()->getOwner());
                    ref = ref->next();
                }
                for (std::set<Unit*>::const_iterator i = Set.begin(); i != Set.end(); ++i)
                {
                    Unit* unit = (*i);
                    if (!unit->InSamePhase(me)) continue;
                    if (/*unit->IsFriendlyTo(master)*/IsInBotParty(unit) || !unit->isInCombat())
                    {
                        //ch.PSendSysMessage("_OnEvade(): %s's hostile reference is removed from %s!", unit->GetName().c_str(), m_botsPet->GetName().c_str());
                        mgr.deleteReference(unit);
                    }
                }
            }
            if (mgr.isEmpty())// has empty threat list and no hostile refs - we have all rights to stop combat
            {
                if (m_botsPet->isInCombat())
                {
                    //ch.PSendSysMessage("_OnEvade(): %s's HostileRef is empty! Combatstop!", pet->GetName().c_str());
                    m_botsPet->CombatStop();
                }
            }
        }
        return;
    }
}

void bot_ai::OnSpellHit(Unit* /*caster*/, SpellInfo const* spell)
{
    for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
    {
        uint32 auraname = spell->Effects[i].ApplyAuraName;
        //remove pet on mount
        if (auraname == SPELL_AURA_MOUNTED)
            me->SetBotsPetDied();
        //update stats
        if (auraname == SPELL_AURA_MOD_STAT)
        {
            doHealth = true;
            doMana = true;
        }
        else
        {
            if (auraname == SPELL_AURA_MOD_INCREASE_HEALTH || 
                auraname == SPELL_AURA_MOD_INCREASE_HEALTH_2 || 
                auraname == SPELL_AURA_230 ||  // SPELL_AURA_MOD_INCREASE_HEALTH_2
                auraname == SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT)
                doHealth = true;
            else if (auraname == SPELL_AURA_MOD_INCREASE_ENERGY || 
                auraname == SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT)
                doMana = true;
        }
    }
}

uint8 bot_ai::GetWait()
{
    if (doHealth)
    {
        doHealth = false;
        _OnHealthUpdate(me->GetBotClass(), master->getLevel());
    }
    if (doMana)
    {
        doMana = false;
        _OnManaUpdate(me->GetBotClass(), master->getLevel());
    }
    CheckAuras(true);
    //0 to 2 plus 1 for every 3 bots except first one
    return (1 + (master->GetNpcBotsCount() - 1)/3 + (irand(0,100) <= 50)*int8(RAND(-1,1)));
}
//Damage Mods
//1) Apply class-specified damage/crit chance/crit damage bonuses
//2) Apply bot damage multiplier
//3) Remove Creature damage multiplier (make independent from original config)
void bot_ai::ApplyBotDamageMultiplierMelee(uint32& damage, CalcDamageInfo& damageinfo) const
{
    ApplyClassDamageMultiplierMelee(damage, damageinfo);
    damage = int32(float(damage)*dmgmult_melee/dmgmod_melee);
}
void bot_ai::ApplyBotDamageMultiplierMelee(int32& damage, SpellNonMeleeDamage& damageinfo, SpellInfo const* spellInfo, WeaponAttackType attackType, bool& crit) const
{
    ApplyClassDamageMultiplierMelee(damage, damageinfo, spellInfo, attackType, crit);
    damage = int32(float(damage)*dmgmult_melee/dmgmod_melee);
}
void bot_ai::ApplyBotDamageMultiplierSpell(int32& damage, SpellNonMeleeDamage& damageinfo, SpellInfo const* spellInfo, WeaponAttackType attackType, bool& crit) const
{
    ApplyClassDamageMultiplierSpell(damage, damageinfo, spellInfo, attackType, crit);
    damage = int32(float(damage)*dmgmult_spell/dmgmod_spell);
}

bool bot_minion_ai::OnGossipHello(Player* player, Creature* creature)
{
    switch (creature->GetBotClass())
    {
        case CLASS_MAGE:
            if (creature->isInCombat())
            {
                player->CLOSE_GOSSIP_MENU();
                break;
            }
            player->ADD_GOSSIP_ITEM(0, "I need food", 6001, GOSSIP_ACTION_INFO_DEF + 1);
            player->ADD_GOSSIP_ITEM(0, "I need drink", 6001, GOSSIP_ACTION_INFO_DEF + 2);
            player->PlayerTalkClass->SendGossipMenu(GOSSIP_SERVE_MASTER, creature->GetGUID());
            break;
        default:
            player->CLOSE_GOSSIP_MENU();
            break;
    }
    return true;
}

bool bot_minion_ai::OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
{
    if (!IsInBotParty(player))
    {
        player->CLOSE_GOSSIP_MENU();
        creature->MonsterWhisper("Get away from me!", player->GetGUID());
        return false;
    }
    switch (creature->GetBotClass())
    {
        case CLASS_MAGE:
            switch (sender)
            {
                case 6001:// food/drink
                {
                    //Prevent high-leveled consumables for low-level characters
                    Unit* checker;
                    if (player->getLevel() < creature->getLevel())
                        checker = player;
                    else
                        checker = creature;

                    // Conjure Refreshment rank 1
                    uint32 food = InitSpell(checker, 42955);
                    bool iswater = (action == GOSSIP_ACTION_INFO_DEF + 2);
                    if (!food)
                    {
                        if (!iswater)// Conjure Food rank 1
                            food = InitSpell(checker, 587);
                        else// Conjure Water rank 1
                            food = InitSpell(checker, 5504);
                    }
                    if (!food)
                    {
                        std::string errorstr = "I can't conjure ";
                        errorstr += iswater ? "water" : "food";
                        errorstr += " yet";
                        creature->MonsterWhisper(errorstr.c_str(), player->GetGUID());
                        //player->PlayerTalkClass->ClearMenus();
                        //return OnGossipHello(player, creature);
                        player->CLOSE_GOSSIP_MENU();
                        return false;
                    }
                    player->CLOSE_GOSSIP_MENU();
                    SpellInfo const* Info = sSpellMgr->GetSpellInfo(food);
                    Spell* foodspell = new Spell(creature, Info, TRIGGERED_NONE, player->GetGUID());
                    SpellCastTargets targets;
                    targets.SetUnitTarget(player);
                    //TODO implement checkcast for bots
                    SpellCastResult result = creature->IsMounted() || CCed(creature) ? SPELL_FAILED_CUSTOM_ERROR : foodspell->CheckPetCast(player);
                    if (result != SPELL_CAST_OK)
                    {
                        foodspell->finish(false);
                        delete foodspell;
                        creature->MonsterWhisper("I can't do it right now", player->GetGUID());
                        creature->SendPetCastFail(food, result);
                    }
                    else
                    {
                        aftercastTargetGuid = player->GetGUID();
                        foodspell->prepare(&targets);
                        creature->MonsterWhisper("Here you go...", player->GetGUID());
                    }
                    break;
                }
            }
            break;
        default:
            break;
    }
    return true;
}

void bot_minion_ai::SummonBotsPet(uint32 entry)
{
    Creature* m_botsPet = me->GetBotsPet();
    if (m_botsPet)
        me->SetBotsPetDied();

    uint8 mylevel = std::min<uint8>(master->getLevel(), 80);
    uint32 originalentry = bot_pet_ai::GetPetOriginalEntry(entry);
    if (!originalentry)
    {
        me->MonsterWhisper("Why am I trying to summon unknown pet!?", master->GetGUID());
        return;
    }
    uint32 armor = 0;
    float x(0),y(0),z(0);
    me->GetClosePoint(x, y, z, me->GetObjectSize());
    m_botsPet = me->SummonCreature(entry, x, y, z);

    if (!m_botsPet)
    {
        me->MonsterWhisper("Failed to summon pet!", master->GetGUID());
        return;
    }

    //std::string name = sObjectMgr->GeneratePetName(originalentry);//voidwalker
    //if (!name.empty())
    //    m_botsPet->SetName(name);

    QueryResult result = WorldDatabase.PQuery("SELECT hp, mana, armor, str, agi, sta, inte, spi FROM `pet_levelstats` WHERE `creature_entry` = '%u' AND `level` = '%u'", originalentry, mylevel);

    if (result)
    {
        Field* fields = result->Fetch();
        uint32 hp = fields[0].GetUInt32();
        uint32 mana = fields[1].GetUInt32();
        armor = fields[2].GetUInt32();
        uint32 str = fields[3].GetUInt32();
        uint32 agi = fields[4].GetUInt32();
        uint32 sta = fields[5].GetUInt32();
        uint32 inte = fields[6].GetUInt32();
        uint32 spi = fields[7].GetUInt32();

        m_botsPet->SetCreateHealth(hp);
        m_botsPet->SetMaxHealth(hp);
        m_botsPet->SetCreateMana(mana);
        m_botsPet->SetMaxPower(POWER_MANA, mana);

        m_botsPet->SetCreateStat(STAT_STRENGTH, str);
        m_botsPet->SetCreateStat(STAT_AGILITY, agi);
        m_botsPet->SetCreateStat(STAT_STAMINA, sta);
        m_botsPet->SetCreateStat(STAT_INTELLECT, inte);
        m_botsPet->SetCreateStat(STAT_SPIRIT, spi);
    }

    m_botsPet->SetBotOwner(master);
    m_botsPet->SetBotClass(bot_pet_ai::GetPetClass(m_botsPet));
    master->SetMinion((Minion* )m_botsPet, true);
    m_botsPet->SetUInt64Value(UNIT_FIELD_CREATEDBY, me->GetGUID());
    m_botsPet->DeleteThreatList();
    m_botsPet->AddUnitTypeMask(UNIT_MASK_MINION);
    //m_botsPet->SetLevel(master->getLevel());
    m_botsPet->AIM_Initialize();
    m_botsPet->InitBotAI(true);
    m_botsPet->setFaction(master->getFaction());
    bot_pet_ai* petai = m_botsPet->GetBotPetAI();
    petai->SetCreatureOwner(me);
    petai->SetBaseArmor(armor);
    petai->setStats(mylevel, bot_pet_ai::GetPetType(m_botsPet), true);
    petai->SetBotCommandState(COMMAND_FOLLOW, true);

    me->SetBotsPet(m_botsPet);

    m_botsPet->SendUpdateToPlayer(master);
}

uint8 bot_minion_ai::GetBotClassForCreature(Creature* bot)
{
    uint8 botClass = bot->GetBotClass();
    switch (botClass)
    {
        case CAT: case BEAR:
            return CLASS_DRUID;
        default:
            return botClass;
    }
}

uint8 bot_pet_ai::GetPetType(Creature* pet)
{
    switch (pet->GetEntry())
    {
        case PET_VOIDWALKER:
            return PET_TYPE_VOIDWALKER;
    }
    return PET_TYPE_NONE;
}

uint8 bot_pet_ai::GetPetClass(Creature* pet)
{
    switch (GetPetType(pet))
    {
        case PET_TYPE_IMP:
            return CLASS_MAGE;
        default:
            return CLASS_PALADIN;
    }
}

uint32 bot_pet_ai::GetPetOriginalEntry(uint32 entry)
{
    switch (entry)
    {
        case PET_VOIDWALKER:
            return ORIGINAL_ENTRY_VOIDWALKER;
        default:
            return 0;
    }
}

void bot_minion_ai::BreakCC(const uint32 diff)
{
    if (pvpTrinket_cd <= diff && CCed(me, true) && (me->getVictim() || !me->getAttackers().empty()))
    {
        temptimer = GC_Timer;
        if (doCast(me, PVPTRINKET))
        {
            pvpTrinket_cd = PVPTRINKET_CD;
            GC_Timer = temptimer;
            return;
        }
    }
}

float bot_ai::InitAttackRange(float origRange, bool ranged) const
{
    if (me->IsMounted())
        origRange *= 0.2f;
    else
    {
        if (ranged)
            origRange *= 1.25f;
        if (master->isDead())
            origRange += sWorld->GetMaxVisibleDistanceOnContinents();
    }
    return origRange;
}

void bot_minion_ai::OnOwnerDamagedBy(Unit* attacker)
{
    if (me->getVictim())
        return;
    if (InDuel(attacker))
        return;
    bool byspell = false, ranged = false;
    switch (GetBotClassForCreature(me))
    {
        case CLASS_DRUID:
            byspell = me->GetShapeshiftForm() == FORM_NONE || 
                me->GetShapeshiftForm() == FORM_TREE || 
                me->GetShapeshiftForm() == FORM_MOONKIN;
            ranged = byspell;
            break;
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_SHAMAN:
            byspell = true;
            ranged = true;
            break;
        case CLASS_HUNTER:
            ranged = true;
            break;
        default:
            break;
    }
    float maxdist = InitAttackRange(float(master->GetBotFollowDist()), ranged);//use increased range
    if (!attacker->IsWithinDist(me, maxdist))
        return;
    if (!CanBotAttack(attacker, byspell))
        return;

    m_botCommandState = COMMAND_ABANDON;//reset AttackStart()
    me->Attack(attacker, !ranged);
}