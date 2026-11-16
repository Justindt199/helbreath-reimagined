// Renderer.cpp: SFML-based renderer (replaces DXC_ddraw / DirectDraw 7).
//////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <climits>
#include <cstring>

#include "Renderer.h"
#include "UiOverlay.h"
#include "FrameProfiler.h"
#include "UiGpu.h"

int g_iRenderWidth = 640, g_iRenderHeight = 480;

extern long    G_lTransG100[64][64], G_lTransRB100[64][64];
extern long    G_lTransG70[64][64], G_lTransRB70[64][64];
extern long    G_lTransG50[64][64], G_lTransRB50[64][64];
extern long    G_lTransG25[64][64], G_lTransRB25[64][64];
extern long    G_lTransG2[64][64], G_lTransRB2[64][64];

//////////////////////////////////////////////////////////////////////
// Configuration
//////////////////////////////////////////////////////////////////////

namespace
{
	// The old client used GDI CreateFont(16, ...) "Trebuchet MS" (a 16px cell height),
	// which corresponds to a 14px em size.
	const unsigned int	kFontCharSize	= 14;
	// GDI drew the text without anti-aliasing. Set to false to get the old hard-edged look.
	const bool			kTextAntialias	= true;
	// Fonts tried in order. A font shipped with the game (fonts\default.ttf) wins.
	const char * const	kLocalFonts[]	= { "fonts\\default.ttf", "font\\default.ttf" };
	const char * const	kSystemFonts[]	= { "trebuc.ttf", "arial.ttf", "tahoma.ttf", "verdana.ttf" };

	struct CachedGlyph
	{
		int advance = 0;
		int left = 0, top = 0;	// offset of the bitmap from the pen position / baseline
		int w = 0, h = 0;
		std::vector<std::uint8_t> alpha;
	};

	inline WORD Rgb565(int r, int g, int b)
	{
		return (WORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
	}
}

struct CRendererImpl
{
	std::optional<sf::RenderWindow>	window;
	std::optional<sf::Texture>		texture;
	std::optional<sf::RenderTexture> upscale;		// sharp whole-number enlargement (Render Quality)
	std::optional<sf::RenderTexture> down1;			// HD off + the HD filter: the picture at game size
	unsigned int					upscaleFactor = 0;
	std::optional<sf::Shader>		hdShader;		// RQ_HD edge-smoothing filter
	bool							hdTried = false;
	// HD rendering: the 2x picture
	std::unordered_map<char32_t, CachedGlyph> glyphs2;	// the UI font at twice the size, for the 2x picture
	std::vector<std::uint32_t>		rgba;		// RENDER_WIDTH * RENDER_HEIGHT, bytes R,G,B,A
	std::vector<std::uint32_t>		lut;		// RGB565 -> RGBA8888
	sf::View						view;

	sf::Font						font;
	bool							fontOk = false;
	// the display font for titles (fonts\display.ttf; CRenderer::m_iFace = 1), any size, cached per size
	sf::Font						font2;
	bool							font2Ok = false;
	int								ascent2 = 13;
	std::unordered_map<unsigned int, std::unordered_map<char32_t, CachedGlyph>> sized2;

	// the interface on the graphics card (UiGpu.h): the list, and the picture it is drawn into
	std::unique_ptr<CUiGpu>			uiGpu;
	std::optional<sf::RenderTexture> uiRT;
	const sf::Texture *				composite = nullptr;	// this frame's world + interface, if any

	float							gameScaleX = 1.f, gameScaleY = 1.f;	// window pixels per game pixel (last frame)
	float							gameOffX = 0.f, gameOffY = 0.f;		// window position of the game image (last frame)
	std::optional<sf::Texture>		cursorTex;
	std::vector<std::uint32_t>		cursorPx;
	int								cursorW = 0, cursorH = 0, cursorOffX = 0, cursorOffY = 0;
	std::unordered_map<char32_t, CachedGlyph> glyphs;
	int								ascent = 13;
	int								lineHeight = 16;

	CRendererImpl() : view(sf::FloatRect({0.f, 0.f}, {(float)RENDER_WIDTH, (float)RENDER_HEIGHT})) {}

	void CacheGlyphs(const char32_t * pCodePoints, size_t iCount) { CacheGlyphsSized(pCodePoints, iCount, kFontCharSize, glyphs); }

	void CacheGlyphsSized(const char32_t * pCodePoints, size_t iCount, unsigned int uSize, std::unordered_map<char32_t, CachedGlyph> & cache,
		int iFace = 0)
	{
		sf::Font & f = (iFace == 1) ? font2 : font;
		if (!(iFace == 1 ? font2Ok : fontOk) || iCount == 0) return;
		std::vector<std::pair<char32_t, sf::Glyph>> loaded;
		loaded.reserve(iCount);
		for (size_t i = 0; i < iCount; i++)
			loaded.emplace_back(pCodePoints[i], f.getGlyph(pCodePoints[i], uSize, false));

		// One read-back of the glyph page for the whole batch.
		const sf::Image page = f.getTexture(uSize).copyToImage();
		const std::uint8_t * pPx = page.getPixelsPtr();
		const int iPageW = (int)page.getSize().x, iPageH = (int)page.getSize().y;

		for (size_t i = 0; i < loaded.size(); i++) {
			const sf::Glyph & g = loaded[i].second;
			CachedGlyph cg;
			cg.advance = (int)std::lround(g.advance);
			cg.left = (int)std::floor(g.bounds.position.x);
			cg.top  = (int)std::floor(g.bounds.position.y);
			cg.w = g.textureRect.size.x;
			cg.h = g.textureRect.size.y;
			if (cg.w > 0 && cg.h > 0 && pPx != NULL) {
				cg.alpha.resize((size_t)cg.w * cg.h);
				for (int y = 0; y < cg.h; y++)
				for (int x = 0; x < cg.w; x++) {
					const int tx = g.textureRect.position.x + x, ty = g.textureRect.position.y + y;
					std::uint8_t a = 0;
					if (tx >= 0 && ty >= 0 && tx < iPageW && ty < iPageH)
						a = pPx[((size_t)ty * iPageW + tx) * 4 + 3];
					if (!kTextAntialias) a = (a >= 128) ? 255 : 0;
					cg.alpha[(size_t)y * cg.w + x] = a;
				}
			}
			else { cg.w = cg.h = 0; }
			cache[loaded[i].first] = std::move(cg);
		}
	}

	const CachedGlyph & Glyph(char32_t cp)
	{
		auto it = glyphs.find(cp);
		if (it != glyphs.end()) return it->second;
		CacheGlyphs(&cp, 1);
		return glyphs[cp];
	}

	// the same glyph at twice the size (HD rendering)
	const CachedGlyph & Glyph2(char32_t cp)
	{
		auto it = glyphs2.find(cp);
		if (it != glyphs2.end()) return it->second;
		CacheGlyphsSized(&cp, 1, kFontCharSize * 2, glyphs2);
		return glyphs2[cp];
	}

	float Kerning(char32_t a, char32_t b)
	{
		return fontOk ? font.getKerning(a, b, kFontCharSize) : 0.f;
	}

