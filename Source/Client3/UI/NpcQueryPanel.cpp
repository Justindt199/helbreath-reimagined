// NpcQueryPanel.cpp: the small window that opens when you click an NPC (dialog 20,
// "NpcActionQuery"): the NPC's name and its choices (Trade / Talk, Offer, Withdraw, Learn, the
// Blacksmith, Shop Keeper and City Hall menus), and the prompts that use the same window (give
// an item to a player, sell / repair an item, deposit in the warehouse, repair all).
//
// Drawn in code in the style of the other windows instead of on the parchment picture: a gold
// header with the NPC's name and a badge for the kind of NPC, and the choices as buttons in two
// columns. The choices, their order and what they do are unchanged; DlgBoxClick_NpcActionQuery
// (Game.cpp) still handles them. The window is as tall as its choices need (NpcQueryLayout,
// DialogBox.cpp) and there is a close button.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>
#include <string>

#if LANGUAGE == ENGLISH
#include "..\lan_eng.h"
#elif LANGUAGE == SPANISH
#include "..\lan_spa.h"
#endif

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kText	= RGB(214, 218, 226);
const COLORREF kGrey	= RGB(150, 156, 168);

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

// The badge for a kind of NPC: an icon and two colours.
enum NpcIcon { ICON_NONE, ICON_SHOP, ICON_MAGIC, ICON_WAREHOUSE, ICON_BLACKSMITH, ICON_CITYHALL, ICON_FLAG, ICON_SHIELD, ICON_SPARKLE, ICON_TOWNSPERSON };
struct NpcKind { NpcIcon icon; const char * label; COLORREF bg, fg; };

NpcKind KindOf(int iType)
{
	switch (iType) {
	case 15: return { ICON_SHOP,       "SHOP",       RGB(18, 51, 38),  RGB(160, 240, 200) };
	case 19: return { ICON_MAGIC,      "MAGIC",      RGB(42, 30, 72),  RGB(200, 176, 255) };
	case 20: return { ICON_WAREHOUSE,  "WAREHOUSE",  RGB(28, 42, 68),  RGB(168, 200, 255) };
	case 24: return { ICON_BLACKSMITH, "BLACKSMITH", RGB(58, 36, 16),  RGB(255, 190, 112) };
	case 25: return { ICON_CITYHALL,   "CITY HALL",  RGB(42, 33, 64),  RGB(214, 194, 255) };
	case 26: return { ICON_FLAG,       "GUILD HALL", RGB(22, 52, 34),  RGB(170, 232, 178) };
	case 90: return { ICON_FLAG,       "HELDENIAN",  RGB(70, 26, 22),  RGB(255, 170, 150) };
	case 21: return { ICON_SHIELD,     "GUARD",      RGB(44, 46, 52),  RGB(206, 210, 220) };
	case 32: return { ICON_SPARKLE,    "UNICORN",    RGB(30, 48, 60),  RGB(190, 230, 255) };
	case 67: case 68: case 69:
	         return { ICON_TOWNSPERSON, "TOWNSPERSON", RGB(48, 42, 34), RGB(232, 218, 190) };
	default: return { ICON_NONE,       "",           RGB(44, 46, 52),  RGB(206, 210, 220) };
	}
}

