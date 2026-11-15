// ValoriaUse.cpp: VALORIA's usable items on the game server (rules: Shared\ValoriaUse.h).
//   - elixirs and the Scroll of Insight: timed buffs (UseItemHandler -> ValoriaUseBuff), added to the
//     character in CalcTotalItemEffect (ValoriaBuffBonus), ended by ValoriaBuffTick (every 3 seconds)
//   - runes and the Blessed Upgrade Stone: used on gear in the bag (_bDepleteDestTypeItemUseEffect ->
//     ValoriaInfuse)
// The client hears about buffs in NOTIFY_VALORIABUFFS, and about runes through the enchanting item list.
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "char\item\Item.h"
#include "char\Quest.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"
#include "..\shared\ValoriaUse.h"
#include "..\shared\ValoriaPassives.h"
#include "..\shared\Maths.h"
#include "..\shared\npcType.h"
#include "char\Npc.h"

#include <cstring>

extern char g_cTxt[512];

using namespace ValoriaUse;

// Buffs carry over log out: the time each has left is saved in the passive tree block's spare bytes
// (char_database.ValoriaPassive), one byte per buff in 5-second steps, so up to 21 minutes.
const int BUFF_SAVE_AT = 40;
const int BUFF_SAVE_STEP = 5;
static_assert(BUFF_SAVE_AT > ValoriaPassive::BIT_BYTES && BUFF_SAVE_AT + BUFF_COUNT <= ValoriaPassive::DATA_BYTES,
	"saved buffs must sit in the passive block's spare bytes");

void CGame::ValoriaSaveBuffs(int iClientH, unsigned char * pPassive)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	const uint32 t = timeGetTime();
	for (int b = 0; b < BUFF_COUNT; b++) {
		uint32 left = (p->m_valBuffEnd[b] != 0 && (int)(p->m_valBuffEnd[b] - t) > 0) ? (p->m_valBuffEnd[b] - t + 999) / 1000 : 0;
		left = (left + BUFF_SAVE_STEP - 1) / BUFF_SAVE_STEP;
		pPassive[BUFF_SAVE_AT + b] = (unsigned char)(left > 255 ? 255 : left);
	}
}

void CGame::ValoriaLoadBuffs(CClient * p, const unsigned char * pPassive)
{
	if (p == NULL) return;
	const uint32 t = timeGetTime();
	for (int b = 0; b < BUFF_COUNT; b++) {
		const uint32 left = (uint32)pPassive[BUFF_SAVE_AT + b] * BUFF_SAVE_STEP;
		p->m_valBuffEnd[b] = left ? t + left * 1000 : 0;
	}
}

namespace {
void Say(CGame * g, int h, const char * s)
{
	char b[256];
	strncpy(b, s, sizeof(b) - 1);
	b[sizeof(b) - 1] = 0;
	g->ShowClientMsg(h, b);
}
}

// Your running buffs: uint8 count, then per buff uint8 buff, uint16 seconds left.
void CGame::ValoriaSendBuffs(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	char cData[16 + 1 + BUFF_COUNT * 3] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	char * pCount = cp++;
	const uint32 t = timeGetTime();
	uint8 n = 0;
	for (int b = 0; b < BUFF_COUNT; b++) {
		if (p->m_valBuffEnd[b] == 0) continue;
		const uint32 left = (p->m_valBuffEnd[b] > t) ? (p->m_valBuffEnd[b] - t + 999) / 1000 : 0;
		Push(cp, (uint8)b);
		Push(cp, (uint16)left);
		n++;
	}
	*pCount = (char)n;
	WantedSendRaw(iClientH, NOTIFY_VALORIABUFFS, cData, cp);
}

