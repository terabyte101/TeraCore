#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Group.h"
#include "Player.h"
#include "SpellAuras.h"
#include "ScriptMgr.h"

#ifndef _BOT_AI_H
#define _BOT_AI_H

enum CommonValues
{
//COMMON SPELLS
    MANAPOTION                          = 32453,//"???" forgot 0_o
    HEALINGPOTION                       = 15504,//"Drinks Holy Elixir to heal the caster"
    DRINK                               = 66041,//"Restores 4% mana per sec for 30 sec"
    EAT                                 = 66478,//"Restores Health"
    PVPTRINKET                          = 42292,//PvP Trinket no CD
//COMMON CDs
    POTION_CD                           = 60000,//default 60sec potion cd
    PVPTRINKET_CD                       = 120000,//default 2 min pvp trinket cd
//COMMON PASSIVES
  //1) "Increase(d) @whatever"
    //SPELL_BONUS_10                      = 33021,//10spp
    SPELL_BONUS_50                      = 45011,//50spp
    SPELL_BONUS_150                     = 28141,//150spp
    SPELL_BONUS_250                     = 69709,//250spp
    FIREDAM_86                          = 33816,//86 fire spp
    MANAREGEN45                         = 35867,//45 mp5
    MANAREGEN100                        = 45216,//100 mp5
  //2) Talents
    HASTE    /*Gift of the EarthMother*/= 51183,//rank 5 10% spell haste
    HASTE2   /*Blood Frenzy - warrior*/ = 29859,//rank2 10% haste, bonus for rend (warriors only)//13789//rank 3 10% haste 6% dodge
    CRITS    /*Thundering Strikes-sham*/= 16305,//rank 5 5% crit
    HOLYCRIT /*Holy Spec - priest*/     = 15011,//rank 5 5% holy crit
    DODGE    /*Anticipation - paladin*/ = 20100,//rank 5 5% dodge
    PARRY    /*Deflection - warrior*/   = 16466,//rank 5 5% parry
    PRECISION /*Precision - warrior*/   = 29592,//rank 3 3% melee hit
    PRECISION2/*Precision - mage*/      = 29440,//rank 3 3% spell hit
    DMG_TAKEN/*Deadened Nerves - rogue*/= 31383,//rank 3 (-6%)
  //3) Pet/Special
    THREAT   /*Tank Class Passive*/     = 57339,//+43% threat
    BOR      /*Blood of Rhino - pet*/   = 53482,//rank 2 +40% healing taken
    RCP      /*Rogue Class Passive*/    = 21184,//-27% threat caused
    DEFENSIVE_STANCE_PASSIVE            = 7376, //+threat/damage reduction
//COMMON GOSSIPS
    GOSSIP_SERVE_MASTER                 = 2279  //"I live only to serve the master."
};

//TODO: slow fall / water walking for master
//enum HoverSpells
//{
//    LEVITATE                            = 1706,
//    SLOW_FALL                           = 130,
//    //WATER_WALKING                       = 546,
//};

enum DruidStances//bot's temp set class
{
    BEAR        = 15,
    CAT         = 25,
    //TRAVEL      = 35,                   //NUY
    //FLY         = 45,                   //NUY
};

enum BotPetTypes
{
    PET_TYPE_NONE,
//Warlock
    PET_TYPE_IMP,
    PET_TYPE_VOIDWALKER,
    PET_TYPE_SUCCUBUS,
    PET_TYPE_FELHUNTER,
    PET_TYPE_FELGUARD,
//Mage
    PET_TYPE_WATER_ELEMENTAL,
//Shaman
    //PET_TYPE_GHOSTLY_WOLF,
    PET_TYPE_FIRE_ELEMENTAL,
    PET_TYPE_EARTH_ELEMENTAL,
//Hunter
    PET_TYPE_VULTURE,

    MAX_PET_TYPES
};

enum WarlockBotPets
{
    //PET_IMP                     = ,
    PET_VOIDWALKER              = 60237,
    //PET_SUCCUBUS                = 
};

enum HunterBotPets
{
    PET_VULTURE                 = 60238
};

enum BotPetsOriginalEntries
{
    ORIGINAL_ENTRY_VOIDWALKER   = 1860
};

