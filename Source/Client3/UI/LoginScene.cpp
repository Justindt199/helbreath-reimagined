// LoginScene.cpp: the live town behind the sign-in screens, the first-launch intro and the logo.
//
// Behind sign in, create account, connecting and the messages before the game, the backdrop is
// a part of Aresden or Elvine drawn from the map (mapdata\aresden.amd, elvine.amd) with the
// game's own tiles, buildings and trees, slowly panning. It changes every 20 seconds, Elvine
// and Aresden in turn, three parts of each. At night (as the server decides it: minutes 40..59 of
// each hour) the town is lit as in the game: moonlight, street lamps, lit windows, fireflies
// (UI\NightLights.cpp, Options > Graphics > Night lighting).
//
// The first time the game starts on a PC, a short intro plays first (registry: IntroSeen):
// the night sky, then Elvine, then Aresden, then the logo and the sign-in panel. A click or a
// key skips it.
//
// The loading screen at startup is the night sky, the logo and a rune circle (below).
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\Engine\UiGpu.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern char G_cSpriteShadeDegree;		// UI\NightLights.cpp

namespace {

const char * const kRegKey = "Software\\Siementech\\Helbreath\\Settings";

const COLORREF kGold    = RGB(242, 207, 115);
const COLORREF kGoldDim = RGB(217, 196, 143);
const COLORREF kElvine  = RGB(111, 168, 255);
const COLORREF kAresden = RGB(255, 106, 85);

// A town's map: per tile its ground sprite and frame, object sprite and frame (as CMapData reads it).
struct TownMap
{
	bool bTried = false, bOk = false;
	int w = 0, h = 0;
	std::vector<short> t;		// 4 per tile: ground sprite, frame, object sprite, frame
	bool Load(const char * pPath)
	{
		if (bTried) return bOk;
		bTried = true;
		FILE * f = fopen(pPath, "rb");
		if (f == NULL) return false;
		char cHead[257] = {};
		if (fread(cHead, 1, 256, f) != 256) { fclose(f); return false; }
		for (int i = 0; i < 256; i++) if (cHead[i] == 0) cHead[i] = ' ';
		const char * px = strstr(cHead, "MAPSIZEX"), * py = strstr(cHead, "MAPSIZEY");
		if (px == NULL || py == NULL) { fclose(f); return false; }
		px += 8; py += 8;
		while (*px == ' ' || *px == '=' || *px == '\t') px++;
		while (*py == ' ' || *py == '=' || *py == '\t') py++;
		w = atoi(px); h = atoi(py);
		if (w <= 0 || h <= 0 || w > 1000 || h > 1000) { fclose(f); return false; }
		std::vector<unsigned char> raw((size_t)w * h * 10);
		const bool bRead = fread(raw.data(), 1, raw.size(), f) == raw.size();
		fclose(f);
		if (!bRead) return false;
		t.resize((size_t)w * h * 4);
		for (size_t i = 0, n = (size_t)w * h; i < n; i++)
			for (int k = 0; k < 4; k++)
				t[i * 4 + k] = (short)(raw[i * 10 + k * 2] | (raw[i * 10 + k * 2 + 1] << 8));
		bOk = true;
		return true;
	}
	const short * At(int x, int y) const { return (x >= 0 && y >= 0 && x < w && y < h) ? &t[((size_t)y * w + x) * 4] : NULL; }
};
TownMap g_aresden, g_elvine;

// A part of a town: its map, the tile at the corner of the part, and how the view drifts across
// it (its top-left at the start and at the end of the 20 seconds, game pixels from that tile,
// for a 640 x 480 view; a larger screen is centred on the same spot).
struct Scene { TownMap * pMap; int tx, ty; float x0, y0, x1, y1; bool bElvine; };
const Scene kScenes[6] = {
	{ &g_elvine,  220, 156,   0, 190, 380, 400, true  },	// the brewery and the well
	{ &g_aresden, 116,  88,  40, 130, 350, 410, false },	// the front of the city hall
	{ &g_elvine,  200, 124,   0, 210, 370, 400, true  },	// the shop on the main road
	{ &g_aresden,  40,  76,   0,  20, 340, 390, false },	// houses by the long pool
	{ &g_elvine,   72,  44, 300,   0,   0, 380, true  },	// houses at the crossroads
	{ &g_aresden, 216,  92, 380,  30,  60, 360, false },	// the houses' yard
};
const DWORD kSceneMs = 20000, kDipMs = 700;		// each part is shown 20 s; dips through dark between them

int FloorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
float Ease(float u) { u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u); return u * u * (3.f - 2.f * u); }
float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

DWORD g_dwSceneT0 = 0;		// when the live town was first drawn

// fireflies over the grass at night
struct Firefly { float x, y, s; };
const std::vector<Firefly> & Fireflies()
{
	static std::vector<Firefly> v;
	if (v.empty()) {
		unsigned int r = 12345;
		auto rnd = [&r]() { r = r * 1103515245u + 12345u; return (float)((r >> 8) & 0xFFFF) / 65535.f; };
		for (int i = 0; i < 24; i++) v.push_back({ rnd(), 0.35f + rnd() * 0.6f, rnd() * 10.f });
	}
	return v;
}

