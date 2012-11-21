#include "bot_ai.h"
/*
Druid NpcBot (reworked by Graff onlysuffering@gmail.com)
Complete - Maybe 30%
TODO: Feral Spells (from scratch), More Forms, Balance Spells + treants...
*/
class druid_bot : public CreatureScript
{
public:
    druid_bot() : CreatureScript("druid_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new bot_druid_ai(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        return bot_minion_ai::OnGossipHello(player, creature);
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {
        if (bot_minion_ai* ai = creature->GetBotMinionAI())
            return ai->OnGossipSelect(player, creature, sender, action);
        return true;
    }

    struct bot_druid_ai : public bot_minion_ai
    {
        bot_druid_ai(Creature* creature) : bot_minion_ai(creature) { }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_DRUID) != SPELL_CAST_OK)
                return false;

            info = sSpellMgr->GetSpellInfo(spellId);
            if (swiftness && info->CalcCastTime() > 0)
            {
                DoCast(victim, spellId, true);
                me->RemoveAurasDueToSpell(NATURES_SWIFTNESS, me->GetGUID(), 0, AURA_REMOVE_BY_EXPIRE);
                me->RemoveAurasDueToSpell(CRIT_50, me->GetGUID(), 0, AURA_REMOVE_BY_EXPIRE);
                swiftness = false;
                return true;
            }
            if (spellId == BEAR_FORM || spellId == CAT_FORM)
            {
                me->ModifyPower(POWER_MANA, - int32(info->CalcPowerCost(me, info->GetSchoolMask())));
                mana = me->GetPower(POWER_MANA);
                if (Unit* u = me->getVictim())
                    GetInPosition(true, false, u);
            }

            bool result = bot_ai::doCast(victim, spellId, triggered);

