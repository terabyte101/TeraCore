#include "bot_ai.h"
/*
Rogue NpcBot (reworked by Graff onlysuffering@gmail.com)
Complete - 25% maybe...
TODO:
*/
#define DMGMIN              1
#define DMGMAX              2
#define MAX_COMBO_POINTS    5
#define EVISCERATE_MAX_RANK 12
const uint32 EVSCRDamage[EVISCERATE_MAX_RANK+1][MAX_COMBO_POINTS+1][DMGMAX+1] = 
{
    { { 0,0,0 }, { 0,0,0 },     { 0,0,0 },      { 0,0,0 },       { 0,0,0 },       { 0,0,0 }       },
    { { 0,0,0 }, { 0,6,11 },    { 0,12,16 },    { 0,17,22 },     { 0,22,28 },     { 0,28,34 }     },
    { { 0,0,0 }, { 0,14,23 },   { 0,26,34 },    { 0,37,46 },     { 0,48,58 },     { 0,60,70 }     },
    { { 0,0,0 }, { 0,25,49 },   { 0,45,59 },    { 0,64,79 },     { 0,83,99 },     { 0,103,119 }   },
    { { 0,0,0 }, { 0,41,62 },   { 0,73,93 },    { 0,104,125 },   { 0,135,157 },   { 0,167,189 }   },
    { { 0,0,0 }, { 0,60,91 },   { 0,106,136 },  { 0,151,182 },   { 0,196,228 },   { 0,242,274 }   },
    { { 0,0,0 }, { 0,93,138 },  { 0,165,209 },  { 0,236,281 },   { 0,307,353 },   { 0,379,425 }   },
    { { 0,0,0 }, { 0,144,213 }, { 0,255,323 },  { 0,365,434 },   { 0,475,545 },   { 0,586,656 }   },
    { { 0,0,0 }, { 0,199,296 }, { 0,351,447 },  { 0,502,599 },   { 0,653,751 },   { 0,805,903 }   },
    { { 0,0,0 }, { 0,224,333 }, { 0,395,503 },  { 0,565,674 },   { 0,735,845 },   { 0,906,1016 }  },
    { { 0,0,0 }, { 0,245,366 }, { 0,431,551 },  { 0,616,737 },   { 0,801,923 },   { 0,987,1109 }  },
    { { 0,0,0 }, { 0,405,614 }, { 0,707,915 },  { 0,1008,1217 }, { 0,1309,1519 }, { 0,1611,1821 } },
    { { 0,0,0 }, { 0,497,752 }, { 0,868,1122 }, { 0,1238,1493 }, { 0,1608,1864 }, { 0,1979,2235 } }
};
#define RUPTURE_MAX_RANK    9
const uint32 RuptureDamage[RUPTURE_MAX_RANK+1][MAX_COMBO_POINTS+1] = 
{
    { 0, 0,   0,   0,    0,    0    },
    { 0, 41,  61,  86,   114,  147  },
    { 0, 61,  91,  128,  170,  219  },
    { 0, 89,  131, 182,  240,  307  },
    { 0, 129, 186, 254,  331,  419  },
    { 0, 177, 256, 350,  457,  579  },
    { 0, 273, 381, 506,  646,  803  },
    { 0, 325, 461, 620,  800,  1003 },
    { 0, 489, 686, 914,  1171, 1459 },
    { 0, 581, 816, 1088, 1395, 1739 }
};

class rogue_bot : public CreatureScript
{
public:
    rogue_bot() : CreatureScript("rogue_bot") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new rogue_botAI(creature);
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

    struct rogue_botAI : public bot_minion_ai
    {
        rogue_botAI(Creature* creature) : bot_minion_ai(creature)
        {
            Reset();
        }

