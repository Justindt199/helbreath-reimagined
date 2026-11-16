// WorldProps.cpp: VALORIA's shrines and treasure chests on the client. Where they stand is
// Shared\ValoriaWorld.h (tools\valoria_port\build_world.py); the game server runs them (HG Server
// WorldProps.cpp) and says when one is used (NOTIFY_WORLDPROP).
//
// They are drawn with the map's objects (DrawObjects, tile by tile, so whoever stands in front of
// one covers it) from VALORIA's own pictures, CONTENTS\world\shrine.png and chest.png: a shrine in
// the colour of its blessing with its name over it, faded while it recharges; a chest darkened
// while it is empty.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"
#include "..\..\shared\ValoriaUse.h"
#include "..\..\shared\ValoriaWorld.h"

namespace {

// VALORIA's blessing colours (shrine.gd)
const COLORREF kBless[World::SHRINE_KINDS] = { RGB(255, 115, 89), RGB(115, 255, 140), RGB(115, 166, 255), RGB(255, 217, 89), RGB(255, 153, 230) };

COLORREF Lerp(COLORREF a, COLORREF b, int pct)
{
	return RGB(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * pct / 100, GetGValue(a) + (GetGValue(b) - GetGValue(a)) * pct / 100,
		GetBValue(a) + (GetBValue(b) - GetBValue(a)) * pct / 100);
}

} // namespace

void CGame::NotifyMsg_WorldProp(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint16 wProp, wSecs;
	Pop(cp, wProp); Pop(cp, wSecs);
	if (wProp >= World::PROP_COUNT) return;
	if (m_propReadyAt.size() != (size_t)World::PROP_COUNT) m_propReadyAt.assign(World::PROP_COUNT, 0);
	m_propReadyAt[wProp] = wSecs ? (m_dwCurTime + (DWORD)wSecs * 1000) | 1 : 0;
}

// Which props are on the map being drawn (once per frame, before its tiles).
void CGame::WorldPropsBegin()
{
	m_propsHere.clear();
	for (int i = 0; i < World::PROP_COUNT; i++)
		if (World::SameMap(World::kProps[i].map, m_cMapName)) m_propsHere.push_back(i);
}

// The prop standing on tile (tx, ty), whose spot on the screen is (sx, sy).
void CGame::DrawWorldProp(int tx, int ty, int sx, int sy)
{
	for (size_t k = 0; k < m_propsHere.size(); k++) {
		const int i = m_propsHere[k];
		const World::Prop & p = World::kProps[i];
		if (p.x != tx || p.y != ty) continue;
		bool bReady = true;
		if ((size_t)i < m_propReadyAt.size() && m_propReadyAt[i] != 0) {
			if ((int)(m_propReadyAt[i] - m_dwCurTime) > 0) bReady = false;
			else m_propReadyAt[i] = 0;
		}
		// VALORIA draws its 256 px prop cells standing on the point (128, 224); here a cell is 160 px
		const int kCell = 160;
		const int x0 = sx - kCell / 2, y0 = sy - kCell * 224 / 256;
		if (p.kind < World::SHRINE_KINDS) {
			const COLORREF c = kBless[p.kind];
			m_Renderer.WorldImage(0, x0, y0, x0 + kCell, y0 + kCell, Lerp(c, RGB(255, 255, 255), 40), bReady ? 255 : 115);
			if (bReady) {
				const char * name = ValoriaUse::BUFFS[ValoriaUse::BUFF_BLESS_MIGHT + p.kind].name;
				const int w = m_Renderer.TextWidthSized(name, 11);
				m_Renderer.TextOutSized(sx - w / 2 + 1, y0 + kCell * 100 / 256 + 1, name, RGB(0, 0, 0), 11);
				m_Renderer.TextOutSized(sx - w / 2, y0 + kCell * 100 / 256, name, Lerp(c, RGB(255, 255, 255), 30), 11);
			}
		}
		else m_Renderer.WorldImage(1, x0, y0, x0 + kCell, y0 + kCell, bReady ? RGB(255, 255, 255) : RGB(140, 128, 115), 255);
	}
}
