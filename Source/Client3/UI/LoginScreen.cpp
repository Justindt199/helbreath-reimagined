// LoginScreen.cpp: the loading screen and the sign-in screens (welcome, sign in, create account),
// drawn in code.
//
// Behind the sign-in screens and the Connecting / Waiting / error messages after them is the
// live town (UI\LoginScene.cpp); the loading screen is the night sky (also UI\LoginScene.cpp).
//
// Optional: CONTENTS\news.txt, shown in a small box at the top left (first line = headline).
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const COLORREF kGold	= RGB(242, 207, 115);
const COLORREF kGoldDim	= RGB(217, 196, 143);
const COLORREF kWhite	= RGB(240, 242, 245);
const COLORREF kGrey	= RGB(154, 162, 172);
const COLORREF kDim		= RGB(111, 119, 130);
const COLORREF kRed		= RGB(255, 140, 140);

const char * const kRegKey = "Software\\Siementech\\Helbreath\\Settings";

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

// a panel's heading: in the display font (fonts\display.ttf), or bold in the interface font
void Title(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx)
{
	if (!r.HasDisplayFont()) { Bold(r, x, y, s, c, iPx); return; }
	r.m_iFace = 1;
	r.TextOutSized(x, y - 1, s, c, iPx + 1);
	r.m_iFace = 0;
}

// CONTENTS\news.txt, read once (up to 4 lines)
const std::vector<std::string> & NewsLines()
{
	static bool s_read = false;
	static std::vector<std::string> s_lines;
	if (!s_read) {
		s_read = true;
		FILE * f = fopen("CONTENTS\\news.txt", "rt");
		if (f != NULL) {
			char line[160];
			while (s_lines.size() < 4 && fgets(line, sizeof(line), f) != NULL) {
				std::string s = line;
				while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
				if (!s.empty()) s_lines.push_back(s);
			}
			fclose(f);
		}
	}
	return s_lines;
}

} // namespace

//////////////////////////////////////////////////////////////////////
// Remembered account name (registry, next to the other settings)

void CGame::LoadRememberedAccount(char * pOut, size_t iSize)
{
	if (pOut == NULL || iSize == 0) return;
	pOut[0] = 0;
	m_bRememberAccount = false;
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return;
	char cBuf[32] = {};
	DWORD dwType = 0, dwLen = sizeof(cBuf) - 1;
	if (RegQueryValueEx(key, "RememberedAccount", NULL, &dwType, (LPBYTE)cBuf, &dwLen) == ERROR_SUCCESS && dwType == REG_SZ && cBuf[0]) {
		strncpy(pOut, cBuf, iSize - 1);
		pOut[iSize - 1] = 0;
		m_bRememberAccount = true;
	}
	RegCloseKey(key);
}

void CGame::SaveRememberedAccount(const char * pName)
{
	HKEY key;
	DWORD dwDisp;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, kRegKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &dwDisp) != ERROR_SUCCESS) return;
	if (pName != NULL && pName[0]) RegSetValueEx(key, "RememberedAccount", 0, REG_SZ, (const BYTE *)pName, (DWORD)strlen(pName) + 1);
	else RegDeleteValue(key, "RememberedAccount");
	RegCloseKey(key);
}

//////////////////////////////////////////////////////////////////////
// Backdrop: the live town, the logo, the version and the news