// An elixir or the Scroll of Insight. Returns false when it isn't used up.
bool CGame::ValoriaUseBuff(int iClientH, int iBuff, int iLevelLimit)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || iBuff < 0 || iBuff >= BUFF_COUNT) return false;
	if (p->m_iLevel < iLevelLimit) {
		wsprintf(g_cTxt, "You need level %d for that.", iLevelLimit);
		Say(this, iClientH, g_cTxt);
		return false;
	}
	const BuffDef & d = BUFFS[iBuff];
	p->m_valBuffEnd[iBuff] = timeGetTime() + (uint32)d.seconds * 1000;
	wsprintf(g_cTxt, "%s: active for %d:%02d.", d.name, d.seconds / 60, d.seconds % 60);
	Say(this, iClientH, g_cTxt);
	CalcTotalItemEffect(iClientH, -1, false);
	ValoriaSendBuffs(iClientH);
	return true;
}

// Adds the running buffs to the gear totals (CalcTotalItemEffect, after the passive tree).
void CGame::ValoriaBuffBonus(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	for (int b = 0; b < BUFF_COUNT; b++) {
		if (p->m_valBuffEnd[b] == 0) continue;
		const BuffDef & d = BUFFS[b];
		p->m_gearStr += d.str;
		p->m_gearDex += d.dex;
		p->m_gearInt += d.intl;
		p->m_gearMag += d.mag;
		p->m_gearDef += d.def;
		p->m_passXpPct += d.xpPct;
		p->m_val.goldFind += d.goldPct;
		p->m_val.itemFind += d.itemPct;
		p->m_val.dodge += d.dodge;
	}
	// VALORIA crit (gear, enchantments, runes, passives): each 1% gives 0.4% chance that a landed hit wins
	// back a critical charge, at most 25% (VALORIA's CRIT_CHARGE_SHARE and CRIT_CAP)
	p->m_val.critCharge = (p->m_gearCrit * 2 / 5 < 25) ? p->m_gearCrit * 2 / 5 : 25;
}

// A Helbreath boss (the same five that drop the passive relics: tools/valoria_port/build_passives.py).
static bool IsBoss(int iType)
{
	return iType == NPC_HELLCLAW || iType == NPC_TIGERWORM || iType == NPC_WYVERN || iType == NPC_FIRE_WYVERN || iType == NPC_ABADDON;
}

// A dropped piece of gear rolls its rarity (better from bosses, and with the killer's item find) and a
// bonus stat per step above Common: VALORIA's make_drop.
void CGame::ValoriaRollRarity(CItem * pItem, int iNpcType, short sKillerH)
{
	if (pItem == NULL || pItem->m_cItemType != ITEMTYPE_EQUIP || !Valoria::IsGearPos(pItem->m_cEquipPos)) return;
	const int find = (sKillerH > 0 && sKillerH < MAXCLIENTS && m_pClientList[sKillerH] != NULL) ? m_pClientList[sKillerH]->m_val.itemFind : 0;
	const int r = Valoria::RollRarity(dice(1, 1000000) / 1000001.0, IsBoss(iNpcType), find);
	if (r == Valoria::RARITY_COMMON) return;
	pItem->m_valoria.rarity = (unsigned char)r;
	pItem->m_valoria.RollBonuses(r, Valoria::IsArmourPos(pItem->m_cEquipPos), Valoria::ItemTier(pItem->m_sLevelLimit),
		[](int lo, int hi) { return lo + (int)dice(1, hi - lo + 1) - 1; });
}

// Boss damage: the extra a player's blow or spell does to a boss.
int CGame::ValoriaBossDamage(short sAttackerH, short sNpcH, int iDamage)
{
	CClient * p = m_pClientList[sAttackerH];
	if (p == NULL || m_pNpcList[sNpcH] == NULL) return iDamage;
	int pct = 0;
	if (p->m_val.bossDmg > 0 && IsBoss(m_pNpcList[sNpcH]->m_sType)) pct += p->m_val.bossDmg;
	// Slayer: +1% per 2 levels against the monster your City Hall hunting quest is for
	if (p->m_iQuest > 0 && m_pQuestConfigList[p->m_iQuest] != NULL && m_pQuestConfigList[p->m_iQuest]->m_iType == QUESTTYPE_MONSTERHUNT
		&& m_pQuestConfigList[p->m_iQuest]->m_iTargetType == m_pNpcList[sNpcH]->m_sType)
		pct += Valoria::PerkSlayerDmgPct(p->m_enchant.LifeLv(Valoria::LIFE_SLAYER));
	return pct > 0 ? iDamage + iDamage * pct / 100 : iDamage;
}

