// Bounty.cpp: player bounties.
//
// Anyone can put gold on the head of an enemy-town player or a PKer (criminal) who is online,
// from the Bounty Board (client: /bounty or the City Hall). Others can add to it. Whoever kills
// the target collects the whole bounty.
//
//   - minimum 50,000 gold per placement, plus a 10% board fee (the fee is not refunded)
//   - each contribution lasts 7 days; if nobody claims it by then the gold (not the fee) goes back
//     to the player who put it up, as reward gold collected at the City Hall
//   - targets: enemy-town players, or PKers (travellers can only put bounties on PKers)
//   - claims: the killing blow, by an enemy of the target or (when the target is a PKer) anyone
//     without PK count. Not from the target's guild, party or IP address. A target can be claimed
//     at most once every 6 hours (a bounty placed meanwhile waits until then). No claims in
//     no-penalty maps (arenas).
//   - the gold goes to the hunter's reward gold (City Hall). Anything that doesn't fit under the
//     reward gold cap waits and is paid as soon as there is room.
//
// Bounties live on this server in GameData\Bounties.txt (written on every change), so they
// survive restarts and the target or the backers being offline.
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "map\Map.h"
#include "ui\Winmain.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include <stdio.h>

extern class CMap ** g_mapList;
extern char g_cTxt[512];

