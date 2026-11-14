// NpcLabels.cpp: a monster's name at its feet, in an outlined serif (the UI font: PT Serif),
// coloured by friend or foe, with its health bar under it. Shown for the monster under the mouse,
// and for 8 seconds after each of your hits on one (a hit is yours if you attacked it, or cast a
// spell at where it stood, in the few seconds before).
//
// As each monster is drawn (Game.cpp, DrawObject_On*) it notes where it stands; once the scene is
// drawn (DrawObjects) the names go on top, so they stay readable over everything. The server sends
// the health a monster has left, as a percentage, with each hit on it (MSGID_MOTION_DAMAGE): the bar
// shows that, with a pale trail of what the last hit took off easing down after it.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"
#if LANGUAGE == ENGLISH
#include "..\lan_eng.h"
#elif LANGUAGE == SPANISH
#include "..\lan_spa.h"
#endif

#include <algorithm>
#include <cmath>

namespace {

const int kNamePx = 13;				// the name's size (game pixels)
const DWORD kBarShowMs = 8000;		// how long a hurt monster keeps its bar after the last hit
const DWORD kTrailWaitMs = 350;		// the pale trail waits this long, then eases down
const float kTrailPerSec = 90.f;	// ... at this many percent a second
const DWORD kAimMs = 3000;			// a hit counts as yours this long after you attacked or cast at it

COLORREF Lighten(COLORREF c, float k)
{
	const int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
	return RGB(r + (int)((255 - r) * k), g + (int)((255 - g) * k), b + (int)((255 - b) * k));
}

// text with a light dark outline (one pixel, straight sides only), so it reads on any ground
void Outlined(CRenderer & R, int x, int y, const char * s, COLORREF c, int iPx)
{
	static const int kRing[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
	for (const auto & d : kRing) R.TextOutSized(x + d[0], y + d[1], s, RGB(0, 0, 0), iPx);
	R.TextOutSized(x, y, s, c, iPx);
}

} // namespace

void CGame::NoteNpcLabel(int sX, int sY, short sType, int iStatus, WORD wID)
{
	if (m_npcLabels.size() >= 200) return;
	NpcLabel L;
	L.x = (short)sX; L.y = (short)sY; L.type = sType; L.status = iStatus; L.id = wID;
	m_npcLabels.push_back(L);
}

// NOTIFY_NPCHEALTH: a monster healed (the server sends this to everyone on its map). Its bar shows
// the new figure next time it's shown; this isn't a hit, so it doesn't bring the bar up by itself.
void CGame::NotifyMsg_NpcHealth(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint16 wObjectID = 0; uint8 cPct = 100;
	Pop(cp, wObjectID); Pop(cp, cPct);
	if (wObjectID < 40000 || cPct > 100) return;
	const WORD wID = (WORD)(wObjectID - 30000);
	auto it = m_npcHealth.find(wID);
	if (it == m_npcHealth.end()) {
		if (cPct >= 100) return;		// (unknown counts as full)
		it = m_npcHealth.emplace(wID, NpcHealth()).first;
	}
	NpcHealth & h = it->second;
	if (cPct >= h.pct) h.ghost = cPct;	// healing: no trail
	h.pct = cPct;
}

// the player attacked this monster (its object id as the map holds it; monsters are 10000 and up)
void CGame::NoteNpcAimedAt(WORD wID)
{
	if (wID < 10000) return;
	const DWORD dwNow = timeGetTime();
	if (m_npcAimedAt.size() > 200) {
		for (auto i = m_npcAimedAt.begin(); i != m_npcAimedAt.end(); )
			i = (dwNow - i->second > kAimMs) ? m_npcAimedAt.erase(i) : std::next(i);
	}
	m_npcAimedAt[wID] = dwNow;
}

// the player cast a spell at this square: the monsters on it and next to it
void CGame::NoteSpellAimedAt(int iX, int iY)
{
	if (m_pMapData == NULL) return;
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++) {
			char cName[32] = {}; short sType = 0; int iStatus = 0; WORD wID = 0;
			if (m_pMapData->bGetOwner(iX + dx, iY + dy, cName, &sType, &iStatus, &wID) && sType > 6) NoteNpcAimedAt(wID);
		}
}

// a hit on a monster: the health it has left (0-100)
void CGame::NoteNpcHealth(WORD wID, int iPct)
{
	const DWORD dwNow = timeGetTime();
	auto it = m_npcHealth.find(wID);
	if (it == m_npcHealth.end()) {
		if (m_npcHealth.size() > 400) {		// forget monsters not hit for a minute
			for (auto i = m_npcHealth.begin(); i != m_npcHealth.end(); )
				i = (dwNow - i->second.dwHitAt > 60000) ? m_npcHealth.erase(i) : std::next(i);
		}
		it = m_npcHealth.emplace(wID, NpcHealth()).first;	// (full before this first hit)
	}
	NpcHealth & h = it->second;
	const float pct = (float)(iPct < 0 ? 0 : (iPct > 100 ? 100 : iPct));
	if (h.ghost < h.pct) h.ghost = h.pct;
	if (pct > h.pct) h.ghost = pct;		// healed since: no trail
	h.pct = pct;
	h.dwHitAt = dwNow;
	auto a = m_npcAimedAt.find(wID);
	if (a != m_npcAimedAt.end() && dwNow - a->second < kAimMs) h.dwMyHitAt = dwNow;	// your hit
}

