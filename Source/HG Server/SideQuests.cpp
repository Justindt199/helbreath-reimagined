// SideQuests.cpp: side quests on the game server, on VALORIA's quest system. The quests and their
// rules are in Shared\ValoriaSideQuests.h; the client shows them in UI\SideQuestPanel.cpp.
//
//   - a quest is taken from, and handed in to, its giver (McGaffin, Perry or Devlin): the player
//     must be standing near one (any town) - MSGID_REQUEST_SIDEQUEST, uint16 action, uint16 quest
//   - progress: kills (Npc.cpp via AchKill), what those kills drop for the quest, and maps entered
//     (AchOnMapEntry). Saved with the character (Shared\ValoriaEnchant.h CharData) and sent to the
//     client with Enchanting, a few seconds after it changes.
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "char\Npc.h"
#include "char\item\Item.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"
#include "..\shared\ValoriaSideQuests.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace Valoria;

extern char g_cTxt[512];

namespace {

const int kGiverRange = 10;		// tiles


bool ObjDone(const SideQ::Obj & o, unsigned short v) { return o.type == SideQ::OBJ_NONE || v >= o.count; }

bool QuestReady(const CharData & d, int a)
{
	const SideQ::Quest & q = SideQ::kQuests[d.sideQuest[a]];
	for (int o = 0; o < SideQ::MAX_OBJ; o++) if (!ObjDone(q.obj[o], d.sideProg[a][o])) return false;
	return true;
}

} // namespace

// To the client: NOTIFY_SIDEEVENT, uint8 code (SideQ::EV_*), uint8 quest, uint8 objective / reason, uint16 value, uint32 XP.
void CGame::SideEvent(int iClientH, int iCode, int iQuest, int iObj, int iValue, int iXP)
{
	char cData[32] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint8)iCode); Push(cp, (uint8)iQuest); Push(cp, (uint8)iObj);
	Push(cp, (uint16)iValue); Push(cp, (uint32)iXP);
	WantedSendRaw(iClientH, NOTIFY_SIDEEVENT, cData, cp);
}

bool CGame::SideNearGiver(int iClientH, int iGiver)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return false;
	for (int i = 1; i < MAXNPCS; i++) {
		CNpc * n = m_pNpcList[i];
		if (n == NULL || n->m_sType != iGiver || n->m_cMapIndex != p->m_cMapIndex) continue;
		if (abs(n->m_sX - p->m_sX) <= kGiverRange && abs(n->m_sY - p->m_sY) <= kGiverRange) return true;
	}
	return false;
}

// One objective of an active quest moved on: tell the player, and whether the quest is ready.
void CGame::SideAdvance(int iClientH, int a, int o, int n)
{
	CClient * p = m_pClientList[iClientH];
	CharData & d = p->m_enchant;
	const SideQ::Quest & q = SideQ::kQuests[d.sideQuest[a]];
	const SideQ::Obj & ob = q.obj[o];
	unsigned short & v = d.sideProg[a][o];
	if (v >= ob.count) return;
	v = (unsigned short)((v + n > ob.count) ? ob.count : v + n);
	p->m_bAchDirty = true;
	SideEvent(iClientH, SideQ::EV_PROGRESS, d.sideQuest[a], o, v);
	if (QuestReady(d, a)) SideEvent(iClientH, SideQ::EV_READY, d.sideQuest[a]);
}

void CGame::SideOnKill(int iClientH, const char * pNpcName)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	CharData & d = p->m_enchant;
	for (int a = 0; a < SIDE_ACTIVE; a++) {
		if (d.sideQuest[a] >= SideQ::QUEST_COUNT) continue;
		const SideQ::Quest & q = SideQ::kQuests[d.sideQuest[a]];
		for (int o = 0; o < SideQ::MAX_OBJ; o++) {
			const SideQ::Obj & ob = q.obj[o];
			if (ob.type == SideQ::OBJ_KILL && SideQ::ListHas(ob.target, pNpcName)) SideAdvance(iClientH, a, o, 1);
			else if (ob.type == SideQ::OBJ_COLLECT && SideQ::ListHas(ob.target, pNpcName) && d.sideProg[a][o] < ob.count
				&& (int)dice(1, 100) <= ob.chance) SideAdvance(iClientH, a, o, 1);
		}
	}
}

void CGame::SideOnMap(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL) return;
	CharData & d = p->m_enchant;
	for (int a = 0; a < SIDE_ACTIVE; a++) {
		if (d.sideQuest[a] >= SideQ::QUEST_COUNT) continue;
		const SideQ::Quest & q = SideQ::kQuests[d.sideQuest[a]];
		for (int o = 0; o < SideQ::MAX_OBJ; o++)
			if (q.obj[o].type == SideQ::OBJ_VISIT && SideQ::ListHas(q.obj[o].target, p->m_cMapName)) SideAdvance(iClientH, a, o, 1);
	}
}