namespace {

const uint32 kMinBounty = 50000;
const uint32 kMaxBounty = 50000000;				// per target, all contributions together
const uint32 kFeePercent = 10;
const uint32 kLifeSec = 7 * 24 * 60 * 60;		// each contribution
const uint32 kClaimCooldownSec = 6 * 60 * 60;	// per target
const int    kMaxBackers = 16;					// contributions per target
const int    kMaxPlacedPerPlayer = 10;			// targets one player can back at once
const char * kFile = "GameData\\Bounties.txt";
const char * kTmpFile = "GameData\\Bounties.tmp";

struct Backer { std::string name; uint32 gold, placed, expires; };
struct Bounty {
	std::string target;
	int side, level, pk;
	std::string map;
	std::vector<Backer> backers;
	uint32 Total() const { uint32 t = 0; for (size_t i = 0; i < backers.size(); i++) t += backers[i].gold; return t; }
	uint32 Expires() const { uint32 e = 0; for (size_t i = 0; i < backers.size(); i++) e = (std::max)(e, backers[i].expires); return e; }
};
struct Stats { uint32 claims, gold; };

std::vector<Bounty> g_bounties;
std::map<std::string, uint32> g_pending;		// gold owed to a player (refunds, claims over the cap)
std::map<std::string, Stats> g_stats;			// bounties claimed by each player
std::map<std::string, uint32> g_lastClaim;		// target -> unix time of the last claim on them
bool g_bLoaded = false;
uint32 g_dwLastPlace[MAXCLIENTS] = {};

uint32 Now() { return (uint32)time(NULL); }

bool SameName(const char * a, const char * b) { return strncmp(a, b, 10) == 0; }

std::string Name10(const char * p)
{
	char c[11] = {};
	strncpy(c, p, 10);
	return c;
}

Bounty * Find(const std::string & target)
{
	for (size_t i = 0; i < g_bounties.size(); i++) if (g_bounties[i].target == target) return &g_bounties[i];
	return NULL;
}

std::string Clean(const std::string & s) { return s.empty() ? std::string("-") : s; }

void Save()
{
	FILE * f = fopen(kTmpFile, "wt");
	if (f == NULL) return;
	fprintf(f, "// Bounties (HG Server Bounty.cpp). Written by the server: don't edit while it runs.\n");
	for (size_t i = 0; i < g_bounties.size(); i++) {
		const Bounty & b = g_bounties[i];
		fprintf(f, "B %s %d %d %d %s\n", b.target.c_str(), b.side, b.level, b.pk, Clean(b.map).c_str());
		for (size_t j = 0; j < b.backers.size(); j++)
			fprintf(f, "C %s %u %u %u\n", b.backers[j].name.c_str(), b.backers[j].gold, b.backers[j].placed, b.backers[j].expires);
	}
	for (std::map<std::string, uint32>::const_iterator it = g_pending.begin(); it != g_pending.end(); ++it)
		if (it->second > 0) fprintf(f, "P %s %u\n", it->first.c_str(), it->second);
	for (std::map<std::string, Stats>::const_iterator it = g_stats.begin(); it != g_stats.end(); ++it)
		fprintf(f, "S %s %u %u\n", it->first.c_str(), it->second.claims, it->second.gold);
	const uint32 now = Now();
	for (std::map<std::string, uint32>::const_iterator it = g_lastClaim.begin(); it != g_lastClaim.end(); ++it)
		if (now - it->second < kClaimCooldownSec) fprintf(f, "L %s %u\n", it->first.c_str(), it->second);
	fclose(f);
	MoveFileExA(kTmpFile, kFile, MOVEFILE_REPLACE_EXISTING);
}

void Load()
{
	g_bLoaded = true;
	FILE * f = fopen(kFile, "rt");
	if (f == NULL) return;
	char line[256];
	Bounty * cur = NULL;
	while (fgets(line, sizeof(line), f)) {
		char a[32] = {}, b[32] = {};
		unsigned int u1 = 0, u2 = 0, u3 = 0;
		int i1 = 0, i2 = 0, i3 = 0;
		switch (line[0]) {
		case 'B':
			if (sscanf(line + 2, "%20s %d %d %d %20s", a, &i1, &i2, &i3, b) == 5) {
				Bounty nb;
				nb.target = a; nb.side = i1; nb.level = i2; nb.pk = i3; nb.map = (strcmp(b, "-") == 0) ? "" : b;
				g_bounties.push_back(nb);
				cur = &g_bounties.back();
			}
			break;
		case 'C':
			if (cur != NULL && sscanf(line + 2, "%20s %u %u %u", a, &u1, &u2, &u3) == 4) {
				Backer k; k.name = a; k.gold = u1; k.placed = u2; k.expires = u3;
				cur->backers.push_back(k);
			}
			break;
		case 'P':
			if (sscanf(line + 2, "%20s %u", a, &u1) == 2) g_pending[a] += u1;
			break;
		case 'S':
			if (sscanf(line + 2, "%20s %u %u", a, &u1, &u2) == 3) { g_stats[a].claims = u1; g_stats[a].gold = u2; }
			break;
		case 'L':
			if (sscanf(line + 2, "%20s %u", a, &u1) == 2) g_lastClaim[a] = u1;
			break;
		}
	}
	fclose(f);
	for (size_t i = 0; i < g_bounties.size(); )		// drop empty ones
		if (g_bounties[i].backers.empty()) g_bounties.erase(g_bounties.begin() + i); else i++;
	wsprintf(g_cTxt, "(Bounty) Loaded %d bounties.", (int)g_bounties.size());
	PutLogList(g_cTxt);
}

void EnsureLoaded() { if (!g_bLoaded) Load(); }

} // namespace

//////////////////////////////////////////////////////////////////////

int CGame::BountyFindOnline(const char * pName, bool bExact)
{
	if (pName == NULL || pName[0] == 0) return 0;
	for (int i = 1; i < MAXCLIENTS; i++) {
		CClient * c = m_pClientList[i];
		if (c == NULL || !c->m_bIsInitComplete) continue;
		if (bExact ? SameName(c->m_cCharName, pName) : (_strnicmp(c->m_cCharName, pName, 10) == 0)) return i;
	}
	return 0;
}

