// AchievePanel.cpp: VALORIA's achievements on the client. The list is Shared\ValoriaAchieveData.h
// (tools\valoria_port\build_achievements.py); the game server counts them (HG Server Achievements.cpp).
//
//   - the Achievements window (DIALOG_ACHIEVE), opened with the Achievements button in the character
//     window: VALORIA's six categories, every monster / counter with its 13 (or fewer) tiers, how far
//     you are to the next one, and the title each grants. Click a row to wear its best title; click
//     it again (or Clear) to wear none.
//   - unlocks: the counters come with Enchanting (NOTIFY_ENCHANTSTATS); whatever passed a number since
//     the last ones is announced. Level is announced on level-up.
//   - titles: shown in front of a player's name (NOTIFY_TITLEINFO, TitleOf, used in DrawObjectName).
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"
#include "..\..\shared\ValoriaAchieveData.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Valoria;

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(110, 114, 124);
const COLORREF kOk		= RGB(120, 220, 130);

const char * const kCats[] = { "Monsters", "Bosses", "Slayer", "Adventure", "Quests", "War" };
const int kCatCount = 6;

const int kTop = 44, kRowH = 46, kListX0 = 156;
const int kTabX0 = 12, kTabX1 = 146, kTabH = 30, kTabGap = 6;

struct Group { int counter, cat, first, n; };

const std::vector<Group> & Groups()
{
	static std::vector<Group> g;
	if (g.empty())
		for (int i = 0; i < Ach::DEF_COUNT; i++) {
			const Ach::Def & d = Ach::kDefs[i];
			if (!g.empty() && g.back().counter == d.counter) g.back().n++;
			else g.push_back(Group{ d.counter, d.cat, i, 1 });
		}
	return g;
}

const char * GroupName(int counter)
{
	if (counter < Ach::TARGET_COUNT) return Ach::kTargets[counter].name;
	switch (counter) {
	case ACH_TOTAL: return "Monsters slain";
	case ACH_ENEMY: return "Enemy players defeated";
	case ACH_QUEST: return "City Hall quests";
	case ACH_SIDE:  return "Side quests";
	case ACH_LEVEL: return "Level";
	}
	return "?";
}

const Ach::Def * DefOf(int counter, int tier)
{
	if (tier <= 0) return NULL;
	for (int i = 0; i < Ach::DEF_COUNT; i++)
		if (Ach::kDefs[i].counter == counter && Ach::kDefs[i].tier == tier) return &Ach::kDefs[i];
	return NULL;
}

bool SameName(const char * a, const char * b) { return strncmp(a, b, 10) == 0; }
bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

std::string Num(unsigned int v)
{
	std::string s = std::to_string(v);
	for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
	return s;
}

} // namespace

unsigned int CGame::AchProgress(int iCounter) const
{
	if (iCounter == ACH_LEVEL) return m_iLevel > 0 ? (unsigned int)m_iLevel : 0;
	return (iCounter >= 0 && iCounter < ACH_SAVED) ? m_enchant.ach[iCounter] : 0;
}

int CGame::AchUnlockedCount() const
{
	int n = 0;
	for (int i = 0; i < Ach::DEF_COUNT; i++)
		if (AchProgress(Ach::kDefs[i].counter) >= Ach::kDefs[i].count) n++;
	return n;
}

const char * CGame::TitleOf(const char * pName) const
{
	if (pName == NULL || pName[0] == 0) return NULL;
	if (SameName(pName, m_cPlayerName)) {
		const Ach::Def * d = DefOf(m_enchant.titleCounter, m_enchant.titleTier);
		return d ? d->title : NULL;
	}
	for (size_t i = 0; i < m_titleOf.size(); i++)
		if (SameName(m_titleOf[i].name, pName)) {
			const Ach::Def * d = DefOf(m_titleOf[i].counter, m_titleOf[i].tier);
			return d ? d->title : NULL;
		}
	return NULL;
}

