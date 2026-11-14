// BountyBoard.cpp: player bounties (client side). The rules are in HG Server\Bounty.cpp.
//
//   - messages from the server: each bounty (NOTIFY_BOUNTYINFO), who backs one
//     (NOTIFY_BOUNTYDETAIL), your claims (NOTIFY_BOUNTYSTATS) and announcements
//     (NOTIFY_BOUNTYEVENT)
//   - the Bounty Board (/bounty, or the City Hall): top bounties, the ones you put up, the one on
//     you; pick a row to see who backs it and add gold, or type a name to put up a new one
//   - a "Bounty: N" line when hovering a player you could claim it on
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"

#include <algorithm>
#include <string>

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(120, 126, 136);
const COLORREF kRed		= RGB(255, 120, 105);

const int kRows = 8;
const int kRowY = 56, kRowH = 19;
const uint32 kMinBounty = 50000;

// Chat colours for announcements (as in the mockup)
const COLORREF kChatTag		= RGB(232, 176, 80);	// [Bounty]
const COLORREF kChatTagW	= RGB(255, 140, 90);	// [Wanted]
const COLORREF kChatTarget	= RGB(255, 130, 120);	// the hunted
const COLORREF kChatPlayer	= RGB(236, 238, 242);	// whoever put gold up
const COLORREF kChatHunter	= RGB(150, 190, 255);	// whoever claimed / brought them down
const COLORREF kChatMe		= RGB(255, 214, 120);	// you
const COLORREF kChatGold	= RGB(240, 196, 100);
const COLORREF kChatBigGold	= RGB(255, 110, 95);	// 1,000,000 and up

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(x, y, s, c, iPx);
	r.TextOutSized(x + 1, y, s, c, iPx);
}

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

bool SameName(const char * a, const char * b) { return strncmp(a, b, 10) == 0; }

std::string Commas(unsigned long v)
{
	std::string s = std::to_string(v);
	for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
	return s;
}

std::string TimeLeft(DWORD dwExpireAt, DWORD dwNow)
{
	if ((int)(dwExpireAt - dwNow) <= 0) return "-";
	const unsigned s = (dwExpireAt - dwNow) / 1000;
	char c[24];
	if (s >= 86400) wsprintf(c, "%ud %02uh", s / 86400, (s % 86400) / 3600);
	else wsprintf(c, "%uh %02um", s / 3600, (s % 3600) / 60);
	return c;
}

uint32 ParseGold(const char * p)
{
	unsigned long long v = 0;
	for (; *p; p++) {
		if (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); if (v > 4000000000ULL) return 4000000000U; }
		else if (*p == 'k' || *p == 'K') v *= 1000;
		else if (*p == 'm' || *p == 'M') v *= 1000000;
	}
	return (uint32)(std::min)(v, 4000000000ULL);
}

void Pill(CRenderer & r, int x, int y, const char * s, COLORREF c)
{
	const int w = r.TextWidthSized(s, 9) + 10;
	r.FillPanel(x, y, x + w, y + 14, 4, c, 40, c, 150);
	r.TextOutSized(x + 5, y, s, c, 9);
}

} // namespace

//////////////////////////////////////////////////////////////////////
// Layout (relative to the window)

void CGame::BountyRect(int id, RECT * pRc)
{
	const int W = BOUNTYWIN_W;
	switch (id) {
	case BNT_CLOSE:		SetRect(pRc, W - 30, 4, W - 8, 26); break;
	case BNT_AMOUNT:	SetRect(pRc, 296, 240, 420, 262); break;
	case BNT_ADD:		SetRect(pRc, 428, 240, W - 20, 262); break;
	case BNT_NAME:		SetRect(pRc, 296, 284, 420, 306); break;
	case BNT_PLACE:		SetRect(pRc, 428, 284, W - 20, 306); break;
	default:
		if (id >= BNT_TAB0 && id < BNT_TAB0 + 3) {
			static const char * const kTab[3] = { "Top bounties", "Placed by me", "On me" };
			int x = 170;
			for (int i = 0; i < id - BNT_TAB0; i++) x += m_Renderer.TextWidthSized(kTab[i], 12) + 22 + 6;
			SetRect(pRc, x, 6, x + m_Renderer.TextWidthSized(kTab[id - BNT_TAB0], 12) + 22, 26);
		}
		else if (id >= BNT_ROW0 && id < BNT_ROW0 + kRows)
			SetRect(pRc, 8, kRowY + (id - BNT_ROW0) * kRowH, W - 8, kRowY + (id - BNT_ROW0 + 1) * kRowH);
		else SetRect(pRc, 0, 0, 0, 0);
		break;
	}
}

