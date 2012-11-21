#include "bot_ai.h"
/*
Hunter NpcBot (reworked by Graff onlysuffering@gmail.com)
Complete - 1%
TODO:
*/
class hunter_bot : public CreatureScript
{
public:
    hunter_bot() : CreatureScript("hunter_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new hunter_botAI(creature);
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

    struct hunter_botAI : public bot_minion_ai
    {
        hunter_botAI(Creature* creature) : bot_minion_ai(creature)
        {
            Reset();
        }

        //void CreatePet()
        //{

        //    pet = me->GetBotsPet(60238);

        //    if (pet == NULL)
        //        return;

        //    pet->UpdateCharmAI();
        //    pet->setFaction(me->getFaction());
        //    pet->SetReactState(REACT_DEFENSIVE);
        //    pet->GetMotionMaster()->MoveFollow(me, PET_FOLLOW_DIST*urand(1, 2),PET_FOLLOW_ANGLE);
        //    CharmInfo* charmInfonewbot = pet->InitCharmInfo();
        //    pet->GetCharmInfo()->SetCommandState(COMMAND_FOLLOW);
        //    pet->UpdateStats(STAT_STRENGTH);
        //    pet->UpdateStats(STAT_AGILITY);
        //    pet->SetLevel(master->getLevel());

        //    /*float val2 = master->getLevel()*4.f + pet->GetStat(STAT_STRENGTH)*5.f;

        //    val2=100.0;
        //    uint32 attPowerMultiplier=1;
        //    pet->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, uint32(val2));
        //    pet->UpdateAttackPowerAndDamage();
        //    pet->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, uint32(val2 * attPowerMultiplier));
        //    pet->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, uint32(val2 * attPowerMultiplier)*3+master->getLevel());
        //    pet->UpdateDamagePhysical(BASE_ATTACK);*/

        //}

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_HUNTER) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId, triggered);
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

        void UpdateAI(const uint32 diff)
        {
            ReduceCD(diff);

            if (IAmDead()) return;

            if (!me->isInCombat())
                DoNonCombatActions(diff);

            //if (pet && pet != NULL && pet->isDead())
            //{
            //    me->SetBotsPetDied();
            //    pet = NULL;
            //}

            //if we think we have a pet, but master doesn't, it means we teleported
            //if (pet && !me->getBotsPet())
            //{
            //    me->SetBotsPetDied();
            //    pet = NULL;
            //}

            DoNormalAttack(diff);
            ScriptedAI::UpdateAI(diff);

            //if low on health, drink a potion
            if (GetHealthPCT(me) < 65)
            {
                if (doCast(me, HEALINGPOTION))
                    Potion_cd = POTION_CD;
            }

            //if low on mana, drink a potion
            if (GetManaPCT(me) < 65 && Potion_cd <= diff)
            {
                if (doCast(me, MANAPOTION))
                    Potion_cd = POTION_CD;
            }

            opponent = SelectTarget(SELECT_TARGET_TOPAGGRO, 0);
            if (!opponent && !me->getVictim())
            {
                me->CombatStop();
                //ResetOrGetNextTarget();

                //to reduce the number of crashes, remove pet whenever we are not in combat
                //if (pet != NULL && pet->isAlive())
                //{
                //    me->SetBotsPetDied();
                //    pet = NULL;
                //}
                return;
            }

            //if (pet == NULL)
            //    CreatePet();

            //if (pet && pet->isAlive() &&
            //    !pet->isInCombat() &&
            //    me->getVictim())
            //{
            //    pet->Attack (me->getVictim(), true);
            //    pet->GetMotionMaster()->MoveChase(me->getVictim(), 1, 0);
            //}
        }