	// the UI font at any other size (big titles); cached per size
	std::unordered_map<unsigned int, std::unordered_map<char32_t, CachedGlyph>> sized;
	const CachedGlyph & GlyphSized(char32_t cp, unsigned int uSize)
	{
		if (uSize == kFontCharSize) return Glyph(cp);
		if (uSize == kFontCharSize * 2) return Glyph2(cp);
		std::unordered_map<char32_t, CachedGlyph> & cache = sized[uSize];
		auto it = cache.find(cp);
		if (it != cache.end()) return it->second;
		CacheGlyphsSized(&cp, 1, uSize, cache);
		return cache[cp];
	}
	float KerningSized(char32_t a, char32_t b, unsigned int uSize)
	{
		return fontOk ? font.getKerning(a, b, uSize) : 0.f;
	}
	// the display font
	const CachedGlyph & GlyphFace2(char32_t cp, unsigned int uSize)
	{
		std::unordered_map<char32_t, CachedGlyph> & cache = sized2[uSize];
		auto it = cache.find(cp);
		if (it != cache.end()) return it->second;
		CacheGlyphsSized(&cp, 1, uSize, cache, 1);
		return cache[cp];
	}
	float KerningFace2(char32_t a, char32_t b, unsigned int uSize)
	{
		return font2Ok ? font2.getKerning(a, b, uSize) : 0.f;
	}
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRenderer::CRenderer()
{
	m_hWnd			= NULL;
	m_cPixelFormat	= 1;
	m_framecnt		= 0;
	m_iWindowW = m_iWindowH = 0;
	m_iUsedWindowW = RENDER_WIDTH * 2;
	m_iUsedWindowH = RENDER_HEIGHT * 2;
	m_bWidescreen	= FALSE;
	m_iFrameLimit	= kMaxFps;	// (until the settings are read)
	m_bVSync		= FALSE;
	m_iRenderQuality = RQ_AUTO;
	m_bLastIntegerScale = false;
	m_pImpl			= NULL;
	m_pOverlay		= NULL;
	m_bHdSprites	= TRUE;
	m_bGpuUi		= TRUE;
	m_bUiRec		= false;
	m_iUiGpuSteps	= 0;
	m_pWorldOverlay	= NULL;
	m_bBorderless	= FALSE;
	SetRect(&m_rcClipArea, 0, 0, RENDER_WIDTH, RENDER_HEIGHT);

	// first start: a normal window with a border; Options (or Alt+Enter) can switch to
	// fullscreen or borderless, and that choice is saved and used from then on
	m_bFullMode		= FALSE;
}

CRenderer::~CRenderer()
{
	delete m_pImpl;		// closes the SFML window / context
	m_pImpl = NULL;
}

BOOL CRenderer::bInit(HWND hWnd)
{
	int iS, iD;

	m_hWnd = hWnd;
	SetRect(&m_rcClipArea, 0, 0, RENDER_WIDTH, RENDER_HEIGHT);

	// The whole game renders in RGB 5:6:5.
	m_cPixelFormat = 1;

	for (iS = 0; iS < 64; iS++)
	for (iD = 0; iD < 64; iD++) {
		m_lTransRB100[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 1.0f);
		m_lTransG100[iD][iS]  = _CalcMaxValue(iS, iD, 'G', 1, 1.0f);

		m_lTransRB70[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.7f);
		m_lTransG70[iD][iS]  = _CalcMaxValue(iS, iD, 'G', 1, 0.7f);

		m_lTransRB50[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.5f);
		m_lTransG50[iD][iS]  = _CalcMaxValue(iS, iD, 'G', 1, 0.5f);

		m_lTransRB25[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.25f);
		m_lTransG25[iD][iS]  = _CalcMaxValue(iS, iD, 'G', 1, 0.25f);

		m_lTransRB2[iD][iS] = _CalcMaxValue(iS, iD, 'R', 2, 1.0f);
		m_lTransG2[iD][iS]  = _CalcMaxValue(iS, iD, 'G', 2, 1.0f);

		m_lFadeRB[iD][iS]  = _CalcMinValue(iS, iD, 'R');
		m_lFadeG[iD][iS]   = _CalcMinValue(iS, iD, 'G');

		G_lTransRB100[iD][iS] = m_lTransRB100[iD][iS];
		G_lTransG100[iD][iS]  = m_lTransG100[iD][iS];
		G_lTransRB70[iD][iS]  = m_lTransRB70[iD][iS];
		G_lTransG70[iD][iS]   = m_lTransG70[iD][iS];
		G_lTransRB50[iD][iS]  = m_lTransRB50[iD][iS];
		G_lTransG50[iD][iS]   = m_lTransG50[iD][iS];
		G_lTransRB25[iD][iS]  = m_lTransRB25[iD][iS];
		G_lTransG25[iD][iS]   = m_lTransG25[iD][iS];
		G_lTransRB2[iD][iS]   = m_lTransRB2[iD][iS];
		G_lTransG2[iD][iS]    = m_lTransG2[iD][iS];
	}

	delete m_pImpl;
	m_pImpl = new CRendererImpl;
	m_pImpl->rgba.assign((size_t)RENDER_WIDTH * RENDER_HEIGHT, 0xFF000000u);
	m_pImpl->lut.resize(65536);
	for (std::uint32_t v = 0; v < 65536; v++) {
		const std::uint32_t r5 = (v >> 11) & 31, g6 = (v >> 5) & 63, b5 = v & 31;
		const std::uint32_t r = (r5 << 3) | (r5 >> 2), g = (g6 << 2) | (g6 >> 4), b = (b5 << 3) | (b5 >> 2);
		m_pImpl->lut[v] = r | (g << 8) | (b << 16) | 0xFF000000u;
	}

	try {
		// Attach SFML to the game's existing Win32 window. SFML subclasses the window and
		// forwards every message to the game's WndProc, so socket/keyboard/IME handling
		// keeps working unchanged.
		m_pImpl->window.emplace(sf::WindowHandle(hWnd));
		m_pImpl->window->setVerticalSyncEnabled(m_bVSync != FALSE);
		m_pImpl->window->setFramerateLimit(_FrameLimitNow());
		m_pImpl->window->setMouseCursorVisible(false);	// the game draws its own cursor

		m_pImpl->texture.emplace();		// black: shown until the first frame is drawn
		if (!m_pImpl->texture->resize({(unsigned)RENDER_WIDTH, (unsigned)RENDER_HEIGHT})) return FALSE;
		m_pImpl->texture->update(reinterpret_cast<const std::uint8_t *>(m_pImpl->rgba.data()));
		m_pImpl->texture->setSmooth(false);
	}
	catch (...) {
		return FALSE;
	}

	_ApplyDisplayMode();

	if (!_LoadFont()) {
		MessageBox(hWnd, "No usable font found (tried fonts\\default.ttf, Trebuchet MS, Arial, Tahoma, Verdana).", "ERROR", MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	// The game is drawn on the graphics card: it needs OpenGL shaders (any graphics card or chip
	// from the last 15 years, with its driver installed).
	if (!_GpuDraws()) {
		MessageBox(hWnd, "Helbreath Unrivaled could not start its graphics.\n\n"
			"Your graphics card or its driver doesn't support OpenGL shaders. Please install the latest "
			"driver for your graphics card (from NVIDIA, AMD or Intel) and try again.",
			"Graphics not supported", MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	ClearBackB4();
	return TRUE;
}

void CRenderer::Shutdown()
{
	delete m_pImpl;
	m_pImpl = NULL;
}

// ---------------------------------------------------------------------------- interface on the GPU

void CRenderer::BeginUiRecording()
{
	m_bUiRec = false;
	if (!MirrorOn()) return;
	m_pImpl->uiGpu->Clear(GetTickCount());
	m_bUiRec = true;
}

void CRenderer::AbortUiRecording()
{
	m_bUiRec = false;
}

CUiGpu * CRenderer::UiGpuRecorder()
{
	return (m_bUiRec && m_pImpl != NULL) ? m_pImpl->uiGpu.get() : NULL;
}

bool CRenderer::_LoadFont()
{
	std::vector<std::string> candidates;
	for (const char * p : kLocalFonts) candidates.push_back(p);

	char cWinDir[MAX_PATH];
	UINT n = GetWindowsDirectory(cWinDir, MAX_PATH);
	if (n > 0 && n < MAX_PATH)
		for (const char * p : kSystemFonts) candidates.push_back(std::string(cWinDir) + "\\Fonts\\" + p);

	bool bShipped = false;
	for (size_t i = 0; i < candidates.size(); i++) {
		const std::string & path = candidates[i];
		if (GetFileAttributes(path.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
		if (m_pImpl->font.openFromFile(path)) { m_pImpl->fontOk = true; bShipped = i < sizeof(kLocalFonts) / sizeof(kLocalFonts[0]); break; }
	}
	if (!m_pImpl->fontOk) return false;

	m_pImpl->lineHeight = (int)std::lround(m_pImpl->font.getLineSpacing(kFontCharSize));
	if (m_pImpl->lineHeight < 8) m_pImpl->lineHeight = 16;
	m_pImpl->ascent = (int)std::lround(m_pImpl->lineHeight * 0.8f);
	if (bShipped) {
		// the game's own font (fonts\default.ttf): set in the same place as Trebuchet MS, which the
		// windows were laid out for: lines 16 apart at the normal size, capitals from 3 px down
		const float fCap = m_pImpl->font.getGlyph(U'H', kFontCharSize, false).bounds.size.y;
		m_pImpl->lineHeight = 16;
		if (fCap > 4.f) m_pImpl->ascent = 3 + (int)std::lround(fCap);
	}

	// Pre-load ASCII + Latin-1 so normal text never needs a texture read-back.
	std::vector<char32_t> warm;
	for (char32_t c = 32; c < 127; c++) warm.push_back(c);
	for (char32_t c = 160; c < 256; c++) warm.push_back(c);
	m_pImpl->CacheGlyphs(warm.data(), warm.size());

	// the display font for titles, if the game folder has it (otherwise titles use the UI font)
	if (GetFileAttributes("fonts\\display.ttf") != INVALID_FILE_ATTRIBUTES && m_pImpl->font2.openFromFile("fonts\\display.ttf")) {
		m_pImpl->font2Ok = true;
		m_pImpl->ascent2 = (int)std::lround(m_pImpl->font2.getLineSpacing(kFontCharSize) * 0.8f);
	}
	return true;
}

bool CRenderer::HasDisplayFont() const
{
	return m_pImpl != NULL && m_pImpl->font2Ok;
}

float CRenderer::ScreenAspect()
{
	if (m_bFullMode) return (float)kFullscreenW / (float)kFullscreenH;
	if (m_pImpl == NULL || !m_pImpl->window) return 4.f / 3.f;
	const sf::Vector2u ws = m_pImpl->window->getSize();
	return (ws.x > 0 && ws.y > 0) ? (float)ws.x / (float)ws.y : 4.f / 3.f;
}

const int CRenderer::kWindowSizes[CRenderer::kNumWindowSizes][2] = { {800, 600}, {1024, 768}, {1280, 960} };

void CRenderer::SetWindowSize(int iW, int iH)
{
	m_iWindowW = (iW > 0 && iH > 0) ? iW : 0;
	m_iWindowH = (iW > 0 && iH > 0) ? iH : 0;
	if (m_pImpl != NULL && !m_bFullMode) _ApplyDisplayMode();
}

void CRenderer::GetWindowSize(int * pW, int * pH) const
{
	*pW = m_iUsedWindowW;
	*pH = m_iUsedWindowH;
}

void CRenderer::SetWidescreen(BOOL bWidescreen)
{
	m_bWidescreen = bWidescreen ? TRUE : FALSE;
}

void CRenderer::SetFrameLimit(int iFps)
{
	m_iFrameLimit = (iFps > 0 && iFps <= kMaxFps) ? iFps : kMaxFps;
	if (m_pImpl != NULL && m_pImpl->window)
		m_pImpl->window->setFramerateLimit(_FrameLimitNow());
}

unsigned int CRenderer::_FrameLimitNow() const
{
	if (m_bVSync) return 0u;		// (VSync paces it)
	const int kMenuFps = 60;
	if (m_bMenuScreen && m_iFrameLimit > kMenuFps) return (unsigned int)kMenuFps;
	return (unsigned int)((m_iFrameLimit > 0 && m_iFrameLimit <= kMaxFps) ? m_iFrameLimit : kMaxFps);
}

void CRenderer::SetMenuScreen(bool bMenu)
{
	if (bMenu == m_bMenuScreen) return;
	m_bMenuScreen = bMenu;
	if (m_pImpl != NULL && m_pImpl->window) m_pImpl->window->setFramerateLimit(_FrameLimitNow());
}

void CRenderer::SetVSync(BOOL bOn)
{
	m_bVSync = bOn ? TRUE : FALSE;
	if (m_pImpl != NULL && m_pImpl->window) {
		// SFML advises against combining the two, so the FPS limit is off while VSync is on.
		m_pImpl->window->setVerticalSyncEnabled(m_bVSync != FALSE);
		m_pImpl->window->setFramerateLimit(_FrameLimitNow());
	}
}

void CRenderer::SetRenderQuality(int iMode)
{
	m_iRenderQuality = (iMode >= RQ_AUTO && iMode <= RQ_HD) ? iMode : RQ_AUTO;
}

void CRenderer::_ApplyDisplayMode()
{
	if (m_hWnd == NULL) return;

	MONITORINFO mi;
	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);
	HMONITOR hMon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
	if (!GetMonitorInfo(hMon, &mi)) {
		SetRect(&mi.rcMonitor, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
		mi.rcWork = mi.rcMonitor;
	}

	// Window style: fullscreen and borderless windows are plain popups; a bordered window
	// gets a normal title bar (with minimise and close buttons) so it can be moved around.
	const bool bFrame = !m_bFullMode && !m_bBorderless;
	const DWORD dwFrameStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	DWORD dwStyle = (DWORD)GetWindowLongPtr(m_hWnd, GWL_STYLE);
	dwStyle &= ~(dwFrameStyle | WS_POPUP | WS_THICKFRAME | WS_MAXIMIZEBOX);
	dwStyle |= bFrame ? dwFrameStyle : WS_POPUP;
	SetWindowLongPtr(m_hWnd, GWL_STYLE, (LONG_PTR)dwStyle);
	const DWORD dwExStyle = (DWORD)GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);

	if (m_bFullMode) {
		// Fullscreen: cover the whole monitor, the image is letterboxed in iFlip().
		SetWindowPos(m_hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_SHOWWINDOW | SWP_FRAMECHANGED);
	}
	else {
		// How much the title bar and frame add around the game area.
		RECT rcFrame = { 0, 0, 0, 0 };
		if (bFrame) AdjustWindowRectEx(&rcFrame, dwStyle, FALSE, dwExStyle);
		const int iFrameW = rcFrame.right - rcFrame.left, iFrameH = rcFrame.bottom - rcFrame.top;

		const int iWorkW = mi.rcWork.right - mi.rcWork.left;
		const int iWorkH = mi.rcWork.bottom - mi.rcWork.top;
		// The requested size if it fits on the screen, otherwise the largest standard size that does.
		int iClientW = m_iWindowW, iClientH = m_iWindowH;
		if (iClientW <= 0 || iClientW + iFrameW > iWorkW || iClientH + iFrameH > iWorkH) {
			iClientW = kWindowSizes[0][0];
			iClientH = kWindowSizes[0][1];
			for (int i = kNumWindowSizes - 1; i >= 0; i--)
				if (kWindowSizes[i][0] + iFrameW <= iWorkW && kWindowSizes[i][1] + iFrameH <= iWorkH) {
					iClientW = kWindowSizes[i][0];
					iClientH = kWindowSizes[i][1];
					break;
				}
		}
		m_iUsedWindowW = iClientW;
		m_iUsedWindowH = iClientH;
		const int iW = iClientW + iFrameW, iH = iClientH + iFrameH;
		int iX = mi.rcWork.left + (iWorkW - iW) / 2;
		int iY = mi.rcWork.top + (iWorkH - iH) / 2;
		if (iY < mi.rcWork.top) iY = mi.rcWork.top;	// keep the title bar reachable
		if (iX < mi.rcWork.left) iX = mi.rcWork.left;
		SetWindowPos(m_hWnd, HWND_TOP, iX, iY, iW, iH, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
	}

	if (m_pImpl != NULL && m_pImpl->window)
		m_pImpl->window->setMouseCursorGrabbed(m_bFullMode ? true : false);
}

void CRenderer::SetBorderless(BOOL bBorderless)
{
	m_bBorderless = bBorderless ? TRUE : FALSE;
	if (m_pImpl != NULL && !m_bFullMode) _ApplyDisplayMode();
}

void CRenderer::ChangeDisplayMode(HWND hWnd)
{
	if (hWnd != NULL) m_hWnd = hWnd;
	m_bFullMode = !m_bFullMode;
	_ApplyDisplayMode();
}

bool CRenderer::iFlip()
{
	if (m_pImpl == NULL || !m_pImpl->window || !m_pImpl->texture) return false;
	sf::RenderWindow & window = *m_pImpl->window;
	if (!window.isOpen()) return false;

	// The frame: drawn on the graphics card (UiGpu.h), into a render texture the size of the 2x
	// picture. (A frame shown without being recorded - nothing should - shows the last one again.)
	LONGLONG tProf = prof::Now();
	m_iUiGpuSteps = 0;
	if (m_bUiRec && m_pImpl->uiGpu) {
		const int W2 = 2 * RENDER_WIDTH, H2 = 2 * RENDER_HEIGHT;
		if (!m_pImpl->uiRT || m_pImpl->uiRT->getSize().x != (unsigned)W2 || m_pImpl->uiRT->getSize().y != (unsigned)H2) {
			m_pImpl->composite = NULL;
			m_pImpl->uiRT.emplace();
			if (!m_pImpl->uiRT->resize({(unsigned)W2, (unsigned)H2})) m_pImpl->uiRT.reset();
		}
		if (m_pImpl->uiRT) {
			sf::RenderTexture & rt = *m_pImpl->uiRT;
			rt.clear(sf::Color::Black);
			m_pImpl->uiGpu->Draw(rt, W2, H2);
			rt.display();
			m_pImpl->composite = &rt.getTexture();
			m_iUiGpuSteps = m_pImpl->uiGpu->Count();
			if (!m_strShot.empty()) {		// a screenshot of this frame
				rt.getTexture().copyToImage().saveToFile(m_strShot);
				m_strShot.clear();
			}
		}
		m_pImpl->uiGpu->Trim(GetTickCount());
		m_bUiRec = false;
	}

	prof::Add(prof::CONVERT, tProf); tProf = prof::Now();

	// Where the game image goes:
	//  - the screen area is the whole window, except in fullscreen where it is a fixed
	//    1920x1080 screen (scaled down/up to fit if the monitor is a different size);
	//  - Letterbox fits 640x480 into it at 4:3, Widescreen (fullscreen only) stretches it.
	const sf::Vector2u ws = window.getSize();
	if (ws.x > 0 && ws.y > 0) {
		float fAreaX = 0.f, fAreaY = 0.f, fAreaW = (float)ws.x, fAreaH = (float)ws.y;
		if (m_bFullMode) {
			const float k = std::min((float)ws.x / kFullscreenW, (float)ws.y / kFullscreenH);
			fAreaW = kFullscreenW * k;
			fAreaH = kFullscreenH * k;
			fAreaX = (ws.x - fAreaW) / 2.f;
			fAreaY = (ws.y - fAreaH) / 2.f;
		}
		int iX, iY, iW, iH;
		bool bSmooth;
		m_bLastIntegerScale = false;
		if (m_bFullMode && m_bWidescreen && RENDER_WIDTH > 640) {
			// Widescreen: stretch the game picture to fill the screen
			iX = (int)std::lround(fAreaX); iY = (int)std::lround(fAreaY);
			iW = std::max(1, (int)std::lround(fAreaW)); iH = std::max(1, (int)std::lround(fAreaH));
			bSmooth = true;
			m_bLastIntegerScale = false;
		}
		else {	// fit the canvas into the area, keeping its shape
			const float fScale = std::min(fAreaW / RENDER_WIDTH, fAreaH / RENDER_HEIGHT);
			iW = std::max(1, (int)std::lround(RENDER_WIDTH * fScale));
			iH = std::max(1, (int)std::lround(RENDER_HEIGHT * fScale));
			iX = (int)std::lround(fAreaX + (fAreaW - iW) / 2.f);
			iY = (int)std::lround(fAreaY + (fAreaH - iH) / 2.f);
			// Whole-number scales stay pixel-sharp; fractional scales are filtered to avoid uneven pixels.
			bSmooth = !((iW % RENDER_WIDTH == 0) && (iH % RENDER_HEIGHT == 0));
			m_bLastIntegerScale = !bSmooth;
		}
		m_pImpl->gameScaleX = (float)iW / RENDER_WIDTH;
		m_pImpl->gameScaleY = (float)iH / RENDER_HEIGHT;
		m_pImpl->gameOffX = (float)iX;
		m_pImpl->gameOffY = (float)iY;
		m_pImpl->view.setViewport(sf::FloatRect(
			{(float)iX / ws.x, (float)iY / ws.y}, {(float)iW / ws.x, (float)iH / ws.y}));
		m_pImpl->texture->setSmooth(bSmooth);
		if (m_pImpl->composite != NULL && m_pImpl->uiRT)
			m_pImpl->uiRT->setSmooth(!(iW % (2 * RENDER_WIDTH) == 0 && iH % (2 * RENDER_HEIGHT) == 0));
	}

	window.setView(m_pImpl->view);
	window.clear(sf::Color::Black);
	_DrawGameImage(window);
	m_pImpl->composite = NULL;
	m_bUiRec = false;

	// Full-resolution world effects, then UI on top (one unit = one window pixel)
	if (m_pWorldOverlay != NULL && m_pWorldOverlay->IsVisible() && ws.x > 0 && ws.y > 0) {
		window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {(float)ws.x, (float)ws.y})));
		m_pWorldOverlay->Draw(window);
	}
	if (m_pOverlay != NULL && m_pOverlay->IsVisible() && ws.x > 0 && ws.y > 0) {
		window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {(float)ws.x, (float)ws.y})));
		m_pOverlay->Draw(window);
	}
	window.display();
	prof::Add(prof::DISPLAY, tProf);

	m_framecnt++;
	return true;
}

// ---------------------------------------------------------------------------- drawing on the graphics card

bool CRenderer::_GpuDraws()
{
	if (!m_bGpuUi || m_pImpl == NULL || !m_pImpl->window || m_pImpl->lut.empty()) return false;
	if (!m_pImpl->uiGpu) m_pImpl->uiGpu.reset(new CUiGpu(m_pImpl->lut.data()));
	return m_pImpl->uiGpu->Ok();
}

bool CRenderer::MirrorOn()
{
	return _GpuDraws();
}

void CRenderer::SetUiCursor(const WORD * pSrc, int iPitch, int iW, int iH, WORD wColorKey, int iOffX, int iOffY)
{
	if (m_pImpl == NULL || pSrc == NULL || iW <= 0 || iH <= 0) return;
	std::vector<std::uint32_t> px((size_t)iW * iH);
	for (int y = 0; y < iH; y++)
		for (int x = 0; x < iW; x++) {
			const WORD w = pSrc[(size_t)y * iPitch + x];
			px[(size_t)y * iW + x] = (w == wColorKey) ? 0u : m_pImpl->lut[w];
		}
	m_pImpl->cursorOffX = iOffX;
	m_pImpl->cursorOffY = iOffY;
	if (m_pImpl->cursorTex && iW == m_pImpl->cursorW && iH == m_pImpl->cursorH && px == m_pImpl->cursorPx) return;	// unchanged

	if (!m_pImpl->cursorTex || iW != m_pImpl->cursorW || iH != m_pImpl->cursorH) {
		m_pImpl->cursorTex.emplace();
		if (!m_pImpl->cursorTex->resize({(unsigned)iW, (unsigned)iH})) { m_pImpl->cursorTex.reset(); return; }
	}
	m_pImpl->cursorW = iW;
	m_pImpl->cursorH = iH;
	m_pImpl->cursorPx.swap(px);
	m_pImpl->cursorTex->update(reinterpret_cast<const std::uint8_t *>(m_pImpl->cursorPx.data()));
}

bool CRenderer::HasUiCursor() const
{
	return m_pImpl != NULL && m_pImpl->cursorTex.has_value();
}

void CRenderer::DrawUiCursor(sf::RenderWindow & window, int iWinX, int iWinY)
{
	if (!HasUiCursor()) return;
	const float sx = m_pImpl->gameScaleX, sy = m_pImpl->gameScaleY;
	m_pImpl->cursorTex->setSmooth(std::fabs(sx - std::round(sx)) > 0.01f || std::fabs(sy - std::round(sy)) > 0.01f);
	sf::Sprite cursor(*m_pImpl->cursorTex);
	cursor.setScale({sx, sy});
	cursor.setPosition({std::round(iWinX + m_pImpl->cursorOffX * sx), std::round(iWinY + m_pImpl->cursorOffY * sy)});
	window.draw(cursor);
}

void CRenderer::GameToWindow(float fGameX, float fGameY, float & fWinX, float & fWinY) const
{
	fWinX = m_pImpl->gameOffX + fGameX * m_pImpl->gameScaleX;
	fWinY = m_pImpl->gameOffY + fGameY * m_pImpl->gameScaleY;
}

float CRenderer::GameScaleX() const { return m_pImpl->gameScaleX; }
float CRenderer::GameScaleY() const { return m_pImpl->gameScaleY; }

const sf::Font * CRenderer::GetDisplayFont() const
{
	if (m_pImpl == NULL || !m_pImpl->font2Ok) return NULL;
	return &m_pImpl->font2;
}

void CRenderer::TitleOut(int x, int y, char const * cStr, COLORREF rgb, int iPx)
{
	if (HasDisplayFont()) {
		const int iFace = m_iFace;
		m_iFace = 1;
		TextOutSized(x + 1, y + 1, cStr, RGB(0, 0, 0), iPx);
		TextOutSized(x, y, cStr, rgb, iPx);
		m_iFace = iFace;
	}
	else { TextOutSized(x, y, cStr, rgb, iPx); TextOutSized(x + 1, y, cStr, rgb, iPx); }
}

int CRenderer::TitleWidth(char const * cStr, int iPx)
{
	if (!HasDisplayFont()) return TextWidthSized(cStr, iPx) + 1;
	const int iFace = m_iFace;
	m_iFace = 1;
	const int w = TextWidthSized(cStr, iPx) + 1;
	m_iFace = iFace;
	return w;
}

const sf::Font * CRenderer::GetFont() const
{
	if (m_pImpl == NULL || !m_pImpl->fontOk) return NULL;
	return &m_pImpl->font;
}

// xBR level 2 (Hyllian's algorithm), one pass at window resolution: for every screen pixel
// it looks at the 21 nearest game pixels, finds edges at 30, 45 and 60 degrees and draws
// them as smooth lines instead of stair-steps, while flat areas and hard outlines stay crisp.
static const char * kHdShader = R"GLSL(
#version 120
uniform sampler2D source;
uniform vec2 sourceSize;
uniform float scale;		// window pixels per game pixel

const float EQ_THRESHOLD = 1.5;		// how different two pixels must be to count as an edge (tuned on Helbreath sprites)
const float LV2_COEF = 2.0;
const vec4 Ao = vec4( 1.0, -1.0, -1.0,  1.0);
const vec4 Bo = vec4( 1.0,  1.0, -1.0, -1.0);
const vec4 Co = vec4( 1.5,  0.5, -0.5,  0.5);
const vec4 Ax = vec4( 1.0, -1.0, -1.0,  1.0);
const vec4 Bx = vec4( 0.5,  2.0, -0.5, -2.0);
const vec4 Cx = vec4( 1.0,  1.0, -0.5,  0.0);
const vec4 Ay = vec4( 1.0, -1.0, -1.0,  1.0);
const vec4 By = vec4( 2.0,  0.5, -2.0, -0.5);
const vec4 Cy = vec4( 2.0,  0.0, -1.0,  0.5);
const vec4 Ci = vec4( 0.25, 0.25, 0.25, 0.25);
const vec3 Yw = vec3(14.352, 28.176, 5.472);		// luma weights x 48

vec3 px(vec2 base, float dx, float dy) { return texture2D(source, (base + vec2(dx, dy)) / sourceSize).rgb; }
float lum(vec3 c) { return dot(c, Yw); }
vec4 df(vec4 a, vec4 b) { return abs(a - b); }
float cdf(vec3 a, vec3 b) { vec3 d = abs(a - b); return d.r + d.g + d.b; }
vec4 eq(vec4 a, vec4 b) { return step(df(a, b), vec4(EQ_THRESHOLD)); }
vec4 neq(vec4 a, vec4 b) { return vec4(1.0) - eq(a, b); }
vec4 wd(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{ return df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h); }

void main()
{
	vec2 pos  = gl_TexCoord[0].xy * sourceSize;
	vec2 fp   = fract(pos);
	vec2 base = floor(pos) + 0.5;

	//        A1 B1 C1
	//     A0  A  B  C C4
	//     D0  D  E  F F4
	//     G0  G  H  I I4
	//        G5 H5 I5
	vec3 A1 = px(base,-1.0,-2.0), B1 = px(base, 0.0,-2.0), C1 = px(base, 1.0,-2.0);
	vec3 A0 = px(base,-2.0,-1.0), A  = px(base,-1.0,-1.0), B  = px(base, 0.0,-1.0), C  = px(base, 1.0,-1.0), C4 = px(base, 2.0,-1.0);
	vec3 D0 = px(base,-2.0, 0.0), D  = px(base,-1.0, 0.0), E  = px(base, 0.0, 0.0), F  = px(base, 1.0, 0.0), F4 = px(base, 2.0, 0.0);
	vec3 G0 = px(base,-2.0, 1.0), G  = px(base,-1.0, 1.0), H  = px(base, 0.0, 1.0), I  = px(base, 1.0, 1.0), I4 = px(base, 2.0, 1.0);
	vec3 G5 = px(base,-1.0, 2.0), H5 = px(base, 0.0, 2.0), I5 = px(base, 1.0, 2.0);

	vec4 b = vec4(lum(B), lum(D), lum(H), lum(F));
	vec4 c = vec4(lum(C), lum(A), lum(G), lum(I));
	vec4 e = vec4(lum(E));
	vec4 d = b.yzwx, f = b.wxyz, g = c.zwxy, h = b.zwxy, i = c.wxyz;
	vec4 i4 = vec4(lum(I4), lum(C1), lum(A0), lum(G5));
	vec4 i5 = vec4(lum(I5), lum(C4), lum(A1), lum(G0));
	vec4 h5 = vec4(lum(H5), lum(F4), lum(B1), lum(D0));
	vec4 f4 = h5.yzwx;

	vec4 fx      = Ao * fp.y + Bo * fp.x;
	vec4 fx_left = Ax * fp.y + Bx * fp.x;
	vec4 fx_up   = Ay * fp.y + By * fp.x;

	vec4 irlv0 = neq(e, f) * neq(e, h);
	vec4 irlv1 = irlv0 * clamp(neq(f, b) * neq(h, d) + eq(e, i) * neq(f, i4) * neq(h, i5) + eq(e, g) + eq(e, c), 0.0, 1.0);
	vec4 irlv2_left = neq(e, g) * neq(d, g);
	vec4 irlv2_up   = neq(e, c) * neq(b, c);

	vec4 delta = vec4(clamp(1.0 / max(scale, 1.0), 0.12, 0.5));
	vec4 fx45i = clamp((fx + delta - Co - Ci) / (2.0 * delta), 0.0, 1.0);
	vec4 fx45  = clamp((fx + delta - Co) / (2.0 * delta), 0.0, 1.0);
	vec4 fx30  = clamp((fx_left + delta - Cx) / (2.0 * delta), 0.0, 1.0);
	vec4 fx60  = clamp((fx_up + delta - Cy) / (2.0 * delta), 0.0, 1.0);

	vec4 wd1 = wd(e, c, g, i, h5, f4, h, f);
	vec4 wd2 = wd(h, d, i5, f, i4, b, e, i);
	vec4 edri     = step(wd1, wd2) * irlv0;
	vec4 edr      = step(wd1 + 0.1, wd2) * irlv1;
	vec4 edr_left = step(LV2_COEF * df(f, g), df(h, c)) * irlv2_left * edr;
	vec4 edr_up   = step(LV2_COEF * df(h, c), df(f, g)) * irlv2_up * edr;

	fx45 = edr * fx45; fx30 = edr_left * fx30; fx60 = edr_up * fx60; fx45i = edri * fx45i;
	vec4 pxs = step(df(e, f), df(e, h));
	vec4 m = max(max(fx30, fx60), max(fx45, fx45i));

	vec3 res1 = E;
	res1 = mix(res1, mix(H, F, pxs.x), m.x);
	res1 = mix(res1, mix(B, D, pxs.z), m.z);
	vec3 res2 = E;
	res2 = mix(res2, mix(F, B, pxs.y), m.y);
	res2 = mix(res2, mix(D, H, pxs.w), m.w);
	vec3 res = mix(res1, res2, step(cdf(E, res1), cdf(E, res2)));
	gl_FragColor = vec4(res, 1.0);
}
)GLSL";

bool CRenderer::IsHdFilterAvailable() const
{
	return sf::Shader::isAvailable();
}

void CRenderer::_DrawGameImage(sf::RenderWindow & window)
{
	// The picture to show: the 2x one when HD rendering produced it this frame, otherwise the
	// normal one. f = picture pixels per game pixel; the view is in game pixels.
	const bool b2 = (m_pImpl->composite != NULL);
	sf::Texture * pTex = b2 ? const_cast<sf::Texture *>(m_pImpl->composite) : &*m_pImpl->texture;
	unsigned int f = b2 ? 2u : 1u;
	// HD Graphics off, drawn on the graphics card at 2x: the HD filter is made for the original
	// pixel art at game size, so the picture is first brought back to it (each 2x2 block
	// averaged: exactly the original sprites, and smooth text and panels).
	if (m_iRenderQuality == RQ_HD && !m_bHdSprites && f == 2) {
		const unsigned int uw = RENDER_WIDTH, uh = RENDER_HEIGHT;
		if (!m_pImpl->down1 || m_pImpl->down1->getSize().x != uw || m_pImpl->down1->getSize().y != uh) {
			m_pImpl->down1.emplace();
			if (!m_pImpl->down1->resize({uw, uh})) m_pImpl->down1.reset();
		}
		if (m_pImpl->down1) {
			sf::RenderTexture & d = *m_pImpl->down1;
			const bool bWasSmooth = pTex->isSmooth();
			pTex->setSmooth(true);		// (sampled at each 2x2 block's centre: their average)
			sf::Sprite half(*pTex);
			half.setScale({0.5f, 0.5f});
			d.clear(sf::Color::Black);
			d.draw(half);
			d.display();
			pTex->setSmooth(bWasSmooth);
			pTex = const_cast<sf::Texture *>(&d.getTexture());
			f = 1;
		}
	}
	sf::Texture & tex = *pTex;
	const float fWinPerSrc = std::min(m_pImpl->gameScaleX, m_pImpl->gameScaleY) / (float)f;	// window pixels per picture pixel

	// HD filter: the xBR shader runs for every window pixel straight from the picture.
	if (m_iRenderQuality == RQ_HD) {
		if (!m_pImpl->hdTried) {
			m_pImpl->hdTried = true;
			if (sf::Shader::isAvailable()) {
				m_pImpl->hdShader.emplace();
				if (!m_pImpl->hdShader->loadFromMemory(std::string(kHdShader), sf::Shader::Type::Fragment))
					m_pImpl->hdShader.reset();		// falls back to Auto below
			}
		}
		if (m_pImpl->hdShader && fWinPerSrc > 1.f) {
			sf::Shader & sh = *m_pImpl->hdShader;
			sh.setUniform("source", sf::Shader::CurrentTexture);
			sh.setUniform("sourceSize", sf::Glsl::Vec2((float)(RENDER_WIDTH * f), (float)(RENDER_HEIGHT * f)));
			sh.setUniform("scale", fWinPerSrc);
			const bool bWasSmooth = tex.isSmooth();
			tex.setSmooth(false);		// the shader reads exact pixels
			sf::Sprite frame(tex);
			frame.setScale({1.f / f, 1.f / f});
			sf::RenderStates states;
			states.shader = &sh;
			window.draw(frame, states);
			tex.setSmooth(bWasSmooth);
			return;
		}
	}

	// Render Quality: enlarge by a whole number first (sharp pixels), then let the GPU smooth
	// the rest of the way to the window size. Not needed when the window is an exact multiple.
	unsigned int k = 1;
	if (!(m_bLastIntegerScale && f == 1)) {
		if (m_iRenderQuality == RQ_X2) k = 2;
		else if (m_iRenderQuality == RQ_X4) k = 4;
		else k = (unsigned int)std::max(1.f, std::min(4.f, std::floor(fWinPerSrc)));
		if (k * f > 4) k = (f >= 4) ? 1 : 4 / f;		// the 2x picture already counts for 2
	}
	if (k > 1) {
		const unsigned int uw = RENDER_WIDTH * f * k, uh = RENDER_HEIGHT * f * k;
		if (!m_pImpl->upscale || m_pImpl->upscaleFactor != k * f || m_pImpl->upscale->getSize().x != uw || m_pImpl->upscale->getSize().y != uh) {
			m_pImpl->upscale.emplace();
			if (!m_pImpl->upscale->resize({uw, uh})) {
				m_pImpl->upscale.reset();
				m_pImpl->upscaleFactor = 0;
			}
			else m_pImpl->upscaleFactor = k * f;
		}
	}
	if (k > 1 && m_pImpl->upscale) {
		sf::RenderTexture & rt = *m_pImpl->upscale;
		const bool bWasSmooth = tex.isSmooth();
		tex.setSmooth(false);
		sf::Sprite big(tex);
		big.setScale({(float)k, (float)k});
		rt.clear(sf::Color::Black);
		rt.draw(big);
		rt.display();
		tex.setSmooth(bWasSmooth);

		rt.setSmooth(true);
		sf::Sprite frame(rt.getTexture());
		frame.setScale({1.f / (k * f), 1.f / (k * f)});		// back to game-pixel view units
		window.draw(frame);
		return;
	}

	sf::Sprite frame(tex);
	frame.setScale({1.f / f, 1.f / f});
	window.draw(frame);
}

bool CRenderer::SetGameSize(int iW, int iH)
{
	if (iW < 64 || iH < 64) return false;
	if (m_pImpl == NULL || !m_pImpl->texture) {		// not initialised yet: bInit uses the new size
		g_iRenderWidth = iW;
		g_iRenderHeight = iH;
		return true;
	}
	if (iW == RENDER_WIDTH && iH == RENDER_HEIGHT) return true;
	if (!m_pImpl->texture->resize({(unsigned)iW, (unsigned)iH})) return false;
	g_iRenderWidth  = iW;
	g_iRenderHeight = iH;
	SetRect(&m_rcClipArea, 0, 0, iW, iH);
	m_pImpl->rgba.assign((size_t)iW * iH, 0xFF000000u);
	m_pImpl->texture->update(reinterpret_cast<const std::uint8_t *>(m_pImpl->rgba.data()));
	m_pImpl->composite = NULL;		// (the last frame is the old size)
	m_pImpl->view = sf::View(sf::FloatRect({0.f, 0.f}, {(float)iW, (float)iH}));
	m_pImpl->upscale.reset();
	m_pImpl->upscaleFactor = 0;
	return true;
}

sf::RenderWindow * CRenderer::GetWindow()
{
	if (m_pImpl == NULL || !m_pImpl->window) return NULL;
	return &(*m_pImpl->window);
}

void CRenderer::MapWindowToGame(int iWinX, int iWinY, short * pX, short * pY)
{
	int x = iWinX, y = iWinY;
	if (m_pImpl != NULL && m_pImpl->window) {
		const sf::Vector2f p = m_pImpl->window->mapPixelToCoords({iWinX, iWinY}, m_pImpl->view);
		x = (int)std::floor(p.x);
		y = (int)std::floor(p.y);
	}
	if (x < 0) x = 0; else if (x > RENDER_WIDTH - 1) x = RENDER_WIDTH - 1;
	if (y < 0) y = 0; else if (y > RENDER_HEIGHT - 1) y = RENDER_HEIGHT - 1;
	*pX = (short)x;
	*pY = (short)y;
}

void CRenderer::MapGameToWindow(short sX, short sY, int * pWinX, int * pWinY)
{
	*pWinX = sX;
	*pWinY = sY;
	if (m_pImpl != NULL && m_pImpl->window) {
		const sf::Vector2i p = m_pImpl->window->mapCoordsToPixel({sX + 0.5f, sY + 0.5f}, m_pImpl->view);
		*pWinX = p.x;
		*pWinY = p.y;
	}
}

CSurface * CRenderer::pCreateOffScreenSurface(WORD wSzX, WORD wSzY)
{
	return new CSurface(wSzX, wSzY);
}

long CRenderer::_CalcMaxValue(int iS, int iD, char cMode, char cMethod, double dAlpha)
{
 long Sum = 0;
 double dTmp;

	switch (cMethod) {
	case 1:
		dTmp = (double)iS;
		dTmp = dTmp * dAlpha;
		iS = (int)dTmp;
		Sum = (iS) + (iD);
		if (Sum < iD) Sum = iD;
		break;

	case 2:
		Sum = (iS + iD) / 2;
		break;
	}

	if (cMode == 'G') {
		if (Sum >= 64) Sum = 63;
	}
	else if (Sum >= 32) Sum = 31;

	return Sum;
}

long CRenderer::_CalcMinValue(int iS, int iD, char cMode)
{
 long Sum;

	Sum = iD - iS;
	if (Sum < 0) Sum = 0;

	if (cMode == 'G') {
		if (Sum >= 64) Sum = 63;
	}
	else if (Sum >= 32) Sum = 31;

	return Sum;
}

void CRenderer::ClearBackB4()
{
	// (nothing to clear: every frame drawn on the graphics card starts black)
}

void CRenderer::FillPanel(int x0, int y0, int x1, int y1, int iRadius, COLORREF fill, int iFillA,
	COLORREF border, int iBorderA, int iTailH, int iTailX)
{
	// drawn on the graphics card: the panel at 2x is a step in its list (shader: UiGpu.cpp)
	if (x1 <= x0 || y1 <= y0 || !m_bUiRec || m_pImpl == NULL || !m_pImpl->uiGpu) return;
	if (m_iFadeA < 255) { iFillA = iFillA * m_iFadeA / 255; iBorderA = iBorderA * m_iFadeA / 255; }
	if (iFillA <= 0 && iBorderA <= 0) return;
	prof::Scope sc(prof::PANELS);
	prof::Count(prof::C_PANELS);
	if (iTailX == INT_MIN) iTailX = (x0 + x1) / 2;
	if (iTailH < 0) iTailH = 0;
	const int maxR = (std::min)(x1 - x0, y1 - y0) / 2;
	if (iRadius > maxR) iRadius = maxR;
	if (iRadius < 0) iRadius = 0;
	const int bx0 = x0 - 1, by0 = y0 - 1, bx1 = x1 + 1, by1 = y1 + iTailH + 1;
	const int W2 = 2 * RENDER_WIDTH, H2 = 2 * RENDER_HEIGHT;
	m_pImpl->uiGpu->AddPanel(2 * x0, 2 * y0, 2 * x1, 2 * y1, 2.f * iRadius, fill, iFillA, border, iBorderA,
		2.f * iTailH, 2.f * iTailX, 2.f, (std::max)(2 * bx0, 0), (std::max)(2 * by0, 0), (std::min)(2 * bx1, W2), (std::min)(2 * by1, H2));
}

void CRenderer::FillEdgeGlow(int x0, int y0, int x1, int y1, int iBand, COLORREF c, int iMaxA)
{
	// colour strongest at the edges, easing out (squared) over the band; on the graphics card
	if (!m_bUiRec || m_pImpl == NULL || !m_pImpl->uiGpu || iBand <= 0 || iMaxA <= 0) return;
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > RENDER_WIDTH) x1 = RENDER_WIDTH;
	if (y1 > RENDER_HEIGHT) y1 = RENDER_HEIGHT;
	if (x1 - x0 < 2 * iBand || y1 - y0 < 2 * iBand) return;
	if (iMaxA > 255) iMaxA = 255;
	m_pImpl->uiGpu->AddEdgeGlow(2 * x0, 2 * y0, 2 * x1, 2 * y1, 2 * iBand, c, iMaxA);
}

