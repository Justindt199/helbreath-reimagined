// BottomBar.cpp: the bar at the bottom of the screen (dialogs 30 "IconPanel" and 29 "GaugePanel"),
// drawn in code in the dark-and-gold style of the other windows instead of on the stone picture
// stretched to the screen width.
//
//   left:   HP and MP bars with "current / max" on them (green while poisoned)
//   middle: the SP bar above the location box, "Map (x, y)" centred with a pin
//   right:  the crusade role (only during a crusade), combat mode, and the six menu buttons
//           (Character, Inventory, Spell Book, Skills, Chat, Options) with their usual icons
//
// Every piece keeps its own size at any screen width (IconBarLayout, DialogBox.cpp, also sets the
// click areas), so nothing is stretched. The experience strip above the bar is DrawExpStrip.
// Clicks are handled as before by DlgBoxClick_IconPanel / bItemDrop_IconPanel (Game.cpp).
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(x, y, s, c, iPx);
	r.TextOutSized(x + 1, y, s, c, iPx);
}

// Text with a dark shadow, bold, centred in [x0, x1) at y.
void CentredShadow(CRenderer & r, int x0, int x1, int y, const char * s, COLORREF c, int iPx)
{
	const int x = (x0 + x1 - r.TextWidthSized(s, iPx)) / 2;
	r.TextOutSized(x + 1, y + 1, s, RGB(0, 0, 0), iPx);
	r.TextOutSized(x + 2, y + 1, s, RGB(0, 0, 0), iPx);
	Bold(r, x, y, s, c, iPx);
}

std::string Commas(long long v)
{
	char cBuf[32];
	wsprintf(cBuf, "%d", (int)(v < 0 ? 0 : v));
	std::string s(cBuf), out;
	for (size_t i = 0; i < s.size(); i++) {
		if (i > 0 && (s.size() - i) % 3 == 0) out += ',';
		out += s[i];
	}
	return out;
}

// A colour times k (0 .. 2), clamped.
COLORREF Shade(COLORREF c, float k)
{
	auto ch = [k](int v) { const int x = (int)(v * k + 0.5f); return x < 0 ? 0 : (x > 255 ? 255 : x); };
	return RGB(ch(GetRValue(c)), ch(GetGValue(c)), ch(GetBValue(c)));
}

// A bar as a glass tube with liquid in it: dark tinted glass, the liquid filled to the fraction (deeper
// at the bottom, bright through the middle, a moving surface at its end and a few rising bubbles), a
// shine along the top of the glass over everything, and "cur / max" on it.
void DrawVital(CRenderer & r, const RECT & rc, double f, COLORREF fill, COLORREF light, const char * pText, int iPx)
{
	const DWORD t = timeGetTime();
	const int h = rc.bottom - rc.top;
	const int x0 = rc.left + 2, x1 = rc.right - 2, y0 = rc.top + 2, y1 = rc.bottom - 2, ih = y1 - y0;
	if (r.GlassTube(rc.left, rc.top, rc.right, rc.bottom, (float)f, fill, light, t)) {	// drawn pixel by pixel (graphics card picture)
		r.StoneBarFrame(rc.left, rc.top, rc.right, rc.bottom);
		if (pText && pText[0]) CentredShadow(r, rc.left, rc.right, rc.top + (h - iPx - 3) / 2, pText, kWhite, iPx);
		return;
	}
	r.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 0, RGB(6, 7, 9), 240);		// the tube's back
	if (ih > 0) r.FillPanel(x0, y0, x1, y1, 0, Shade(fill, 0.45f), 60);				// empty glass, faintly tinted
	if (f < 0) f = 0;
	if (f > 1) f = 1;
	const int w = (int)((x1 - x0) * f + 0.5);
	if (w > 0 && ih > 0) {
		const bool bFull = (w >= x1 - x0);
		for (int y = y0; y < y1; y++) {		// the liquid, row by row
			const float v = (ih > 1) ? (float)(y - y0) / (ih - 1) : 0.5f;	// 0 top .. 1 bottom
			const float k = (1.05f + 0.7f * sinf(3.14159f * v) * (1.f - v)) * (1.f - 0.4f * v);	// bright upper middle, deep bottom
			int xe = x0 + w;
			if (!bFull) xe += (int)floorf(1.4f * sinf(t * 0.006f + y * 0.9f) + 0.5f);	// the surface moves
			xe = (std::min)(xe, x1);
			if (xe > x0) r.FillPanel(x0, y, xe, y + 1, 0, Shade(fill, k), 255);
		}
		if (!bFull && w > 2) {	// the surface: a bright edge
			r.FillPanel(x0 + w - 2, y0, (std::min)(x0 + w, x1), y1, 0, light, 140);
			r.FillPanel(x0 + w - 4, y0, x0 + w - 2, y1, 0, light, 50);
		}
		for (int b = 0; b < 4 && w > 6 && ih > 4; b++) {	// bubbles rising through the liquid
			const DWORD period = 1400 + b * 370;
			const DWORD cycle = (t + b * 911) / period;
			const float ph = (float)((t + b * 911) % period) / period;
			const unsigned seed = (unsigned)(cycle * 2654435761u + b * 40503u);
			const int bx = x0 + 2 + (int)(seed % (unsigned)(w - 4));
			const int by = y1 - 2 - (int)(ph * (ih - 3));
			r.FillPanel(bx, by, bx + 2, by + (ih > 8 ? 2 : 1), 0, Shade(light, 1.15f), (int)(170 * (1.f - ph * 0.6f)));
		}
	}
	if (ih > 0) {	// the glass over it all: a shine along the top, a softer one below the middle, a rim line
		const int band = (std::max)(1, ih * 3 / 10);
		r.FillPanel(x0, y0, x1, y0 + 1, 0, RGB(255, 255, 255), 95);
		r.FillPanel(x0 + 1, y0 + 1, x1 - 1, y0 + band, 0, RGB(255, 255, 255), 48);
		if (ih > 6) r.FillPanel(x0 + 2, y0 + ih * 6 / 10, x1 - 2, y0 + ih * 6 / 10 + 1, 0, RGB(255, 255, 255), 18);
		r.FillPanel(x0, y1 - 1, x1, y1, 0, RGB(255, 255, 255), 28);
	}
	r.StoneBarFrame(rc.left, rc.top, rc.right, rc.bottom);	// the iron housing
	if (pText && pText[0]) CentredShadow(r, rc.left, rc.right, rc.top + (h - iPx - 3) / 2, pText, kWhite, iPx);
}

