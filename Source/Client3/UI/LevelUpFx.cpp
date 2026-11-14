// LevelUpFx.cpp: the level-up celebration played over a player, seen by everyone nearby.
//
// The game server sends MSGID_EVENT_COMMON / COMMONTYPE_LEVELUPFX to every player who can see
// someone level up (object id, new level, kind). Four versions:
//   normal level       gold ring, ground glow, spiralling gold sparks, "LEVEL UP" shield
//   every 10 levels    + pillar of light, a second (white) ring, more sparks, bigger shield
//   max level (180)    + longer pulsing pillar, four rings, many more sparks, "MAX LEVEL" crown
//   Majestic point     the normal version in silver, with a "+1 MAJESTIC" shield
//
// Everything on the ground and behind the player is drawn just before the player's body
// (DrawLevelUpFxBack, from DrawObjectFOE); sparks in front of the player after all objects
// (DrawLevelUpFxFront); the badge by CLevelUpBadges at window resolution. The light (spots,
// rings, the pillar) is worked out per pixel on the graphics card (CUiGpu::AddLight...), at
// twice the game's resolution.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\Engine\UiGpu.h"

extern DWORD G_dwGlobalTime;
extern WORD  _tmp_wObjectID;		// the object being drawn (Game.cpp)

namespace {

struct RGBf { float r, g, b; };
const RGBf kGold   = { 255.f, 205.f, 80.f };
const RGBf kWhite  = { 255.f, 250.f, 225.f };
const RGBf kSilver = { 205.f, 215.f, 235.f };
const RGBf kSilverW= { 245.f, 248.f, 255.f };

// Small deterministic random numbers, so every viewer's sparks follow the same paths.
float Rand01(unsigned int seed, unsigned int i, unsigned int k)
{
	unsigned int h = seed * 2654435761u ^ (i * 40503u + k * 2246822519u);
	h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
	return (h & 0xFFFFFF) / 16777216.f;
}
float RandIn(unsigned int seed, unsigned int i, unsigned int k, float lo, float hi) { return lo + (hi - lo) * Rand01(seed, i, k); }

// Additive light, worked out per pixel on the graphics card (CUiGpu::AddLight..., at twice the
// game's resolution). Positions in game pixels; each shape is cut to the clip area.
struct Canvas
{
	CUiGpu * pGpu;
	RECT rc;
	Canvas(CRenderer & r) : pGpu(r.UiGpuRecorder())
	{
		rc = r.m_rcClipArea;
		if (rc.left < 0) rc.left = 0;
		if (rc.top < 0) rc.top = 0;
		if (rc.right > RENDER_WIDTH) rc.right = RENDER_WIDTH;
		if (rc.bottom > RENDER_HEIGHT) rc.bottom = RENDER_HEIGHT;
	}
	// round soft spot: amp x exp(-d^2 / (2 sig^2))
	void Blob(float x, float y, float sig, float amp, const RGBf & c)
	{
		if (pGpu == NULL || amp <= 0.01f) return;
		if (sig < 0.45f) sig = 0.45f;
		const float rr = sig * 3.f;
		pGpu->AddLightSpot(x, y, sig * 1.41421356f, sig * 1.41421356f, amp, c.r, c.g, c.b,
			x - rr, y - rr, x + rr + 1, y + rr + 1, rc.left, rc.top, rc.right, rc.bottom);
	}
	// flat soft oval on the ground
	void Glow(float cx, float cy, float sx, float sy, float amp, const RGBf & c)
	{
		if (pGpu == NULL || amp <= 0.01f) return;
		pGpu->AddLightSpot(cx, cy, sx, sy, amp, c.r, c.g, c.b,
			cx - sx * 2.5f, cy - sy * 2.5f, cx + sx * 2.5f + 1, cy + sy * 2.5f + 1, rc.left, rc.top, rc.right, rc.bottom);
	}
	// expanding shockwave ring on the ground: starts at t0, lasts dur seconds, grows to rmax
	void Ring(float cx, float cy, float t, float t0, float dur, float rmax, float strength, const RGBf & c)
	{
		const float a = (t - t0) / dur;
		if (pGpu == NULL || a < 0.f || a > 1.f) return;
		const float e = 1.f - (1.f - a) * (1.f - a) * (1.f - a);
		const float rx = 5.f + (rmax - 5.f) * e, ry = rx * 0.5f;
		const float th = 2.0f * (1.f - a) + 0.6f;			// half thickness in pixels
		const float fade = std::pow(1.f - a, 0.8f) * strength;
		pGpu->AddLightRing(cx, cy, rx, ry, th, fade, a, c.r, c.g, c.b,
			cx - rx - 4, cy - ry - 4, cx + rx + 5, cy + ry + 5, rc.left, rc.top, rc.right, rc.bottom);
	}
	// pillar of light from the top of the screen down to the player's feet; after tend it
	// fades away from the bottom up
	void Pillar(float cx, float feetY, float t, float tend, float width, bool pulse, float amp, const RGBf & c)
	{
		if (pGpu == NULL || t < 0.f || t > tend + 0.8f) return;
		const float grow = (std::min)(1.f, t / 0.18f);
		const float fade = (t < tend) ? 1.f : (std::max)(0.f, 1.f - (t - tend) / 0.8f);
		const float cut = (t < tend) ? feetY : feetY - (t - tend) / 0.8f * feetY;
		const float pm = pulse ? 1.f + 0.25f * std::sin(t * 9.f) : 1.f;
		const float wid = width * grow * (1.f + 0.08f * std::sin(t * 40.f)) * pm + 0.1f;
		const float sCore = wid * 0.35f + 0.1f;
		pGpu->AddLightPillar(cx, feetY, cut, fade * amp, sCore, wid, c.r, c.g, c.b,
			cx - wid * 2.5f, 0.f, cx + wid * 2.5f + 1, feetY + 4, rc.left, rc.top, rc.right, rc.bottom);
	}
};

struct Style
{
	float dur;				// seconds
	float ringMax;			// first ring size
	int   sparks;
	float sparkSpread;		// how long new sparks keep appearing (s)
	bool  milestone, maxLevel;
	RGBf  c1, c2;
};

Style GetStyle(char cKind)
{
	Style s;
	s.dur = 2.3f; s.ringMax = 43.f; s.sparks = 26; s.sparkSpread = 0.65f;
	s.milestone = false; s.maxLevel = false; s.c1 = kGold; s.c2 = kWhite;
	switch (cKind) {
	case CGame::LVLFX_MILESTONE: s.dur = 2.7f; s.ringMax = 54.f; s.sparks = 54; s.milestone = true; break;
	case CGame::LVLFX_MAX:       s.dur = 3.6f; s.ringMax = 54.f; s.sparks = 90; s.sparkSpread = 1.25f; s.maxLevel = true; break;
	case CGame::LVLFX_MAJESTIC:  s.c1 = kSilver; s.c2 = kSilverW; break;
	}
	return s;
}

// The sparks spiral up around the player; front = the half of the turn nearer the camera.
void Sparks(Canvas & cv, const CGame::LevelUpFx & fx, const Style & st, float t, bool bFront)
{
	const unsigned int seed = fx.wObjectID * 7919u + fx.dwStart;
	for (int i = 0; i < st.sparks; i++) {
		const float ts   = RandIn(seed, i, 1, 0.05f, st.sparkSpread);
		const float life = RandIn(seed, i, 2, 0.9f, st.maxLevel ? 1.6f : 1.4f);
		const float age = t - ts;
		if (age < 0.f || age > life) continue;
		const float a0 = RandIn(seed, i, 3, 0.f, 6.2832f);
		const float w  = RandIn(seed, i, 4, 3.f, 5.f) * (Rand01(seed, i, 5) < 0.5f ? 1.f : -1.f);
		const float a = a0 + w * age;
		if ((std::sin(a) >= 0.f) != bFront) continue;
		const float r = RandIn(seed, i, 6, 8.f, st.maxLevel ? 18.f : 15.f) * (1.f - 0.35f * age / life);
		const float v = RandIn(seed, i, 7, 50.f, 85.f);
		const float x = fx.iX + r * std::cos(a);
		const float y = fx.iY - 3.f - v * age + r * std::sin(a) * 0.45f;
		const float lf = 1.f - age / life;
		const float tw = 0.6f + 0.4f * std::sin(RandIn(seed, i, 8, 0.f, 6.28f) + age * 25.f);
		const float sz = RandIn(seed, i, 9, 1.1f, 2.0f);
		cv.Blob(x, y, sz, 1.6f * lf * tw, st.c1);
		cv.Blob(x, y, sz * 0.35f, 1.5f * lf, st.c2);
	}
}

} // namespace

