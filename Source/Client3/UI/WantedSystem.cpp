// WantedSystem.cpp: Most Wanted, enemy-kill streaks (client side). The server side is
// HG Server\Wanted.cpp, which explains the rules.
//
//   - messages from the server: who is wanted (NOTIFY_WANTEDINFO), your own streak
//     (NOTIFY_WANTEDSTATS), where an enemy wanted player is (NOTIFY_WANTEDPOS) and who brought
//     a wanted player down (NOTIFY_WANTEDENDED)
//   - gold stars above wanted players' heads (always shown), and a line under their name
//   - your streak counter at the top of the screen
//   - red stars on the minimap for enemy wanted players at 4 and 5 stars
//   - the Most Wanted board (character window button, or /wanted)
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"

#include <algorithm>
#include <string>


namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kStar	= RGB(255, 200, 70);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kOrange	= RGB(255, 170, 100);

// Chat colours for announcements (as in the mockup)
const COLORREF kChatTag		= RGB(232, 176, 80);	// [Bounty]
const COLORREF kChatTagW	= RGB(255, 140, 90);	// [Wanted]
const COLORREF kChatTarget	= RGB(255, 130, 120);	// the hunted
const COLORREF kChatPlayer	= RGB(236, 238, 242);	// whoever put gold up
const COLORREF kChatHunter	= RGB(150, 190, 255);	// whoever claimed / brought them down
const COLORREF kChatMe		= RGB(255, 214, 120);	// you
const COLORREF kChatGold	= RGB(240, 196, 100);
const COLORREF kChatBigGold	= RGB(255, 110, 95);	// 1,000,000 and up
const COLORREF kChatLevel	= RGB(255, 170, 100);	// "Notorious"

const int kLevelKills[6] = { 0, 5, 10, 20, 35, 50 };
const char * const kLevelGold[6] = { "", "50,000", "150,000", "400,000", "1,000,000", "2,500,000" };

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

int LevelFor(int iStreak)
{
	int lv = 0;
	for (int i = 1; i <= 5; i++) if (iStreak >= kLevelKills[i]) lv = i;
	return lv;
}

COLORREF SideColor(int iSide)
{
	return iSide == ARESDEN ? RGB(255, 150, 140) : (iSide == ELVINE ? RGB(160, 196, 255) : RGB(210, 210, 210));
}

} // namespace

const char * CGame::WantedLevelName(int iLevel)
{
	static const char * const k[6] = { "", "Wanted", "Hunted", "Notorious", "Most Wanted", "Legend" };
	return (iLevel >= 0 && iLevel <= 5) ? k[iLevel] : "";
}

int CGame::WantedLevelOf(const char * pName)
{
	if (pName == NULL || pName[0] == 0) return 0;
	if (SameName(pName, m_cPlayerName)) return LevelFor(m_iWantedStreak);
	for (size_t i = 0; i < m_wanted.size(); i++)
		if (SameName(m_wanted[i].name, pName)) return m_wanted[i].level;
	return 0;
}

// a row of n small stars centred on (cx, cy), with a dark edge so they read on any ground
void CGame::DrawWantedStars(int cx, int cy, int n, float fR, int iTotal)
{
	const float gap = fR * 2.3f;
	const int total = (iTotal > 0) ? iTotal : n;
	const float x0 = cx - gap * (total - 1) / 2.f;
	for (int i = 0; i < total; i++) {
		const float x = x0 + gap * i;
		m_Renderer.FillStar(x, (float)cy + 0.6f, fR + 1.2f, RGB(0, 0, 0), 200);
		m_Renderer.FillStar(x, (float)cy, fR, i < n ? kStar : RGB(70, 74, 82), 255);
	}
}

//////////////////////////////////////////////////////////////////////
// Messages from the server

