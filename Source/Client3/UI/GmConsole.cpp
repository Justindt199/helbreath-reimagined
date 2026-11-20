// GmConsole.cpp: the in-game GM console (VALORIA's developer console, for Helbreath). Only Game
// Masters get it: the server tells the client its GM level on entering the game (NOTIFY_GMLEVEL,
// HG Server GmConsole.cpp), and checks it again on every command.
//
//   - toggle with Ctrl+Tab; the command line takes the typing while it is open (help lists the
//     commands, Up / Down go through the ones you typed, Enter runs one).
//   - tabs: Player (fixes, cheats, progress, stats), Teleport (every map, shrines and chests),
//     Items (search and give), Monsters (search and spawn, kill), World (weather, map tools).
//   - with Click-teleport on, Ctrl + left-click on the ground jumps there.
//   - what the server answers is copied into the console's log while it is open.
//
// The commands run Helbreath's own GM commands (/tp, /ci, /summon, /invincible ...) and the
// console's own /gm commands (heal, levelup, stat, learn, killall, town).
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"
#include "..\..\shared\ValoriaWorld.h"
#include "GmData.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

const COLORREF kAccent	= RGB(115, 215, 255);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kText	= RGB(204, 210, 220);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(110, 114, 124);
const COLORREF kOk		= RGB(150, 230, 150);
const COLORREF kBad		= RGB(255, 135, 115);
const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kBoss	= RGB(255, 165, 90);

const int W = CGame::GMWIN_W, H = CGame::GMWIN_H;
const int kBodyTop = 66, kLogH = 104, kInputH = 24;
const int kListRows = 12;

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

enum { TAB_PLAYER, TAB_TELEPORT, TAB_ITEMS, TAB_MONSTERS, TAB_WORLD, TAB_COUNT };
const char * const kTabs[TAB_COUNT] = { "Player", "Teleport", "Items", "Monsters", "World" };

// Something clickable: a button runs its command; a few have special actions.
enum { ACT_RUN, ACT_TAB, ACT_PAGE, ACT_QTY, ACT_SEARCH, ACT_INPUT, ACT_CLOSE, ACT_HEAD, ACT_LABEL };
struct Btn {
	RECT rc;
	int act, arg;
	std::string label, cmd;
	bool on;
	COLORREF col;
};

int BodyBottom() { return H - kLogH - kInputH - 20; }

// Lays buttons out left to right, wrapping, inside the body.
struct Flow {
	std::vector<Btn> & out;
	int X, Y, x, y, x1;
	CRenderer & R;
	Flow(std::vector<Btn> & o, CRenderer & r, int bx, int by) : out(o), X(bx), Y(by), x(bx + 16), y(by + kBodyTop + 4), x1(bx + W - 16), R(r) {}
	void Head(const char * s) {
		if (x != X + 16) NewLine();
		Btn b; SetRect(&b.rc, X + 16, y, X + W - 16, y + 16); b.act = ACT_HEAD; b.arg = 0; b.label = s; b.on = false; b.col = kAccent;
		out.push_back(b); y += 18;
	}
	void Label(const std::string & s, int w, COLORREF c = kText) {
		if (x + w > x1) NewLine();
		Btn b; SetRect(&b.rc, x, y, x + w, y + 20); b.act = ACT_LABEL; b.arg = 0; b.label = s; b.on = false; b.col = c;
		out.push_back(b); x += w + 4;
	}
	void Button(const std::string & s, const std::string & cmd, bool on = false, int act = ACT_RUN, int arg = 0, int minW = 0) {
		int w = R.TextWidthSized(s.c_str(), 11) + 18;
		if (w < minW) w = minW;
		if (x + w > x1) NewLine();
		Btn b; SetRect(&b.rc, x, y, x + w, y + 20); b.act = act; b.arg = arg; b.label = s; b.cmd = cmd; b.on = on; b.col = kWhite;
		out.push_back(b); x += w + 5;
	}
	void NewLine() { x = X + 16; y += 24; }
	void Gap() { if (x != X + 16) NewLine(); y += 4; }
};

