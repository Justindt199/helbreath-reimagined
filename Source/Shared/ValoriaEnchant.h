// ValoriaEnchant.h: VALORIA's enchanting, disenchanting and empowering rules, shared by the game
// server (HG Server\Enchanting.cpp) and the client (Client3\UI\EnchantPanel.cpp) so both always agree.
//
// Ported from VALORIA's game_state.gd. What changes for Helbreath:
//   * an item's tier (VALORIA's 1-10) is worked out from its required level: (level + 13) / 14, so
//     tier 10 is level 127 and up. Helbreath items without a level count as tier 1;
//   * gold costs are VALORIA's / 4, the same rule as item prices and recipe fees;
//   * Helbreath items have no rarity, runes or upgrade "plus", so those parts of VALORIA's
//     formulas count as 0;
//   * the modifiers are the ones Helbreath Reimagined applies: STR, DEX, VIT, INT, MAG, max HP,
//     max MP, defense and crit, plus weapon damage for empowering. VALORIA's resistances,
//     accuracy, knockback, cooldown, move speed and the unique-item special effects come later.
//
// Wire formats (little endian, the same on every machine that runs the game):
//   item data, 48 bytes:      6 x enchant key (uint8, Mod + 1, 0 = empty), 6 x enchant value (int16),
//                             10 x empower level (uint8, one per Mod), 2 x rune (uint8, Mod + 1),
//                             2 spare, rarity (uint8, 0 Common .. 4 Legendary), 4 x bonus key
//                             (uint8, Bonus + 1, 0 = empty), 4 x bonus value (int16), 3 spare
//   character data, 80 bytes: Enchanting XP (uint32), 10 x known tier (uint8, 0 = not learned),
//                             10 x modifier XP (uint32), 10 spare, 4 x life skill XP (uint32, Life)
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <cstring>