void CRenderer::FillStar(float cx, float cy, float fRadius, COLORREF fill, int iFillA)
{
	// a five-pointed star, on the graphics card
	if (!m_bUiRec || m_pImpl == NULL || !m_pImpl->uiGpu || fRadius <= 0.f || iFillA <= 0) return;
	const int bx0 = (int)std::floor(cx - fRadius) - 1, by0 = (int)std::floor(cy - fRadius) - 1;
	const int bx1 = (int)std::ceil(cx + fRadius) + 1, by1 = (int)std::ceil(cy + fRadius) + 1;
	m_pImpl->uiGpu->AddStar(2.f * cx, 2.f * cy, 2.f * fRadius, fill, iFillA, (std::max)(2 * bx0, 0), (std::max)(2 * by0, 0),
		(std::min)(2 * bx1, 2 * RENDER_WIDTH), (std::min)(2 * by1, 2 * RENDER_HEIGHT));
}

void CRenderer::DrawShadowBox(short sX, short sY, short dX, short dY, int iType)
{
	if (dX >= RENDER_WIDTH) dX = RENDER_WIDTH - 1;
	if (dX < 0) return;
	if (sX < 0) sX = 0;
	if (sX >= RENDER_WIDTH) return;
	if (dY >= RENDER_HEIGHT) dY = RENDER_HEIGHT - 1;
	if (dY < 0) return;
	if (sY < 0) sY = 0;
	if (sY >= RENDER_HEIGHT) return;
	if (!m_bUiRec || m_pImpl == NULL || !m_pImpl->uiGpu) return;
	// on the graphics card: type 0 halves what is there, the others are solid
	const WORD w = (iType == 1) ? (WORD)0x38e7 : (iType == 2 ? (WORD)0x1863 : (WORD)0);
	m_pImpl->uiGpu->AddRect(2 * sX, 2 * sY, 2 * (dX + 1), 2 * (dY + 1), (iType == 0) ? 0u : (m_pImpl->lut[w] & 0xFFFFFFu), (iType == 0) ? 128 : 255);
}

