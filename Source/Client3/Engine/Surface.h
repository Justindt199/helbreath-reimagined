// Surface.h: a plain system-memory 16-bit (RGB565) pixel surface.
//
// Replaces IDirectDrawSurface7. The game has always rendered in software into
// 16-bit surfaces; DirectDraw only provided the memory and a colour-keyed blit.
// This class provides exactly that, with no dependency on DirectX.
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_SURFACE_H
#define HB_ENGINE_SURFACE_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <windows.h>
#include <vector>

// Blit flags (replacements for DDBLTFAST_NOCOLORKEY / DDBLTFAST_SRCCOLORKEY)
#define BLT_NOCOLORKEY		0x0
#define BLT_SRCCOLORKEY		0x1

class CSurface
{
public:
	CSurface(int iWidth, int iHeight);
	~CSurface();

	WORD *	GetPixels()				{ return m_pixels.empty() ? NULL : &m_pixels[0]; }
	const WORD * GetPixels() const	{ return m_pixels.empty() ? NULL : &m_pixels[0]; }
	int		GetWidth()  const		{ return m_iWidth; }
	int		GetHeight() const		{ return m_iHeight; }
	int		GetPitch()  const		{ return m_iPitch; }	// in pixels (WORDs), not bytes

	void	SetColorKey(WORD wKey)	{ m_wColorKey = wKey; }
	WORD	GetColorKey() const		{ return m_wColorKey; }

	void	Clear(WORD wValue = 0);

	// Copies pSrcRect of pSrc to (dX, dY) on this surface. Clips against both surfaces.
	// With BLT_SRCCOLORKEY, source pixels equal to pSrc's colour key are skipped.
	void	BltFast(int dX, int dY, const CSurface * pSrc, const RECT * pSrcRect, DWORD dwFlags);

	// Loads a BMP (as stored in the game's .pak files) starting at dwOffset in pFileName
	// and converts it to RGB565. Returns NULL on failure.
	static CSurface * CreateFromBitmapFile(const char * pFileName, DWORD dwOffset);
	static CSurface * CreateFromBitmapMemory(const void * pData, size_t iSize);

private:
	std::vector<WORD> m_pixels;
	int  m_iWidth, m_iHeight, m_iPitch;
	WORD m_wColorKey;
};

#endif // HB_ENGINE_SURFACE_H