void CGame::StartLevelUpFx(WORD wObjectID, short sLevel, char cServerKind)
{
	char cKind = LVLFX_NORMAL;
	if (cServerKind == 1) cKind = LVLFX_MAJESTIC;
	else if (sLevel >= MAXLEVEL) cKind = LVLFX_MAX;
	else if (sLevel > 0 && (sLevel % 10) == 0) cKind = LVLFX_MILESTONE;

	// one effect per player: a new level-up restarts it
	for (size_t i = 0; i < m_levelUpFx.size(); i++)
		if (m_levelUpFx[i].wObjectID == wObjectID) { m_levelUpFx.erase(m_levelUpFx.begin() + i); break; }
	LevelUpFx fx;
	fx.wObjectID = wObjectID;
	fx.sLevel = sLevel;
	fx.cKind = cKind;
	fx.dwStart = G_dwGlobalTime;
	fx.iX = fx.iY = 0;
	fx.iBackFrame = -1;
	fx.bVisible = false;
	m_levelUpFx.push_back(fx);
}

// Called for every character just before its body is drawn (see DrawObjectFOE).
void CGame::DrawLevelUpFxBack(int sX, int sY)
{
	if (m_levelUpFx.empty()) return;
	for (size_t i = 0; i < m_levelUpFx.size(); i++) {
		LevelUpFx & fx = m_levelUpFx[i];
		if (fx.wObjectID != _tmp_wObjectID || fx.iBackFrame == m_iLevelUpFxFrame) continue;
		fx.iBackFrame = m_iLevelUpFxFrame;
		fx.iX = sX; fx.iY = sY + 2;
		const Style st = GetStyle(fx.cKind);
		const float t = (G_dwGlobalTime - fx.dwStart) / 1000.f;
		Canvas cv(m_Renderer);
		const float cx = (float)fx.iX, cy = (float)fx.iY;

		const float fGlow = st.maxLevel ? (std::max)(0.f, 1.f - std::fabs(t - 0.8f) / 1.8f) : (std::max)(0.f, 1.f - std::fabs(t - 0.4f) / 0.9f);
		cv.Glow(cx, cy, st.maxLevel ? 21.f : 18.f, st.maxLevel ? 9.f : 8.f, 0.8f * fGlow, st.c1);
		if (st.maxLevel) {
			cv.Ring(cx, cy, t, 0.00f, 0.85f, 54.f, 1.5f, kGold);
			cv.Ring(cx, cy, t, 0.20f, 0.85f, 68.f, 1.3f, kWhite);
			cv.Ring(cx, cy, t, 0.45f, 0.85f, 80.f, 1.2f, kGold);
			cv.Ring(cx, cy, t, 1.20f, 0.85f, 64.f, 1.0f, kGold);
			cv.Pillar(cx, cy, t, 2.2f, 16.f, true, 0.85f, kGold);
		}
		else {
			cv.Ring(cx, cy, t, 0.f, 0.75f, st.ringMax, 1.4f, st.c1);
			if (st.milestone) {
				cv.Ring(cx, cy, t, 0.22f, 0.85f, 68.f, 1.2f, kWhite);
				cv.Pillar(cx, cy, t, 1.1f, 14.f, false, 0.85f, kGold);
			}
		}
		Sparks(cv, fx, st, t, false);
	}
}