void CRenderer::PutPixel(short sX, short sY, WORD wR, WORD wG, WORD wB)
{
	// (the mini map's dots) the pixel, doubled, on the graphics card
	if ((sX < 0) || (sY < 0) || (sX > RENDER_WIDTH - 1) || (sY > RENDER_HEIGHT - 1)) return;
	if (!m_bUiRec || m_pImpl == NULL || !m_pImpl->uiGpu) return;
	const WORD w = (WORD)( ((wR>>3)<<11) | ((wG>>2)<<5) | (wB>>3) );
	m_pImpl->uiGpu->AddRect(2 * sX, 2 * sY, 2 * sX + 2, 2 * sY + 2, m_pImpl->lut[w] & 0xFFFFFFu, 255);
}

void CRenderer::ColorTransferRGB(COLORREF fcolor, int * iR, int * iG, int * iB)
{
	// RGB 5:6:5 component values
	*iR = (int)((fcolor & 0x000000f8) >> 3);
	*iG = (int)((fcolor & 0x0000fc00) >> 10);
	*iB = (int)((fcolor & 0x00f80000) >> 19);
}

//////////////////////////////////////////////////////////////////////
// Text
//////////////////////////////////////////////////////////////////////

namespace
{
	// Converts game text (system ANSI code page) to UTF-16.
	std::wstring ToWide(const char * pStr, int iLen)
	{
		if (pStr == NULL) return std::wstring();
		if (iLen < 0) iLen = (int)strlen(pStr);
		if (iLen == 0) return std::wstring();
		int n = MultiByteToWideChar(CP_ACP, 0, pStr, iLen, NULL, 0);
		if (n <= 0) return std::wstring();
		std::wstring w((size_t)n, L'\0');
		MultiByteToWideChar(CP_ACP, 0, pStr, iLen, &w[0], n);
		return w;
	}
}

