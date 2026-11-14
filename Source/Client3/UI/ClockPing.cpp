// ClockPing.cpp: the strip across the top of the game screen with the FPS, ping and clock.
//
// Every few seconds the client sends MSGID_REQUEST_PING with its own clock (timeGetTime); the
// HG Server answers at once with NOTIFY_PONG, echoing it and adding its time of day. The round
// trip is the ping; the server time keeps counting locally between answers.
//
// A thin see-through strip across the top of the screen shows the FPS, the ping (bars and ms: green under 100 ms, orange to 250, red above, grey "..." when no answer
// has come back for a while) and the clock, side by side from the left; hovering the ping shows the average and
// the time with seconds. "Clock and ping" in the settings turns it off. The event messages in the
// top-left corner start below it.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"
#include "..\Engine\FrameProfiler.h"

#include <algorithm>

namespace {

const DWORD kPingEvery = 5000;		// ms between pings
const DWORD kPingLost  = 10000;		// no answer for this long: shown as lost

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(x, y, s, c, iPx);
	r.TextOutSized(x + 1, y, s, c, iPx);
}

// Hovering the FPS: where each frame's time went in the last second (Engine\FrameProfiler.h).
void DrawFrameTimes(CRenderer & R, int x, int y)
{
	using namespace prof;
	const double dChars = (std::max)(0.0, g.ms[OBJECTS] - g.ms[RINGS] - g.ms[NAMES]);
	char cA[16], cB[16], cC[16], cD[16];
	wsprintf(cA, "(%d)", (int)(g.per[C_CHARS] + 0.5));
	wsprintf(cB, "(%d)", (int)(g.per[C_RINGS] + 0.5));
	wsprintf(cC, "(%d)", (int)(g.per[C_NAMES] + 0.5));
	wsprintf(cD, "(%d/s)", (int)(g.per[C_MSGS] + 0.5));
	struct Row { const char * pLabel; double ms; const char * pNote; };
	// what the timed sections inside the update don't cover, and what is outside the update
	double dInside = 0;
	for (Sec s : { MAP, OBJECTS, EFFECTS, UI, CONVERT, DISPLAY, LOGIC }) dInside += g.ms[s];
	const double dOtherUpdate = (std::max)(0.0, g.ms[UPDATE] - dInside);
	const double dWinMsg = (std::max)(0.0, g.ms[WINMSG] - g.ms[NET]);
	const double dElse = (std::max)(0.0, g.ms[FRAME] - g.ms[UPDATE] - g.ms[IDLE] - g.ms[WINMSG]);
	char cE[24];
	wsprintf(cE, "(%d checks)", (int)(g.per[C_IDLE] + 0.5));
	const Row rows[] = {
		{ "Map and lights",         g.ms[MAP],     "" },
		{ "Characters",             dChars,        cA },
		{ "Circle indicators",      g.ms[RINGS],   cB },
		{ "Names",                  g.ms[NAMES],   cC },
		{ "Effects and chat",       g.ms[EFFECTS], "" },
		{ "Interface",              g.ms[UI],      "" },
		{ "Picture to screen",      g.ms[CONVERT], "" },
		{ "Show + wait (VSync, limit)", g.ms[DISPLAY], "" },
		{ "Server messages",        g.ms[NET],     cD },
		{ "Game logic",             g.ms[LOGIC],   "" },
		{ "Rest of the game update", dOtherUpdate, "" },
		{ "Waiting for a change",   g.ms[IDLE],    cE },
		{ "Mouse, keys, timers",    dWinMsg,       "" },
		{ "Everything else",        dElse,         "" },
	};
	const int nRows = (int)(sizeof(rows) / sizeof(rows[0]));
	// the three windows that took longest
	int top[3] = { -1, -1, -1 };
	for (int i = 1; i < kMaxDlg; i++) {
		if (g.dlgMs[i] < 0.05 || g.dlgName[i] == NULL) continue;
		for (int k = 0; k < 3; k++)
			if (top[k] < 0 || g.dlgMs[i] > g.dlgMs[top[k]]) {
				for (int m = 2; m > k; m--) top[m] = top[m - 1];
				top[k] = i;
				break;
			}
	}
	int nTop = 0;
	while (nTop < 3 && top[nTop] >= 0) nTop++;
	double dTotal = 0, dWork = 0;
	for (int i = 0; i < nRows; i++) dTotal += rows[i].ms;	// (TEXT and PANELS are inside these)
	dWork = dTotal - g.ms[DISPLAY] - g.ms[IDLE] - dElse;	// (waiting and anything outside the game left out)

	const int iPx = 11, kRowH = 15, W = 330, H = 30 + nRows * kRowH + 36 + 22 + (nTop + 2) * kRowH;
	R.FillPanel(x, y, x + W, y + H, 6, RGB(8, 9, 12), 235, RGB(70, 74, 82), 200);
	char cTxt[96];
	if (g.lastFrames <= 0) {
		R.TextOutSized(x + 10, y + 8, "Frame times: measuring...", RGB(236, 238, 242), iPx);
		return;
	}
	wsprintf(cTxt, "Frame time, average of the last %d frames", g.lastFrames);
	R.TextOutSized(x + 10, y + 8, cTxt, RGB(236, 238, 242), iPx);

	const int xNum = x + 210, xBar = x + 220, wBar = W - 230;
	for (int i = 0; i < nRows; i++) {
		const int ry = y + 28 + i * kRowH;
		const bool bWait = (i == 7 || i == 11);	// (show + wait, waiting for a change)
		const COLORREF cLab = bWait ? RGB(150, 156, 168) : RGB(214, 218, 226);
		char cL[64];
		if (rows[i].pNote[0] != 0) wsprintf(cL, "%s %s", rows[i].pLabel, rows[i].pNote);
		else lstrcpyA(cL, rows[i].pLabel);
		R.TextOutSized(x + 10, ry, cL, cLab, iPx);
		const int iTenths = (int)(rows[i].ms * 10.0 + 0.5);
		wsprintf(cTxt, "%d.%d ms", iTenths / 10, iTenths % 10);
		R.TextOutSized(xNum - R.TextWidthSized(cTxt, iPx), ry, cTxt, cLab, iPx);
		// bar: a full bar is 16.7 ms, the whole of a frame at 60 FPS
		int bw = (int)(rows[i].ms / 16.7 * wBar + 0.5);
		if (bw > wBar) bw = wBar;
		R.FillPanel(xBar, ry + 4, xBar + wBar, ry + 10, 0, RGB(40, 43, 50), 255);
		if (bw > 0) {
			const COLORREF cBar = bWait ? RGB(110, 116, 128) : rows[i].ms >= 4.0 ? RGB(255, 96, 80)
				: rows[i].ms >= 1.5 ? RGB(255, 176, 64) : RGB(127, 224, 138);
			R.FillPanel(xBar, ry + 4, xBar + bw, ry + 10, 0, cBar, 255);
		}
	}
	const int fy = y + 28 + nRows * kRowH + 4;
	const int iW = (int)(dWork * 10.0 + 0.5);
	const int iMax = (dWork > 0.05) ? (int)(1000.0 / dWork) : 9999;
	wsprintf(cTxt, "Work per frame %d.%d ms  -  enough for about %d FPS", iW / 10, iW % 10, iMax);
	R.TextOutSized(x + 10, fy, cTxt, RGB(255, 196, 80), iPx);
	const int iF = (int)(g.ms[FRAME] * 10.0 + 0.5);
	wsprintf(cTxt, "Whole frame %d.%d ms (all rows above)", iF / 10, iF % 10);
	R.TextOutSized(x + 10, fy + 15, cTxt, RGB(150, 156, 168), iPx);

	// inside those: all the text and code-drawn panels, and the slowest windows
	int ty = fy + 37;
	const int iT = (int)(g.ms[TEXT] * 10.0 + 0.5), iP = (int)(g.ms[PANELS] * 10.0 + 0.5);
	wsprintf(cTxt, "All text %d.%d ms (%d)   All panels %d.%d ms (%d)", iT / 10, iT % 10, (int)(g.per[C_TEXT] + 0.5),
		iP / 10, iP % 10, (int)(g.per[C_PANELS] + 0.5));
	R.TextOutSized(x + 10, ty, cTxt, RGB(214, 218, 226), iPx);
	ty += kRowH;
	// the game drawn on the graphics card (Engine\UiGpu.h)
	if (R.m_iUiGpuSteps <= 0) lstrcpyA(cTxt, "Draw on graphics card: not available");
	else wsprintf(cTxt, "Draw on graphics card: on (%d steps)", R.m_iUiGpuSteps);
	R.TextOutSized(x + 10, ty, cTxt, R.m_iUiGpuSteps > 0 ? RGB(127, 224, 138) : RGB(150, 156, 168), iPx);
	ty += kRowH;
	R.TextOutSized(x + 10, ty, nTop ? "Slowest windows:" : "No windows open.", RGB(150, 156, 168), iPx);
	for (int k = 0; k < nTop; k++) {
		ty += kRowH;
		const int iD = (int)(g.dlgMs[top[k]] * 10.0 + 0.5);
		wsprintf(cTxt, "%d.%d ms", iD / 10, iD % 10);
		R.TextOutSized(x + 22, ty, g.dlgName[top[k]], RGB(214, 218, 226), iPx);
		R.TextOutSized(xNum - R.TextWidthSized(cTxt, iPx), ty, cTxt, RGB(214, 218, 226), iPx);
		int bw = (int)(g.dlgMs[top[k]] / 16.7 * wBar + 0.5);
		if (bw > wBar) bw = wBar;
		R.FillPanel(xBar, ty + 4, xBar + wBar, ty + 10, 0, RGB(40, 43, 50), 255);
		if (bw > 0) R.FillPanel(xBar, ty + 4, xBar + bw, ty + 10, 0, g.dlgMs[top[k]] >= 4.0 ? RGB(255, 96, 80)
			: g.dlgMs[top[k]] >= 1.5 ? RGB(255, 176, 64) : RGB(127, 224, 138), 255);
	}
}

} // namespace

