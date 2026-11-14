// GameView.cpp: how much of the map the client sees, and the game canvas that goes with it.
//
// Classic Helbreath shows 20 x 15 tiles on a 640x480 canvas. This client asks the game
// server for a bigger view (MSGID_REQUEST_VIEWSIZE) when it connects and whenever the
// display settings change, and uses the canvas size that matches what the server granted:
//
//   view (half x / half y)   tiles on screen   canvas
//   10 x 7  (classic)         20 x 15           640 x 480     old servers
//   14 x 9                    28 x 19           896 x 600     every resolution and display mode
//
// The player stands in the middle of the canvas, so the outermost columns are half tiles on
// both sides, as in the classic view (W = 2 * halfX tiles). The server sends everything out to
// halfX tiles each way, which covers those half columns too.
//
// The window size, fullscreen and the aspect ratio only change how big the picture is drawn
// (Widescreen stretches it to fill the screen), never how much of the map it shows.
//
// Outside the game (character select, loading) the canvas stays 640x480 so those screens look
// exactly as they always did. The sign-in screens over the live town (UI\LoginScene.cpp) are
// 480 tall and as wide as the screen's shape, so the town fills the window or the screen.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

static const int kViewX = 14, kViewY = 9;	// 27 whole columns + a half on each side, 896 x 600

void CGame::GetDesiredView(int * pHalfX, int * pHalfY) const
{
	*pHalfX = kViewX;
	*pHalfY = kViewY;
}

void CGame::GetCanvasForView(int iHalfX, int iHalfY, int * pW, int * pH)
{
	if (iHalfX <= VIEW_HALFX_LEGACY && iHalfY <= VIEW_HALFY_LEGACY) { *pW = 640;  *pH = 480; return; }
	// general case: the player in the middle of the screen (14 x 9 gives 896 x 600). The width
	// is 2 * halfX tiles like the classic 640 (2 * 10): the view starts at the centre of the tile
	// halfX to the left, so it ends at the centre of the tile halfX to the right. (It used to be
	// one tile wider, which showed half a column beyond what the server sends: monsters there
	// were invisible.)
	*pW = 2 * iHalfX * 32;
	*pH = iHalfY * 64 + 24;
}

void CGame::RequestViewSize()
{
	if (m_pGSock == NULL) return;
	int iHalfX, iHalfY;
	GetDesiredView(&iHalfX, &iHalfY);
	if (iHalfX == m_iViewHalfX && iHalfY == m_iViewHalfY) return;
	bSendCommand(MSGID_REQUEST_VIEWSIZE, NULL, NULL, iHalfX, iHalfY, NULL, NULL);
}

// MSGID_RESPONSE_VIEWSIZE: short halfX, short halfY, char hasMapData, [short originX, short originY, map data]
void CGame::ViewSizeResponseHandler(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	short sHalfX = *(short *)cp; cp += 2;
	short sHalfY = *(short *)cp; cp += 2;
	const char cHasMapData = *cp; cp++;
	if (sHalfX < VIEW_HALFX_LEGACY || sHalfX > VIEW_HALFX_MAX || sHalfY < VIEW_HALFY_LEGACY || sHalfY > VIEW_HALFY_MAX) return;

	m_iViewHalfX = sHalfX;
	m_iViewHalfY = sHalfY;

	if (cHasMapData != 0 && m_pMapData != NULL) {
		// Already in the game: re-centre the object buffer for the new view and fill it.
		const short sOriginX = *(short *)cp; cp += 2;
		const short sOriginY = *(short *)cp; cp += 2;
		m_pMapData->Init();
		m_pMapData->m_sPivotX = m_sPlayerX - (m_iViewHalfX + MAPVIEW_ORIGINX);
		m_pMapData->m_sPivotY = m_sPlayerY - (m_iViewHalfY + MAPVIEW_ORIGINY);
		if (m_bIsObserverMode == FALSE) {
			m_pMapData->bSetOwner(m_sPlayerObjectID, m_sPlayerX, m_sPlayerY, m_sPlayerType, m_cPlayerDir,
				m_sPlayerAppr1, m_sPlayerAppr2, m_sPlayerAppr3, m_sPlayerAppr4, m_iPlayerApprColor,
				m_iPlayerStatus, m_cPlayerName, OBJECTSTOP, NULL, NULL, NULL);
		}
		m_sViewDstX = m_sViewPointX = (m_sPlayerX - m_iViewHalfX) * 32;
		m_sViewDstY = m_sViewPointY = (m_sPlayerY - m_iViewHalfY) * 32;
		_ReadMapData(sOriginX, sOriginY, cp);
		m_bIsRedrawPDBGS = TRUE;
	}

	ApplyGameCanvas();
}