// Pay gold into a player's reward gold now if they are online and it fits, the rest later.
void CGame::BountyCredit(const char * pName, uint32 dwGold)
{
	const std::string name = Name10(pName);
	uint32 owed = g_pending[name] + dwGold;
	const int h = BountyFindOnline(name.c_str(), true);
	if (h > 0) {
		CClient * c = m_pClientList[h];
		const uint32 room = (c->m_iRewardGold < MAXREWARDGOLD) ? (uint32)(MAXREWARDGOLD - c->m_iRewardGold) : 0;
		const uint32 pay = (std::min)(room, owed);
		if (pay > 0) {
			c->m_iRewardGold += (int)pay;
			owed -= pay;
			SendNotifyMsg(NULL, h, NOTIFY_REWARDGOLD, NULL, NULL, NULL, NULL);
		}
	}
	if (owed > 0) g_pending[name] = owed; else g_pending.erase(name);
}

void CGame::BountySendEvent(int iToH, int iType, uint32 dwAmount, uint32 dwTotal, int iSide, const char * pName1, const char * pName2)
{
	char cData[64] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	char n1[12] = {}, n2[12] = {};
	if (pName1) strncpy(n1, pName1, 10);
	if (pName2) strncpy(n2, pName2, 10);
	Push(cp, (uint8)iType);
	Push(cp, (uint32)dwAmount);
	Push(cp, (uint32)dwTotal);
	Push(cp, (uint8)iSide);
	Push(cp, n1, 10);
	Push(cp, n2, 10);
	if (iToH > 0) { WantedSendRaw(iToH, NOTIFY_BOUNTYEVENT, cData, cp); return; }
	for (int i = 1; i < MAXCLIENTS; i++)
		if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) WantedSendRaw(i, NOTIFY_BOUNTYEVENT, cData, cp);
}

// NOTIFY_BOUNTYINFO about one target to one player (with what that player put on it).
void CGame::BountySendInfo(int iToH, const char * pTarget)
{
	CClient * to = m_pClientList[iToH];
	if (to == NULL) return;
	const std::string target = Name10(pTarget);
	const Bounty * b = Find(target);
	char cData[80] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	char cName[12] = {}, cMap[12] = {};
	strncpy(cName, target.c_str(), 10);
	uint32 total = 0, secs = 0, mine = 0;
	int side = 0, pk = 0, level = 0, online = 0;
	if (b != NULL) {
		total = b->Total();
		const uint32 now = Now(), e = b->Expires();
		secs = (e > now) ? e - now : 0;
		for (size_t j = 0; j < b->backers.size(); j++)
			if (b->backers[j].name == Name10(to->m_cCharName)) mine += b->backers[j].gold;
		side = b->side; pk = b->pk; level = b->level;
		strncpy(cMap, b->map.c_str(), 10);
		online = BountyFindOnline(target.c_str(), true) > 0 ? 1 : 0;
	}
	Push(cp, cName, 10);
	Push(cp, (uint32)total);
	Push(cp, (uint8)side);
	Push(cp, (uint8)pk);
	Push(cp, (uint16)level);
	Push(cp, (uint8)online);
	Push(cp, cMap, 10);
	Push(cp, (uint32)secs);
	Push(cp, (uint32)mine);
	WantedSendRaw(iToH, NOTIFY_BOUNTYINFO, cData, cp);
}

void CGame::BountyBroadcastInfo(const char * pTarget)
{
	for (int i = 1; i < MAXCLIENTS; i++)
		if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) BountySendInfo(i, pTarget);
}

void CGame::BountySendStats(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	std::map<std::string, Stats>::const_iterator it = g_stats.find(Name10(p->m_cCharName));
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint32)(it != g_stats.end() ? it->second.claims : 0));
	Push(cp, (uint32)(it != g_stats.end() ? it->second.gold : 0));
	WantedSendRaw(h, NOTIFY_BOUNTYSTATS, cData, cp);
}

// A target's town, level, PK status and map, as last seen. Returns true when something changed.
bool CGame::BountyRefreshTarget(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return false;
	Bounty * b = Find(Name10(p->m_cCharName));
	if (b == NULL) return false;
	const int pk = (p->m_iPKCount > 0) ? 1 : 0;
	const std::string map = Name10(p->m_cMapName);
	if (b->side == p->m_side && b->level == p->m_iLevel && b->pk == pk && b->map == map) return false;
	b->side = p->m_side; b->level = p->m_iLevel; b->pk = pk; b->map = map;
	return true;
}

