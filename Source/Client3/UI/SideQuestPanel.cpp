// SideQuestPanel.cpp: side quests on the client (VALORIA's quest system with Helbreath quests). The
// quests are Shared\ValoriaSideQuests.h; the game server runs them (HG Server SideQuests.cpp).
//
//   - the Side Quests window (DIALOG_SIDEQUEST): opened from a townsperson's menu (McGaffin, Perry,
//     Devlin: "Side quests"), listing that giver's quests, or from the Character window ("Journal"),
//     listing the ones you have taken. The selected quest: its story, objectives with your progress,
//     the rewards, and Take / Hand in / Abandon.
//   - progress comes with Enchanting (NOTIFY_ENCHANTSTATS, Shared\ValoriaEnchant.h CharData), and
//     what just happened in NOTIFY_SIDEEVENT, written out here.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"
#include "..\..\shared\ValoriaSideQuests.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Valoria;

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kText	= RGB(214, 218, 226);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(110, 114, 124);
const COLORREF kOk		= RGB(120, 220, 130);
const COLORREF kBad		= RGB(230, 120, 110);

const int kTop = 44, kListX1 = 214, kRowH = 34;

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

enum { ST_LOCKED, ST_LEVEL, ST_OPEN, ST_TAKEN, ST_READY, ST_DONE };

} // namespace

int CGame::SideStatus(int q) const
{
	const CharData & d = m_enchant;
	if (q < 0 || q >= SideQ::QUEST_COUNT) return ST_LOCKED;
	if (d.sideState[q] == SIDE_DONE) return ST_DONE;
	const int a = d.SideSlotOf(q);
	if (a >= 0) {
		for (int o = 0; o < SideQ::MAX_OBJ; o++) {
			const SideQ::Obj & ob = SideQ::kQuests[q].obj[o];
			if (ob.type != SideQ::OBJ_NONE && d.sideProg[a][o] < ob.count) return ST_TAKEN;
		}
		return ST_READY;
	}
	const int prev = SideQ::Previous(q);
	if (prev >= 0 && d.sideState[prev] != SIDE_DONE) return ST_LOCKED;
	if (m_iLevel < SideQ::kQuests[q].minLevel) return ST_LEVEL;
	return ST_OPEN;
}

std::vector<int> CGame::SideList() const
{
	std::vector<int> v;
	for (int q = 0; q < SideQ::QUEST_COUNT; q++) {
		if (m_iSideGiver != 0 ? SideQ::kQuests[q].giver == m_iSideGiver : m_enchant.SideSlotOf(q) >= 0) v.push_back(q);
	}
	return v;
}

void CGame::OpenSideQuests(int iGiver)
{
	m_iSideGiver = iGiver;
	m_iSideSel = -1;
	const std::vector<int> v = SideList();
	for (size_t i = 0; i < v.size() && m_iSideSel < 0; i++) {	// the first one worth looking at
		const int st = SideStatus(v[i]);
		if (st == ST_READY || st == ST_OPEN || st == ST_TAKEN) m_iSideSel = v[i];
	}
	if (m_iSideSel < 0 && !v.empty()) m_iSideSel = v[0];
	EnableDialogBox(DIALOG_SIDEQUEST, NULL, NULL, NULL);
	PlaySound('E', 14, 5);
}

//////////////////////////////////////////////////////////////////////
// Messages from the server