// the screens drawn over the live town and the loading screens: sign in, create account,
// character select and creation, quitting, their messages and waits, and entering the world
// (not changing the password, still 640 wide)
bool CGame::OnLoginSceneScreen() const
{
	switch (m_cGameMode) {
	case GAMEMODE_ONLOADING: case GAMEMODE_ONMAINMENU: case GAMEMODE_ONLOGIN: case GAMEMODE_ONCREATENEWACCOUNT: case GAMEMODE_ONQUIT:
	case GAMEMODE_ONMSG: case GAMEMODE_ONLOGRESMSG: case GAMEMODE_ONCONNECTIONLOST: case GAMEMODE_ONVERSIONNOTMATCH:
	case GAMEMODE_ONSELECTCHARACTER: case GAMEMODE_ONCREATENEWCHARACTER: case GAMEMODE_ONQUERYDELETECHARACTER:
	case GAMEMODE_ONQUERYFORCELOGIN:
		return true;
	case GAMEMODE_ONCONNECTING: case GAMEMODE_ONWAITINGRESPONSE:
		return m_cMsg[0] != '4';		// (not from changing the password: that screen is still 640 wide)
	case GAMEMODE_ONWAITINGINITDATA:
		return m_bEnterWorld;			// entering the world after Play
	default:
		return false;
	}
}

void CGame::ApplyGameCanvas()
{
	int iW = 640, iH = 480;
	if (m_cGameMode == GAMEMODE_ONMAINGAME) GetCanvasForView(m_iViewHalfX, m_iViewHalfY, &iW, &iH);
	else if (OnLoginSceneScreen()) {
		// 480 tall, as wide as the window or screen (4:3 and narrower: 640; at most 21:9)
		const float fAspect = m_Renderer.ScreenAspect();
		iW = (int)(480.f * fAspect + 0.5f) & ~1;
		if (iW < 640) iW = 640;
		if (iW > 1120) iW = 1120;
	}
	if (iW == GAME_W && iH == GAME_H) return;

	const int iOldW = GAME_W, iOldH = GAME_H;
	if (!m_Renderer.SetGameSize(iW, iH)) return;
	RelayoutDialogs(iOldW, iOldH);
	m_bIsRedrawPDBGS = TRUE;
}

void CGame::RelayoutDialogs(int iOldW, int iOldH)
{
	const int dX = (GAME_W - iOldW) / 2;	// keep windows where they were relative to the centre
	const int dY = (GAME_H - iOldH) / 2;
	// the chat panel stays in the bottom-left corner unless the player has moved it
	const bool bChatAtHome = (m_dialogBoxes[DIALOG_CHAT].m_X == CHATPANEL_DEFX && m_dialogBoxes[DIALOG_CHAT].m_Y == ChatPanelDefaultY(iOldH));
	for (int i = 0; i < MAXDIALOGBOX; i++) {
		CDialogBox & dlg = m_dialogBoxes[i];
		if (i == 29 || i == 30) continue;	// bottom bar pieces, placed below
		int x = dlg.m_X + dX, y = dlg.m_Y + dY;
		if (x + dlg.sSizeX > GAME_W) x = GAME_W - dlg.sSizeX;
		if (y + dlg.sSizeY > GAME_H) y = GAME_H - dlg.sSizeY;
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		dlg.m_X = (short)x;
		dlg.m_Y = (short)y;
	}
	// Bottom bar: the classic bar stretched across the whole width (see DrawDialogBox_IconPanel).
	m_dialogBoxes[29].m_X = 0;
	m_dialogBoxes[29].m_Y = (short)(GAME_H - 46);
	m_dialogBoxes[30].m_X = 0;
	m_dialogBoxes[30].m_Y = (short)VIEW_BOTTOM;
	m_dialogBoxes[30].LayoutIconPanel((short)GAME_W);
	if (bChatAtHome) {
		m_dialogBoxes[DIALOG_CHAT].m_X = CHATPANEL_DEFX;
		m_dialogBoxes[DIALOG_CHAT].m_Y = ChatPanelDefaultY(GAME_H);
	}
}