namespace Valoria {

enum Mod { MOD_STR, MOD_DEX, MOD_VIT, MOD_INT, MOD_MAG, MOD_HP, MOD_MP, MOD_DEF, MOD_CRIT, MOD_DMG, MOD_COUNT };
const unsigned char MOD_NONE = 255;

const int ENCHANTABLE   = MOD_CRIT + 1;	// weapon damage can be empowered, not enchanted
const int MAX_ENCHANTS  = 6;
const int MAX_RUNES     = 2;	// VALORIA: runes an item can hold (bytes 28-29 of ItemData)
const int EMPOWER_MAX   = 5;
const int ENCHANT_MAX   = 50;			// Enchanting level
const int MOD_MAX       = 50;			// each modifier's own enchanting level
const int CORE_MOD_MAX  = 5;
const int ITEM_BYTES    = 48;		// was 32 before rarity: older saves read with the rest as zero (Common)
const int CHAR_BYTES    = 80;		// was 64 before the life skills: older saves read them as 0 XP
const int GOLD_DIVISOR  = 4;
const int MAX_TIER      = 10;
const double SKILL_XP_MULT = 0.5;		// VALORIA's skill XP multiplier

inline bool IsCore(int k) { return k >= MOD_STR && k <= MOD_MAG; }

inline const char * ModName(int k)
{
	static const char * n[MOD_COUNT] = { "STR", "DEX", "VIT", "INT", "MAG", "Max HP", "Max MP", "Defense", "Crit recharge %", "Damage %" };
	return (k >= 0 && k < MOD_COUNT) ? n[k] : "?";
}

// ---- rarity (VALORIA's game_state.gd: roll_rarity, roll_bonus, bonus_pool_for) -----------------------
// Dropped gear rolls a rarity; each step above Common adds one random bonus stat. The Reforging Orb
// rerolls the bonuses, the Ascension Orb raises Common..Rare one step (adding a bonus), the Radiant Orb
// Epic to Legendary. Bonuses: the Mod stats STR..crit, and on worn armour physical and magic
// resistance (% of a hit taken off, up to RES_CAP). VALORIA's accuracy, knockback and spell cooldown
// have no Helbreath counterpart and are left out.

enum Rarity { RARITY_COMMON, RARITY_UNCOMMON, RARITY_RARE, RARITY_EPIC, RARITY_LEGENDARY, RARITY_COUNT };
enum Bonus { BONUS_PHYS_RES = MOD_DMG, BONUS_MAG_RES, BONUS_COUNT };	// 0..MOD_CRIT are the Mod stats
const int MAX_BONUSES = 4;		// one per step above Common
const int RES_CAP     = 50;		// highest physical / magic resistance % gear can give

inline const char * RarityName(int r)
{
	static const char * n[RARITY_COUNT] = { "Common", "Uncommon", "Rare", "Epic", "Legendary" };
	return (r >= 0 && r < RARITY_COUNT) ? n[r] : "Common";
}
// VALORIA's rarity colours (RARITY_COLORS), as 0xRRGGBB
inline unsigned int RarityRGB(int r)
{
	static const unsigned int c[RARITY_COUNT] = { 0xE0D9C7, 0x73E673, 0x66A6FF, 0xBF73FF, 0xFF9926 };
	return (r >= 0 && r < RARITY_COUNT) ? c[r] : c[0];
}
inline const char * BonusName(int k)
{
	if (k == BONUS_PHYS_RES) return "Physical resist %";
	if (k == BONUS_MAG_RES) return "Magic resist %";
	return ModName(k);
}

// A rarity from u in [0, 1): VALORIA's weights, a boss's better ones, item find raising the steps above Common.
inline int RollRarity(double u, bool bBoss, int iItemFindPct)
{
	double w[RARITY_COUNT] = { 74.0, 20.0, 5.4, 0.55, 0.05 };
	if (bBoss) { w[0] = 32.0; w[1] = 38.0; w[2] = 24.0; w[3] = 5.0; w[4] = 1.0; }
	const double f = 1.0 + (iItemFindPct > 0 ? iItemFindPct : 0) / 100.0;
	double total = 0;
	for (int i = 0; i < RARITY_COUNT; i++) { if (i > 0) w[i] *= f; total += w[i]; }
	double r = u * total;
	for (int i = 0; i < RARITY_COUNT; i++) { r -= w[i]; if (r < 0) return i; }
	return RARITY_COMMON;
}

// The bonus stats gear can roll (resistances only on worn armour); returns how many went into pool.
inline int BonusPool(bool bArmour, int pool[BONUS_COUNT])
{
	int n = 0;
	for (int k = MOD_STR; k <= MOD_CRIT; k++) pool[n++] = k;
	if (bArmour) { pool[n++] = BONUS_PHYS_RES; pool[n++] = BONUS_MAG_RES; }
	return n;
}

// One bonus stat's value at an item tier: VALORIA's roll_bonus_one, from a dice function d(lo, hi).
template <class Dice> inline int RollBonusValue(int k, int iTier, Dice d)
{
	if (k == MOD_HP) return d(5, 15) * iTier;
	if (k == MOD_MP) return d(4, 10) * iTier;
	if (k == MOD_CRIT || k == BONUS_PHYS_RES || k == BONUS_MAG_RES) return d(1, iTier + 1);
	if (k >= MOD_STR && k <= MOD_MAG) return d(1, (2 * iTier < CORE_MOD_MAX) ? 2 * iTier : CORE_MOD_MAX);
	return d(1, 2 * iTier);		// defense
}

// ---- items ---------------------------------------------------------------------------------------

struct ItemData {
	unsigned char enKey[MAX_ENCHANTS];
	short         enVal[MAX_ENCHANTS];
	unsigned char em[MOD_COUNT];
	unsigned char rune[MAX_RUNES];	// VALORIA runes (Rune of Might ...): modifier, MOD_NONE = empty slot
	unsigned char rarity;				// Rarity
	unsigned char bKey[MAX_BONUSES];	// rarity bonus stats: Bonus, MOD_NONE = empty
	short         bVal[MAX_BONUSES];