// stars for the intro's sky
struct Star { float x, y, r, s; };
const std::vector<Star> & Stars()
{
	static std::vector<Star> v;
	if (v.empty()) {
		unsigned int r = 777;
		auto rnd = [&r]() { r = r * 1103515245u + 12345u; return (float)((r >> 8) & 0xFFFF) / 65535.f; };
		for (int i = 0; i < 150; i++) v.push_back({ rnd(), rnd() * 0.8f, 1.f + (rnd() < 0.2f ? 1.f : 0.f), rnd() * 9.f });
	}
	return v;
}

COLORREF Mix(COLORREF a, COLORREF b, float k)
{
	k = Clamp01(k);
	return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * k), (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * k),
		(int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * k));
}

// the night sky's colour: near black at the top, lightening smoothly towards the ground (thin
// bands of rising strength: a single panel would leave a hard edge across the screen)
void DrawSkyGradient(CRenderer & R, float a)
{
	const int W = GAME_W, H = GAME_H;
	R.FillPanel(0, 0, W, H, 0, RGB(5, 6, 14), (int)(255 * a));
	const int y0 = H * 2 / 5;
	for (int y = y0; y < H; y += 2) {
		const float u = (float)(y - y0) / (float)(H - y0);
		const int iA = (int)(a * 170.f * u * u * (3.f - 2.f * u));
		if (iA > 0) R.FillPanel(0, y, W, y + 2, 0, RGB(26, 32, 60), iA);
	}
}

} // namespace

//////////////////////////////////////////////////////////////////////
// Flags kept in the registry with the other settings

bool CGame::LoginFlag(const char * pName)
{
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
	DWORD dwVal = 0, dwType = 0, dwLen = sizeof(dwVal);
	const bool bOk = RegQueryValueEx(key, pName, NULL, &dwType, (LPBYTE)&dwVal, &dwLen) == ERROR_SUCCESS && dwType == REG_DWORD && dwVal != 0;
	RegCloseKey(key);
	return bOk;
}

void CGame::SetLoginFlag(const char * pName)
{
	HKEY key;
	DWORD dwDisp;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, kRegKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &dwDisp) != ERROR_SUCCESS) return;
	const DWORD dwVal = 1;
	RegSetValueEx(key, pName, 0, REG_DWORD, (const BYTE *)&dwVal, sizeof(dwVal));
	RegCloseKey(key);
}

bool CGame::LoginIsNight()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	return st.wMinute >= 40;		// the server: NIGHTTIME (HG.h), on its clock; time zones differ by whole hours
}

//////////////////////////////////////////////////////////////////////
// The town

bool CGame::LoginSceneReady()
{
	if (m_pTileSpr[0] == NULL || m_pTileSpr[223] == NULL) return false;	// the map sprites aren't loaded yet
	g_aresden.Load("mapdata\\aresden.amd");
	g_elvine.Load("mapdata\\elvine.amd");
	return g_aresden.bOk && g_elvine.bOk && m_Renderer.UiGpuRecorder() != NULL;
}