void CGame::DrawLoginBackdrop(bool bNews)
{
	CRenderer & R = m_Renderer;
	const int W = GAME_W, H = GAME_H;
	DrawLoginScene();		// the town, its name bottom left
	DrawLoadHandover(false);	// straight after loading: the night sky fading off it
	DrawLoginLogo();		// top left
	char cVer[32];
	wsprintf(cVer, "Client %d.%d", UPPER_VERSION, LOWER_VERSION);
	R.TextOutSized(W - 12 - Width(R, cVer, 11), H - 18, cVer, kDim, 11);
	const int iNewsY = 104;

	// news, top left (under the logo over the live town)
	const std::vector<std::string> & news = NewsLines();
	if (bNews && !news.empty()) {
		int nw = Width(R, "NEWS", 11);
		for (size_t i = 0; i < news.size(); i++) nw = (std::max)(nw, Width(R, news[i].c_str(), i == 0 ? 0 : 12));
		nw = (std::min)(nw + 24, W / 2);
		const int nh = 26 + (int)news.size() * 16 + 6, ny = iNewsY;
		R.FillPanel(20, ny, 20 + nw, ny + nh, 8, RGB(10, 12, 16), 160, RGB(255, 255, 255), 26);
		R.TextOutSized(32, ny + 6, "NEWS", kGold, 11);
		for (size_t i = 0; i < news.size(); i++) {
			if (i == 0) R.TextOut(32, ny + 22, news[i].c_str(), kWhite);
			else R.TextOutSized(32, ny + 24 + (int)i * 16, news[i].c_str(), kGrey, 12);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Loading screen

void CGame::UpdateScreen_OnLoading_Progress()
{
	CRenderer & R = m_Renderer;
	R.BeginUiRecording();		// (also shown while the game loads, outside UpdateScreen)
	DrawLoadingScreen((int)m_cLoading);	// UI\LoginScene.cpp
	char cVer[32];
	wsprintf(cVer, "Client %d.%d", UPPER_VERSION, LOWER_VERSION);
	R.TextOutSized(GAME_W - 12 - Width(R, cVer, 11), GAME_H - 18, cVer, kDim, 11);
	R.iFlip();
}

//////////////////////////////////////////////////////////////////////
// Sign-in screen

void CGame::LoginRects(RECT * pPanel, RECT * pAcc, RECT * pPw, RECT * pRemember, RECT * pConnect, RECT * pCreate, RECT * pQuit)
{
	const int W = GAME_W, H = GAME_H;
	const int pw = 240, ph = 292, px = W - 26 - pw + m_iPanelDX, py = (H - ph) / 2 - 10 + m_iPanelDY;
	if (pPanel) SetRect(pPanel, px, py, px + pw, py + ph);
	const int fx0 = px + 18, fx1 = px + pw - 18;
	if (pAcc) SetRect(pAcc, fx0, py + 78, fx1, py + 106);
	if (pPw) SetRect(pPw, fx0, py + 132, fx1, py + 160);
	if (pRemember) SetRect(pRemember, fx0, py + 170, fx0 + 140, py + 188);
	if (pConnect) SetRect(pConnect, fx0, py + 198, fx1, py + 232);
	const int half = (fx1 - fx0 - 8) / 2;
	if (pCreate) SetRect(pCreate, fx0, py + 240, fx0 + half, py + 264);
	if (pQuit) SetRect(pQuit, fx1 - half, py + 240, fx1, py + 264);
}

void CGame::_Draw_OnLogin(char * pAccount, char * pPassword, int msX, int msY, int iFrame)
{
	CRenderer & R = m_Renderer;
	DrawLoginBackdrop(true);

	int iAlpha;
	LoginPanelAnim(&m_iPanelDX, &m_iPanelDY, &iAlpha);
	R.m_iFadeA = iAlpha;
	RECT rcPanel, rcAcc, rcPw, rcRem, rcGo, rcNew, rcQuit;
	LoginRects(&rcPanel, &rcAcc, &rcPw, &rcRem, &rcGo, &rcNew, &rcQuit);
	const int px = rcPanel.left, py = rcPanel.top;

	R.StonePanel(rcPanel.left, rcPanel.top, rcPanel.right, rcPanel.bottom);
	Title(R, px + 18, py + 16, "SIGN IN", kGold, 15);
	R.TextOutSized(px + 18, py + 38, "Welcome back, adventurer.", kGrey, 12);

	// account and password boxes; the one being typed in is outlined in gold
	const bool bNameOk = (strlen(pAccount) > 0 && m_Misc.bCheckValidName(pAccount) != FALSE) || strlen(pAccount) == 0;
	const bool bPwOk = (strlen(pPassword) > 0 && m_Misc.bCheckValidString(pPassword) != FALSE) || strlen(pPassword) == 0;
	const bool bCaret = ((G_dwGlobalTime / 450) % 2) == 0;
	const struct { const RECT * rc; const char * label; bool focus; } box[2] = {
		{ &rcAcc, "ACCOUNT", m_cCurFocus == 1 }, { &rcPw, "PASSWORD", m_cCurFocus == 2 } };
	for (int i = 0; i < 2; i++) {
		const RECT & rc = *box[i].rc;
		R.TextOutSized(rc.left, rc.top - 18, box[i].label, kGrey, 11);
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(0, 0, 0), 120,
			box[i].focus ? RGB(232, 192, 112) : RGB(255, 255, 255), box[i].focus ? 230 : 40);
		const int tx = rc.left + 10, ty = rc.top + 6;
		int iTextW = 0;
		if (i == 0) {
			R.TextOut(tx, ty, pAccount, bNameOk ? kWhite : kRed);
			iTextW = Width(R, pAccount);
		}
		else {
			const int n = (int)strlen(pPassword);
			for (int k = 0; k < n; k++)	// dots, drawn as small circles
				R.FillPanel(tx + k * 9, ty + 4, tx + k * 9 + 6, ty + 10, 3, bPwOk ? kWhite : kRed, 255);
			iTextW = n * 9 - 2;
		}
		if (box[i].focus && bCaret)
			R.FillPanel(tx + iTextW + 2, ty, tx + iTextW + 3, ty + 15, 0, RGB(255, 255, 255), 230);
	}

	// remember account
	{
		const bool bOn = InRect(rcRem, msX, msY);
		const int cx = rcRem.left, cy = rcRem.top + 2;
		if (m_bRememberAccount) {
			R.FillPanel(cx, cy, cx + 14, cy + 14, 3, RGB(232, 192, 112), 255);
			R.FillPanel(cx + 4, cy + 4, cx + 10, cy + 10, 1, RGB(26, 20, 8), 255);
		}
		else R.FillPanel(cx, cy, cx + 14, cy + 14, 3, RGB(0, 0, 0), 90, RGB(255, 255, 255), bOn ? 120 : 70);
		R.TextOutSized(cx + 21, cy, "Remember account", bOn ? kWhite : RGB(185, 192, 201), 12);
	}

	// Connect, Create account, Quit
	{
		const bool bReady = strlen(pAccount) > 0 && strlen(pPassword) > 0 && bNameOk && bPwOk;
		const bool bOn = InRect(rcGo, msX, msY) || m_cCurFocus == 3;
		R.StoneButton(rcGo.left, rcGo.top, rcGo.right, rcGo.bottom, bReady ? 1 : 0);	// gold-rimmed once it can go
		const char * pGo = "CONNECT";
		Bold(R, (rcGo.left + rcGo.right - Width(R, pGo, 15)) / 2, rcGo.top + 8, pGo,
			bReady ? (bOn ? RGB(255, 242, 199) : RGB(245, 219, 158)) : RGB(150, 144, 130), 15);
	}
	const struct { const RECT * rc; const char * label; bool on; } btn[2] = {
		{ &rcNew, "Create account", InRect(rcNew, msX, msY) || m_cCurFocus == 4 }, { &rcQuit, "Quit", InRect(rcQuit, msX, msY) } };
	for (int i = 0; i < 2; i++) {
		const RECT & rc = *btn[i].rc;
		R.StoneButton(rc.left, rc.top, rc.right, rc.bottom, btn[i].on ? 1 : 0);
		R.TextOutSized((rc.left + rc.right - Width(R, btn[i].label, 12)) / 2, rc.top + 5, btn[i].label, btn[i].on ? kWhite : RGB(220, 224, 230), 12);
	}

	R.m_iFadeA = 255;
	DrawPanelEdgeLight(rcPanel);
	m_iPanelDX = m_iPanelDY = 0;

	if ((m_bIsHideLocalCursor != TRUE) && (msX != 0) && (msY != 0))
		m_pSprite[SPRID_MOUSECURSOR]->PutSpriteFast(msX, msY, 0, timeGetTime());
}

//////////////////////////////////////////////////////////////////////
// Welcome panel: what a new player sees instead of sign in (no account used on this PC yet)

void CGame::WelcomeRects(RECT * pPanel, RECT * pCreate, RECT * pHave, RECT * pQuit)
{
	const int W = GAME_W, H = GAME_H;
	const int pw = 240, ph = 206, px = W - 26 - pw + m_iPanelDX, py = (H - ph) / 2 - 10 + m_iPanelDY;
	if (pPanel) SetRect(pPanel, px, py, px + pw, py + ph);
	const int fx0 = px + 18, fx1 = px + pw - 18;
	if (pCreate) SetRect(pCreate, fx0, py + 86, fx1, py + 120);
	if (pHave) SetRect(pHave, fx0, py + 128, fx1, py + 154);
	if (pQuit) SetRect(pQuit, fx0, py + 170, fx0 + 54, py + 190);
}

void CGame::_Draw_OnWelcome(int iFocus, int msX, int msY)
{
	CRenderer & R = m_Renderer;
	DrawLoginBackdrop(true);

	int iAlpha;
	LoginPanelAnim(&m_iPanelDX, &m_iPanelDY, &iAlpha);
	R.m_iFadeA = iAlpha;
	RECT rcPanel, rcCreate, rcHave, rcQuit;
	WelcomeRects(&rcPanel, &rcCreate, &rcHave, &rcQuit);
	const int px = rcPanel.left, py = rcPanel.top;

	R.StonePanel(rcPanel.left, rcPanel.top, rcPanel.right, rcPanel.bottom);
	Title(R, px + 18, py + 16, "NEW TO HELBREATH?", kGold, 15);
	R.TextOutSized(px + 18, py + 42, "Make a free account and your first", kGrey, 12);
	R.TextOutSized(px + 18, py + 58, "character in about a minute.", kGrey, 12);

	// Create account: gold, glowing gently
	{
		const bool bOn = InRect(rcCreate, msX, msY) || iFocus == 1;
		const float fPulse = 0.5f + 0.5f * std::sin((timeGetTime() % 2400) / 2400.f * 6.2832f - 1.5708f);
		R.FillPanel(rcCreate.left - 4, rcCreate.top - 4, rcCreate.right + 4, rcCreate.bottom + 4, 11, RGB(255, 210, 120), (int)(70 * fPulse));
		R.StoneButton(rcCreate.left, rcCreate.top, rcCreate.right, rcCreate.bottom, 1);	// the main button: gold-rimmed stone
		const char * p = "CREATE ACCOUNT";
		Bold(R, (rcCreate.left + rcCreate.right - Width(R, p, 15)) / 2, rcCreate.top + 8, p, bOn ? RGB(255, 242, 199) : RGB(245, 219, 158), 15);
	}
	// I have an account
	{
		const bool bOn = InRect(rcHave, msX, msY) || iFocus == 2;
		R.StoneButton(rcHave.left, rcHave.top, rcHave.right, rcHave.bottom, bOn ? 1 : 0);
		const char * p = "I have an account";
		R.TextOutSized((rcHave.left + rcHave.right - Width(R, p, 12)) / 2, rcHave.top + 6, p, bOn ? kWhite : RGB(220, 224, 230), 12);
	}
	// Quit, and day or night in the towns
	{
		const bool bOn = InRect(rcQuit, msX, msY) || iFocus == 3;
		R.TextOutSized(rcQuit.left, rcQuit.top + 3, "Quit", bOn ? kWhite : kGrey, 12);
		const char * p = LoginIsNight() ? "Night falls on both towns" : "Day in both towns";
		R.TextOutSized(rcPanel.right - 18 - Width(R, p, 11), rcQuit.top + 4, p, kDim, 11);
	}

	R.m_iFadeA = 255;
	DrawPanelEdgeLight(rcPanel);
	m_iPanelDX = m_iPanelDY = 0;

	if ((m_bIsHideLocalCursor != TRUE) && (msX != 0) && (msY != 0))
		m_pSprite[SPRID_MOUSECURSOR]->PutSpriteFast(msX, msY, 0, timeGetTime());
}

//////////////////////////////////////////////////////////////////////
// Create-account screen (same picture and panel style as sign-in; replaces CreateNewAcc.pak)
//
// Fields: 0 account, 1 password, 2 confirm, 3 e-mail, 4 secret question, 5 answer.
// Password and confirm share a row.

void CGame::CreateAccountRects(RECT * pPanel, RECT * pField, RECT * pCreate, RECT * pClear, RECT * pBack)
{
	const int W = GAME_W, H = GAME_H;
	const int pw = 300, ph = 420, px = W - 26 - pw + m_iPanelDX, py = (std::max)(8, (H - ph) / 2 - 10) + m_iPanelDY;
	if (pPanel) SetRect(pPanel, px, py, px + pw, py + ph);
	const int fx0 = px + 18, fx1 = px + pw - 18;
	const int half = (fx1 - fx0 - 10) / 2;
	if (pField) {
		SetRect(&pField[0], fx0, py + 76, fx1, py + 102);
		SetRect(&pField[1], fx0, py + 124, fx0 + half, py + 150);
		SetRect(&pField[2], fx1 - half, py + 124, fx1, py + 150);
		SetRect(&pField[3], fx0, py + 172, fx1, py + 198);
		SetRect(&pField[4], fx0, py + 220, fx1, py + 246);
		SetRect(&pField[5], fx0, py + 268, fx1, py + 294);
	}
	if (pCreate) SetRect(pCreate, fx0, py + 344, fx1, py + 376);
	if (pClear) SetRect(pClear, fx0, py + 384, fx0 + half, py + 406);
	if (pBack) SetRect(pBack, fx1 - half, py + 384, fx1, py + 406);
}

void CGame::_Draw_OnCreateAccount(const char * const * pField, const bool * bOk, const bool * bGood, const char * const * pHint,
	bool bHintError, bool bReady, int msX, int msY, int iFrame)
{
	CRenderer & R = m_Renderer;
	DrawLoginBackdrop(false);

	int iAlpha;
	LoginPanelAnim(&m_iPanelDX, &m_iPanelDY, &iAlpha);
	R.m_iFadeA = iAlpha;
	RECT rcPanel, rcField[6], rcGo, rcClear, rcBack;
	CreateAccountRects(&rcPanel, rcField, &rcGo, &rcClear, &rcBack);
	const int px = rcPanel.left, py = rcPanel.top;

	R.StonePanel(rcPanel.left, rcPanel.top, rcPanel.right, rcPanel.bottom);
	Title(R, px + 18, py + 16, "CREATE ACCOUNT", kGold, 15);
	R.TextOutSized(px + 18, py + 38, "Choose your name and join the war.", kGrey, 12);

	static const char * const kLabel[6] = { "ACCOUNT", "PASSWORD", "CONFIRM PASSWORD", "E-MAIL", "SECRET QUESTION", "ANSWER" };
	const bool bCaret = ((G_dwGlobalTime / 450) % 2) == 0;
	for (int i = 0; i < 6; i++) {
		const RECT & rc = rcField[i];
		const bool bFocus = (m_cCurFocus == i + 1);
		const char * s = (pField[i] != NULL) ? pField[i] : "";
		const bool bShowBad = !bOk[i] && s[0] != 0;
		R.TextOutSized(rc.left, rc.top - 16, kLabel[i], kGrey, 11);
		R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, RGB(0, 0, 0), 120,
			bFocus ? RGB(232, 192, 112) : (bShowBad ? RGB(255, 120, 120) : RGB(255, 255, 255)), bFocus ? 230 : (bShowBad ? 150 : 40));
		const int tx = rc.left + 9, ty = rc.top + 5, iRoom = rc.right - rc.left - 22;
		int iTextW = 0;
		if (i == 1 || i == 2) {
			const int n = (std::min)((int)strlen(s), iRoom / 9);
			for (int k = 0; k < n; k++)	// dots, drawn as small circles
				R.FillPanel(tx + k * 9, ty + 5, tx + k * 9 + 6, ty + 11, 3, bShowBad ? kRed : kWhite, 255);
			iTextW = n * 9 - 2;
		}
		else {
			// long text (e-mail, question) scrolls: show the end that fits
			const char * p = s;
			while (*p && Width(R, p) > iRoom) p++;
			R.TextOut(tx, ty, p, bShowBad ? kRed : kWhite);
			iTextW = Width(R, p);
		}
		if (bFocus && bCaret)
			R.FillPanel(tx + iTextW + 2, ty + 1, tx + iTextW + 3, ty + 16, 0, RGB(255, 255, 255), 230);
		// a green tick once the box is right
		if (bGood != NULL && bGood[i] && iAlpha >= 255) {
			const int kx = rc.right - 13, ky = (rc.top + rc.bottom) / 2;
			for (int d = 0; d < 2; d++) {
				DrawLine(kx - 5, ky + d, kx - 2, ky + 3 + d, 13, 54, 15);		// (5 / 6 / 5-bit light, added)
				DrawLine(kx - 2, ky + 3 + d, kx + 4, ky - 4 + d, 13, 54, 15);
			}
		}
	}

	// help for the field being typed in, or what's still missing
	for (int i = 0; i < 3; i++)
		if (pHint[i] != NULL && pHint[i][0])
			R.TextOutSized(px + 18, py + 300 + i * 13, pHint[i], (bHintError && i == 0) ? kRed : (i == 0 ? RGB(205, 210, 218) : kGrey), 12);

	// Create, Clear, Back
	{
		const bool bOn = InRect(rcGo, msX, msY) || m_cCurFocus == 7;
		R.FillPanel(rcGo.left, rcGo.top, rcGo.right, rcGo.bottom, 7,
			bReady ? (bOn ? RGB(250, 214, 128) : RGB(232, 188, 92)) : RGB(120, 104, 70), bReady ? 255 : 180);
		const char * pGo = "CREATE ACCOUNT";
		Bold(R, (rcGo.left + rcGo.right - Width(R, pGo, 15)) / 2, rcGo.top + 7, pGo, RGB(30, 22, 8), 15);
	}
	const struct { const RECT * rc; const char * label; bool on; } btn[2] = {
		{ &rcClear, "Clear", InRect(rcClear, msX, msY) || m_cCurFocus == 8 },
		{ &rcBack, "Back to sign in", InRect(rcBack, msX, msY) || m_cCurFocus == 9 } };
	for (int i = 0; i < 2; i++) {
		const RECT & rc = *btn[i].rc;
		R.StoneButton(rc.left, rc.top, rc.right, rc.bottom, btn[i].on ? 1 : 0);
		R.TextOutSized((rc.left + rc.right - Width(R, btn[i].label, 12)) / 2, rc.top + 4, btn[i].label, btn[i].on ? kWhite : RGB(220, 224, 230), 12);
	}

	R.m_iFadeA = 255;
	DrawPanelEdgeLight(rcPanel);
	m_iPanelDX = m_iPanelDY = 0;

	if ((msX != 0) && (msY != 0))
		m_pSprite[SPRID_MOUSECURSOR]->PutSpriteFast(msX, msY, 0, timeGetTime());
}

//////////////////////////////////////////////////////////////////////
// Message panel: connecting, waiting, errors and yes/no questions before the game
// (replaces the stone message box on those screens), plus the quit screen.

namespace {

// word-wraps each line to iMaxW pixels
std::vector<std::string> WrapLines(CRenderer & r, const std::vector<std::string> & in, int iMaxW, int iPx)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < in.size(); i++) {
		std::string s = in[i];
		while (!s.empty() && s[0] == ' ') s.erase(0, 1);
		if (s.empty()) { out.push_back(""); continue; }
		std::string cur;
		size_t p = 0;
		while (p < s.size()) {
			size_t q = s.find(' ', p);
			if (q == std::string::npos) q = s.size();
			const std::string w = s.substr(p, q - p);
			const std::string t = cur.empty() ? w : cur + " " + w;
			if (!cur.empty() && Width(r, t.c_str(), iPx) > iMaxW) { out.push_back(cur); cur = w; }
			else cur = t;
			p = q + 1;
		}
		if (!cur.empty()) out.push_back(cur);
	}
	return out;
}

