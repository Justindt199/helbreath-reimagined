// LevelUpBadges.h: the floating badge of the level-up effect ("LEVEL UP" with the new level
// in a shield, a silver "+1 MAJESTIC" shield, or the "MAX LEVEL" crown).
//
// The game works out where each badge is and how far through its animation it is; this
// overlay just draws them sharply at window resolution, on top of the game picture.
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_LEVELUPBADGES_H
#define HB_ENGINE_LEVELUPBADGES_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <string>
#include <vector>

#include "UiOverlay.h"

class CRenderer;

class CLevelUpBadges : public CUiOverlay
{
public:
	enum Style { STYLE_GOLD = 0, STYLE_SILVER = 1, STYLE_CROWN = 2 };

	struct Badge
	{
		float x = 0, y = 0;		// centre of the badge, in game-canvas pixels
		int   style = STYLE_GOLD;
		std::string number;		// inside the shield / on the crown band
		std::string label;		// above it
		float alpha = 1.f;		// 0..1
		float scale = 1.f;		// pop-in and milestone size
	};

	CLevelUpBadges() : m_pRenderer(0) {}
	void Init(CRenderer * pRenderer) { m_pRenderer = pRenderer; }
	void Clear() { m_badges.clear(); }
	void Add(const Badge & b) { m_badges.push_back(b); }

	// CUiOverlay
	virtual bool IsVisible() const { return !m_badges.empty(); }
	virtual void Draw(sf::RenderWindow & window);

private:
	CRenderer * m_pRenderer;
	std::vector<Badge> m_badges;
};

#endif // HB_ENGINE_LEVELUPBADGES_H