// The icon inside the badge, centred on (cx, cy), drawn from panels and stars so it stays sharp
// at twice the size in HD. Sizes are in game pixels.
void DrawNpcIcon(CRenderer & R, NpcIcon icon, int cx, int cy, COLORREF fg, COLORREF bg)
{
	auto Box = [&](int x0, int y0, int x1, int y1, int r, COLORREF c) { R.FillPanel(cx + x0, cy + y0, cx + x1, cy + y1, r, c, 255); };
	switch (icon) {
	case ICON_BLACKSMITH:	// anvil with a hammer above it
		Box(-8, -3, 7, 0, 1, fg); Box(-11, -3, -7, -1, 1, fg); Box(-3, 0, 3, 4, 0, fg); Box(-6, 4, 6, 7, 1, fg);
		Box(1, -10, 8, -7, 1, fg); Box(4, -7, 6, -4, 0, fg);
		break;
	case ICON_SHOP:			// potion flask
		Box(-6, -2, 6, 9, 6, fg); Box(-2, -6, 2, -1, 0, fg); Box(-3, -9, 3, -6, 1, RGB(150, 110, 70)); Box(-3, 1, -1, 4, 1, RGB(255, 255, 255));
		break;
	case ICON_WAREHOUSE:	// chest
		Box(-9, -7, 9, -2, 4, fg); Box(-9, -1, 9, 7, 1, fg); Box(-9, -2, 9, -1, 0, bg); Box(-2, -3, 2, 2, 1, bg); Box(-1, -2, 1, 1, 0, RGB(255, 214, 120));
		break;
	case ICON_MAGIC:		// star
		R.FillStar((float)cx, (float)cy, 9.f, fg, 255);
		break;
	case ICON_CITYHALL:		// hall with columns
		Box(-3, -10, 3, -8, 0, fg); Box(-6, -8, 6, -6, 0, fg); Box(-9, -6, 9, -4, 0, fg);
		Box(-7, -3, -5, 5, 0, fg); Box(-1, -3, 1, 5, 0, fg); Box(5, -3, 7, 5, 0, fg);
		Box(-9, 6, 9, 8, 0, fg);
		break;
	case ICON_FLAG:			// banner on a pole
		Box(-7, -10, -5, 10, 1, fg); Box(-5, -9, 8, 0, 1, fg); Box(-9, 9, -3, 11, 1, fg);
		break;
	case ICON_SHIELD:		// shield with a cross
		R.FillPanel(cx - 7, cy - 9, cx + 7, cy + 2, 2, fg, 255, fg, 0, 8, cx);
		Box(-1, -7, 1, 6, 0, bg); Box(-5, -4, 5, -2, 0, bg);
		break;
	case ICON_SPARKLE:		// two stars
		R.FillStar((float)(cx - 2), (float)(cy + 1), 7.f, fg, 255);
		R.FillStar((float)(cx + 6), (float)(cy - 6), 3.5f, fg, 255);
		break;
	case ICON_TOWNSPERSON:	// speech bubble
		R.FillPanel(cx - 9, cy - 8, cx + 9, cy + 4, 5, fg, 255, fg, 0, 5, cx - 4);
		Box(-6, -3, -3, 0, 2, bg); Box(-1, -3, 2, 0, 2, bg); Box(4, -3, 7, 0, 2, bg);
		break;
	default:
		R.FillPanel(cx - 4, cy - 4, cx + 4, cy + 4, 4, fg, 255);
		break;
	}
}

const char * NpcName(int iType)
{
	switch (iType) {
	case 15: return NPC_NAME_SHOP_KEEPER;
	case 19: return NPC_NAME_MAGICIAN;
	case 20: return NPC_NAME_WAREHOUSE_KEEPER;
	case 24: return NPC_NAME_BLACKSMITH_KEEPER;
	case 25: return NPC_NAME_CITYHALL_OFFICER;
	case 26: return NPC_NAME_GUILDHALL_OFFICER;
	case 90: return NPC_NAME_CMDHALL_OFFICER;
	case 21: return NPC_NAME_GUARD;
	case 32: return NPC_NAME_UNICORN;
	case 67: return NPC_NAME_MCGAFFIN;
	case 68: return NPC_NAME_PERRY;
	case 69: return NPC_NAME_DEVLIN;
	default: return "";
	}
}

} // namespace