void CGame::BountyOnMapEntry(int h)
{
	EnsureLoaded();
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	for (size_t i = 0; i < g_bounties.size(); i++) BountySendInfo(h, g_bounties[i].target.c_str());
	BountySendStats(h);
	if (BountyRefreshTarget(h)) Save();
	if (Find(Name10(p->m_cCharName)) != NULL) BountyBroadcastInfo(p->m_cCharName);	// map / online for everyone
	if (g_pending.count(Name10(p->m_cCharName))) { BountyCredit(p->m_cCharName, 0); Save(); }
}

void CGame::BountyOnLogout(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL || !p->m_bIsInitComplete || !g_bLoaded) return;
	if (Find(Name10(p->m_cCharName)) == NULL) return;
	char cName[12] = {};
	memcpy(cName, p->m_cCharName, 10);
	p->m_bIsInitComplete = false;	// so BountyFindOnline sees them gone while we tell everyone
	BountyBroadcastInfo(cName);
	p->m_bIsInitComplete = true;
}

// Every second from OnTimer: once a minute, expired contributions are refunded, targets' details
// refreshed, and owed gold paid to players who now have room for it.
void CGame::BountyTick()
{
	static uint32 s_dwLast = 0;
	const uint32 t = timeGetTime();
	if (t - s_dwLast < 60 * 1000) return;
	s_dwLast = t;
	EnsureLoaded();
	const uint32 now = Now();
	bool bSave = false;

	for (size_t i = 0; i < g_bounties.size(); ) {
		Bounty & b = g_bounties[i];
		bool bChanged = false;
		for (size_t j = 0; j < b.backers.size(); ) {
			if (b.backers[j].expires > now) { j++; continue; }
			const Backer k = b.backers[j];
			b.backers.erase(b.backers.begin() + j);
			BountyCredit(k.name.c_str(), k.gold);
			const int h = BountyFindOnline(k.name.c_str(), true);
			if (h > 0) BountySendEvent(h, BOUNTYEV_REFUND, k.gold, b.Total(), b.side, b.target.c_str(), NULL);
			wsprintf(g_cTxt, "(Bounty) %s's %u gold on %s expired and was refunded.", k.name.c_str(), k.gold, b.target.c_str());
			PutLogList(g_cTxt);
			bChanged = bSave = true;
		}
		const std::string target = b.target;
		if (b.backers.empty()) g_bounties.erase(g_bounties.begin() + i); else i++;
		if (bChanged) BountyBroadcastInfo(target.c_str());
	}

	for (int h = 1; h < MAXCLIENTS; h++) {
		CClient * p = m_pClientList[h];
		if (p == NULL || !p->m_bIsInitComplete) continue;
		if (BountyRefreshTarget(h)) { BountyBroadcastInfo(p->m_cCharName); bSave = true; }
		if (g_pending.count(Name10(p->m_cCharName)) && p->m_iRewardGold < MAXREWARDGOLD) { BountyCredit(p->m_cCharName, 0); bSave = true; }
	}
	if (bSave) Save();
}