int CRenderer::_DrawGlyphRun(int x, int y, const wchar_t * pText, int iLen, COLORREF rgb, bool bDraw, unsigned int uPx)
{
	if (m_pImpl == NULL || !m_pImpl->fontOk) return 0;
	const LONGLONG tProf = prof::Now();
	struct ProfEnd { LONGLONG t; bool b; ~ProfEnd() { if (b) { prof::Add(prof::TEXT, t); prof::Count(prof::C_TEXT); } } } profEnd = { tProf, bDraw };

	const int cr = GetRValue(rgb), cg = GetGValue(rgb), cb = GetBValue(rgb);
	const WORD wSrc = Rgb565(cr, cg, cb);
	const unsigned int uSize = (uPx == 0) ? kFontCharSize : uPx;
	const bool b2 = (m_iFace == 1 && m_pImpl->font2Ok);	// the display font (titles)
	const int iAscent = b2 ? m_pImpl->ascent2 : m_pImpl->ascent;
	const int iBaseline = y + ((uSize == kFontCharSize) ? iAscent
		: (int)std::lround(iAscent * (float)uSize / (float)kFontCharSize));
	auto glyphAt = [&](char32_t c, unsigned int s) -> const CachedGlyph & { return b2 ? m_pImpl->GlyphFace2(c, s) : m_pImpl->GlyphSized(c, s); };

	// the glyphs are drawn from the twice-as-large font, so text is spaced (and measured) by its
	// advances, halved: the normal size's whole-pixel advances, doubled, space some fonts unevenly
	float fPen = (float)x;
	char32_t prev = 0;
	for (int i = 0; i < iLen; i++) {
		char32_t cp = (char32_t)pText[i];
		if (cp == L'\r' || cp == L'\n') continue;
		if (cp == L'\t') cp = L' ';
		if (cp < 32) continue;

		if (prev != 0) fPen += 0.5f * (b2 ? m_pImpl->KerningFace2(prev, cp, uSize * 2) : m_pImpl->KerningSized(prev, cp, uSize * 2));
		prev = cp;

		const CachedGlyph & g = glyphAt(cp, uSize);
		if (bDraw && g.w > 0 && m_bUiRec && m_pImpl->uiGpu && m_iFadeA > 0) {
			// drawn on the graphics card: the glyph of the twice-as-large font is a step
			// (the top byte: its alpha when faded, m_iFadeA; 0 = solid)
			const CachedGlyph & g2 = glyphAt(cp, uSize * 2);
			const std::uint32_t uA = (m_iFadeA < 255) ? ((std::uint32_t)m_iFadeA << 24) : 0u;
			if (g2.w > 0 && g2.h > 0)	// (the size is the glyph's key on the graphics card: the display font's apart)
				m_pImpl->uiGpu->AddGlyph(uSize * 2 + (b2 ? 0x10000u : 0u), cp, g2.alpha.data(), g2.w, g2.h,
					(int)std::lround(2.f * fPen) + g2.left, 2 * iBaseline + g2.top, (m_pImpl->lut[wSrc] & 0xFFFFFFu) | uA);
		}
		fPen += 0.5f * (float)glyphAt(cp, uSize * 2).advance;
	}
	return (int)std::lround(fPen) - x;
}