            if (result && 
                //spellId != BEAR_FORM && spellId != CAT_FORM && 
                spellId != MANAPOTION && spellId != WARSTOMP && 
                me->HasAura(OMEN_OF_CLARITY_BUFF))
            {
                cost = info->CalcPowerCost(me, info->GetSchoolMask());
                clearcast = true;
                power = me->getPowerType();
            }
            return result;
        }

        void EnterCombat(Unit* u) { OnEnterCombat(u); }
        void Aggro(Unit*) { }
        void AttackStart(Unit*) { }
        void KilledUnit(Unit*) { }
        void EnterEvadeMode() { }
        void MoveInLineOfSight(Unit*) { }
        void JustDied(Unit*) { removeFeralForm(true, false); master->SetNpcBotDied(me->GetGUID()); }

        void warstomp(const uint32 diff)
        {
            if (me->getRace() != RACE_TAUREN) return;
            if (Warstomp_Timer > diff) return;
            if (me->GetShapeshiftForm() != FORM_NONE)
                return;

            AttackerSet b_attackers = me->getAttackers();

            if (b_attackers.empty())
            {
                Unit* u = me->SelectNearestTarget(5);
                if (u && u->isInCombat() && u->isTargetableForAttack())
                {
                    if (doCast(me, WARSTOMP))
                    {
                        Warstomp_Timer = 30000; //30sec
                        return;
                    }
                }
            }
            for(AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
            {
                if (!(*iter) || (*iter)->isDead()) continue;
                if (!(*iter)->isTargetableForAttack()) continue;
                if (me->GetDistance((*iter)) <= 5)
                {
                    if (doCast(me, WARSTOMP))
                        Warstomp_Timer = 30000; //30sec
                }
            }
        }

        bool DamagePossible()
        {
            return true;
            //return (GetManaPCT(me) < 30 || GetHealthPCT(master) < 50);
            /*if (GetHealthPCT(master) < 75 || GetHealthPCT(me) < 75) return false;

            //if (master->IsPlayerbot() && master->getAttackers().size() > 2)
            //    return false;

            if (Group* pGroup = master->GetGroup())
            {
                uint8 LHPcount = 0;
                uint8 DIScount = 0;
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* tPlayer = itr->getSource();
                    if (!tPlayer || tPlayer->isDead()) continue;
                    if (me->GetExactDist(tPlayer) > 30) continue;
                    if (tPlayer->GetHealth()*100 / tPlayer->GetMaxHealth() < 75)
                        ++LHPcount;
                    Unit::AuraApplicationMap const& auras = tPlayer->GetAppliedAuras();
                    for (Unit::AuraApplicationMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                        if (itr->second->GetBase()->GetSpellInfo()->Dispel == DISPEL_POISON)
                            ++DIScount;
                }
                uint8 members = master->GetGroup()->GetMembersCount();

                if (members > 10)
                {
                    if (LHPcount > 1 || DIScount > 2) return false;
                }
                if (members > 4)
                {
                    if (LHPcount > 0 || DIScount > 1) return false;
                }
                if (members < 5)
                {
                    if (LHPcount > 0 || DIScount > 0) return false;
                }
            }//endif unitlist

            Unit* u = master->getVictim();
            if (master->getAttackers().size() > 4 || 
              (!master->getAttackers().empty() && 
                u != NULL && u->GetHealth() > me->GetMaxHealth()*17))
                return false;

            return true;*/
        }

        void removeFeralForm(bool force = false, bool init = true, const uint32 diff = 0)
        {
            if (!force && formtimer > diff) return;
            ShapeshiftForm form = me->GetShapeshiftForm();
            if (form != FORM_NONE)
            {
                switch (form)
                {
                case FORM_DIREBEAR:
                case FORM_BEAR:
                    me->RemoveAurasDueToSpell(BEAR_FORM);
                    break;
                case FORM_CAT:
                    me->RemoveAurasDueToSpell(CAT_FORM);
                    me->RemoveAurasDueToSpell(ENERGIZE);
                    break;
                default:
                    break;
                }
                SetStats(CLASS_DRUID, init);
            }
        }

        void StartAttack(Unit* u, bool force = false)
        {
            if (GetBotCommandState() == COMMAND_ATTACK && !force) return;
            Aggro(u); 
            SetBotCommandState(COMMAND_ATTACK);
            GetInPosition(force, me->GetShapeshiftForm() == FORM_NONE);
        }

        void doBearActions(const uint32 diff)
        {
            if (me->getPowerType() != POWER_RAGE) return;

            if (GetHealthPCT(me) < 75)
                HealTarget(me, GetHealthPCT(me), diff);
            opponent = me->getVictim();
            if (opponent)
                StartAttack(opponent, true);
            else
                return;

            //range check (melee) to prevent fake casts
            if (me->GetDistance(opponent) > 5) return;

            if (MangleB_Timer <= diff && rage >= 200 && doCast(opponent, MANGLE_BEAR))
            {
                MangleB_Timer = 6000 - me->getLevel()/4 * 100;
                return;
            }

            if (GC_Timer <= diff && rage >= 200 && doCast(opponent, SWIPE))
                return;

        }//end doBearActions

        void doCatActions(const uint32 diff)
        {
            if (GetHealthPCT(me) < 75)
                HealTarget(me, GetHealthPCT(me), diff);
            opponent = me->getVictim();
            if (opponent)
                StartAttack(opponent, true);
            else
                return;
            uint32 energy = me->GetPower(POWER_ENERGY);

            if (MoveBehind(*opponent))
                wait = 5;
            //{ wait = 5; return; }

            //range check (melee) to prevent fake casts
            if (me->GetDistance(opponent) > 5) return;

            if (Mangle_Cat_Timer <= diff && energy > 45 && doCast(opponent, MANGLE_CAT))
                Mangle_Cat_Timer = 6000;
            if (Rake_Timer <= diff && energy > 40 && doCast(opponent, RAKE))
                Rake_Timer = 10000;
            if (Shred_Timer <= diff && energy > 60 && !opponent->HasInArc(M_PI, me) && doCast(opponent, SHRED))
                Shred_Timer = 12000;
            if (Rip_Timer <= diff && energy > 30 && doCast(opponent, RIP))
                Rip_Timer = 15000;
            if (Claw_Timer <= diff && energy > 45 && doCast(opponent, CLAW))
                Claw_Timer = GC_Timer;
        }//end doCatActions

        void doBalanceActions(const uint32 diff)
        {
            removeFeralForm(true, true);
            opponent = me->getVictim();
            if (opponent)
            {
                if (!IsCasting())
                    StartAttack(opponent);
            }
            else
                return;
            AttackerSet m_attackers = master->getAttackers();
            AttackerSet b_attackers = me->getAttackers();

            //range check (melee) to prevent fake casts
            if (me->GetExactDist(opponent) > 30 || !DamagePossible()) return;

            if (HURRICANE && Hurricane_Timer <= diff && GC_Timer <= diff && Rand() > 35 && !IsCasting())
            {
                Unit* target = FindAOETarget(30, true);
                if (target && doCast(target, HURRICANE))
                {
                    Hurricane_Timer = 5000;
                    return;
                }
                Hurricane_Timer = 2000;//fail
            }
            if (GC_Timer <= diff && !opponent->HasAura(FAIRIE_FIRE))
                if (doCast(opponent, FAIRIE_FIRE))
                    return;
            if (Rand() > 30 && Moonfire_Timer <= diff && GC_Timer <= diff && 
                !opponent->HasAura(MOONFIRE, me->GetGUID()))
                if (doCast(opponent, MOONFIRE))
                {
                    Moonfire_Timer = 5000;
                    return;
                }
            if (Rand() > 30 && Starfire_Timer <= diff && GC_Timer <= diff && 
                doCast(opponent, STARFIRE))
            {
                Starfire_Timer = 11000;
                return;
            }
            if (Rand() > 50 && Wrath_Timer <= diff && GC_Timer <= diff && 
                doCast(opponent, WRATH))
            {
                Wrath_Timer = uint32(sSpellMgr->GetSpellInfo(WRATH)->CalcCastTime()/100 * me->GetFloatValue(UNIT_MOD_CAST_SPEED) + 1);
                return;
            }
        }

        bool MassGroupHeal(Player* gPlayer, const uint32 diff)
        {
            if (!gPlayer || GC_Timer > diff) return false;
            if (!TRANQUILITY && !WILD_GROWTH) return false;
            if (Tranquility_Timer > diff && Wild_Growth_Timer > diff) return false;
            if (Rand() > 30) return false;
            if (IsCasting()) return false; // if I'm already casting
            Group* pGroup = gPlayer->GetGroup();
            if (!pGroup) return false;
            uint8 LHPcount = 0;
            uint8 pct = 100;
            Unit* healTarget = NULL;
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* tPlayer = itr->getSource();
                if (!tPlayer || (tPlayer->isDead() && !tPlayer->HaveBot())) continue;
                if (me->GetExactDist(tPlayer) > 39) continue;
                if (GetHealthPCT(tPlayer) < 80)
                {
                    if (GetHealthPCT(tPlayer) < pct)
                    {
                        pct = GetHealthPCT(tPlayer);
                        healTarget = tPlayer;
                    }
                    ++LHPcount;
                    if (LHPcount > 2) break;
                }
                if (tPlayer->HaveBot())
                {
                    for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
                    {
                        Creature* bot = tPlayer->GetBotMap()[i]._Cre();
                        if (bot && bot->IsInWorld() && bot->GetExactDist(me) < 40 && GetHealthPCT(bot) < 80)
                        {
                            if (GetHealthPCT(bot) < pct)
                            {
                                pct = GetHealthPCT(bot);
                                healTarget = bot;
                            }
                            ++LHPcount;
                            if (LHPcount > 2) break;
                        }
                    }
                }
            }
            if (LHPcount > 2 && TRANQUILITY && Tranquility_Timer <= diff && 
                doCast(me, TRANQUILITY))
                { Tranquility_Timer = 45000; return true; }
            if (LHPcount > 0 && WILD_GROWTH && Wild_Growth_Timer <= diff && 
                doCast(healTarget, WILD_GROWTH))
                { Wild_Growth_Timer = 6000; return true; }
            return false;
        }//end MassGroupHeal

        void UpdateAI(const uint32 diff)
        {
            ReduceCD(diff);
            if ((me->GetShapeshiftForm() == FORM_DIREBEAR || me->GetShapeshiftForm() == FORM_BEAR) && 
                me->getPowerType() != POWER_RAGE)
                me->setPowerType(POWER_RAGE);
            if (me->GetShapeshiftForm() == FORM_CAT && me->getPowerType() != POWER_ENERGY)
                me->setPowerType(POWER_ENERGY);
            if (me->GetShapeshiftForm() == FORM_NONE && me->getPowerType() != POWER_MANA)
                me->setPowerType(POWER_MANA);
            if (IAmDead()) return;
            if (!me->getVictim())
                Evade();
            if (me->GetShapeshiftForm() == FORM_DIREBEAR || me->GetShapeshiftForm() == FORM_BEAR)
            {
                rage = me->GetPower(POWER_RAGE);
                if (ragetimer2 <= diff)
                {
                    if (me->isInCombat() && me->getLevel() >= 30)
                    {
                        if (rage < 990 && rage >= 0)
                            me->SetPower(POWER_RAGE, rage + uint32(10.f*rageIncomeMult));//1 rage per 2 sec
                        else
                            me->SetPower(POWER_RAGE, 1000);//max
                    }
                    ragetimer2 = 2000;
                }
                if (ragetimer <= diff)
                {
                    if (!me->isInCombat())
                    {
                        if (rage > 10.f*rageLossMult)
                            me->SetPower(POWER_RAGE, rage - uint32(10.f*rageLossMult));//-1 rage per 1.5 sec
                        else
                            me->SetPower(POWER_RAGE, 0);//min
                    }
                    ragetimer = 1500;
                    if (rage > 1000) me->SetPower(POWER_RAGE, 1000);
                    if (rage < 10) me->SetPower(POWER_RAGE, 0);
                }
            }
            if (clearcast && me->HasAura(OMEN_OF_CLARITY_BUFF) && !me->IsNonMeleeSpellCasted(false))
            {
                me->ModifyPower(POWER_MANA, cost);
                me->RemoveAurasDueToSpell(OMEN_OF_CLARITY_BUFF,me->GetGUID(),0,AURA_REMOVE_BY_EXPIRE);
                clearcast = false;
            }
            if (wait == 0)
                wait = GetWait();
            else
                return;
            CheckAuras();
            BreakCC(diff);
            if (CCed(me)) return;
            warstomp(diff);

            if (me->getPowerType() == POWER_MANA && GetManaPCT(me) < 20 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, MANAPOTION))
                    Potion_cd = POTION_CD;
                GC_Timer = temptimer;
            }

            //Heal master
            if (GetHealthPCT(master) < 85)
                HealTarget(master, GetHealthPCT(master), diff);
            //Innervate
            if (INNERVATE && Innervate_Timer <= diff && GC_Timer <= diff)
            {
                doInnervate();
                if (Innervate_Timer <= diff)//if failed or not found target
                    Innervate_Timer = 3000;//set delay
            }

            MassGroupHeal(master, diff);
            if (!me->isInCombat())
                DoNonCombatActions(diff);
            else
                CheckBattleRez(diff);
            BuffAndHealGroup(master, diff);
            CureTarget(master, CURE_POISON, diff);
            CureGroup(master, CURE_POISON, diff);

            if (!CheckAttackTarget(CLASS_DRUID))
                return;

            if (GetHealthPCT(me) < 75)
            {
                HealTarget(me, GetHealthPCT(me), diff);
                return;
            }

            if (IsCasting()) return;//Casting heal or something
            CheckRoots(diff);

            if (DamagePossible())
            {
                Unit* u = opponent->getVictim();
                //if the target is attacking us, we want to go bear
                if (BEAR_FORM && !CCed(opponent) && 
                    (u == me || (tank == me && IsInBotParty(u))) || 
                    (!me->getAttackers().empty() && (*me->getAttackers().begin()) == opponent && opponent->GetMaxHealth() > me->GetMaxHealth()*2))
                {
                    //if we don't have bear yet
                    if (me->GetShapeshiftForm() != FORM_DIREBEAR && 
                        me->GetShapeshiftForm() != FORM_BEAR && 
                        formtimer <= diff && 
                        doCast(me, BEAR_FORM))
                    {
                        SetStats(BEAR);
                        formtimer = 1500;
                    }
                    if (me->GetShapeshiftForm() == FORM_DIREBEAR || 
                        me->GetShapeshiftForm() == FORM_BEAR)
                    {
                        doBearActions(diff);
                        ScriptedAI::UpdateAI(diff);
                    }
                }
                else
                if (CAT_FORM && master->getVictim() != opponent && tank && 
                    u == tank && u != me && 
                    opponent->GetMaxHealth() < tank->GetMaxHealth()*3)
                {
                    //if we don't have cat yet
                    if (me->GetShapeshiftForm() != FORM_CAT && formtimer <= diff)
                    {
                        if (doCast(me, CAT_FORM))
                        {
                            SetStats(CAT);
                            formtimer = 1500;
                        }
                    }
                    if (me->GetShapeshiftForm() == FORM_CAT)
                    {
                        doCatActions(diff);
                        ScriptedAI::UpdateAI(diff);
                    }
                }
                else if (tank != me)
                {
                    doBalanceActions(diff);
                }
            }
            else if (tank != me)
            {
                doBalanceActions(diff);
            }
        }

        bool HealTarget(Unit* target, uint8 hp, const uint32 diff)
        {
            if (hp > 95) return false;
            if (!target || target->isDead()) return false;
            if (tank == me && hp > 35) return false;
            if (hp > 50 && me->GetShapeshiftForm() != FORM_NONE) return false;//do not waste heal if in feral or so
            if (Rand() > 50 + 20*target->isInCombat() + 50*master->GetMap()->IsRaid() - 50*me->GetShapeshiftForm()) return false;
            if (me->GetExactDist(target) > 40) return false;

            if ((hp < 15 || (hp < 35 && target->getAttackers().size() > 2)) && 
                Nature_Swiftness_Timer <= diff && (target->isInCombat() || !target->getAttackers().empty()))
            {
                if (me->IsNonMeleeSpellCasted(false))
                    me->InterruptNonMeleeSpells(false);
                if (NATURES_SWIFTNESS && doCast(me, NATURES_SWIFTNESS) && RefreshAura(CRIT_50, 2))//need to be critical
                {
                    swiftness = true;
                    if (doCast(target, HEALING_TOUCH, true))
                    {
                        Nature_Swiftness_Timer = 120000;//2 min
                        Heal_Timer = 3000;
                        return true;
                    }
                }
            }
            if (SWIFTMEND && (hp < 25 || GetLostHP(target) > 5000) && Swiftmend_Timer <= 3000 && 
                (HasAuraName(target, REGROWTH) || HasAuraName(target, REJUVENATION)))
            {
                if (doCast(target, SWIFTMEND))
                {
                    Swiftmend_Timer = 10000;
                    if (GetHealthPCT(target) > 75)
                        return true;
                    else if (!target->getAttackers().empty())
                    {
                        if (doCast(target, REGROWTH))
                        {
                            GC_Timer = 300;
                            return true;
                        }
                    }
                }
            }
            if (hp > 35 && (hp < 75 || GetLostHP(target) > 3000) && Heal_Timer <= diff && NOURISH)
            {
                switch (urand(1,3))
                {
                case 1:
                case 2:
                    if (doCast(target, NOURISH))
                    { Heal_Timer = 3000; return true; }
                    break;
                case 3:
                    if (doCast(target, HEALING_TOUCH))
                    { Heal_Timer = 3000; return true; }
                    break;
                }
            }
            //maintain HoTs
            Unit* u = target->getVictim();
            Creature* boss = u && u->ToCreature() && u->ToCreature()->isWorldBoss() ? u->ToCreature() : NULL;
            bool tanking = tank == target && boss;
            if (( (hp < 80 || GetLostHP(target) > 3500 || tanking) && 
                Regrowth_Timer <= diff && GC_Timer <= diff && !target->HasAura(REGROWTH, me->GetGUID()) )
                || 
                (target->HasAura(REGROWTH, me->GetGUID()) && target->HasAura(REJUVENATION, me->GetGUID()) && 
                (hp < 70 || GetLostHP(target) > 3000) && Regrowth_Timer <= diff && GC_Timer <= diff))
            {
                if (doCast(target, REGROWTH))
                { Regrowth_Timer = 2000; return true; }
            }
            if (hp > 25 && (hp < 90 || GetLostHP(target) > 2000 || tanking) && GC_Timer <= diff && 
                !HasAuraName(target, REJUVENATION, me->GetGUID()))
            {
                if (doCast(target, REJUVENATION))
                {
                    if (!target->getAttackers().empty() && (hp < 75 || GetLostHP(target) > 4000))
                        if (SWIFTMEND && Swiftmend_Timer <= diff && doCast(target, SWIFTMEND))
                            Swiftmend_Timer = 10000;
                    GC_Timer = 500;
                    return true;
                }
            }
            if (LIFEBLOOM != 0 && GC_Timer <= diff && 
                ((hp < 85 && hp > 40) || (hp > 70 && tanking) || 
                (hp < 70 && hp > 25 && HasAuraName(target, REGROWTH) && HasAuraName(target, REJUVENATION)) || 
                (GetLostHP(target) > 1500 && hp > 35)))
            {
                Aura* bloom = target->GetAura(LIFEBLOOM, me->GetGUID());
                if ((!bloom || bloom->GetStackAmount() < 3) && doCast(target, LIFEBLOOM))
                    return true;
            }
            if (hp > 30 && (hp < 70 || GetLostHP(target) > 3000) && Heal_Timer <= diff && 
                doCast(target, HEALING_TOUCH))
            {
                Heal_Timer = 3000;
                return true;
            }
            return false;
        }

        bool BuffTarget(Unit* target, const uint32 diff)
        {
            if (GC_Timer > diff || Rand() > 40) return false;
            if (me->isInCombat() && !master->GetMap()->IsRaid()) return false;
            if (target && target->isAlive() && me->GetExactDist(target) < 30)
            {
                if (!HasAuraName(target, MARK_OF_THE_WILD))
                    if (doCast(target, MARK_OF_THE_WILD))
                        return true;
                if (!HasAuraName(target, THORNS))
                    if (doCast(target, THORNS))
                        return true;
            }
            return false;
        }

        void DoNonCombatActions(const uint32 diff)
        {
            //if eating or drinking don't do anything
            if (GC_Timer > diff || me->IsMounted()) return;

            RezGroup(REVIVE, master);

            if (Feasting()) return;

            if (BuffTarget(master, diff))
            {
                /*GC_Timer = 800;*/
                return;
            }
            if (BuffTarget(me, diff))
            {
                /*GC_Timer = 800;*/
                return;
            }
        }

        void doInnervate(uint8 minmanaval = 30)
        {
            Unit* iTarget = NULL;

            if (master->isInCombat() && GetManaPCT(master) < 20)
                iTarget = master;
            else if (me->isInCombat() && GetManaPCT(me) < 20)
                iTarget = me;

            Group* group = master->GetGroup();
            if (!iTarget && !group)//first check master's bots
            {
                for (uint8 i = 0; i != master->GetMaxNpcBots(); ++i)
                {
                    Creature* bot = master->GetBotMap()[i]._Cre();
                    if (!bot || !bot->isInCombat() || bot->isDead()) continue;
                    if (me->GetExactDist(bot) > 30) continue;
                    if (GetManaPCT(bot) < minmanaval)
                    {
                        iTarget = bot;
                        break;
                    }
                }
            }
            if (!iTarget)//cycle through player members...
            {
                for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* tPlayer = itr->getSource();
                    if (tPlayer == NULL || !tPlayer->isInCombat() || tPlayer->isDead()) continue;
                    if (me->GetExactDist(tPlayer) > 30) continue;
                    if (GetManaPCT(tPlayer) < minmanaval)
                    {
                        iTarget = tPlayer;
                        break;
                    }
                }
            }
            if (!iTarget)//... and their bots.
            {
                for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* tPlayer = itr->getSource();
                    if (tPlayer == NULL || !tPlayer->HaveBot()) continue;
                    for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
                    {
                        Creature* bot = tPlayer->GetBotMap()[i]._Cre();
                        if (!bot || bot->isDead()) continue;
                        if (me->GetExactDist(bot) > 30) continue;
                        if (GetManaPCT(bot) < minmanaval)
                        {
                            iTarget = bot;
                            break;
                        }
                    }
                }
            }
            
            if (iTarget && !iTarget->HasAura(INNERVATE) && doCast(iTarget, INNERVATE))
            {
                if (iTarget->GetTypeId() == TYPEID_PLAYER)
                    me->MonsterWhisper("Innervate on You!", iTarget->GetGUID());
                Innervate_Timer = iTarget->GetTypeId() == TYPEID_PLAYER ? 60000 : 30000;//1 min if player and 30 sec if bot
            }
        }

        void CheckRoots(const uint32 diff)
        {
            if (!ENTANGLING_ROOTS || GC_Timer > diff) return;
            if (me->GetShapeshiftForm() != FORM_NONE) return;
            if (FindAffectedTarget(ENTANGLING_ROOTS, me->GetGUID(), 60)) return;
            if (Unit* target = FindRootTarget(30, ENTANGLING_ROOTS))
                if (doCast(target, ENTANGLING_ROOTS))
                    return;
        }

        void CheckBattleRez(const uint32 diff)
        {
            if (!REBIRTH || Rebirth_Timer > diff || Rand() > 10 || IsCasting() || me->IsMounted()) return;
            Group* gr = master->GetGroup();
            if (!gr)
            {
                Unit* target = master;
                if (master->isAlive()) return;
                if (master->isRessurectRequested()) return; //ressurected
                if (master->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                    target = (Unit*)master->GetCorpse();
                if (me->GetExactDist(target) > 30)
                {
                    me->GetMotionMaster()->MovePoint(master->GetMapId(), *target);
                    Rebirth_Timer = 1500;
                    return;
                }
                else if (!target->IsWithinLOSInMap(me))
                    me->Relocate(*target);

                if (doCast(target, REBIRTH))//rezzing
                {
                    me->MonsterWhisper("Rezzing You", master->GetGUID());
                    Rebirth_Timer = me->getLevel() >= 60 ? 300000 : 600000; //5-10 min (improved possible)
                }
                return;
            }
            for (GroupReference* itr = gr->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* tPlayer = itr->getSource();
                Unit* target = tPlayer;
                if (!tPlayer || tPlayer->isAlive()) continue;
                if (tPlayer->isRessurectRequested()) continue; //ressurected
                if (tPlayer->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                    target = (Unit*)tPlayer->GetCorpse();
                if (master->GetMap() != target->GetMap()) continue;
                if (me->GetExactDist(target) > 30)
                {
                    me->GetMotionMaster()->MovePoint(target->GetMapId(), *target);
                    Rebirth_Timer = 1500;
                    return;
                }
                else if (!target->IsWithinLOSInMap(me))
                    me->Relocate(*target);

                if (doCast(target, REBIRTH))//rezzing
                {
                    me->MonsterWhisper("Rezzing You", tPlayer->GetGUID());
                    Rebirth_Timer = me->getLevel() >= 60 ? 300000 : 600000; //5-10 min (improved possible)
                    return;
                }
            }
        }

        void SetStats(uint8 form, bool init = true)
        {
            switch(form)
            {
            case BEAR:
                me->SetBotClass(BEAR);
                if (me->getPowerType() != POWER_RAGE)
                {
                    me->setPowerType(POWER_RAGE);
                    me->SetMaxPower(POWER_RAGE, 1000);
                }
                if (me->getLevel() >= 15)
                    me->SetPower(POWER_RAGE, 200);
                else
                    me->SetPower(POWER_RAGE, 0);
                if (me->getLevel() >= 40 && !me->HasAura(LEADER_OF_THE_PACK))
                    RefreshAura(LEADER_OF_THE_PACK);
                setStats(BEAR, me->getRace(), master->getLevel());
                break;
            case CAT:
                me->SetBotClass(CAT);
                if (me->getPowerType() != POWER_ENERGY)
                {
                    me->setPowerType(POWER_ENERGY);
                    me->SetMaxPower(POWER_ENERGY, 100);
                    me->SetPower(POWER_ENERGY, 0);
                }
                if (me->getLevel() >= 15)
                    me->SetPower(POWER_ENERGY, 60);
                else
                    me->SetPower(POWER_ENERGY, 0);
                if (me->getLevel() >= 40 && !me->HasAura(LEADER_OF_THE_PACK))
                    RefreshAura(LEADER_OF_THE_PACK);
                RefreshAura(ENERGIZE, me->getLevel()/40 + master->Has310Flyer(false));
                setStats(CAT, me->getRace(), master->getLevel());
                break;
            case CLASS_DRUID:
                me->SetBotClass(CLASS_DRUID);
                if (me->getPowerType() != POWER_MANA)
                    me->setPowerType(POWER_MANA);
                if (init)
                    me->SetPower(POWER_MANA, mana);
                setStats(CLASS_DRUID, me->getRace(), master->getLevel());
                break;
            }
        }

        void SpellHit(Unit* caster, SpellInfo const* spell)
        {
            OnSpellHit(caster, spell);
        }

        void DamageTaken(Unit* u, uint32& /*damage*/)
        {
            OnEnterCombat(u);
            OnOwnerDamagedBy(u);
        }

        void OwnerDamagedBy(Unit* u)
        {
            OnOwnerDamagedBy(u);
        }

        void Reset()
        {
            Heal_Timer = 0;
            Regrowth_Timer = 0;
            Swiftmend_Timer = 0;
            Wild_Growth_Timer = 0;
            Tranquility_Timer = 0;
            Nature_Swiftness_Timer = 0;
            Rebirth_Timer = 0;
            Warstomp_Timer = 0;
            MangleB_Timer = 0;
            Claw_Timer = 0;
            Rake_Timer = 0;
            Shred_Timer = 0;
            Rip_Timer = 0;
            Mangle_Cat_Timer = 0;
            Moonfire_Timer = 0;
            Starfire_Timer = 0;
            Wrath_Timer = 0;
            Hurricane_Timer = 0;
            Innervate_Timer = 0;
            formtimer = 0;
            clearcast = false;
            swiftness = false;
            power = POWER_MANA;
            mana = 0;
            rage = 0;
            rageIncomeMult = sWorld->getRate(RATE_POWER_RAGE_INCOME);
            rageLossMult = sWorld->getRate(RATE_POWER_RAGE_LOSS);
            ragetimer = 0;
            ragetimer2 = 0;
        
            if (master)
            {
                setStats(CLASS_DRUID, me->getRace(), master->getLevel(), true);
                ApplyClassPassives();
                ApplyPassives(CLASS_DRUID);
            }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (MangleB_Timer > diff)               MangleB_Timer -= diff;
            if (Claw_Timer > diff)                  Claw_Timer -= diff;
            if (Rake_Timer > diff)                  Rake_Timer -= diff;
            if (Shred_Timer > diff)                 Shred_Timer -= diff;
            if (Mangle_Cat_Timer > diff)            Mangle_Cat_Timer -= diff;
            if (Moonfire_Timer > diff)              Moonfire_Timer -= diff;
            if (Starfire_Timer > diff)              Starfire_Timer -= diff;
            if (Wrath_Timer > diff)                 Wrath_Timer -= diff;
            if (Hurricane_Timer > diff)             Hurricane_Timer -= diff;
            if (Innervate_Timer > diff)             Innervate_Timer -= diff;
            if (Rip_Timer > diff)                   Rip_Timer -= diff;
            if (Regrowth_Timer > diff)              Regrowth_Timer -= diff;
            if (Heal_Timer > diff)                  Heal_Timer -= diff;
            if (Swiftmend_Timer > diff)             Swiftmend_Timer -= diff;
            if (Wild_Growth_Timer > diff)           Wild_Growth_Timer -= diff;
            if (Nature_Swiftness_Timer > diff)      Nature_Swiftness_Timer -= diff;
            if (Tranquility_Timer > diff)           Tranquility_Timer -= diff;
            if (Rebirth_Timer > diff)               Rebirth_Timer -= diff;
            if (Warstomp_Timer > diff)              Warstomp_Timer -= diff;
            if (formtimer > diff)                   formtimer -= diff;
            if (ragetimer > diff)                   ragetimer -= diff;
            if (ragetimer2 > diff)                  ragetimer2 -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            uint8 lvl = me->getLevel();
            MARK_OF_THE_WILD                        = InitSpell(me, MARK_OF_THE_WILD_1);
            THORNS                                  = InitSpell(me, THORNS_1);
            HEALING_TOUCH                           = InitSpell(me, HEALING_TOUCH_1);
            REGROWTH                                = InitSpell(me, REGROWTH_1);
            REJUVENATION                            = InitSpell(me, REJUVENATION_1);
            LIFEBLOOM                               = InitSpell(me, LIFEBLOOM_1);
            NOURISH                                 = InitSpell(me, NOURISH_1);
     /*tal*/WILD_GROWTH                 = lvl >= 60 ? InitSpell(me, WILD_GROWTH_1) : 0;
     /*tal*/SWIFTMEND                   = lvl >= 40 ? InitSpell(me, SWIFTMEND_1) : 0;
            TRANQUILITY                             = InitSpell(me, TRANQUILITY_1);
            REVIVE                                  = InitSpell(me, REVIVE_1);
            REBIRTH                                 = InitSpell(me, REBIRTH_1);
            BEAR_FORM                               = InitSpell(me, BEAR_FORM_1);
            SWIPE                                   = InitSpell(me, SWIPE_1);
     /*tal*/MANGLE_BEAR                 = lvl >= 50 ? InitSpell(me, MANGLE_BEAR_1) : 0;
            BASH                                    = InitSpell(me, BASH_1);
            CAT_FORM                                = InitSpell(me, CAT_FORM_1);
            CLAW                                    = InitSpell(me, CLAW_1);
            RAKE                                    = InitSpell(me, RAKE_1);
            SHRED                                   = InitSpell(me, SHRED_1);
            RIP                                     = InitSpell(me, RIP_1);
     /*tal*/MANGLE_CAT                  = lvl >= 50 ? InitSpell(me, MANGLE_CAT_1) : 0;
            MOONFIRE                                = InitSpell(me, MOONFIRE_1);
            STARFIRE                                = InitSpell(me, STARFIRE_1);
            WRATH                                   = InitSpell(me, WRATH_1);
            HURRICANE                               = InitSpell(me, HURRICANE_1);
            FAIRIE_FIRE                             = InitSpell(me, FAIRIE_FIRE_1);
            CURE_POISON                             = InitSpell(me, CURE_POISON_1);
            INNERVATE                               = InitSpell(me, INNERVATE_1);
            ENTANGLING_ROOTS                        = InitSpell(me, ENTANGLING_ROOTS_1);
     /*tal*/NATURES_SWIFTNESS           = lvl >= 30 ? InitSpell(me, NATURES_SWIFTNESS_1) : 0;
            WARSTOMP                                = WARSTOMP_1;
        }

        void ApplyClassPassives()
        {
            uint8 level = master->getLevel();
            if (level >= 78)
                RefreshAura(SPELLDMG2, 3); //+18%
            else if (level >= 65)
                RefreshAura(SPELLDMG2, 2); //+12%
            else if (level >= 50)
                RefreshAura(SPELLDMG2); //+6%
            if (level >= 45)
                RefreshAura(NATURAL_PERFECTION3); //4%
            else if (level >= 43)
                RefreshAura(NATURAL_PERFECTION2); //3%
            else if (level >= 41)
                RefreshAura(NATURAL_PERFECTION1); //2%
            if (level >= 50)
                RefreshAura(LIVING_SEED3); //100%
            else if (level >= 48)
                RefreshAura(LIVING_SEED2); //66%
            else if (level >= 46)
                RefreshAura(LIVING_SEED1); //33%
            if (level >= 55)
                RefreshAura(REVITALIZE3, 5); //75% (15%)x5
            else if (level >= 53)
                RefreshAura(REVITALIZE2, 3); //30% (10%)x3
            else if (level >= 51)
                RefreshAura(REVITALIZE1, 3); //15%  (5%)x3
            if (level >= 70)
                RefreshAura(OMEN_OF_CLARITY, 3); //x3
            else if (level >= 40)
                RefreshAura(OMEN_OF_CLARITY, 2); //x2
            else if (level >= 20)
                RefreshAura(OMEN_OF_CLARITY); //x1
            if (level >= 45)
                RefreshAura(GLYPH_SWIFTMEND); //no comsumption
            if (level >= 40)
                RefreshAura(GLYPH_INNERVATE); //no comsumption
            if (level >= 20)
                RefreshAura(NATURESGRACE);
            if (level >= 78)
            {
                RefreshAura(T9_RESTO_P4_BONUS);
                RefreshAura(T8_RESTO_P4_BONUS);
                RefreshAura(T9_BALANCE_P2_BONUS);
                RefreshAura(T10_BALANCE_P2_BONUS);
                RefreshAura(T10_BALANCE_P4_BONUS);
            }
        }

    private:
        uint32
   /*Buffs*/MARK_OF_THE_WILD, THORNS, 
/*Heal/Rez*/HEALING_TOUCH, REGROWTH, REJUVENATION, LIFEBLOOM, NOURISH, WILD_GROWTH, SWIFTMEND, TRANQUILITY, REVIVE, REBIRTH, 
    /*Bear*/BEAR_FORM, SWIPE, MANGLE_BEAR, BASH, 
     /*Cat*/CAT_FORM, CLAW, RAKE, SHRED, RIP, MANGLE_CAT, 
 /*Balance*/MOONFIRE, STARFIRE, WRATH, HURRICANE, FAIRIE_FIRE, 
    /*Misc*/CURE_POISON, INNERVATE, ENTANGLING_ROOTS, NATURES_SWIFTNESS, WARSTOMP;
        //Timers/other
/*Heal*/uint32 Heal_Timer, Regrowth_Timer, Swiftmend_Timer, Wild_Growth_Timer,
/*Heal*/    Tranquility_Timer, Nature_Swiftness_Timer, Rebirth_Timer;
/*Bear*/uint32 MangleB_Timer;
/*Cat*/ uint32 Claw_Timer, Rake_Timer, Shred_Timer, Rip_Timer, Mangle_Cat_Timer;
/*Bal*/ uint32 Moonfire_Timer, Starfire_Timer, Wrath_Timer, Hurricane_Timer, Innervate_Timer;
/*Misc*/uint32 formtimer, ragetimer, ragetimer2, Warstomp_Timer;
/*Chck*/bool clearcast, swiftness;
/*Misc*/Powers power; uint32 mana, rage;
/*Misc*/float rageIncomeMult, rageLossMult;

        enum DruidBaseSpells
        {
            MARK_OF_THE_WILD_1                  = 1126,
            THORNS_1                            = 467,
            HEALING_TOUCH_1                     = 5185,
            REGROWTH_1                          = 8936,
            REJUVENATION_1                      = 774,
            LIFEBLOOM_1                         = 33763,
            NOURISH_1                           = 50464,
     /*tal*/WILD_GROWTH_1                       = 48438,
     /*tal*/SWIFTMEND_1                         = 18562,
            TRANQUILITY_1                       = 740,
            REVIVE_1                            = 50769,
            REBIRTH_1                           = 20484,
            BEAR_FORM_1                         = 5487,
            SWIPE_1                             = 779,
     /*tal*/MANGLE_BEAR_1                       = 33878,
            BASH_1                              = 5211,
            CAT_FORM_1                          = 768,
            CLAW_1                              = 1082,
            RAKE_1                              = 1822,
            SHRED_1                             = 5221,
            RIP_1                               = 1079,
     /*tal*/MANGLE_CAT_1                        = 33876,
            MOONFIRE_1                          = 8921,
            STARFIRE_1                          = 2912,
            WRATH_1                             = 5176,
            HURRICANE_1                         = 16914,
            FAIRIE_FIRE_1                       = 770,
            CURE_POISON_1                       = 8946,
            INNERVATE_1                         = 29166,
            ENTANGLING_ROOTS_1                  = 339,
     /*tal*/NATURES_SWIFTNESS_1                 = 17116,
            WARSTOMP_1                          = 20549,
        };
        enum DruidPassives
        {
        //Talents
            OMEN_OF_CLARITY                     = 16864,//clearcast
            NATURESGRACE                        = 61346,//haste 20% for 3 sec
            NATURAL_PERFECTION1                 = 33881,
            NATURAL_PERFECTION2                 = 33882,
            NATURAL_PERFECTION3                 = 33883,
            LIVING_SEED1                        = 48496,//rank 1
            LIVING_SEED2                        = 48499,//rank 2
            LIVING_SEED3                        = 48500,//rank 3
            REVITALIZE1                         = 48539,//rank 1
            REVITALIZE2                         = 48544,//rank 2
            REVITALIZE3                         = 48545,//rank 3
  /*Talent*/LEADER_OF_THE_PACK                  = 24932,
        //Glyphs
            GLYPH_SWIFTMEND                     = 54824,//no consumption
            GLYPH_INNERVATE                     = 54832,//self regen
        //other
            T9_RESTO_P4_BONUS                   = 67128,//rejuve crits
            T8_RESTO_P4_BONUS                   = 64760,//rejuve init heal 
            T9_BALANCE_P2_BONUS                 = 67125,//moonfire crits
            T10_BALANCE_P2_BONUS                = 70718,//omen of doom (15%)
            T10_BALANCE_P4_BONUS                = 70723,//Languish(DOT)
            SPELLDMG/*Arcane Instability-mage*/ = 15060,//rank3 3% dam/crit
            SPELLDMG2/*Earth and Moon - druid*/ = 48511,//rank3 6% dam
            ENERGIZE                            = 27787,//Rogue Armor Energize (chance: +35 energy on hit)
            CRIT_50                             = 23434,//50% spell crit
        };
        enum DruidSpecial
        {
            //NATURESGRACEBUFF                    = 16886,
            OMEN_OF_CLARITY_BUFF                = 16870,
        };
    };
};

void AddSC_druid_bot()
{
    new druid_bot();
}