std::string Lower(std::string s) { for (auto & c : s) c = (char)tolower((unsigned char)c); return s; }

} // namespace

//////////////////////////////////////////////////////////////////////
// State, messages, keys

void CGame::NotifyMsg_GmLevel(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	uint8 lv;
	Pop(cp, lv);
	m_iGmLevel = lv;
	if (m_iGmLevel <= 0 && m_bIsDialogEnabled[DIALOG_GMCONSOLE]) ToggleGmConsole();
	if (m_iGmLevel > 0 && m_gmLog.empty()) {
		GmLog("GM console. Type help for the commands. Ctrl+click teleports when Click-teleport is on.", kAccent);
		AddEventList("GM console ready: press Ctrl+Tab.", 10);
	}
}

void CGame::GmLog(const std::string & s, COLORREF c)
{
	m_gmLog.push_back(s);
	m_gmLogCol.push_back(c);
	if (m_gmLog.size() > 200) { m_gmLog.erase(m_gmLog.begin()); m_gmLogCol.erase(m_gmLogCol.begin()); }
}

bool CGame::GmTyping() const
{
	return m_bInputStatus && (m_pInputBuffer == m_cGmInput || m_pInputBuffer == m_cGmSearch);
}

void CGame::ToggleGmConsole()
{
	if (m_bIsDialogEnabled[DIALOG_GMCONSOLE]) {
		if (GmTyping()) EndInputString();
		DisableDialogBox(DIALOG_GMCONSOLE);
		return;
	}
	if (m_iGmLevel <= 0 || m_cGameMode != GAMEMODE_ONMAINGAME) return;
	if (m_bInputStatus) EndInputString();		// the chat line gives way
	EnableDialogBox(DIALOG_GMCONSOLE, NULL, NULL, NULL);
	CDialogBox & dlg = m_dialogBoxes[DIALOG_GMCONSOLE];
	ZeroMemory(m_cGmInput, sizeof(m_cGmInput));
	StartInputString(dlg.m_X + 22, dlg.m_Y + H - kInputH - 8, sizeof(m_cGmInput), m_cGmInput, FALSE, dlg.m_X + W - 22);
	PlaySound('E', 14, 5);
}

// Enter while typing in the console: runs the line (or ends the search box).
bool CGame::GmConsoleOnEnter()
{
	if (!GmTyping()) return false;
	if (!m_bIsDialogEnabled[DIALOG_GMCONSOLE]) { EndInputString(); return true; }	// closed while typing: nothing to run
	if (m_pInputBuffer == m_cGmSearch) { EndInputString(); m_iGmPage = 0; return true; }
	EndInputString();
	char cLine[sizeof(m_cGmInput)];
	strcpy(cLine, m_cGmInput);
		std::string s = cLine;
	while (!s.empty() && s.back() == ' ') s.pop_back();
	while (!s.empty() && s[0] == ' ') s.erase(0, 1);
	if (!s.empty()) {
		if (m_gmHistory.empty() || m_gmHistory.back() != s) m_gmHistory.push_back(s);
		m_iGmHist = (int)m_gmHistory.size();
		GmLog("> " + s, kGrey);
		GmRun(s.c_str());
	}
	ZeroMemory(m_cGmInput, sizeof(m_cGmInput));
	if (m_bIsDialogEnabled[DIALOG_GMCONSOLE]) {
		CDialogBox & dlg = m_dialogBoxes[DIALOG_GMCONSOLE];
		StartInputString(dlg.m_X + 22, dlg.m_Y + H - kInputH - 8, sizeof(m_cGmInput), m_cGmInput, FALSE, dlg.m_X + W - 22);
	}
	return true;
}