class bot_ai : public ScriptedAI
{
    public:
        virtual ~bot_ai();
        bot_ai(Creature* creature);
        //Player* GetMaster() const { return master; }
        virtual bool IsMinionAI() const = 0;
        virtual bool IsPetAI() const = 0;
        virtual void SetBotCommandState(CommandStates /*st*/, bool /*force*/ = false, Position* /*newpos*/ = NULL) = 0;
        virtual const bot_minion_ai* GetMinionAI() const { return NULL; }
        virtual const bot_pet_ai* GetPetAI() const { return NULL; }
        bool IsInBotParty(Unit* unit) const;
        bool CanBotAttack(Unit* target, int8 byspell = 0) const;
        bool InDuel(Unit* target) const;
        void ApplyBotDamageMultiplierMelee(uint32& damage, CalcDamageInfo& damageinfo) const;
        void ApplyBotDamageMultiplierMelee(int32& damage, SpellNonMeleeDamage& damageinfo, SpellInfo const* spellInfo, WeaponAttackType attackType, bool& crit) const;
        void ApplyBotDamageMultiplierSpell(int32& damage, SpellNonMeleeDamage& damageinfo, SpellInfo const* spellInfo, WeaponAttackType attackType, bool& crit) const;
        inline void UpdateHealth() { doHealth = true; }
        inline void SetBotTank(Unit* newtank) { tank = newtank; m_TankGuid = newtank ? newtank->GetGUID() : 0; }
        static Unit* GetBotGroupMainTank(Group* group) { return _GetBotGroupMainTank(group); }
        inline float GetManaRegen() const { return regen_mp5; }
        inline float GetHitRating() const { return hit; }
        inline uint64 GetBotTankGuid() const { return m_TankGuid; }
        inline int32 GetSpellPower() const { return m_spellpower; }
        inline uint8 GetHaste() const { return haste; }

        void ReceiveEmote(Player* player, uint32 emote);
        void ApplyPassives(uint8 botOrPetType) const;

    protected:
        static inline bool CCed(Unit* target, bool root = false)
        {
            return target->HasUnitState(UNIT_STATE_CONFUSED | UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_DISTRACTED | UNIT_STATE_CONFUSED_MOVE | UNIT_STATE_FLEEING_MOVE) || (root && target->HasUnitState(UNIT_STATE_ROOT));
        }
        static uint32 InitSpell(Unit* caster, uint32 spell);

        bool HasAuraName(Unit* unit, const std::string spell, uint64 casterGuid = 0, bool exclude = false) const;
        bool HasAuraName(Unit* unit, uint32 spellId, uint64 casterGuid = 0, bool exclude = false) const;
        bool RefreshAura(uint32 spell, int8 count = 1, Unit* target = NULL) const;
        bool CheckAttackTarget(uint8 botOrPetType);
        bool MoveBehind(Unit &target) const;
        bool CheckImmunities(uint32 spell, Unit* target = NULL) const { return (spell && target && !target->ToCorpse() && target->IsHostileTo(me) ? !target->IsImmunedToDamage(sSpellMgr->GetSpellInfo(spell)) : true); }

        //everything cast-related
        bool doCast(Unit* victim, uint32 spellId, bool triggered = false, uint64 originalCaster = 0);
        SpellCastResult checkBotCast(Unit* victim, uint32 spellId, uint8 botclass) const;
        virtual void removeFeralForm(bool /*force*/ = false, bool /*init*/ = true, const uint32 /*diff*/ = 0) {}

        inline bool Feasting() const { return (me->HasAura(EAT) || me->HasAura(DRINK)); }
        inline bool isMeleeClass(uint8 m_class) const { return (m_class == CLASS_WARRIOR || m_class == CLASS_ROGUE || m_class == CLASS_PALADIN || m_class == CLASS_DEATH_KNIGHT || m_class == BEAR); }
        inline bool IsChanneling(Unit* u = NULL) const { if (!u) u = me; return u->GetCurrentSpell(CURRENT_CHANNELED_SPELL); }
        inline bool IsCasting(Unit* u = NULL) const { if (!u) u = me; return (u->HasUnitState(UNIT_STATE_CASTING) || IsChanneling(u) || u->IsNonMeleeSpellCasted(false)); }

        void GetInPosition(bool force = false, bool ranged = true, Unit* newtarget = NULL, Position* pos = NULL);
        void OnSpellHit(Unit* caster, SpellInfo const* spell);
        void FindTank();
        void listAuras(Player* player, Unit* unit) const;
        void CalculateAttackPos(Unit* target, Position &pos) const;

