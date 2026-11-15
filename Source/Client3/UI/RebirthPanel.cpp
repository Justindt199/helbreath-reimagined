// RebirthPanel.cpp: rebirth (client side). The rules are in HG Server\Rebirth.cpp.
//
//   - the Rebirth window (City Hall menu): your rebirth level, what the next one needs and does,
//     and the Rebirth / Normal Mode / Resume buttons (each asks for a second click to confirm)
//   - messages from the server: your rebirth state (NOTIFY_REBIRTHSTATS), other players' rebirth
//     levels (NOTIFY_REBIRTHINFO, for the hover line) and results / announcements
//     (NOTIFY_REBIRTHEVENT)
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"

#include <cmath>
#include <string>

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(120, 126, 136);
const COLORREF kPurple	= RGB(196, 160, 255);
const COLORREF kOk		= RGB(110, 210, 120);
const COLORREF kBad		= RGB(255, 110, 100);

const int kMaxRebirth = 20;
const int kMaxLevel = MAXLEVEL;	// the max level (Shared\version.h)
const std::string kMaxLevelText = std::to_string(MAXLEVEL);
const unsigned kSwitchGold = 50000;

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

unsigned RebirthGold(int R)	{ unsigned g = 200000u * (unsigned)R; return g > 1000000u ? 1000000u : g; }

} // namespace

int CGame::RebirthStatBonus() const
{
	return 3 * ((m_iRebirthMode == 2) ? (m_iRebirth - 1) : m_iRebirth);
}

int CGame::RebirthOf(const char * pName) const
{
	if (pName == NULL || pName[0] == 0) return 0;
	if (SameName(pName, m_cPlayerName)) return m_iRebirth;
	for (size_t i = 0; i < m_rebirthOf.size(); i++)
		if (SameName(m_rebirthOf[i].name, pName)) return m_rebirthOf[i].level;
	return 0;
}

//////////////////////////////////////////////////////////////////////
// Messages from the server

void CGame::NotifyMsg_RebirthStats(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 r, mode;
	uint16 alt;
	Pop(cp, r); Pop(cp, mode); Pop(cp, alt);
	m_iRebirth = r;
	m_iRebirthMode = mode;
	m_iRebirthAltLevel = alt;
}

void CGame::NotifyMsg_RebirthInfo(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	RebirthMark m;
	memset(&m, 0, sizeof(m));
	uint8 r;
	Pop(cp, m.name, 10);
	Pop(cp, r);
	m.level = r;
	for (size_t i = 0; i < m_rebirthOf.size(); i++)
		if (SameName(m_rebirthOf[i].name, m.name)) { m_rebirthOf[i].level = m.level; return; }
	if (m.level > 0) m_rebirthOf.push_back(m);
}