// Up / Down on the command line go through what was typed.
bool CGame::GmConsoleKey(WPARAM wParam)
{
	if (!m_bIsDialogEnabled[DIALOG_GMCONSOLE] || !m_bInputStatus || m_pInputBuffer != m_cGmInput || m_gmHistory.empty()) return false;
	if (wParam == VK_UP) m_iGmHist = (std::max)(0, m_iGmHist - 1);
	else if (wParam == VK_DOWN) m_iGmHist = (std::min)((int)m_gmHistory.size(), m_iGmHist + 1);
	else return false;
	ZeroMemory(m_cGmInput, sizeof(m_cGmInput));
	if (m_iGmHist < (int)m_gmHistory.size()) strncpy(m_cGmInput, m_gmHistory[m_iGmHist].c_str(), sizeof(m_cGmInput) - 1);
	ZeroMemory(m_cEdit, sizeof(m_cEdit));
	return true;
}

// Ctrl + left-click on the ground with Click-teleport on: jump there.
bool CGame::GmClickTeleport(short msX, short msY)
{
	if (!m_bGmClickTp || !m_bIsDialogEnabled[DIALOG_GMCONSOLE] || !m_bCtrlPressed) return false;
	char cCmd[64], cMap[12] = {};
	memcpy(cMap, m_cMapName, 10);
	wsprintf(cCmd, "tp %s %d %d", cMap, (m_sViewPointX + msX) / 32, (m_sViewPointY + msY) / 32);
	GmRun(cCmd);
	return true;
}

//////////////////////////////////////////////////////////////////////
// Commands

namespace {

const char * const kHelp[] = {
	"heal  |  town  |  levelup [n]  |  stat <str|dex|vit|int|mag|chr> <+n>  |  learn",
	"god  |  noaggro  |  invis  |  eth  |  clicktp",
	"tp <map> [x y]  |  goto <player>  |  bring <player>  |  maps [text]",
	"give <item> [n]  |  gold <n>  |  items <text>",
	"spawn <monster> [n]  |  kill  |  killall  |  unsummon  |  monsters [text]",
	"weather <0 sunny, 1-3 rain, 4-6 snow>  |  clearmap  |  who  |  count  |  clear (the log)",
	"Anything starting with / goes to the server as it is.",
};

} // namespace

void CGame::GmSend(const std::string & s)
{
	char cTxt[200];
	strncpy(cTxt, s.c_str(), sizeof(cTxt) - 1);
	cTxt[sizeof(cTxt) - 1] = 0;
	bSendCommand(MSGID_COMMAND_CHATMSG, NULL, NULL, NULL, NULL, NULL, cTxt);
}

