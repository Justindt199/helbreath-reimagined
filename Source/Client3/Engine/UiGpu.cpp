// UiGpu.cpp: the interface drawn on the graphics card (see UiGpu.h).
//////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "UiGpu.h"
#include "Sprite.h"

namespace {

// ---------------------------------------------------------------------------- shaders
// Positions arrive as texture coordinates in 2x picture pixels (pixel centres at .5), for the
// shapes, which are drawn without a texture (SFML then passes the coordinates through as they are).

// CRenderer::FillPanel, worked out per pixel exactly as there; premultiplied output
const char * const kPanelShader = R"(
uniform vec4 box;	// centre x, y, half width, half height
uniform vec4 shape;	// corner radius, border width, pointer height, pointer x
uniform float fy1;	// bottom edge
uniform vec4 fillC;	// colour 0..1, alpha
uniform vec4 bordC;
void main()
{
	vec2 p = gl_TexCoord[0].xy;
	float r = shape.x, bw = shape.y, tH = shape.z, tX = shape.w;
	float qx = abs(p.x - box.x) - (box.z - r);
	float qy = abs(p.y - box.y) - (box.w - r);
	float d = length(vec2(max(qx, 0.0), max(qy, 0.0))) + min(max(qx, qy), 0.0) - r;
	float cBox = clamp(0.5 - d, 0.0, 1.0);
	float cTail = 0.0;
	if (tH > 0.0 && p.y > fy1 - bw - 1.0 && p.y < fy1 + tH + 1.0) {
		float t = max((p.y - fy1) / tH, 0.0);
		float hw = tH * (1.0 - t);
		cTail = clamp(hw - abs(p.x - tX) + 0.5, 0.0, 1.0) * clamp(fy1 + tH - p.y + 0.5, 0.0, 1.0);
	}
	float cov = max(cBox, cTail);
	if (cov <= 0.002) discard;
	float cB = 0.0;
	if (bordC.a > 0.0) {
		cB = cBox * clamp(d + bw + 0.5, 0.0, 1.0);
		if (tH > 0.0 && p.y > fy1 - bw - 1.0) cB *= 1.0 - clamp(tH - bw - abs(p.x - tX), 0.0, 1.0);
	}
	float a = fillC.a * cov;
	float b = bordC.a * cB;
	gl_FragColor = vec4(fillC.rgb * a * (1.0 - b) + bordC.rgb * b, 1.0 - (1.0 - a) * (1.0 - b));
}
)";

// CRenderer::FillStar: 4 x 4 samples per pixel against the ten corners; premultiplied output
const char * const kStarShader = R"(
uniform vec2 pts[10];
uniform vec4 col;
bool inside(vec2 q)
{
	bool inn = false;
	vec2 b = pts[9];
	for (int i = 0; i < 10; i++) {
		vec2 a = pts[i];
		if (((a.y > q.y) != (b.y > q.y)) && (q.x < (b.x - a.x) * (q.y - a.y) / (b.y - a.y) + a.x)) inn = !inn;
		b = a;
	}
	return inn;
}
void main()
{
	vec2 p0 = gl_TexCoord[0].xy - vec2(0.5);
	float n = 0.0;
	for (int sy = 0; sy < 4; sy++)
		for (int sx = 0; sx < 4; sx++)
			if (inside(p0 + vec2((float(sx) + 0.5) / 4.0, (float(sy) + 0.5) / 4.0))) n += 1.0;
	if (n == 0.0) discard;
	float a = col.a * n / 16.0;
	gl_FragColor = vec4(col.rgb * a, a);
}
)";

// the sprite modes (EUiSpriteMode); the blending itself is set per mode. The vertex colour's
// red is 0 for a sprite drawn with the night shading (-3, -3, +2 in 5 / 6 / 5-bit steps).
const char * const kSpriteShader = R"(
uniform sampler2D tex;
uniform int mode;
uniform float useKey;
uniform float alpha;
uniform vec3 add;
void main()
{
	vec4 t = texture2D(tex, gl_TexCoord[0].xy);
	float k = (useKey > 0.5) ? t.a : 1.0;
	vec3 c = t.rgb;
	vec3 m = vec3(31.0, 63.0, 31.0);
	if (gl_Color.r < 0.5) c = clamp(floor(c * m + 0.5) + vec3(-3.0, -3.0, 2.0), vec3(0.0), m) / m;
	if (mode == 0) gl_FragColor = vec4(c, k);
	else if (mode == 1) gl_FragColor = vec4(c, k * alpha);
	else if (mode == 2) gl_FragColor = vec4(c, k * 0.5);
	else if (mode == 3) gl_FragColor = vec4(0.0, 0.0, 0.0, k * 0.75);
	else if (mode == 4) gl_FragColor = vec4(clamp(floor(c * m + 0.5) + add, vec3(1.0), m) / m, k);
	else gl_FragColor = vec4(clamp(floor(c * m + 0.5) + add, vec3(0.0), m) / m, k);
}
)";

// CRenderer::FillEdgeGlow: alpha by distance (in pixels) from the nearest edge, eased out; premultiplied
const char * const kGlowShader = R"(
uniform vec4 rect;	// x0, y0, x1, y1
uniform float band;
uniform vec4 col;	// colour 0..1, alpha at the edge
void main()
{
	vec2 p = gl_TexCoord[0].xy;
	float d = min(min(p.x - rect.x, rect.z - p.x), min(p.y - rect.y, rect.w - p.y)) - 0.5;
	if (d < 0.0 || d >= band) discard;
	float t = 1.0 - floor(d) / band;
	float a = t * t * col.a;
	gl_FragColor = vec4(col.rgb * a, a);
}
)";

// light added to what is there: mode 0 soft spots (each its own centre and size, taken from the
// vertices: texture coordinates are the offset from the centre in widths, the colour's alpha a
// quarter of the strength), 1 a ring, 2 a pillar (both from the uniforms, in game pixels)
const char * const kLightShader = R"(
uniform int mode;
uniform vec4 p0;
uniform vec4 p1;
uniform vec3 col;
void main()
{
	vec2 t = gl_TexCoord[0].xy;
	vec3 c = col;
	float l;
	if (mode == 0) {
		l = exp(-dot(t, t)) * gl_Color.a * 4.0;
		c = gl_Color.rgb;
	}
	else if (mode == 1) {
		vec2 d = t - p0.xy;
		float dd = length(d / p0.zw);
		float len = length(d);
		float k = (dd > 0.0001) ? len / dd : p0.w;
		l = max(0.0, 1.0 - abs((dd - 1.0) * k) / p1.x);
		if (dd < 1.0) l += (1.0 - dd) * 0.25 * (1.0 - p1.z);
		l *= p1.y;
	}
	else {
		float dx = t.x - p0.x;
		float v = pow(max(0.0, t.y / max(1.0, p0.y)), 0.6) * clamp((p0.z + 14.0 - t.y) / 14.0, 0.0, 1.0) * p0.w;
		float core = exp(-(dx / p1.x) * (dx / p1.x));
		float halo = exp(-(dx / p1.y) * (dx / p1.y)) * 0.45;
		l = (core * 1.3 + halo) * v;
	}
	gl_FragColor = vec4(c * l, 1.0);
}
)";