void CGame::NotifyMsg_WantedInfo(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 lv, side, flag;
	uint16 streak;
	uint32 worth;
	char cName[12] = {}, cMap[12] = {};
	Pop(cp, lv); Pop(cp, side); Pop(cp, streak); Pop(cp, worth); Pop(cp, flag);
	Pop(cp, cName, 10); Pop(cp, cMap, 10);

	int iOld = 0;
	size_t i = 0;
	for (; i < m_wanted.size(); i++) if (SameName(m_wanted[i].name, cName)) break;
	if (i < m_wanted.size()) iOld = m_wanted[i].level;
	if (lv == 0) {		// off the board (streak ended, decayed or offline)
		if (i < m_wanted.size()) m_wanted.erase(m_wanted.begin() + i);
		return;
	}
	if (i == m_wanted.size()) { m_wanted.push_back(WantedEntry()); memset(&m_wanted.back(), 0, sizeof(WantedEntry)); }
	WantedEntry & e = m_wanted[i];
	memcpy(e.name, cName, 10);
	memcpy(e.map, cMap, 10);
	e.level = lv; e.side = side; e.streak = streak; e.worth = worth;

	if (flag == 1 && lv > iOld) {
		char cMapName[64] = {};
		GetOfficialMapName(cMap, cMapName);
		if (SameName(cName, m_cPlayerName)) {
			wsprintf(G_cTxt, "You are now %s (%d stars)! %s gold to whoever brings you down.", WantedLevelName(lv), lv, kLevelGold[lv]);
			SetTopMsg(G_cTxt, 6);
		}
		else if (lv >= 3) {
			wsprintf(G_cTxt, "%s (%s) is now %s - worth %s gold to the enemy!", cName, sideName[side < MAXSIDES ? side : 0], WantedLevelName(lv), kLevelGold[lv]);
			SetTopMsg(G_cTxt, 5);
		}
		char cKills[16];
		wsprintf(cKills, "%d", (int)streak);
		AddColoredEvent(ChatSpan(kChatTagW, "[Wanted]") + " " + ChatSpan(SameName(cName, m_cPlayerName) ? kChatMe : kChatTarget, cName)
			+ " (" + sideName[side < MAXSIDES ? side : 0] + ") is now " + ChatSpan(kChatLevel, WantedLevelName(lv)) + ": " + cKills
			+ " enemy kills in a row, last seen in " + (cMapName[0] ? cMapName : cMap) + ". Worth "
			+ ChatSpan(lv >= 4 ? kChatBigGold : kChatGold, kLevelGold[lv]) + " gold.");
	}
}

void CGame::NotifyMsg_WantedStats(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint16 streak, best;
	uint32 ended;
	Pop(cp, streak); Pop(cp, best); Pop(cp, ended);
	m_iWantedStreak = streak;
	m_iWantedBest = best;
	m_iWantedEnded = (int)ended;
}

void CGame::NotifyMsg_WantedPos(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 lv;
	uint16 x, y;
	char cName[12] = {};
	Pop(cp, lv); Pop(cp, x); Pop(cp, y); Pop(cp, cName, 10);
	for (size_t i = 0; i < m_wantedPos.size(); i++)
		if (SameName(m_wantedPos[i].name, cName)) { m_wantedPos.erase(m_wantedPos.begin() + i); break; }
	WantedPos p;
	memset(&p, 0, sizeof(p));
	memcpy(p.name, cName, 10);
	memcpy(p.map, m_cMapName, 10);
	p.x = x; p.y = y; p.level = lv; p.dwTime = m_dwCurTime;
	m_wantedPos.push_back(p);
}