namespace {

// one part of a town with its view's top-left at (fx, fy) (game pixels, in the map)
void DrawTown(CGame * g, const TownMap & M, float fx, float fy)
{
	CRenderer & R = g->m_Renderer;
	const int cx2 = (int)std::floor(fx * 2.f), cy2 = (int)std::floor(fy * 2.f);
	const int gx = FloorDiv(cx2, 2), gy = FloorDiv(cy2, 2);
	R.m_iShiftX2 = -(cx2 - 2 * gx);		// the half pixel, so it pans smoothly
	R.m_iShiftY2 = -(cy2 - 2 * gy);
	const RECT rcWas = R.m_rcClipArea;
	SetRect(&R.m_rcClipArea, 0, 0, GAME_W, GAME_H);
	const DWORD dwTime = timeGetTime();
	const int X0 = FloorDiv(gx, 32), Y0 = FloorDiv(gy, 32);
	const int nX = GAME_W / 32 + 2, nY = GAME_H / 32 + 2;

	// the ground (buildings drawn as ground tiles get their lit windows)
	for (int Y = Y0; Y <= Y0 + nY; Y++)
		for (int X = X0; X <= X0 + nX; X++) {
			const short * p = M.At(X, Y);
			if (p == NULL || p[0] < 0 || p[0] >= MAXTILES || g->m_pTileSpr[p[0]] == NULL) continue;
			const int sx = X * 32 - gx, sy = Y * 32 - gy;
			g->m_pTileSpr[p[0]]->PutSpriteFastNoColorKey(sx, sy, p[1], dwTime);
			g->NightLightsWindows(p[0], p[1], sx, sy);
		}

	// buildings, objects and trees, a row at a time (tall ones below the view reach up into it)
	for (int Y = Y0 - 1; Y <= Y0 + nY + 12; Y++)
		for (int X = X0 - 4; X <= X0 + nX + 4; X++) {
			const short * p = M.At(X, Y);
			if (p == NULL) continue;
			const short sObj = p[2], sFrame = p[3];
			if (sObj <= 0 || sObj >= MAXTILES) continue;
			const int ix = X * 32 - gx + 16, iy = Y * 32 - gy + 16;
			if (sObj < 100 || sObj >= 200) {
				CSprite * s = g->m_pTileSpr[sObj];
				if (s == NULL) continue;
				if (sObj == 200 || sObj == 223 || (sObj == 224 && !(sFrame == 24 || (sFrame >= 34 && sFrame <= 38))))
					s->PutShadowSprite(ix - 16, iy - 16, sFrame, dwTime);
				s->PutSpriteFast(ix - 16, iy - 16, sFrame, dwTime);
				g->NightLightsWindows(sObj, sFrame, ix - 16, iy - 16);
				if (sObj == 200 && sFrame == 32) {
					const POINT heads[1] = { { ix - 2, iy - 81 } };
					g->NightLightsLamp(ix, iy, heads, 1);
				}
				else if (sObj == 223 && sFrame == 4) {
					const POINT heads[3] = { { ix - 2, iy - 156 }, { ix + 16, iy - 102 }, { ix - 19, iy - 132 } };
					g->NightLightsLamp(ix, iy, heads, 3);
				}
			}
			else {	// trees (as the game draws them)
				const int t = (sObj < 111) ? 104 : (sObj < 123) ? 109 : (sObj < 132) ? 123 : 132;
				if (g->m_pTileSpr[t] != NULL) g->m_pTileSpr[t]->PutSpriteFast(ix - 16, iy - 16, sFrame, dwTime);
			}
		}

	R.m_iShiftX2 = R.m_iShiftY2 = 0;
	R.m_rcClipArea = rcWas;
}

} // namespace

// The campfire on the character select screen (UI\CharScreens.cpp): the grass in Elvine with the
// view's top-left at (fx, fy) (map pixels), its houses' windows and lamps lit for the night.
void CGame::DrawCampGround(float fx, float fy)
{
	if (!LoginSceneReady()) return;
	m_dwCurTime = timeGetTime();
	const char cShadeWas = G_cSpriteShadeDegree;
	G_cSpriteShadeDegree = 1;		// lit by the light pass, not shaded in the sprites
	m_bLoginNight = true;			// (always night at the campfire)
	DrawTown(this, g_elvine, fx, fy);
	m_bLoginNight = false;
	G_cSpriteShadeDegree = cShadeWas;
}

// Draws the part of a town (iScene) with the view drifting (u: 0..1 through it), lit at night.
static void DrawSceneAt(CGame * g, int iScene, float u)
{
	if (!g->LoginSceneReady()) return;	// loads the two towns' maps the first time (then cached)
	const Scene & s = kScenes[iScene];
	const float e = Ease(u);
	const float cx = s.tx * 32.f + s.x0 + (s.x1 - s.x0) * e + 320.f;	// the view's centre
	const float cy = s.ty * 32.f + s.y0 + (s.y1 - s.y0) * e + 240.f;
	DrawTown(g, *s.pMap, cx - GAME_W * 0.5f, cy - GAME_H * 0.5f);
}

// the moonlight over what was drawn (night only), and the particles: fireflies at night,
// motes of dust and pollen drifting in the sun by day
static void LightTheScene(CGame * g, DWORD dwNow)
{
	CUiGpu * pG = g->m_Renderer.UiGpuRecorder();
	if (pG == NULL) return;
	const float t = dwNow / 1000.f;
	if (g->NightLightsOn()) {
		const int iD = g->m_iNightDarkness < 0 ? 0 : (g->m_iNightDarkness > 100 ? 100 : g->m_iNightDarkness);
		const float fAmb = 1.f - iD / 100.f * 0.82f;
		pG->SetLightPass(fAmb * 0.62f, fAmb * 0.72f, fAmb);
		for (const Firefly & f : Fireflies()) {
			const float a = 0.5f + 0.5f * std::sin(t * 2.f + f.s * 3.f);
			const float x = std::fmod(f.x * GAME_W + std::sin(t * 0.4f + f.s) * 60.f + GAME_W, (float)GAME_W);
			const float y = f.y * GAME_H + std::sin(t * 0.7f + f.s * 2.f) * 30.f;
			pG->AddLightGlow(2.f * x, 2.f * y, 2.f * 6.f, 0.86f, 1.f, 0.55f, 0.9f * a);
			pG->AddLightGlow(2.f * x, 2.f * y, 2.f * 2.f, 1.f, 1.f, 0.85f, a);
		}
	}
	else {
		// slow, drifting right and upward, each twinkling a little
		for (const Firefly & f : Fireflies()) {
			const float a = 0.55f + 0.45f * std::sin(t * 1.3f + f.s * 2.f);
			const float x = std::fmod(f.x * GAME_W + t * (6.f + f.s * 1.5f) + std::sin(t * 0.5f + f.s) * 20.f, (float)GAME_W + 20.f) - 10.f;
			const float y = std::fmod(f.y * GAME_H - t * (4.f + f.s) + GAME_H * 4.f, (float)GAME_H);
			pG->AddLightGlow(2.f * x, 2.f * y, 2.f * 4.f, 1.f, 0.95f, 0.78f, 0.35f * a);
			pG->AddLightGlow(2.f * x, 2.f * y, 2.f * 1.5f, 1.f, 1.f, 0.92f, 0.6f * a);
		}
	}
}

