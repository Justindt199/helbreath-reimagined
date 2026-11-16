// Game.h: interface for the CGame class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GAME_H__0089D9E3_74E6_11D2_A8E6_00001C7030A6__INCLUDED_)
#define AFX_GAME_H__0089D9E3_74E6_11D2_A8E6_00001C7030A6__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mmsystem.h> 
#include <winbase.h>
#include <cstring>
#include <vector>
#include <map>
#include <functional>

#include <process.h>
#include <direct.h>
#include <set>
#include <string>
#include <deque>

#include "..\shared\common.h"
#include "..\shared\NetMessages.h"
#include "..\shared\npcType.h"
#include "..\shared\maths.h"
#include "..\shared\ValoriaEnchant.h"
#include "..\shared\ValoriaPassives.h"
#include "..\shared\ValoriaUse.h"

#include "GlobalDef.h"
#include "Engine\Renderer.h"
#include "Engine\Mouse.h"
#include "Engine\SoundBuffer.h"
#include "Engine\AudioDevice.h"
#include "net\XSocket.h"
#include "Engine\Sprite.h"
#include "Engine\SpriteID.h"
#include "Misc.h"
#include "StrTok.h"
#include "net\Msg.h"
#include "Engine\Effect.h"
#include "Engine\LevelUpBadges.h"
#include "map\MapData.h"
#include "char\ActionID.h"
#include "ui\MouseInterface.h"
#include "char\CharInfo.h"
#include "char\item\Item.h"
#include "char\Magic.h"
#include "char\Skill.h"
#include "char\DynamicObjectID.h"
#include "char\item\BuildItem.h"
#include "char\item\ItemName.h"
#include "char\Curse.h"
#include "char\guild.h"
#include "char\mail.h"
#include "ui\DialogBox.h"

#ifdef USING_WIN_IME
	#include <RICHEDIT.H>
#endif

#define BTNSZX				74
#define BTNSZY				20
#define LBTNPOSX			30
#define RBTNPOSX			154
#define BTNPOSY			292

#define MAXDIALOGBOX		71


#define INDEX4_MSGID		0
#define INDEX2_MSGTYPE		4

#define SOCKETBLOCKLIMIT	300

#define MAXSPRITES			25000	// 20000 // Snoopy: Adjusted!
#define MAXTILES			1000	// 800 // Snoopy: Adjusted!
#define MAXEFFECTSPR		300
#define MAXSOUNDEFFECTS		200		// 110   //Snoopy: Adjusted!
#define MAXCHATMSGS			500
#define MAXWHISPERMSG		5
#define MAXCHATSCROLLMSGS	80
#define MAXEFFECTS			300	//600 <- original
#define CHATTIMEOUT_A		4000
#define CHATTIMEOUT_B		500
#define CHATTIMEOUT_C		2000
#define MAXITEMS			50
#define MAXMENUITEMS		140  //v2.15  120 ->140
#define TEXTDLGMAXLINES		300 //v2.18 3000->300

#define MAXMAGICTYPE		100
#define MAXWEATHEROBJECTS	600
#define MAXBUILDITEMS		100
#define MAXGAMEMSGS			300
#define MAXITEMNAMES		1000
#define MAXGUILDNAMES		100
#define MAXSELLLIST			12

#define WM_USER_GAMESOCKETEVENT	WM_USER + 2000
#define WM_USER_LOGSOCKETEVENT	WM_USER + 2001
#define WM_USER_VOTESOCKETEVENT	WM_USER + 2002

#define GAMEMODE_NULL					-2
#define GAMEMODE_ONQUIT					-1
#define GAMEMODE_ONMAINMENU				0
#define GAMEMODE_ONCONNECTING			1
#define GAMEMODE_ONLOADING				2
#define GAMEMODE_ONWAITINGINITDATA		3
#define GAMEMODE_ONMAINGAME				4
#define GAMEMODE_ONCONNECTIONLOST		5
#define GAMEMODE_ONMSG					6
#define GAMEMODE_ONCREATENEWACCOUNT		7
#define GAMEMODE_ONLOGIN				8
#define GAMEMODE_ONQUERYFORCELOGIN		9
#define GAMEMODE_ONSELECTCHARACTER		10
#define GAMEMODE_ONCREATENEWCHARACTER	11
#define GAMEMODE_ONWAITINGRESPONSE		12
#define GAMEMODE_ONQUERYDELETECHARACTER 13
#define GAMEMODE_ONLOGRESMSG			14
#define GAMEMODE_ONCHANGEPASSWORD		15
#define GAMEMODE_ONVERSIONNOTMATCH		17
#define GAMEMODE_ONINTRODUCTION			18
#define GAMEMODE_ONAGREEMENT			19
#define GAMEMODE_ONSELECTSERVER			20
#define GAMEMODE_ONINPUTKEYCODE			21

#define SERVERTYPE_GAME			1
#define SERVERTYPE_LOG			2

#define CURSORSTATUS_NULL		0
#define CURSORSTATUS_PRESSED	1
#define CURSORSTATUS_SELECTED	2
#define CURSORSTATUS_DRAGGING	3

#define SELECTEDOBJTYPE_DLGBOX	1
#define SELECTEDOBJTYPE_ITEM	2

// A double-click: the second click within the Windows double-click time (Control Panel > Mouse,
// 500 ms by default; HB used a fixed 300 ms, too short for many players) and within DOUBLECLICKMOVE pixels.
#define DOUBLECLICKTIME			((DWORD)GetDoubleClickTime())
#define DOUBLECLICKMOVE			4
#define MAXCRUSADESTRUCTURES	300

#define NONE						-1
#define STAT_STR					0
#define STAT_VIT					1
#define STAT_DEX					2
#define STAT_INT					3
#define STAT_MAG					4
#define STAT_CHR					5

#define MAP_PING_DURATION		12

//#define GAMEFUNCT(x) std::bind1st(std::mem_fun(&CGame::x), this)
#define GAMEFUNCT(x) std::bind(&CGame::x, this)


// Game canvas helpers (the canvas is 640x480 outside the game and bigger in it).
#define GAME_W			(RENDER_WIDTH)
#define GAME_H			(RENDER_HEIGHT)
#define VIEW_BOTTOM		(GAME_H - 53)			// top of the bottom bar (427 on 640x480)
#define UI_OX			((GAME_W - 640) / 2)	// centre a classic 640-wide element
#define BAR_SCALE		((float)GAME_W / 640.0f)	// the classic bottom bar is stretched to the canvas width
#define BAR_X(x)		((short)((x) * GAME_W / 640))	// a classic bar x position, stretched
#define UI_OY			(GAME_H - 480)			// classic bottom-of-screen positions, moved down
#define VIEW_CX			(m_iViewHalfX * 32)		// player's screen position, for sound panning (320 / 240 classic)
#define VIEW_CY			(m_iViewHalfY * 32 + 16)

class CGame
{
public:
	void ShowSoccerVictory(short sSide);
	void NotifyMsg_Soccer(char * pData);
	void DrawSoccerInfo();

	void DrawDialogBox_QuestList();
	void DlgBoxClick_QuestList();

	void TopsiteVote();
	void OnVoteSocketEvent(WPARAM wParam, LPARAM lParam);
	void NotifyMsg_Heldenian(char * pData);
	void DrawHeldenianStats();
	void TimeStamp(char * pString);
	void DrawDialogBox_GuildBank();//14
	void DlgBoxClick_GuildBank();
	void bItemDrop_GuildBank();
	
	void ReceiveGuildInfo(char * data);
	void ReceiveGuildsmanStatus(char * data);
	void GuildContributeRsp(char * data);
	void NotifyMsg_ItemToGuildBank(char * pData);
	void InitItemList_GuildBank(char * data);
	void RetrieveItemHandler_GuildBank(char * pData);

	BOOL m_bHappyHour;
	void DrawQuestHelper();
	void LoadFriendList();
	void SaveFriendList();
	void DrawDialogBox_FriendList();
	void DlgBoxClick_FriendList();

	uint32 m_adminLevel;
	uint8 m_GMFlags;
	uint32 m_eventPot;

	Vector<string> m_voteUrls;
	int m_voteState;
	XSocket * m_voteSocket;
	DWORD lastScreenUpdate;

	int m_iTotalFriends;
	int m_iFriendIndex;
	DWORD lastFriendUpdate;
	struct {
		bool online;
		char friendName[13];
		bool updated;
	} friendsList[13];

	// CLEROTH - AURAS
	void CheckActiveAura(short sX, short sY, DWORD dwTime, short sOwnerType);
	void CheckActiveAura2(short sX, short sY, DWORD dwTime, short sOwnerType);

	// MJ Stats Change Related functions - Alastor
	void DrawDialogBox_ChangeStatsMajestic();// Change stats using majestic - Alastor
	void DlgBoxClick_ChangeStatsMajestic();			// Change stats using majestic - Alastor

	int m_iTeleportMapCount;
	void ResponseTeleportList(char * pData);
	void ResponseChargedTeleport(char * pData);

	int m_iQuestCount;
	void ResponseQuestList(char * pData);

	void * operator new (size_t size)
	{
		return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	};

