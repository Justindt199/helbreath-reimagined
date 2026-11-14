// MagicPanel.cpp: the magic list (F7) in the dark panel style of the character and inventory
// windows, with the original layout: circle name at the top, the learned spells and their mana
// cost below it, the circle numerals (I - X) near the bottom, the cast
// chance under them and the Alchemy button at the bottom right.
//
// The click areas are set up in DialogBox.cpp (case 3) exactly as before: 1-9 spells (in list
// order), 10-19 circles, 20 Alchemy, plus 21 for the close button in the title bar.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>

#if LANGUAGE == ENGLISH
#include "..\lan_eng.h"
#elif LANGUAGE == SPANISH
#include "..\lan_spa.h"
#endif

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kMana	= RGB(130, 170, 255);
const COLORREF kShort	= RGB(255, 130, 120);

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(x, y, s, c, iPx);
	r.TextOutSized(x + 1, y, s, c, iPx);
}

void Centered(CRenderer & r, int x0, int x1, int y, const char * s, COLORREF c, int iPx, bool bBold = false)
{
	const int x = (x0 + x1 - r.TextWidthSized(s, iPx)) / 2;
	if (bBold) Bold(r, x, y, s, c, iPx);
	else r.TextOutSized(x, y, s, c, iPx);
}

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

// the circle numerals, centred in the old click areas (DialogBox.cpp case 3, buttons 10-19)
const char * const kNumeral[10] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X" };
const int kNumX0[10] = { 16, 39, 57, 82, 102, 117, 138, 166, 198, 218 };
const int kNumX1[10] = { 38, 56, 81, 101, 116, 137, 165, 197, 217, 239 };

} // namespace

void CGame::MagicButtonRect(int id, RECT * pRc)
{
	if (id == 20) SetRect(pRc, RBTNPOSX, 285, RBTNPOSX + BTNSZX, 285 + BTNSZY);		// Alchemy
	else SetRect(pRc, MAGICWIN_W - 28, 4, MAGICWIN_W - 6, 26);						// close
}