// The label of choice id (1..5) in the current mode, or NULL when that choice isn't shown
// (exactly the cases where the parchment version left it out).
const char * CGame::NpcQueryLabel(int id)
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_NPCACTIONQUERY];
	const bool bTalk = (m_bIsDialogEnabled[DIALOG_NPCTALK] == FALSE);
	switch (dlg.GetMode()) {
	case 0:
		if (id == 1) {
			switch (dlg.sV3) {
			case 25: return DRAW_DIALOGBOX_NPCACTION_QUERY13;	// Offer
			case 20: return DRAW_DIALOGBOX_NPCACTION_QUERY17;	// Withdraw
			case 19: return DRAW_DIALOGBOX_NPCACTION_QUERY19;	// Learn
			default: return DRAW_DIALOGBOX_NPCACTION_QUERY21;	// Trade
			}
		}
		if (id == 2) return bTalk ? DRAW_DIALOGBOX_NPCACTION_QUERY25 : NULL;	// Talk
		return NULL;
	case 1:
		if (id == 1) return DRAW_DIALOGBOX_NPCACTION_QUERY30;	// Give
		if (id == 2) return DRAW_DIALOGBOX_NPCACTION_QUERY34;	// Exchange
		return NULL;
	case 2:
		if (id == 1) return DRAW_DIALOGBOX_NPCACTION_QUERY39;	// Sell
		if (id == 2) {
			const CItem * p = (dlg.sV1 >= 0 && dlg.sV1 < MAXITEMS) ? m_pItemList[dlg.sV1] : NULL;
			if (p == NULL || p->m_cItemType == ITEMTYPE_CONSUME || p->m_cItemType == ITEMTYPE_ARROW) return NULL;
			return DRAW_DIALOGBOX_NPCACTION_QUERY43;			// Repair
		}
		return NULL;
	case 3:
	case 10:
		return (id == 1) ? DRAW_DIALOGBOX_NPCACTION_QUERY48 : NULL;	// Deposit
	case 4:
		return (id == 1 && bTalk) ? DRAW_DIALOGBOX_NPCACTION_QUERY25 : NULL;	// Talk
	case 5:
		if (id == 1) return DRAW_DIALOGBOX_NPCACTION_QUERY21;	// Trade
		if (id == 2) return DRAW_DIALOGBOX_NPCACTION_QUERY39;	// Sell
		if (id == 3) return bTalk ? DRAW_DIALOGBOX_NPCACTION_QUERY25 : NULL;	// Talk
		return NULL;
	case 6:
		return (id == 1) ? DRAW_DIALOGBOX_NPCACTION_QUERY21 : NULL;	// Trade
	case 7: {
		static const char * const k[5] = { DRAW_BLACKSMITH1, DRAW_BLACKSMITH2, DRAW_BLACKSMITH3, DRAW_BLACKSMITH4, DRAW_BLACKSMITH5 };
		return (id >= 1 && id <= 5) ? k[id - 1] : NULL;
	}
	case 8: {
		static const char * const k[4] = { DRAW_SHOP1, DRAW_SHOP2, DRAW_SHOP3, DRAW_SHOP4 };
		return (id >= 1 && id <= 4) ? k[id - 1] : NULL;
	}
	case 9: {
		static const char * const k[4] = { DRAW_CITYHALL1, DRAW_CITYHALL4, DRAW_CITYHALL2, DRAW_CITYHALL3 };	// Offer, Mailbox, Buy Weapons, Talk
		return (id >= 1 && id <= 4) ? k[id - 1] : NULL;
	}
	case 11:
		return (id == 1) ? DRAW_DIALOGBOX_NPCACTION_QUERY43 : NULL;	// Repair
	}
	return NULL;
}