// The classic windows wrote dark ink for parchment; on VALORIA's stone it would not show. While
// m_bStoneInk is on (a classic window is being drawn, CGame::DrawDialogBoxes), dark text is lifted
// toward VALORIA's parchment lettering, keeping its hue; black (the drop shadows) stays black.
COLORREF CRenderer::StoneInk(COLORREF c) const
{
	if (!m_bStoneInk || c == RGB(0, 0, 0)) return c;
	const int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
	if ((std::max)(r, (std::max)(g, b)) >= 120) return c;
	return RGB(r + (230 - r) * 3 / 4, g + (217 - g) * 3 / 4, b + (189 - b) * 3 / 4);
}

void CRenderer::TextOut(int x, int y, char const * cStr, COLORREF rgb)
{
	rgb = StoneInk(rgb);
	const std::wstring w = ToWide(cStr, -1);
	_DrawGlyphRun(x, y, w.c_str(), (int)w.size(), rgb, true);
}

void CRenderer::TextOutSized(int x, int y, char const * cStr, COLORREF rgb, int iPx)
{
	const std::wstring w = ToWide(cStr, -1);
	_DrawGlyphRun(x, y, w.c_str(), (int)w.size(), rgb, true, (unsigned int)(iPx > 0 ? iPx : 0));
}

