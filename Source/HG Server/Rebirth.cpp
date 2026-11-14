// Rebirth.cpp: rebirth, after the Helbreath Olympia rebirth system, at this server's max level.
//
//   - at level 180 (PLAYERMAXLEVEL), inside a building (the City Hall), alive, a player can be
//     reborn from the City Hall Officer's menu. Rebirth level R (the new one) costs
//     min(200,000 x R, 1,000,000) gold and 5 x R Majestic points. Up to 20 rebirths.
//   - the character goes back to level 1 with every stat at 10. Items (unequipped), skills and
//     spells are kept. Each rebirth level gives 3 extra stat points, so a character at rebirth R
//     has 3 x R more points to place than a normal character of the same level.
//   - while levelling a rebirth back up, XP is multiplied by 0.8^R (R = the current rebirth
//     level); at level 180 the rebirth is finished and XP is normal again.
//   - Normal Mode: for 50,000 gold a player can pause an unfinished rebirth and play their
//     full-level character again, and switch back later the same way. Each switch swaps the
//     level, XP and stats of the two (the other set is kept in m_iAlt* / m_altStat).
//   - carry weight: after paying, the items must fit what the new set can carry (a reborn
//     character carries (10 STR + level 1) x 500 = 55 weight).
//
// A rebirth or a switch changes level and stats, so the player is saved and logged out to load
// them; they log back in where they were.
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "map\Map.h"
#include "ui\Winmain.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"

#include <cmath>

extern class CMap ** g_mapList;
extern char g_cTxt[512];

namespace {

const int    kMaxRebirth = 20;
const uint32 kSwitchGold = 50000;
const int    kStartStat = 10;

uint32 RebirthGold(int R)		{ uint32 g = 200000u * (uint32)R; return g > 1000000u ? 1000000u : g; }
int    RebirthMajestic(int R)	{ return 5 * R; }

} // namespace

void CGame::RebirthSendStats(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint8)p->m_iRebirth);
	Push(cp, (uint8)p->m_iRebirthMode);
	Push(cp, (uint16)p->m_iAltLevel);
	WantedSendRaw(h, NOTIFY_REBIRTHSTATS, cData, cp);
}

void CGame::RebirthSendInfo(int iToH, int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, p->m_cCharName, 10);
	Push(cp, (uint8)p->m_iRebirth);
	WantedSendRaw(iToH, NOTIFY_REBIRTHINFO, cData, cp);
}

// Request results (to the player) and announcements (iType REBORN / COMPLETED to everyone).
void CGame::RebirthResult(int h, int iCode)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint8)iCode);
	Push(cp, (uint8)p->m_iRebirth);
	Push(cp, p->m_cCharName, 10);
	WantedSendRaw(h, NOTIFY_REBIRTHEVENT, cData, cp);
}

void CGame::RebirthAnnounce(int iType, int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint8)iType);
	Push(cp, (uint8)p->m_iRebirth);
	Push(cp, p->m_cCharName, 10);
	for (int i = 1; i < MAXCLIENTS; i++)
		if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) WantedSendRaw(i, NOTIFY_REBIRTHEVENT, cData, cp);
}

void CGame::RebirthOnMapEntry(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	if (p->m_iRebirthMode == 1 && p->m_iLevel >= PLAYERMAXLEVEL) RebirthOnMaxLevel(h);
	RebirthSendStats(h);
	for (int i = 1; i < MAXCLIENTS; i++) {
		CClient * c = m_pClientList[i];
		if (c == NULL || i == h || !c->m_bIsInitComplete || c->m_iRebirth <= 0) continue;
		RebirthSendInfo(h, i);
	}
	if (p->m_iRebirth > 0)
		for (int i = 1; i < MAXCLIENTS; i++)
			if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) RebirthSendInfo(i, h);
}

// CalcExpStock: XP x 0.8^R while levelling a rebirth back up.
void CGame::RebirthScaleExp(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL || p->m_iRebirthMode != 1 || p->m_iRebirth <= 0 || p->m_iLevel >= PLAYERMAXLEVEL) return;
	if (p->m_iExpStock <= 0) return;
	p->m_iExpStock = (int)floor((double)p->m_iExpStock * pow(0.8, (double)p->m_iRebirth));
}

