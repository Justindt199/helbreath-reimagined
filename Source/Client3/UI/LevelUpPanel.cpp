// LevelUpPanel.cpp: the level-up settings window, drawn in code like the character window.
//
// Points to spend at the top, one row per stat (coloured tag, name, a bar up to the 200 cap,
// current value -> new value, and - / + buttons), and a footer with the Majestic stat change
// and Apply. The rules for spending points are unchanged (DlgBoxClick_LevelUpSettings).
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>
#include <string>

namespace {

const int kPad		= 14;
const int kTitleH	= 34;
const int kRowsY	= 112;		// first stat row
const int kRowH		= 40, kRowGap = 6;
const int kBtn		= 22;		// - / + button size
const int kFootH	= 44;
const int kStatMax	= 200;

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(112, 118, 130);
const COLORREF kGreen	= RGB(126, 227, 138);

struct StatRow { int stat; const char * tag; const char * name; COLORREF bg, fg; };
const StatRow kRows[6] = {
	{ STAT_STR, "STR", "Strength",     RGB(96, 33, 33),  RGB(255, 160, 160) },
	{ STAT_VIT, "VIT", "Vitality",     RGB(31, 78, 42),  RGB(160, 226, 170) },
	{ STAT_DEX, "DEX", "Dexterity",    RGB(80, 74, 22),  RGB(240, 230, 140) },
	{ STAT_INT, "INT", "Intelligence", RGB(31, 47, 92),  RGB(172, 188, 255) },
	{ STAT_MAG, "MAG", "Magic",        RGB(66, 40, 94),  RGB(214, 176, 255) },
	{ STAT_CHR, "CHR", "Charisma",     RGB(96, 62, 18),  RGB(255, 200, 110) },
};

int Width(CRenderer & r, const char * s, int iPx = 0)
{
	if (s == NULL || s[0] == 0) return 0;
	return (iPx > 0) ? r.TextWidthSized(s, iPx) : (int)r.GetTextExtent(s).cx;
}

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx = 0)
{
	if (iPx > 0) { r.TextOutSized(x, y, s, c, iPx); r.TextOutSized(x + 1, y, s, c, iPx); }
	else { r.TextOut(x, y, s, c); r.TextOut(x + 1, y, s, c); }
}

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

} // namespace

// The parts you can click, in screen coordinates. Codes match the old buttons: 1 Apply,
// 2 Majestic change, 3/5/7/9/11/13 + and 4/6/8/10/12/14 - for each stat; 20 close.
void CGame::LevelUpRects(RECT * pClose, RECT * pApply, RECT * pMajestic, RECT * pPlus, RECT * pMinus)
{
	const int X = m_dialogBoxes[DIALOG_LEVELUPSETTING].m_X, Y = m_dialogBoxes[DIALOG_LEVELUPSETTING].m_Y;
	const int W = LEVELUPWIN_W, H = LEVELUPWIN_H;
	if (pClose) SetRect(pClose, X + W - 32, Y + 6, X + W - 8, Y + 30);
	const int by0 = Y + H - kFootH + 9, by1 = by0 + 26;
	if (pApply) SetRect(pApply, X + W - kPad - 96, by0, X + W - kPad, by1);
	if (pMajestic) SetRect(pMajestic, X + kPad, by0, X + kPad + 170, by1);
	for (int i = 0; i < 6; i++) {
		const int ry = Y + kRowsY + i * (kRowH + kRowGap), cy = ry + (kRowH - kBtn) / 2;
		const int px1 = X + W - kPad - 8, px0 = px1 - kBtn, mx1 = px0 - 6, mx0 = mx1 - kBtn;
		if (pPlus) SetRect(&pPlus[i], px0, cy, px1, cy + kBtn);
		if (pMinus) SetRect(&pMinus[i], mx0, cy, mx1, cy + kBtn);
	}
}