        virtual void ApplyClassDamageMultiplierMelee(uint32& /*damage*/, CalcDamageInfo& /*damageinfo*/) const {}
        virtual void ApplyClassDamageMultiplierMelee(int32& /*damage*/, SpellNonMeleeDamage& /*damageinfo*/, SpellInfo const* /*spellInfo*/, WeaponAttackType /*attackType*/, bool& /*crit*/) const {}
        virtual void ApplyClassDamageMultiplierSpell(int32& /*damage*/, SpellNonMeleeDamage& /*damageinfo*/, SpellInfo const* /*spellInfo*/, WeaponAttackType /*attackType*/, bool& /*crit*/) const {}
        virtual void CureGroup(Player* /*pTarget*/, uint32 /*cureSpell*/, const uint32 /*diff*/) {}
        virtual void CheckAuras(bool /*force*/ = false) {}
        virtual void BuffAndHealGroup(Player* /*gPlayer*/, const uint32 /*diff*/) {}
        virtual void RezGroup(uint32 /*REZZ*/, Player* /*gPlayer*/) {}
        //virtual void DoNonCombatActions(const uint32 /*diff*/) {}
        //virtual void StartAttack(Unit* /*u*/, bool /*force*/ = false) {}
        virtual void InitSpells() {}
        virtual void _OnHealthUpdate(uint8 /*myclass*/, uint8 /*mylevel*/) const = 0;
        virtual void _OnManaUpdate(uint8 /*myclass*/, uint8 /*mylevel*/) const = 0;
        //virtual void _OnMeleeDamageUpdate(uint8 /*myclass*/) const = 0;
        inline void OnEnterCombat(Unit* enemy) { if (enemy->IsPvP() && !InDuel(enemy)) master->SetInCombatState(true, enemy); }

        //virtual void ReceiveEmote(Player* /*player*/, uint32 /*emote*/) {}
        //virtual void CommonTimers(const uint32 diff) = 0;

        virtual bool HealTarget(Unit* /*target*/, uint8 /*pct*/, const uint32 /*diff*/) { return false; }
        virtual bool BuffTarget(Unit* /*target*/, const uint32 /*diff*/) { return false; }
        virtual bool CureTarget(Unit* /*target*/, uint32 /*cureSpell*/, const uint32 /*diff*/) { return false; }

        uint8 GetWait();
        inline float InitAttackRange(float origRange, bool ranged) const;
        inline uint16 Rand() const { return urand(0, 100 + (master->GetNpcBotsCount() - 1) * 10); }
        inline uint32 GetLostHP(Unit* unit) const { return unit->GetMaxHealth() - unit->GetHealth(); }
        inline uint8 GetHealthPCT(Unit* hTarget) const { if (!hTarget || hTarget->isDead()) return 100; return (hTarget->GetHealth()*100/hTarget->GetMaxHealth()); }
        inline uint8 GetManaPCT(Unit* hTarget) const { if (!hTarget || hTarget->isDead() || hTarget->getPowerType() != POWER_MANA) return 100; return (hTarget->GetPower(POWER_MANA)*100/hTarget->GetMaxPower(POWER_MANA)); }

        Unit* getTarget(bool byspell, bool ranged, bool &reset) const;

        CommandStates GetBotCommandState() const { return m_botCommandState; }

        typedef std::set<Unit*> AttackerSet;

        Player* master;
        Unit* opponent;
        Unit* tank;
        CommandStates m_botCommandState;
        SpellInfo const* info;
        Position pos, attackpos;
        float stat, atpower, maxdist, regen_mp5, hit,
            ap_mod, spp_mod, crit_mod;
        uint64 aftercastTargetGuid;
        int32 cost, value, sppower, m_spellpower;
        uint32 GC_Timer, temptimer, checkAurasTimer, wait, currentSpell;
        uint8 clear_cd, haste, healTargetIconFlags;
        bool doHealth, doMana;

    private:
        static Unit* _GetBotGroupMainTank(Group* group);
        inline float _getAttackDistance(float distance) const { return distance > 0.f ? distance*0.72 : 0.f; }
        Unit* extank;
        float dmgmult_melee, dmgmult_spell;
        float dmgmod_melee, dmgmod_spell;
        uint64 m_TankGuid;

//Unit checkers
    class NearestHostileUnitCheck
    {
        public:
            explicit NearestHostileUnitCheck(Unit const* unit, float dist, bool magic, bot_ai const* m_ai, bool targetCCed = false) : 
            me(unit), m_range(dist), byspell(magic), ai(m_ai), AttackCCed(targetCCed) { }
            bool operator()(Unit* u)
            {
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;
                if (!u->isInCombat())
                    return false;
                if (!ai->CanBotAttack(u, byspell))
                    return false;
                if (ai->InDuel(u))
                    return false;
                if (!AttackCCed && CCed(u))
                    return false;//do not allow CCed units if checked
                //if (u->HasUnitState(UNIT_STATE_CASTING) && (u->GetTypeId() == TYPEID_PLAYER || u->isPet()))
                //    for (uint8 i = 0; i != CURRENT_MAX_SPELL; ++i)
                //        if (Spell* spell = u->GetCurrentSpell(i))
                //            if (ai->IsInBotParty(spell->m_targets.GetUnitTarget()))
                //                return true;
                if (!(ai->IsInBotParty(u->getVictim()) || (u->getThreatManager().getThreat(const_cast<Unit*>(me)) > 0.f && u->HasUnitState(UNIT_STATE_FLEEING))))
                    return false;

                m_range = me->GetDistance(u);   // use found unit range as new range limit for next check
                return true;
            }
    private:
        Unit const* me;
        float m_range;
        bool byspell;
        bot_ai const* ai;
        bool AttackCCed;
        NearestHostileUnitCheck(NearestHostileUnitCheck const&);
    };

};