// bCheckLevelUp: a rebirth reached level 180. Finished: this is the character now.
void CGame::RebirthOnMaxLevel(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL || p->m_iRebirthMode != 1) return;
	p->m_iRebirthMode = 0;
	p->m_iAltLevel = p->m_iAltExp = 0;
	for (int r = 0; r < 6; r++) p->m_altStat[r] = 0;
	RebirthSendStats(h);
	RebirthAnnounce(REBIRTHRES_COMPLETED, h);
	wsprintf(g_cTxt, "(Rebirth) %s finished rebirth %d.", p->m_cCharName, p->m_iRebirth);
	PutLogList(g_cTxt);
}

// Normal Mode <-> rebirth: swap level, XP and stats with the stored set.
void CGame::RebirthSwapSets(int h)
{
	CClient * p = m_pClientList[h];
	const int cur[6] = { p->GetBaseStr(), p->m_iVit, p->GetBaseDex(), p->GetBaseInt(), p->GetBaseMag(), p->m_iCharisma };
	const int lv = p->m_iLevel, xp = p->m_iExp;
	p->m_iLevel = p->m_iAltLevel;
	p->m_iExp = p->m_iAltExp;
	p->SetStr(p->m_altStat[0], false);
	p->m_iVit = p->m_altStat[1];
	p->SetDex(p->m_altStat[2]);
	p->SetInt(p->m_altStat[3], false);
	p->SetMag(p->m_altStat[4]);
	p->m_iCharisma = p->m_altStat[5];
	p->m_iAltLevel = lv;
	p->m_iAltExp = xp;
	for (int r = 0; r < 6; r++) p->m_altStat[r] = cur[r];
}

