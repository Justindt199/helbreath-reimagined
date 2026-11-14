// UiGpu.h: the game drawn on the graphics card - the world (ground, characters, monsters, items,
// spell effects, weather, lights), the interface (windows, bars, text, icons, the cursor) and
// every screen before the game.
//
// (It began with the interface only, hence the name.) While a frame is drawn
// (CRenderer::BeginUiRecording, at the start of the frame, until the frame is shown), every
// drawing call adds a step to this list instead of drawing in software. When the frame is
// shown, the list is drawn in order into a texture at twice the game's size (2x picture pixels),
// with the same shapes and blending, and the result goes through the usual enlargement to the
// window (Render Quality).
//
// Steps: sprites (every PutSprite... variant, map tiles included, from the HD sheet when there
// is one, otherwise the classic one at twice the size), panels (CRenderer::FillPanel), stars,
// glows, rectangles, single pixels, text (the glyphs of the 2x font), light (the level-up
// effect, worked out in a shader) and pictures made on the CPU (rings, the login backdrop).
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_UIGPU_H
#define HB_ENGINE_UIGPU_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <cstdint>
#include <functional>
#include <memory>

namespace sf { class RenderTarget; class Texture; }
class CSprite;

// how a sprite is blended (the software sprite functions, see HdMirrorArgs)
enum EUiSpriteMode {
	UISPR_COPY = 0,		// plain, with or without the colour key
	UISPR_ADD,			// the "trans" tables: source x alpha added to what is there
	UISPR_AVERAGE,		// the 50 / 50 table (m_lTransRB2)
	UISPR_SHADE,		// darkens what is there to a quarter where the sprite has pixels
	UISPR_TINT,			// colour offset added to the sprite, drawn solid
	UISPR_ADDTINT,		// colour offset added to the sprite, then added to what is there
	UISPR_SUBTRACT		// the "fade" tables: sprite (+ depth) taken off what is there
};

// how a picture or rectangle is blended onto what is there
enum EUiBlend {
	UIBL_ALPHA = 0,		// colour at its alpha
	UIBL_PREMUL,		// premultiplied colour: colour + what is there x (1 - alpha)
	UIBL_ADD			// colour (x alpha) added to what is there (light)
};

class CSurface;
struct CUiGpuImpl;

class CUiGpu
{
public:
	// lut: RGB565 -> RGBA8888 (bytes R, G, B, A), 65536 entries; kept, not copied
	explicit CUiGpu(const std::uint32_t * lut);
	~CUiGpu();

	// Shaders compiled and ready; false: the interface stays on the CPU.
	bool Ok();

	void Clear(unsigned int uNowMs);	// a new frame's interface (uNowMs: timeGetTime)
	bool Empty() const;
	int  Count() const;			// steps recorded this frame

