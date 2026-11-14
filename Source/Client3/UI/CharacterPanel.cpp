// CharacterPanel.cpp: the character window (F5), drawn in code.
//
// The window is a see-through rounded panel drawn into the back buffer (and at twice the size
// when HD is on), laid out as:
//   - a title bar with a close button
//   - left: the paper doll (the game's own equipment sprites, drawn by DrawDialogBox_Character
//     at CharDollOrigin) in an inset box, with a tooltip for the item under the mouse, and a
//     list of kills, PK count, contribution and Majestic points under it
//   - right: name, town and guild, a level medallion, bars for experience, health, mana,
//     stamina and weight, and the six attributes as cards (item / angel bonus in blue), with
//     the number of level-up points still to spend
//   - a footer with Quests, Party, Guild and Level-up settings buttons
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>
#include <string>

namespace {

// layout, relative to the window's top-left corner
const int kPad		= 14;
const int kTitleH	= 34;
const int kDollX0	= kPad, kDollY0 = 42, kDollX1 = 214, kDollY1 = 292;	// paper doll box
const int kListY	= 302;												// kills etc. under it
const int kRightX	= 230;												// right column
const int kFootH	= 44;												// footer (buttons)

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(112, 118, 130);
const COLORREF kBonus	= RGB(120, 170, 255);

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx = 0)
{
	if (iPx > 0) { r.TextOutSized(x, y, s, c, iPx); r.TextOutSized(x + 1, y, s, c, iPx); }
	else { r.TextOut(x, y, s, c); r.TextOut(x + 1, y, s, c); }
}

int Width(CRenderer & r, const char * s, int iPx = 0)
{
	return (iPx > 0) ? r.TextWidthSized(s, iPx) : (int)r.GetTextExtent(s).cx;
}

void RightText(CRenderer & r, int xRight, int y, const char * s, COLORREF c)
{
	r.TextOut(xRight - Width(r, s), y, s, c);
}

// text with a dark outline-ish shadow, for text over bars
void ShadowText(CRenderer & r, int x, int y, const char * s, COLORREF c)
{
	r.TextOut(x + 1, y + 1, s, RGB(0, 0, 0));
	r.TextOut(x, y, s, c);
}

// 1,234,567
std::string Commas(long long v)
{
	const bool bNeg = v < 0;
	unsigned long long u = bNeg ? (unsigned long long)(-v) : (unsigned long long)v;
	std::string s = std::to_string(u);
	for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
	return bNeg ? "-" + s : s;
}

// a small round dot between words
void Dot(CRenderer & r, int x, int y)
{
	r.FillPanel(x, y, x + 4, y + 4, 2, RGB(120, 126, 138), 255);
}

void Bar(CRenderer & r, int x, int y, int w, int h, double fFrac, COLORREF c)
{
	r.FillPanel(x, y, x + w, y + h, h / 2, RGB(40, 43, 50), 230);
	if (fFrac < 0) fFrac = 0;
	if (fFrac > 1) fFrac = 1;
	const int fw = (int)(w * fFrac + 0.5);
	if (fw >= 2) r.FillPanel(x, y, x + fw, y + h, (std::min)(h / 2, fw / 2), c, 245);
}

struct AttrStyle { const char * tag; const char * name; COLORREF bg, fg; };
const AttrStyle kAttr[6] = {
	{ "STR", "Strength",     RGB(96, 33, 33),  RGB(255, 160, 160) },
	{ "VIT", "Vitality",     RGB(31, 78, 42),  RGB(160, 226, 170) },
	{ "DEX", "Dexterity",    RGB(80, 74, 22),  RGB(240, 230, 140) },
	{ "INT", "Intelligence", RGB(31, 47, 92),  RGB(172, 188, 255) },
	{ "MAG", "Magic",        RGB(66, 40, 94),  RGB(214, 176, 255) },
	{ "CHR", "Charisma",     RGB(96, 62, 18),  RGB(255, 200, 110) },
};
const int kAttrStat[6] = { STAT_STR, STAT_VIT, STAT_DEX, STAT_INT, STAT_MAG, STAT_CHR };

struct FootBtn { int id; const char * label; RECT rc; bool gold; };

