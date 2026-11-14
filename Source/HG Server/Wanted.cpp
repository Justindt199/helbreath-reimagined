// Wanted.cpp: Most Wanted, enemy-kill streaks.
//
// Killing enemy-town players without dying builds a streak. The streak gives wanted levels:
//   5 kills = 1 star "Wanted", 10 = 2 "Hunted", 20 = 3 "Notorious", 35 = 4 "Most Wanted",
//   50 = 5 "Legend".
// The player on the streak gets nothing for it but fame (stars on their name, announcements,
// the Most Wanted board). Whoever brings a wanted player down gets gold: the killing blow gets
// 60%, and the other enemies who hit them in the last 30 seconds share 40% (100% if nobody
// helped). The gold goes to the hunter's reward gold, collected at the City Hall like other
// kill rewards. Any death ends the streak (except in no-penalty maps such as arenas).
//
// Streak kills that don't count: victims too low for an EK (the usual EK level rule), victims
// more than 30 levels below the killer, the same victim again within 15 minutes, and kills
// between players on the same IP address. A hunter can claim the reward on the same wanted
// player once every 3 hours, and never from the same IP address as the wanted player.
//
// The streak is saved with the character (char_database WantedStreak, WantedLastKill,
// BestStreak, StreaksEnded). Every 12 hours without a streak kill it drops one wanted level.
// At 4 stars the enemy on the same map sees the player on the minimap every 60 seconds, at 5
// stars every 5 seconds.
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "map\Map.h"
#include "ui\Winmain.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"

#include <ctime>
#include <map>
#include <string>

extern class CMap ** g_mapList;
extern char g_cTxt[512];

namespace {

const int kLevelKills[6] = { 0, 5, 10, 20, 35, 50 };						// kills in a row for each level
const uint32 kLevelGold[6] = { 0, 50000, 150000, 400000, 1000000, 2500000 };	// reward for ending it

const uint32 kAssistWindowMs = 30 * 1000;		// hits that count toward a reward share
const uint32 kRepeatVictimSec = 15 * 60;		// same victim again doesn't count within this
const uint32 kClaimCooldownSec = 3 * 60 * 60;	// same hunter, same wanted player
const uint32 kDecaySec = 12 * 60 * 60;			// one level lost per 12 hours without a streak kill
const int kMaxLevelGap = 30;					// victims this far below the killer don't count

std::map<std::string, uint32> g_wantedClaims;	// "hunter|wanted" -> unix time of the last claim

uint32 Now() { return (uint32)time(NULL); }

bool SameName(const char * a, const char * b) { return strncmp(a, b, 10) == 0; }

} // namespace

int CGame::WantedLevelFor(int iStreak)
{
	int lv = 0;
	for (int i = 1; i <= 5; i++) if (iStreak >= kLevelKills[i]) lv = i;
	return lv;
}

uint32 CGame::WantedWorthFor(int iLevel)
{
	return (iLevel >= 0 && iLevel <= 5) ? kLevelGold[iLevel] : 0;
}

void CGame::WantedSendRaw(int iToH, uint16 wType, char * cData, char * cp)
{
	CClient * p = m_pClientList[iToH];
	if (p == NULL || p->m_bIsInitComplete == false || p->m_pXSock == NULL) return;
	*(uint32 *)(cData + INDEX4_MSGID) = MSGID_NOTIFY;
	*(uint16 *)(cData + INDEX2_MSGTYPE) = wType;
	p->m_pXSock->iSendMsg(cData, (uint32)(cp - cData));
}

// NOTIFY_WANTEDINFO about player h to client iToH. cFlag: 0 = quiet update, 1 = announce a new
// level, 2 = went offline (remove from the board).
void CGame::WantedSendInfo(int iToH, int h, char cFlag)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	char cData[64] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	const int lv = (cFlag == 2) ? 0 : WantedLevelFor(p->m_iWantedStreak);
	Push(cp, (uint8)lv);
	Push(cp, (uint8)p->m_side);
	Push(cp, (uint16)((p->m_iWantedStreak > 65535) ? 65535 : p->m_iWantedStreak));
	Push(cp, (uint32)WantedWorthFor(lv));
	Push(cp, (uint8)cFlag);
	Push(cp, p->m_cCharName, 10);
	Push(cp, p->m_cMapName, 10);
	WantedSendRaw(iToH, NOTIFY_WANTEDINFO, cData, cp);
}