	ItemData() { Clear(); }
	void Clear() { memset(enKey, MOD_NONE, sizeof(enKey)); memset(enVal, 0, sizeof(enVal)); memset(em, 0, sizeof(em)); memset(rune, MOD_NONE, sizeof(rune));
		rarity = RARITY_COMMON; memset(bKey, MOD_NONE, sizeof(bKey)); memset(bVal, 0, sizeof(bVal)); }
	bool Empty() const { return Enchants() == 0 && EmpowerTotal() == 0 && Runes() == 0 && rarity == RARITY_COMMON && Bonuses() == 0; }
	int  Bonuses() const { int n = 0; for (int i = 0; i < MAX_BONUSES; i++) if (bKey[i] < BONUS_COUNT) n++; return n; }
	int  BonusValue(int k) const { int v = 0; for (int i = 0; i < MAX_BONUSES; i++) if (bKey[i] == k) v += bVal[i]; return v; }
	// rolls n bonus stats, all different (VALORIA's roll_bonus); keeps any already there when bKeep
	template <class Dice> void RollBonuses(int n, bool bArmour, int iTier, Dice d, bool bKeep = false) {
		if (!bKeep) { memset(bKey, MOD_NONE, sizeof(bKey)); memset(bVal, 0, sizeof(bVal)); }
		int pool[BONUS_COUNT];
		int np = BonusPool(bArmour, pool);
		for (int i = 0; i < np; i++) for (int j = 0; j < MAX_BONUSES; j++) if (bKey[j] == pool[i]) { pool[i--] = pool[--np]; break; }
		for (int s = 0; s < MAX_BONUSES && n > 0 && np > 0; s++) {
			if (bKey[s] < BONUS_COUNT) continue;
			const int pick = d(0, np - 1);
			bKey[s] = (unsigned char)pool[pick];
			bVal[s] = (short)RollBonusValue(pool[pick], iTier, d);
			pool[pick] = pool[--np];
			n--;
		}
	}
	int  Runes() const { int n = 0; for (int i = 0; i < MAX_RUNES; i++) if (rune[i] < MOD_COUNT) n++; return n; }
	int  Enchants() const { int n = 0; for (int i = 0; i < MAX_ENCHANTS; i++) if (enKey[i] < MOD_COUNT) n++; return n; }
	int  EnchantSlotOf(int k) const { for (int i = 0; i < MAX_ENCHANTS; i++) if (enKey[i] == k) return i; return -1; }
	int  EnchantValue(int k) const { const int s = EnchantSlotOf(k); return s < 0 ? 0 : enVal[s]; }
	int  EmpowerTotal() const { int n = 0; for (int k = 0; k < MOD_COUNT; k++) n += em[k]; return n; }

