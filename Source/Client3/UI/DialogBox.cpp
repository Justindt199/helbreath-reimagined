#include "DialogBox.h"
#include "..\Engine\Renderer.h"

#include <algorithm>

//extern class CGame *   g_game;

CDialogBox::CDialogBox(void)
{
}

CDialogBox::~CDialogBox(void)
{
}

void CDialogBox::SetupDialog(char dialogID, short X, short Y, short sizeX, short sizeY, short background, int backFrame, int titleTxtFrame, bool trans)
{
	m_X = X;
	m_Y = Y;
	sSizeX = sizeX;
	sSizeY = sizeY; 

	_dialogID  = dialogID;
	_clickHandler = NULL;
	_drawHandler = NULL;
	_dblClickHandler = NULL;
	_dropItemHandler = NULL;

	_buttonCount = 0;
	_scrollBar = 0;
	_isTopDialog = FALSE;

	_background = background;
	_backgroundFrame = backFrame;
	_titleTextFrame = titleTxtFrame;
	_trans = trans;

	sV1= sV2= sV3= sV4= sV5= sV6= sV7= sV8 = 0;
	dwV1= dwV2= dwT1= 0;
	bFlag= 0;
	sView= 0;
	_mode= 1; //To bypass mode check for default button setup
	bIsScrollSelected= 0;
}

void CDialogBox::SetupHandlers(HandlerFunct draw, HandlerFunct click, HandlerFunct dblClick, HandlerFunct dropItem)
{
	_drawHandler = draw;
	_clickHandler = click;
	_dblClickHandler = dblClick;
	_dropItemHandler = dropItem;
}


void CDialogBox::GetAllButtons(std::vector<RECT> *rects) const
{
	for(int i = 1; i <= _buttonCount; ++i) {
		rects->push_back(GetButton(i));
	}
}

bool CDialogBox::HasBackground(int &sprite, int &frame) const
{
	if (_background != -1 && _backgroundFrame != -1)
	{
		sprite = _background;
		frame = _backgroundFrame;
		return true;
	}
	return false;
}

bool CDialogBox::HasTitleText(int &frame) const
{
	if (_titleTextFrame != -1)
	{
		frame = _titleTextFrame;
		return true;
	}
	return false;
}

bool CDialogBox::GetTrans() const
{
	return _trans;
}

char CDialogBox::GetMode() const
{
	return _mode;
}

void CDialogBox::AddButton(short X, short X2, short Y, short Y2, char ID)
{
	//if (ID <= 0) ID = _buttonCount;
	_buttons[_buttonCount].SetupButton(X, X2, Y, Y2, ID);
	_buttonCount++;
}

// The bottom bar is the classic 640-wide bar stretched sideways to sWidth, so its buttons
// are stretched by the same amount.
// ---------------------------------------------------------------------------- bottom bar layout

void IconBarLayout(int W, bool bCrusade, IconBarRects & r)
{
	if (W < 640) W = 640;
	const int kPad = 6;
	// menu buttons on the right
	const int kSlotW = 34, kSlotGap = 3, kSlotsW = 6 * kSlotW + 5 * kSlotGap;
	const int x0Slots = W - kPad - kSlotsW;
	for (int k = 0; k < 6; k++) SetRect(&r.btn[k], x0Slots + k * (kSlotW + kSlotGap), 5, x0Slots + k * (kSlotW + kSlotGap) + kSlotW, 49);
	// combat mode, then the crusade role, to their left
	SetRect(&r.combat, x0Slots - 8 - 82, 9, x0Slots - 8, 44);
	SetRect(&r.crusade, r.combat.left - 6 - 100, 9, r.combat.left - 6, 44);
	// HP / MP and SP / location share what is left
	const int xEnd = (bCrusade ? r.crusade.left : r.combat.left) - 8;
	const int avail = xEnd - kPad;
	int vw = (std::min)(avail * 44 / 100, 124);	// HP / MP: about as long as the old tubes
	int mw = (std::min)(avail - vw - 8, 320);
	SetRect(&r.hp, kPad, 10, kPad + vw, 24);
	SetRect(&r.mp, kPad, 29, kPad + vw, 43);
	const int mx = kPad + vw + 8;
	SetRect(&r.sp, mx, 5, mx + mw, 16);
	SetRect(&r.loc, mx, 20, mx + mw, 48);
}

// Click areas of the bottom bar, from the same layout as the drawing (UI\BottomBar.cpp).
void CDialogBox::LayoutIconPanel(short sWidth)
{
	if (sWidth < 640) sWidth = 640;
	sSizeX = sWidth;
	sSizeY = ICONBAR_H;
	ClearButtons();
	IconBarRects L;
	IconBarLayout(sWidth, true, L);
	auto Add = [&](const RECT & rc, char id) { AddButton((short)rc.left, (short)rc.right, (short)rc.top, (short)rc.bottom, id); };
	Add(L.crusade, 1);	// crusade role (only does something during a crusade)
	Add(L.combat, 2);	// combat mode
	for (int k = 0; k < 6; k++) Add(L.btn[k], (char)(3 + k));	// Character, Inventory, Magic, Skill, Chat, Options
}