void CGame::GmRun(const char * pCmd)
{
	if (m_iGmLevel <= 0) return;
	std::string line = pCmd;
	if (line.empty()) return;
	if (line[0] == '/') { GmSend(line); return; }
	const size_t sp = line.find(' ');
	const std::string c = Lower(line.substr(0, sp));
	std::string arg = (sp == std::string::npos) ? "" : line.substr(sp + 1);
	while (!arg.empty() && arg[0] == ' ') arg.erase(0, 1);
	std::string a1 = arg.substr(0, arg.find(' ')), a2;
	if (arg.find(' ') != std::string::npos) { a2 = arg.substr(arg.find(' ') + 1); while (!a2.empty() && a2[0] == ' ') a2.erase(0, 1); }
	char cTxt[200];

	if (c == "help" || c == "?") { for (const char * h : kHelp) GmLog(h, kText); }
	else if (c == "clear") { m_gmLog.clear(); m_gmLogCol.clear(); }
	else if (c == "heal" || c == "learn" || c == "killall" || c == "town") GmSend("/gm " + c);
	else if (c == "levelup" || c == "level") GmSend("/gm levelup " + (a1.empty() ? std::string("1") : a1));
	else if (c == "stat") GmSend("/gm stat " + arg);
	else if (c == "god") { m_bGmGod = !m_bGmGod; GmSend("/invincible"); }
	else if (c == "noaggro") { m_bGmNoAggro = !m_bGmNoAggro; GmSend("/noaggro"); }
	else if (c == "invis" || c == "invi") { m_bGmInvis = !m_bGmInvis; GmSend("/invi"); }
	else if (c == "eth" || c == "noclip") { m_bGmEth = !m_bGmEth; GmSend("/eth"); }
	else if (c == "clicktp") {
		m_bGmClickTp = !m_bGmClickTp;
		GmLog(m_bGmClickTp ? "Click-teleport on: Ctrl + left-click on the ground jumps there." : "Click-teleport off.", kOk);
	}
	else if (c == "tp") {
		if (a1.empty()) { GmLog("tp <map> [x y]   (maps lists them)", kBad); return; }
		std::string id = a1;		// a map's friendly name works too
		for (int i = 0; i < GmData::MAP_COUNT; i++)
			if (Lower(GmData::kMaps[i].name) == Lower(arg) || Lower(GmData::kMaps[i].id) == Lower(a1)) { id = GmData::kMaps[i].id; if (Lower(GmData::kMaps[i].name) == Lower(arg)) a2 = ""; break; }
		GmSend("/tp " + id + (a2.empty() ? "" : " " + a2));
	}
	else if (c == "maps") {
		std::string s;
		for (int i = 0; i < GmData::MAP_COUNT; i++) {
			if (!a1.empty() && Lower(GmData::kMaps[i].id).find(Lower(a1)) == std::string::npos && Lower(GmData::kMaps[i].name).find(Lower(a1)) == std::string::npos) continue;
			if (!s.empty()) s += ", ";
			s += GmData::kMaps[i].id;
			if (s.size() > 90) { GmLog(s, kText); s.clear(); }
		}
		GmLog(s.empty() ? "no matches" : s, kText);
	}
	else if (c == "goto") GmSend("/goto " + a1);
	else if (c == "bring") GmSend("/summonplayer " + a1);
	else if (c == "give") {
		if (a1.empty()) { GmLog("give <item> [n]   (items <text> finds names)", kBad); return; }
		const int n = a2.empty() ? 1 : (std::max)(1, atoi(a2.c_str()));
		wsprintf(cTxt, "/ci %s 0 0 %d", a1.c_str(), n);
		GmSend(cTxt);
	}
	else if (c == "gold") { wsprintf(cTxt, "/ci Gold 0 0 %d", a1.empty() ? 10000 : (std::max)(1, atoi(a1.c_str()))); GmSend(cTxt); }
	else if (c == "items") {
		std::string s; int n = 0;
		for (int i = 0; i < MAXITEMNAMES && n < 60; i++) {
			if (m_pItemNameList[i] == NULL) continue;
			const std::string o = m_pItemNameList[i]->m_cOriginName, d = m_pItemNameList[i]->m_cName;
			if (!a1.empty() && Lower(o).find(Lower(arg)) == std::string::npos && Lower(d).find(Lower(arg)) == std::string::npos) continue;
			if (!s.empty()) s += ", ";
			s += o; n++;
			if (s.size() > 90) { GmLog(s, kText); s.clear(); }
		}
		GmLog(s.empty() ? (n ? "" : "no matches") : s, kText);
	}
	else if (c == "spawn" || c == "summon") {
		if (a1.empty()) { GmLog("spawn <monster> [n]   (monsters lists them)", kBad); return; }
		GmSend("/summon " + a1 + " " + (a2.empty() ? std::string("1") : a2));
	}
	else if (c == "monsters") {
		std::string s;
		for (int i = 0; i < GmData::MONSTER_COUNT; i++) {
			if (!a1.empty() && Lower(GmData::kMonsters[i].name).find(Lower(a1)) == std::string::npos) continue;
			if (!s.empty()) s += ", ";
			s += GmData::kMonsters[i].name;
			if (s.size() > 90) { GmLog(s, kText); s.clear(); }
		}
		GmLog(s.empty() ? "no matches" : s, kText);
	}
	else if (c == "kill") GmSend("/vkill");
	else if (c == "unsummon") GmSend("/unsummonall");
	else if (c == "weather") GmSend("/setweather " + (a1.empty() ? std::string("0") : a1));
	else if (c == "clearmap") GmSend("/clearmap");
	else if (c == "who") GmSend("/who");
	else if (c == "count") GmSend("/mcount");
	else GmLog("Unknown command: " + c + "   (type help)", kBad);
}