// night lighting: a soft spot whose strength falls from 1 at the centre to v1 at s1 (of the
// radius) and to 0 at the edge; the colour's alpha is half the strength (texture coordinates:
// the offset from the centre in radii)
const char * const kSpotShader = R"(
uniform float s1;
uniform float v1;
void main()
{
	float d = length(gl_TexCoord[0].xy);
	float f = (d < s1) ? mix(1.0, v1, d / s1) : mix(v1, 0.0, clamp((d - s1) / (1.0 - s1), 0.0, 1.0));
	gl_FragColor = vec4(gl_Color.rgb * gl_Color.a * 2.0 * f, 1.0);
}
)";

// colour channels only; the picture's alpha stays as it is
const sf::BlendMode kBlendAlpha(sf::BlendMode::Factor::SrcAlpha, sf::BlendMode::Factor::OneMinusSrcAlpha, sf::BlendMode::Equation::Add,
	sf::BlendMode::Factor::Zero, sf::BlendMode::Factor::One, sf::BlendMode::Equation::Add);
const sf::BlendMode kBlendPremul(sf::BlendMode::Factor::One, sf::BlendMode::Factor::OneMinusSrcAlpha, sf::BlendMode::Equation::Add,
	sf::BlendMode::Factor::Zero, sf::BlendMode::Factor::One, sf::BlendMode::Equation::Add);
const sf::BlendMode kBlendAdd(sf::BlendMode::Factor::SrcAlpha, sf::BlendMode::Factor::One, sf::BlendMode::Equation::Add,
	sf::BlendMode::Factor::Zero, sf::BlendMode::Factor::One, sf::BlendMode::Equation::Add);
const sf::BlendMode kBlendSub(sf::BlendMode::Factor::SrcAlpha, sf::BlendMode::Factor::One, sf::BlendMode::Equation::ReverseSubtract,
	sf::BlendMode::Factor::Zero, sf::BlendMode::Factor::One, sf::BlendMode::Equation::Add);

const unsigned int kAtlasSize = 2048;
const unsigned int kSheetIdleMs = 30000;	// sheet textures unused this long are freed

unsigned int g_uSheetGen = 1;

inline sf::Color ColorOf(std::uint32_t rgb, int a)	// rgb as a COLORREF (0x00BBGGRR)
{
	return sf::Color((std::uint8_t)(rgb & 0xFF), (std::uint8_t)((rgb >> 8) & 0xFF), (std::uint8_t)((rgb >> 16) & 0xFF),
		(std::uint8_t)(a < 0 ? 0 : (a > 255 ? 255 : a)));
}


} // namespace

// ---------------------------------------------------------------------------- data

struct CUiGpuImpl
{
	const std::uint32_t * lut = nullptr;
	// the zoomed scene (SetZoom): round (zoomX, zoomY), zoomZ times, for the commands before zoomEnd
	float zoomX = 0.f, zoomY = 0.f, zoomZ = 1.f;
	size_t zoomEnd = (size_t)-1;
	bool tried = false, ok = false;
	sf::Shader panelSh, starSh, spriteSh, glowSh, lightSh, spotSh;

	enum { T_PANEL, T_STAR, T_TRIS, T_GLYPHS, T_SPRITES, T_ADDTRIS, T_PRETRIS, T_IMAGE, T_GLOW, T_LIGHTSPOTS, T_LIGHT, T_LIGHTPASS };
	// before the light pass, these are overlays (names, bars, rings of light): drawn after it, unlit
	static bool Unlit(int t) { return t == T_PANEL || t == T_STAR || t == T_TRIS || t == T_GLYPHS || t == T_ADDTRIS
		|| t == T_PRETRIS || t == T_GLOW || t == T_LIGHTSPOTS || t == T_LIGHT; }

	// night lighting: this frame's lights and glows (2x picture pixels), and the light map
	std::vector<sf::Vertex> lights, glows;
	float amb[3] = { 1.f, 1.f, 1.f };
	std::optional<sf::RenderTexture> lightRT;
	struct Cmd {
		int type = T_TRIS;
		size_t first = 0, count = 0;		// vertices in 'verts'
		// panel / star
		float box[4] = {}, shape[4] = {}, fy1 = 0.f, fillC[4] = {}, bordC[4] = {};
		sf::Glsl::Vec2 pts[10];
		// sprites
		const sf::Texture * tex = nullptr;
		int mode = 0;
		int blend = UIBL_ALPHA;			// images
		bool key = true;
		float alpha = 1.f;
		int add[3] = {};
	};
	std::vector<Cmd> cmds;
	std::vector<sf::Vertex> verts;

	// sprite sheets on the graphics card
	// A sheet can be bigger than the graphics card takes in one texture (the map tile sheets
	// are, especially their HD copies), so it is kept as a grid of pages of at most 'page' pixels.
	struct Sheet {
		unsigned int gen = 0, genMask = 0;
		unsigned int lastUse = 0;
		int W = 0, H = 0, page = 1, cols = 0, rows = 0;
		std::vector<std::unique_ptr<sf::Texture>> pages;
		const sf::Texture * Page(int c, int r) const { return pages[(size_t)r * cols + c].get(); }
	};
	std::map<std::pair<const CSprite *, bool>, Sheet> sheets;
	unsigned int frameMs = 0;

	// pictures made on the CPU: kept by key, or one-off (a pool reused frame to frame)
	struct Img { sf::Texture tex; std::uint32_t version = 0; unsigned int lastUse = 0; bool used = false; };
	std::unordered_map<std::uint64_t, Img> images;
	std::vector<std::unique_ptr<sf::Texture>> pool;
	size_t poolNext = 0;
	std::vector<std::uint32_t> scratch;

	// the last values given to the sprite shader (setting a uniform costs a program switch)
	int lastMode = -1; float lastKey = -1.f, lastAlpha = -1.f; int lastAdd[3] = { 9999, 9999, 9999 };

	// glyphs: one texture, filled row by row
	std::optional<sf::Texture> atlas;
	struct GlyphSlot { int x = 0, y = 0, w = 0, h = 0; };
	std::unordered_map<std::uint64_t, GlyphSlot> glyphs;
	unsigned int penX = 0, penY = 0, rowH = 0;
	bool atlasFull = false;