// From the client (MSGID_REQUEST_BOUNTY): type 1 = put up / add gold (uint32 amount, name[10]),
// type 2 = who backs a bounty (name[10]).
void CGame::BountyRequest(int iClientH, char * pData)
{
	EnsureLoaded();
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->m_bIsInitComplete) return;
	uint16 wType;
	Pop(pData, wType);

	if (wType == 2) {
		char cName[12] = {};
		Pop(pData, cName, 10);
		const Bounty * b = Find(Name10(cName));
		char cData[400] = {};
		char * cp = cData + INDEX2_MSGTYPE + 2;
		Push(cp, cName, 10);
		const int n = (b != NULL) ? (int)(std::min)(b->backers.size(), (size_t)kMaxBackers) : 0;
		Push(cp, (uint8)n);
		const uint32 now = Now();
		for (int j = 0; j < n; j++) {
			char cBacker[12] = {};
			strncpy(cBacker, b->backers[j].name.c_str(), 10);
			Push(cp, cBacker, 10);
			Push(cp, (uint32)b->backers[j].gold);
			Push(cp, (uint32)(b->backers[j].expires > now ? b->backers[j].expires - now : 0));
		}
		WantedSendRaw(iClientH, NOTIFY_BOUNTYDETAIL, cData, cp);
		return;
	}
	if (wType != 1) return;

	uint32 dwAmount;
	char cName[12] = {};
	Pop(pData, dwAmount);
	Pop(pData, cName, 10);

	const uint32 t = timeGetTime();
	if (g_dwLastPlace[iClientH] != 0 && t - g_dwLastPlace[iClientH] < 2000) return;		// double clicks
	g_dwLastPlace[iClientH] = t;

	auto Fail = [&](int iCode) { BountySendEvent(iClientH, BOUNTYEV_ERROR, (uint32)iCode, 0, 0, cName, NULL); };

	if (dwAmount < kMinBounty) { Fail(BOUNTYERR_MIN); return; }
	const int th = BountyFindOnline(cName, false);
	Bounty * b = (th > 0) ? Find(Name10(m_pClientList[th]->m_cCharName)) : Find(Name10(cName));
	CClient * tgt = (th > 0) ? m_pClientList[th] : NULL;
	if (tgt == NULL && b == NULL) { Fail(BOUNTYERR_NOTFOUND); return; }	// new bounties: the target must be online
	const std::string target = tgt ? Name10(tgt->m_cCharName) : b->target;
	memset(cName, 0, sizeof(cName));
	strncpy(cName, target.c_str(), 10);
	if (target == Name10(p->m_cCharName)) { Fail(BOUNTYERR_SELF); return; }

	// who can be targeted: enemies of the backer, or PKers
	const int tSide = tgt ? tgt->m_side : b->side;
	const bool tPK = tgt ? (tgt->m_iPKCount > 0) : (b->pk != 0);
	const bool bEnemy = (p->m_side != NEUTRAL && tSide != NEUTRAL && p->m_side != tSide);
	if (!bEnemy && !tPK) { Fail(BOUNTYERR_NOTALLOWED); return; }

	// limits
	int nPlaced = 0;
	for (size_t i = 0; i < g_bounties.size(); i++)
		for (size_t j = 0; j < g_bounties[i].backers.size(); j++)
			if (g_bounties[i].backers[j].name == Name10(p->m_cCharName)) { nPlaced++; break; }
	const Backer * mine = NULL;
	if (b != NULL) for (size_t j = 0; j < b->backers.size(); j++) if (b->backers[j].name == Name10(p->m_cCharName)) mine = &b->backers[j];
	if (mine == NULL && nPlaced >= kMaxPlacedPerPlayer) { Fail(BOUNTYERR_LIMIT); return; }
	if (mine == NULL && b != NULL && (int)b->backers.size() >= kMaxBackers) { Fail(BOUNTYERR_FULL); return; }
	const uint32 oldTotal = b ? b->Total() : 0;
	if ((unsigned long long)oldTotal + dwAmount > kMaxBounty) { Fail(BOUNTYERR_MAX); return; }

	// the gold, plus the fee
	const unsigned long long cost = (unsigned long long)dwAmount + dwAmount * kFeePercent / 100;
	const uint32 goldIdx = p->HasItem(ITEM_GOLD);
	if (goldIdx == ITEM_NONE || p->m_pItemList[goldIdx] == NULL || p->m_pItemList[goldIdx]->m_disabled) { Fail(BOUNTYERR_GOLD); return; }
	const uint32 have = p->GetItemCount(ITEM_GOLD);
	if (cost > have) { Fail(BOUNTYERR_GOLD); return; }
	p->SetItemCount(ITEM_GOLD, have - (uint32)cost, true);

	// put it up
	const uint32 now = Now();
	if (b == NULL) {
		Bounty nb;
		nb.target = target; nb.side = tSide; nb.pk = tPK ? 1 : 0;
		nb.level = tgt ? tgt->m_iLevel : 0;
		nb.map = tgt ? Name10(tgt->m_cMapName) : "";
		g_bounties.push_back(nb);
		b = &g_bounties.back();
	}
	bool bAdded = false;
	for (size_t j = 0; j < b->backers.size(); j++)
		if (b->backers[j].name == Name10(p->m_cCharName)) {
			b->backers[j].gold += dwAmount;
			b->backers[j].expires = now + kLifeSec;		// adding renews your part for 7 days
			bAdded = true;
		}
	if (!bAdded) { Backer k; k.name = Name10(p->m_cCharName); k.gold = dwAmount; k.placed = now; k.expires = now + kLifeSec; b->backers.push_back(k); }
	const uint32 total = b->Total();
	Save();
	p->Save();		// the gold is gone from the character now, not only at the next save

	BountyBroadcastInfo(target.c_str());
	BountySendEvent(0, oldTotal == 0 ? BOUNTYEV_PLACED : BOUNTYEV_RAISED, dwAmount, total, b->side, p->m_cCharName, target.c_str());
	if (tgt != NULL) BountySendEvent(th, BOUNTYEV_ONYOU, dwAmount, total, b->side, p->m_cCharName, target.c_str());

	wsprintf(g_cTxt, "(Bounty) %s put %u gold (+%u fee) on %s, total %u.", p->m_cCharName, dwAmount, (uint32)(cost - dwAmount), target.c_str(), total);
	PutLogList(g_cTxt);
}

