#include "bot_ai.h"
/*
Warlock NpcBot (reworked by Graff onlysuffering@gmail.com)
Voidwalker pet AI included
Complete - 3%
TODO:
*/
class warlock_bot : public CreatureScript
{
public:
    warlock_bot() : CreatureScript("warlock_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new warlock_botAI(creature);
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

    struct warlock_botAI : public bot_minion_ai
    {
        warlock_botAI(Creature* creature) : bot_minion_ai(creature) { }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_WARRIOR) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId, triggered);
        }

        void EnterCombat(Unit* u) { OnEnterCombat(u); }
        void Aggro(Unit*) { }
        void AttackStart(Unit*) { }
        void KilledUnit(Unit*) { }
        void EnterEvadeMode() { }
        void MoveInLineOfSight(Unit*) { }
        void JustDied(Unit*) { me->SetBotsPetDied(); master->SetNpcBotDied(me->GetGUID()); }
        void DoNonCombatActions()
        {}

        void StartAttack(Unit* u, bool force = false)
        {
            if (GetBotCommandState() == COMMAND_ATTACK && !force) return;
            Aggro(u);
            GetInPosition(force, true);
            SetBotCommandState(COMMAND_ATTACK);
            fear_cd = std::max<uint32>(fear_cd, 1000);
        }

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
            if (CCed(me)) return;

            //if pet is dead or unreachable
            //Creature* m_botsPet = me->GetBotsPet();
            //if (!m_botsPet || m_botsPet->GetMap() != master->GetMap() || (me->GetDistance2d(m_botsPet) > sWorld->GetMaxVisibleDistanceOnContinents() - 20.f))
            //    if (master->getLevel() >= 10 && !me->isInCombat() && !IsCasting() && !me->IsMounted())
            //        SummonBotsPet(PET_VOIDWALKER);

            //TODO: implement healthstone
            if (GetHealthPCT(me) < 50 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, HEALINGPOTION))
                    Potion_cd = POTION_CD;
                GC_Timer = temptimer;
            }
            if (GetManaPCT(me) < 50 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, MANAPOTION))
                    Potion_cd = POTION_CD;
                GC_Timer = temptimer;
            }
            if (!me->isInCombat())
                DoNonCombatActions();

            if (!CheckAttackTarget(CLASS_WARLOCK))
                return;

            DoNormalAttack(diff);
        }

        void DoNormalAttack(const uint32 diff)
        {
            opponent = me->getVictim();
            if (opponent)
            {
                if (!IsCasting())
                    StartAttack(opponent);
            }
            else
                return;

            //TODO: add more damage spells

            if (fear_cd <= diff && GC_Timer <= diff)
            { CheckFear(); fear_cd = 2000; }

            if (RAIN_OF_FIRE && Rand() < 25 && Rain_of_fire_cd <= diff && GC_Timer <= diff)
            {
                Unit* blizztarget = FindAOETarget(30, true);
                if (blizztarget && doCast(blizztarget, RAIN_OF_FIRE))
                {
                    Rain_of_fire_cd = 5000;
                    return;
                }
                Rain_of_fire_cd = 2000;//fail
            }

            if (Rand() < 25 && CURSE_OF_THE_ELEMENTS && GC_Timer <= diff && !HasAuraName(opponent, CURSE_OF_THE_ELEMENTS) && 
                doCast(opponent, CURSE_OF_THE_ELEMENTS))
            {
                GC_Timer = 800;
                return;
            }

            if (GC_Timer <= 0 && Rand() < 25 && !opponent->HasAura(CORRUPTION, me->GetGUID()) && 
                doCast(opponent, CORRUPTION))
                { return; }

            if (HAUNT && Rand() < 25 && Haunt_cd <= diff && GC_Timer <= diff && !opponent->HasAura(HAUNT, me->GetGUID()) && 
                doCast(opponent, HAUNT))
            {
                Haunt_cd = 8000;
                return;
            }

            if (GC_Timer <= diff && Rand() < 15 && !Afflicted(opponent))
            {
                if (conflagarate_cd <= 8000 && doCast(opponent, IMMOLATE))
                    return;
                else if (UNSTABLE_AFFLICTION && doCast(opponent, UNSTABLE_AFFLICTION))
                    return;
            }

            if (CONFLAGRATE && Rand() < 35 && conflagarate_cd <= diff && GC_Timer <= diff && 
                HasAuraName(opponent, IMMOLATE) && 
                doCast(opponent, CONFLAGRATE))
            {
                conflagarate_cd = 10000;
                return;
            }

            if (CHAOS_BOLT && Rand() < 50 && chaos_bolt_cd <= diff && GC_Timer <= diff && doCast(opponent, CHAOS_BOLT))
            {
                chaos_bolt_cd = 16000 - me->getLevel() * 100;
                return;
            }

            if (GC_Timer <= diff && doCast(opponent, SHADOW_BOLT))
                return;

        }

        uint8 Afflicted(Unit* target)
        {
            if (!target || target->isDead()) return 0;
            bool aff = HasAuraName(target, UNSTABLE_AFFLICTION, me->GetGUID());
            bool imm = HasAuraName(target, IMMOLATE, me->GetGUID());
            if (imm) return 1;
            if (aff) return 2;
            return 0;
        }

        void CheckFear()
        {
            if (Unit* u = FindAffectedTarget(FEAR, me->GetGUID()))
                if (Aura* aura = u->GetAura(FEAR, me->GetGUID()))
                    if (aura->GetDuration() > 3000)
                        return;
            Unit* feartarget = FindFearTarget();
            if (feartarget && doCast(feartarget, FEAR)) {}
        }

        void SummonedCreatureDies(Creature* summon, Unit* /*killer*/)
        {
            if (summon == me->GetBotsPet())
                me->SetBotsPetDied();
        }

        void SummonedCreatureDespawn(Creature* summon)
        {
            if (summon == me->GetBotsPet())
                me->SetBotsPet(NULL);
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
            Rain_of_fire_cd = 0;
            Haunt_cd = 0;
            conflagarate_cd = 0;
            chaos_bolt_cd = 0;
            fear_cd = 0;

            if (master)
            {
                setStats(CLASS_WARLOCK, me->getRace(), master->getLevel(), true);
                //TODO: passives
                ApplyClassPassives();
                ApplyPassives(CLASS_WARLOCK);
            }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (Rain_of_fire_cd > diff)     Rain_of_fire_cd -= diff;
            if (Haunt_cd > diff)            Haunt_cd -= diff;
            if (conflagarate_cd > diff)     conflagarate_cd -= diff;
            if (chaos_bolt_cd > diff)       chaos_bolt_cd -= diff;
            if (fear_cd > diff)             fear_cd -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            uint8 lvl = me->getLevel();
            CURSE_OF_THE_ELEMENTS                   = InitSpell(me, CURSE_OF_THE_ELEMENTS_1);
            SHADOW_BOLT                             = InitSpell(me, SHADOW_BOLT_1);
            IMMOLATE                                = InitSpell(me, IMMOLATE_1);
            CONFLAGRATE                 = lvl >= 40 ? CONFLAGRATE_1 : 0;
  /*Talent*/CHAOS_BOLT                  = lvl >= 60 ? InitSpell(me, CHAOS_BOLT_1) : 0;
            RAIN_OF_FIRE                            = InitSpell(me, RAIN_OF_FIRE_1);
  /*Talent*/HAUNT                       = lvl >= 60 ? InitSpell(me, HAUNT_1) : 0;
            CORRUPTION                              = InitSpell(me, CORRUPTION_1);
  /*Talent*/UNSTABLE_AFFLICTION         = lvl >= 50 ? InitSpell(me, UNSTABLE_AFFLICTION_1) : 0;
            FEAR                                    = InitSpell(me, FEAR_1);
        }

        void ApplyClassPassives() {}

    private:
        uint32 
  /*Curses*/CURSE_OF_THE_ELEMENTS, 