	bool Init()
	{
		if (tried) return ok;
		tried = true;
		if (!sf::Shader::isAvailable()) return ok = false;
		ok = panelSh.loadFromMemory(std::string(kPanelShader), sf::Shader::Type::Fragment)
			&& starSh.loadFromMemory(std::string(kStarShader), sf::Shader::Type::Fragment)
			&& spriteSh.loadFromMemory(std::string(kSpriteShader), sf::Shader::Type::Fragment)
			&& glowSh.loadFromMemory(std::string(kGlowShader), sf::Shader::Type::Fragment)
			&& lightSh.loadFromMemory(std::string(kLightShader), sf::Shader::Type::Fragment)
			&& spotSh.loadFromMemory(std::string(kSpotShader), sf::Shader::Type::Fragment);
		return ok;
	}

	void Quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, sf::Color c)
	{
		const sf::Vertex a{{x0, y0}, c, {u0, v0}}, b{{x1, y0}, c, {u1, v0}};
		const sf::Vertex d{{x0, y1}, c, {u0, v1}}, e{{x1, y1}, c, {u1, v1}};
		verts.push_back(a); verts.push_back(b); verts.push_back(d);
		verts.push_back(b); verts.push_back(e); verts.push_back(d);
	}

	// the last step, if it is of this type and can take more vertices; otherwise a new one
	Cmd & Batch(int type)
	{
		if (cmds.empty() || cmds.back().type != type) {
			cmds.emplace_back();
			cmds.back().type = type;
			cmds.back().first = verts.size();
		}
		return cmds.back();
	}

	const Sheet * SheetTexture(CSprite * pSheet, CSprite * pMask)
	{
		if (pSheet == nullptr) return nullptr;
		if (pMask != nullptr && (pMask->m_lpSurface == nullptr || pMask->m_lpSurface->GetPixels() == nullptr)) pMask = nullptr;
		Sheet & s = sheets[std::make_pair((const CSprite *)pSheet, pMask != nullptr)];
		s.lastUse = frameMs;
		// (the mask - the normal sheet's see-through pixels - never changes: the night shading
		// recolours that sheet but keeps its see-through pixels, so it doesn't count)
		const unsigned int genMask = pMask ? 1u : 0u;
		if (s.gen == pSheet->m_uSheetGen && s.genMask == genMask && !s.pages.empty()) return &s;

		// An HD copy's pixels are only in RAM to be sent here (CSprite::m_bHdCopy): read from
		// disk if they were freed, and freed again once sent.
		if (pSheet->m_bHdCopy && pSheet->m_lpSurface == nullptr) pSheet->_iOpenSprite();
		struct FreeAfter {
			CSprite * p;
			~FreeAfter() { if (p->m_bHdCopy) p->_CloseOwn(); }
		} freeAfter{ pSheet };
		if (pSheet->m_lpSurface == nullptr || pSheet->m_lpSurface->GetPixels() == nullptr) return nullptr;

		CSurface * pS = pSheet->m_lpSurface;
		const int W = pS->GetWidth(), H = pS->GetHeight(), iPitch = pS->GetPitch();
		if (W <= 0 || H <= 0) return nullptr;
		std::vector<std::uint32_t> px((size_t)W * H);
		const WORD * pSrc = pS->GetPixels();
		const WORD wKey = pSheet->m_wColorKey;
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				const WORD v = pSrc[(size_t)y * iPitch + x];
				std::uint32_t c = lut[v] & 0x00FFFFFFu;			// bytes R, G, B (little-endian)
				if (v != wKey) c |= 0xFF000000u;
				px[(size_t)y * W + x] = c;
			}
		// an HD sheet: see-through wherever the normal sheet is (each of its pixels covers 2 x 2),
		// frame by frame, as the 2x software drawing does (the old software drawing did the same)
		if (pMask != nullptr) {
			CSurface * pM = pMask->m_lpSurface;
			const WORD * pMs = pM->GetPixels();
			const int MW = pM->GetWidth(), MH = pM->GetHeight(), iMPitch = pM->GetPitch();
			const WORD wKey1 = pMask->m_wColorKey;
			const int n = (std::min)(pSheet->m_iTotalFrame, pMask->m_iTotalFrame);
			for (int f = 0; f < n; f++) {
				const stBrush & b = pSheet->m_stBrush[f], & b1 = pMask->m_stBrush[f];
				if (b.szx != 2 * b1.szx || b.szy != 2 * b1.szy) continue;
				for (int iy = 0; iy < b.szy; iy++) {
					const int y = b.sy + iy, my = b1.sy + iy / 2;
					if (y < 0 || y >= H) continue;
					for (int ix = 0; ix < b.szx; ix++) {
						const int x = b.sx + ix, mx = b1.sx + ix / 2;
						if (x < 0 || x >= W) continue;
						if (mx < 0 || my < 0 || mx >= MW || my >= MH || pMs[(size_t)my * iMPitch + mx] == wKey1)
							px[(size_t)y * W + x] &= 0x00FFFFFFu;
					}
				}
			}
		}
		// into pages (one, for most sheets)
		const int P = (int)(std::min)(sf::Texture::getMaximumSize(), 4096u);
		const int cols = (W + P - 1) / P, rows = (H + P - 1) / P;
		if (s.W != W || s.H != H || s.page != P || (int)s.pages.size() != cols * rows) {
			s.pages.clear();
			for (int i = 0; i < cols * rows; i++) s.pages.emplace_back(new sf::Texture());
		}
		s.W = W; s.H = H; s.page = P; s.cols = cols; s.rows = rows;
		std::vector<std::uint32_t> part;
		for (int r = 0; r < rows; r++)
			for (int c = 0; c < cols; c++) {
				const int x0 = c * P, y0 = r * P, pw = (std::min)(P, W - x0), ph = (std::min)(P, H - y0);
				sf::Texture & t = *s.pages[(size_t)r * cols + c];
				if (t.getSize().x != (unsigned)pw || t.getSize().y != (unsigned)ph)
					if (!t.resize({(unsigned)pw, (unsigned)ph})) { s.pages.clear(); return nullptr; }
				const std::uint32_t * pData = px.data() + (size_t)y0 * W + x0;
				if (cols > 1) {
					part.resize((size_t)pw * ph);
					for (int y = 0; y < ph; y++) std::copy(pData + (size_t)y * W, pData + (size_t)y * W + pw, part.begin() + (size_t)y * pw);
					pData = part.data();
				}
				t.update(reinterpret_cast<const std::uint8_t *>(pData));
				t.setSmooth(false);		// the software drawing doubles and stretches with whole pixels
			}
		s.gen = pSheet->m_uSheetGen;
		s.genMask = genMask;
		return &s;
	}

	bool GlyphSlotFor(unsigned int uSize, char32_t cp, const std::uint8_t * pAlpha, int w, int h, GlyphSlot & out)
	{
		const std::uint64_t key = ((std::uint64_t)uSize << 32) | (std::uint64_t)cp;
		auto it = glyphs.find(key);
		if (it != glyphs.end()) { out = it->second; return true; }
		if (!atlas) {
			atlas.emplace();
			if (!atlas->resize({kAtlasSize, kAtlasSize})) { atlas.reset(); return false; }
			atlas->setSmooth(false);
			penX = penY = rowH = 0;
		}
		if (w > (int)kAtlasSize || h > (int)kAtlasSize) return false;
		if (penX + w + 1 > kAtlasSize) { penX = 0; penY += rowH + 1; rowH = 0; }
		if (penY + h + 1 > kAtlasSize) { atlasFull = true; return false; }
		std::vector<std::uint32_t> px((size_t)w * h);
		for (int i = 0; i < w * h; i++) px[(size_t)i] = 0x00FFFFFFu | ((std::uint32_t)pAlpha[i] << 24);
		atlas->update(reinterpret_cast<const std::uint8_t *>(px.data()), {(unsigned)w, (unsigned)h}, {penX, penY});
		GlyphSlot g;
		g.x = (int)penX; g.y = (int)penY; g.w = w; g.h = h;
		glyphs[key] = g;
		penX += w + 1;
		if ((unsigned)h > rowH) rowH = h;
		out = g;
		return true;
	}
};

