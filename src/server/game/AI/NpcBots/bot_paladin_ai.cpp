#include "bot_ai.h"
#include "SpellAuraEffects.h"
/*
Paladin NpcBot (reworked by Graff onlysuffering@gmail.com)
Complete - Around 45-50%
TODO: Repentance Work Improve, Tanking, Shield Abilities, Auras
*/
class paladin_bot : public CreatureScript
{
public:
    paladin_bot() : CreatureScript("paladin_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new paladin_botAI(creature);
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

    struct paladin_botAI : public bot_minion_ai
    {
        paladin_botAI(Creature* creature) : bot_minion_ai(creature) { }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_PALADIN) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId, triggered);
        }

        void HOFGroup(Player* pTarget, const uint32 diff)
        {
            if (!HOF || HOF_Timer > diff || GC_Timer > diff || Rand() > 60) return;
            if (IsCasting()) return;//I'm busy

            if (Group* pGroup = pTarget->GetGroup())
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* tPlayer = itr->getSource();
                    if (!tPlayer) continue;
                    if (HOFTarget(tPlayer, diff))
                        return;
                }
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* tPlayer = itr->getSource();
                    if (!tPlayer || !tPlayer->HaveBot()) continue;
                    for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
                    {
                        Creature* cre = tPlayer->GetBotMap()[i]._Cre();
                        if (!cre || !cre->IsInWorld()) continue;
                        if (HOFTarget(cre, diff))
                            return;
                    }
                }
            }
        }

        bool HOFTarget(Unit* target, const uint32 diff)
        {
            if (!HOF || HOF_Timer > diff || GC_Timer > diff) return false;
            if (!target || target->isDead()) return false;
            if (target->ToCreature() && Rand() > 25) return false;
            if (me->GetExactDist(target) > 30) return false;//too far away
            if (HasAuraName(target, HOF)) return false;     //Alredy has HOF

            Unit::AuraMap const &auras = target->GetOwnedAuras();
            for (Unit::AuraMap::const_iterator i = auras.begin(); i != auras.end(); ++i)
            {
                Aura* aura = i->second;
                if (aura->IsPassive()) continue;//most
                if (aura->GetDuration() < 2000) continue;
                if (AuraApplication* app = aura->GetApplicationOfTarget(target->GetGUID()))
                    if (app->IsPositive()) continue;
                SpellInfo const* spellInfo = aura->GetSpellInfo();
                if (spellInfo->AttributesEx & SPELL_ATTR0_HIDDEN_CLIENTSIDE) continue;
                if (me->getLevel() >= 40 && (spellInfo->GetAllEffectsMechanicMask() & (1<<MECHANIC_STUN)))
                {
                    if (doCast(target, HOF))
                    {
                        if (target->ToCreature())
                            HOF_Timer = 10000;//10 sec for selfcast after stun
                        else
                            HOF_Timer = 15000;//improved
                        HOFGuid = target->GetGUID();
                        return true;
                    }
                }
       /*else */if (spellInfo->GetAllEffectsMechanicMask() & (1<<MECHANIC_SNARE) || 
                    spellInfo->GetAllEffectsMechanicMask() & (1<<MECHANIC_ROOT))
                {
                    uint32 spell = (spellInfo->Dispel == DISPEL_MAGIC || spellInfo->Dispel == DISPEL_DISEASE || spellInfo->Dispel == DISPEL_POISON) && CLEANSE ? CLEANSE : HOF;
                    if (doCast(target, spell))
                    {
                        if (spell == HOF)
                        {
                            if (target->ToCreature())
                                HOF_Timer = 5000;//5 sec for bots
                            else
                                HOF_Timer = 15000;//improved
                            if (me->getLevel() >= 40)
                                HOFGuid = target->GetGUID();
                        }
                        return true;
                    }
                }
            }
            return false;
        }

        void HOSGroup(Player* hTarget, const uint32 diff)
        {
            if (!HOS || HOS_Timer > diff || GC_Timer > diff || Rand() > 30) return;
            if (IsCasting()) return;
            if (Group* pGroup = hTarget->GetGroup())
            {
                bool bots = false;
                float threat;
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* HOSPlayer = itr->getSource();
                    if (!HOSPlayer) continue;
                    if (HOSPlayer->HaveBot()) bots = true;
                    if (HOSPlayer->isDead()) continue;
                    if (tank && HOSPlayer == tank) continue;//tanks do not need it
                    if (master->GetMap() != HOSPlayer->GetMap() || me->GetExactDist(HOSPlayer) > 30) continue;
                    if (HasAuraName(HOSPlayer, HOS)) continue;
                    AttackerSet h_attackers = HOSPlayer->getAttackers();
                    if (h_attackers.empty()) continue;
                    for (AttackerSet::iterator iter = h_attackers.begin(); iter != h_attackers.end(); ++iter)
                    {
                        if (!(*iter)) continue;
                        if ((*iter)->isDead()) continue;
                        if (!(*iter)->CanHaveThreatList()) continue;
                        threat = (*iter)->getThreatManager().getThreat(HOSPlayer);
                        if (threat < 25.f) continue;//too small threat
                        if ((*iter)->getThreatManager().getThreat(tank) < threat * 0.33f) continue;//would be useless
                        if (HOSPlayer->GetDistance((*iter)) > 10) continue;
                        if (HOSTarget(HOSPlayer, diff)) return;
                    }//end for
                }//end for
                if (!bots) return;
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* pl = itr->getSource();
                    if (!pl) continue;
                    if (!pl->HaveBot()) continue;
                    if (master->GetMap() != pl->GetMap()) continue;
                    for (uint8 i = 0; i != pl->GetMaxNpcBots(); ++i)
                    {
                        Creature* cre = pl->GetBotMap()[i]._Cre();
                        if (!cre || cre->isDead()) continue;
                        if (tank && cre == tank) continue;
                        if (me->GetExactDist(cre) > 30) continue;
                        if (HasAuraName(cre, HOS)) continue;     //Alredy has HOS
                        AttackerSet h_attackers = cre->getAttackers();
                        if (h_attackers.empty()) continue;
                        for (AttackerSet::iterator iter = h_attackers.begin(); iter != h_attackers.end(); ++iter)
                        {
                            if (!(*iter)) continue;
                            if ((*iter)->isDead()) continue;
                            if (!(*iter)->CanHaveThreatList()) continue;
                            threat = (*iter)->getThreatManager().getThreat(cre);
                            if (threat < 25.f) continue;//too small threat
                            if ((*iter)->getThreatManager().getThreat(tank) < threat * 0.33f) continue;//would be useless
                            if (cre->GetDistance((*iter)) > 10) continue;
                            if (HOSTarget(cre, diff)) return;
                        }//end for
                    }//end for
                }//end for
            }//end if
        }

        bool HOSTarget(Unit* target, const uint32 diff)
        {
            if (!target || target->isDead()) return false;
            if (!HOS || HOS_Timer > diff || GC_Timer > diff || Rand() > 50) return false;
            if (tank && target == tank) return false;       //tanks do not need it
            if (IsCasting()) return false;          //I'm busy casting
            if (me->GetExactDist(target) > 30) return false; //too far away
            if (HasAuraName(target, HOS)) return false;     //Alredy has HOS

            AttackerSet h_attackers = target->getAttackers();
            if (h_attackers.empty()) return false; //no aggro
            float threat;
            uint8 Tattackers = 0;
            for (AttackerSet::iterator iter = h_attackers.begin(); iter != h_attackers.end(); ++iter)
            {
                if (!(*iter)) continue;
                if ((*iter)->isDead()) continue;
                if (!(*iter)->CanHaveThreatList()) continue;
                threat = (*iter)->getThreatManager().getThreat(target);
                if (threat < 25.f) continue;//too small threat
                if ((*iter)->getThreatManager().getThreat(tank) < threat * 0.33f) continue;//would be useless
                if (target->GetDistance((*iter)) <= 10)
                    Tattackers++;
            }
            if (Tattackers > 0 && doCast(target, HOS))
            {
                for (AttackerSet::iterator iter = h_attackers.begin(); iter != h_attackers.end(); ++iter)
                    if ((*iter)->getThreatManager().getThreat(target) > 0.f)
                        (*iter)->getThreatManager().modifyThreatPercent(target, -(30 + 50*(target->HasAura(586))));//Fade
                HOS_Timer = 25000 - 20000*IS_CREATURE_GUID(target->GetGUID());
                return true;
            }
            return false;
        }
        //Holy_Shock setup (Modify HERE)
        bool HS(Unit* target, const uint32 diff)
        {
            if (!target || target->isDead()) return false;
            if (!HOLY_SHOCK || HS_Timer > diff || GC_Timer > diff) return false;
            if (IsCasting()) return false;
            if (target->GetTypeId() == TYPEID_PLAYER && (target->isCharmed() || target->isPossessed())) return false;//do not damage friends under control
            if (me->GetExactDist(target) > 40) return false;

            if (doCast(target, HOLY_SHOCK))
            {
                HS_Timer = target->ToCreature() ? 3500 : 5000;
                return true;
            }
            return false;
        }

        bool HealTarget(Unit* target, uint8 hp, const uint32 diff)
        {
            if (!target || target->isDead()) return false;
            if (hp > 97) return false;
            //sLog->outBasic("HealTarget() by %s on %s", me->GetName().c_str(), target->GetName().c_str());
            if (Rand() > 40 + 20*target->isInCombat() + 50*master->GetMap()->IsRaid()) return false;
            if (me->GetExactDist(target) > 35) return false;
            if (IsCasting()) return false;
            if (HAND_OF_PROTECTION && BOP_Timer <= diff && IS_PLAYER_GUID(target->GetGUID()) && 
                (master->GetGroup() && master->GetGroup()->IsMember(target->GetGUID()) || target == master) && 
                ((hp < 30 && !target->getAttackers().empty()) || (hp < 50 && target->getAttackers().size() > 3)) && 
                me->GetExactDist(target) < 30 && 
                !HasAuraName(target, HAND_OF_PROTECTION) && 
                !HasAuraName(target, "Forbearance"))
            {
                if (doCast(target, HAND_OF_PROTECTION))
                {
                    if (target->GetTypeId() == TYPEID_PLAYER)
                        me->MonsterWhisper("BOP on you!", target->GetGUID());
                    BOP_Timer = 60000; //1 min
                    if (!HasAuraName(target, "Forbearance"))
                        me->AddAura(25771, target);//Forbearance
                    if (HasAuraName(target, "Forbearance") && !target->HasAura(HAND_OF_PROTECTION))
                        me->AddAura(HAND_OF_PROTECTION, target);
                }
                return true;
            }
            else if (hp < 20 && !HasAuraName(target, HAND_OF_PROTECTION))
            {
                // 20% to cast loh, else just do a Shock
                switch (rand()%3)
                {
                    case 1:
                        if (LAY_ON_HANDS && LOH_Timer <= diff && hp < 20 && 
                            target->GetTypeId() == TYPEID_PLAYER && 
                            (target->isInCombat() || !target->getAttackers().empty()) && 
                            !HasAuraName(target, "Forbearance"))
                        {
                            if (doCast(target, LAY_ON_HANDS))
                            {
                                me->MonsterWhisper("Lay of Hands on you!", target->GetGUID());
                                LOH_Timer = 60000; //1 min
                                return true;
                            }
                        }
                    case 2:
                        if (GC_Timer > diff) return false;
                        if (FLASH_OF_LIGHT && doCast(target, FLASH_OF_LIGHT))
                            return true;
                    case 3:
                        if (GC_Timer > diff) return false;
                        if (HOLY_SHOCK && HS_Timer <= diff && HS(target, diff))
                            return true;
                }
            }
            if (GC_Timer > diff) return false;
            Unit* u = target->getVictim();
            if (SACRED_SHIELD && SSH_Timer <= diff && target->GetTypeId() == TYPEID_PLAYER && 
                (hp < 65 || target->getAttackers().size() > 1 || (u && u->GetMaxHealth() > target->GetMaxHealth()*10 && target->isInCombat())) && 
                !target->HasAura(SACRED_SHIELD) && 
                ((master->GetGroup() && master->GetGroup()->IsMember(target->GetGUID())) || target == master))
            {
                Unit* aff = FindAffectedTarget(SACRED_SHIELD, me->GetGUID(), 50, 1);//use players since we cast only on them
                if ((!aff || (aff->getAttackers().empty() && tank != aff)) && 
                    doCast(target, SACRED_SHIELD))
                {
                    SSH_Timer = 3000;
                    return true;
                }
            }
            if (HOLY_SHOCK && (hp < 85 || GetLostHP(target) > 6000) && HS_Timer <= diff)
                if (HS(target, diff))
                    return true;
            if ((hp > 35 && (hp < 75 || GetLostHP(target) > 8000)) || (!FLASH_OF_LIGHT && hp < 85))
                if (doCast(target, HOLY_LIGHT))
                    return true;
            if (FLASH_OF_LIGHT && (hp < 90 || GetLostHP(target) > 1500))
                if (doCast(target, FLASH_OF_LIGHT))
                    return true;
            return false;
        }//end HealTarget

        void StartAttack(Unit* u, bool force = false)
        {
            if (GetBotCommandState() == COMMAND_ATTACK && !force) return;
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

        void UpdateAI(const uint32 diff)
        {
            ReduceCD(diff);
            if (HOFGuid != 0)
            {
                if (Unit* ally = sObjectAccessor->FindUnit(HOFGuid))
                    if (Aura* hof = ally->GetAura(HOF, me->GetGUID()))
                        hof->SetDuration(hof->GetDuration() + 4000);//Guardian's Favor part 2 (handled separately)
                HOFGuid = 0;
            }
            if (IAmDead()) return;
            if (me->getVictim())
                DoMeleeAttackIfReady();
            else
                Evade();
            if (wait == 0)
                wait = GetWait();
            else
                return;
            CheckAuras();
            BreakCC(diff);
            //HOFTarget(me, diff);//self stun cure goes FIRST
            if (CCed(me)) return;

            if (GetManaPCT(me) < 30 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, MANAPOTION))
                    Potion_cd = POTION_CD;
                GC_Timer = temptimer;
            }
            if (GetManaPCT(me) < 40 && DIVINE_PLEA && Divine_Plea_Timer <= diff)
                if (doCast(me, DIVINE_PLEA))
                    Divine_Plea_Timer = 45000;

            CureTarget(me, CLEANSE, diff);//maybe unnecessary but this goes FIRST
            HOFTarget(master, diff);//maybe unnecessary
            CureTarget(master, CLEANSE, diff);//maybe unnecessary
            BuffAndHealGroup(master, diff);
            HOSTarget(master, diff);
            CureGroup(master, CLEANSE, diff);
            HOFGroup(master, diff);
            HOSGroup(master, diff);

            if (GetHealthPCT(me) < 50 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, HEALINGPOTION))
                    Potion_cd = POTION_CD;
                GC_Timer = temptimer;
            }
            if (!me->isInCombat())
                DoNonCombatActions(diff);
            //buff
            if (SEAL_OF_COMMAND && GC_Timer <= diff && !me->HasAura(SEAL_OF_COMMAND) && 
                doCast(me, SEAL_OF_COMMAND))
                GC_Timer = 500;

            // Heal myself
            if (GetHealthPCT(me) < 80)
                HealTarget(me, GetHealthPCT(me), diff);

            if (!CheckAttackTarget(CLASS_PALADIN))
                return;

            Repentance(diff);
            //Counter(diff);
            DoNormalAttack(diff);
        }

        void DoNonCombatActions(const uint32 diff)
        {
            if (GC_Timer > diff || me->IsMounted()) return;

            RezGroup(REDEMPTION, master);

            if (Feasting()) return;

            //aura
            if (master->isAlive() && me->GetExactDist(master) < 20)
            {
                uint8 myAura;
                if (me->HasAura(DEVOTION_AURA, me->GetGUID()))
                    myAura = DEVOTIONAURA;
                else if (me->HasAura(CONCENTRATION_AURA, me->GetGUID()))
                    myAura = CONCENTRATIONAURA;
                else myAura = NOAURA;

                if (myAura != NOAURA)
                    return; //do not bother

                Aura* concAura = master->GetAura(CONCENTRATION_AURA);
                Aura* devAura = master->GetAura(DEVOTION_AURA);
                if (devAura && concAura) return;
                if (devAura && devAura->GetCasterGUID() == me->GetGUID()) return;
                if (concAura && concAura->GetCasterGUID() == me->GetGUID()) return;

                if ((master->getClass() == CLASS_MAGE || 
                    master->getClass() == CLASS_PRIEST || 
                    master->getClass() == CLASS_WARLOCK || 
                    master->getClass() == CLASS_DRUID || devAura) && 
                    !concAura && 
                    doCast(me, CONCENTRATION_AURA))
                {
                    /*GC_Timer = 800;*/
                    return;
                }
                if (!devAura && doCast(me, DEVOTION_AURA))
                {
                    /*GC_Timer = 800;*/
                    return;
                }
            }
        }

        bool BuffTarget(Unit* target, const uint32 diff)
        {
            if (!target || target->isDead() || GC_Timer > diff || Rand() > 30) return false;
            if (me->isInCombat() && !master->GetMap()->IsRaid()) return false;
            if (me->GetExactDist(target) > 30) return false;
            if (HasAuraName(target, "Blessing of Wisdom", me->GetGUID()) || 
                HasAuraName(target, "Blessing of Might", me->GetGUID()) || 
                HasAuraName(target, "Blessing of Kings", me->GetGUID()) || 
                HasAuraName(target, "Blessing of Sanctuary", me->GetGUID()))
                return false;
            //if (HasAuraName(target, "Greater Blessing of Wisdom", me->GetGUID()) || 
            //    HasAuraName(target, "Greater Blessing of Might", me->GetGUID()) || 
            //    HasAuraName(target, "Greater Blessing of Kings", me->GetGUID()) || 
            //    HasAuraName(target, "Greater Blessing of Sanctuary", me->GetGUID()))
            //    return false;
            bool wisdom = HasAuraName(target, BLESSING_OF_WISDOM) || HasAuraName(target, "Greater Blessing of Wisdom");
            bool kings = HasAuraName(target, BLESSING_OF_KINGS) || HasAuraName(target, "Greater Blessing of Kings");
            bool sanctuary = HasAuraName(target, BLESSING_OF_SANCTUARY) || HasAuraName(target, "Greater Blessing of Sanctuary");
            bool might = (HasAuraName(target, BLESSING_OF_MIGHT) || HasAuraName(target, "Greater Blessing of Might") || HasAuraName(target, "Battle Shout"));

            uint8 Class = 0;
            if (target->GetTypeId() == TYPEID_PLAYER)
                Class = target->ToPlayer()->getClass();
            else if (target->ToCreature())
                Class = target->ToCreature()->GetBotClass();
            switch (Class)
            {
            case CLASS_PRIEST:
                if (BLESSING_OF_WISDOM && !wisdom && doCast(target, BLESSING_OF_WISDOM))
                    return true;
                else if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                break;
            case CLASS_DEATH_KNIGHT:
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_ROGUE:
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
                if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                else if (!might && doCast(target, BLESSING_OF_MIGHT))
                    return true;
                else if (BLESSING_OF_SANCTUARY && !sanctuary && doCast(target, BLESSING_OF_SANCTUARY))
                    return true;
                else if (BLESSING_OF_WISDOM && !wisdom && target->getPowerType() == POWER_MANA && doCast(target, BLESSING_OF_WISDOM))
                    return true;
                break;
            default:
                if (BLESSING_OF_KINGS && !kings && doCast(target, BLESSING_OF_KINGS))
                    return true;
                else if (BLESSING_OF_WISDOM && !wisdom && target->getPowerType() == POWER_MANA && doCast(target, BLESSING_OF_WISDOM))
                    return true;
                else if (BLESSING_OF_SANCTUARY && !sanctuary && doCast(target, BLESSING_OF_SANCTUARY))
                    return true;
                else if (!might && doCast(target, BLESSING_OF_MIGHT))
                    return true;
                break;
            }
            return false;
        }

        void Repentance(const uint32 diff, Unit* target = NULL)
        {
            if (target && Repentance_Timer < 25000 && doCast(target, REPENTANCE))
            {
                temptimer = GC_Timer;
                Repentance_Timer = 45000;
                GC_Timer = temptimer;
                return;
            }
            if (REPENTANCE && Repentance_Timer <= diff)
            {
                Unit* u = FindRepentanceTarget();
                if (u && u->getVictim() != me && doCast(u, REPENTANCE))
                    Repentance_Timer = 45000;
            }
        }

        void Counter(const uint32 diff)
        {
            if (Rand() > 60 || IsCasting()) return;
            Unit* target = Repentance_Timer < 25000 ? FindCastingTarget(20, false, REPENTANCE) : NULL;
            if (target)
                Repentance(diff, target);//first check repentance
            else if (TURN_EVIL && Turn_Evil_Timer < 1500)
            {
                target = FindCastingTarget(20, false, TURN_EVIL);
                temptimer = GC_Timer;
                if (target && doCast(target, TURN_EVIL, true))
                    Turn_Evil_Timer = 3000;
                GC_Timer = temptimer;
            }
            else if (HOLY_WRATH && Holy_Wrath_Timer < 8000)
            {
                target = FindCastingTarget(8, false, TURN_EVIL);//here we check target as with turn evil cuz of same requirements
                temptimer = GC_Timer;
                if (target && doCast(me, HOLY_WRATH))
                    Holy_Wrath_Timer = 23000 - me->getLevel() * 100; //23 - 0...8 sec (15 sec on 80 as with glyph)
                GC_Timer = temptimer;
            }
            else if (HAMMER_OF_JUSTICE && HOJ_Timer <= 7000/* && GC_Timer <= diff*/)
            {
                target = FindCastingTarget(10);
                if (target && doCast(opponent, HAMMER_OF_JUSTICE))
                    HOJ_Timer = 65000 - master->getLevel()*500; //25 sec on 80
            }
        }

        void TurnEvil(const uint32 diff)
        {
            if (!TURN_EVIL || Turn_Evil_Timer > diff || GC_Timer > diff || Rand() > 50 || 
                FindAffectedTarget(TURN_EVIL, me->GetGUID(), 50))
                return;
            Unit* target = FindUndeadCCTarget(20, TURN_EVIL);
            if (target && 
                (target != me->getVictim() || GetHealthPCT(me) < 70 || target->getVictim() == master) && 
                doCast(target, TURN_EVIL, true))
            {
                Turn_Evil_Timer = 3000;
                return;
            }
            else
            if ((opponent->GetCreatureType() == CREATURE_TYPE_UNDEAD || opponent->GetCreatureType() == CREATURE_TYPE_DEMON) && 
                !CCed(opponent) && 
                opponent->getVictim() && tank && opponent->getVictim() != tank && opponent->getVictim() != me && 
                GetHealthPCT(me) < 90 && 
                doCast(opponent, TURN_EVIL, true))
                Turn_Evil_Timer = 3000;
        }

        void Wrath(const uint32 diff)
        {
            if (!HOLY_WRATH || Holy_Wrath_Timer > diff || GC_Timer > diff || Rand() > 50)
                return;
            if ((opponent->GetCreatureType() == CREATURE_TYPE_UNDEAD || opponent->GetCreatureType() == CREATURE_TYPE_DEMON) && 
                me->GetExactDist(opponent) <= 8 && doCast(me, HOLY_WRATH))
                Holy_Wrath_Timer = 23000 - me->getLevel() * 100; //23 - 0...8 sec (15 sec on 80 as with glyph)
            else 
            {
                Unit* target = FindUndeadCCTarget(8, HOLY_WRATH);
                if (target && doCast(me, HOLY_WRATH))
                    Holy_Wrath_Timer = 23000 - me->getLevel() * 100; //23 - 0...8 sec (15 sec on 80 as with glyph)
            }
        }

        void DoNormalAttack(const uint32 diff)
        {
            opponent = me->getVictim();
            if (opponent)
            {
                if (!IsCasting())
                    StartAttack(opponent, true);
            }
            else
                return;

            Counter(diff);
            TurnEvil(diff);

            if (MoveBehind(*opponent))
                wait = 5;
            //{ wait = 5; return; }

            if (HOW && HOW_Timer <= diff && GC_Timer <= diff && Rand() < 50 && GetHealthPCT(opponent) < 20 && 
                me->GetExactDist(opponent) < 30)
                if (doCast(opponent, HOW))
                    HOW_Timer = 6000; //6 sec

            Unit* u = opponent->getVictim();
            if (Rand() < 50 && HANDOFRECKONING && Hand_Of_Reckoning_Timer <= diff && me->GetExactDist(opponent) < 30 && 
                u && u != me && u != tank && (IsInBotParty(u) || tank == me))//No GCD
            {
                Creature* cre = opponent->ToCreature();
                temptimer = GC_Timer;
                if (((cre && cre->isWorldBoss() && !isMeleeClass(u->getClass())) || 
                    GetHealthPCT(u) < GetHealthPCT(me) - 5 || tank == me) && 
                    doCast(opponent, HANDOFRECKONING))
                    Hand_Of_Reckoning_Timer = 8000 - (me == tank)*2000;
                GC_Timer = temptimer;
            }

            if (Rand() < 20 && HAMMER_OF_JUSTICE && HOJ_Timer <= diff && GC_Timer <= diff && 
                !CCed(opponent) && me->GetExactDist(opponent) < 10)
                if (doCast(opponent, HAMMER_OF_JUSTICE))
                    HOJ_Timer = 65000 - master->getLevel()*500; //25 sec on 80

            if (JUDGEMENT_OF_LIGHT && JOL_Timer <= diff && GC_Timer <= diff && Rand() < 50 && 
                me->GetExactDist(opponent) < 10 && me->HasAura(SEAL_OF_COMMAND))
                if (doCast(opponent, JUDGEMENT_OF_LIGHT))
                    JOL_Timer = 8000;

            if (Rand() < 50 && CONSECRATION && Consecration_cd <= diff && GC_Timer <= diff && 
                me->GetDistance(opponent) < 7 && !opponent->isMoving())
                if (doCast(me, CONSECRATION))
                    Consecration_cd = 8000;

            if (Rand() < 25 && AVENGING_WRATH && AW_Timer <= diff && 
                (opponent->GetHealth() > master->GetMaxHealth()*2/3))
            {
                temptimer = GC_Timer;
                if (doCast(me, AVENGING_WRATH))
                    AW_Timer = 60000; //1 min
                GC_Timer = temptimer;
            }

            if (CRUSADER_STRIKE && Crusader_cd <= diff && GC_Timer <= diff && me->GetDistance(opponent) < 5)
                if (doCast(opponent, CRUSADER_STRIKE))
                    Crusader_cd = 12000 - me->getLevel() * 100;//4 sec on 80

            if (EXORCISM && Exorcism_Timer <= diff && GC_Timer <= diff && me->GetExactDist(opponent) < 30)
                if (doCast(opponent, EXORCISM/*, true)*/))//possible instacast here
                    Exorcism_Timer = 15000;

            Wrath(diff);

            if (DIVINE_STORM && DS_Timer <= diff && GC_Timer <= diff && me->GetExactDist(opponent) < 7)
                if (doCast(opponent, DIVINE_STORM))
                    DS_Timer = 10000 - me->getLevel()/4 * 100; //10 - 2 sec
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
                //Fanaticism: 18% additional critical chance for all Judgements (not shure which check is right)
                if (lvl >= 45 && (spellInfo->Category == SPELLCATEGORY_JUDGEMENT || spellInfo->GetSpellSpecific() == SPELL_SPECIFIC_JUDGEMENT))
                    aftercrit += 18.f;

                if (aftercrit > 0.f)
                    crit = roll_chance_f(aftercrit);
            }

            //2) apply bonus damage mods
            float pctbonus = 0.0f;
            //if (crit)
            //{
            //}
            //Sanctity of Battle: 15% bonus damage for Exorcism and Crusader Strike
            if (lvl >= 25 && spellId == EXORCISM)
                pctbonus += 0.15f;
            //The Art of War (damage part): 10% bonus damage for Judgements, Crusader Strike and Divine Storm
            if (lvl >= 40 &&
                (spellInfo->Category == SPELLCATEGORY_JUDGEMENT ||
                spellInfo->GetSpellSpecific() == SPELL_SPECIFIC_JUDGEMENT || 
                spellId == CRUSADER_STRIKE ||
                spellId == DIVINE_STORM))
                pctbonus += 0.1f;

            damage = int32(fdamage * (1.0f + pctbonus));
        }

        void ApplyClassDamageMultiplierSpell(int32& damage, SpellNonMeleeDamage& /*damageinfo*/, SpellInfo const* spellInfo, WeaponAttackType /*attackType*/, bool& crit) const
        {
            uint32 spellId = spellInfo->Id;
            uint8 lvl = me->getLevel();
            float fdamage = float(damage);
            //1) apply additional crit chance. This additional chance roll will replace original (balance safe)
            if (!crit)
            {
                float aftercrit = 0.f;
                //Sanctified Wrath: 50% additional critical chance for Hammer of Wrath
                if (lvl >= 45 && spellId == HOW)
                    aftercrit += 50.f;

                if (aftercrit > 0.f)
                    crit = roll_chance_f(aftercrit);
            }

            //2) apply bonus damage mods
            float pctbonus = 0.0f;
            //if (crit)
            //{
            //}

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
            Crusader_cd = 0;
            Consecration_cd = 0;
            LOH_Timer = 0;
            HOJ_Timer = 0;
            HOF_Timer = 0;
            JOL_Timer = 0;
            HS_Timer = 0;
            BOP_Timer = 0;
            HOW_Timer = 0;
            DS_Timer = 0;
            AW_Timer = 10000;
            HOS_Timer = 0;
            SSH_Timer = 0;
            Hand_Of_Reckoning_Timer = 0;
            Divine_Plea_Timer = 0;
            Repentance_Timer = 0;
            Exorcism_Timer = 0;
            Holy_Wrath_Timer = 0;
            Turn_Evil_Timer = 0;

            HOFGuid = 0;

            if (master)
            {
                setStats(CLASS_PALADIN, me->getRace(), master->getLevel(), true);
                ApplyClassPassives();
                ApplyPassives(CLASS_PALADIN);
            }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (HOW_Timer > diff)                   HOW_Timer -= diff;
            if (DS_Timer > diff)                    DS_Timer -= diff;
            if (AW_Timer > diff)                    AW_Timer -= diff;
            if (HOS_Timer > diff)                   HOS_Timer -= diff;
            if (HS_Timer > diff)                    HS_Timer -= diff;
            if (BOP_Timer > diff)                   BOP_Timer -= diff;
            if (Consecration_cd > diff)             Consecration_cd -= diff;
            if (Crusader_cd > diff)                 Crusader_cd -= diff;
            if (LOH_Timer > diff)                   LOH_Timer -= diff;
            if (HOJ_Timer > diff)                   HOJ_Timer -= diff;
            if (HOF_Timer > diff)                   HOF_Timer -= diff;
            if (JOL_Timer > diff)                   JOL_Timer -= diff;
            if (SSH_Timer > diff)                   SSH_Timer -= diff;
            if (Hand_Of_Reckoning_Timer > diff)     Hand_Of_Reckoning_Timer -= diff;
            if (Divine_Plea_Timer > diff)           Divine_Plea_Timer -= diff;
            if (Repentance_Timer > diff)            Repentance_Timer -= diff;
            if (Exorcism_Timer > diff)              Exorcism_Timer -= diff;
            if (Holy_Wrath_Timer > diff)            Holy_Wrath_Timer -= diff;
            if (Turn_Evil_Timer > diff)             Turn_Evil_Timer -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            uint8 lvl = me->getLevel();
            FLASH_OF_LIGHT                          = InitSpell(me, FLASH_OF_LIGHT_1);
            HOLY_LIGHT                              = InitSpell(me, HOLY_LIGHT_1);
            LAY_ON_HANDS                            = InitSpell(me, LAY_ON_HANDS_1);
            SACRED_SHIELD                           = InitSpell(me, SACRED_SHIELD_1);
            HOLY_SHOCK                  = lvl >= 40 ? InitSpell(me, HOLY_SHOCK_1) : 0;
            CLEANSE                                 = InitSpell(me, CLEANSE_1);
            REDEMPTION                              = InitSpell(me, REDEMPTION_1);
            HAMMER_OF_JUSTICE                       = InitSpell(me, HAMMER_OF_JUSTICE_1);
            REPENTANCE                  = lvl >= 45 ? REPENTANCE_1 : 0;
            TURN_EVIL                               = InitSpell(me, TURN_EVIL_1);
            HOLY_WRATH                              = InitSpell(me, HOLY_WRATH_1);
            EXORCISM                                = InitSpell(me, EXORCISM_1);
            SEAL_OF_COMMAND             = lvl >= 25 ? SEAL_OF_COMMAND_1 : 0;
            CRUSADER_STRIKE             = lvl >= 20 ? CRUSADER_STRIKE_1 : 0;//exception
            JUDGEMENT_OF_LIGHT                      = InitSpell(me, JUDGEMENT_OF_LIGHT_1);
            CONSECRATION                            = InitSpell(me, CONSECRATION_1);
            DIVINE_STORM                = lvl >= 60 ? DIVINE_STORM_1 : 0;
            HOW /*Hammer of Wrath*/                 = InitSpell(me, HOW_1);
            AVENGING_WRATH                          = InitSpell(me, AVENGING_WRATH_1);
            BLESSING_OF_MIGHT                       = InitSpell(me, BLESSING_OF_MIGHT_1);
            BLESSING_OF_WISDOM                      = InitSpell(me, BLESSING_OF_WISDOM_1);
            BLESSING_OF_KINGS                       = InitSpell(me, BLESSING_OF_KINGS_1);
            BLESSING_OF_SANCTUARY       = lvl >= 30 ? BLESSING_OF_SANCTUARY_1 : 0;
            DEVOTION_AURA                           = InitSpell(me, DEVOTION_AURA_1);
            CONCENTRATION_AURA                      = InitSpell(me, CONCENTRATION_AURA_1);
            DIVINE_PLEA                             = InitSpell(me, DIVINE_PLEA_1);
            HAND_OF_PROTECTION                      = InitSpell(me, HAND_OF_PROTECTION_1);
            HOF/*Hand of Freedom*/                  = InitSpell(me, HOF_1);
            HOS /*Hand of salvation*/               = InitSpell(me, HOS_1);
            HANDOFRECKONING                         = InitSpell(me, HANDOFRECKONING_1);
        }

        void ApplyClassPassives()
        {
            uint8 level = master->getLevel();
            //1 - SPD 3% crit 3%
            if (level >= 78)
                RefreshAura(SPELLDMG,5); //+15%
            else if (level >= 75)
                RefreshAura(SPELLDMG,4); //+12%
            else if (level >= 55)
                RefreshAura(SPELLDMG,3); //+9%
            else if (level >= 35)
                RefreshAura(SPELLDMG,2); //+6%
            else if (level >= 15)
                RefreshAura(SPELLDMG); //+3%
            //2 - SPD 6%
            if (level >= 55)
                RefreshAura(SPELLDMG2,3); //+18%
            else if (level >= 35)
                RefreshAura(SPELLDMG2,2); //+12%
            else if (level >= 15)
                RefreshAura(SPELLDMG2); //+6%
            //Talents
            if (level >= 55)
                RefreshAura(PURE);
            if (level >= 35)
                RefreshAura(WISE);
            if (level >= 50)
                RefreshAura(RECKONING5); //10%
            else if (level >= 45)
                RefreshAura(RECKONING4); //8%
            else if (level >= 40)
                RefreshAura(RECKONING3); //6%
            else if (level >= 35)
                RefreshAura(RECKONING2); //4%
            else if (level >= 30)
                RefreshAura(RECKONING1); //2%
            //if (level >= 50)
            //    RefreshAura(RIGHTEOUS_VENGEANCE3);
            //else if (level >= 47)
            //    RefreshAura(RIGHTEOUS_VENGEANCE2);
            //else if (level >= 45)
            //    RefreshAura(RIGHTEOUS_VENGEANCE1);
            if (level >= 30)
                RefreshAura(VENGEANCE3);
            else if (level >= 27)
                RefreshAura(VENGEANCE2);
            else if (level >= 25)
                RefreshAura(VENGEANCE1);
            if (level >= 60)
                RefreshAura(SHOFL3);
            else if (level >= 55)
                RefreshAura(SHOFL2);
            else if (level >= 50)
                RefreshAura(SHOFL1);
            if (level >= 45)
                RefreshAura(SACRED_CLEANSING);
            if (level >= 35)
                RefreshAura(DIVINE_PURPOSE);
            if (level >= 25)
                RefreshAura(VINDICATION2);
            else if (level >= 20)
                RefreshAura(VINDICATION1);
            if (level >= 30)
                RefreshAura(LAYHANDS);
            if (level >= 20)
                RefreshAura(FANATICISM,2); //-60% aggro
            if (level >= 15)
                RefreshAura(GLYPH_HOLY_LIGHT); //10% heal
            //if (level >= 70)
            //    RefreshAura(PALADIN_T9_2P_BONUS); //Righteous Vengeance Crits
        }

    private:
        uint32
   /*Heals*/FLASH_OF_LIGHT, HOLY_LIGHT, HOLY_SHOCK, LAY_ON_HANDS, SACRED_SHIELD,
      /*CC*/HAMMER_OF_JUSTICE, REPENTANCE, TURN_EVIL,
  /*Damage*/SEAL_OF_COMMAND, HOLY_WRATH, EXORCISM, CRUSADER_STRIKE, JUDGEMENT_OF_LIGHT,
  /*Damage*/CONSECRATION, DIVINE_STORM, AVENGING_WRATH, HOW,//hammer of wrath