//////////////////////////////////////////////////////////////////////
// The window

namespace {

// Everything clickable, for the current state (shared by drawing and clicking).
void GmButtons(CGame & g, std::vector<Btn> & out)
{
	CDialogBox & dlg = g.m_dialogBoxes[DIALOG_GMCONSOLE];
	const short X = dlg.m_X, Y = dlg.m_Y;
	Btn b;
	b.on = false; b.col = kWhite; b.arg = 0;
	SetRect(&b.rc, X + W - 32, Y + 6, X + W - 8, Y + 30); b.act = ACT_CLOSE; b.label = "x"; out.push_back(b);
	int tx = X + 14;
	for (int t = 0; t < TAB_COUNT; t++) {
		const int w = g.m_Renderer.TextWidthSized(kTabs[t], 12) + 26;
		SetRect(&b.rc, tx, Y + 38, tx + w, Y + 60); b.act = ACT_TAB; b.arg = t; b.label = kTabs[t]; b.on = (t == g.m_iGmTab);
		out.push_back(b); tx += w + 4;
	}
	SetRect(&b.rc, X + 14, Y + H - kInputH - 12, X + W - 14, Y + H - 10); b.act = ACT_INPUT; b.label = ""; b.on = false; out.push_back(b);

	Flow f(out, g.m_Renderer, X, Y);
	char cTxt[128];
	switch (g.m_iGmTab) {
	case TAB_PLAYER: {
		f.Head("FIXES");
		f.Button("Full heal", "heal"); f.Button("Return to town", "town"); f.Button("Unsummon all", "unsummon");
		f.Gap(); f.Head("CHEATS (they switch on and off)");
		f.Button(std::string("God mode: ") + (g.m_bGmGod ? "ON" : "off"), "god", g.m_bGmGod);
		f.Button(std::string("No aggro: ") + (g.m_bGmNoAggro ? "ON" : "off"), "noaggro", g.m_bGmNoAggro);
		f.Button(std::string("Invisible: ") + (g.m_bGmInvis ? "ON" : "off"), "invis", g.m_bGmInvis);
		f.Button(std::string("Ethereal: ") + (g.m_bGmEth ? "ON" : "off"), "eth", g.m_bGmEth);
		f.Button(std::string("Click-teleport: ") + (g.m_bGmClickTp ? "ON" : "off"), "clicktp", g.m_bGmClickTp);
		f.Gap(); f.Head("PROGRESS");
		f.Button("+1 level", "levelup 1"); f.Button("+5 levels", "levelup 5"); f.Button("+10 levels", "levelup 10");
		f.Button("+10,000 gold", "gold 10000"); f.Button("+1,000,000 gold", "gold 1000000"); f.Button("Learn all spells", "learn");
		f.Gap(); f.Head("STATS");
		static const char * const kStat[6] = { "str", "dex", "vit", "int", "mag", "chr" };
		static const int kIdx[6] = { STAT_STR, STAT_DEX, STAT_VIT, STAT_INT, STAT_MAG, STAT_CHR };
		for (int s = 0; s < 6; s++) {
			wsprintf(cTxt, "%s  %d", kStat[s], g.m_stat[kIdx[s]]);
			for (char * p = cTxt; *p && *p != ' '; p++) *p = (char)toupper((unsigned char)*p);
			f.Label(cTxt, 86);
			static const int kD[5] = { -10, -1, 1, 10, 50 };
			for (int d = 0; d < 5; d++) {
				char cLab[8], cCmd[32];
				wsprintf(cLab, "%+d", kD[d]);
				wsprintf(cCmd, "stat %s %d", kStat[s], kD[d]);
				f.Button(cLab, cCmd, false, ACT_RUN, 0, 40);
			}
			f.NewLine();
		}
		break; }
	case TAB_TELEPORT: {
		f.Head("MAPS");
		const int first = g.m_iGmPage * 30;
		for (int i = first; i < GmData::MAP_COUNT && i < first + 30; i++) {
			const bool bHere = strncmp(g.m_cMapName, GmData::kMaps[i].id, 10) == 0;
			f.Button(std::string(GmData::kMaps[i].name) + (bHere ? " (here)" : ""), std::string("tp ") + GmData::kMaps[i].id, bHere);
		}
		f.Gap();
		if (GmData::MAP_COUNT > 30) {
			wsprintf(cTxt, "Maps %d-%d of %d", first + 1, (std::min)(first + 30, GmData::MAP_COUNT), GmData::MAP_COUNT);
			f.Label(cTxt, 150, kGrey);
			f.Button("< Prev", "", false, ACT_PAGE, -1); f.Button("Next >", "", false, ACT_PAGE, 1);
		}
		f.Gap(); f.Head("SHRINES AND CHESTS");
		int n = 0;
		for (int i = 0; i < World::PROP_COUNT && n < 24; i++) {
			const World::Prop & p = World::kProps[i];
			if (strncmp(p.map, g.m_cMapName, 10) != 0) continue;		// the ones on this map
			wsprintf(cTxt, "%s %d,%d", p.kind == World::CHEST ? "Chest" : "Shrine", p.x, p.y);
			char cCmd[64]; wsprintf(cCmd, "tp %s %d %d", p.map, p.x, p.y + 1);
			f.Button(cTxt, cCmd); n++;
		}
		if (n == 0) f.Label("None on this map.", 200, kDim);
		f.Gap();
		f.Label("tp <map> x y on the command line goes anywhere; Ctrl+click with Click-teleport on.", W - 40, kDim);
		break; }
	case TAB_ITEMS:
	case TAB_MONSTERS: {
		const bool bItems = g.m_iGmTab == TAB_ITEMS;
		f.Head(bItems ? "GIVE ITEMS (search, then Give)" : "SPAWN MONSTERS NEXT TO YOU");
		f.Button(g.m_cGmSearch[0] ? std::string("Search: ") + g.m_cGmSearch : std::string("Search..."), "", g.GmTyping() && g.m_pInputBuffer == g.m_cGmSearch, ACT_SEARCH, 0, 240);
		if (bItems) {
			f.Label("Qty", 30, kGrey);
			static const int kQ[5] = { 1, 10, 50, 100, 1000 };
			for (int q = 0; q < 5; q++) { char cl[8]; wsprintf(cl, "%d", kQ[q]); f.Button(cl, "", g.m_iGmQty == kQ[q], ACT_QTY, kQ[q], 34); }
		}
		else { f.Button("Kill nearby", "kill"); f.Button("Kill all on map", "killall"); f.Button("Unsummon all", "unsummon"); }
		f.NewLine();
		// the matches
		std::vector<int> v;
		const std::string q = Lower(g.m_cGmSearch);
		if (bItems) {
			for (int i = 0; i < MAXITEMNAMES; i++) {
				if (g.m_pItemNameList[i] == NULL) continue;
				if (!q.empty() && Lower(g.m_pItemNameList[i]->m_cOriginName).find(q) == std::string::npos && Lower(g.m_pItemNameList[i]->m_cName).find(q) == std::string::npos) continue;
				v.push_back(i);
			}
		}
		else for (int i = 0; i < GmData::MONSTER_COUNT; i++) if (q.empty() || Lower(GmData::kMonsters[i].name).find(q) != std::string::npos) v.push_back(i);
		const int pages = (std::max)(1, ((int)v.size() + kListRows - 1) / kListRows);
		if (g.m_iGmPage >= pages) g.m_iGmPage = pages - 1;
		for (int r = 0; r < kListRows; r++) {
			const int k = g.m_iGmPage * kListRows + r;
			if (k >= (int)v.size()) break;
			if (bItems) {
				const CItemName * pn = g.m_pItemNameList[v[k]];
				wsprintf(cTxt, "%s", pn->m_cName);
				f.Label(cTxt, 330);
				char cCmd[80], cLab[24];
				wsprintf(cLab, "Give %d", g.m_iGmQty);
				wsprintf(cCmd, "give %s %d", pn->m_cOriginName, g.m_iGmQty);
				f.Button(cLab, cCmd, false, ACT_RUN, 0, 80);
			}
			else {
				const GmData::Monster & m = GmData::kMonsters[v[k]];
				wsprintf(cTxt, "%s   HD %d  XP %d%s", m.name, m.hp, m.xp, m.boss ? "  BOSS" : "");
				f.Label(cTxt, 330, m.boss ? kBoss : kText);
				char cCmd[64];
				wsprintf(cCmd, "spawn %s 1", m.name); f.Button("x1", cCmd, false, ACT_RUN, 0, 44);
				wsprintf(cCmd, "spawn %s 5", m.name); f.Button("x5", cCmd, false, ACT_RUN, 0, 44);
			}
			f.NewLine();
			f.y -= 2;
		}
		f.Gap();
		wsprintf(cTxt, "%d matches  -  page %d of %d", (int)v.size(), g.m_iGmPage + 1, pages);
		f.Label(cTxt, 200, kGrey);
		f.Button("< Prev", "", false, ACT_PAGE, -1); f.Button("Next >", "", false, ACT_PAGE, 1);
		break; }
	case TAB_WORLD:
		f.Head("WEATHER");
		f.Button("Sunny", "weather 0"); f.Button("Light rain", "weather 1"); f.Button("Rain", "weather 2"); f.Button("Heavy rain", "weather 3");
		f.Button("Light snow", "weather 4"); f.Button("Snow", "weather 5"); f.Button("Heavy snow", "weather 6");
		f.Gap(); f.Head("MAP");
		f.Button("Kill all monsters", "killall"); f.Button("Clear items on the ground", "clearmap"); f.Button("Unsummon all", "unsummon");
		f.Button("Monster count", "count");
		f.Gap(); f.Head("PLAYERS");
		f.Button("Who is online", "who");
		f.Label("goto <name> jumps to a player, bring <name> brings them to you.", W - 40, kDim);
		f.Gap(); f.Head("CONSOLE");
		f.Button("Help", "help"); f.Button("Clear log", "clear");
		break;
	}
}

} // namespace