void CGame::DrawDialogBox_NpcActionQuery()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_NPCACTIONQUERY];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int iMode = dlg.GetMode();
	const int W = NPCQUERY_W, H = NpcQueryLayout(iMode, 0, NULL);
	const bool bTop = dlg.IsTop();
	const char onButton = dlg.OnButton();
	RECT rc;

	R.FillPanel(X, Y, X + W, Y + H, 10, RGB(14, 16, 20), m_bDialogTrans ? 205 : 255, RGB(150, 128, 82), 150);	// solid unless see-through windows are on (a solid fill is much faster)

	// header: badge, name, kind
	const char * pTitle = NULL;
	int iKind = dlg.sV3;
	switch (iMode) {
	case 0: case 4: case 5: pTitle = NpcName(dlg.sV3); break;
	case 6: pTitle = DRAW_DIALOGBOX_NPCACTION_QUERY49; iKind = 90; break;
	case 7: pTitle = DRAW_BLACKSMITH; iKind = 24; break;
	case 8: pTitle = DRAW_SHOP; iKind = 15; break;
	case 9: pTitle = DRAW_CITYHALL; iKind = 25; break;
	}
	if (NpcQueryHasTitle(iMode)) {
		const NpcKind k = KindOf(iKind);
		R.FillPanel(X + 10, Y + 8, X + 36, Y + 34, 6, k.bg, 255, RGB(255, 255, 255), 40);
		DrawNpcIcon(R, k.icon, X + 23, Y + 21, k.fg, k.bg);
		R.TitleOut(X + 44, Y + 13, pTitle ? pTitle : "", kGold, 14);
		R.FillPanel(X + 10, Y + 40, X + W - 10, Y + 41, 0, RGB(255, 255, 255), 26);
	}

	// the prompt text (give / sell or repair / deposit / repair all)
	char cA[128] = {}, cB[128] = {};
	bool bItemLine = false;
	switch (iMode) {
	case 1: case 2: case 3: case 10: {
		char cStr1[64] = {}, cStr2[64] = {}, cStr3[64] = {};
		if (dlg.sV1 >= 0 && dlg.sV1 < MAXITEMS && m_pItemList[dlg.sV1] != NULL)
			GetItemName(m_pItemList[dlg.sV1]->m_cName, m_pItemList[dlg.sV1]->m_dwAttribute, cStr1, cStr2, cStr3);
		wsprintf(cA, DRAW_DIALOGBOX_NPCACTION_QUERY29, dlg.sV3, cStr1);	// "%d %s to"
		wsprintf(cB, DRAW_DIALOGBOX_NPCACTION_QUERY29_1, dlg.cStr);		// "%s"
		bItemLine = true;
		break;
	}
	case 11:
		strcpy(cA, DRAW_DIALOGBOX_NPCACTION_QUERY50);					// "Would you like me to repair"
		wsprintf(cB, DRAW_DIALOGBOX_NPCACTION_QUERY51, dlg.sV2);		// "your items for %d gold?"
		break;
	}
	if (cA[0]) {
		const int ty = Y + NpcQueryTextTop(iMode);
		R.FillPanel(X + 10, ty - 4, X + W - 10, ty + 2 * 16 + 4, 7, RGB(0, 0, 0), 110);
		R.TextOutSized(X + 18, ty, cA, bItemLine ? kGold : kText, 12);
		R.TextOutSized(X + 18, ty + 16, cB, kText, 12);
	}

	// the choices
	for (int id = 1; id <= 5; id++) {
		const char * pLabel = NpcQueryLabel(id);
		if (pLabel == NULL) continue;
		NpcQueryLayout(iMode, id, &rc);
		if (rc.right <= rc.left) continue;
		OffsetRect(&rc, X, Y);
		const bool bOn = bTop && (onButton == id);
		if (bOn) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(232, 192, 112), 70, kGold, 255);
		else if (id == 1) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(232, 188, 92), 26, kGold, 140);
		else R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(255, 255, 255), 12, RGB(255, 255, 255), 45);
		const bool bWide = (rc.right - rc.left) > 140;		// a single choice: label in the middle
		int iPx = 12, tw = R.TextWidthSized(pLabel, iPx);
		while (iPx > 9 && tw > (rc.right - rc.left) - 16) tw = R.TextWidthSized(pLabel, --iPx);	// long labels a little smaller
		const int tx = bWide ? (rc.left + rc.right - tw) / 2 : rc.left + 9;
		R.TextOutSized(tx, rc.top + 4 + (12 - iPx) / 2, pLabel, bOn ? kWhite : kText, iPx);
	}

	// close
	NpcQueryLayout(iMode, NPCQUERY_CLOSE, &rc);
	OffsetRect(&rc, X, Y);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 6, rc.top, "x", bOnClose ? kWhite : kGrey, 15);
}