// A player died (CClient::KilledHandler, after the no-penalty map check): the killer may collect.
void CGame::BountyOnDeath(int iVictimH, int iAttackerH, char cAttackerType)
{
	EnsureLoaded();
	CClient * v = m_pClientList[iVictimH];
	if (v == NULL || cAttackerType != OWNERTYPE_PLAYER || iAttackerH <= 0 || iAttackerH >= MAXCLIENTS || iAttackerH == iVictimH) return;
	CClient * a = m_pClientList[iAttackerH];
	if (a == NULL) return;
	const std::string target = Name10(v->m_cCharName);
	Bounty * b = Find(target);
	if (b == NULL || b->backers.empty()) return;

	const bool bEnemy = (!a->IsNeutral() && !v->IsNeutral() && a->m_side != v->m_side);
	const bool bPKer = (v->m_iPKCount > 0);
	if (a->m_iPKCount > 0 || (!bEnemy && !bPKer)) return;
	if (strcmp(a->m_cIPaddress, v->m_cIPaddress) == 0) return;
	if (a->m_guild != NULL && a->m_guild == v->m_guild) return;
	if (a->GetParty() != NULL && a->GetParty() == v->GetParty()) return;
	const uint32 now = Now();
	std::map<std::string, uint32>::iterator it = g_lastClaim.find(target);
	if (it != g_lastClaim.end() && now - it->second < kClaimCooldownSec) return;

	const uint32 total = b->Total();
	const int side = v->m_side;
	g_bounties.erase(g_bounties.begin() + (b - &g_bounties[0]));
	g_lastClaim[target] = now;
	Stats & s = g_stats[Name10(a->m_cCharName)];
	s.claims++;
	s.gold += total;
	BountyCredit(a->m_cCharName, total);
	Save();

	BountyBroadcastInfo(target.c_str());		// total 0: off the board
	BountySendEvent(0, BOUNTYEV_CLAIMED, total, total, side, a->m_cCharName, target.c_str());
	BountySendStats(iAttackerH);

	wsprintf(g_cTxt, "(Bounty) %s claimed %u gold on %s.", a->m_cCharName, total, target.c_str());
	PutLogList(g_cTxt);
}