// Called once a frame after all objects are drawn.
void CGame::DrawLevelUpFxFront()
{
	m_levelUpBadges.Clear();
	for (size_t i = 0; i < m_levelUpFx.size(); ) {
		LevelUpFx & fx = m_levelUpFx[i];
		const Style st = GetStyle(fx.cKind);
		const float t = (G_dwGlobalTime - fx.dwStart) / 1000.f;
		if (t > st.dur || m_cGameMode != GAMEMODE_ONMAINGAME) { m_levelUpFx.erase(m_levelUpFx.begin() + i); continue; }
		fx.bVisible = (fx.iBackFrame == m_iLevelUpFxFrame);
		if (fx.bVisible) {
			Canvas cv(m_Renderer);
			if (st.maxLevel)       cv.Pillar((float)fx.iX, (float)fx.iY, t, 2.2f, 16.f, true, 0.16f, kGold);
			else if (st.milestone) cv.Pillar((float)fx.iX, (float)fx.iY, t, 1.1f, 14.f, false, 0.16f, kGold);
			Sparks(cv, fx, st, t, true);

			// the floating badge
			CLevelUpBadges::Badge b;
			const float t0 = st.maxLevel ? 0.2f : 0.15f;
			const float bdur = st.maxLevel ? 3.0f : 2.0f;
			const float tt = t - t0;
			if (tt >= 0.f && tt <= bdur) {
				const float popLen = st.maxLevel ? 0.22f : 0.18f, popAmt = st.maxLevel ? 0.5f : 0.4f;
				b.scale = (1.f + popAmt - popAmt * (std::min)(1.f, tt / popLen)) * (st.maxLevel ? 1.25f : (st.milestone ? 1.2f : 1.f));
				const float fadeLen = st.maxLevel ? 0.55f : 0.5f;
				b.alpha = (tt < bdur - fadeLen) ? (std::min)(1.f, tt / 0.08f) : (std::max)(0.f, (bdur - tt) / fadeLen);
				b.x = (float)fx.iX;
				b.y = (float)fx.iY - (st.maxLevel ? 104.f : 94.f) - (st.maxLevel ? 14.f * (std::min)(1.f, tt / 2.2f) : 15.f * (std::min)(1.f, tt / 1.6f));
				char cNum[16];
				if (fx.cKind == LVLFX_MAJESTIC) { b.style = CLevelUpBadges::STYLE_SILVER; b.label = "MAJESTIC"; b.number = "+1"; }
				else if (st.maxLevel)           { b.style = CLevelUpBadges::STYLE_CROWN;  b.label = "MAX LEVEL"; wsprintf(cNum, "%d", fx.sLevel); b.number = cNum; }
				else                            { b.style = CLevelUpBadges::STYLE_GOLD;   b.label = "LEVEL UP";  wsprintf(cNum, "%d", fx.sLevel); b.number = cNum; }
				m_levelUpBadges.Add(b);
			}
		}
		i++;
	}
	m_iLevelUpFxFrame++;
}