void CGame::NotifyMsg_WantedEnded(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 lv, nHelpers, killerSide;
	uint16 streak;
	uint32 gold, share;
	char cKiller[12] = {}, cVictim[12] = {};
	Pop(cp, lv); Pop(cp, streak); Pop(cp, gold); Pop(cp, nHelpers); Pop(cp, share); Pop(cp, killerSide);
	Pop(cp, cKiller, 10); Pop(cp, cVictim, 10);
	std::string helpers;
	bool bMeHelper = false;
	for (int i = 0; i < nHelpers && i < 8; i++) {
		char cH[12] = {};
		Pop(cp, cH, 10);
		if (!helpers.empty()) helpers += ", ";
		helpers += cH;
		if (SameName(cH, m_cPlayerName)) bMeHelper = true;
	}

	wsprintf(G_cTxt, "%s brought down %s (%s)!", cKiller, cVictim, WantedLevelName(lv));
	SetTopMsg(G_cTxt, 6);
	{
		const std::string tag = ChatSpan(kChatTagW, "[Wanted]") + " ";
		auto Who = [this](const char * p, COLORREF c) { return ChatSpan(SameName(p, m_cPlayerName) ? kChatMe : c, p); };
		auto Gold = [](uint32 g) { return ChatSpan(g >= 1000000 ? kChatBigGold : kChatGold, Commas(g)); };
		char cKills[16];
		wsprintf(cKills, "%d", (int)streak);
		AddColoredEvent(tag + Who(cKiller, kChatHunter) + " ended " + Who(cVictim, kChatTarget) + "'s " + cKills + "-kill streak ("
			+ ChatSpan(kChatLevel, WantedLevelName(lv)) + "): +" + Gold(gold) + " gold.");
		if (nHelpers > 0) {
			std::string who;
			for (size_t a = 0, b; a < helpers.size(); a = b + 2) {
				b = helpers.find(", ", a);
				if (b == std::string::npos) b = helpers.size();
				if (!who.empty()) who += ", ";
				who += Who(helpers.substr(a, b - a).c_str(), kChatHunter);
			}
			AddColoredEvent(tag + "Shared with the hunt: " + who + ", +" + Gold(share) + " gold each.");
		}
		if (SameName(cKiller, m_cPlayerName) || bMeHelper)
			AddColoredEvent(tag + "Your " + Gold(SameName(cKiller, m_cPlayerName) ? gold : share)
				+ " gold has been added to your reward gold: collect it at the City Hall.");
	}
	for (size_t i = 0; i < m_wantedPos.size(); i++)
		if (SameName(m_wantedPos[i].name, cVictim)) { m_wantedPos.erase(m_wantedPos.begin() + i); break; }
}

//////////////////////////////////////////////////////////////////////
// A skull over Most Wanted and Legend players' heads (the stars are drawn above the hover name,
// in DrawObjectName)

// Called for every character drawn (DrawObjects); the skulls are drawn on top afterwards.
void CGame::WantedNoteObject(int ix, int iy, const char * pName, short sOwnerType, int iStatus)
{
	if (sOwnerType < 1 || sOwnerType > 6) return;
	const int lv = WantedLevelOf(pName);
	if (lv <= 0) return;
	const bool bMe = SameName(pName, m_cPlayerName);
	if (!bMe && (iStatus & STATUS_INVISIBILITY) != 0) return;
	if (lv < 4) return;
	WantedTag t;
	memset(&t, 0, sizeof(t));
	const bool bRect = (m_rcBodyRect.right > m_rcBodyRect.left && m_rcBodyRect.bottom > m_rcBodyRect.top);
	t.x = bRect ? (m_rcBodyRect.left + m_rcBodyRect.right) / 2 : ix;
	t.y = bRect ? m_rcBodyRect.top : iy - 72;
	t.level = lv;
	t.status = iStatus;
	t.me = bMe;
	memcpy(t.name, pName, 10);
	m_wantedTags.push_back(t);
}