/*Destruct*/SHADOW_BOLT, IMMOLATE, CONFLAGRATE, CHAOS_BOLT, RAIN_OF_FIRE, 
 /*Afflict*/HAUNT, CORRUPTION, UNSTABLE_AFFLICTION, 
   /*Other*/FEAR;
        //Timers
        uint32 Rain_of_fire_cd, Haunt_cd, conflagarate_cd, chaos_bolt_cd, fear_cd;

        enum WarlockBaseSpells// all orignals
        {
            CURSE_OF_THE_ELEMENTS_1             = 1490,
            SHADOW_BOLT_1                       = 686,
            IMMOLATE_1                          = 348,
            CONFLAGRATE_1                       = 17962,
            CHAOS_BOLT_1                        = 50796,
            RAIN_OF_FIRE_1                      = 5740,
            HAUNT_1                             = 59164,
            CORRUPTION_1                        = 172,
            UNSTABLE_AFFLICTION_1               = 30404,
            FEAR_1                              = 6215,
        };
        enum WarlockPassives
        {
        };
    };
};

class voidwalker_bot : public CreatureScript
{
public:
    voidwalker_bot() : CreatureScript("voidwalker_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new voidwalker_botAI(creature);
    }

    struct voidwalker_botAI : public bot_pet_ai
    {
        voidwalker_botAI(Creature* creature) : bot_pet_ai(creature) { }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_NONE) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId, triggered);
        }

        void EnterCombat(Unit* u) { OnEnterCombat(u); }
        void Aggro(Unit*) { }
        void AttackStart(Unit*) { }
        void KilledUnit(Unit*) { }
        void EnterEvadeMode() { }
        void MoveInLineOfSight(Unit*) { }
        void JustDied(Unit*) { m_creatureOwner->SetBotsPetDied(); }

        void DoNonCombatActions()
        {}

        void StartAttack(Unit* u, bool force = false)
        {
            if (GetBotCommandState() == COMMAND_ATTACK && !force) return;
            Aggro(u);
            GetInPosition(force, false);
            SetBotCommandState(COMMAND_ATTACK);
        }

        void UpdateAI(const uint32 diff)
        {
            ReduceCD(diff);
            if (IAmDead()) return;
            if (me->getVictim())
                DoMeleeAttackIfReady();
            if (wait == 0)
                wait = GetWait();
            else
                return;
            CheckAuras();
            if (CCed(me)) return;

            //TODO: add checks to help owner

            if (!me->isInCombat())
                DoNonCombatActions();

            if (!CheckAttackTarget(PET_TYPE_VOIDWALKER))
                return;

            DoNormalAttack(diff);
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
            if (MoveBehind(*opponent))
                wait = 5;

            //TORMENT
            if (Torment_cd <= diff && me->GetDistance(opponent) <= 5 && (!tank || tank == me || opponent->getVictim() == m_creatureOwner))
            {
                temptimer = GC_Timer;
                if (doCast(opponent, TORMENT))
                    Torment_cd = 5000;
                GC_Timer = temptimer;
            }
        }

        void SpellHit(Unit* caster, SpellInfo const* spell)
        {
            OnSpellHit(caster, spell);
        }

        void DamageTaken(Unit* u, uint32& /*damage*/)
        {
            OnEnterCombat(u);
        }

        //debug
        //void ListSpells(ChatHandler* ch) const
        //{
        //    ch->PSendSysMessage("Spells list:");
        //    ch->PSendSysMessage("Torment: %u", TORMENT);
        //    ch->PSendSysMessage("End of spells list.");
        //}

        void Reset()
        {
            if (master)
            {
                //setStats(master->getLevel(), PET_TYPE_VOIDWALKER, 0, true);
                ApplyPassives(PET_TYPE_VOIDWALKER);
                ApplyClassPassives();
            }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (Torment_cd > diff)              Torment_cd -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            TORMENT                             = InitSpell(me, TORMENT_1);
        }

        void ApplyClassPassives() {}

    private:
        uint32 
            TORMENT;
        //Timers
        uint32 Torment_cd;

        enum VoidwalkerBaseSpells
        {
            TORMENT_1                           = 3716,
        };
        enum VoidwalkerPassives
        {
        };
    };
};

void AddSC_warlock_bot()
{
    new warlock_bot();
    new voidwalker_bot();
}
