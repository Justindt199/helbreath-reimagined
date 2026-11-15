// Renderer.h: SFML-based replacement for the old DirectDraw wrapper (DXC_ddraw).
//
// The game is drawn on the graphics card (Engine\UiGpu.h): every drawing call (sprites, text,
// panels, the ground...) adds a step to a list while a frame is drawn, and iFlip draws the list
// into a texture at twice the game's size, then enlarges it to the window (Render Quality).
// A graphics card with OpenGL shaders is required (bInit fails without one).
//
// There is no software drawing any more. Sprites still work out the rectangle they cover
// (CSprite::m_rcBound, for the mouse) in game pixels, and the sprite sheets stay in RAM (RGB565)
// for mouse picking and to be sent to the graphics card. m_rcClipArea is the game's clip
// rectangle, in game pixels, for every drawing call.
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_RENDERER_H
#define HB_ENGINE_RENDERER_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <math.h>
#include <stdio.h>
#include <windows.h>
#include <memory.h>
#include <string.h>
#include <vector>
#include <string>
#include <cstdint>
#include <climits>
#include "Surface.h"
#include "../GlobalDef.h"
#include "../../Shared/TypeDefs.h"

// Size of the game canvas (the software back buffer). It is 640x480 on the login and
// character screens and bigger in the game itself (see CRenderer::SetGameSize).
extern int g_iRenderWidth, g_iRenderHeight;
#define RENDER_WIDTH	g_iRenderWidth
#define RENDER_HEIGHT	g_iRenderHeight

namespace sf { class RenderWindow; class Font; }
struct CRendererImpl;
class CUiOverlay;
class CUiGpu;

class CRenderer
{
public:
	CRenderer();
	virtual ~CRenderer();

	BOOL bInit(HWND hWnd);
	// Changes the canvas size (recreates the back buffer, background surface and texture).
	bool SetGameSize(int iW, int iH);
	void Shutdown();					// releases the SFML window/context (call while the HWND still exists)
	void ChangeDisplayMode(HWND hWnd);	// toggles windowed <-> borderless fullscreen
	void SetBorderless(BOOL bBorderless);	// windowed mode only: no title bar / frame (fullscreen never has one)
	bool iFlip();						// presents the back buffer

	// Software drawing helpers (unchanged behaviour)
	void ColorTransferRGB(COLORREF fcolor, int * iR, int * iG, int * iB);
	void PutPixel(short sX, short sY, WORD wR, WORD wG, WORD wB);
	void DrawShadowBox(short sX, short sY, short dX, short dY, int iType = 0);
	// A see-through rounded box with smooth edges, for code-drawn UI (chat panel, speech
	// bubbles): [x0,x1) x [y0,y1) in game pixels, filled with 'fill' at iFillA (0-255) and
	// outlined one pixel wide with 'border' at iBorderA. With iTailH > 0 a little pointer
	// iTailH pixels tall hangs from the bottom edge at x = iTailX (the middle if INT_MIN).
	// Drawn again at twice the size into the 2x picture when HD is on.
	void FillPanel(int x0, int y0, int x1, int y1, int iRadius, COLORREF fill, int iFillA,
		COLORREF border = 0, int iBorderA = 0, int iTailH = 0, int iTailX = INT_MIN);
	// A filled five-pointed star centred on (cx, cy), fRadius game pixels to the tips, smooth
	// edges; drawn again at twice the size into the 2x picture when HD is on.
	void FillStar(float cx, float cy, float fRadius, COLORREF fill, int iFillA);
	// A soft glow along the inside edges of [x0, x1) x [y0, y1): colour c at alpha iMaxA right at
	// the edge, fading to nothing iBand pixels in (drawn at twice the size too when HD is on).
	void FillEdgeGlow(int x0, int y0, int x1, int y1, int iBand, COLORREF c, int iMaxA);
	void ClearBackB4();
	CSurface * pCreateOffScreenSurface(WORD iSzX, WORD iSzY);

	// Text, rendered with SFML into the back buffer. Same semantics as the old GDI calls:
	// TextOut draws at the top-left of the text cell, DrawText centres in the rect,
	// DrawTextEx takes DT_LEFT / DT_CENTER / DT_RIGHT / DT_VCENTER / DT_BOTTOM /
	// DT_SINGLELINE / DT_WORDBREAK / DT_CALCRECT.
	void TextOut(int x, int y, char const * cStr, COLORREF rgb);
	void DrawText(LPRECT pRect, char const * const pString, COLORREF rgb, bool wordWrap = false);
	void DrawTextEx(LPRECT pRect, char const * const pString, COLORREF rgb, DWORD flags, bool wordWrap = false);
	SIZE GetTextExtent(char const * pString, int iLength = -1);
	// The same font at another pixel size (titles, big numbers); y is the top of the text cell.
	void TextOutSized(int x, int y, char const * cStr, COLORREF rgb, int iPx);
	int  TextWidthSized(char const * cStr, int iPx);