// A small bone-white skull centred on (cx, cy), about 16 x 19 px; red eyes for a Legend.
void CGame::DrawWantedSkull(int cx, int cy, bool bLegend)
{
	CRenderer & R = m_Renderer;
	const COLORREF bone = RGB(238, 232, 214), dark = RGB(30, 24, 22);
	const COLORREF eye = bLegend ? RGB(220, 40, 30) : dark;
	// dark edge
	R.FillPanel(cx - 8, cy - 10, cx + 8, cy + 5, 8, RGB(0, 0, 0), 210);
	R.FillPanel(cx - 6, cy + 1, cx + 6, cy + 9, 3, RGB(0, 0, 0), 210);
	// cranium and jaw
	R.FillPanel(cx - 7, cy - 9, cx + 7, cy + 4, 7, bone, 255);
	R.FillPanel(cx - 5, cy + 2, cx + 5, cy + 8, 2, bone, 255);
	// eyes, nose, teeth
	if (bLegend) {
		R.FillPanel(cx - 6, cy - 5, cx, cy + 1, 3, RGB(255, 60, 40), 90);
		R.FillPanel(cx, cy - 5, cx + 6, cy + 1, 3, RGB(255, 60, 40), 90);
	}
	R.FillPanel(cx - 5, cy - 4, cx - 1, cy, 2, eye, 255);
	R.FillPanel(cx + 1, cy - 4, cx + 5, cy, 2, eye, 255);
	R.FillPanel(cx - 1, cy + 1, cx + 1, cy + 3, 1, dark, 255);
	for (int k = -1; k <= 1; k++) R.FillPanel(cx + k * 2, cy + 5, cx + k * 2 + 1, cy + 8, 0, dark, 200);
}

void CGame::DrawWantedTags()
{
	for (size_t i = 0; i < m_wantedTags.size(); i++) {
		const WantedTag & t = m_wantedTags[i];
		if (t.level >= 4) DrawWantedSkull(t.x, t.y - 24, t.level >= 5);
	}
	m_wantedTags.clear();
}

// The line under a wanted player's name and town when the mouse is on them: level (streak).
bool CGame::DrawWantedNameLine(short sX, short sY, const char * pName)
{
	const int lv = WantedLevelOf(pName);
	if (lv <= 0) return false;
	int iStreak = 0;
	if (SameName(pName, m_cPlayerName)) iStreak = m_iWantedStreak;
	else for (size_t i = 0; i < m_wanted.size(); i++) if (SameName(m_wanted[i].name, pName)) { iStreak = m_wanted[i].streak; break; }
	wsprintf(G_cTxt, "%s (%d)", WantedLevelName(lv), iStreak);
	PutString2Centred(sX, sY, G_cTxt, 255, 170, 100);
	return true;
}

//////////////////////////////////////////////////////////////////////
// Your streak counter

void CGame::DrawWantedCounter()
{
	if (m_iWantedStreak <= 0) return;
	CRenderer & R = m_Renderer;
	const int lv = LevelFor(m_iWantedStreak);
	const int W = 268, H = 44, X = (GAME_W - W) / 2, Y = 34;
	R.FillPanel(X, Y, X + W, Y + H, 8, RGB(14, 16, 20), 205, RGB(150, 128, 82), 130);
	DrawWantedStars(X + 32, Y + 12, lv, 4.f, 5);
	Bold(R, X + 62, Y + 5, lv > 0 ? WantedLevelName(lv) : "Streak", lv > 0 ? kOrange : kGrey, 12);
	char cKills[48];
	wsprintf(cKills, "%d kill%s in a row", m_iWantedStreak, m_iWantedStreak == 1 ? "" : "s");
	R.TextOutSized(X + W - 10 - R.TextWidthSized(cKills, 11), Y + 6, cKills, kGrey, 11);
	// progress to the next level
	const int next = (lv < 5) ? kLevelKills[lv + 1] : 0, base = kLevelKills[lv];
	R.FillPanel(X + 10, Y + 24, X + W - 10, Y + 28, 2, RGB(255, 255, 255), 28);
	const int fw = (lv < 5) ? (W - 20) * (m_iWantedStreak - base) / (std::max)(1, next - base) : (W - 20);
	if (fw >= 3) R.FillPanel(X + 10, Y + 24, X + 10 + fw, Y + 28, 2, RGB(240, 150, 70), 255);
	char cLeft[64], cRight[64];
	if (lv < 5) wsprintf(cLeft, "%s at %d", WantedLevelName(lv + 1), next);
	else strcpy(cLeft, "Top level");
	if (lv > 0) wsprintf(cRight, "Worth %s to the enemy", kLevelGold[lv]);
	else strcpy(cRight, "Not wanted yet");
	R.TextOutSized(X + 10, Y + 30, cLeft, RGB(130, 136, 144), 10);
	R.TextOutSized(X + W - 10 - R.TextWidthSized(cRight, 10), Y + 30, cRight, lv > 0 ? kGold : RGB(130, 136, 144), 10);
}