// ---------------------------------------------------------------------------- CUiGpu

CUiGpu::CUiGpu(const std::uint32_t * lut) : m(new CUiGpuImpl()) { m->lut = lut; }
CUiGpu::~CUiGpu() {}

bool CUiGpu::Ok() { return m->Init(); }
unsigned int CUiGpu::NextSheetGen() { return ++g_uSheetGen; }
bool CUiGpu::Empty() const { return m->cmds.empty(); }
int  CUiGpu::Count() const { return (int)m->cmds.size(); }

void CUiGpu::Clear(unsigned int uNowMs)
{
	m->frameMs = uNowMs;
	m->cmds.clear();
	m->verts.clear();
	m->lights.clear();
	m->glows.clear();
	m->poolNext = 0;
	m->zoomZ = 1.f;
	m->zoomEnd = (size_t)-1;
	if (m->atlasFull) {		// start the glyph texture over (between frames, so nothing uses it)
		m->glyphs.clear();
		m->penX = m->penY = m->rowH = 0;
		m->atlasFull = false;
	}
}

void CUiGpu::SetLightPass(float ambR, float ambG, float ambB)
{
	if (HasLightPass()) return;
	m->amb[0] = ambR; m->amb[1] = ambG; m->amb[2] = ambB;
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_LIGHTPASS;
	c.first = m->verts.size();
	c.count = 1;		// (no vertices of its own; not empty)
	m->cmds.push_back(c);
}

void CUiGpu::SetZoom(float cx, float cy, float z)
{
	m->zoomX = cx;
	m->zoomY = cy;
	m->zoomZ = (z > 0.01f) ? z : 1.f;
	m->zoomEnd = (size_t)-1;
}

void CUiGpu::EndZoom()
{
	m->zoomEnd = m->cmds.size();
}

bool CUiGpu::HasLightPass() const
{
	for (const CUiGpuImpl::Cmd & c : m->cmds) if (c.type == CUiGpuImpl::T_LIGHTPASS) return true;
	return false;
}

// a quad round (x, y), texture coordinates -1..1, colour with alpha = strength / 2
static void SpotQuad(std::vector<sf::Vertex> & v, float x, float y, float rx, float ry, float r, float g, float b, float a)
{
	const auto ch = [](float f) { return (std::uint8_t)(f <= 0.f ? 0 : (f >= 1.f ? 255 : (int)(f * 255.f + 0.5f))); };
	const sf::Color c(ch(r), ch(g), ch(b), ch(a * 0.5f));
	const sf::Vertex p0{{x - rx, y - ry}, c, {-1.f, -1.f}}, p1{{x + rx, y - ry}, c, {1.f, -1.f}};
	const sf::Vertex p2{{x - rx, y + ry}, c, {-1.f, 1.f}}, p3{{x + rx, y + ry}, c, {1.f, 1.f}};
	v.push_back(p0); v.push_back(p1); v.push_back(p2);
	v.push_back(p1); v.push_back(p3); v.push_back(p2);
}

void CUiGpu::AddLight(float x, float y, float rx, float ry, float r, float g, float b, float a)
{
	if (a <= 0.f || rx <= 0.f || ry <= 0.f) return;
	SpotQuad(m->lights, x, y, rx, ry, r, g, b, a);
}

void CUiGpu::AddLightGlow(float x, float y, float rad, float r, float g, float b, float a)
{
	if (a <= 0.f || rad <= 0.f) return;
	SpotQuad(m->glows, x, y, rad, rad, r, g, b, a);
}

void CUiGpu::AddPanel(int x0, int y0, int x1, int y1, float fRadius, std::uint32_t fillRGB, int iFillA,
	std::uint32_t borderRGB, int iBorderA, float fTailH, float fTailX, float fBorderW, int bx0, int by0, int bx1, int by1)
{
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_PANEL;
	c.box[0] = (x0 + x1) * 0.5f; c.box[1] = (y0 + y1) * 0.5f; c.box[2] = (x1 - x0) * 0.5f; c.box[3] = (y1 - y0) * 0.5f;
	c.shape[0] = fRadius; c.shape[1] = fBorderW; c.shape[2] = fTailH; c.shape[3] = fTailX;
	c.fy1 = (float)y1;
	const sf::Color f = ColorOf(fillRGB, iFillA), b = ColorOf(borderRGB, iBorderA);
	c.fillC[0] = f.r / 255.f; c.fillC[1] = f.g / 255.f; c.fillC[2] = f.b / 255.f; c.fillC[3] = f.a / 255.f;
	c.bordC[0] = b.r / 255.f; c.bordC[1] = b.g / 255.f; c.bordC[2] = b.b / 255.f; c.bordC[3] = b.a / 255.f;
	c.first = m->verts.size();
	m->Quad((float)bx0, (float)by0, (float)bx1, (float)by1, (float)bx0, (float)by0, (float)bx1, (float)by1, sf::Color::White);
	c.count = 6;
	m->cmds.push_back(c);
}

void CUiGpu::AddStar(float cx, float cy, float fRadius, std::uint32_t rgb, int iA, int bx0, int by0, int bx1, int by1)
{
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_STAR;
	const sf::Color col = ColorOf(rgb, iA);
	c.fillC[0] = col.r / 255.f; c.fillC[1] = col.g / 255.f; c.fillC[2] = col.b / 255.f; c.fillC[3] = col.a / 255.f;
	const float Ri = fRadius * 0.42f;
	for (int i = 0; i < 10; i++) {
		const float a = -1.5707963f + i * 0.6283185f, rr = (i % 2 == 0) ? fRadius : Ri;
		c.pts[i] = sf::Glsl::Vec2(cx + rr * std::cos(a), cy + rr * std::sin(a));
	}
	c.first = m->verts.size();
	m->Quad((float)bx0, (float)by0, (float)bx1, (float)by1, (float)bx0, (float)by0, (float)bx1, (float)by1, sf::Color::White);
	c.count = 6;
	m->cmds.push_back(c);
}

