// SettingsMenuSetup.cpp: puts the game's options into the full-resolution settings menu.
//
// Every option the old System Menu (F12) and Extended Menu offered lives here now,
// sorted into tabs. To add an option, add one m.AddToggle / AddChoice / AddSlider /
// AddButton line with a getter and a setter. Options that should be remembered between
// sessions also need a line in CGame::ReadSettings / CGame::WriteSettings.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\Engine\SettingsMenu.h"

#if LANGUAGE == ENGLISH
#include "..\lan_eng.h"
#elif LANGUAGE == SPANISH
#include "..\lan_spa.h"
#endif

#include <string>

extern HWND G_hWnd;

static const int kFpsLimits[4] = { 60, 100, 144, 240 };	// FPS Limit choices (240 is the most: CRenderer::kMaxFps)
extern void SetKeyboardHook(bool enable);

//////////////////////////////////////////////////////////////////////
// Opening / closing
//////////////////////////////////////////////////////////////////////

bool CGame::IsSettingsMenuOpen() const
{
	return m_pSettingsMenu != NULL && m_pSettingsMenu->IsOpen();
}

void CGame::OpenSettingsMenu()
{
	if (m_pSettingsMenu == NULL || m_cGameMode != GAMEMODE_ONMAINGAME) return;
	m_pSettingsMenu->Open();
}

void CGame::CloseSettingsMenu()
{
	if (m_pSettingsMenu != NULL) m_pSettingsMenu->Close();
}

void CGame::ToggleSettingsMenu()
{
	if (IsSettingsMenuOpen()) CloseSettingsMenu();
	else OpenSettingsMenu();
}

// Hands the game's normal arrow cursor (frame 0 of the cursor sprite) to the renderer,
// which draws it on top of the menu at the same size as in game.
void CGame::SetMenuCursor()
{
	CSprite * pSpr = m_pSprite[SPRID_MOUSECURSOR];
	if (pSpr == NULL || pSpr->m_stBrush == NULL || pSpr->m_iTotalFrame < 1) return;
	if (pSpr->m_bIsSurfaceEmpty && !pSpr->_iOpenSprite()) return;
	if (pSpr->m_lpSurface == NULL || pSpr->m_lpSurface->GetPixels() == NULL) return;
	const stBrush & b = pSpr->m_stBrush[0];
	const int iPitch = pSpr->m_lpSurface->GetPitch();
	if (b.sx < 0 || b.sy < 0 || b.szx <= 0 || b.szy <= 0
		|| b.sx + b.szx > pSpr->m_lpSurface->GetWidth() || b.sy + b.szy > pSpr->m_lpSurface->GetHeight()) return;
	const WORD * pSrc = pSpr->m_lpSurface->GetPixels() + (size_t)b.sy * iPitch + b.sx;
	m_Renderer.SetUiCursor(pSrc, iPitch, b.szx, b.szy, pSpr->m_wColorKey, b.pvx, b.pvy);
}

//////////////////////////////////////////////////////////////////////
// Option helpers (same behaviour as the old System Menu buttons)
//////////////////////////////////////////////////////////////////////

void CGame::ApplyMusicVolume()
{
	if (!m_bSoundFlag || m_pBGM == NULL) return;
	int iVol = (m_cMusicVolume - 100) * 20;
	if (iVol > 0) iVol = 0;
	if (iVol < -10000) iVol = -10000;
	m_pBGM->bStop(TRUE);
	m_pBGM->Play(FALSE, 0, iVol);
}

void CGame::SetSoundEnabled(bool bOn)
{
	if (!m_bSoundFlag || (m_bSoundStat != FALSE) == bOn) return;
	if (!bOn) {
		if (m_pESound[38] != NULL) m_pESound[38]->bStop();
		m_bSoundStat = FALSE;
		AddEventList(NOTIFY_MSG_SOUND_OFF, 10);
	}
	else {
		m_bSoundStat = TRUE;
		AddEventList(NOTIFY_MSG_SOUND_ON, 10);
	}
}

void CGame::SetMusicEnabled(bool bOn)
{
	if (!m_bSoundFlag || (m_bMusicStat != FALSE) == bOn) return;
	if (!bOn) {
		m_bMusicStat = FALSE;
		AddEventList(NOTIFY_MSG_MUSIC_OFF, 10);
		if (m_pBGM != NULL) {
			m_pBGM->bStop();
			delete m_pBGM;
			m_pBGM = NULL;
		}
	}
	else {
		m_bMusicStat = TRUE;
		AddEventList(NOTIFY_MSG_MUSIC_ON, 10);
		StartBGM();
	}
}