struct MsgLayout {
	RECT rcPanel, rcBtn[2];
	int iIconX, iIconY, iTextX, iTitleY, iLinesY;
	std::vector<std::string> lines;
};

const int kMsgW = 340, kMsgPad = 20, kMsgIcon = 36, kMsgLineH = 16, kMsgBtnW = 116, kMsgBtnH = 26;

MsgLayout LayoutMessage(CRenderer & r, const CGame::MsgPanel & m, int W, int H)
{
	MsgLayout L;
	const int iTextOff = (m.iIcon != CGame::MSGICON_NONE) ? kMsgIcon : 0;
	L.lines = WrapLines(r, m.lines, kMsgW - kMsgPad * 2 - iTextOff, 12);
	int nBtn = 0;
	for (int i = 0; i < 2; i++) if (!m.btn[i].empty()) nBtn = i + 1;

	// heights first, relative to the panel top
	int cur = 18;
	const int iTitleY = cur;
	if (!m.title.empty()) cur += 26;
	const int iLinesY = cur;
	cur += (int)L.lines.size() * kMsgLineH;
	if (m.iIcon != CGame::MSGICON_NONE) cur = (std::max)(cur, 18 + 28);
	int iBtnY = 0;
	if (nBtn > 0) { cur += 14; iBtnY = cur; cur += kMsgBtnH; }
	cur += 18;

	const int x0 = (W - kMsgW) / 2, y0 = (H - cur) / 2;
	SetRect(&L.rcPanel, x0, y0, x0 + kMsgW, y0 + cur);
	L.iIconX = x0 + kMsgPad;
	L.iIconY = y0 + 16;
	L.iTextX = x0 + kMsgPad + iTextOff;
	L.iTitleY = y0 + iTitleY;
	L.iLinesY = y0 + iLinesY;
	SetRect(&L.rcBtn[0], 0, 0, 0, 0);
	SetRect(&L.rcBtn[1], 0, 0, 0, 0);
	const int xr = x0 + kMsgW - kMsgPad;
	if (nBtn == 1) SetRect(&L.rcBtn[0], xr - kMsgBtnW, y0 + iBtnY, xr, y0 + iBtnY + kMsgBtnH);
	else if (nBtn == 2) {
		SetRect(&L.rcBtn[1], xr - kMsgBtnW, y0 + iBtnY, xr, y0 + iBtnY + kMsgBtnH);
		SetRect(&L.rcBtn[0], xr - kMsgBtnW * 2 - 8, y0 + iBtnY, xr - kMsgBtnW - 8, y0 + iBtnY + kMsgBtnH);
	}
	return L;
}