int CGame::LevelUpButtonAt(short msX, short msY)
{
	RECT rcClose, rcApply, rcMaj, rcPlus[6], rcMinus[6];
	LevelUpRects(&rcClose, &rcApply, &rcMaj, rcPlus, rcMinus);
	if (InRect(rcClose, msX, msY)) return 20;
	if (InRect(rcApply, msX, msY)) return 1;
	if (InRect(rcMaj, msX, msY)) return 2;
	for (int i = 0; i < 6; i++) {
		if (InRect(rcPlus[i], msX, msY)) return 3 + i * 2;
		if (InRect(rcMinus[i], msX, msY)) return 4 + i * 2;
	}
	return 0;
}

void CGame::DrawDialogBox_LevelUpSetting()
{
	CRenderer & R = m_Renderer;
	const int X = m_dialogBoxes[DIALOG_LEVELUPSETTING].m_X, Y = m_dialogBoxes[DIALOG_LEVELUPSETTING].m_Y;
	const int W = LEVELUPWIN_W, H = LEVELUPWIN_H;
	const bool bTop = m_dialogBoxes[DIALOG_LEVELUPSETTING].IsTop();
	const int iOn = bTop ? LevelUpButtonAt(ms::X, ms::Y) : 0;
	char cBuf[64];

	RECT rcClose, rcApply, rcMaj, rcPlus[6], rcMinus[6];
	LevelUpRects(&rcClose, &rcApply, &rcMaj, rcPlus, rcMinus);

	R.FillPanel(X, Y, X + W, Y + H, 10, RGB(14, 16, 20), m_bDialogTrans ? 205 : 255, RGB(150, 128, 82), 150);	// solid unless see-through windows are on (a solid fill is much faster)

	// title bar
	R.TitleOut(X + kPad, Y + 9, "LEVEL-UP SETTINGS", kGold, 16);
	if (iOn == 20) R.FillPanel(rcClose.left, rcClose.top, rcClose.right, rcClose.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rcClose.left + 8, rcClose.top + 1, "x", iOn == 20 ? kWhite : kGrey, 18);
	R.FillPanel(X + 1, Y + kTitleH, X + W - 1, Y + kTitleH + 1, 0, RGB(255, 255, 255), 26);

	// points to spend
	int iPending = 0;
	for (int i = 0; i < 6; i++) iPending += m_luStat[kRows[i].stat];
	{
		const int py = Y + 44;
		R.FillPanel(X + kPad, py, X + W - kPad, py + 40, 8, RGB(255, 255, 255), 12, RGB(255, 255, 255), 24);
		R.TextOut(X + kPad + 12, py + 4, "Points to spend", kGrey);
		R.TextOutSized(X + kPad + 12, py + 21, iPending > 0 ? "Press Apply to keep your changes" : "Click + to add a point to a stat",
			iPending > 0 ? RGB(214, 200, 160) : kDim, 12);
		wsprintf(cBuf, "%d", m_iLU_Point);
		const int nw = Width(R, cBuf, 26);
		Bold(R, X + W - kPad - 14 - nw, py + 3, cBuf, m_iLU_Point > 0 ? kGold : kDim, 26);
	}
	R.TextOutSized(X + kPad, Y + 92, "Ctrl+click to add or take back 5 points at a time", kDim, 12);

	// one row per stat
	for (int i = 0; i < 6; i++) {
		const StatRow & s = kRows[i];
		const int rx0 = X + kPad, rx1 = X + W - kPad, ry = Y + kRowsY + i * (kRowH + kRowGap);
		const int iBase = m_stat[s.stat] - m_angelStat[s.stat];		// without the angel bonus
		const int iNew = iBase + m_luStat[s.stat];
		const bool bChanged = m_luStat[s.stat] > 0;
		R.FillPanel(rx0, ry, rx1, ry + kRowH, 7, RGB(255, 255, 255), bChanged ? 20 : 12,
			bChanged ? RGB(126, 227, 138) : RGB(255, 255, 255), bChanged ? 90 : 24);

		// tag and name
		R.FillPanel(rx0 + 8, ry + 11, rx0 + 44, ry + 29, 4, s.bg, 255);
		R.TextOut(rx0 + 26 - Width(R, s.tag) / 2, ry + 12, s.tag, s.fg);
		R.TextOut(rx0 + 54, ry + 4, s.name, kWhite);

		// bar up to the cap: current in the tag colour, the points being added in green
		const int bx0 = rx0 + 54, bx1 = rcMinus[i].left - 96, bw = (std::max)(20, bx1 - bx0), by = ry + 25;
		R.FillPanel(bx0, by, bx0 + bw, by + 6, 3, RGB(255, 255, 255), 26);
		const int wBase = bw * (std::min)(iBase, kStatMax) / kStatMax;
		const int wNew = bw * (std::min)(iNew, kStatMax) / kStatMax;
		if (wNew > wBase) R.FillPanel(bx0, by, bx0 + wNew, by + 6, 3, kGreen, 230);
		if (wBase >= 2) R.FillPanel(bx0, by, bx0 + wBase, by + 6, 3, s.fg, 220);

		// current -> new
		const int vx1 = rcMinus[i].left - 10;
		if (bChanged) {
			wsprintf(cBuf, "%d", iNew);
			const int w1 = Width(R, cBuf, 16);
			Bold(R, vx1 - w1, ry + 11, cBuf, kGreen, 16);
			char cOld[16];
			wsprintf(cOld, "%d  >", iBase);
			R.TextOut(vx1 - w1 - 8 - Width(R, cOld), ry + 12, cOld, kDim);
		}
		else {
			wsprintf(cBuf, "%d", iBase);
			Bold(R, vx1 - Width(R, cBuf, 16), ry + 11, cBuf, kWhite, 16);
		}

		// - and +
		const bool bCanAdd = m_iLU_Point > 0 && iNew < kStatMax;
		const bool bCanTake = m_luStat[s.stat] > 0;
		const struct { const RECT * rc; bool on; bool hover; const char * label; } btn[2] = {
			{ &rcMinus[i], bCanTake, iOn == 4 + i * 2, "-" }, { &rcPlus[i], bCanAdd, iOn == 3 + i * 2, "+" } };
		for (int k = 0; k < 2; k++) {
			const RECT & rc = *btn[k].rc;
			if (btn[k].on) {
				const bool bPlus = (k == 1);
				R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5,
					bPlus ? kGold : RGB(255, 255, 255), bPlus ? (btn[k].hover ? 255 : 215) : (btn[k].hover ? 50 : 26),
					bPlus ? kGold : RGB(255, 255, 255), bPlus ? 255 : 70);
				Bold(R, rc.left + (kBtn - Width(R, btn[k].label, 18)) / 2, rc.top, btn[k].label, bPlus ? RGB(36, 28, 10) : kWhite, 18);
			}
			else {
				R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 6, RGB(255, 255, 255), 22);
				R.TextOutSized(rc.left + (kBtn - Width(R, btn[k].label, 18)) / 2, rc.top, btn[k].label, RGB(80, 86, 96), 18);
			}
		}
	}

	// footer: Majestic stat change and Apply
	R.FillPanel(X + 1, Y + H - kFootH, X + W - 1, Y + H - kFootH + 1, 0, RGB(255, 255, 255), 26);
	{
		const bool bMaj = (m_iGizonItemUpgradeLeft > 0) && (m_iLU_Point <= 0) && iPending == 0;
		wsprintf(cBuf, "Majestic change (%d)", m_iGizonItemUpgradeLeft);
		const RECT & rc = rcMaj;
		const bool bHover = bMaj && iOn == 2;
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(255, 255, 255), bMaj ? (bHover ? 36 : 16) : 6,
			RGB(255, 255, 255), bMaj ? (bHover ? 70 : 42) : 20);
		R.TextOut(rc.left + (rc.right - rc.left - Width(R, cBuf)) / 2, rc.top + 5, cBuf, bMaj ? RGB(220, 224, 230) : RGB(80, 86, 96));
	}
	{
		const char * pLabel = (iPending > 0) ? "Apply" : "Done";
		const RECT & rc = rcApply;
		const bool bHover = iOn == 1;
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, bHover ? RGB(248, 208, 120) : kGold, 245);
		R.TitleOut(rc.left + (rc.right - rc.left - R.TitleWidth(pLabel)) / 2, rc.top + 5, pLabel, RGB(36, 28, 10));
	}
}
