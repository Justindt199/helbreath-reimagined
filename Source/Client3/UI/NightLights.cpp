// NightLights.cpp: real night lighting (Options > Graphics > Night lighting).
//
// At night the world is no longer just tinted: it is darkened to moonlight (Night darkness)
// and lit back by what gives off light - the street lamps, the player, and spells (fire,
// lightning, ice, energy, meteors). It is done on the graphics card (CUiGpu::SetLightPass):
// the ground, buildings and characters are drawn first, then multiplied by a light map
// (the moonlight plus every light, drawn as soft pools); spell effects, names, chat and the
// interface come after it, unaffected.
//
// The old night shading of the sprites (CSprite::_SetAlphaDegree) is not used while this is
// on: the moonlight replaces it (G_cSpriteShadeDegree).
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\Engine\UiGpu.h"
#include <cmath>
#include <unordered_map>

extern char G_cSpriteAlphaDegree;		// 1 day, 2 night (from the server)
char G_cSpriteShadeDegree = 1;			// what the sprites are shaded for (Engine\Sprite.cpp)

namespace {

// a gentle flicker, 0..1, different for each seed
float Flicker(float t, float seed)
{
	return 0.5f + 0.25f * std::sin(t * 7.1f + seed) + 0.15f * std::sin(t * 13.7f + seed * 3.1f) + 0.1f * std::sin(t * 23.3f + seed * 5.3f);
}

// The light a spell effect gives off: colour (0..1), size (game pixels across, half),
// strength, whether it flickers like lightning.
struct EffectLight { float r, g, b, rx, a; bool bFlash; };
const EffectLight kFire      = { 1.00f, 0.55f, 0.24f, 150.f, 1.6f, false };
const EffectLight kFireSmall = { 1.00f, 0.58f, 0.26f,  90.f, 1.0f, false };
const EffectLight kFireStrike= { 1.00f, 0.55f, 0.24f, 130.f, 1.4f, false };
const EffectLight kEnergy    = { 0.86f, 0.80f, 1.00f, 110.f, 1.2f, false };
const EffectLight kEnergyFly = { 0.86f, 0.80f, 1.00f,  80.f, 0.9f, false };
const EffectLight kLightning = { 0.67f, 0.76f, 1.00f, 220.f, 1.3f, true  };
const EffectLight kCold      = { 0.47f, 0.78f, 1.00f, 120.f, 1.0f, false };
const EffectLight kIceStrike = { 0.47f, 0.78f, 1.00f, 140.f, 1.3f, false };
const EffectLight kIceGolem  = { 0.47f, 0.78f, 1.00f,  90.f, 0.8f, false };
const EffectLight kBlizzard  = { 0.51f, 0.70f, 1.00f, 110.f, 0.8f, false };
const EffectLight kMeteor    = { 1.00f, 0.51f, 0.20f, 300.f, 1.8f, false };

const EffectLight * LightOf(short sType)
{
	switch (sType) {
	case 5:   return &kFire;						// Fire Ball explosion
	case 120: return &kFireSmall;					// Fire Ball, flying
	case 30: case 31: case 130: case 161: return &kFireStrike;	// (Mass) Fire Strike
	case 6: case 7: return &kEnergy;				// Energy Bolt, Magic Missile explosions
	case 100: case 110: case 147: case 160: return &kEnergyFly;	// Magic Missile, Energy Bolt / Strike, Triple Energy Bolt
	case 10: case 137: case 143: case 151: case 156: case 174: return &kLightning;	// Lightning (Arrow / Bolt / Strike)
	case 145: case 56: case 163: return &kCold;		// (Mass) Chill Wind
	case 157: case 172: return &kIceStrike;			// (Mass) Ice Strike
	case 75: case 76: case 77: return &kIceGolem;
	case 47: case 48: case 49: case 72: case 191: return &kBlizzard;
	// (not 65: the meteor's dark smoke trail, also used for the chimney smoke at night)
	case 66: case 67: case 181: return &kMeteor;	// Meteor Strike: the explosion, its fire
	default:  return NULL;
	}
}

// Lit windows: building sprite (or ground tile sprite: some buildings are drawn as ground
// tiles), frame (the part of the building drawn on one tile), and the window's centre from where
// that frame is drawn (ix - 16, iy - 16), in game pixels. Marked by hand on the HD building
// pictures (the Window Marker page): 223 windows.
struct WindowSpot { short sSprite, sFrame, dx, dy; };
const WindowSpot kWindows[] = {
	{ 225, 2, -16, -42 }, { 225, 2, 19, -42 }, { 370, 4, 1, -92 }, { 370, 4, 24, -80 }, { 370, 10, 6, -108 }, { 370, 10, 26, -42 },
	{ 370, 10, 28, -122 }, { 370, 12, 9, -66 }, { 372, 2, 24, -61 }, { 372, 3, 16, -39 }, { 372, 3, 20, -114 }, { 372, 4, 2, -99 },
	{ 372, 9, 4, -46 }, { 372, 9, 30, -58 }, { 374, 5, 20, -65 }, { 374, 6, 25, -43 }, { 374, 6, 28, -120 }, { 374, 7, 14, -108 },
	{ 374, 13, 0, -79 }, { 374, 13, 26, -95 }, { 376, 3, 29, -144 }, { 376, 4, 10, -122 }, { 376, 9, 30, -104 }, { 376, 10, 18, -113 },
	{ 376, 10, 22, -36 }, { 376, 12, 0, -38 }, { 378, 1, 30, -20 }, { 378, 4, 0, -77 }, { 378, 17, 21, 22 }, { 378, 18, 6, -55 },
	{ 378, 18, 27, 9 }, { 379, 6, 8, -74 }, { 379, 17, 4, -83 }, { 379, 18, 16, -72 }, { 380, 2, 24, -38 }, { 380, 3, 4, -96 },
	{ 380, 3, 23, 1 }, { 380, 17, 0, -60 }, { 380, 18, 14, 6 }, { 380, 19, 8, 0 }, { 381, 18, 6, -80 }, { 381, 19, 2, -56 },
	{ 382, 5, 4, -94 }, { 382, 6, 15, -74 }, { 382, 12, 26, 11 }, { 382, 14, 4, -12 }, { 382, 15, 30, -43 }, { 383, 11, 1, -46 },
	{ 383, 12, 28, -48 }, { 384, 13, 6, 8 }, { 384, 14, 14, -24 }, { 385, 8, 16, -126 }, { 385, 12, 25, -147 }, { 386, 5, 15, 9 },
	{ 386, 9, 26, 4 }, { 387, 2, 24, -78 }, { 387, 5, 16, -270 }, { 387, 9, 22, -268 }, { 387, 14, 8, -138 }, { 388, 7, 16, 8 },
	{ 388, 25, 22, 10 }, { 390, 130, 15, 28 }, { 390, 131, 18, 26 }, { 390, 223, 2, 26 }, { 390, 223, 32, 25 }, { 390, 260, 23, 10 },
	{ 390, 269, 15, 12 }, { 391, 148, 18, 6 }, { 391, 151, 17, 12 }, { 391, 185, 21, 0 }, { 391, 194, 19, 16 }, { 391, 237, 26, 20 },
	{ 394, 133, 2, 12 }, { 394, 170, 2, 16 }, { 394, 206, 24, 20 }, { 394, 251, 17, 15 }, { 394, 270, 2, 12 }, { 396, 21, 16, 14 },
	{ 396, 22, 0, 18 }, { 396, 22, 20, 8 }, { 396, 38, 16, 14 }, { 396, 39, 3, 17 }, { 396, 39, 20, 14 }, { 396, 58, 0, 6 },
	{ 396, 61, 16, 12 }, { 396, 62, 0, 16 }, { 396, 76, 26, 4 }, { 396, 95, 12, 26 }, { 396, 108, 20, 32 }, { 396, 113, 28, 22 },
	{ 396, 129, 4, 6 }, { 396, 129, 20, 3 }, { 396, 132, 0, 10 }, { 396, 144, 22, 29 }, { 396, 150, 20, 8 }, { 396, 155, 10, 26 },
	{ 396, 166, 28, 6 }, { 396, 168, 20, 1 }, { 396, 169, 7, 6 }, { 396, 169, 22, 2 }, { 396, 173, 29, 20 }, { 396, 191, 31, 26 },
	{ 396, 210, 18, 22 }, { 396, 246, 22, 16 }, { 397, 191, 12, 23 }, { 397, 191, 28, 25 }, { 397, 192, 16, 18 }, { 397, 298, 18, 14 },
	{ 397, 299, 2, 20 }, { 397, 299, 18, 16 }, { 398, 1, 10, 9 }, { 398, 2, 10, 12 }, { 399, 17, 4, 4 }, { 399, 30, 17, 23 },
	{ 399, 57, 4, 0 }, { 399, 77, 8, 4 }, { 400, 26, 6, 30 }, { 400, 47, 9, 14 }, { 400, 48, 12, 9 }, { 400, 66, 6, 6 },
	{ 400, 67, 6, 28 }, { 400, 68, 12, 29 }, { 400, 82, 7, 21 }, { 400, 83, 6, 7 }, { 400, 107, 17, 14 }, { 400, 108, 10, 11 },
	{ 400, 126, 26, 4 }, { 400, 126, 26, 30 }, { 400, 143, 15, 20 }, { 400, 162, 0, 22 }, { 400, 166, 22, 14 }, { 400, 206, 18, 20 },
	{ 400, 223, 15, 4 }, { 400, 226, 22, 24 }, { 400, 242, 0, 2 }, { 401, 85, 16, 12 }, { 401, 87, 22, 26 }, { 401, 105, 14, 28 },
	{ 401, 106, 14, 0 }, { 401, 126, 16, 14 }, { 401, 127, 19, 3 }, { 401, 130, 26, 13 }, { 401, 132, 7, 10 }, { 401, 145, 14, 8 },
	{ 401, 146, 16, 32 }, { 401, 207, 25, 6 }, { 401, 213, 31, 6 }, { 401, 214, 30, 20 }, { 401, 216, 8, 18 }, { 401, 233, 32, 18 },
	{ 401, 236, 4, 25 }, { 401, 239, 4, 9 }, { 401, 254, 27, 4 }, { 401, 257, 6, 18 }, { 401, 259, 6, 1 }, { 401, 272, 13, 2 },
	{ 401, 273, 30, 1 }, { 401, 274, 26, 6 }, { 401, 276, 2, 7 }, { 401, 277, 9, 9 }, { 401, 279, 4, 1 }, { 402, 7, 11, 9 },
	{ 402, 8, 16, 18 }, { 402, 17, 8, 4 }, { 402, 37, 10, 22 }, { 402, 143, 16, 22 }, { 402, 163, 28, 6 }, { 402, 163, 29, 28 },
	{ 402, 183, 29, 30 }, { 402, 213, 32, 6 }, { 402, 214, 30, 22 }, { 402, 216, 4, 18 }, { 402, 223, 15, 3 }, { 402, 233, 31, 18 },
	{ 402, 236, 5, 30 }, { 402, 239, 4, 10 }, { 402, 242, 0, 2 }, { 402, 255, 2, 8 }, { 402, 257, 4, 20 }, { 402, 259, 6, 2 },
	{ 402, 272, 18, 6 }, { 402, 273, 32, 0 }, { 402, 274, 29, 6 }, { 402, 276, 8, 9 }, { 402, 277, 9, 8 }, { 402, 279, 4, 4 },
	{ 403, 2, 10, 11 }, { 404, 209, 14, 18 }, { 404, 227, 25, 20 }, { 404, 246, 6, 22 }, { 404, 282, 12, 29 }, { 404, 283, 30, 4 },
	{ 405, 27, 19, 2 }, { 405, 158, 25, 26 }, { 422, 14, 2, -151 }, { 422, 16, 2, -70 }, { 422, 17, 13, -67 }, { 423, 2, -22, -38 },
	{ 423, 2, 16, -37 }, { 424, 1, 2, 2 }, { 424, 2, 16, 8 }, { 424, 3, 4, -3 }, { 424, 6, 0, -98 }, { 424, 7, 6, -98 },
	{ 424, 9, 11, -182 }, { 425, 10, 15, -197 }, { 425, 11, -3, -195 }, { 425, 11, 16, -204 }, { 425, 17, 18, -46 }, { 425, 18, 2, -42 },
	{ 425, 18, 18, -44 }, { 426, 9, 0, 24 }, { 426, 11, 3, -8 }, { 426, 12, 24, -40 }, { 426, 14, 14, -78 }, { 429, 6, 3, 11 },
	{ 429, 8, 31, -48 }, { 429, 11, 29, -114 }, { 441, 242, 26, 7 }, { 441, 246, 4, 23 }, { 442, 47, 26, 11 }, { 442, 51, 18, 32 },
	{ 442, 53, 0, 2 }, { 442, 77, 10, 9 }, { 442, 236, 24, 28 },
};

} // namespace