void CGame::DrawDialogBox_GmConsole()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_GMCONSOLE];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const bool bTop = dlg.IsTop();

	R.StonePanel(X, Y, X + W, Y + H);
	R.TitleOut(X + 14, Y + 8, "GM CONSOLE", kAccent, 15);
	R.TextOutSized(X + 140, Y + 13, "Ctrl+Tab opens and closes it", kDim, 10);
	R.FillPanel(X + 1, Y + 33, X + W - 1, Y + 34, 0, RGB(255, 255, 255), 26);
	R.Slate(X + 10, Y + kBodyTop, X + W - 10, Y + BodyBottom());

	std::vector<Btn> bs;
	GmButtons(*this, bs);
	for (const Btn & b : bs) {
		const bool bOn = bTop && InRect(b.rc, ms::X, ms::Y);
		switch (b.act) {
		case ACT_HEAD: R.TextOutSized(b.rc.left, b.rc.top, b.label.c_str(), b.col, 10); break;
		case ACT_LABEL: R.TextOutSized(b.rc.left, b.rc.top + 3, b.label.c_str(), b.col, 11); break;
		case ACT_CLOSE:
			if (bOn) R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 5, RGB(255, 255, 255), 34);
			R.TextOutSized(b.rc.left + 8, b.rc.top + 1, "x", bOn ? kWhite : kGrey, 18);
			break;
		case ACT_INPUT: {
			const bool bActive = m_bInputStatus && m_pInputBuffer == m_cGmInput;
			R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 4, RGB(0, 0, 0), 170, bActive ? kAccent : RGB(255, 255, 255), bActive ? 200 : (bOn ? 90 : 50));
			if (bActive) { m_iInputX = b.rc.left + 8; m_iInputY = b.rc.top + 5; m_iInputX2 = b.rc.right - 8; }	// follows the window
			else R.TextOutSized(b.rc.left + 8, b.rc.top + 5, m_cGmInput[0] ? m_cGmInput : "click here to type a command (help lists them)", m_cGmInput[0] ? kWhite : kDim, 11);
			break; }
		case ACT_TAB:
			R.StoneButton(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, b.on || bOn ? 1 : 0);
			R.TitleCentered(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, b.label.c_str(), b.on ? RGB(255, 242, 199) : kText, 12);
			break;
		default: {
			if (b.act == ACT_SEARCH && m_bInputStatus && m_pInputBuffer == m_cGmSearch) {
				R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 4, RGB(0, 0, 0), 170, kAccent, 200);
				m_iInputX = b.rc.left + 8; m_iInputY = b.rc.top + 4; m_iInputX2 = b.rc.right - 8;
				break;
			}
			if (b.on) R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 4, RGB(40, 110, 150), 220, kAccent, 230);
			else R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 4, bOn ? RGB(45, 62, 78) : RGB(30, 33, 40), 230, bOn ? kAccent : RGB(80, 86, 96), 200);
			const int tw = R.TextWidthSized(b.label.c_str(), 11);
			R.TextOutSized((b.rc.left + b.rc.right - tw) / 2, b.rc.top + 4, b.label.c_str(), b.act == ACT_SEARCH && !m_cGmSearch[0] ? kDim : kWhite, 11);
			break; }
		}
	}

	// the log: newest at the bottom
	const int ly0 = Y + BodyBottom() + 6, ly1 = Y + H - kInputH - 16;
	R.FillPanel(X + 10, ly0, X + W - 10, ly1, 4, RGB(0, 0, 0), 140, RGB(255, 255, 255), 30);
	const int lines = (ly1 - ly0 - 6) / 14;
	const int n = (int)m_gmLog.size();
	for (int i = 0; i < lines && i < n; i++) {
		const int k = n - 1 - i;
		R.TextOutSized(X + 18, ly1 - 17 - i * 14, m_gmLog[k].c_str(), m_gmLogCol[k], 10);
	}
}

