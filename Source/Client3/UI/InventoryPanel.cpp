// InventoryPanel.cpp: the inventory (F6) window, drawn in code in the same style as the character
// window, instead of the leather bag picture (GameDialog.pak sheet 7).
//
// The bag keeps free placement: items are drawn by DrawDialogBox_Inventory at
// (window + 32 + item x, window + 44 + item y) exactly as before, so every item stays where the
// player put it and picking up, dropping and double-clicking are unchanged. This file draws
//   - the window, a title bar with the item count and a close button, and the bag inset
//   - a small count pill on stacks and a gold "+N" badge on upgraded items
//   - the Upgrade and Manufacture buttons
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>

namespace {

const int kPad	 = 8;
const int kTitleH = 26;
const int kBagX0 = 8, kBagY0 = 28, kBagX1 = 218, kBagY1 = 174;		// the bag inset
const int kBtnY0 = 180, kBtnY1 = 202;								// footer buttons

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(x, y, s, c, iPx);
	r.TextOutSized(x + 1, y, s, c, iPx);
}

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

void CloseRect(short X, short Y, RECT & rc)
{
	SetRect(&rc, X + CGame::INVWIN_W - 28, Y + 3, X + CGame::INVWIN_W - 6, Y + 24);
}

} // namespace

// Button rects relative to the window, for CDialogBox::AddButton (DialogBox.cpp case 2):
// 1 Upgrade, 2 Manufacture, 3 close.
void CGame::InvButtonRect(int id, RECT * pRc)
{
	const int half = (kBagX1 - kBagX0 - 8) / 2;
	switch (id) {
	case 1: SetRect(pRc, kBagX0, kBtnY0, kBagX0 + half, kBtnY1); break;
	case 2: SetRect(pRc, kBagX1 - half, kBtnY0, kBagX1, kBtnY1); break;
	default: SetRect(pRc, INVWIN_W - 28, 3, INVWIN_W - 6, 24); break;
	}
}

void CGame::DrawInvPanelBack()
{
	CRenderer & R = m_Renderer;
	const short X = m_dialogBoxes[DIALOG_INVENTORY].m_X, Y = m_dialogBoxes[DIALOG_INVENTORY].m_Y;
	const int W = INVWIN_W, H = INVWIN_H;
	const bool bTop = m_dialogBoxes[DIALOG_INVENTORY].IsTop();

	R.StonePanel(X, Y, X + W, Y + H);	// VALORIA stone (CRenderer::StoneBox)

	// title bar: name, how many items, close
	R.TitleOut(X + kPad + 4, Y + 6, "INVENTORY", kGold, 14);
	int n = 0;
	for (int i = 0; i < MAXITEMS; i++) if (m_pItemList[i] != NULL) n++;
	char cCount[24];
	wsprintf(cCount, "%d / %d", n, MAXITEMS);
	const int tx = X + kPad + 4 + R.TitleWidth("INVENTORY", 14) + 10;
	const int cw = R.TextWidthSized(cCount, 10) + 12;
	R.FillPanel(tx, Y + 6, tx + cw, Y + 21, 7, RGB(255, 255, 255), 22);
	R.TextOutSized(tx + 6, Y + 7, cCount, n >= MAXITEMS ? RGB(255, 150, 140) : RGB(200, 205, 213), 10);

	RECT rc;
	CloseRect(X, Y, rc);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 7, rc.top, "x", bOnClose ? kWhite : kGrey, 16);
	R.FillPanel(X + 1, Y + kTitleH, X + W - 1, Y + kTitleH + 1, 0, RGB(255, 255, 255), 26);

	// the bag: a dark inset with a faint warm centre
	R.Slate(X + kBagX0, Y + kBagY0, X + kBagX1, Y + kBagY1);
	R.FillPanel(X + kBagX0 + 30, Y + kBagY0 + 18, X + kBagX1 - 30, Y + kBagY1 - 18, 50, RGB(232, 170, 80), 10);

}

// The count pill on stacks and the "+N" badge on upgraded items, for the bag item drawn at
// (iX, iY) (its pivot point).
void CGame::DrawInvItemBadges(int id, int iX, int iY)
{
	if (id < 0 || id >= MAXITEMS || m_pItemList[id] == NULL) return;
	CItem * p = m_pItemList[id];
	CSprite * pSpr = m_pSprite[SPRID_ITEMPACK_PIVOTPOINT + p->m_sSprite];
	if (pSpr == NULL) return;
	pSpr->_GetSpriteRect(iX, iY, p->m_sSpriteFrame);
	const RECT b = pSpr->m_rcBound;
	if (b.right <= b.left || b.bottom <= b.top) return;
	CRenderer & R = m_Renderer;

	if (p->m_cItemType == ITEMTYPE_CONSUME || p->m_cItemType == ITEMTYPE_ARROW) {
		DisplayCommaNumber_G_cTxt((int)p->m_dwCount);
		const int w = R.TextWidthSized(G_cTxt, 9) + 8;
		const int x1 = (std::max)((int)b.right + 2, (int)b.left + w), y1 = b.bottom + 2;
		R.Slate(x1 - w, y1 - 13, x1, y1);
		R.TextOutSized(x1 - w + 4, y1 - 13, G_cTxt, kWhite, 9);
	}

	const int iUp = (int)GetNibble(p->m_dwAttribute, 7);
	if (iUp > 0) {
		char cUp[8];
		wsprintf(cUp, "+%d", iUp);
		const int w = R.TextWidthSized(cUp, 9) + 8;
		const int x1 = b.right + 2, y0 = (std::max)((int)b.top - 2, (int)m_dialogBoxes[DIALOG_INVENTORY].m_Y + kBagY0 + 2);
		R.FillPanel(x1 - w, y0, x1, y0 + 13, 6, RGB(232, 192, 112), 255);
		Bold(R, x1 - w + 4, y0, cUp, RGB(30, 22, 8), 9);
	}
}

void CGame::DrawInvPanelFront()
{
	CRenderer & R = m_Renderer;
	const short X = m_dialogBoxes[DIALOG_INVENTORY].m_X, Y = m_dialogBoxes[DIALOG_INVENTORY].m_Y;
	const bool bTop = m_dialogBoxes[DIALOG_INVENTORY].IsTop();
	static const char * const kLabel[2] = { "Upgrade", "Manufacture" };
	for (int i = 0; i < 2; i++) {
		RECT rc;
		InvButtonRect(i + 1, &rc);
		OffsetRect(&rc, X, Y);
		const bool bOn = bTop && InRect(rc, ms::X, ms::Y);
		R.StoneButton(rc.left, rc.top, rc.right, rc.bottom, bOn ? 1 : 0);
		R.TextOutSized((rc.left + rc.right - R.TextWidthSized(kLabel[i], 12)) / 2, rc.top + 4, kLabel[i], bOn ? kWhite : RGB(220, 224, 230), 12);
	}
}
