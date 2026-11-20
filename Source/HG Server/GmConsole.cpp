// GmConsole.cpp: the server side of the in-game GM console (Client3 UI\GmConsole.cpp).
//
//   - on entering the game every player is told their GM level (NOTIFY_GMLEVEL, uint8 admin level);
//     the client only opens the console for a level above 0.
//   - "/gm <command>" from chat runs the console's own commands. Every one checks the player is a
//     GM here, on the server: the client hiding the console is only a convenience.
//
//     /gm heal                 full HP, MP and SP
//     /gm levelup [n]          n more levels (default 1, up to the level cap)
//     /gm stat <str|dex|vit|int|mag|chr> <+n|-n>
//     /gm learn                every spell
//     /gm killall              every monster on your map, as your kills
//     /gm town                 back to your town (the middle of the map for travellers)
//
//   The console also sends Helbreath's own GM commands (/tp, /ci, /summon, /invincible ...).
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "char\Npc.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

extern char g_cTxt[512];

namespace {

void Reply(CGame * g, int h, const char * s)
{
	char b[256];
	strncpy(b, s, sizeof(b) - 1);
	b[sizeof(b) - 1] = 0;
	g->ShowClientMsg(h, b);
}

// the next word of s (lower case) into w; returns what follows it
const char * Word(const char * s, char * w, int n)
{
	while (*s == ' ' || *s == '\t') s++;
	int i = 0;
	while (*s && *s != ' ' && *s != '\t') { if (i < n - 1) w[i++] = (char)tolower((unsigned char)*s); s++; }
	w[i] = 0;
	return s;
}

} // namespace

void CGame::GmSendLevel(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	char cData[16] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint8)(p->m_iAdminUserLevel > 0 ? p->m_iAdminUserLevel : 0));
	WantedSendRaw(iClientH, NOTIFY_GMLEVEL, cData, cp);
}