void CGame::WantedBroadcastInfo(int h, char cFlag)
{
	for (int i = 1; i < MAXCLIENTS; i++)
		if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) WantedSendInfo(i, h, cFlag);
}

void CGame::WantedSendStats(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint16)((p->m_iWantedStreak > 65535) ? 65535 : p->m_iWantedStreak));
	Push(cp, (uint16)((p->m_iWantedBest > 65535) ? 65535 : p->m_iWantedBest));
	Push(cp, (uint32)p->m_iWantedEnded);
	WantedSendRaw(h, NOTIFY_WANTEDSTATS, cData, cp);
}

// On entering a map (logging in or teleporting): your own numbers, everyone who is wanted, and
// your own entry for the others (so the board shows the map you are on).
void CGame::WantedOnMapEntry(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	WantedDecay(h);
	WantedSendStats(h);
	for (int i = 1; i < MAXCLIENTS; i++)
		if (i != h && m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete
			&& WantedLevelFor(m_pClientList[i]->m_iWantedStreak) > 0)
			WantedSendInfo(h, i, 0);
	if (WantedLevelFor(p->m_iWantedStreak) > 0) WantedBroadcastInfo(h, 0);
}

void CGame::WantedOnLogout(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL || p->m_bIsInitComplete == false) return;
	if (WantedLevelFor(p->m_iWantedStreak) > 0) WantedBroadcastInfo(h, 2);
}

// An enemy player hit this player: remembered for 30 seconds, for reward shares.
void CGame::WantedNoteHit(int iVictimH, int iAttackerH)
{
	if (iVictimH <= 0 || iVictimH >= MAXCLIENTS || iAttackerH <= 0 || iAttackerH >= MAXCLIENTS) return;
	CClient * v = m_pClientList[iVictimH];
	CClient * a = m_pClientList[iAttackerH];
	if (v == NULL || a == NULL || v == a) return;
	if (WantedLevelFor(v->m_iWantedStreak) == 0) return;		// only wanted players need this
	const uint32 t = timeGetTime();
	int iSlot = 0;
	for (int i = 0; i < 8; i++) {
		if (SameName(v->m_wantedHits[i].name, a->m_cCharName)) { iSlot = i; break; }
		if (v->m_wantedHits[i].t < v->m_wantedHits[iSlot].t) iSlot = i;
	}
	memset(v->m_wantedHits[iSlot].name, 0, sizeof(v->m_wantedHits[iSlot].name));
	memcpy(v->m_wantedHits[iSlot].name, a->m_cCharName, 10);
	v->m_wantedHits[iSlot].t = t;
}

// 12 hours without a streak kill: one wanted level less (down to the start of the level below).
void CGame::WantedDecay(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL || p->m_iWantedStreak <= 0) return;
	const uint32 now = Now();
	if (p->m_dwWantedLastKill == 0 || p->m_dwWantedLastKill > now) { p->m_dwWantedLastKill = now; return; }
	bool bChanged = false;
	while (p->m_iWantedStreak > 0 && now - p->m_dwWantedLastKill >= kDecaySec) {
		const int lv = WantedLevelFor(p->m_iWantedStreak);
		p->m_iWantedStreak = (lv <= 1) ? 0 : kLevelKills[lv - 1];
		p->m_dwWantedLastKill += kDecaySec;
		bChanged = true;
	}
	if (bChanged && p->m_bIsInitComplete) {
		WantedSendStats(h);
		WantedBroadcastInfo(h, 0);
	}
}