        void DoNormalAttack(const uint32 diff)
        {
            AttackerSet m_attackers = master->getAttackers();
            if (!opponent || opponent->isDead()) return;

            // try to get rid of enrage effect
            if (TRANQ_SHOT && (HasAuraName(opponent, "Enrage") || (HasAuraName(opponent, "Frenzy")))) 
            {
                me->InterruptNonMeleeSpells(true, AUTO_SHOT);
                me->MonsterSay("Tranquil shot!", LANG_UNIVERSAL, opponent->GetGUID());
                doCast(opponent, TRANQ_SHOT, true);
              //  doCast(opponent, AUTO_SHOT);
              //  return;
            }

            // silence it
            if (SILENCING_SHOT && opponent->HasUnitState(UNIT_STATE_CASTING) && SilencingShot_Timer <= diff)
            {
                doCast(opponent, SILENCING_SHOT);
                SilencingShot_Timer = 25000;
              //  doCast(opponent, AUTO_SHOT);
              //  return;
            }

            // mark it
            if (!HasAuraName(opponent, "Hunter's Mark"))
            {
                doCast(opponent, HUNTERS_MARK);
              //  doCast(opponent, AUTO_SHOT);
              //  return;
            }

            // sting it
            if (SCORPID_STING && !opponent->HasAura(SCORPID_STING, me->GetGUID())) 
            {
                me->InterruptNonMeleeSpells(true, AUTO_SHOT);
                doCast(opponent, SCORPID_STING);
               // me->MonsterSay("Scorpid Sting!", LANG_UNIVERSAL, NULL);
               // doCast(opponent, AUTO_SHOT);
               // return;
            }

             if (CHIMERA_SHOT && ChimeraShot_Timer <= diff && GC_Timer <= diff)
             {
                me->InterruptNonMeleeSpells(true, AUTO_SHOT);
                doCast(opponent, CHIMERA_SHOT);
                ChimeraShot_Timer = 10000;
               // me->MonsterSay("Chimera Sting!", LANG_UNIVERSAL, NULL);
              //  doCast(opponent, AUTO_SHOT);
              //  return;
            }

            if (ARCANE_SHOT && ArcaneShot_cd <= diff && GC_Timer <= diff)
            {
                me->InterruptNonMeleeSpells(true, AUTO_SHOT);
                doCast(opponent, ARCANE_SHOT);
               // me->MonsterSay("Arcane shot!", LANG_UNIVERSAL, NULL);
                ArcaneShot_cd = 60;
              //  doCast(opponent, AUTO_SHOT);
              //  return;
            }

            if (AIMED_SHOT && AimedShot_Timer <= diff && GC_Timer <= diff)
            {
                me->InterruptNonMeleeSpells( true, AUTO_SHOT );
                doCast(opponent, AIMED_SHOT);
               // me->MonsterSay("Aimed shot!", LANG_UNIVERSAL, NULL);
                AimedShot_Timer = 120;
              //  doCast(opponent, AUTO_SHOT);
              //  return;
            }
            //Temp Feign death For Debug
            AttackerSet b_attackers = me->getAttackers();
            if (!b_attackers.empty())
            {
            for(AttackerSet::iterator iter = b_attackers.begin(); iter != b_attackers.end(); ++iter)
                if (*iter && (*iter)->getVictim()->GetGUID() == me->GetGUID() && 
                    me->GetDistance(*iter) < 10 && 
                    Feign_Death_Timer <= diff && GC_Timer <= diff)
                {
                    doCast(me, FEIGN_DEATH, true);
                    opponent->AddThreat(me, -100000);
                    me->CombatStop();
                    Feign_Death_Timer = 25000;
                    me->CombatStart(opponent);
                }
            }

            doCast(opponent, AUTO_SHOT);
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
            ArcaneShot_cd = 0;
            ChimeraShot_Timer = 0;
            SilencingShot_Timer = 0;
            AimedShot_Timer = 0;
            Feign_Death_Timer = 0;

            if (master)
            {
                setStats(CLASS_HUNTER, me->getRace(), master->getLevel(), true);
                ApplyClassPassives();
                ApplyPassives(CLASS_HUNTER);
             }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (ArcaneShot_cd > diff)               ArcaneShot_cd -= diff;
            if (ChimeraShot_Timer > diff)           ChimeraShot_Timer -= diff;
            if (SilencingShot_Timer > diff)         SilencingShot_Timer -= diff;
            if (AimedShot_Timer > diff)             AimedShot_Timer -= diff;
            if (Feign_Death_Timer > diff)           Feign_Death_Timer -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            uint8 lvl = me->getLevel();
            AUTO_SHOT                               = AUTO_SHOT_1;
            TRANQ_SHOT                              = InitSpell(me, TRANQ_SHOT_1);
            SCORPID_STING                           = InitSpell(me, SCORPID_STING_1);
            HUNTERS_MARK                            = InitSpell(me, HUNTERS_MARK_1);
            ARCANE_SHOT                             = InitSpell(me, ARCANE_SHOT_1);
            CHIMERA_SHOT                = lvl >= 60 ? CHIMERA_SHOT_1 : 0;
            AIMED_SHOT                  = lvl >= 20 ? InitSpell(me, AIMED_SHOT_1) : 0;
            SILENCING_SHOT              = lvl >= 50 ? SILENCING_SHOT_1 : 0;
            ASPECT_OF_THE_DRAGONHAWK                = InitSpell(me, ASPECT_OF_THE_DRAGONHAWK_1);
            FEIGN_DEATH                             = InitSpell(me, FEIGN_DEATH_1);
        }

        void ApplyClassPassives()
        { }

    private:
        uint32
        AUTO_SHOT, TRANQ_SHOT, SCORPID_STING, HUNTERS_MARK, ARCANE_SHOT, CHIMERA_SHOT, AIMED_SHOT, 
        SILENCING_SHOT, ASPECT_OF_THE_DRAGONHAWK, FEIGN_DEATH;
        //Timers
        uint32 ArcaneShot_cd, ChimeraShot_Timer, SilencingShot_Timer, AimedShot_Timer, Feign_Death_Timer;

        enum HunterBaseSpells
        {
            AUTO_SHOT_1                         = 75,
            TRANQ_SHOT_1                        = 19801,
            SCORPID_STING_1                     = 3043,
            HUNTERS_MARK_1                      = 14325,
            ARCANE_SHOT_1                       = 3044,
            CHIMERA_SHOT_1                      = 53209,
            AIMED_SHOT_1                        = 19434,
            SILENCING_SHOT_1                    = 34490,
            ASPECT_OF_THE_DRAGONHAWK_1          = 61846,
            FEIGN_DEATH_1                       = 5384,
        };

        enum HunterPassives
        {
        };

        enum HunterSpecial
        {
        };
    };
};

void AddSC_hunter_bot()
{
    new hunter_bot();
}