void CUiGpu::AddRect(int x0, int y0, int x1, int y1, std::uint32_t rgb, int iA, int iBlend)
{
	if (x1 <= x0 || y1 <= y0) return;
	CUiGpuImpl::Cmd & c = m->Batch(iBlend == UIBL_ADD ? CUiGpuImpl::T_ADDTRIS : (iBlend == UIBL_PREMUL ? CUiGpuImpl::T_PRETRIS : CUiGpuImpl::T_TRIS));
	m->Quad((float)x0, (float)y0, (float)x1, (float)y1, 0.f, 0.f, 0.f, 0.f, ColorOf(rgb, iA));
	c.count += 6;
}

void CUiGpu::AddGlyph(unsigned int uSize, char32_t cp, const std::uint8_t * pAlpha, int w, int h, int x, int y, std::uint32_t rgb)
{
	if (w <= 0 || h <= 0 || pAlpha == nullptr) return;
	CUiGpuImpl::GlyphSlot g;
	if (!m->GlyphSlotFor(uSize, cp, pAlpha, w, h, g)) return;
	CUiGpuImpl::Cmd & c = m->Batch(CUiGpuImpl::T_GLYPHS);
	const int a = (int)(rgb >> 24);		// faded text (CRenderer::m_iFadeA); 0 = solid
	m->Quad((float)x, (float)y, (float)(x + w), (float)(y + h), (float)g.x, (float)g.y, (float)(g.x + w), (float)(g.y + h),
		ColorOf(rgb & 0xFFFFFFu, a == 0 ? 255 : a));
	c.count += 6;
}

void CUiGpu::AddSprite(CSprite * pSheet, CSprite * pMask, int sx, int sy, int sw, int sh,
	float dx, float dy, float dw, float dh, int cx0, int cy0, int cx1, int cy1,
	int iMode, bool bKey, float fAlpha, int addR, int addG, int addB, bool bNight)
{
	if (sw <= 0 || sh <= 0 || dw <= 0.f || dh <= 0.f) return;
	// cut to the clip rectangle here (whole sheet pixels map to whole picture pixels, or are
	// stretched with nearest sampling, so cutting the texture rectangle the same way is exact)
	float x0 = dx, y0 = dy, x1 = dx + dw, y1 = dy + dh;
	float u0 = (float)sx, v0 = (float)sy, u1 = (float)(sx + sw), v1 = (float)(sy + sh);
	const float ku = sw / dw, kv = sh / dh;
	if (x0 < cx0) { u0 += (cx0 - x0) * ku; x0 = (float)cx0; }
	if (y0 < cy0) { v0 += (cy0 - y0) * kv; y0 = (float)cy0; }
	if (x1 > cx1) { u1 -= (x1 - cx1) * ku; x1 = (float)cx1; }
	if (y1 > cy1) { v1 -= (y1 - cy1) * kv; y1 = (float)cy1; }
	if (x1 <= x0 || y1 <= y0) return;
	const CUiGpuImpl::Sheet * pS = m->SheetTexture(pSheet, pMask);
	if (pS == nullptr) return;
	// each page the texture rectangle touches gets its part
	const int P = pS->page;
	const int c0 = (std::max)(0, (int)std::floor(u0) / P), c1 = (std::min)(pS->cols - 1, (int)std::ceil(u1 - 1.f) / P);
	const int r0 = (std::max)(0, (int)std::floor(v0) / P), r1 = (std::min)(pS->rows - 1, (int)std::ceil(v1 - 1.f) / P);
	for (int r = r0; r <= r1; r++)
		for (int c = c0; c <= c1; c++) {
			const float pu0 = (std::max)(u0, (float)(c * P)), pu1 = (std::min)(u1, (float)((c + 1) * P));
			const float pv0 = (std::max)(v0, (float)(r * P)), pv1 = (std::min)(v1, (float)((r + 1) * P));
			if (pu1 <= pu0 || pv1 <= pv0) continue;
			const sf::Texture * pTex = pS->Page(c, r);
			CUiGpuImpl::Cmd * pC = nullptr;
			if (!m->cmds.empty()) {
				CUiGpuImpl::Cmd & l = m->cmds.back();
				if (l.type == CUiGpuImpl::T_SPRITES && l.tex == pTex && l.mode == iMode && l.key == bKey && l.alpha == fAlpha
					&& l.add[0] == addR && l.add[1] == addG && l.add[2] == addB) pC = &l;
			}
			if (pC == nullptr) {
				m->cmds.emplace_back();
				pC = &m->cmds.back();
				pC->type = CUiGpuImpl::T_SPRITES;
				pC->first = m->verts.size();
				pC->tex = pTex; pC->mode = iMode; pC->key = bKey; pC->alpha = fAlpha;
				pC->add[0] = addR; pC->add[1] = addG; pC->add[2] = addB;
			}
			const float qx0 = x0 + (pu0 - u0) / ku, qx1 = x0 + (pu1 - u0) / ku;
			const float qy0 = y0 + (pv0 - v0) / kv, qy1 = y0 + (pv1 - v0) / kv;
			m->Quad(qx0, qy0, qx1, qy1, pu0 - c * P, pv0 - r * P, pu1 - c * P, pv1 - r * P,
				bNight ? sf::Color(0, 255, 255) : sf::Color::White);
			pC->count += 6;
		}
}

// the area a light may cover, in game pixels, cut to the clip rectangle; false: nothing left
static bool LightArea(float & x0, float & y0, float & x1, float & y1, int cx0, int cy0, int cx1, int cy1)
{
	x0 = (std::max)(x0, (float)cx0); y0 = (std::max)(y0, (float)cy0);
	x1 = (std::min)(x1, (float)cx1); y1 = (std::min)(y1, (float)cy1);
	return x1 > x0 && y1 > y0;
}

void CUiGpu::AddLightSpot(float cx, float cy, float sx, float sy, float amp, float r, float g, float b,
	float bx0, float by0, float bx1, float by1, int cx0, int cy0, int cx1, int cy1)
{
	if (amp <= 0.f || sx <= 0.f || sy <= 0.f || !LightArea(bx0, by0, bx1, by1, cx0, cy0, cx1, cy1)) return;
	CUiGpuImpl::Cmd & c = m->Batch(CUiGpuImpl::T_LIGHTSPOTS);
	const auto ch = [](float v) { return (std::uint8_t)(v < 0.f ? 0 : (v > 255.f ? 255 : (int)(v + 0.5f))); };
	const sf::Color col(ch(r), ch(g), ch(b), ch((std::min)(4.f, amp) * 255.f / 4.f));
	m->Quad(2.f * bx0, 2.f * by0, 2.f * bx1, 2.f * by1,
		(bx0 - cx) / sx, (by0 - cy) / sy, (bx1 - cx) / sx, (by1 - cy) / sy, col);
	c.count += 6;
}