//////////////////////////////////////////////////////////////////////
// Minimap: enemy wanted players at 4 and 5 stars (shown for 8 seconds after each update)

void CGame::DrawWantedOnMinimap(short sX, short sY, short shX, short shY, bool bZoom)
{
	if (m_wantedPos.empty() || m_pMapData == NULL) return;
	for (size_t i = 0; i < m_wantedPos.size(); ) {
		if (m_dwCurTime - m_wantedPos[i].dwTime > 8000) { m_wantedPos.erase(m_wantedPos.begin() + i); continue; }
		i++;
	}
	for (size_t i = 0; i < m_wantedPos.size(); i++) {
		const WantedPos & p = m_wantedPos[i];
		if (strncmp(p.map, m_cMapName, 10) != 0) continue;
		int x, y;
		if (bZoom) {
			if (p.x < shX || p.x > shX + 128 || p.y < shY || p.y > shY + 128) continue;
			x = sX + p.x - shX; y = sY + p.y - shY;
		}
		else {
			if (m_pMapData->m_sMapSizeX <= 0 || m_pMapData->m_sMapSizeY <= 0) continue;
			x = sX + (p.x * 128) / m_pMapData->m_sMapSizeX;
			y = sY + (p.y * 128) / m_pMapData->m_sMapSizeY;
		}
		const int pulse = (int)((m_dwCurTime / 120) % 6);
		m_Renderer.FillPanel(x - 6 - pulse, y - 6 - pulse, x + 7 + pulse, y + 7 + pulse, 6 + pulse, RGB(255, 80, 50), 70 - pulse * 10);
		m_Renderer.FillStar((float)x + 0.5f, (float)y + 0.8f, 5.6f, RGB(0, 0, 0), 220);
		m_Renderer.FillStar((float)x + 0.5f, (float)y + 0.5f, 4.6f, RGB(255, 90, 60), 255);
	}
}

//////////////////////////////////////////////////////////////////////
// The Most Wanted board