// Called every second from OnTimer: decay once a minute, and minimap reveals at 4 and 5 stars.
void CGame::WantedTick()
{
	static uint32 s_dwLastDecay = 0;
	const uint32 t = timeGetTime();
	const bool bDecay = (t - s_dwLastDecay) >= 60 * 1000;
	if (bDecay) s_dwLastDecay = t;

	for (int h = 1; h < MAXCLIENTS; h++) {
		CClient * p = m_pClientList[h];
		if (p == NULL || p->m_bIsInitComplete == false) continue;
		if (bDecay) WantedDecay(h);
		const int lv = WantedLevelFor(p->m_iWantedStreak);
		if (lv < 4 || p->IsNeutral()) continue;
		const uint32 dwEvery = (lv >= 5) ? 5 * 1000 : 60 * 1000;
		if (p->m_dwWantedLastPos != 0 && t - p->m_dwWantedLastPos < dwEvery) continue;
		p->m_dwWantedLastPos = t;
		char cData[48] = {};
		char * cp = cData + INDEX2_MSGTYPE + 2;
		Push(cp, (uint8)lv);
		Push(cp, (uint16)p->m_sX);
		Push(cp, (uint16)p->m_sY);
		Push(cp, p->m_cCharName, 10);
		for (int i = 1; i < MAXCLIENTS; i++) {
			CClient * e = m_pClientList[i];
			if (e == NULL || i == h || e->m_bIsInitComplete == false) continue;
			if (e->m_cMapIndex != p->m_cMapIndex || e->IsNeutral() || e->m_side == p->m_side) continue;
			WantedSendRaw(i, NOTIFY_WANTEDPOS, cData, cp);
		}
	}
}