// A player is about to take iDamage. Returns what they take: 0 when they dodge (melee only).
// Damage taken lowers it; thorns hit the attacker back for a fixed amount (melee only, never lethal).
int CGame::ValoriaHurt(short sTargetH, short sAttackerH, char cAttackerType, int iDamage, bool bMelee)
{
	CClient * p = m_pClientList[sTargetH];
	if (p == NULL || iDamage <= 0) return iDamage;
	const CClient::ValoriaCombat & v = p->m_val;
	if (bMelee && v.dodge > 0 && dice(1, 100) <= v.dodge) return 0;
	// rarity resistances: % of a physical (blows, arrows) or magic hit taken off
	const int res = bMelee ? p->m_gearPhysRes : p->m_gearMagRes;
	if (res > 0) { iDamage -= iDamage * res / 100; if (iDamage < 1) iDamage = 1; }
	if (v.dmgRed > 0) { iDamage -= iDamage * v.dmgRed / 100; if (iDamage < 1) iDamage = 1; }
	if (bMelee && v.thorns > 0) {
		if (cAttackerType == OWNERTYPE_PLAYER && m_pClientList[sAttackerH] != NULL && m_pClientList[sAttackerH]->m_iHP > 1) {
			CClient * a = m_pClientList[sAttackerH];
			a->m_iHP = (a->m_iHP - v.thorns > 1) ? a->m_iHP - v.thorns : 1;
			SendNotifyMsg(NULL, sAttackerH, NOTIFY_HP, NULL, NULL, NULL, NULL);
		}
		else if (cAttackerType == OWNERTYPE_NPC && m_pNpcList[sAttackerH] != NULL && m_pNpcList[sAttackerH]->m_iHP > 1)
			m_pNpcList[sAttackerH]->m_iHP = (m_pNpcList[sAttackerH]->m_iHP - v.thorns > 1) ? m_pNpcList[sAttackerH]->m_iHP - v.thorns : 1;
	}
	return iDamage;
}

// A player's blow or spell landed for iDamage: crit recharge, life steal, mana steal.
void CGame::ValoriaOnHit(short sAttackerH, int iDamage)
{
	CClient * p = m_pClientList[sAttackerH];
	if (p == NULL || iDamage <= 0 || p->m_bIsKilled) return;
	const CClient::ValoriaCombat & v = p->m_val;
	if (v.critCharge > 0 && p->m_iSuperAttackLeft < p->m_iLevel / 10 && dice(1, 100) <= v.critCharge) {
		p->m_iSuperAttackLeft++;
		SendNotifyMsg(NULL, sAttackerH, NOTIFY_SUPERATTACKLEFT, NULL, NULL, NULL, NULL);
	}
	if (v.lifeSteal > 0 && p->m_iHP < p->GetMaxHP()) {
		const int heal = (iDamage * v.lifeSteal + 50) / 100;
		if (heal > 0) { p->m_iHP = (p->m_iHP + heal < p->GetMaxHP()) ? p->m_iHP + heal : p->GetMaxHP(); SendNotifyMsg(NULL, sAttackerH, NOTIFY_HP, NULL, NULL, NULL, NULL); }
	}
	if (v.manaSteal > 0 && p->m_iMP < p->GetMaxMP()) {
		const int mana = (iDamage * v.manaSteal + 50) / 100;
		if (mana > 0) { p->m_iMP = (p->m_iMP + mana < p->GetMaxMP()) ? p->m_iMP + mana : p->GetMaxMP(); SendNotifyMsg(NULL, sAttackerH, NOTIFY_MP, NULL, NULL, NULL, NULL); }
	}
}