	void operator delete(void * mem)
	{
		HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, mem);
	};

	void ItemEquipHandler(char cItemID);
	void ReleaseEquipHandler(char cEquipPos);

	void ReadSettings();
	uint32 ReadSettingsVar(const char * var);
	void WriteSettings();
	void WriteSettingsVar(const char * var, uint32 val);

	// Full-resolution settings menu (UI\SettingsMenuSetup.cpp)
	void BuildSettingsMenu();
	void OpenSettingsMenu();
	void CloseSettingsMenu();
	void ToggleSettingsMenu();
	bool IsSettingsMenuOpen() const;
	void ApplyMusicVolume();
	void SetSoundEnabled(bool bOn);
	void SetMusicEnabled(bool bOn);
	void RequestLogout();
	void RequestRestart();
	void SetMenuCursor();
	int  m_iWindowSizeIdx;		// chosen windowed size (index into CRenderer::kWindowSizes), -1 = automatic
	class CSettingsMenu * m_pSettingsMenu;

	int  iGetManaCost(int iMagicNo);
	void UseMagic(int iMagicNo);
	BOOL _bCheckMoveable( short sx, short sy );
	BOOL FindGuildName(char* pName, int* ipIndex);
	void bItemDrop_SkillDialog();
	void bItemDrop_IconPanel();
	void bItemDrop_Character();
	void bItemDrop_Inventory();
	void bItemDrop_ItemUpgrade();
	void bItemDrop_SellList();
	void bItemDrop_ExchangeDialog();
	void bItemDrop_Bank();
	void bItemDrop_ExternalScreen(char cItemID, short msX, short msY);
	void CreateScreenShot();
	void CrusadeWarResult(int iWinnerSide);
	void CrusadeContributionResult(int iWarContribution);
	void CannotConstruct(int iCode);
	void DrawTopMsg();
	void SetTopMsg(const char * pString, unsigned char iLastSec);
	void DrawAstoriaStats();
	void DrawObjectFOE(int ix, int iy, int iFrame);
	// Circle indicators (Graphics > World > Circle Indicators): an animated ring drawn in code
	// on the ground under enemies, monsters and/or party members.
	enum { CIRCLE_ENEMIES = 1, CIRCLE_MONSTERS = 2, CIRCLE_PARTY = 4, CIRCLE_ALL = 7 };
	int   m_iCircleIndicators;	// CIRCLE_* bits
	int   _GetCircleKind(int iStatus, short sOwnerType, const char * pName, WORD wObjectID, char cAction) const;	// CIRCLE_* or 0
	void  DrawCircleIndicator(int cx, int cy, int iKind);

	// Chat panel (F9, UI\ChatPanel.cpp): a see-through window with tabs, a coloured channel
	// tag on every line, the message being typed, and speech bubbles over characters.
	enum { CHAT_SYSTEM = 99 };		// event messages, shown in the System tab
	enum { CHATTAB_ALL = 0, CHATTAB_LOCAL, CHATTAB_GUILD, CHATTAB_PARTY, CHATTAB_WHISPER, CHATTAB_SYSTEM, CHATTAB_COUNT };
	enum { CHATPANEL_W = 400, CHATPANEL_H = 174, CHATPANEL_ROWS = 8, CHATPANEL_ROWH = 15, CHATPANEL_DEFX = 6, CHATPANEL_MAXLINES = 500 };
	// An item linked in a chat line: sent as {Name|attribute|sockets} (hex, the last two left out
	// when zero) and shown as [Item name] in the item's quality colour, with its tooltip on hover.
	struct ChatLink
	{
		std::string name;			// the item's internal name
		DWORD attr;
		uint8 sockets[MAXITEMSOCKETS];
		COLORREF color;
		std::vector<std::string> tip;	// tooltip lines (first = display name)
		int sprite = -1, frame = 0, itemColor = 0;	// its picture, when the link carries one
		bool bSpan = false;			// not an item: a coloured run of text in a system line (ChatSpan)
	};
	enum { CHATLINK_MAX = 8 };
	struct ChatLine
	{
		int  ch;				// ChatType or CHAT_SYSTEM
		std::string name;		// who said it (for whispers: the other person)
		std::string text;		// with links turned into marked-up display text (see ChatRich)
		std::vector<ChatLink> links;
		bool bMine;				// said by this player
		bool bTo;				// a whisper this player sent (name = who it went to)
		char cTime[6];			// "hh:mm"
	};
	struct ChatRow { short y0, y1; std::string name; };	// on-screen rows, for Ctrl+T on a line
	std::deque<ChatLine> m_chatLines;
	std::vector<ChatRow> m_chatRows;
	int   m_iChatTab;
	int   m_iChatUnread[CHATTAB_COUNT];
	short m_chatTabX0[CHATTAB_COUNT], m_chatTabX1[CHATTAB_COUNT];	// tab positions, relative to the panel
	short m_chatNewestX0, m_chatNewestX1, m_chatNewestY0, m_chatNewestY1;	// "Newest" button (0 width = hidden)
	DWORD m_dwChatLastLine;
	bool  m_bChatEcho;			// AddEventList is echoing a chat line (not a system message)
	bool  m_bWhisperModeOn;		// typed lines go to m_cWhisperName
	void  AddChatLine(int ch, const char * pName, const char * pText, bool bTo = false);
	bool  ChatLineInTab(const ChatLine & line, int iTab) const;
	bool  IsChatPanelInput() const;
	void  CycleChatChannel(bool bBack);
	const char * ChatRowNameAt(short msX, short msY) const;
	short ChatPanelDefaultY(int iGameH) const { return (short)(iGameH - 53 - CHATPANEL_H - 6); }
	void  DrawChatBubble(short sX, short sY, int iChatIndex, int iLoc);
	// Item tooltips (UI\ItemTooltip.cpp): built from an item (or from a name + attribute, for chat
	// links and trades), drawn as a see-through panel next to the mouse.
	struct ItemTip
	{
		struct Bonus { std::string text, source; COLORREF color; };
		struct Gem { std::string text; COLORREF dot; };
		std::string name;				// prefix + display name, without the +N
		int upgrade = 0;				// +N
		std::string type;				// "Two-handed weapon · Crafted"
		COLORREF color = RGB(255, 255, 255);	// quality colour (name and border)
		std::vector<Bonus> bonus;
		std::vector<Gem> gems;
		int emptySockets = 0;
		std::vector<std::string> info;	// other lines (what a gem does, uses left, ...)
		int durCur = 0, durMax = 0;		// endurance (durMax 0 = none shown)
		int reqLevel = 0, reqStr = 0;
		char reqGender = 0;				// 1 male only, 2 female only
		int weight = -1;				// in 1/100 units, -1 = unknown
		int value = -1;					// base value in gold, -1 = unknown
		DWORD count = 1, total = 0;		// stack size, and how many the player has in all (0 = not shown)
		int sprite = -1, frame = 0;		// icon (SPRID_ITEMPACK_PIVOTPOINT + sprite)
		char itemColor = 0;
		bool weaponTint = false;
		bool showIcon = false;			// draw the item's picture beside the name (chat links)
		std::string header;				// a small strip above the name (chat links: "ITEM LINK - from ...")
	};
	void  BuildItemTip(ItemTip & t, const CItem * pItem, const char * pName = NULL, DWORD attr = 0,
		const uint8 * pSockets = NULL, DWORD count = 1);
	void  DrawItemTip(const ItemTip & t, int mx, int my, bool bAbove = false);
	void  DrawItemTipFor(const CItem * pItem, int mx, int my);		// build + draw
	int   InventoryItemAt(short msX, short msY);		// the bag item under the mouse, or -1

	// item links in chat (UI\ChatPanel.cpp)
	static bool ParseChatLink(const char * p, size_t * pLen, std::string * pName, DWORD * pAttr, uint8 * pSockets,
		int * pSprite = NULL, int * pFrame = NULL, int * pColor = NULL);
	std::string ChatRich(const char * pRaw, std::vector<ChatLink> & links, bool bColor = false);	// links -> marked display text
	static std::string ChatSpan(COLORREF c, const std::string & s);	// coloured text for a system line
	void  AddColoredEvent(const std::string & sMarked);	// a system line with ChatSpan colours
	std::string ChatPlainText(const char * pRaw);		// links -> "[Item name]" (for the old message list)
	bool  TryLinkItemToChat(int iItem);					// Shift+click on an item: put a link in the chat line
	void  FilterChatCurses(char * pBuf, size_t iBufSize);	// the curse filter, leaving item links alone
	bool  ChatBackspaceLink();							// Backspace right after a link removes all of it

	// Level-up settings window, drawn in code (UI\LevelUpPanel.cpp)
	enum { LEVELUPWIN_W = 380, LEVELUPWIN_H = 436 };
	void  LevelUpRects(RECT * pClose, RECT * pApply, RECT * pMajestic, RECT * pPlus, RECT * pMinus);
	int   LevelUpButtonAt(short msX, short msY);	// same codes as the old buttons; 20 = close

	// Character window (F5), drawn in code (UI\CharacterPanel.cpp)
	enum { CHARWIN_W = 540, CHARWIN_H = 486 };
	// inventory window drawn in code (UI\InventoryPanel.cpp)
	enum { INVWIN_W = 226, INVWIN_H = 210 };
	// magic list drawn in code (UI\MagicPanel.cpp)
	enum { MAGICWIN_W = 258, MAGICWIN_H = 342 };
	void MagicButtonRect(int id, RECT * pRc);
	void DrawMagicPanel(int iChance, int onButton);
	// skills list drawn in code (UI\SkillPanel.cpp)
	enum { SKILLWIN_W = 258, SKILLWIN_H = 445 };	// 339 + the life skills (UI\SkillPanel.cpp)
	class CSkill * ShownSkill(int i) const;	// a Helbreath skill the Skills window lists, or NULL
	void SkillCloseRect(RECT * pRc);
	void DrawSkillPanel(int onButton);
	void InvButtonRect(int id, RECT * pRc);
	void DrawInvPanelBack();
	void DrawInvPanelFront();
	void DrawInvItemBadges(int id, int iX, int iY);
	// Most Wanted: enemy-kill streaks (UI\WantedSystem.cpp)
	struct WantedEntry { char name[12]; char map[12]; int level, side, streak; uint32 worth; };
	struct WantedPos { char name[12]; char map[12]; int x, y, level; DWORD dwTime; };
	struct WantedTag { int x, y, level, status; bool me; char name[12]; };
	std::vector<WantedEntry> m_wanted;
	std::vector<WantedPos> m_wantedPos;
	std::vector<WantedTag> m_wantedTags;
	int m_iWantedStreak = 0, m_iWantedBest = 0, m_iWantedEnded = 0;
	enum { WANTEDWIN_W = 460, WANTEDWIN_H = 380 };
	static const char * WantedLevelName(int iLevel);
	int  WantedLevelOf(const char * pName);
	void DrawWantedStars(int cx, int cy, int n, float fR, int iTotal = 0);
	void NotifyMsg_WantedInfo(char * pData);
	void NotifyMsg_WantedStats(char * pData);
	void NotifyMsg_WantedPos(char * pData);
	void NotifyMsg_WantedEnded(char * pData);
	void WantedNoteObject(int ix, int iy, const char * pName, short sOwnerType, int iStatus);
	void DrawWantedTags();
	void DrawWantedSkull(int cx, int cy, bool bLegend);
	bool DrawWantedNameLine(short sX, short sY, const char * pName);
	void DrawWantedCounter();
	void DrawWantedOnMinimap(short sX, short sY, short shX, short shY, bool bZoom);
	void DrawDialogBox_MostWanted();
	void DlgBoxClick_MostWanted();

	// Bounties (UI\BountyBoard.cpp)
	struct BountyEntry { char name[12]; char map[12]; uint32 total, mine; int side, pk, level, online; DWORD dwExpireAt; };
	struct BountyBacker { char name[12]; uint32 gold; DWORD dwExpireAt; };
	std::vector<BountyEntry> m_bounties;
	std::vector<BountyBacker> m_bountyBackers;	// who backs m_cBountySel
	char m_cBountySel[12] = {}, m_cBountyAmount[16] = {}, m_cBountyName[16] = {};
	int  m_iBountyTab = 0;
	uint32 m_dwBountyClaims = 0, m_dwBountyClaimGold = 0;
	enum { BOUNTYWIN_W = 540, BOUNTYWIN_H = 390 };
	enum { BNT_CLOSE = 1, BNT_AMOUNT, BNT_ADD, BNT_NAME, BNT_PLACE, BNT_TAB0 = 10, BNT_ROW0 = 20 };
	void BountyRect(int id, RECT * pRc);
	void BountyList(std::vector<int> & out);
	const BountyEntry * BountyFor(const char * pName);
	void NotifyMsg_BountyInfo(char * pData);
	void NotifyMsg_BountyStats(char * pData);
	void NotifyMsg_BountyDetail(char * pData);
	void NotifyMsg_BountyEvent(char * pData);
	bool DrawBountyNameLine(short sX, short sY, const char * pName, int iStatus);
	void OpenBountyBoard();
	void DrawDialogBox_Bounty();
	void DlgBoxClick_Bounty();
	void BountySubmit(bool bAddToSelected);
	bool BountyOnEnter();

	// Rebirth (UI\RebirthPanel.cpp)
	struct RebirthMark { char name[12]; int level; };
	std::vector<RebirthMark> m_rebirthOf;
	int  m_iRebirth = 0, m_iRebirthMode = 0, m_iRebirthAltLevel = 0;
	DWORD m_dwRebirthConfirmAt = 0;
	enum { REBIRTHWIN_W = 440, REBIRTHWIN_H = 392 };
	int  RebirthStatBonus() const;
	int  RebirthOf(const char * pName) const;
	void NotifyMsg_RebirthStats(char * pData);
	// clock and ping panel, top-left (UI\ClockPing.cpp)
	DWORD m_dwPingLastSent = 0, m_dwPingLastAnswer = 0, m_dwServerSecAt = 0;
	int   m_iPingMs = -1, m_iPingAvg = -1, m_iServerSecOfDay = -1;
	// the strip across the top (UI\ClockPing.cpp): each part can be switched off; it shows if any is on
	bool  m_bStripFps = true, m_bStripPing = true, m_bStripClock = true;
	bool  m_bClock24h = true;		// military (15:07) or standard (3:07 PM) time
	// night lighting (UI\NightLights.cpp): on / off, and how dark the moonlight is (0..100)
	bool  m_bNightLights = true;
	int   m_iNightDarkness = 62;
	bool  NightLightsOn();
	void  NightLightsBeginFrame();
	void  NightLightsPass();
	void  NightLightsLamp(int ix, int iy, const POINT * pHeads, int nHeads);
	void  NightLightsEffects();
	void  NightLightsWindows(int sSprite, int sFrame, int x, int y);
	void  ClockPingTick(DWORD dwTime);
	void  NotifyMsg_Pong(char * pData);
	void  ClockPingRect(RECT * pRc);
	void  DrawClockPing();
	void NotifyMsg_RebirthInfo(char * pData);
	void NotifyMsg_RebirthEvent(char * pData);
	bool DrawRebirthNameLine(short sX, short sY, const char * pName);
	void RebirthButtonRect(int id, RECT * pRc);
	void DrawDialogBox_Rebirth();
	void DlgBoxClick_Rebirth();

	// VALORIA enchanting (UI\EnchantPanel.cpp)
	enum { ENCHANTWIN_W = 480, ENCHANTWIN_H = 440 };
	Valoria::CharData m_enchant;
	int  m_iEnchantTab = 0, m_iEnchantSel = -1, m_iEnchantScroll = 0;
	DWORD m_dwEnchantConfirmAt = 0;
	void NotifyMsg_EnchantStats(char * pData);
	void NotifyMsg_EnchantItems(char * pData);
	void OpenEnchantWindow();
	std::string ItemDisplayName(const char * pName) const;
	void EnchantItemMods(const class CItem * it, bool has[Valoria::MOD_COUNT]) const;
	void EnchantTipLines(ItemTip & t, const class CItem * it);
	void DrawDialogBox_Enchant();
	void DlgBoxClick_Enchant();
	bool GearRowOf(const char * pName, int base[Valoria::MOD_CRIT + 1]) const;	// UI\ValoriaGear.cpp

	// VALORIA passive tree (UI\PassivePanel.cpp)
	enum { PASSIVEWIN_W = 620, PASSIVEWIN_H = 450 };
	ValoriaPassive::Data m_passive;
	int  m_iPassiveZoom = 2, m_iPassivePanX = 0, m_iPassivePanY = 0, m_iPassivePick = -1;
	DWORD m_dwPassiveOrbAt = 0, m_dwPassivePickAt = 0;
	bool m_bPassiveTotals = false;
	void NotifyMsg_Passives(char * pData);
	int  PassiveTotal(int iMod) const;
	void OpenPassiveWindow();
	void DrawDialogBox_Passives();
	void DlgBoxClick_Passives();

	// VALORIA achievements (UI\AchievePanel.cpp)
	enum { ACHWIN_W = 600, ACHWIN_H = 440 };
	struct TitleMark { char name[12]; int counter, tier; };
	std::vector<TitleMark> m_titleOf;
	int  m_iAchCat = 0, m_iAchScroll = 0;
	bool m_bAchSeen = false;			// the first counters after entering the world aren't announced
	unsigned int AchProgress(int iCounter) const;
	int  AchUnlockedCount() const;
	const char * TitleOf(const char * pName) const;
	void AchAnnounce(int iCounter, unsigned int before, unsigned int after);
	void NotifyMsg_TitleInfo(char * pData);
	void OpenAchieveWindow();
	void DrawDialogBox_Achievements();
	void DlgBoxClick_Achievements();

	// VALORIA side quests (UI\SideQuestPanel.cpp)
	enum { SIDEWIN_W = 640, SIDEWIN_H = 420 };
	int  m_iSideGiver = 0, m_iSideSel = -1;		// giver: 67 / 68 / 69, or 0 for the journal
	DWORD m_dwSideAbandonAt = 0;
	int  SideStatus(int q) const;
	std::vector<int> SideList() const;
	int  SideActions(int out[2]) const;
	void OpenSideQuests(int iGiver);
	void NotifyMsg_SideEvent(char * pData);
	void DrawDialogBox_SideQuests();
	void DlgBoxClick_SideQuests();

	// VALORIA usable items: elixir / scroll buffs, rune and stone tooltips (UI\ValoriaUse.cpp)
	DWORD m_dwValBuffEnd[ValoriaUse::BUFF_COUNT] = {};
	void NotifyMsg_ValoriaBuffs(char * pData);
	int  ValoriaBuffTotal(int iKind) const;
	void DrawValoriaBuffs();
	void ValoriaUseTipLines(ItemTip & t, const CItem * it);
	enum { CHARBTN_NONE = 0, CHARBTN_CLOSE, CHARBTN_QUEST, CHARBTN_PARTY, CHARBTN_GUILD, CHARBTN_LEVELUP, CHARBTN_WANTED, CHARBTN_ACHIEVE, CHARBTN_JOURNAL };
	void  CharDollOrigin(short & sX, short & sY);	// where the paper doll's pivot math starts
	void  DrawCharPanelBack();
	void  DrawCharPanelFront(int iHoverSlot, const char * cEquipPoiStatus);
	int   CharPanelButtonAt(short msX, short msY);
	int   CharSlotAt(short msX, short msY);	// the equipment position of the slot box under the mouse, or -1
	const char * CombatantText() const;
	const char * GuildRankText(int iRank) const;
	void  DrawExpStrip();

	// Level-up celebration over a player, seen by everyone nearby (UI\LevelUpFx.cpp)
	enum { LVLFX_NORMAL = 0, LVLFX_MILESTONE, LVLFX_MAX, LVLFX_MAJESTIC };
	struct LevelUpFx
	{
		WORD  wObjectID;
		short sLevel;
		char  cKind;		// LVLFX_*
		DWORD dwStart;
		int   iX, iY;		// the player's feet on the game canvas (this frame)
		int   iBackFrame;	// frame in which the ground part was drawn
		bool  bVisible;
	};
	std::vector<LevelUpFx> m_levelUpFx;
	CLevelUpBadges m_levelUpBadges;
	int   m_iLevelUpFxFrame;
	void  StartLevelUpFx(WORD wObjectID, short sLevel, char cServerKind);
	void  DrawLevelUpFxBack(int sX, int sY);
	void  DrawLevelUpFxFront();
	void GrandMagicResult(char * pMapName, int iV1, int iV2, int iV3, int iV4, int iHP1, int iHP2, int iHP3, int iHP4) ;
	void MeteorStrikeComing(int iCode);
	void _Draw_OnLogin(char * pAccount, char * pPassword, int msX, int msY, int iFrame = 60000);
	bool DrawStoneClassic(char cType, int sX, int sY, int iFrame);
	bool IsClassicDialog(int id) const;	// VALORIA's stone look for the classic windows
	void DrawNewDialogBox(char cType, int sX, int sY, int iFrame, BOOL bIsNoColorKey = FALSE, BOOL bIsTrans = FALSE);
	void AddMapStatusInfo(char * pData, BOOL bIsLastData);
	void _RequestMapStatus(char * pMapName, int iMode);
	int  GetCharKind(char *str, int index);
	void ReceiveString(char * pString);
	void EndInputString();
	void ClearInputString();
	void ShowReceivedString(BOOL bIsHide = FALSE);
	bool GetText(HWND hWnd,UINT msg,WPARAM wparam, LPARAM lparam);
	
	BOOL bReadItemNameConfigFile();
	void DrawDialogBoxes();
	void DrawDialogBox_Character();//1
	void DrawDialogBox_Inventory();//2
	void DrawDialogBox_Magic();//3
	void DrawDialogBox_GuildMenu();//7
	void DrawDialogBox_GuildOperation();//8
	void DrawDialogBox_GuideMap();//9
	void DrawDialogBox_Chat();//10
	void DrawDialogBox_Shop();//11
	void DrawDialogBox_LevelUpSetting();//12
	void DrawDialogBox_CityHallMenu();//13
	void DrawDialogBox_Bank();//14
	void DrawDialogBox_Skill();//15
	void DrawDialogBox_MagicShop();//16
	void DrawDialogBox_QueryDropItemAmount();//17
	void DrawDialogBox_Text();//18
	void DrawDialogBox_SysMenu();//19
	void DrawDialogBox_NpcActionQuery();//20
	const char * NpcQueryLabel(int id);	// UI\NpcQueryPanel.cpp
	void DrawDialogBox_NpcTalk();//21
	void DrawDialogBox_Map();//22
	void DrawDialogBox_SellorRepairItem();//23
	void DrawDialogBox_Fishing();//24
	void DrawDialogBox_ShutDownMsg();//25
	void DrawDialogBox_SkillDlg();//26
	void DrawDialogBox_Exchange();//27
	void DrawDialogBox_Quest();//28
	void DrawDialogBox_GaugePanel();//29
	void DrawDialogBox_IconPanel();//30
	bool IsLowHp() const;			// under 25% HP and alive (UI\BottomBar.cpp)
	// VALORIA gear bonuses (UI\ValoriaGear.cpp); same order as contents\valoria_gear.txt
	enum { GEAR_STR, GEAR_DEX, GEAR_VIT, GEAR_INT, GEAR_MAG, GEAR_HP, GEAR_MP, GEAR_DEF, GEAR_CRIT, GEAR_COUNT };
	int GearBonus(int iKind) const;		// sum over the equipped items
	int GearStatBonus(int iStat) const;	// for a STAT_ index
	int MaxHP() const;
	int MaxMP() const;
	int MaxSP() const;
	void DrawLowHpGlow();			// red glow round the screen edges while IsLowHp
	void DrawDialogBox_SellList();//31
	void DrawDialogBox_Party();//32
	void DrawDialogBox_CrusadeJob();//33
	void DrawDialogBox_ItemUpgrade();//34
	void DrawDialogBox_Help();//35
	void DrawDialogBox_Commander();//36
	void DrawDialogBox_Constructor();//37
	void DrawDialogBox_Soldier();//38
   void DrawDialogBox_ItemDrop();//4
	void DrawDialogBox_WarningMsg();//6
	void DrawDialogBox_15AgeMsg();//5
	void DrawDialogBox_FeedBackCard();//40
	void DrawDialogBox_Guild();//57
	void DlgBoxClick_Guild(); //57
   void DrawDialogBox_GuildContribute();//58
   void DlgBoxClick_GuildContribute();//58
	void DrawDialogBox_ExtendedSysMenu();//59
	void DlgBoxClick_ExtendedSysMenu();//59
	void DrawDialogBox_MuteList();//60
	void DlgBoxClick_MuteList();//60
	void DrawDialogBox_Mailbox();

	void DisplayCommaNumber_G_cTxt(int iGold);
	
	void DrawDialogBox_ConfirmExchange();//41
	void DlgBoxClick_ConfirmExchange();
	void DrawDialogBox_YesNo();
	void DlgBoxClick_YesNo();

	void DrawDialogBox_DKMenuWeapons();// 53
	void DlgBoxClick_DKMenuWeapons();

	// Slates - Alastor
	void bItemDrop_Slates();
	void DlgBoxClick_Slates();
	void DrawDialogBox_Slates();//40

	void DlgBoxClick_WarningMsg();
	void DlgBoxClick_15AgeMsg();
	void DlgBoxClick_ItemDrop();
	void DlgBoxClick_Character();
	void DlgBoxClick_Inventory();
	void DlgBoxClick_Chat();
	void DlgBoxDoubleClick_Chat();

	void EnableChat(ChatType t, bool enable);
	void DlgBoxClick_Magic();
	void DlgBoxClick_GuildMenu();
	void DlgBoxClick_GuildOp();
	void DlgBoxClick_GuideMap();
	void DlgBoxClick_Shop();
	void DlgBoxClick_LevelUpSettings();
	void DlgBoxClick_CityhallMenu();
	void DlgBoxClick_Bank();
	void DlgBoxClick_Skill();
	void DlgBoxClick_MagicShop();
	void DlgBoxClick_FeedBackCard();
	void DlgBoxClick_Text();
	void DlgBoxClick_SysMenu();
	void DlgBoxClick_NpcActionQuery();
	void DlgBoxClick_NpcTalk();
	void DlgBoxClick_ItemSellorRepair();
	void DlgBoxClick_Fish();
	void DlgBoxClick_ShutDownMsg();
	void DlgBoxClick_SkillDlg();
	void DlgBoxClick_Exchange();

	void DlgBoxClick_Quest();
	void DlgBoxClick_SellList();
	void DlgBoxClick_IconPanel();
	void DlgBoxClick_Party();
	void DlgBoxClick_CrusadeJob();
	void DlgBoxClick_ItemUpgrade();
	void DlgBoxClick_Help();
	void DlgBoxClick_Commander();
	void DlgBoxClick_Constructor();
	void DlgBoxClick_Soldier();
	void DlgBoxClick_Mailbox();
	void ItemDrop_Mailbox();
	
	void ReceiveGuildBoard(char * data);
	void ReceiveGuildBoardPost(char * data);

	void ReceiveMailbox(char * data);
	void ReceiveMailData(char * data);

	void NotifyMsgHandler(char * pData);
	void NotifyMsg_GlobalAttackMode(char * pData);
	void NotifyMsg_QuestReward(char * pData);
	void NotifyMsg_QuestContents(char * pData);
	void NotifyMsg_ItemColorChange(char * pData);
	void NotifyMsg_ItemSocketChange(char * pData);
	void NotifyMsg_DropItemFin_CountChanged(char * pData);
	void NotifyMsg_CannotGiveItem(char * pData);
	void NotifyMsg_GiveItemFin_CountChanged(char * pData);
	void NotifyMsg_SetExchangeItem(char * pData);
	void NotifyMsg_OpenExchageWindow(char * pData);
	void NotifyMsg_DownSkillIndexSet(char * pData);
	void NotifyMsg_AdminInfo(char * pData);
	void NotifyMsg_WeatherChange(char * pData);
	void NotifyMsg_FishChance(char * pData);
	void NotifyMsg_EventFishMode(char * pData);
	void NotifyMsg_NoticeMsg(char * pData);
	void NotifyMsg_RatingPlayer(char * pData);
	void NotifyMsg_CannotRating(char * pData);
	void NotifyMsg_PlayerShutUp(char * pData);
	void NotifyMsg_TimeChange(char * pData);
	void NotifyMsg_Hunger(char * pData);
	void NotifyMsg_PlayerProfile(char * pData);
	void NotifyMsg_WhisperMode(BOOL bActive, char * pData);
	void NotifyMsg_UnitStatus(BOOL bOnGame, char * pData);
	void NotifyMsg_Charisma(char * pData);
	void NotifyMsg_ItemRepaired(char * pData);
	void NotifyMsg_RepairItemPrice(char * pData);
	void NotifyMsg_CannotRepairItem(char * pData);
	void NotifyMsg_CannotSellItem(char * pData);
	void NotifyMsg_SellItemPrice(char * pData);
	void NotifyMsg_ShowMap(char * pData);
	void NotifyMsg_SkillUsingEnd(char * pData);
	void NotifyMsg_TotalUsers(char * pData);
	void NotifyMsg_EventStart(char * pData);
	void NotifyMsg_EventStarting(char * pData);
	void NotifyMsg_Casualties(char * pData);	
	void NotifyMsg_ShieldHP(char * pData);	
	void NotifyMsg_RelicInAltar(char * pData);
	void NotifyMsg_RelicGrabbed(char * pData);
	void NotifyMsg_EventWinner(char * pData);
	void NotifyMsg_MagicEffectOff(char * pData);
	void NotifyMsg_MagicEffectOn(char * pData);
	void NotifyMsg_SetItemCount(char * pData);
	void NotifyMsg_ItemDepleted_EraseItem(char * pData);
	void NotifyMsg_ServerChange(char * pData);
	void NotifyMsg_Skill(char * pData);
	void NotifyMsg_DropItemFin_EraseItem(char * pData);
	void NotifyMsg_GiveItemFin_EraseItem(char * pData);
	void NotifyMsg_EnemyKillReward(char * pData);
	void NotifyMsg_PKcaptured(char * pData);
	void NotifyMsg_PKpenalty(char * pData);
	void NotifyMsg_ItemToBank(char * pData);
	void NotifyMsg_ItemLifeSpanEnd(char * pData);
	void NotifyMsg_ItemReleased(char * pData);
	void NotifyMsg_LevelUp(char * pData);
	void NotifyMsg_SettingSuccess(char * pData); // CLEROTH - LU
	void NotifyMsg_MP(char * pData);
	void NotifyMsg_SP(char * pData);
	void NotifyMsg_SkillTrainSuccess(char * pData);
	void NotifyMsg_MagicStudyFail(char * pData);
	void NotifyMsg_MagicStudySuccess(char * pData);
	void NotifyMsg_DismissGuildsMan(char * pData);
	void NotifyMsg_NewGuildsMan(char * pData);
	void NotifyMsg_CannotJoinMoreGuildsMan(char * pData);
	void NotifyMsg_GuildDisbanded(char * pData);
	void NotifyMsg_Exp(char * pData);
	void NotifyMsg_Killed(char * pData);
	void NotifyMsg_HP(char * pData);
	void NotifyMsg_ItemPurchased(char * pData);
	void NotifyMsg_DismissGuildReject(char * pData);
	void NotifyMsg_DismissGuildApprove(char * pData);
	void NotifyMsg_JoinGuildReject(char * pData);
	void NotifyMsg_JoinGuildApprove(char * pData);
	void NotifyMsg_QueryDismissGuildPermission(char * pData);
	void NotifyMsg_QueryJoinGuildPermission(char * pData);
	void NotifyMsg_ItemObtained(char * pData);
	void NotifyMsg_ForceDisconn(char * pData);
	void NotifyMsg_FriendOnGame(char * pData);
	void NotifyMsg_RepairAllPrice(char * pData);

	void ResponsePanningHandler(char * pData);
	void _CalcSocketClosed();
	void UpdateScreen_OnSelectServer();
	void StartInputString(int left, int top, uint32 len, char * pBuffer, BOOL bIsHide = FALSE, int right = 0);
	void _SetIlusionEffect(int iOwnerH);
	int _iGetFOE(int iStatus);
	void NoticementHandler(char * pData);
	void GetItemName(char * cItemName, DWORD dwAttribute, char *pStr1, char *pStr2, char *pStr3);
	void GetItemName(CItem * pItem, char * pStr1, char * pStr2, char * pStr3);
	Vector<string> * GetItemName(char * cItemName, DWORD attr, uint8 sockets[MAXITEMSOCKETS], uint32 count);
	Vector<string> * GetItemName(CItem * pItem);
	void _InitOnCreateNewCharacter();
	BOOL _bCheckCurrentBuildItemStatus();
	BOOL _bCheckBuildItemStatus();
	BOOL _bDecodeBuildItemContents();
	void GetNpcName(short sType, char * pName);