	void Write(unsigned char * p) const {
		memset(p, 0, ITEM_BYTES);
		for (int i = 0; i < MAX_ENCHANTS; i++) p[i] = (enKey[i] < MOD_COUNT) ? (unsigned char)(enKey[i] + 1) : 0;	// 0 = empty, so all-zero bytes are an item without enchantments
		for (int i = 0; i < MAX_ENCHANTS; i++) { p[6 + i*2] = (unsigned char)(enVal[i] & 0xFF); p[7 + i*2] = (unsigned char)((enVal[i] >> 8) & 0xFF); }
		for (int k = 0; k < MOD_COUNT; k++) p[18 + k] = em[k];
		for (int i = 0; i < MAX_RUNES; i++) p[28 + i] = (rune[i] < MOD_COUNT) ? (unsigned char)(rune[i] + 1) : 0;
		p[32] = rarity;
		for (int i = 0; i < MAX_BONUSES; i++) p[33 + i] = (bKey[i] < BONUS_COUNT) ? (unsigned char)(bKey[i] + 1) : 0;
		for (int i = 0; i < MAX_BONUSES; i++) { p[37 + i*2] = (unsigned char)(bVal[i] & 0xFF); p[38 + i*2] = (unsigned char)((bVal[i] >> 8) & 0xFF); }
	}
	void Read(const unsigned char * p) {
		for (int i = 0; i < MAX_ENCHANTS; i++) enKey[i] = (p[i] >= 1 && p[i] <= MOD_COUNT) ? (unsigned char)(p[i] - 1) : MOD_NONE;
		for (int i = 0; i < MAX_ENCHANTS; i++) enVal[i] = (short)(p[6 + i*2] | (p[7 + i*2] << 8));
		for (int k = 0; k < MOD_COUNT; k++) em[k] = (p[18 + k] <= EMPOWER_MAX) ? p[18 + k] : EMPOWER_MAX;
		for (int i = 0; i < MAX_RUNES; i++) rune[i] = (p[28 + i] >= 1 && p[28 + i] <= MOD_COUNT) ? (unsigned char)(p[28 + i] - 1) : MOD_NONE;
		for (int i = 0; i < MAX_ENCHANTS; i++) if (enKey[i] == MOD_NONE) enVal[i] = 0;
		rarity = (p[32] < RARITY_COUNT) ? p[32] : (unsigned char)RARITY_COMMON;
		for (int i = 0; i < MAX_BONUSES; i++) {
			bKey[i] = (p[33 + i] >= 1 && p[33 + i] <= BONUS_COUNT) ? (unsigned char)(p[33 + i] - 1) : MOD_NONE;
			bVal[i] = (bKey[i] == MOD_NONE) ? 0 : (short)(p[37 + i*2] | (p[38 + i*2] << 8));
		}
	}
};

// An item's tier from its required level.
inline int ItemTier(int iLevelLimit)
{
	int t = (iLevelLimit + 13) / 14;
	return t < 1 ? 1 : (t > MAX_TIER ? MAX_TIER : t);
}

// ---- the character's Enchanting --------------------------------------------------------------------

// ---- life skills (VALORIA's skills.json; they replace Helbreath's Alchemy / Manufacturing / Crafting) ----
// Levels 1..LIFE_MAX, XP for a level: LIFE_CURVE * (level - 1)^2. Enchanting is the fifth skill; it keeps
// its own XP and levels (CharData::xp, EnchantLevel).
enum Life { LIFE_SMITHING, LIFE_CRAFTING, LIFE_ALCHEMY, LIFE_SLAYER, LIFE_COUNT };
const int LIFE_MAX   = 60;
const int LIFE_CURVE = 60;

inline const char * LifeName(int s)
{
	static const char * n[LIFE_COUNT] = { "Smithing & Forging", "Crafting", "Alchemy", "Slayer" };
	return (s >= 0 && s < LIFE_COUNT) ? n[s] : "?";
}
inline unsigned int LifeXPTotal(int lv) { return lv <= 1 ? 0u : (unsigned int)(LIFE_CURVE * (lv - 1) * (lv - 1)); }
inline int LifeLevel(unsigned int xp) { int lv = 1; while (lv < LIFE_MAX && xp >= LifeXPTotal(lv + 1)) lv++; return lv; }
// XP for a skill event: VALORIA's [base, per tier] (skills.json "xp"), times SKILL_XP_MULT (SkillXP)
struct LifeEvent { int skill, base, perTier; };
const LifeEvent LIFE_FORGE   = { LIFE_SMITHING, 30, 20 };	// made at the Blacksmith (Manufacture)
const LifeEvent LIFE_UPGRADE = { LIFE_SMITHING, 40, 0 };	// upgraded gear (Blessed Upgrade Stone)
const LifeEvent LIFE_CRAFT   = { LIFE_CRAFTING, 30, 20 };	// made in the Crafting Vessel
const LifeEvent LIFE_BREW    = { LIFE_ALCHEMY, 25, 15 };	// brewed in the Alchemy Pot
const LifeEvent LIFE_KILL    = { LIFE_SLAYER, 12, 6 };		// killed a City Hall quest target
const LifeEvent LIFE_BOUNTY  = { LIFE_SLAYER, 150, 40 };	// handed in a City Hall hunting quest
// Perks (skills.json "perks"):
inline int PerkCraftDiscountPct(int craftingLv) { const int v = craftingLv / 4; return v > 15 ? 15 : v; }	// Crafting: 1% per 4 levels
inline int PerkExtraBrewPct(int alchemyLv)	{ return alchemyLv; }			// Alchemy: 1% per level to brew one extra
inline bool PerkForgeRarity(int lv, int s)	{ return (s == LIFE_SMITHING && lv >= 20) || (s == LIFE_CRAFTING && lv >= 30); }	// a second roll for Rare
inline int PerkSlayerDmgPct(int slayerLv)	{ return slayerLv / 2; }		// Slayer: +1% against your quest target per 2 levels
inline int PerkSlayerGoldPct(int slayerLv)	{ return slayerLv >= 25 ? 25 : 0; }	// Slayer 25: quest gold +25%

struct CharData {
	unsigned int  xp;					// Enchanting XP
	unsigned char known[MOD_COUNT];		// highest tier each modifier was learned at (0 = not known)
	unsigned int  modXp[MOD_COUNT];		// each modifier's own enchanting XP
	unsigned int  lifeXp[LIFE_COUNT];	// life skill XP (Life)