void CDialogBox::AddButtonList(short X, short X2, short Y, short Y2, char count, char startingID, short spacing)
{
	int totalGap;
	for(int i = 0; i < count; ++i) {
		totalGap = i *(Y2 - Y) + (i*spacing);
		_buttons[i + _buttonCount].SetupButton(X, X2, Y + totalGap, Y2 + totalGap, startingID + i);
	}
	_buttonCount += count;
}

void CDialogBox::AddScrollBar(short X, short X2, short Y, short Y2, char ID/*, bool vertical*/)
{
	AddButton(X, X2, Y, Y2, ID);
	_scrollBar = ID;
}

char CDialogBox::OnButton() const
{
	short X = ms::X - m_X;
	short Y = ms::Y - m_Y;
	for(int i = 0; i < _buttonCount; ++i) {
		if(X < _buttons[i]._X) continue;
		if(Y < _buttons[i]._Y) continue;
		if(X > _buttons[i]._X2) continue;
		if(Y > _buttons[i]._Y2) continue;
		return _buttons[i]._ID;
	}
	return 0;
}

bool CDialogBox::IsOnScrollBar()  const
{
	if (_scrollBar && OnButton() == _scrollBar) return true; //TODO base on precalcuation

	return false;
}

Point CDialogBox::HandleScroll(int visibleAmt, int listSize)
{
	Point ret;

	CButton *scroll;
		
	if (_scrollBar)
	{
		scroll = &_buttons[_scrollBar - 1];
		ret.x = m_X + scroll->_X + 10;
		ret.y = m_Y + scroll->_Y + 10;
	}
	else
	{
		ret.x = m_X;
		ret.y = m_Y;
	}

	if (listSize >= visibleAmt && _scrollBar) //Adjust scroll only if list large enough
	{
		double d2 = listSize - visibleAmt;
		double d3 = scroll->_Y2 - scroll->_Y - 25;

		if (IsTop()) //Scroll only if top dialog
		{
			if (ms::LB != 0) 
			{
				if(OnButton() == _scrollBar)// Possibly change to use pre-calculated onButton value
				{
					double d1 = ms::Y - m_Y - scroll->_Y - 10;

					sView = (short)((d2 * d1)/d3 + 0.5);
				}
			}
			else 
			{
				bIsScrollSelected = FALSE;
			}

			if (ms::Z != 0)
			{
				if (d2 > 8) //Scroll slower on smaller lists
					sView -= ms::Z/60;
				else
					sView -= ms::Z/120;				
			}

			if( sView < 0 ) sView = 0;
			if( sView > listSize - visibleAmt ) sView = listSize - visibleAmt;
		}

		d3 = (d3 * sView)/d2;
		ret.y += (long)(d3 + 0.5);
	}
	else
	{
		sView = 0;
		bIsScrollSelected = FALSE;
	}

	return ret;
}

void CDialogBox::ClearButtons()
{
	_buttonCount = 0;
	_scrollBar = 0;
}

bool CDialogBox::OnDialogBox() const
{
	if(ms::X < m_X || ms::X > m_X + sSizeX ||
		ms::Y < m_Y || ms::Y > m_Y + sSizeY)
	{
		return false;
	}
	else {
		return true;
	}
}

const RECT CDialogBox::GetButton(int buttonID) const
{
	RECT ret;
	buttonID--;
	ret.bottom = m_Y + _buttons[buttonID]._Y2;
	ret.top = m_Y + _buttons[buttonID]._Y;
	ret.left = m_X + _buttons[buttonID]._X;
	ret.right = m_X + _buttons[buttonID]._X2;
	return ret;
}

const int CDialogBox::ButtonX(int buttonID) const
{
	return m_X + _buttons[buttonID - 1]._X;
}

const int CDialogBox::ButtonY(int buttonID) const
{
	return m_Y + _buttons[buttonID - 1]._Y;
}

void CDialogBox::ValidateButtons() const
{
	#ifdef _DEBUG
	int j, i, err = 0;
	if (_buttonCount == 0) return;

	for (i = 0; i < _buttonCount; i++)
	{
		if(_buttons[i]._ID <= 0) err = 1; //Incorrect ID
		if(_buttons[i]._X < 0) err = 2; //Btn going off left
		if(_buttons[i]._Y < 0) err = 3; //Btn going off top
		if(_buttons[i]._X2 > sSizeX) err = 4; //Btn going off right
		if(_buttons[i]._Y2 > sSizeY) err = 5; //Btn going bottom

		if(_buttons[i]._X >= _buttons[i]._X2) err = 6; //0 or negative width
		if(_buttons[i]._Y >= _buttons[i]._Y2) err = 7; //0 or negative height

		for (j = i + 1; j < _buttonCount; j++)
		{
			if(_buttons[i]._ID == _buttons[j]._ID) 
			{
				err = 8; //Duplicate button ID's
				break;
			}
			if(_buttons[i]._X >= _buttons[j]._X2) continue;
			if(_buttons[i]._Y >= _buttons[j]._Y2) continue;
			if(_buttons[i]._X2 <= _buttons[j]._X) continue;
			if(_buttons[i]._Y2 <= _buttons[j]._Y) continue;
			err =  9; //Overlapping buttons
			break;
		}
		if (err) break;
	}

	if (err)
	{
		return; //Error	
	}
	#endif
}