// A player died (called from CClient::KilledHandler, after the no-penalty map check).
void CGame::WantedOnDeath(int iVictimH, int iAttackerH, char cAttackerType)
{
	CClient * v = m_pClientList[iVictimH];
	if (v == NULL) return;
	CClient * a = (cAttackerType == OWNERTYPE_PLAYER && iAttackerH > 0 && iAttackerH < MAXCLIENTS && iAttackerH != iVictimH)
		? m_pClientList[iAttackerH] : NULL;
	const bool bEnemyKill = (a != NULL && !a->IsNeutral() && !v->IsNeutral() && a->m_side != v->m_side);
	const uint32 now = Now();

	// 1. the victim was wanted: whoever brought them down gets the gold
	const int iVictimLv = WantedLevelFor(v->m_iWantedStreak);
	if (iVictimLv > 0 && bEnemyKill && strcmp(a->m_cIPaddress, v->m_cIPaddress) != 0) {
		char cKey[32];
		wsprintf(cKey, "%.10s|%.10s", a->m_cCharName, v->m_cCharName);
		std::map<std::string, uint32>::iterator it = g_wantedClaims.find(cKey);
		if (it == g_wantedClaims.end() || now - it->second >= kClaimCooldownSec) {
			g_wantedClaims[cKey] = now;
			const uint32 dwTotal = WantedWorthFor(iVictimLv);

			// helpers: other enemies of the victim who hit them in the last 30 seconds
			int iHelpers[8], nHelpers = 0;
			const uint32 t = timeGetTime();
			for (int k = 0; k < 8; k++) {
				const CClient::WantedMark & m = v->m_wantedHits[k];
				if (m.name[0] == 0 || t - m.t > kAssistWindowMs || SameName(m.name, a->m_cCharName)) continue;
				for (int i = 1; i < MAXCLIENTS; i++) {
					CClient * c = m_pClientList[i];
					if (c != NULL && c->m_bIsInitComplete && SameName(c->m_cCharName, m.name)
						&& c->m_side == a->m_side && strcmp(c->m_cIPaddress, v->m_cIPaddress) != 0) {
						bool bDup = false;
						for (int j = 0; j < nHelpers; j++) if (iHelpers[j] == i) bDup = true;
						if (!bDup && nHelpers < 8) iHelpers[nHelpers++] = i;
						break;
					}
				}
			}
			const uint32 dwKiller = (nHelpers > 0) ? dwTotal * 6 / 10 : dwTotal;
			const uint32 dwShare = (nHelpers > 0) ? (dwTotal - dwKiller) / (uint32)nHelpers : 0;

			auto Pay = [&](CClient * c, uint32 g) {
				long long v2 = (long long)c->m_iRewardGold + g;
				if (v2 > MAXREWARDGOLD) v2 = MAXREWARDGOLD;
				c->m_iRewardGold = (int)v2;
			};
			Pay(a, dwKiller);
			a->m_iWantedEnded++;
			for (int j = 0; j < nHelpers; j++) Pay(m_pClientList[iHelpers[j]], dwShare);

			// tell everyone
			char cData[200] = {};
			char * cp = cData + INDEX2_MSGTYPE + 2;
			Push(cp, (uint8)iVictimLv);
			Push(cp, (uint16)((v->m_iWantedStreak > 65535) ? 65535 : v->m_iWantedStreak));
			Push(cp, (uint32)dwKiller);
			Push(cp, (uint8)nHelpers);
			Push(cp, (uint32)dwShare);
			Push(cp, (uint8)a->m_side);
			Push(cp, a->m_cCharName, 10);
			Push(cp, v->m_cCharName, 10);
			for (int j = 0; j < nHelpers; j++) Push(cp, m_pClientList[iHelpers[j]]->m_cCharName, 10);
			for (int i = 1; i < MAXCLIENTS; i++)
				if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) WantedSendRaw(i, NOTIFY_WANTEDENDED, cData, cp);

			WantedSendStats(iAttackerH);
			SendNotifyMsg(NULL, iAttackerH, NOTIFY_REWARDGOLD, NULL, NULL, NULL, NULL);
			for (int j = 0; j < nHelpers; j++) SendNotifyMsg(NULL, iHelpers[j], NOTIFY_REWARDGOLD, NULL, NULL, NULL, NULL);

			wsprintf(g_cTxt, "(Wanted) %s ended %s's streak of %d: %u gold (+%d helpers x %u)",
				a->m_cCharName, v->m_cCharName, v->m_iWantedStreak, dwKiller, nHelpers, dwShare);
			PutLogList(g_cTxt);
		}
	}

	// 2. the killer's streak goes up, if this kill counts
	if (bEnemyKill && a->m_iPKCount == 0 && strcmp(a->m_cIPaddress, v->m_cIPaddress) != 0) {
		const int iEK_Level = (a->m_iLevel >= 80) ? 80 : 30;			// same rule as EKs
		bool bCounts = (v->m_iLevel >= iEK_Level) && (v->m_iLevel >= a->m_iLevel - kMaxLevelGap);
		int iSlot = 0;
		for (int k = 0; k < 8 && bCounts; k++) {
			if (SameName(a->m_wantedRecent[k].name, v->m_cCharName) && now - a->m_wantedRecent[k].t < kRepeatVictimSec) bCounts = false;
			if (a->m_wantedRecent[k].t < a->m_wantedRecent[iSlot].t) iSlot = k;
		}
		if (bCounts) {
			memset(a->m_wantedRecent[iSlot].name, 0, sizeof(a->m_wantedRecent[iSlot].name));
			memcpy(a->m_wantedRecent[iSlot].name, v->m_cCharName, 10);
			a->m_wantedRecent[iSlot].t = now;

			const int iOldLv = WantedLevelFor(a->m_iWantedStreak);
			a->m_iWantedStreak++;
			a->m_dwWantedLastKill = now;
			if (a->m_iWantedStreak > a->m_iWantedBest) a->m_iWantedBest = a->m_iWantedStreak;
			const int iNewLv = WantedLevelFor(a->m_iWantedStreak);
			WantedSendStats(iAttackerH);
			if (iNewLv > 0) WantedBroadcastInfo(iAttackerH, (iNewLv > iOldLv) ? 1 : 0);
		}
	}

	// 3. any death ends the victim's streak
	if (v->m_iWantedStreak > 0) {
		const bool bWasWanted = (iVictimLv > 0);
		v->m_iWantedStreak = 0;
		memset(v->m_wantedHits, 0, sizeof(v->m_wantedHits));
		WantedSendStats(iVictimH);
		if (bWasWanted) WantedBroadcastInfo(iVictimH, 0);	// level 0 = off the board
	}
}
