#include "bot_ai.h"
/*
Shaman NpcBot (reworked by Graff onlysuffering@gmail.com)
Complete - 1%
TODO:
*/
class shaman_bot : public CreatureScript
{
public:
    shaman_bot() : CreatureScript("shaman_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new shaman_botAI(creature);
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

    struct shaman_botAI : public bot_minion_ai
    {
        shaman_botAI(Creature* creature) : bot_minion_ai(creature)
        {
            Reset();
        }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_SHAMAN) != SPELL_CAST_OK)
                return false;
            uint64 originalCaster = 0;
            if (spellId == STONESKIN_TOTEM ||
                spellId == SEARING_TOTEM ||
                spellId == WINDFURY_TOTEM ||
                spellId == HEALINGSTREAM_TOTEM ||
                spellId == MANASPRING_TOTEM)
                originalCaster = master->GetGUID();
            return bot_ai::doCast(victim, spellId, triggered, originalCaster);
        }

        void StartAttack(Unit* u, bool force = false)
        {
            if (GetBotCommandState() == COMMAND_ATTACK && !force) return;
            Aggro(u);
            GetInPosition(force, false/*STYLE == MELEE*//*SPEC == ENCHANCEMENT*/);
            SetBotCommandState(COMMAND_ATTACK);
        }

        void EnterCombat(Unit* u) { OnEnterCombat(u); }
        void Aggro(Unit*) { }
        void AttackStart(Unit*) { }
        void KilledUnit(Unit*) { }
        void EnterEvadeMode() { }
        void MoveInLineOfSight(Unit*) { }
        void JustDied(Unit*) { master->SetNpcBotDied(me->GetGUID()); }

        void CheckTotems(const uint32 diff)
        {
            if (GC_Timer > diff || Rand() > 50 || IsCasting() || me->GetDistance(master) > 20 || Feasting())
                return;
            if (me->isInCombat())
            {
                if (WINDFURY_TOTEM && !HasAuraName(master, WINDFURY_TOTEM))
                {
                    if (doCast(me, WINDFURY_TOTEM))
                        return;
                }
                else if (STONESKIN_TOTEM && !HasAuraName(master, STONESKIN_AURA))
                {
                    if (doCast(me, STONESKIN_TOTEM))
                        return;
                }

                if (Unit* u = me->getVictim())
                {
                    if (SEARING_TOTEM && Searing_Totem_Timer <= diff)
                    {
                        if (me->GetExactDist(u) < (u->isMoving() ? 10 : 25))
                        {
                            if (doCast(me, SEARING_TOTEM))
                            {
                                Searing_Totem_Timer = 30000;
                                return;
                            }
                        }
                    }
                }
            }
            else if (!me->isMoving() && !master->isMoving())
            {
                uint8 manapct = GetManaPCT(master);
                uint8 hppct = GetHealthPCT(master);
                if (HEALINGSTREAM_TOTEM &&
                    (master->getPowerType() != POWER_MANA || hppct < 25 || manapct > hppct) &&
                    hppct < 98 && !HasAuraName(master, HEALINGSTREAM_AURA))
                {
                    if (doCast(me, HEALINGSTREAM_TOTEM))
                        return;
                }
                else if (MANASPRING_TOTEM && manapct < 98 && !HasAuraName(master, MANASPRING_AURA))
                {
                    if (doCast(me, MANASPRING_TOTEM))
                        return;
                }
            }
        }

        void UpdateAI(const uint32 diff)
        {
            ReduceCD(diff);
            if (IAmDead()) return;
            if (!me->getVictim()) Evade();
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
            if (CCed(me)) return;

            if (GetManaPCT(me) < 30 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, MANAPOTION))
                    Potion_cd = POTION_CD;
                GC_Timer = temptimer;
            }

            BuffAndHealGroup(master, diff);
            CheckTotems(diff);

            if (!me->isInCombat())
                DoNonCombatActions(diff);
            //buff myself
            if (LIGHTNING_SHIELD && GC_Timer <= diff && !me->HasAura(LIGHTNING_SHIELD))
                if (doCast(me, LIGHTNING_SHIELD))
                    GC_Timer = 500;

            if (!CheckAttackTarget(CLASS_SHAMAN))
                return;

            //Counter(diff);
            DoNormalAttack(diff);
        }

        void Counter(const uint32 /*diff*/)
        {}

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

            //AttackerSet m_attackers = master->getAttackers();
            //AttackerSet b_attackers = me->getAttackers();
            float dist = me->GetExactDist(opponent);
            //float meleedist = me->GetDistance(opponent);

            if (MoveBehind(*opponent))
                wait = 5;

            if (Shock_Timer <= diff && GC_Timer <= diff && dist < 20)
            {
                if (!opponent->HasAura(FLAME_SHOCK, me->GetGUID()))
                {
                    if (doCast(opponent, FLAME_SHOCK))
                    {
                        Shock_Timer = 9000;
                        return;
                    }
                }
                else if (!opponent->HasAura(EARTH_SHOCK))
                {
                    if (doCast(opponent, EARTH_SHOCK))
                    {
                        Shock_Timer = 9000;
                        return;
                    }
                }
            }

            if (Lightning_Bolt_Timer <= diff && GC_Timer <= diff && dist < 25)
            {
                if (doCast(opponent, LIGHTNING_BOLT))
                {
                    Lightning_Bolt_Timer = uint32(float(sSpellMgr->GetSpellInfo(LIGHTNING_BOLT)->CalcCastTime()/100) * me->GetFloatValue(UNIT_MOD_CAST_SPEED) + 200);
                    return;
                }
            }
        }

        void DoNonCombatActions(const uint32 diff)
        {
            if (GC_Timer > diff || Rand() > 50 || me->IsMounted()) return;

            RezGroup(ANCESTRAL_SPIRIT, master);
            //BuffAndHealGroup(master, diff);
           // CureGroup(master, diff);
        }

        bool HealTarget(Unit* target, uint8 hp, const uint32 diff)
        {
            if (hp > 95) return false;
            if (!target || target->isDead() || me->GetExactDist(target) > 40)
                return false;
            if (Rand() > 50 + 20*target->isInCombat() + 50*master->GetMap()->IsRaid()) return false;

            //PLACEHOLDER: Instant spell req. interrupt current spell

            if (IsCasting()) return false;

            
            /*if (hp < 70 && Heal_Timer <= diff)
            {
                doCast(target, HEALING_WAVE);
            }
            else */if (hp < 90 && Heal_Timer <= diff)
            {
                doCast(target, CHAIN_HEAL);
            }
            else if (hp < 95)
            {
                doCast(target, LESSER_HEAL);
                return true;
            }
            return true;
        }

        //void ApplyBotDamageMultiplierMelee(int32& damage, SpellNonMeleeDamage& /*damageinfo*/, SpellInfo const* spellInfo, WeaponAttackType /*attackType*/, bool& /*crit*/) const
        //{
        //    uint32 spellId = spellInfo->Id;
        //    uint8 lvl = me->getLevel();
        //    float fdamage = float(damage);
        //    //1) apply additional crit chance. This new chance roll will replace original (balance unsafe)
        //    if (!crit)
        //    {
        //        float crit_chance = me->GetUnitCriticalChance(BASE_ATTACK, me);
        //        float aftercrit = crit_chance;

        //        //second roll (may be illogical)
        //        if (aftercrit > crit_chance)
        //            crit = roll_chance_f(aftercrit);
        //    }

        //    2) apply bonus damage mods
        //    float pctbonus = 0.0f;
        //    if (crit)
        //    {
        //    }

        //    fdamage *= (1.0f + pctbonus);
        //    damage = int32(fdamage);
        //    //last: overall multiplier
        //    bot_ai::ApplyBotDamageMultiplierMelee(damage, damageinfo, spellInfo, attackType, crit);
        //}

        void DamageDealt(Unit* /*victim*/, uint32& /*damage*/, DamageEffectType /*damageType*/)
        {
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
            Shock_Timer = 0;
            Lightning_Bolt_Timer = 0;
            Searing_Totem_Timer = 0;

            if (master)
            {
                setStats(CLASS_SHAMAN, me->getRace(), master->getLevel(), true);
                ApplyClassPassives();
                ApplyPassives(CLASS_SHAMAN);
            }
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (Heal_Timer > diff)              Heal_Timer -= diff;
            if (Shock_Timer > diff)             Shock_Timer -= diff;
            if (Lightning_Bolt_Timer > diff)    Lightning_Bolt_Timer -= diff;
            if (Searing_Totem_Timer > diff)     Searing_Totem_Timer -= diff;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            //uint8 lvl = me->getLevel();
            CHAIN_HEAL                              = InitSpell(me, CHAIN_HEAL_1);
            LESSER_HEAL                             = InitSpell(me, LESSER_HEAL_1);
            ANCESTRAL_SPIRIT                        = InitSpell(me, ANCESTRAL_SPIRIT_1);
            FLAME_SHOCK                             = InitSpell(me, FLAME_SHOCK_1);
            EARTH_SHOCK                             = InitSpell(me, EARTH_SHOCK_1);
            LIGHTNING_BOLT                          = InitSpell(me, LIGHTNING_BOLT_1);
            LIGHTNING_SHIELD                        = InitSpell(me, LIGHTNING_SHIELD_1);
            STONESKIN_TOTEM                         = InitSpell(me, STONESKIN_TOTEM_1);
            HEALINGSTREAM_TOTEM                     = InitSpell(me, HEALINGSTREAM_TOTEM_1);
            MANASPRING_TOTEM                        = InitSpell(me, MANASPRING_TOTEM_1);
            SEARING_TOTEM                           = InitSpell(me, SEARING_TOTEM_1);
            WINDFURY_TOTEM                          = InitSpell(me, WINDFURY_TOTEM_1);
        }

        void ApplyClassPassives()
        {
        }

    private:
        uint32
            CHAIN_HEAL, LESSER_HEAL, ANCESTRAL_SPIRIT,
            FLAME_SHOCK, EARTH_SHOCK, LIGHTNING_BOLT, LIGHTNING_SHIELD,
            STONESKIN_TOTEM, HEALINGSTREAM_TOTEM, MANASPRING_TOTEM, SEARING_TOTEM, WINDFURY_TOTEM;
        //Timers
        uint32 Heal_Timer, Shock_Timer, Lightning_Bolt_Timer, Searing_Totem_Timer;

        enum ShamanBaseSpells
        {
            CHAIN_HEAL_1                        = 1064,
            LESSER_HEAL_1                       = 8004,
            ANCESTRAL_SPIRIT_1                  = 2008,
            FLAME_SHOCK_1                       = 8050,
            EARTH_SHOCK_1                       = 8042,
            LIGHTNING_BOLT_1                    = 403,
            LIGHTNING_SHIELD_1                  = 324,
            STONESKIN_TOTEM_1                   = 8071,
            HEALINGSTREAM_TOTEM_1               = 5394,
            MANASPRING_TOTEM_1                  = 5675,
            SEARING_TOTEM_1                     = 3599,
            WINDFURY_TOTEM_1                    = 8512,
        };

        enum ShamanPassives
        {
        };

        enum ShamanSpecial
        {
            STONESKIN_AURA                      = 8072,
            HEALINGSTREAM_AURA                  = 5672,
            MANASPRING_AURA                     = 5677,
        };
    };
};


void AddSC_shaman_bot()
{
    new shaman_bot();
}