// The bounties shown on the current tab, biggest first.
void CGame::BountyList(std::vector<int> & out)
{
	out.clear();
	for (int i = 0; i < (int)m_bounties.size(); i++) {
		const BountyEntry & e = m_bounties[i];
		if (m_iBountyTab == 1 && e.mine == 0) continue;
		if (m_iBountyTab == 2 && !SameName(e.name, m_cPlayerName)) continue;
		out.push_back(i);
	}
	std::sort(out.begin(), out.end(), [this](int a, int b) { return m_bounties[a].total > m_bounties[b].total; });
}

const CGame::BountyEntry * CGame::BountyFor(const char * pName)
{
	if (pName == NULL || pName[0] == 0) return NULL;
	for (size_t i = 0; i < m_bounties.size(); i++) if (SameName(m_bounties[i].name, pName)) return &m_bounties[i];
	return NULL;
}

//////////////////////////////////////////////////////////////////////
// Messages from the server

void CGame::NotifyMsg_BountyInfo(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	BountyEntry e;
	memset(&e, 0, sizeof(e));
	uint32 total, secs, mine;
	uint8 side, pk, online;
	uint16 level;
	Pop(cp, e.name, 10);
	Pop(cp, total); Pop(cp, side); Pop(cp, pk); Pop(cp, level); Pop(cp, online);
	Pop(cp, e.map, 10);
	Pop(cp, secs); Pop(cp, mine);
	e.total = total; e.side = side; e.pk = pk; e.level = level; e.online = online; e.mine = mine;
	e.dwExpireAt = m_dwCurTime + secs * 1000;

	size_t i = 0;
	for (; i < m_bounties.size(); i++) if (SameName(m_bounties[i].name, e.name)) break;
	if (total == 0) {
		if (i < m_bounties.size()) m_bounties.erase(m_bounties.begin() + i);
		if (SameName(m_cBountySel, e.name)) { m_bountyBackers.clear(); }
		return;
	}
	if (i < m_bounties.size()) m_bounties[i] = e; else m_bounties.push_back(e);
	// the bounty picked on the board changed: ask again who backs it
	if (m_bIsDialogEnabled[DIALOG_BOUNTY] && SameName(m_cBountySel, e.name))
		bSendCommand(MSGID_REQUEST_BOUNTY, 2, NULL, 0, 0, 0, m_cBountySel);
}

void CGame::NotifyMsg_BountyStats(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint32 n, g;
	Pop(cp, n); Pop(cp, g);
	m_dwBountyClaims = n;
	m_dwBountyClaimGold = g;
}

void CGame::NotifyMsg_BountyDetail(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	char cName[12] = {};
	uint8 n;
	Pop(cp, cName, 10);
	Pop(cp, n);
	if (!SameName(cName, m_cBountySel)) return;
	m_bountyBackers.clear();
	for (int i = 0; i < n && i < 16; i++) {
		BountyBacker b;
		memset(&b, 0, sizeof(b));
		uint32 gold, secs;
		Pop(cp, b.name, 10);
		Pop(cp, gold); Pop(cp, secs);
		b.gold = gold;
		b.dwExpireAt = m_dwCurTime + secs * 1000;
		m_bountyBackers.push_back(b);
	}
	std::sort(m_bountyBackers.begin(), m_bountyBackers.end(), [](const BountyBacker & a, const BountyBacker & b) { return a.gold > b.gold; });
}