	// Saves the next frame shown to FileName, as it is shown (drawn on the graphics card: at
	// twice the size, from the finished frame there; otherwise from the CPU picture).
	void RequestScreenshot(const char * FileName) { m_strShot = (FileName != NULL) ? FileName : ""; }
	std::string m_strShot;

	// Window / coordinate helpers used by the mouse code
	sf::RenderWindow * GetWindow();
	void MapWindowToGame(int iWinX, int iWinY, short * pX, short * pY);
	void MapGameToWindow(short sX, short sY, int * pWinX, int * pWinY);
	// Windowed-mode window size (the area inside the frame). 0 x 0 = the largest of
	// kWindowSizes that fits the screen. A size that does not fit falls back the same way.
	void SetWindowSize(int iW, int iH);
	void GetWindowSize(int * pW, int * pH) const;	// the size actually in use
	static constexpr int kNumWindowSizes = 3;
	static const int kWindowSizes[kNumWindowSizes][2];	// 800x600, 1024x768, 1280x960

	// Fullscreen is a fixed 1920x1080 screen (scaled to fit the monitor if it differs).
	// Widescreen (fullscreen only) stretches the in-game picture to fill the screen.
	static constexpr int kFullscreenW = 1920, kFullscreenH = 1080;
	void SetWidescreen(BOOL bWidescreen);

	// Frame pacing and picture quality
	// The most frames a second (ignored while VSync is on, which follows the monitor). There is
	// no "no limit": past what a monitor shows, more frames only work the PC (and graphics card)
	// harder, so it is at most kMaxFps; 0 or anything above it means kMaxFps.
	static const int kMaxFps = 240;
	void SetFrameLimit(int iFps);
	void SetVSync(BOOL bOn);
	// The screens before the game (sign-in, character select...) are cheap to draw: with no
	// limit they would run at thousands of frames a second, working the graphics card for
	// nothing (and making it whine). On those screens the limit is at most 60.
	void SetMenuScreen(bool bMenu);
	bool m_bMenuScreen = false;
	unsigned int _FrameLimitNow() const;
	// How the game picture is enlarged: it is first scaled up by a whole number with
	// sharp pixels, then smoothed to the window size. RQ_AUTO picks the factor from the
	// window size, RQ_X2 / RQ_X4 force it. RQ_HD uses an edge-smoothing pixel-art filter
	// (xBR) on the graphics card instead; without shader support it behaves like RQ_AUTO.
	enum { RQ_AUTO = 0, RQ_X2 = 1, RQ_X4 = 2, RQ_HD = 3 };
	bool IsHdFilterAvailable() const;
	void SetRenderQuality(int iMode);
	int  m_iFrameLimit;
	BOOL m_bVSync;
	int  m_iRenderQuality;

	// UI drawn on top of the game at full window resolution (e.g. the settings menu)
	void SetOverlay(CUiOverlay * pOverlay) { m_pOverlay = pOverlay; }
	// Full-resolution effects that belong to the game world (e.g. level-up badges); drawn
	// before the menu overlay, so the menu stays on top.
	void SetWorldOverlay(CUiOverlay * pOverlay) { m_pWorldOverlay = pOverlay; }
	// Window position of a game-canvas pixel, and window pixels per game pixel (last frame).
	void GameToWindow(float fGameX, float fGameY, float & fWinX, float & fWinY) const;
	float GameScaleX() const;
	float GameScaleY() const;
	const sf::Font * GetFont() const;	// the UI font, or NULL if none could be loaded

	// The game's mouse cursor for full-resolution UI: a sprite frame (RGB565 with a colour
	// key) drawn on top of the overlay at the same size as the scaled-up game image.
	void SetUiCursor(const WORD * pSrc, int iPitch, int iW, int iH, WORD wColorKey, int iOffX, int iOffY);
	bool HasUiCursor() const;
	void DrawUiCursor(sf::RenderWindow & window, int iWinX, int iWinY);

	long    m_lTransG100[64][64], m_lTransRB100[64][64];
	long    m_lTransG70[64][64], m_lTransRB70[64][64];
	long    m_lTransG50[64][64], m_lTransRB50[64][64];
	long    m_lTransG25[64][64], m_lTransRB25[64][64];
	long    m_lTransG2[64][64], m_lTransRB2[64][64];
	long    m_lFadeG[64][64],  m_lFadeRB[64][64];

	BOOL m_bFullMode;
	BOOL m_bBorderless;		// windowed-mode window style: FALSE = normal title bar and frame
	// HD Graphics: sprites (map tiles included) are drawn from their double-resolution sheet in
	// sprites\HD\ where there is one; off, from the classic sheets at exactly twice the size.
	BOOL m_bHdSprites;
	bool MirrorOn();				// the game can be drawn on the graphics card
	bool _GpuDraws();				// (the same: shaders compiled)

	// Drawing on the graphics card (Engine\UiGpu.h). Every screen calls BeginUiRecording at the
	// start of its frame; from then until the frame is shown (iFlip), drawing calls add steps
	// to the graphics card's list instead of drawing into the back buffer.
	BOOL m_bGpuUi;					// always on; off only to test the old software drawing
	void BeginUiRecording();
	void AbortUiRecording();		// stops recording without drawing (start of a frame)
	bool UiRecording() const { return m_bUiRec; }
	CUiGpu * UiGpuRecorder();		// the list being recorded, or NULL

