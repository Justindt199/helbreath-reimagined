// Passives.cpp: VALORIA's passive skill tree on the game server.
// The rules and the tree are in Shared\ValoriaPassives.h and Shared\ValoriaPassiveTree.h, shared with
// the client (Client3\UI\PassivePanel.cpp).
//
//   - 1 passive point per level after level 1, plus 1 per Tome of the Unbound Path read.
//   - Take a node: it must link to a node already taken, and a point must be free.
//   - Orb of Unmaking: every point comes back (the tree goes back to the root).
//   - The taken nodes add to the character in CalcTotalItemEffect (PassiveBonus), like worn gear.
//
// The tree travels to the client in NOTIFY_PASSIVES and is saved with the character (48 bytes after the
// enchanting data; see _iComposePlayerDataFileContents and the login server).
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "char\item\Item.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"
#include "..\shared\ValoriaPassives.h"

#include <cstring>

extern char g_cTxt[512];

using namespace ValoriaPassive;

namespace {

char kOrb[]		= "OrbOfUnmaking";
char kTome[]	= "TomeOfTheUnboundPath";

enum { PASS_TAKE = 1, PASS_UNMAKE, PASS_TOME };

// ShowClientMsg takes a char *: copy the text first.
void Say(CGame * g, int h, const char * s)
{
	char b[256];
	strncpy(b, s, sizeof(b) - 1);
	b[sizeof(b) - 1] = 0;
	g->ShowClientMsg(h, b);
}

} // namespace

// Your passive tree (48 bytes).
void CGame::PassiveSendState(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	char cData[16 + DATA_BYTES] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	p->m_passive.Write((unsigned char *)cp);
	cp += DATA_BYTES;
	WantedSendRaw(iClientH, NOTIFY_PASSIVES, cData, cp);
}

// Adds the taken nodes to the gear totals (called from CalcTotalItemEffect, after the worn gear).
// The tree gives nothing while more points are spent than the character has.
void CGame::PassiveBonus(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	p->m_passHpPct = p->m_passMpPct = p->m_passDefPct = p->m_passXpPct = 0;
	if (!p->m_passive.Active(p->m_iLevel)) return;
	int t[PM_COUNT];
	p->m_passive.Totals(t);
	p->m_gearStr += t[PM_STR];
	p->m_gearDex += t[PM_DEX];
	p->m_gearVit += t[PM_VIT];
	p->m_gearInt += t[PM_INT];
	p->m_gearMag += t[PM_MAG];
	p->m_gearHP  += t[PM_HP];
	p->m_gearMP  += t[PM_MP];
	p->m_gearDef += t[PM_DEF];
	p->m_gearDmgPct += t[PM_DMG_PCT];
	p->m_passHpPct  = t[PM_HP_PCT];
	p->m_passMpPct  = t[PM_MP_PCT];
	p->m_passDefPct = t[PM_DEF_PCT];
	p->m_passXpPct  = t[PM_XP];
}

//////////////////////////////////////////////////////////////////////
// From the client: MSGID_REQUEST_PASSIVE, uint16 action, uint16 node

void CGame::PassiveRequest(int iClientH, char * pData)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->m_bIsInitComplete || p->IsDead()) return;
	uint16 wAction, wNode;
	Pop(pData, wAction);
	Pop(pData, wNode);

	const uint32 t = timeGetTime();
	if (p->m_dwPassiveBusy != 0 && t - p->m_dwPassiveBusy < 150) return;
	p->m_dwPassiveBusy = t;

	Data & d = p->m_passive;
	switch (wAction) {
	case PASS_TAKE:
		if (!d.CanTake(wNode)) break;					// stale click: the state below puts the client right
		if (d.Spare(p->m_iLevel) <= 0) { Say(this, iClientH, "You have no passive points left."); break; }
		d.Set(wNode);
		wsprintf(g_cTxt, "Passive learned: %s (%d points left)", NODES[wNode].name, d.Spare(p->m_iLevel));
		Say(this, iClientH, g_cTxt);
		break;

	case PASS_UNMAKE: {
		const int n = d.Taken();
		if (n == 0) { Say(this, iClientH, "No passive points are spent."); break; }
		if (dwGetItemCount(iClientH, kOrb) == 0) { Say(this, iClientH, "You need an Orb of Unmaking (dropped by bosses)."); break; }
		SetItemCount(iClientH, kOrb, dwGetItemCount(iClientH, kOrb) - 1);
		d.Reset();
		wsprintf(g_cTxt, "The Orb of Unmaking shatters: %d passive points returned.", n);
		Say(this, iClientH, g_cTxt);
		wsprintf(g_cTxt, "(Passive) %s used an Orb of Unmaking (%d points).", p->m_cCharName, n);
		PutLogList(g_cTxt);
		break; }

	case PASS_TOME:
		if (dwGetItemCount(iClientH, kTome) == 0) { Say(this, iClientH, "You need a Tome of the Unbound Path (dropped by bosses)."); break; }
		if (d.tomes >= MAX_TOMES) { Say(this, iClientH, "No Tome can teach you more."); break; }
		SetItemCount(iClientH, kTome, dwGetItemCount(iClientH, kTome) - 1);
		d.tomes++;
		wsprintf(g_cTxt, "Tome of the Unbound Path: +1 passive point (%d read).", (int)d.tomes);
		Say(this, iClientH, g_cTxt);
		wsprintf(g_cTxt, "(Passive) %s read a Tome of the Unbound Path (%d read).", p->m_cCharName, (int)d.tomes);
		PutLogList(g_cTxt);
		break;

	default:
		return;
	}

	CalcTotalItemEffect(iClientH, -1, false);
	PassiveSendState(iClientH);
}