void CGame::DrawNpcLabels()
{
	CRenderer & R = m_Renderer;
	const DWORD dwNow = timeGetTime();
	static DWORD s_dwLast = 0;
	const float dt = (s_dwLast == 0) ? 0.f : (std::min)(0.1f, (dwNow - s_dwLast) / 1000.f);
	s_dwLast = dwNow;
	for (auto & kv : m_npcHealth) {			// the pale trails ease down
		NpcHealth & h = kv.second;
		if (h.ghost > h.pct && dwNow - h.dwHitAt > kTrailWaitMs) h.ghost = (std::max)(h.pct, h.ghost - kTrailPerSec * dt);
	}

	const bool bMuddled = m_bIsConfusion || (m_iIlusionOwnerH != NULL);
	for (const NpcLabel & L : m_npcLabels) {
		// only the one under the mouse, and those you've hit in the last 8 seconds
		const bool bFocus = (L.id == m_wFocusNpcID);
		auto it = m_npcHealth.find(L.id);
		const bool bMine = (it != m_npcHealth.end()) && it->second.dwMyHitAt != 0 && (dwNow - it->second.dwMyHitAt < kBarShowMs);
		if (!bFocus && !bMine) continue;

		char cName[64] = {};
		GetNpcName(L.type, cName);
		if (cName[0] == 0) continue;
		if ((L.status & STATUS_BERSERK) != 0) strcat(cName, DRAW_OBJECT_NAME50);	// " Berserk"
		if ((L.status & STATUS_FROZEN) != 0) strcat(cName, DRAW_OBJECT_NAME51);		// " Frozen"

		// friend or foe: coral red for an enemy, light blue neutral, green friendly
		const int iFOE = _iGetFOE(L.status);
		COLORREF col;
		if (bMuddled || m_bIsObserverMode == TRUE) col = RGB(190, 190, 190);
		else {
			col = (iFOE < 0) ? RGB(255, 116, 102) : (iFOE == 0 ? RGB(150, 190, 255) : RGB(159, 227, 154));
		}
		if (bFocus) col = Lighten(col, 0.35f);

		const int w = R.TextWidthSized(cName, kNamePx) + 1;
		const int x = L.x - w / 2, y = L.y + 6;
		Outlined(R, x, y, cName, col, kNamePx);
		int yNext = y + 16;

		// the health bar: full for a monster under the mouse that nobody has hit yet; none for
		// townsfolk and the like that have never been hit. (A monster's own side, not how it
		// stands to you: to a GM or a traveller even monsters aren't foes.)
		const bool bMonster = ((Side)(L.status >> 28) == HOSTILE);
		if (it != m_npcHealth.end() || bMonster) {
			static const NpcHealth kFull;
			const NpcHealth & h = (it != m_npcHealth.end()) ? it->second : kFull;
			{
				const int bw = (std::max)(40, (std::min)(72, w + 8)), bx = L.x - bw / 2, by = yNext;
				const int wFill = (int)std::lround(bw * h.pct / 100.f), wTrail = (int)std::lround(bw * h.ghost / 100.f);
				R.FillPanel(bx - 1, by - 1, bx + bw + 1, by + 5, 0, RGB(0, 0, 0), 200);		// its edge
				R.FillPanel(bx, by, bx + bw, by + 4, 0, RGB(43, 20, 20), 255);					// the track
				if (wTrail > wFill) R.FillPanel(bx + wFill, by, bx + wTrail, by + 4, 0, RGB(255, 236, 190), 225);	// what the last hit took
				if (wFill > 0) {
					R.FillPanel(bx, by, bx + wFill, by + 4, 0, h.pct > 30.f ? RGB(212, 56, 44) : RGB(255, 90, 42), 255);
					R.FillPanel(bx, by, bx + wFill, by + 1, 0, RGB(255, 170, 150), 150);		// a lighter top edge
				}
				yNext = by + 8;
			}
		}
		// the one under the mouse: its special trait (Poisonous, Explosive ...)
		if (bFocus && m_cFocusNpcExtra[0] != 0) {
			const int we = R.TextWidthSized(m_cFocusNpcExtra, 11);
			Outlined(R, L.x - we / 2, yNext, m_cFocusNpcExtra, RGB(240, 240, 70), 11);
		}
	}
}