void CDialogBox::MoveOnScreen(short X, short Y)
{
	if (X != -1) m_X = X;
	if (Y != -1) m_Y = Y;
	if (m_X < 0) m_X = 0;
	if (m_X + sSizeX > RENDER_WIDTH - 1) m_X = RENDER_WIDTH - 1 - sSizeX;
	if (m_Y < 0) m_Y = 0;
	if (m_Y + sSizeY > RENDER_HEIGHT - 1) m_Y = RENDER_HEIGHT - 1 - sSizeY;
}

void CDialogBox::CentreOverPoint(short X, short Y)
{
	m_X = X - sSizeX/2;
	m_Y = Y - sSizeY/2;	
	MoveOnScreen();
}

#define BTNSZX				74
#define BTNSZY				20
#define LBTNPOSX			30
#define RBTNPOSX			154
#define BTNPOSY			292

// ---------------------------------------------------------------------------- NPC window layout

namespace {
	// per mode: a header with the NPC's name, lines of prompt text, number of choices
	struct NpcQuerySpec { bool title; int lines; int choices; };
	NpcQuerySpec NpcQuerySpecOf(int iMode)
	{
		switch (iMode) {
		case 0: return { true, 0, 2 };		// Trade / Offer / Withdraw / Learn, Talk
		case 1: return { false, 2, 2 };		// item to another player: Give, Exchange
		case 2: return { false, 2, 2 };		// item on a shop / blacksmith: Sell, Repair
		case 3: return { false, 2, 1 };		// item to the warehouse: Deposit
		case 4: return { true, 0, 1 };		// Talk (guard, unicorn, quest NPCs)
		case 5: return { true, 0, 3 };		// Trade, Sell, Talk
		case 6: return { true, 0, 1 };		// Heldenian staff officer: Trade
		case 7: return { true, 0, 5 };		// Blacksmith
		case 8: return { true, 0, 4 };		// Shop Keeper
		case 9: return { true, 0, 4 };		// City Hall Officer
		case 10: return { false, 2, 1 };	// item to the guild warehouse: Deposit
		case 11: return { false, 2, 1 };	// repair all: Repair
		default: return { true, 0, 2 };
		}
	}
	const int kPad = 12, kHeaderH = 48, kLineH = 16, kBtnH = 22, kGap = 6;
}

bool NpcQueryHasTitle(int iMode) { return NpcQuerySpecOf(iMode).title; }

int NpcQueryTextTop(int iMode) { return NpcQuerySpecOf(iMode).title ? kHeaderH : kPad + 2; }

int NpcQueryLayout(int iMode, int id, RECT * pRc)
{
	const NpcQuerySpec q = NpcQuerySpecOf(iMode);
	int top = NpcQueryTextTop(iMode);
	if (q.lines > 0) top += q.lines * kLineH + 14;
	const int cols = (q.choices == 1) ? 1 : 2;
	const int bw = (cols == 1) ? NPCQUERY_W - 2 * kPad : (NPCQUERY_W - 2 * kPad - 8) / 2;
	const int rows = (q.choices + cols - 1) / cols;
	const int h = top + rows * (kBtnH + kGap) - kGap + kPad;
	if (pRc != NULL) {
		SetRect(pRc, 0, 0, 0, 0);
		if (id == NPCQUERY_CLOSE) SetRect(pRc, NPCQUERY_W - 28, 6, NPCQUERY_W - 8, 26);
		else if (id >= 1 && id <= q.choices) {
			const int k = id - 1, r = k / cols, c = k % cols;
			const int x = kPad + c * (bw + 8), y = top + r * (kBtnH + kGap);
			SetRect(pRc, x, y, x + bw, y + kBtnH);
		}
	}
	return h;
}