// 0.35 .. 1, once a second: how bright the low-HP glow is right now.
float LowHpPulse(DWORD dwTime)
{
	const float t = (float)(dwTime % 1000) / 1000.f;
	return 0.35f + 0.65f * (0.5f + 0.5f * cosf(t * 6.2831853f));
}

// Draws frame sFrame of the bar sprite sheet at (x, y), showing only the part inside clip.
void DrawClipped(CSprite * pSpr, CRenderer & r, int x, int y, int sFrame, const RECT & clip, DWORD dwTime)
{
	if (pSpr == NULL) return;
	const RECT was = r.m_rcClipArea;
	RECT c;
	if (!IntersectRect(&c, &was, &clip)) return;
	r.m_rcClipArea = c;
	pSpr->PutSpriteFast(x, y, sFrame, dwTime);
	r.m_rcClipArea = was;
}

} // namespace

void CGame::DrawDialogBox_IconPanel()
{
	CRenderer & R = m_Renderer;
	const DWORD dwTime = m_dwCurTime;
	const short X = m_dialogBoxes[30].m_X, Y = m_dialogBoxes[30].m_Y;
	const int W = m_dialogBoxes[30].sSizeX;
	const char onButton = m_dialogBoxes[30].OnButton();
	const bool bCrusade = (m_bIsCrusadeMode && m_iCrusadeDuty != 0);
	CSprite * pBar = m_pSprite[SPRID_INTERFACE_ND_ICONPANNEL];

	IconBarRects L;
	IconBarLayout(W, bCrusade, L);
	auto At = [&](const RECT & rc) { RECT o = rc; OffsetRect(&o, X, Y); return o; };

	// the panel
	R.StonePanel(X, Y, X + W, Y + ICONBAR_H);	// VALORIA stone

	// HP and MP
	{
		const int iMaxHP = MaxHP();
		const int iMaxMP = MaxMP();
		if (iMaxHP > 0 && m_iHP > iMaxHP) m_iHP = iMaxHP;
		if (iMaxMP > 0 && m_iMP > iMaxMP) m_iMP = iMaxMP;
		const int iPx = (L.hp.right - L.hp.left) >= 110 ? 10 : 9;
		std::string s = Commas(m_iHP) + " / " + Commas(iMaxHP);
		if (IsLowHp()) {	// a red glow round the HP bar, pulsing with the screen edges
			const RECT rc = At(L.hp);
			const int a = (int)(LowHpPulse(dwTime) * 200);
			R.FillPanel(rc.left - 3, rc.top - 3, rc.right + 3, rc.bottom + 3, (rc.bottom - rc.top) / 2 + 3, RGB(255, 50, 30), a / 4, RGB(255, 90, 60), a);
		}
		if (m_bIsPoisoned) {
			DrawVital(R, At(L.hp), iMaxHP > 0 ? (double)m_iHP / iMaxHP : 0, RGB(52, 160, 64), RGB(150, 240, 140), s.c_str(), iPx);
			const RECT rc = At(L.hp);
			if (rc.right - rc.left >= 150) R.TextOutSized(rc.left + 8, rc.top + 2, "Poisoned", RGB(210, 255, 200), 8);
		}
		else DrawVital(R, At(L.hp), iMaxHP > 0 ? (double)m_iHP / iMaxHP : 0, RGB(196, 46, 38), RGB(255, 130, 115), s.c_str(), iPx);
		s = Commas(m_iMP) + " / " + Commas(iMaxMP);
		DrawVital(R, At(L.mp), iMaxMP > 0 ? (double)m_iMP / iMaxMP : 0, RGB(48, 86, 206), RGB(140, 170, 255), s.c_str(), iPx);
	}

	// SP above the location
	{
		const int iMaxSP = MaxSP();
		if (iMaxSP > 0 && m_iSP > iMaxSP) m_iSP = iMaxSP;
		const std::string s = Commas(m_iSP) + " / " + Commas(iMaxSP);
		DrawVital(R, At(L.sp), iMaxSP > 0 ? (double)m_iSP / iMaxSP : 0, RGB(56, 164, 72), RGB(150, 235, 150), s.c_str(), 8);
	}

	// location: "Map (x, y)", centred with a pin; smaller, then shortened, if it doesn't fit
	{
		const RECT rc = At(L.loc);
		R.Slate(rc.left, rc.top, rc.right, rc.bottom);
		char cCoords[32];
		wsprintf(cCoords, " (%d, %d)", m_sPlayerX, m_sPlayerY);
		std::string name = m_cMapMessage, text = name + cCoords;
		const int kPinW = 14, avail = (rc.right - rc.left) - 14 - kPinW;
		int iPx = 12, tw = R.TextWidthSized(text.c_str(), iPx);
		while (iPx > 9 && tw > avail) tw = R.TextWidthSized(text.c_str(), --iPx);
		while (tw > avail && name.size() > 1) {
			name.erase(name.size() - 1);
			text = name + "..." + cCoords;
			tw = R.TextWidthSized(text.c_str(), iPx);
		}
		const int x0 = (rc.left + rc.right - (kPinW + tw)) / 2;
		const int cy = (rc.top + rc.bottom) / 2;
		// pin: a gold drop with a dark dot
		R.FillPanel(x0, cy - 7, x0 + 10, cy + 2, 5, kGold, 255, kGold, 0, 5, x0 + 5);
		R.FillPanel(x0 + 3, cy - 4, x0 + 7, cy, 2, RGB(40, 28, 10), 255);
		const int ty = cy - (iPx + 3) / 2;
		R.TextOutSized(x0 + kPinW + 1, ty + 1, text.c_str(), RGB(0, 0, 0), iPx);
		Bold(R, x0 + kPinW, ty, text.c_str(), kWhite, iPx);
	}

	// crusade role (only during a crusade)
	if (bCrusade) {
		const RECT rc = At(L.crusade);
		const bool bOn = (onButton == 1);
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 8, RGB(170, 40, 30), bOn ? 80 : 46, RGB(255, 120, 100), bOn ? 255 : 180);
		int iFrame = -1;
		if (m_side == ARESDEN) iFrame = bOn ? 1 : 2;
		else if (m_side == ELVINE) iFrame = bOn ? 0 : 15;
		RECT clip;
		SetRect(&clip, rc.left + 4, rc.top + 4, rc.left + 28, rc.bottom - 4);
		if (iFrame >= 0) DrawClipped(pBar, R, clip.left - 6, rc.top + (rc.bottom - rc.top - 44) / 2, iFrame, clip, dwTime);
		static const char * const kRole[4] = { "", "Fighter", "Constructor", "Commander" };
		const char * pRole = (m_iCrusadeDuty >= 1 && m_iCrusadeDuty <= 3) ? kRole[m_iCrusadeDuty] : "Crusade";
		Bold(R, rc.left + 32, rc.top + 6, pRole, RGB(255, 176, 154), 10);
		R.TextOutSized(rc.left + 32, rc.top + 20, "Crusade role", kGrey, 8);
	}

	// combat mode
	{
		const RECT rc = At(L.combat);
		const bool bOn = (onButton == 2);
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, (rc.bottom - rc.top) / 2, kGold, bOn ? 60 : (m_bIsCombatMode ? 34 : 14),
			kGold, bOn ? 255 : (m_bIsCombatMode ? 170 : 80));
		const int ox = rc.left + 3, oy = rc.top + (rc.bottom - rc.top - 30) / 2;
		if (pBar) pBar->PutSpriteFast(ox, oy, (m_bIsCombatMode && !m_bIsSafeAttackMode) ? 5 : 4, dwTime);
		if (!m_bIsCombatMode) R.FillPanel(ox + 1, oy + 1, ox + 29, oy + 29, 14, RGB(0, 0, 0), 150);	// dimmed while peaceful
		const int tx = ox + 34;
		if (m_bIsCombatMode) {
			Bold(R, tx, rc.top + 6, "Combat", RGB(255, 215, 122), 10);
			R.TextOutSized(tx, rc.top + 20, m_bIsSafeAttackMode ? "Safe on" : "Safe off", kGrey, 8);
		}
		else Bold(R, tx, rc.top + (rc.bottom - rc.top - 13) / 2, "Peaceful", RGB(200, 204, 212), 10);

		// critical hits left, on the orb as on the old bar: the lightning orb while Alt is held (100%
		// weapon mastery) and the count in the sprite font, light with full mastery, dark without
		if (m_iSuperAttackLeft > 0) {
			const bool bMastery = (m_cSkillMastery[_iGetWeaponSkillType()] == 100);
			if (bMastery && (GetAsyncKeyState(VK_MENU) >> 15) && pBar) pBar->PutTransSprite(ox - 1, oy - 1, 3, dwTime);
			char cN[12];
			wsprintf(cN, "%d", m_iSuperAttackLeft);
			if (bMastery) PutString_SprFont2(ox + 12, oy + 14, cN, 220, 200, 200);
			else PutString_SprFont2(ox + 12, oy + 14, cN, 10, 10, 10);
		}
	}

	// the menu buttons, with their usual icons
	static const char * const kName[6] = { "Character", "Inventory", "Spell Book", "Skills", "Chat", "Options" };
	static const char * const kKey[6]  = { "F5", "F6", "F7", "F8", "F9", "F12" };
	for (int k = 0; k < 6; k++) {
		const RECT rc = At(L.btn[k]);
		const bool bOn = (onButton == 3 + k);
		R.StoneSlot(rc.left, rc.top, rc.right, rc.bottom, bOn);	// VALORIA's recessed slot, gold-rimmed under the mouse
		// the icon tile is 36 x 44 with a light edge (3 px, top / left) and a dark red one (3 px,
		// right / bottom): show its inside, 30 x 38, centred in the slot
		const int sx = rc.left + (rc.right - rc.left - 30) / 2 - 3, sy = rc.top + (rc.bottom - rc.top - 38) / 2 - 3;
		RECT clip;
		SetRect(&clip, sx + 3, sy + 3, sx + 33, sy + 41);
		DrawClipped(pBar, R, sx, sy, 6 + k, clip, dwTime);
		if (!bOn) R.FillPanel(clip.left, clip.top, clip.right, clip.bottom, 0, RGB(0, 0, 0), 40);
		else {
			// name and key above the button
			char cTip[32];
			wsprintf(cTip, "%s  %s", kName[k], kKey[k]);
			const int tw = R.TextWidthSized(cTip, 11) + 16;
			int tx = (rc.left + rc.right - tw) / 2;
			if (tx + tw > X + W - 4) tx = X + W - 4 - tw;
			const int ty = Y - 30;
			R.FillPanel(tx, ty, tx + tw, ty + 19, 6, RGB(8, 9, 12), 245, RGB(201, 162, 82), 255);
			R.TextOutSized(tx + 8, ty + 3, kName[k], kWhite, 11);
			R.TextOutSized(tx + 8 + R.TextWidthSized(kName[k], 11) + R.TextWidthSized("  ", 11), ty + 3, kKey[k], kGrey, 11);
		}
	}
}

