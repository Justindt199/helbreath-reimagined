// Achievements.cpp: VALORIA's achievements on the game server. The list is
// Shared\ValoriaAchieveData.h (tools\valoria_port\build_achievements.py); the client draws it in
// UI\AchievePanel.cpp.
//
//   - every achievement is a counter reaching a number: kills of one monster (counted by its Npc.cfg
//     name), all monster kills, enemy players defeated (the same rule as an enemy kill), City Hall
//     quests handed in, side quests, and level. The counters are saved with the character in the
//     VALORIA block (Shared\ValoriaEnchant.h CharData); nothing else is, so an achievement is unlocked
//     whenever its counter is at its number.
//   - the counters travel to the client with Enchanting (NOTIFY_ENCHANTSTATS) a few seconds after
//     they change; the client announces what they unlocked.
//   - each achievement grants a title. The one a player picks (MSGID_REQUEST_TITLE) is shown in front
//     of their name to everyone (NOTIFY_TITLEINFO, sent like the rebirth levels).
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "char\Npc.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"
#include "..\shared\ValoriaAchieveData.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace Valoria;

extern char g_cTxt[512];

namespace {

void Say(CGame * g, int h, const char * s)
{
	char b[256];
	strncpy(b, s, sizeof(b) - 1);
	b[sizeof(b) - 1] = 0;
	g->ShowClientMsg(h, b);
}

int SlotOfNpc(const char * pNpcName)
{
	for (int i = 0; i < Ach::TARGET_COUNT; i++)
		if (strncmp(pNpcName, Ach::kTargets[i].npc, 20) == 0) return i;
	return -1;
}

unsigned int Progress(const CClient * p, int counter)
{
	if (counter == ACH_LEVEL) return p->m_iLevel > 0 ? (unsigned int)p->m_iLevel : 0;
	return (counter >= 0 && counter < ACH_SAVED) ? p->m_enchant.ach[counter] : 0;
}

} // namespace

void CGame::AchAdd(int iClientH, int iCounter, unsigned int n)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || iCounter < 0 || iCounter >= ACH_SAVED) return;
	unsigned int & v = p->m_enchant.ach[iCounter];
	v = (v > 0xFFFFFFFFu - n) ? 0xFFFFFFFFu : v + n;
	p->m_bAchDirty = true;
}

void CGame::AchKill(int iClientH, const char * pNpcName)
{
	if (m_pClientList[iClientH] == NULL || pNpcName == NULL) return;
	const int s = SlotOfNpc(pNpcName);
	if (s >= 0) AchAdd(iClientH, s);
	AchAdd(iClientH, ACH_TOTAL);
	SideOnKill(iClientH, pNpcName);	// side quests (SideQuests.cpp)
}

void CGame::AchSendTitle(int iToH, int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, p->m_cCharName, 10);
	Push(cp, (uint8)p->m_enchant.titleCounter);
	Push(cp, (uint8)p->m_enchant.titleTier);
	WantedSendRaw(iToH, NOTIFY_TITLEINFO, cData, cp);
}

// Joining a map: the titles of everyone online, and yours to them.
void CGame::AchOnMapEntry(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	SideOnMap(h);		// side quests: maps to visit (SideQuests.cpp)
	WorldOnMapEntry(h);	// VALORIA shrines and chests that are used up (WorldProps.cpp)
	for (int i = 1; i < MAXCLIENTS; i++) {
		CClient * c = m_pClientList[i];
		if (c == NULL || i == h || !c->m_bIsInitComplete || c->m_enchant.titleTier == 0) continue;
		AchSendTitle(h, i);
	}
	if (p->m_enchant.titleTier != 0)
		for (int i = 1; i < MAXCLIENTS; i++)
			if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) AchSendTitle(i, h);
}

// From the client: MSGID_REQUEST_TITLE, uint16 counter, uint16 tier (tier 0 = no title).
void CGame::AchTitleRequest(int iClientH, char * pData)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->m_bIsInitComplete) return;
	uint16 wCounter, wTier;
	Pop(pData, wCounter);
	Pop(pData, wTier);
	if (wTier != 0) {
		const Ach::Def * d = NULL;
		for (int i = 0; i < Ach::DEF_COUNT && d == NULL; i++)
			if (Ach::kDefs[i].counter == wCounter && Ach::kDefs[i].tier == wTier) d = &Ach::kDefs[i];
		if (d == NULL || Progress(p, d->counter) < d->count) return;	// not unlocked: ignore
	}
	else wCounter = 0;
	p->m_enchant.titleCounter = (unsigned char)wCounter;
	p->m_enchant.titleTier = (unsigned char)wTier;
	EnchantSendStats(iClientH);
	for (int i = 1; i < MAXCLIENTS; i++)
		if (m_pClientList[i] != NULL && m_pClientList[i]->m_bIsInitComplete) AchSendTitle(i, iClientH);
}

// GM: /vkill kills the monsters within 8 tiles as your own kills (testing kill counters and quests).
void CGame::AchAdminKill(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || p->m_iAdminUserLevel < 4) return;
	int n = 0;
	for (int i = 1; i < MAXNPCS; i++) {
		CNpc * m = m_pNpcList[i];
		if (m == NULL || m->m_bIsKilled || m->m_cMapIndex != p->m_cMapIndex) continue;
		if (abs(m->m_sX - p->m_sX) > 8 || abs(m->m_sY - p->m_sY) > 8 || SlotOfNpc(m->m_cNpcName) < 0) continue;
		m->ReduceHP(m->m_iHP);
		NpcKilledHandler((short)iClientH, OWNERTYPE_PLAYER, i, 1);
		n++;
	}
	wsprintf(g_cTxt, "/vkill: %d monsters.", n);
	Say(this, iClientH, g_cTxt);
}

// GM: /vach <counter> [amount] adds to one of your achievement counters (testing).
void CGame::AchAdmin(int iClientH, const char * pArgs)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || p->m_iAdminUserLevel < 4) return;
	int c = -1, n = 1;
	sscanf(pArgs, "%d %d", &c, &n);
	if (c < 0 || c >= ACH_SAVED || n <= 0) { Say(this, iClientH, "/vach <counter 0-67> [amount]"); return; }
	AchAdd(iClientH, c, (unsigned int)n);
	wsprintf(g_cTxt, "/vach: counter %d is now %u.", c, p->m_enchant.ach[c]);
	Say(this, iClientH, g_cTxt);
}