bool CGame::NightLightsOn()
{
	// in the game at night, or the live town behind the sign-in screens at night (UI\LoginScene.cpp)
	const bool bGame = G_cSpriteAlphaDegree == 2 && m_cGameMode == GAMEMODE_ONMAINGAME;
	const bool bLogin = m_bLoginNight && m_cGameMode != GAMEMODE_ONMAINGAME;
	return m_bNightLights && (bGame || bLogin) && m_Renderer.UiGpuRecorder() != NULL;
}

// At the start of a frame: what the sprites are shaded for.
void CGame::NightLightsBeginFrame()
{
	G_cSpriteShadeDegree = (G_cSpriteAlphaDegree == 2 && m_bNightLights && m_Renderer.MirrorOn()) ? 1 : G_cSpriteAlphaDegree;
}

// After the world, before spell effects: the light pass (its lights are added during the frame).
void CGame::NightLightsPass()
{
	if (!NightLightsOn()) return;
	CUiGpu * pG = m_Renderer.UiGpuRecorder();
	const float fDark = (m_iNightDarkness < 0 ? 0 : (m_iNightDarkness > 100 ? 100 : m_iNightDarkness)) / 100.f;
	const float fAmb = 1.f - fDark * 0.82f;
	pG->SetLightPass(fAmb * 0.62f, fAmb * 0.72f, fAmb);		// moonlight: blue

	// a soft light round the player, so you can always see where you are
	if (!IsRectEmpty(&m_rcPlayerRect)) {
		const float x = (m_rcPlayerRect.left + m_rcPlayerRect.right) * 0.5f, y = (float)m_rcPlayerRect.bottom - 12.f;
		pG->AddLight(2.f * x, 2.f * y, 2.f * 150.f, 2.f * 100.f, 1.f, 0.75f, 0.5f, 0.5f);
	}
}