// slot boxes beside the paper doll for the small equipment (window-relative top-left corners)
struct SlotBox { int pos; int x, y; const char * label; };
const int kSlotSz = 36;
const SlotBox kSlots[3] = {
	{ EQUIPPOS_NECK,    kDollX0 + 35,  kDollY0 + 37,  "Necklace" },	// left of the shoulders
	{ EQUIPPOS_RFINGER, kDollX0 + 35,  kDollY0 + 152, "Ring" },		// left, by the knees
	{ EQUIPPOS_LFINGER, kDollX0 + 137, kDollY0 + 121, "Angel" },	// right, by the hand
};

} // namespace

// Where DrawDialogBox_Character / DlgBoxPress_Character / DlgBoxDoubleClick_Character start
// their paper doll pivot math: the body sprite is centred in the doll box.
void CGame::CharDollOrigin(short & sX, short & sY)
{
	sX = m_dialogBoxes[DIALOG_CHARACTER].m_X;
	sY = m_dialogBoxes[DIALOG_CHARACTER].m_Y;

	int iSpr = -1, iFrame = 0;
	if (m_sPlayerType >= 1 && m_sPlayerType <= 3) { iSpr = SPRID_ITEMEQUIP_PIVOTPOINT + 0;  iFrame = m_sPlayerType - 1; }
	else if (m_sPlayerType >= 4 && m_sPlayerType <= 6) { iSpr = SPRID_ITEMEQUIP_PIVOTPOINT + 40; iFrame = m_sPlayerType - 4; }

	int cx = 92, cy = 190;	// the body's middle in the old window, if the sprite can't tell us
	if (iSpr >= 0 && m_pSprite[iSpr] != NULL && m_pSprite[iSpr]->m_stBrush != NULL
		&& iFrame < m_pSprite[iSpr]->m_iTotalFrame) {
		const stBrush & b = m_pSprite[iSpr]->m_stBrush[iFrame];
		if (b.szx > 0 && b.szy > 0) {
			cx = 171 + b.pvx + b.szx / 2;
			cy = 290 + b.pvy + b.szy / 2;
		}
	}
	sX += (short)((kDollX0 + kDollX1) / 2 - cx);
	sY += (short)((kDollY0 + kDollY1) / 2 + 6 - cy);
}

// Footer buttons, left to right, in screen coordinates; returns how many.
static int CharFooterButtons(CGame & g, CRenderer & r, short X, short Y, bool bGuild, FootBtn out[5])
{
	const int H = CGame::CHARWIN_H, W = CGame::CHARWIN_W;
	const int y0 = Y + H - kFootH + 9, y1 = y0 + 26;
	int n = 0, x = X + kPad;
	const struct { int id; const char * label; } left[4] = {
		{ CGame::CHARBTN_QUEST, "Quests" }, { CGame::CHARBTN_PARTY, "Party" }, { CGame::CHARBTN_GUILD, "Guild" },
		{ CGame::CHARBTN_WANTED, "Most Wanted" } };
	for (int i = 0; i < 4; i++) {
		if (left[i].id == CGame::CHARBTN_GUILD && !bGuild) continue;
		const int w = Width(r, left[i].label) + 26;
		out[n].id = left[i].id; out[n].label = left[i].label; out[n].gold = false;
		SetRect(&out[n].rc, x, y0, x + w, y1);
		x += w + 8; n++;
	}
	const char * pLU = "Level-up settings";
	const int w = Width(r, pLU) + 32;
	out[n].id = CGame::CHARBTN_LEVELUP; out[n].label = pLU; out[n].gold = true;
	SetRect(&out[n].rc, X + W - kPad - w, y0, X + W - kPad, y1);
	n++;
	(void)g;
	return n;
}

static void CloseRect(short X, short Y, RECT & rc)
{
	SetRect(&rc, X + CGame::CHARWIN_W - 32, Y + 6, X + CGame::CHARWIN_W - 8, Y + 30);
}

