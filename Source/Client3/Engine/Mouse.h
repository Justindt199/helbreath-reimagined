// Mouse.h: SFML-based mouse input (replaces DXC_dinput / DirectInput).
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_MOUSE_H
#define HB_ENGINE_MOUSE_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <windows.h>

// Global mouse state read throughout the UI code (unchanged interface).
struct ms
{
	static short X, Y, Z;
	static char LB, RB, MB;
};

class CMouse
{
public:
	CMouse();
	virtual ~CMouse();

	BOOL bInit(class CRenderer * pRenderer);
	void SetAcquire(BOOL bFlag);		// called when the window gains / loses focus
	void UpdateMouseState(short * pX, short * pY, short * pZ, char * pLB, char * pRB, char * pMB);
	void UpdateMouseState();
	void SetPosition(short sX, short sY);	// moves the cursor (game coordinates)

	// Full-resolution UI (settings menu): while captured, the game sees no clicks or wheel,
	// and the UI reads the mouse in window pixels instead. After release, buttons that are
	// still held stay hidden from the game until they are let go.
	void SetUiCapture(bool bCapture);
	bool IsUiCaptured() const { return m_bUiCapture; }
	void GetUiState(int * pX, int * pY, bool * pLeftDown, bool * pLeftClicked, int * pWheel);	// clears click and wheel

	short m_sX, m_sY, m_sZ;		// position in 640x480 game coordinates, last wheel delta (120 per notch)

private:
	void _Poll();
	bool _HideButtonsFromGame();

	class CRenderer * m_pRenderer;
	bool m_bDown[3];		// current button state
	bool m_bLatched[3];		// pressed since last update (so very short clicks are never lost)
	bool m_bActive;

	bool m_bUiCapture;
	bool m_bUiClicked;		// left click seen while captured, not yet read by the UI
	bool m_bSuppress;		// hide held buttons from the game after the UI lets go
	int  m_iWinX, m_iWinY;	// cursor position in window pixels
	int  m_iUiWheel;		// wheel delta while captured (120 per notch)
};

#endif // HB_ENGINE_MOUSE_H
