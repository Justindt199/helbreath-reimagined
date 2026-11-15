// SettingsMenu.cpp: the full-resolution system / settings menu (see SettingsMenu.h).
//////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "SettingsMenu.h"
#include "Renderer.h"
#include "Mouse.h"

//////////////////////////////////////////////////////////////////////
// Layout (logical units; everything is scaled to the window by m_fScale)
//////////////////////////////////////////////////////////////////////

namespace
{
	// Same look as the in-game windows drawn in code (chat panel, character window): a dark
	// see-through panel with a thin light edge, gold title, gold accents.
	const float kW			= 820.f;	// panel size
	const float kH			= 600.f;
	const float kRadius		= 12.f;		// panel corner radius
	const float kTitleH		= 46.f;		// title bar
	const float kSideW		= 190.f;	// tab column
	const float kFooterH	= 54.f;		// help strip under the settings
	const float kTabH		= 42.f;
	const float kTabsTop	= 12.f;		// first tab, below the title bar
	const float kSideBtnH	= 34.f;
	const float kSideBtnGap	= 10.f;
	const float kPad		= 24.f;		// left / right padding of the settings list
	const float kHeadingH	= 56.f;		// the tab's name at the top of the list
	const float kRowH		= 36.f;
	const float kRowGap		= 2.f;
	const float kSectionTitleH = 34.f;
	const float kSectionGap	= 10.f;

	const float kTitleSize	= 17.f;
	const float kHeadingSize = 22.f;
	const float kTabSize	= 15.f;
	const float kLabelSize	= 14.5f;
	const float kSectionSize = 12.f;
	const float kHelpSize	= 13.5f;
	const float kSmallSize	= 12.f;

	// Colours, 0xRRGGBBAA
	const unsigned int kDim			= 0x00000050;
	const unsigned int kShadow		= 0x00000066;
	const unsigned int kPanel		= 0x0A0C10E2;
	const unsigned int kPanelEdge	= 0xFFFFFF22;
	const unsigned int kTitleTint	= 0xE8C0701A;
	const unsigned int kLine		= 0xFFFFFF17;
	const unsigned int kGold		= 0xE8C070FF;
	const unsigned int kGoldTitle	= 0xF2CF73FF;
	const unsigned int kOnGold		= 0x1A1408FF;	// text / knob on a gold fill
	const unsigned int kWhite		= 0xFFFFFFFF;
	const unsigned int kText		= 0xD4D9DFFF;
	const unsigned int kTextDim		= 0x8F97A1FF;
	const unsigned int kTextTab		= 0xA9B0BAFF;
	const unsigned int kTextOff		= 0x5F6670FF;
	const unsigned int kIcon		= 0xC9CED6CC;
	const unsigned int kSidebar		= 0x0000002E;
	const unsigned int kTabSel		= 0xFFFFFF17;
	const unsigned int kTabHover	= 0xFFFFFF0A;
	const unsigned int kRowHover	= 0xFFFFFF0F;
	const unsigned int kRowHoverEdge = 0xE8C07059;
	const unsigned int kCtl			= 0x00000059;	// dark control background
	const unsigned int kCtlHover	= 0x00000080;
	const unsigned int kCtlEdge		= 0xFFFFFF1F;
	const unsigned int kSwitchOff	= 0xFFFFFF1F;
	const unsigned int kKnobOff		= 0x9AA2ACFF;
	const unsigned int kTrack		= 0xFFFFFF1F;
	const unsigned int kButton		= 0xFFFFFF12;
	const unsigned int kButtonHover	= 0xFFFFFF24;
	const unsigned int kButtonEdge	= 0xFFFFFF21;
	const unsigned int kButtonText	= 0xDFE4EAFF;
	const unsigned int kDanger		= 0x9628282E;
	const unsigned int kDangerHover	= 0x96282855;
	const unsigned int kDangerEdge	= 0xFF78788C;
	const unsigned int kDangerText	= 0xFFB0B0FF;
	const unsigned int kList		= 0x101318F8;	// open drop-down list
	const unsigned int kListHover	= 0xFFFFFF14;
	const unsigned int kFooter		= 0x00000047;

	const char * const kDefaultHelp = "Point at an option to see what it does.";
	const std::string kMoveHelp = "Drag the title bar to move this menu.";
	const char * const kEscHint = "Esc to close";

	inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

	// "Show FPS:" -> "Show FPS"
	std::string NoColon(const std::string & s)
	{
		size_t n = s.size();
		while (n > 0 && (s[n - 1] == ':' || s[n - 1] == ' ')) n--;
		return s.substr(0, n);
	}

	std::string Upper(const std::string & s)
	{
		std::string u = s;
		for (size_t i = 0; i < u.size(); i++) if (u[i] >= 'a' && u[i] <= 'z') u[i] = (char)(u[i] - 'a' + 'A');
		return u;
	}

	std::string Lower(const std::string & s)
	{
		std::string u = s;
		for (size_t i = 0; i < u.size(); i++) if (u[i] >= 'A' && u[i] <= 'Z') u[i] = (char)(u[i] - 'A' + 'a');
		return u;
	}
}

//////////////////////////////////////////////////////////////////////
// Construction / building
//////////////////////////////////////////////////////////////////////

CSettingsMenu::CSettingsMenu()
{
	m_pRenderer = NULL;
	m_pMouse = NULL;
	m_pFont = NULL;
	m_bOpen = false;
	m_title = "Options";
	m_iTab = 0;
	m_fScale = 1.f;
	m_fOX = m_fOY = 0.f;
	m_fPanX = m_fPanY = 0.f;
	m_bMovingPanel = false;
	m_iMoveStartX = m_iMoveStartY = 0;
	m_fMoveStartPanX = m_fMoveStartPanY = 0.f;
	m_fScroll = 0.f;
	m_fContentHeight = 0.f;
	m_iDragItem = -1;
	m_iOpenDropdown = -1;
	m_pHoverHelp = NULL;
}

CSettingsMenu::~CSettingsMenu()
{
}

void CSettingsMenu::Init(CRenderer * pRenderer, CMouse * pMouse)
{
	m_pRenderer = pRenderer;
	m_pMouse = pMouse;
}

void CSettingsMenu::Open()
{
	if (m_bOpen) return;
	m_bOpen = true;
	m_iDragItem = -1;
	m_iOpenDropdown = -1;
	m_bMovingPanel = false;
	if (m_pMouse != NULL) m_pMouse->SetUiCapture(true);
}

void CSettingsMenu::Close()
{
	if (!m_bOpen) return;
	m_bOpen = false;
	m_iDragItem = -1;
	m_iOpenDropdown = -1;
	m_layout.clear();
	if (m_pMouse != NULL) m_pMouse->SetUiCapture(false);
	if (m_onClose) m_onClose();
}

void CSettingsMenu::Toggle()
{
	if (m_bOpen) Close(); else Open();
}

void CSettingsMenu::Clear()
{
	m_tabs.clear();
	m_sideButtons.clear();
	m_layout.clear();
	m_iTab = 0;
	m_fScroll = 0.f;
	m_iDragItem = -1;
	m_iOpenDropdown = -1;
	m_pHoverHelp = NULL;
}

int CSettingsMenu::AddTab(const std::string & name, const std::string & emptyText)
{
	Tab tab;
	tab.name = name;
	tab.emptyText = emptyText;
	m_tabs.push_back(tab);
	return (int)m_tabs.size() - 1;
}