int CRenderer::TextWidthSized(char const * cStr, int iPx)
{
	const std::wstring w = ToWide(cStr, -1);
	return _DrawGlyphRun(0, 0, w.c_str(), (int)w.size(), 0, false, (unsigned int)(iPx > 0 ? iPx : 0));
}

SIZE CRenderer::GetTextExtent(char const * pString, int iLength)
{
	SIZE sz;
	const std::wstring w = ToWide(pString, iLength);
	sz.cx = _DrawGlyphRun(0, 0, w.c_str(), (int)w.size(), 0, false);
	sz.cy = (m_pImpl != NULL) ? m_pImpl->lineHeight : 16;
	return sz;
}

void CRenderer::_DrawTextLines(const RECT & rc, const wchar_t * pText, int iLen, COLORREF rgb, DWORD flags, bool bWordWrap, RECT * pCalcRect)
{
	if (m_pImpl == NULL) return;

	const bool bSingle = (flags & DT_SINGLELINE) != 0;
	const int iMaxW = rc.right - rc.left;

	// Pen position before each character (advance + kerning), so any segment's width is a subtraction.
	std::vector<int> pos((size_t)iLen + 1, 0);
	if (bWordWrap && !bSingle && iMaxW > 0) {
		float fPen = 0.f;
		char32_t prev = 0;
		for (int i = 0; i < iLen; i++) {
			pos[i] = (int)std::lround(fPen);
			char32_t cp = (char32_t)pText[i];
			if (cp == L'\t') cp = L' ';
			if (cp < 32) { prev = 0; continue; }
			if (prev != 0) fPen += m_pImpl->Kerning(prev, cp);
			prev = cp;
			fPen += (float)m_pImpl->Glyph(cp).advance;
		}
		pos[iLen] = (int)std::lround(fPen);
	}

	// Split into lines: hard breaks on '\n' (unless DT_SINGLELINE), soft breaks at spaces when word-wrapping.
	std::vector<std::pair<int, int>> lines;	// (start, length)
	int iPara = 0;
	while (iPara <= iLen) {
		int iEnd = iPara;
		if (!bSingle) while (iEnd < iLen && pText[iEnd] != L'\n') iEnd++;
		else iEnd = iLen;

		int iParaEnd = iEnd;
		if (iParaEnd > iPara && pText[iParaEnd - 1] == L'\r') iParaEnd--;

		if (!bWordWrap || bSingle || iMaxW <= 0) {
			lines.emplace_back(iPara, iParaEnd - iPara);
		}
		else {
			int iStart = iPara;
			while (true) {
				// Greedy: take as many words as fit; a single word longer than the line is kept whole (GDI behaviour).
				int iBest = -1, i = iStart;
				while (i <= iParaEnd) {
					if (i == iParaEnd || pText[i] == L' ') {
						const int w = pos[i] - pos[iStart];
						if (w <= iMaxW || iBest < 0) iBest = i;
						if (w > iMaxW) break;
					}
					i++;
				}
				if (iBest < 0) iBest = iParaEnd;
				lines.emplace_back(iStart, iBest - iStart);
				iStart = iBest;
				while (iStart < iParaEnd && pText[iStart] == L' ') iStart++;
				if (iStart >= iParaEnd) break;
			}
		}
		if (iEnd >= iLen) break;
		iPara = iEnd + 1;
	}

	const int iLineH = m_pImpl->lineHeight;
	int y = rc.top;
	if (bSingle) {
		if (flags & DT_VCENTER) y = rc.top + ((rc.bottom - rc.top) - iLineH) / 2;
		else if (flags & DT_BOTTOM) y = rc.bottom - iLineH;
	}

	int iMaxLineW = 0;
	for (size_t i = 0; i < lines.size(); i++) {
		const wchar_t * pLine = pText + lines[i].first;
		const int iLineLen = lines[i].second;
		const int w = _DrawGlyphRun(0, 0, pLine, iLineLen, 0, false);
		if (w > iMaxLineW) iMaxLineW = w;
		if (pCalcRect == NULL) {
			int x = rc.left;
			if (flags & DT_CENTER) x = rc.left + (iMaxW - w) / 2;
			else if (flags & DT_RIGHT) x = rc.right - w;
			_DrawGlyphRun(x, y, pLine, iLineLen, rgb, true);
		}
		y += iLineH;
	}

	if (pCalcRect != NULL) {
		pCalcRect->right  = pCalcRect->left + iMaxLineW;
		pCalcRect->bottom = pCalcRect->top + (int)lines.size() * iLineH;
	}
}

void CRenderer::DrawText(LPRECT pRect, char const * const pString, COLORREF rgb, bool wordWrap)
{
	DrawTextEx(pRect, pString, rgb, DT_CENTER, wordWrap);
}

