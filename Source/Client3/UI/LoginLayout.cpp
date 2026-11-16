// LoginLayout.cpp: where the pieces of the sign-in screen go and what they say, read from
// CONTENTS\login_layout.cfg (the file explains itself). The dashboard's "Login screen" tab edits that
// file (tools\hb_dashboard); a missing file or line keeps the value below, which is the layout the
// screen had before it could be changed.
//
// Used by UI\LoginScreen.cpp (sign-in and create-account panels, boxes, buttons, news, version),
// UI\LoginScene.cpp (logo, the town's name) and UI\CharScreens.cpp (character select). LoginRects (LoginScreen.cpp) is built from it, so clicks follow whatever moves.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

const char * const kNames[CGame::LM_COUNT] = { "logo", "news", "town", "version", "panel", "account", "password", "remember", "connect", "create", "quit",
	"characters title", "character info", "hint", "back", "delete", "play", "log out", "change password",
	"new account panel", "new account", "new password", "confirm password", "e-mail", "secret question", "answer", "account help",
	"create account", "clear", "back to sign in" };
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
	// character select (UI\CharScreens.cpp CampRects): each piece on the screen
	Set(CGame::LM_SEL_TITLE, 0, 16, 14, 0, 0, "CHARACTERS");
	Set(CGame::LM_SEL_INFO, 2, -86, 43, 256, 176, "");	// where it was on a 16:9 screen
	Set(CGame::LM_SEL_HINT, 7, 0, -54, 0, 12, "");
	g_mod[CGame::LM_SEL_HINT].line[0] = "Choose a character to take a closer look.";
	g_mod[CGame::LM_SEL_HINT].line[1] = "Press Play to enter the world.";
	g_mod[CGame::LM_SEL_HINT].line[2] = "An empty seat at the fire.";
	Set(CGame::LM_SEL_BACK, 7, -92, -20, 84, 26, "BACK");
	Set(CGame::LM_SEL_DELETE, 7, 0, -20, 84, 26, "DELETE", "CREATE");
	Set(CGame::LM_SEL_PLAY, 7, 92, -20, 84, 26, "PLAY");
	Set(CGame::LM_SEL_LOGOUT, 6, 16, -20, 84, 26, "LOG OUT");
	Set(CGame::LM_SEL_CHANGEPW, 6, 112, -24, 110, 18, "Change password");
	// create account (UI\LoginScreen.cpp CreateAccountRects): the panel on the screen, the rest inside it
	Set(CGame::LM_NEW_PANEL, 5, -26, -10, 300, 420, "CREATE ACCOUNT", "Choose your name and join the war.");
	Set(CGame::LM_NEW_ACCOUNT, 0, 18, 76, 264, 26, "ACCOUNT");
	Set(CGame::LM_NEW_PASSWORD, 0, 18, 124, 127, 26, "PASSWORD");
	Set(CGame::LM_NEW_CONFIRM, 0, 155, 124, 127, 26, "CONFIRM PASSWORD");
	Set(CGame::LM_NEW_EMAIL, 0, 18, 172, 264, 26, "E-MAIL");
	Set(CGame::LM_NEW_QUESTION, 0, 18, 220, 264, 26, "SECRET QUESTION");
	Set(CGame::LM_NEW_ANSWER, 0, 18, 268, 264, 26, "ANSWER");
	Set(CGame::LM_NEW_HELP, 0, 18, 300, 264, 39, "");
	Set(CGame::LM_NEW_CREATE, 0, 18, 344, 264, 32, "CREATE ACCOUNT");
	Set(CGame::LM_NEW_CLEAR, 0, 18, 384, 127, 22, "Clear");
	Set(CGame::LM_NEW_BACK, 0, 155, 384, 127, 22, "Back to sign in");

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
