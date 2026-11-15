// CharScreens.cpp: character select and character creation, drawn in code over the live town
// (UI\LoginScene.cpp) like the sign-in screens: the character stands on a lit pedestal in the
// middle, the panels sit at the sides, the buttons along the bottom. The canvas is as wide as the
// screen (UI\GameView.cpp), so everything is placed from GAME_W.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\Engine\UiGpu.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

const COLORREF kGold	= RGB(242, 207, 115);
const COLORREF kGoldLine = RGB(232, 192, 112);
const COLORREF kWhite	= RGB(240, 242, 245);
const COLORREF kGrey	= RGB(154, 162, 172);
const COLORREF kDim		= RGB(111, 119, 130);
const COLORREF kInk		= RGB(30, 22, 8);		// text on gold

int Width(CRenderer & r, const char * s, int iPx = 0)
{
	if (s == NULL || s[0] == 0) return 0;
	return r.TextWidthSized(s, iPx);
}

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }
float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float Ease(float u) { u = Clamp01(u); return u * u * (3.f - 2.f * u); }

void Panel(CRenderer & r, const RECT & rc)
{
	r.StonePanel(rc.left, rc.top, rc.right, rc.bottom);
}

// cuts the text with ".." so it fits iMaxW pixels
std::string Fit(CRenderer & r, const char * s, int iPx, int iMaxW)
{
	std::string t = (s != NULL) ? s : "";
	if (Width(r, t.c_str(), iPx) <= iMaxW) return t;
	while (!t.empty() && Width(r, (t + "..").c_str(), iPx) > iMaxW) t.pop_back();
	return t + "..";
}

void RightText(CRenderer & r, int xRight, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(xRight - Width(r, s, iPx), y, s, c, iPx);
}

void CenterText(CRenderer & r, int cx, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(cx - Width(r, s, iPx) / 2, y, s, c, iPx);
}

// a main button: 0 plain, 1 gold, 2 greyed out
void Button(CRenderer & r, const RECT & rc, const char * pLabel, int iStyle, bool bOn)
{
	if (iStyle == 1) {
		r.StoneButton(rc.left, rc.top, rc.right, rc.bottom, 1);	// the main button: gold-rimmed stone
		r.TitleOut((rc.left + rc.right - r.TitleWidth(pLabel, 13)) / 2, rc.top + 6, pLabel, bOn ? RGB(255, 242, 199) : RGB(245, 219, 158), 13);
	}
	else if (iStyle == 2) {
		r.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 7, RGB(255, 255, 255), 8, RGB(255, 255, 255), 24);
		r.TitleOut((rc.left + rc.right - r.TitleWidth(pLabel, 13)) / 2, rc.top + 6, pLabel, RGB(90, 96, 104), 13);
	}
	else {
		r.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 7, RGB(18, 16, 14), bOn ? 235 : 200, kGoldLine, bOn ? 200 : 110);
		r.TitleOut((rc.left + rc.right - r.TitleWidth(pLabel, 13)) / 2, rc.top + 6, pLabel, bOn ? kWhite : RGB(224, 226, 230), 13);
	}
}

// a small choice: gold when chosen
void Chip(CRenderer & r, const RECT & rc, const char * pLabel, bool bSel, bool bOn)
{
	r.StoneButton(rc.left, rc.top, rc.right, rc.bottom, bSel ? 2 : bOn ? 1 : 0);
	r.TextOutSized((rc.left + rc.right - Width(r, pLabel, 11)) / 2, rc.top + 3, pLabel, bSel ? RGB(242, 199, 102) : (bOn ? RGB(255, 242, 199) : RGB(245, 219, 158)), 11);
}

// a round colour swatch; the chosen one has a gold ring
void Swatch(CRenderer & r, const RECT & rc, COLORREF c, bool bSel, bool bOn)
{
	const int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2, rad = (rc.right - rc.left) / 2 - 1;
	if (bSel) r.FillPanel(cx - rad - 3, cy - rad - 3, cx + rad + 4, cy + rad + 4, rad + 3, RGB(0, 0, 0), 0, kGoldLine, 255);
	else if (bOn) r.FillPanel(cx - rad - 2, cy - rad - 2, cx + rad + 3, cy + rad + 3, rad + 2, RGB(0, 0, 0), 0, RGB(255, 255, 255), 140);
	r.FillPanel(cx - rad, cy - rad, cx + rad + 1, cy + rad + 1, rad, c, 255, RGB(0, 0, 0), 120);
}

// a small square button with a minus, plus or arrow
void StepButton(CRenderer & r, const RECT & rc, char cSign, bool bEnabled, bool bOn)
{
	r.FillPanel(rc.left, rc.top, rc.right, rc.bottom, 4, RGB(255, 255, 255), bEnabled ? (bOn ? 40 : 18) : 6,
		RGB(255, 255, 255), bEnabled ? (bOn ? 120 : 60) : 25);
	const COLORREF c = bEnabled ? kWhite : RGB(90, 96, 104);
	const int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
	if (cSign == '-' || cSign == '+') r.FillPanel(cx - 3, cy - 1, cx + 4, cy + 1, 0, c, 255);
	if (cSign == '+') r.FillPanel(cx - 1, cy - 3, cx + 1, cy + 4, 0, c, 255);
	if (cSign == '<' || cSign == '>') {
		for (int k = 0; k < 4; k++) {		// a small arrow head
			const int x = (cSign == '<') ? cx - 2 + k : cx + 1 - k;
			r.FillPanel(x, cy - 3 + k, x + 1, cy + 4 - k, 0, c, 255);
		}
	}
}

// a filled ellipse, row by row
void Ellipse(CRenderer & r, int cx, int cy, float rx, float ry, COLORREF c, int a)
{
	const int iy = (int)ry;
	for (int dy = -iy; dy <= iy; dy++) {
		const float k = 1.f - (dy * dy) / (ry * ry);
		const int hw = (int)(rx * std::sqrt(k < 0.f ? 0.f : k) + 0.5f);
		if (hw > 0) r.FillPanel(cx - hw, cy + dy, cx + hw, cy + dy + 1, 0, c, a);
	}
}

// dots round an ellipse (a ring); fTurn moves them round
void Ring(CRenderer & r, int cx, int cy, float rx, float ry, int n, int iSize, COLORREF c, int a, float fTurn)
{
	for (int i = 0; i < n; i++) {
		const float ang = 6.28318f * i / n + fTurn;
		const int x = cx + (int)(std::cos(ang) * rx), y = cy + (int)(std::sin(ang) * ry);
		r.FillPanel(x - iSize / 2, y - iSize / 2, x - iSize / 2 + iSize, y - iSize / 2 + (iSize > 2 ? iSize - 1 : iSize), 0, c, a);
	}
}

// the stat a class leans on most: 0 balanced, 1 warrior, 2 mage, 3 master
int ClassOf(int iStr, int iDex, int iInt, int iMag, int iChr)
{
	const int war = iStr + iDex - 20, mag = iInt + iMag - 20, mas = iChr + iMag - 20;
	const int m = (std::max)(war, (std::max)(mag, mas));
	if (m < 3) return 0;
	if (m == war) return 1;
	if (m == mag) return 2;
	return 3;
}
const char * kClassName[4] = { "Balanced", "Warrior", "Mage", "Master" };
const COLORREF kClassCol[4] = { RGB(200, 200, 210), RGB(205, 225, 255), RGB(120, 170, 255), RGB(255, 214, 120) };

