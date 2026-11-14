// LevelUpBadges.cpp: drawing of the level-up badges (see LevelUpBadges.h).
//////////////////////////////////////////////////////////////////////

#define NOMINMAX
#include <windows.h>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>

#include "LevelUpBadges.h"
#include "Renderer.h"

namespace {

sf::Color Col(int r, int g, int b, float a) { return sf::Color((std::uint8_t)r, (std::uint8_t)g, (std::uint8_t)b, (std::uint8_t)std::clamp(a * 255.f, 0.f, 255.f)); }

void DrawText(sf::RenderWindow & w, const sf::Font & font, const std::string & s, float cx, float top, float px,
	sf::Color fill, sf::Color outline, float outlinePx)
{
	sf::Text t(font, s, (unsigned int)std::max(6.f, px));
	t.setStyle(sf::Text::Bold);
	t.setFillColor(fill);
	t.setOutlineColor(outline);
	t.setOutlineThickness(outlinePx);
	const sf::FloatRect b = t.getLocalBounds();
	t.setPosition({ std::round(cx - b.position.x - b.size.x / 2.f), std::round(top - b.position.y) });
	w.draw(t);
}

// Same, but centred both ways on (cx, cy) by the glyphs' actual ink, so a number sits in the
// middle of its badge whatever its digits.
void DrawTextCentered(sf::RenderWindow & w, const sf::Font & font, const std::string & s, float cx, float cy, float px,
	sf::Color fill, sf::Color outline, float outlinePx)
{
	sf::Text t(font, s, (unsigned int)std::max(6.f, px));
	t.setStyle(sf::Text::Bold);
	t.setFillColor(fill);
	t.setOutlineColor(outline);
	t.setOutlineThickness(outlinePx);
	const sf::FloatRect b = t.getLocalBounds();
	t.setPosition({ std::round(cx - b.position.x - b.size.x / 2.f), std::round(cy - b.position.y - b.size.y / 2.f) });
	w.draw(t);
}

// The shield: flat top, straight sides, pointed bottom. Centre (cx, cy), half width hw, half height hh.
void Shield(sf::ConvexShape & s, float cx, float cy, float hw, float hh)
{
	s.setPointCount(5);
	s.setPoint(0, { cx - hw, cy - hh });
	s.setPoint(1, { cx + hw, cy - hh });
	s.setPoint(2, { cx + hw, cy + hh * 0.45f });
	s.setPoint(3, { cx,      cy + hh * 1.35f });
	s.setPoint(4, { cx - hw, cy + hh * 0.45f });
}

} // namespace