void CGame::NotifyMsg_RebirthEvent(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 code, r;
	char cName[12] = {};
	Pop(cp, code); Pop(cp, r); Pop(cp, cName, 10);
	const bool bMe = SameName(cName, m_cPlayerName);
	const std::string tag = ChatSpan(kPurple, "[Rebirth]") + " ";
	const std::string who = ChatSpan(bMe ? RGB(255, 214, 120) : kWhite, cName);
	char cR[16];
	wsprintf(cR, "Rebirth %d", (int)r);

	switch (code) {
	case REBIRTHRES_REBORN:
		AddColoredEvent(tag + who + " has been reborn: " + ChatSpan(kPurple, cR) + "!");
		if (bMe) {
			wsprintf(G_cTxt, "You have been reborn (Rebirth %d)! Logging out to load your new character - log in again.", (int)r);
			SetTopMsg(G_cTxt, 8);
			DisableDialogBox(DIALOG_REBIRTH);
		}
		break;
	case REBIRTHRES_COMPLETED:
		AddColoredEvent(tag + who + " finished " + ChatSpan(kPurple, cR) + " and is back at level " + kMaxLevelText + "!");
		if (bMe) {
			wsprintf(G_cTxt, "Rebirth %d complete!", (int)r);
			SetTopMsg(G_cTxt, 6);
		}
		break;
	case REBIRTHRES_PAUSED:
		SetTopMsg("Normal Mode: your rebirth is paused. Logging out to load your full-level character - log in again.", 8);
		DisableDialogBox(DIALOG_REBIRTH);
		break;
	case REBIRTHRES_RESUMED:
		SetTopMsg("Rebirth resumed. Logging out to load your rebirth character - log in again.", 8);
		DisableDialogBox(DIALOG_REBIRTH);
		break;
	default: {
		const char * p = "Rebirth isn't possible right now.";
		switch (code) {
		case REBIRTHRES_LEVEL:			wsprintf(G_cTxt, "You must be level %d to be reborn.", kMaxLevel); p = G_cTxt; break;
		case REBIRTHRES_BUILDING:		p = "You must be inside the City Hall."; break;
		case REBIRTHRES_GOLD:			p = "You don't have enough gold."; break;
		case REBIRTHRES_MAJESTIC:		p = "You don't have enough Majestic points."; break;
		case REBIRTHRES_WEIGHT:			p = "You are carrying too much: store some items in the warehouse first."; break;
		case REBIRTHRES_MAX:			p = "You have reached the highest rebirth level (20)."; break;
		case REBIRTHRES_DEAD:			p = "You can't do that while dead."; break;
		case REBIRTHRES_PAUSEDFIRST:	p = "Your rebirth is paused: resume it first."; break;
		case REBIRTHRES_NOTREBIRTHING:	p = "You have no rebirth to pause or resume."; break;
		case REBIRTHRES_BUSY:			p = "Please wait a moment."; break;
		}
		AddEventList(p, 10);
		break; }
	}
}

// Hover line under the name: "Rebirth N".
bool CGame::DrawRebirthNameLine(short sX, short sY, const char * pName)
{
	const int r = RebirthOf(pName);
	if (r <= 0) return false;
	wsprintf(G_cTxt, "Rebirth %d", r);
	PutString2Centred(sX, sY, G_cTxt, 196, 160, 255);
	return true;
}

//////////////////////////////////////////////////////////////////////
// The window

void CGame::RebirthButtonRect(int id, RECT * pRc)
{
	const int W = REBIRTHWIN_W, H = REBIRTHWIN_H;
	if (id == 1) SetRect(pRc, W - 28, 4, W - 6, 26);					// close
	else SetRect(pRc, 16, H - 44, W - 16, H - 16);						// the action button
}