// A street lamp at the tile drawn at (ix, iy): a pool of light on the paving, and its flames.
// heads: the lamp heads (1 or 3), in game pixels.
void CGame::NightLightsLamp(int ix, int iy, const POINT * pHeads, int nHeads)
{
	if (!NightLightsOn()) return;
	CUiGpu * pG = m_Renderer.UiGpuRecorder();
	const float t = m_dwCurTime / 1000.f, seed = (float)((m_sViewPointX + ix) * 0.013f + (m_sViewPointY + iy) * 0.031f);
	const float f = 0.9f + 0.1f * Flicker(t, seed);
	pG->AddLight(2.f * ix, 2.f * (iy - 20), 2.f * 180.f, 2.f * 115.f, 1.f, 0.77f, 0.47f, 1.15f * f);
	pG->AddLight(2.f * ix, 2.f * (iy - 80), 2.f * 80.f, 2.f * 80.f, 1.f, 0.82f, 0.59f, 0.6f * f);
	const float fg = 0.85f + 0.15f * Flicker(t, seed + 1.f);
	for (int i = 0; i < nHeads; i++)
		pG->AddLightGlow(2.f * pHeads[i].x, 2.f * pHeads[i].y, 2.f * 23.f, 1.f, 0.84f, 0.59f, 0.85f * fg);
}