void CGame::DrawLoginScene()
{
	CRenderer & R = m_Renderer;
	const DWORD dwNow = timeGetTime();
	if (g_dwSceneT0 == 0) g_dwSceneT0 = dwNow;
	const DWORD t = dwNow - g_dwSceneT0;
	const int iPart = (int)((t / kSceneMs) % 6);
	const DWORD u = t % kSceneMs;

	// (no black fill first: a panel recorded before the light pass would be drawn after it,
	// over the town; the town covers the whole screen)
	m_dwCurTime = dwNow;
	m_bLoginNight = LoginIsNight();
	const char cShadeWas = G_cSpriteShadeDegree;
	G_cSpriteShadeDegree = 1;		// lit by the moonlight, not shaded in the sprites
	DrawSceneAt(this, iPart, (float)u / kSceneMs);
	LightTheScene(this, dwNow);
	G_cSpriteShadeDegree = cShadeWas;
	m_bLoginNight = false;

	// dip through dark between the parts
	float fDip = 0.f;
	if (u < kDipMs && t >= kSceneMs) fDip = 1.f - (float)u / kDipMs;
	else if (u > kSceneMs - kDipMs) fDip = (float)(u - (kSceneMs - kDipMs)) / kDipMs;
	if (fDip > 0.f) R.FillPanel(0, 0, GAME_W, GAME_H, 0, RGB(0, 0, 0), (int)(255 * Ease(fDip)));

	// the town's name, bottom left
	const bool bElvine = kScenes[iPart].bElvine;
	const char * pName = bElvine ? "E L V I N E" : "A R E S D E N";
	R.m_iFadeA = (int)(255 * (1.f - fDip));
	R.m_iFace = 1;
	if (LoginModule(LM_TOWN).show) {	// bottom left, unless login_layout.cfg moves it
		int tx, ty;
		LoginPlace(LM_TOWN, R.TextWidthSized(pName, 14), 14, &tx, &ty);
		R.TextOutSized(tx + 1, ty + 1, pName, RGB(0, 0, 0), 14);
		R.TextOutSized(tx, ty, pName, bElvine ? kElvine : kAresden, 14);
	}
	R.m_iFace = 0;
	R.m_iFadeA = 255;
}

//////////////////////////////////////////////////////////////////////
// The logo, top left: a gold glint runs across it every few seconds

void CGame::DrawLoginLogo()
{
	if (!LoginModule(LM_LOGO).show) return;	// login_layout.cfg (UI\LoginLayout.cpp)
	DrawLogoAt(LoadHandover(), 1.f);	// gliding into the corner straight after loading
}