// Called every frame in the game: sends a ping every kPingEvery ms.
void CGame::ClockPingTick(DWORD dwTime)
{
	if (m_cGameMode != GAMEMODE_ONMAINGAME || m_pGSock == NULL) return;
	if (m_dwPingLastSent != 0 && dwTime - m_dwPingLastSent < kPingEvery) return;
	m_dwPingLastSent = dwTime;
	bSendCommand(MSGID_REQUEST_PING, 0, NULL, (int)timeGetTime(), 0, 0, NULL);	// the same clock as NotifyMsg_Pong
}

void CGame::NotifyMsg_Pong(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint32 dwSent;
	uint8 h, m, s;
	Pop(cp, dwSent); Pop(cp, h); Pop(cp, m); Pop(cp, s);
	const DWORD dwNow = timeGetTime();
	const DWORD rtt = dwNow - dwSent;
	if (rtt > 60000) return;		// not one of ours
	m_iPingMs = (int)rtt;
	m_iPingAvg = (m_iPingAvg < 0) ? m_iPingMs : (m_iPingAvg * 3 + m_iPingMs + 2) / 4;
	m_dwPingLastAnswer = dwNow;
	m_iServerSecOfDay = h * 3600 + m * 60 + s;
	m_dwServerSecAt = dwNow - rtt / 2;	// the server read its clock about half way through
}