/*Blessing*/BLESSING_OF_MIGHT, BLESSING_OF_WISDOM, BLESSING_OF_KINGS, BLESSING_OF_SANCTUARY,
   /*Auras*/DEVOTION_AURA, CONCENTRATION_AURA,
   /*Hands*/HAND_OF_PROTECTION, HOF, HOS, HANDOFRECKONING,
    /*Misc*/CLEANSE, REDEMPTION, DIVINE_PLEA;
        //Timers
        uint32 Crusader_cd, Consecration_cd, Exorcism_Timer, Holy_Wrath_Timer, JOL_Timer, HOF_Timer,
            HS_Timer, HOW_Timer, DS_Timer, HOS_Timer, SSH_Timer, Hand_Of_Reckoning_Timer, Turn_Evil_Timer,
            LOH_Timer, HOJ_Timer, BOP_Timer, AW_Timer, Divine_Plea_Timer, Repentance_Timer;
        uint64 HOFGuid;

        enum PaladinBaseSpells// all orignals
        {
            FLASH_OF_LIGHT_1                    = 19750,
            HOLY_LIGHT_1                        = 635,
            LAY_ON_HANDS_1                      = 633,
            REDEMPTION_1                        = 7328,
            HOF_1  /*Hand of Freedom*/          = 1044,
            SACRED_SHIELD_1                     = 53601,
            HOLY_SHOCK_1                        = 20473,
            CLEANSE_1                           = 4987,
            HAND_OF_PROTECTION_1                = 1022,
            HOS_1 /*Hand of salvation*/         = 1038,
            SEAL_OF_COMMAND_1                   = 20375,
            HANDOFRECKONING_1                   = 62124,
            DIVINE_PLEA_1                       = 54428,
            REPENTANCE_1                        = 20066,
            TURN_EVIL_1                         = 10326,
            CRUSADER_STRIKE_1                   = 35395,
            JUDGEMENT_OF_LIGHT_1                = 20271,
            CONSECRATION_1                      = 26573,
            HAMMER_OF_JUSTICE_1                 = 853,
            DIVINE_STORM_1                      = 53385,
            HOW_1   /*Hammer of Wrath*/         = 24275,
            EXORCISM_1                          = 879,
            HOLY_WRATH_1                        = 2812,
            AVENGING_WRATH_1                    = 31884,
            BLESSING_OF_MIGHT_1                 = 19740,
            BLESSING_OF_WISDOM_1                = 19742,
            BLESSING_OF_KINGS_1                 = 20217,
            BLESSING_OF_SANCTUARY_1             = 20911,
            DEVOTION_AURA_1                     = 465,
            CONCENTRATION_AURA_1                = 19746,
        };
        enum PaladinPassives
        {
        //Talents
            DIVINE_PURPOSE                      = 31872,
            PURE/*Judgements of the Pure*/      = 54155,
            WISE/*Judgements of the Wise*/      = 31878,
            SACRED_CLEANSING                    = 53553,//rank 3
            RECKONING1                          = 20177,
            RECKONING2                          = 20179,
            RECKONING3                          = 20181,
            RECKONING4                          = 20180,
            RECKONING5                          = 20182,
            VINDICATION1                        = 9452 ,//rank 1
            VINDICATION2                        = 26016,//rank 2
            LAYHANDS  /*Improved LOH rank 2*/   = 20235,
            FANATICISM                          = 31881,//rank 3
            //RIGHTEOUS_VENGEANCE1                = 53380,//rank 1
            //RIGHTEOUS_VENGEANCE2                = 53381,//rank 2
            //RIGHTEOUS_VENGEANCE3                = 53382,//rank 3
            VENGEANCE1                          = 20049,//rank 1
            VENGEANCE2                          = 20056,//rank 2
            VENGEANCE3                          = 20057,//rank 3
            SHOFL1      /*Sheath of Light*/     = 53501,//rank 1
            SHOFL2                              = 53502,//rank 2
            SHOFL3                              = 53503,//rank 3
        //Glyphs
            GLYPH_HOLY_LIGHT                    = 54937,
        //other
            SPELLDMG/*Arcane Instability-mage*/ = 15060,//rank3 3% dam/crit
            SPELLDMG2/*Earth and Moon - druid*/ = 48511,//rank3 6% dam
            //PALADIN_T9_2P_BONUS                 = 67188,//Righteous Vengeance Crits
        };

        enum PaladinSpecial
        {
            NOAURA,
            DEVOTIONAURA,
            CONCENTRATIONAURA,
        };
    };
};

void AddSC_paladin_bot()
{
    new paladin_bot();
}