// What passed its number when counter went from before to after.
void CGame::AchAnnounce(int iCounter, unsigned int before, unsigned int after)
{
	int n = 0;
	for (int i = 0; i < Ach::DEF_COUNT; i++) {
		const Ach::Def & d = Ach::kDefs[i];
		if (d.counter != iCounter || d.count <= before || d.count > after) continue;
		if (++n <= 3) {
			AddColoredEvent(ChatSpan(kGold, "[Achievement]") + " " + d.name + " - title " + ChatSpan(kGold, std::string("\"") + d.title + "\""));
			if (n == 1) {
				wsprintf(G_cTxt, "Achievement unlocked: %s", d.name);
				SetTopMsg(G_cTxt, 5);
			}
		}
	}
	if (n > 3) {
		wsprintf(G_cTxt, "...and %d more achievements.", n - 3);
		AddColoredEvent(ChatSpan(kGold, G_cTxt));
	}
	if (n > 0) PlaySound('E', 23, 0);
}

//////////////////////////////////////////////////////////////////////
// Messages from the server

void CGame::NotifyMsg_TitleInfo(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	TitleMark m;
	memset(&m, 0, sizeof(m));
	uint8 c, t;
	Pop(cp, m.name, 10);
	Pop(cp, c); Pop(cp, t);
	m.counter = c; m.tier = t;
	if (SameName(m.name, m_cPlayerName)) return;	// ours comes with Enchanting
	for (size_t i = 0; i < m_titleOf.size(); i++)
		if (SameName(m_titleOf[i].name, m.name)) { m_titleOf[i] = m; return; }
	if (m.tier > 0) m_titleOf.push_back(m);
}

//////////////////////////////////////////////////////////////////////
// The window

void CGame::OpenAchieveWindow()
{
	EnableDialogBox(DIALOG_ACHIEVE, NULL, NULL, NULL);
	PlaySound('E', 14, 5);
}

namespace {

void CloseRect(short X, short Y, RECT & rc)	{ SetRect(&rc, X + CGame::ACHWIN_W - 32, Y + 6, X + CGame::ACHWIN_W - 8, Y + 30); }
void TabRect(short X, short Y, int i, RECT & rc)	{ SetRect(&rc, X + kTabX0, Y + kTop + i * (kTabH + kTabGap), X + kTabX1, Y + kTop + i * (kTabH + kTabGap) + kTabH); }
void ClearRect(short X, short Y, RECT & rc)	{ SetRect(&rc, X + kTabX0, Y + CGame::ACHWIN_H - 44, X + kTabX1, Y + CGame::ACHWIN_H - 16); }
void ListRect(short X, short Y, RECT & rc)	{ SetRect(&rc, X + kListX0, Y + kTop, X + CGame::ACHWIN_W - 12, Y + CGame::ACHWIN_H - 14); }
int  Rows()									{ return (CGame::ACHWIN_H - 14 - kTop - 8) / kRowH; }

std::vector<int> GroupsOf(int cat)
{
	std::vector<int> v;
	const std::vector<Group> & g = Groups();
	for (size_t i = 0; i < g.size(); i++) if (g[i].cat == cat) v.push_back((int)i);
	return v;
}

} // namespace