void CGame::NotifyMsg_SideEvent(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 code, q, o;
	uint16 v;
	uint32 xp;
	Pop(cp, code); Pop(cp, q); Pop(cp, o); Pop(cp, v); Pop(cp, xp);
	if (q >= SideQ::QUEST_COUNT) return;
	const SideQ::Quest & Q = SideQ::kQuests[q];
	const std::string tag = ChatSpan(kGold, "[Quest]") + " ";
	char cTxt[300];
	switch (code) {
	case SideQ::EV_PROGRESS:
		if (o >= SideQ::MAX_OBJ) return;
		if (Q.obj[o].type == SideQ::OBJ_VISIT) wsprintf(cTxt, "%s: you have reached %s.", Q.title, Q.obj[o].label);
		else if (Q.obj[o].type == SideQ::OBJ_COLLECT || v >= Q.obj[o].count || Q.obj[o].count <= 20 || v % 5 == 0)
			wsprintf(cTxt, "%s: %s %d/%d", Q.title, Q.obj[o].label, (int)v, (int)Q.obj[o].count);
		else return;
		AddColoredEvent(tag + cTxt);
		break;
	case SideQ::EV_READY:
		wsprintf(cTxt, "%s is complete - return to %s.", Q.title, SideQ::GiverName(Q.giver));
		AddColoredEvent(tag + ChatSpan(kOk, cTxt));
		SetTopMsg(cTxt, 5);
		break;
	case SideQ::EV_TAKEN:
		AddColoredEvent(tag + "Taken: " + Q.title);
		break;
	case SideQ::EV_ABANDONED:
		AddColoredEvent(tag + "Abandoned: " + Q.title);
		break;
	case SideQ::EV_COMPLETE:
		AddColoredEvent(ChatSpan(kGold, std::string(SideQ::GiverName(Q.giver)) + ":") + " \"" + Q.done + "\"");
		wsprintf(cTxt, "Quest complete: %s (+%u XP, %u gold, %s x%d)", Q.title, (unsigned)xp, SideQ::GoldReward(Q), ItemDisplayName(Q.item).c_str(), (int)Q.itemCount);
		AddColoredEvent(tag + ChatSpan(kOk, cTxt));
		wsprintf(cTxt, "Quest complete: %s", Q.title);
		SetTopMsg(cTxt, 5);
		PlaySound('E', 23, 0);
		break;
	case SideQ::EV_ERROR:
		if (o == SideQ::ERR_LEVEL) wsprintf(cTxt, "You need level %d for %s.", (int)Q.minLevel, Q.title);
		else if (o == SideQ::ERR_FAR) wsprintf(cTxt, "%s isn't close enough - talk to them in town.", SideQ::GiverName(Q.giver));
		else if (o == SideQ::ERR_FULL) wsprintf(cTxt, "You already have %d side quests. Finish or abandon one first.", SIDE_ACTIVE);
		else return;
		AddEventList(cTxt, 10);
		break;
	}
}

//////////////////////////////////////////////////////////////////////
// The window

namespace {

void CloseRect(short X, short Y, RECT & rc)	{ SetRect(&rc, X + CGame::SIDEWIN_W - 32, Y + 6, X + CGame::SIDEWIN_W - 8, Y + 30); }
void RowRect(short X, short Y, int r, RECT & rc)	{ SetRect(&rc, X + 14, Y + kTop + 4 + r * kRowH, X + kListX1 - 4, Y + kTop + 4 + r * kRowH + kRowH - 4); }
void ActRect(short X, short Y, int i, RECT & rc)	{ const int w = 120; const int x1 = X + CGame::SIDEWIN_W - 16 - i * (w + 8); SetRect(&rc, x1 - w, Y + CGame::SIDEWIN_H - 42, x1, Y + CGame::SIDEWIN_H - 14); }

const char * StatusText(int st, int lv, char * buf)
{
	switch (st) {
	case ST_LOCKED: return "Not yet";
	case ST_LEVEL:  wsprintf(buf, "Level %d", lv); return buf;
	case ST_OPEN:   return "Available";
	case ST_TAKEN:  return "In progress";
	case ST_READY:  return "Hand in!";
	case ST_DONE:   return "Done";
	}
	return "";
}
COLORREF StatusColour(int st)
{
	switch (st) { case ST_OPEN: return kGold; case ST_TAKEN: return kText; case ST_READY: return kOk; case ST_DONE: return kDim; }
	return kGrey;
}

} // namespace

// The buttons the selected quest has now: Take, or Hand in and Abandon.
int CGame::SideActions(int out[2]) const
{
	const int st = SideStatus(m_iSideSel);
	int n = 0;
	if (st == ST_OPEN) out[n++] = SideQ::SQ_TAKE;
	if (st == ST_READY) out[n++] = SideQ::SQ_HANDIN;
	if (st == ST_TAKEN || st == ST_READY) out[n++] = SideQ::SQ_ABANDON;
	return n;
}