// The strip's rectangle (game pixels); empty when it isn't shown.
void CGame::ClockPingRect(RECT * pRc)
{
	SetRect(pRc, 0, 0, 0, 0);
	if (!(m_bStripFps || m_bStripPing || m_bStripClock) || m_cGameMode != GAMEMODE_ONMAINGAME) return;
	SetRect(pRc, 0, 0, GAME_W, 17);
}

// A thin see-through strip across the top: FPS, the ping and the clock, side by side from the left.
void CGame::DrawClockPing()
{
	RECT rc;
	ClockPingRect(&rc);
	if (rc.right <= rc.left) return;
	CRenderer & R = m_Renderer;
	const DWORD dwNow = timeGetTime();
	const int iPx = 11, ty = rc.top + 2;

	R.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 0, RGB(0, 0, 0), 110);

	// FPS, ping and clock side by side from the left, kGap apart (whichever are switched on)
	const int kGap = 36;
	int x = rc.left + 8;
	char cFps[16];
	wsprintf(cFps, "%d FPS", m_iFPS);
	const int fx = x;
	if (m_bStripFps) {
		Bold(R, fx, ty, cFps, RGB(127, 224, 138), iPx);	// the same green as a good ping
		x += R.TextWidthSized("000 FPS", iPx) + kGap;		// room for up to 3 digits, so the rest doesn't jump
	}

	// ping: four bars and the milliseconds, in the middle
	const bool bLost = (m_dwPingLastAnswer != 0 && dwNow - m_dwPingLastAnswer > kPingLost + kPingEvery);	// answers stopped
	int nBars;
	COLORREF c;
	if (m_iPingMs < 0 || bLost) { nBars = 0; c = RGB(150, 156, 168); }
	else if (m_iPingMs < 60)  { nBars = 4; c = RGB(127, 224, 138); }
	else if (m_iPingMs < 100) { nBars = 3; c = RGB(127, 224, 138); }
	else if (m_iPingMs < 250) { nBars = 2; c = RGB(255, 176, 64); }
	else                      { nBars = 1; c = RGB(255, 96, 80); }
	char cPing[16];
	if (m_iPingMs < 0 || bLost) strcpy(cPing, "... ms");
	else wsprintf(cPing, "%d ms", m_iPingMs);
	const int pw = 16 + 5 + R.TextWidthSized(cPing, iPx);
	const int px = x, by = rc.bottom - 4;
	if (m_bStripPing) {
		for (int k = 0; k < 4; k++) {
			const int h = 3 + k * 3;
			R.FillPanel(px + k * 4, by - h, px + k * 4 + 3, by, 0, k < nBars ? c : RGB(70, 74, 82), 255);
		}
		Bold(R, px + 21, ty, cPing, c, iPx);
		x += 21 + R.TextWidthSized("000 ms", iPx) + kGap;
	}

	// the clock: the server's time, or this computer's until the server has answered
	int iSec;
	if (m_iServerSecOfDay >= 0) iSec = (m_iServerSecOfDay + (int)((dwNow - m_dwServerSecAt) / 1000)) % 86400;
	else { SYSTEMTIME t; GetLocalTime(&t); iSec = t.wHour * 3600 + t.wMinute * 60 + t.wSecond; }
	const int iH = iSec / 3600, iM = (iSec / 60) % 60, iS = iSec % 60;
	const int iH12 = (iH % 12 == 0) ? 12 : iH % 12;
	const char * pAmPm = (iH < 12) ? "AM" : "PM";
	char cClock[16];
	if (m_bClock24h) wsprintf(cClock, "%02d:%02d", iH, iM);
	else wsprintf(cClock, "%d:%02d %s", iH12, iM, pAmPm);
	if (m_bStripClock) Bold(R, x, ty, cClock, RGB(255, 196, 80), iPx);
	char cTime[16];		// with seconds, for the ping's details
	if (m_bClock24h) wsprintf(cTime, "%02d:%02d:%02d", iH, iM, iS);
	else wsprintf(cTime, "%d:%02d:%02d %s", iH12, iM, iS, pAmPm);

	// where the frame time goes, when hovering the FPS
	// (the tooltip's own drawing is left out of the numbers: everything it adds is put back)
	if (m_bStripFps && ms::X >= fx - 6 && ms::X < fx + R.TextWidthSized(cFps, iPx) + 6 && ms::Y >= rc.top && ms::Y < rc.bottom) {
		LONGLONG acc[prof::COUNT];
		int cnt[prof::C_COUNT];
		memcpy(acc, prof::g.acc, sizeof(acc));
		memcpy(cnt, prof::g.cnt, sizeof(cnt));
		const LONGLONG t0 = prof::Now();
		DrawFrameTimes(R, fx - 6, rc.bottom + 4);
		const LONGLONG dt = prof::Now() - t0;
		memcpy(prof::g.acc, acc, sizeof(acc));
		memcpy(prof::g.cnt, cnt, sizeof(cnt));
		prof::g.acc[prof::UI] -= dt;		// the UI section is still being timed; this takes the tooltip out of it
		prof::g.acc[prof::UPDATE] -= dt;	// ...and so is the whole update
	}

	// details when hovering the ping
	if (m_bStripPing && ms::X >= px - 6 && ms::X < px + pw + 6 && ms::Y >= rc.top && ms::Y < rc.bottom) {
		char cTip[96];
		if (m_iPingMs >= 0) wsprintf(cTip, "Ping %d ms  -  average %d  -  server time %s", m_iPingMs, m_iPingAvg, cTime);
		else wsprintf(cTip, "Waiting for the server...  -  time %s", cTime);
		const int tw = R.TextWidthSized(cTip, 11) + 16;
		const int tx = px - 6;
		R.FillPanel(tx, rc.bottom + 4, tx + tw, rc.bottom + 23, 6, RGB(8, 9, 12), 235);
		R.TextOutSized(tx + 8, rc.bottom + 7, cTip, RGB(236, 238, 242), 11);
	}
}