void CUiGpu::AddLightRing(float cx, float cy, float rx, float ry, float th, float fade, float a, float r, float g, float b,
	float bx0, float by0, float bx1, float by1, int cx0, int cy0, int cx1, int cy1)
{
	if (fade <= 0.f || rx <= 0.f || ry <= 0.f || th <= 0.f || !LightArea(bx0, by0, bx1, by1, cx0, cy0, cx1, cy1)) return;
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_LIGHT;
	c.mode = 1;
	c.box[0] = cx; c.box[1] = cy; c.box[2] = rx; c.box[3] = ry;
	c.shape[0] = th; c.shape[1] = fade; c.shape[2] = a; c.shape[3] = 0.f;
	c.fillC[0] = r / 255.f; c.fillC[1] = g / 255.f; c.fillC[2] = b / 255.f;
	c.first = m->verts.size();
	m->Quad(2.f * bx0, 2.f * by0, 2.f * bx1, 2.f * by1, bx0, by0, bx1, by1, sf::Color::White);
	c.count = 6;
	m->cmds.push_back(c);
}

void CUiGpu::AddLightPillar(float cx, float feetY, float cut, float v, float sCore, float wid, float r, float g, float b,
	float bx0, float by0, float bx1, float by1, int cx0, int cy0, int cx1, int cy1)
{
	if (v <= 0.f || sCore <= 0.f || wid <= 0.f || !LightArea(bx0, by0, bx1, by1, cx0, cy0, cx1, cy1)) return;
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_LIGHT;
	c.mode = 2;
	c.box[0] = cx; c.box[1] = feetY; c.box[2] = cut; c.box[3] = v;
	c.shape[0] = sCore; c.shape[1] = wid; c.shape[2] = 0.f; c.shape[3] = 0.f;
	c.fillC[0] = r / 255.f; c.fillC[1] = g / 255.f; c.fillC[2] = b / 255.f;
	c.first = m->verts.size();
	m->Quad(2.f * bx0, 2.f * by0, 2.f * bx1, 2.f * by1, bx0, by0, bx1, by1, sf::Color::White);
	c.count = 6;
	m->cmds.push_back(c);
}

void CUiGpu::AddEdgeGlow(int x0, int y0, int x1, int y1, int iBand, std::uint32_t rgb, int iMaxA)
{
	if (x1 <= x0 || y1 <= y0 || iBand <= 0 || iMaxA <= 0) return;
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_GLOW;
	c.box[0] = (float)x0; c.box[1] = (float)y0; c.box[2] = (float)x1; c.box[3] = (float)y1;
	c.shape[0] = (float)iBand;
	const sf::Color col = ColorOf(rgb, iMaxA);
	c.fillC[0] = col.r / 255.f; c.fillC[1] = col.g / 255.f; c.fillC[2] = col.b / 255.f; c.fillC[3] = col.a / 255.f;
	c.first = m->verts.size();
	m->Quad((float)x0, (float)y0, (float)x1, (float)y1, (float)x0, (float)y0, (float)x1, (float)y1, sf::Color::White);
	c.count = 6;
	m->cmds.push_back(c);
}

// a picture's quad, cut to the clip rectangle (texture coordinates in its pixels)
static bool ClipQuad(float & x0, float & y0, float & x1, float & y1, float & u0, float & v0, float & u1, float & v1,
	int cx0, int cy0, int cx1, int cy1)
{
	const float ku = (u1 - u0) / (x1 - x0), kv = (v1 - v0) / (y1 - y0);
	if (x0 < cx0) { u0 += (cx0 - x0) * ku; x0 = (float)cx0; }
	if (y0 < cy0) { v0 += (cy0 - y0) * kv; y0 = (float)cy0; }
	if (x1 > cx1) { u1 -= (x1 - cx1) * ku; x1 = (float)cx1; }
	if (y1 > cy1) { v1 -= (y1 - cy1) * kv; y1 = (float)cy1; }
	return x1 > x0 && y1 > y0;
}

void CUiGpu::AddImageCached(std::uint64_t key, std::uint32_t version, int w, int h,
	const std::function<void(std::uint32_t *)> & fill, bool bSmooth,
	float dx, float dy, float dw, float dh, int cx0, int cy0, int cx1, int cy1, int iBlend)
{
	if (w <= 0 || h <= 0 || dw <= 0.f || dh <= 0.f) return;
	float x0 = dx, y0 = dy, x1 = dx + dw, y1 = dy + dh, u0 = 0.f, v0 = 0.f, u1 = (float)w, v1 = (float)h;
	if (!ClipQuad(x0, y0, x1, y1, u0, v0, u1, v1, cx0, cy0, cx1, cy1)) return;
	CUiGpuImpl::Img & img = m->images[key];
	img.lastUse = m->frameMs;
	if (!img.used || img.version != version || img.tex.getSize().x != (unsigned)w || img.tex.getSize().y != (unsigned)h) {
		if (img.tex.getSize().x != (unsigned)w || img.tex.getSize().y != (unsigned)h)
			if (!img.tex.resize({(unsigned)w, (unsigned)h})) return;
		m->scratch.assign((size_t)w * h, 0u);
		fill(m->scratch.data());
		img.tex.update(reinterpret_cast<const std::uint8_t *>(m->scratch.data()));
		img.version = version;
		img.used = true;
	}
	img.tex.setSmooth(bSmooth);
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_IMAGE;
	c.tex = &img.tex;
	c.blend = iBlend;
	c.first = m->verts.size();
	m->Quad(x0, y0, x1, y1, u0, v0, u1, v1, sf::Color::White);
	c.count = 6;
	m->cmds.push_back(c);
}

void CUiGpu::AddImage(const std::uint32_t * pRgba, int w, int h, bool bSmooth,
	float dx, float dy, float dw, float dh, int cx0, int cy0, int cx1, int cy1, int iBlend)
{
	if (pRgba == nullptr || w <= 0 || h <= 0 || dw <= 0.f || dh <= 0.f) return;
	float x0 = dx, y0 = dy, x1 = dx + dw, y1 = dy + dh, u0 = 0.f, v0 = 0.f, u1 = (float)w, v1 = (float)h;
	if (!ClipQuad(x0, y0, x1, y1, u0, v0, u1, v1, cx0, cy0, cx1, cy1)) return;
	if (m->poolNext >= m->pool.size()) m->pool.emplace_back(new sf::Texture());
	sf::Texture & t = *m->pool[m->poolNext++];
	if (t.getSize().x != (unsigned)w || t.getSize().y != (unsigned)h)
		if (!t.resize({(unsigned)w, (unsigned)h})) return;
	t.update(reinterpret_cast<const std::uint8_t *>(pRgba));
	t.setSmooth(bSmooth);
	CUiGpuImpl::Cmd c;
	c.type = CUiGpuImpl::T_IMAGE;
	c.tex = &t;
	c.blend = iBlend;
	c.first = m->verts.size();
	m->Quad(x0, y0, x1, y1, u0, v0, u1, v1, sf::Color::White);
	c.count = 6;
	m->cmds.push_back(c);
}