void CGame::DrawDialogBox_MostWanted()
{
	CRenderer & R = m_Renderer;
	const short X = m_dialogBoxes[DIALOG_MOSTWANTED].m_X, Y = m_dialogBoxes[DIALOG_MOSTWANTED].m_Y;
	const int W = WANTEDWIN_W, H = WANTEDWIN_H;
	const bool bTop = m_dialogBoxes[DIALOG_MOSTWANTED].IsTop();

	R.StonePanel(X, Y, X + W, Y + H);	// VALORIA stone (CRenderer::StoneBox)
	R.TitleOut(X + 14, Y + 8, "MOST WANTED", kGold, 15);
	RECT rc;
	SetRect(&rc, X + W - 28, Y + 4, X + W - 6, Y + 26);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 7, rc.top, "x", bOnClose ? kWhite : kGrey, 17);
	R.FillPanel(X + 1, Y + 30, X + W - 1, Y + 31, 0, RGB(255, 255, 255), 26);

	// you
	char cTxt[128];
	wsprintf(cTxt, "Your streak: %d     Best: %d     Streaks ended: %d", m_iWantedStreak, m_iWantedBest, m_iWantedEnded);
	R.TextOutSized(X + 14, Y + 38, cTxt, RGB(200, 205, 213), 12);

	// the two sides, top streaks first
	std::vector<WantedEntry> list = m_wanted;
	std::sort(list.begin(), list.end(), [](const WantedEntry & a, const WantedEntry & b) { return a.streak > b.streak; });
	const int kSide[2] = { ARESDEN, ELVINE };
	const char * const kHead[2] = { "ARESDEN  -  WANTED BY ELVINE", "ELVINE  -  WANTED BY ARESDEN" };
	int y = Y + 60;
	for (int s = 0; s < 2; s++) {
		Bold(R, X + 14, y, kHead[s], SideColor(kSide[s]), 11);
		y += 17;
		const int cName = X + 14, cStars = X + 112, cLevel = X + 160, cStreak = X + 246, cMap = X + 284, cWorth = X + W - 14;
		R.TextOutSized(cName, y, "PLAYER", RGB(120, 126, 136), 9);
		R.TextOutSized(cLevel, y, "LEVEL", RGB(120, 126, 136), 9);
		R.TextOutSized(cStreak, y, "KILLS", RGB(120, 126, 136), 9);
		R.TextOutSized(cMap, y, "LAST SEEN", RGB(120, 126, 136), 9);
		R.TextOutSized(cWorth - R.TextWidthSized("WORTH", 9), y, "WORTH", RGB(120, 126, 136), 9);
		y += 14;
		int n = 0;
		for (size_t i = 0; i < list.size() && n < 4; i++) {
			const WantedEntry & e = list[i];
			if (e.side != kSide[s]) continue;
			R.FillPanel(X + 10, y - 1, X + W - 10, y, 0, RGB(255, 255, 255), 16);
			R.TextOut(cName, y + 2, e.name, SameName(e.name, m_cPlayerName) ? kGold : kWhite);
			DrawWantedStars(cStars + 18, y + 9, e.level, 3.4f, 5);
			R.TextOutSized(cLevel, y + 3, WantedLevelName(e.level), kOrange, 11);
			wsprintf(cTxt, "%d", e.streak);
			R.TextOutSized(cStreak, y + 3, cTxt, kWhite, 11);
			char cMapName[64] = {};
			GetOfficialMapName(e.map, cMapName);
			R.TextOutSized(cMap, y + 3, cMapName[0] ? cMapName : e.map, kGrey, 11);
			const std::string g = Commas(e.worth);
			Bold(R, cWorth - R.TextWidthSized(g.c_str(), 11), y + 3, g.c_str(), kGold, 11);
			y += 19; n++;
		}
		if (n == 0) {
			R.TextOutSized(cName, y + 3, "Nobody is wanted right now.", RGB(120, 126, 136), 11);
			y += 19;
		}
		y = (s == 0) ? (std::max)(y + 8, Y + 60 + 17 + 14 + 4 * 19 + 10) : y;
	}

	// rewards
	const int fy = Y + H - 58;
	R.Slate(X + 10, fy, X + W - 10, Y + H - 10);
	R.TextOutSized(X + 20, fy + 6, "GOLD FOR BRINGING ONE DOWN", RGB(120, 126, 136), 9);
	int x = X + 20;
	for (int lv = 1; lv <= 5; lv++) {
		DrawWantedStars(x + 3, fy + 26, 1, 3.6f);
		wsprintf(cTxt, "%d kills", kLevelKills[lv]);
		R.TextOutSized(x + 10, fy + 19, cTxt, kGrey, 10);
		R.TextOutSized(x + 10, fy + 31, kLevelGold[lv], kGold, 10);
		x += (W - 40) / 5;
	}
}

void CGame::DlgBoxClick_MostWanted()
{
	if (m_dialogBoxes[DIALOG_MOSTWANTED].OnButton() == 1) {
		DisableDialogBox(DIALOG_MOSTWANTED);
		PlaySound('E', 14, 5);
	}
}