// Ends buffs whose time is up (from CheckClientResponseTime, every 3 seconds).
void CGame::ValoriaBuffTick(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	if (p->m_bValoriaItemsDirty) { p->m_bValoriaItemsDirty = false; EnchantSendItems(iClientH); }	// after the item itself arrived
	const uint32 t = timeGetTime();
	bool bEnded = false;
	for (int b = 0; b < BUFF_COUNT; b++) {
		if (p->m_valBuffEnd[b] == 0 || (int)(p->m_valBuffEnd[b] - t) > 0) continue;
		p->m_valBuffEnd[b] = 0;
		wsprintf(g_cTxt, "%s has worn off.", BUFFS[b].name);
		Say(this, iClientH, g_cTxt);
		bEnded = true;
	}
	if (!bEnded) return;
	CalcTotalItemEffect(iClientH, -1, false);
	ValoriaSendBuffs(iClientH);
}

// A rune or the Blessed Upgrade Stone used on the bag item sDest. Returns true when it is used up.
bool CGame::ValoriaInfuse(int iClientH, short sItemIndex, short sDest)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || sDest < 0 || sDest >= MAXITEMS || sDest == sItemIndex) return false;
	CItem * pUse = p->m_pItemList[sItemIndex];
	CItem * pDest = p->m_pItemList[sDest];
	if (pUse == NULL || pDest == NULL || pDest->m_disabled) return false;
	if (p->m_iLevel < pUse->m_sLevelLimit) {
		wsprintf(g_cTxt, "You need level %d for that.", pUse->m_sLevelLimit);
		Say(this, iClientH, g_cTxt);
		return false;
	}

	const int v = pUse->m_sItemEffectValue1;
	if (v == INFUSE_REFORGE || v == INFUSE_ASCEND || v == INFUSE_RADIANT) {	// VALORIA's orbs (apply_enhance)
		if (!Valoria::IsGearPos(pDest->m_cEquipPos) || pDest->m_cItemType != ITEMTYPE_EQUIP) { Say(this, iClientH, "Orbs only work on gear."); return false; }
		Valoria::ItemData & d = pDest->m_valoria;
		const int r = d.rarity;
		const bool bArmour = Valoria::IsArmourPos(pDest->m_cEquipPos);
		const int tier = Valoria::ItemTier(pDest->m_sLevelLimit);
		auto D = [](int lo, int hi) { return lo + (int)dice(1, hi - lo + 1) - 1; };
		if (v == INFUSE_REFORGE) {
			if (r < Valoria::RARITY_UNCOMMON) { Say(this, iClientH, "Common items have no bonuses to reforge."); return false; }
			d.RollBonuses(r, bArmour, tier, D);
		}
		else if (v == INFUSE_ASCEND) {
			if (r >= Valoria::RARITY_EPIC) { Say(this, iClientH, r == Valoria::RARITY_EPIC ? "Epic or better: use a Radiant Orb." : "That item is already Legendary."); return false; }
			d.rarity = (unsigned char)(r + 1);
			d.RollBonuses(1, bArmour, tier, D, true);
		}
		else {
			if (r != Valoria::RARITY_EPIC) { Say(this, iClientH, "The Radiant Orb only works on Epic gear."); return false; }
			d.rarity = Valoria::RARITY_LEGENDARY;
			d.RollBonuses(1, bArmour, tier, D, true);
		}
		wsprintf(g_cTxt, "%s: your %s is now %s.", pUse->m_cName, pDest->m_cName, Valoria::RarityName(d.rarity));
		Say(this, iClientH, g_cTxt);
		EnchantSendItems(iClientH);
	}
	else if (v == INFUSE_BLESSED) {
		const int lvl = (pDest->m_dwAttribute & 0xF0000000) >> 28;
		if (!BlessedCategory(pDest->m_cCategory)) { Say(this, iClientH, "The Blessed Upgrade Stone only works on weapons, bows, shields, armour, robes and capes."); return false; }
		if (pDest->m_sTouchEffectType == ITET_UNIQUE_OWNER) { Say(this, iClientH, "Owner-bound items are upgraded the Helbreath way, not with a Blessed Upgrade Stone."); return false; }
		if (lvl >= MAX_UPGRADE) { Say(this, iClientH, "That item is already +15."); return false; }
		pDest->m_dwAttribute = (pDest->m_dwAttribute & 0x0FFFFFFF) | ((uint32)(lvl + 1) << 28);
		LifeAddXP(iClientH, Valoria::LIFE_UPGRADE, Valoria::ItemTier(pDest->m_sLevelLimit));	// Smithing
		SendNotifyMsg(NULL, iClientH, NOTIFY_ITEMATTRIBUTECHANGE, sDest, pDest->m_dwAttribute, NULL, NULL);
		wsprintf(g_cTxt, "Blessed Upgrade Stone: your item is now +%d.", lvl + 1);
		Say(this, iClientH, g_cTxt);
		wsprintf(g_cTxt, "(Valoria) %s blessed %s to +%d.", p->m_cCharName, pDest->m_cName, lvl + 1);
		PutLogList(g_cTxt);
	}
	else {
		const int k = v - 1;
		if (k < 0 || k >= Valoria::MOD_COUNT || !Valoria::ModActive(k)) return false;
		if (!Valoria::IsGearPos(pDest->m_cEquipPos) || pDest->m_cItemType != ITEMTYPE_EQUIP) { Say(this, iClientH, "Runes only go on gear."); return false; }
		Valoria::ItemData & d = pDest->m_valoria;
		int slot = -1;
		for (int i = 0; i < Valoria::MAX_RUNES; i++) if (d.rune[i] >= Valoria::MOD_COUNT) { slot = i; break; }
		if (slot < 0) { Say(this, iClientH, "That item has no rune slots left (2 at most)."); return false; }
		d.rune[slot] = (unsigned char)k;
		const int amt = Valoria::RuneAmount(k, Valoria::ItemTier(pDest->m_sLevelLimit));
		wsprintf(g_cTxt, "The rune sinks in: +%d %s (runes %d/%d).", amt, Valoria::ModName(k), d.Runes(), Valoria::MAX_RUNES);
		Say(this, iClientH, g_cTxt);
		EnchantSendItems(iClientH);
	}
	if (p->m_bIsItemEquipped[sDest]) CalcTotalItemEffect(iClientH, -1, false);
	return true;
}

