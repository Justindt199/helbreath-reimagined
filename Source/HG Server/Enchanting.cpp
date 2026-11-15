// Enchanting.cpp: VALORIA's enchanting, disenchanting and empowering on the game server.
// The numbers (costs, values, XP, levels) are in Shared\ValoriaEnchant.h, shared with the client.
//
//   - the Enchanter's Tome (a tool item, like the Alchemy Bowl) opens the enchanting window on the
//     client; every request needs one in the bag.
//   - the Enchanter's Lesson: the first time, the Tome teaches Enchanting level 1 and gives an
//     Initiate's Blade with three modifiers on it, to disenchant and learn them.
//   - Disenchant (needs Enchanting): unravels an unequipped piece of gear into Mystic Shards, teaches
//     its modifiers (at the item's tier) and gives Enchanting XP and modifier XP. The only way to
//     level Enchanting.
//   - Enchant: puts a learned modifier on a piece of gear (gold, Mystic Shards, Prismatic Shards from
//     the 4th enchantment). Slots: 1 + level / 10. Gear tier up to 3 + level / 8. Enchanting a
//     modifier the item already has replaces its value.
//   - Empower: raises a modifier the item already has, up to 5 levels (no Enchanting needed).
//   - worn gear adds its enchantments and empowering to the character (CalcTotalItemEffect).
//
// Item and character data travel to the client in NOTIFY_ENCHANTITEMS / NOTIFY_ENCHANTSTATS and are
// saved with the character (see _iComposePlayerDataFileContents and the login server).
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "char\item\Item.h"
#include "Valoria.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"
#include "..\shared\Maths.h"

#include <string>

extern char g_cTxt[512];

using namespace Valoria;

namespace {

char kTome[]	= "EnchantersTome";
char kShard[]	= "MysticShard";
char kPrism[]	= "PrismaticShard";
char kGold[]	= "Gold";
char kBlade[]	= "InitiatesBlade";

enum { ENCH_ENCHANT = 1, ENCH_DISENCHANT, ENCH_EMPOWER, ENCH_LESSON };

bool IsGear(const CItem * it)	{ return it != NULL && it->m_cItemType == ITEMTYPE_EQUIP && IsGearPos(it->m_cEquipPos); }
int  Tier(const CItem * it)		{ return ItemTier(it->m_sLevelLimit); }

// The item's own VALORIA stats in Mod order (STR..CRIT), or false when it has none.
bool GearBase(const CItem * it, int base[MOD_CRIT + 1])
{
	const ValoriaGearBonus * b = Valoria_FindGear(it->m_cName);
	for (int k = 0; k <= MOD_CRIT; k++) base[k] = 0;
	if (b == NULL) return false;
	base[MOD_STR] = b->str; base[MOD_DEX] = b->dex; base[MOD_VIT] = b->vit; base[MOD_INT] = b->iint; base[MOD_MAG] = b->mag;
	base[MOD_HP] = b->hp;   base[MOD_MP] = b->mp;   base[MOD_DEF] = b->def; base[MOD_CRIT] = b->crit;
	return true;
}

void Mods(const CItem * it, bool has[MOD_COUNT])
{
	int base[MOD_CRIT + 1];
	const bool bBase = GearBase(it, base);
	ItemMods(it->m_valoria, bBase ? base : 0, it->m_cEquipPos, has);
}

std::string ItemLabel(const CItem * it) { return std::string(it->m_cName); }

// ShowClientMsg takes a char *: copy the text first.
void Say(CGame * g, int h, const char * s)
{
	char b[256];
	strncpy(b, s, sizeof(b) - 1);
	b[sizeof(b) - 1] = 0;
	g->ShowClientMsg(h, b);
}

} // namespace

//////////////////////////////////////////////////////////////////////
// To the client

// Your Enchanting: XP, learned modifiers and their XP (64 bytes).
void CGame::EnchantSendStats(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	char cData[16 + CHAR_BYTES] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	p->m_enchant.Write((unsigned char *)cp);
	cp += CHAR_BYTES;
	WantedSendRaw(iClientH, NOTIFY_ENCHANTSTATS, cData, cp);
}