// a round icon with one character in it
void DrawBadge(CRenderer & r, int x, int y, COLORREF c, const char * s)
{
	r.FillPanel(x, y, x + 26, y + 26, 13, c, 40, c, 170);
	Bold(r, x + (26 - Width(r, s, 15)) / 2, y + 4, s, c, 15);
}

} // namespace

void CGame::MessagePanelRects(const MsgPanel & m, RECT * pPanel, RECT * pBtn)
{
	const MsgLayout L = LayoutMessage(m_Renderer, m, GAME_W, GAME_H);
	if (pPanel) *pPanel = L.rcPanel;
	if (pBtn) { pBtn[0] = L.rcBtn[0]; pBtn[1] = L.rcBtn[1]; }
}

void CGame::DrawMessagePanel(const MsgPanel & m, int msX, int msY, bool bDim)
{
	CRenderer & R = m_Renderer;
	const MsgLayout L = LayoutMessage(R, m, GAME_W, GAME_H);
	if (bDim) R.FillPanel(0, 0, GAME_W, GAME_H, 0, RGB(0, 0, 0), 120);
	const RECT & p = L.rcPanel;
	R.StonePanel(p.left, p.top, p.right, p.bottom);

	switch (m.iIcon) {
	case MSGICON_WAIT: {
		// eight dots going round
		const int cx = L.iIconX + 13, cy = L.iIconY + 13, iPhase = (int)((G_dwGlobalTime / 90) % 8);
		for (int k = 0; k < 8; k++) {
			const double a = k * 3.14159265 / 4.0;
			const int dx = cx + (int)(cos(a) * 9.0), dy = cy + (int)(sin(a) * 9.0);
			const int iAge = (iPhase - k + 8) % 8;
			R.FillPanel(dx - 2, dy - 2, dx + 3, dy + 3, 2, RGB(232, 192, 112), 255 - iAge * 28);
		}
		break; }
	case MSGICON_WARN:		DrawBadge(R, L.iIconX, L.iIconY, RGB(255, 140, 110), "!"); break;
	case MSGICON_OK:		DrawBadge(R, L.iIconX, L.iIconY, RGB(120, 214, 120), "i"); break;
	case MSGICON_QUESTION:	DrawBadge(R, L.iIconX, L.iIconY, RGB(232, 192, 112), "?"); break;
	}

	if (!m.title.empty()) {
		if (m.bTitleBold) Bold(R, L.iTextX, L.iTitleY, m.title.c_str(), kGold, 15);
		else R.TextOutSized(L.iTextX, L.iTitleY + 1, m.title.c_str(), kWhite, 14);
	}
	for (size_t i = 0; i < L.lines.size(); i++)
		R.TextOutSized(L.iTextX, L.iLinesY + (int)i * kMsgLineH, L.lines[i].c_str(), RGB(200, 205, 213), 12);

	for (int i = 0; i < 2; i++) {
		if (m.btn[i].empty()) continue;
		const RECT & rc = L.rcBtn[i];
		const bool bOn = InRect(rc, msX, msY);
		const char * s = m.btn[i].c_str();
		if (i == 0) {	// the main button
			const COLORREF c = m.bDanger ? (bOn ? RGB(236, 112, 92) : RGB(208, 84, 66)) : (bOn ? RGB(250, 214, 128) : RGB(232, 188, 92));
			R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 6, c, 255);
			Bold(R, (rc.left + rc.right - Width(R, s, 13)) / 2, rc.top + 6, s, m.bDanger ? RGB(255, 245, 240) : RGB(30, 22, 8), 13);
		}
		else {
			R.StoneButton(rc.left, rc.top, rc.right, rc.bottom, bOn ? 1 : 0);
			R.TextOutSized((rc.left + rc.right - Width(R, s, 12)) / 2, rc.top + 6, s, bOn ? kWhite : RGB(220, 224, 230), 12);
		}
	}
}