void CRenderer::DrawTextEx(LPRECT pRect, char const * const pString, COLORREF rgb, DWORD flags, bool wordWrap)
{
	rgb = StoneInk(rgb);
	if (pRect == NULL || pString == NULL) return;
	const std::wstring w = ToWide(pString, -1);
	const bool bWrap = wordWrap || (flags & DT_WORDBREAK) != 0;
	_DrawTextLines(*pRect, w.c_str(), (int)w.size(), rgb, flags, bWrap, (flags & DT_CALCRECT) ? pRect : NULL);
}


//////////////////////////////////////////////////////////////////////
// VALORIA's stone interface (CRenderer::StoneBox and friends, see Renderer.h)

namespace {
struct StoneTex {
	const char * file;
	int mx, my;		// carved edge: texture pixels that do not stretch (VALORIA's UIStyle.TEX_MARGIN)
	float k;		// texture pixels -> 2x picture pixels
	std::vector<std::uint32_t> px;
	int w = 0, h = 0;
	bool tried = false;
};
StoneTex g_stone[CRenderer::STONE_COUNT] = {
	{ "CONTENTS\\ui\\panel.png", 24, 24, 0.75f },
	{ "CONTENTS\\ui\\button.png", 16, 9, 0.7f },
	{ "CONTENTS\\ui\\button_hover.png", 16, 9, 0.7f },
	{ "CONTENTS\\ui\\button_pressed.png", 16, 9, 0.7f },
	{ "CONTENTS\\ui\\slot.png", 10, 10, 0.8f },
	{ "CONTENTS\\ui\\slot_active.png", 10, 10, 0.8f },
	{ "CONTENTS\\ui\\bar_frame.png", 11, 6, 1.0f },
};
StoneTex * Stone(int i)
{
	if (i < 0 || i >= CRenderer::STONE_COUNT) return NULL;
	StoneTex & t = g_stone[i];
	if (!t.tried) {
		t.tried = true;
		sf::Image img;
		if (img.loadFromFile(t.file)) {
			t.w = (int)img.getSize().x; t.h = (int)img.getSize().y;
			t.px.resize((size_t)t.w * t.h);
			std::memcpy(t.px.data(), img.getPixelsPtr(), t.px.size() * 4);
		}
	}
	return t.w > 0 ? &t : NULL;
}
}

void CRenderer::WorldImage(int iPic, int x0, int y0, int x1, int y1, COLORREF tint, int iAlpha)
{
	static StoneTex pics[2] = { { "CONTENTS\\world\\shrine.png", 0, 0, 1.f }, { "CONTENTS\\world\\chest.png", 0, 0, 1.f } };
	CUiGpu * pG = UiGpuRecorder();
	if (pG == NULL || iPic < 0 || iPic > 1 || iAlpha <= 0) return;
	StoneTex * t = &pics[iPic];
	if (!t->tried) {
		t->tried = true;
		sf::Image img;
		if (img.loadFromFile(t->file)) {
			t->w = (int)img.getSize().x; t->h = (int)img.getSize().y;
			t->px.resize((size_t)t->w * t->h);
			std::memcpy(t->px.data(), img.getPixelsPtr(), t->px.size() * 4);
		}
	}
	if (t->w <= 0) return;
	const std::uint32_t tr = GetRValue(tint), tg = GetGValue(tint), tb = GetBValue(tint), ta = (std::uint32_t)(iAlpha > 255 ? 255 : iAlpha);
	auto Fill = [t, tr, tg, tb, ta](std::uint32_t * out) {
		const size_t n = (size_t)t->w * t->h;
		for (size_t i = 0; i < n; i++) {
			const std::uint32_t p = t->px[i];
			out[i] = ((p & 0xFF) * tr / 255) | ((((p >> 8) & 0xFF) * tg / 255) << 8) | ((((p >> 16) & 0xFF) * tb / 255) << 16) | (((p >> 24) * ta / 255) << 24);
		}
	};
	const std::uint64_t key = 0x574F524C00000000ull ^ ((std::uint64_t)(iPic + 1) << 56) ^ ((std::uint64_t)ta << 24) ^ (std::uint64_t)(tint & 0xFFFFFF);
	pG->AddImageCached(key, 1u, t->w, t->h, Fill, true, 2.f * x0, 2.f * y0, 2.f * (x1 - x0), 2.f * (y1 - y0), 0, 0, 2 * RENDER_WIDTH, 2 * RENDER_HEIGHT, UIBL_ALPHA);
}

void CRenderer::StoneBox(int iStone, int x0, int y0, int x1, int y1)
{
	if (x1 <= x0 || y1 <= y0) return;
	CUiGpu * pG = UiGpuRecorder();
	StoneTex * t = Stone(iStone);
	if (pG == NULL || t == NULL) {	// no pictures: the old flat look
		if (iStone == STONE_PANEL) FillPanel(x0, y0, x1, y1, 10, RGB(14, 16, 20), 255, RGB(150, 128, 82), 150);
		else FillPanel(x0, y0, x1, y1, 6, RGB(20, 20, 24), 230, RGB(150, 128, 82), iStone == STONE_SLOT_ACTIVE || iStone == STONE_BUTTON_HOVER ? 220 : 110);
		return;
	}
	const float X0 = 2.f * x0, Y0 = 2.f * y0, X1 = 2.f * x1, Y1 = 2.f * y1;
	float ex = t->mx * t->k, ey = t->my * t->k;					// edge size in 2x pixels
	if (2 * ex > X1 - X0) ex = (X1 - X0) / 2;
	if (2 * ey > Y1 - Y0) ey = (Y1 - Y0) / 2;
	const int sxs[4] = { 0, t->mx, t->w - t->mx, t->w }, sys[4] = { 0, t->my, t->h - t->my, t->h };
	const float dxs[4] = { X0, X0 + ex, X1 - ex, X1 }, dys[4] = { Y0, Y0 + ey, Y1 - ey, Y1 };
	const int W2 = 2 * RENDER_WIDTH, H2 = 2 * RENDER_HEIGHT;
	const int fade = m_iFadeA;
	for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) {
		const int sw = sxs[c + 1] - sxs[c], sh = sys[r + 1] - sys[r];
		const float dw = dxs[c + 1] - dxs[c], dh = dys[r + 1] - dys[r];
		if (sw <= 0 || sh <= 0 || dw <= 0.f || dh <= 0.f) continue;
		const int sx = sxs[c], sy = sys[r];
		auto Fill = [t, sx, sy, sw, sh, fade](std::uint32_t * out) {
			for (int y = 0; y < sh; y++) {
				const std::uint32_t * src = t->px.data() + (size_t)(sy + y) * t->w + sx;
				std::uint32_t * dst = out + (size_t)y * sw;
				if (fade >= 255) std::memcpy(dst, src, (size_t)sw * 4);
				else for (int x = 0; x < sw; x++) {
					const std::uint32_t p = src[x];
					dst[x] = (p & 0x00FFFFFFu) | ((((p >> 24) * (std::uint32_t)fade) / 255u) << 24);
				}
			}
		};
		if (fade >= 255) {
			const std::uint64_t key = 0x53544F4E45000000ull | ((std::uint64_t)iStone << 8) | (std::uint64_t)(r * 3 + c);
			pG->AddImageCached(key, 1u, sw, sh, Fill, true, dxs[c], dys[r], dw, dh, 0, 0, W2, H2, UIBL_ALPHA);
		}
		else {
			if (fade <= 0) continue;
			std::vector<std::uint32_t> buf((size_t)sw * sh);
			Fill(buf.data());
			pG->AddImage(buf.data(), sw, sh, true, dxs[c], dys[r], dw, dh, 0, 0, W2, H2, UIBL_ALPHA);
		}
	}
}

void CRenderer::TitleCentered(int x0, int y0, int x1, int y1, const char * cStr, COLORREF rgb, int iPx)
{
	if (cStr == NULL || cStr[0] == 0) return;
	const int w = TitleWidth(cStr, iPx);
	TitleOut((x0 + x1 - w) / 2, (y0 + y1) / 2 - (iPx * 11) / 16, cStr, rgb, iPx);
}

void CRenderer::StoneButton(int x0, int y0, int x1, int y1, int iState, const char * cLabel)
{
	StoneBox(iState == 2 ? STONE_BUTTON_PRESSED : iState == 1 ? STONE_BUTTON_HOVER : STONE_BUTTON, x0, y0, x1, y1);
	if (cLabel == NULL) return;
	// VALORIA's button lettering: warm parchment, brighter under the mouse, gold when pressed
	const COLORREF c = iState == 2 ? RGB(242, 199, 102) : iState == 1 ? RGB(255, 242, 199) : RGB(245, 219, 158);
	const int iPx = (std::min)(13, (std::max)(9, (y1 - y0) * 11 / 20));
	TitleCentered(x0, y0 + (iState == 2 ? 1 : 0), x1, y1 + (iState == 2 ? 1 : 0), cLabel, c, iPx);
}

void CRenderer::StoneBarFrame(int x0, int y0, int x1, int y1)
{
	StoneBox(STONE_BAR, x0 - 4, y0 - 3, x1 + 4, y1 + 3);	// VALORIA puts the housing 9 x 5 px outside the bar
}

void CRenderer::Slate(int x0, int y0, int x1, int y1, int iFillA)
{
	FillPanel(x0, y0, x1, y1, 1, RGB(8, 8, 9), iFillA, RGB(107, 97, 84), 230);
}