void CUiGpu::AddShadow(CSprite * pSheet, CSprite * pMask, int sx, int sy, int sw, int sh,
	float xTop, float yTop, float xBot, float yBot, float w, float yMax)
{
	if (sw <= 0 || sh <= 0 || w <= 0.f || yBot <= yTop) return;
	float v0 = (float)sy, v1 = (float)(sy + sh);
	if (yBot > yMax) {			// cut at the bottom: the sides move along with the cut
		if (yTop >= yMax) return;
		const float f = (yMax - yTop) / (yBot - yTop);
		xBot = xTop + (xBot - xTop) * f;
		v1 = v0 + (v1 - v0) * f;
		yBot = yMax;
	}
	const CUiGpuImpl::Sheet * pS = m->SheetTexture(pSheet, pMask);
	if (pS == nullptr) return;
	const float u0 = (float)sx, u1 = (float)(sx + sw);
	// sheet (u, v) -> picture: across by w over the width, down and sideways over the height
	auto map = [&](float u, float v, float & x, float & y) {
		const float fu = (u - u0) / (u1 - u0), fv = (v - v0) / (v1 - v0);
		x = xTop + (xBot - xTop) * fv + w * fu;
		y = yTop + (yBot - yTop) * fv;
	};
	const int P = pS->page;
	const int c0 = (std::max)(0, (int)u0 / P), c1 = (std::min)(pS->cols - 1, (int)std::ceil(u1 - 1.f) / P);
	const int r0 = (std::max)(0, (int)v0 / P), r1 = (std::min)(pS->rows - 1, (int)std::ceil(v1 - 1.f) / P);
	for (int r = r0; r <= r1; r++)
		for (int c = c0; c <= c1; c++) {
			const float pu0 = (std::max)(u0, (float)(c * P)), pu1 = (std::min)(u1, (float)((c + 1) * P));
			const float pv0 = (std::max)(v0, (float)(r * P)), pv1 = (std::min)(v1, (float)((r + 1) * P));
			if (pu1 <= pu0 || pv1 <= pv0) continue;
			const sf::Texture * pTex = pS->Page(c, r);
			CUiGpuImpl::Cmd * pC = nullptr;
			if (!m->cmds.empty()) {
				CUiGpuImpl::Cmd & l = m->cmds.back();
				if (l.type == CUiGpuImpl::T_SPRITES && l.tex == pTex && l.mode == UISPR_SHADE && l.key) pC = &l;
			}
			if (pC == nullptr) {
				m->cmds.emplace_back();
				pC = &m->cmds.back();
				pC->type = CUiGpuImpl::T_SPRITES;
				pC->first = m->verts.size();
				pC->tex = pTex; pC->mode = UISPR_SHADE; pC->key = true; pC->alpha = 1.f;
			}
			float ax, ay, bx, by, dx_, dy_, ex, ey;
			map(pu0, pv0, ax, ay); map(pu1, pv0, bx, by); map(pu0, pv1, dx_, dy_); map(pu1, pv1, ex, ey);
			const float tu0 = pu0 - c * P, tu1 = pu1 - c * P, tv0 = pv0 - r * P, tv1 = pv1 - r * P;
			const sf::Color col = sf::Color::White;
			const sf::Vertex A{{ax, ay}, col, {tu0, tv0}}, B{{bx, by}, col, {tu1, tv0}};
			const sf::Vertex D{{dx_, dy_}, col, {tu0, tv1}}, E{{ex, ey}, col, {tu1, tv1}};
			m->verts.push_back(A); m->verts.push_back(B); m->verts.push_back(D);
			m->verts.push_back(B); m->verts.push_back(E); m->verts.push_back(D);
			pC->count += 6;
		}
}

