// SkillPanel.cpp: the skills list (F8) in the dark panel style of the other windows, with the
// original layout: 17 rows of skill name and mastery %, the "skill to lower" box at the right of
// each row, and the scroll bar on the right edge. Click areas are as before (DialogBox.cpp case
// 15: 1-17 skills, 18-34 lower boxes, 35 scroll bar) plus 40 for the close button.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

} // namespace

void CGame::SkillCloseRect(RECT * pRc)
{
	SetRect(pRc, SKILLWIN_W - 26, 2, SKILLWIN_W - 4, 22);
}

// Helbreath's skills the window lists: all but Alchemy, Manufacturing and Crafting, which VALORIA's life
// skills replaced (listed below them).
CSkill * CGame::ShownSkill(int i) const
{
	if (i < 0 || i >= MAXSKILLTYPE || i == 12 || i == 13 || i == 15) return NULL;
	return m_pSkillCfgList[i];
}

void CGame::DrawSkillPanel(int onButton)
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_SKILL];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int W = SKILLWIN_W, H = SKILLWIN_H;
	const bool bTop = dlg.IsTop();
	RECT rc;

	R.StonePanel(X, Y, X + W, Y + H);	// VALORIA stone (CRenderer::StoneBox)

	// title bar
	R.TitleOut(X + 14, Y + 5, "SKILLS", kGold, 14);
	SkillCloseRect(&rc);
	OffsetRect(&rc, X, Y);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 7, rc.top - 1, "x", bOnClose ? kWhite : kGrey, 16);
	R.FillPanel(X + 1, Y + 25, X + W - 1, Y + 26, 0, RGB(255, 255, 255), 26);

	// the list, in a soft inset
	R.Slate(X + 20, Y + 38, X + 232, Y + 45 + 17 * 15 + 6);
	if (dlg.GetMode() != 0) return;

	for (int line = 0, skillIndex = 0; line < 17; skillIndex++) {
		if (line >= MAXSKILLTYPE || skillIndex + dlg.sView >= MAXSKILLTYPE) break;
		CSkill * p = ShownSkill(skillIndex + dlg.sView);
		if (p == NULL) continue;
		char cName[64], cPct[16];
		wsprintf(cName, "%s", p->m_cName);
		m_Misc.ReplaceString(cName, '-', ' ');
		wsprintf(cPct, "%d%%", p->m_iLevel);

		const bool bUsable = p->m_bIsUseable && p->m_iLevel != 0;
		const bool bOn = bTop && bUsable && (onButton == line + 1);
		const int ry = Y + 45 + line * 15;
		if (bOn) R.FillPanel(X + 24, ry - 1, X + 212, ry + 15, 4, RGB(232, 192, 112), 36, RGB(232, 192, 112), 140);
		const COLORREF c = bOn ? RGB(255, 236, 190) : (bUsable ? RGB(160, 196, 255) : (p->m_iLevel != 0 ? RGB(214, 218, 226) : RGB(120, 124, 132)));
		R.TextOut(X + 30, ry, cName, c);
		R.TextOut(X + 207 - R.GetTextExtent(cPct).cx, ry, cPct, c);

		// the "lower this skill" box
		const int bx = X + 215, by = ry + 2;
		const bool bDown = (m_iDownSkillIndex == skillIndex + dlg.sView);
		const bool bOnBox = bTop && (onButton == line + 18);
		R.FillPanel(bx, by, bx + 10, by + 10, 2, RGB(0, 0, 0), 120, bDown ? RGB(255, 130, 110) : RGB(255, 255, 255), bDown ? 230 : (bOnBox ? 140 : 60));
		if (bDown) {	// a small down arrow
			R.FillPanel(bx + 2, by + 3, bx + 8, by + 4, 0, RGB(255, 140, 120), 255);
			R.FillPanel(bx + 3, by + 5, bx + 7, by + 6, 0, RGB(255, 140, 120), 255);
			R.FillPanel(bx + 4, by + 7, bx + 6, by + 8, 0, RGB(255, 140, 120), 255);
		}
		line++;
	}

	int iTotalLines = 0;
	for (int i = 0; i < MAXSKILLTYPE; i++) if (ShownSkill(i) != NULL) iTotalLines++;
	const Point pointerLoc = dlg.HandleScroll(17, iTotalLines);
	if (iTotalLines > 17) {
		// track and thumb, where the old scroll bar was
		R.FillPanel(X + 237, Y + 32, X + 245, Y + 316, 4, RGB(255, 255, 255), 18, RGB(255, 255, 255), 30);
		R.FillPanel(pointerLoc.x - 5, pointerLoc.y - 5, pointerLoc.x + 8, pointerLoc.y + 9, 5, RGB(232, 192, 112), 230, RGB(255, 236, 190), 120);
	}

	// VALORIA's life skills (Shared\ValoriaEnchant.h): level and progress to the next
	R.TitleOut(X + 22, Y + 316, "LIFE SKILLS", kGold, 12);
	R.Slate(X + 20, Y + 334, X + 232, Y + 334 + 5 * 20 + 6);
	for (int s = 0; s <= Valoria::LIFE_COUNT; s++) {
		const bool bEnch = (s == Valoria::LIFE_COUNT);	// Enchanting: its own XP and levels
		const char * pName = bEnch ? "Enchanting" : Valoria::LifeName(s);
		int lv, maxLv;
		double f;
		if (bEnch) {
			const unsigned int xp = m_enchant.xp;
			lv = Valoria::EnchantLevel(xp); maxLv = Valoria::ENCHANT_MAX;
			const unsigned int a = Valoria::EnchantXPTotal(lv), b = Valoria::EnchantXPTotal(lv + 1);
			f = (lv >= maxLv || b <= a) ? 1.0 : (double)(xp - a) / (double)(b - a);
		}
		else {
			const unsigned int xp = m_enchant.lifeXp[s];
			lv = Valoria::LifeLevel(xp); maxLv = Valoria::LIFE_MAX;
			const unsigned int a = Valoria::LifeXPTotal(lv), b = Valoria::LifeXPTotal(lv + 1);
			f = (lv >= maxLv || b <= a) ? 1.0 : (double)(xp - a) / (double)(b - a);
		}
		const int ry = Y + 339 + s * 20;
		char cLv[16];
		wsprintf(cLv, "Lv %d", lv);
		R.TextOut(X + 30, ry, pName, RGB(214, 218, 226));
		R.TextOut(X + 222 - R.GetTextExtent(cLv).cx, ry, cLv, lv >= maxLv ? kGold : RGB(214, 218, 226));
		R.FillPanel(X + 30, ry + 15, X + 222, ry + 17, 0, RGB(255, 255, 255), 22);
		const int w = (int)(192 * (f < 0 ? 0 : (f > 1 ? 1 : f)));
		if (w > 0) R.FillPanel(X + 30, ry + 15, X + 30 + w, ry + 17, 0, kGold, 200);
	}
}
