// WorldProps.cpp: VALORIA's shrines and treasure chests on the game server. Where they stand is
// Shared\ValoriaWorld.h (tools\valoria_port\build_world.py); the client draws them in UI\WorldProps.cpp.
//
//   - a shrine blesses whoever comes within World::TOUCH_RANGE tiles of it (a timed buff, Shared\
//     ValoriaUse.h BUFF_BLESS_*) and then recharges for World::SHRINE_RECHARGE seconds
//   - a chest opens for whoever comes that near: it spills gold, a potion, a crafting material and
//     maybe a piece of VALORIA gear of its tier on the ground around it, and refills after
//     World::CHEST_REFILL seconds
//   - both are shared: used by one player, they are used for everyone until they come back.
//     NOTIFY_WORLDPROP (uint16 prop, uint16 seconds until ready) tells the players on the map.
//   - checked for each player every 3 seconds (ValoriaBuffTick).
//////////////////////////////////////////////////////////////////////

#include "HG.h"
#include "char\Client.h"
#include "char\item\Item.h"
#include "map\Map.h"
#include "net\MessageIndex.h"
#include "..\shared\NetMessages.h"
#include "..\shared\buffer.h"
#include "..\shared\ValoriaUse.h"
#include "..\shared\ValoriaWorld.h"

#include <cstdlib>
#include <cstring>

extern char g_cTxt[512];

namespace {

uint32 g_readyAt[World::PROP_COUNT];	// 0 = ready, else the tick it comes back

int SecondsLeft(int i, uint32 t)
{
	if (g_readyAt[i] == 0) return 0;
	const int ms = (int)(g_readyAt[i] - t);
	if (ms <= 0) { g_readyAt[i] = 0; return 0; }
	return (ms + 999) / 1000;
}

} // namespace

void CGame::WorldSendProp(int iToH, int iProp)
{
	char cData[16] = {};
	char * cp = cData + INDEX2_MSGTYPE + 2;
	Push(cp, (uint16)iProp);
	Push(cp, (uint16)SecondsLeft(iProp, timeGetTime()));
	WantedSendRaw(iToH, NOTIFY_WORLDPROP, cData, cp);
}

// Joining a map: which of its shrines and chests are used up.
void CGame::WorldOnMapEntry(int h)
{
	CClient * p = m_pClientList[h];
	if (p == NULL) return;
	const uint32 t = timeGetTime();
	for (int i = 0; i < World::PROP_COUNT; i++)
		if (World::SameMap(World::kProps[i].map, p->m_cMapName) && SecondsLeft(i, t) > 0) WorldSendProp(h, i);
}

// One thing out of a chest, on the ground within 2 tiles of it.
void CGame::WorldSpill(int iClientH, int iProp, const char * pItemName, uint32 dwCount, bool bGear)
{
	CClient * p = m_pClientList[iClientH];
	const World::Prop & pr = World::kProps[iProp];
	CItem * pItem = new CItem;
	if (!pItem->InitItemAttr(pItemName)) { delete pItem; return; }
	if (bGear) {
		if (pItem->m_cEquipPos == EQUIPPOS_NONE) { delete pItem; return; }
		ValoriaRollRarity(pItem, 0, (short)iClientH);
	}
	else pItem->m_dwCount = dwCount;
	const short x = (short)(pr.x + (int)dice(1, 5) - 3), y = (short)(pr.y + (int)dice(1, 5) - 3);
	m_pMapList[p->m_cMapIndex]->bSetItem(x, y, pItem);
	SendEventToNearClient_TypeB(MSGID_EVENT_COMMON, COMMONTYPE_ITEMDROP, p->m_cMapIndex, x, y,
		pItem->m_sSprite, pItem->m_sSpriteFrame, pItem->m_cItemColor);
	AddGroundItem(pItem, x, y, p->m_cMapIndex, TILECLEANTIME);
}

void CGame::WorldTick(int iClientH)
{
	CClient * p = m_pClientList[iClientH];
	if (p == NULL || !p->m_bIsInitComplete || p->IsDead()) return;
	const uint32 t = timeGetTime();
	for (int i = 0; i < World::PROP_COUNT; i++) {
		const World::Prop & pr = World::kProps[i];
		if (abs(pr.x - p->m_sX) > World::TOUCH_RANGE || abs(pr.y - p->m_sY) > World::TOUCH_RANGE) continue;
		if (!World::SameMap(pr.map, p->m_cMapName) || SecondsLeft(i, t) > 0) continue;

		if (pr.kind < World::SHRINE_KINDS) {
			const int b = ValoriaUse::BUFF_BLESS_MIGHT + pr.kind;
			const ValoriaUse::BuffDef & d = ValoriaUse::BUFFS[b];
			p->m_valBuffEnd[b] = (t + (uint32)d.seconds * 1000) | 1;
			wsprintf(g_cTxt, "%s washes over you (%d minutes).", d.name, d.seconds / 60);
			ShowClientMsg(iClientH, g_cTxt);
			CalcTotalItemEffect(iClientH, -1, false);
			ValoriaSendBuffs(iClientH);
			g_readyAt[i] = (t + World::SHRINE_RECHARGE * 1000) | 1;
		}
		else {
			const int tier = (pr.tier >= 1 && pr.tier <= 5) ? pr.tier : 1;
			strcpy(g_cTxt, "You open a treasure chest!");
			ShowClientMsg(iClientH, g_cTxt);
			WorldSpill(iClientH, i, "Gold", World::kChestGold[tier - 1] * dice(1, 3), false);
			WorldSpill(iClientH, i, World::kChestPotion[tier - 1], dice(1, 3), false);
			WorldSpill(iClientH, i, World::kChestMaterial[tier - 1], 1, false);
			if ((int)dice(1, 100) <= World::ChestGearPct(tier)) {
				// the wearable VALORIA gear of the chest's tier, or of the best tier below it that has some
				// (the craftable list also has ingots and potions; no gear needs a tier-5 level)
				static int s_pool[6][World::GEAR_COUNT], s_n[6] = { -1 };
				if (s_n[0] < 0) {
					for (int t = 0; t < 6; t++) s_n[t] = 0;
					for (int g = 0; g < World::GEAR_COUNT; g++) {
						CItem probe;
						if (!probe.InitItemAttr(World::kGear[g].name) || probe.m_cEquipPos == EQUIPPOS_NONE) continue;
						const int t = Valoria::ItemTier(World::kGear[g].level);
						if (t >= 1 && t <= 5) s_pool[t][s_n[t]++] = g;
					}
				}
				int t = tier;
				while (t > 1 && s_n[t] == 0) t--;
				if (s_n[t] > 0) WorldSpill(iClientH, i, World::kGear[s_pool[t][dice(1, s_n[t]) - 1]].name, 1, true);
			}
			g_readyAt[i] = (t + World::CHEST_REFILL * 1000) | 1;
		}
		for (int c = 1; c < MAXCLIENTS; c++)
			if (m_pClientList[c] != NULL && m_pClientList[c]->m_bIsInitComplete && m_pClientList[c]->m_cMapIndex == p->m_cMapIndex) WorldSendProp(c, i);
	}
}