void CGame::DrawMagicPanel(int iChance, int onButton)
{
	CRenderer & R = m_Renderer;
	const short X = m_dialogBoxes[DIALOG_MAGIC].m_X, Y = m_dialogBoxes[DIALOG_MAGIC].m_Y;
	const int W = MAGICWIN_W, H = MAGICWIN_H;
	const bool bTop = m_dialogBoxes[DIALOG_MAGIC].IsTop();
	const int iView = (m_dialogBoxes[DIALOG_MAGIC].sView >= 0 && m_dialogBoxes[DIALOG_MAGIC].sView < 10) ? m_dialogBoxes[DIALOG_MAGIC].sView : 0;
	RECT rc;

	R.FillPanel(X, Y, X + W, Y + H, 10, RGB(14, 16, 20), m_bDialogTrans ? 205 : 255, RGB(150, 128, 82), 150);	// solid unless see-through windows are on (a solid fill is much faster)

	// title bar
	R.TitleOut(X + 14, Y + 8, "MAGIC", kGold, 15);
	MagicButtonRect(21, &rc);
	OffsetRect(&rc, X, Y);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 7, rc.top, "x", bOnClose ? kWhite : kGrey, 17);
	R.FillPanel(X + 1, Y + 30, X + W - 1, Y + 31, 0, RGB(255, 255, 255), 26);

	// circle name
	static const char * const kCircle[10] = { DRAW_DIALOGBOX_MAGIC1, DRAW_DIALOGBOX_MAGIC2, DRAW_DIALOGBOX_MAGIC3,
		DRAW_DIALOGBOX_MAGIC4, DRAW_DIALOGBOX_MAGIC5, DRAW_DIALOGBOX_MAGIC6, DRAW_DIALOGBOX_MAGIC7,
		DRAW_DIALOGBOX_MAGIC8, DRAW_DIALOGBOX_MAGIC9, DRAW_DIALOGBOX_MAGIC10 };
	Centered(R, X, X + W, Y + 46, kCircle[iView], kGold, 13, true);

	// the spell list, in a soft inset
	R.FillPanel(X + 18, Y + 66, X + W - 18, Y + 66 + 9 * 18 + 6, 8, RGB(0, 0, 0), 90, RGB(255, 255, 255), 20);
	const int iPivot = iView * 10;
	int iYloc = 0, j = 0;
	for (int i = 0; i < 9; i++) {
		if (m_cMagicMastery[iPivot + i] == NULL || m_pMagicCfgList[iPivot + i] == NULL) continue;
		j++;
		char cName[64];
		wsprintf(cName, "%s", m_pMagicCfgList[iPivot + i]->m_cName);
		m_Misc.ReplaceString(cName, '-', ' ');
		const int iMana = iGetManaCost(iPivot + i);
		const bool bShort = (iMana > m_iMP);
		const bool bOn = bTop && (onButton == j) && !bShort;
		const int ry = Y + 70 + iYloc;
		if (bOn) R.FillPanel(X + 22, ry, X + W - 22, ry + 18, 5, RGB(232, 192, 112), 36, RGB(232, 192, 112), 140);
		R.TextOut(X + 30, ry + 1, cName, bShort ? RGB(120, 124, 132) : (bOn ? RGB(255, 236, 190) : kWhite));
		char cMana[16];
		wsprintf(cMana, "%d", iMana);
		Bold(R, X + 230 - R.TextWidthSized(cMana, 12), ry + 2, cMana, bShort ? kShort : kMana, 12);
		iYloc += 18;
	}
	if (iYloc == 0) {
		static const char * const kEmpty[5] = { DRAW_DIALOGBOX_MAGIC11, DRAW_DIALOGBOX_MAGIC12, DRAW_DIALOGBOX_MAGIC13,
			DRAW_DIALOGBOX_MAGIC14, DRAW_DIALOGBOX_MAGIC15 };
		for (int k = 0; k < 5; k++) Centered(R, X, X + W, Y + 100 + k * 15, kEmpty[k], kGrey, 12);
	}

	// circle numerals I - X, drawn as text (the old sprites were dark ink for the parchment,
	// tiny and with tan edges, so they looked muddy on the dark panel); the chosen one in gold,
	// the one under the mouse in white, circles with a learned spell brighter than empty ones
	R.FillPanel(X + 12, Y + 242, X + W - 12, Y + 266, 8, RGB(0, 0, 0), 90, RGB(255, 255, 255), 20);
	for (int i = 0; i < 10; i++) {
		bool bAny = false;
		for (int k = 0; k < 10 && !bAny; k++)
			if (m_cMagicMastery[i * 10 + k] != NULL && m_pMagicCfgList[i * 10 + k] != NULL) bAny = true;
		const bool bSel = (i == iView), bOn = bTop && (onButton == 10 + i);
		const int x0 = X + kNumX0[i], x1 = X + kNumX1[i];
		if (bSel) R.FillPanel(x0 + 1, Y + 245, x1 - 1, Y + 263, 4, RGB(232, 192, 112), 40, RGB(232, 192, 112), 170);
		const COLORREF c = bSel ? RGB(250, 214, 128) : (bOn ? kWhite : (bAny ? RGB(190, 194, 202) : RGB(105, 110, 120)));
		Centered(R, x0, x1, Y + 246, kNumeral[i], c, 12, bSel);
	}

	// cast chance
	char cTxt[64];
	wsprintf(cTxt, DRAW_DIALOGBOX_MAGIC16, iChance);
	Centered(R, X, X + W, Y + 270, cTxt, iChance >= 80 ? RGB(150, 220, 150) : (iChance >= 50 ? kGold : kShort), 12);

	// Alchemy
	MagicButtonRect(20, &rc);
	OffsetRect(&rc, X, Y);
	{
		const bool bOn = bTop && (onButton == 20);
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(255, 255, 255), bOn ? 36 : 16, RGB(255, 255, 255), bOn ? 70 : 40);
		Centered(R, rc.left, rc.right, rc.top + 3, "Alchemy", bOn ? kWhite : RGB(220, 224, 230), 12);
	}
}