void CGame::DrawLogoAt(float k, float fAlpha)
{
	if (fAlpha <= 0.f) return;
	CRenderer & R = m_Renderer;
	const DWORD dwNow = timeGetTime();
	if (g_dwSceneT0 == 0) g_dwSceneT0 = dwNow;
	k = Ease(k);
	const bool bDisplay = R.HasDisplayFont();
	// sizes in 2 px steps, so the glyphs are cached at a few sizes only
	const int iPx = ((int)(60.f - 18.f * k + 0.5f) & ~1) + (bDisplay ? 0 : 2), iSmall = (k < 0.5f) ? 16 : 14;
	const char * pSmall = LoginModule(LM_LOGO).text.c_str();	// login_layout.cfg (UI\LoginLayout.cpp)
	const char * pBig = LoginModule(LM_LOGO).text2.c_str();
	const int n = (int)strlen(pBig), iStep = bDisplay ? 1 : 2;
	R.m_iFace = 1;			// the display font (fonts\display.ttf), when there is one
	int wBig = 0;
	for (int i = 0; i < n; i++) { const char c[2] = { pBig[i], 0 }; wBig += R.TextWidthSized(c, iPx) + iStep; }
	const int wSmall = R.TextWidthSized(pSmall, iSmall);
	// in the middle (a little above it) while loading; top left over the town
	const float fxC = (GAME_W - wBig) / 2.f, fbyC = GAME_H * 0.38f - iPx * 0.8f;
	int iLogoX, iLogoY;				// where it rests over the town: 26, 20 unless login_layout.cfg moves it
	LoginPlace(LM_LOGO, wBig, 60, &iLogoX, &iLogoY);
	const int x = (int)(fxC + (iLogoX - fxC) * k), by = (int)(fbyC + (iLogoY + 18 - fbyC) * k);
	const int iGap = (int)(30.f - 12.f * k), sx = x + 1 + (int)((wBig - wSmall) / 2.f * (1.f - k));
	R.m_iFadeA = (int)(255 * Clamp01(fAlpha));
	R.TextOutSized(sx + 1, by - iGap + 1, pSmall, RGB(0, 0, 0), iSmall);
	R.TextOutSized(sx, by - iGap, pSmall, kGoldDim, iSmall);

	const float fCycle = (float)((dwNow - g_dwSceneT0 + 4200) % 6000) / 1300.f;	// 0..1 while the glint runs
	const float fG = -2.f + fCycle * (n + 4);
	int cx = x;
	for (int i = 0; i < n; i++) {
		const char c[2] = { pBig[i], 0 };
		const float kg = (fCycle <= 1.f) ? (std::max)(0.f, 1.f - std::fabs(i - fG) / 1.4f) : 0.f;
		const COLORREF col = Mix(kGold, RGB(255, 250, 232), kg);
		R.TextOutSized(cx + 2, by + 3, c, RGB(0, 0, 0), iPx);
		R.TextOutSized(cx, by, c, col, iPx);
		if (!bDisplay) R.TextOutSized(cx + 1, by, c, col, iPx);	// (the display font is bold already)
		cx += R.TextWidthSized(c, iPx) + iStep;
	}
	R.m_iFace = 0;
	// a gold rule under it, fading out to the right
	const int iRule = cx - x;
	for (int i = 0; i < 6; i++)
		R.FillPanel(x + iRule * i / 6, by + iPx + 6, x + iRule * (i + 1) / 6, by + iPx + 8, 0, RGB(232, 188, 92), 220 - i * 36);
	R.m_iFadeA = 255;
}

//////////////////////////////////////////////////////////////////////
// The loading screen at startup: a night sky with embers drifting up, the logo in the middle and
// the rune circle filling with the percentage. The game loads in steps and shows a frame after
// each one, so this is drawn a frame at a time as loading goes.

namespace {

// the night sky: dark blue, lighter near the ground, twinkling stars and embers drifting up
void DrawNightSky(CRenderer & R, float a, float t)
{
	if (a <= 0.f) return;
	const int W = GAME_W, H = GAME_H;
	DrawSkyGradient(R, a);
	for (const Star & s : Stars()) {
		const float k = (0.35f + 0.65f * (0.5f + 0.5f * std::sin(t * 1.3f + s.s))) * a;
		const int sx = (int)(s.x * W), sy = (int)(s.y * H), r = (int)s.r;
		R.FillPanel(sx, sy, sx + r + 1, sy + r + 1, r, RGB(230, 236, 255), (int)(230 * k));
	}
	static float s_fEmber[40][4];		// x, y, speed, phase
	static bool s_bEmbers = false;
	if (!s_bEmbers) {
		unsigned int r = 4242;
		auto rnd = [&r]() { r = r * 1103515245u + 12345u; return (float)((r >> 8) & 0xFFFF) / 65535.f; };
		for (auto & e : s_fEmber) { e[0] = rnd(); e[1] = rnd(); e[2] = 0.02f + rnd() * 0.04f; e[3] = rnd() * 9.f; }
		s_bEmbers = true;
	}
	for (const auto & e : s_fEmber) {
		float y = std::fmod(e[1] - t * e[2], 1.f);
		if (y < 0.f) y += 1.f;
		const float k = std::sin(y * 3.14159f) * a;
		const int x = (int)(e[0] * W + std::sin(t * 0.6f + e[3]) * 20.f), iy = (int)((y * 1.1f - 0.05f) * H);
		R.FillPanel(x - 4, iy - 4, x + 5, iy + 5, 4, RGB(255, 150, 70), (int)(60 * k));
		R.FillPanel(x - 1, iy - 1, x + 2, iy + 2, 1, RGB(255, 220, 160), (int)(220 * k));
	}
}

// the rune circle: a gold ring filling clockwise from the top, runes turning inside it, and the
// percentage in the middle
void DrawRunes(CRenderer & R, int cx, int cy, float p, float a, float t)
{
	if (a <= 0.f) return;
	const float kPi2 = 6.28318f;
	const int iA = (int)(255 * Clamp01(a));
	const int nRing = 120;
	for (int i = 0; i < nRing; i++) {		// the faint track: two rings
		const float ang = kPi2 * i / nRing;
		const int x = cx + (int)(std::sin(ang) * 46.f), y = cy - (int)(std::cos(ang) * 46.f);
		R.FillPanel(x - 1, y - 1, x + 1, y + 1, 1, RGB(232, 188, 92), iA * 60 / 255);
		if (i % 2 == 0) {
			const int x2 = cx + (int)(std::sin(ang) * 35.f), y2 = cy - (int)(std::cos(ang) * 35.f);
			R.FillPanel(x2 - 1, y2 - 1, x2 + 1, y2 + 1, 1, RGB(232, 188, 92), iA * 50 / 255);
		}
	}
	const int nLit = (int)(nRing * Clamp01(p) + 0.5f);
	for (int i = 0; i < nLit; i++) {		// the filled part, glowing
		const float ang = kPi2 * i / nRing;
		const int x = cx + (int)(std::sin(ang) * 46.f), y = cy - (int)(std::cos(ang) * 46.f);
		R.FillPanel(x - 5, y - 5, x + 6, y + 6, 5, RGB(255, 200, 100), iA * 26 / 255);
		R.FillPanel(x - 2, y - 2, x + 2, y + 2, 2, RGB(240, 200, 102), iA);
	}
	for (int i = 0; i < 16; i++) {			// the runes, turning slowly
		const float ang = kPi2 * i / 16.f + t * 0.4f;
		const int x = cx + (int)(std::sin(ang) * 40.5f), y = cy - (int)(std::cos(ang) * 40.5f);
		const bool bLit = (float)i / 16.f < p;
		const COLORREF col = bLit ? RGB(255, 231, 168) : RGB(232, 188, 92);
		const int ia = bLit ? iA : iA * 80 / 255;
		if (i % 2) R.FillPanel(x - 2, y - 2, x + 3, y + 3, 0, col, ia);
		else R.FillPanel(x - 2, y - 2, x + 3, y + 3, 2, col, ia);
	}
	char cPct[8];
	wsprintf(cPct, "%d%%", (int)(Clamp01(p) * 100.f + 0.5f));
	R.m_iFadeA = iA;
	R.TextOutSized(cx - R.TextWidthSized(cPct, 15) / 2, cy - 9, cPct, RGB(236, 238, 242), 15);
	R.m_iFadeA = 255;
}

// a soft burst of light
void DrawFlash(CRenderer & R, int cx, int cy, float a)
{
	if (a <= 0.f) return;
	for (int i = 0; i < 5; i++) {
		const int r = 30 + i * 22;
		R.FillPanel(cx - r, cy - r, cx + r, cy + r, r, RGB(255, 220, 150), (int)(a * (70 - i * 12)));
	}
}

const char * LoadingStage(int iPct)
{
	if (iPct < 4) return "Preparing the interface...";
	if (iPct < 16) return "Loading the towns...";
	if (iPct < 24) return "Loading characters...";
	if (iPct < 52) return "Loading monsters...";
	if (iPct < 100) return "Loading items and spells...";
	return "Loading effects and sounds...";
}

} // namespace