#ifdef MAKE_ACCOUNT
	int m_iAgreeView;
	void _LoadAgreementTextContents(char cType);
	void UpdateScreen_OnAgreement();
	void UpdateScreen_OnCreateNewAccount();
#endif

	void UpdateFriendsStatus();
	void UseShortCut( int num );
	void UpdateScreen();
	void UpdateScreen_OnMainMenu();
	void UpdateScreen_OnGame();
	void UpdateScreen_OnConnecting();
	void UpdateScreen_OnWaitInitData();
	void MakeSprite( char* FileName, short sStart, short sCount, bool bAlphaEffect = TRUE);
	void MakeLegionSprite( char* FileName, short sStart, short sCount, bool bAlphaEffect = TRUE);
	void MakeTileSpr( char* FileName, short sStart, short sCount, bool bAlphaEffect = TRUE);
	void MakeLegionTileSpr( char* FileName, short sStart, short sCount, bool bAlphaEffect = TRUE);
	void MakeEffectSpr( char* FileName, short sStart, short sCount, bool bAlphaEffect = TRUE);
	void MakeLegionEffectSpr( char* FileName, short sStart, short sCount, bool bAlphaEffect = TRUE);
	void UpdateScreen_OnLoading(bool bActive);
	void UpdateScreen_OnConnectionLost();
	void UpdateScreen_OnLogin();
	void UpdateScreen_OnMsg();
	void UpdateScreen_OnQuit();
	void UpdateScreen_OnQueryForceLogin();
	void UpdateScreen_OnQueryDeleteCharacter();
	void UpdateScreen_OnWaitingResponse();
	void UpdateScreen_OnCreateNewCharacter();
	void UpdateScreen_OnSelectCharacter();
	void UpdateScreen_OnLogResMsg();
	void UpdateScreen_OnSelectCharacter(short sX, short sY, short msX, short msY, BOOL bIgnoreFocus = FALSE);
	void UpdateScreen_OnChangePassword();
	void UpdateScreen_OnLoading_Progress();
	// loading / sign-in screens drawn in code (UI\LoginScreen.cpp)
	void DrawLoginBackdrop(bool bNews);
	void LoginRects(RECT * pPanel, RECT * pAcc, RECT * pPw, RECT * pRemember, RECT * pConnect, RECT * pCreate, RECT * pQuit);
	void CreateAccountRects(RECT * pPanel, RECT * pField, RECT * pCreate, RECT * pClear, RECT * pBack);
	void _Draw_OnCreateAccount(const char * const * pField, const bool * bOk, const bool * bGood, const char * const * pHint,
		bool bHintError, bool bReady, int msX, int msY, int iFrame);
	// the welcome panel for a new player (UI\LoginScreen.cpp): Create account, I have an account, Quit
	void WelcomeRects(RECT * pPanel, RECT * pCreate, RECT * pHave, RECT * pQuit);
	void _Draw_OnWelcome(int iFocus, int msX, int msY);
	// the live town behind the sign-in screens, the first-launch intro and the logo (UI\LoginScene.cpp)
	bool LoginSceneReady();
	bool OnLoginSceneScreen() const;		// UI\GameView.cpp: the canvas follows the screen's shape
	void DrawLoginScene();					// the town, lit for the time of day, with its name
	bool DrawLoginIntro(DWORD dwElapsed);	// false once it has finished
	void DrawLoginLogo();
	// the loading screen at startup and its hand-over to the sign-in screen (UI\LoginScene.cpp)
	void DrawLoadingScreen(int iPct);		// the night sky, the logo, the rune circle and what's loading
	void DrawLogoAt(float k, float fAlpha);	// k 0: big in the middle (loading), 1: in the top-left corner
	float LoadHandover();					// 0..1 over the 1.3 s after loading finished; 1 otherwise
	void DrawLoadHandover(bool bIntro);		// the sky, rune circle and logo fading off the town
	bool LoginIsNight();					// as the server decides it (minutes 40..59 of each hour)
	static bool LoginFlag(const char * pName);
	static void SetLoginFlag(const char * pName);
	void LoginPanelAnim(int * pDX, int * pDY, int * pAlpha);	// the panel sliding / fading in
	void DrawPanelEdgeLight(const RECT & rc);	// a light running once round a panel's edge
	int   m_iLoginPanel = -1;		// sign-in screen shows: 0 the welcome panel, 1 sign in (-1: not decided)
	bool  m_bLoginIntro = false;	// the intro is playing
	DWORD m_dwLoginIntroT0 = 0;
	DWORD m_dwPanelT0 = 0;			// when the panel being shown appeared
	bool  m_bPanelSwap = false;		// it slides in from the side (another panel before it) rather than rising
	int   m_iPanelDX = 0, m_iPanelDY = 0;	// added to the panel rectangles while one is drawn
	bool  m_bLoginNight = false;	// the live town is being drawn at night (UI\NightLights.cpp)
	DWORD m_dwLoadT0 = 0, m_dwLoadDoneT = 0;	// when loading started and finished
	// message panel for the screens before the game: connecting, errors, yes/no questions
	enum { MSGICON_NONE, MSGICON_WAIT, MSGICON_WARN, MSGICON_OK, MSGICON_QUESTION };
	struct MsgPanel
	{
		std::string title;
		std::vector<std::string> lines;		// word-wrapped to the panel
		int iIcon = MSGICON_NONE;
		std::string btn[2];					// btn[0] is the main (gold) button; empty = none
		bool bDanger = false;				// main button in red
		bool bTitleBold = true;				// gold heading, or a plain white one (status text)
	};
	void MessagePanelRects(const MsgPanel & m, RECT * pPanel, RECT * pBtn);
	void DrawMessagePanel(const MsgPanel & m, int msX, int msY, bool bDim = true);
	void DrawQuitScreen(int iMsElapsed, int iMsTotal);
	void DrawPreGameBackdrop();
	void BuildLogResMessage(MsgPanel & mp);
	// character select and creation over the live town (UI\CharScreens.cpp)
	struct CreateLayout { RECT look, gender[2], skin[3], hairPrev, hairNext, hairCol[16], underCol[8], name, stats,
		preset[3], minus[6], plus[6], statRow[6], back, create; };
	enum { CREATECLICK_NONE = -1, CREATECLICK_NAME = 0, CREATECLICK_MALE, CREATECLICK_FEMALE, CREATECLICK_SKIN,
		CREATECLICK_HAIRPREV = CREATECLICK_SKIN + 3, CREATECLICK_HAIRNEXT, CREATECLICK_HAIRCOL,
		CREATECLICK_UNDERCOL = CREATECLICK_HAIRCOL + 16, CREATECLICK_PRESET = CREATECLICK_UNDERCOL + 8,
		CREATECLICK_MINUS = CREATECLICK_PRESET + 3, CREATECLICK_PLUS = CREATECLICK_MINUS + 6,
		CREATECLICK_BACK = CREATECLICK_PLUS + 6, CREATECLICK_CREATE, CREATECLICK_COUNT };
	void DrawCharScreenBackdrop();			// the town, darkened a little, and the version
	void CharPedestalSpot(int * pX, int * pY);
	void DrawPedestal(float fGlow);			// the lit pedestal the character stands on
	// character select: everyone round a campfire, the camera gliding in on the one chosen
	void CampReset();						// the screen opens: round the fire
	int  CampSelected();					// the slot the camera is in close on, or -1
	void CampChoose(int i);
	void CampBack();
	void CampRects(RECT * pInfo, RECT * pBack, RECT * pDelete, RECT * pPlay, RECT * pLogout, RECT * pChangePw);
	void DrawCampGround(float fx, float fy);	// the grass in Elvine, lit at night (UI\LoginScene.cpp)
	bool m_bMenuStand = false;				// DrawObject_OnMove_ForMenu: standing, not walking on the spot
	char  m_cPrevGameMode = 0;				// the screen before this one (ChangeGameMode)
	DWORD m_dwSignInT0 = 0;					// Connect pressed: the town dims while signing in
	void DrawSignInDim();
	bool m_bMenuGhost = false;				// ... and (standing) only the body, see-through
	bool CharSlotFilled(int i);
	bool CanDeleteCharacter(int i);			// (Game.cpp)
	void SetMenuCharacter(int iSlot, int iDir = 0);	// the _tmp_ appearance for a slot, (-1) the one being made, (-2) a plain body (Game.cpp)
	bool SelectEnterGame();					// Play: enter with the chosen character, or create one (Game.cpp)
	void _Draw_SelectScreen(int msX, int msY, bool bIgnoreFocus);
	int  SelectScreenClick(int msX, int msY, char cLB);
	void CreateRects(CreateLayout * L);
	bool _Draw_CreateScreen(const char * pName, int msX, int msY, int iPoint);
	int  CreateScreenClick(int msX, int msY, char cLB);
	int  CreateStatIndex(int k);
	void ApplyCreatePreset(int i);
	// entering the world after Play: the night sky and rune circle (UI\LoginScene.cpp)
	void DrawEnteringScreen(int iStage);	// 0 connecting, 1 waiting for the server, 2 waiting for the world
	bool  m_bEnterWorld = false;
	DWORD m_dwEnterT0 = 0;
	float m_fEnterP = 0.f;
	void LoadRememberedAccount(char * pOut, size_t iSize);
	void SaveRememberedAccount(const char * pName);
	bool m_bRememberAccount = false;
	void UpdateScreen_OnVersionNotMatch();
	void NpcTalkHandler(char * pData);
	int  _iGetWeaponSkillType();
	void SetCameraShakingEffect(short sDist, int iMul = 0);
	BOOL DlgBoxPress_SkillDlg();
	BOOL DlgBoxPress_Inventory();
	BOOL DlgBoxPress_Character();
	BOOL DlgBoxPress_CharacterPick();
	void ClearSkillUsingStatus();
	BOOL bCheckItemOperationEnabled(char cItemID);
	void _DrawThunderEffect(int sX, int sY, int dX, int dY, int rX, int rY, char cType);
	void DrawLine2(int x0, int y0, int x1, int y1, int iR, int iG, int iB);
	void DrawLine(int x0, int y0, int x1, int y1, int iR, int iG, int iB);
	void SetWeatherStatus(Weather type);
	void WeatherObjectFrameCounter();
	void DrawWeatherEffects();
	BOOL bCheckLocalChatCommand(char const * const pMsg);
	char GetOfficialMapName(char const * const pMapName, char * pName);
	int iGetLevelExp(int iLevel);
	int _iCalcTotalWeight();
	void DrawVersion(BOOL bAuthor = FALSE);
	BOOL _bIsItemOnHand();
	void DynamicObjectHandler(char * pData);
	BOOL _bCheckItemByType(char cType);
	void _DrawBlackRect(int iSize);
	void DrawNpcName(   short sX, short sY, short sOwnerType, int iStatus);
	// monster names at their feet and their health bars (UI\NpcLabels.cpp)
	struct NpcLabel { short x, y, type; int status; WORD id; };
	struct NpcHealth { float pct = 100.f, ghost = 100.f; DWORD dwHitAt = 0, dwMyHitAt = 0; };
	std::vector<NpcLabel> m_npcLabels;			// this frame's, gathered as the monsters are drawn
	std::map<WORD, NpcHealth> m_npcHealth;		// the health each has left, from the server's hit messages
	std::map<WORD, DWORD> m_npcAimedAt;			// when the player last attacked or cast at each
	WORD m_wFocusNpcID = 0;						// the one under the mouse
	char m_cFocusNpcExtra[64] = {};				// ... and its special trait (Poisonous, Explosive ...)
	void NoteNpcLabel(int sX, int sY, short sType, int iStatus, WORD wID);
	void NoteNpcHealth(WORD wID, int iPct);
	void NoteNpcAimedAt(WORD wID);
	void NotifyMsg_NpcHealth(char * pData);
	void NoteSpellAimedAt(int iX, int iY);
	void DrawNpcLabels();
	void DrawObjectName(short sX, short sY, char * pName, int iStatus);
	void PlaySound(char cType, int iNum, int iDist, long lPan = 0);
	void _RemoveChatMsgListByObjectID(int iObjectID);
	void _LoadTextDlgContents(int cType);
	int  _iLoadTextDlgContents2(int iType);
	void DrawChatMsgs(short sX, short sY, short dX, short dY);
	void RequestFullObjectData(WORD wObjectID);
	BOOL bInitSkillCfgList();
	BOOL bCheckImportantFile();
	void DlgBoxDoubleClick_Inventory();
	void DlgBoxDoubleClick_Character();
	void DlgBoxDoubleClick_GuideMap();
	Point GetGuideMapPos(uint32 x, uint32 y);
	void EraseItem(char cItemID);
	void RetrieveItemHandler(char * pData);
	void CivilRightAdmissionHandler(char * pData);
	void _Draw_CharacterBody(short sX, short sY, short sType);
	void ClearContents_OnSelectCharacter();
	void ClearContents_OnCreateNewAccount();
	void _Draw_UpdateScreen_OnCreateNewAccount();
	BOOL _bDraw_OnCreateNewCharacter(char * pName, short msX, short msY, int iPoint);
	BOOL _bGetIsStringIsNumber(char * pStr);
	BOOL bInitMagicCfgList();
	BOOL __bDecodeContentsAndBuildItemForSaleList(char * pBuffer);
	void _LoadShopMenuContents(char cType);
	void PutChatScrollList(char * pMsg, char cType);
	void RequestTeleportAndWaitData();
	void DrawEffectLights();
	void PointCommandHandler(int indexX, int indexY, char cItemID = -1);
	void DrawEffects();
	void bAddNewEffect(short sType, int sX, int sY, int dX, int dY, char cStartFrame, int iV1 = 1);
	void AddEventList(char const * const pTxt, char cColor = 0, BOOL bDupAllow = TRUE);
	void ShowEventList(DWORD dwTime);
	void SetItemCount(char * pItemName, DWORD dwCount);
	uint32 GetItemCount(char * pItemName);
	void _ShiftGuildOperationList();
	void _PutGuildOperationList(char * pName, char cOpMode);
	void DisbandGuildResponseHandler(char * pData);
	void InitPlayerCharacteristics(char * pData);
	void CreateNewGuildResponseHandler(char * pData);
	void _GetHairColorRGB(int iColorType , int * pR, int * pG, int * pB);
	void InitGameSettings();
	void CommonEventHandler(char * pData);
	BOOL _bCheckDraggingItemRelease(char dlgID);
	void _SetItemOrder(char cWhere, char cItemID);
	int iGetTopDialogBoxIndex();
	void DisableDialogBox(int iBoxID);
	void EnableDialogBox(int iBoxID, int cType = 0, int sV1 = 0, int sV2 = 0, const char * pString = NULL);

	void InitItemList(char * pData);
	int _iCheckDlgBoxFocus(char cButtonSide);
	void GetPlayerTurn();
	BOOL __fastcall DrawObject_OnDead(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnDying(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnMagic(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnAttack(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnAttackMove(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnStop(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnMove_ForMenu(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnMove(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnDamageMove(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnRun(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnDamage(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL __fastcall DrawObject_OnGetItem(int indexX, int indexY, int sX, int sY, BOOL bTrans, DWORD dwTime, int msX, int msY);
	BOOL bEffectFrameCounter();
	void ClearGuildNameList();
	void DrawBackground(short sDivX, short sModX, short sDivY, short sModY);
	void DrawChatMsgBox(short sX, short sY, int iChatIndex, BOOL bIsPreDC);
	void ReleaseTimeoverChatMsg();
	void ChatMsgHandler(char * pData);
	void ReleaseUnusedSprites();
	BOOL bReadIp();
	void OnKeyUp(WPARAM wParam);
	void OnSysKeyDown(WPARAM wParam);
	void OnSysKeyUp(WPARAM wParam);
	void ChangeGameMode(char cMode);
	void PutString(int iX, int iY, char const * pString, COLORREF color);
	void PutString(int iX, int iY, char const * pString, COLORREF color, BOOL bHide, char cBGtype, BOOL bIsPreDC = FALSE);
	void PutString2(int iX, int iY, char * pString, short sR, short sG, short sB);
	void PutString2Centred(int iCX, int iY, char const * pString, short sR, short sG, short sB);	// PutString2, centred on iCX
	void PutString3(int iX, int iY, char const * pString, COLORREF color);
	void PutAlignedString(int iX1, int iX2, int iY, char const * const pString, short sR = 0, short sG = 0, short sB = 0);
	void PutAlignedLabel(int iX1, int iX2, int iY, char const * const pString);
	void PutAlignedLabel(const RECT * rect, char const * const pString);
	void PutLabel(const RECT & rect, char const * const pString, Alignment pos = POS_CENTER);
	void PutAlignedString(int iX1, int iX2, int iY, char const * const pString, bool highlight);
	void PutAlignedString(int iX1, int iX2, int iY, char const * const pString, COLORREF color);
	void PutAlignedString(int iX1, int iX2, int iY, char const * const pString, bool wordWrap, COLORREF color);
	void PutAlignedString(const RECT * rect, char const * const pString, COLORREF color);
	void ButtonString(CDialogBox & dlg, int button, char const * const pString, Alignment pos = POS_CENTER);
	void ButtonStringToggle(CDialogBox & dlg, int button, char const * const pString, bool enabled = true, Alignment pos = POS_CENTER);
	void PutStringRight(int iX1, int iX2, int iY, char const * const pString, COLORREF color = RGB(0,0,0));
	void PutStringLeft(int iX1, int iX2, int iY, char const * const pString, COLORREF color = RGB(0,0,0), bool wordWrap = false);
	void PutStringLeft(int iX1, int iX2, int iY, char const * const pString, bool highlight, bool wordWrap = false);
	void PutStringLeft(const RECT * rect, char const * const pString, COLORREF color = RGB(0,0,0), bool wordWrap = false);
	void PutString_SprFont(int iX, int iY, char * pStr, short sR, short sG, short sB);
	void PutString_SprFont2(int iX, int iY, char * pStr, short sR, short sG, short sB);
	void PutString_SprFont3(int iX, int iY, char * pStr, short sR, short sG, short sB, BOOL bTrans = FALSE, int iType = 0);
	void PutString_SprNum(int iX, int iY, char * pStr, short sR, short sG, short sB);
	void LogRecvMsgHandler(char * pData);
	void LogResponseHandler(char * pData);
	void OnLogSocketEvent(WPARAM wParam, LPARAM lParam);
	void OnTimer();
	void _ReadMapData(short sPivotX, short sPivotY, char * pData);
	void MotionEventHandler(char * pData);
	void InitDataResponseHandler(char * pData);
	void InitPlayerResponseHandler(char * pData);
	void ConnectionEstablishHandler(char cWhere);
	void MotionResponseHandler(char * pData);
	void GameRecvMsgHandler(DWORD dwMsgSize, char * pData);
	void DrawObjects(short sPivotX, short sPivotY, short sDivX, short sDivY, short sModX, short sModY, short msX, short msY);
	BOOL bSendCommand(DWORD dwMsgID, WORD wCommand = NULL, char cDir = NULL, int iV1 = NULL, int iV2 = NULL, int iV3 = NULL, char const * const pString = NULL, int iV4 = NULL); 
	BOOL SendLoginCommand(DWORD dwMsgID); 
	char cGetNextMoveDir(short sX, short sY, short dstX, short dstY, BOOL bMoveCheck = FALSE, BOOL isMIM = FALSE);
	void CommandProcessor(short msX, short msY, short indexX, short indexY, char cLB, char cRB, char cMB);
	void OnGameSocketEvent(WPARAM wParam, LPARAM lParam);
	void CalcViewPoint();
	void OnKeyDown(WPARAM wParam);
	void Quit();
	BOOL bInit(HWND hWnd, HINSTANCE hInst, char * pCmdLine);

	void ReserveFightzoneResponseHandler(char * pData);
	int _iGetAttackType();
	BOOL __bDecodeBuildItemContents(char * pBuffer);
	int _iGetBankItemCount();
	int _iGetTotalItemNum();
	uint32 GetGold() const;
	void SetGold(uint32 val);
	LONG GetRegKey(HKEY key, LPCTSTR subkey, LPTSTR retdata);
	void GoHomepage();
	void StartBGM();

	//Snoopy: added function:
	void DebugLog(char * cStr);
	BOOL bReadLoginConfigFile(char * cFn);
	int bHasHeroSet( short Appr3, short Appr4, char OwnerType);
	void ShowHeldenianVictory(short sSide);
	void DrawDialogBox_Resurect();
	void DlgBoxClick_Resurect();
	void DrawDialogBox_GuildSummons();
	void DlgBoxClick_GuildSummons();
	void DrawDialogBox_GuildQuery();
	void DlgBoxClick_GuildQuery();
	void DrawDialogBox_CMDHallMenu();
	void DlgBoxClick_CMDHallMenu();
	void ResponseHeldenianTeleportList(char *pData);
	void DKGlare(int iWeaponColor, int iWeaponIndex, int *iWeaponGlare);
	void DrawDruncncity();
	void Abaddon_corpse(int sX, int sY);
	void DrawAngel(int iSprite, short sX, short sY, char cFrame, DWORD dwTime);

	void LoadMuteList();
	void SaveMuteList();

	char MouseOverDialog();
	void SetupDialogBoxes();
	void SetupDialogBox(int dialog,  short X, short Y, short background, int backFrame, int titleTxtFrame = -1, bool trans = false);

	void ClearPartyMembers();

	BOOL _ItemDropHistory(char * ItemName);
	CGame();
	virtual ~CGame();

	struct {
		short sX;
		short sY;
		short sCursorFrame;
		char  cPrevStatus;
		char  cSelectedObjectType;
		short sSelectedObjectID;
		short sPrevX, sPrevY, sDistX, sDistY;
		DWORD dwSelectClickTime;
		short sClickX, sClickY;
	} m_stMCursor;

	CDialogBox m_dialogBoxes[MAXDIALOGBOX];

	char m_cDialogBoxOrder[MAXDIALOGBOX];
	int m_bIsDialogEnabled[MAXDIALOGBOX];//was BOOL
	ItemMapIter m_guildBankIt;
	
	struct {
		int   sV1, sV2, sV3, sV4, sV5, sV6, sV7, sItemID;
		DWORD dwV1;
		uint8 sockets[MAXITEMSOCKETS];
		char  cStr1[32], cStr2[32];
	} m_stDialogBoxExchangeInfo[8];

	struct {
		int iIndex;
		int iAmount;
	} m_stSellItemList[MAXSELLLIST];

	struct {
		char cName[22];
		char cOpMode;
	} m_stGuildOpList[100];

	struct {
		DWORD dwTime;
		char  cColor;
		char  cTxt[96];
	} m_stEventHistory[6];

	struct {
		DWORD dwTime;
		char  cColor;
		char  cTxt[96];
	} m_stEventHistory2[6];

	struct {
		short sX, sY, sBX;
		char cStep;
	} m_stWeatherObject[MAXWEATHEROBJECTS];

	struct MapPing{
		Point pos;
		time_t t;

		MapPing(uint32 x, uint32 y)
		{
			pos.x = x;
			pos.y = y;
			t = time(0);
		}

		static bool Expired(const MapPing ping)
		{
			return (time(0) - ping.t >= MAP_PING_DURATION) ? true : false;
		}
	};

	Vector<MapPing> m_mapPings;
	void AddMapPing(MapPing ping);

	struct {
		BOOL bIsQuestCompleted;
		short sWho, sQuestType, sContribution, sTargetType, sTargetCount, sX, sY, sRange;
		short sCurrentCount; // by Snoopy
		char cTargetName[22];
	} m_stQuest;

	struct partyMember{
		short sX, sY;
		string cName;
		partyMember(char * inName) : cName(inName), sX(0), sY(0) {} 
	}; 

	Vector<partyMember *> m_stPartyMember;
	typedef Vector<partyMember *>::iterator partyIterator;
	
	MailVec m_mails;
	MailVec m_gldBoard;

	struct {
		short sX, sY;
		char cType;
		char cSide;
	} m_stCrusadeStructureInfo[MAXCRUSADESTRUCTURES];

	struct {
		DWORD dwRefTime;
		int iGuildRank;
		char cCharName[12];
		char cGuildName[24];
	} m_stGuildName[MAXGUILDNAMES];

	struct {
		int iIndex;
		char mapname[12];
		int iX;
		int iY;
		int iCost;
		bool donateEvent;
	} m_stTeleportList[20];

	struct {
		int iIndex;
		int Type;
		int RewardAmount;
		int Contribution;
		int TargetType;
		int MaxCount;
		int sX;
		int sY;
		int Range;
		char TargetName[22];
	} m_stQuestList[50];

	class CAudioDevice m_Audio;
	class CSoundBuffer *	m_pCSound[MAXSOUNDEFFECTS];
	class CSoundBuffer *	m_pMSound[MAXSOUNDEFFECTS];
	class CSoundBuffer *	m_pESound[MAXSOUNDEFFECTS];
	class CSoundBuffer *    m_pBGM;
	class CRenderer m_Renderer;
	class CMouse    m_Mouse;
	class CMisc      m_Misc;
	class CSprite  * m_pSprite[MAXSPRITES];
	class CSprite  * m_pTileSpr[MAXTILES];
	class CSprite  * m_pEffectSpr[MAXEFFECTSPR];
	class CMapData * m_pMapData;
	class XSocket * m_pGSock;
	class XSocket * m_pLSock;
	class CMsg    * m_pChatMsgList[MAXCHATMSGS];

	typedef Vector<CMsg*> MsgVec;
	MsgVec * m_chatDisplay;
	MsgVec m_chatMsgs[CHAT_MAX];
	bool m_chatToggle[CHAT_MAX];

	class CMsg    * m_pWhisperMsg[MAXWHISPERMSG];
	class CEffect * m_pEffectList[MAXEFFECTS];
	class CItem   * m_pItemList[MAXITEMS];
	class CItem   * m_pBankList[MAXBANKITEMS];
	ItemMap m_guildBankMap;
	uint32 m_guildBankVer;
	class CMagic * m_pMagicCfgList[MAXMAGICTYPE];
	class CSkill * m_pSkillCfgList[MAXSKILLTYPE];
	class CMsg * m_pMsgTextList[TEXTDLGMAXLINES];
	class CMsg * m_pMsgTextList2[TEXTDLGMAXLINES];
	class CMsg * m_pAgreeMsgTextList[TEXTDLGMAXLINES];
	class CBuildItem * m_pBuildItemList[MAXBUILDITEMS];
	class CBuildItem * m_pDispBuildItemList[MAXBUILDITEMS];


	class CItem * m_pItemForSaleList[MAXMENUITEMS];
	class CCharInfo * m_pCharList[4];
	class CItemName * m_pItemNameList[MAXITEMNAMES];
	class CCurse m_curse;

	char * m_pInputBuffer;

	DWORD G_dwGlobalTime;
	DWORD m_dwCommandTime; //v2.15 SpeedHack
	DWORD m_dwConnectMode;
	DWORD m_dwTime;
	DWORD m_dwCurTime;
	DWORD m_dwCheckConnTime, m_dwCheckSprTime, m_dwCheckChatTime;
	DWORD m_dwDialogCloseTime;
	int  m_dwLogOutCountTime;//was DWORD
	DWORD m_dwRestartCountTime;
	DWORD m_dwWOFtime; //v1.4
	DWORD m_dwObserverCamTime;
	DWORD m_dwDamagedTime;
	DWORD m_dwSpecialAbilitySettingTime;
	DWORD m_dwCommanderCommandRequestedTime;
	DWORD m_dwTopMsgTime;
	DWORD m_dwEnvEffectTime;

	//v2.2
	DWORD m_dwMonsterEventTime;
	short m_sMonsterID;
	short m_sEventX, m_sEventY;

	bool m_shieldDisabled;
	bool m_armorDisabled;
	bool m_magicDisabled[MAXMAGICTYPE];

	Side m_side;

	BOOL m_bZoomMap;
	BOOL m_bIsProgramActive;
	int m_bCommandAvailable;//was BOOL
	BOOL m_bSoundFlag;
	BOOL m_bSoundStat, m_bMusicStat; // On/Off
	int m_bIsItemEquipped[MAXITEMS];//was BOOL
	int m_bIsItemDisabled[MAXITEMS];//was BOOL
	int m_bIsGetPointingMode;//was BOOL
	BOOL m_bEnterPressed, m_bEscPressed, m_bCtrlPressed, m_bRunningMode, m_bShiftPressed, m_altPressed;

	BOOL m_bDialogTrans;
	BOOL m_bIsCombatMode;
	BOOL m_bIsSafeAttackMode;
	int m_bSkillUsingStatus;//was BOOL
	int m_bItemUsingStatus;//was BOOL
	BOOL m_bSuperAttackMode;	//
	BOOL m_bIsObserverMode, m_bIsObserverCommanded;
	int m_bIsPoisoned;//was BOOL
	BOOL m_bIsFirstConn;
	BOOL m_bIsConfusion;
	BOOL m_bIsRedrawPDBGS;
	BOOL m_bDrawFlagDir;
	BOOL m_bIsCrusadeMode;
	BOOL m_bIsAstoriaMode;
	Casualties m_astoriaStats[MAXSIDES];
	uint16 m_astoriaShieldsHP[MAXSIDES];
	DWORD m_relicOwnedTime;
	Side m_relicOwnedSide;
	int m_bIsSpecialAbilityEnabled;//was BOOL
	BOOL m_bInputStatus;
	BOOL m_bToggleScreen;
	BOOL m_bIsSpecial;
	COLORREF m_itemColor;

	BOOL m_bIsF1HelpWindowEnabled;
	int m_bIsTeleportRequested;//was BOOL
	BOOL m_bIsPrevMoveBlocked;
	BOOL m_bIsHideLocalCursor;

	int m_bForceDisconn;//was BOOL
	BOOL m_targetNeutral;
	BOOL m_bForceAttack;
	BOOL m_bParalyze;

	int m_iFPS;
	DWORD m_dwFPStime;
	bool  m_showTime;
	bool  m_showGrid;
	bool  m_showAllDmg;
	bool  m_bigItems;
	bool  m_showTimeStamp;
	bool  m_windowsKey;
	bool  m_displayQuest;

	int m_iFightzoneNumber;
	int m_iFightzoneNumberTemp;
	int m_iPlayerApprColor; 
	int m_iHP;//was int			// Hit Point
	int m_iMP;//was int			// Mana Point
	int m_iSP;//was int			// Staminar Point
	int m_iAC;						// Armour Class
	int m_iTHAC0;					// To Hit Armour Class 0

	int m_iLevel, m_iExp, m_iContribution;
	int m_stat[6];
	//int m_angelStat[STAT_STR], m_angelStat[STAT_INT], m_angelStat[STAT_DEX], m_angelStat[STAT_MAG];
	int m_angelStat[6];
	char m_createStat[6];
	short m_luStat[6];
	int m_iLU_Point;

	int m_iEnemyKillCount;
	int m_iEnemyKillTotalCount;
	int m_iPKCount;
	int m_iRewardGold;
	CGuild m_guild;
	GuildRank m_iGuildRank;
	int m_iTotalGuildsMan;
	int m_iPointCommandType;
	int m_iTotalChar;
//	int m_iAccountStatus;
	short m_sMagicShortCut;
	int m_iCameraShakingDegree;
	int m_iSuperAttackLeft;
	int m_iAccntYear, m_iAccntMonth, m_iAccntDay;
	int m_iIpYear, m_iIpMonth, m_iIpDay;
	int m_iDownSkillIndex;

	int m_iIlusionOwnerH;
	int m_iApprColor_IE;
	int m_iInputX, m_iInputY, m_iInputX2;
	int m_iPDBGSdivX, m_iPDBGSdivY;			   // Pre-Draw Background Surface
	short m_sRecentShortCut;
	short m_sShortCut[6]; // Snoopy: 6 shortcuts
	int	m_iSpecialAbilityTimeLeftSec;
	int m_iDrawFlag;
	int m_iSpecialAbilityType;
	int m_iTimeLeftSecAccount, m_iTimeLeftSecIP;
	int m_iCrusadeDuty;
	int m_iLogServerPort;
	int m_iRating; //Rating

	int m_iPrevMoveX, m_iPrevMoveY;
	int m_iBlockYear, m_iBlockMonth, m_iBlockDay;
	unsigned char m_iTopMsgLastSec;
	int m_iConstructionPoint;
	int m_iWarContribution;
	int m_iConstructLocX, m_iConstructLocY;
	int m_iNetLagCount;
	int m_iTeleportLocX, m_iTeleportLocY;
	int m_iTotalPartyMember;
	int m_iPartyStatus;
	int m_iGizonItemUpgradeLeft;
	//int m_iFeedBackCardIndex; // removed by Snoopy

	short m_sItemEquipmentStatus[MAXITEMEQUIPPOS];
	short m_sPlayerX, m_sPlayerY;
	short m_sPlayerObjectID;
	short m_sPlayerType;
	short m_sPlayerAppr1, m_sPlayerAppr2, m_sPlayerAppr3, m_sPlayerAppr4;
	UnitStatus m_iPlayerStatus;
	short m_sMCX, m_sMCY;
	short m_sCommX, m_sCommY;
	int   m_iCastingMagicType;
	short m_sDamageMove, m_sDamageMoveAmount;
	short m_sAppr1_IE, m_sAppr2_IE, m_sAppr3_IE, m_sAppr4_IE;
	int m_iStatus_IE;
	short m_sViewDstX, m_sViewDstY;
	short m_sViewPointX, m_sViewPointY;

	// ---- How much of the map this client sees (UI\GameView.cpp) -------------------
	// The server sends map data for (2*halfX + 1) x (2*halfY + 2) tiles around the player
	// and the player stands at column halfX, row halfY of the screen. 10 x 7 is the
	// classic 640x480 view; the game canvas size follows the view the server granted.
	int  m_iViewHalfX, m_iViewHalfY;
	void GetDesiredView(int * pHalfX, int * pHalfY) const;
	static void GetCanvasForView(int iHalfX, int iHalfY, int * pW, int * pH);
	void RequestViewSize();						// asks the server for the view the settings want
	void ViewSizeResponseHandler(char * pData);
	void ApplyGameCanvas();						// canvas: classic 640x480 outside the game, view-sized in it
	void RelayoutDialogs(int iOldW, int iOldH);	// keeps the interface centred / bottom-anchored
	short m_sVDL_X, m_sVDL_Y;

	WORD m_wCommObjectID;
	WORD m_wEnterGameType;
	WORD m_wR[16], m_wG[16], m_wB[16];
	WORD m_wWR[16], m_wWG[16], m_wWB[16];

	uint32 m_inputMaxLen;
	char m_cEdit[4];
	char G_cTxt[128];
	char G_cTxt2[MAX_MAIL_MSG_LENGTH+1];
	char m_cGameModeCount;
	char m_cBGMmapName[12];
	char m_cItemOrder[MAXITEMS];
	char m_cAmountString[12];
	char m_cLogOutCount;
	char m_cRestartCount;
	char m_cGameMode;
	char m_cWhisperIndex;
	char m_cWhisperName[12];
	char m_cAccountName[12];
	char m_cAccountPassword[12];
	char m_cAccountAge[12];
	char m_cNewPassword[12];
	char m_cNewPassConfirm[12];
	char m_cAccountCountry[18];
	char m_cAccountSSN[32];
	char m_cEmailAddr[52];
	char m_cAccountQuiz[46];// Quiz
	char m_cAccountAnswer[22];
	char m_cPlayerName[12];
	char m_cPlayerDir;
	char m_cMsg[200];
	char m_cLocation[12];
	char m_cCurLocation[12];
	char m_cGuildName[22];
	char m_cMCName[12];
	char m_cMapName[12];
	char m_cMapMessage[32];
	char m_cMapIndex;
	char m_cPlayerTurn;
	char m_cCommand;
	char m_cCurFocus, m_cMaxFocus;
	char m_cEnterCheck, m_cTabCheck, m_cLeftArrowCheck;
	char m_cArrowPressed;
	char m_cLogServerAddr[16];
	char m_cChatMsg[90];		// the server takes up to 91 characters of chat text
	char m_cBackupChatMsg[90];
	char m_cGender, m_cSkinCol, m_cHairStyle, m_cHairCol, m_cUnderCol;

	char m_cMagicMastery[MAXMAGICTYPE];
	unsigned char m_cSkillMastery[MAXSKILLTYPE]; 
	uint32 m_cash;

	char m_cWorldServerName[32];
	char m_cDetailLevel;
	char m_cMenuDir, m_cMenuDirCnt, m_cMenuFrame;
	char m_cSoundVolume, m_cMusicVolume;
	Weather m_weather;
	char m_cIlusionOwnerType;
	char m_cName_IE[12];
	char m_sViewDX, m_sViewDY;
	char m_cCommandCount;
	char m_cLoading;
	char m_cDiscount;

	char m_cStatusMapName[12];
	char m_cTopMsg[128];
	char m_cTeleportMapName[12];
	char m_cConstructMapName[12];
	char m_cGameServerName[22]; //  Gateway

	char m_cItemDrop[25][25];

	RECT m_rcPlayerRect, m_rcBodyRect;
	HWND m_hWnd;

	HANDLE m_hPakFile;

	BOOL m_bWhisper;
	BOOL m_bShout;

	BOOL m_bItemDrop;
    int  m_iItemDropCnt;

	// Snoopy: Apocalypse Gate
	char m_cGateMapName[12];
	int  m_iGatePositX, m_iGatePositY;
	int m_iHeldenianAresdenLeftTower;
	int m_iHeldenianElvineLeftTower;
	int m_iHeldenianAresdenFlags;
	int m_iHeldenianElvineFlags;
	int m_iHeldenianAresdenDead;
	int m_iHeldenianElvineDead;
	int m_iHeldenianAresdenKill;
	int m_iHeldenianElvineKill;
	bool m_bIsHeldenianMode;
	bool m_bIsHeldenianMap;

	BOOL m_bIllusionMVT;
	int m_iGameServerMode;
	BOOL m_bIsXmas;
	BOOL m_bUsingSlate;


	//Snoopy: Avatar
	BOOL m_bIsAvatarMode;
	BOOL m_bIsAvatarMessenger;

	//Snoopy: Crafting
	//BOOL _bDecodeCraftItemContents();
	//BOOL __bDecodeCraftItemContents(char *pBuffer);	
	//BOOL _bCheckCraftItemStatus();
	//BOOL _bCheckCurrentCraftItemStatus();

	class CBuildItem * m_pCraftItemList[MAXBUILDITEMS];
	class CBuildItem * m_pDispCraftItemList[MAXBUILDITEMS];
	int   m_iContributionPrice;

	char m_cTakeHeroItemName[100]; //Drajwer - hero item str

	int m_levelExpTable[MAXLEVEL +20];

	std::set<string> m_MuteList;

	BOOL m_partyAutoAccept;

	bool m_ekScreenshot;
	char m_cShotFile[256] = {};		// a screenshot to take of the next frame (CreateScreenShot)
	char m_cShotStamp[32] = {};		// its date and time, drawn on it
	DWORD m_ekSSTime;

	bool m_tabbedNotification;
	int m_relicX, m_relicY;

	bool m_manuAutoFill;

	DWORD m_weight;

	bool m_droppedOnIconbar;
	bool m_showBtns;

	int m_SoccerAresdenGoals, m_SoccerElvineGoals;
	bool m_bSoccerMode;
};

#endif // !defined(AFX_GAME_H__0089D9E3_74E6_11D2_A8E6_00001C7030A6__INCLUDED_)