void CGame::DrawDialogBox_Achievements()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_ACHIEVE];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int W = ACHWIN_W, H = ACHWIN_H;
	const bool bTop = dlg.IsTop();
	const std::vector<Group> & G = Groups();
	char cTxt[200];
	RECT rc;

	R.StonePanel(X, Y, X + W, Y + H);
	R.TitleOut(X + 14, Y + 8, "ACHIEVEMENTS", kGold, 15);
	wsprintf(cTxt, "%d of %d unlocked", AchUnlockedCount(), Ach::DEF_COUNT);
	R.TextOutSized(X + 196, Y + 13, cTxt, kGrey, 11);
	CloseRect(X, Y, rc);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 8, rc.top + 1, "x", bOnClose ? kWhite : kGrey, 18);
	R.FillPanel(X + 1, Y + 33, X + W - 1, Y + 34, 0, RGB(255, 255, 255), 26);

	// category tabs, with what you have of each
	for (int c = 0; c < kCatCount; c++) {
		int have = 0, all = 0;
		for (int i = 0; i < Ach::DEF_COUNT; i++)
			if (Ach::kDefs[i].cat == c) { all++; if (AchProgress(Ach::kDefs[i].counter) >= Ach::kDefs[i].count) have++; }
		TabRect(X, Y, c, rc);
		const bool bOn = bTop && InRect(rc, ms::X, ms::Y), bSel = (c == m_iAchCat);
		R.StoneButton(rc.left, rc.top, rc.right, rc.bottom, bSel ? 2 : (bOn ? 1 : 0));
		R.TextOut(rc.left + 10, rc.top + 7, kCats[c], bSel ? RGB(255, 242, 199) : (bOn ? kWhite : RGB(220, 224, 230)));
		wsprintf(cTxt, "%d/%d", have, all);
		R.TextOutSized(rc.right - 8 - R.TextWidthSized(cTxt, 10), rc.top + 9, cTxt, have == all ? kOk : kGrey, 10);
	}

	// your title
	const int ty = Y + kTop + kCatCount * (kTabH + kTabGap) + 8;
	R.TextOutSized(X + kTabX0 + 2, ty, "YOUR TITLE", kGrey, 10);
	const Ach::Def * pMine = DefOf(m_enchant.titleCounter, m_enchant.titleTier);
	R.Slate(X + kTabX0, ty + 16, X + kTabX1, ty + 44);
	R.TextOutSized(X + kTabX0 + 8, ty + 22, pMine ? pMine->title : "(none)", pMine ? kGold : kDim, 11);
	R.TextOutSized(X + kTabX0 + 2, ty + 52, "Click a row to wear", kDim, 10);
	R.TextOutSized(X + kTabX0 + 2, ty + 64, "its best title.", kDim, 10);
	ClearRect(X, Y, rc);
	const bool bOnClear = bTop && pMine != NULL && InRect(rc, ms::X, ms::Y);
	R.StoneButton(rc.left, rc.top, rc.right, rc.bottom, bOnClear ? 1 : 0);
	R.TitleCentered(rc.left, rc.top, rc.right, rc.bottom, "Clear title", pMine ? (bOnClear ? kWhite : RGB(220, 224, 230)) : kDim, 11);

	// the list
	const std::vector<int> rows = GroupsOf(m_iAchCat);
	const int nRows = Rows();
	ListRect(X, Y, rc);
	const RECT rcList = rc;
	if (bTop && ms::Z != 0 && InRect(rcList, ms::X, ms::Y)) {
		m_iAchScroll += (ms::Z > 0) ? -2 : 2;
		m_Mouse.m_sZ = 0;
		ms::Z = 0;
	}
	m_iAchScroll = (std::max)(0, (std::min)(m_iAchScroll, (int)rows.size() - nRows));
	R.Slate(rcList.left, rcList.top, rcList.right, rcList.bottom);

	for (int r = 0; r < nRows && m_iAchScroll + r < (int)rows.size(); r++) {
		const Group & g = G[rows[m_iAchScroll + r]];
		const unsigned int v = AchProgress(g.counter);
		int done = 0;
		for (int k = 0; k < g.n; k++) if (v >= Ach::kDefs[g.first + k].count) done = k + 1;
		const int x0 = rcList.left + 4, x1 = rcList.right - 14, y0 = rcList.top + 4 + r * kRowH;
		const bool bOn = bTop && ms::X >= x0 && ms::X < x1 && ms::Y >= y0 && ms::Y < y0 + kRowH - 2;
		const bool bWorn = pMine != NULL && pMine->counter == g.counter;
		if (bOn && done > 0) R.FillPanel(x0, y0, x1, y0 + kRowH - 2, 4, RGB(255, 255, 255), 18);
		if (bWorn) R.FillPanel(x0, y0, x1, y0 + kRowH - 2, 4, kGold, 22);

		R.TextOutSized(x0 + 8, y0 + 4, GroupName(g.counter), done > 0 ? kGold : kWhite, 12);
		wsprintf(cTxt, "Tier %d/%d", done, g.n);
		R.TextOutSized(x1 - 8 - R.TextWidthSized(cTxt, 10), y0 + 6, cTxt, done == g.n ? kOk : kGrey, 10);

		if (done < g.n) {				// the next one: what it takes, how far you are
			const Ach::Def & next = Ach::kDefs[g.first + done];
			const unsigned int lo = done > 0 ? Ach::kDefs[g.first + done - 1].count : 0;
			std::string s = std::string(next.name) + "  -  " + Num(v) + " / " + Num(next.count);
			R.TextOutSized(x0 + 8, y0 + 21, s.c_str(), kGrey, 10);
			const int bx0 = x0 + 8, bx1 = x1 - 150, by = y0 + 36;
			R.FillPanel(bx0, by, bx1, by + 4, 2, RGB(40, 38, 36), 255);
			const double f = (next.count > lo) ? (double)(v - (std::min)(v, lo)) / (double)(next.count - lo) : 0.0;
			const int fx = bx0 + (int)((bx1 - bx0) * (std::min)(1.0, f));
			if (fx > bx0) R.FillPanel(bx0, by, fx, by + 4, 2, kGold, 230);
		}
		else R.TextOutSized(x0 + 8, y0 + 21, "Complete!", kOk, 10);

		if (done > 0) {					// the best title it grants
			const char * t = Ach::kDefs[g.first + done - 1].title;
			R.TextOutSized(x1 - 8 - R.TextWidthSized(t, 10), y0 + 26, t, bWorn ? RGB(255, 242, 199) : kGold, 10);
		}
	}

	// scroll position
	if ((int)rows.size() > nRows) {
		const int sx = rcList.right - 8, sy0 = rcList.top + 6, sy1 = rcList.bottom - 6;
		R.FillPanel(sx, sy0, sx + 3, sy1, 1, RGB(255, 255, 255), 22);
		const int len = (std::max)(16, (sy1 - sy0) * nRows / (int)rows.size());
		const int pos = sy0 + (sy1 - sy0 - len) * m_iAchScroll / (std::max)(1, (int)rows.size() - nRows);
		R.FillPanel(sx, pos, sx + 3, pos + len, 1, kGold, 200);
	}
}

