#include "PlayerbotPriestAI.h"
#include "ObjectAccessor.h"
#include "SpellAuras.h"

PlayerbotPriestAI::PlayerbotPriestAI(Player* const master, Player* const bot, PlayerbotAI* const ai) : PlayerbotClassAI(master, bot, ai)
{
    RENEW                         = ai->initSpell(RENEW_1);
    LESSER_HEAL                   = ai->initSpell(LESSER_HEAL_1);
    FLASH_HEAL                    = ai->initSpell(FLASH_HEAL_1);
    (FLASH_HEAL > 0) ? FLASH_HEAL : FLASH_HEAL = LESSER_HEAL;
    HEAL                          = ai->initSpell(HEAL_1);
    (HEAL > 0) ? HEAL : HEAL = FLASH_HEAL;
    GREATER_HEAL                  = ai->initSpell(GREATER_HEAL_1);
    (GREATER_HEAL > 0) ? GREATER_HEAL : GREATER_HEAL = HEAL;
    RESURRECTION                  = ai->initSpell(RESURRECTION_1);
    SMITE                         = ai->initSpell(SMITE_1);
    MANA_BURN                     = ai->initSpell(MANA_BURN_1);
    HOLY_NOVA                     = ai->initSpell(HOLY_NOVA_1);
    HOLY_FIRE                     = ai->initSpell(HOLY_FIRE_1);
    DESPERATE_PRAYER              = ai->initSpell(DESPERATE_PRAYER_1);
    PRAYER_OF_HEALING             = ai->initSpell(PRAYER_OF_HEALING_1);
    CIRCLE_OF_HEALING             = ai->initSpell(CIRCLE_OF_HEALING_1);
    BINDING_HEAL                  = ai->initSpell(BINDING_HEAL_1);
    PRAYER_OF_MENDING             = ai->initSpell(PRAYER_OF_MENDING_1);
    CURE_DISEASE                  = ai->initSpell(CURE_DISEASE_1);

    // SHADOW
    FADE                          = ai->initSpell(FADE_1);
    SHADOW_WORD_PAIN              = ai->initSpell(SHADOW_WORD_PAIN_1);
    MIND_BLAST                    = ai->initSpell(MIND_BLAST_1);
    SCREAM                        = ai->initSpell(PSYCHIC_SCREAM_1);
    MIND_FLAY                     = ai->initSpell(MIND_FLAY_1);
    DEVOURING_PLAGUE              = ai->initSpell(DEVOURING_PLAGUE_1);
    SHADOW_PROTECTION             = ai->initSpell(SHADOW_PROTECTION_1);
    VAMPIRIC_TOUCH                = ai->initSpell(VAMPIRIC_TOUCH_1);
    PRAYER_OF_SHADOW_PROTECTION   = ai->initSpell(PRAYER_OF_SHADOW_PROTECTION_1);
    SHADOWFIEND                   = ai->initSpell(SHADOWFIEND_1);
    MIND_SEAR                     = ai->initSpell(MIND_SEAR_1);

    // DISCIPLINE
    PENANCE                       = ai->initSpell(PENANCE_1);
    INNER_FIRE                    = ai->initSpell(INNER_FIRE_1);
    POWER_WORD_SHIELD             = ai->initSpell(POWER_WORD_SHIELD_1);
    POWER_WORD_FORTITUDE          = ai->initSpell(POWER_WORD_FORTITUDE_1);
    PRAYER_OF_FORTITUDE           = ai->initSpell(PRAYER_OF_FORTITUDE_1);
    FEAR_WARD                     = ai->initSpell(FEAR_WARD_1);
    DIVINE_SPIRIT                 = ai->initSpell(DIVINE_SPIRIT_1);
    PRAYER_OF_SPIRIT              = ai->initSpell(PRAYER_OF_SPIRIT_1);
    MASS_DISPEL                   = ai->initSpell(MASS_DISPEL_1);
    POWER_INFUSION                = ai->initSpell(POWER_INFUSION_1);
    INNER_FOCUS                   = ai->initSpell(INNER_FOCUS_1);

    RECENTLY_BANDAGED  = 11196; // first aid check

    // racial
    ARCANE_TORRENT                = ai->initSpell(ARCANE_TORRENT_MANA_CLASSES);
    GIFT_OF_THE_NAARU             = ai->initSpell(GIFT_OF_THE_NAARU_PRIEST); // draenei
    STONEFORM                     = ai->initSpell(STONEFORM_ALL); // dwarf
    EVERY_MAN_FOR_HIMSELF         = ai->initSpell(EVERY_MAN_FOR_HIMSELF_ALL); // human
    SHADOWMELD                    = ai->initSpell(SHADOWMELD_ALL);
    BERSERKING                    = ai->initSpell(BERSERKING_ALL); // troll
    WILL_OF_THE_FORSAKEN          = ai->initSpell(WILL_OF_THE_FORSAKEN_ALL); // undead
}