const int kStat[6] = { STAT_STR, STAT_VIT, STAT_DEX, STAT_INT, STAT_MAG, STAT_CHR };
const char * kStatName[6] = { "Strength", "Vitality", "Dexterity", "Intelligence", "Magic", "Charisma" };
const char * kStatTag[6] = { "STR", "VIT", "DEX", "INT", "MAG", "CHR" };
const char * kStatHelp[6] = {
	"Strength: more HP and stamina; heavier weapons and shields.",
	"Vitality: much more HP, faster recovery, a little less physical damage.",
	"Dexterity: hit more often and dodge more physical hits.",
	"Intelligence: learn more spells, cast them more surely, more MP.",
	"Magic: more magic resistance, more MP and faster MP recovery.",
	"Charisma: cheaper shopping; enough of it lets you found a guild." };

// presets, in kStat order (the client's own: Warrior, Mage, and the old Priest, now Master)
const int kPreset[3][6] = { { 14, 12, 14, 10, 10, 10 }, { 10, 12, 10, 14, 14, 10 }, { 14, 10, 10, 10, 12, 14 } };
const char * kPresetName[3] = { "Warrior", "Mage", "Master" };

// colours of the choices (the swatches are a guide: the game tints the sprites itself)
const COLORREF kSkin[3] = { RGB(122, 82, 58), RGB(232, 200, 170), RGB(166, 118, 82) };	// the game's: 1 dark, 2 light, 3 tan
const COLORREF kUnder[8] = { RGB(70, 74, 160), RGB(150, 40, 40), RGB(40, 120, 50), RGB(150, 130, 40),
	RGB(110, 50, 130), RGB(40, 120, 140), RGB(170, 170, 170), RGB(40, 40, 44) };
const int kHairTint[16][3] = { { 14, -5, -5 }, { 20, 0, 0 }, { 22, 13, -10 }, { 0, 10, 0 }, { 0, 0, 22 }, { -5, -5, 15 },
	{ 15, -5, 16 }, { -6, -6, -6 }, { 10, 3, 10 }, { 10, 3, -10 }, { -10, 3, 10 }, { 10, 3, 20 }, { 21, 3, 3 },
	{ 3, 3, 25 }, { 3, 11, 3 }, { 6, 8, 0 } };
COLORREF HairColour(int i)
{
	const auto ch = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
	return RGB(ch(110 + kHairTint[i][0] * 7), ch(90 + kHairTint[i][1] * 5), ch(80 + kHairTint[i][2] * 7));
}

// clicks: a press and a release on the same thing
struct Clicker {
	char cPrevLB = 0;
	int iPressed = -1;
	// the index clicked this frame, or -1
	int Update(char cLB, int iHit) {
		int iClick = -1;
		if (cLB != 0 && cPrevLB == 0) iPressed = iHit;
		else if (cLB == 0 && cPrevLB != 0) {
			if (iHit >= 0 && iHit == iPressed) iClick = iHit;
			iPressed = -1;
		}
		cPrevLB = cLB;
		return iClick;
	}
};

int HitTest(const RECT * pRc, int n, int x, int y)
{
	for (int i = 0; i < n; i++) if (pRc[i].right > pRc[i].left && InRect(pRc[i], x, y)) return i;
	return -1;
}

Clicker g_selectClick, g_createClick;

} // namespace

//////////////////////////////////////////////////////////////////////
// Shared: the town behind, darkened a little, and the pedestal with its spotlight

void CGame::DrawCharScreenBackdrop()
{
	DrawLoginScene();
	m_Renderer.FillPanel(0, 0, GAME_W, GAME_H, 0, RGB(0, 0, 0), 70);
	char cVer[32];
	wsprintf(cVer, "Client %d.%d", UPPER_VERSION, LOWER_VERSION);
	m_Renderer.TextOutSized(GAME_W - 12 - Width(m_Renderer, cVer, 11), GAME_H - 18, cVer, kDim, 11);
}

void CGame::CharPedestalSpot(int * pX, int * pY)
{
	*pX = GAME_W / 2;
	*pY = GAME_H - 112;
}