void CGame::NotifyMsg_BountyEvent(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 type, side;
	uint32 amount, total;
	char n1[12] = {}, n2[12] = {};
	Pop(cp, type); Pop(cp, amount); Pop(cp, total); Pop(cp, side);
	Pop(cp, n1, 10); Pop(cp, n2, 10);
	const char * pSide = sideName[side < MAXSIDES ? side : 0];
	auto Who = [this](const char * p, COLORREF c) { return ChatSpan(SameName(p, m_cPlayerName) ? kChatMe : c, p); };
	auto Gold = [](uint32 g) { return ChatSpan(g >= 1000000 ? kChatBigGold : kChatGold, Commas(g)); };
	const std::string tag = ChatSpan(kChatTag, "[Bounty]") + " ";

	switch (type) {
	case BOUNTYEV_PLACED:
		AddColoredEvent(tag + Who(n1, kChatPlayer) + " placed a bounty of " + Gold(amount) + " on " + Who(n2, kChatTarget)
			+ " (" + pSide + ").");
		break;
	case BOUNTYEV_RAISED:
		AddColoredEvent(tag + Who(n1, kChatPlayer) + " added " + Gold(amount) + " to the bounty on " + Who(n2, kChatTarget)
			+ " (now " + Gold(total) + ").");
		break;
	case BOUNTYEV_CLAIMED:
		wsprintf(G_cTxt, "%s claimed the %s bounty on %s!", n1, Commas(amount).c_str(), n2);
		SetTopMsg(G_cTxt, 5);
		AddColoredEvent(tag + Who(n1, kChatHunter) + " claimed the " + Gold(amount) + " bounty on " + Who(n2, kChatTarget) + "!");
		if (SameName(n1, m_cPlayerName))
			AddColoredEvent(tag + Gold(amount) + " gold has been added to your reward gold: collect it at the City Hall.");
		break;
	case BOUNTYEV_ONYOU:
		wsprintf(G_cTxt, "A bounty of %s gold has been placed on you by %s.", Commas(amount).c_str(), n1);
		SetTopMsg(G_cTxt, 7);
		break;
	case BOUNTYEV_REFUND:
		AddColoredEvent(tag + "Your bounty on " + Who(n1, kChatTarget) + " expired unclaimed: " + Gold(amount)
			+ " gold refunded to your reward gold (City Hall).");
		break;
	case BOUNTYEV_ERROR: {
		const char * p = "The bounty could not be placed.";
		switch (amount) {
		case BOUNTYERR_MIN:			p = "The minimum bounty is 50,000 gold."; break;
		case BOUNTYERR_NOTFOUND:	p = "That player isn't online. New bounties need the target to be online."; break;
		case BOUNTYERR_SELF:		p = "You can't put a bounty on yourself."; break;
		case BOUNTYERR_NOTALLOWED:	p = "Bounties can only be put on enemies and PKers."; break;
		case BOUNTYERR_LIMIT:		p = "You are already backing 10 bounties."; break;
		case BOUNTYERR_FULL:		p = "That bounty already has 16 backers."; break;
		case BOUNTYERR_MAX:			p = "A bounty can't go over 50,000,000 gold."; break;
		case BOUNTYERR_GOLD:		p = "You don't have enough gold (the bounty plus the 10% board fee)."; break;
		}
		AddEventList(p, 10);
		break; }
	}
}

//////////////////////////////////////////////////////////////////////
// Hover line

bool CGame::DrawBountyNameLine(short sX, short sY, const char * pName, int iStatus)
{
	const BountyEntry * e = BountyFor(pName);
	if (e == NULL || SameName(pName, m_cPlayerName)) return false;
	const Side side = (Side)((iStatus & STATUS_SIDE) >> 28);
	const bool bEnemy = (m_side != NEUTRAL && side != NEUTRAL && side != m_side);
	const bool bPKer = (iStatus & STATUS_PK) != 0;
	if (m_iPKCount > 0 || (!bEnemy && !bPKer)) return false;		// only shown to players who can claim it
	wsprintf(G_cTxt, "Bounty: %s", Commas(e->total).c_str());
	if (e->total >= 1000000) PutString2Centred(sX, sY, G_cTxt, 255, 110, 95);
	else PutString2Centred(sX, sY, G_cTxt, 240, 196, 100);
	return true;
}