// GM (admin level 4): "/vuse ElixirOfStrength" or "/vuse RuneOfMight LongSword" uses a bag item by name,
// on the named bag item, the same way a double-click does.
void CGame::ValoriaAdminUse(int iClientH, const char * pArgs)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || p->m_iAdminUserLevel < 4) return;
	char cUse[32] = {}, cDest[32] = {};
	sscanf(pArgs, "%31s %31s", cUse, cDest);
	short sUse = -1, sDest = -1;
	for (int i = 0; i < MAXITEMS; i++) {
		CItem * it = p->m_pItemList[i];
		if (it == NULL) continue;
		if (sUse < 0 && _stricmp(it->m_cName, cUse) == 0) sUse = (short)i;
		else if (sDest < 0 && cDest[0] && _stricmp(it->m_cName, cDest) == 0 && !p->m_bIsItemEquipped[i]) sDest = (short)i;
	}
	if (sUse < 0) { Say(this, iClientH, "/vuse: no such item in your bag."); return; }
	CItem * it = p->m_pItemList[sUse];
	bool bUsed = false;
	if (it->m_sItemEffectType == ITEMEFFECTTYPE_VALORIA_BUFF) bUsed = ValoriaUseBuff(iClientH, it->m_sItemEffectValue1, it->m_sLevelLimit);
	else if (it->m_sItemEffectType == ITEMEFFECTTYPE_VALORIA_INFUSE) {
		if (sDest < 0) { Say(this, iClientH, "/vuse: name the bag item to use it on."); return; }
		bUsed = ValoriaInfuse(iClientH, sUse, sDest);
	}
	else { Say(this, iClientH, "/vuse: not a VALORIA usable item."); return; }
	if (bUsed) ItemDepleteHandler(iClientH, sUse, true);
}