	CharData() { Clear(); }
	void Clear() { xp = 0; memset(known, 0, sizeof(known)); memset(modXp, 0, sizeof(modXp)); memset(lifeXp, 0, sizeof(lifeXp)); }
	int  LifeLv(int s) const { return (s >= 0 && s < LIFE_COUNT) ? LifeLevel(lifeXp[s]) : 1; }

	void Write(unsigned char * p) const {
		memset(p, 0, CHAR_BYTES);
		for (int b = 0; b < 4; b++) p[b] = (unsigned char)((xp >> (8*b)) & 0xFF);
		for (int k = 0; k < MOD_COUNT; k++) p[4 + k] = known[k];
		for (int k = 0; k < MOD_COUNT; k++)
			for (int b = 0; b < 4; b++) p[14 + k*4 + b] = (unsigned char)((modXp[k] >> (8*b)) & 0xFF);
		for (int s = 0; s < LIFE_COUNT; s++)
			for (int b = 0; b < 4; b++) p[64 + s*4 + b] = (unsigned char)((lifeXp[s] >> (8*b)) & 0xFF);
	}
	void Read(const unsigned char * p) {
		xp = 0;
		for (int b = 0; b < 4; b++) xp |= (unsigned int)p[b] << (8*b);
		for (int k = 0; k < MOD_COUNT; k++) known[k] = (p[4 + k] <= MAX_TIER) ? p[4 + k] : MAX_TIER;
		for (int k = 0; k < MOD_COUNT; k++) {
			modXp[k] = 0;
			for (int b = 0; b < 4; b++) modXp[k] |= (unsigned int)p[14 + k*4 + b] << (8*b);
		}
		for (int s = 0; s < LIFE_COUNT; s++) {
			lifeXp[s] = 0;
			for (int b = 0; b < 4; b++) lifeXp[s] |= (unsigned int)p[64 + s*4 + b] << (8*b);
		}
	}
};

// Total Enchanting XP for level lv (level 1 = 100 XP: the Enchanter's Lesson).
inline unsigned int EnchantXPTotal(int lv)
{
	if (lv <= 1) return lv == 1 ? 100u : 0u;
	return 100u + (unsigned int)(80.0 * (double)(lv - 1) * (double)(lv - 1));
}

inline int EnchantLevel(unsigned int xp)
{
	int lv = 0;
	while (lv < ENCHANT_MAX && xp >= EnchantXPTotal(lv + 1)) lv++;
	return lv;
}

inline int EnchantSlots(int lv)	{ return lv <= 0 ? 0 : 1 + lv / 10; }
inline int EnchantTierCap(int lv)	{ return lv <= 0 ? 0 : 3 + lv / 8; }

// A modifier's own level: 1 when learned, up to 50.
inline unsigned int ModXPTotal(int lv)	{ return lv <= 1 ? 0u : (unsigned int)(30.0 * (double)(lv - 1) * (double)(lv - 1)); }
inline int ModLevel(const CharData & c, int k)
{
	if (k < 0 || k >= MOD_COUNT || c.known[k] == 0) return 0;
	int lv = 1;
	while (lv < MOD_MAX && c.modXp[k] >= ModXPTotal(lv + 1)) lv++;
	return lv;
}
// How strong this modifier's enchantments are: 0.5x at level 1 .. 1.6x at level 50.
inline double ModPower(int lv)	{ return 0.5 + 1.1 * (double)((lv < 1 ? 1 : lv) - 1) / (double)(MOD_MAX - 1); }

inline int CoreMod(int k, double v)
{
	if (!IsCore(k) || v == 0.0) return (int)floor(v + 0.5);
	int r = (int)floor(v + 0.5);
	return r < 1 ? 1 : (r > CORE_MOD_MAX ? CORE_MOD_MAX : r);
}

// The value an enchantment of modifier k gets on an item of this tier, at the character's levels.
inline int EnchantValueFor(const CharData & c, int k, int iItemTier)
{
	const int power = (iItemTier < c.known[k]) ? iItemTier : c.known[k];
	double base;
	switch (k) {
	case MOD_HP:   base = 10.0 * power; break;
	case MOD_MP:   base = 7.0 * power; break;
	case MOD_CRIT: base = 1.0 + power * 0.5; break;
	default:       base = 1.5 * power; break;		// core stats and defense
	}
	const double v = floor(base * ModPower(ModLevel(c, k)) + 0.5);
	if (IsCore(k)) return CoreMod(k, v < 1.0 ? 1.0 : v);
	return v < 1.0 ? 1 : (int)v;
}

struct Cost { int gold, shards, prismatic; };

// Enchanting an item that already has n enchantments (re-enchanting a modifier it has costs the same).
inline Cost EnchantCost(int iItemTier, int n)
{
	Cost c;
	c.gold = (150 * iItemTier * (n + 1) + GOLD_DIVISOR / 2) / GOLD_DIVISOR;
	c.shards = iItemTier + 2 * n;
	c.prismatic = (n >= 3) ? 1 : 0;
	return c;
}

// What one Empower level adds to modifier k (damage: +6% weapon damage).
inline int EmpowerStep(int k, int iItemTier)
{
	if (IsCore(k)) return 1;
	switch (k) {
	case MOD_HP:   return 5 * iItemTier;
	case MOD_MP:   return 4 * iItemTier;
	case MOD_CRIT: return 1 + iItemTier / 3;
	case MOD_DMG:  return 6;
	default:       return 1 + iItemTier / 2;		// defense
	}
}

// The next Empower level (lvl = the level it would reach, 1..5).
inline Cost EmpowerCost(int iItemTier, int lvl)
{
	Cost c;
	c.shards = 2 * lvl + iItemTier;
	c.prismatic = (lvl >= 4) ? 1 : 0;
	c.gold = (40 * iItemTier * lvl + GOLD_DIVISOR / 2) / GOLD_DIVISOR;
	return c;
}

// Mystic Shards from disenchanting (no rarity or runes in Helbreath, so no Prismatic Shards).
// VALORIA rune_amount: what one rune adds, by the item's tier.
inline int RuneAmount(int k, int iItemTier)
{
	if (IsCore(k)) return (2 + iItemTier < CORE_MOD_MAX) ? 2 + iItemTier : CORE_MOD_MAX;
	if (k == MOD_CRIT) return 1 + iItemTier / 2;
	if (k == MOD_DEF) return 1 + iItemTier;
	return 2 + iItemTier;
}

inline int DisenchantShards(int iItemTier, int iEmpowerTotal, int iRunes = 0)
{
	const int n = iItemTier / 2 + 2 * iRunes + iEmpowerTotal / 2;
	return n < 1 ? 1 : n;
}

// Enchanting XP for disenchanting (before SKILL_XP_MULT), and the XP each modifier on the item gets.
inline int DisenchantXP(int iItemTier, int iNewMods)	{ return (int)(8.0 * pow((double)iItemTier, 1.6)) + 20 * iNewMods; }
inline int DisenchantModXP(int iItemTier)				{ return (int)(6.0 * pow((double)iItemTier, 1.4)); }
inline int EnchantModXP(int iItemTier)					{ return 10 * iItemTier + 5; }
inline unsigned int SkillXP(int n)						{ const int v = (int)floor(n * SKILL_XP_MULT + 0.5); return (unsigned int)(v < 1 ? 1 : v); }

} // namespace Valoria