// The HP / MP / SP bars are part of the bar above (DrawDialogBox_IconPanel); this only draws the
// experience strip on top of it.
void CGame::DrawDialogBox_GaugePanel()
{
	DrawExpStrip();
}

//////////////////////////////////////////////////////////////////////
// Low HP warning: under 25% of your maximum HP the HP bar glows and the edges of the screen glow
// red, pulsing once a second, stronger the lower you are.

bool CGame::IsLowHp() const
{
	if (m_cGameMode != GAMEMODE_ONMAINGAME || m_bIsObserverMode || m_iHP <= 0) return false;
	const int iMaxHP = MaxHP();
	return iMaxHP > 0 && m_iHP * 4 < iMaxHP;
}

void CGame::DrawLowHpGlow()
{
	if (!IsLowHp()) return;
	const int iMaxHP = MaxHP();
	const float fLow = 1.f - (float)m_iHP / (iMaxHP * 0.25f);		// 0 at 25%, 1 near 0 HP
	const float f = LowHpPulse(m_dwCurTime) * (0.6f + 0.4f * (fLow < 0 ? 0 : (fLow > 1 ? 1 : fLow)));
	const int iBand = (std::max)(40, (std::min)((int)GAME_W, (int)VIEW_BOTTOM) / 7);
	m_Renderer.FillEdgeGlow(0, 0, GAME_W, VIEW_BOTTOM, iBand, RGB(200, 20, 10), (int)(f * 150));
}