// the quit screen: same picture, a small farewell panel, a countdown, then a fade to black
void CGame::DrawQuitScreen(int iMsElapsed, int iMsTotal)
{
	CRenderer & R = m_Renderer;
	DrawLoginBackdrop(false);
	const int W = GAME_W, H = GAME_H;
	const int pw = 270, ph = 150, px = W - 26 - pw, py = (H - ph) / 2 - 30;
	const int iShow = (std::min)(255, iMsElapsed * 255 / 300);	// panel fades in over 0.3s
	R.StonePanel(px, py, px + pw, py + ph);
	if (iShow > 120) {
		Bold(R, px + 20, py + 18, "FAREWELL, ADVENTURER", kGold, 15);
		R.TextOutSized(px + 20, py + 44, "Thanks for playing Helbreath Unrivaled.", RGB(200, 205, 213), 12);
		R.TextOutSized(px + 20, py + 60, "See you back on the battlefield soon.", RGB(200, 205, 213), 12);
		const int iLeft = (std::max)(0, (iMsTotal - iMsElapsed + 999) / 1000);
		char cSec[8];
		wsprintf(cSec, "%d", iLeft);
		R.TextOutSized(px + 20, py + 88, "Closing the game...", kGrey, 11);
		R.TextOutSized(px + pw - 20 - Width(R, cSec, 11), py + 88, cSec, kGold, 11);
		const int bx0 = px + 20, bx1 = px + pw - 20, by = py + 104;
		R.FillPanel(bx0, by, bx1, by + 6, 3, RGB(255, 255, 255), 34);
		const int fw = (bx1 - bx0) * (std::min)(iMsElapsed, iMsTotal) / (std::max)(1, iMsTotal);
		if (fw >= 4) R.FillPanel(bx0, by, bx0 + fw, by + 6, 3, RGB(232, 188, 92), 255);
		R.TextOutSized(px + 20, py + 120, "Click or press Enter to close now", kDim, 11);
	}
	// fade to black over the last 0.4s
	const int iFadeFrom = iMsTotal - 400;
	if (iMsElapsed > iFadeFrom) R.FillPanel(0, 0, W, H, 0, RGB(0, 0, 0), (std::min)(255, (iMsElapsed - iFadeFrom) * 255 / 400));
}