PlayerbotPriestAI::~PlayerbotPriestAI() {}

bool PlayerbotPriestAI::DoFirstCombatManeuver(Unit* /*pTarget*/)
{
    return false;
}

bool PlayerbotPriestAI::HealTarget(Unit* target)
{
    PlayerbotAI* ai = GetAI();
    uint8 hp = target->GetHealth() * 100 / target->GetMaxHealth();
    uint8 hpSelf = GetAI()->GetHealthPercent();

    if (CURE_DISEASE > 0 && ai->GetCombatOrder() != PlayerbotAI::ORDERS_NODISPEL)
    {
        Unit::AuraMap const& auras = target->GetOwnedAuras();
        for (Unit::AuraMap::const_iterator itr = auras.begin(); itr != auras.end(); itr++)
        {
            Aura *aura = itr->second;
            if (aura->IsPassive()) continue;
            if (aura->GetSpellInfo()->AttributesEx & SPELL_ATTR0_HIDDEN_CLIENTSIDE) continue;
            if (aura->GetId() == 64844 || //Divine Hymn
                aura->GetId() == 6346) // Fear Ward
                continue;
            DispelType dispel = DispelType(aura->GetSpellInfo()->Dispel);
            if (dispel != DISPEL_DISEASE)
                continue;
            AuraApplication *aurApp = aura->GetApplicationOfTarget(target->GetGUID());
            if (!aurApp)
                continue;
            if (!aurApp->IsPositive() && ai->CastSpell(CURE_DISEASE, *target))
                return true;
        }
    }

    if (hp >= 80)
        return false;

    if (hp < 30 && FLASH_HEAL > 0 && ai->CastSpell(FLASH_HEAL, *target))
        return true;
    else if (hp < 40 && GREATER_HEAL > 0 && ai->CastSpell(GREATER_HEAL, *target))
        return true;
    // Heals target AND self for equal amount
    else if (hp < 60 && hpSelf < 80 && BINDING_HEAL > 0 && ai->CastSpell(BINDING_HEAL, *target))
        return true;
    else if (hp < 60 && HEAL > 0 && ai->CastSpell(HEAL, *target))
        return true;
    else if (hp < 80 && RENEW > 0 && !target->HasAura(RENEW) && ai->CastSpell(RENEW, *target))
        return true;
    else
        return false;
} // end HealTarget