void CGame::RequestLogout()
{
	if (m_bForceDisconn) return;
	if (m_bSkillUsingStatus) {
		AddEventList(SKILL_LOGOUT, 10);
		CloseSettingsMenu();
		return;
	}
	if (m_cLogOutCount == -1) {
#ifdef _DEBUG
		m_cLogOutCount = 1;
#else
		m_cLogOutCount = 11;
#endif
	}
	else {
		m_cLogOutCount = -1;
		AddEventList(DLGBOX_CLICK_SYSMENU2, 10);
		CloseSettingsMenu();
	}
}

void CGame::RequestRestart()
{
	if (m_bForceDisconn) return;
	if ((m_iHP <= 0) && (m_cRestartCount == -1)) {
		m_cRestartCount = 5;
		m_dwRestartCountTime = timeGetTime();
		CloseSettingsMenu();
		wsprintf(G_cTxt, DLGBOX_CLICK_SYSMENU1, m_cRestartCount);	// "Restarting game....%d"
		AddEventList(G_cTxt, 10);
	}
}

//////////////////////////////////////////////////////////////////////
// Building the menu
//////////////////////////////////////////////////////////////////////

void CGame::BuildSettingsMenu()
{
	if (m_pSettingsMenu == NULL) return;
	CSettingsMenu & m = *m_pSettingsMenu;
	m.Clear();
	m.SetTitle("Options");
	m.SetCanShow([this]() { return m_cGameMode == GAMEMODE_ONMAINGAME; });
	m.SetClickSound([this]() { PlaySound('E', 14, 5); });
	// (no clock in the corner: the strip at the top of the screen has it, UI\ClockPing.cpp)
	m.SetOnClose([this]() { WriteSettings(); });

	// ---------------------------------------------------------------- General
	const int tGeneral = m.AddTab("General");

	m.AddSection(tGeneral, "SERVER");
	m.AddInfo(tGeneral, "Server:", "The game world you are playing on.", [this]() {
#ifdef _DEBUG
		return std::string("Test server");
#else
		if (strcmp(m_cWorldServerName, NAME_WORLDNAME1) == 0) return std::string(MSG_WORLDNAME1);
		if (strcmp(m_cWorldServerName, NAME_WORLDNAME2) == 0) return std::string(MSG_WORLDNAME2);
		return std::string(m_cWorldServerName);
#endif
	});
	m.AddInfo(tGeneral, "Date:", "Today's date on your computer.", []() {
		SYSTEMTIME t;
		GetLocalTime(&t);
		char buf[32];
		wsprintf(buf, "%d/%d/%d", t.wMonth, t.wDay, t.wYear);
		return std::string(buf);
	});

	m.AddSection(tGeneral, "ON SCREEN");
	// the strip across the top of the screen (UI\ClockPing.cpp)
	m.AddToggle(tGeneral, "Show FPS:", "Frames per second, in the strip across the top of the screen. Hover it to see where each frame's time goes.",
		[this]() { return m_bStripFps ? 1 : 0; },
		[this](int v) { m_bStripFps = (v != 0); });
	m.AddToggle(tGeneral, "Show ping:", "Your connection speed to the server, in the strip across the top of the screen.",
		[this]() { return m_bStripPing ? 1 : 0; },
		[this](int v) { m_bStripPing = (v != 0); });
	m.AddToggle(tGeneral, "Show clock:", "The server's time, in the strip across the top of the screen.",
		[this]() { return m_bStripClock ? 1 : 0; },
		[this](int v) { m_bStripClock = (v != 0); });
	{
		CSettingsMenu::Item & it = m.AddChoice(tGeneral, "Clock format:", "How the clock shows the time: military (15:07) or standard (3:07 PM).",
			{ "Military (15:07)", "Standard (3:07 PM)" },
			[this]() { return m_bClock24h ? 0 : 1; },
			[this](int v) { m_bClock24h = (v == 0); });
		it.enabled = [this]() { return m_bStripClock; };
	}
	m.AddToggle(tGeneral, "Show time:", "Shows the current time on screen.",
		[this]() { return m_showTime ? 1 : 0; },
		[this](int v) { m_showTime = (v != 0); });

	m.AddSection(tGeneral, "SYSTEM");
	m.AddToggle(tGeneral, "Windows key:", "When Off, the Windows key is blocked while you play, so it can't minimise the game by accident.",
		[this]() { return m_windowsKey ? 1 : 0; },
		[this](int v) { m_windowsKey = (v != 0); SetKeyboardHook(!m_windowsKey); });
	m.AddToggle(tGeneral, "Screenshot on kill:", "Automatically saves a screenshot when you kill an enemy player.",
		[this]() { return m_ekScreenshot ? 1 : 0; },
		[this](int v) {
			m_ekScreenshot = (v != 0);
			AddEventList(m_ekScreenshot ? BCHECK_LOCAL_CHAT_COMMAND16 : BCHECK_LOCAL_CHAT_COMMAND17, 10);
		});
	m.AddButton(tGeneral, "Away from keyboard:", "Marks you as away (AFK), or back again.",
		[]() { return std::string("Toggle AFK"); },
		[this]() { bSendCommand(MSGID_COMMAND_CHATMSG, NULL, NULL, NULL, NULL, NULL, "/afk"); });

	// ---------------------------------------------------------------- Graphics
	const int tGraphics = m.AddTab("Graphics");

	m.AddSection(tGraphics, "WINDOW");
	m.AddChoice(tGraphics, "Display Mode:", "Fullscreen fills your monitor. You can also switch at any time with Alt+Enter.",
		{ "Windowed", "Fullscreen" },
		[this]() { return m_Renderer.m_bFullMode ? 1 : 0; },
		[this](int v) {
			if ((v != 0) == (m_Renderer.m_bFullMode != FALSE)) return;
			m_bIsRedrawPDBGS = TRUE;
			m_Renderer.ChangeDisplayMode(G_hWnd);
			WriteSettingsVar("Fullscreen", m_Renderer.m_bFullMode ? 1 : 0);
			RequestViewSize();		// widescreen view only applies in fullscreen
		});
	{
		CSettingsMenu::Item & it = m.AddChoice(tGraphics, "Window Style:",
			"Bordered gives the window a normal title bar, so you can move or minimise it. Borderless hides it. Only for windowed mode.",
			{ "Bordered", "Borderless" },
			[this]() { return (!m_Renderer.m_bFullMode && m_Renderer.m_bBorderless) ? 1 : 0; },	// fullscreen always shows Bordered
			[this](int v) {
				if (m_Renderer.m_bFullMode || (v != 0) == (m_Renderer.m_bBorderless != FALSE)) return;
				m_bIsRedrawPDBGS = TRUE;
				m_Renderer.SetBorderless(v != 0 ? TRUE : FALSE);
				WriteSettingsVar("WindowBorderless", m_Renderer.m_bBorderless ? 1 : 0);
			});
		it.enabled = [this]() { return m_Renderer.m_bFullMode == FALSE; };
	}
	{
		CSettingsMenu::Item & it = m.AddChoice(tGraphics, "Aspect Ratio:",
			"Letterbox keeps the picture's shape with black bars at the sides. Widescreen stretches it to fill the screen. Both show the same area of the map. Widescreen is only for fullscreen.",
			{ "Letterbox", "Widescreen" },
			[this]() { return (m_Renderer.m_bFullMode && m_Renderer.m_bWidescreen) ? 1 : 0; },	// windowed is always Letterbox
			[this](int v) {
				if (!m_Renderer.m_bFullMode) return;
				m_Renderer.SetWidescreen(v != 0 ? TRUE : FALSE);
				WriteSettingsVar("Widescreen", m_Renderer.m_bWidescreen ? 1 : 0);
				RequestViewSize();
			});
		it.enabled = [this]() { return m_Renderer.m_bFullMode != FALSE; };
	}
	{
		std::vector<std::string> sizes;
		for (int i = 0; i < CRenderer::kNumWindowSizes; i++) {
			char buf[32];
			wsprintf(buf, "%dx%d", CRenderer::kWindowSizes[i][0], CRenderer::kWindowSizes[i][1]);
			sizes.push_back(buf);
		}
		char full[32];
		wsprintf(full, "%dx%d", CRenderer::kFullscreenW, CRenderer::kFullscreenH);
		CSettingsMenu::Item & it = m.AddDropdown(tGraphics, "Resolution:",
			"Size of the game window. Fullscreen always uses 1920x1080. Every size shows the same area of the map, just larger or smaller.",
			sizes,
			[this]() {
				int w, h;
				m_Renderer.GetWindowSize(&w, &h);
				for (int i = 0; i < CRenderer::kNumWindowSizes; i++)
					if (CRenderer::kWindowSizes[i][0] == w && CRenderer::kWindowSizes[i][1] == h) return i;
				return -1;
			},
			[this](int v) {
				if (m_Renderer.m_bFullMode || v < 0 || v >= CRenderer::kNumWindowSizes) return;
				m_iWindowSizeIdx = v;
				m_bIsRedrawPDBGS = TRUE;
				m_Renderer.SetWindowSize(CRenderer::kWindowSizes[v][0], CRenderer::kWindowSizes[v][1]);
				WriteSettingsVar("WindowSize", (uint32)v);
			});
		it.enabled = [this]() { return m_Renderer.m_bFullMode == FALSE; };
		it.disabledText = full;
	}

	m.AddSection(tGraphics, "QUALITY");
	m.AddChoice(tGraphics, "Detail Level:", "Lower detail draws fewer effects, which helps on slower computers.",
		{ "Low", "Normal", "High" },
		[this]() { return (int)m_cDetailLevel; },
		[this](int v) {
			m_cDetailLevel = (char)v;
			if (v == 0) AddEventList(NOTIFY_MSG_DETAIL_LEVEL_LOW, 10);
			else if (v == 1) AddEventList(NOTIFY_MSG_DETAIL_LEVEL_MEDIUM, 10);
			else AddEventList(NOTIFY_MSG_DETAIL_LEVEL_HIGH, 10);
		});
	m.AddToggle(tGraphics, "HD Graphics:",
		"Draws the game at twice the resolution, using the high-definition sprites in SPRITES\\HD where there are any. Off shows the original graphics (and uses less memory).",
		[this]() { return m_Renderer.m_bHdSprites ? 1 : 0; },
		[this](int v) {
			m_Renderer.m_bHdSprites = (v != 0) ? TRUE : FALSE;
			WriteSettingsVar("HdSprites", m_Renderer.m_bHdSprites ? 1 : 0);
			m_bIsRedrawPDBGS = TRUE;	// redraw the ground with / without its HD copy
		});
	m.AddChoice(tGraphics, "Render Quality:",
		"How the picture is enlarged to your screen. Auto, x2 and Max x4 keep the original pixels (sharper at higher settings). HD smooths the edges of sprites and scenery so they look clean instead of blocky, without blurring.",
		{ "Auto", "x2", "Max x4", "HD" },
		[this]() { return m_Renderer.m_iRenderQuality; },
		[this](int v) {
			m_Renderer.SetRenderQuality(v);
			WriteSettingsVar("RenderQuality", (uint32)m_Renderer.m_iRenderQuality);
		});
	{
		CSettingsMenu::Item & it = m.AddChoice(tGraphics, "FPS Limit:",
			"The most frames per second the game will draw. Pick your monitor's refresh rate: more frames than it shows only work your PC harder. Turn VSync off to use it.",
			{ "60", "100", "144", "240" },
			[this]() {	// (the nearest choice, for a limit set some other way)
				int iBest = 3;
				for (int i = 0; i < 4; i++)
					if (abs(m_Renderer.m_iFrameLimit - kFpsLimits[i]) < abs(m_Renderer.m_iFrameLimit - kFpsLimits[iBest])) iBest = i;
				return iBest;
			},
			[this](int v) {
				m_Renderer.SetFrameLimit((v >= 0 && v < 4) ? kFpsLimits[v] : 240);
				WriteSettingsVar("FPSLimit", (uint32)m_Renderer.m_iFrameLimit);
			});
		it.enabled = [this]() { return m_Renderer.m_bVSync == FALSE; };
	}
	m.AddToggle(tGraphics, "VSync:", "Matches the frame rate to your monitor's refresh rate, which stops tearing. The FPS limit is not used while it's on.",
		[this]() { return m_Renderer.m_bVSync ? 1 : 0; },
		[this](int v) {
			m_Renderer.SetVSync(v ? TRUE : FALSE);
			WriteSettingsVar("VSync", m_Renderer.m_bVSync ? 1 : 0);
		});

	m.AddSection(tGraphics, "WORLD");
	m.AddToggle(tGraphics, "Night lighting:",
		"At night the world darkens to moonlight, and street lamps, spells and your character light it up. Off shows the old night colours.",
		[this]() { return m_bNightLights ? 1 : 0; },
		[this](int v) { m_bNightLights = (v != 0); WriteSettingsVar("NightLights", m_bNightLights ? 1 : 0); });
	{
		CSettingsMenu::Item & it = m.AddSlider(tGraphics, "Night darkness:", "How dark the night is away from any light.", 0, 100, "%",
			[this]() { return m_iNightDarkness; },
			[this](int v) { m_iNightDarkness = v; });
		it.enabled = [this]() { return m_bNightLights; };
	}
	m.AddToggle(tGraphics, "Show big items on floor:", "Draws items lying on the ground larger, so they are easier to spot.",
		[this]() { return m_bigItems ? 1 : 0; },
		[this](int v) {
			m_bigItems = (v != 0);
			AddEventList(m_bigItems ? BCHECK_LOCAL_CHAT_COMMAND14 : BCHECK_LOCAL_CHAT_COMMAND15, 10);
		});
	m.AddToggle(tGraphics, "Tile Grid:", "Draws a grid over the map tiles.",
		[this]() { return m_showGrid ? 1 : 0; },
		[this](int v) { m_showGrid = (v != 0); });
	m.AddMultiDropdown(tGraphics, "Circle Indicators:",
		"Draws a glowing circle on the ground under characters: red under enemy players, orange under monsters, green under your party. Tick any mix of them.",
		{ "Off", "Enemies", "Monsters", "Party", "All" },
		[this]() { return m_iCircleIndicators; },
		[this](int v) { m_iCircleIndicators = v & CIRCLE_ALL; });

	// ---------------------------------------------------------------- Interface
	const int tInterface = m.AddTab("Interface");

	m.AddSection(tInterface, "WINDOWS");
	m.AddToggle(tInterface, "Transparent windows:", "Makes the game's dialog windows see-through. F11 or Ctrl+W also switches this.",
		[this]() { return m_bDialogTrans ? 1 : 0; },
		[this](int v) { m_bDialogTrans = v ? TRUE : FALSE; });
	m.AddToggle(tInterface, "Guide map:", "Shows the small guide map of the area you are in.",
		[this]() { return m_bIsDialogEnabled[9] ? 1 : 0; },
		[this](int v) {
			if (v) EnableDialogBox(9, 0, 0, 0, NULL);
			else DisableDialogBox(9);
		});
	m.AddToggle(tInterface, "Quest counter:", "Shows your current quest progress on screen.",
		[this]() { return m_displayQuest ? 1 : 0; },
		[this](int v) {
			m_displayQuest = (v != 0);
			AddEventList(m_displayQuest ? BCHECK_LOCAL_CHAT_COMMAND22 : BCHECK_LOCAL_CHAT_COMMAND23, 10);
		});

	m.AddSection(tInterface, "COMBAT");
	m.AddToggle(tInterface, "Group damage numbers:", "On: damage numbers are added up into groups of hits. Off: every hit shows its own number.",
		[this]() { return m_showAllDmg ? 1 : 0; },
		[this](int v) {
			m_showAllDmg = (v != 0);
			AddEventList(m_showAllDmg ? BCHECK_LOCAL_CHAT_COMMAND12 : BCHECK_LOCAL_CHAT_COMMAND13, 10);
		});

	// ---------------------------------------------------------------- Gameplay
	const int tGameplay = m.AddTab("Gameplay");

	m.AddSection(tGameplay, "PARTY");
	m.AddToggle(tGameplay, "Auto-accept party invites:", "Joins a party automatically when someone invites you.",
		[this]() { return m_partyAutoAccept ? 1 : 0; },
		[this](int v) { m_partyAutoAccept = v ? TRUE : FALSE; });

	m.AddSection(tGameplay, "CRAFTING");
	m.AddToggle(tGameplay, "Auto-fill manufacturing:", "Picks the ingredients from your bag automatically when you manufacture.",
		[this]() { return m_manuAutoFill ? 1 : 0; },
		[this](int v) {
			m_manuAutoFill = (v != 0);
			AddEventList(m_manuAutoFill ? BCHECK_LOCAL_CHAT_COMMAND20 : BCHECK_LOCAL_CHAT_COMMAND21, 10);
		});

	// ---------------------------------------------------------------- Chat
	const int tChat = m.AddTab("Chat");

	m.AddSection(tChat, "MESSAGES");
	m.AddToggle(tChat, "Whispers:", "Receive private messages (whispers) from other players.",
		[this]() { return m_bWhisper ? 1 : 0; },
		[this](int v) {
			m_bWhisper = v ? TRUE : FALSE;
			AddEventList(m_bWhisper ? BCHECK_LOCAL_CHAT_COMMAND6 : BCHECK_LOCAL_CHAT_COMMAND7, 10);
		});
	m.AddToggle(tChat, "Shouts:", "See public shout messages from other players.",
		[this]() { return m_bShout ? 1 : 0; },
		[this](int v) {
			m_bShout = v ? TRUE : FALSE;
			AddEventList(m_bShout ? BCHECK_LOCAL_CHAT_COMMAND8 : BCHECK_LOCAL_CHAT_COMMAND9, 10);
		});
	m.AddToggle(tChat, "Time stamps:", "Shows the time next to each chat message.",
		[this]() { return m_showTimeStamp ? 1 : 0; },
		[this](int v) {
			m_showTimeStamp = (v != 0);
			AddEventList(m_showTimeStamp ? BCHECK_LOCAL_CHAT_COMMAND10 : BCHECK_LOCAL_CHAT_COMMAND11, 10);
		});
	m.AddToggle(tChat, "Name alert:", "Flashes the game in the taskbar when someone mentions your name while you are in another window.",
		[this]() { return m_tabbedNotification ? 1 : 0; },
		[this](int v) {
			m_tabbedNotification = (v != 0);
			AddEventList(m_tabbedNotification ? BCHECK_LOCAL_CHAT_COMMAND18 : BCHECK_LOCAL_CHAT_COMMAND19, 10);
		});

	m.AddSection(tChat, "PLAYERS");
	m.AddButton(tChat, "Friend list:", "Opens your friend list.",
		[]() { return std::string("Open"); },
		[this]() {
			CloseSettingsMenu();
			LoadFriendList();
			UpdateFriendsStatus();
			EnableDialogBox(43, NULL, NULL, NULL);
		});
	m.AddButton(tChat, "Mute list:", "Opens the list of players you have muted.",
		[]() { return std::string("Open"); },
		[this]() {
			CloseSettingsMenu();
			if (!m_bIsDialogEnabled[60]) EnableDialogBox(60, NULL, NULL, NULL);
		});

	// ---------------------------------------------------------------- Audio
	const int tAudio = m.AddTab("Audio");
	const std::string noAudio = "No sound device";

	m.AddSection(tAudio, "SOUND");
	{
		CSettingsMenu::Item & it = m.AddToggle(tAudio, "Sound effects:", "Turns all game sound effects on or off.",
			[this]() { return m_bSoundStat ? 1 : 0; },
			[this](int v) { SetSoundEnabled(v != 0); });
		it.enabled = [this]() { return m_bSoundFlag != FALSE; };
		it.disabledText = noAudio;
	}
	{
		CSettingsMenu::Item & it = m.AddSlider(tAudio, "Effects volume:", "How loud sound effects are.", 0, 100, "%",
			[this]() { return (int)m_cSoundVolume; },
			[this](int v) { m_cSoundVolume = (char)v; });
		it.enabled = [this]() { return m_bSoundFlag != FALSE; };
		it.disabledText = noAudio;
	}
	{
		CSettingsMenu::Item & it = m.AddToggle(tAudio, "Music:", "Turns the background music on or off.",
			[this]() { return m_bMusicStat ? 1 : 0; },
			[this](int v) { SetMusicEnabled(v != 0); });
		it.enabled = [this]() { return m_bSoundFlag != FALSE; };
		it.disabledText = noAudio;
	}
	{
		CSettingsMenu::Item & it = m.AddSlider(tAudio, "Music volume:", "How loud the background music is.", 0, 100, "%",
			[this]() { return (int)m_cMusicVolume; },
			[this](int v) { m_cMusicVolume = (char)v; ApplyMusicVolume(); });
		it.enabled = [this]() { return m_bSoundFlag != FALSE; };
		it.disabledText = noAudio;
	}

	// ---------------------------------------------------------------- Bottom-left buttons
	m.AddSideButton(
		[this]() { return std::string((m_cLogOutCount == -1) ? "Log Out" : "Cancel Log Out"); },
		[this]() { return !m_bForceDisconn; },
		[this]() { RequestLogout(); },
		"Logs out after a short countdown. Click again to cancel.", true);
	m.AddSideButton(
		[]() { return std::string("Restart"); },
		[this]() { return (m_iHP <= 0) && (m_cRestartCount == -1) && !m_bForceDisconn; },
		[this]() { RequestRestart(); },
		"Restarts at your town's respawn point. Only available while you are dead.");
}