void CGame::DrawDialogBox_Rebirth()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_REBIRTH];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int W = REBIRTHWIN_W, H = REBIRTHWIN_H;
	const bool bTop = dlg.IsTop();
	RECT rc;

	R.FillPanel(X, Y, X + W, Y + H, 10, RGB(14, 16, 20), m_bDialogTrans ? 205 : 255, RGB(150, 128, 82), 150);	// solid unless see-through windows are on (a solid fill is much faster)
	R.TitleOut(X + 14, Y + 8, "REBIRTH", kGold, 15);
	RebirthButtonRect(1, &rc); OffsetRect(&rc, X, Y);
	const bool bOnClose = bTop && InRect(rc, ms::X, ms::Y);
	if (bOnClose) R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 5, RGB(255, 255, 255), 34);
	R.TextOutSized(rc.left + 7, rc.top, "x", bOnClose ? kWhite : kGrey, 17);
	R.FillPanel(X + 1, Y + 32, X + W - 1, Y + 33, 0, RGB(255, 255, 255), 26);

	// where you are
	char cTxt[160];
	const int mcx = X + W - 50, mcy = Y + 72, mr = 28;
	R.FillPanel(mcx - mr, mcy - mr, mcx + mr, mcy + mr, mr, RGB(30, 22, 40), 240, kPurple, 220);
	R.TextOutSized(mcx - R.TextWidthSized("REBIRTH", 9) / 2, mcy - 20, "REBIRTH", RGB(170, 140, 220), 9);
	wsprintf(cTxt, "%d", m_iRebirth);
	Bold(R, mcx - R.TextWidthSized(cTxt, 22) / 2, mcy - 8, cTxt, kPurple, 22);

	const int R0 = m_iRebirth, Rn = m_iRebirth + 1;
	int y = Y + 44;
	if (m_iRebirthMode == 0) {
		wsprintf(cTxt, "Rebirth level %d of %d", R0, kMaxRebirth);
		Bold(R, X + 16, y, cTxt, kWhite, 13);
		y += 20;
		R.TextOutSized(X + 16, y, R0 >= kMaxRebirth ? "You have reached the highest rebirth." : "Start over at level 1, stronger each time.", kGrey, 11);
	}
	else {
		wsprintf(cTxt, m_iRebirthMode == 1 ? "Rebirth %d in progress" : "Rebirth %d paused", R0);
		Bold(R, X + 16, y, cTxt, kWhite, 13);
		y += 20;
		const int iRebLevel = (m_iRebirthMode == 1) ? m_iLevel : m_iRebirthAltLevel;
		wsprintf(cTxt, m_iRebirthMode == 1 ? "Level %d of %d. Normal Mode lets you play your level %d character."
			: "Your rebirth waits at level %d of %d.", iRebLevel, kMaxLevel, m_iRebirthAltLevel);
		R.TextOutSized(X + 16, y, cTxt, kGrey, 10);
	}
	y = Y + 110;

	// requirements
	char cGold[] = "Gold";
	const unsigned long dwGold = (unsigned long)GetItemCount(cGold);
	const bool bInHall = (_strnicmp(m_cMapName, "cityhall", 8) == 0);
	struct Req { std::string text, have; bool ok, blocking; };
	std::vector<Req> reqs;
	if (m_iRebirthMode == 0) {
		const unsigned g = RebirthGold(Rn);
		reqs.push_back({ "Level " + kMaxLevelText, std::to_string(m_iLevel), m_iLevel >= kMaxLevel, true });
		reqs.push_back({ "Inside the City Hall", bInHall ? "yes" : "no", bInHall, true });
		reqs.push_back({ Commas(g) + " gold", Commas(dwGold), dwGold >= g, true });
		reqs.push_back({ std::to_string(5 * Rn) + " Majestic points", std::to_string(m_iGizonItemUpgradeLeft), m_iGizonItemUpgradeLeft >= 5 * Rn, true });
		// the server checks this after taking the gold, so it doesn't block the button here
		reqs.push_back({ "Carrying 55 weight or less", std::to_string((int)m_weight), (int)m_weight <= 55, false });
	}
	else {
		reqs.push_back({ "Inside the City Hall", bInHall ? "yes" : "no", bInHall, true });
		reqs.push_back({ Commas(kSwitchGold) + " gold", Commas(dwGold), dwGold >= kSwitchGold, true });
		reqs.push_back({ "What you carry fits the other character", "", true, false });
	}
	R.TextOutSized(X + 16, y, m_iRebirthMode == 0 ? "TO BE REBORN" : (m_iRebirthMode == 1 ? "TO SWITCH TO NORMAL MODE" : "TO RESUME YOUR REBIRTH"), kDim, 9);
	y += 16;
	bool bAllOk = true;
	for (size_t i = 0; i < reqs.size(); i++) {
		const Req & q = reqs[i];
		R.FillPanel(X + 18, y + 4, X + 26, y + 12, 4, q.ok ? kOk : kBad, 255);
		R.TextOutSized(X + 34, y, q.text.c_str(), q.ok ? RGB(214, 218, 226) : RGB(240, 190, 180), 11);
		if (!q.have.empty()) R.TextOutSized(X + W - 16 - R.TextWidthSized(q.have.c_str(), 11), y, q.have.c_str(), q.ok ? kGrey : kBad, 11);
		if (!q.ok && q.blocking) bAllOk = false;
		y += 17;
	}
	if (m_iRebirthMode == 0 && R0 >= kMaxRebirth) bAllOk = false;

	// what happens
	y += 6;
	R.FillPanel(X + 12, y, X + W - 12, y + 1, 0, RGB(255, 255, 255), 20);
	y += 8;
	R.TextOutSized(X + 16, y, "WHAT HAPPENS", kDim, 9);
	y += 16;
	std::vector<std::string> lines;
	if (m_iRebirthMode == 0) {
		lines.push_back("Back to level 1 with every stat at 10");
		lines.push_back("+3 stat points per rebirth: +" + std::to_string(3 * Rn) + " for you at Rebirth " + std::to_string(Rn));
		lines.push_back("Items (unequipped), skills and spells are kept");
		const int iPct = (int)floor(pow(0.8, (double)Rn) * 1000.0 + 0.5);
		char c[64];
		wsprintf(c, "XP x%d.%d%% until you reach level %d again", iPct / 10, iPct % 10, kMaxLevel);
		lines.push_back(c);
		lines.push_back("Normal Mode: play your level " + kMaxLevelText + " character meanwhile");
	}
	else {
		lines.push_back(m_iRebirthMode == 1 ? "You play your full-level character; the rebirth waits" : "You go back to your rebirth where you left it");
		lines.push_back("Items are unequipped; skills and spells are kept");
		lines.push_back("You can switch back any time for 50,000 gold");
	}
	lines.push_back("You are logged out to load the new character");
	for (size_t i = 0; i < lines.size(); i++) {
		R.FillPanel(X + 20, y + 6, X + 24, y + 10, 2, kPurple, 220);
		R.TextOutSized(X + 32, y, lines[i].c_str(), RGB(200, 205, 213), 11);
		y += 16;
	}

	// the button
	RebirthButtonRect(2, &rc); OffsetRect(&rc, X, Y);
	const bool bConfirm = (m_dwRebirthConfirmAt != 0 && m_dwCurTime - m_dwRebirthConfirmAt < 4000);
	const bool bOn = bTop && bAllOk && InRect(rc, ms::X, ms::Y);
	std::string label;
	if (bConfirm) label = "Click again to confirm";
	else if (m_iRebirthMode == 0) label = (R0 >= kMaxRebirth) ? "Highest rebirth reached" : ("Be reborn - Rebirth " + std::to_string(Rn));
	else if (m_iRebirthMode == 1) label = "Switch to Normal Mode - 50,000 gold";
	else label = "Resume rebirth - 50,000 gold";
	R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, bAllOk ? (bConfirm ? RGB(255, 140, 90) : kGold) : RGB(80, 76, 66), bOn ? 255 : 225);
	const int tw = R.TitleWidth(label.c_str(), 13);
	R.TitleOut((rc.left + rc.right - tw) / 2, rc.top + 5, label.c_str(), bAllOk ? RGB(34, 26, 10) : RGB(150, 144, 130), 13);
}

void CGame::DlgBoxClick_Rebirth()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_REBIRTH];
	const short X = dlg.m_X, Y = dlg.m_Y;
	RECT rc;
	RebirthButtonRect(1, &rc); OffsetRect(&rc, X, Y);
	if (InRect(rc, ms::X, ms::Y)) { DisableDialogBox(DIALOG_REBIRTH); PlaySound('E', 14, 5); return; }
	RebirthButtonRect(2, &rc); OffsetRect(&rc, X, Y);
	if (!InRect(rc, ms::X, ms::Y)) return;
	if (m_iRebirthMode == 0 && (m_iRebirth >= kMaxRebirth || m_iLevel < kMaxLevel)) return;

	if (m_dwRebirthConfirmAt == 0 || m_dwCurTime - m_dwRebirthConfirmAt >= 4000) {	// first click: ask to confirm
		m_dwRebirthConfirmAt = m_dwCurTime;
		PlaySound('E', 14, 5);
		return;
	}
	m_dwRebirthConfirmAt = 0;
	bSendCommand(MSGID_REQUEST_REBIRTH, m_iRebirthMode == 0 ? 1 : 2, NULL, 0, 0, 0, NULL);
	PlaySound('E', 14, 5);
}