void CGame::GmCommand(int iClientH, const char * pArgs)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->IsGM()) return;
	char cmd[32], a1[32], a2[32];
	const char * rest = Word(pArgs, cmd, sizeof(cmd));
	rest = Word(rest, a1, sizeof(a1));
	Word(rest, a2, sizeof(a2));

	if (strcmp(cmd, "heal") == 0) {
		p->m_iHP = p->GetMaxHP();
		p->m_iMP = p->GetMaxMP();
		p->m_iSP = p->GetMaxSP();
		SendNotifyMsg(NULL, iClientH, NOTIFY_HP, NULL, NULL, NULL, NULL);
		SendNotifyMsg(NULL, iClientH, NOTIFY_MP, NULL, NULL, NULL, NULL);
		SendNotifyMsg(NULL, iClientH, NOTIFY_SP, NULL, NULL, NULL, NULL);
		Reply(this, iClientH, "GM: fully healed.");
	}
	else if (strcmp(cmd, "levelup") == 0) {
		int n = a1[0] ? atoi(a1) : 1;
		if (n < 1) n = 1;
		int target = p->m_iLevel + n;
		if (target > PLAYERMAXLEVEL) target = PLAYERMAXLEVEL;
		if (target <= p->m_iLevel) { Reply(this, iClientH, "GM: already at the level cap."); return; }
		if (p->m_iExp < m_iLevelExpTable[target]) p->m_iExp = m_iLevelExpTable[target];
		SendNotifyMsg(NULL, iClientH, NOTIFY_EXP, NULL, NULL, NULL, NULL);
		bCheckLevelUp(iClientH);
		wsprintf(g_cTxt, "GM: now level %d.", p->m_iLevel);
		Reply(this, iClientH, g_cTxt);
	}
	else if (strcmp(cmd, "stat") == 0) {
		const int d = atoi(a2);
		if (d == 0) { Reply(this, iClientH, "GM: /gm stat <str|dex|vit|int|mag|chr> <+n|-n>"); return; }
		auto Clamp = [](int v) { return v < 10 ? 10 : (v > CHARPOINTLIMIT ? CHARPOINTLIMIT : v); };
		if (strcmp(a1, "str") == 0) p->SetStr(Clamp(p->GetBaseStr() + d), false);
		else if (strcmp(a1, "dex") == 0) p->SetDex(Clamp(p->GetBaseDex() + d));
		else if (strcmp(a1, "vit") == 0) p->m_iVit = Clamp(p->m_iVit + d);
		else if (strcmp(a1, "int") == 0) p->SetInt(Clamp(p->GetBaseInt() + d), false);
		else if (strcmp(a1, "mag") == 0) p->SetMag(Clamp(p->GetBaseMag() + d));
		else if (strcmp(a1, "chr") == 0) p->m_iCharisma = Clamp(p->m_iCharisma + d);
		else { Reply(this, iClientH, "GM: stats are str, dex, vit, int, mag, chr."); return; }
		CalcTotalItemEffect(iClientH, -1, false);
		SendNotifyMsg(NULL, iClientH, NOTIFY_LEVELUP, NULL, NULL, NULL, NULL);	// sends the stats
		SendNotifyMsg(NULL, iClientH, NOTIFY_HP, NULL, NULL, NULL, NULL);
		SendNotifyMsg(NULL, iClientH, NOTIFY_MP, NULL, NULL, NULL, NULL);
		SendNotifyMsg(NULL, iClientH, NOTIFY_SP, NULL, NULL, NULL, NULL);
		wsprintf(g_cTxt, "GM: STR %d  DEX %d  VIT %d  INT %d  MAG %d  CHR %d", p->GetBaseStr(), p->GetBaseDex(), p->m_iVit,
			p->GetBaseInt(), p->GetBaseMag(), p->m_iCharisma);
		Reply(this, iClientH, g_cTxt);
	}
	else if (strcmp(cmd, "learn") == 0) {
		int n = 0;
		for (int i = 0; i < MAXMAGICTYPE; i++) {
			if (m_pMagicConfigList[i] == NULL || p->m_cMagicMastery[i] != 0) continue;
			p->m_cMagicMastery[i] = 1;
			char cData[48] = {};
			char * cp = cData + INDEX2_MSGTYPE + 2;
			*cp++ = (char)i;
			memcpy(cp, m_pMagicConfigList[i]->m_cName, 30);
			cp += 30;
			WantedSendRaw(iClientH, NOTIFY_MAGICSTUDYSUCCESS, cData, cp);
			n++;
		}
		wsprintf(g_cTxt, "GM: learned %d spells.", n);
		Reply(this, iClientH, g_cTxt);
	}
	else if (strcmp(cmd, "killall") == 0) {
		int n = 0;
		for (int i = 1; i < MAXNPCS; i++) {
			CNpc * m = m_pNpcList[i];
			if (m == NULL || m->m_bIsKilled || m->m_cMapIndex != p->m_cMapIndex || m->m_bIsSummoned) continue;
			if (m->IsAres() || m->IsElv() || m->m_iHitDice <= 0) continue;	// town guards and shopkeepers stay
			m->ReduceHP(m->m_iHP);
			NpcKilledHandler((short)iClientH, OWNERTYPE_PLAYER, i, 1);
			n++;
		}
		wsprintf(g_cTxt, "GM: killed %d monsters on this map.", n);
		Reply(this, iClientH, g_cTxt);
	}
	else if (strcmp(cmd, "town") == 0) {
		char cBuf[64];
		const char * town = p->IsElv() ? sideMap[ELVINE] : (p->IsAres() ? sideMap[ARESDEN] : "middleland");
		wsprintf(cBuf, "/tp %s", town);
		AdminOrder_Teleport(iClientH, cBuf, (uint32)strlen(cBuf));
	}
	else Reply(this, iClientH, "GM: /gm heal, levelup [n], stat <str|dex|vit|int|mag|chr> <+n>, learn, killall, town");
}