//////////////////////////////////////////////////////////////////////
// The board

void CGame::OpenBountyBoard()
{
	EnableDialogBox(DIALOG_BOUNTY);
}

void CGame::DrawDialogBox_Bounty()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_BOUNTY];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int W = BOUNTYWIN_W, H = BOUNTYWIN_H;
	const bool bTop = dlg.IsTop();
	RECT rc;
	auto Hot = [&](int id) { BountyRect(id, &rc); OffsetRect(&rc, X, Y); return bTop && InRect(rc, ms::X, ms::Y); };

	R.FillPanel(X, Y, X + W, Y + H, 10, RGB(14, 16, 20), m_bDialogTrans ? 205 : 255, RGB(150, 128, 82), 150);	// solid unless see-through windows are on (a solid fill is much faster)
	R.TitleOut(X + 14, Y + 8, "BOUNTY BOARD", kGold, 15);

	// tabs
	static const char * const kTab[3] = { "Top bounties", "Placed by me", "On me" };
	for (int i = 0; i < 3; i++) {
		const bool bOn = Hot(BNT_TAB0 + i);
		if (i == m_iBountyTab) {
			R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(255, 255, 255), 22);
			R.FillPanel(rc.left + 6, rc.bottom - 2, rc.right - 6, rc.bottom, 1, kGold, 255);
		}
		else if (bOn) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(255, 255, 255), 12);
		const COLORREF c = (i == m_iBountyTab) ? kWhite : (bOn ? RGB(214, 218, 226) : kGrey);
		if (i == m_iBountyTab) Bold(R, rc.left + 11, rc.top + 2, kTab[i], c, 12);
		else R.TextOutSized(rc.left + 11, rc.top + 2, kTab[i], c, 12);
	}
	{
		const bool bOn = Hot(BNT_CLOSE);
		if (bOn) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
		R.TextOutSized(rc.left + 7, rc.top, "x", bOn ? kWhite : kGrey, 17);
	}
	R.FillPanel(X + 1, Y + 32, X + W - 1, Y + 33, 0, RGB(255, 255, 255), 26);

	// the list
	const int cNum = X + 14, cName = X + 32, cSide = X + 138, cLv = X + 212, cMap = X + 248, cTime = X + 374, cGold = X + W - 14;
	const int hy = Y + 40;
	R.TextOutSized(cNum, hy, "#", kDim, 9);
	R.TextOutSized(cName, hy, "TARGET", kDim, 9);
	R.TextOutSized(cSide, hy, "SIDE", kDim, 9);
	R.TextOutSized(cLv, hy, "LV", kDim, 9);
	R.TextOutSized(cMap, hy, "LAST SEEN", kDim, 9);
	R.TextOutSized(cTime, hy, "TIME LEFT", kDim, 9);
	R.TextOutSized(cGold - R.TextWidthSized("BOUNTY", 9), hy, "BOUNTY", kDim, 9);

	std::vector<int> list;
	BountyList(list);
	for (int r = 0; r < kRows; r++) {
		const int ry = Y + kRowY + r * kRowH;
		R.FillPanel(X + 8, ry, X + W - 8, ry + 1, 0, RGB(255, 255, 255), 14);
		if (r >= (int)list.size()) continue;
		const BountyEntry & e = m_bounties[list[r]];
		const bool bSel = SameName(e.name, m_cBountySel);
		const bool bOn = Hot(BNT_ROW0 + r);
		if (bSel) {
			R.FillPanel(X + 8, ry + 1, X + W - 8, ry + kRowH, 3, RGB(232, 192, 112), 30);
			R.FillPanel(X + 8, ry + 1, X + 10, ry + kRowH, 0, kGold, 255);
		}
		else if (bOn) R.FillPanel(X + 8, ry + 1, X + W - 8, ry + kRowH, 3, RGB(255, 255, 255), 14);

		char cTxt[64];
		wsprintf(cTxt, "%d", r + 1);
		R.TextOutSized(cNum, ry + 3, cTxt, kDim, 11);
		// offline targets are dimmed
		R.TextOut(cName, ry + 2, e.name, SameName(e.name, m_cPlayerName) ? kGold : (e.online ? (bSel ? kWhite : RGB(214, 218, 226)) : kDim));
		if (e.pk) Pill(R, cSide, ry + 3, "PKer", RGB(255, 150, 90));
		else if (e.side == ARESDEN) Pill(R, cSide, ry + 3, "ARESDEN", RGB(255, 125, 115));
		else if (e.side == ELVINE) Pill(R, cSide, ry + 3, "ELVINE", RGB(130, 170, 255));
		else Pill(R, cSide, ry + 3, "TRAVELLER", kGrey);
		wsprintf(cTxt, "%d", e.level);
		R.TextOutSized(cLv, ry + 3, e.level > 0 ? cTxt : "-", RGB(214, 218, 226), 11);
		char cMapName[64] = {};
		GetOfficialMapName(e.map, cMapName);
		R.TextOutSized(cMap, ry + 3, cMapName[0] ? cMapName : (e.map[0] ? e.map : "-"), kGrey, 11);
		R.TextOutSized(cTime, ry + 3, TimeLeft(e.dwExpireAt, m_dwCurTime).c_str(), kGrey, 11);
		const std::string g = Commas(e.total);
		Bold(R, cGold - R.TextWidthSized(g.c_str(), 11), ry + 3, g.c_str(), e.total >= 1000000 ? kRed : kGold, 11);
	}
	if (list.empty()) {
		const char * p = m_iBountyTab == 0 ? "No bounties right now." : (m_iBountyTab == 1 ? "You aren't backing any bounties." : "There is no bounty on you.");
		R.TextOutSized(cName, Y + kRowY + 4, p, kDim, 11);
	}

	// the picked bounty: who backs it, and putting gold up
	const int by0 = Y + 216, by1 = Y + 316;
	R.FillPanel(X + 10, by0, X + W - 10, by1, 8, RGB(0, 0, 0), 90, RGB(255, 255, 255), 20);
	const BountyEntry * sel = BountyFor(m_cBountySel);
	if (sel != NULL) {
		char cHead[64];
		wsprintf(cHead, "%s  -  BACKERS", sel->name);
		for (char * p = cHead; *p; p++) *p = (char)toupper((unsigned char)*p);
		R.TextOutSized(X + 22, by0 + 8, cHead, kDim, 9);
		const int n = (int)(std::min)(m_bountyBackers.size(), (size_t)4);
		for (int i = 0; i < n; i++) {
			const BountyBacker & b = m_bountyBackers[i];
			const int yy = by0 + 24 + i * 16;
			R.TextOutSized(X + 22, yy, b.name, SameName(b.name, m_cPlayerName) ? kGold : RGB(214, 218, 226), 11);
			const std::string g = Commas(b.gold);
			R.TextOutSized(X + 272 - R.TextWidthSized(g.c_str(), 11), yy, g.c_str(), kGold, 11);
			R.TextOutSized(X + 150, yy + 1, TimeLeft(b.dwExpireAt, m_dwCurTime).c_str(), kDim, 9);
		}
		if (m_bountyBackers.size() > 4) {
			wsprintf(cHead, "and %d more", (int)m_bountyBackers.size() - 4);
			R.TextOutSized(X + 22, by0 + 24 + 4 * 16, cHead, kDim, 9);
		}
	}
	else R.TextOutSized(X + 22, by0 + 8, "PICK A BOUNTY TO SEE WHO BACKS IT", kDim, 9);
	R.FillPanel(X + 284, by0 + 10, X + 285, by1 - 10, 0, RGB(255, 255, 255), 20);

	R.TextOutSized(X + 296, by0 + 8, sel ? "ADD TO THIS BOUNTY" : "GOLD", kDim, 9);
	auto Field = [&](int id, char * pBuf, const char * pHint) {
		const bool bActive = m_bInputStatus && m_pInputBuffer == pBuf;
		const bool bOn = Hot(id);
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(0, 0, 0), 150, bActive ? kGold : RGB(255, 255, 255), bActive ? 220 : (bOn ? 90 : 50));
		if (!bActive) {		// while typing, the game draws the text being typed
			if (pBuf[0]) R.TextOut(rc.left + 6, rc.top + 4, pBuf, kWhite);
			else R.TextOutSized(rc.left + 7, rc.top + 4, pHint, kDim, 11);
		}
	};
	auto Button = [&](int id, const char * pLabel, bool bGold, bool bEnabled) {
		const bool bOn = bEnabled && Hot(id);
		if (!bEnabled) BountyRect(id, &rc), OffsetRect(&rc, X, Y);
		if (bGold) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, bEnabled ? RGB(232, 188, 92) : RGB(90, 84, 70), bOn ? 255 : 225);
		else R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), bOn ? 36 : 16, RGB(255, 255, 255), bOn ? 80 : 45);
		const COLORREF c = bGold ? (bEnabled ? RGB(34, 26, 10) : RGB(150, 144, 130)) : (bEnabled ? (bOn ? kWhite : RGB(220, 224, 230)) : kDim);
		const int tw = R.TextWidthSized(pLabel, 12);
		if (bGold) Bold(R, (rc.left + rc.right - tw) / 2, rc.top + 3, pLabel, c, 12);
		else R.TextOutSized((rc.left + rc.right - tw) / 2, rc.top + 3, pLabel, c, 12);
	};
	Field(BNT_AMOUNT, m_cBountyAmount, "amount");
	Button(BNT_ADD, "Add gold", true, sel != NULL && !SameName(sel->name, m_cPlayerName));
	R.TextOutSized(X + 296, by0 + 54, "or place a new bounty:", kGrey, 10);
	Field(BNT_NAME, m_cBountyName, "player name");
	Button(BNT_PLACE, "Place bounty", false, true);

	// what the gold costs
	const uint32 amt = ParseGold(m_cBountyAmount);
	if (amt > 0) {
		char cCost[64];
		wsprintf(cCost, "costs %s with the fee", Commas((unsigned long)(amt + amt / 10)).c_str());
		R.TextOutSized(X + W - 20 - R.TextWidthSized(cCost, 9), by0 + 8, cCost, amt >= kMinBounty ? kDim : kRed, 9);
	}

	// you
	std::string sYou;
	const BountyEntry * me = BountyFor(m_cPlayerName);
	if (me != NULL) sYou = "On you: " + Commas(me->total) + " (" + TimeLeft(me->dwExpireAt, m_dwCurTime) + " left)";
	else sYou = "No bounty on you";
	int nBacked = 0;
	unsigned long dwBacked = 0;
	for (size_t i = 0; i < m_bounties.size(); i++) if (m_bounties[i].mine > 0) { nBacked++; dwBacked += m_bounties[i].mine; }
	char cYou[160];
	wsprintf(cYou, "%s     You back: %d (%s)     You claimed: %u (%s)", sYou.c_str(), nBacked, Commas(dwBacked).c_str(),
		m_dwBountyClaims, Commas(m_dwBountyClaimGold).c_str());
	R.TextOutSized(X + 14, Y + 322, cYou, me ? RGB(255, 170, 150) : RGB(200, 205, 213), 11);

	// the rules
	static const char * const kRules[2][4] = {
		{ "Min 50,000", "10% board fee", "7 days, refunded if unclaimed", "Enemies & PKers only" },
		{ "1 claim / target / 6h", "No guild, party or same-IP claims", "Paid at the City Hall", NULL } };
	for (int row = 0; row < 2; row++) {
		int x = X + 12;
		const int y = Y + 342 + row * 21;
		for (int i = 0; i < 4 && kRules[row][i]; i++) {
			const int w = R.TextWidthSized(kRules[row][i], 10) + 14;
			R.FillPanel(x, y, x + w, y + 17, 8, RGB(255, 255, 255), 18);
			R.TextOutSized(x + 7, y + 2, kRules[row][i], RGB(190, 196, 206), 10);
			x += w + 6;
		}
	}
}

