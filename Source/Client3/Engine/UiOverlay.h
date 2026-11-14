// UiOverlay.h: interface for UI drawn on top of the game at the window's full resolution.
//
// The game itself is drawn into the 640x480 back buffer and scaled up to the window.
// An overlay is drawn after that, straight into the SFML window in window pixels,
// so its text and shapes stay sharp at any window size.
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_UIOVERLAY_H
#define HB_ENGINE_UIOVERLAY_H

#if _MSC_VER >= 1000
#pragma once
#endif

namespace sf { class RenderWindow; }

class CUiOverlay
{
public:
	virtual ~CUiOverlay() {}
	virtual bool IsVisible() const = 0;
	// Called by CRenderer::iFlip() after the game image is drawn. The window's view is
	// set so that one unit is one window pixel.
	virtual void Draw(sf::RenderWindow & window) = 0;
};

#endif // HB_ENGINE_UIOVERLAY_H