void CLevelUpBadges::Draw(sf::RenderWindow & w)
{
	if (m_pRenderer == NULL) return;
	const sf::Font * pFont = m_pRenderer->GetFont();
	const float kBadgeSize = 1.6f;						// overall size of the badges and their text
	const float k = m_pRenderer->GameScaleY() * kBadgeSize;	// window pixels per badge unit
	if (k <= 0) return;

	for (const Badge & b : m_badges) {
		if (b.alpha <= 0.f) continue;
		float cx, cy;
		m_pRenderer->GameToWindow(b.x, b.y, cx, cy);
		const float s = k * b.scale;
		const float a = b.alpha;

		if (b.style == STYLE_CROWN) {
			// glow
			for (int i = 3; i >= 1; i--) {
				sf::CircleShape g(14.f * s + i * 3.f * s);
				g.setOrigin({ g.getRadius(), g.getRadius() });
				g.setPosition({ cx, cy - 4.f * s });
				g.setScale({ 1.f, 0.7f });
				g.setFillColor(Col(255, 215, 110, a * 0.10f));
				w.draw(g);
			}
			// spikes (three tall, two short), then the band and the jewels
			const float bandTop = cy - 2.7f * s, bandBot = cy + 4.5f * s, hw = 10.9f * s;
			const float tipX[5] = { -10.9f, -5.5f, 0.f, 5.5f, 10.9f };
			const float tipY[5] = { -10.f,  -7.3f, -11.8f, -7.3f, -10.f };
			for (int i = 0; i < 5; i++) {
				sf::ConvexShape sp(3);
				const float x = cx + tipX[i] * s;
				const float half = (i == 0 || i == 4) ? 2.6f * s : 3.6f * s;
				sp.setPoint(0, { std::clamp(x - half, cx - hw, cx + hw), bandTop + 0.5f });
				sp.setPoint(1, { std::clamp(x + half, cx - hw, cx + hw), bandTop + 0.5f });
				sp.setPoint(2, { x, cy + tipY[i] * s });
				sp.setFillColor(Col(232, 178, 52, a));
				sp.setOutlineColor(Col(255, 245, 200, a));
				sp.setOutlineThickness(std::max(1.f, 0.35f * s));
				w.draw(sp);
			}
			sf::RectangleShape band({ hw * 2.f, bandBot - bandTop });
			band.setPosition({ cx - hw, bandTop });
			band.setFillColor(Col(120, 70, 15, a * 0.95f));
			band.setOutlineColor(Col(255, 230, 150, a));
			band.setOutlineThickness(std::max(1.f, 0.35f * s));
			w.draw(band);
			for (int i = 0; i < 5; i += 2) {
				sf::CircleShape j(std::max(1.5f, 1.0f * s));
				j.setOrigin({ j.getRadius(), j.getRadius() });
				j.setPosition({ cx + tipX[i] * s, cy + tipY[i] * s });
				j.setFillColor(Col(255, 255, 235, a));
				w.draw(j);
			}
			if (pFont) {
				DrawTextCentered(w, *pFont, b.number, cx, (bandTop + bandBot) / 2.f, 6.2f * s, Col(255, 240, 180, a), Col(70, 35, 0, a), 0.f);
				DrawText(w, *pFont, b.label, cx, cy - 22.f * s, 6.6f * s, Col(255, 240, 170, a), Col(70, 35, 0, a * 0.9f), std::max(1.f, 0.8f * s));
			}
			continue;
		}

		// shield badge
		const bool silver = (b.style == STYLE_SILVER);
		const float hw = 7.7f * s, hh = 6.4f * s;
		sf::ConvexShape sh;
		for (int i = 3; i >= 1; i--) {			// soft glow behind
			Shield(sh, cx, cy, hw + i * 1.6f * s, hh + i * 1.6f * s);
			sh.setFillColor(silver ? Col(220, 230, 255, a * 0.12f) : Col(255, 200, 90, a * 0.12f));
			sh.setOutlineThickness(0);
			w.draw(sh);
		}
		Shield(sh, cx, cy, hw, hh);
		sh.setFillColor(silver ? Col(55, 62, 78, a * 0.92f) : Col(95, 52, 12, a * 0.92f));
		sh.setOutlineColor(silver ? Col(235, 240, 255, a) : Col(255, 215, 110, a));
		sh.setOutlineThickness(std::max(1.f, 0.45f * s));
		w.draw(sh);
		Shield(sh, cx, cy, hw - 1.4f * s, hh - 1.4f * s);	// inner line
		sh.setFillColor(sf::Color::Transparent);
		sh.setOutlineColor(silver ? Col(150, 160, 185, a * 0.8f) : Col(200, 150, 60, a * 0.8f));
		sh.setOutlineThickness(std::max(1.f, 0.25f * s));
		w.draw(sh);
		if (pFont) {
			const sf::Color txt = silver ? Col(240, 244, 255, a) : Col(255, 232, 150, a);
			const sf::Color edge = silver ? Col(25, 30, 45, a * 0.9f) : Col(60, 30, 0, a * 0.9f);
			// the shield's centre of area is at about cy (flat top at cy - hh, point at cy + 1.35 hh)
			DrawTextCentered(w, *pFont, b.number, cx, cy - 0.03f * hh, 6.8f * s, txt, edge, 0.f);
			DrawText(w, *pFont, b.label, cx, cy - hh - 8.6f * s, 5.9f * s, txt, edge, std::max(1.f, 0.8f * s));
		}
	}
}
