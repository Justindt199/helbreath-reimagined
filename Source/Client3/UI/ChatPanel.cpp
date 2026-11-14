// ChatPanel.cpp: the chat window (F9) and the speech bubbles over characters' heads.
//
// The window is drawn in code into the back buffer (and at twice the size when HD is on), so it
// layers with the other windows and the mouse cursor like any dialog:
//   - a see-through rounded panel that fades back when nothing has been said for a while
//   - tabs (All, Local, Guild, Party, Whispers, System) with unread counts
//   - every line: optional time, a coloured channel tag, the speaker's name, the message
//   - the line being typed, with the channel it goes to (Tab / Shift+Tab switches it)
// Speech bubbles: a rounded box with a pointer over the speaker, tinted for guild and party,
// with the speaker's previous line faded above it when they talk quickly.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>
#include <string>
#include <vector>

extern DWORD G_dwGlobalTime;

namespace {

struct ChanStyle
{
	const char * tag;
	COLORREF tagBg, tagFg, name, msg;
};

ChanStyle StyleOf(int ch)
{
	switch (ch) {
	case CHAT_NORMAL:		return { "Local",   RGB(51, 54, 59),  RGB(214, 214, 214), RGB(255, 255, 255), RGB(230, 230, 230) };
	case CHAT_GUILD:		return { "Guild",   RGB(31, 74, 40),  RGB(159, 224, 168), RGB(191, 240, 198), RGB(201, 239, 206) };
	case CHAT_SHOUT:		return { "Shout",   RGB(90, 31, 31),  RGB(255, 154, 154), RGB(255, 176, 176), RGB(255, 201, 201) };
	case CHAT_NATIONSHOUT:	return { "Nation",  RGB(29, 44, 85),  RGB(169, 184, 255), RGB(195, 206, 255), RGB(207, 216, 255) };
	case CHAT_PARTY:		return { "Party",   RGB(74, 69, 21),  RGB(239, 228, 138), RGB(246, 239, 171), RGB(243, 237, 192) };
	case CHAT_WHISPER:		return { "Whisper", RGB(61, 37, 87),  RGB(212, 173, 255), RGB(226, 198, 255), RGB(230, 212, 255) };
	case CHAT_GM:			return { "GM",      RGB(90, 58, 16),  RGB(255, 197, 107), RGB(255, 212, 138), RGB(255, 225, 173) };
	default:				return { "System",  RGB(42, 48, 56),  RGB(159, 180, 200), RGB(159, 180, 200), RGB(159, 180, 200) };
	}
}

const char * const kTabNames[CGame::CHATTAB_COUNT] = { "All", "Local", "Guild", "Party", "Whispers", "System" };
const COLORREF kMyName = RGB(242, 207, 115);
const COLORREF kGold = RGB(232, 192, 112);

int TextW(CRenderer & r, const char * s) { return (s == NULL || s[0] == 0) ? 0 : (int)r.GetTextExtent(s).cx; }

// Draws text twice, one pixel apart, for a bold look (the game has one font weight).
void BoldText(CRenderer & r, int x, int y, const char * s, COLORREF c)
{
	r.TextOut(x, y, s, c);
	r.TextOut(x + 1, y, s, c);
}

// Splits a message into lines: the first fits iFirstW pixels, the others iRestW. Breaks at
// spaces; a word too long for a line is cut where it reaches the edge.
void WrapText(CRenderer & r, const std::string & s, int iFirstW, int iRestW, std::vector<std::string> & out)
{
	out.clear();
	std::string line;
	size_t i = 0;
	while (i < s.size()) {
		size_t j = s.find(' ', i);
		if (j == std::string::npos) j = s.size();
		const std::string word = s.substr(i, j - i);
		const int iMax = out.empty() ? iFirstW : iRestW;
		std::string cand = line.empty() ? word : line + " " + word;
		if (TextW(r, cand.c_str()) <= iMax) line = cand;
		else if (!line.empty() && TextW(r, word.c_str()) <= iRestW) {
			out.push_back(line);
			line = word;
		}
		else {	// cut the word up
			std::string piece = line.empty() ? std::string() : line + " ";
			for (size_t k = 0; k < word.size(); k++) {
				const int iLim = out.empty() ? iFirstW : iRestW;
				std::string next = piece + word[k];
				if (TextW(r, next.c_str()) > iLim && !piece.empty()) {
					out.push_back(piece);
					piece = std::string(1, word[k]);
				}
				else piece = next;
			}
			line = piece;
		}
		i = j + 1;
	}
	if (!line.empty() || out.empty()) out.push_back(line);
}

COLORREF Mix(COLORREF a, COLORREF b, float t)
{
	return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
		(int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
		(int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

// The channel a typed line goes to, from its first character (as the server reads it).
int TypedChannel(const char * buf, bool bWhisperMode)
{
	switch (buf[0]) {
	case '!': return CHAT_SHOUT;
	case '~': return CHAT_NATIONSHOUT;
	case '@': case '^': return CHAT_GUILD;
	case '$': return CHAT_PARTY;
	case '%': return CHAT_GM;
	case '/': return -1;		// a command
	}
	return bWhisperMode ? CHAT_WHISPER : CHAT_NORMAL;
}

bool IsChannelPrefix(char c) { return c == '!' || c == '~' || c == '@' || c == '^' || c == '$' || c == '%'; }

// Item links in display text: \x01, then (\x10 + link index), then the link's text, then \x02.
// The markers are control characters, which the text renderer neither draws nor measures, so the
// marked-up text wraps and measures like the plain text. Spaces inside a link are no-break
// spaces so a link is never split across lines.
const char kLinkStart = '\x01', kLinkEnd = '\x02', kLinkIdx0 = '\x10';

// Draws marked-up text; links in their colour (underlined when the mouse is on them).
// Returns the link under the mouse, if any.
const CGame::ChatLink * DrawRich(CRenderer & r, int x, int y, const std::string & s, COLORREF base,
	const std::vector<CGame::ChatLink> * pLinks, bool bCheckHover)
{
	const CGame::ChatLink * pHover = NULL;
	size_t i = 0;
	int cx = x;
	while (i < s.size()) {
		const CGame::ChatLink * pLink = NULL;
		size_t j;
		if (s[i] == kLinkStart && i + 1 < s.size()) {
			const int idx = (unsigned char)s[i + 1] - (unsigned char)kLinkIdx0;
			if (pLinks != NULL && idx >= 0 && idx < (int)pLinks->size()) pLink = &(*pLinks)[idx];
			i += 2;
			j = s.find(kLinkEnd, i);
			if (j == std::string::npos) j = s.size();
		}
		else {
			j = s.find(kLinkStart, i + 1);
			if (j == std::string::npos) j = s.size();
		}
		std::string run = s.substr(i, j - i);
		if (!run.empty()) {
			const int w = TextW(r, run.c_str());
			if (pLink != NULL && pLink->bSpan) r.TextOut(cx, y, run.c_str(), pLink->color);
			else if (pLink != NULL) {
				const bool bOn = bCheckHover && ms::X >= cx && ms::X < cx + w && ms::Y >= y && ms::Y < y + 15;
				r.TextOut(cx, y, run.c_str(), pLink->color);
				if (bOn) {
					r.FillPanel(cx, y + 14, cx + w, y + 15, 0, pLink->color, 220);
					pHover = pLink;
				}
			}
			else r.TextOut(cx, y, run.c_str(), base);
			cx += w;
		}
		i = (j < s.size() && s[j] == kLinkEnd) ? j + 1 : j;
	}
	return pHover;
}

int HexVal(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

} // namespace

//////////////////////////////////////////////////////////////////////
// Item links

// p points at '{'. A link is {Name}, {Name|attr}, {Name|attr|sockets} or {Name|attr|sockets|picture}:
// attr up to 8 hex digits; sockets 2 hex digits per socket, or empty; picture = sprite.frame.colour
// in hex (so everyone can draw the item, even without owning one). On success *pLen is its length.
bool CGame::ParseChatLink(const char * p, size_t * pLen, std::string * pName, DWORD * pAttr, uint8 * pSockets,
	int * pSprite, int * pFrame, int * pColor)
{
	if (p == NULL || p[0] != '{') return false;
	size_t i = 1;
	std::string name;
	while (p[i] != 0 && p[i] != '|' && p[i] != '}') {
		const unsigned char c = (unsigned char)p[i];
		if (c < 33 || c > 126 || c == '{') return false;
		name += (char)c;
		if (name.size() > 20) return false;
		i++;
	}
	if (name.empty()) return false;
	DWORD attr = 0;
	uint8 sockets[MAXITEMSOCKETS];
	for (int k = 0; k < MAXITEMSOCKETS; k++) sockets[k] = SG_NONE;	// left out = no gems
	if (p[i] == '|') {
		i++;
		int n = 0;
		while (HexVal(p[i]) >= 0) { attr = (attr << 4) | (DWORD)HexVal(p[i]); i++; if (++n > 8) return false; }
		if (n == 0) return false;
		if (p[i] == '|') {
			i++;
			if (p[i] != '|' && p[i] != '}') {	// sockets (may be left empty)
				for (int k = 0; k < MAXITEMSOCKETS; k++) {
					const int hi = HexVal(p[i]), lo = (hi >= 0) ? HexVal(p[i + 1]) : -1;
					if (hi < 0 || lo < 0) return false;
					sockets[k] = (uint8)(hi * 16 + lo);
					i += 2;
				}
			}
			if (p[i] == '|') {	// picture: sprite.frame.colour
				i++;
				int v[3] = { 0, 0, 0 };
				for (int f = 0; f < 3; f++) {
					int n = 0;
					while (HexVal(p[i]) >= 0) { v[f] = v[f] * 16 + HexVal(p[i]); i++; if (++n > 4) return false; }
					if (n == 0) return false;
					if (f < 2) { if (p[i] != '.') return false; i++; }
				}
				if (pSprite) *pSprite = v[0];
				if (pFrame) *pFrame = v[1];
				if (pColor) *pColor = v[2];
			}
		}
	}
	if (p[i] != '}') return false;
	if (pLen) *pLen = i + 1;
	if (pName) *pName = name;
	if (pAttr) *pAttr = attr;
	if (pSockets) memcpy(pSockets, sockets, sizeof(sockets));
	return true;
}

// A coloured run in a system line: \x03, the colour as 6 hex digits (RRGGBB), the text, \x04.
// Spaces in it become no-break spaces so the run stays on one line.
std::string CGame::ChatSpan(COLORREF c, const std::string & s)
{
	char cHex[8];
	wsprintf(cHex, "%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
	std::string out = "\x03";
	out += cHex;
	for (size_t i = 0; i < s.size(); i++) out += (s[i] == ' ') ? '\xA0' : s[i];
	out += "\x04";
	return out;
}

// A system message with ChatSpan colours: coloured in the chat panel, plain in the old list.
void CGame::AddColoredEvent(const std::string & sMarked)
{
	std::string plain;
	for (size_t i = 0; i < sMarked.size(); i++) {
		if (sMarked[i] == '\x03') { i += 6; continue; }
		if (sMarked[i] == '\x04') continue;
		plain += (sMarked[i] == '\xA0') ? ' ' : sMarked[i];
	}
	const bool bWas = m_bChatEcho;
	m_bChatEcho = true;			// the chat panel gets the coloured line below instead
	AddEventList(plain.c_str(), 10);
	m_bChatEcho = bWas;
	if (m_cGameMode == GAMEMODE_ONMAINGAME) AddChatLine(CHAT_SYSTEM, NULL, sMarked.c_str());
}

std::string CGame::ChatRich(const char * pRaw, std::vector<ChatLink> & links, bool bColor)
{
	links.clear();
	std::string out;
	if (pRaw == NULL) return out;
	for (size_t i = 0; pRaw[i] != 0; ) {
		size_t len = 0;
		ChatLink link;
		if (bColor && pRaw[i] == '\x03' && links.size() < 16) {	// a ChatSpan
			int v[6], k = 0;
			for (; k < 6 && pRaw[i + 1 + k]; k++) if ((v[k] = HexVal(pRaw[i + 1 + k])) < 0) break;
			const char * pEnd = (k == 6) ? strchr(pRaw + i + 7, '\x04') : NULL;
			if (pEnd != NULL) {
				link.bSpan = true;
				link.color = RGB(v[0] * 16 + v[1], v[2] * 16 + v[3], v[4] * 16 + v[5]);
				out += kLinkStart;
				out += (char)(kLinkIdx0 + (char)links.size());
				for (const char * q = pRaw + i + 7; q < pEnd; q++) if ((unsigned char)*q >= 32) out += *q;
				out += kLinkEnd;
				links.push_back(link);
				i = (size_t)(pEnd - pRaw) + 1;
				continue;
			}
		}
		if (pRaw[i] == '{' && links.size() < CHATLINK_MAX
			&& ParseChatLink(pRaw + i, &len, &link.name, &link.attr, link.sockets, &link.sprite, &link.frame, &link.itemColor)) {
			char cName[24] = {};
			strncpy(cName, link.name.c_str(), 20);
			Vector<string> * pLines = GetItemName(cName, link.attr, link.sockets, 1);
			link.color = m_itemColor;
			if (pLines != NULL) for (size_t k = 0; k < pLines->size(); k++) link.tip.push_back((*pLines)[k]);
			if (link.tip.empty()) link.tip.push_back(link.name);
			std::string shown = "[" + link.tip[0] + "]";
			for (size_t k = 0; k < shown.size(); k++) if (shown[k] == ' ') shown[k] = '\xA0';
			out += kLinkStart;
			out += (char)(kLinkIdx0 + (char)links.size());
			out += shown;
			out += kLinkEnd;
			links.push_back(link);
			i += len;
		}
		else {
			const char c = pRaw[i++];
			if ((unsigned char)c >= 32 || c == '\t') out += c;	// no stray control characters
		}
	}
	return out;
}

std::string CGame::ChatPlainText(const char * pRaw)
{
	std::vector<ChatLink> links;
	std::string s = ChatRich(pRaw, links), out;
	for (size_t i = 0; i < s.size(); i++) {
		const unsigned char c = (unsigned char)s[i];
		if (c == (unsigned char)kLinkStart) { i++; continue; }	// skip the index too
		if (c == (unsigned char)kLinkEnd) continue;
		out += (c == 0xA0) ? ' ' : (char)c;
	}
	return out;
}

bool CGame::TryLinkItemToChat(int iItem)
{
	// only while typing a chat line, so Shift+drag in the bag (moves every item of that kind) still works
	if (!m_bShiftPressed || m_cGameMode != GAMEMODE_ONMAINGAME) return false;
	if (!m_bInputStatus || m_pInputBuffer != m_cChatMsg) return false;
	if (iItem < 0 || iItem >= MAXITEMS || m_pItemList[iItem] == NULL) return false;
	CItem * pItem = m_pItemList[iItem];

	std::string tok = "{" + std::string(pItem->m_cName, strnlen(pItem->m_cName, 20));
	bool bSockets = false;
	for (int k = 0; k < MAXITEMSOCKETS; k++)
		if (pItem->m_sockets[k] != 0 && pItem->m_sockets[k] != SG_NONE) bSockets = true;	// only when it has gems
	char cBuf[32];
	wsprintf(cBuf, "|%X|", pItem->m_dwAttribute);
	tok += cBuf;
	if (bSockets)
		for (int k = 0; k < MAXITEMSOCKETS; k++) { wsprintf(cBuf, "%02X", pItem->m_sockets[k]); tok += cBuf; }
	wsprintf(cBuf, "|%X.%X.%X}", (int)pItem->m_sSprite & 0xFFFF, (int)pItem->m_sSpriteFrame & 0xFFFF, (int)pItem->m_cItemColor & 0xF);
	tok += cBuf;
	if (!ParseChatLink(tok.c_str(), NULL, NULL, NULL, NULL)) return false;	// a name we can't send

	std::string cur = m_cChatMsg;
	std::string add = (!cur.empty() && cur.back() != ' ' && !(cur.size() == 1 && IsChannelPrefix(cur[0]))) ? " " + tok : tok;
	if (cur.size() + add.size() >= sizeof(m_cChatMsg)) {
		AddEventList("There is no room left in your message for that item.", 10);
		return true;
	}
	strcat(m_cChatMsg, add.c_str());
	PlaySound('E', 14, 5);
	return true;
}

void CGame::FilterChatCurses(char * pBuf, size_t iBufSize)
{
	if (pBuf == NULL || iBufSize == 0) return;
	std::string out, seg;
	auto flush = [&]() {
		if (seg.empty()) return;
		char cTmp[200] = {};
		strncpy(cTmp, seg.c_str(), sizeof(cTmp) - 1);
		m_curse.ConvertString(cTmp, (int)strlen(cTmp));
		out += cTmp;
		seg.clear();
	};
	for (size_t i = 0; pBuf[i] != 0; ) {
		size_t len = 0;
		if (pBuf[i] == '{' && ParseChatLink(pBuf + i, &len, NULL, NULL, NULL)) {
			flush();
			out.append(pBuf + i, len);
			i += len;
		}
		else seg += pBuf[i++];
	}
	flush();
	if (out.size() >= iBufSize) out.resize(iBufSize - 1);
	memcpy(pBuf, out.c_str(), out.size() + 1);
}

bool CGame::ChatBackspaceLink()
{
	if (m_pInputBuffer != m_cChatMsg) return false;
	const size_t len = strlen(m_cChatMsg);
	if (len == 0 || m_cChatMsg[len - 1] != '}') return false;
	for (size_t i = len - 1; i-- > 0; ) {
		if (m_cChatMsg[i] != '{') continue;
		size_t l = 0;
		if (ParseChatLink(m_cChatMsg + i, &l, NULL, NULL, NULL) && i + l == len) {
			m_cChatMsg[i] = 0;
			return true;
		}
		return false;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////
// Chat lines

void CGame::AddChatLine(int ch, const char * pName, const char * pText, bool bTo)
{
	if (pText == NULL) return;
	ChatLine line;
	line.ch = ch;
	line.name = (pName != NULL) ? std::string(pName, strnlen(pName, 10)) : std::string();
	while (*pText == ' ') pText++;
	line.text = ChatRich(pText, line.links, ch == CHAT_SYSTEM);
	if (line.text.empty()) return;
	line.bTo = bTo;
	line.bMine = !bTo && !line.name.empty() && strncmp(line.name.c_str(), m_cPlayerName, 10) == 0;
	SYSTEMTIME st;
	GetLocalTime(&st);
	wsprintf(line.cTime, "%02d:%02d", st.wHour, st.wMinute);

	m_chatLines.push_back(line);
	if (m_chatLines.size() > CHATPANEL_MAXLINES) m_chatLines.pop_front();

	CDialogBox & dlg = m_dialogBoxes[DIALOG_CHAT];
	if (dlg.sView > 0 && ChatLineInTab(line, m_iChatTab)) dlg.sView++;	// keep the reader's place
	for (int t = CHATTAB_LOCAL; t < CHATTAB_COUNT; t++)
		if (t != m_iChatTab && t != CHATTAB_LOCAL && t != CHATTAB_SYSTEM && ChatLineInTab(line, t) && m_iChatUnread[t] < 99)
			m_iChatUnread[t]++;
	m_dwChatLastLine = timeGetTime();
}

bool CGame::ChatLineInTab(const ChatLine & line, int iTab) const
{
	switch (iTab) {
	case CHATTAB_LOCAL:		return line.ch == CHAT_NORMAL;
	case CHATTAB_GUILD:		return line.ch == CHAT_GUILD;
	case CHATTAB_PARTY:		return line.ch == CHAT_PARTY;
	case CHATTAB_WHISPER:	return line.ch == CHAT_WHISPER;
	case CHATTAB_SYSTEM:	return line.ch == CHAT_SYSTEM;
	default:				return true;
	}
}

bool CGame::IsChatPanelInput() const
{
	return m_bInputStatus && m_pInputBuffer == m_cChatMsg && m_bIsDialogEnabled[DIALOG_CHAT];
}

// Tab while typing: Local -> Guild -> Party -> Shout -> Nation -> Local (Shift+Tab goes back).
void CGame::CycleChatChannel(bool bBack)
{
	static const char kOrder[] = { 0, '@', '$', '!', '~' };
	const int n = (int)sizeof(kOrder);
	int cur = 0;
	const char c0 = m_cChatMsg[0];
	for (int i = 1; i < n; i++) if (kOrder[i] == c0 || (c0 == '^' && kOrder[i] == '@')) cur = i;
	const int next = (cur + (bBack ? n - 1 : 1)) % n;

	std::string body = m_cChatMsg;
	if (!body.empty() && IsChannelPrefix(body[0])) body.erase(0, 1);
	std::string out = (kOrder[next] != 0) ? std::string(1, kOrder[next]) + body : body;
	if (out.size() >= sizeof(m_cChatMsg)) out.resize(sizeof(m_cChatMsg) - 1);
	ZeroMemory(m_cChatMsg, sizeof(m_cChatMsg));
	memcpy(m_cChatMsg, out.c_str(), out.size());
}

const char * CGame::ChatRowNameAt(short msX, short msY) const
{
	const CDialogBox & dlg = m_dialogBoxes[DIALOG_CHAT];
	if (!m_bIsDialogEnabled[DIALOG_CHAT]) return NULL;
	if (msX < dlg.m_X || msX >= dlg.m_X + CHATPANEL_W) return NULL;
	for (size_t i = 0; i < m_chatRows.size(); i++)
		if (msY >= m_chatRows[i].y0 && msY < m_chatRows[i].y1)
			return m_chatRows[i].name.empty() ? NULL : m_chatRows[i].name.c_str();
	return NULL;
}

//////////////////////////////////////////////////////////////////////
// The window

void CGame::DrawDialogBox_Chat()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_CHAT];
	CRenderer & R = m_Renderer;
	const int X = dlg.m_X, Y = dlg.m_Y, W = CHATPANEL_W, H = CHATPANEL_H;
	const DWORD dwNow = timeGetTime();
	const bool bTyping = IsChatPanelInput();
	const bool bOver = (ms::X >= X && ms::X < X + W && ms::Y >= Y && ms::Y < Y + H);

	// how many lines the current tab has; mouse wheel scrolls by lines
	int iCount = 0;
	for (size_t i = 0; i < m_chatLines.size(); i++) if (ChatLineInTab(m_chatLines[i], m_iChatTab)) iCount++;
	if (ms::Z != 0 && (bOver || dlg.IsTop())) {
		dlg.sView += ms::Z / 30;
		m_Mouse.m_sZ = 0;
	}
	if (dlg.sView > iCount - 1) dlg.sView = (short)(iCount > 0 ? iCount - 1 : 0);
	if (dlg.sView < 0) dlg.sView = 0;

	// the panel fades back after 10 s of quiet while the mouse is elsewhere
	float f = 1.f;
	const DWORD dwIdle = dwNow - m_dwChatLastLine;
	if (!bTyping && !bOver && dwIdle > 10000) f = (std::max)(0.4f, 1.f - (float)(dwIdle - 10000) / 1000.f * 0.6f);
	R.FillPanel(X, Y, X + W, Y + H, 6, RGB(10, 12, 16), (int)(190 * f), RGB(255, 255, 255), (int)(32 * f));

	// tabs
	int tx = X + 5;
	for (int t = 0; t < CHATTAB_COUNT; t++) {
		const char * pName = kTabNames[t];
		char cBadge[8] = "";
		if (m_iChatUnread[t] > 0) wsprintf(cBadge, "%d", m_iChatUnread[t]);
		const int iNameW = TextW(R, pName) + 1;
		const int iBadgeW = cBadge[0] ? TextW(R, cBadge) + 10 : 0;
		const int iTabW = 12 + iNameW + (iBadgeW ? iBadgeW + 5 : 0);
		const bool bActive = (t == m_iChatTab);
		const bool bHover = bOver && ms::X >= tx && ms::X < tx + iTabW && ms::Y >= Y + 3 && ms::Y < Y + 21;
		if (bActive) {
			R.FillPanel(tx, Y + 3, tx + iTabW, Y + 21, 4, RGB(255, 255, 255), 28);
			R.FillPanel(tx + 1, Y + 19, tx + iTabW - 1, Y + 21, 0, kGold, 255);
			BoldText(R, tx + 6, Y + 4, pName, RGB(255, 255, 255));
		}
		else R.TextOut(tx + 6, Y + 4, pName, bHover ? RGB(235, 238, 242) : RGB(169, 176, 186));
		if (iBadgeW) {
			COLORREF bc = RGB(217, 83, 79);
			if (t == CHATTAB_GUILD) bc = RGB(63, 143, 79);
			else if (t == CHATTAB_WHISPER) bc = RGB(154, 95, 214);
			else if (t == CHATTAB_PARTY) bc = RGB(166, 142, 36);
			const int bx = tx + 6 + iNameW + 5;
			R.FillPanel(bx, Y + 5, bx + iBadgeW, Y + 19, 7, bc, 255);
			R.TextOut(bx + 5, Y + 4, cBadge, RGB(255, 255, 255));
		}
		m_chatTabX0[t] = (short)(tx - X);
		m_chatTabX1[t] = (short)(tx + iTabW - X);
		tx += iTabW + 2;
	}
	R.FillPanel(X + 4, Y + 21, X + W - 4, Y + 22, 0, RGB(255, 255, 255), (int)(28 * f));

	// lines: newest at the bottom; a long message wraps under its name
	const int iTop = Y + 25, iRight = X + W - 12;
	struct VRow { const ChatLine * pLine; std::string s; int x; bool bFirst; int xTag, xName, xMsg; };
	std::vector<VRow> rows;		// bottom-up
	int iSkip = dlg.sView;
	std::vector<std::string> parts;
	for (auto it = m_chatLines.rbegin(); it != m_chatLines.rend() && (int)rows.size() < CHATPANEL_ROWS; ++it) {
		const ChatLine & L = *it;
		if (!ChatLineInTab(L, m_iChatTab)) continue;
		if (iSkip > 0) { iSkip--; continue; }
		const ChanStyle st = StyleOf(L.ch);
		const int xTag = X + 8 + (m_showTimeStamp ? 38 : 0);
		const int xName = xTag + TextW(R, st.tag) + 8 + 5;
		std::string label;
		if (L.ch == CHAT_WHISPER) label = (L.bTo ? "to " : "from ") + L.name + ":";
		else if (!L.name.empty()) label = L.name + ":";
		const int xMsg = label.empty() ? xName : xName + TextW(R, label.c_str()) + 1 + 5;
		WrapText(R, L.text, iRight - xMsg, iRight - xName, parts);
		for (int k = (int)parts.size() - 1; k >= 0 && (int)rows.size() < CHATPANEL_ROWS; k--)
			rows.push_back({ &L, parts[k], k == 0 ? xMsg : xName, k == 0, xTag, xName, xMsg });
	}

	m_chatRows.clear();
	const ChatLink * pHoverLink = NULL;
	std::string sHoverFrom;		// who linked it
	for (size_t k = 0; k < rows.size(); k++) {
		const VRow & row = rows[k];
		const ChatLine & L = *row.pLine;
		const ChanStyle st = StyleOf(L.ch);
		const int y = iTop + (CHATPANEL_ROWS - 1 - (int)k) * CHATPANEL_ROWH;
		if (row.bFirst) {
			if (m_showTimeStamp) R.TextOut(X + 8, y, L.cTime, RGB(111, 119, 130));
			const int iTagW = TextW(R, st.tag) + 8;
			R.FillPanel(row.xTag, y + 1, row.xTag + iTagW, y + 15, 3, st.tagBg, 255);
			R.TextOut(row.xTag + 4, y, st.tag, st.tagFg);
			if (L.ch == CHAT_WHISPER) {
				std::string label = (L.bTo ? "to " : "from ") + L.name + ":";
				BoldText(R, row.xName, y, label.c_str(), st.name);
			}
			else if (!L.name.empty()) {
				std::string label = L.name + ":";
				BoldText(R, row.xName, y, label.c_str(), L.bMine ? kMyName : st.name);
			}
		}
		const ChatLink * pOn = DrawRich(R, row.x, y, row.s, st.msg, &L.links, bOver && dlg.IsTop());
		if (pOn != NULL) { pHoverLink = pOn; sHoverFrom = L.name; }
		if (L.ch != CHAT_SYSTEM) m_chatRows.push_back({ (short)y, (short)(y + CHATPANEL_ROWH), L.name });
	}

	// scroll position and the button back to the newest line
	m_chatNewestX0 = m_chatNewestX1 = 0;
	const int iLogH = CHATPANEL_ROWS * CHATPANEL_ROWH;
	if (iCount > CHATPANEL_ROWS) {
		R.FillPanel(X + W - 6, iTop, X + W - 4, iTop + iLogH, 1, RGB(255, 255, 255), (int)(22 * f));
		const int iThumbH = (std::max)(12, iLogH * CHATPANEL_ROWS / iCount);
		const int iThumbY = iTop + (iLogH - iThumbH) - (int)((long long)(iLogH - iThumbH) * dlg.sView / (std::max)(1, iCount - 1));
		R.FillPanel(X + W - 6, iThumbY, X + W - 4, iThumbY + iThumbH, 1, RGB(200, 205, 212), (int)(150 * f));
	}
	if (dlg.sView > 0) {
		const char * pNewest = "Newest";
		const int bw = TextW(R, pNewest) + 14;
		const int bx = X + W - 14 - bw, by = iTop + iLogH - 17;
		R.FillPanel(bx, by, bx + bw, by + 16, 8, RGB(232, 192, 112), 235);
		R.TextOut(bx + 7, by, pNewest, RGB(26, 20, 8));
		m_chatNewestX0 = (short)(bx - X); m_chatNewestX1 = (short)(bx + bw - X);
		m_chatNewestY0 = (short)(by - Y); m_chatNewestY1 = (short)(by + 16 - Y);
	}

	// the line being typed
	const int iy = Y + H - 24;
	if (bTyping) {
		R.FillPanel(X + 6, iy, X + W - 6, iy + 20, 5, RGB(0, 0, 0), 120, kGold, 150);
		const int ch = TypedChannel(m_cChatMsg, m_bWhisperModeOn);
		std::string chip;
		const char * pBody = m_cChatMsg;
		ChanStyle st = StyleOf(ch < 0 ? CHAT_SYSTEM : ch);
		if (ch < 0) chip = "Command";
		else if (ch == CHAT_WHISPER) chip = std::string("To ") + std::string(m_cWhisperName, strnlen(m_cWhisperName, 10));
		else chip = st.tag;
		if (IsChannelPrefix(m_cChatMsg[0])) pBody = m_cChatMsg + 1;
		while (*pBody == ' ') pBody++;
		const int iChipW = TextW(R, chip.c_str()) + 10;
		R.FillPanel(X + 10, iy + 3, X + 10 + iChipW, iy + 17, 3, st.tagBg, 255);
		R.TextOut(X + 15, iy + 2, chip.c_str(), st.tagFg);
		const int xText = X + 10 + iChipW + 7;
		const int iAvail = X + W - 14 - xText;
		std::vector<ChatLink> typedLinks;
		std::string shown = ChatRich(pBody, typedLinks);
		while (!shown.empty() && TextW(R, shown.c_str()) > iAvail - 4) shown.erase(0, 1);
		const int iTextW = TextW(R, shown.c_str());
		DrawRich(R, xText, iy + 2, shown, RGB(255, 255, 255), &typedLinks, false);
		if (((G_dwGlobalTime / 450) % 2) == 0)
			R.FillPanel(xText + iTextW + 1, iy + 4, xText + iTextW + 2, iy + 16, 0, RGB(255, 255, 255), 230);
		const char * pHint = "Shift+click item: link   Tab: channel";
		if (xText + iTextW + 16 >= X + W - 12 - TextW(R, pHint)) pHint = "Tab: channel";
		const int iHintW = TextW(R, pHint);
		if (xText + iTextW + 16 < X + W - 12 - iHintW)
			R.TextOut(X + W - 12 - iHintW, iy + 2, pHint, RGB(113, 121, 133));
	}
	else {
		R.FillPanel(X + 6, iy, X + W - 6, iy + 20, 5, RGB(0, 0, 0), (int)(70 * f), RGB(255, 255, 255), (int)(18 * f));
		R.TextOut(X + 12, iy + 2, "Press Enter to chat", RGB(110, 118, 128));
	}

	if (pHoverLink != NULL) {	// the shared item tooltip (UI\ItemTooltip.cpp), above the line
		ItemTip tip;
		BuildItemTip(tip, NULL, pHoverLink->name.c_str(), pHoverLink->attr, pHoverLink->sockets, 1);
		if (pHoverLink->sprite >= 0) {	// the picture the link carries
			tip.sprite = pHoverLink->sprite;
			tip.frame = pHoverLink->frame;
			tip.itemColor = (char)pHoverLink->itemColor;
			tip.weaponTint = (tip.sprite == 1 || tip.sprite == 2 || tip.sprite == 3 || tip.sprite == 15 || tip.sprite == 17);
		}
		tip.showIcon = true;
		DrawItemTip(tip, ms::X, ms::Y, true);
	}
}

void CGame::DlgBoxClick_Chat()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_CHAT];
	const int mx = ms::X - dlg.m_X, my = ms::Y - dlg.m_Y;
	if (my >= 3 && my < 21) {
		for (int t = 0; t < CHATTAB_COUNT; t++)
			if (mx >= m_chatTabX0[t] && mx < m_chatTabX1[t]) {
				m_iChatTab = t;
				m_iChatUnread[t] = 0;
				dlg.sView = 0;
				PlaySound('E', 14, 5);
				return;
			}
	}
	if (m_chatNewestX1 > m_chatNewestX0 && mx >= m_chatNewestX0 && mx < m_chatNewestX1 && my >= m_chatNewestY0 && my < m_chatNewestY1)
		dlg.sView = 0;
}

//////////////////////////////////////////////////////////////////////
// Speech bubbles

void CGame::DrawChatBubble(short sX, short sY, int iChatIndex, int iLoc)
{
	CMsg * pMsg = m_pChatMsgList[iChatIndex];
	if (pMsg == NULL) return;
	CRenderer & R = m_Renderer;
	const int ch = pMsg->m_cChannel;
	const bool bTinted = (ch == CHAT_GUILD || ch == CHAT_PARTY);
	const ChanStyle st = StyleOf(ch);
	const int kMaxW = 160, kLH = CHATPANEL_ROWH;

	COLORREF fill = RGB(14, 16, 20), border = RGB(255, 255, 255);
	int iFillA = 205, iBorderA = 30;
	if (ch == CHAT_GUILD) { fill = RGB(22, 46, 28); border = RGB(130, 220, 140); iFillA = 220; iBorderA = 140; }
	else if (ch == CHAT_PARTY) { fill = RGB(48, 44, 14); border = RGB(235, 225, 120); iFillA = 220; iBorderA = 140; }

	std::vector<ChatLink> links;
	std::vector<std::string> lines;
	WrapText(R, ChatRich(pMsg->m_pMsg, links), kMaxW, kMaxW, lines);
	if (lines.size() > 4) { lines.resize(4); lines[3] += "..."; }
	std::string header;
	if (bTinted) header = std::string("[") + st.tag + "] " + std::string(pMsg->m_cName, strnlen(pMsg->m_cName, 10));

	int iW = header.empty() ? 0 : TextW(R, header.c_str()) + 1;
	for (size_t i = 0; i < lines.size(); i++) iW = (std::max)(iW, TextW(R, lines[i].c_str()));
	iW += 14;
	const int iH = (int)lines.size() * kLH + (header.empty() ? 0 : kLH) + 7;
	const int iBottom = sY - 58 - iLoc, iTopY = iBottom - iH, iLeft = sX - iW / 2;

	// the line they said just before, faded, above this one
	if (pMsg->m_pPrev != NULL) {
		std::vector<ChatLink> prevLinks;
		std::vector<std::string> prev;
		WrapText(R, ChatRich(pMsg->m_pPrev, prevLinks), kMaxW, kMaxW, prev);
		if (prev.size() > 2) { prev.resize(2); prev[1] += "..."; }
		int pw = 0;
		for (size_t i = 0; i < prev.size(); i++) pw = (std::max)(pw, TextW(R, prev[i].c_str()));
		pw += 14;
		const int ph = (int)prev.size() * kLH + 7, pb = iTopY - 4, pl = sX - pw / 2;
		R.FillPanel(pl, pb - ph, pl + pw, pb, 7, fill, iFillA * 6 / 10, border, iBorderA * 6 / 10);
		for (size_t i = 0; i < prev.size(); i++)
			DrawRich(R, pl + 7, pb - ph + 3 + (int)i * kLH, prev[i], Mix(RGB(241, 241, 241), fill, 0.4f), &prevLinks, false);
	}

	R.FillPanel(iLeft, iTopY, iLeft + iW, iBottom, 7, fill, iFillA, border, iBorderA, 6, sX);
	int y = iTopY + 3;
	if (!header.empty()) {
		BoldText(R, iLeft + 7, y, header.c_str(), st.tagFg);
		y += kLH;
	}
	const COLORREF textC = (pMsg->m_cType == 20) ? RGB(255, 255, 120) : RGB(241, 241, 241);
	for (size_t i = 0; i < lines.size(); i++, y += kLH)
		DrawRich(R, iLeft + 7, y, lines[i], textC, &links, false);
}