void PlayerbotPriestAI::DoNextCombatManeuver(Unit *pTarget)
{
    Unit* pVictim = pTarget->getVictim();
    PlayerbotAI* ai = GetAI();
    if (!ai)
        return;

    switch (ai->GetScenarioType())
    {
        case PlayerbotAI::SCENARIO_DUEL:
            (ai->HasAura(SCREAM, *pTarget) && ai->GetHealthPercent() < 60 && ai->CastSpell(HEAL)) ||
            ai->CastSpell(SHADOW_WORD_PAIN) ||
            (ai->GetHealthPercent() < 80 && ai->CastSpell(RENEW)) ||
            (GetCombatDistance(pTarget) <= 5 && ai->CastSpell(SCREAM)) ||
            ai->CastSpell(MIND_BLAST) ||
            (ai->GetHealthPercent() < 20 && ai->CastSpell(GREATER_HEAL)) ||
            ai->CastSpell(SMITE);
            return;
    }

    // ------- Non Duel combat ----------

    Player *m_bot = GetPlayerBot();
    Group *m_group = m_bot->GetGroup();

    // Fade has nothing to do with health and everything to do with having aggro/threat
    Unit *newTarget = ai->FindAttacker((PlayerbotAI::ATTACKERINFOTYPE) (PlayerbotAI::AIT_VICTIMSELF | PlayerbotAI::AIT_HIGHESTTHREAT), m_bot);
    if (newTarget && FADE > 0 && !m_bot->HasAura(FADE, 0))
    {
        ai->TellMaster("I'm casting fade.");
        ai->CastSpell(FADE, *m_bot);
    }
    // Heal myself
    else if (ai->GetHealthPercent() < 25 && POWER_WORD_SHIELD > 0 && !m_bot->HasAura(POWER_WORD_SHIELD, 0))
    {
        ai->TellMaster("I'm casting PW:S on myself.");
        ai->CastSpell(POWER_WORD_SHIELD);
    }
    else if (ai->GetHealthPercent() < 35 && DESPERATE_PRAYER > 0)
    {
        ai->TellMaster("I'm casting desperate prayer.");
        ai->CastSpell(DESPERATE_PRAYER, *m_bot);
    }
    else if (ai->GetHealthPercent() < 60 || (BINDING_HEAL == 0 && ai->GetHealthPercent() < 80))
        HealTarget(m_bot);

    // Heal master
    uint32 masterHP = GetMaster()->GetHealth() * 100 / GetMaster()->GetMaxHealth();
    if (GetMaster()->isAlive())
    {
        if (masterHP < 25 && POWER_WORD_SHIELD > 0 && !GetMaster()->HasAura(POWER_WORD_SHIELD, 0))
            ai->CastSpell(POWER_WORD_SHIELD, *(GetMaster()));
        else if (masterHP < 25 || ((GetAI()->GetCombatOrder() & PlayerbotAI::ORDERS_HEAL) && masterHP < 80))
            HealTarget(GetMaster());
    }

    // TODO: Prioritize group healing in some way. If 3 members (including master/self) should be healed, pick one of these:
    // Group heal. Not really useful until a group check is available?
    //else if (hp < 40 && PRAYER_OF_HEALING > 0 && ai->CastSpell(PRAYER_OF_HEALING, *target))
    //    return true;
    // Group heal. Not really useful until a group check is available?
    //else if (hp < 50 && CIRCLE_OF_HEALING > 0 && ai->CastSpell(CIRCLE_OF_HEALING, *target))
    //    return true;

    // Heal group
    if (m_group)
    {
        Group::MemberSlotList const& groupSlot = m_group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player *m_groupMember = sObjectAccessor->FindPlayer(itr->guid);
            if (!m_groupMember || !m_groupMember->isAlive())
                continue;

            uint32 memberHP = m_groupMember->GetHealth() * 100 / m_groupMember->GetMaxHealth();
            if (memberHP < 25 && POWER_WORD_SHIELD > 0 && !m_groupMember->HasAura(POWER_WORD_SHIELD, 0))
                ai->CastSpell(POWER_WORD_SHIELD, *(GetMaster()));
            else if (memberHP < 25 || ((GetAI()->GetCombatOrder() & PlayerbotAI::ORDERS_HEAL) && memberHP < 80))
                HealTarget(m_groupMember);
        }
    }

    if (ai->GetCombatOrder() == PlayerbotAI::ORDERS_HEAL)
        SpellSequence = SPELL_HOLY;
    else if (ai->GetCombatOrder() == PlayerbotAI::ORDERS_ASSIST)
        SpellSequence = SPELL_SHADOWMAGIC;
    else
        SpellSequence = SPELL_HOLY;

    // Damage Spells
    float dist = GetCombatDistance(pTarget);

    switch (SpellSequence)
    {
        case SPELL_HOLY:
            if (SMITE > 0 && LastSpellHoly < 1 && !pTarget->HasAura(SMITE, 0) && ai->GetManaPercent() >= 17)
            {
                ai->CastSpell(SMITE, *pTarget);
                LastSpellHoly = LastSpellHoly + 1;
            }
            else if (MANA_BURN > 0 && LastSpellHoly < 2 && pTarget->GetPower(POWER_MANA) > 0 && ai->GetManaPercent() < 70 && ai->GetManaPercent() >= 14)
            {
                //ai->TellMaster("I'm casting mana burn.");
                ai->CastSpell(MANA_BURN, *pTarget);
                ai->SetIgnoreUpdateTime(3);
                LastSpellHoly = LastSpellHoly + 1;
            }
            else if (HOLY_NOVA > 0 && LastSpellHoly < 3 && dist <= ATTACK_DISTANCE && ai->GetManaPercent() >= 22)
            {
                //ai->TellMaster("I'm casting holy nova.");
                ai->CastSpell(HOLY_NOVA);
                LastSpellHoly = LastSpellHoly + 1;
            }
            else if (HOLY_FIRE > 0 && LastSpellHoly < 4 && !pTarget->HasAura(HOLY_FIRE, 0) && ai->GetManaPercent() >= 13)
            {
                //ai->TellMaster("I'm casting holy fire.");
                ai->CastSpell(HOLY_FIRE, *pTarget);
                LastSpellHoly = LastSpellHoly + 1;
            }
            else if (PRAYER_OF_MENDING > 0 && LastSpellHoly < 5 && pVictim == GetMaster() && GetMaster()->GetHealth() <= GetMaster()->GetMaxHealth() * 0.7 && !GetMaster()->HasAura(PRAYER_OF_MENDING, 0) && ai->GetManaPercent() >= 15)
            {
                //ai->TellMaster("I'm casting prayer of mending on master.");
                ai->CastSpell(PRAYER_OF_MENDING, *GetMaster());
                LastSpellHoly = LastSpellHoly + 1;
            }
            else
                LastSpellHoly = 0;

            if (ai->GetCombatOrder() == PlayerbotAI::ORDERS_NONE)
                SpellSequence = SPELL_SHADOWMAGIC;
            break;

        case SPELL_SHADOWMAGIC:
            if (SHADOW_WORD_PAIN > 0 && LastSpellShadowMagic < 1 && !pTarget->HasAura(SHADOW_WORD_PAIN, 0) && ai->GetManaPercent() >= 25)
            {
                //ai->TellMaster("I'm casting pain.");
                ai->CastSpell(SHADOW_WORD_PAIN, *pTarget);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (MIND_BLAST > 0 && LastSpellShadowMagic < 2 && ai->GetManaPercent() >= 19)
            {
                //ai->TellMaster("I'm casting mind blast.");
                ai->CastSpell(MIND_BLAST, *pTarget);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (SCREAM > 0 && LastSpellShadowMagic < 3 && ai->GetAttackerCount() >= 3 && ai->GetManaPercent() >= 15)
            {
                ai->TellMaster("I'm casting scream.");
                ai->CastSpell(SCREAM);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (MIND_FLAY > 0 && LastSpellShadowMagic < 4 && !pTarget->HasAura(MIND_FLAY, 0) && ai->GetManaPercent() >= 10)
            {
                //ai->TellMaster("I'm casting mind flay.");
                ai->CastSpell(MIND_FLAY, *pTarget);
                ai->SetIgnoreUpdateTime(3);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (DEVOURING_PLAGUE > 0 && LastSpellShadowMagic < 5 && !pTarget->HasAura(DEVOURING_PLAGUE, 0) && ai->GetManaPercent() >= 28)
            {
                ai->CastSpell(DEVOURING_PLAGUE, *pTarget);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (SHADOW_PROTECTION > 0 && LastSpellShadowMagic < 6 && ai->GetManaPercent() >= 60)
            {
                ai->CastSpell(SHADOW_PROTECTION, *pTarget);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (VAMPIRIC_TOUCH > 0 && LastSpellShadowMagic < 7 && !pTarget->HasAura(VAMPIRIC_TOUCH, 0) && ai->GetManaPercent() >= 18)
            {
                ai->CastSpell(VAMPIRIC_TOUCH, *pTarget);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (SHADOWFIEND > 0 && LastSpellShadowMagic < 8)
            {
                ai->CastSpell(SHADOWFIEND);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else if (MIND_SEAR > 0 && LastSpellShadowMagic < 9 && ai->GetAttackerCount() >= 3 && ai->GetManaPercent() >= 28)
            {
                ai->CastSpell(MIND_SEAR, *pTarget);
                ai->SetIgnoreUpdateTime(5);
                LastSpellShadowMagic = LastSpellShadowMagic + 1;
            }
            else
                LastSpellShadowMagic = 0;

            if (ai->GetCombatOrder() == PlayerbotAI::ORDERS_NONE || ai->GetCombatOrder() == PlayerbotAI::ORDERS_ASSIST)
                SpellSequence = SPELL_DISCIPLINE;
            break;

        case SPELL_DISCIPLINE:
            if (FEAR_WARD > 0 && LastSpellDiscipline < 1 && ai->GetManaPercent() >= 3)
            {
                //ai->TellMaster("I'm casting fear ward");
                ai->CastSpell(FEAR_WARD, *(GetMaster()));
                LastSpellDiscipline = LastSpellDiscipline + 1;
            }
            else if (POWER_INFUSION > 0 && LastSpellDiscipline < 2 && ai->GetManaPercent() >= 16)
            {
                //ai->TellMaster("I'm casting power infusion");
                ai->CastSpell(POWER_INFUSION, *(GetMaster()));
                LastSpellDiscipline = LastSpellDiscipline + 1;
            }
            else if (MASS_DISPEL > 0 && LastSpellDiscipline < 3 && ai->GetManaPercent() >= 33)
            {
                //ai->TellMaster("I'm casting mass dispel");
                ai->CastSpell(MASS_DISPEL);
                LastSpellDiscipline = LastSpellDiscipline + 1;
            }
            else if (INNER_FOCUS > 0 && !m_bot->HasAura(INNER_FOCUS, 0) && LastSpellDiscipline < 4)
            {
                //ai->TellMaster("I'm casting inner focus");
                ai->CastSpell(INNER_FOCUS, *m_bot);
                LastSpellDiscipline = LastSpellDiscipline + 1;
            }
            else if (PENANCE > 0 && LastSpellDiscipline < 5 && ai->GetManaPercent() >= 16)
            {
                //ai->TellMaster("I'm casting PENANCE");
                ai->CastSpell(PENANCE);
                LastSpellDiscipline = LastSpellDiscipline + 1;
            }
            else
                LastSpellDiscipline = 0;

            if (ai->GetCombatOrder() == PlayerbotAI::ORDERS_NONE)
                SpellSequence = SPELL_HOLY;
            else if (ai->GetCombatOrder() == PlayerbotAI::ORDERS_ASSIST)
                SpellSequence = SPELL_SHADOWMAGIC;
            break;
    }
} // end DoNextCombatManeuver

void PlayerbotPriestAI::DoNonCombatActions()
{
    PlayerbotAI* ai = GetAI();
    Player * m_bot = GetPlayerBot();
    Player * master = GetMaster();
    if (!m_bot || !master)
        return;

    SpellSequence = SPELL_HOLY;

    // selfbuff goes first
    if (ai->SelfBuff(INNER_FIRE))
        return;

    // mana check
    if (m_bot->getStandState() != UNIT_STAND_STATE_STAND)
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);

    Item* pItem = ai->FindDrink();
    Item* fItem = ai->FindBandage();

    if (pItem != NULL && ai->GetManaPercent() < 30)
    {
        ai->TellMaster("I could use a drink.");
        ai->UseItem(pItem);
        return;
    }

    // hp check
    if (m_bot->getStandState() != UNIT_STAND_STATE_STAND)
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);

    pItem = ai->FindFood();

    if (pItem != NULL && ai->GetHealthPercent() < 30)
    {
        ai->TellMaster("I could use some food.");
        ai->UseItem(pItem);
        return;
    }
    else if (pItem == NULL && fItem != NULL && !m_bot->HasAura(RECENTLY_BANDAGED, 0) && ai->GetHealthPercent() < 70)
    {
        ai->TellMaster("I could use first aid.");
        ai->UseItem(fItem);
        return;
    }

    // buff and heal master's group
    if (master->GetGroup())
    {
        // Buff master with group buffs
        if (!master->duel && master->isAlive())
        {
            if (PRAYER_OF_FORTITUDE && ai->HasSpellReagents(PRAYER_OF_FORTITUDE) && ai->Buff(PRAYER_OF_FORTITUDE, master))
                return;

            if (PRAYER_OF_SPIRIT && ai->HasSpellReagents(PRAYER_OF_SPIRIT) && ai->Buff(PRAYER_OF_SPIRIT, master))
                return;

            if (PRAYER_OF_SHADOW_PROTECTION && ai->HasSpellReagents(PRAYER_OF_SHADOW_PROTECTION) && ai->Buff(PRAYER_OF_SHADOW_PROTECTION, master))
                return;
        }

        Group::MemberSlotList const& groupSlot = GetMaster()->GetGroup()->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player *tPlayer = sObjectAccessor->FindPlayer(itr->guid);
            if (!tPlayer || !tPlayer->IsInWorld() || tPlayer->IsBeingTeleported() || 
                tPlayer->GetMap() != m_bot->GetMap() || tPlayer->GetDistance(m_bot) > 30)
                continue;

            if (tPlayer->duel && tPlayer->duel->opponent == GetMaster())
                continue;

            // first rezz em
            if (!tPlayer->isAlive())
            {
                if (ai->CastSpell(RESURRECTION, *tPlayer))
                {
                    std::string msg = "Resurrecting ";
                    msg += tPlayer->GetName();
                    m_bot->Say(msg, LANG_UNIVERSAL);
                    return;
                }
                else
                    continue;
            }
            else
            {
                // buff and heal
                if (BuffPlayer(tPlayer))
                    return;

                if (HealTarget(tPlayer))
                    return;
            }
        }
    }
    else
    {
        if (master->isAlive() && !master->duel)
        {
            if (BuffPlayer(master))
                return;
            if (HealTarget(master))
                return;
        }
        else if (ai->CastSpell(RESURRECTION, *master))
            ai->TellMaster("Resurrecting you, Master.");
    }

    BuffPlayer(m_bot);
} // end DoNonCombatActions

bool PlayerbotPriestAI::BuffPlayer(Player* target)
{
    PlayerbotAI * ai = GetAI();
    Pet * pet = target->GetPet();

    if ((pet && !pet->HasAuraType(SPELL_AURA_MOD_UNATTACKABLE)) && ai->Buff(POWER_WORD_FORTITUDE, pet))
        return true;

    if (ai->Buff(POWER_WORD_FORTITUDE, target))
        return true;

    if ((target->getClass() == CLASS_DRUID || target->getPowerType() == POWER_MANA) && ai->Buff(DIVINE_SPIRIT, target))
        return true;

    return false;
}
