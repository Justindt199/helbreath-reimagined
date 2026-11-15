// ValoriaUse.h: VALORIA's usable items in Helbreath Reimagined, shared by the game server
// (HG Server\ValoriaUse.cpp) and the client (UI\ValoriaUse.cpp).
//
//   * Elixirs and the Scroll of Insight (item effect ITEMEFFECTTYPE_VALORIA_BUFF, value 1 = buff):
//     a timed buff with VALORIA's values and durations. Using one again restarts its timer; different
//     buffs stack. Buffs end at log out.
//   * Runes and the Blessed Upgrade Stone (item effect ITEMEFFECTTYPE_VALORIA_INFUSE, value 1 = rune
//     modifier + 1, or INFUSE_BLESSED): double-click, then click the gear in your bag.
//       - a rune adds Valoria::RuneAmount to the item, at most Valoria::MAX_RUNES per item
//       - the Blessed Upgrade Stone adds +1 to the item's Helbreath upgrade level (up to +15) on the gear
//         Helbreath upgrades (weapons, bows, shields, armour, robes, capes), not on owner-bound items
//
// Not ported: Upgrade Stone, Reforging / Ascension / Radiant Orbs, Scroll of the Crypt, Scroll of the Smith's Luck.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdio>
#include <cstring>
#include "ValoriaEnchant.h"

#define ITEMEFFECTTYPE_VALORIA_INFUSE	35
#define ITEMEFFECTTYPE_VALORIA_BUFF		36

namespace ValoriaUse {

const int INFUSE_BLESSED = 100;
const int MAX_UPGRADE    = 15;

enum Buff { BUFF_STRENGTH, BUFF_IRONSKIN, BUFF_SWIFTNESS, BUFF_WISDOM, BUFF_INSIGHT, BUFF_FORTUNE, BUFF_COUNT };

struct BuffDef {
	const char * name;
	int seconds;
	int str, dex, intl, mag, def;	// Valoria modifiers
	int xpPct, dodge;				// dodge: % chance to avoid a melee hit
	int goldPct, itemPct;			// gold find, item find
};

static const BuffDef BUFFS[BUFF_COUNT] = {
	{ "Elixir of Strength",  180, 10, 0,  0,  0,  0,  0,  0 },
	{ "Elixir of Iron Skin", 180,  0, 0,  0,  0, 15,  0,  0 },
	{ "Elixir of Swiftness", 180,  0, 5,  0,  0,  0,  0,  5 },
	{ "Elixir of Wisdom",    180,  0, 0, 15, 15,  0,  0,  0 },
	{ "Scroll of Insight",   600,  0, 0,  0,  0,  0, 50,  0 },
	{ "Elixir of Fortune",   300,  0, 0,  0,  0,  0,  0,  0, 50, 50 },
};

// The buff's lines for tooltips: "+10 STR", "+50% XP", "+5% Dodge".
inline int BuffLines(int b, char out[4][48])
{
	if (b < 0 || b >= BUFF_COUNT) return 0;
	const BuffDef & d = BUFFS[b];
	int n = 0;
	if (d.str)  _snprintf(out[n++], 47, "+%d STR", d.str);
	if (d.dex)  _snprintf(out[n++], 47, "+%d DEX", d.dex);
	if (d.intl) _snprintf(out[n++], 47, "+%d INT", d.intl);
	if (d.mag)  _snprintf(out[n++], 47, "+%d MAG", d.mag);
	if (d.def)  _snprintf(out[n++], 47, "+%d Defense", d.def);
	if (d.xpPct) _snprintf(out[n++], 47, "+%d%% XP", d.xpPct);
	if (d.goldPct && n < 4) _snprintf(out[n++], 47, "+%d%% Gold find", d.goldPct);
	if (d.itemPct && n < 4) _snprintf(out[n++], 47, "+%d%% Item find", d.itemPct);
	if (d.dodge && n < 4) _snprintf(out[n++], 47, "+%d%% Dodge", d.dodge);
	for (int i = 0; i < n; i++) out[i][47] = 0;
	return n;
}

// The client knows bag items by name only: which usable item a name is (the rows that
// tools/valoria_port/build_items.py writes). Returns the item effect (0 = none) and its value 1.
inline int UseKindOf(const char * pName, int * pValue)
{
	static const struct { const char * name; int eff, v; } k[] = {
		{ "ElixirOfStrength", ITEMEFFECTTYPE_VALORIA_BUFF, BUFF_STRENGTH }, { "ElixirOfIronSkin", ITEMEFFECTTYPE_VALORIA_BUFF, BUFF_IRONSKIN },
		{ "ElixirOfSwiftness", ITEMEFFECTTYPE_VALORIA_BUFF, BUFF_SWIFTNESS }, { "ElixirOfWisdom", ITEMEFFECTTYPE_VALORIA_BUFF, BUFF_WISDOM },
		{ "ScrollOfInsight", ITEMEFFECTTYPE_VALORIA_BUFF, BUFF_INSIGHT }, { "ElixirOfFortune", ITEMEFFECTTYPE_VALORIA_BUFF, BUFF_FORTUNE },
		{ "RuneOfPrecision", ITEMEFFECTTYPE_VALORIA_INFUSE, Valoria::MOD_CRIT + 1 },
		{ "RuneOfMight", ITEMEFFECTTYPE_VALORIA_INFUSE, Valoria::MOD_STR + 1 }, { "RuneOfAgility", ITEMEFFECTTYPE_VALORIA_INFUSE, Valoria::MOD_DEX + 1 },
		{ "RuneOfVigor", ITEMEFFECTTYPE_VALORIA_INFUSE, Valoria::MOD_VIT + 1 }, { "RuneOfIntellect", ITEMEFFECTTYPE_VALORIA_INFUSE, Valoria::MOD_INT + 1 },
		{ "RuneOfSorcery", ITEMEFFECTTYPE_VALORIA_INFUSE, Valoria::MOD_MAG + 1 }, { "RuneOfWarding", ITEMEFFECTTYPE_VALORIA_INFUSE, Valoria::MOD_DEF + 1 },
		{ "BlessedUpgradeStone", ITEMEFFECTTYPE_VALORIA_INFUSE, INFUSE_BLESSED },
	};
	for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); i++)
		if (strcmp(pName, k[i].name) == 0) { *pValue = k[i].v; return k[i].eff; }
	return 0;
}

// Gear the Blessed Upgrade Stone works on: Helbreath's upgradable categories.
inline bool BlessedCategory(int iCategory)
{
	switch (iCategory) { case 1: case 3: case 5: case 6: case 11: case 12: case 13: case 15: return true; }
	return false;
}

}	// namespace ValoriaUse