void CGame::DrawLoadingScreen(int iPct)
{
	CRenderer & R = m_Renderer;
	const DWORD dwNow = timeGetTime();
	if (m_dwLoadT0 == 0) m_dwLoadT0 = dwNow;
	const float t = (dwNow - m_dwLoadT0) / 1000.f;
	if (iPct < 0) iPct = 0;
	if (iPct > 100) iPct = 100;
	const int W = GAME_W, H = GAME_H, cy = (int)(H * 0.66f);

	DrawNightSky(R, 1.f, t);
	DrawLogoAt(0.f, 1.f);
	DrawRunes(R, W / 2, cy, iPct / 100.f, 1.f, t);
	const char * pStage = LoadingStage(iPct);
	R.TextOutSized((W - R.TextWidthSized(pStage, 13)) / 2, cy + 64, pStage, RGB(184, 190, 200), 13);
	// fading up from black at the start
	if (t < 1.2f) R.FillPanel(0, 0, W, H, 0, RGB(0, 0, 0), (int)(255 * (1.f - t / 1.2f)));
}

float CGame::LoadHandover()
{
	if (m_dwLoadDoneT == 0) return 1.f;
	const int e = (int)(timeGetTime() - m_dwLoadDoneT);
	return (e < 0) ? 0.f : Clamp01(e / 1300.f);
}

