// SettingsMenu.h: the full-resolution system / settings menu.
//
// The menu is a generic list of tabs, sections and settings. It knows nothing about the
// game: every setting reads and writes its value through small callbacks that the game
// supplies (see CGame::BuildSettingsMenu in UI\SettingsMenuSetup.cpp). Adding an option
// is one AddToggle / AddChoice / AddSlider / AddButton call.
//
// It is drawn as a CUiOverlay, i.e. directly into the SFML window at window resolution,
// on top of the scaled-up 640x480 game image. While it is open it takes the mouse
// (CMouse::SetUiCapture) so clicks never reach the game underneath.
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_SETTINGSMENU_H
#define HB_ENGINE_SETTINGSMENU_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "UiOverlay.h"

class CRenderer;
class CMouse;

namespace sf { class Font; class Text; class View; }

class CSettingsMenu : public CUiOverlay
{
public:
	enum ItemType { ITEM_TOGGLE, ITEM_CHOICE, ITEM_SLIDER, ITEM_BUTTON, ITEM_INFO, ITEM_DROPDOWN };

	typedef std::function<int()>			GetFn;
	typedef std::function<void(int)>		SetFn;
	typedef std::function<bool()>			BoolFn;
	typedef std::function<std::string()>	TextFn;
	typedef std::function<void()>			ActionFn;

	struct Item
	{
		ItemType type = ITEM_INFO;
		std::string label;
		std::string help;
		std::vector<std::string> choices;	// ITEM_CHOICE / ITEM_DROPDOWN
		int minValue = 0, maxValue = 100;	// ITEM_SLIDER
		std::string suffix;					// ITEM_SLIDER value suffix, e.g. "%"
		GetFn get;							// current value (toggle: 0/1, choice: index, slider: value)
		SetFn set;							// called with the new value
		ActionFn action;					// ITEM_BUTTON
		TextFn text;						// ITEM_BUTTON caption / ITEM_INFO text
		BoolFn enabled;						// optional: greyed out and inactive when false
		std::string disabledText;			// optional: shown instead of the control when disabled
		bool multi = false;					// ITEM_DROPDOWN: choices are "none", flags..., "all"; value is a bit mask
	};

	struct Section
	{
		std::string title;
		std::vector<Item> items;
	};

	struct Tab
	{
		std::string name;
		std::vector<Section> sections;
		std::string emptyText;				// shown when the tab has no sections yet
	};

	struct SideButton
	{
		TextFn label;
		BoolFn visible;
		ActionFn onClick;
		std::string help;
		bool danger = false;				// drawn in red (e.g. Log Out)
	};

	CSettingsMenu();
	virtual ~CSettingsMenu();

	void Init(CRenderer * pRenderer, CMouse * pMouse);

	void Open();
	void Close();
	void Toggle();
	bool IsOpen() const { return m_bOpen; }

	// Building the menu
	void Clear();
	void SetTitle(const std::string & title) { m_title = title; }
	void SetCornerText(TextFn fn) { m_cornerText = fn; }			// small text at the top left (e.g. a clock)
	void SetCanShow(BoolFn fn) { m_canShow = fn; }					// the menu closes itself when this returns false
	void SetClickSound(ActionFn fn) { m_clickSound = fn; }
	void SetOnClose(ActionFn fn) { m_onClose = fn; }

	int  AddTab(const std::string & name, const std::string & emptyText = "");
	void AddSection(int iTab, const std::string & title);			// following items go into this section
	Item & AddToggle(int iTab, const std::string & label, const std::string & help, GetFn get, SetFn set);
	Item & AddChoice(int iTab, const std::string & label, const std::string & help,
		const std::vector<std::string> & choices, GetFn get, SetFn set);
	Item & AddSlider(int iTab, const std::string & label, const std::string & help,
		int iMin, int iMax, const std::string & suffix, GetFn get, SetFn set);
	Item & AddButton(int iTab, const std::string & label, const std::string & help, TextFn caption, ActionFn action);
	Item & AddInfo(int iTab, const std::string & label, const std::string & help, TextFn text);
	Item & AddDropdown(int iTab, const std::string & label, const std::string & help,
		const std::vector<std::string> & choices, GetFn get, SetFn set);	// when disabled, shows disabledText (if set) in the box
	// Drop-down where several entries can be ticked. choices = { none, flag 1, flag 2, ..., all };
	// the value is a bit mask (flag i is bit i-1). "none" clears every flag, "all" sets every flag.
	Item & AddMultiDropdown(int iTab, const std::string & label, const std::string & help,
		const std::vector<std::string> & choices, GetFn get, SetFn set);
	// Buttons at the bottom of the tab column. 'visible' false greys the button out (it stays in place).
	void AddSideButton(TextFn label, BoolFn visible, ActionFn onClick, const std::string & help = "", bool bDanger = false);