void CGame::BountySubmit(bool bAddToSelected)
{
	const uint32 amt = ParseGold(m_cBountyAmount);
	char cName[12] = {};
	if (bAddToSelected) strncpy(cName, m_cBountySel, 10);
	else strncpy(cName, m_cBountyName, 10);
	if (cName[0] == 0) { AddEventList("Type the name of the player to put a bounty on.", 10); return; }
	if (amt < kMinBounty) { AddEventList("The minimum bounty is 50,000 gold.", 10); return; }
	char cGold[] = "Gold";
	if ((unsigned long long)GetItemCount(cGold) < (unsigned long long)amt + amt / 10) {
		AddEventList("You don't have enough gold (the bounty plus the 10% board fee).", 10);
		return;
	}
	bSendCommand(MSGID_REQUEST_BOUNTY, 1, NULL, (int)amt, 0, 0, cName);
	ZeroMemory(m_cBountyAmount, sizeof(m_cBountyAmount));
	if (!bAddToSelected) {
		strncpy(m_cBountySel, cName, 10);		// show it once the server puts it up
		ZeroMemory(m_cBountyName, sizeof(m_cBountyName));
		m_bountyBackers.clear();
	}
	PlaySound('E', 14, 5);
}

// Enter while typing in one of the board's fields.
bool CGame::BountyOnEnter()
{
	if (!m_bIsDialogEnabled[DIALOG_BOUNTY] || (m_pInputBuffer != m_cBountyAmount && m_pInputBuffer != m_cBountyName)) return false;
	const bool bName = (m_pInputBuffer == m_cBountyName);
	EndInputString();
	if (bName || m_cBountyName[0]) BountySubmit(false);
	else if (BountyFor(m_cBountySel) != NULL) BountySubmit(true);
	return true;
}