void CGame::DrawDialogBox_SideQuests()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_SIDEQUEST];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int W = SIDEWIN_W, H = SIDEWIN_H;
	const bool bTop = dlg.IsTop();
	char cTxt[300], cBuf[32];
	RECT rc;

	R.StonePanel(X, Y, X + W, Y + H);
	if (m_iSideGiver != 0) wsprintf(cTxt, "%s'S QUESTS", SideQ::GiverName(m_iSideGiver));
	else strcpy(cTxt, "QUEST JOURNAL");
	for (char * c = cTxt; *c; c++) *c = (char)toupper((unsigned char)*c);
	R.TitleOut(X + 14, Y + 8, cTxt, kGold, 15);
	int nDone = 0;
	for (int q = 0; q < SideQ::QUEST_COUNT; q++) if (m_enchant.sideState[q] == SIDE_DONE) nDone++;
	wsprintf(cTxt, "%d of %d side quests done", nDone, SideQ::QUEST_COUNT);
	R.TextOutSized(X + 230, Y + 13, cTxt, kGrey, 11);
	CloseRect(X, Y, rc);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 8, rc.top + 1, "x", bOnClose ? kWhite : kGrey, 18);
	R.FillPanel(X + 1, Y + 33, X + W - 1, Y + 34, 0, RGB(255, 255, 255), 26);

	// the list
	const std::vector<int> v = SideList();
	R.Slate(X + 10, Y + kTop, X + kListX1, Y + H - 14);
	if (v.empty()) R.TextOutSized(X + 20, Y + kTop + 12, m_iSideGiver ? "No quests." : "No quests taken.", kDim, 11);
	for (size_t i = 0; i < v.size(); i++) {
		RowRect(X, Y, (int)i, rc);
		if (rc.bottom > Y + H - 16) break;
		const int q = v[i], st = SideStatus(q);
		const bool bSel = (q == m_iSideSel), bOn = bTop && InRect(rc, ms::X, ms::Y);
		if (bSel) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 4, kGold, 30);
		else if (bOn) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 4, RGB(255, 255, 255), 18);
		R.TextOutSized(rc.left + 8, rc.top + 2, SideQ::kQuests[q].title, st == ST_DONE ? kDim : (bSel ? RGB(255, 242, 199) : kWhite), 11);
		R.TextOutSized(rc.left + 8, rc.top + 16, StatusText(st, SideQ::kQuests[q].minLevel, cBuf), StatusColour(st), 10);
		if (m_iSideGiver == 0) R.TextOutSized(rc.right - 8 - R.TextWidthSized(SideQ::GiverName(SideQ::kQuests[q].giver), 10), rc.top + 16, SideQ::GiverName(SideQ::kQuests[q].giver), kGrey, 10);
	}

	// the selected quest
	const int x0 = X + kListX1 + 14, x1 = X + W - 16;
	if (m_iSideSel < 0 || m_iSideSel >= SideQ::QUEST_COUNT) return;
	const int q = m_iSideSel, st = SideStatus(q);
	const SideQ::Quest & Q = SideQ::kQuests[q];
	int y = Y + kTop + 2;
	R.TitleOut(x0, y, Q.title, kGold, 14);
	y += 22;
	wsprintf(cTxt, "%s  -  level %d  -  %s", SideQ::GiverName(Q.giver), (int)Q.minLevel, StatusText(st, Q.minLevel, cBuf));
	R.TextOutSized(x0, y, cTxt, StatusColour(st) == kDim ? kGrey : StatusColour(st), 10);
	y += 18;

	if (st == ST_LOCKED) {
		const int prev = SideQ::Previous(q);
		wsprintf(cTxt, "%s has something else for you first: %s.", SideQ::GiverName(Q.giver), prev >= 0 ? SideQ::kQuests[prev].title : "?");
		SetRect(&rc, x0, y, x1, y + 40);
		R.DrawTextEx(&rc, cTxt, kGrey, DT_LEFT, true);
		return;
	}
	SetRect(&rc, x0, y, x1, y + 96);
	R.DrawTextEx(&rc, Q.text, kText, DT_LEFT, true);
	y += 100;

	R.TextOutSized(x0, y, "OBJECTIVES", kGrey, 10);
	y += 16;
	const int a = m_enchant.SideSlotOf(q);
	for (int o = 0; o < SideQ::MAX_OBJ; o++) {
		const SideQ::Obj & ob = Q.obj[o];
		if (ob.type == SideQ::OBJ_NONE) continue;
		const int have = (st == ST_DONE) ? ob.count : (a >= 0 ? m_enchant.sideProg[a][o] : 0);
		const bool bDone = have >= ob.count;
		if (ob.type == SideQ::OBJ_KILL) wsprintf(cTxt, "Kill %s: %d/%d", ob.label, have, (int)ob.count);
		else if (ob.type == SideQ::OBJ_COLLECT) wsprintf(cTxt, "%s (%d%% from a kill): %d/%d", ob.label, (int)ob.chance, have, (int)ob.count);
		else wsprintf(cTxt, "Go to %s%s", ob.label, bDone ? " - done" : "");
		R.FillPanel(x0 + 2, y + 5, x0 + 8, y + 11, 1, bDone ? kOk : kDim, 255);
		R.TextOutSized(x0 + 14, y, cTxt, bDone ? kOk : kText, 11);
		y += 17;
	}
	y += 8;
	R.TextOutSized(x0, y, "REWARDS", kGrey, 10);
	y += 16;
	wsprintf(cTxt, "XP (a third of a level %d), %u gold, %s x%d", (int)Q.minLevel, SideQ::GoldReward(Q), ItemDisplayName(Q.item).c_str(), (int)Q.itemCount);
	R.TextOutSized(x0, y, cTxt, kGold, 11);

	// actions
	static const char * const kAct[4] = { "", "Take quest", "Hand in", "Abandon" };
	int acts[2];
	const int n = SideActions(acts);
	for (int i = 0; i < n; i++) {
		ActRect(X, Y, i, rc);
		const bool bOn = bTop && InRect(rc, ms::X, ms::Y);
		const bool bMain = acts[i] != SideQ::SQ_ABANDON;
		R.StoneButton(rc.left, rc.top, rc.right, rc.bottom, bMain ? 1 : (bOn ? 1 : 0));
		R.TitleCentered(rc.left, rc.top, rc.right, rc.bottom, kAct[acts[i]],
			bMain ? (bOn ? RGB(255, 242, 199) : RGB(245, 219, 158)) : (bOn ? kWhite : RGB(220, 224, 230)), 11);
	}
	if (m_dwSideAbandonAt != 0 && m_dwCurTime - m_dwSideAbandonAt < 4000)
		R.TextOutSized(x0, Y + H - 36, "Click Abandon again to lose this quest's progress.", kBad, 10);
}