// straight after loading, over the town or the intro: the sky fades off, the rune circle flashes
// and fades, and (into the intro) the logo in the middle fades out; over the town the logo glides
// into its corner instead (DrawLoginLogo)
void CGame::DrawLoadHandover(bool bIntro)
{
	if (m_dwLoadDoneT == 0) return;
	const int e = (int)(timeGetTime() - m_dwLoadDoneT);
	if (e < 0 || e >= 1300) return;
	CRenderer & R = m_Renderer;
	const float s = e / 1000.f, t = (timeGetTime() - m_dwLoadT0) / 1000.f;
	const int cx = GAME_W / 2, cy = (int)(GAME_H * 0.66f);
	if (!bIntro) DrawNightSky(R, 1.f - Ease((s - 0.2f) / 1.1f), t);
	DrawRunes(R, cx, cy, 1.f, 1.f - Clamp01((s - 0.1f) / 0.6f), t);
	DrawFlash(R, cx, cy, Clamp01(1.f - std::fabs(s - 0.15f) / 0.25f));
	if (bIntro) DrawLogoAt(0.f, 1.f - Clamp01(s / 0.8f));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Entering the world after Play: the loading screen's night sky and rune circle, where the
// character is going, and a tip. The server gives no progress, so the circle fills with each
// step (connecting, the server's answer, the world's data) and creeps on while it waits.

namespace {

struct Tip { const char * pBold, * pRest, * pLine2; };
const Tip kTips[] = {
	{ "Talk to the town guide", " first. They explain the basics", "and point you to your first quest." },
	{ "Stat points", " come with every level. Spend them to grow", "into a warrior, a mage or something in between." },
	{ "Aresden and Elvine", " are at war.", "Pick your side when you are ready." },
	{ "Shops", " sell starter gear. Your first coins come", "from hunting just outside town." },
};

} // namespace

void CGame::DrawEnteringScreen(int iStage)
{
	CRenderer & R = m_Renderer;
	const DWORD dwNow = timeGetTime();
	if (m_dwEnterT0 == 0) m_dwEnterT0 = dwNow;
	const float t = (dwNow - m_dwEnterT0) / 1000.f;
	const float fWait = (float)(int)(dwNow - m_dwTime) / 1000.f;		// in this step
	if (iStage < 0) iStage = 0;
	if (iStage > 2) iStage = 2;
	static const float kFrom[3] = { 0.f, 0.35f, 0.65f }, kTo[3] = { 0.35f, 0.65f, 0.95f };
	const float p = kFrom[iStage] + (kTo[iStage] - kFrom[iStage]) * (1.f - std::exp(-(fWait < 0.f ? 0.f : fWait) / 1.2f));
	if (p > m_fEnterP) m_fEnterP = p;

	const int W = GAME_W, H = GAME_H, cy = (int)(H * 0.40f);
	DrawNightSky(R, 1.f, t);
	DrawRunes(R, W / 2, cy, m_fEnterP, 1.f, t);

	// where to: "E N T E R I N G   A R E S D E N"
	char cMap[64] = {}, cId[12] = {};
	memcpy(cId, m_cMapName, 10);
	GetOfficialMapName(cId, cMap);
	if (cMap[0] == 0) strcpy(cMap, cId);
	std::string s = "E N T E R I N G  ";
	const char * pWhere = cMap[0] ? cMap : "THE WORLD";
	for (const char * q = pWhere; *q; q++) { s += ' '; s += (char)toupper((unsigned char)*q); }
	R.TitleOut((W - R.TitleWidth(s.c_str(), 16)) / 2, (int)(H * 0.62f), s.c_str(), kGold, 16);

	// a tip, changing every few seconds
	const Tip & tip = kTips[(int)(t / 5.f) % (int)(sizeof(kTips) / sizeof(kTips[0]))];
	const int wB = R.TitleWidth(tip.pBold, 12), wR = R.TextWidthSized(tip.pRest, 12);
	const int ty = (int)(H * 0.62f) + 34, tx = (W - (wB + wR)) / 2;
	R.TitleOut(tx, ty, tip.pBold, RGB(240, 242, 245), 12);
	R.TextOutSized(tx + wB, ty, tip.pRest, RGB(170, 176, 186), 12);
	R.TextOutSized((W - R.TextWidthSized(tip.pLine2, 12)) / 2, ty + 17, tip.pLine2, RGB(170, 176, 186), 12);

	if (fWait > 7.f) {
		const char * pSlow = "This is taking longer than usual. Press Esc to go back.";
		R.TextOutSized((W - R.TextWidthSized(pSlow, 11)) / 2, H - 30, pSlow, RGB(130, 136, 146), 11);
	}
	// fading up from black as the screen opens
	if (t < 0.4f) R.FillPanel(0, 0, W, H, 0, RGB(0, 0, 0), (int)(255 * (1.f - t / 0.4f)));
}

//////
// The first-launch intro: about 16 seconds. False once it has finished.

namespace {

void Caption(CRenderer & R, const char * s, int iPx, COLORREF c, float a, int y)
{
	if (a <= 0.f) return;
	R.m_iFace = 1;		// the display font
	const int w = R.TextWidthSized(s, iPx), x = (GAME_W - w) / 2;
	R.m_iFadeA = (int)(255 * a);
	R.TextOutSized(x + 2, y + 2, s, RGB(0, 0, 0), iPx);
	R.TextOutSized(x, y, s, c, iPx);
	if (!R.HasDisplayFont()) R.TextOutSized(x + 1, y, s, c, iPx);
	R.m_iFadeA = 255;
	R.m_iFace = 0;
}

// 0 outside [a, b], fading in and out over 0.6 s at each end
float Window(float t, float a, float b) { return Clamp01((t - a) / 0.6f) * Clamp01((b - t) / 0.6f); }

} // namespace

bool CGame::DrawLoginIntro(DWORD dwElapsed)
{
	CRenderer & R = m_Renderer;
	const float t = dwElapsed / 1000.f;
	const DWORD dwNow = timeGetTime();

	m_dwCurTime = dwNow;
	m_bLoginNight = LoginIsNight();
	const char cShadeWas = G_cSpriteShadeDegree;
	G_cSpriteShadeDegree = 1;
	if (t >= 3.2f && t < 8.4f) {			// Elvine
		DrawSceneAt(this, 0, (t - 3.2f) / 5.2f * 0.6f);
		LightTheScene(this, dwNow);
	}
	else if (t >= 8.4f) {					// Aresden: the city hall
		DrawSceneAt(this, 1, (t - 8.4f) / 7.8f * 0.7f);
		LightTheScene(this, dwNow);
	}
	G_cSpriteShadeDegree = cShadeWas;
	m_bLoginNight = false;

	// the night sky, fading into Elvine
	if (t < 4.4f) {
		const float a = (t < 3.2f) ? 1.f : 1.f - (t - 3.2f) / 1.2f;
		DrawSkyGradient(R, a);
		for (const Star & s : Stars()) {
			const float k = (0.5f + 0.5f * std::sin(t * 1.3f + s.s)) * a;
			const int sx = (int)(s.x * GAME_W), sy = (int)(s.y * GAME_H), r = (int)s.r;
			R.FillPanel(sx, sy, sx + r + 1, sy + r + 1, r, RGB(230, 236, 255), (int)(230 * k));
		}
	}
	DrawLoadHandover(true);		// straight after loading: the logo and rune circle fade off the sky
	// through dark from Elvine to Aresden, and a little darker at the end for the panel
	if (t >= 7.8f && t < 9.0f) R.FillPanel(0, 0, GAME_W, GAME_H, 0, RGB(0, 0, 0), (int)(255 * Ease(1.f - std::fabs(t - 8.4f) / 0.6f)));
	if (t > 13.f) R.FillPanel(0, 0, GAME_W, GAME_H, 0, RGB(0, 0, 0), (int)(80 * Clamp01((t - 13.f) / 2.f)));

	const int cy = GAME_H * 2 / 5;
	Caption(R, "Two kingdoms share one land.", 20, RGB(236, 238, 242), Window(t, 0.4f, 3.0f), cy);
	Caption(R, "E L V I N E", 46, kElvine, Window(t, 3.6f, 7.6f), cy - 14);
	Caption(R, "A R E S D E N", 46, kAresden, Window(t, 8.6f, 12.6f), cy - 14);
	Caption(R, "One land. Unrivaled.", 22, RGB(236, 238, 242), Window(t, 13.2f, 15.6f), cy);

	// how to skip, bottom right
	R.m_iFadeA = 170;
	const char * pSkip = "Click to skip";
	R.TextOutSized(GAME_W - 16 - R.TextWidthSized(pSkip, 11), GAME_H - 22, pSkip, RGB(200, 204, 212), 11);
	R.m_iFadeA = 255;
	return t < 16.2f;
}

//////////////////////////////////////////////////////////////////////
// A panel appearing: rising into place the first time, sliding in from the right after another
// panel; then a light runs once round its gold edge.

void CGame::LoginPanelAnim(int * pDX, int * pDY, int * pAlpha)
{
	const float e = (float)(int)(timeGetTime() - m_dwPanelT0);		// below 0: not yet
	const float fDelay = m_bPanelSwap ? 0.f : 250.f, fDur = m_bPanelSwap ? 320.f : 650.f;
	const float u = Clamp01((e - fDelay) / fDur), k = 1.f - (1.f - u) * (1.f - u) * (1.f - u);
	*pDX = m_bPanelSwap ? (int)((1.f - k) * 26.f) : 0;
	*pDY = m_bPanelSwap ? 0 : (int)((1.f - k) * 20.f);
	*pAlpha = (int)(255 * u);
}

void CGame::DrawPanelEdgeLight(const RECT & rc)
{
	const float fStart = m_bPanelSwap ? 250.f : 800.f, fDur = 1400.f;
	const float s = ((float)(int)(timeGetTime() - m_dwPanelT0) - fStart) / fDur;
	if (s <= 0.f || s >= 1.f) return;
	const float w = (float)(rc.right - rc.left), h = (float)(rc.bottom - rc.top), L = 2.f * (w + h);
	const float fFade = std::sin(s * 3.14159f);
	auto at = [&](float d, float & x, float & y) {
		d = std::fmod(d + L, L);
		if (d < w) { x = rc.left + d; y = (float)rc.top; }
		else if (d < w + h) { x = (float)rc.right; y = rc.top + (d - w); }
		else if (d < 2 * w + h) { x = rc.right - (d - w - h); y = (float)rc.bottom; }
		else { x = (float)rc.left; y = rc.bottom - (d - 2 * w - h); }
	};
	const float head = s * L + w * 0.3f;
	for (int i = 0; i < 18; i++) {
		float x, y;
		at(head - i * 5.f, x, y);
		const int r = (i < 3) ? 2 : 1;
		m_Renderer.FillPanel((int)x - r, (int)y - r, (int)x + r + 1, (int)y + r + 1, r + 1, RGB(255, 236, 180), (int)(fFade * (230 - i * 12)));
	}
}