class bot_minion_ai : public bot_ai
{
    public:
        virtual ~bot_minion_ai();
        bot_minion_ai(Creature* creature);
        const bot_minion_ai* GetMinionAI() const { return this; }
        bool IsMinionAI() const { return true; }
        bool IsPetAI() const { return false; }
        void SummonBotsPet(uint32 entry);
        inline bool IAmDead() const { return (!master || me->isDead()); }
        void SetBotCommandState(CommandStates st, bool force = false, Position* newpos = NULL);
        //virtual bool HealTarget(Unit* /*target*/, uint8 /*pct*/, const uint32 /*diff*/) { return false; }
        //virtual bool BuffTarget(Unit* /*target*/, const uint32 /*diff*/) { return false; }
        //virtual bool doCast(Unit*  /*victim*/, uint32 /*spellId*/, bool /*triggered*/ = false) { return false; }
        void CureGroup(Player* pTarget, uint32 cureSpell, const uint32 diff);
        bool CureTarget(Unit* target, uint32 cureSpell, const uint32 diff);
        void CheckAuras(bool force = false);
        //virtual void DoNonCombatActions(const uint32 /*diff*/) {}
        //virtual void StartAttack(Unit* /*u*/, bool /*force*/ = false) {}
        void setStats(uint8 myclass, uint8 myrace, uint8 mylevel, bool force = false);

        static bool OnGossipHello(Player* player, Creature* creature);
        bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action);

        void InitSpells() {}
        void _OnHealthUpdate(uint8 myclass, uint8 mylevel) const;
        void _OnManaUpdate(uint8 myclass, uint8 mylevel) const;
        void _OnMeleeDamageUpdate(uint8 myclass) const;

        static inline uint8 GetBotClassForCreature(Creature* bot);

    protected:
        void BuffAndHealGroup(Player* gPlayer, const uint32 diff);
        void RezGroup(uint32 REZZ, Player* gPlayer);

        void Follow(bool force = false, Position* newpos = NULL)
        {
            if (force || 
                (me->isAlive() && (!me->isInCombat() || !opponent) && m_botCommandState != COMMAND_STAY))
                SetBotCommandState(COMMAND_FOLLOW, force, newpos);
        }

        void OnOwnerDamagedBy(Unit* attacker);

        inline void Evade() { _OnEvade(); }

        virtual void BreakCC(const uint32 diff);

        void CommonTimers(const uint32 diff)
        {
            if (pvpTrinket_cd > diff)       pvpTrinket_cd -= diff;
            if (Potion_cd > diff)           Potion_cd -= diff;
            if (GC_Timer > diff)            GC_Timer -= diff;
            if (temptimer > diff)           temptimer -= diff;
            if (checkAurasTimer != 0)       --checkAurasTimer;
            if (wait != 0)                  --wait;
        }

        Unit* FindHostileDispelTarget(float dist = 30, bool stealable = false) const;
        Unit* FindAffectedTarget(uint32 spellId, uint64 caster = 0, float dist = DEFAULT_VISIBILITY_DISTANCE, uint8 hostile = 0) const;
        Unit* FindPolyTarget(float dist = 30, Unit* currTarget = NULL) const;
        Unit* FindFearTarget(float dist = 30) const;
        Unit* FindRepentanceTarget(float dist = 20) const;
        Unit* FindUndeadCCTarget(float dist = 30, uint32 spellId = 0) const;
        Unit* FindRootTarget(float dist = 30, uint32 spellId = 0) const;
        Unit* FindCastingTarget(float dist = 10, bool isFriend = false, uint32 spellId = 0) const;
        Unit* FindAOETarget(float dist = 30, bool checkbots = false, bool targetfriend = true) const;
        Unit* FindSplashTarget(float dist = 5, Unit* To = NULL) const;

        uint32 Potion_cd, pvpTrinket_cd;

    private:
        bool CanCureTarget(Unit* target, uint32 cureSpell, const uint32 diff) const;
        void CalculatePos(Position& pos);
        void UpdateMountedState();
        void UpdateStandState() const;
        void UpdateRations() const;
        void _OnEvade();
        PlayerClassLevelInfo classinfo;
        float myangle, armor_mod, haste_mod, dodge_mod, parry_mod;
        uint8 rezz_cd;