// The enchantments of every item in the bag and the warehouse: the client clears its own copy and
// takes these (uint8 count, then per item: uint8 0 = bag / 1 = warehouse, uint8 index, 32 bytes).
void CGame::EnchantSendItems(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	static char cData[16 + (MAXITEMS + MAXBANKITEMS) * (2 + ITEM_BYTES)];
	char * cp = cData + INDEX2_MSGTYPE + 2;
	char * pCount = cp++;
	uint8 n = 0;
	for (int w = 0; w < 2; w++) {
		const int iMax = (w == 0) ? MAXITEMS : MAXBANKITEMS;
		for (int i = 0; i < iMax; i++) {
			CItem * it = (w == 0) ? p->m_pItemList[i] : p->m_pItemInBankList[i];
			if (it == NULL || it->m_valoria.Empty()) continue;
			*cp++ = (char)w;
			*cp++ = (char)i;
			it->m_valoria.Write((unsigned char *)cp);
			cp += ITEM_BYTES;
			n++;
		}
	}
	*pCount = (char)n;
	WantedSendRaw(iClientH, NOTIFY_ENCHANTITEMS, cData, cp);
}

// Sends the item enchantments after an item with some arrived at a new place (bag or warehouse).
void CGame::EnchantItemMoved(int iClientH, CItem * pItem)
{
	if (pItem != NULL && !pItem->m_valoria.Empty()) EnchantSendItems(iClientH);
}

// Worn gear: adds the enchantments and empowering to the gear totals (called from CalcTotalItemEffect).
void CGame::EnchantGearBonus(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	p->m_gearDmgPct = 0;
	for (int i = 0; i < MAXITEMS; i++) {
		CItem * it = p->m_pItemList[i];
		if (it == NULL || !p->m_bIsItemEquipped[i] || it->m_valoria.Empty()) continue;
		const ItemData & d = it->m_valoria;
		const int t = Tier(it);
		p->m_gearStr  += ItemBonus(d, MOD_STR, t);
		p->m_gearDex  += ItemBonus(d, MOD_DEX, t);
		p->m_gearVit  += ItemBonus(d, MOD_VIT, t);
		p->m_gearInt  += ItemBonus(d, MOD_INT, t);
		p->m_gearMag  += ItemBonus(d, MOD_MAG, t);
		p->m_gearHP   += ItemBonus(d, MOD_HP, t);
		p->m_gearMP   += ItemBonus(d, MOD_MP, t);
		p->m_gearDef  += ItemBonus(d, MOD_DEF, t);
		p->m_gearCrit += ItemBonus(d, MOD_CRIT, t);
		if (IsWeaponPos(it->m_cEquipPos)) p->m_gearDmgPct += ItemBonus(d, MOD_DMG, t);
	}
}

//////////////////////////////////////////////////////////////////////
// Helpers

namespace {
bool PayItems(CGame * g, int h, char * pName, uint32 n)
{
	if (n == 0) return true;
	const uint32 have = g->dwGetItemCount(h, pName);
	if (have < n) return false;
	g->SetItemCount(h, pName, have - n);
	return true;
}
}

// Adds Enchanting XP (SKILL_XP_MULT applied) and announces a new level.
void CGame::EnchantAddXP(int iClientH, int n)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || n <= 0) return;
	const int before = EnchantLevel(p->m_enchant.xp);
	p->m_enchant.xp += SkillXP(n);
	const int after = EnchantLevel(p->m_enchant.xp);
	if (after > before) {
		wsprintf(g_cTxt, "Enchanting level %d! (up to %d enchantments per item, gear up to tier %d)", after, EnchantSlots(after), EnchantTierCap(after));
		ShowClientMsg(iClientH, g_cTxt);
	}
}

// Modifier XP (no multiplier, as in VALORIA); announces a new modifier level.
void CGame::EnchantAddModXP(int iClientH, int k, int n)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || n <= 0 || k < 0 || k >= MOD_COUNT || p->m_enchant.known[k] == 0) return;
	const int before = ModLevel(p->m_enchant, k);
	p->m_enchant.modXp[k] += (unsigned int)n;
	const int after = ModLevel(p->m_enchant, k);
	if (after > before) {
		wsprintf(g_cTxt, "%s enchanting level %d! (%s enchantments are now %d%% strength)", ModName(k), after, ModName(k), (int)floor(ModPower(after) * 100.0 + 0.5));
		ShowClientMsg(iClientH, g_cTxt);
	}
}

//////////////////////////////////////////////////////////////////////
// From the client: MSGID_REQUEST_ENCHANT, uint16 action, uint8 item index, uint8 modifier