// From the client (MSGID_REQUEST_REBIRTH): type 1 = rebirth, type 2 = switch between Normal Mode
// and the paused rebirth.
void CGame::RebirthRequest(int iClientH, char * pData)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->m_bIsInitComplete) return;
	uint16 wType;
	Pop(pData, wType);
	if (wType != 1 && wType != 2) return;

	const uint32 t = timeGetTime();
	if (p->m_dwRebirthConfirm != 0 && t - p->m_dwRebirthConfirm < 3000) { RebirthResult(iClientH, REBIRTHRES_BUSY); return; }
	p->m_dwRebirthConfirm = t;

	if (p->IsDead()) { RebirthResult(iClientH, REBIRTHRES_DEAD); return; }
	if (_strnicmp(p->m_cMapName, "cityhall", 8) != 0) { RebirthResult(iClientH, REBIRTHRES_BUILDING); return; }

	const uint32 goldIdx = p->HasItem(ITEM_GOLD);
	const bool bGoldOk = (goldIdx != ITEM_NONE && p->m_pItemList[goldIdx] != NULL && !p->m_pItemList[goldIdx]->m_disabled);
	const uint32 have = bGoldOk ? p->GetItemCount(ITEM_GOLD) : 0;
	// weight after paying 'cost' gold
	auto WeightAfter = [&](uint32 cost) {
		int w = iCalcTotalWeight(iClientH);
		if (bGoldOk && cost > 0) w -= p->m_pItemList[goldIdx]->GetWeight(have) - p->m_pItemList[goldIdx]->GetWeight(have - cost);
		return w;
	};
	auto Unequip = [&]() {
		for (int i = 0; i < MAXITEMS; i++)
			if (p->m_pItemList[i] != NULL && p->m_bIsItemEquipped[i]) ReleaseItemHandler(iClientH, (short)i, true);
		CalcTotalItemEffect(iClientH, -1, false);
	};
	auto Finish = [&](int iCode) {
		p->m_iNextLevelExp = m_iLevelExpTable[p->m_iLevel + 1];
		const int iStats = p->GetBaseStr() + p->m_iVit + p->GetBaseDex() + p->GetBaseInt() + p->GetBaseMag() + p->m_iCharisma;
		p->m_iLU_Pool = p->m_iLevel * 3 - (iStats - 70) + p->RebirthStatBonus();
		p->m_iHP = p->GetMaxHP();
		p->m_iMP = p->GetMaxMP();
		p->m_iSP = p->GetMaxSP();
		p->m_iExpStock = 0;
		RebirthSendStats(iClientH);
		RebirthResult(iClientH, iCode);
		p->Save();
		SendNotifyMsg(NULL, iClientH, NOTIFY_FORCEDISCONN, 5, NULL, NULL, NULL);	// log in again to load the new level and stats
	};

	if (wType == 1) {	// rebirth
		if (p->m_iRebirthMode == 2) { RebirthResult(iClientH, REBIRTHRES_PAUSEDFIRST); return; }
		if (p->m_iRebirthMode == 1 || p->m_iLevel < PLAYERMAXLEVEL) { RebirthResult(iClientH, REBIRTHRES_LEVEL); return; }
		if (p->m_iRebirth >= kMaxRebirth) { RebirthResult(iClientH, REBIRTHRES_MAX); return; }
		const int R = p->m_iRebirth + 1;
		const uint32 gold = RebirthGold(R);
		if (have < gold) { RebirthResult(iClientH, REBIRTHRES_GOLD); return; }
		if (p->m_iGizonItemUpgradeLeft < RebirthMajestic(R)) { RebirthResult(iClientH, REBIRTHRES_MAJESTIC); return; }
		if (WeightAfter(gold) > (kStartStat + 1) * 500) { RebirthResult(iClientH, REBIRTHRES_WEIGHT); return; }

		p->SetItemCount(ITEM_GOLD, have - gold, true);
		p->m_iGizonItemUpgradeLeft -= RebirthMajestic(R);
		SendNotifyMsg(NULL, iClientH, NOTIFY_GIZONITEMUPGRADELEFT, p->m_iGizonItemUpgradeLeft, NULL, NULL, NULL);
		Unequip();

		// the full-level character waits in the other set (Normal Mode)
		p->m_iAltLevel = p->m_iLevel;
		p->m_iAltExp = p->m_iExp;
		p->m_altStat[0] = p->GetBaseStr(); p->m_altStat[1] = p->m_iVit; p->m_altStat[2] = p->GetBaseDex();
		p->m_altStat[3] = p->GetBaseInt(); p->m_altStat[4] = p->GetBaseMag(); p->m_altStat[5] = p->m_iCharisma;

		p->m_iRebirth = R;
		p->m_iRebirthMode = 1;
		p->m_iLevel = 1;
		p->m_iExp = m_iLevelExpTable[1];
		p->SetStr(kStartStat, false);
		p->m_iVit = kStartStat;
		p->SetDex(kStartStat);
		p->SetInt(kStartStat, false);
		p->SetMag(kStartStat);
		p->m_iCharisma = kStartStat;

		wsprintf(g_cTxt, "(Rebirth) %s was reborn: rebirth %d (%u gold, %d Majestic).", p->m_cCharName, R, gold, RebirthMajestic(R));
		PutLogList(g_cTxt);
		RebirthAnnounce(REBIRTHRES_REBORN, iClientH);
		for (int i = 1; i < MAXCLIENTS; i++)
			if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) RebirthSendInfo(i, iClientH);
		Finish(REBIRTHRES_REBORN);
		return;
	}

	// switch: Normal Mode <-> rebirth
	if (p->m_iRebirthMode == 0) { RebirthResult(iClientH, REBIRTHRES_NOTREBIRTHING); return; }
	if (have < kSwitchGold) { RebirthResult(iClientH, REBIRTHRES_GOLD); return; }
	if (WeightAfter(kSwitchGold) > (p->m_altStat[0] + p->m_iAltLevel) * 500) { RebirthResult(iClientH, REBIRTHRES_WEIGHT); return; }

	p->SetItemCount(ITEM_GOLD, have - kSwitchGold, true);
	Unequip();
	RebirthSwapSets(iClientH);
	const bool bPausing = (p->m_iRebirthMode == 1);
	p->m_iRebirthMode = bPausing ? 2 : 1;
	wsprintf(g_cTxt, "(Rebirth) %s %s rebirth %d (now level %d).", p->m_cCharName, bPausing ? "paused" : "resumed", p->m_iRebirth, p->m_iLevel);
	PutLogList(g_cTxt);
	Finish(bPausing ? REBIRTHRES_PAUSED : REBIRTHRES_RESUMED);
}