static bool InRect(const RECT & rc, int x, int y)
{
	return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

int CGame::CharSlotAt(short msX, short msY)
{
	const short X = m_dialogBoxes[DIALOG_CHARACTER].m_X, Y = m_dialogBoxes[DIALOG_CHARACTER].m_Y;
	for (int i = 0; i < 3; i++) {
		const int x0 = X + kSlots[i].x, y0 = Y + kSlots[i].y;
		if (msX >= x0 && msX < x0 + kSlotSz && msY >= y0 && msY < y0 + kSlotSz) return kSlots[i].pos;
	}
	return -1;
}

int CGame::CharPanelButtonAt(short msX, short msY)
{
	const short X = m_dialogBoxes[DIALOG_CHARACTER].m_X, Y = m_dialogBoxes[DIALOG_CHARACTER].m_Y;
	RECT rc;
	CloseRect(X, Y, rc);
	if (InRect(rc, msX, msY)) return CHARBTN_CLOSE;
	FootBtn b[5];
	const int n = CharFooterButtons(*this, m_Renderer, X, Y, m_iGuildRank != GUILDRANK_NONE, b);
	for (int i = 0; i < n; i++)
		if (InRect(b[i].rc, msX, msY)) return b[i].id;
	return CHARBTN_NONE;
}

// The window, title bar and doll box: drawn before the paper doll.
void CGame::DrawCharPanelBack()
{
	CRenderer & R = m_Renderer;
	const short X = m_dialogBoxes[DIALOG_CHARACTER].m_X, Y = m_dialogBoxes[DIALOG_CHARACTER].m_Y;
	const int W = CHARWIN_W, H = CHARWIN_H;
	const bool bTop = m_dialogBoxes[DIALOG_CHARACTER].IsTop();

	R.FillPanel(X, Y, X + W, Y + H, 10, RGB(14, 16, 20), m_bDialogTrans ? 205 : 255, RGB(150, 128, 82), 150);	// solid unless see-through windows are on (a solid fill is much faster)

	// title bar
	R.TitleOut(X + kPad, Y + 9, "CHARACTER", kGold, 16);
	RECT rc;
	CloseRect(X, Y, rc);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 8, rc.top + 1, "x", bOnClose ? kWhite : kGrey, 18);
	R.FillPanel(X + 1, Y + kTitleH, X + W - 1, Y + kTitleH + 1, 0, RGB(255, 255, 255), 26);

	// doll box
	R.FillPanel(X + kDollX0, Y + kDollY0, X + kDollX1, Y + kDollY1, 8, RGB(0, 0, 0), 95, RGB(255, 255, 255), 24);
	// a soft floor under the feet
	R.FillPanel(X + (kDollX0 + kDollX1) / 2 - 46, Y + kDollY1 - 34, X + (kDollX0 + kDollX1) / 2 + 46, Y + kDollY1 - 22,
		6, RGB(0, 0, 0), 70);
}