	// --- recording (all positions in 2x picture pixels) -----------------------------------
	// A rounded panel, as CRenderer::FillPanel with s = 2 (see there).
	void AddPanel(int x0, int y0, int x1, int y1, float fRadius, std::uint32_t fillRGB, int iFillA,
		std::uint32_t borderRGB, int iBorderA, float fTailH, float fTailX, float fBorderW,
		int bx0, int by0, int bx1, int by1);
	// A five-pointed star (CRenderer::FillStar): centre, radius to the tips.
	void AddStar(float cx, float cy, float fRadius, std::uint32_t rgb, int iA, int bx0, int by0, int bx1, int by1);
	// A plain rectangle: colour at alpha (0 = see-through .. 255 = solid), blended as iBlend.
	void AddRect(int x0, int y0, int x1, int y1, std::uint32_t rgb, int iA, int iBlend = UIBL_ALPHA);
	// Light added to what is there (the level-up effect, UI\LevelUpFx.cpp), worked out per pixel
	// on the graphics card. Unlike the other steps these take GAME pixels (drawn at 2x, so
	// smoother); colour 0..255 per channel; [bx0, bx1) x [by0, by1): the area it may light,
	// cut to the clip rectangle [cx0, cx1) x [cy0, cy1).
	// A soft spot: amp x exp(-((x - cx) / sx)^2 - ((y - cy) / sy)^2).
	void AddLightSpot(float cx, float cy, float sx, float sy, float amp, float r, float g, float b,
		float bx0, float by0, float bx1, float by1, int cx0, int cy0, int cx1, int cy1);
	// A ring on the ground: the oval (rx, ry) round (cx, cy), th pixels thick on each side, its
	// inside lit a little ((1 - a) / 4), all x fade.
	void AddLightRing(float cx, float cy, float rx, float ry, float th, float fade, float a, float r, float g, float b,
		float bx0, float by0, float bx1, float by1, int cx0, int cy0, int cx1, int cy1);
	// A pillar of light down to feetY: brighter towards the bottom, cut off below 'cut', a core
	// (sCore wide) and a halo (wid wide) round x = cx; v: its overall strength.
	void AddLightPillar(float cx, float feetY, float cut, float v, float sCore, float wid, float r, float g, float b,
		float bx0, float by0, float bx1, float by1, int cx0, int cy0, int cx1, int cy1);
	// CRenderer::FillEdgeGlow: colour fading in from the edges of [x0, x1) x [y0, y1) over iBand pixels.
	void AddEdgeGlow(int x0, int y0, int x1, int y1, int iBand, std::uint32_t rgb, int iMaxA);
	// A picture made on the CPU (bytes R, G, B, A), drawn into [dx, dx+dw) x [dy, dy+dh), cut to
	// the clip rectangle. Cached: kept on the graphics card under 'key' and refilled (fill) only
	// when 'version' changes. One-off: uploaded for this frame only.
	void AddImageCached(std::uint64_t key, std::uint32_t version, int w, int h,
		const std::function<void(std::uint32_t *)> & fill, bool bSmooth,
		float dx, float dy, float dw, float dh, int cx0, int cy0, int cx1, int cy1, int iBlend);
	void AddImage(const std::uint32_t * pRgba, int w, int h, bool bSmooth,
		float dx, float dy, float dw, float dh, int cx0, int cy0, int cx1, int cy1, int iBlend);
	// One glyph of the font: its alpha bitmap (w x h), uploaded once per (size, code point).
	void AddGlyph(unsigned int uSize, char32_t cp, const std::uint8_t * pAlpha, int w, int h,
		int x, int y, std::uint32_t rgb);
	// Part of a sprite sheet: sheet pixels [sx, sx+sw) x [sy, sy+sh) onto [dx, dx+dw) x [dy, dy+dh),
	// cut to the clip rectangle. pMask: the normal-size sheet whose see-through pixels (each
	// covering 2 x 2 of this one) also count as see-through (an HD sheet), or NULL.
	// add: the colour offset of the tint modes, in 5 / 6 / 5-bit steps. bNight: the night
	// shading (CSprite::_SetAlphaDegree) applied as it is drawn (an HD sheet, see m_bNightTint).
	void AddSprite(CSprite * pSheet, CSprite * pMask, int sx, int sy, int sw, int sh,
		float dx, float dy, float dw, float dh, int cx0, int cy0, int cx1, int cy1,
		int iMode, bool bKey, float fAlpha, int addR, int addG, int addB, bool bNight = false);

	// A sprite's slanted shadow (CSprite::PutShadowSprite): the sheet part [sx, sx+sw) x [sy, sy+sh)
	// squashed onto a parallelogram whose top edge runs from (xTop, yTop) to (xTop + w, yTop) and
	// bottom edge from (xBot, yBot) to (xBot + w, yBot), darkening what is there; cut at yMax.
	void AddShadow(CSprite * pSheet, CSprite * pMask, int sx, int sy, int sw, int sh,
		float xTop, float yTop, float xBot, float yBot, float w, float yMax);

	// --- night lighting (CGame::NightLights) ----------------------------------------------------
	// Placed after the world (ground, buildings, characters) and before spell effects: the world
	// so far is multiplied by a light map - the ambient moonlight (0..1 per channel) plus every
	// light added this frame, before or after this call. Text, panels, rectangles, lines and
	// glows recorded before it (names, health bars over heads) are drawn after it, unlit.
	void SetLightPass(float ambR, float ambG, float ambB);
	// --- a zoomed scene (the campfire on the character select screen) -----------------------------
	// Everything recorded from the start of the frame until EndZoom (the scene, its light pass and
	// glows) is drawn magnified z times round (cx, cy) (2x picture pixels: what lands there stays
	// there); what follows (panels, text, the cursor) is drawn as it is. Cleared every frame.
	void SetZoom(float cx, float cy, float z);
	void EndZoom();
	bool HasLightPass() const;
	// A pool of light (2x picture pixels): an ellipse of radii (rx, ry) round (x, y), colour
	// 0..1, strength a (0..2): soft, brightest in the middle.
	void AddLight(float x, float y, float rx, float ry, float r, float g, float b, float a);
	// A bright glowing core (a lamp's flame), added to the picture after the light map.
	void AddLightGlow(float x, float y, float rad, float r, float g, float b, float a);

	// --- drawing ------------------------------------------------------------------------------
	// Draws the list onto rt (whose view is the 2x picture, W2 x H2 pixels).
	void Draw(sf::RenderTarget & rt, int W2, int H2);
	// Frees sheet textures not used for a while (call once a frame, after Draw).
	void Trim(unsigned int uNowMs);

	// a sprite sheet whose pixels changed (reopened, day / night shading): its texture is remade
	static unsigned int NextSheetGen();

private:
	std::unique_ptr<CUiGpuImpl> m;
};

#endif // HB_ENGINE_UIGPU_H