// GM: /vcraft Item [Item ...] - crafts from the bag items named, as if they were dropped into the
// Crafting Vessel's window (Valoria_Craft). Names as in the item list, e.g. /vcraft VerdantEssence.
void CGame::ValoriaAdminCraft(int iClientH, const char * pArgs)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || p->m_iAdminUserLevel < 4) return;
	char cNames[6][32] = {};
	const int iNames = sscanf(pArgs, "%31s %31s %31s %31s %31s %31s", cNames[0], cNames[1], cNames[2], cNames[3], cNames[4], cNames[5]);
	short sIdx[6];
	int n = 0;
	for (int k = 0; k < iNames && k < 6; k++) {
		short s = -1;
		for (int i = 0; i < MAXITEMS && s < 0; i++) {
			CItem * it = p->m_pItemList[i];
			if (it == NULL || it->m_disabled || p->m_bIsItemEquipped[i] || _stricmp(it->m_cName, cNames[k]) != 0) continue;
			bool bDup = false;
			for (int j = 0; j < n; j++) if (sIdx[j] == i) bDup = true;
			if (!bDup) s = (short)i;
		}
		if (s < 0) { Say(this, iClientH, "/vcraft: an item named is not in your bag."); return; }
		sIdx[n++] = s;
	}
	if (n == 0) { Say(this, iClientH, "/vcraft: name the bag items to craft with."); return; }
	Valoria_Craft(iClientH, sIdx, n);
}

// GM: /vrarity Item [0-4] - gives the bag item a rarity (default: a fresh drop roll) and new bonus stats.
void CGame::ValoriaAdminRarity(int iClientH, const char * pArgs)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || p->m_iAdminUserLevel < 4) return;
	char cName[32] = {};
	int r = -1;
	sscanf(pArgs, "%31s %d", cName, &r);
	for (int i = 0; i < MAXITEMS; i++) {
		CItem * it = p->m_pItemList[i];
		if (it == NULL || _stricmp(it->m_cName, cName) != 0) continue;
		if (it->m_cItemType != ITEMTYPE_EQUIP || !Valoria::IsGearPos(it->m_cEquipPos)) { Say(this, iClientH, "/vrarity: not gear."); return; }
		if (r < 0 || r >= Valoria::RARITY_COUNT) r = Valoria::RollRarity(dice(1, 1000000) / 1000001.0, false, p->m_val.itemFind);
		it->m_valoria.rarity = (unsigned char)r;
		it->m_valoria.RollBonuses(r, Valoria::IsArmourPos(it->m_cEquipPos), Valoria::ItemTier(it->m_sLevelLimit),
			[](int lo, int hi) { return lo + (int)dice(1, hi - lo + 1) - 1; });
		wsprintf(g_cTxt, "/vrarity: %s is now %s.", it->m_cName, Valoria::RarityName(r));
		Say(this, iClientH, g_cTxt);
		EnchantSendItems(iClientH);
		if (p->m_bIsItemEquipped[i]) CalcTotalItemEffect(iClientH, -1, false);
		return;
	}
	Say(this, iClientH, "/vrarity: no such item in your bag.");
}