void CGame::DlgBoxClick_SideQuests()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_SIDEQUEST];
	const short X = dlg.m_X, Y = dlg.m_Y;
	RECT rc;
	CloseRect(X, Y, rc);
	if (InRect(rc, ms::X, ms::Y)) { DisableDialogBox(DIALOG_SIDEQUEST); PlaySound('E', 14, 5); return; }

	const std::vector<int> v = SideList();
	for (size_t i = 0; i < v.size(); i++) {
		RowRect(X, Y, (int)i, rc);
		if (InRect(rc, ms::X, ms::Y)) { m_iSideSel = v[i]; m_dwSideAbandonAt = 0; PlaySound('E', 14, 5); return; }
	}
	int acts[2];
	const int n = SideActions(acts);
	for (int i = 0; i < n; i++) {
		ActRect(X, Y, i, rc);
		if (!InRect(rc, ms::X, ms::Y)) continue;
		PlaySound('E', 14, 5);
		if (acts[i] == SideQ::SQ_ABANDON) {		// a second click within 4 seconds confirms
			if (m_dwSideAbandonAt == 0 || m_dwCurTime - m_dwSideAbandonAt >= 4000) { m_dwSideAbandonAt = m_dwCurTime; return; }
			if (m_dwCurTime - m_dwSideAbandonAt < 300) return;
		}
		m_dwSideAbandonAt = 0;
		bSendCommand(MSGID_REQUEST_SIDEQUEST, (WORD)acts[i], NULL, m_iSideSel, 0, 0, NULL);
		return;
	}
}
