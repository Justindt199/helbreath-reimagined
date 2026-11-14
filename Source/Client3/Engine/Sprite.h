// Sprite.h: interface for the CSprite class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SPRITE_H__0089D9E2_74E6_11D2_A8E6_00001C7030A6__INCLUDED_)
#define AFX_SPRITE_H__0089D9E2_74E6_11D2_A8E6_00001C7030A6__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "Renderer.h"
#include "Surface.h"

typedef struct stBrushtag
{
	short sx;
	short sy;
	short szx;
	short szy;
	short pvx;
	short pvy;
} stBrush;

void ReadFramePositions(HANDLE hPakFile, std::vector<int> & framePositions, int frames);

// How a draw is made on the graphics card (CSprite::_MirrorRecord)
struct HdMirrorArgs
{
	enum { COPY = 0, TRANS, SHADE, RGB, TRANSRGB, REVTRANS };
	int   op = COPY;
	bool  bKey = true;							// skip the sheet's transparent colour
	const long (*tRB)[64] = NULL;				// TRANS: the transparency tables used
	const long (*tG)[64] = NULL;
	int   r = 0, g = 0, b = 0;					// RGB / TRANSRGB: colour + 255, as the normal draw
	int   alphaDepth = 0;						// REVTRANS
	int   width = -1;							// only the first `width` columns (game pixels)
	float stretchX = 1.f;						// stretched sideways
	int   shX = 0, shY = 0, fixedW = 0, fixedH = 0;	// PutShift*: part of the sheet at an offset, fixed size
	bool  bShadow = false;						// PutShadowSprite: the slanted shadow
};

class CSprite  
{
public:
	void * operator new (size_t size) 
	{	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	};
	
	void operator delete(void * mem)
	{	HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, mem);
	};

	CSprite(HANDLE hPakFile, class CRenderer * pRenderer, char * cPakFileName, short sNthFile, bool bAlphaEffect = TRUE, std::vector<int> * framePositions = NULL);
	virtual ~CSprite();

	void PutSpriteRGB(int sX, int sY, int sFrame, int sRed, int sGreen, int sBlue, DWORD dwTime);

	void PutSpriteFast(int sX, int sY, int sFrame, DWORD dwTime);
	void PutShiftSpriteFast(int sX, int sY, int shX, int shY, int sFrame, DWORD dwTime);
	void PutShiftTransSprite2(int sX, int sY, int shX, int shY, int sFrame, DWORD dwTime);
	void PutSpriteFastWidth(int sX, int sY, int sFrame, int sWidth, DWORD dwTime);
	// Like PutSpriteFastWidth, but stretched sideways by fScaleX (sWidth < 0: the whole frame).
	void PutSpriteStretchX(int sX, int sY, int sFrame, float fScaleX, int sWidth, DWORD dwTime);
	// HD ground: draws a ground tile onto the 2x background surface at (2*sX, 2*sY), using the HD
	// copy when there is one and otherwise the normal tile with every pixel doubled. No colour
	// key, no clip rectangle (just the surface edges). Returns TRUE when the HD copy was used.
	void PutSpriteFastNoColorKey(int sX, int sY, int sFrame, DWORD dwTime);

	void PutTransSprite(int sX, int sY, int sFrame, DWORD dwTime, int alphaDepth = 30);
	void PutTransSprite2(int sX, int sY, int sFrame, DWORD dwTime);
	void PutTransSprite_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime, int alphaDepth = 0);
	void PutTransSpriteRGB_NoColorKey(int sX, int sY, int sFrame, int sRed, int sGreen, int sBlue, DWORD dwTime);
	void PutTransSpriteRGB(int sX, int sY, int sFrame, int sRed, int sGreen, int sBlue, DWORD dwTime);
	void PutTransSprite70(int sX, int sY, int sFrame, DWORD dwTime);
	void PutTransSprite50(int sX, int sY, int sFrame, DWORD dwTime);
	void PutTransSprite25(int sX, int sY, int sFrame, DWORD dwTime);
	void PutTransSprite70_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime);
	void PutTransSprite50_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime);
	void PutTransSprite25_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime);

	void PutShadowSpriteClip(int sX, int sY, int sFrame, DWORD dwTime);
	void PutShadowSprite(int sX, int sY, int sFrame, DWORD dwTime);

	void PutRevTransSprite(int sX, int sY, int sFrame, DWORD dwTime, int alphaDepth = 0);

	void PutFadeSprite(short sX, short sY, short sFrame, DWORD dwTime);


	void _SetAlphaDegree();
	BOOL _bCheckCollison(int sX, int sY, short sFrame, int msX, int msY);
	void _GetSpriteRect(int sX, int sY, int sFrame);
	// HD rendering: the double-resolution copy of this sheet (sprites\HD\<name>.pak or .lpk),
	// found the first time it is needed; NULL when there is none.
	CSprite * m_pHD;
	short m_sNthFile;			// which sheet of the file this is
	bool  m_bHdTried;
	CSprite * _GetHd();			// the HD copy, ready to draw from, or NULL
	void _LoadHd();
	bool _PrepareSurface();		// opens the sheet / applies day-night shading if needed
	bool _MirrorRect(int sFrame, int sX, int sY, const HdMirrorArgs & a, RECT & rc);	// game pixels a draw may touch
	// The same draw as a step for the interface on the graphics card (Engine\UiGpu.h), while
	// the renderer records one; false: nothing is being recorded.
	bool _MirrorRecord(int sFrame, int sX, int sY, const HdMirrorArgs & a);
	// changes whenever the sheet's pixels do (opened, closed, day / night shading), so a copy of
	// it on the graphics card knows to be remade (unique over all sheets; an HD copy: see m_bHdCopy)
	unsigned int m_uSheetGen = 0;
	// An HD copy is never shaded for the night in its pixels (that rewrote hundreds of MB at
	// dusk, and threw them away to reload from disk at dawn); it is shaded as it is drawn
	// instead, on the graphics card: m_bNightTint (its normal sheet takes the shading).
	bool m_bNightTint = false;
	bool _HdNight(const CSprite * s) const;	// s (this sheet or its HD copy) needs shading as drawn
	// An HD copy is only ever drawn on the graphics card, and its pixels never change: they are
	// read from disk to be sent there (CUiGpu), then freed (only the frame list stays in RAM),
	// and read again only if the graphics card has let them go. Its m_uSheetGen stays the same
	// across that. m_iSheetW / H: the sheet's size, known from its first opening.
	bool m_bHdCopy = false;
	int  m_iSheetW = 0, m_iSheetH = 0;
	void _iCloseSprite();
	void _CloseOwn();				// as _iCloseSprite, but its HD copy stays (day / night change)
	bool _iOpenSprite();
	CSurface * _pMakeSpriteSurface();
		
	RECT	m_rcBound;
	DWORD	m_dwRefTime;
	bool	m_bIsSurfaceEmpty;
	bool	m_bOnCriticalSection;
	bool	m_bAlphaEffect;
	short	m_sPivotX, m_sPivotY;
	class	CRenderer * m_pRenderer;
	WORD*	m_pSurfaceAddr;
	DWORD	m_dwBitmapFileStartLoc;
	short	m_sPitch;
	int		m_iTotalFrame;
	char	m_cAlphaDegree;
	WORD	m_wBitmapSizeX, m_wBitmapSizeY;
	WORD	m_wColorKey;
	char	m_cPakFileName[32];
	stBrush* m_stBrush;
	CSurface * m_lpSurface;
};

#endif // !defined(AFX_SPRITE_H__0089D9E2_74E6_11D2_A8E6_00001C7030A6__INCLUDED_)
