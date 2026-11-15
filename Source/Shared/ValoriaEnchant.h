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
//   item data, 32 bytes:      6 x enchant key (uint8, Mod + 1, 0 = empty), 6 x enchant value (int16),
//                             10 x empower level (uint8, one per Mod), 4 spare
//   character data, 64 bytes: Enchanting XP (uint32), 10 x known tier (uint8, 0 = not learned),
//                             10 x modifier XP (uint32), 10 spare
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <cstring>

namespace Valoria {

enum Mod { MOD_STR, MOD_DEX, MOD_VIT, MOD_INT, MOD_MAG, MOD_HP, MOD_MP, MOD_DEF, MOD_CRIT, MOD_DMG, MOD_COUNT };
const unsigned char MOD_NONE = 255;

const int ENCHANTABLE   = MOD_CRIT + 1;	// weapon damage can be empowered, not enchanted
const int MAX_ENCHANTS  = 6;
const int EMPOWER_MAX   = 5;
const int ENCHANT_MAX   = 50;			// Enchanting level
const int MOD_MAX       = 50;			// each modifier's own enchanting level
const int CORE_MOD_MAX  = 5;
const int ITEM_BYTES    = 32;
const int CHAR_BYTES    = 64;
const int GOLD_DIVISOR  = 4;
const int MAX_TIER      = 10;
const double SKILL_XP_MULT = 0.5;		// VALORIA's skill XP multiplier

inline bool IsCore(int k) { return k >= MOD_STR && k <= MOD_MAG; }

inline const char * ModName(int k)
{
	static const char * n[MOD_COUNT] = { "STR", "DEX", "VIT", "INT", "MAG", "Max HP", "Max MP", "Defense", "Crit recharge %", "Damage %" };
	return (k >= 0 && k < MOD_COUNT) ? n[k] : "?";
}

// ---- items ---------------------------------------------------------------------------------------

struct ItemData {
	unsigned char enKey[MAX_ENCHANTS];
	short         enVal[MAX_ENCHANTS];
	unsigned char em[MOD_COUNT];

	ItemData() { Clear(); }
	void Clear() { memset(enKey, MOD_NONE, sizeof(enKey)); memset(enVal, 0, sizeof(enVal)); memset(em, 0, sizeof(em)); }
	bool Empty() const { return Enchants() == 0 && EmpowerTotal() == 0; }
	int  Enchants() const { int n = 0; for (int i = 0; i < MAX_ENCHANTS; i++) if (enKey[i] < MOD_COUNT) n++; return n; }
	int  EnchantSlotOf(int k) const { for (int i = 0; i < MAX_ENCHANTS; i++) if (enKey[i] == k) return i; return -1; }
	int  EnchantValue(int k) const { const int s = EnchantSlotOf(k); return s < 0 ? 0 : enVal[s]; }
	int  EmpowerTotal() const { int n = 0; for (int k = 0; k < MOD_COUNT; k++) n += em[k]; return n; }

	void Write(unsigned char * p) const {
		memset(p, 0, ITEM_BYTES);
		for (int i = 0; i < MAX_ENCHANTS; i++) p[i] = (enKey[i] < MOD_COUNT) ? (unsigned char)(enKey[i] + 1) : 0;	// 0 = empty, so all-zero bytes are an item without enchantments
		for (int i = 0; i < MAX_ENCHANTS; i++) { p[6 + i*2] = (unsigned char)(enVal[i] & 0xFF); p[7 + i*2] = (unsigned char)((enVal[i] >> 8) & 0xFF); }
		for (int k = 0; k < MOD_COUNT; k++) p[18 + k] = em[k];
	}
	void Read(const unsigned char * p) {
		for (int i = 0; i < MAX_ENCHANTS; i++) enKey[i] = (p[i] >= 1 && p[i] <= MOD_COUNT) ? (unsigned char)(p[i] - 1) : MOD_NONE;
		for (int i = 0; i < MAX_ENCHANTS; i++) enVal[i] = (short)(p[6 + i*2] | (p[7 + i*2] << 8));
		for (int k = 0; k < MOD_COUNT; k++) em[k] = (p[18 + k] <= EMPOWER_MAX) ? p[18 + k] : EMPOWER_MAX;
		for (int i = 0; i < MAX_ENCHANTS; i++) if (enKey[i] == MOD_NONE) enVal[i] = 0;
	}
};

// An item's tier from its required level.
inline int ItemTier(int iLevelLimit)
{
	int t = (iLevelLimit + 13) / 14;
	return t < 1 ? 1 : (t > MAX_TIER ? MAX_TIER : t);
}

// ---- the character's Enchanting --------------------------------------------------------------------

struct CharData {
	unsigned int  xp;					// Enchanting XP
	unsigned char known[MOD_COUNT];		// highest tier each modifier was learned at (0 = not known)
	unsigned int  modXp[MOD_COUNT];		// each modifier's own enchanting XP

	CharData() { Clear(); }
	void Clear() { xp = 0; memset(known, 0, sizeof(known)); memset(modXp, 0, sizeof(modXp)); }

	void Write(unsigned char * p) const {
		memset(p, 0, CHAR_BYTES);
		for (int b = 0; b < 4; b++) p[b] = (unsigned char)((xp >> (8*b)) & 0xFF);
		for (int k = 0; k < MOD_COUNT; k++) p[4 + k] = known[k];
		for (int k = 0; k < MOD_COUNT; k++)
			for (int b = 0; b < 4; b++) p[14 + k*4 + b] = (unsigned char)((modXp[k] >> (8*b)) & 0xFF);
	}
	void Read(const unsigned char * p) {
		xp = 0;
		for (int b = 0; b < 4; b++) xp |= (unsigned int)p[b] << (8*b);
		for (int k = 0; k < MOD_COUNT; k++) known[k] = (p[4 + k] <= MAX_TIER) ? p[4 + k] : MAX_TIER;
		for (int k = 0; k < MOD_COUNT; k++) {
			modXp[k] = 0;
			for (int b = 0; b < 4; b++) modXp[k] |= (unsigned int)p[14 + k*4 + b] << (8*b);
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
inline int DisenchantShards(int iItemTier, int iEmpowerTotal)
{
	const int n = iItemTier / 2 + iEmpowerTotal / 2;
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
// bytes and each warehouse item record 62, followed by the item's 32 VALORIA bytes.
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
inline bool ModActive(int k)		{ return k >= 0 && k < MOD_COUNT && k != MOD_CRIT; }

// The modifiers an item has, so the ones Empower can raise and Disenchant teaches: its own VALORIA
// stats (base[] in Mod order, STR..CRIT, from valoria_gear.txt), its enchantments, defense on armour
// and damage on weapons.
inline void ItemMods(const ItemData & d, const int * base, int iEquipPos, bool has[MOD_COUNT])
{
	for (int k = 0; k < MOD_COUNT; k++) has[k] = false;
	if (base != 0) for (int k = 0; k <= MOD_CRIT; k++) if (base[k] != 0) has[k] = true;
	for (int i = 0; i < MAX_ENCHANTS; i++) if (d.enKey[i] < MOD_COUNT) has[d.enKey[i]] = true;
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
	return v;
}
}