//Unit Checkers
    class HostileDispelTargetCheck
    {
        public:
            explicit HostileDispelTargetCheck(Unit const* unit, float dist = 30, bool stealable = false, bot_ai const* m_ai = NULL) : 
            me(unit), m_range(dist), checksteal(stealable), ai(m_ai) { if (!ai) return; }
            bool operator()(Unit* u)
            {
                if (u->IsWithinDistInMap(me, m_range) && 
                    u->isAlive() && 
                    u->InSamePhase(me) && 
                    u->isInCombat() && 
                    u->isTargetableForAttack() && 
                    u->IsVisible() && 
                    u->GetReactionTo(me) < REP_NEUTRAL && 
                    (ai->IsInBotParty(u->getVictim()) || u->getThreatManager().getThreat(const_cast<Unit*>(me)) > 0.f))
                {
                    if (checksteal && u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(30449))) return false;//immune to steal
                    if (!checksteal)
                    {
                        if (me->getLevel() >= 70 && u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(32375))) return false;//immune to mass dispel
                        if (me->getLevel() < 70 && u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(527))) return false;//immune to direct dispel
                    }
                    Unit::AuraMap const &Auras = u->GetOwnedAuras();
                    for (Unit::AuraMap::const_iterator itr = Auras.begin(); itr != Auras.end(); ++itr)
                    {
                        Aura* aura = itr->second;
                        SpellInfo const* Info = aura->GetSpellInfo();
                        if (Info->Dispel != DISPEL_MAGIC) continue;
                        if (Info->Attributes & (SPELL_ATTR0_PASSIVE | SPELL_ATTR0_HIDDEN_CLIENTSIDE)) continue;
                        if (checksteal && (Info->AttributesEx4 & SPELL_ATTR4_NOT_STEALABLE)) continue;
                        AuraApplication* aurApp = aura->GetApplicationOfTarget(u->GetGUID());
                        if (aurApp && aurApp->IsPositive())
                        {
                            const std::string name = Info->SpellName[0];
                            if (name == "Vengeance" || name == "Bloody Vengeance")
                                continue;
                            return true;
                        }
                    }
                }
                return false;
            }
        private:
            Unit const* me;
            float m_range;
            bool checksteal;
            bot_ai const* ai;
            HostileDispelTargetCheck(HostileDispelTargetCheck const&);
    };

    class AffectedTargetCheck
    {
        public:
            explicit AffectedTargetCheck(uint64 casterguid, float dist, uint32 spellId, Player const* groupCheck = 0, uint8 hostileCheckType = 0) : 
            caster(casterguid), m_range(dist), spell(spellId), checker(groupCheck), needhostile(hostileCheckType)
            { if (checker->GetTypeId() != TYPEID_PLAYER) return; gr = checker->GetGroup(); }
            bool operator()(Unit* u)
            {
                if (caster && u->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))
                    return false;
                if (needhostile == 0 && !u->IsHostileTo(checker)) return false;
                if (needhostile == 1 && !(gr && gr->IsMember(u->GetGUID()) && u->GetTypeId() == TYPEID_PLAYER)) return false;
                if (needhostile == 2 && !(gr && gr->IsMember(u->GetGUID()))) return false;
                if (needhostile == 3 && !u->IsFriendlyTo(checker)) return false;

                if (u->isAlive() && checker->IsWithinDistInMap(u, m_range))
                {
                    Unit::AuraMap const &Auras = u->GetOwnedAuras();
                    for (Unit::AuraMap::const_iterator itr = Auras.begin(); itr != Auras.end(); ++itr)
                    {
                        Aura* aura = itr->second;
                        if (aura->GetId() == spell)
                            if (caster == 0 || aura->GetCasterGUID() == caster)
                                return true;
                    }
                }
                return false;
            }
        private:
            uint64 const caster;
            float m_range;
            uint32 const spell;
            Player const* checker;
            uint8 needhostile;
            Group const* gr;
            AffectedTargetCheck(AffectedTargetCheck const&);
    };

    class PolyUnitCheck
    {
        public:
            explicit PolyUnitCheck(Unit const* unit, float dist, Unit const* currTarget) : me(unit), m_range(dist), mytar(currTarget) {}
            bool operator()(Unit* u)
            {
                if (u == mytar)
                    return false;
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;
                if (!u->isInCombat() || !u->isAlive() || !u->getVictim())
                    return false;
                if (u->GetCreatureType() != CREATURE_TYPE_HUMANOID && 
                    u->GetCreatureType() != CREATURE_TYPE_BEAST)
                    return false;
                if (me->GetDistance(u) < 6 || mytar->GetDistance(u) < 5 || u->GetHealthPct() < 70)
                    return false;
                if (!u->InSamePhase(me))
                    return false;
                if (!u->isTargetableForAttack())
                    return false;
                if (!u->IsVisible())
                    return false;
                if (!u->getAttackers().empty())
                    return false;
                if (!u->IsHostileTo(me))
                    return false;
                if (u->IsPolymorphed() || 
                    u->isFrozen() || 
                    u->isInRoots() || 
                    u->HasAura(51514)/*hex*/ || 
                    u->HasAura(20066)/*repentance*/ || 
                    //u->HasAuraTypeWithAffectMask(SPELL_AURA_PERIODIC_DAMAGE, sSpellMgr->GetSpellInfo(339)) || //entangling roots
                    //u->HasAuraTypeWithAffectMask(SPELL_AURA_PERIODIC_DAMAGE, sSpellMgr->GetSpellInfo(16914)) || //hurricane
                    //u->HasAuraTypeWithAffectMask(SPELL_AURA_PERIODIC_DAMAGE, sSpellMgr->GetSpellInfo(10)) || //blizzard
                    //u->HasAuraTypeWithAffectMask(SPELL_AURA_PERIODIC_DAMAGE, sSpellMgr->GetSpellInfo(2121)) || //flamestrike
                    //u->HasAuraTypeWithAffectMask(SPELL_AURA_PERIODIC_DAMAGE, sSpellMgr->GetSpellInfo(20116)) || //consecration
                    u->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE))
                    return false;
                if (!u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(12826)))//Polymorph rank 4
                    return true;

                return false;
            }
        private:
            Unit const* me;
            float m_range;
            Unit const* mytar;
            PolyUnitCheck(PolyUnitCheck const&);
    };

    class FearUnitCheck
    {
        public:
            explicit FearUnitCheck(Unit const* unit, float dist = 30) : me(unit), m_range(dist) {}
            bool operator()(Unit* u)
            {
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;
                if (!u->InSamePhase(me))
                    return false;
                if (!u->isInCombat())
                    return false;
                if (u->GetCreatureType() == CREATURE_TYPE_UNDEAD)
                    return false;
                if (!u->isAlive())
                    return false;
                if (!u->isTargetableForAttack())
                    return false;
                if (!u->IsVisible())
                    return false;
                if (u->getAttackers().size() > 1 && u->getVictim() != me)
                    return false;
                if (CCed(u))
                    return false;
                if (u->isFeared())
                    return false;
                if (u->GetReactionTo(me) > REP_NEUTRAL)
                    return false;
                
                if (!u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(5782)))//fear rank1
                    return true;

                return false;
            }
        private:
            Unit const* me;
            float m_range;
            FearUnitCheck(FearUnitCheck const&);
    };

    class StunUnitCheck
    {
        public:
            explicit StunUnitCheck(Unit const* unit, float dist = 20) : me(unit), m_range(dist) {}
            bool operator()(Unit* u)
            {
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;
                if (!u->isInCombat())
                    return false;
                if (me->getVictim() == u)
                    return false;
                if (me->GetTypeId() == TYPEID_UNIT)
                    if (Player* mymaster = me->ToCreature()->GetBotOwner())
                        if (mymaster->getVictim() == u)
                            return false;
                if (!u->InSamePhase(me))
                    return false;
                if (u->GetReactionTo(me) > REP_NEUTRAL)
                    return false;
                if (!u->isAlive())
                    return false;
                if (!u->IsVisible())
                    return false;
                if (!u->isTargetableForAttack())
                    return false;
                if (!u->getAttackers().empty())
                    return false;
                if (u->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE))
                    return false;
                if (!(u->GetCreatureType() == CREATURE_TYPE_HUMANOID || 
                    u->GetCreatureType() == CREATURE_TYPE_DEMON || 
                    u->GetCreatureType() == CREATURE_TYPE_DRAGONKIN || 
                    u->GetCreatureType() == CREATURE_TYPE_GIANT || 
                    u->GetCreatureType() == CREATURE_TYPE_UNDEAD))
                    return false;
                if (me->GetDistance(u) < 10)//do not allow close cast to prevent break due to consecration
                    return false;
                if (u->IsPolymorphed() || 
                    u->HasAura(51514)/*hex*/ || 
                    u->HasAura(20066)/*repentance*/ || 
                    u->HasAuraWithMechanic(1<<MECHANIC_SHACKLE)/*shackleundead*/)
                    return false;
                if (!u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(20066)))//repentance
                    return true;

                return false;
            }
        private:
            Unit const* me;
            float m_range;
            StunUnitCheck(StunUnitCheck const&);
    };

    class UndeadCCUnitCheck
    {
        public:
            explicit UndeadCCUnitCheck(Unit const* unit, float dist = 30, uint32 spell = 0) : me(unit), m_range(dist), m_spellId(spell) { if (!spell) return; }
            bool operator()(Unit* u)
            {
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;
                if (!u->InSamePhase(me))
                    return false;
                if (!u->isInCombat())
                    return false;
                if (u->GetReactionTo(me) > REP_NEUTRAL)
                    return false;
                if (!u->isAlive())
                    return false;
                if (!u->isTargetableForAttack())
                    return false;
                if (!u->IsVisible())
                    return false;
                if (me->getVictim() == u && u->getVictim() == me)
                    return false;
                if (!u->getAttackers().empty())
                    return false;
                if (u->GetCreatureType() != CREATURE_TYPE_UNDEAD && 
                    (m_spellId == 9484 || m_spellId == 9485 || m_spellId == 10955))//shackle undead
                    return false;
                //most horrible hacks
                if (u->GetCreatureType() != CREATURE_TYPE_UNDEAD && 
                    u->GetCreatureType() != CREATURE_TYPE_DEMON && 
                    (m_spellId == 2812 || m_spellId == 10318 || //holy
                    m_spellId == 27139 || m_spellId == 48816 || //wra
                    m_spellId == 48817 ||                       //th or
                    m_spellId == 10326))                        //turn evil
                    return false;
                if (CCed(u))
                    return false;
                if (u->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) && 
                    (m_spellId == 9484 || m_spellId == 9485 || m_spellId == 10955))//shackle undead
                    return false;
                if (!u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(m_spellId)))
                    return true;

                return false;
            }
        private:
            Unit const* me;
            float m_range;
            uint32 m_spellId;
            UndeadCCUnitCheck(UndeadCCUnitCheck const&);
    };

    class RootUnitCheck
    {
        public:
            explicit RootUnitCheck(Unit const* unit, Unit const* mytarget, float dist = 30, uint32 spell = 0) : me(unit), curtar(mytarget), m_range(dist), m_spellId(spell)
            { if (!spell) return; }
            bool operator()(Unit* u)
            {
                if (u == curtar)
                    return false;
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;
                if (!u->isAlive())
                    return false;
                if (!u->isInCombat())
                    return false;
                if (me->GetDistance(u) < 8)
                    return false;
                if (!u->InSamePhase(me))
                    return false;
                if (!u->IsVisible())
                    return false;
                if (!u->isTargetableForAttack())
                    return false;
                if (u->GetReactionTo(me) > REP_NEUTRAL)
                    return false;
                if (u->isFrozen() || u->isInRoots())
                    return false;
                if (!u->getAttackers().empty())
                    return false;
                if (u->IsPolymorphed() || 
                    u->HasAura(51514)/*hex*/ || 
                    u->HasAura(20066)/*repentance*/ || 
                    u->HasAuraWithMechanic(1<<MECHANIC_SHACKLE)/*shackleundead*/)
                    return false;
                if (!u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(m_spellId)))
                    return true;

                return false;
            }
        private:
            Unit const* me;
            Unit const* curtar;
            float m_range;
            uint32 m_spellId;
            RootUnitCheck(RootUnitCheck const&);
    };

    class CastingUnitCheck
    {
        public:
            explicit CastingUnitCheck(Unit const* unit, float dist = 30, bool friendly = false, uint32 spell = 0) : me(unit), m_range(dist), m_friend(friendly), m_spell(spell) { if (!m_spell) return; }
            bool operator()(Unit* u)
            {
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;
                if (!u->isAlive())
                    return false;
                if (!u->InSamePhase(me))
                    return false;
                if (!u->IsVisible())
                    return false;
                if (!m_friend && !u->isTargetableForAttack())
                    return false;
                //if (!m_friend && u->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))//prevent double silence
                //    return false;
                if (!u->IsNonMeleeSpellCasted(false))
                    return false;
                if (m_friend == (u->GetReactionTo(me) < REP_FRIENDLY))
                    return false;
                if (m_spell == 10326 && //turn evil
                    u->GetCreatureType() != CREATURE_TYPE_UNDEAD && 
                    u->GetCreatureType() != CREATURE_TYPE_DEMON)
                    return false;
                if (m_spell == 20066 && //repentance
                    !(u->GetCreatureType() == CREATURE_TYPE_HUMANOID || 
                    u->GetCreatureType() == CREATURE_TYPE_DEMON || 
                    u->GetCreatureType() == CREATURE_TYPE_DRAGONKIN || 
                    u->GetCreatureType() == CREATURE_TYPE_GIANT || 
                    u->GetCreatureType() == CREATURE_TYPE_UNDEAD))
                    return false;
                if (!m_spell || !u->IsImmunedToSpell(sSpellMgr->GetSpellInfo(m_spell)))
                    return true;

                return false;
            }
        private:
            Unit const* me;
            float m_range;
            bool m_friend;
            uint32 m_spell;
            CastingUnitCheck(CastingUnitCheck const&);
    };

    class SecondEnemyCheck
    {
        public:
            explicit SecondEnemyCheck(Unit const* unit, float dist, Unit const* currtarget, bot_ai const* m_ai) : me(unit), m_range(dist), mytar(currtarget), ai(m_ai) {}
            bool operator()(Unit* u)
            {
                if (u == mytar)
                    return false;//We need to find SECONDARY target
                if (!u->isInCombat())
                    return false;
                if (u->isMoving() != mytar->isMoving())//only when both targets idle or both moving
                    return false;
                if (!me->IsWithinDistInMap(u, m_range + 1.f))//distance check
                    return false;
                if (mytar->GetDistance(u) > 4)//not close enough to each other
                    return false;

                if (ai->CanBotAttack(u))
                    return true;

                return false;
            }
        private:
            Unit const* me;
            float m_range;
            Unit const* mytar;
            bot_ai const* ai;
            SecondEnemyCheck(SecondEnemyCheck const&);
    };
};