void CGame::EnchantRequest(int iClientH, char * pData)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->m_bIsInitComplete || p->IsDead()) return;
	uint16 wAction;
	uint8 idx, key;
	Pop(pData, wAction);
	Pop(pData, idx);
	Pop(pData, key);

	const uint32 t = timeGetTime();
	if (p->m_dwEnchantBusy != 0 && t - p->m_dwEnchantBusy < 300) return;
	p->m_dwEnchantBusy = t;

	if (dwGetItemCount(iClientH, kTome) == 0) {
		Say(this, iClientH, "You need an Enchanter's Tome (sold by the Shopkeeper).");
		return;
	}
	CharData & c = p->m_enchant;
	const int lv = EnchantLevel(c.xp);

	if (wAction == ENCH_LESSON) {
		if (lv > 0) return;
		c.xp = EnchantXPTotal(1);
		Say(this, iClientH, "You learned Enchanting (level 1). Disenchant the Initiate's Blade to learn its modifiers.");
		CItem * pBlade = new CItem;
		if (pBlade->InitItemAttr(kBlade)) {
			// VALORIA's lesson blade is Epic: three rolled modifiers to learn from. Helbreath has no
			// rarity, so the blade comes with three of the modifiers enchanted on, at its tier.
			const int bt = Tier(pBlade);
			CharData roll;
			int keys[ENCHANTABLE], nk = 0;
			for (int k = 0; k < ENCHANTABLE; k++) if (ModActive(k)) { keys[nk++] = k; roll.known[k] = (unsigned char)bt; }
			for (int e = 0; e < 3 && nk > 0; e++) {
				const int pick = dice(1, nk) - 1;
				pBlade->m_valoria.enKey[e] = (unsigned char)keys[pick];
				pBlade->m_valoria.enVal[e] = (short)EnchantValueFor(roll, keys[pick], bt);
				keys[pick] = keys[--nk];
			}
			pBlade->m_sTouchEffectType = ITET_ID;
			pBlade->m_sTouchEffectValue1 = dice(1, 100000);
			pBlade->m_sTouchEffectValue2 = dice(1, 100000);
			pBlade->m_sTouchEffectValue3 = (short)timeGetTime();
			bAddItem(iClientH, pBlade);
		}
		else delete pBlade;
		EnchantSendStats(iClientH);
		return;
	}

	if (idx >= MAXITEMS || p->m_pItemList[idx] == NULL) return;
	CItem * it = p->m_pItemList[idx];
	if (it->m_disabled) return;
	if (!IsGear(it)) { Say(this, iClientH, "Only gear can be enchanted, disenchanted or empowered."); return; }
	const int tier = Tier(it);
	ItemData & d = it->m_valoria;
	bool has[MOD_COUNT];
	Mods(it, has);
	char cTxt[200];

	switch (wAction) {
	case ENCH_DISENCHANT: {
		if (lv <= 0) { Say(this, iClientH, "Learn Enchanting first: use the Enchanter's Tome."); return; }
		if (p->m_bIsItemEquipped[idx]) { Say(this, iClientH, "Unequip it first."); return; }
		// learn its modifiers (at the item's tier) and practise every one of them
		std::string learned;
		int iNew = 0;
		for (int k = 0; k < MOD_COUNT; k++) {
			if (!has[k] || k >= ENCHANTABLE) continue;
			if (tier > c.known[k]) {
				if (c.known[k] == 0) c.modXp[k] = 0;
				c.known[k] = (unsigned char)tier;
				if (!learned.empty()) learned += ", ";
				learned += ModName(k);
				iNew++;
			}
		}
		const int shards = DisenchantShards(tier, d.EmpowerTotal());
		wsprintf(cTxt, "Disenchanted %s -> %d Mystic Shard%s (+%d Enchanting XP)", it->m_cName, shards, shards == 1 ? "" : "s", (int)SkillXP(DisenchantXP(tier, iNew)));
		ItemDepleteHandler(iClientH, idx, false);
		CItem * pShard = new CItem;
		if (pShard->InitItemAttr(kShard)) { pShard->m_dwCount = shards; bAddItem(iClientH, pShard); }
		else delete pShard;
		ShowClientMsg(iClientH, cTxt);
		if (!learned.empty()) {
			std::string s = "Learned for Enchanting: " + learned;
			ShowClientMsg(iClientH, (char *)s.c_str());
		}
		for (int k = 0; k < ENCHANTABLE; k++) if (has[k] && c.known[k] > 0) EnchantAddModXP(iClientH, k, DisenchantModXP(tier));
		EnchantAddXP(iClientH, DisenchantXP(tier, iNew));
		EnchantSendStats(iClientH);
		EnchantSendItems(iClientH);
		return; }

	case ENCH_ENCHANT: {
		if (key >= ENCHANTABLE || !ModActive(key)) return;
		if (lv <= 0) { Say(this, iClientH, "Learn Enchanting first: use the Enchanter's Tome."); return; }
		if (tier > EnchantTierCap(lv)) { wsprintf(cTxt, "Tier %d gear needs Enchanting %d.", tier, (tier - 3) * 8); ShowClientMsg(iClientH, cTxt); return; }
		if (c.known[key] == 0) { Say(this, iClientH, "You haven't learned that modifier: disenchant gear that has it."); return; }
		int slot = d.EnchantSlotOf(key);
		const int n = d.Enchants();
		if (slot < 0 && n >= EnchantSlots(lv)) { wsprintf(cTxt, "No enchant slots left (%d).", EnchantSlots(lv)); ShowClientMsg(iClientH, cTxt); return; }
		const Cost cost = EnchantCost(tier, n);
		if (dwGetItemCount(iClientH, kGold) < (uint32)cost.gold) { wsprintf(cTxt, "You need %d gold.", cost.gold); ShowClientMsg(iClientH, cTxt); return; }
		if (dwGetItemCount(iClientH, kShard) < (uint32)cost.shards) { wsprintf(cTxt, "You need %d Mystic Shards.", cost.shards); ShowClientMsg(iClientH, cTxt); return; }
		if (dwGetItemCount(iClientH, kPrism) < (uint32)cost.prismatic) { Say(this, iClientH, "You need a Prismatic Shard."); return; }
		PayItems(this, iClientH, kGold, cost.gold);
		PayItems(this, iClientH, kShard, cost.shards);
		PayItems(this, iClientH, kPrism, cost.prismatic);
		if (slot < 0) for (int i = 0; i < MAX_ENCHANTS; i++) if (d.enKey[i] == MOD_NONE) { slot = i; break; }
		if (slot < 0) return;
		d.enKey[slot] = key;
		d.enVal[slot] = (short)EnchantValueFor(c, key, tier);
		wsprintf(cTxt, "Enchanted %s with %s +%d (%s level %d)", it->m_cName, ModName(key), (int)d.enVal[slot], ModName(key), ModLevel(c, key));
		ShowClientMsg(iClientH, cTxt);
		EnchantAddModXP(iClientH, key, EnchantModXP(tier));
		break; }

	case ENCH_EMPOWER: {
		if (key >= MOD_COUNT || !has[key]) { Say(this, iClientH, "The item doesn't have that modifier."); return; }
		if (d.em[key] >= EMPOWER_MAX) { Say(this, iClientH, "Already fully empowered."); return; }
		const Cost cost = EmpowerCost(tier, d.em[key] + 1);
		if (dwGetItemCount(iClientH, kShard) < (uint32)cost.shards) { wsprintf(cTxt, "You need %d Mystic Shards.", cost.shards); ShowClientMsg(iClientH, cTxt); return; }
		if (dwGetItemCount(iClientH, kPrism) < (uint32)cost.prismatic) { Say(this, iClientH, "You need a Prismatic Shard."); return; }
		if (dwGetItemCount(iClientH, kGold) < (uint32)cost.gold) { wsprintf(cTxt, "You need %d gold.", cost.gold); ShowClientMsg(iClientH, cTxt); return; }
		PayItems(this, iClientH, kShard, cost.shards);
		PayItems(this, iClientH, kPrism, cost.prismatic);
		PayItems(this, iClientH, kGold, cost.gold);
		d.em[key]++;
		wsprintf(cTxt, "Empowered %s: %s level %d/%d", it->m_cName, ModName(key), (int)d.em[key], EMPOWER_MAX);
		ShowClientMsg(iClientH, cTxt);
		break; }

	default:
		return;
	}

	// the item changed: the client's copy, and the character if it is worn
	if (p->m_pItemList[idx] == it && p->m_bIsItemEquipped[idx]) CalcTotalItemEffect(iClientH, -1, false);
	EnchantSendStats(iClientH);
	EnchantSendItems(iClientH);
}
