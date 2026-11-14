// Mouse.cpp: SFML-based mouse input (replaces DXC_dinput / DirectInput).
//////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SFML/Graphics.hpp>
#include <optional>

#include "Mouse.h"
#include "Renderer.h"

short ms::X;
short ms::Y;
short ms::Z;
char ms::LB;
char ms::RB;
char ms::MB;

CMouse::CMouse()
{
	m_pRenderer = NULL;
	m_sX = RENDER_WIDTH / 2;
	m_sY = RENDER_HEIGHT / 2;
	m_sZ = 0;
	m_bActive = true;
	for (int i = 0; i < 3; i++) m_bDown[i] = m_bLatched[i] = false;
	m_bUiCapture = m_bUiClicked = m_bSuppress = false;
	m_iWinX = m_iWinY = 0;
	m_iUiWheel = 0;
}

CMouse::~CMouse()
{
}

BOOL CMouse::bInit(CRenderer * pRenderer)
{
	m_pRenderer = pRenderer;
	if (m_pRenderer == NULL || m_pRenderer->GetWindow() == NULL) return FALSE;
	_Poll();
	return TRUE;
}

void CMouse::SetAcquire(BOOL bFlag)
{
	m_bActive = (bFlag != FALSE);
	for (int i = 0; i < 3; i++) m_bDown[i] = m_bLatched[i] = false;

	sf::RenderWindow * pWindow = (m_pRenderer != NULL) ? m_pRenderer->GetWindow() : NULL;
	if (pWindow != NULL)
		pWindow->setMouseCursorGrabbed(m_bActive && m_pRenderer->m_bFullMode);
}

void CMouse::SetPosition(short sX, short sY)
{
	m_sX = sX;
	m_sY = sY;
	sf::RenderWindow * pWindow = (m_pRenderer != NULL) ? m_pRenderer->GetWindow() : NULL;
	if (pWindow != NULL && m_bActive) {
		int iWinX, iWinY;
		m_pRenderer->MapGameToWindow(sX, sY, &iWinX, &iWinY);
		sf::Mouse::setPosition({iWinX, iWinY}, *pWindow);
	}
}

void CMouse::_Poll()
{
	sf::RenderWindow * pWindow = (m_pRenderer != NULL) ? m_pRenderer->GetWindow() : NULL;
	if (pWindow == NULL) return;

	while (const std::optional<sf::Event> event = pWindow->pollEvent())
	{
		if (const auto * pPressed = event->getIf<sf::Event::MouseButtonPressed>()) {
			int i = -1;
			if (pPressed->button == sf::Mouse::Button::Left) i = 0;
			else if (pPressed->button == sf::Mouse::Button::Right) i = 1;
			else if (pPressed->button == sf::Mouse::Button::Middle) i = 2;
			if (i >= 0) m_bDown[i] = m_bLatched[i] = true;
		}
		else if (const auto * pReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
			int i = -1;
			if (pReleased->button == sf::Mouse::Button::Left) i = 0;
			else if (pReleased->button == sf::Mouse::Button::Right) i = 1;
			else if (pReleased->button == sf::Mouse::Button::Middle) i = 2;
			if (i >= 0) m_bDown[i] = false;
		}
		else if (const auto * pWheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
			if (pWheel->wheel == sf::Mouse::Wheel::Vertical && pWheel->delta != 0.f) {
				if (m_bUiCapture) m_iUiWheel += (int)(pWheel->delta * 120.f);
				else m_sZ = (short)(pWheel->delta * 120.f);	// same units as DirectInput (WHEEL_DELTA)
			}
		}
		else if (event->is<sf::Event::FocusLost>()) {
			for (int i = 0; i < 3; i++) m_bDown[i] = m_bLatched[i] = false;
		}
	}

	if (m_bActive) {
		const sf::Vector2i p = sf::Mouse::getPosition(*pWindow);
		m_iWinX = p.x;
		m_iWinY = p.y;
		m_pRenderer->MapWindowToGame(p.x, p.y, &m_sX, &m_sY);
	}
}

void CMouse::SetUiCapture(bool bCapture)
{
	if (bCapture == m_bUiCapture) return;
	m_bUiCapture = bCapture;
	m_bUiClicked = false;
	m_iUiWheel = 0;
	m_sZ = 0;
	for (int i = 0; i < 3; i++) m_bLatched[i] = false;
	m_bSuppress = !bCapture && (m_bDown[0] || m_bDown[1] || m_bDown[2]);
}

void CMouse::GetUiState(int * pX, int * pY, bool * pLeftDown, bool * pLeftClicked, int * pWheel)
{
	if (m_bLatched[0] && m_bUiCapture) { m_bUiClicked = true; m_bLatched[0] = false; }
	*pX = m_iWinX;
	*pY = m_iWinY;
	*pLeftDown = m_bDown[0];
	*pLeftClicked = m_bUiClicked;
	*pWheel = m_iUiWheel;
	m_bUiClicked = false;
	m_iUiWheel = 0;
}

void CMouse::UpdateMouseState(short * pX, short * pY, short * pZ, char * pLB, char * pRB, char * pMB)
{
	_Poll();
	*pX = m_sX;
	*pY = m_sY;
	*pZ = m_sZ;
	*pLB = (char)((m_bDown[0] || m_bLatched[0]) ? 0x80 : 0);
	*pRB = (char)((m_bDown[1] || m_bLatched[1]) ? 0x80 : 0);
	*pMB = (char)((m_bDown[2] || m_bLatched[2]) ? 0x80 : 0);
	if (_HideButtonsFromGame()) { *pLB = *pRB = *pMB = 0; *pZ = 0; }
	for (int i = 0; i < 3; i++) m_bLatched[i] = false;
}

void CMouse::UpdateMouseState()
{
	_Poll();
	ms::X = m_sX;
	ms::Y = m_sY;
	ms::Z = m_sZ;
	ms::LB = (char)((m_bDown[0] || m_bLatched[0]) ? 0x80 : 0);
	ms::RB = (char)((m_bDown[1] || m_bLatched[1]) ? 0x80 : 0);
	ms::MB = (char)((m_bDown[2] || m_bLatched[2]) ? 0x80 : 0);
	if (_HideButtonsFromGame()) { ms::LB = ms::RB = ms::MB = 0; ms::Z = 0; }
	for (int i = 0; i < 3; i++) m_bLatched[i] = false;
}

bool CMouse::_HideButtonsFromGame()
{
	if (m_bUiCapture) {
		if (m_bLatched[0]) m_bUiClicked = true;	// keep the click for the UI
		return true;
	}
	if (m_bSuppress) {
		if (!m_bDown[0] && !m_bDown[1] && !m_bDown[2]) m_bSuppress = false;
		return true;
	}
	return false;
}