void CGame::DlgBoxClick_Bounty()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_BOUNTY];
	const short X = dlg.m_X, Y = dlg.m_Y;
	auto Hit = [&](int id) { RECT rc; BountyRect(id, &rc); OffsetRect(&rc, X, Y); return InRect(rc, ms::X, ms::Y); };
	const bool bTyping = m_bInputStatus && (m_pInputBuffer == m_cBountyAmount || m_pInputBuffer == m_cBountyName);

	if (Hit(BNT_CLOSE)) {
		if (bTyping) EndInputString();
		DisableDialogBox(DIALOG_BOUNTY);
		PlaySound('E', 14, 5);
		return;
	}
	for (int i = 0; i < 3; i++)
		if (Hit(BNT_TAB0 + i)) { m_iBountyTab = i; PlaySound('E', 14, 5); return; }
	if (Hit(BNT_AMOUNT) || Hit(BNT_NAME)) {
		if (bTyping) EndInputString();
		RECT rc;
		const bool bAmt = Hit(BNT_AMOUNT);
		BountyRect(bAmt ? BNT_AMOUNT : BNT_NAME, &rc);
		OffsetRect(&rc, X, Y);
		StartInputString(rc.left + 6, rc.top + 4, 11, bAmt ? m_cBountyAmount : m_cBountyName);
		return;
	}
	if (bTyping) EndInputString();
	if (Hit(BNT_ADD)) {
		const BountyEntry * sel = BountyFor(m_cBountySel);
		if (sel != NULL && !SameName(sel->name, m_cPlayerName)) BountySubmit(true);
		return;
	}
	if (Hit(BNT_PLACE)) { BountySubmit(false); return; }

	std::vector<int> list;
	BountyList(list);
	for (int r = 0; r < kRows && r < (int)list.size(); r++)
		if (Hit(BNT_ROW0 + r)) {
			const BountyEntry & e = m_bounties[list[r]];
			ZeroMemory(m_cBountySel, sizeof(m_cBountySel));
			strncpy(m_cBountySel, e.name, 10);
			m_bountyBackers.clear();
			bSendCommand(MSGID_REQUEST_BOUNTY, 2, NULL, 0, 0, 0, m_cBountySel);
			PlaySound('E', 14, 5);
			return;
		}
}
