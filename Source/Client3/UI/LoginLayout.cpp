// LoginLayout.cpp: where the pieces of the sign-in screen go and what they say, read from
// CONTENTS\login_layout.cfg (the file explains itself). The dashboard's "Login screen" tab edits that
// file (tools\hb_dashboard); a missing file or line keeps the value below, which is the layout the
// screen had before it could be changed.
//
// Used by UI\LoginScreen.cpp (panel, boxes, buttons, news, version) and UI\LoginScene.cpp (logo, the
// town's name). LoginRects (LoginScreen.cpp) is built from it, so clicks follow whatever moves.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

const char * const kNames[CGame::LM_COUNT] = { "logo", "news", "town", "version", "panel", "account", "password", "remember", "connect", "create", "quit" };
const char * const kAnchors[9] = { "top-left", "top", "top-right", "left", "center", "right", "bottom-left", "bottom", "bottom-right" };

CGame::LoginMod g_mod[CGame::LM_COUNT];
bool g_loaded = false;

void Set(int i, int anchor, int x, int y, int w, int h, const char * t0, const char * t1 = "")
{
	CGame::LoginMod & m = g_mod[i];
	m.anchor = anchor; m.x = x; m.y = y; m.w = w; m.h = h; m.show = true; m.text = t0; m.text2 = t1;
}

std::string Trim(const std::string & s)
{
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
	return s.substr(a, b - a);
}

void Load()
{
	g_loaded = true;
	Set(CGame::LM_LOGO, 0, 26, 20, 0, 0, "H E L B R E A T H", "UNRIVALED");
	Set(CGame::LM_NEWS, 0, 20, 104, 0, 0, "NEWS");
	Set(CGame::LM_TOWN, 6, 30, -24, 0, 0, "");
	Set(CGame::LM_VERSION, 8, -12, -7, 0, 0, "");
	Set(CGame::LM_PANEL, 5, -26, -10, 240, 292, "SIGN IN", "Welcome back, adventurer.");
	Set(CGame::LM_ACCOUNT, 0, 18, 78, 204, 28, "ACCOUNT");
	Set(CGame::LM_PASSWORD, 0, 18, 132, 204, 28, "PASSWORD");
	Set(CGame::LM_REMEMBER, 0, 18, 170, 140, 18, "Remember account");
	Set(CGame::LM_CONNECT, 0, 18, 198, 204, 34, "CONNECT");
	Set(CGame::LM_CREATE, 0, 18, 240, 98, 24, "Create account");
	Set(CGame::LM_QUIT, 0, 124, 240, 98, 24, "Quit");

	FILE * f = fopen("CONTENTS\\login_layout.cfg", "rt");
	if (f == NULL) return;
	char line[256];
	int cur = -1;
	while (fgets(line, sizeof(line), f) != NULL) {
		const std::string s = Trim(line);
		if (s.empty() || s.compare(0, 2, "//") == 0) continue;
		if (s[0] == '[') {
			const std::string name = Trim(s.substr(1, s.find(']') == std::string::npos ? std::string::npos : s.find(']') - 1));
			cur = -1;
			for (int i = 0; i < CGame::LM_COUNT; i++) if (name == kNames[i]) cur = i;
			continue;
		}
		const size_t eq = s.find('=');
		if (cur < 0 || eq == std::string::npos) continue;
		const std::string key = Trim(s.substr(0, eq)), val = Trim(s.substr(eq + 1));
		CGame::LoginMod & m = g_mod[cur];
		if (key == "x") m.x = atoi(val.c_str());
		else if (key == "y") m.y = atoi(val.c_str());
		else if (key == "width") { if (atoi(val.c_str()) > 0) m.w = atoi(val.c_str()); }
		else if (key == "height") { if (atoi(val.c_str()) > 0) m.h = atoi(val.c_str()); }
		else if (key == "show") m.show = !(val == "no" || val == "0");
		else if (key == "text") m.text = val;
		else if (key == "text 2") m.text2 = val;
		else if (key == "anchor") { for (int a = 0; a < 9; a++) if (val == kAnchors[a]) m.anchor = a; }
		else if (key.compare(0, 5, "line ") == 0) { const int n = atoi(key.c_str() + 5); if (n >= 1 && n <= 4) m.line[n - 1] = val; }
	}
	fclose(f);
}

} // namespace

const CGame::LoginMod & CGame::LoginModule(int i)
{
	if (!g_loaded) Load();
	return g_mod[(i >= 0 && i < LM_COUNT) ? i : 0];
}

// The top left corner on the screen of piece i when it is w x h pixels: its anchor point on the
// screen, the same point of the piece, and its x and y from there.
void CGame::LoginPlace(int i, int w, int h, int * px, int * py)
{
	const LoginMod & m = LoginModule(i);
	const int col = m.anchor % 3, row = m.anchor / 3;
	if (px) *px = GAME_W * col / 2 - w * col / 2 + m.x;
	if (py) *py = GAME_H * row / 2 - h * row / 2 + m.y;
}