void CDialogBox::SetMode(char mode)
{

	if (_mode == mode) return;

	ClearButtons();
	_mode = mode;
	switch (_dialogID){
	case 1: ///////Character: drawn and clicked in code (UI\CharacterPanel.cpp), no fixed buttons
		break;

	case 2: ///////Inventory: drawn in code (UI\InventoryPanel.cpp, CGame::InvButtonRect)
		AddButton(8, 109, 180, 202, 1);		// Upgrade
		AddButton(117, 218, 180, 202, 2);	// Manufacture
		AddButton(198, 220, 3, 24, 3);		// close
		break;

	case 3: ////////Magic: drawn in code (UI\MagicPanel.cpp), same click areas as the old window
		AddButtonList(25, 240, 70, 88, 9, 1); 

		AddButton(16, 38, 240, 268, 10);
		AddButton(39, 56, 240, 268, 11);
		AddButton(57, 81, 240, 268, 12);
		AddButton(82, 101, 240, 268, 13);
		AddButton(102, 116, 240, 268, 14);
		AddButton(117, 137, 240, 268, 15);
		AddButton(138, 165, 240, 268, 16);
		AddButton(166, 197, 240, 268, 17);
		AddButton(198, 217, 240, 268, 18);
		AddButton(218, 239, 240, 268, 19);
		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, 285, 285 + BTNSZY, 20);
		AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, 285, 285 + BTNSZY, 22);	// Enchant (UI\EnchantPanel.cpp)
		AddButton(LBTNPOSX, RBTNPOSX + BTNSZX, 310, 310 + BTNSZY, 23);	// Passive Skills (UI\PassivePanel.cpp)
		AddButton(230, 252, 4, 26, 21);		// close
		break;

	case 4: ///////ItemDrop
		AddButton(30, 30 + BTNSZX, 55, 55 + BTNSZY,1 );
		AddButton(170 , 170 + BTNSZX , 55 , 55 + BTNSZY, 2);
		AddButton(35, 240 , 80, 95, 3);
		break;

	case 5: //////15AgeMsg
		AddButton(120 , 120 + BTNSZX , 127 , 127 + BTNSZY, 1);
		break;

	case 6: //////WarningMsg
		AddButton(120 , 120 + BTNSZX , 127 , 127 + BTNSZY, 1);
		break;

	case 7: //////GuildMenu
		switch (mode) {
		case 0:
			AddButtonList(44, 218, 94, 114, 5, 1);
			/*AddButton(55, 230, 30 + 63, 30 + 78, 1);
			AddButton(59, -13 + 222, 30 + 82, 30 + 99, 2);
			AddButton(-13 + 61, -13 + 226, 30 + 103, 30 + 120, 3);
			AddButton(-13 + 60, -13 + 227, 30 + 123, 30 + 139, 4);
			AddButton(-13 + 72, -13 + 228, 30 + 143, 30 + 169, 5);*/
			break;
		case 3:
		case 4:
		case 7:
		case 8:
		case 10:
		case 12:
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1); 
			break;
		case 1:
			AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		case 9:
			AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		case 11:
			AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		case 5:
			AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2); 
			break;
		case 13:
			AddButton(-13 + 65, -13 + 137, 30 + 168, 30 + 185, 1);
			AddButton(-13 + 150, -13 + 222, 30 + 168, 30 + 185, 2);
			AddButton(-13 + 65, -13 + 137, 30 + 188, 30 + 205, 3);
			AddButton(-13 + 150, -13 + 222, 30 + 188, 30 + 205, 4);
			AddButton(-13 + 65, -13 + 137, 30 + 208, 30 + 225, 5);
			AddButton(-13 + 150, -13 + 222, 30 + 208, 30 + 225, 6);
			AddButton(-13 + 65, -13 + 137, 30 + 228, 30 + 245, 7);
			AddButton(-13 + 150, -13 + 222, 30 + 228, 30 + 245, 8);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 9);
			break ;
		case 14:
		case 15:
		case 16:
		case 17:
		case 21:
		case 22:
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			break ;
		}
		break;

	case 8: //////GuildOp
		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
		AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
		break;

	case 10: //////Chat: drawn and clicked in code (UI\ChatPanel.cpp), no fixed buttons
		break;

	case 11: //////Shop
		switch (mode){
		case 0: //Select item
			AddButtonList(19, 220, 64, 82, 13, 1);
			AddScrollBar(230, 255, 25, 325, 14);
			break;
		default: //Item details
			AddButton(145, 162, 209, 230, 1);//Up 10
			AddButton(145, 162, 234, 251, 2);//Down 10
			AddButton(163, 180, 209, 230, 3);//Up 1
			AddButton(163, 180, 234, 251, 4);//Down 1
			AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 5);//Purchase
			AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 6); //Cancel
			break;
		}
		break;

	case 12: //////LevelUpSettings: drawn and clicked in code (UI\LevelUpPanel.cpp), no fixed buttons
		break;

	case 13: //////CityHall
		switch (mode) {
		case 0:
			AddButton(35, 220, 70, 95, 1);// citizenship rq
			AddButton(35, 220, 95, 120, 2);// rq reward gold
			AddButton(35, 220, 120, 145, 3);
			AddButton(35, 220, 145, 170, 4);
			AddButton(35, 220, 170, 195, 5);// Legion point services
			AddButton(35, 220, 195, 220, 6);// Teleport List
			AddButton(35, 220, 220, 245, 7);
			AddButton(35, 220, 245, 270, 8);
			AddButton(35, 220, 270, 292, 9);// Bounty board
			AddButton(35, 220, 295, 317, 10);// Rebirth
			break;
		case 1:
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1); // Yes Click
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2); // No Click
			AddScrollBar(230, 255, 25, 325, 3);
			break;
		case 3:
		case 4:	
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1); // No Click
			break;
		case 5:
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1); // Yes
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2); 
			break;
		case 7:
			AddButtonList(35, 220, 95, 110, 7, 1, 15);
			// Hero's Cape// Hero's Helm// Hero's Cap// Hero's Armor// Hero's Robe// Hero's Hauberk// Hero's Leggings
			break;
		case 8:
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		case 9:
			AddButton(160, 230, 35, 50, 1);
			AddButtonList(30, 230, 73, 88, 16, 2);
			AddScrollBar(230, 255, 25, 325, 18);
			break;
		case 10:
			AddButtonList(LBTNPOSX, RBTNPOSX + BTNSZX, 130, 145, 13, 1);	
			break;
		case 11: // Hero confirm
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		case 12: // Bleeding Island Teleport
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;

		case 13: // Players Arena Teleport
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;

		case 14: // Dungeon Level 2 Teleport
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		}
		break;

	case 14: ///////Bank
		switch (mode) {
		case -1:
			break;
		case 0:
			AddButtonList(30, 210, 110, 125, 13, 1);
			AddScrollBar(230, 255, 25, 325, 14);
			//addButton(230, 260, 40, 320, 14);
			break;
		}
		break;

	case 15: ///////Skill Menu(F8)
		AddButtonList(30, 135 + 44, 45, 60, 17, 1);
		AddButtonList(210, 230, 45, 60, 17, 18);
		AddScrollBar(230, 255, 25, 325, 35);
		AddButton(232, 254, 2, 22, 40);		// close (drawn in code, UI\SkillPanel.cpp)
		break;

	case 16: ///////MagicShop
		AddButtonList(22, 165, 70, 88, 9, 1); 

		AddButton(85, 200, 260, 275, 10);
		AddButton(-20 + 42 +31, -20 + 50 +31, 248, 260 , 11);
		AddButton(-20 + 55 +31, -20 + 68 +31, 248, 260, 12);
		AddButton(-20 + 73 +31, -20 + 93 +31, 248, 260, 13);
		AddButton(-20 + 98 +31, -20 + 113 +31, 248, 260, 14);
		AddButton(-20 + 118 +31, -20 + 129 +31, 248, 260, 15);
		AddButton(-20 + 133 +31, -20 + 150 +31, 248, 260, 16);
		AddButton(-20 + 154 +31, -20 + 177 +31, 248, 260, 17);
		AddButton(-20 + 181 +31, -20 + 210 +31, 248, 260, 18);
		AddButton(-20 + 214 +31, -20 + 230 +31, 248, 260, 19);
		AddButton(-20 + 234 +31, -20 + 245 +31, 248, 260, 20);
		break;

	case 18: //////Text
		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
		AddScrollBar(230, 255, 25, 325, 2);
		break;

	case 19: //////SysMenu
		AddButton(120, 150, 63, 74, 1);
		AddButton(151, 200, 63, 74, 2);
		AddButton(201, 234, 63, 74, 3);
		AddButton(24, 115, 81, 100, 4);
		AddButton(116, 202, 81, 100, 5);
		AddButton(23, 108, 108, 119, 6);
		AddButton(123, 203, 108, 119, 7);
		AddButton(28, 235, 156, 171, 8);//Transparency Change
		AddButton(28, 127, 178, 193, 9);//Guide Map Toggle
		AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, 225, 225 + BTNSZY, 10);
		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, 225, 225 + BTNSZY, 11);
		AddButton(125, 240, 122, 138, 12); //Sound slider
		AddButton(125, 240, 139, 155, 13); //Music slider
		AddButton(125, 235, 204, 219, 14);
		break;

	case 20: //////NpcActionQuery: drawn in code (UI\NpcQueryPanel.cpp), as tall as its choices
		sSizeX = NPCQUERY_W;
		sSizeY = (short)NpcQueryLayout(mode, 0, NULL);
		for (int id = 1; id <= 5; id++) {
			RECT rc;
			NpcQueryLayout(mode, id, &rc);
			if (rc.right > rc.left) AddButton((short)rc.left, (short)rc.right, (short)rc.top, (short)rc.bottom, (char)id);
		}
		{
			RECT rc;
			NpcQueryLayout(mode, NPCQUERY_CLOSE, &rc);
			AddButton((short)rc.left, (short)rc.right, (short)rc.top, (short)rc.bottom, NPCQUERY_CLOSE);
		}
		break;
	case 21: //////NpcTalk
		switch (mode) {
		case 0: //  OK
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			break;

		case 1: // Accept / Decline
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;

		case 2:	// Next
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			break;
		}
		break;

	case 23: ///////SellOrItemRepair
		switch (mode) {
		case 1:
			AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1); // Sell
			AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2); // Cancel
			break;

		case 2:
			AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1); // Repair
			AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2); // Cancel
			break;
		}
		break;

	case 24: //////Fish
		AddButton(160, 253, 70, 90, 1);
		break;

	case 25: //////ShutDown
		AddButton(210, 210 + BTNSZX, 127, 127 + BTNSZY, 1);
		break;

	case 26: //////SkillDlg
		switch (mode) {
		case 1:
			AddButton(5 + 60, 5 + 153, 8 + 175, 8 + 195, 1);
			break;

		case 7:// Crafting
			AddButton(5 + 60, 5 + 153, 8 + 175, 8 + 195, 1);
			break;

		case 3:
			AddButtonList(35, 180, 60, 60 + 15, 13, 1);
			AddButton(220, 245, 210, 230, 14);
			AddButton(220, 245, 230, 250, 15);
			AddButton(30, sSizeX - 31, 345, 360, 16);
			//AddScrollBar(1, 2, 3, 4, 16);
			break;

		case 4:
			AddButton(25, 94, 346, 365, 1);
			AddButton(150, 245, 346, 365, 2);
			break;

		case 6:
			AddButton(31, 94, 346, 365, 1);
			break;
		}
		break;

	case 27: //////Exchange
		switch (mode) {
		case 1: // Not yet confirmed the exchange
			AddButton(220, 220 + BTNSZX, 310, 310 + BTNSZY, 1); // Exchange
			AddButton(440, 440 + BTNSZX, 310, 310 + BTNSZY, 2); // Cancel only possible if confirmation is not activated
			break;
		case 2: // Someone already confirmed the exchange
			AddButton(440, 440 + BTNSZX, 310, 310 + BTNSZY, 1); 
			break;
		}
		break;

	case 28: //////Quest
		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1); 
		break;

	case 30: //////IconPanel
		LayoutIconPanel(sSizeX);
		break;

	case 31: //////SellList
		AddButtonList(25, 250, 55, 70, 12/*MAXSELLLIST*/, 1);
		AddButton(30, 30 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 12/*MAXSELLLIST*/ + 1 ); // Sell
		AddButton(154, 154 + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 12/*MAXSELLLIST*/ + 2); // Cancel
		break;

	case 32: /////Party
		switch (mode) {
		case 0:
			AddButtonList(50, 200, 80, 100, 4, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 5); 
			break;

		case 1:
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;

		case 2:
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			break;

		case 3: // cancel
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			break;

		case 4:
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddScrollBar(230, 255, 25, 325, 2);
			break;

		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			break;

		case 11:
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		}
		break;

	case 33: //////CrusadeJob
		switch (mode) {
		case 1:
			AddButton(24, 246, 150, 165, 1);
			AddButton(24, 246, 175, 190, 2);
			AddButton(24, 246, 200, 215, 3);
			AddButton(195, 250, 296, 316, 4);
			break;

		case 2:
			AddButton(24, 246, 160, 175, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		}
		break;

	case 34: //////ItemUpgrade
		switch (mode) {
		case 1:	// Upgrade Majestic, items in the window
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);
			break;
		case 3: // Success
		case 4: // Not possible
		case 7: // Lost item
		case 8: // Failed
		case 9: // Failed
		case 10:// Failed
		case 12:// 12 Need stone!
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			break;
		case 5: // main menu
			AddButton(24, 248, 100, 115, 1);
			AddButton(24, 248, 120, 135, 2);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 3);// Cancel
			break;

		case 6: // Upgrade Xelima / Merien
			AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 1);
			AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 2);// Cancel
			break;
		}
		break;

	case 35: //////Help
		AddButtonList(25, sSizeX -26, 50, 50+15, 14, 1);
		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 15);
		break;

	case 36: //////Commander
		switch (mode) {
		case 0: // Main
			AddButton(20, 20 +46, 340, 392, 1);
			AddButton(20 +50, 20 +46 +50, 340, 392, 2);
			AddButton(20 +100, 20 +46 +100, 340, 392, 3);
			AddButton(20 +150, 20 +46 +150, 340, 392, 4);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 5);
			break;

		case 1: // Set TP
			AddButton(15, 15 + 278, 60, 60 + 272, 1);
			AddButton(20 +150 + 74 -50, 20 +46 +150 + 74 -50, 340, 392, 2);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 3);
			break;

		case 2: // Use TP
			AddButton(20 +50, 20 +46 +50, 340, 392, 1);
			AddButton(20 +150 + 74 -50, 20 +46 +150 + 74 -50, 340, 392, 2);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 3);
			break;

		case 3: // Summon Unit
			AddButton(20, 20 +46, 220, 220 +50, 1);
			AddButton(20 +50, 20 +50 +45, 220, 220 +50, 2);
			AddButton(20 +100, 20 +100 +45, 220, 220 +50, 3);
			AddButton(20 +150, 20 +150 +45, 220, 220 +50, 4);
			AddButton(20, 280, 140, 160, 5);
			AddButton(20, 280, 160, 175, 6);
			AddButton(20 +150 + 74 -50, 20 +46 +150 + 74 -50, 340, 392, 7);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 8);
			break;

		case 4: // Set constr
			AddButton(15, 15 + 278, 60, 60 + 272, 1);
			AddButton(20 +150 + 74 -50, 20 +46 +150 + 74 -50, 340, 340 + 52, 2);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 340 + 52, 3);
			break;
		}
		break;

	case 37: //////Constructor
		switch (mode) {
		case 0: // Main
			AddButton(20, 20 +46, 340, 392, 1);
			AddButton(20 +50, 20 +46 +50, 340, 392, 2);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 3);
			break;

		case 1: // Choose building
			AddButton(20, 20 +46, 220, 220 +50, 1);
			AddButton(20 +50, 20 +50 +45, 220, 220 +50, 2);
			AddButton(20 +100, 20 +100 +45, 220, 220 +50, 3);
			AddButton(20 +150, 20 +150 +45, 220, 220 +50, 4);
			AddButton(20 +150 + 74 -50, 20 +46 +150 + 74 -50, 322, 322 + 52, 5);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 322, 322 + 52, 6);
			break;

		case 2: // Use TP
			AddButton(20 +50, 20 +46 +50, 340, 392, 1);
			AddButton(20 +150 + 74 -50, 20 +46 +150 + 74 -50, 340, 392, 2);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 3);
			break;
		}
		break;

	case 38: //////Soilder
		switch (mode) {
		case 0: // Main dlg
			AddButton(20, 20 +46, 340, 392, 1);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 2);
			break;

		case 1: // Use TP
			AddButton(20, 20 +46+50, 340, 392, 1);
			AddButton(20 +150 + 74 -50, 20 +46 +150 + 74 -50, 340, 392, 2);
			AddButton(20 +150 + 74, 20 +46 +150 + 74, 340, 392, 3);
			break;
		}
		break;

	case 40: //////Slate
		switch (mode) {
		case 1:
			AddButton(120, 180, 150, 165, 1);
			break;
		}
		break;

	case 41:
		switch (mode) {
		case 1: // Not yet confirmed the exchange
			AddButton(30, 30 + BTNSZX, 55, 55 + BTNSZY, 1);// yes
			AddButton(170 , 170 + BTNSZX , 55 , 55 + BTNSZY, 2);// No
			break;
		case 2: // waiting for other side to confirm
			break;
		}
		break;

	case 42: //////ChangeStatsMajestic
	
		//Points up
		AddButton(195, 206, 127, 138, 1);//STR
		AddButton(195, 206, 146, 157, 2);//VIT
		AddButton(195, 206, 165, 176, 3);//DEX
		AddButton(195, 206, 184, 195, 4);//INT
		AddButton(195, 206, 203, 214, 5);//MAG
		AddButton(195, 206, 222, 233, 6);//CHR
		//Points down
		AddButton(210, 221, 127, 138, 7);//STR
		AddButton(210, 221, 146, 157, 8);//VIT
		AddButton(210, 221, 165, 176, 9);//DEX
		AddButton(210, 221, 184, 195, 10);//INT
		AddButton(210, 221, 203, 214, 11);//MAG
		AddButton(210, 221, 222, 233, 12);//CHR

		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 13);
		AddButton(LBTNPOSX, LBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 14);
		break;

	case 43: //////FriendList
		AddButton(25, 233, 45+16, 45+13*16, 1);
		AddButton(25, 25+25, 45+14*16, 45+15*16, 2);
		AddButton(80, 80+30, 45+14*16, 45+15*16, 3);
		AddButton(140, 140+45, 45+14*16, 45+15*16, 4);
		break;
	case 44: //////GuildQuery
		switch (mode) 
		{
		case 0:
			AddButton(0, 126, 25, 40, 1);
			AddButton(126, 252, 25, 40, 2);
			AddButton(0, 126, 40, 55, 3);
			AddButton(126, 252, 40, 55, 4);
			AddButton(0, 126, 55, 70, 5);
			break;
		}
		break;

	case 49: ///////GuildTeleportRequest
		AddButton(30, 30 + BTNSZX, 65, 65 + BTNSZY, 1);
		AddButton(170 , 170 + BTNSZX , 65 , 65 + BTNSZY, 2);
		break; //

	case 50: ///////Ressurect
		AddButton(30, 30 + BTNSZX, 65, 65 + BTNSZY, 1);
		AddButton(170 , 170 + BTNSZX , 65 , 65 + BTNSZY, 2);
		break; //

	case 51: //////CmdHallMenu
		switch (mode) 
		{
		case 0: // initial diag
			AddButton(35, 220, 70, 95, 1);// TP diag
			AddButton(35, 220, 95, 120, 2);// Soldier Diag
			AddButton(35, 220, 120, 145, 3);// Flag Diag
			AddButton(35, 220, 145, 170, 4);// Angel Diag
			break;

		case 1: // TP now
			AddButtonList(LBTNPOSX, RBTNPOSX + BTNSZX, 130, 144, 13, 1);
			break;

		case 2: // Buy a soldier scroll
			AddButton(35, 220, 70, 95, 1); // Sorceress
			AddButton(35, 220, 95, 120, 2); // ATK
			AddButton(35, 220, 120, 145, 3); // Elf
			AddButton(35, 220, 145, 170, 4); // DSK
			AddButton(35, 220, 170, 195, 5); // HBT
			AddButton(35, 220, 195, 220, 6); // Barbarian

			break;

		case 3: // Buy a Flag
			AddButton(35, 220, 140, 165, 1);
			break;

		case 4: // Buy an Angel
			AddButton(35, 220, 175, 200, 1);
			AddButton(35, 220, 200, 225, 2);
			AddButton(35, 220, 225, 250, 3);
			AddButton(35, 220, 250, 275, 4);

			break;
		}
		break;

	case 53: //////DKWeaponsMenu
		AddButtonList(25, 248, 50, 65, 4, 1); 
		// Dark Mage Staff // Dark Knight Rapier// Dark Knight Great Sword// Dark Knight Flameberge
		AddButton(RBTNPOSX, RBTNPOSX + BTNSZX, BTNPOSY, BTNPOSY + BTNSZY, 5);
		break;

	case 56:
		AddButtonList(30, 210, 110, 125, 13, 1);
		AddScrollBar(230, 255, 25, 325, 14);
		break;

	case 57: //////Guild
		switch (mode)
		{
		case 0:
			AddButtonList(-13 + 61, -13 + 226, 30 + 45, 30 + 45+15, 6, 1);
			break;

		case 1:
			AddButton(61, 226, 295, 295+15, 1);
			AddButtonList(10, 85, 55, 70, 16, 2);
			AddButtonList(85, 150, 55, 70, 16, 18);
			AddScrollBar(230, 255, 25, 325, 34);
			break;

		case 2:
			AddButtonList(-13 + 61, -13 + 226, 30 + 45, 30 + 45+15, 7, 1);
			break;

		case 3: // warehouse upgrade
			AddButton(172, 258, 30 + 45+15*0, 30 + 45+15*1, 1); // upgrade
			AddButton(-13 +61, -13 +226, 30 + 45+15*1, 30 + 45+15*2, 2); // back
			break;

		case 4: // teleport upgrade
			AddButton(172, 258, 30 + 45, 30 + 45+15*1, 1); // upgrade
			AddButton(-13 +61, -13 +226, 30 + 45+15*1, 30 + 45+15*2, 2); // back
			break;

		case 5: // Captain upgrade	
			AddButton(172, 258, 30 + 45, 30 + 45+15*1, 1); // upgrade
			AddButton(-13 +61, -13 +226 , 30 + 45+15*1, 30 + 45+15*2, 2);// back
			break;

		case 6: // raidmaster upgrade
			AddButton(172, 258, 30 + 45, 30 + 45+15, 1); // upgrade
			AddButton(-13 +61, -13 +226, 30 + 45+15*1, 30 + 45+15*2, 2); // back
			break;

		case 7: // huntmaster upgrade
			AddButton(172, 258, 30 + 45, 30 + 45+15*1, 1); // upgrade
			AddButton(-13 +61, -13 +226, 30 + 45+15*1, 30 + 45+15*2, 2); // back
			break;

		case 8:
			AddButton(30, 150, 35, 50, 1); // Post message
			AddScrollBar(230, 255, 25, 325, 2);
			AddButton(0, sSizeX, 300, 315, 3); // Back
			AddButtonList(30, 230, 73, 88, 15, 4);
			break;

		case 9: // post
			AddButton(30, 235, 35, 50, 1); // from
			AddButton(30, 235, 50, 65, 2); // title
			AddButton(30, 235, 65, 300, 3); // msg
			AddButton(130, 180, 300, 315, 4); // delete
			AddButton(180, 235, 300, 315, 5); // back
			break;
			
		case 10:
			AddButton(30, 235, 35, 65, 1); // title
			AddButton(30, 235, 50, 300, 2); // msg
			AddButton(130, 180, 300, 315, 3); // send
			AddButton(180, 235, 300, 315, 4); // cancel
			break;
		}
		break;
	case 59:
		AddButtonList(20, 170, 55, 75, 6, 1);
		AddButtonList(170, 320, 55, 75, 6, 7);
		break;

	case 60:
		AddScrollBar(230, 255, 25, 325, 1);
		AddButton(110, 258, 290, 305, 2);
		AddButtonList(30, 250, 45, 60, 15, 3);
		break;

	case 61:
		switch (mode)
		{
		case 0: //Select item
			AddButtonList(20, 250, 45, 60, 17, 1);
			AddScrollBar(230, 255, 25, 325, 18);
			break;
		}
		break;

	case DIALOG_MAILBOX:
		switch(mode)
		{
		case 0:
			AddButton(30, 120, 35, 50, 1); // send mail
			AddScrollBar(230, 255, 25, 325, 2);
			AddButtonList(30, 230, 73, 88, 16, 3);
			break;

		case 1:
			AddButton(30, 235, 35, 50, 1); // to
			AddButton(30, 235, 50, 65, 2); // title
			AddButton(30, 235, 65, 300, 3); // msg
			AddButton(30, 130, 300, 315, 4); // attach items
			AddButton(130, 180, 300, 315, 5); // send
			AddButton(180, 235, 300, 315, 6); // cancel
			break;

		case 2:
			AddButton(30, 235, 35, 50, 1); // from
			AddButton(30, 235, 50, 65, 2); // title
			AddButton(30, 235, 65, 300, 3); // msg
			AddButton(30, 130, 300, 315, 4); // attached items
			AddButton(130, 180, 300, 315, 5); // delete
			AddButton(180, 235, 300, 315, 6); // back
			break;

		case 3:
			AddButtonList(30, 235, 120, 135, 10, 1);
			AddButton(130, 180, 300, 315, 11); // attach
			AddButton(180, 235, 300, 315, 12); // cancel
			break;

		case 4:
			AddButtonList(30, 210, 110, 125, 13, 1);
			AddScrollBar(230, 255, 25, 325, 14);
			AddButton(180, 235, 300, 315, 15); // back
			break;
		}
		break;

	case DIALOG_YESNO:
		AddButton(30, 30 + BTNSZX, 55, 55 + BTNSZY, 1);// yes
		AddButton(170 , 170 + BTNSZX , 55 , 55 + BTNSZY, 2);// No
		break;

	case DIALOG_REBIRTH:	// Rebirth: drawn and clicked in code (UI\RebirthPanel.cpp), no fixed buttons
		break;

	case DIALOG_ENCHANT:	// VALORIA enchanting: drawn and clicked in code (UI\EnchantPanel.cpp), no fixed buttons
		break;

	case DIALOG_PASSIVE:	// VALORIA passive tree: drawn and clicked in code (UI\PassivePanel.cpp), no fixed buttons
		break;

	case DIALOG_ACHIEVE:	// VALORIA achievements: drawn and clicked in code (UI\AchievePanel.cpp), no fixed buttons
		break;

	case DIALOG_BOUNTY:	// Bounty board: drawn and clicked in code (UI\BountyBoard.cpp), no fixed buttons
		break;

	case DIALOG_MOSTWANTED: // Most Wanted board, drawn in code (UI\WantedSystem.cpp), 460 wide
		AddButton(432, 454, 4, 26, 1);		// close
		break;
	}
	ValidateButtons();
}