namespace Valoria {
// Character data saved between the game server and the login server: each bag item record is 67
// bytes and each warehouse item record 62, followed by the item's ITEM_BYTES VALORIA bytes.
const int BAG_RECORD  = 67 + ITEM_BYTES;
const int BANK_RECORD = 62 + ITEM_BYTES;
}

namespace Valoria {
// Equip positions (the same numbers on the server and the client: EQUIPPOS_* in char\item\Item.h).
inline bool IsGearPos(int pos)		{ return pos >= 1 && pos <= 13; }
inline bool IsWeaponPos(int pos)	{ return pos == 8 || pos == 9; }						// right hand, two hands
inline bool IsArmourPos(int pos)	{ return pos == 1 || pos == 2 || pos == 3 || pos == 4 || pos == 5 || pos == 7 || pos == 12 || pos == 13; }

// Modifiers that do something in Helbreath Reimagined yet. Crit is stored on VALORIA gear but not
// used in combat, so it can't be enchanted, learned or empowered until it is.
inline bool ModActive(int k)		{ return k >= 0 && k < MOD_COUNT; }

// The modifiers an item has, so the ones Empower can raise and Disenchant teaches: its own VALORIA
// stats (base[] in Mod order, STR..CRIT, from valoria_gear.txt), its enchantments, defense on armour
// and damage on weapons.
inline void ItemMods(const ItemData & d, const int * base, int iEquipPos, bool has[MOD_COUNT])
{
	for (int k = 0; k < MOD_COUNT; k++) has[k] = false;
	if (base != 0) for (int k = 0; k <= MOD_CRIT; k++) if (base[k] != 0) has[k] = true;
	for (int i = 0; i < MAX_ENCHANTS; i++) if (d.enKey[i] < MOD_COUNT) has[d.enKey[i]] = true;
	for (int i = 0; i < MAX_RUNES; i++) if (d.rune[i] < MOD_COUNT) has[d.rune[i]] = true;
	if (IsArmourPos(iEquipPos)) has[MOD_DEF] = true;
	if (IsWeaponPos(iEquipPos)) has[MOD_DMG] = true;
	for (int k = 0; k < MOD_COUNT; k++) if (!ModActive(k)) has[k] = false;
}

// What an item's enchantments and empowering add to modifier k while it is worn (damage: %).
inline int ItemBonus(const ItemData & d, int k, int iItemTier)
{
	if (!ModActive(k)) return 0;
	int v = (k < ENCHANTABLE) ? d.EnchantValue(k) : 0;
	if (d.em[k] > 0) v += (IsCore(k) ? CoreMod(k, d.em[k]) : d.em[k] * EmpowerStep(k, iItemTier));
	for (int i = 0; i < MAX_RUNES; i++) if (d.rune[i] == k) v += RuneAmount(k, iItemTier);
	if (k <= MOD_CRIT) v += d.BonusValue(k);		// rarity bonus stats
	return v;
}

// Physical (bMagic false) or magic resistance % an item's rarity bonuses give while it is worn.
inline int ItemResist(const ItemData & d, bool bMagic) { return d.BonusValue(bMagic ? BONUS_MAG_RES : BONUS_PHYS_RES); }
}
