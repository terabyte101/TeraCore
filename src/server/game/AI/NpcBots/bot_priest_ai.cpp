#include "bot_ai.h"
/*
Priest NpcBot (reworked by Graff onlysuffering@gmail.com)
Complete - Around 50%
TODO: maybe remove Divine Spirit or so, too much buffs
*/
class priest_bot : public CreatureScript
{
public:
    priest_bot() : CreatureScript("priest_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new priest_botAI(creature);
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

    struct priest_botAI : public bot_minion_ai
    {
        priest_botAI(Creature* creature) : bot_minion_ai(creature) { }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_PRIEST) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId, triggered);
        }

        bool MassGroupHeal(Player* player, const uint32 diff)
        {
            if (!PRAYER_OF_HEALING && !DIVINE_HYMN) return false;
            if (!player->GetGroup()) return false;
            if (Rand() > 30) return false;
            if (IsCasting()) return false;

            if (DIVINE_HYMN && Divine_Hymn_Timer <= diff)
            {
                Group* gr = player->GetGroup();
                uint8 LHPcount = 0;
                for (GroupReference* itr = gr->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* tPlayer = itr->getSource();
                    if (!tPlayer || me->GetMap() != tPlayer->GetMap() || 
                        tPlayer->isPossessed() || tPlayer->isCharmed()) continue;
                    if (tPlayer->isAlive())
                    {
                        if (me->GetExactDist(tPlayer) > 35) continue;
                        uint8 pct = 50 + tPlayer->getAttackers().size()*10;
                        pct = pct < 80 ? pct : 80;
                        if (GetHealthPCT(tPlayer) < pct && GetLostHP(tPlayer) > 4000)
                            ++LHPcount;
                    }
                    if (LHPcount > 1)
                        break;
                    if (!tPlayer->HaveBot()) continue;
                    for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
                    {
                        Creature* bot = tPlayer->GetBotMap()[i]._Cre();
                        if (bot && GetHealthPCT(bot) < 40 && me->GetExactDist(bot) < 30)
                            ++LHPcount;
                        if (LHPcount > 1)
                            break;
                    }
                }
                if (LHPcount > 1 && doCast(me, DIVINE_HYMN))
                {
                    Divine_Hymn_Timer = 180000; //3 min
                    return true;
                }
            }
            if (PRAYER_OF_HEALING)
            {
                Group* gr = player->GetGroup();
                Unit* castTarget = NULL;
                uint8 LHPcount = 0;
                for (GroupReference* itr = gr->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    uint8 lowestPCT = 100;
                    Player* tPlayer = itr->getSource();
                    if (!tPlayer || me->GetMap() != tPlayer->GetMap() || 
                        tPlayer->isPossessed() || tPlayer->isCharmed()) continue;
                    if (tPlayer->isAlive())
                    {
                        if (me->GetExactDist(tPlayer) > 25) continue;
                        if (GetHealthPCT(tPlayer) < 85)
                        {
                            ++LHPcount;
                            if (GetHealthPCT(tPlayer) < lowestPCT)
                            lowestPCT = GetHealthPCT(tPlayer);
                            castTarget = tPlayer;
                        }
                    }
                    if (LHPcount > 2)
                        break;
                    if (!tPlayer->HaveBot()) continue;
                    for (uint8 i = 0; i != tPlayer->GetMaxNpcBots(); ++i)
                    {
                        Creature* bot = tPlayer->GetBotMap()[i]._Cre();
                        if (bot && GetHealthPCT(bot) < 70 && me->GetExactDist(bot) < 15)
                        {
                            ++LHPcount;
                            if (GetHealthPCT(bot) < lowestPCT)
                            lowestPCT = GetHealthPCT(bot);
                            castTarget = bot;
                        }
                        if (LHPcount > 2)
                            break;
                    }
                }
                if (LHPcount > 2 && castTarget && doCast(castTarget, PRAYER_OF_HEALING))
                    return true;
            }
            return false;
        }//end MassGroupHeal

        bool ShieldTarget(Unit* target, const uint32 diff)
        {
            if (PWS_Timer > diff || Rand() > 50 || IsCasting()) return false;
            if (target->HasAura(WEAKENED_SOUL)) return false;
            if (HasAuraName(target, PW_SHIELD)) return false;
            //if (me->GetExactDist(target) > 40) return false;//checked already in HealTarget()

            if (!target->getAttackers().empty() || GetHealthPCT(target) < 33 || target->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE))
            {
                if (doCast(target, PW_SHIELD))
                {
                    if (me->getLevel() >= 30 || // improved
                        (target->ToCreature() && target->ToCreature()->GetIAmABot()))
                        PWS_Timer = 0;
                    else
                        PWS_Timer = 4000;
                    GC_Timer = 800;
                    return true;
                }
            }
            return false;
        }

        void StartAttack(Unit* u, bool force = false)
        {
            if (GetBotCommandState() == COMMAND_ATTACK && !force) return;
            Aggro(u);
            GetInPosition(force, true);
            SetBotCommandState(COMMAND_ATTACK);
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
            if (IAmDead()) return;
            if (!me->getVictim())
                Evade();
            if (wait == 0)
                wait = GetWait();
            else
                return;
            CheckAuras();
            Disperse(diff);
            BreakCC(diff);
            if (CCed(me)) return;
            DoDevCheck(diff);

            if (GetManaPCT(me) < 33 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, MANAPOTION))
                    Potion_cd = POTION_CD;
                GC_Timer = temptimer;
            }
            //check possible fear
            doDefend(diff);
            //buff and heal master's group
            MassGroupHeal(master, diff);
            BuffAndHealGroup(master, diff);
            CureGroup(master, DISPELMAGIC, diff);
            CureGroup(master, CURE_DISEASE, diff);
            //ShieldGroup(master);
            if (master->isInCombat() || me->isInCombat())
            {
                CheckDispel(diff);
                CheckSilence(diff);
            }
            if (me->isInCombat())
                CheckShackles(diff);

            if (!me->isInCombat())
                DoNonCombatActions(diff);

            if (!CheckAttackTarget(CLASS_PRIEST))
                return;

            AttackerSet m_attackers = master->getAttackers();
            AttackerSet b_attackers = me->getAttackers();

            if (GetHealthPCT(master) > 90 && GetManaPCT(me) > 35 && GetHealthPCT(me) > 90 && 
                (m_attackers.size() < 4 || b_attackers.size() + m_attackers.size() < 3) && 
                !IsCasting())
                //general rule
            {
                opponent = me->getVictim();
                if (opponent)
                {
                    if (!IsCasting())
                        StartAttack(opponent);
                }
                else
                    return;
                bool isBoss = opponent->GetTypeId() == TYPEID_UNIT ? opponent->ToCreature()->isWorldBoss() : false;
                if (me->GetExactDist(opponent) < 30)
                {
                    if (SW_DEATH && Rand() < 50 && SW_Death_Timer <= diff && 
                        (GetHealthPCT(opponent) < 15 || opponent->GetHealth() < me->GetMaxHealth()/6) && 
                        doCast(opponent, SW_DEATH))
                    {
                        SW_Death_Timer = 10000;
                        return;
                    }
                    if (Rand() < 30 && GC_Timer <= diff && !opponent->HasAura(SW_PAIN, me->GetGUID()) && 
                        opponent->GetHealth() > me->GetMaxHealth()/4 && 
                        doCast(opponent, SW_PAIN))
                        return;
                    if (VAMPIRIC_TOUCH && GC_Timer <= diff && !isBoss && Rand() < 50 && !opponent->HasAura(VAMPIRIC_TOUCH, me->GetGUID()) && 
                        opponent->GetHealth() > me->GetMaxHealth()/4 && 
                        doCast(opponent, VAMPIRIC_TOUCH))
                        return;
                    if (DEVOURING_PLAGUE && GC_Timer <= diff && !isBoss && Rand() < 30 && !Devcheck && !opponent->HasAura(DEVOURING_PLAGUE, me->GetGUID()) && 
                        opponent->GetHealth() > me->GetMaxHealth()/3 && 
                        doCast(opponent, DEVOURING_PLAGUE))
                        return;
                    if (Mind_Blast_Timer <= diff && GC_Timer <= 300 && !isBoss && Rand() < 50 && (!VAMPIRIC_TOUCH || HasAuraName(opponent, VAMPIRIC_TOUCH)) && 
                        doCast(opponent, MIND_BLAST))
                    {
                        Mind_Blast_Timer = 7500 - me->getLevel()/4*100;//5.5 sec on 80 lvl (as improved)
                        return;
                    }
                    if (MIND_FLAY && Mind_Flay_Timer <= diff && GC_Timer <= 300 && !isBoss && !me->isMoving() && Rand() < 40 && me->GetExactDist(opponent) < 30 && 
                        
                        (opponent->isMoving() || opponent->GetHealth() < me->GetMaxHealth()/3 || 
                        (opponent->HasAura(SW_PAIN, me->GetGUID()) && 
                        opponent->HasAura(DEVOURING_PLAGUE, me->GetGUID()))) && 
                        doCast(opponent, MIND_FLAY))
                    {
                        Mind_Flay_Timer = 3000;
                        return;
                    }
                    if (MIND_SEAR && GC_Timer <= diff && !me->isMoving() && !opponent->isMoving() && Rand() < 50 && 
                        opponent->HasAura(SW_PAIN, me->GetGUID()) && 
                        opponent->HasAura(DEVOURING_PLAGUE, me->GetGUID()))
                        if (Unit* u = FindSplashTarget(30, opponent))
                            if (doCast(u, MIND_SEAR))
                                return;
                }//endif opponent
            }//endif damage
            //check horror after dots/damage
            if (PSYCHIC_HORROR && Psychic_Horror_Timer <= diff && Rand() < 30 && 
                opponent->GetCreatureType() != CREATURE_TYPE_UNDEAD && 
                opponent->GetHealth() > me->GetMaxHealth()/5 && 
                me->GetExactDist(opponent) < 30 && !HasAuraName(opponent, PSYCHIC_HORROR) && 
                !CCed(opponent))
            {
                if (doCast(opponent, PSYCHIC_HORROR))
                {
                    Psychic_Horror_Timer = 60000;
                    return;
                }
            }
        }//end UpdateAI

        bool HealTarget(Unit* target, uint8 hp, const uint32 diff)
        {
            if (hp > 98) return false;
            if (!target || target->isDead() || me->GetExactDist(target) > 40)
                return false;
            if (Rand() > 50 + 20*target->isInCombat() + 50*master->GetMap()->IsRaid()) return false;

            //GUARDIAN SPIRIT
            if (GUARDIAN_SPIRIT && Guardian_Spirit_Timer <= diff && Rand() < 70 && 
                target->isInCombat() && !target->getAttackers().empty() && 
                hp < (5 + std::min(20, uint8(target->getAttackers().size())*5)) && 
                (master->GetGroup() && master->GetGroup()->IsMember(target->GetGUID()) || target == master) && 
                !target->HasAura(GUARDIAN_SPIRIT))
            {
                temptimer = GC_Timer;
                if (me->IsNonMeleeSpellCasted(true))
                    me->InterruptNonMeleeSpells(true);
                if (doCast(target, GUARDIAN_SPIRIT))
                {
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (target->HasAura(GUARDIAN_SPIRIT, me->GetGUID()))
                            me->MonsterWhisper("Guardin Spirit on you!", target->GetGUID());
                        Guardian_Spirit_Timer = 90000;//1.5 min
                    }
                    else
                        Guardian_Spirit_Timer = 30000;//30 sec for creatures
                    GC_Timer = temptimer;
                    return true;
                }
            }

            if (IsCasting()) return false;

            //PAIN SUPPRESSION
            if (hp < 35 && PAIN_SUPPRESSION && Pain_Suppression_Timer <= diff && Rand() < 50 && 
                (target->isInCombat() || !target->getAttackers().empty()) && 
                !target->HasAura(PAIN_SUPPRESSION))
            {
                temptimer = GC_Timer;
                if (doCast(target, PAIN_SUPPRESSION))
                {
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (target->HasAura(PAIN_SUPPRESSION, me->GetGUID()))
                            me->MonsterWhisper("Pain Suppression on you!", target->GetGUID());
                        Pain_Suppression_Timer = 45000;//45 sec
                    }
                    else
                        Pain_Suppression_Timer = 15000;//15 sec for creatures
                    GC_Timer = temptimer;
                    return true;
                }
            }

            //Now Heals Requires GCD
            if ((hp < 80 || !target->getAttackers().empty()) && 
                PWS_Timer <= diff && ShieldTarget(target, diff))
                return true;

            //PENANCE/Greater Heal
            if (hp < 75 || GetLostHP(target) > 4000)
            {
                if (PENANCE && Penance_Timer <= diff && 
                    !me->isMoving() && //better check then stop moving every try (furthermore it doesn't always work properly)
                    (target->GetTypeId() != TYPEID_PLAYER || !(target->ToPlayer()->isCharmed() || target->ToPlayer()->isPossessed())) && 
                    doCast(target, PENANCE))
                {
                    Penance_Timer = 8000;
                    return true;
                }
                else if (Heal_Timer <= diff && GC_Timer <= diff && hp > 50 && doCast(target, HEAL))
                {
                    Heal_Timer = 2500;
                    return true;
                }
            }
            //Flash Heal
            if (((hp > 75 && hp < 90) || hp < 50 || GetLostHP(target) > 1500) && 
                GC_Timer <= diff && FLASH_HEAL && 
                doCast(target, FLASH_HEAL))
                    return true;
            //maintain HoTs
            Unit* u = target->getVictim();
            Creature* boss = u && u->ToCreature() && u->ToCreature()->isWorldBoss() ? u->ToCreature() : NULL;
            bool tanking = tank == target && boss;
            //Renew
            if (((hp < 98 && hp > 70) || GetLostHP(target) > 500 || tanking) && 
                !HasAuraName(target, RENEW, me->GetGUID()) && 
                GC_Timer <= diff && doCast(target, RENEW))
            {
                GC_Timer = 800;
                return true;
            }

            return false;
        }

        bool BuffTarget(Unit* target, const uint32 diff)
        {
            if (GC_Timer > diff || Rand() > 60) return false;
            if (!target || target->isDead() || me->GetExactDist(target) > 30) return false;

            if (Fear_Ward_Timer <= diff && !HasAuraName(target, FEAR_WARD) && doCast(target, FEAR_WARD))
            {
                Fear_Ward_Timer = target->GetTypeId() == TYPEID_PLAYER ? 60000 : 30000;//30sec for bots
                GC_Timer = 800;
                return true;
            }

            if (target == me)
            {
                if (!me->HasAura(INNER_FIRE) && doCast(me, INNER_FIRE))
                {
                    GC_Timer = 800;
                    return true;
                }
                if (VAMPIRIC_EMBRACE && !me->HasAura(VAMPIRIC_EMBRACE) && doCast(me, VAMPIRIC_EMBRACE))
                {
                    GC_Timer = 800;
                    return true;
                }
            }

            if (me->isInCombat() && !master->GetMap()->IsRaid()) return false;

            if (Rand() < 70 && !HasAuraName(target, PW_FORTITUDE) && doCast(target, PW_FORTITUDE))
            {
                /*GC_Timer = 800;*/
                return true;
            }
            if (Rand() < 30 && !HasAuraName(target, SHADOW_PROTECTION) && doCast(target, SHADOW_PROTECTION))
            {
                /*GC_Timer = 800;*/
                return true;
            }
            if (Rand() < 30 && !HasAuraName(target, DIVINE_SPIRIT) && doCast(target, DIVINE_SPIRIT))
            {
                /*GC_Timer = 800;*/
                return true;
            }
            return false;
        }

        void DoNonCombatActions(const uint32 diff)
        {
            if (Rand() > 50 || GC_Timer > diff || me->IsMounted()) return;

            RezGroup(RESURRECTION, master);

            if (Feasting()) return;

            if (BuffTarget(master, diff))
                return;
            if (BuffTarget(me, diff))
                return;
        }

        void CheckDispel(const uint32 diff)
        {
            if (CheckDispelTimer > diff || Rand() > 25 || IsCasting())
                return;
            Unit* target = FindHostileDispelTarget();
            if (target && doCast(target, DISPELMAGIC))
            {
                CheckDispelTimer = 3000;
                GC_Timer = 500;
            }
            CheckDispelTimer = 1000;
        }

        void CheckShackles(const uint32 diff)
        {
            if (!SHACKLE_UNDEAD || ShackleTimer > diff || GC_Timer > diff || IsCasting())
                return;
            if (FindAffectedTarget(SHACKLE_UNDEAD, me->GetGUID()))
                return;
            Unit* target = FindUndeadCCTarget(30, SHACKLE_UNDEAD);
            if (target && doCast(target, SHACKLE_UNDEAD))
            {
                ShackleTimer = 3000;
                GC_Timer = 800;
            }
        }

        void CheckSilence(const uint32 diff)
        {
            if (IsCasting()) return;
            temptimer = GC_Timer;
            if (SILENCE && Silence_Timer <= diff)
            {
                if (Unit* target = FindCastingTarget(30))
                    if (doCast(target, SILENCE))
                        Silence_Timer = 30000;
            }
            else if (PSYCHIC_HORROR && Psychic_Horror_Timer <= 20000)
            {
                if (Unit* target = FindCastingTarget(30))
                    if (doCast(target, PSYCHIC_HORROR))
                        Psychic_Horror_Timer = 60000;
            }
            GC_Timer = temptimer;
        }

        void doDefend(const uint32 diff)
        {
            AttackerSet m_attackers = master->getAttackers();
            AttackerSet b_attackers = me->getAttackers();
            //fear master's attackers
            if (!m_attackers.empty() && PSYCHIC_SCREAM && Fear_Timer <= diff && 
                (master != tank || GetHealthPCT(master) < 75))
            {
                uint8 tCount = 0;
                for (AttackerSet::iterator iter = m_attackers.begin(); iter != m_attackers.end(); ++iter)
                {
                    if (!(*iter)) continue;
                    if ((*iter)->GetCreatureType() == CREATURE_TYPE_UNDEAD) continue;
                    if (me->GetExactDist((*iter)) > 7) continue;
                    if (CCed(*iter) && me->GetExactDist((*iter)) > 5) continue;
                    if (me->IsValidAttackTarget(*iter))
                        ++tCount;
                }
                if (tCount > 1 && doCast(me, PSYCHIC_SCREAM))
                {
                    Fear_Timer = 24000;//with improved 24 sec
                    return;
                }
            }

            // Defend myself (psychic horror)
            if (!b_attackers.empty() && PSYCHIC_SCREAM && Fear_Timer <= diff)
            {
                uint8 tCount = 0;
                for (AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
                {
                    if (!(*iter)) continue;
                    if ((*iter)->GetCreatureType() == CREATURE_TYPE_UNDEAD) continue;
                    if (me->GetExactDist((*iter)) > 7) continue;
                    if (CCed(*iter) && me->GetExactDist((*iter)) > 5) continue;
                    if (me->IsValidAttackTarget(*iter))
                        ++tCount;
                }
                if (tCount > 0 && doCast(me, PSYCHIC_SCREAM))
                {
                    Fear_Timer = 24000;//with improved 24 sec
                    return;
                }
            }
            // Heal myself
            if (GetHealthPCT(me) < 99 && !b_attackers.empty())
            {
                if (ShieldTarget(me, diff)) return;

                if (FADE && Fade_Timer <= diff && me->isInCombat())
                {
                    if (b_attackers.empty()) return; //no aggro
                    uint8 Tattackers = 0;
                    for (AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
                    {
                        if (!(*iter)) continue;
                        if ((*iter)->isDead()) continue;
                        if (!(*iter)->ToCreature()) continue;
                        if (!(*iter)->CanHaveThreatList()) continue;
                        if (me->GetExactDist((*iter)) <= 15)
                            Tattackers++;
                    }
                    if (Tattackers > 0)
                    {
                        temptimer = GC_Timer;
                        if (doCast(me, FADE))
                        {
                            for (AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
                                if ((*iter)->getThreatManager().getThreat(me) > 0.f)
                                    (*iter)->getThreatManager().modifyThreatPercent(me, -50);
                            Fade_Timer = 6000;
                            GC_Timer = temptimer;
                            return;
                        }
                    }
                }
                if (GetHealthPCT(me) < 90 && HealTarget(me, GetHealthPCT(me), diff))
                    return;
            }
        }

        void DoDevCheck(const uint32 diff)
        {
            if (DevcheckTimer <= diff)
            {
                Devcheck = FindAffectedTarget(DEVOURING_PLAGUE, me->GetGUID());
                DevcheckTimer = 5000;
            }
        }

        void Disperse(const uint32 diff)
        {
            if (!DISPERSION || GC_Timer > diff || Dispersion_Timer > diff || IsCasting()) return;
            //attackers case
            if ((me->getAttackers().size() > 3 && Fade_Timer > diff && GetHealthPCT(me) < 90) || 
                (GetHealthPCT(me) < 20 && me->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE)) || 
                (GetManaPCT(me) < 30) || 
                (me->getAttackers().size() > 1 && me->HasAuraWithMechanic((1<<MECHANIC_SNARE)|(1<<MECHANIC_ROOT))))
            {
                temptimer = GC_Timer;
                if (doCast(me, DISPERSION))
                    Dispersion_Timer = 75000;//with glyph
                GC_Timer = temptimer;
                return;
            }
            Dispersion_Timer = 2000;//fail
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
            Divine_Hymn_Timer = 0;
            Pain_Suppression_Timer = 0;
            Guardian_Spirit_Timer = 0;
            PWS_Timer = 0;
            Fade_Timer = 0;
            Fear_Timer = 0;
            Mind_Blast_Timer = 0;
            SW_Death_Timer = 0;
            Fear_Ward_Timer = 0;
            Psychic_Horror_Timer = 0;
            Silence_Timer = 0;
            Dispersion_Timer = 0;
            Mind_Flay_Timer = 0;
            Penance_Timer = 0;
            CheckDispelTimer = 0;
            ShackleTimer = 0;
            DevcheckTimer = 20;
            Devcheck = false;

            if (master)
            {
                setStats(CLASS_PRIEST, me->getRace(), master->getLevel(), true);
                ApplyClassPassives();
                ApplyPassives(CLASS_PRIEST);
            }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (Heal_Timer > diff)                 Heal_Timer -= diff;
            if (Fade_Timer > diff)                 Fade_Timer -= diff;
            if (Divine_Hymn_Timer > diff)          Divine_Hymn_Timer -= diff;
            if (Pain_Suppression_Timer > diff)     Pain_Suppression_Timer -= diff;
            if (Guardian_Spirit_Timer > diff)      Guardian_Spirit_Timer -= diff;
            if (PWS_Timer > diff)                  PWS_Timer -= diff;
            if (Fear_Timer > diff)                 Fear_Timer -= diff;
            if (Mind_Blast_Timer > diff)           Mind_Blast_Timer -= diff;
            if (SW_Death_Timer > diff)             SW_Death_Timer -= diff;
            if (Fear_Ward_Timer > diff)            Fear_Ward_Timer -= diff;
            if (Psychic_Horror_Timer > diff)       Psychic_Horror_Timer -= diff;
            if (Silence_Timer > diff)              Silence_Timer -= diff;
            if (Dispersion_Timer > diff)           Dispersion_Timer -= diff;
            if (Mind_Flay_Timer > diff)            Mind_Flay_Timer -= diff;
            if (Penance_Timer > diff)              Penance_Timer -= diff;
            if (CheckDispelTimer > diff)           CheckDispelTimer -= diff;
            if (ShackleTimer > diff)               ShackleTimer -= diff;
            if (DevcheckTimer > diff)              DevcheckTimer -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            uint8 lvl = me->getLevel();
            DISPELMAGIC                = lvl >= 70 ? MASS_DISPEL_1 : InitSpell(me, DISPEL_MAGIC_1);
            CURE_DISEASE                            = InitSpell(me, CURE_DISEASE_1);
            FEAR_WARD                               = InitSpell(me, FEAR_WARD_1);
  /*Talent*/PAIN_SUPPRESSION            = lvl >= 50 ? PAIN_SUPPRESSION_1 : 0;
            PSYCHIC_SCREAM                          = InitSpell(me, PSYCHIC_SCREAM_1);
            FADE                                    = InitSpell(me, FADE_1);
  /*Talent*/PSYCHIC_HORROR              = lvl >= 50 ? PSYCHIC_HORROR_1 : 0;
  /*Talent*/SILENCE                     = lvl >= 30 ? SILENCE_1 : 0;
  /*Talent*/PENANCE                     = lvl >= 60 ? InitSpell(me, PENANCE_1) : 0;
  /*Talent*/VAMPIRIC_EMBRACE            = lvl >= 30 ? VAMPIRIC_EMBRACE_1 : 0;
  /*Talent*/DISPERSION                  = lvl >= 60 ? DISPERSION_1 : 0;
            MIND_SEAR                               = InitSpell(me, MIND_SEAR_1);
  /*Talent*/GUARDIAN_SPIRIT             = lvl >= 60 ? GUARDIAN_SPIRIT_1 : 0;
            SHACKLE_UNDEAD                          = InitSpell(me, SHACKLE_UNDEAD_1);
            HEAL                        = lvl >= 40 ? InitSpell(me, GREATER_HEAL_1) : lvl >= 16 ? InitSpell(me, NORMAL_HEAL_1) : InitSpell(me, LESSER_HEAL_1);
            RENEW                                   = InitSpell(me, RENEW_1);
            FLASH_HEAL                              = InitSpell(me, FLASH_HEAL_1);
            PRAYER_OF_HEALING                       = InitSpell(me, PRAYER_OF_HEALING_1);
            DIVINE_HYMN                             = InitSpell(me, DIVINE_HYMN_1);
            RESURRECTION                            = InitSpell(me, RESURRECTION_1);
            PW_SHIELD                               = InitSpell(me, PW_SHIELD_1);
            INNER_FIRE                              = InitSpell(me, INNER_FIRE_1);
            PW_FORTITUDE                            = InitSpell(me, PW_FORTITUDE_1);
            SHADOW_PROTECTION                       = InitSpell(me, SHADOW_PROTECTION_1);
            DIVINE_SPIRIT                           = InitSpell(me, DIVINE_SPIRIT_1);
            SW_PAIN                                 = InitSpell(me, SW_PAIN_1);
            MIND_BLAST                              = InitSpell(me, MIND_BLAST_1);
            SW_DEATH                                = InitSpell(me, SW_DEATH_1);
            DEVOURING_PLAGUE                        = InitSpell(me, DEVOURING_PLAGUE_1);
  /*Talent*/MIND_FLAY                   = lvl >= 20 ? InitSpell(me, MIND_FLAY_1) : 0;
  /*Talent*/VAMPIRIC_TOUCH              = lvl >= 50 ? InitSpell(me, VAMPIRIC_TOUCH_1) : 0;
        }
        void ApplyClassPassives()
        {
            uint8 level = master->getLevel();
            if (level >= 65)
                RefreshAura(BORROWED_TIME); //25%haste/40%bonus
            if (level >= 55)
                RefreshAura(DIVINE_AEGIS); //30%
            if (level >= 55)
                RefreshAura(EMPOWERED_RENEW3); //15%
            else if (level >= 50)
                RefreshAura(EMPOWERED_RENEW2); //10%
            else if (level >= 45)
                RefreshAura(EMPOWERED_RENEW1); //5%
            if (level >= 45)
                RefreshAura(BODY_AND_SOUL1); //30%
            if (level >= 50)
                RefreshAura(PAINANDSUFFERING3); //100%
            else if (level >= 48)
                RefreshAura(PAINANDSUFFERING2); //66%
            else if (level >= 45)
                RefreshAura(PAINANDSUFFERING1); //33%
            if (level >= 50)
                RefreshAura(MISERY3); //3%
            else if (level >= 48)
                RefreshAura(MISERY2); //2%
            else if (level >= 45)
                RefreshAura(MISERY1); //1%
            if (level >= 45)
                RefreshAura(GRACE); //100%
            if (level >= 35)
                RefreshAura(IMP_DEV_PLAG); //30%
            if (level >= 25)
                RefreshAura(INSPIRATION3); //10%
            else if (level >= 23)
                RefreshAura(INSPIRATION2); //6%
            else if (level >= 20)
                RefreshAura(INSPIRATION1); //3%
            if (level >= 30)
                RefreshAura(SHADOW_WEAVING3); //100%
            else if (level >= 28)
                RefreshAura(SHADOW_WEAVING2); //66%
            else if (level >= 25)
                RefreshAura(SHADOW_WEAVING1); //33%
            if (level >= 15)
            {
                RefreshAura(GLYPH_SW_PAIN);
                RefreshAura(GLYPH_PW_SHIELD); //20% heal
            }
            if (level >= 40)
                RefreshAura(SHADOWFORM); //allows dots to crit, passive
            if (level >= 70)
                RefreshAura(PRIEST_T10_2P_BONUS);
        }

    private:
        uint32
   /*Buffs*/INNER_FIRE, PW_FORTITUDE, DIVINE_SPIRIT, SHADOW_PROTECTION,
    /*Disc*/FEAR_WARD, PAIN_SUPPRESSION, SHACKLE_UNDEAD, PW_SHIELD, DISPELMAGIC, CURE_DISEASE, PENANCE,
    /*Holy*/HEAL, FLASH_HEAL, RENEW, PRAYER_OF_HEALING, DIVINE_HYMN, GUARDIAN_SPIRIT, RESURRECTION,
  /*Shadow*/SW_PAIN, MIND_BLAST, SW_DEATH, DEVOURING_PLAGUE, MIND_FLAY, VAMPIRIC_TOUCH,
  /*Shadow*/PSYCHIC_SCREAM, FADE, PSYCHIC_HORROR, VAMPIRIC_EMBRACE, DISPERSION, MIND_SEAR, SILENCE;
        //Timers/other
/*Disc*/uint32 Penance_Timer, PWS_Timer, Pain_Suppression_Timer, Fear_Ward_Timer;
/*Holy*/uint32 Heal_Timer, Divine_Hymn_Timer, Guardian_Spirit_Timer;
/*Shdw*/uint32 Fade_Timer, Fear_Timer, Mind_Blast_Timer, SW_Death_Timer, Mind_Flay_Timer,
/*Shdw*/    Psychic_Horror_Timer, Silence_Timer, Dispersion_Timer;
/*Misc*/uint16 CheckDispelTimer, ShackleTimer, DevcheckTimer;
/*Misc*/bool Devcheck;

        enum PriestBaseSpells
        {
            DISPEL_MAGIC_1                      = 527,
            MASS_DISPEL_1                       = 32375,
            CURE_DISEASE_1                      = 528,
            FEAR_WARD_1                         = 6346,
  /*Talent*/PAIN_SUPPRESSION_1                  = 33206,
            PSYCHIC_SCREAM_1                    = 8122,
            FADE_1                              = 586,
  /*Talent*/PSYCHIC_HORROR_1                    = 64044,
  /*Talent*/SILENCE_1                           = 15487,
  /*Talent*/PENANCE_1                           = 47540,
  /*Talent*/VAMPIRIC_EMBRACE_1                  = 15286,
  /*Talent*/DISPERSION_1                        = 47585,
            MIND_SEAR_1                         = 48045,
  /*Talent*/GUARDIAN_SPIRIT_1                   = 47788,
            SHACKLE_UNDEAD_1                    = 9484,
            LESSER_HEAL_1                       = 2050,
            NORMAL_HEAL_1                       = 2054,
            GREATER_HEAL_1                      = 2060,
            RENEW_1                             = 139,
            FLASH_HEAL_1                        = 2061,
            PRAYER_OF_HEALING_1                 = 596,
            DIVINE_HYMN_1                       = 64843,
            RESURRECTION_1                      = 2006,
            PW_SHIELD_1                         = 17,
            INNER_FIRE_1                        = 588,
            PW_FORTITUDE_1                      = 1243,
            SHADOW_PROTECTION_1                 = 976,
            DIVINE_SPIRIT_1                     = 14752,
            SW_PAIN_1                           = 589,
            MIND_BLAST_1                        = 8092,
            SW_DEATH_1                          = 32379,
            DEVOURING_PLAGUE_1                  = 2944,
  /*Talent*/MIND_FLAY_1                         = 15407,
  /*Talent*/VAMPIRIC_TOUCH_1                    = 34914,
        };
        enum PriestPassives
        {
            SHADOWFORM  /*For DOT crits*/   = 49868,
        //Talents
            IMP_DEV_PLAG                    = 63627,//rank 3
            MISERY1                         = 33191,
            MISERY2                         = 33192,
            MISERY3                         = 33193,
            PAINANDSUFFERING1               = 47580,
            PAINANDSUFFERING2               = 47581,
            PAINANDSUFFERING3               = 47582,
            SHADOW_WEAVING1                 = 15257,
            SHADOW_WEAVING2                 = 15331,
            SHADOW_WEAVING3                 = 15332,
            DIVINE_AEGIS                    = 47515,//rank 3
            BORROWED_TIME                   = 52800,//rank 5
            GRACE                           = 47517,//rank 2
            EMPOWERED_RENEW1                = 63534,
            EMPOWERED_RENEW2                = 63542,
            EMPOWERED_RENEW3                = 63543,
            INSPIRATION1                    = 14892,
            INSPIRATION2                    = 15362,
            INSPIRATION3                    = 15363,
            BODY_AND_SOUL1                  = 64127,
        //Glyphs
            GLYPH_SW_PAIN                   = 55681,
            GLYPH_PW_SHIELD                 = 55672,
        //other
            PRIEST_T10_2P_BONUS             = 70770,//33% renew
        };
        enum PriestSpecial
        {
            WEAKENED_SOUL                   = 6788,
        };
    }; //end priest_bot
};

void AddSC_priest_bot()
{
    new priest_bot();
}