        bool doCast(Unit* victim, uint32 spellId, bool triggered = false)
        {
            if (checkBotCast(victim, spellId, CLASS_ROGUE) != SPELL_CAST_OK)
                return false;
            return bot_ai::doCast(victim, spellId, triggered);
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
        void JustDied(Unit*) { comboPoints = 0; tempComboPoints = 0; master->SetNpcBotDied(me->GetGUID()); }
        void DoNonCombatActions(const uint32 /*diff*/)
        {}

        //This method should be used to emulate energy usage reduction
        void modenergy(int32 mod, bool set = false)
        {
            //can't set enery to -x (2 cases)
            if (set && mod < 0)
                return;
            if (mod < 0 && energy < uint32(abs(mod)))
                return;

            if (set)
                energy = mod;
            else
                energy += mod;

            energy = std::min<uint32>(energy, 100);
            me->SetPower(POWER_ENERGY, energy);
        }

        uint32 getenergy()
        {
            energy = me->GetPower(POWER_ENERGY);
            return energy;
        }

        void UpdateAI(const uint32 diff)
        {
            ReduceCD(diff);
            if (KidneyTarget)
            {
                //kidney shot is casted as rank 1 (1 or 2 secs) so add duration accordingly
                //i.e. kedney rank 1, points = 5, we have aura with duration 1 sec so add 4 more secs
                //tempComboPoints -= 1;
                if (tempComboPoints)
                {
                    if (Unit* u = sObjectAccessor->FindUnit(KidneyTarget))
                    {
                        if (Aura* kidney = u->GetAura(KIDNEY_SHOT, me->GetGUID()))
                        {
                            uint32 dur = kidney->GetDuration() + tempComboPoints*1000;
                            kidney->SetDuration(dur);
                            kidney->SetMaxDuration(dur);
                        }
                    }
                    else//spell is failed to hit: restore cp
                    {
                        if (comboPoints == 0)
                            comboPoints = tempComboPoints;
                    }
                    tempComboPoints = 0;
                }
                KidneyTarget = 0;
            }
            else if (RuptureTarget)
            {
                //tempComboPoints -= 1;
                if (tempComboPoints)
                {
                    if (Unit* u = sObjectAccessor->FindUnit(RuptureTarget))
                    {
                        if (Aura* rupture = u->GetAura(RUPTURE, me->GetGUID()))
                        {
                            uint32 dur = rupture->GetDuration() + tempComboPoints*2000;
                            rupture->SetDuration(dur);
                            rupture->SetMaxDuration(dur);
                        }
                    }
                    else//spell is failed to hit: restore cp
                    {
                        if (comboPoints == 0)
                            comboPoints = tempComboPoints;
                    }
                    tempComboPoints = 0;
                }
                RuptureTarget = 0;
            }
            else if (tempDICE)
            {
                //tempComboPoints -= 1;
                if (tempComboPoints)
                {
                    if (Aura* dice = me->GetAura(SLICE_DICE))
                    {
                        uint32 dur = (dice->GetDuration()*3)/2 + tempComboPoints*4500;//with Improved Slice and Dice
                        dice->SetDuration(dur);
                        dice->SetMaxDuration(dur);
                    }
                    tempComboPoints = 0;
                }
                tempDICE = false;
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
            if (checkAurasTimer == 0)
                CheckAuras();
            BreakCC(diff);
            if (CCed(me)) return;

            if (GetHealthPCT(me) < 33 && Potion_cd <= diff)
            {
                temptimer = GC_Timer;
                if (doCast(me, HEALINGPOTION))
                {
                    Potion_cd = POTION_CD;
                    GC_Timer = temptimer;
                }
            }

            if (!me->isInCombat())
                DoNonCombatActions(diff);

            if (!CheckAttackTarget(CLASS_ROGUE))
                return;

            Attack(diff);
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

            comboPoints = std::min<uint8>(comboPoints, 5);
            //AttackerSet m_attackers = master->getAttackers();
            AttackerSet b_attackers = me->getAttackers();
            float dist = me->GetExactDist(opponent);
            float meleedist = me->GetDistance(opponent);

            if (BLADE_FLURRY && Blade_Flurry_Timer <= diff && meleedist <= 5 && 
                Rand() < 30 && getenergy() >= 25 && FindSplashTarget(7, opponent))
            {
                temptimer = GC_Timer;
                if (doCast(me, BLADE_FLURRY))
                {
                    Blade_Flurry_Timer = 90000;
                    GC_Timer = temptimer;
                    return;//return here to allow cast only on next update
                }
            }

            if (MoveBehind(*opponent))
                wait = 5;

            //KICK
            if (KICK && Kick_Timer <= diff && meleedist <= 5 && Rand() < 80 && getenergy() >= 25 &&
                opponent->IsNonMeleeSpellCasted(false))
            {
                temptimer = GC_Timer;
                if (doCast(opponent, KICK))
                {
                    Kick_Timer = 8000;//improved
                    GC_Timer = temptimer;
                    //return;
                }
            }
            //SHADOWSTEP
            if (SHADOWSTEP && Shadowstep_Timer <= diff && dist < 25 &&
                (opponent->getVictim() != me || opponent->GetTypeId() == TYPEID_PLAYER) &&
                Rand() < 30 && getenergy() >= 10)
            {
                temptimer = GC_Timer;
                if (doCast(opponent, SHADOWSTEP))
                {
                    Shadowstep_Timer = 20000;
                    GC_Timer = temptimer;
                    //return;
                }
                //doCast(opponent, BACKSTAB, true);
            }
            //BACKSTAB
            if (BACKSTAB && GC_Timer <= diff && meleedist <= 5 && comboPoints < 5 &&
                /*Rand() < 90 && */getenergy() >= 60 && !opponent->HasInArc(M_PI, me))
            {
                if (doCast(opponent, BACKSTAB))
                    return;
            }
            //SINISTER STRIKE
            if (SINISTER_STRIKE && GC_Timer <= diff && meleedist <= 5 && comboPoints < 5 &&
                Rand() < 25 && getenergy() >= 45)
            {
                if (doCast(opponent, SINISTER_STRIKE))
                    return;
            }
            //SLICE AND DICE
            if (SLICE_DICE && Slice_Dice_Timer <= diff && GC_Timer <= diff && dist < 20 && comboPoints > 1 &&
                (b_attackers.size() <= 1 || Blade_Flurry_Timer > 80000) && Rand() < 30 && getenergy() >= 25)
            {
                if (doCast(opponent, SLICE_DICE))
                {
                    //DICE = true;
                    //tempDICE = true;
                    //tempComboPoints = comboPoints;
                    ////comboPoints = 0;
                    return;
                }
            }
            //KIDNEY SHOT
            if (KIDNEY_SHOT && GC_Timer <= diff && Kidney_Timer <= diff && meleedist <= 5 && comboPoints > 0 &&
                !CCed(opponent) && getenergy() >= 25 && ((Rand() < 15 + comboPoints*15 && opponent->getVictim() == me && comboPoints > 2) || opponent->IsNonMeleeSpellCasted(false)))
            {
                if (doCast(opponent, KIDNEY_SHOT))
                {
                    KidneyTarget = opponent->GetGUID();
                    tempComboPoints = comboPoints;
                    //comboPoints = 0;
                    Kidney_Timer = 20000;
                    return;
                }
            }
            //EVISCERATE
            if (EVISCERATE && GC_Timer <= diff && meleedist <= 5 && comboPoints > 2 &&
                getenergy() >= 35 && Rand() < comboPoints*15)
            {
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(EVISCERATE);
                uint8 rank = spellInfo->GetRank();
                float ap = me->GetTotalAttackPowerValue(BASE_ATTACK);
                float combo = float(comboPoints);
                int32 damage = int32(urand(EVSCRDamage[rank][comboPoints][DMGMIN], EVSCRDamage[rank][comboPoints][DMGMAX]));//base damage
                damage += irand(int32(ap*combo*0.03f), int32(ap*combo*0.07f));//ap bonus

                //PlaceHolder::damage bonuses
                // Eviscerate and Envenom Bonus Damage (Deathmantle item set effect)
                //if (me->HasAura(37169))
                //    damage += comboPoints*100;

                currentSpell = EVISCERATE;
                me->CastCustomSpell(opponent, EVISCERATE, &damage, NULL, NULL, false);
                return;
            }
            //MUTILATE
            //if (isTimerReady(Mutilate_Timer) && energy>60) 
            //{
            //    // TODO: calculate correct dmg for mutilate (dont forget poison bonus)
            //    // for now use same formula as evicerate
            //    uint32 base_attPower = me->GetUInt32Value(UNIT_FIELD_ATTACK_POWER);
            //    //float minDmg = me->GetFloatValue(UNIT_FIELD_MINDAMAGE);
            //    float minDmg = me->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
            //    int damage = irand(int32(base_attPower*7*0.03f),int32(base_attPower*7*0.08f))+minDmg+me->getLevel();

            //    // compensate for lack of attack power
            //    damage = damage*(rand()%4+1);

            //    me->CastCustomSpell(opponent, MUTILATE, &damage, NULL, NULL, false, NULL, NULL);

            //    //doCast (me, MUTILATE);
            //    Mutilate_Timer = 10;
            //    comboPoints+=3;
            //    energy -= 60;
            //}

            //RUPTURE
            if (RUPTURE && Rupture_Timer <= diff && GC_Timer <= diff && meleedist <= 5 && comboPoints > 2 &&
                opponent->GetHealth() > me->GetMaxHealth()/3 && getenergy() >= 25 && Rand() < 50 + 50*opponent->isMoving())
            {
                //no damage range for rupture
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(RUPTURE);
                uint8 rank = spellInfo->GetRank();
                float ap = me->GetTotalAttackPowerValue(BASE_ATTACK);
                float AP_per_combo[6] = {0.0f, 0.015f, 0.024f, 0.03f, 0.03428571f, 0.0375f};
                float divider[6] = {0.0f, 4.f, 5.f, 6.f, 7.f, 8.f};//duration/2 = number of ticks
                int32 damage = int32(RuptureDamage[rank][comboPoints]/divider[comboPoints]);//base damage
                damage += int32(ap*AP_per_combo[comboPoints]);//ap bonus is strict - applied to every tick

                currentSpell = RUPTURE;
                me->CastCustomSpell(opponent, RUPTURE, &damage, NULL, NULL, false);
                return;
                //if (doCast(opponent, RUPTURE))
                //{
                //    RuptureTarget = opponent->GetGUID();
                //    tempComboPoints = comboPoints;
                //    Rupture_Timer = 6000 + (comboPoints-1)*2000;
                //    //comboPoints = 0;
                //    return;
                //}
            }
            //DISMANTLE
            if (DISMANTLE && Dismantle_Timer <= diff && meleedist <= 5 &&
                opponent->GetTypeId() == TYPEID_PLAYER && 
                Rand() < 20 && getenergy() >= 25 && !CCed(opponent) &&
                !opponent->HasAuraType(SPELL_AURA_MOD_DISARM) && 
                opponent->ToPlayer()->GetWeaponForAttack(BASE_ATTACK))
            {
                temptimer = GC_Timer;
                if (doCast(opponent, DISMANTLE))
                {
                    Dismantle_Timer = 60000;
                    GC_Timer = temptimer;
                }
            }
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
                //Puncturing Wounds: 30% additional critical chance for Backstab
                if (lvl >= 15 && spellId == BACKSTAB)
                    aftercrit += 30.f;
                //Puncturing Wounds: 15% additional critical chance for Mutilate
                else if (spellId == MUTILATE)
                    aftercrit += 15.f;
                //Glyph of Eviscerate: 10% additional critical chance for Eviscerate
                else if (spellId == EVISCERATE)
                    aftercrit += 10.f;
                //Improved Ambush: 50% additional critical chance for Ambush
                //else if (spellId == AMBUSH)
                //    crit_chance += 50.f;
                if (lvl >= 50 && spellInfo->HasEffect(SPELL_EFFECT_ADD_COMBO_POINTS) && me->HasAura(TURN_THE_TABLES_EFFECT))
                    aftercrit += 6.f;

                //second roll (may be illogical)
                if (aftercrit > 0.f)
                    crit = roll_chance_f(aftercrit);
            }

            //2) apply bonus damage mods
            float pctbonus = 0.0f;
            if (crit)
            {
                //!!!Melee spell damage is not yet critical, all reduced by half
                //Lethality: 30% crit damage bonus for non-stealth combo-generating abilities (on 25 lvl)
                if (lvl >= 25 && !(spellInfo->Attributes & SPELL_ATTR0_ONLY_STEALTHED) &&
                    spellInfo->HasEffect(SPELL_EFFECT_ADD_COMBO_POINTS))
                    pctbonus += 0.15f;
            }
            //Shadowstep: 20% bonus damage to all abilities once
            //if (shadowstep == true)
            //{
            //    shadowstep = false;
            //    me->RemoveAurasDueToSpell(SHADOWSTEP_EFFECT_DAMAGE);
            //    pctbonus += 0.2f;
            //}
            //Find Weakness: 6% bonus damage to all abilities
            if (lvl >= 45)
                pctbonus += 0.06f;
            //DeathDealer set bonus: 15% damage bonus for Eviscerate
            if (lvl >= 60 && spellId == EVISCERATE)
                pctbonus += 0.15f;
            //Imoroved Eviscerate: 20% damage bonus for Eviscerate
            if (spellId == EVISCERATE)
                pctbonus += 0.2f;
            //Opportunity: 20% damage bonus for Backstab, Mutilate, Garrote and Ambush
            if (spellId == BACKSTAB || spellId == MUTILATE/* || 
                spellId == GARROTE || spellId == AMBUSH*/)
                pctbonus += 0.2f;
            //Aggression: 15% damage bonus for Sinister Strike, Backstab and Eviscerate
            if (lvl >= 30 && (spellId == SINISTER_STRIKE || spellId == BACKSTAB || spellId == EVISCERATE))
                pctbonus += 0.15f;
            //Blood Spatter: 30% bonus damage for Rupture and Garrote
            if (lvl >= 15 && (spellId == RUPTURE/* || spellId == GARROTE*/))
                pctbonus += 0.3f;
            //Serrated Blades: 30% bonus damage for Rupture
            if (lvl >= 20 && spellId == RUPTURE)
                pctbonus += 0.3f;
            //Surprise Attacks: 10% bonus damage for Sinister Strike, Backstab, Shiv, Hemmorhage and Gouge
            if (lvl >= 50 && (spellId == SINISTER_STRIKE || spellId == BACKSTAB/* ||
                spellId == SHIV || spellId == HEMMORHAGE || spellId == GOUGE*/))
                pctbonus += 0.1f;

            damage = int32(fdamage * (1.0f + pctbonus));
        }

        void DamageDealt(Unit* victim, uint32& /*damage*/, DamageEffectType damageType)
        {
            if (!WOUND_POISON && !MIND_NUMBING_POISON)
                return;

            if (damageType == DIRECT_DAMAGE/* || damageType == SPELL_DIRECT_DAMAGE*/)
            {
                if (victim && me->GetExactDist(victim) <= 30)
                {
                    switch (rand()%2)
                    {
                        case 0:
                            break;
                        case 1:
                        {
                            switch (rand()%2)
                            {
                                case 0:
                                    if (WOUND_POISON)
                                    {
                                        currentSpell = WOUND_POISON;
                                        DoCast(victim, WOUND_POISON, true);
                                    }
                                    break;
                                case 1:
                                    if (MIND_NUMBING_POISON)
                                    {
                                        currentSpell = MIND_NUMBING_POISON;
                                        DoCast(opponent, MIND_NUMBING_POISON, true);
                                    }
                                    break;
                            }
                        }
                    }
                }
            }
        }

        void SpellHit(Unit* caster, SpellInfo const* spell)
        {
            OnSpellHit(caster, spell);
        }

        void SpellHitTarget(Unit* target, SpellInfo const* spell)
        {
            uint32 spellId = spell->Id;
            if (currentSpell == 0)
                return;

            if (spellId == SLICE_DICE)
            {
                tempDICE = true;
                tempComboPoints = comboPoints;
                Slice_Dice_Timer = 15000 + (comboPoints-1)*4500;
            }
            else if (spellId == RUPTURE)
            {
                RuptureTarget = target->GetGUID();
                tempComboPoints = comboPoints;
                Rupture_Timer = 8000 + (comboPoints-1)*2000;
                GC_Timer = 800;
            }
            else if (spellId == EVISCERATE)
                GC_Timer = 800;

            //if (spellId == EVISCERATE || spellId == KIDNEY_SHOT || spellId == SLICE_DICE || spellId == RUPTURE/* || spellId == EXPOSE_ARMOR || spellId == ENVENOM*/)
            //Relentless Strikes
            if (spell->NeedsComboPoints())
            {
                //std::ostringstream msg;
                //msg << "casting ";
                //if (spellId == EVISCERATE)
                //    msg << "Eviscerate, ";
                //else if (spellId == RUPTURE)
                //    msg << "Rupture, ";
                //else if (spellId == SLICE_DICE)
                //    msg << "Slice and Dice, ";
                //else if (spellId == KIDNEY_SHOT)
                //    msg << "Kidney Shot, ";
                ////else if (spellId == EXPOSE_ARMOR)
                ////    msg << "Expose Armor, ";
                ////else if (spellId == ENVENOM)
                ////    msg << "Envenom, ";
                //msg << "combo points: " << uint32(std::min<uint32>(comboPoints,5));
                //me->MonsterWhisper(msg.str().c_str(), master->GetGUID());
                if (rand()%100 < 1 + 20*(comboPoints > 5 ? 5 : comboPoints))
                    DoCast(me, RELENTLESS_STRIKES_EFFECT, true);
                tempComboPoints = comboPoints;
                //CP adding effects are handled before actual finisher so use temp value
                //std::ostringstream msg2;
                //msg2 << "cp set to 0";
                if (tempAddCP)
                {
                    //msg2 << " + " << uint32(tempAddCP) << " (temp)";
                    comboPoints = tempAddCP;
                    tempAddCP = 0;
                }
                else
                    comboPoints = 0;
                //me->MonsterWhisper(msg2.str().c_str(), master->GetGUID());
            }
            else if (spellId == SINISTER_STRIKE ||
                spellId == BACKSTAB/* ||
                spellId == GOUGE ||
                spellId == HEMORRHAGE ||
                spellId == SHOSTLY_STRIKE*/)
            {
                ++comboPoints;
                //std::ostringstream msg;
                //msg << "1 cp generated ";
                //if (spellId == SINISTER_STRIKE)
                //    msg << "(Sinister Strike)";
                //else if (spellId == BACKSTAB)
                //    msg << "(Backstab)";
                //msg << " set to " << uint32(comboPoints);
                //if (tempAddCP)
                //    msg << " + " << uint32(tempAddCP) << " (triggered)";
                //me->MonsterWhisper(msg.str().c_str(), master->GetGUID());
                if (tempAddCP)
                {
                    comboPoints += tempAddCP;
                    tempAddCP = 0;
                }
            }
            else if (spellId == MUTILATE/* ||
                spellId == AMBUSH*/)
            {
                comboPoints += 2;
                //std::ostringstream msg;
                //msg << "2 cp generated (Mutilate), set to " << uint32(comboPoints);
                //if (tempAddCP)
                //    msg << " + " << uint32(tempAddCP) << " (triggered)";
                //me->MonsterWhisper(msg.str().c_str(), master->GetGUID());
                if (tempAddCP)
                {
                    comboPoints += tempAddCP;
                    tempAddCP = 0;
                }
            }
            else if (spellId == SEAL_FATE_EFFECT ||
                spellId == RUTHLESSNESS_EFFECT)
            {
                ++tempAddCP;
                //std::ostringstream msg;
                //msg << "1 temp cp generated ";
                //if (spellId == SEAL_FATE_EFFECT)
                //    msg << "(Seal Fate)";
                //else if (spellId == RUTHLESSNESS_EFFECT)
                //    msg << "(Ruthleness)";
                //me->MonsterWhisper(msg.str().c_str(), master->GetGUID());
            }

            if (spellId == SINISTER_STRIKE)
            {
                //Improved Sinister Strike
                //instead of restoring energy we should override current value
                if (me->getLevel() >= 10)
                    modenergy(-40, true);//45 - 5
            }
            //Glyph of Sinister Strike (50% to add cp on crit)
            //Seal Fate means crit so this glyph is enabled from lvl 35)
            if (spellId == SEAL_FATE_EFFECT && currentSpell == SINISTER_STRIKE && rand()%100 >= 50)
            {
                ++tempAddCP;
                //me->MonsterWhisper("1 temp cp generated (glyph of SS)", master->GetGUID());
            }
            //Slaughter from the Shadows energy restore
            //instead of restoring energy we should override current value
            if (me->getLevel() >= 55)
            {
                if (spellId == BACKSTAB/* || spellId == AMBUSH*/)
                    modenergy(-40, true);
                //else if (spellId == HEMORRHAGE)
                //    modenergy(-30, true);
            }

            //if (spellId == SHADOWSTEP)
            //{
            //    Shadowstep_eff_Timer = 10000;
            //    shadowstep = true;
            //}

            //move behind on Kidney Shot and Gouge (optionally)
            if (spellId == KIDNEY_SHOT/* || spellId == GOUGE*/)
                if (MoveBehind(*target))
                    wait = 3;

            if (spellId == currentSpell)
                currentSpell = 0;
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
            Mutilate_Timer = 0;
            Rupture_Timer = 0;
            Dismantle_Timer = 0;
            Kick_Timer = 0;
            Kidney_Timer = 0;
            Shadowstep_Timer = 0;
            Blade_Flurry_Timer = 0;
            Slice_Dice_Timer = 0;
            //Shadowstep_eff_Timer = 0;

            comboPoints = 0;
            tempComboPoints = 0;
            tempAddCP = 0;

            KidneyTarget = 0;
            RuptureTarget = 0;

            tempDICE = false;
            spellHitTarget = true;
            //shadowstep = false;

            me->setPowerType(POWER_ENERGY);
            //10 energy gained per stack
            RefreshAura(GLADIATOR_VIGOR, 10);

            if (master)
            {
                setStats(CLASS_ROGUE, me->getRace(), master->getLevel(), true);
                ApplyClassPassives();
                ApplyPassives(CLASS_ROGUE);
            }

            me->SetPower(POWER_ENERGY, me->GetMaxPower(POWER_ENERGY));
        }

        void ReduceCD(const uint32 diff)
        {
            CommonTimers(diff);
            if (Kick_Timer > diff)                  Kick_Timer -= diff;
            if (Rupture_Timer > diff)               Rupture_Timer -= diff;
            if (Shadowstep_Timer > diff)            Shadowstep_Timer -= diff;
            if (Mutilate_Timer > diff)              Mutilate_Timer -= diff;
            if (Kidney_Timer > diff)                Kidney_Timer -= diff;
            if (Dismantle_Timer > diff)             Dismantle_Timer -= diff;
            if (Blade_Flurry_Timer > diff)          Blade_Flurry_Timer -= diff;
            if (Slice_Dice_Timer > diff)            Slice_Dice_Timer -= diff;
            //if (Shadowstep_eff_Timer > diff)        Shadowstep_eff_Timer -= diff;
            //else if (shadowstep)                    shadowstep = false;
        }

        bool CanRespawn()
        {return false;}

        void InitSpells()
        {
            uint8 lvl = me->getLevel();
            BACKSTAB                                = InitSpell(me, BACKSTAB_1);
            SINISTER_STRIKE                         = InitSpell(me, SINISTER_STRIKE_1);
            SLICE_DICE                              = InitSpell(me, SLICE_DICE_1);
            EVISCERATE                              = InitSpell(me, EVISCERATE_1);
            KICK                                    = InitSpell(me, KICK_1);
            RUPTURE                                 = InitSpell(me, RUPTURE_1);
            KIDNEY_SHOT                             = InitSpell(me, KIDNEY_SHOT_1);
            MUTILATE                    = lvl >= 50 ? InitSpell(me, MUTILATE_1) : 0;
            SHADOWSTEP                  = lvl >= 50 ? SHADOWSTEP_1 : 0;
            DISMANTLE                               = InitSpell(me, DISMANTLE_1);
            BLADE_FLURRY                = lvl >= 30 ? BLADE_FLURRY_1 : 0;

            WOUND_POISON                            = InitSpell(me, WOUND_POISON_1);
            MIND_NUMBING_POISON                     = InitSpell(me, MIND_NUMBING_POISON_1);
        }

        void ApplyClassPassives()
        {
            uint8 level = master->getLevel();

            //if (level >= 78)
            //    RefreshAura(ROGUE_ARMOR_ENERGIZE,2);
            //else if (level >= 60)
            //    RefreshAura(ROGUE_ARMOR_ENERGIZE);
            if (level >= 70)
                RefreshAura(COMBAT_POTENCY5,2);
            else if (level >= 55)
                RefreshAura(COMBAT_POTENCY5);
            else if (level >= 52)
                RefreshAura(COMBAT_POTENCY4);
            else if (level >= 49)
                RefreshAura(COMBAT_POTENCY3);
            else if (level >= 47)
                RefreshAura(COMBAT_POTENCY2);
            else if (level >= 45)
                RefreshAura(COMBAT_POTENCY1);
            if (level >= 35)
                RefreshAura(SEAL_FATE5);
            else if (level >= 32)
                RefreshAura(SEAL_FATE4);
            else if (level >= 29)
                RefreshAura(SEAL_FATE3);
            else if (level >= 27)
                RefreshAura(SEAL_FATE2);
            else if (level >= 25)
                RefreshAura(SEAL_FATE1);
            if (level >= 78)
                RefreshAura(VITALITY,4);
            else if (level >= 70)
                RefreshAura(VITALITY,3);
            else if (level >= 55)
                RefreshAura(VITALITY,2);
            else if (level >= 40)
                RefreshAura(VITALITY);
            if (level >= 55)
                RefreshAura(TURN_THE_TABLES);
            if (level >= 40)
                RefreshAura(DEADLY_BREW);
            if (level >= 35)
                RefreshAura(BLADE_TWISTING1);//allow rank 1 only
            if (level >= 35)
                RefreshAura(QUICK_RECOVERY2);
            else if (level >= 30)
                RefreshAura(QUICK_RECOVERY1);
            if (level >= 30)
                RefreshAura(IMPROVED_KIDNEY_SHOT);
            if (level >= 10)
                RefreshAura(GLYPH_BACKSTAB);
            if (level >= 10)
                RefreshAura(SURPRISE_ATTACKS);

            //On 25 get Glyph of Vigor
            if (level >= 25)
                RefreshAura(ROGUE_VIGOR,2);
            else if (level >= 20)
                RefreshAura(ROGUE_VIGOR);
        }

    private:
        uint32
            BACKSTAB, SINISTER_STRIKE, SLICE_DICE, EVISCERATE, KICK, RUPTURE, KIDNEY_SHOT, MUTILATE,
            SHADOWSTEP, DISMANTLE, BLADE_FLURRY,
            WOUND_POISON, MIND_NUMBING_POISON;
        //Timers/other
        uint64 KidneyTarget, RuptureTarget;
        uint32 Rupture_Timer, Dismantle_Timer,
            Kick_Timer, Shadowstep_Timer, Mutilate_Timer, Kidney_Timer,
            Blade_Flurry_Timer, Slice_Dice_Timer/*, Shadowstep_eff_Timer*/;
        uint32 energy;
        uint8 comboPoints, tempComboPoints, tempAddCP;
        bool tempDICE, spellHitTarget/*, shadowstep*/;

        enum RogueBaseSpells
        {
            BACKSTAB_1                          = 53,
            SINISTER_STRIKE_1                   = 1757,
            SLICE_DICE_1                        = 5171,
            EVISCERATE_1                        = 2098,//11300
            KICK_1                              = 1766,
            RUPTURE_1                           = 1943,
            KIDNEY_SHOT_1                       = 408,//8643
  /*Talent*/MUTILATE_1                          = 1329,//48666
  /*Talent*/SHADOWSTEP_1                        = 36554,
            DISMANTLE_1                         = 51722,
            BLADE_FLURRY_1                      = 33735,

            WOUND_POISON_1                      = 13218,
            MIND_NUMBING_POISON_1               = 5760
        };

        enum RoguePassives
        {
            //Talents
            SEAL_FATE1                          = 14189,
            SEAL_FATE2                          = 14190,
            SEAL_FATE3                          = 14193,
            SEAL_FATE4                          = 14194,
            SEAL_FATE5                          = 14195,
            COMBAT_POTENCY1                     = 35541,
            COMBAT_POTENCY2                     = 35550,
            COMBAT_POTENCY3                     = 35551,
            COMBAT_POTENCY4                     = 35552,
            COMBAT_POTENCY5                     = 35553,
            QUICK_RECOVERY1                     = 31244,
            QUICK_RECOVERY2                     = 31245,
            BLADE_TWISTING1                     = 31124,
            //BLADE_TWISTING2                     = 31126,
            VITALITY                            = 61329,//rank 3
            DEADLY_BREW                         = 51626,//rank 2
            IMPROVED_KIDNEY_SHOT                = 14176,//rank 3
            TURN_THE_TABLES                     = 51629,//rank 3
            SURPRISE_ATTACKS                    = 32601,
            ROGUE_VIGOR                         = 14983,
            //Other
            //ROGUE_ARMOR_ENERGIZE/*Deathmantle*/ = 27787,
            GLADIATOR_VIGOR                     = 21975,
            GLYPH_BACKSTAB                      = 56800
        };

        enum RogueSpecial
        {
            //WOUND_POISON                        = 13218,
            //DEADLY_POISON                       = 2818,
            //MIND_NUMBING_POISON                 = 5760,
            RELENTLESS_STRIKES_EFFECT           = 14181,
            RUTHLESSNESS_EFFECT                 = 14157,
            SEAL_FATE_EFFECT                    = 14189,
            //SHADOWSTEP_EFFECT_DAMAGE            = 36563,
            TURN_THE_TABLES_EFFECT              = 52910
        };
    };
};

void AddSC_rogue_bot()
{
    new rogue_bot();
}