class bot_pet_ai : public bot_ai
{
    public:
        virtual ~bot_pet_ai();
        bot_pet_ai(Creature* creature);
        const bot_pet_ai* GetPetAI() const { return this; }
        Creature* GetCreatureOwner() const { return m_creatureOwner; }
        bool IsMinionAI() const { return false; }
        bool IsPetAI() const { return true; }
        inline bool IAmDead() const { return (!master || !m_creatureOwner || me->isDead()); }
        void SetCreatureOwner(Creature* newowner) { m_creatureOwner = newowner; }
        void SetBotCommandState(CommandStates st, bool force = false, Position* newpos = NULL);
        //virtual bool HealTarget(Unit* /*target*/, uint8 /*pct*/, const uint32 /*diff*/) { return false; }
        //virtual bool BuffTarget(Unit* /*target*/, const uint32 /*diff*/) { return false; }
        //void BuffAndHealGroup(Player* /*gPlayer*/, const uint32 /*diff*/) {}
        //void RezGroup(uint32 /*REZZ*/, Player* /*gPlayer*/) {}
        //virtual bool doCast(Unit*  /*victim*/, uint32 /*spellId*/, bool /*triggered*/ = false) { return false; }
        //void CureGroup(Player* /*pTarget*/, uint32 /*cureSpell*/, const uint32 /*diff*/) {}
        //bool CureTarget(Unit* /*target*/, uint32 /*cureSpell*/, const uint32 /*diff*/) { return false; }
        void CheckAuras(bool force = false);
        //virtual void DoNonCombatActions(const uint32 /*diff*/) {}
        //virtual void StartAttack(Unit* /*u*/, bool /*force*/ = false) {}
        void setStats(uint8 mylevel, uint8 petType, bool force = false);

        static uint8 GetPetType(Creature* pet);
        static uint8 GetPetClass(Creature* pet);
        static uint32 GetPetOriginalEntry(uint32 entry);

        //debug
        //virtual void ListSpells(ChatHandler* /*handler*/) const {}

        void InitSpells() {}
        void _OnHealthUpdate(uint8 petType, uint8 mylevel) const;
        void _OnManaUpdate(uint8 petType, uint8 mylevel) const;
        //void _OnMeleeDamageUpdate(uint8 /*myclass*/) const {}
        void SetBaseArmor(uint32 armor) { basearmor = armor; }

    protected:
        void CommonTimers(const uint32 diff)
        {
            if (GC_Timer > diff)            GC_Timer -= diff;
            if (temptimer > diff)           temptimer -= diff;
            if (checkAurasTimer != 0)       --checkAurasTimer;
            if (wait != 0)                  --wait;
        }

        Creature* m_creatureOwner;
    private:
        uint32 basearmor;
};

#endif