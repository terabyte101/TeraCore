#include "bot_ai.h"
#include "SpellAuraEffects.h"
/*
Warrior NpcBot (reworked by Graff onlysuffering@gmail.com)
Complete - Around 50-55%
TODO: Solve 'DeathWish + Enrage', Thunder Clap, Piercing Howl, Challenging Shout, other tanking stuff
*/
class warrior_bot : public CreatureScript
{
public:
    warrior_bot() : CreatureScript("warrior_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new warrior_botAI(creature);
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

    struct warrior_botAI : public bot_minion_ai
    {
        warrior_botAI(Creature* creature) : bot_minion_ai(creature) { }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_WARRIOR) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId, triggered);
        }

        void UpdateAI(const uint32 diff)
        {
            ReduceCD(diff);
            if (rendTarget)
            {
                //Glyph of Rending: Increases Rend duration by 6 sec.
                if (Unit* u = sObjectAccessor->FindUnit(rendTarget))
                {
                    if (Aura* rend = u->GetAura(REND, me->GetGUID()))
                    {
                        uint32 dur = rend->GetDuration() + 6000;
                        rend->SetDuration(dur);
                        rend->SetMaxDuration(dur);
                    }
                }
                rendTarget = 0;
            }
            if (IAmDead()) return;
            if (me->getVictim())
                DoMeleeAttackIfReady();
            else
                Evade();

            if (ragetimer2 <= diff)
            {
                if (me->isInCombat() && me->getLevel() >= 20)
                {
                    if (getrage() < 990)
                        me->SetPower(POWER_RAGE, rage + uint32(10.f*rageIncomeMult));//1 rage per 2 sec
                    else
                        me->SetPower(POWER_RAGE, 1000);//max
                }
                ragetimer2 = 2000;
            }
            if (ragetimer <= diff)
            {
                if (!me->isInCombat() && !HasAuraName(me, BLOODRAGE_1))
                {
                    if (getrage() > uint32(10.f*rageLossMult))
                        me->SetPower(POWER_RAGE, rage - uint32(10.f*rageLossMult));//-1 rage per 1.5 sec
                    else
                        me->SetPower(POWER_RAGE, 0);//min
                }
                ragetimer = 1500;
                //if (getrage() > 1000) me->SetPower(POWER_RAGE, 1000);
                //if (getrage() < 10) me->SetPower(POWER_RAGE, 0);
            }

            if (wait == 0)
                wait = GetWait();
            else
                return;
            if (checkAurasTimer == 0)
            {
                SS = SWEEPING_STRIKES && me->HasAura(SWEEPING_STRIKES);
                CheckAuras();
            }
            BreakCC(diff);
            if (CCed(me)) return;

            if (GetHealthPCT(me) < 67 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, HEALINGPOTION))
                {
                    Potion_cd = POTION_CD;
                    GC_Timer = temptimer;
                }
            }
            CheckIntervene(diff);
            if (!me->isInCombat())
                DoNonCombatActions(diff);

            if (!CheckAttackTarget(CLASS_WARRIOR))
            {
                if (tank != me && !me->isInCombat() && battleStance != true && master->getAttackers().empty() && stancetimer <= diff)
                    stanceChange(diff, 1);
                return;
            }

            if (Rand() < 30 && battleShout_cd <= diff && GC_Timer <= diff && getrage() > 100 && 
                !HasAuraName(master, BATTLESHOUT) && 
                !HasAuraName(master, "Blessing of Might") && 
                !HasAuraName(master, "Greater Blessing of Might") && 
                master->IsWithinLOSInMap(me) && 
                doCast(me, BATTLESHOUT))
                battleShout_cd = BATTLESHOUT_CD;

            if (Rand() < 20 && BLOODRAGE && bloodrage_cd <= diff && me->isInCombat() && getrage() < 500 && 
                !me->HasAura(ENRAGED_REGENERATION))
            {
                temptimer = GC_Timer;
                if (doCast(me, BLOODRAGE))
                {
                    bloodrage_cd = BLOODRAGE_CD;
                    GC_Timer = temptimer;
                }
            }

            Attack(diff);
        }

        void StartAttack(Unit* u, bool force = false)
        {
            if (GetBotCommandState() == (COMMAND_ATTACK) && !force) return;
            Aggro(u);
            SetBotCommandState(COMMAND_ATTACK);
            GetInPosition(force, false);
        }

        void EnterCombat(Unit* u) { OnEnterCombat(u); }
        void Aggro(Unit*) { }
        void AttackStart(Unit*) { }
        void KilledUnit(Unit*) { }
        void EnterEvadeMode() { }
        void MoveInLineOfSight(Unit*) { }
        void JustDied(Unit*) { master->SetNpcBotDied(me->GetGUID()); }
        void DoNonCombatActions(const uint32 /*diff*/)
        {}

        void modrage(int32 mod, bool set = false)
        {
            if (set && mod < 0)
                return;
            if (mod < 0 && rage < uint32(abs(mod)))
                return;

            if (set)
                rage = mod*10;
            else
                rage += mod*10;
            me->SetPower(POWER_RAGE, rage);
        }

        uint32 getrage()
        {
            rage = me->GetPower(POWER_RAGE);
            return rage;
        }

        void BreakCC(const uint32 diff)
        {
            if (me->HasAuraWithMechanic((1<<MECHANIC_FEAR)|(1<<MECHANIC_SAPPED)|(1<<MECHANIC_DISORIENTED)))
            {
                if (BERSERKERRAGE && !me->HasAura(ENRAGED_REGENERATION) && 
                    berserkerRage_cd <= diff && GC_Timer <= diff && doCast(me, BERSERKERRAGE))
                {
                    berserkerRage_cd = BERSERKERRAGE_CD;
                    if (me->getLevel() >= 40)
                        modrage(20);
                    return;
                }
            }
            bot_minion_ai::BreakCC(diff);
        }

        void Attack(const uint32 diff)
        {
            opponent = me->getVictim();
            if (opponent)
            {
                if (!IsCasting())
                    StartAttack(opponent, true);
            }
            else
                return;
            //Keep stance in raid if tank
            if (master->GetBotTankGuid() == me->GetGUID() && 
                defensiveStance != true && stancetimer <= diff/* && 
                (!master->GetGroup() || master->GetGroup()->isRaidGroup())*/)
                stanceChange(diff, 2);
            //SelfHeal
            if (ENRAGED_REGENERATION)
            {
                if (Rand() < 20 && GetHealthPCT(me) < 40 && getrage() > 150 && regen_cd <= diff && 
                    me->HasAuraWithMechanic(MECHANIC_ENRAGED))//no GC_Timer
                {
                    temptimer = 0;
                    if (doCast(me, ENRAGED_REGENERATION))
                    {
                        regen_cd = ENRAGED_REGENERATION_CD;
                        GC_Timer = temptimer;
                        return;
                    }
                }
                //maybe not needed part
                if (me->HasAura(ENRAGED_REGENERATION))
                {
                    if (HasAuraName(me, "Enrage"))
                        me->RemoveAurasWithMechanic(MECHANIC_ENRAGED);
                    if (HasAuraName(me, BLOODRAGE))
                        me->RemoveAurasDueToSpell(BLOODRAGE);
                    if (HasAuraName(me, DEATHWISH))
                        me->RemoveAurasDueToSpell(DEATHWISH);
                    if (HasAuraName(me, BERSERKERRAGE))
                        me->RemoveAurasDueToSpell(BERSERKERRAGE);
                }
            }//end SelfHeal

            AttackerSet m_attackers = master->getAttackers();
            AttackerSet b_attackers = me->getAttackers();
            float dist = me->GetExactDist(opponent);
            float meleedist = me->GetDistance(opponent);
            //charge + warbringer
            if (CHARGE && charge_cd <= diff && dist > 11 && dist < 25 && me->HasInArc(M_PI, opponent) && 
                (me->getLevel() >= 50 || 
                (!me->isInCombat() && (battleStance == true || stanceChange(diff, 1)))))
            {
                temptimer = GC_Timer;
                if (me->getLevel() >= 50)
                    me->RemoveMovementImpairingAuras();
                if (doCast(opponent, CHARGE, me->isInCombat()))
                {
                    charge_cd = CHARGE_CD;
                    GC_Timer = temptimer;
                    return;
                }
            }
            //intercept
            if (INTERCEPT && intercept_cd <= diff && 
                getrage() > 100 && dist > 11 && dist < 25 && me->HasInArc(M_PI, opponent) && 
                !CCed(opponent) && (berserkerStance == true || stanceChange(diff, 3)))
            {
                if (doCast(opponent, INTERCEPT))
                {
                    intercept_cd = INTERCEPT_CD;
                    //modrage(-10);
                    return;
                }
            }
            //FEAR
            if (Rand() < 70 && INTIMIDATING_SHOUT && getrage() > 250 && intimidatingShout_cd <= diff && GC_Timer <= diff)
            {
                if (opponent->IsNonMeleeSpellCasted(false, false, true) && dist <= 8 && 
                    !(opponent->ToCreature() && opponent->ToCreature()->GetCreatureType() == CREATURE_TYPE_UNDEAD))
                {
                    if (doCast(opponent, INTIMIDATING_SHOUT))
                    {
                        intimidatingShout_cd = INTIMIDATINGSHOUT_CD;
                        return;
                    }
                }
                Unit* fearTarget = NULL;
                bool triggered = false;
                uint8 tCount = 0;
                //fear master's attackers
                if (!m_attackers.empty() && 
                    ((master->getClass() != CLASS_DEATH_KNIGHT && 
                    master->getClass() != CLASS_WARRIOR && 
                    master->getClass() != CLASS_PALADIN) || 
                    GetHealthPCT(master) < 70))
                {
                    for(AttackerSet::iterator iter = m_attackers.begin(); iter != m_attackers.end(); ++iter)
                    {
                        if (!(*iter)) continue;
                        if ((*iter)->GetCreatureType() == CREATURE_TYPE_UNDEAD) continue;
                        if (me->GetExactDist((*iter)) <= 8 && (*iter)->isTargetableForAttack())
                        {
                            ++tCount;
                            fearTarget = (*iter);
                            if (tCount > 1) break;
                        }
                    }
                    if (tCount > 0 && !fearTarget)
                    {
                        fearTarget = opponent;
                        triggered = true;
                    }
                    if (tCount > 1 && doCast(fearTarget, INTIMIDATING_SHOUT, triggered))
                    {
                        intimidatingShout_cd = INTIMIDATINGSHOUT_CD;
                        if (triggered)
                            modrage(-25);
                        return;
                    }
                }
                //Defend myself
                if (b_attackers.size() > 1)
                {
                    tCount = 0;
                    fearTarget = NULL;
                    triggered = false;
                    for(AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
                    {
                        if (!(*iter)) continue;
                        if ((*iter)->GetCreatureType() == CREATURE_TYPE_UNDEAD) continue;
                        if (me->GetExactDist((*iter)) <= 8 && (*iter)->isTargetableForAttack())
                        {
                            ++tCount;
                            fearTarget = (*iter);
                            if (tCount > 0) break;
                        }
                    }
                    if (tCount > 0 && !fearTarget)
                    {
                        fearTarget = opponent;
                        triggered = true;
                    }
                    if (tCount > 0 && doCast(fearTarget, INTIMIDATING_SHOUT, triggered))
                    {
                        intimidatingShout_cd = INTIMIDATINGSHOUT_CD;
                        if (triggered)
                            modrage(-25);
                        return;
                    }
                }
            }//end FEAR
            //OVERPOWER
            if (OVERPOWER && getrage() > 50 && meleedist <= 5 && GC_Timer <= diff && (battleStance == true || stancetimer <= diff))
            {
                bool blood = me->HasAura(TASTE_FOR_BLOOD_BUFF);
                if ((((opponent->GetTypeId() == TYPEID_PLAYER && UNRELENTING_ASSAULT && blood && opponent->IsNonMeleeSpellCasted(false) && overpower_cd <= 3000) || 
                    (opponent->IsNonMeleeSpellCasted(false) && blood) || 
                    (overpower_cd <= diff && blood))))
                {
                    if (battleStance == true || stanceChange(diff, 1))
                    {
                        if (doCast(opponent, OVERPOWER))
                        {
                            overpower_cd = 5000;
                            if (blood)
                                me->RemoveAura(TASTE_FOR_BLOOD_BUFF);
                            return;
                        }
                    }
                }
            }

            if (MoveBehind(*opponent))
                wait = 5;
            //{ wait = 5; return; }
            //HAMSTRING
            if (HAMSTRING && Rand() < 50 && getrage() > 100 && GC_Timer <= diff && meleedist <= 5 && opponent->isMoving() && 
                (battleStance == true || berserkerStance == true || stancetimer <= diff) && 
                !opponent->HasAuraWithMechanic((1<<MECHANIC_SNARE)|(1<<MECHANIC_ROOT)))
            {
                if (battleStance == true || berserkerStance == true || stanceChange(diff, 5))
                    if (doCast(opponent, HAMSTRING))
                        return;
            }
            //UBERS
            //Dont use RETA unless capable circumstances
            if (Rand() < 20 && uber_cd <= diff && GC_Timer <= diff)//mod here
            {
                if (RETALIATION && b_attackers.size() > 4 && 
                    (battleStance == true || stanceChange(diff, 1)))
                {
                    if (doCast(me, RETALIATION))
                    {
                        uber_cd = UBER_CD;
                        return;
                    }
                }
                //Dont use RECKL unless capable circumstances
                if (RECKLESSNESS && tank != me && 
                    (m_attackers.size() > 3 || opponent->GetHealth() > me->GetHealth()*10) && 
                    (berserkerStance == true || stanceChange(diff, 3)))
                {
                    if (doCast(me, RECKLESSNESS))
                    {
                        uber_cd = UBER_CD;
                        return;
                    }
                }
            }
            //DEATHWISH
            if (DEATHWISH && Rand() < 20 && deathwish_cd <= diff && GC_Timer <= diff && 
                getrage() > 100 && opponent->GetHealth() > me->GetHealth()/2 && 
                !me->HasAura(ENRAGED_REGENERATION))//mod here
            {
                if (doCast(me, DEATHWISH))
                {
                    //modrage(-10);
                    deathwish_cd = DEATHWISH_CD;
                    return;
                }
            }
            //TAUNT
            Unit* u = opponent->getVictim();
            if (TAUNT && taunt_cd <= diff && u && u != me && u != tank && dist <= 30 &&
                (IsInBotParty(u) || tank == me) && !CCed(opponent) && 
                (defensiveStance == true || (stancetimer <= diff && stanceChange(diff, 2))))//No GCD
            {
                temptimer = GC_Timer;
                if (doCast(opponent, TAUNT, true))
                {
                    taunt_cd = TAUNT_CD;
                    GC_Timer = temptimer;
                    return;
                }
            }
            //EXECUTE
            if (EXECUTE && tank != me && Rand() < 70 && GC_Timer <= diff && getrage() > 150 && meleedist <= 5 && GetHealthPCT(opponent) < 20 && 
                (battleStance == true || berserkerStance == true || stancetimer <= diff))
            {
                if ((battleStance == true || berserkerStance == true || stanceChange(diff, 5)) && 
                    doCast(opponent, EXECUTE))
                {
                    //sudden death
                    if (me->getLevel() >= 50 && getrage() <= 400)
                        modrage(10, true);
                    else if (getrage() > 300)
                        modrage(-30);
                    else
                        modrage(0, true);
                    return;
                }
            }
            //SUNDER
            if (SUNDER && Rand() < 35 && meleedist <= 5 && tank == me && 
                opponent->GetHealth() > me->GetMaxHealth() && 
                GC_Timer <= diff && getrage() > 150 && (sunder_cd <= diff || getrage() > 500))
            {
                Aura* sunder = opponent->GetAura(SUNDER, me->GetGUID());
                if ((!sunder || sunder->GetStackAmount() < 5 || sunder->GetDuration() < 15000) && 
                    doCast(opponent, SUNDER))
                {
                    sunder_cd = SUNDER_CD;
                    GC_Timer = 1000;
                    return;
                }
            }
            //SS
            if (SWEEPING_STRIKES && sweeping_strikes_cd <= diff && tank != me && Rand() < 20 && 
                (battleStance == true || berserkerStance == true || stancetimer <= diff) && 
                (b_attackers.size() > 1 || FindSplashTarget(7, opponent)))
            {
                temptimer = GC_Timer;
                if ((battleStance == true || berserkerStance == true || stanceChange(diff, 5)) && 
                    doCast(me, SWEEPING_STRIKES, true))//no rage use as with glyph
                {
                    SS = true;
                    sweeping_strikes_cd = SWEEPING_STRIKES_CD;
                    GC_Timer = temptimer;
                    return;
                }
            }
            //WHIRLWIND
            if (WHIRLWIND && Rand() < 50 && whirlwind_cd <= diff && GC_Timer <= diff && getrage() >= 250 && 
               (FindSplashTarget(7, opponent) || (getrage() > 500 && dist <= 7)) && 
               (berserkerStance == true || stancetimer <= diff))
            {
                if ((berserkerStance == true || stanceChange(diff, 3)) && 
                    doCast(me, WHIRLWIND))
                {
                    whirlwind_cd = WHIRLWIND_CD;
                    return;
                }
            }
            //BLADESTORM
            if (BLADESTORM && bladestorm_cd <= diff && GC_Timer <= diff && 
               getrage() >= 250 && (Rand() < 20 || me->HasAura(RECKLESSNESS)) && 
               (b_attackers.size() > 1 || opponent->GetHealth() > me->GetMaxHealth()))
            {
                if (doCast(me, BLADESTORM))
                {
                    bladestorm_cd = BLADESTORM_CD;
                    return;
                }
            }
            //Mortal Strike
            if (MORTALSTRIKE && getrage() > 300 && meleedist <= 5 && mortalStrike_cd <= diff && GC_Timer <= diff)
            {
                if (doCast(opponent, MORTALSTRIKE/*, true*/))
                {
                    mortalStrike_cd = MORTALSTRIKE_CD;
                    slam_cd = 0;//reset here
                }
            }
            //Slam
            bool triggered = mortalStrike_cd == 7000;
            if (SLAM && Rand() < (30 + triggered*60) && slam_cd <= diff && getrage() > 150 && meleedist <= 5)
            {
                if (doCast(opponent, SLAM, triggered))
                {
                    slam_cd = 4500;//4.5sec (must be > MORTALSTRIKE_CD/2)
                    if (triggered)
                        modrage(-15);
                    return;
                }
            }
            //PUMMEL
            if (PUMMEL && Rand() < 80 && pummel_cd <= diff && getrage() > 100 && meleedist <= 5 && 
                opponent->IsNonMeleeSpellCasted(false) && 
                (berserkerStance == true || stancetimer <= diff))
            {
                temptimer = GC_Timer;
                if ((berserkerStance == true || stanceChange(diff, 3)) && 
                    doCast(opponent, PUMMEL))
                {
                    pummel_cd = PUMMEL_CD;
                    GC_Timer = temptimer;
                    return;
                }
            }
            //REND
            if (REND && Rand() < 30 && getrage() > 100 && GC_Timer <= diff && meleedist <= 5 && 
                opponent->GetHealth() > me->GetHealth()/2 && 
                (battleStance == true || defensiveStance == true || stancetimer <= diff) && 
                !opponent->HasAura(REND, me->GetGUID()))
            {
                if ((battleStance == true || defensiveStance == true || stanceChange(diff, 1)) && 
                    doCast(opponent, REND))
                {
                    rendTarget = opponent->GetGUID();
                    return;
                }
            }
            //Cleave
            if (Rand() < 30 && CLEAVE && cleave_cd <= diff && me->getLevel() >= 20 && getrage() > 200 && meleedist <= 5)//noGC_Timer
            {
                temptimer = GC_Timer;
                u = FindSplashTarget(5);
                if (u && doCast(opponent, CLEAVE))
                {
                    rage -= 200;//not visible
                    cleave_cd = me->getAttackTimer(BASE_ATTACK);//once per swing, prevents rage loss
                    GC_Timer = temptimer;
                    return;
                }
            }
            else {}//HEROIC STRIKE placeholder
            //DISARM DEPRECATED
            /*if (disarm_cd <= diff && meleedist < 5 &&
                (opponent->getVictim()->GetGUID() == master->GetGUID() || 
                opponent->getVictim()->GetGUID() == m_creature->GetGUID()) &&
                getrage() > 15 &&
                !HasAuraName(opponent, GetSpellName(DISARM)) &&
                GC_Timer <= diff)
            {
                if (opponent->getClass() == CLASS_ROGUE  ||
                    opponent->getClass() == CLASS_WARRIOR   ||
                    opponent->getClass() == CLASS_SHAMAN    ||
                    opponent->getClass() == CLASS_PALADIN)
                {
                    if (defensiveStance == true)
                    {
                        doCast(opponent, DISARM, true);
                        //rage -= 100;
                        disarm_cd = DISARM_CD;
                    }
                    else stanceChange(diff, 2);
                }
            }*/
        }//end Attack

        void CheckIntervene(const uint32 diff)
        {
            if (INTERVENE && intervene_cd <= diff && getrage() > 100 && Rand() < ((tank == me) ? 80 : 30) && 
                (defensiveStance == true || (stancetimer <= diff && !SS)))
            {
                if (!master->isInCombat() && master->getAttackers().empty() && master->isMoving())
                {
                    float mydist = me->GetExactDist(master);
                    if (mydist < 25 && mydist > 18 && (defensiveStance == true || stanceChange(diff, 2)))
                    {
                        temptimer = GC_Timer;
                        if (doCast(master, INTERVENE))
                        {
                            //modrage(-10);
                            intervene_cd = INTERVENE_CD;
                            GC_Timer = temptimer;
                            Follow(true);
                            return;
                        }
                    }
                }
                //sLog->outBasic("%s: Intervene check.", me->GetName().c_str());
                Group* gr = master->GetGroup();
                if (!gr)
                {
                    if (GetHealthPCT(master) < 95 && !master->getAttackers().empty() && 
                        me->getAttackers().size() <= master->getAttackers().size())
                    {
                        float dist = me->GetExactDist(master);
                        if (dist > 25 || dist < 10) return;
                        if (!(defensiveStance == true || stanceChange(diff, 2))) return;
                        temptimer = GC_Timer;
                        if (doCast(master, INTERVENE))
                        {
                            //modrage(-10);
                            intervene_cd = INTERVENE_CD;
                            GC_Timer = temptimer;
                            return;
                        }
                    }
                }
                else
                {
                    bool Bots = false;
                    float dist;
                    for (GroupReference* itr = gr->GetFirstMember(); itr != NULL; itr = itr->next())
                    {
                        Player* tPlayer = itr->getSource();
                        if (!tPlayer) continue;
                        if (tPlayer->GetMap() != me->GetMap()) continue;
                        //sLog->outBasic("checking player %s", tPlayer->GetName().c_str());
                        if (tPlayer->HaveBot())
                            Bots = true;
                        if (tPlayer->isDead() || GetHealthPCT(tPlayer) > 90 || tank == tPlayer) continue;
                        //sLog->outBasic("alive and health < 80%");
                        if (tPlayer->getAttackers().size() < me->getAttackers().size()) continue;
                        //sLog->outBasic("attackers checked");
                        dist = me->GetExactDist(tPlayer);
                        if (dist > 25 || dist < 10) continue;
                        //sLog->outBasic("and whithin reach");
                        if ((defensiveStance == true || stanceChange(diff, 2)))
                        {
                            //sLog->outBasic("defensive stance acuired, attempt cast");
                            temptimer = GC_Timer;
                            if (doCast(tPlayer, INTERVENE))
                            {
                                //sLog->outBasic("cast succeed");
                                //modrage(-10);
                                intervene_cd = INTERVENE_CD;
                                GC_Timer = temptimer;
                                return;
                            }
                        }
                    }
                    if (!Bots) return;
                    for (GroupReference* itr = gr->GetFirstMember(); itr != NULL; itr = itr->next())
                    {
                        Player* tPlayer = itr->getSource();
                        if (!tPlayer || !tPlayer->HaveBot()) continue;
                        if (tPlayer->GetMap() != me->GetMap()) continue;
                        for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
                        {
                            Creature* bot = tPlayer->GetBotMap()[i]._Cre();
                            if (!bot || bot == me || bot->isDead()) continue;
                            if (GetHealthPCT(bot) > 90 || tank == bot) continue;
                            dist = me->GetExactDist(bot);
                            if (dist > 25 || dist < 10) continue;
                            if (bot->getAttackers().size() <= me->getAttackers().size()) continue;
                            if ((defensiveStance == true || stanceChange(diff, 2)))
                            {
                                //sLog->outBasic("defensive stance acuired, attempt cast");
                                temptimer = GC_Timer;
                                if (doCast(bot, INTERVENE))
                                {
                                    //sLog->outBasic("cast succeed");
                                    //modrage(-10);
                                    intervene_cd = INTERVENE_CD/2; //half for bot
                                    GC_Timer = temptimer;
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }

        void SpellHitTarget(Unit* target, SpellInfo const* spell)
        {
            switch (spell->Id)
            {
            case OVERPOWER_1:
                if (target->GetTypeId() != TYPEID_UNIT || //only creatures lol
                    !UNRELENTING_ASSAULT)
                    return;
                if (target->HasUnitState(UNIT_STATE_CASTING))
                {
                    uint32 spell = 0;
                    if (me->HasAura(UNRELENTING_ASSAULT2))
                        spell = UNRELENTING_ASSAULT_SPELL2;
                    else if (me->HasAura(UNRELENTING_ASSAULT1))
                        spell = UNRELENTING_ASSAULT_SPELL1;
                    if (!spell)
                        return;
                    target->CastSpell(target, spell, true);
                }
                break;
            default:
                break;
            }
        }

        bool stanceChange(const uint32 diff, uint8 stance)
        {
            if (!(stancetimer <= diff) || !stance) return false;

            if (stance == 5)
            {
                switch (urand(0,1))
                {
                case 0: stance = 1; break;
                case 1: if (me->getLevel() < 30) stance = 1; else stance = 3; break;
                }
            }
            if (stance == 2 && (me->getLevel() < 10 || SS)) return false;
            if (stance == 3 && me->getLevel() < 30) return false;

            temptimer = GC_Timer;
            uint32 temprage = 0;
            uint32 myrage = me->GetPower(POWER_RAGE);
            if (me->getLevel() >= 15)
                temprage = myrage > 250 ? 250 : myrage;
            else if (me->getLevel() >= 10)
                temprage = myrage > 100 ? 100 : myrage;
            switch(stance)
            {
            case 1:
                if (doCast(me, BATTLESTANCE))
                {
                    if (me->HasAura(BATTLESTANCE)) 
                    {
                        battleStance = true;
                        defensiveStance = false;
                        berserkerStance = false;
                        me->SetPower(POWER_RAGE, temprage);
                        stancetimer = 2100 - me->getLevel()*20;//2100-1600 on 80
                        GC_Timer = temptimer;
                        return true;
                    }
                }
                break;
            case 2:
                if (doCast(me, DEFENSIVESTANCE))
                {
                    if (me->HasAura(DEFENSIVESTANCE))
                    {
                        defensiveStance = true;
                        battleStance = false;
                        berserkerStance = false;
                        me->SetPower(POWER_RAGE, temprage);
                        stancetimer = 2100 - me->getLevel()*20;//2100-1600 on 80
                        GC_Timer = temptimer;
                        return true;
                    }
                }
                break;
            case 3:
                if (doCast(me, BERSERKERSTANCE))
                {
                    if (me->HasAura(BERSERKERSTANCE))
                    {
                        berserkerStance = true;
                        battleStance = false;
                        defensiveStance = false;
                        me->SetPower(POWER_RAGE, temprage);
                        stancetimer = 2100 - me->getLevel()*20;//2100-1600 on 80
                        GC_Timer = temptimer;
                        return true;
                    }
                }
                break;
            default:
                break;
            }
            GC_Timer = temptimer;
            return false;
        }

        void ApplyClassDamageMultiplierMelee(int32& damage, SpellNonMeleeDamage& /*damageinfo*/, SpellInfo const* spellInfo, WeaponAttackType /*attackType*/, bool& crit) const
        {
            uint32 spellId = spellInfo->Id;
            uint8 lvl = me->getLevel();
            float fdamage = float(damage);
            //1) apply additional crit chance. This additional chance roll will replace original (balance safe)
            if (!crit)
            {
                float aftercrit = 0.f;
                //Incite: 15% additional critical chance for Cleave, Heroic Strike and Thunder Clap
                if (lvl >= 15 && spellId == CLEAVE /*|| spellId == HEROICSTRIKE || spellId == THUNDERCLAP*/)
                    aftercrit += 15.f;
                //Improved Overpower: 50% additional critical chance for Overpower
                if (lvl >= 20 && spellId == OVERPOWER)
                    aftercrit += 50.f;

                //second roll (may be illogical)
                if (aftercrit > 0.f)
                    crit = roll_chance_f(aftercrit);
            }

            //2) apply bonus damage mods
            float pctbonus = 0.0f;
            if (crit)
            {
                //!!!Melee spell damage is not yet critical, all reduced by half
                //Impale: 20% crit damage bonus for all abilities
                if (lvl >= 20)
                    pctbonus += 0.10f;
            }
            //Improved Rend: 20% bonus damage for Rend
            if (spellId == REND)
                pctbonus += 0.2f;
            //Improved Whirlwind: 20% bonus damage for Whirlwind
            if (lvl >= 40 && spellId == WHIRLWIND)
                pctbonus += 0.2f;
            //Glyph of Mortal Strike: 10% bonus damage for Mortal Strike
            if (lvl >= 40 && spellId == MORTALSTRIKE)
                pctbonus += 0.1f;
            //Unrelenting Assault (part 2): 20% bonus damage for Overpower and Revenge
            if (lvl >= 45 && (spellId == OVERPOWER/* || spellId == REVENGE*/))
                pctbonus += 0.2f;
            //Improved Mortal Strike: 10% bonus damage for Mortal Strike
            if (lvl >= 45 && spellId == MORTALSTRIKE)
                pctbonus += 0.1f;
            //Undending Fury: 10% bonus damage for Whirlwind, Slam and Bloodthirst
            if (lvl >= 55 && (spellId == WHIRLWIND || spellId == SLAM /*|| spellId == BLOODTHIRST*/))
                pctbonus += 0.1f;

            damage = int32(fdamage * (1.0f + pctbonus));
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
            slam_cd = 0;
            regen_cd = 20000;
            sweeping_strikes_cd = 0;
            charge_cd = 0;
            deathwish_cd = 0;
            mortalStrike_cd = 0;
            overpower_cd = 0;
            uber_cd = 0;
            berserkerRage_cd = 0;
            battleShout_cd = 0;
            intercept_cd = 0;
            intimidatingShout_cd = 0;
            pummel_cd = 0;
            whirlwind_cd = 5000;
            cleave_cd = 0;
            bladestorm_cd = 10000;
            bloodrage_cd = 0;
            intervene_cd = 0;
            taunt_cd = 0;
            sunder_cd = 0;
            stancetimer = 0;
            ragetimer = 1500;
            ragetimer2 = 3000;

            rendTarget = 0;

            battleStance = true;
            defensiveStance = false;
            berserkerStance = false;

            rageIncomeMult = sWorld->getRate(RATE_POWER_RAGE_INCOME);
            rageLossMult = sWorld->getRate(RATE_POWER_RAGE_LOSS);
            me->setPowerType(POWER_RAGE);
            rage = 0;

            if (master)
            {
                setStats(CLASS_WARRIOR, me->getRace(), master->getLevel(), true);
                ApplyClassPassives();
                ApplyPassives(CLASS_WARRIOR);
                //mob generates abnormal amounts rage so increase/reduce rate with level(from 188% down to 30% at level 80)//not seems to work
                //for (int8 i = 0; i < 3; ++i)
                //    me->ApplyEffectModifiers(sSpellMgr->GetSpellInfo(29623), i, float(90 - master->getLevel()*2));
            }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (regen_cd > diff)                    regen_cd -= diff;
            if (slam_cd > diff)                     slam_cd -= diff;
            if (battleShout_cd > diff)              battleShout_cd -= diff;
            if (sweeping_strikes_cd > diff)         sweeping_strikes_cd -= diff;
            if (deathwish_cd > diff)                deathwish_cd -= diff;
            if (mortalStrike_cd > diff)             mortalStrike_cd -= diff;
            if (overpower_cd > diff)                overpower_cd -= diff;
            if (uber_cd > diff)                     uber_cd -= diff;
            if (berserkerRage_cd > diff)            berserkerRage_cd -= diff;
            if (charge_cd > diff)                   charge_cd -= diff;
            if (intercept_cd > diff)                intercept_cd -= diff;
            if (intimidatingShout_cd > diff)        intimidatingShout_cd -= diff;
            if (pummel_cd > diff)                   pummel_cd -= diff;
            if (whirlwind_cd > diff)                whirlwind_cd -= diff;
            if (bladestorm_cd > diff)               bladestorm_cd -= diff;
            if (cleave_cd > diff)                   cleave_cd -= diff;
            if (bloodrage_cd > diff)                bloodrage_cd -= diff;
            if (intervene_cd > diff)                intervene_cd -= diff;
            if (taunt_cd > diff)                    taunt_cd -= diff;
            if (sunder_cd > diff)                   sunder_cd -= diff;

            if (stancetimer > diff)                 stancetimer -= diff;
            if (ragetimer > diff)                   ragetimer -= diff;
            if (ragetimer2 > diff)                  ragetimer2 -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            uint8 lvl = me->getLevel();
            //CHALLENGING_SHOUT                       = InitSpell(me, CHALLENGING_SHOUT_1);
            INTIMIDATING_SHOUT                      = InitSpell(me, INTIMIDATING_SHOUT_1);
            ENRAGED_REGENERATION                    = InitSpell(me, ENRAGED_REGENERATION_1);
            CHARGE                                  = InitSpell(me, CHARGE_1);
            OVERPOWER                               = InitSpell(me, OVERPOWER_1);
   /*Quest*/TAUNT                       = lvl >= 10 ? TAUNT_1 : 0;
            //DISARM                                  = InitSpell(DISARM_1);
            BLOODRAGE                               = InitSpell(me, BLOODRAGE_1);
            BERSERKERRAGE                           = InitSpell(me, BERSERKERRAGE_1);
            INTERCEPT                               = InitSpell(me, INTERCEPT_1);
            CLEAVE                                  = InitSpell(me, CLEAVE_1);
            HAMSTRING                               = InitSpell(me, HAMSTRING_1);
            INTERVENE                               = InitSpell(me, INTERVENE_1);
            WHIRLWIND                               = InitSpell(me, WHIRLWIND_1);
  /*Talent*/BLADESTORM                  = lvl >= 60 ? BLADESTORM_1 : 0;
            BATTLESHOUT                             = InitSpell(me, BATTLESHOUT_1);
            REND                                    = InitSpell(me, REND_1);
            EXECUTE                                 = InitSpell(me, EXECUTE_1);
            PUMMEL                                  = InitSpell(me, PUMMEL_1);
  /*Talent*/MORTALSTRIKE                = lvl >= 40 ? InitSpell(me, MORTALSTRIKE_1) : 0;
            SLAM                                    = InitSpell(me, SLAM_1);
   /*Quest*/SUNDER                      = lvl >= 10 ? InitSpell(me, SUNDER_1) : 0;
  /*Talent*/SWEEPING_STRIKES            = lvl >= 30 ? SWEEPING_STRIKES_1 : 0;
            BATTLESTANCE                            = BATTLESTANCE_1;
   /*Quest*/DEFENSIVESTANCE             = lvl >= 10 ? DEFENSIVESTANCE_1 : 0;
   /*Quest*/BERSERKERSTANCE             = lvl >= 30 ? BERSERKERSTANCE_1 : 0;
            RECKLESSNESS                            = InitSpell(me, RECKLESSNESS_1);
            RETALIATION                             = InitSpell(me, RETALIATION_1);
  /*Talent*/DEATHWISH                   = lvl >= 30 ? DEATHWISH_1 : 0;
        }

        void ApplyClassPassives()
        {
            uint8 level = master->getLevel();
            if (level >= 70)
                RefreshAura(WC5); //10%
            else if (level >= 68)
                RefreshAura(WC4); //8%
            else if (level >= 66)
                RefreshAura(WC3); //6%
            else if (level >= 64)
                RefreshAura(WC2); //4%
            else if (level >= 62)
                RefreshAura(WC1); //2%
            if (level >= 39)
                RefreshAura(FLURRY5); //30%
            else if (level >= 38)
                RefreshAura(FLURRY4); //24%
            else if (level >= 37)
                RefreshAura(FLURRY3); //18%
            else if (level >= 36)
                RefreshAura(FLURRY2); //12%
            else if (level >= 35)
                RefreshAura(FLURRY1); //6%
            if (level >= 60)
                RefreshAura(SWORD_SPEC5,2);//twice
            else if (level >= 50)
                RefreshAura(SWORD_SPEC5);//once
            else if (level >= 45)
                RefreshAura(SWORD_SPEC4);//once
            else if (level >= 40)
                RefreshAura(SWORD_SPEC3);//once
            else if (level >= 35)
                RefreshAura(SWORD_SPEC2);//once
            else if (level >= 30)
                RefreshAura(SWORD_SPEC1);//once
            if (level >= 60)
                RefreshAura(RAMPAGE);
            if (level >= 55)
                RefreshAura(TRAUMA2);//30%
            else if (level >= 35)
                RefreshAura(TRAUMA1);//15%
            if (level >= 50)
            {
                RefreshAura(UNRELENTING_ASSAULT2);
                UNRELENTING_ASSAULT = true;
            }
            else if (level >= 45)
            {
                RefreshAura(UNRELENTING_ASSAULT1);
                UNRELENTING_ASSAULT = true;
            }
            if (level >= 45)
                RefreshAura(BLOOD_FRENZY);
            if (level >= 40)
                RefreshAura(SECOND_WIND);
            if (level >= 40)
                RefreshAura(TOUGHNESS,2);//-60%
            else if (level >= 15)
                RefreshAura(TOUGHNESS);//-30%
            if (level >= 40)
                RefreshAura(IMP_HAMSTRING,2);//30%
            else if (level >= 35)
                RefreshAura(IMP_HAMSTRING);//15%
            if (level >= 30)
                RefreshAura(TASTE_FOR_BLOOD3);//100%
            else if (level >= 28)
                RefreshAura(TASTE_FOR_BLOOD2);//66%
            else if (level >= 25)
                RefreshAura(TASTE_FOR_BLOOD1);//33%
            if (level >= 30)
                RefreshAura(BLOOD_CRAZE3);
            else if (level >= 25)
                RefreshAura(BLOOD_CRAZE2);
            else if (level >= 20)
                RefreshAura(BLOOD_CRAZE1);
            //BloodRage Absorb
            if (level >= 60)
                RefreshAura(WARRIOR_T10_4P);
        }

    private:
        uint32
  /*Shouts*/INTIMIDATING_SHOUT, BATTLESHOUT, CHALLENGING_SHOUT, 
 /*Charges*/CHARGE, INTERCEPT, INTERVENE, 
  /*Damage*/OVERPOWER, CLEAVE, REND, EXECUTE, WHIRLWIND, BLADESTORM, MORTALSTRIKE, SLAM, 
 /*Stances*/BATTLESTANCE, DEFENSIVESTANCE, BERSERKERSTANCE, 
   /*Ubers*/RECKLESSNESS, RETALIATION, DEATHWISH, 
  /*Others*/TAUNT, DISARM, BLOODRAGE, ENRAGED_REGENERATION, BERSERKERRAGE, HAMSTRING, PUMMEL, SUNDER, SWEEPING_STRIKES;

        //CDs/Timers/misc
/*shts*/uint32 battleShout_cd, intimidatingShout_cd;
/*chrg*/uint32 charge_cd, intercept_cd, intervene_cd;;
 /*Dmg*/uint32 mortalStrike_cd, overpower_cd, slam_cd, whirlwind_cd, cleave_cd, bladestorm_cd;
/*else*/uint32 regen_cd, sweeping_strikes_cd, deathwish_cd, uber_cd, berserkerRage_cd, pummel_cd, 
            bloodrage_cd, taunt_cd, sunder_cd;
/*tmrs*/uint32 stancetimer, ragetimer, ragetimer2;
/*misc*/uint64 rendTarget;
/*misc*/uint32 rage;
/*misc*/float rageIncomeMult, rageLossMult;
/*Chck*/bool battleStance, defensiveStance, berserkerStance, SS, UNRELENTING_ASSAULT;

        enum WarriorBaseSpells
        {
            //CHALLENGING_SHOUT_1                     = 1161,
            INTIMIDATING_SHOUT_1                    = 5246,
            ENRAGED_REGENERATION_1                  = 55694,
            CHARGE_1                                = 11578,
            OVERPOWER_1                             = 7384,
            TAUNT_1                                 = 355,
            //DISARM_1                                = 676,
            BLOODRAGE_1                             = 2687,
            BERSERKERRAGE_1                         = 18499,
            INTERCEPT_1                             = 20252,
            CLEAVE_1                                = 845,//59992,
            HAMSTRING_1                             = 1715,
            INTERVENE_1                             = 3411,
            WHIRLWIND_1                             = 1680,
            BLADESTORM_1                            = 46924,//67541,
            BATTLESHOUT_1                           = 6673,
            REND_1                                  = 772,
            EXECUTE_1                               = 5308,
            PUMMEL_1                                = 6552,
            MORTALSTRIKE_1                          = 12294,
            SLAM_1                                  = 1464,
            SUNDER_1                                = 7386,//16145,
            SWEEPING_STRIKES_1                      = 12328,
            BATTLESTANCE_1                          = 2457,//7165, //2457, original warrior one
            DEFENSIVESTANCE_1                       = 71,//71, original warrior one
            BERSERKERSTANCE_1                       = 2458,//7366, //2458, original warrior spell
            RECKLESSNESS_1                          = 13847,//1719, original warrior spell
            RETALIATION_1                           = 22857,//20230, original warrior spell
            DEATHWISH_1                             = 12292,
        };
        enum WarriorPassives
        {
        //Talents
            WC1  /*WRECKING CREW1*/                 = 46867,
            WC2  /*WRECKING CREW2*/                 = 56611,
            WC3  /*WRECKING CREW3*/                 = 56612,
            WC4  /*WRECKING CREW4*/                 = 56613,
            WC5  /*WRECKING CREW5*/                 = 56614,
            FLURRY1                                 = 16256,
            FLURRY2                                 = 16281,
            FLURRY3                                 = 16282,
            FLURRY4                                 = 16283,
            FLURRY5                                 = 16284,
            SWORD_SPEC1                             = 12281,
            SWORD_SPEC2                             = 12812,
            SWORD_SPEC3                             = 12813,
            SWORD_SPEC4                             = 12814,
            SWORD_SPEC5                             = 12815,
            BLOOD_CRAZE1                            = 16487,
            BLOOD_CRAZE2                            = 16489,
            BLOOD_CRAZE3                            = 16492,
            TASTE_FOR_BLOOD1                        = 56636,
            TASTE_FOR_BLOOD2                        = 56637,
            TASTE_FOR_BLOOD3                        = 56638,
            UNRELENTING_ASSAULT1                    = 46859,
            UNRELENTING_ASSAULT2                    = 46860,
            TRAUMA1                                 = 46854,
            TRAUMA2                                 = 46855,
            BLOOD_FRENZY                            = 29859,
            RAMPAGE                                 = 29801,
            SECOND_WIND                             = 29838,//rank 2
            TOUGHNESS                               = 12764,//rank 5
            IMP_HAMSTRING                           = 23695,//rank 3
        //other
            WARRIOR_T10_4P                          = 70844,
        };
        enum WarriorSpecial
        {
            TASTE_FOR_BLOOD_BUFF                    = 60503,
            UNRELENTING_ASSAULT_SPELL1              = 64849,
            UNRELENTING_ASSAULT_SPELL2              = 64850,
        };
        enum WarriorCooldowns
        {
            ENRAGED_REGENERATION_CD = 90000, //1.5 min
            SWEEPING_STRIKES_CD     = 30000,
            CHARGE_CD               = 15000,
            DEATHWISH_CD            = 90000, //1.5 min
            MORTALSTRIKE_CD         = 7000,
            UBER_CD                 = 150000, //RETALIATION_RECKLESSNESS_SHIELDWALL 2.5 min NEED SEPARATE
            BERSERKERRAGE_CD        = 25000,
            INTERCEPT_CD            = 15000,
            INTIMIDATINGSHOUT_CD    = 45000,
            PUMMEL_CD               = 10000,
            WHIRLWIND_CD            = 8000,
            BLADESTORM_CD           = 60000,
            BLOODRAGE_CD            = 40000,
            //DISARM_CD               = 40000,
            INTERVENE_CD            = 25000,
            BATTLESHOUT_CD          = 25000,
            //SPELLREFLECTION_CD      = 8000,
            TAUNT_CD                = 8000,
            SUNDER_CD               = 7000,
        };
    };
};

void AddSC_warrior_bot()
{
    new warrior_bot();
}