void CUiGpu::Draw(sf::RenderTarget & rt, int W2, int H2)
{
	if (!m->Init() || m->cmds.empty()) return;
	// the zoomed scene (SetZoom), then the rest as it is
	const sf::View vFlat(sf::FloatRect({0.f, 0.f}, {(float)W2, (float)H2}));
	const bool bZoom = m->zoomZ != 1.f;
	sf::View vZoom = vFlat;
	if (bZoom) {
		vZoom.setCenter({m->zoomX, m->zoomY});
		vZoom.setSize({(float)W2 / m->zoomZ, (float)H2 / m->zoomZ});
	}
	const size_t iZoomEnd = bZoom ? m->zoomEnd : 0;
	rt.setView(bZoom ? vZoom : vFlat);
	const sf::Vertex * pV = m->verts.data();
	const auto drawCmd = [&](const CUiGpuImpl::Cmd & c) {
		sf::RenderStates st;
		switch (c.type) {
		case CUiGpuImpl::T_PANEL:
			m->panelSh.setUniform("box", sf::Glsl::Vec4(c.box[0], c.box[1], c.box[2], c.box[3]));
			m->panelSh.setUniform("shape", sf::Glsl::Vec4(c.shape[0], c.shape[1], c.shape[2], c.shape[3]));
			m->panelSh.setUniform("fy1", c.fy1);
			m->panelSh.setUniform("fillC", sf::Glsl::Vec4(c.fillC[0], c.fillC[1], c.fillC[2], c.fillC[3]));
			m->panelSh.setUniform("bordC", sf::Glsl::Vec4(c.bordC[0], c.bordC[1], c.bordC[2], c.bordC[3]));
			st.shader = &m->panelSh;
			st.blendMode = kBlendPremul;
			break;
		case CUiGpuImpl::T_STAR:
			m->starSh.setUniformArray("pts", c.pts, 10);
			m->starSh.setUniform("col", sf::Glsl::Vec4(c.fillC[0], c.fillC[1], c.fillC[2], c.fillC[3]));
			st.shader = &m->starSh;
			st.blendMode = kBlendPremul;
			break;
		case CUiGpuImpl::T_TRIS:
			st.blendMode = kBlendAlpha;
			break;
		case CUiGpuImpl::T_ADDTRIS:
			st.blendMode = kBlendAdd;
			break;
		case CUiGpuImpl::T_PRETRIS:
			st.blendMode = kBlendPremul;
			break;
		case CUiGpuImpl::T_LIGHTSPOTS:
		case CUiGpuImpl::T_LIGHT:
			m->lightSh.setUniform("mode", c.type == CUiGpuImpl::T_LIGHTSPOTS ? 0 : c.mode);
			if (c.type == CUiGpuImpl::T_LIGHT) {
				m->lightSh.setUniform("p0", sf::Glsl::Vec4(c.box[0], c.box[1], c.box[2], c.box[3]));
				m->lightSh.setUniform("p1", sf::Glsl::Vec4(c.shape[0], c.shape[1], c.shape[2], c.shape[3]));
				m->lightSh.setUniform("col", sf::Glsl::Vec3(c.fillC[0], c.fillC[1], c.fillC[2]));
			}
			st.shader = &m->lightSh;
			st.blendMode = kBlendAdd;
			break;
		case CUiGpuImpl::T_IMAGE:
			st.texture = c.tex;
			st.blendMode = (c.blend == UIBL_ADD) ? kBlendAdd : (c.blend == UIBL_PREMUL ? kBlendPremul : kBlendAlpha);
			break;
		case CUiGpuImpl::T_GLOW:
			m->glowSh.setUniform("rect", sf::Glsl::Vec4(c.box[0], c.box[1], c.box[2], c.box[3]));
			m->glowSh.setUniform("band", c.shape[0]);
			m->glowSh.setUniform("col", sf::Glsl::Vec4(c.fillC[0], c.fillC[1], c.fillC[2], c.fillC[3]));
			st.shader = &m->glowSh;
			st.blendMode = kBlendPremul;
			break;
		case CUiGpuImpl::T_GLYPHS:
			if (!m->atlas) return;
			st.texture = &*m->atlas;
			st.blendMode = kBlendAlpha;
			break;
		case CUiGpuImpl::T_SPRITES:
			// (only what changed: each uniform set switches the shader program on and back)
			if (m->lastMode < 0) m->spriteSh.setUniform("tex", sf::Shader::CurrentTexture);
			if (c.mode != m->lastMode) { m->spriteSh.setUniform("mode", c.mode); m->lastMode = c.mode; }
			if ((c.key ? 1.f : 0.f) != m->lastKey) { m->lastKey = c.key ? 1.f : 0.f; m->spriteSh.setUniform("useKey", m->lastKey); }
			if (c.alpha != m->lastAlpha) { m->spriteSh.setUniform("alpha", c.alpha); m->lastAlpha = c.alpha; }
			if (c.add[0] != m->lastAdd[0] || c.add[1] != m->lastAdd[1] || c.add[2] != m->lastAdd[2]) {
				m->spriteSh.setUniform("add", sf::Glsl::Vec3((float)c.add[0], (float)c.add[1], (float)c.add[2]));
				m->lastAdd[0] = c.add[0]; m->lastAdd[1] = c.add[1]; m->lastAdd[2] = c.add[2];
			}
			st.texture = c.tex;
			st.shader = &m->spriteSh;
			st.blendMode = (c.mode == UISPR_ADD || c.mode == UISPR_ADDTINT) ? kBlendAdd
				: (c.mode == UISPR_SUBTRACT ? kBlendSub : kBlendAlpha);
			break;
		}
		rt.draw(pV + c.first, c.count, sf::PrimitiveType::Triangles, st);
	};

	// the light pass, if there is one: the overlays recorded before it wait until after it
	// (no pass: iPass is past the end, nothing waits and everything is drawn in order)
	size_t iPass = (size_t)-1;
	for (size_t i = 0; i < m->cmds.size(); i++) if (m->cmds[i].type == CUiGpuImpl::T_LIGHTPASS) { iPass = i; break; }
	// Sprites that darken what is there (smoke, shadows: the shade and subtract modes) after the
	// pass go before it instead, so the night darkens them with the world; after it they would
	// take the little colour left at night away and show as black.
	// (with a zoomed scene, only those in it: what follows it is drawn unzoomed, in order)
	const auto darkens = [&](const CUiGpuImpl::Cmd & c, size_t i) {
		return c.type == CUiGpuImpl::T_SPRITES && (c.mode == UISPR_SHADE || c.mode == UISPR_SUBTRACT) && (!bZoom || i < iZoomEnd);
	};
	for (size_t i = 0; i < m->cmds.size(); i++) {
		if (bZoom && i == iZoomEnd) rt.setView(vFlat);
		const CUiGpuImpl::Cmd & c = m->cmds[i];
		if (c.count == 0) continue;
		if (iPass != (size_t)-1 && i < iPass && CUiGpuImpl::Unlit(c.type)) continue;
		if (iPass != (size_t)-1 && i > iPass && darkens(c, i)) continue;
		if (i != iPass) { drawCmd(c); continue; }

		for (size_t j = iPass + 1; j < m->cmds.size(); j++)
			if (m->cmds[j].count != 0 && darkens(m->cmds[j], j)) drawCmd(m->cmds[j]);

		// the light map, at a quarter of the size (smoothed when stretched): moonlight, then every
		// light added on top
		const unsigned int lw = (unsigned int)(W2 + 3) / 4, lh = (unsigned int)(H2 + 3) / 4;
		if (!m->lightRT || m->lightRT->getSize().x != lw || m->lightRT->getSize().y != lh) {
			m->lightRT.emplace();
			if (!m->lightRT->resize({lw, lh})) m->lightRT.reset();
			else m->lightRT->setSmooth(true);
		}
		if (m->lightRT) {
			sf::RenderTexture & L = *m->lightRT;
			const auto ch = [](float f) { return (std::uint8_t)(f <= 0.f ? 0 : (f >= 1.f ? 255 : (int)(f * 255.f + 0.5f))); };
			L.clear(sf::Color(ch(m->amb[0]), ch(m->amb[1]), ch(m->amb[2])));
			L.setView(sf::View(sf::FloatRect({0.f, 0.f}, {(float)lw * 4.f, (float)lh * 4.f})));
			if (!m->lights.empty()) {
				m->spotSh.setUniform("s1", 0.35f);
				m->spotSh.setUniform("v1", 0.55f);
				sf::RenderStates ls;
				ls.shader = &m->spotSh;
				ls.blendMode = sf::BlendAdd;
				L.draw(m->lights.data(), m->lights.size(), sf::PrimitiveType::Triangles, ls);
			}
			L.display();
			sf::Sprite map(L.getTexture());
			map.setScale({4.f, 4.f});
			rt.draw(map, sf::RenderStates(sf::BlendMultiply));
		}
		// the lamps' flames, bright on top
		if (!m->glows.empty()) {
			m->spotSh.setUniform("s1", 0.25f);
			m->spotSh.setUniform("v1", 0.45f);
			sf::RenderStates gs;
			gs.shader = &m->spotSh;
			gs.blendMode = sf::BlendAdd;
			rt.draw(m->glows.data(), m->glows.size(), sf::PrimitiveType::Triangles, gs);
		}
		// the overlays from before, unlit
		for (size_t j = 0; j < iPass; j++)
			if (m->cmds[j].count != 0 && CUiGpuImpl::Unlit(m->cmds[j].type)) drawCmd(m->cmds[j]);
	}
}

void CUiGpu::Trim(unsigned int uNowMs)
{
	for (auto it = m->images.begin(); it != m->images.end(); ) {
		if (uNowMs - it->second.lastUse > kSheetIdleMs) it = m->images.erase(it);
		else ++it;
	}
	for (auto it = m->sheets.begin(); it != m->sheets.end(); ) {
		if (uNowMs - it->second.lastUse > kSheetIdleMs) it = m->sheets.erase(it);
		else ++it;
	}
}