void CSettingsMenu::AddSection(int iTab, const std::string & title)
{
	if (iTab < 0 || iTab >= (int)m_tabs.size()) return;
	Section sec;
	sec.title = title;
	m_tabs[iTab].sections.push_back(sec);
}

CSettingsMenu::Item & CSettingsMenu::_AddItem(int iTab, ItemType type, const std::string & label, const std::string & help)
{
	static Item s_dummy;
	if (iTab < 0 || iTab >= (int)m_tabs.size()) { s_dummy = Item(); return s_dummy; }
	Tab & tab = m_tabs[iTab];
	if (tab.sections.empty()) AddSection(iTab, "");
	Item item;
	item.type = type;
	item.label = label;
	item.help = help;
	tab.sections.back().items.push_back(item);
	return tab.sections.back().items.back();
}

CSettingsMenu::Item & CSettingsMenu::AddToggle(int iTab, const std::string & label, const std::string & help, GetFn get, SetFn set)
{
	Item & item = _AddItem(iTab, ITEM_TOGGLE, label, help);
	item.get = get;
	item.set = set;
	return item;
}

CSettingsMenu::Item & CSettingsMenu::AddChoice(int iTab, const std::string & label, const std::string & help,
	const std::vector<std::string> & choices, GetFn get, SetFn set)
{
	Item & item = _AddItem(iTab, ITEM_CHOICE, label, help);
	item.choices = choices;
	item.get = get;
	item.set = set;
	return item;
}

CSettingsMenu::Item & CSettingsMenu::AddSlider(int iTab, const std::string & label, const std::string & help,
	int iMin, int iMax, const std::string & suffix, GetFn get, SetFn set)
{
	Item & item = _AddItem(iTab, ITEM_SLIDER, label, help);
	item.minValue = iMin;
	item.maxValue = (iMax > iMin) ? iMax : iMin + 1;
	item.suffix = suffix;
	item.get = get;
	item.set = set;
	return item;
}

CSettingsMenu::Item & CSettingsMenu::AddButton(int iTab, const std::string & label, const std::string & help, TextFn caption, ActionFn action)
{
	Item & item = _AddItem(iTab, ITEM_BUTTON, label, help);
	item.text = caption;
	item.action = action;
	return item;
}

CSettingsMenu::Item & CSettingsMenu::AddInfo(int iTab, const std::string & label, const std::string & help, TextFn text)
{
	Item & item = _AddItem(iTab, ITEM_INFO, label, help);
	item.text = text;
	return item;
}

CSettingsMenu::Item & CSettingsMenu::AddDropdown(int iTab, const std::string & label, const std::string & help,
	const std::vector<std::string> & choices, GetFn get, SetFn set)
{
	Item & item = _AddItem(iTab, ITEM_DROPDOWN, label, help);
	item.choices = choices;
	item.get = get;
	item.set = set;
	return item;
}

CSettingsMenu::Item & CSettingsMenu::AddMultiDropdown(int iTab, const std::string & label, const std::string & help,
	const std::vector<std::string> & choices, GetFn get, SetFn set)
{
	Item & item = AddDropdown(iTab, label, help, choices, get, set);
	item.multi = true;
	return item;
}

// Box text for a multi-select drop-down: "Off", "All", or the ticked entries joined.
std::string CSettingsMenu::_MultiText(const Item & item, int iMask)
{
	const int n = (int)item.choices.size();
	if (n < 3) return "";
	const int iAll = (1 << (n - 2)) - 1;
	if ((iMask & iAll) == 0) return item.choices[0];
	if ((iMask & iAll) == iAll) return item.choices[n - 1];
	std::string s;
	for (int i = 1; i < n - 1; i++)
		if (iMask & (1 << (i - 1))) { if (!s.empty()) s += ", "; s += item.choices[i]; }
	return s;
}

void CSettingsMenu::AddSideButton(TextFn label, BoolFn visible, ActionFn onClick, const std::string & help, bool bDanger)
{
	SideButton b;
	b.label = label;
	b.visible = visible;
	b.onClick = onClick;
	b.help = help;
	b.danger = bDanger;
	m_sideButtons.push_back(b);
}

void CSettingsMenu::SelectTab(int iTab)
{
	if (iTab < 0 || iTab >= (int)m_tabs.size() || iTab == m_iTab) return;
	m_iTab = iTab;
	m_fScroll = 0.f;
	m_iDragItem = -1;
	m_iOpenDropdown = -1;
	m_layout.clear();
}

bool CSettingsMenu::_IsEnabled(const Item & item) const
{
	return !item.enabled || item.enabled();
}

//////////////////////////////////////////////////////////////////////
// Text helpers
//////////////////////////////////////////////////////////////////////

unsigned int CSettingsMenu::Px(float size) const
{
	const long px = std::lround(size * m_fScale);
	return (unsigned int)std::max(8L, px);
}

float CSettingsMenu::_CapCenter(unsigned int px, bool bold)
{
	std::map<unsigned int, float> & cache = m_capCenter[m_bCapFace2 ? 2 : (bold ? 1 : 0)];
	std::map<unsigned int, float>::iterator it = cache.find(px);
	if (it != cache.end()) return it->second;
	float c = px * 0.6f;
	if (m_pFont != NULL) {
		sf::Text t(*m_pFont, "HXE", px);
		if (bold) t.setStyle(sf::Text::Bold);
		const sf::FloatRect b = t.getLocalBounds();
		c = b.position.y + b.size.y / 2.f;
	}
	cache[px] = c;
	return c;
}

float CSettingsMenu::_TextWidth(const std::string & s, float size, bool bold)
{
	if (m_pFont == NULL || s.empty()) return 0.f;
	sf::Text t(*m_pFont, sf::String(s), Px(size));
	if (bold) t.setStyle(sf::Text::Bold);
	const sf::FloatRect b = t.getLocalBounds();
	return (b.position.x + b.size.x) / m_fScale;
}

void CSettingsMenu::_Text(sf::RenderWindow & w, const std::string & s, float x, float yCenter, float size,
	unsigned int color, int align, bool bold)
{
	if (m_pFont == NULL || s.empty()) return;
	const unsigned int px = Px(size);
	sf::Text t(*m_pFont, sf::String(s), px);
	if (bold) t.setStyle(sf::Text::Bold);
	t.setFillColor(sf::Color(color));
	const sf::FloatRect b = t.getLocalBounds();
	float fx = m_fOX + S(x);
	if (align == 1) fx -= (b.position.x + b.size.x) / 2.f;
	else if (align == 2) fx -= (b.position.x + b.size.x);
	const float fy = m_fOY + S(yCenter) - _CapCenter(px, bold);
	t.setPosition({std::round(fx), std::round(fy)});
	w.draw(t);
}

void CSettingsMenu::_Wrap(const std::string & s, float size, float width, std::vector<std::string> & lines)
{
	lines.clear();
	std::string line, word;
	size_t i = 0;
	while (i <= s.size()) {
		const char c = (i < s.size()) ? s[i] : ' ';
		if (c == ' ' || c == '\n') {
			if (!word.empty()) {
				const std::string candidate = line.empty() ? word : line + " " + word;
				if (!line.empty() && _TextWidth(candidate, size) > width) {
					lines.push_back(line);
					line = word;
				}
				else line = candidate;
				word.clear();
			}
			if (c == '\n') { lines.push_back(line); line.clear(); }
		}
		else word += c;
		i++;
	}
	if (!line.empty()) lines.push_back(line);
}

//////////////////////////////////////////////////////////////////////
// Shape helpers
//////////////////////////////////////////////////////////////////////