void CGame::DlgBoxClick_Achievements()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_ACHIEVE];
	const short X = dlg.m_X, Y = dlg.m_Y;
	RECT rc;
	CloseRect(X, Y, rc);
	if (InRect(rc, ms::X, ms::Y)) { DisableDialogBox(DIALOG_ACHIEVE); PlaySound('E', 14, 5); return; }
	for (int c = 0; c < kCatCount; c++) {
		TabRect(X, Y, c, rc);
		if (InRect(rc, ms::X, ms::Y)) { if (m_iAchCat != c) { m_iAchCat = c; m_iAchScroll = 0; } PlaySound('E', 14, 5); return; }
	}
	ClearRect(X, Y, rc);
	if (InRect(rc, ms::X, ms::Y)) {
		if (m_enchant.titleTier != 0) { bSendCommand(MSGID_REQUEST_TITLE, 0, NULL, 0, 0, 0, NULL); PlaySound('E', 14, 5); }
		return;
	}
	ListRect(X, Y, rc);
	if (!InRect(rc, ms::X, ms::Y)) return;
	const int r = (ms::Y - rc.top - 4) / kRowH;
	const std::vector<int> rows = GroupsOf(m_iAchCat);
	if (r < 0 || r >= Rows() || m_iAchScroll + r >= (int)rows.size()) return;
	const Group & g = Groups()[rows[m_iAchScroll + r]];
	const unsigned int v = AchProgress(g.counter);
	int best = 0;
	for (int k = 0; k < g.n; k++) if (v >= Ach::kDefs[g.first + k].count) best = Ach::kDefs[g.first + k].tier;
	if (best == 0) { AddEventList("You haven't unlocked a title there yet.", 10); return; }
	PlaySound('E', 14, 5);
	if (m_enchant.titleCounter == g.counter && m_enchant.titleTier == best)
		bSendCommand(MSGID_REQUEST_TITLE, 0, NULL, 0, 0, 0, NULL);		// worn: take it off
	else bSendCommand(MSGID_REQUEST_TITLE, (WORD)g.counter, NULL, best, 0, 0, NULL);
}