	// VALORIA's stone interface: carved granite panels, stone button plaques, recessed slots and
	// the stone housing round a bar (CONTENTS\ui\*.png, the same pictures VALORIA uses), each
	// 9-sliced: the carved edges keep their size, the middle stretches. Drawn on the graphics card
	// only; without the pictures (or the graphics card) the old flat panel is drawn instead.
	enum EStone { STONE_PANEL, STONE_BUTTON, STONE_BUTTON_HOVER, STONE_BUTTON_PRESSED, STONE_SLOT,
		STONE_SLOT_ACTIVE, STONE_BAR, STONE_COUNT };
	void StoneBox(int iStone, int x0, int y0, int x1, int y1);
	void StonePanel(int x0, int y0, int x1, int y1)					{ StoneBox(STONE_PANEL, x0, y0, x1, y1); }
	void StoneSlot(int x0, int y0, int x1, int y1, bool bActive)	{ StoneBox(bActive ? STONE_SLOT_ACTIVE : STONE_SLOT, x0, y0, x1, y1); }
	// iState: 0 normal, 1 under the mouse, 2 pressed; the label in VALORIA's button lettering
	void StoneButton(int x0, int y0, int x1, int y1, int iState, const char * cLabel = NULL);
	// the housing goes round [x0, x1) x [y0, y1) (a bar), a little outside it
	void StoneBarFrame(int x0, int y0, int x1, int y1);
	// a dark recessed box with a thin carved edge (VALORIA's "slate": lists, text boxes, tooltips)
	void Slate(int x0, int y0, int x1, int y1, int iFillA = 235);
	// text centred in [x0, x1) x [y0, y1) in the display font (Cinzel Decorative), drop shadow
	void TitleCentered(int x0, int y0, int x1, int y1, const char * cStr, COLORREF rgb, int iPx);
	// on while a classic window (once parchment) is drawn: dark text is lifted to read on stone
	bool m_bStoneInk = false;
	COLORREF StoneInk(COLORREF c) const;
	int  m_iUiGpuSteps;				// steps drawn in the last frame (for the frame-time tooltip)

	BOOL m_bWidescreen;		// fullscreen only: stretch the image to 16:9 instead of letterboxing

	RECT  m_rcClipArea;
	// Shifts every sprite drawn on the graphics card by this many 2x picture pixels (0 or -1):
	// half-pixel steps for the live town behind the sign-in screens (UI\LoginScene.cpp), so it
	// pans smoothly. Always 0 in the game.
	int   m_iShiftX2 = 0, m_iShiftY2 = 0;
	// Fades the panels (FillPanel) and text drawn next: 255 = as given, 0 = invisible
	// (the sign-in panels sliding in). Always 255 otherwise.
	int   m_iFadeA = 255;
	// The font text is drawn in: 0 the interface font, 1 the display font for titles
	// (fonts\display.ttf in the game folder; the interface font when it's missing).
	int   m_iFace = 0;
	bool  HasDisplayFont() const;
	// titles, names and main buttons in the display font, with a drop shadow (bold UI font without it)
	void  TitleOut(int x, int y, char const * cStr, COLORREF rgb, int iPx = 0);
	int   TitleWidth(char const * cStr, int iPx = 0);
	const sf::Font * GetDisplayFont() const;	// NULL without fonts\display.ttf
	// Width / height of the area the game picture is shown in: the window, or in fullscreen the
	// 16:9 screen (the sign-in screens make their canvas this shape, UI\GameView.cpp).
	float ScreenAspect();
	char   m_cPixelFormat;		// always 1 (RGB 5:6:5)

	uint32 m_framecnt;
	uint32 GetFrameCnt()	const { return m_framecnt; }
	void ResetFrameCnt()			{ m_framecnt = 0; }

private:
	long _CalcMinValue(int iS, int iD, char cMode);
	long _CalcMaxValue(int iS, int iD, char cMode, char cMethod, double dAlpha);
	void _ApplyDisplayMode();
	void _DrawGameImage(sf::RenderWindow & window);
	bool m_bLastIntegerScale;	// last frame's picture was an exact whole-number scale (already sharp)
	bool m_bUiRec;				// recording this frame for the graphics card
	bool _LoadFont();
	void _DrawTextLines(const RECT & rc, const wchar_t * pText, int iLen, COLORREF rgb, DWORD flags, bool bWordWrap, RECT * pCalcRect);
	int  _DrawGlyphRun(int x, int y, const wchar_t * pText, int iLen, COLORREF rgb, bool bDraw, unsigned int uPx = 0);

	HWND m_hWnd;
	int  m_iWindowW, m_iWindowH;	// requested windowed size (0 = automatic)
	int  m_iUsedWindowW, m_iUsedWindowH;
	CRendererImpl * m_pImpl;
	CUiOverlay * m_pOverlay;
	CUiOverlay * m_pWorldOverlay;
};

#endif // HB_ENGINE_RENDERER_H