// From the client: MSGID_REQUEST_SIDEQUEST, uint16 action, uint16 quest.
void CGame::SideRequest(int iClientH, char * pData)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->m_bIsInitComplete || p->IsDead()) return;
	uint16 wAction, wQuest;
	Pop(pData, wAction);
	Pop(pData, wQuest);
	if (wQuest >= SideQ::QUEST_COUNT || wQuest >= SIDE_SLOTS) return;
	const SideQ::Quest & q = SideQ::kQuests[wQuest];
	CharData & d = p->m_enchant;
	const int a = d.SideSlotOf(wQuest);

	switch (wAction) {
	case SideQ::SQ_TAKE: {
		if (d.sideState[wQuest] != SIDE_NONE || a >= 0) return;
		const int prev = SideQ::Previous(wQuest);
		if (prev >= 0 && d.sideState[prev] != SIDE_DONE) return;
		if (p->m_iLevel < q.minLevel) { SideEvent(iClientH, SideQ::EV_ERROR, wQuest, SideQ::ERR_LEVEL); return; }
		if (!SideNearGiver(iClientH, q.giver)) { SideEvent(iClientH, SideQ::EV_ERROR, wQuest, SideQ::ERR_FAR); return; }
		const int free = d.SideSlotOf(0xFF);
		if (free < 0) { SideEvent(iClientH, SideQ::EV_ERROR, wQuest, SideQ::ERR_FULL); return; }
		d.sideState[wQuest] = SIDE_TAKEN;
		d.sideQuest[free] = (unsigned char)wQuest;
		memset(d.sideProg[free], 0, sizeof(d.sideProg[free]));
		SideEvent(iClientH, SideQ::EV_TAKEN, wQuest);
		EnchantSendStats(iClientH);
		SideOnMap(iClientH);		// already standing where it sends you
		return; }

	case SideQ::SQ_ABANDON:
		if (a < 0) return;
		d.sideQuest[a] = 0xFF;
		memset(d.sideProg[a], 0, sizeof(d.sideProg[a]));
		d.sideState[wQuest] = SIDE_NONE;
		SideEvent(iClientH, SideQ::EV_ABANDONED, wQuest);
		EnchantSendStats(iClientH);
		return;

	case SideQ::SQ_HANDIN: {
		if (a < 0 || !QuestReady(d, a)) return;
		if (!SideNearGiver(iClientH, q.giver)) { SideEvent(iClientH, SideQ::EV_ERROR, wQuest, SideQ::ERR_FAR); return; }
		// the rewards must fit before anything is handed over
		CItem * gold = new CItem, * item = new CItem;
		const bool bItem = item->InitItemAttr(q.item);
		gold->InitItemAttr("Gold");
		gold->m_dwCount = SideQ::GoldReward(q);
		if (bItem) item->m_dwCount = q.itemCount;
		if (!_bCheckItemReceiveCondition(iClientH, gold) || (bItem && !_bCheckItemReceiveCondition(iClientH, item))) {
			delete gold; delete item;
			SendItemNotifyMsg(iClientH, NOTIFY_CANNOTCARRYMOREITEM, NULL, NULL, false);
			return;
		}
		int iErase = 0;
		_bAddClientItemList(iClientH, gold, &iErase);
		SendItemNotifyMsg(iClientH, NOTIFY_ITEMOBTAINED, gold, NULL, false);
		if (iErase == 1) delete gold;
		if (bItem) {
			iErase = 0;
			_bAddClientItemList(iClientH, item, &iErase);
			SendItemNotifyMsg(iClientH, NOTIFY_ITEMOBTAINED, item, NULL, false);
			if (iErase == 1) delete item;
		}
		else delete item;
		const int lv = (q.minLevel < 299) ? q.minLevel : 298;
		const int xp = (m_iLevelExpTable[lv + 1] - m_iLevelExpTable[lv]) / 3;
		if (xp > 0) p->m_iExpStock += xp;

		d.sideQuest[a] = 0xFF;
		memset(d.sideProg[a], 0, sizeof(d.sideProg[a]));
		d.sideState[wQuest] = SIDE_DONE;
		AchAdd(iClientH, ACH_SIDE);
		SideEvent(iClientH, SideQ::EV_COMPLETE, wQuest, 0, 0, xp);
		EnchantSendStats(iClientH);
		return; }
	}
}

// GM: /vside <quest> fills an active quest's objectives (testing hand-ins).
void CGame::SideAdmin(int iClientH, const char * pArgs)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || p->m_iAdminUserLevel < 4) return;
	int q = -1;
	sscanf(pArgs, "%d", &q);
	const int a = (q >= 0) ? p->m_enchant.SideSlotOf(q) : -1;
	if (a < 0) return;
	for (int o = 0; o < SideQ::MAX_OBJ; o++)
		if (SideQ::kQuests[q].obj[o].type != SideQ::OBJ_NONE) SideAdvance(iClientH, a, o, SideQ::kQuests[q].obj[o].count);
}