	void SelectTab(int iTab);

	// CUiOverlay
	virtual bool IsVisible() const { return m_bOpen; }
	virtual void Draw(sf::RenderWindow & window);

private:
	struct Rect
	{
		float x = 0, y = 0, w = 0, h = 0;
		bool Contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
	};

	// Where each part of an item ended up this frame (in content coordinates, before scrolling).
	struct ItemLayout
	{
		Item * pItem = NULL;
		Rect cell;					// the whole row cell
		Rect control;				// the clickable control
		std::vector<Rect> segments;	// ITEM_CHOICE
		Rect track;					// ITEM_SLIDER
	};

	Item & _AddItem(int iTab, ItemType type, const std::string & label, const std::string & help);
	bool _IsEnabled(const Item & item) const;
	void _Layout(float fContentWidth);
	void _HandleInput(float mx, float my, bool bLeftDown, bool bClicked, int iWheel);
	void _PlacePanel(unsigned int iWinW, unsigned int iWinH);
	void _DropdownRows(const ItemLayout & il, std::vector<Rect> & rows) const;	// panel coordinates
	void _DrawDropdownList(sf::RenderWindow & w, float mx, float my);
	static std::string _MultiText(const Item & item, int iMask);
	void _Activate(ItemLayout & il, float mx, float cy);
	void _SetSliderFromMouse(ItemLayout & il, float mx);

	// Drawing helpers (all positions and sizes in logical units, scaled by m_fScale)
	float S(float v) const { return v * m_fScale; }
	unsigned int Px(float size) const;
	void _Rect(sf::RenderWindow & w, const Rect & r, unsigned int fill, unsigned int outline = 0, float outlineWidth = 0, float radius = 0);
	// VALORIA's stone (CONTENTS\ui): 0 panel, 1 button, 2 button under the mouse; false without the picture
	bool _Stone(sf::RenderWindow & w, const Rect & r, int iKind);
	void _Text(sf::RenderWindow & w, const std::string & s, float x, float yCenter, float size, unsigned int color, int align = 0, bool bold = false);
	float _TextWidth(const std::string & s, float size, bool bold = false);
	float _CapCenter(unsigned int px, bool bold);
	void _Wrap(const std::string & s, float size, float width, std::vector<std::string> & lines);
	void _DrawItem(sf::RenderWindow & w, const ItemLayout & il, bool bHover);
	void _DrawTabIcon(sf::RenderWindow & w, const std::string & name, float cx, float cy, unsigned int color);
	sf::View _ClipTo(sf::RenderWindow & w, const Rect & r);	// limits drawing to r (panel units); returns the old view
	void _SideButtonRect(size_t k, Rect & r) const;
	static float _SectionTitleH(const Section & sec);
	void _DrawCursor(sf::RenderWindow & w, float x, float y);

	CRenderer *	m_pRenderer;
	CMouse *	m_pMouse;
	const sf::Font * m_pFont;

	bool m_bOpen;
	std::string m_title;
	std::vector<Tab> m_tabs;
	std::vector<SideButton> m_sideButtons;
	TextFn m_cornerText;
	BoolFn m_canShow;
	ActionFn m_clickSound;
	ActionFn m_onClose;

	int m_iTab;
	float m_fScale;
	float m_fOX, m_fOY;			// window position of the panel's top-left corner (pixels)
	float m_fPanX, m_fPanY;		// how far the player has moved the panel from the centre (pixels)
	bool  m_bMovingPanel;		// dragging the panel by its title bar
	int   m_iMoveStartX, m_iMoveStartY;
	float m_fMoveStartPanX, m_fMoveStartPanY;
	float m_fScroll;			// content scroll offset (logical units)
	float m_fContentHeight;		// laid out content height (logical units)

	// Interaction state
	std::vector<ItemLayout> m_layout;
	int m_iDragItem;			// index into m_layout of the slider being dragged, or -1
	int m_iOpenDropdown;		// index into m_layout of the open drop-down list, or -1
	const std::string * m_pHoverHelp;
	std::string m_hoverLabel;	// name of the hovered option, shown in bold before its help text
	std::map<unsigned int, float> m_capCenter[3];	// UI font, bold, the display font
	bool m_bCapFace2 = false;
};

#endif // HB_ENGINE_SETTINGSMENU_H