void CGame::DlgBoxClick_GmConsole()
{
	std::vector<Btn> bs;
	GmButtons(*this, bs);
	CDialogBox & dlg = m_dialogBoxes[DIALOG_GMCONSOLE];
	for (const Btn & b : bs) {
		if (!InRect(b.rc, ms::X, ms::Y)) continue;
		switch (b.act) {
		case ACT_CLOSE: ToggleGmConsole(); PlaySound('E', 14, 5); return;
		case ACT_TAB: m_iGmTab = b.arg; m_iGmPage = 0; PlaySound('E', 14, 5); return;
		case ACT_PAGE: m_iGmPage = (std::max)(0, m_iGmPage + b.arg); PlaySound('E', 14, 5); return;
		case ACT_QTY: m_iGmQty = b.arg; PlaySound('E', 14, 5); return;
		case ACT_SEARCH:
			if (m_bInputStatus) EndInputString();
			StartInputString(b.rc.left + 8, b.rc.top + 4, sizeof(m_cGmSearch), m_cGmSearch, FALSE, b.rc.right - 8);
			m_iGmPage = 0;
			return;
		case ACT_INPUT:
			if (m_bInputStatus && m_pInputBuffer == m_cGmInput) return;
			if (m_bInputStatus) EndInputString();
			StartInputString(dlg.m_X + 22, dlg.m_Y + H - kInputH - 8, sizeof(m_cGmInput), m_cGmInput, FALSE, dlg.m_X + W - 22);
			return;
		case ACT_RUN:
			if (b.cmd.empty()) return;
			PlaySound('E', 14, 5);
			GmLog("> " + b.cmd, kGrey);
			GmRun(b.cmd.c_str());
			return;
		default: return;
		}
	}
}