void CGame::DrawPedestal(float fGlow)
{
	CRenderer & R = m_Renderer;
	int cx, cy;
	CharPedestalSpot(&cx, &cy);
	const float t = timeGetTime() / 1000.f;

	// at night the pedestal lights the ground round it
	m_bLoginNight = LoginIsNight();
	if (NightLightsOn()) {
		CUiGpu * pG = R.UiGpuRecorder();
		if (pG != NULL) pG->AddLight(2.f * cx, 2.f * cy, 2.f * 120.f, 2.f * 46.f, 1.f, 0.88f, 0.66f, 0.9f);
	}
	m_bLoginNight = false;

	// the spotlight from above
	for (int y = 0; y < cy; y += 3) {
		const float u = (float)y / cy;
		const int hw = (int)(22.f + 70.f * u);
		R.FillPanel(cx - hw, y, cx + hw, y + 3, 0, RGB(255, 226, 170), (int)(28.f * u * u * (1.f + fGlow)));
	}
	// the stone, its edge and the gold rings
	Ellipse(R, cx, cy + 7, 66.f, 18.f, RGB(36, 32, 29), 255);
	Ellipse(R, cx, cy, 66.f, 18.f, RGB(74, 67, 59), 255);
	Ellipse(R, cx, cy, 58.f, 15.f, RGB(255, 230, 180), (int)(26.f * (1.f + fGlow)));
	Ring(R, cx, cy, 60.f, 15.5f, 110, 2, RGB(232, 188, 92), 150, 0.f);
	Ring(R, cx, cy, 40.f, 10.f, 70, 2, RGB(232, 188, 92), 70, 0.f);
	Ring(R, cx, cy, 51.f, 13.f, 24, 3, RGB(240, 200, 110), 170, t * 0.15f);
	// a soft burst (a character just made or chosen)
	if (fGlow > 0.f) {
		for (int i = 0; i < 4; i++) {
			const int r = 20 + i * 16;
			R.FillPanel(cx - r, cy - 40 - r, cx + r, cy - 40 + r, r, RGB(255, 226, 170), (int)(fGlow * (50 - i * 11)));
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Character select: the characters stand round a campfire on the grass in Elvine at night. Click
// one and the camera glides in (the scene drawn magnified on the graphics card: CUiGpu::SetZoom),
// they turn to face you and their details slide in; Back, Esc or the grass pulls out again.

namespace {

const float kFireX = 220 * 32.f + 625.f, kFireY = 156 * 32.f + 320.f;	// the fire, in Elvine (map pixels)
const float kZoomCircle = 1.5f, kZoomOne = 3.1f;
const int kSeatAng[4] = { 205, 250, 290, 335 };		// round the far side of the fire
const int kSeatOrder[4] = { 1, 2, 0, 3 };			// back to front

void SeatPos(int i, float * x, float * y)
{
	const float a = kSeatAng[i] * 3.14159265f / 180.f;
	*x = kFireX + std::cos(a) * 94.f;
	*y = kFireY + std::sin(a) * 46.f;
}
// facing the fire, a little towards the screen (1 N, 2 NE, 3 E, 4 SE, 5 S, 6 SW, 7 W, 8 NW)
int FaceDir(int i)
{
	float x, y;
	SeatPos(i, &x, &y);
	const int s = (int)std::lround(std::atan2(kFireY + 60.f - y, kFireX - x) / (3.14159265f / 4.f));
	static const int kDir[9] = { 7, 8, 1, 2, 3, 4, 5, 6, 7 };		// sectors -4..4
	return kDir[s + 4];
}

struct Camp {
	bool bInit = false;
	float x = kFireX, y = kFireY - 34.f, z = kZoomCircle;		// the camera: the map pixel at the centre, and the zoom
	float fx = 0, fy = 0, fz = 1, tx = 0, ty = 0, tz = 1;
	DWORD dwGlideT0 = 0, dwGlideMs = 0;
	int iSel = -1;
	int dir[4] = { 5, 5, 5, 5 }, tdir[4] = { 5, 5, 5, 5 };
	DWORD dwTurnAt[4] = {};
	DWORD dwChosenAt = 0;
	DWORD dwLastFire = 0;
	DWORD dwOpenT0 = 0;			// the screen opened: fading up from dark, the fire catching
};
Camp g_camp;

void Glide(float x, float y, float z, DWORD ms)
{
	Camp & c = g_camp;
	c.fx = c.x; c.fy = c.y; c.fz = c.z;
	c.tx = x; c.ty = y; c.tz = z;
	c.dwGlideT0 = timeGetTime();
	c.dwGlideMs = ms;
}
void GlideToSeat(int i, DWORD ms)
{
	float x, y;
	SeatPos(i, &x, &y);
	Glide(x + (0.5f - 0.34f) * GAME_W / kZoomOne, y - 30.f, kZoomOne, ms);	// the character a third of the way across
}

// how brightly the fire burns: layered smooth noise, never a steady rhythm (a slow swell, a quicker
// waver, a fast shimmer), and now and then a gust that makes it gutter. About 0.4 .. 1.
float Noise1(float x)
{
	static float v[256];
	static bool bInit = false;
	if (!bInit) {
		unsigned int r = 11;
		for (float & f : v) { r = r * 1103515245u + 12345u; f = (float)((r >> 8) & 0xFFFF) / 65535.f; }
		bInit = true;
	}
	const int i = (int)std::floor(x);
	const float f = x - i, u = f * f * (3.f - 2.f * f), a = v[i & 255], b = v[(i + 1) & 255];
	return a + (b - a) * u;
}
float FireLevel(float t)
{
	float L = 0.6f + 0.22f * Noise1(t * 1.7f) + 0.13f * Noise1(t * 5.3f + 40.f) + 0.07f * Noise1(t * 12.7f + 80.f);
	const float gust = Noise1(t * 0.55f + 200.f);
	if (gust > 0.74f) L -= (gust - 0.74f) * 1.8f * (0.5f + 0.5f * Noise1(t * 9.f + 300.f));
	return (std::max)(0.35f, L);
}
// the light's centre moves a little with the flames (map pixels), so the light on the ground sways
void FireWobble(float t, float * dx, float * dy)
{
	*dx = (Noise1(t * 2.3f + 500.f) - .5f) * 6.f;
	*dy = (Noise1(t * 1.9f + 600.f) - .5f) * 3.f;
}

// the fire: flames and embers (map pixels from the fire)
struct Spark { float x, y, vx, vy, age, life, s; };
std::vector<Spark> g_flames, g_embers;
float Rnd() { return (float)(rand() % 10000) / 10000.f; }
void StepFire(float dt, float t, float fCatch)
{
	// the flames burn higher as it flares and lean as it sways; now and then a pop of sparks
	const float L = FireLevel(t);
	float lean, unused;
	FireWobble(t, &lean, &unused);
	const int n = Rnd() < (0.2f + 0.6f * fCatch * L) ? 3 : 2;
	for (int i = 0; i < n && g_flames.size() < 160; i++)
		g_flames.push_back({ (Rnd() - .5f) * 14.f, -2.f, (Rnd() - .5f) * 6.f + lean * 1.5f, (-12.f - Rnd() * 18.f) * (0.45f + 0.55f * fCatch) * (0.6f + 0.5f * L), 0.f,
			(.35f + Rnd() * .5f) * (0.5f + 0.5f * fCatch) * (0.7f + 0.4f * L), 0.f });
	if (Rnd() < 0.12f && g_embers.size() < 40)
		g_embers.push_back({ (Rnd() - .5f) * 10.f, -10.f, (Rnd() - .5f) * 10.f, -26.f - Rnd() * 20.f, 0.f, 1.4f + Rnd() * 1.4f, Rnd() * 9.f });
	if (Rnd() < 0.006f * fCatch)
		for (int i = 0; i < 8 && g_embers.size() < 48; i++)
			g_embers.push_back({ (Rnd() - .5f) * 6.f, -6.f, (Rnd() - .5f) * 40.f, -40.f - Rnd() * 50.f, 0.f, .5f + Rnd() * .8f, Rnd() * 9.f });
	for (std::vector<Spark> * a : { &g_flames, &g_embers })
		for (size_t i = a->size(); i-- > 0; ) {
			Spark & p = (*a)[i];
			p.age += dt;
			p.x += p.vx * dt + (a == &g_embers ? std::sin(t * 3.f + p.s) * 6.f * dt : 0.f);
			p.y += p.vy * dt;
			p.vx *= 0.97f;
			if (p.age > p.life) a->erase(a->begin() + i);
		}
}

// the character last played (the latest date), or -1
int LastPlayed(CCharInfo * const * pList)
{
	long long best = -1;
	int iBest = -1;
	for (int i = 0; i < 4; i++) {
		CCharInfo * p = pList[i];
		if (p == NULL || p->m_sSex == NULL) continue;
		const long long v = (long long)p->m_iYear * 1000000 + p->m_iMonth * 60000 + p->m_iDay * 1700 + p->m_iHour * 70 + p->m_iMinute;
		if (v > best) { best = v; iBest = i; }
	}
	return iBest;
}

} // namespace

// the screen opens: all round the fire, the camera settling down onto them
void CGame::CampReset()
{
	Camp & c = g_camp;
	c.bInit = true;
	c.iSel = -1;
	c.dwOpenT0 = timeGetTime();
	for (int i = 0; i < 4; i++) c.dir[i] = c.tdir[i] = FaceDir(i);
	c.x = kFireX; c.y = kFireY - 60.f; c.z = 1.15f;
	Glide(kFireX, kFireY - 34.f, kZoomCircle, 1800);
	const int iLast = LastPlayed(m_pCharList);
	m_cCurFocus = (char)((iLast >= 0 ? iLast : 0) + 1);
}

int CGame::CampSelected() { return g_camp.iSel; }

void CGame::CampChoose(int i)
{
	if (i < 0 || i > 3) return;
	Camp & c = g_camp;
	if (c.iSel >= 0 && c.iSel != i) c.tdir[c.iSel] = FaceDir(c.iSel);
	const bool bMoving = c.iSel >= 0;
	c.iSel = i;
	c.tdir[i] = 5;			// turns to face you
	c.dwChosenAt = timeGetTime();
	m_cCurFocus = (char)(i + 1);
	GlideToSeat(i, bMoving ? 560 : 720);
}

void CGame::CampBack()
{
	Camp & c = g_camp;
	if (c.iSel >= 0) c.tdir[c.iSel] = FaceDir(c.iSel);
	c.iSel = -1;
	Glide(kFireX, kFireY - 34.f, kZoomCircle, 620);
}

// the buttons and the details panel: Log out and Change password with everyone round the fire;
// Back, Delete and Play with one chosen
void CGame::CampRects(RECT * pInfo, RECT * pBack, RECT * pDelete, RECT * pPlay, RECT * pLogout, RECT * pChangePw)
{
	const int W = GAME_W, H = GAME_H;
	if (pInfo) { const int pw = (std::max)(176, W * 30 / 100), x0 = W * 60 / 100; SetRect(pInfo, x0, H * 9 / 100, (std::min)(x0 + pw, W - 12), H * 9 / 100 + 176); }
	const int bw = 84, bh = 26, gap = 8, x0 = (W - (3 * bw + 2 * gap)) / 2, y = H - 46;
	if (pBack) SetRect(pBack, x0, y, x0 + bw, y + bh);
	if (pDelete) SetRect(pDelete, x0 + bw + gap, y, x0 + 2 * bw + gap, y + bh);
	if (pPlay) SetRect(pPlay, x0 + 2 * (bw + gap), y, x0 + 3 * bw + 2 * gap, y + bh);
	if (pLogout) SetRect(pLogout, 16, y, 16 + bw, y + bh);
	if (pChangePw) SetRect(pChangePw, 16 + bw + 12, y + 4, 16 + bw + 12 + 110, y + 22);
}

bool CGame::CharSlotFilled(int i)
{
	if (i < 0 || i > 3) return false;
	CCharInfo * p = m_pCharList[i];
	return p != NULL && p->m_sSex != NULL && m_Misc.bCheckValidString(p->m_cName) == TRUE;
}

void CGame::_Draw_SelectScreen(int msX, int msY, bool bIgnoreFocus)
{
	CRenderer & R = m_Renderer;
	Camp & c = g_camp;
	if (!c.bInit) CampReset();
	const DWORD dwNow = timeGetTime();
	const float t = dwNow / 1000.f;
	const int W = GAME_W, H = GAME_H;
	const bool bHere = m_cGameMode == GAMEMODE_ONSELECTCHARACTER && !bIgnoreFocus;	// (not behind a message)

	// the camera gliding, the characters turning (a direction at a time, the short way round)
	if (c.dwGlideMs > 0) {
		const float e = Ease((float)(int)(dwNow - c.dwGlideT0) / c.dwGlideMs);
		c.x = c.fx + (c.tx - c.fx) * e; c.y = c.fy + (c.ty - c.fy) * e; c.z = c.fz + (c.tz - c.fz) * e;
		if (e >= 1.f) c.dwGlideMs = 0;
	}
	for (int i = 0; i < 4; i++)
		if (c.dir[i] != c.tdir[i] && dwNow - c.dwTurnAt[i] > 70) {
			const int d = ((c.tdir[i] - c.dir[i]) + 8) % 8;
			c.dir[i] = ((c.dir[i] - 1 + (d <= 4 ? 1 : 7)) % 8) + 1;
			c.dwTurnAt[i] = dwNow;
		}
	const float dt = (c.dwLastFire == 0) ? 0.016f : (std::min)(0.05f, (dwNow - c.dwLastFire) / 1000.f);
	c.dwLastFire = dwNow;
	StepFire(dt, t, 0.3f + 0.7f * Ease((float)(int)(dwNow - c.dwOpenT0) / 1400.f));
	const float zk = Clamp01((c.z - kZoomCircle) / (kZoomOne - kZoomCircle));	// 0 round the fire .. 1 in close

	// screen positions: the scene is drawn as if unzoomed (the map pixel minus the view's corner),
	// then magnified round the middle of the screen
	const float fx = c.x - W * 0.5f, fy = c.y - H * 0.5f;
	const int vx = (int)std::floor(fx), vy = (int)std::floor(fy);
	auto onScreen = [&](float mx, float my, float * sx, float * sy) {	// after the zoom
		*sx = W * 0.5f + (mx - c.x) * c.z; *sy = H * 0.5f + (my - c.y) * c.z; };
	// the seat under the mouse
	int iHover = -1;
	if (bHere) {
		const float wx = c.x + (msX - W * 0.5f) / c.z, wy = c.y + (msY - H * 0.5f) / c.z;
		for (int i = 0; i < 4; i++) {
			float x, y;
			SeatPos(i, &x, &y);
			if (wx > x - 17 && wx < x + 17 && wy > y - 66 && wy < y + 8) iHover = i;
			else if (zk < 0.2f && std::fabs(wx - x) < 24.f && std::fabs(wy - (y - 76.f)) < 10.f) iHover = i;	// its name plate
		}
	}

	CUiGpu * pG = R.UiGpuRecorder();
	if (pG != NULL) pG->SetZoom((float)W, (float)H, c.z);	// (the middle of the 2x picture)
	DrawCampGround(fx, fy);		// UI\LoginScene.cpp: the grass, the houses round it, their lamps

	// the characters, back to front, standing; an empty seat is a faint outline
	R.m_iShiftX2 = -((int)std::floor(fx * 2.f) - 2 * vx);	// the half pixel, as the ground
	R.m_iShiftY2 = -((int)std::floor(fy * 2.f) - 2 * vy);
	m_bMenuStand = true;
	for (int k = 0; k < 4; k++) {
		const int i = kSeatOrder[k];
		float x, y;
		SeatPos(i, &x, &y);
		const int sx = (int)std::floor(x) - vx, sy = (int)std::floor(y) - vy;
		if (CharSlotFilled(i)) {
			SetMenuCharacter(i, c.dir[i]);
			DrawObject_OnMove_ForMenu(0, 0, sx, sy, FALSE, dwNow, 0, 0);
		}
		else if (m_pCharList[i] == NULL) {
			SetMenuCharacter(-2, c.dir[i]);		// a plain body, see-through
			m_bMenuGhost = true;
			DrawObject_OnMove_ForMenu(0, 0, sx, sy, TRUE, dwNow, 0, 0);
			m_bMenuGhost = false;
		}
	}
	m_bMenuStand = false;
	R.m_iShiftX2 = R.m_iShiftY2 = 0;

	// night, and the firelight (a soft light on the one chosen too): the brightness wavers, with a
	// slight change of reach; the light sways and reddens as it dims; stacked lights make it fall off
	// steeply from a hot centre into a long faint reach, as real light does
	const float flick = FireLevel(t);
	float wx, wy;
	FireWobble(t, &wx, &wy);
	// opening: the screen fades up from dark (signing in dimmed the town) and the fire catches
	const float tOpen = (float)(int)(dwNow - c.dwOpenT0) / 1000.f;
	const float fCatch = 0.3f + 0.7f * Ease(tOpen / 1.4f);
	const float fsx = kFireX - vx, fsy = kFireY - vy;
	if (pG != NULL) {
		const float k = fCatch * flick * 1.15f, reach = 0.94f + 0.08f * flick;
		const float lx = 2.f * (fsx + wx), ly = 2.f * (fsy - 7.f + wy), gG = 0.55f + 0.2f * flick, gB = 0.27f + 0.15f * flick;
		pG->SetLightPass(0.17f, 0.2f, 0.33f);
		pG->AddLight(lx, ly, 2.f * 240.f * reach, 2.f * 150.f * reach, 0.95f, gG - 0.08f, gB - 0.06f, 0.5f * k);	// the long faint reach
		pG->AddLight(lx, ly, 2.f * 120.f * reach, 2.f * 76.f * reach, 1.f, gG + 0.08f, gB + 0.06f, 0.95f * k);
		pG->AddLight(lx, ly, 2.f * 42.f * reach, 2.f * 28.f * reach, 1.f, 0.86f, 0.62f, 1.f * k);			// the hot centre
		pG->AddLight(lx, ly - 2.f * 26.f, 2.f * 80.f, 2.f * 64.f, 1.f, gG + 0.15f, gB + 0.12f, 0.42f * k);	// up on faces
		if (c.iSel >= 0 && zk > 0.f) {
			float x, y;
			SeatPos(c.iSel, &x, &y);
			pG->AddLight(2.f * (x - vx + 10.f), 2.f * (y - vy - 40.f), 2.f * 60.f, 2.f * 60.f, 1.f, 0.88f, 0.68f, 0.9f * zk);
		}
	}

	// after the light: the fire pit (coloured as the fire lights it), the flames and embers
	{
		const int cx = (int)fsx, cy = (int)fsy;
		for (int i = 0; i < 12; i++) {		// a ring of stones
			const float a = i / 12.f * 6.2832f;
			const int x = cx + (int)std::lround(std::cos(a) * 17.f), y = cy + (int)std::lround(std::sin(a) * 8.5f);
			const bool bFront = std::sin(a) > 0.f;
			R.FillPanel(x - 4, y - 2, x + 4, y + 3, 2, (i % 2) ? RGB(150, 118, 92) : RGB(128, 100, 80), 255, RGB(40, 28, 22), bFront ? 200 : 140);
		}
		for (int s = -12; s <= 12; s++) {		// two crossed logs
			const int y1 = cy + (int)std::lround(s * 0.45f), y2 = cy + 1 - (int)std::lround(s * 0.45f);
			R.FillPanel(cx + s, y1 - 2, cx + s + 2, y1 + 2, 0, RGB(92, 56, 30), 255);
			R.FillPanel(cx + s, y2 - 2, cx + s + 2, y2 + 2, 0, RGB(84, 50, 26), 255);
		}
		R.FillPanel(cx - 7, cy - 3, cx + 8, cy + 4, 3, RGB(255, 130, 50), 200);
		for (const Spark & p : g_flames) {
			const float k = p.age / p.life;
			const COLORREF col = k < .25f ? RGB(255, 240, 170) : (k < .5f ? RGB(255, 190, 80) : (k < .75f ? RGB(240, 110, 40) : RGB(150, 40, 20)));
			const int x = cx + (int)std::lround(p.x), y = cy + (int)std::lround(p.y), sz = k < .5f ? 2 : 1;
			R.FillPanel(x, y, x + sz, y + sz, 0, col, (int)(230 * (1.f - k * .6f)));
		}
		for (const Spark & p : g_embers) {
			const float k = p.age / p.life;
			const int x = cx + (int)std::lround(p.x), y = cy + (int)std::lround(p.y);
			R.FillPanel(x, y, x + 1, y + 1, 0, RGB(255, (int)(170 - k * 80), 80), (int)(230 * (1.f - k)));
		}
		if (pG != NULL) pG->AddLightGlow(2.f * (fsx + wx * 0.5f), 2.f * (fsy - 6.f), 2.f * 16.f * (0.8f + 0.25f * flick), 1.f, 0.55f + 0.2f * flick, 0.3f, 0.9f * fCatch * flick);
	}
	// a gold ring at the feet of the one pointed at or chosen, and a burst as they are chosen
	{
		const int iRing = (c.iSel >= 0) ? c.iSel : iHover;
		if (iRing >= 0) {
			float x, y;
			SeatPos(iRing, &x, &y);
			Ring(R, (int)(x - vx), (int)(y - vy) + 1, 20.f, 7.f, 70, 1, RGB(240, 200, 110), 230, 0.f);
		}
		const float u = (dwNow - c.dwChosenAt) / 1100.f;
		if (c.iSel >= 0 && u < 1.f) {
			float x, y;
			SeatPos(c.iSel, &x, &y);
			for (int i = 0; i < 26; i++) {
				const float a = i / 26.f * 6.2832f + i, r = 6.f + u * 22.f, h = u * (14.f + (i % 5) * 7.f);
				const int px = (int)(x - vx + std::cos(a) * r), py = (int)(y - vy + std::sin(a) * r * .35f - h);
				R.FillPanel(px, py, px + 1, py + 1, 0, RGB(255, 200 + (i % 3) * 20, 120), (int)(230 * (1.f - u)));
			}
		}
	}
	if (pG != NULL) pG->EndZoom();

	// --- on the screen, unzoomed ---
	if (zk > 0.f) R.FillEdgeGlow(0, 0, W, H, W / 3, RGB(0, 0, 0), (int)(170 * zk));	// the others fall into shadow

	// round the fire: the title and a name plate over each one (fading as the camera goes in)
	const int iLast = LastPlayed(m_pCharList);
	if (zk < 1.f) {
		R.m_iFadeA = (int)(255 * (1.f - zk) * Clamp01((tOpen - 0.6f) / 0.4f));
		R.TitleOut(16, 14, "CHARACTERS", kGold, 15);
		for (int i = 0; i < 4; i++) {
			R.m_iFadeA = (int)(255 * (1.f - zk) * Clamp01((tOpen - 0.9f - i * 0.15f) / 0.4f));	// one after another
			if (R.m_iFadeA <= 0) continue;
			float x, y, sx, sy;
			SeatPos(i, &x, &y);
			onScreen(x, y - 66.f, &sx, &sy);
			const bool bOn = (i == iHover);
			char cName[16] = {}, cSub[48];
			if (CharSlotFilled(i)) {
				CCharInfo * p = m_pCharList[i];
				for (int k = 0; k < 10 && p->m_cName[k]; k++) cName[k] = (char)toupper((unsigned char)p->m_cName[k]);
				wsprintf(cSub, "Lv %d \xB7 %s", (int)p->m_sLevel, kClassName[ClassOf(p->m_sStr, p->m_sDex, p->m_sInt, p->m_sMag, p->m_sChr)]);
			}
			else if (m_pCharList[i] == NULL) { strcpy(cName, "+ CREATE"); strcpy(cSub, "Empty slot"); }
			else { strcpy(cName, "UNAVAILABLE"); cSub[0] = 0; }
			const int w = (std::max)(R.TitleWidth(cName, 13), Width(R, cSub, 10)) + 16, bh = 30;
			const int bx = (int)sx - w / 2, by = (int)sy - bh;
			R.FillPanel(bx, by, bx + w, by + bh, 5, RGB(10, 12, 16), 190, bOn ? RGB(240, 200, 110) : RGB(170, 140, 84), bOn ? 240 : 115);
			R.TitleOut((int)sx - R.TitleWidth(cName, 13) / 2, by + 3, cName, CharSlotFilled(i) ? (bOn ? RGB(255, 231, 168) : kGold) : (bOn ? kWhite : RGB(200, 204, 212)), 13);
			CenterText(R, (int)sx, by + 17, cSub, RGB(163, 171, 181), 10);
			if (i == iLast) R.TitleOut((int)sx - R.TitleWidth("LAST PLAYED", 9) / 2, by - 12, "LAST PLAYED", RGB(232, 188, 92), 9);
		}
		R.m_iFadeA = 255;
	}

	// in close: the details, sliding in
	RECT rcInfo, rcBack, rcDel, rcPlay, rcLogout, rcPw;
	CampRects(&rcInfo, &rcBack, &rcDel, &rcPlay, &rcLogout, &rcPw);
	if (c.iSel >= 0) {
		const float k = 1.f - std::pow(1.f - Clamp01(((float)(int)(dwNow - c.dwChosenAt) - 150.f) / 450.f), 3.f);
		const int iSel = c.iSel;
		R.m_iFadeA = (int)(255 * k);
		OffsetRect(&rcInfo, (int)((1.f - k) * 20.f), 0);
		const bool bChar = CharSlotFilled(iSel);
		if (!bChar) rcInfo.bottom = rcInfo.top + 84;
		Panel(R, rcInfo);
		const int x0 = rcInfo.left + 12, x1 = rcInfo.right - 12, y0 = rcInfo.top;
		if (bChar) {
			CCharInfo * p = m_pCharList[iSel];
			char cName[12] = {}, cMap[64] = {}, cMapId[12] = {}, cLv[16];
			memcpy(cName, p->m_cName, 10);
			for (char * q = cName; *q; q++) *q = (char)toupper((unsigned char)*q);
			R.TitleOut(x0, y0 + 10, cName, kGold, 15);
			memcpy(cMapId, p->m_cMapName, 10);
			GetOfficialMapName(cMapId, cMap);
			if (cMap[0] == 0) strcpy(cMap, cMapId);
			wsprintf(cLv, "%d", (int)p->m_sLevel);
			const int iClass = ClassOf(p->m_sStr, p->m_sDex, p->m_sInt, p->m_sMag, p->m_sChr);
			const struct { const char * k; const char * v; } row[3] = { { "Level", cLv }, { "Location", cMap }, { "Class", kClassName[iClass] } };
			for (int r = 0; r < 3; r++) {
				const int y = y0 + 36 + r * 18;
				R.TextOutSized(x0, y, row[r].k, kGrey, 12);
				const std::string v = Fit(R, row[r].v, 12, (x1 - x0) * 6 / 10);
				R.TitleOut(x1 - R.TitleWidth(v.c_str(), 12), y, v.c_str(), kWhite, 12);
			}
			const int v[6] = { p->m_sStr, p->m_sVit, p->m_sDex, p->m_sInt, p->m_sMag, p->m_sChr };
			const int cw = (x1 - x0 - 8) / 3;
			for (int s = 0; s < 6; s++) {
				const int cx0 = x0 + (s % 3) * (cw + 4), cy0 = y0 + 94 + (s / 3) * 22;
				R.FillPanel(cx0, cy0, cx0 + cw, cy0 + 18, 4, RGB(255, 255, 255), 12);
				R.TextOutSized(cx0 + 5, cy0 + 3, kStatTag[s], kGrey, 10);
				char cV[12];
				wsprintf(cV, "%d", v[s]);
				RightText(R, cx0 + cw - 5, cy0 + 2, cV, kWhite, 12);
			}
			const int lv = p->m_sLevel;
			const int hpmpsp[3] = { p->m_sVit * 3 + lv * 2 + p->m_sStr / 2, p->m_sMag * 2 + lv * 2 + p->m_sInt / 2, lv * 2 + p->m_sStr * 2 };
			const char * pTag[3] = { "HP", "MP", "SP" };
			for (int s = 0; s < 3; s++) {
				const int bx = x0 + s * (cw + 4), by = y0 + 142;
				R.FillPanel(bx, by, bx + cw, by + 26, 5, RGB(255, 255, 255), 12);
				CenterText(R, bx + cw / 2, by + 2, pTag[s], kGrey, 9);
				char cV[16];
				wsprintf(cV, "%d", hpmpsp[s]);
				R.TitleOut(bx + (cw - R.TitleWidth(cV, 13)) / 2, by + 11, cV, kWhite, 13);
			}
		}
		else {
			R.TitleOut(x0, y0 + 10, "EMPTY SLOT", kGold, 15);
			R.TextOutSized(x0, y0 + 38, "No one sits here yet. Make a new", kGrey, 12);
			R.TextOutSized(x0, y0 + 55, "character to join the others.", kGrey, 12);
		}
		R.m_iFadeA = 255;
	}

	// the hint, and the buttons
	const bool bChosen = c.iSel >= 0, bChar = bChosen && CharSlotFilled(c.iSel);
	const char * pHint = !bChosen ? "Choose a character to take a closer look."
		: (bChar ? "Press Play to enter the world." : "An empty seat at the fire.");
	CenterText(R, W / 2, rcPlay.top - 20, pHint, RGB(186, 192, 200), 12);
	if (bChosen) {
		const bool bCanDelete = bChar && CanDeleteCharacter(c.iSel);
		Button(R, rcBack, "BACK", 0, bHere && InRect(rcBack, msX, msY));
		if (bChar) {
			Button(R, rcDel, "DELETE", bCanDelete ? 0 : 2, bHere && bCanDelete && InRect(rcDel, msX, msY));
			Button(R, rcPlay, "PLAY", 1, bHere && InRect(rcPlay, msX, msY));
		}
		else Button(R, rcDel, "CREATE", 1, bHere && InRect(rcDel, msX, msY));
	}
	else {
		Button(R, rcLogout, "LOG OUT", 0, bHere && InRect(rcLogout, msX, msY));
		const bool bOn = bHere && InRect(rcPw, msX, msY);
		R.TextOutSized(rcPw.left, rcPw.top, "Change password", bOn ? kGold : kDim, 11);
	}
	char cVer[32];
	wsprintf(cVer, "Client %d.%d", UPPER_VERSION, LOWER_VERSION);
	R.TextOutSized(W - 12 - Width(R, cVer, 11), H - 18, cVer, kDim, 11);
	if (tOpen < 1.f) R.FillPanel(0, 0, W, H, 0, RGB(0, 0, 0), (int)(255.f * (1.f - Ease(tOpen))));	// fading up
}

// what was clicked on the select screen: 1-4 a character (or empty seat), 5 Play (or Create on an
// empty seat), 7 Delete, 8 Change password, 9 Log out, 10 Back (or the grass, in close); 0 nothing
int CGame::SelectScreenClick(int msX, int msY, char cLB)
{
	Camp & c = g_camp;
	const int W = GAME_W, H = GAME_H;
	RECT rc[6];
	CampRects(NULL, &rc[0], &rc[1], &rc[2], &rc[3], &rc[4]);
	const bool bChosen = c.iSel >= 0, bChar = bChosen && CharSlotFilled(c.iSel);
	int iHit = -1;
	if (bChosen) {
		if (InRect(rc[0], msX, msY)) iHit = 10;
		else if (InRect(rc[1], msX, msY)) iHit = bChar ? 7 : 5;
		else if (bChar && InRect(rc[2], msX, msY)) iHit = 5;
	}
	else {
		if (InRect(rc[3], msX, msY)) iHit = 9;
		else if (InRect(rc[4], msX, msY)) iHit = 8;
	}
	if (iHit < 0) {
		const float wx = c.x + (msX - W * 0.5f) / c.z, wy = c.y + (msY - H * 0.5f) / c.z;
		const float zk = Clamp01((c.z - kZoomCircle) / (kZoomOne - kZoomCircle));
		for (int i = 0; i < 4; i++) {
			float x, y;
			SeatPos(i, &x, &y);
			if ((wx > x - 17 && wx < x + 17 && wy > y - 66 && wy < y + 8)
				|| (zk < 0.2f && std::fabs(wx - x) < 24.f && std::fabs(wy - (y - 76.f)) < 10.f)) iHit = i + 1;
		}
		if (iHit < 0 && bChosen) {		// the grass, round the details: back out
			RECT rcInfo;
			CampRects(&rcInfo, NULL, NULL, NULL, NULL, NULL);
			if (!InRect(rcInfo, msX, msY)) iHit = 10;
		}
	}
	const int iClick = g_selectClick.Update(cLB, iHit);
	return (iClick < 0) ? 0 : iClick;
}

//////////////////////////////////////////////////////////////////////
// Character creation: the look on the left, the name at the top, the stats on the right

void CGame::CreateRects(CreateLayout * L)
{
	const int W = GAME_W, H = GAME_H;
	SetRect(&L->look, 16, 18, 196, 18 + 172);
	const int ax = L->look.left + 74, ay = L->look.top;
	SetRect(&L->gender[0], ax, ay + 34, ax + 44, ay + 52);
	SetRect(&L->gender[1], ax + 48, ay + 34, ax + 100, ay + 52);
	for (int i = 0; i < 3; i++) SetRect(&L->skin[i], ax + i * 19, ay + 60, ax + i * 19 + 16, ay + 76);
	SetRect(&L->hairPrev, ax, ay + 84, ax + 16, ay + 100);
	SetRect(&L->hairNext, ax + 60, ay + 84, ax + 76, ay + 100);
	for (int i = 0; i < 16; i++) SetRect(&L->hairCol[i], ax + (i % 8) * 13, ay + 108 + (i / 8) * 14, ax + (i % 8) * 13 + 12, ay + 108 + (i / 8) * 14 + 12);
	for (int i = 0; i < 8; i++) SetRect(&L->underCol[i], ax + i * 13, ay + 146, ax + i * 13 + 12, ay + 158);

	SetRect(&L->name, W / 2 - 80, 32, W / 2 + 80, 58);

	SetRect(&L->stats, W - 16 - 180, 18, W - 16, 18 + 250);
	const int sx0 = L->stats.left + 14, sx1 = L->stats.right - 14, sy = L->stats.top;
	const int pw[3] = { 52, 40, 48 };
	int px = sx0;
	for (int i = 0; i < 3; i++) { SetRect(&L->preset[i], px, sy + 34, px + pw[i], sy + 52); px += pw[i] + 4; }
	for (int k = 0; k < 6; k++) {
		const int ry = sy + 62 + k * 19;
		SetRect(&L->minus[k], sx1 - 60, ry, sx1 - 46, ry + 14);
		SetRect(&L->plus[k], sx1 - 14, ry, sx1, ry + 14);
		SetRect(&L->statRow[k], sx0, ry - 2, sx1, ry + 16);
	}
	const int bw = 84, bh = 26, gap = 10, x0 = (W - (2 * bw + gap)) / 2, by = H - 50;
	SetRect(&L->back, x0, by, x0 + bw, by + bh);
	SetRect(&L->create, x0 + bw + gap, by, x0 + 2 * bw + gap, by + bh);
}

bool CGame::_Draw_CreateScreen(const char * pName, int msX, int msY, int iPoint)
{
	CRenderer & R = m_Renderer;
	const DWORD dwTime = timeGetTime();
	DrawCharScreenBackdrop();
	CreateLayout L;
	CreateRects(&L);
	const bool bHere = m_cGameMode == GAMEMODE_ONCREATENEWCHARACTER;
	auto on = [&](const RECT & rc) { return bHere && InRect(rc, msX, msY); };

	// the look
	Panel(R, L.look);
	const int lx = L.look.left + 14, ly = L.look.top;
	R.TitleOut(lx, ly + 10, "APPEARANCE", kGold, 14);
	R.TextOutSized(lx, ly + 36, "Gender", kGrey, 12);
	Chip(R, L.gender[0], "Male", m_cGender == 1, on(L.gender[0]));
	Chip(R, L.gender[1], "Female", m_cGender == 2, on(L.gender[1]));
	R.TextOutSized(lx, ly + 61, "Skin", kGrey, 12);
	for (int i = 0; i < 3; i++) Swatch(R, L.skin[i], kSkin[i], m_cSkinCol == i + 1, on(L.skin[i]));
	R.TextOutSized(lx, ly + 85, "Hair style", kGrey, 12);
	StepButton(R, L.hairPrev, '<', true, on(L.hairPrev));
	StepButton(R, L.hairNext, '>', true, on(L.hairNext));
	{
		char c[16];
		wsprintf(c, "%d / 8", m_cHairStyle + 1);
		CenterText(R, (L.hairPrev.right + L.hairNext.left) / 2, ly + 85, c, kWhite, 12);
	}
	R.TextOutSized(lx, ly + 114, "Hair colour", kGrey, 12);
	for (int i = 0; i < 16; i++) Swatch(R, L.hairCol[i], HairColour(i), m_cHairCol == i, on(L.hairCol[i]));
	R.TextOutSized(lx, ly + 145, "Underwear", kGrey, 12);
	for (int i = 0; i < 8; i++) Swatch(R, L.underCol[i], kUnder[i], m_cUnderCol == i, on(L.underCol[i]));

	// the name
	{
		CenterText(R, (L.name.left + L.name.right) / 2, L.name.top - 14, "CHARACTER NAME", kGrey, 10);
		const bool bFocus = (m_cCurFocus == 1);
		R.FillPanel(L.name.left, L.name.top, L.name.right, L.name.bottom, 7, RGB(10, 12, 16), 225,
			bFocus ? kGoldLine : RGB(255, 255, 255), bFocus ? 220 : 60);
		const bool bValid = pName[0] == 0 || m_Misc.bCheckValidName((char *)pName) != FALSE;
		const COLORREF c = bValid ? kWhite : RGB(255, 140, 140);
		if (pName[0] == 0) CenterText(R, (L.name.left + L.name.right) / 2, L.name.top + 6, "Choose a name", kDim, 12);
		const int w = pName[0] ? R.TitleWidth(pName, 15) : 0, tx = (L.name.left + L.name.right - w) / 2;
		if (pName[0]) R.TitleOut(tx, L.name.top + 4, pName, c, 15);
		if (bFocus && ((dwTime / 450) % 2) == 0) {
			const int cx = pName[0] ? tx + w + 1 : (L.name.left + L.name.right) / 2 + Width(R, "Choose a name", 12) / 2 + 3;
			R.FillPanel(cx, L.name.top + 6, cx + 1, L.name.bottom - 6, 0, RGB(255, 255, 255), 230);
		}
	}

	// the stats
	Panel(R, L.stats);
	const int sx0 = L.stats.left + 14, sx1 = L.stats.right - 14, sy = L.stats.top;
	R.TitleOut(sx0, sy + 10, "STATS", kGold, 14);
	int v[6];
	for (int k = 0; k < 6; k++) v[k] = m_createStat[kStat[k]];
	for (int i = 0; i < 3; i++) {
		bool bSame = true;
		for (int k = 0; k < 6; k++) if (v[k] != kPreset[i][k]) bSame = false;
		Chip(R, L.preset[i], kPresetName[i], bSame, on(L.preset[i]));
	}
	for (int k = 0; k < 6; k++) {
		const int ry = L.minus[k].top;
		R.TextOutSized(sx0, ry, kStatName[k], on(L.statRow[k]) ? kWhite : RGB(200, 204, 212), 12);
		StepButton(R, L.minus[k], '-', v[k] > 10, on(L.minus[k]));
		StepButton(R, L.plus[k], '+', v[k] < 14 && iPoint > 0, on(L.plus[k]));
		char c[8];
		wsprintf(c, "%d", v[k]);
		CenterText(R, (L.minus[k].right + L.plus[k].left) / 2, ry, c, kWhite, 12);
	}
	{
		const int y = sy + 62 + 6 * 19 + 2;
		R.TextOutSized(sx0, y, "Points left", kGrey, 12);
		char c[8];
		wsprintf(c, "%d", iPoint);
		R.TitleOut(sx1 - R.TitleWidth(c, 13), y - 1, c, iPoint > 0 ? kGold : RGB(140, 146, 154), 13);
	}
	{	// HP, MP and SP at level 1 (the game's formulas)
		const int hpmpsp[3] = { v[1] * 3 + 2 + v[0] / 2, v[4] * 2 + 2 + v[3] / 2, v[0] * 2 + 2 };
		const char * pTag[3] = { "HP", "MP", "SP" };
		const int cw = (sx1 - sx0 - 8) / 3, by = sy + 200;
		for (int k = 0; k < 3; k++) {
			const int bx = sx0 + k * (cw + 4);
			R.FillPanel(bx, by, bx + cw, by + 26, 5, RGB(255, 255, 255), 10);
			CenterText(R, bx + cw / 2, by + 2, pTag[k], kGrey, 9);
			char c[16];
			wsprintf(c, "%d", hpmpsp[k]);
			R.TitleOut(bx + (cw - R.TitleWidth(c, 13)) / 2, by + 11, c, kWhite, 13);
		}
		const int iClass = ClassOf(v[0], v[2], v[3], v[4], v[5]);
		char cLean[32];
		if (iClass == 0) strcpy(cLean, "BALANCED");
		else wsprintf(cLean, "LEANING %s", iClass == 1 ? "WARRIOR" : (iClass == 2 ? "MAGE" : "MASTER"));
		R.TitleOut((L.stats.left + L.stats.right - R.TitleWidth(cLean, 11)) / 2, sy + 232, cLean, kClassCol[iClass], 11);
	}

	// the character on the pedestal
	DrawPedestal(0.f);
	SetMenuCharacter(-1);
	{
		int cx, cy;
		CharPedestalSpot(&cx, &cy);
		DrawObject_OnMove_ForMenu(0, 0, cx, cy - 6, FALSE, dwTime, 0, 0);
	}

	// what's left to do (or what the stat under the mouse does), and the buttons
	const bool bReady = pName[0] != 0 && iPoint == 0 && m_Misc.bCheckValidName((char *)pName) != FALSE;
	const char * pHint = NULL;
	for (int k = 0; k < 6; k++) if (on(L.statRow[k])) pHint = kStatHelp[k];
	char cHint[96];
	if (pHint == NULL) {
		if (pName[0] == 0) pHint = "Pick your look, then choose a name.";
		else if (m_Misc.bCheckValidName((char *)pName) == FALSE) pHint = "Names use letters and numbers only.";
		else if (iPoint > 0) { wsprintf(cHint, "Spend your last %d stat point%s, or pick a class above.", iPoint, iPoint == 1 ? "" : "s"); pHint = cHint; }
		else pHint = "Ready. Press Create.";
	}
	CenterText(R, GAME_W / 2, L.create.top - 22, pHint, RGB(186, 192, 200), 12);
	Button(R, L.back, "BACK", 0, on(L.back));
	Button(R, L.create, "CREATE", bReady ? 1 : 2, bReady && on(L.create));
	return bReady;
}

// what was clicked on the creation screen (CreateClick ids), or CREATECLICK_NONE
int CGame::CreateScreenClick(int msX, int msY, char cLB)
{
	CreateLayout L;
	CreateRects(&L);
	RECT rc[CREATECLICK_COUNT];
	rc[CREATECLICK_NAME] = L.name;
	rc[CREATECLICK_MALE] = L.gender[0];
	rc[CREATECLICK_FEMALE] = L.gender[1];
	for (int i = 0; i < 3; i++) rc[CREATECLICK_SKIN + i] = L.skin[i];
	rc[CREATECLICK_HAIRPREV] = L.hairPrev;
	rc[CREATECLICK_HAIRNEXT] = L.hairNext;
	for (int i = 0; i < 16; i++) rc[CREATECLICK_HAIRCOL + i] = L.hairCol[i];
	for (int i = 0; i < 8; i++) rc[CREATECLICK_UNDERCOL + i] = L.underCol[i];
	for (int i = 0; i < 3; i++) rc[CREATECLICK_PRESET + i] = L.preset[i];
	for (int k = 0; k < 6; k++) { rc[CREATECLICK_MINUS + k] = L.minus[k]; rc[CREATECLICK_PLUS + k] = L.plus[k]; }
	rc[CREATECLICK_BACK] = L.back;
	rc[CREATECLICK_CREATE] = L.create;
	const int iHit = HitTest(rc, CREATECLICK_COUNT, msX, msY);
	return g_createClick.Update(cLB, iHit);
}

// the stat for row k of the creation screen (STR, VIT, DEX, INT, MAG, CHR)
int CGame::CreateStatIndex(int k) { return kStat[k]; }
void CGame::ApplyCreatePreset(int i)
{
	for (int k = 0; k < 6; k++) m_createStat[kStat[k]] = kPreset[i][k];
}