bool CSettingsMenu::_Stone(sf::RenderWindow & w, const Rect & r, int iKind)
{
	// the same pictures and carved edges as the game's windows (CRenderer::StoneBox), 9-sliced
	static const struct { const char * file; int mx, my; float k; } kStone[3] = {
		{ "CONTENTS\\ui\\panel.png", 24, 24, 0.75f }, { "CONTENTS\\ui\\button.png", 16, 9, 0.7f },
		{ "CONTENTS\\ui\\button_hover.png", 16, 9, 0.7f } };
	static sf::Texture tex[3];
	static int state[3] = { 0, 0, 0 };	// 0 not tried, 1 loaded, -1 missing
	if (iKind < 0 || iKind > 2) return false;
	if (state[iKind] == 0) {
		state[iKind] = tex[iKind].loadFromFile(kStone[iKind].file) ? 1 : -1;
		if (state[iKind] == 1) tex[iKind].setSmooth(true);
	}
	if (state[iKind] != 1) return false;
	const float x0 = std::round(m_fOX + S(r.x)), y0 = std::round(m_fOY + S(r.y));
	const float x1 = std::round(m_fOX + S(r.x + r.w)), y1 = std::round(m_fOY + S(r.y + r.h));
	if (x1 <= x0 || y1 <= y0) return true;
	const int tw = (int)tex[iKind].getSize().x, th = (int)tex[iKind].getSize().y;
	const int mx = kStone[iKind].mx, my = kStone[iKind].my;
	float ex = S(mx * kStone[iKind].k * 0.5f) * 2.f, ey = S(my * kStone[iKind].k * 0.5f) * 2.f;
	ex = std::min(ex, (x1 - x0) / 2.f); ey = std::min(ey, (y1 - y0) / 2.f);
	const int sxs[4] = { 0, mx, tw - mx, tw }, sys[4] = { 0, my, th - my, th };
	const float dxs[4] = { x0, x0 + ex, x1 - ex, x1 }, dys[4] = { y0, y0 + ey, y1 - ey, y1 };
	for (int row = 0; row < 3; row++) for (int col = 0; col < 3; col++) {
		const int sw = sxs[col + 1] - sxs[col], sh = sys[row + 1] - sys[row];
		const float dw = dxs[col + 1] - dxs[col], dh = dys[row + 1] - dys[row];
		if (sw <= 0 || sh <= 0 || dw <= 0.f || dh <= 0.f) continue;
		sf::Sprite piece(tex[iKind], sf::IntRect({ sxs[col], sys[row] }, { sw, sh }));
		piece.setPosition({ dxs[col], dys[row] });
		piece.setScale({ dw / sw, dh / sh });
		w.draw(piece);
	}
	return true;
}

void CSettingsMenu::_Rect(sf::RenderWindow & w, const Rect & r, unsigned int fill, unsigned int outline, float outlineWidth, float radius)
{
	const float x0 = std::round(m_fOX + S(r.x)), y0 = std::round(m_fOY + S(r.y));
	const float x1 = std::round(m_fOX + S(r.x + r.w)), y1 = std::round(m_fOY + S(r.y + r.h));
	const float fw = x1 - x0, fh = y1 - y0;
	if (fw <= 0.f || fh <= 0.f) return;
	const float t = (outline != 0 && outlineWidth > 0.f) ? std::max(1.f, std::round(S(outlineWidth))) : 0.f;

	float rad = std::min(S(radius), std::min(fw, fh) / 2.f);
	if (rad < 1.f) {
		sf::RectangleShape shape({fw, fh});
		shape.setPosition({x0, y0});
		shape.setFillColor(sf::Color(fill));
		if (t > 0.f) { shape.setOutlineThickness(-t); shape.setOutlineColor(sf::Color(outline)); }
		w.draw(shape);
		return;
	}

	const int kCornerPts = 6;
	sf::ConvexShape shape;
	shape.setPointCount(kCornerPts * 4);
	const float cx[4] = { x1 - rad, x0 + rad, x0 + rad, x1 - rad };
	const float cy[4] = { y1 - rad, y1 - rad, y0 + rad, y0 + rad };
	const float kPi = 3.14159265f;
	for (int c = 0; c < 4; c++)
		for (int i = 0; i < kCornerPts; i++) {
			const float a = (c * 90.f + i * 90.f / (kCornerPts - 1)) * kPi / 180.f;
			shape.setPoint(c * kCornerPts + i, {cx[c] + std::cos(a) * rad, cy[c] + std::sin(a) * rad});
		}
	shape.setFillColor(sf::Color(fill));
	if (t > 0.f) { shape.setOutlineThickness(-t); shape.setOutlineColor(sf::Color(outline)); }
	w.draw(shape);
}

void CSettingsMenu::_DrawCursor(sf::RenderWindow & w, float x, float y)
{
	// The game's own cursor when it is available, otherwise a plain arrow.
	if (m_pRenderer != NULL && m_pRenderer->HasUiCursor()) {
		m_pRenderer->DrawUiCursor(w, (int)x, (int)y);
		return;
	}

	const float k = std::max(1.f, m_fScale);
	static const float pts[7][2] = { {0,0}, {0,17}, {4.5f,13}, {7.5f,20}, {10.5f,18.7f}, {7.5f,12}, {12.5f,12} };
	sf::ConvexShape arrow;
	arrow.setPointCount(7);
	for (int i = 0; i < 7; i++) arrow.setPoint(i, {x + pts[i][0] * k, y + pts[i][1] * k});
	arrow.setFillColor(sf::Color::White);
	arrow.setOutlineColor(sf::Color::Black);
	arrow.setOutlineThickness(std::max(1.f, std::round(k)));
	w.draw(arrow);
}

//////////////////////////////////////////////////////////////////////
// Layout
//////////////////////////////////////////////////////////////////////

float CSettingsMenu::_SectionTitleH(const Section & sec)
{
	return sec.title.empty() ? 6.f : kSectionTitleH;
}