// Everything else: drawn after the paper doll. iHoverSlot is the equipment position under the
// mouse (-1 for none) and cEquipPoiStatus the item index worn at each position.
void CGame::DrawCharPanelFront(int iHoverSlot, const char * cEquipPoiStatus)
{
	CRenderer & R = m_Renderer;
	const short X = m_dialogBoxes[DIALOG_CHARACTER].m_X, Y = m_dialogBoxes[DIALOG_CHARACTER].m_Y;
	const int W = CHARWIN_W, H = CHARWIN_H;
	const int RX = X + kRightX, RR = X + W - kPad, RW = RR - RX;
	const bool bTop = m_dialogBoxes[DIALOG_CHARACTER].IsTop();
	char cBuf[128];

	// ---- header: name, town, guild, level medallion
	char cName[16] = {};
	memcpy(cName, m_cPlayerName, sizeof(m_cPlayerName));
	R.TitleOut(RX, Y + 40, cName, kWhite, 24);

	int x = RX;
	const int yTown = Y + 74;
	COLORREF cSide = kGrey;
	if (m_side == ARESDEN) cSide = RGB(255, 112, 112);
	else if (m_side == ELVINE) cSide = RGB(122, 164, 255);
	Bold(R, x, yTown, sideName[m_side], cSide);
	x += Width(R, sideName[m_side]) + 7;
	if (m_side != NEUTRAL) {
		Dot(R, x, yTown + 7); x += 10;
		const char * pC = CombatantText();
		while (*pC == ' ') pC++;
		R.TextOut(x, yTown, pC, kGrey);
		x += Width(R, pC) + 7;
	}
	if (m_iPKCount > 0) {
		Dot(R, x, yTown + 7); x += 10;
		R.TextOut(x, yTown, "Criminal", RGB(255, 120, 120));
		x += Width(R, "Criminal") + 7;
	}
	if (m_iRebirth > 0) {	// rebirth level (UI\RebirthPanel.cpp)
		Dot(R, x, yTown + 7); x += 10;
		wsprintf(cBuf, m_iRebirthMode == 1 ? "Rebirth %d (levelling)" : (m_iRebirthMode == 2 ? "Rebirth %d (paused)" : "Rebirth %d"), m_iRebirth);
		R.TextOut(x, yTown, cBuf, RGB(196, 160, 255));
	}

	if (m_iGuildRank != GUILDRANK_NONE) {
		char cGuild[24] = {};
		memcpy(cGuild, m_cGuildName, sizeof(m_cGuildName));
		const char * pRank = GuildRankText(m_iGuildRank);
		const int w1 = Width(R, cGuild), w2 = Width(R, pRank);
		const bool bRank = (RX + w1 + w2 + 34 <= RR - 72);	// leave room for the level medallion
		const int px0 = RX, py0 = Y + 95, px1 = RX + w1 + (bRank ? w2 + 34 : 18), py1 = py0 + 20;
		R.FillPanel(px0, py0, px1, py1, 5, RGB(28, 66, 36), 225, RGB(92, 168, 104), 200);
		R.TextOut(px0 + 9, py0 + 2, cGuild, RGB(170, 232, 178));
		if (bRank) {
			Dot(R, px0 + 14 + w1, py0 + 9);
			R.TextOut(px0 + 23 + w1, py0 + 2, pRank, RGB(170, 232, 178));
		}
	}

	const int mcx = RR - 34, mcy = Y + 76, mr = 32;
	R.FillPanel(mcx - mr, mcy - mr, mcx + mr, mcy + mr, mr, RGB(30, 26, 18), 240, kGold, 230);
	R.TextOut(mcx - Width(R, "LEVEL") / 2, mcy - 22, "LEVEL", RGB(196, 170, 112));
	wsprintf(cBuf, "%d", m_iLevel);
	Bold(R, mcx - Width(R, cBuf, 24) / 2, mcy - 8, cBuf, kGold, 24);

	// ---- experience
	int y = Y + 124;
	R.TextOut(RX, y, "Experience", kGrey);
	const bool bMax = (m_iLevel >= MAXLEVEL);
	const long long llBase = bMax ? 0 : m_levelExpTable[m_iLevel];
	const long long llNext = bMax ? 0 : m_levelExpTable[m_iLevel + 1];
	std::string sExp = Commas(m_iExp);
	if (!bMax) sExp += " / " + Commas(llNext);
	RightText(R, RR, y, sExp.c_str(), kWhite);
	y += 18;
	double fExp = 1.0;
	if (!bMax && llNext > llBase) fExp = (double)(m_iExp - llBase) / (double)(llNext - llBase);
	if (fExp < 0) fExp = 0;
	if (fExp > 1) fExp = 1;
	Bar(R, RX, y, RW, 16, fExp, RGB(214, 164, 64));
	if (bMax) strcpy(cBuf, "Max level");
	else {
		const int iTenths = (int)(fExp * 1000.0);
		wsprintf(cBuf, "%d.%d%% to level %d", iTenths / 10, iTenths % 10, m_iLevel + 1);
	}
	ShadowText(R, RX + (RW - Width(R, cBuf)) / 2, y, cBuf, RGB(255, 255, 255));
	y += 24;

	// ---- vitals
	const int iMaxHP = MaxHP();
	const int iMaxMP = MaxMP();
	const int iMaxSP = MaxSP();
	const int iMaxWt = m_stat[STAT_STR] * 5 + m_iLevel * 5;
	const double fWt = iMaxWt > 0 ? (double)m_weight / iMaxWt : 0;
	const struct { const char * label; long long cur, max; COLORREF c; } vit[4] = {
		{ "Health",  m_iHP, iMaxHP, RGB(212, 60, 60) },
		{ "Mana",    m_iMP, iMaxMP, RGB(68, 112, 232) },
		{ "Stamina", m_iSP, iMaxSP, RGB(72, 182, 84) },
		{ "Weight",  (long long)m_weight, iMaxWt, fWt >= 1.0 ? RGB(222, 70, 60) : fWt >= 0.8 ? RGB(230, 150, 50) : RGB(150, 156, 168) },
	};
	for (int i = 0; i < 4; i++) {
		R.TextOut(RX, y, vit[i].label, kGrey);
		const std::string s = Commas(vit[i].cur) + " / " + Commas(vit[i].max);
		RightText(R, RR, y, s.c_str(), kWhite);
		Bar(R, RX, y + 17, RW, 9, vit[i].max > 0 ? (double)vit[i].cur / vit[i].max : 0, vit[i].c);
		y += 31;
	}

	// ---- attributes
	y += 2;
	R.TextOut(RX, y, "ATTRIBUTES", kDim);
	const int iPoints = RebirthStatBonus() + m_iLevel * 3
		- ((m_stat[STAT_STR] + m_stat[STAT_VIT] + m_stat[STAT_DEX] + m_stat[STAT_INT] + m_stat[STAT_MAG] + m_stat[STAT_CHR]) - 70)
		- 3 + m_angelStat[STAT_STR] + m_angelStat[STAT_DEX] + m_angelStat[STAT_INT] + m_angelStat[STAT_MAG];
	int xLine = RX + Width(R, "ATTRIBUTES") + 10;
	if (iPoints > 0) {
		wsprintf(cBuf, iPoints == 1 ? "%d point to spend" : "%d points to spend", iPoints);
		const int w = Width(R, cBuf) + 18;
		R.FillPanel(xLine, y, xLine + w, y + 18, 9, kGold, 240);
		Bold(R, xLine + 9, y + 1, cBuf, RGB(36, 28, 10));
		xLine += w + 10;
	}
	if (xLine < RR) R.FillPanel(xLine, y + 9, RR, y + 10, 0, RGB(255, 255, 255), 24);
	y += 24;

	// each card: the coloured tag on the left, the value (and bonus) on top, the full name under it
	const int cw = (RW - 8) / 2, ch = 36;
	for (int i = 0; i < 6; i++) {
		const int cx0 = RX + (i % 2) * (cw + 8), cy0 = y + (i / 2) * (ch + 5);
		R.FillPanel(cx0, cy0, cx0 + cw, cy0 + ch, 6, RGB(255, 255, 255), 14, RGB(255, 255, 255), 26);
		R.FillPanel(cx0 + 6, cy0 + 9, cx0 + 42, cy0 + 27, 4, kAttr[i].bg, 255);
		R.TextOut(cx0 + 24 - Width(R, kAttr[i].tag) / 2, cy0 + 10, kAttr[i].tag, kAttr[i].fg);
		const int iStat = kAttrStat[i];
		const int iAngel = (iStat == STAT_VIT || iStat == STAT_CHR) ? 0 : m_angelStat[iStat];
		const int iBonus = iAngel + GearStatBonus(iStat);		// angel + VALORIA gear
		wsprintf(cBuf, "%d", m_stat[iStat] - iAngel);
		Bold(R, cx0 + 50, cy0 + 1, cBuf, kWhite, 16);
		if (iBonus != 0) {
			char cB[16];
			wsprintf(cB, iBonus > 0 ? "+%d" : "%d", iBonus);
			R.TextOut(cx0 + 55 + Width(R, cBuf, 16), cy0 + 2, cB, kBonus);
		}
		R.TextOutSized(cx0 + 50, cy0 + 19, kAttr[i].name, kGrey, 12);
	}

	// ---- kills, PK, contribution, Majestic points (left, under the doll)
	{
		const int lx0 = X + kDollX0, lx1 = X + kDollX1;
		int ly = Y + kListY;
		char cV[4][40];
		wsprintf(cV[0], "%d / %d", m_iEnemyKillCount, m_iEnemyKillTotalCount);
		wsprintf(cV[1], "%d", m_iPKCount);
		wsprintf(cV[2], "%s", Commas(m_iContribution).c_str());
		wsprintf(cV[3], "%d", m_iGizonItemUpgradeLeft);
		const char * pL[4] = { "Enemy kills", "PK count", "Contribution", "Majestic points" };
		for (int i = 0; i < 4; i++) {
			R.TextOut(lx0 + 2, ly, pL[i], kGrey);
			RightText(R, lx1 - 2, ly, cV[i], (i == 1 && m_iPKCount > 0) ? RGB(255, 120, 120) : kWhite);
			if (i < 3) R.FillPanel(lx0, ly + 19, lx1, ly + 20, 0, RGB(255, 255, 255), 18);
			ly += 24;
		}
	}

	// ---- footer
	R.FillPanel(X + 1, Y + H - kFootH, X + W - 1, Y + H - kFootH + 1, 0, RGB(255, 255, 255), 26);
	FootBtn b[5];
	const int n = CharFooterButtons(*this, R, X, Y, m_iGuildRank != GUILDRANK_NONE, b);
	for (int i = 0; i < n; i++) {
		const RECT & rc = b[i].rc;
		const bool bOn = bTop && InRect(rc, ms::X, ms::Y);
		const int ty = rc.top + (rc.bottom - rc.top - 16) / 2;
		if (b[i].gold) {
			R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, bOn ? RGB(248, 208, 120) : kGold, 245);
			Bold(R, rc.left + 16, ty, b[i].label, RGB(36, 28, 10));
		}
		else {
			R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(255, 255, 255), bOn ? 36 : 16, RGB(255, 255, 255), bOn ? 70 : 42);
			R.TextOut(rc.left + 13, ty, b[i].label, bOn ? RGB(255, 255, 255) : RGB(220, 224, 230));
		}
	}

	// ---- slot boxes: necklace, ring, angel (these items are not drawn on the doll)
	const int iBoxHover = bTop ? CharSlotAt(ms::X, ms::Y) : -1;
	for (int i = 0; i < 3; i++) {
		const int bx = X + kSlots[i].x, by = Y + kSlots[i].y, pos = kSlots[i].pos;
		const int iItem = cEquipPoiStatus[pos];
		CItem * pItem = (iItem >= 0 && iItem < MAXITEMS) ? m_pItemList[iItem] : NULL;
		const bool bOn = (iBoxHover == pos);
		COLORREF cBorder = RGB(255, 255, 255);
		int iBorderA = bOn ? 90 : 40;
		if (pItem != NULL) {
			GetItemName(pItem);		// sets m_itemColor from the item's quality
			cBorder = m_itemColor;
			iBorderA = bOn ? 255 : 190;
		}
		R.FillPanel(bx, by, bx + kSlotSz, by + kSlotSz, 6, RGB(0, 0, 0), pItem ? 150 : 110, cBorder, iBorderA);
		if (pItem != NULL) {
			const int iSpr = SPRID_ITEMPACK_PIVOTPOINT + pItem->m_sSprite;
			CSprite * pSpr = m_pSprite[iSpr];
			if (pSpr != NULL && pSpr->m_stBrush != NULL && pItem->m_sSpriteFrame >= 0 && pItem->m_sSpriteFrame < pSpr->m_iTotalFrame) {
				const stBrush & b = pSpr->m_stBrush[pItem->m_sSpriteFrame];
				// centre the icon in the box
				const int dx = bx + kSlotSz / 2 - (b.pvx + b.szx / 2);
				const int dy = by + kSlotSz / 2 - (b.pvy + b.szy / 2);
				const char c = pItem->m_cItemColor;
				if (m_bIsItemDisabled[iItem]) pSpr->PutTransSprite2(dx, dy, pItem->m_sSpriteFrame, m_dwCurTime);
				else if (c == 0) pSpr->PutSpriteFast(dx, dy, pItem->m_sSpriteFrame, m_dwCurTime);
				else pSpr->PutSpriteRGB(dx, dy, pItem->m_sSpriteFrame, m_wR[c] - m_wR[0], m_wG[c] - m_wG[0], m_wB[c] - m_wB[0], m_dwCurTime);
			}
			// upgrade level badge (+N) on the top-right corner
			const int iUp = (int)((pItem->m_dwAttribute & 0xF0000000) >> 28);
			if (iUp > 0) {
				char cUp[8];
				wsprintf(cUp, "+%d", iUp);
				const int w = Width(R, cUp) + 8;
				R.FillPanel(bx + kSlotSz - w + 4, by - 6, bx + kSlotSz + 4, by + 10, 6, kGold, 245);
				R.TextOut(bx + kSlotSz - w + 8, by - 6, cUp, RGB(36, 28, 10));
			}
		}
	}
	if (iBoxHover >= 0) iHoverSlot = iBoxHover;

	// no tooltip on hover: holding the mouse on an item picks it up, and the held item shows its
	// tooltip (UI\ItemTooltip.cpp)
	(void)iHoverSlot;
}