// Every spell effect on screen that gives off light.
void CGame::NightLightsEffects()
{
	if (!NightLightsOn()) return;
	CUiGpu * pG = m_Renderer.UiGpuRecorder();
	for (int i = 0; i < MAXEFFECTS; i++) {
		const CEffect * e = m_pEffectList[i];
		if (e == NULL || e->m_cFrame < 0) continue;
		const EffectLight * L = LightOf(e->m_sType);
		if (L == NULL) continue;
		// bright at once, fading towards the end of the effect
		const float q = (e->m_cMaxFrame > 0) ? (float)e->m_cFrame / (float)(e->m_cMaxFrame + 1) : 0.f;
		float k = 1.f - q * q;
		if (L->bFlash) k *= 0.6f + 0.4f * (float)(rand() % 100) / 100.f;
		if (k <= 0.f) continue;
		const float x = (float)(e->m_mX - m_sViewPointX), y = (float)(e->m_mY - m_sViewPointY) + 20.f;
		pG->AddLight(2.f * x, 2.f * y, 2.f * L->rx, 2.f * L->rx * 0.65f, L->r, L->g, L->b, L->a * k);
	}
}

// The lit windows of a building strip just drawn at (x, y) (= ix - 16, iy - 16): warm light in
// the window and spilling below it, and a soft glow on top.
void CGame::NightLightsWindows(int sSprite, int sFrame, int x, int y)
{
	static std::unordered_map<int, std::pair<int, int>> s_index;	// sprite * 1024 + frame -> [first, last)
	if (s_index.empty()) {
		const int n = (int)(sizeof(kWindows) / sizeof(kWindows[0]));
		for (int i = 0; i < n; ) {
			int j = i;
			while (j < n && kWindows[j].sSprite == kWindows[i].sSprite && kWindows[j].sFrame == kWindows[i].sFrame) j++;
			s_index[kWindows[i].sSprite * 1024 + kWindows[i].sFrame] = std::make_pair(i, j);
			i = j;
		}
	}
	const auto it = s_index.find(sSprite * 1024 + sFrame);
	if (it == s_index.end() || !NightLightsOn()) return;
	CUiGpu * pG = m_Renderer.UiGpuRecorder();
	const float t = m_dwCurTime / 1000.f;
	for (int i = it->second.first; i < it->second.second; i++) {
		const float wx = (float)(x + kWindows[i].dx), wy = (float)(y + kWindows[i].dy);
		const float f = 0.92f + 0.08f * Flicker(t * 0.6f, (m_sViewPointX + wx) * 0.07f + (m_sViewPointY + wy) * 0.11f);
		pG->AddLight(2.f * wx, 2.f * wy, 70.f, 70.f, 1.f, 0.67f, 0.35f, 0.8f * f);
		pG->AddLight(2.f * wx, 2.f * wy + 120.f, 140.f, 70.f, 1.f, 0.67f, 0.35f, 0.25f * f);
		pG->AddLightGlow(2.f * wx, 2.f * wy, 34.f, 1.f, 0.7f, 0.39f, 0.45f);
	}
}