// One option per row, full width, under the tab's name; sections get a small title with a rule.
void CSettingsMenu::_Layout(float fContentWidth)
{
	m_layout.clear();
	m_fContentHeight = 0.f;
	if (m_iTab < 0 || m_iTab >= (int)m_tabs.size()) return;
	Tab & tab = m_tabs[m_iTab];

	const float rowX = kPad - 10.f;
	const float rowW = fContentWidth - kPad * 2.f + 20.f - 6.f;	// room for the scroll bar
	float y = kHeadingH;

	for (size_t s = 0; s < tab.sections.size(); s++) {
		Section & sec = tab.sections[s];
		if (sec.items.empty()) continue;
		y += _SectionTitleH(sec);
		for (size_t i = 0; i < sec.items.size(); i++) {
			Item & item = sec.items[i];
			ItemLayout il;
			il.pItem = &item;
			il.cell = { rowX, y, rowW, kRowH };
			y += kRowH + kRowGap;

			const float right = il.cell.x + il.cell.w - 10.f;
			const float cy = il.cell.y + il.cell.h / 2.f;
			const float labelW = _TextWidth(NoColon(item.label), kLabelSize);
			const float maxCtl = il.cell.w - labelW - 50.f;		// what the label leaves free
			switch (item.type) {
			case ITEM_TOGGLE:
				il.control = { right - 40.f, cy - 11.f, 40.f, 22.f };
				break;
			case ITEM_CHOICE: {
				float segW = 0.f;
				for (size_t c = 0; c < item.choices.size(); c++)
					segW = std::max(segW, _TextWidth(item.choices[c], kSmallSize + 1.f, true) + 24.f);
				const float n = (float)std::max<size_t>(1, item.choices.size());
				const float maxTotal = std::min(il.cell.w * 0.62f, maxCtl) - 4.f;
				if (segW * n > maxTotal) segW = maxTotal / n;
				il.control = { right - segW * n - 4.f, cy - 14.f, segW * n + 4.f, 28.f };
				for (size_t c = 0; c < item.choices.size(); c++)
					il.segments.push_back({ il.control.x + 2.f + segW * c, il.control.y, segW, il.control.h });
				break; }
			case ITEM_SLIDER: {
				const float valueW = 48.f;
				const float trackW = std::min(220.f, std::max(80.f, maxCtl - valueW - 20.f));
				il.track = { right - valueW - 12.f - trackW, cy - 3.f, trackW, 6.f };
				il.control = { il.track.x - 10.f, cy - 14.f, trackW + 20.f, 28.f };
				break; }
			case ITEM_BUTTON: {
				const std::string cap = item.text ? item.text() : item.label;
				const float bw = std::min(std::max(60.f, maxCtl), _TextWidth(cap, kLabelSize) + 34.f);
				il.control = { right - bw, cy - 14.f, bw, 28.f };
				break; }
			case ITEM_INFO: {
				const std::string txt = item.text ? item.text() : "";
				const float tw = _TextWidth(txt, kLabelSize);
				il.control = { right - tw, cy - 12.f, tw, 24.f };
				break; }
			case ITEM_DROPDOWN: {
				float tw = _TextWidth(item.disabledText, kLabelSize);
				if (item.multi && item.choices.size() >= 3) {
					const int iAll = (1 << (item.choices.size() - 2)) - 1;
					for (int m = 0; m <= iAll; m++) tw = std::max(tw, _TextWidth(_MultiText(item, m), kLabelSize));
				}
				for (size_t c = 0; c < item.choices.size(); c++)
					tw = std::max(tw, _TextWidth(item.choices[c], kLabelSize) + (item.multi ? 22.f : 0.f));
				const float bw = Clamp(tw + 44.f, 130.f, std::max(130.f, std::min(il.cell.w * 0.5f, maxCtl)));
				il.control = { right - bw, cy - 14.f, bw, 28.f };
				break; }
			}
			m_layout.push_back(il);
		}
		y += kSectionGap;
	}
	m_fContentHeight = y + 6.f;
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

void CSettingsMenu::_SetSliderFromMouse(ItemLayout & il, float mx)
{
	Item & item = *il.pItem;
	if (!item.set) return;
	const float f = Clamp((mx - il.track.x) / il.track.w, 0.f, 1.f);
	const int v = item.minValue + (int)std::lround(f * (item.maxValue - item.minValue));
	if (!item.get || item.get() != v) item.set(v);
}

void CSettingsMenu::_Activate(ItemLayout & il, float mx, float cy)
{
	Item & item = *il.pItem;
	if (!_IsEnabled(item)) return;

	switch (item.type) {
	case ITEM_TOGGLE:
		if (item.set) {
			if (m_clickSound) m_clickSound();
			item.set((item.get && item.get()) ? 0 : 1);
		}
		break;
	case ITEM_CHOICE:
		for (size_t c = 0; c < il.segments.size(); c++)
			if (il.segments[c].Contains(mx, cy)) {
				if (item.set && (!item.get || item.get() != (int)c)) {
					if (m_clickSound) m_clickSound();
					item.set((int)c);
				}
				break;
			}
		break;
	case ITEM_SLIDER:
		if (il.control.Contains(mx, cy)) {
			m_iDragItem = (int)(&il - &m_layout[0]);
			_SetSliderFromMouse(il, mx);
		}
		break;
	case ITEM_BUTTON:
		if (il.control.Contains(mx, cy) && item.action) {
			if (m_clickSound) m_clickSound();
			item.action();		// may close or rebuild the menu: touch nothing afterwards
		}
		break;
	case ITEM_INFO:
		break;
	case ITEM_DROPDOWN:
		if (il.control.Contains(mx, cy)) {
			const int idx = (int)(&il - &m_layout[0]);
			if (m_clickSound) m_clickSound();
			m_iOpenDropdown = (m_iOpenDropdown == idx) ? -1 : idx;
		}
		break;
	}
}

void CSettingsMenu::_DropdownRows(const ItemLayout & il, std::vector<Rect> & rows) const
{
	rows.clear();
	const float rowH = 26.f;
	const size_t n = il.pItem->choices.size();
	const float contentTop = kTitleH, contentBottom = kH - kFooterH;
	// control position in panel coordinates
	const float x = kSideW + il.control.x;
	const float yTop = kTitleH + il.control.y - m_fScroll;
	float y = yTop + il.control.h + 2.f;
	if (y + n * rowH > contentBottom && yTop - 2.f - n * rowH >= contentTop) y = yTop - 2.f - n * rowH;	// open upwards
	for (size_t i = 0; i < n; i++)
		rows.push_back({ x, y + i * rowH, il.control.w, rowH });
}

// Side buttons sit at the bottom of the tab column, last one lowest.
void CSettingsMenu::_SideButtonRect(size_t k, Rect & r) const
{
	const size_t n = m_sideButtons.size();
	const float y = kH - 16.f - (n - k) * (kSideBtnH + kSideBtnGap) + kSideBtnGap;
	r = { 14.f, y, kSideW - 28.f, kSideBtnH };
}

void CSettingsMenu::_HandleInput(float mx, float my, bool bLeftDown, bool bClicked, int iWheel)
{
	m_pHoverHelp = NULL;
	m_hoverLabel.clear();
	const Rect content = { kSideW, kTitleH, kW - kSideW, kH - kTitleH - kFooterH };
	const bool bInContent = content.Contains(mx, my);
	const float lx = mx - content.x;
	const float ly = my - content.y + m_fScroll;

	// Dragging a slider keeps going even outside its row.
	if (m_iDragItem >= 0 && m_iDragItem < (int)m_layout.size()) {
		if (bLeftDown) {
			_SetSliderFromMouse(m_layout[m_iDragItem], lx);
			m_pHoverHelp = &m_layout[m_iDragItem].pItem->help;
			m_hoverLabel = NoColon(m_layout[m_iDragItem].pItem->label);
			return;
		}
		m_iDragItem = -1;
	}
	else m_iDragItem = -1;

	// An open drop-down list takes every click: pick an entry, or click anywhere else to close it.
	if (m_iOpenDropdown >= 0) {
		if (m_iOpenDropdown >= (int)m_layout.size() || !_IsEnabled(*m_layout[m_iOpenDropdown].pItem)) { m_iOpenDropdown = -1; }
		else {
			ItemLayout & il = m_layout[m_iOpenDropdown];
			std::vector<Rect> rows;
			_DropdownRows(il, rows);
			m_pHoverHelp = &il.pItem->help;
			m_hoverLabel = NoColon(il.pItem->label);
			if (bClicked) {
				bool bKeepOpen = false;
				for (size_t i = 0; i < rows.size(); i++)
					if (rows[i].Contains(mx, my)) {
						Item & it = *il.pItem;
						if (it.multi) {
							// none / all close the list; the flags in between tick on and off
							const int n = (int)it.choices.size();
							const int iAll = (1 << (n - 2)) - 1;
							const int iOld = it.get ? it.get() : 0;
							int iNew = iOld;
							if (i == 0) iNew = 0;
							else if ((int)i == n - 1) iNew = iAll;
							else { iNew = iOld ^ (1 << (i - 1)); bKeepOpen = true; }
							if (it.set && iNew != iOld) { if (m_clickSound) m_clickSound(); it.set(iNew); }
						}
						else if (it.set && (!it.get || it.get() != (int)i)) {
							if (m_clickSound) m_clickSound();
							it.set((int)i);
						}
						break;
					}
				if (!bKeepOpen) m_iOpenDropdown = -1;
			}
			return;
		}
	}

	if (iWheel != 0 && bInContent) {
		m_fScroll -= (iWheel / 120.f) * 40.f;
		m_fScroll = Clamp(m_fScroll, 0.f, std::max(0.f, m_fContentHeight - content.h));
	}

	// Close button
	const Rect closeBtn = { kW - 40.f, 9.f, 28.f, 28.f };
	if (closeBtn.Contains(mx, my)) {
		if (bClicked) { if (m_clickSound) m_clickSound(); Close(); }
		return;
	}

	// Title bar (dragging it is handled in Draw)
	const Rect titleBar = { 0.f, 0.f, kW, kTitleH };
	if (titleBar.Contains(mx, my)) {
		m_pHoverHelp = &kMoveHelp;
		return;
	}

	// Tabs
	for (size_t i = 0; i < m_tabs.size(); i++) {
		const Rect r = { 10.f, kTitleH + kTabsTop + i * kTabH, kSideW - 20.f, kTabH - 4.f };
		if (r.Contains(mx, my)) {
			if (bClicked && (int)i != m_iTab) { if (m_clickSound) m_clickSound(); SelectTab((int)i); }
			return;
		}
	}

	// Side buttons (greyed out ones show their help but do nothing)
	for (size_t k = 0; k < m_sideButtons.size(); k++) {
		Rect r;
		_SideButtonRect(k, r);
		if (r.Contains(mx, my)) {
			SideButton & b = m_sideButtons[k];
			if (!b.help.empty()) m_pHoverHelp = &b.help;
			m_hoverLabel = b.label ? b.label() : "";
			const bool bOn = !b.visible || b.visible();
			if (bClicked && bOn && b.onClick) {
				if (m_clickSound) m_clickSound();
				b.onClick();	// may close the menu
			}
			return;
		}
	}

	// Settings
	if (!bInContent) return;
	for (size_t i = 0; i < m_layout.size(); i++) {
		ItemLayout & il = m_layout[i];
		if (!il.cell.Contains(lx, ly)) continue;
		m_pHoverHelp = &il.pItem->help;
		m_hoverLabel = NoColon(il.pItem->label);
		if (bClicked) _Activate(il, lx, ly);
		return;
	}
}

//////////////////////////////////////////////////////////////////////
// Drawing
//////////////////////////////////////////////////////////////////////

sf::View CSettingsMenu::_ClipTo(sf::RenderWindow & w, const Rect & r)
{
	const sf::View old = w.getView();
	const sf::Vector2u ws = w.getSize();
	const float px = std::round(m_fOX + S(r.x)), py = std::round(m_fOY + S(r.y));
	const float pw = std::round(S(r.w)), ph = std::round(S(r.h));
	sf::View clip(sf::FloatRect({px, py}, {pw, ph}));
	clip.setViewport(sf::FloatRect({px / ws.x, py / ws.y}, {pw / ws.x, ph / ws.y}));
	w.setView(clip);
	return old;
}

// Small line icons for the tabs, picked by the tab's name.
void CSettingsMenu::_DrawTabIcon(sf::RenderWindow & w, const std::string & name, float cx, float cy, unsigned int color)
{
	const std::string n = Lower(name);
	const sf::Color c(color);
	const float t = std::max(1.f, std::round(S(1.8f)));
	auto P = [&](float x, float y) { return sf::Vector2f(std::round(m_fOX + S(cx + x)), std::round(m_fOY + S(cy + y))); };
	auto Line = [&](float x0, float y0, float x1, float y1) {
		const sf::Vector2f a = P(x0, y0), b = P(x1, y1);
		const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
		sf::RectangleShape bar({len, t});
		bar.setOrigin({0.f, t / 2.f});
		bar.setPosition(a);
		bar.setRotation(sf::radians(std::atan2(dy, dx)));
		bar.setFillColor(c);
		w.draw(bar);
	};
	if (n.find("general") != std::string::npos) {
		sf::CircleShape ring(S(6.f));
		ring.setOrigin({S(6.f), S(6.f)});
		ring.setPosition(P(0, 0));
		ring.setFillColor(sf::Color::Transparent);
		ring.setOutlineThickness(-t);
		ring.setOutlineColor(c);
		w.draw(ring);
		sf::CircleShape dot(S(2.2f));
		dot.setOrigin({S(2.2f), S(2.2f)});
		dot.setPosition(P(0, 0));
		dot.setFillColor(c);
		w.draw(dot);
	}
	else if (n.find("graphic") != std::string::npos || n.find("video") != std::string::npos) {
		_Rect(w, { cx - 7.f, cy - 6.f, 14.f, 10.f }, 0, color, 1.8f, 2.f);
		_Rect(w, { cx - 3.f, cy + 5.5f, 6.f, 2.f }, color);
	}
	else if (n.find("interface") != std::string::npos) {
		_Rect(w, { cx - 7.f, cy - 7.f, 14.f, 14.f }, 0, color, 1.8f, 2.f);
		_Rect(w, { cx - 7.f, cy - 7.f, 14.f, 4.5f }, color, 0, 0, 1.f);
	}
	else if (n.find("gameplay") != std::string::npos) {
		Line(-6.f, 6.f, 6.f, -6.f);
		Line(1.5f, -6.f, 6.f, -6.f);
		Line(6.f, -6.f, 6.f, -1.5f);
	}
	else if (n.find("chat") != std::string::npos) {
		_Rect(w, { cx - 7.f, cy - 6.f, 14.f, 9.f }, 0, color, 1.8f, 2.f);
		sf::ConvexShape tail(3);
		tail.setPoint(0, P(-4.f, 2.5f));
		tail.setPoint(1, P(0.f, 2.5f));
		tail.setPoint(2, P(-5.f, 7.f));
		tail.setFillColor(c);
		w.draw(tail);
	}
	else if (n.find("audio") != std::string::npos || n.find("sound") != std::string::npos) {
		sf::ConvexShape spk(4);
		spk.setPoint(0, P(-7.f, -2.5f));
		spk.setPoint(1, P(-2.f, -6.5f));
		spk.setPoint(2, P(-2.f, 6.5f));
		spk.setPoint(3, P(-7.f, 2.5f));
		spk.setFillColor(c);
		w.draw(spk);
		Line(2.f, -3.5f, 5.f, -1.f);
		Line(5.f, -1.f, 5.f, 1.f);
		Line(5.f, 1.f, 2.f, 3.5f);
	}
	else {
		sf::CircleShape dot(S(3.f));
		dot.setOrigin({S(3.f), S(3.f)});
		dot.setPosition(P(0, 0));
		dot.setFillColor(c);
		w.draw(dot);
	}
}

void CSettingsMenu::_DrawItem(sf::RenderWindow & w, const ItemLayout & il, bool bHover)
{
	const Item & item = *il.pItem;
	const bool bEnabled = _IsEnabled(item);
	const float cy = il.cell.y + il.cell.h / 2.f;

	if (bHover && bEnabled) _Rect(w, il.cell, kRowHover, kRowHoverEdge, 1.f, 7.f);
	_Text(w, NoColon(item.label), il.cell.x + 10.f, cy, kLabelSize, bEnabled ? kText : kTextOff);

	if (!bEnabled && !item.disabledText.empty() && item.type != ITEM_DROPDOWN) {
		_Text(w, item.disabledText, il.cell.x + il.cell.w - 10.f, cy, kLabelSize, kTextOff, 2);
		return;
	}

	switch (item.type) {
	case ITEM_TOGGLE: {
		// a switch: gold with the knob on the right when on
		const bool bOn = item.get && item.get() != 0;
		const Rect & r = il.control;
		if (bOn) _Rect(w, r, bEnabled ? kGold : kTextOff, 0, 0, r.h / 2.f);
		else     _Rect(w, r, kSwitchOff, kCtlEdge, 1.f, r.h / 2.f);
		const float k = r.h - 6.f;
		const Rect knob = { bOn ? r.x + r.w - 3.f - k : r.x + 3.f, r.y + 3.f, k, k };
		_Rect(w, knob, bOn ? kOnGold : (bEnabled ? kKnobOff : kTextOff), 0, 0, k / 2.f);
		break; }

	case ITEM_CHOICE: {
		// segmented buttons: the chosen one filled gold
		const int sel = item.get ? item.get() : -1;
		_Rect(w, il.control, kCtl, kCtlEdge, 1.f, 7.f);
		for (size_t c = 0; c < il.segments.size(); c++) {
			const Rect & s = il.segments[c];
			const bool bSel = ((int)c == sel);
			if (bSel) _Rect(w, { s.x + 1.f, s.y + 3.f, s.w - 2.f, s.h - 6.f }, bEnabled ? kGold : kTextOff, 0, 0, 5.f);
			unsigned int col = bSel ? kOnGold : kTextDim;
			if (!bEnabled && !bSel) col = kTextOff;
			_Text(w, item.choices[c], s.x + s.w / 2.f, s.y + s.h / 2.f, kSmallSize + 1.f, col, 1, bSel);
		}
		break; }

	case ITEM_SLIDER: {
		const int v = item.get ? item.get() : item.minValue;
		const float f = Clamp((float)(v - item.minValue) / (item.maxValue - item.minValue), 0.f, 1.f);
		const Rect & t = il.track;
		_Rect(w, t, kTrack, 0, 0, 3.f);
		_Rect(w, { t.x, t.y, t.w * f, t.h }, bEnabled ? kGold : kTextOff, 0, 0, 3.f);
		_Rect(w, { t.x + t.w * f - 8.f, cy - 8.f, 16.f, 16.f }, bEnabled ? kGold : kTextOff, kOnGold, 2.f, 8.f);
		char buf[32];
		snprintf(buf, sizeof(buf), "%d%s", v, item.suffix.c_str());
		_Text(w, buf, il.cell.x + il.cell.w - 10.f, cy, kLabelSize, bEnabled ? kWhite : kTextOff, 2);
		break; }

	case ITEM_BUTTON: {
		const std::string cap = item.text ? item.text() : item.label;
		if (!bEnabled || !_Stone(w, il.control, (bHover && bEnabled) ? 2 : 1))
			_Rect(w, il.control, (bHover && bEnabled) ? kButtonHover : kButton, bEnabled ? kButtonEdge : kCtlEdge, 1.f, 7.f);
		_Text(w, cap, il.control.x + il.control.w / 2.f, cy, kLabelSize, bEnabled ? (bHover ? 0xFFF2C7FF : 0xF5DB9EFF) : kTextOff, 1);
		break; }

	case ITEM_INFO: {
		const std::string txt = item.text ? item.text() : "";
		_Text(w, txt, il.cell.x + il.cell.w - 10.f, cy, kLabelSize, bEnabled ? kWhite : kTextOff, 2);
		break; }

	case ITEM_DROPDOWN: {
		const bool bOpen = (m_iOpenDropdown >= 0 && m_iOpenDropdown < (int)m_layout.size() && &m_layout[m_iOpenDropdown] == &il);
		std::string txt;
		if (!bEnabled && !item.disabledText.empty()) txt = item.disabledText;
		else {
			const int sel = item.get ? item.get() : -1;
			if (item.multi) txt = _MultiText(item, sel);
			else if (sel >= 0 && sel < (int)item.choices.size()) txt = item.choices[sel];
		}
		const Rect & r = il.control;
		_Rect(w, r, (bEnabled && (bHover || bOpen)) ? kCtlHover : kCtl, (bEnabled && bOpen) ? kGold : kCtlEdge, 1.f, 7.f);
		_Text(w, txt, r.x + 11.f, cy, kLabelSize, bEnabled ? kWhite : kTextOff);
		// arrow
		sf::ConvexShape tri;
		tri.setPointCount(3);
		const float ax = m_fOX + S(r.x + r.w - 16.f), ay = m_fOY + S(cy);
		const float aw = S(4.5f), ah = S(3.f);
		if (bOpen) { tri.setPoint(0, {ax - aw, ay + ah}); tri.setPoint(1, {ax + aw, ay + ah}); tri.setPoint(2, {ax, ay - ah}); }
		else       { tri.setPoint(0, {ax - aw, ay - ah}); tri.setPoint(1, {ax + aw, ay - ah}); tri.setPoint(2, {ax, ay + ah}); }
		tri.setFillColor(sf::Color(bEnabled ? kGold : kTextOff));
		w.draw(tri);
		break; }
	}
}

void CSettingsMenu::_DrawDropdownList(sf::RenderWindow & w, float mx, float my)
{
	if (m_iOpenDropdown < 0 || m_iOpenDropdown >= (int)m_layout.size()) return;
	const ItemLayout & il = m_layout[m_iOpenDropdown];
	std::vector<Rect> rows;
	_DropdownRows(il, rows);
	if (rows.empty()) return;
	const Rect box = { rows.front().x, rows.front().y, rows.front().w, rows.back().y + rows.back().h - rows.front().y };
	_Rect(w, { box.x + 2.f, box.y + 5.f, box.w, box.h }, kShadow, 0, 0, 7.f);		// shadow
	_Rect(w, box, kList, 0xFFFFFF26, 1.f, 7.f);
	const int sel = il.pItem->get ? il.pItem->get() : -1;
	const bool bMulti = il.pItem->multi;
	const int n = (int)rows.size();
	const int iAll = bMulti ? (1 << (n - 2)) - 1 : 0;
	for (int i = 0; i < n; i++) {
		const Rect & r = rows[i];
		const bool bHover = r.Contains(mx, my);
		if (bHover) _Rect(w, { r.x + 3.f, r.y + 1.f, r.w - 6.f, r.h - 2.f }, kListHover, 0, 0, 5.f);
		bool bOn;
		float tx = r.x + 11.f;
		if (bMulti) {
			if (i == 0) bOn = ((sel & iAll) == 0);
			else if (i == n - 1) bOn = ((sel & iAll) == iAll);
			else {
				bOn = (sel & (1 << (i - 1))) != 0;
				// tick box
				const Rect cb = { r.x + 11.f, r.y + r.h / 2.f - 6.f, 12.f, 12.f };
				_Rect(w, cb, bOn ? kGold : 0x00000000, bOn ? kGold : kCtlEdge, 1.f, 3.f);
				if (bOn) _Rect(w, { cb.x + 3.f, cb.y + 3.f, 6.f, 6.f }, kOnGold, 0, 0, 1.f);
			}
			tx += 22.f;
		}
		else bOn = (i == sel);
		_Text(w, il.pItem->choices[i], tx, r.y + r.h / 2.f, kLabelSize, bOn ? kGold : (bHover ? kWhite : kText), 0, bOn);
	}
}

void CSettingsMenu::_PlacePanel(unsigned int iWinW, unsigned int iWinH)
{
	// Keep the whole panel inside the window; the position is remembered while the game runs.
	const float fFreeX = std::max(0.f, (float)iWinW - S(kW)) / 2.f;
	const float fFreeY = std::max(0.f, (float)iWinH - S(kH)) / 2.f;
	m_fPanX = Clamp(m_fPanX, -fFreeX, fFreeX);
	m_fPanY = Clamp(m_fPanY, -fFreeY, fFreeY);
	m_fOX = std::round((iWinW - S(kW)) / 2.f + m_fPanX);
	m_fOY = std::round((iWinH - S(kH)) / 2.f + m_fPanY);
}

void CSettingsMenu::Draw(sf::RenderWindow & window)
{
	if (!m_bOpen) return;
	if (m_canShow && !m_canShow()) { Close(); return; }
	if (m_pFont == NULL && m_pRenderer != NULL) m_pFont = m_pRenderer->GetFont();

	const sf::Vector2u ws = window.getSize();
	if (ws.x == 0 || ws.y == 0) return;
	const sf::View uiView = window.getView();

	// Size the panel for the window: big enough to read, small enough to leave room to move it.
	const float fOldScale = m_fScale;
	const float fFit = std::min(ws.x * 0.96f / kW, ws.y * 0.96f / kH);
	m_fScale = Clamp(std::min(fFit, std::max(0.7f, ws.y / 880.f)), 0.5f, 2.5f);
	if (m_fScale != fOldScale) { m_capCenter[0].clear(); m_capCenter[1].clear(); }
	_PlacePanel(ws.x, ws.y);

	// Mouse
	int iWinX = 0, iWinY = 0, iWheel = 0;
	bool bDown = false, bClicked = false;
	if (m_pMouse != NULL) m_pMouse->GetUiState(&iWinX, &iWinY, &bDown, &bClicked, &iWheel);

	// Moving the panel: press on the title bar (not the close button) and drag.
	if (m_bMovingPanel) {
		if (bDown) {
			m_fPanX = m_fMoveStartPanX + (iWinX - m_iMoveStartX);
			m_fPanY = m_fMoveStartPanY + (iWinY - m_iMoveStartY);
			_PlacePanel(ws.x, ws.y);
		}
		else m_bMovingPanel = false;
		bClicked = false;
	}
	else if (bClicked) {
		const float px = (iWinX - m_fOX) / m_fScale, py = (iWinY - m_fOY) / m_fScale;
		const Rect titleBar = { 0.f, 0.f, kW, kTitleH };
		const Rect closeBtn = { kW - 40.f, 9.f, 28.f, 28.f };
		if (titleBar.Contains(px, py) && !closeBtn.Contains(px, py)) {
			m_bMovingPanel = true;
			m_iMoveStartX = iWinX;
			m_iMoveStartY = iWinY;
			m_fMoveStartPanX = m_fPanX;
			m_fMoveStartPanY = m_fPanY;
			bClicked = false;
		}
	}

	// Mouse, in panel units
	const float mx = (iWinX - m_fOX) / m_fScale;
	const float my = (iWinY - m_fOY) / m_fScale;

	const Rect content = { kSideW, kTitleH, kW - kSideW, kH - kTitleH - kFooterH };

	if (m_pFont != NULL) {
		_Layout(content.w);
		m_fScroll = Clamp(m_fScroll, 0.f, std::max(0.f, m_fContentHeight - content.h));
		if (m_bMovingPanel) { m_pHoverHelp = &kMoveHelp; m_hoverLabel.clear(); }
		else _HandleInput(mx, my, bDown, bClicked, iWheel);
		if (!m_bOpen) return;					// closed by a click
		_Layout(content.w);						// tab, values or captions may have changed
		m_fScroll = Clamp(m_fScroll, 0.f, std::max(0.f, m_fContentHeight - content.h));
	}

	// Dim the game a little, then the panel with a soft shadow
	{
		sf::RectangleShape dim({(float)ws.x, (float)ws.y});
		dim.setFillColor(sf::Color(kDim));
		window.draw(dim);
	}
	_Rect(window, { 4.f, 10.f, kW, kH }, kShadow, 0, 0, kRadius);
	if (!_Stone(window, { 0.f, 0.f, kW, kH }, 0)) _Rect(window, { 0.f, 0.f, kW, kH }, kPanel, kPanelEdge, 1.f, kRadius);
	if (m_pFont == NULL) { _DrawCursor(window, (float)iWinX, (float)iWinY); return; }

	// Title bar: a faint gold tint, the title on the left, the clock and close button on the right
	{
		const sf::View old = _ClipTo(window, { 1.f, 1.f, kW - 2.f, kTitleH - 1.f });
		_Rect(window, { 1.f, 1.f, kW - 2.f, kTitleH + kRadius * 2.f }, kTitleTint, 0, 0, kRadius - 1.f);
		window.setView(old);
	}
	_Rect(window, { 1.f, kTitleH, kW - 2.f, 1.f }, kLine);
	{	// in the display font when the game has one
		const sf::Font * pDisp = (m_pRenderer != NULL) ? m_pRenderer->GetDisplayFont() : NULL;
		const sf::Font * pWas = m_pFont;
		if (pDisp != NULL) { m_pFont = pDisp; m_bCapFace2 = true; }
		_Text(window, Upper(m_title), 20.f, kTitleH / 2.f, kTitleSize, kGoldTitle, 0, pDisp == NULL);
		m_pFont = pWas;
		m_bCapFace2 = false;
	}
	if (m_cornerText) _Text(window, m_cornerText(), kW - 52.f, kTitleH / 2.f, kSmallSize, kTextDim, 2);
	{
		const Rect closeBtn = { kW - 40.f, 9.f, 28.f, 28.f };
		const bool bHover = closeBtn.Contains(mx, my) && !m_bMovingPanel;
		if (bHover) _Rect(window, closeBtn, 0xFFFFFF1A, 0, 0, 6.f);
		for (int k = 0; k < 2; k++) {
			sf::RectangleShape bar({S(14.f), std::max(1.f, std::round(S(1.8f)))});
			bar.setOrigin({S(7.f), bar.getSize().y / 2.f});
			bar.setPosition({std::round(m_fOX + S(closeBtn.x + closeBtn.w / 2.f)), std::round(m_fOY + S(closeBtn.y + closeBtn.h / 2.f))});
			bar.setRotation(sf::degrees(k == 0 ? 45.f : -45.f));
			bar.setFillColor(sf::Color(bHover ? kWhite : 0x8A929CFF));
			window.draw(bar);
		}
	}

	// Tab column
	{
		const sf::View old = _ClipTo(window, { 1.f, kTitleH + 1.f, kSideW - 1.f, kH - kTitleH - 2.f });
		_Rect(window, { 1.f, kTitleH + 1.f - kRadius, kSideW + kRadius, kH - kTitleH - 2.f + kRadius }, kSidebar, 0, 0, kRadius - 1.f);
		window.setView(old);
	}
	_Rect(window, { kSideW, kTitleH + 1.f, 1.f, kH - kTitleH - 2.f }, kLine);
	for (size_t i = 0; i < m_tabs.size(); i++) {
		const Rect r = { 10.f, kTitleH + kTabsTop + i * kTabH, kSideW - 20.f, kTabH - 4.f };
		const bool bSel = ((int)i == m_iTab);
		const bool bHover = r.Contains(mx, my) && !m_bMovingPanel;
		if (bSel) {
			_Rect(window, r, kTabSel, 0, 0, 7.f);
			_Rect(window, { r.x, r.y + 5.f, 3.f, r.h - 10.f }, kGold, 0, 0, 1.5f);
		}
		else if (bHover) _Rect(window, r, kTabHover, 0, 0, 7.f);
		_DrawTabIcon(window, m_tabs[i].name, r.x + 20.f, r.y + r.h / 2.f, bSel ? kGold : kIcon);
		_Text(window, m_tabs[i].name, r.x + 40.f, r.y + r.h / 2.f, kTabSize,
			bSel ? kWhite : (bHover ? kText : kTextTab), 0, bSel);
	}
	for (size_t k = 0; k < m_sideButtons.size(); k++) {
		Rect r;
		_SideButtonRect(k, r);
		const SideButton & b = m_sideButtons[k];
		const bool bOn = !b.visible || b.visible();
		const bool bHover = bOn && r.Contains(mx, my) && !m_bMovingPanel;
		unsigned int fill, edge, text;
		if (!bOn)          { fill = 0x00000000; edge = 0xFFFFFF14; text = kTextOff; }
		else if (b.danger) { fill = bHover ? kDangerHover : kDanger; edge = kDangerEdge; text = kDangerText; }
		else               { fill = bHover ? kButtonHover : kButton; edge = kButtonEdge; text = kButtonText; }
		if (!bOn || !_Stone(window, r, bHover ? 2 : 1)) _Rect(window, r, fill, edge, 1.f, 7.f);
		else if (b.danger) text = kDangerText;
		else text = bHover ? 0xFFF2C7FF : 0xF5DB9EFF;	// VALORIA's button lettering
		_Text(window, b.label ? b.label() : "", r.x + r.w / 2.f, r.y + r.h / 2.f, kLabelSize, text, 1);
	}

	// Settings (clipped to the content area and scrolled)
	{
		const float px = m_fOX + S(content.x), py = m_fOY + S(content.y);
		const float pw = S(content.w), ph = S(content.h);
		sf::View clip(sf::FloatRect({px, py + S(m_fScroll)}, {pw, ph}));
		clip.setViewport(sf::FloatRect({px / ws.x, py / ws.y}, {pw / ws.x, ph / ws.y}));
		window.setView(clip);

		// Content is laid out relative to the content area; shift the origin for drawing.
		const float fSaveOX = m_fOX, fSaveOY = m_fOY;
		m_fOX = px; m_fOY = py;

		const float lx = mx - content.x, ly = my - content.y + m_fScroll;
		const bool bMouseIn = content.Contains(mx, my) && !m_bMovingPanel && m_iOpenDropdown < 0;
		const Tab * pTab = (m_iTab >= 0 && m_iTab < (int)m_tabs.size()) ? &m_tabs[m_iTab] : NULL;

		if (pTab != NULL) _Text(window, pTab->name, kPad, kHeadingH / 2.f + 2.f, kHeadingSize, kWhite, 0, true);

		if (pTab != NULL && pTab->sections.empty()) {
			_Text(window, pTab->emptyText.empty() ? "Nothing here yet." : pTab->emptyText,
				content.w / 2.f, content.h / 2.f, kLabelSize, kTextDim, 1);
		}
		else if (pTab != NULL) {
			// Section titles with a thin rule after them
			const float ruleRight = content.w - kPad - 6.f;
			size_t idx = 0;
			for (size_t s = 0; s < pTab->sections.size(); s++) {
				const Section & sec = pTab->sections[s];
				if (sec.items.empty()) continue;
				if (!sec.title.empty() && idx < m_layout.size()) {
					const float ty = m_layout[idx].cell.y - kSectionTitleH / 2.f + 2.f;
					const std::string t = Upper(sec.title);
					_Text(window, t, kPad, ty, kSectionSize, kTextDim);
					const float x0 = kPad + _TextWidth(t, kSectionSize) + 10.f;
					if (x0 < ruleRight) _Rect(window, { x0, ty, ruleRight - x0, 1.f }, kLine);
				}
				idx += sec.items.size();
			}
			for (size_t i = 0; i < m_layout.size(); i++) {
				const bool bHover = (m_iDragItem == (int)i) || (bMouseIn && m_iDragItem < 0 && m_layout[i].cell.Contains(lx, ly));
				_DrawItem(window, m_layout[i], bHover);
			}
		}

		m_fOX = fSaveOX; m_fOY = fSaveOY;
		window.setView(uiView);

		// Scroll bar
		if (m_fContentHeight > content.h) {
			const float trackX = kW - 10.f, trackY = content.y + 8.f, trackH = content.h - 16.f;
			const float thumbH = std::max(30.f, trackH * content.h / m_fContentHeight);
			const float thumbY = trackY + (trackH - thumbH) * (m_fScroll / (m_fContentHeight - content.h));
			_Rect(window, { trackX, trackY, 4.f, trackH }, 0xFFFFFF10, 0, 0, 2.f);
			_Rect(window, { trackX, thumbY, 4.f, thumbH }, 0xE8C07080, 0, 0, 2.f);
		}
	}

	// Help strip: the hovered option's name in gold, what it does, and how to close the menu
	{
		const Rect footer = { kSideW + 1.f, kH - kFooterH, kW - kSideW - 2.f, kFooterH - 1.f };
		{
			const sf::View old = _ClipTo(window, footer);
			_Rect(window, { footer.x - kRadius, footer.y, footer.w + kRadius, footer.h }, kFooter, 0, 0, kRadius - 1.f);
			window.setView(old);
		}
		_Rect(window, { footer.x, footer.y, footer.w, 1.f }, kLine);
		const float escW = _TextWidth(kEscHint, kSmallSize);
		_Text(window, kEscHint, footer.x + footer.w - 18.f, footer.y + footer.h / 2.f, kSmallSize, 0x6F7782FF, 2);

		const bool bHelp = (m_pHoverHelp != NULL && !m_pHoverHelp->empty());
		const std::string & help = bHelp ? *m_pHoverHelp : std::string(kDefaultHelp);
		float x = footer.x + 22.f;
		if (bHelp && !m_hoverLabel.empty()) {
			_Text(window, m_hoverLabel, x, footer.y + footer.h / 2.f, kHelpSize, kGold, 0, true);
			x += _TextWidth(m_hoverLabel, kHelpSize, true) + 12.f;
		}
		const float width = footer.x + footer.w - 18.f - escW - 20.f - x;
		std::vector<std::string> lines;
		_Wrap(help, kHelpSize, std::max(60.f, width), lines);
		const size_t nLines = std::min<size_t>(lines.size(), 3);
		const float lineH = (nLines >= 3) ? 15.f : 18.f;
		float y = footer.y + footer.h / 2.f - ((float)nLines - 1.f) * lineH / 2.f;
		for (size_t i = 0; i < nLines; i++, y += lineH)
			_Text(window, lines[i], x, y, kHelpSize, bHelp ? 0xB9C0C9FF : kTextDim);
	}

	_DrawDropdownList(window, mx, my);
	_DrawCursor(window, (float)iWinX, (float)iWinY);
}
