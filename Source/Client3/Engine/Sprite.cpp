// Sprite.cpp: implementation of the CSprite class.
//
//////////////////////////////////////////////////////////////////////

#include "Sprite.h"
#include "UiGpu.h"
#include <algorithm>

extern char G_cSpriteShadeDegree;	// the night shading of the sprites (1 day, 2 night; UI\NightLights.cpp)

extern int G_iAddTable31[64][510], G_iAddTable63[64][510];
extern int G_iAddTransTable31[510][64], G_iAddTransTable63[510][64]; 

extern long    G_lTransG100[64][64], G_lTransRB100[64][64];
extern long    G_lTransG70[64][64], G_lTransRB70[64][64];
extern long    G_lTransG50[64][64], G_lTransRB50[64][64];
extern long    G_lTransG25[64][64], G_lTransRB25[64][64];
extern long    G_lTransG2[64][64], G_lTransRB2[64][64];


// ---------------------------------------------------------------------------- HD rendering

bool CSprite::_HdNight(const CSprite * s) const
{
	return s != this && s->m_bNightTint && G_cSpriteShadeDegree == 2;
}

// Set up at the start of a drawing function: while a frame is drawn on the graphics card, the
// draw becomes a step in its list (CSprite::_MirrorRecord). The function then only works out
// m_rcBound (for the mouse) and stops: the normal back buffer isn't shown.
class CHdMirrorScope
{
public:
	CHdMirrorScope(CSprite * pSpr, int sFrame, int sX, int sY, const HdMirrorArgs & a) : m_bRec(false)
	{
		if (pSpr == NULL || pSpr->m_stBrush == NULL || sFrame < 0 || sFrame >= pSpr->m_iTotalFrame) return;
		CRenderer * r = pSpr->m_pRenderer;
		if (r == NULL || !r->UiRecording()) return;
		RECT rc;
		m_bRec = true;		// (a draw entirely off the screen: nothing to add, and nothing to draw)
		if (!pSpr->_MirrorRect(sFrame, sX, sY, a, rc)) return;
		pSpr->_MirrorRecord(sFrame, sX, sY, a);
	}
	bool Recorded() const { return m_bRec; }
private:
	bool m_bRec;
};

static HdMirrorArgs HdTrans(const long (*tRB)[64], const long (*tG)[64], bool bKey)
{
	HdMirrorArgs a; a.op = HdMirrorArgs::TRANS; a.tRB = tRB; a.tG = tG; a.bKey = bKey; return a;
}

bool CSprite::_PrepareSurface()
{
	if (m_stBrush == NULL) return false;
	if (m_bIsSurfaceEmpty == TRUE) {
		if (_iOpenSprite() == FALSE) return false;
	}
	else if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {	// day / night shading
		if (G_cSpriteShadeDegree == 2) _SetAlphaDegree();
		else { _CloseOwn(); if (_iOpenSprite() == FALSE) return false; }
	}
	return m_lpSurface != NULL;
}

// Looks for this sheet in sprites\HD\ (same file name, same sheet number, same frame count).
void CSprite::_LoadHd()
{
	if (memcmp(m_cPakFileName, "HD\\", 3) == 0) return;		// this already is an HD copy
	char cName[40], cPath[80];
	wsprintf(cName, "HD\\%s", m_cPakFileName);
	if (strlen(cName) >= sizeof(m_cPakFileName)) return;
	const bool bLgn = (memcmp(m_cPakFileName, "lgn_", 4) == 0);
	wsprintf(cPath, "sprites\\%s.%s", cName, bLgn ? "lpk" : "pak");
	HANDLE hFile = CreateFile(cPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return;
	DWORD n = 0;
	const DWORD dwSize = GetFileSize(hFile, NULL);
	int iTotal = 0, iStart = 0, iFrames = 0;
	SetFilePointer(hFile, 20, NULL, FILE_BEGIN);
	ReadFile(hFile, &iTotal, 4, &n, NULL);
	bool bOk = (iTotal > m_sNthFile) || (bLgn && (iTotal ^ 712345) > m_sNthFile);
	if (bOk) {
		SetFilePointer(hFile, 24 + m_sNthFile * 8, NULL, FILE_BEGIN);
		ReadFile(hFile, &iStart, 4, &n, NULL);
		bOk = (iStart > 0 && (DWORD)iStart + 108 < dwSize);
	}
	if (bOk) {
		SetFilePointer(hFile, iStart + 100, NULL, FILE_BEGIN);
		ReadFile(hFile, &iFrames, 4, &n, NULL);
		bOk = (iFrames == m_iTotalFrame && iFrames > 0);
	}
	if (bOk) {
		CSprite * h = new CSprite(hFile, m_pRenderer, cName, m_sNthFile, false, NULL);	// see m_bNightTint
		h->m_bNightTint = m_bAlphaEffect;
		h->m_bHdCopy = true;
		if (h->m_stBrush != NULL) m_pHD = h;
		else delete h;
	}
	CloseHandle(hFile);
}

CSprite * CSprite::_GetHd()
{
	if (m_pRenderer == NULL || !m_pRenderer->m_bHdSprites) return NULL;
	if (m_pHD == NULL) {
		if (m_bHdTried) return NULL;
		m_bHdTried = true;
		_LoadHd();
		if (m_pHD == NULL) return NULL;
	}
	m_pHD->m_dwRefTime = m_dwRefTime;
	// (its pixels are on the graphics card: read from disk only when it needs them, see m_bHdCopy;
	// the first time, to learn the sheet's size)
	if (m_pHD->m_iSheetW <= 0 && !m_pHD->_PrepareSurface()) return NULL;
	if (m_pHD->m_iSheetW <= 0 || m_pHD->m_stBrush == NULL) return NULL;
	return m_pHD;
}

// The game pixels a draw may change (one pixel extra all round, for the HD copy's edges).
bool CSprite::_MirrorRect(int sFrame, int sX, int sY, const HdMirrorArgs & a, RECT & rc)
{
	const stBrush & b = m_stBrush[sFrame];
	const int W = RENDER_WIDTH, H = RENDER_HEIGHT;
	int x0, y0, x1, y1;
	if (a.bShadow) {
		x0 = sX + b.pvx - b.szy / 3 - 2;	x1 = sX + b.pvx + b.szx + 1;
		y0 = sY + b.pvy + (2 * b.szy) / 3 - 1;	y1 = sY + b.pvy + b.szy + 1;
		if (y1 > H - 53) y1 = H - 53;
	}
	else {
		int w = b.szx, h = b.szy;
		if (a.fixedW > 0) { w = a.fixedW; h = a.fixedH; }
		if (a.width >= 0 && a.width < w) w = a.width;
		x0 = sX + (int)(b.pvx * a.stretchX + 0.5f) - 1;
		x1 = x0 + 1 + (int)(w * a.stretchX + 0.5f) + 1;
		y0 = sY + b.pvy - 1;
		y1 = y0 + h + 2;
		const RECT & c = m_pRenderer->m_rcClipArea;
		if (x0 < c.left) x0 = c.left;
		if (y0 < c.top) y0 = c.top;
		if (x1 > c.right) x1 = c.right;
		if (y1 > c.bottom) y1 = c.bottom;
	}
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > W) x1 = W;
	if (y1 > H) y1 = H;
	if (x0 >= x1 || y0 >= y1) return false;
	SetRect(&rc, x0, y0, x1, y1);
	return true;
}

// The draw, as a step for the graphics card: the same
// sheet (the HD one when there is one), part of it, place, size, clipping and blending.
bool CSprite::_MirrorRecord(int sFrame, int sX, int sY, const HdMirrorArgs & a)
{
	CUiGpu * pG = m_pRenderer->UiGpuRecorder();
	if (pG == NULL) return false;
	const bool bThisOk = _PrepareSurface();
	CSprite * h = _GetHd();
	if (h != NULL && sFrame >= h->m_iTotalFrame) h = NULL;
	CSprite * s = (h != NULL) ? h : this;
	if (h == NULL && !bThisOk) return true;		// nothing to draw
	if (h == NULL && (m_lpSurface == NULL || m_lpSurface->GetPixels() == NULL)) return true;
	const int k = (h != NULL) ? 1 : 2;		// 2x pixels per sheet pixel
	const int u = (h != NULL) ? 2 : 1;		// sheet pixels per game pixel
	const stBrush & b = s->m_stBrush[sFrame];
	const stBrush & b1 = m_stBrush[sFrame];
	const int iSheetW = (h != NULL) ? h->m_iSheetW : m_lpSurface->GetWidth();
	const int iSheetH = (h != NULL) ? h->m_iSheetH : m_lpSurface->GetHeight();
	const bool bMask = (h != NULL) && a.bKey && bThisOk && m_lpSurface != NULL &&
		s->m_stBrush[sFrame].szx == 2 * b1.szx && s->m_stBrush[sFrame].szy == 2 * b1.szy;

	if (a.bShadow) {
		// the slanted shadow (see PutShadowSprite): the frame squashed to a third of its height and
		// slanted left by a third of it, darkening what is there, not below the bottom bar
		const float fh3 = b.szy / 3.f;
		const float ox = (float)m_pRenderer->m_iShiftX2, oy = (float)m_pRenderer->m_iShiftY2;
		const float xTop = 2.f * sX + k * (b.pvx - fh3) + ox, yTop = 2.f * sY + k * (b.pvy + 2.f * fh3) + oy;
		const float xBot = 2.f * sX + k * b.pvx + ox, yBot = 2.f * sY + k * (b.pvy + (float)b.szy) + oy;
		pG->AddShadow(s, bMask ? this : NULL, b.sx, b.sy, b.szx, b.szy, xTop, yTop, xBot, yBot, (float)(k * b.szx),
			2.f * (RENDER_HEIGHT - 53));
		return true;
	}

	int fx = b.sx, fy = b.sy, fw = b.szx, fh = b.szy;
	if (a.fixedW > 0) { fx += a.shX * u; fy += a.shY * u; fw = a.fixedW * u; fh = a.fixedH * u; }
	if (a.width >= 0 && a.width * u < fw) fw = a.width * u;
	if (fx < 0 || fy < 0) return true;
	if (fx + fw > iSheetW) fw = iSheetW - fx;
	if (fy + fh > iSheetH) fh = iSheetH - fy;
	if (fw <= 0 || fh <= 0) return true;

	const float sc = a.stretchX;
	const int X0 = 2 * sX + (int)(b.pvx * k * sc + 0.5f) + m_pRenderer->m_iShiftX2;
	const int Y0 = 2 * sY + b.pvy * k + m_pRenderer->m_iShiftY2;
	const int DW = (int)(fw * k * sc + 0.5f), DH = fh * k;
	CRenderer * r = m_pRenderer;
	const RECT & c = r->m_rcClipArea;
	const int W2 = 2 * RENDER_WIDTH, H2 = 2 * RENDER_HEIGHT;
	const int cx0 = (std::max)(2 * (int)c.left, 0), cy0 = (std::max)(2 * (int)c.top, 0);
	const int cx1 = (std::min)(2 * (int)c.right, W2), cy1 = (std::min)(2 * (int)c.bottom, H2);
	if (cx0 >= cx1 || cy0 >= cy1) return true;

	int iMode = UISPR_COPY, add[3] = { 0, 0, 0 };
	float fAlpha = 1.f;
	switch (a.op) {
	case HdMirrorArgs::TRANS:
		if (a.tRB == G_lTransRB2 || a.tRB == r->m_lTransRB2) iMode = UISPR_AVERAGE;
		else {
			iMode = UISPR_ADD;
			if (a.tRB == G_lTransRB70 || a.tRB == r->m_lTransRB70) fAlpha = 0.7f;
			else if (a.tRB == G_lTransRB50 || a.tRB == r->m_lTransRB50) fAlpha = 0.5f;
			else if (a.tRB == G_lTransRB25 || a.tRB == r->m_lTransRB25) fAlpha = 0.25f;
		}
		break;
	case HdMirrorArgs::SHADE:    iMode = UISPR_SHADE; break;
	case HdMirrorArgs::RGB:      iMode = UISPR_TINT;    add[0] = a.r - 255; add[1] = a.g - 255; add[2] = a.b - 255; break;
	case HdMirrorArgs::TRANSRGB: iMode = UISPR_ADDTINT; add[0] = a.r - 255; add[1] = a.g - 255; add[2] = a.b - 255; break;
	case HdMirrorArgs::REVTRANS: iMode = UISPR_SUBTRACT; add[0] = a.alphaDepth; add[1] = 2 * a.alphaDepth; add[2] = a.alphaDepth; break;
	default: break;
	}
	pG->AddSprite(s, bMask ? this : NULL, fx, fy, fw, fh, (float)X0, (float)Y0, (float)DW, (float)DH,
		cx0, cy0, cx1, cy1, iMode, a.bKey, fAlpha, add[0], add[1], add[2], _HdNight(s));
	return true;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CSprite::CSprite(HANDLE hPakFile, CRenderer *pRenderer, char *cPakFileName, short sNthFile, bool bAlphaEffect, std::vector<int> * framePositions)
{
	DWORD  nCount;
	int iASDstart;

	m_stBrush	= NULL;
	m_lpSurface = NULL;
	m_bIsSurfaceEmpty = TRUE;
	ZeroMemory(m_cPakFileName, sizeof(m_cPakFileName));

	m_cAlphaDegree = 1;
	m_bOnCriticalSection = FALSE;
	m_iTotalFrame = 0;
	m_pRenderer = pRenderer;
	if (framePositions) {
		iASDstart = (*framePositions)[sNthFile];
	} 
	else {
		SetFilePointer(hPakFile, 24 + sNthFile*8, NULL, FILE_BEGIN);
		ReadFile(hPakFile, &iASDstart,  4, &nCount, NULL); 
	}
	//i+100       Sprite Confirm
	SetFilePointer(hPakFile, iASDstart+100, NULL, FILE_BEGIN); 
	ReadFile(hPakFile, &m_iTotalFrame,  4, &nCount, NULL);
	m_dwBitmapFileStartLoc = iASDstart  + (108 + (12*m_iTotalFrame));
	m_stBrush = new stBrush[m_iTotalFrame];
	ReadFile(hPakFile, m_stBrush, 12*m_iTotalFrame, &nCount, NULL);
	// PAK
	memcpy(m_cPakFileName, cPakFileName, std::min<size_t>(strlen(cPakFileName), sizeof(m_cPakFileName) - 1));
	m_bAlphaEffect = bAlphaEffect;
	m_sNthFile = sNthFile;
}

CSprite::~CSprite()
{
	if (m_pHD != NULL) { delete m_pHD; m_pHD = NULL; }
	if (m_stBrush != NULL) delete[] m_stBrush;
	if (m_lpSurface != NULL) delete m_lpSurface;
}

CSurface * CSprite::_pMakeSpriteSurface()
{
 CSurface * pSurface;
 char cPathName[64];

	m_bOnCriticalSection = TRUE;

	if( m_stBrush == NULL ) return NULL;

	// The sprite sheet is a BMP stored inside the .pak (.lpk for "lgn_" files).
	const char * pBase = (memcmp(m_cPakFileName, "HD\\", 3) == 0) ? m_cPakFileName + 3 : m_cPakFileName;
	if (memcmp(pBase, "lgn_", 4) == 0)
		wsprintf(cPathName, "sprites\\%s.lpk", m_cPakFileName);
	else
		wsprintf(cPathName, "sprites\\%s.pak", m_cPakFileName);

	pSurface = CSurface::CreateFromBitmapFile(cPathName, m_dwBitmapFileStartLoc);
	if (pSurface == NULL) return NULL;

	m_wBitmapSizeX = (WORD)pSurface->GetWidth();
	m_wBitmapSizeY = (WORD)pSurface->GetHeight();

	// The top-left pixel is the transparent colour, as before.
	m_wColorKey = pSurface->GetPixels()[0];

	m_bOnCriticalSection = FALSE;

	return pSurface;
}


void CSprite::PutSpriteFast(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
	RECT rcRect;
	if( this == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if( m_stBrush == NULL ) return;
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdMirrorArgs());	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left)
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx <= 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	// (not "else": a sprite wider than the clip area is cut on both sides)
	if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx <= 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top)
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy <= 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy <= 0) {
			m_rcBound.top = -1;
			return;
		}
	}
	
	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE)
	{
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	rcRect.left = sx;
	rcRect.top  = sy;
	rcRect.right  = sx + szx;
	rcRect.bottom = sy + szy;

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutSpriteFastNoColorKey(int sX, int sY, int sFrame, DWORD dwTime)
{short dX,dY,sx,sy,szx,szy,pvx,pvy;
 RECT rcRect;
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.bKey = false;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;
	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;
  	dX = sX + pvx;
	dY = sY + pvy;
	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{	sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx <= 0) 
		{	m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{	szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx <= 0) 
		{	m_rcBound.top = -1;
			return;
	}	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{	sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy <= 0) 
		{	m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{	szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy <= 0) 
		{	m_rcBound.top = -1;
			return;
	}	}
	
	m_dwRefTime = dwTime;
	if (m_bIsSurfaceEmpty == TRUE)
	{	if( _iOpenSprite() == FALSE ) return;
	}else 
	{	if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) 
			{	_SetAlphaDegree();
			}else 
			{	_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
	}	}	}

	//SetRect(&rcRect,  sx, sy, sx + szx, sy + szy); // our fictitious sprite bitmap is 
	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	rcRect.left = sx;
	rcRect.top  = sy;
	rcRect.right  = sx + szx;
	rcRect.bottom = sy + szy;

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutSpriteStretchX(int sX, int sY, int sFrame, float fScaleX, int sWidth, DWORD dwTime)
{
	if (this == NULL) return;
	m_rcBound.top = -1;
	if (m_stBrush == NULL) return;
	if ((m_iTotalFrame - 1 < sFrame) || (sFrame < 0)) return;
	if (fScaleX == 1.0f) {
		if (sWidth < 0) PutSpriteFast(sX, sY, sFrame, dwTime);
		else PutSpriteFastWidth(sX, sY, sFrame, sWidth, dwTime);
		return;
	}
	HdMirrorArgs hda; hda.stretchX = fScaleX; hda.width = sWidth;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);
	int szx = m_stBrush[sFrame].szx;
	const int szy = m_stBrush[sFrame].szy;
	if (sWidth >= 0 && sWidth < szx) szx = sWidth;
	if (szx <= 0 || szy <= 0 || fScaleX <= 0) return;

	m_dwRefTime = dwTime;
	if (m_bIsSurfaceEmpty == TRUE) {
		if (_iOpenSprite() == FALSE) return;
	}
	else if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
		if (G_cSpriteShadeDegree == 2) _SetAlphaDegree();
		else { _CloseOwn(); if (_iOpenSprite() == FALSE) return; }
	}
	if (m_lpSurface == NULL) return;
	m_bOnCriticalSection = TRUE;

	// destination columns [dX0, dX1), rows [dY0, dY0+szy)
	const int dX0 = sX + (int)(m_stBrush[sFrame].pvx * fScaleX + 0.5f);
	const int dX1 = dX0 + (int)(szx * fScaleX + 0.5f);
	const int dY0 = sY + m_stBrush[sFrame].pvy;
	const RECT & rc = m_pRenderer->m_rcClipArea;
	const int cx0 = std::max<int>(std::max<int>(dX0, rc.left), 0);
	const int cx1 = std::min<int>(std::min<int>(dX1, rc.right), RENDER_WIDTH);
	const int cy0 = std::max<int>(std::max<int>(dY0, rc.top), 0);
	const int cy1 = std::min<int>(std::min<int>(dY0 + szy, rc.bottom), RENDER_HEIGHT);
	if (cx0 >= cx1 || cy0 >= cy1) { m_bOnCriticalSection = FALSE; return; }
	m_rcBound.left = cx0; m_rcBound.top = cy0; m_rcBound.right = cx1; m_rcBound.bottom = cy1;
	m_bOnCriticalSection = FALSE;
}

void CSprite::PutSpriteFastWidth(int sX, int sY, int sFrame, int sWidth, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 RECT rcRect;
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.width = sWidth;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (sWidth < szx)
		szx = sWidth;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx <= 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx <= 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy <= 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy <= 0) {
			m_rcBound.top = -1;
			return;
		}
	}
	
	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&rcRect,  sx, sy, sx + szx, sy + szy); // our fictitious sprite bitmap is 
	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	rcRect.left = sx;
	rcRect.top  = sy;
	rcRect.right  = sx + szx;
	rcRect.bottom = sy + szy;

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutShadowSprite(int sX, int sY, int sFrame, DWORD dwTime)
{

	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.bShadow = true;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	// (only drawn on the graphics card: CHdMirrorScope)
	m_dwRefTime = dwTime;
}


void CSprite::PutShadowSpriteClip(int sX, int sY, int sFrame, DWORD dwTime)
{

	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.op = HdMirrorArgs::SHADE;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	// (only drawn on the graphics card: CHdMirrorScope)
	m_dwRefTime = dwTime;
}


void CSprite::PutTransSprite(int sX, int sY, int sFrame, DWORD dwTime, int alphaDepth)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB100, G_lTransG100, true));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::PutTransSprite_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime, int alphaDepth)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB100, G_lTransG100, false));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::PutTransSprite70(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB70, G_lTransG70, true));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutTransSprite70_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB70, G_lTransG70, false));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::PutTransSprite50(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB50, G_lTransG50, true));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutTransSprite50_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB50, G_lTransG50, false));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::PutTransSprite25(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB25, G_lTransG25, true));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutTransSprite25_NoColorKey(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB25, G_lTransG25, false));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutTransSprite2(int sX, int sY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	CHdMirrorScope hdm(this, sFrame, sX, sY, HdTrans(G_lTransRB2, G_lTransG2, true));	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;

	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	
	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::PutShiftTransSprite2(int sX, int sY, int shX, int shY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda = HdTrans(G_lTransRB2, G_lTransG2, true); hda.shX = shX; hda.shY = shY; hda.fixedW = 128; hda.fixedH = 128;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = 128;//m_stBrush[sFrame].szx;
	szy = 128;//m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

	sx += shX;
	sy += shY;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;

	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	
	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutFadeSprite(short sX, short sY, short sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 //int dX,dY,sx,sy,szx,szy,pvx,pvy,sTmp;

	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.op = HdMirrorArgs::SHADE;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}
	
	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


bool CSprite::_iOpenSprite()
{
  	if (m_lpSurface != NULL) return FALSE;
	m_lpSurface = _pMakeSpriteSurface(); 
	if (m_lpSurface == NULL) return FALSE;
	m_lpSurface->SetColorKey(m_wColorKey);
	m_bIsSurfaceEmpty  = FALSE;
	m_pSurfaceAddr = m_lpSurface->GetPixels();
	m_sPitch = (short)m_lpSurface->GetPitch();
	m_iSheetW = m_lpSurface->GetWidth();
	m_iSheetH = m_lpSurface->GetHeight();
	_SetAlphaDegree();
	// (an HD copy reads the same pixels each time: its copy on the graphics card stays good)
	if (!m_bHdCopy || m_uSheetGen == 0) m_uSheetGen = CUiGpu::NextSheetGen();
//	m_dwRefTime = timeGetTime();
	return TRUE;
}

void CSprite::_iCloseSprite()
{
	if (m_pHD != NULL) m_pHD->_iCloseSprite();		// its HD copy goes with it
	_CloseOwn();
}

void CSprite::_CloseOwn()
{
	if( m_stBrush == NULL ) return;
	if (m_lpSurface == NULL) return;
	delete m_lpSurface;
	m_lpSurface = NULL;
	m_bIsSurfaceEmpty = TRUE;
	m_cAlphaDegree = 1;
	if (!m_bHdCopy) m_uSheetGen = CUiGpu::NextSheetGen();
}

void CSprite::PutSpriteRGB(int sX, int sY, int sFrame, int sRed, int sGreen, int sBlue, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.op = HdMirrorArgs::RGB; hda.r = sRed + 255; hda.g = sGreen + 255; hda.b = sBlue + 255;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}
	
	m_dwRefTime = dwTime;
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	if ((szx == 0) || (szy == 0)) return;

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}


void CSprite::PutTransSpriteRGB(int sX, int sY, int sFrame, int sRed, int sGreen, int sBlue, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.op = HdMirrorArgs::TRANSRGB; hda.r = sRed + 255; hda.g = sGreen + 255; hda.b = sBlue + 255;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	if ((szx == 0) || (szy == 0)) return;

	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::PutTransSpriteRGB_NoColorKey(int sX, int sY, int sFrame, int sRed, int sGreen, int sBlue, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.op = HdMirrorArgs::TRANSRGB; hda.bKey = false; hda.r = sRed + 255; hda.g = sGreen + 255; hda.b = sBlue + 255;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	m_dwRefTime = dwTime;
	
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::_GetSpriteRect(int sX, int sY, int sFrame)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;	

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	
	m_rcBound.top    = -1;
	m_rcBound.bottom = -1;
	m_rcBound.left   = -1;
	m_rcBound.right  = -1;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}
	
	SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_sPivotX = pvx;
	m_sPivotY = pvy;
}

void CSprite::_SetAlphaDegree()
{
 WORD * pSrc, wR, wG, wB, wTemp, ix, iy;
 int iR, iG, iB, sRed, sGreen, sBlue;
 
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_bOnCriticalSection = TRUE;
	if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
		m_uSheetGen = CUiGpu::NextSheetGen();	// its pixels change
		m_cAlphaDegree = G_cSpriteShadeDegree;
		switch (m_cAlphaDegree) {
		case 1:
			sRed = sGreen = sBlue = 0;
			break;

		case 2:
			sRed   = -3;
			sGreen = -3;
			sBlue  =  2;
			break;
		}

		pSrc = (WORD *)m_pSurfaceAddr;
		
		switch (m_pRenderer->m_cPixelFormat) {
		case 1:
			for (iy = 0; iy < m_wBitmapSizeY; iy++) 
			{	for (ix = 0; ix < m_wBitmapSizeX; ix++) 
				{	if (pSrc == NULL) return; 
					if (pSrc[ix] != m_wColorKey) 
					{	wR = (WORD)(pSrc[ix]&0xF800)>>11;
						wG = (WORD)(pSrc[ix]&0x7E0)>>5;
						wB = (WORD)(pSrc[ix]&0x1F);
						iR = (int)wR + sRed;
						iG = (int)wG + sGreen;
						iB = (int)wB + sBlue;
						
						if (iR < 0) iR = 0;
						else if (iR > 31) iR = 31;
						if (iG < 0) iG = 0;
						else if (iG > 63) iG = 63;
						if (iB < 0) iB = 0;
						else if (iB > 31) iB = 31;
						
						wTemp = (WORD)((iR<<11) | (iG<<5) | iB);
						if (wTemp != m_wColorKey)
							 pSrc[ix] = wTemp;
						else pSrc[ix] = (WORD)((iR<<11) | (iG<<5) | (iB+1));
				}	}
				pSrc += m_sPitch;
			}
			break;
			
		case 2:
			for (iy = 0; iy < m_wBitmapSizeY; iy++) 
			{	for (ix = 0; ix < m_wBitmapSizeX; ix++) 
				{	if (pSrc == NULL) return; 
					if (pSrc[ix] != m_wColorKey)	
					{	wR = (WORD)(pSrc[ix]&0x7C00)>>10;
						wG = (WORD)(pSrc[ix]&0x3E0)>>5;
						wB = (WORD)(pSrc[ix]&0x1F);
						iR = (int)wR + sRed;
						iG = (int)wG + sGreen;
						iB = (int)wB + sBlue;						
						if (iR < 0) iR = 0;
						else if (iR > 31) iR = 31;
						if (iG < 0) iG = 0;
						else if (iG > 31) iG = 31;
						if (iB < 0) iB = 0;
						else if (iB > 31) iB = 31;						
						wTemp = (WORD)((iR<<10) | (iG<<5) | iB);
						if (wTemp != m_wColorKey)
							 pSrc[ix] = wTemp;
						else pSrc[ix] = (WORD)((iR<<10) | (iG<<5) | (iB+1));				
				}	}
				pSrc += m_sPitch;
			}
			break;	
	}	}

	m_bOnCriticalSection = FALSE;
}

BOOL CSprite::_bCheckCollison(int sX, int sY, short sFrame, int msX, int msY)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
	int  ix, iy;
	WORD * pSrc;
	int  tX, tY;
	
	if( this == NULL ) return FALSE;
	if( m_stBrush == NULL ) return FALSE;
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return FALSE;
	if( m_bIsSurfaceEmpty == TRUE ) return FALSE;
	if( msX < m_pRenderer->m_rcClipArea.left+3 ) return FALSE;
	if( msX > m_pRenderer->m_rcClipArea.right-3 ) return FALSE;
	if( msY < m_pRenderer->m_rcClipArea.top+3 ) return FALSE;
	if( msY > m_pRenderer->m_rcClipArea.bottom-3 ) return FALSE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

  	dX = sX + pvx;
	dY = sY + pvy;

	if( msX < dX ) return FALSE;
	if( msX > dX+szx ) return FALSE;
	if( msY < dY ) return FALSE;
	if( msY > dY+szy ) return FALSE;

//	if (dX < m_pRenderer->m_rcClipArea.left+3) return FALSE;
//	if (dX+szx > m_pRenderer->m_rcClipArea.right-3) return FALSE;
//	if (dY < m_pRenderer->m_rcClipArea.top+3) return FALSE;
//	if (dY+szy > m_pRenderer->m_rcClipArea.bottom-3) return FALSE;

	if (dX < m_pRenderer->m_rcClipArea.left+3)
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left+3 - dX);
		szx = szx - (m_pRenderer->m_rcClipArea.left+3 - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return FALSE;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left+3;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right-3)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right-3);
		if (szx < 0) {
			m_rcBound.top = -1;
			return FALSE;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top+3)
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top+3 - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top+3 - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return FALSE;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top+3;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom-3)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom-3);
		if (szy < 0) {
			m_rcBound.top = -1;
			return FALSE;
		}
	}
	
	SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);

	pSrc = (WORD *)m_pSurfaceAddr + sx + ((sy)*m_sPitch);
	tX = dX;
	tY = dY;


//	pSrc += m_sPitch * ( msY - tY );
//	if( pSrc[msX-tX] != m_wColorKey ) return TRUE;
//	else return FALSE;

	if( msY < tY + 3 ) return FALSE;
	if( msX < tX + 3 ) return FALSE;
	pSrc += m_sPitch * ( msY - tY - 3 );
	for( iy=0 ; iy<=6 ; iy++ )
	{
		for( ix=msX-tX-3 ; ix<=msX-tX+3 ; ix++ )
		{
			if( pSrc[ix] != m_wColorKey ) return TRUE;
		}
		pSrc += m_sPitch;
	}
	return FALSE;
}

void CSprite::PutShiftSpriteFast(int sX, int sY, int shX, int shY, int sFrame, DWORD dwTime)
{
	short dX,dY,sx,sy,szx,szy,pvx,pvy;
 RECT rcRect;
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.shX = shX; hda.shY = shY; hda.fixedW = 128; hda.fixedH = 128;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;

	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = 128;//m_stBrush[sFrame].szx;
	szy = 128;//m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;

	sx += shX;
	sy += shY;

  	dX = sX + pvx;
	dY = sY + pvy;

	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx <= 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx <= 0) {
			m_rcBound.top = -1;
			return;
		}
	}

	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy <= 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy <= 0) {
			m_rcBound.top = -1;
			return;
		}
	}
	
	m_dwRefTime = dwTime;

	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}

	//SetRect(&rcRect,  sx, sy, sx + szx, sy + szy); // our fictitious sprite bitmap is 
	//SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	rcRect.left = sx;
	rcRect.top  = sy;
	rcRect.right  = sx + szx;
	rcRect.bottom = sy + szy;

	m_rcBound.left = dX;
	m_rcBound.top  = dY;
	m_rcBound.right  = dX + szx;
	m_rcBound.bottom = dY + szy;
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void CSprite::PutRevTransSprite(int sX, int sY, int sFrame, DWORD dwTime, int alphaDepth)
{
	int dX,dY,sx,sy,szx,szy,pvx,pvy;//,sTmp;
	
	if( this == NULL ) return;
	if( m_stBrush == NULL ) return;
	m_rcBound.top = -1; // Fix by Snoopy.... (Reco at mine)
	if ((m_iTotalFrame-1 < sFrame) || (sFrame < 0)) return;
	HdMirrorArgs hda; hda.op = HdMirrorArgs::REVTRANS; hda.alphaDepth = alphaDepth;
	CHdMirrorScope hdm(this, sFrame, sX, sY, hda);	
	m_bOnCriticalSection = TRUE;
	
	sx  = m_stBrush[sFrame].sx;
	sy  = m_stBrush[sFrame].sy;
	szx = m_stBrush[sFrame].szx;
	szy = m_stBrush[sFrame].szy;
	pvx = m_stBrush[sFrame].pvx;
	pvy = m_stBrush[sFrame].pvy;
	
	dX = sX + pvx;
	dY = sY + pvy;
		
	if (dX < m_pRenderer->m_rcClipArea.left) 								  
	{
		sx = sx	+ (m_pRenderer->m_rcClipArea.left - dX);							
		szx = szx - (m_pRenderer->m_rcClipArea.left - dX);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
		dX = (short)m_pRenderer->m_rcClipArea.left;
	}
	else if (dX+szx > m_pRenderer->m_rcClipArea.right)
	{
		szx = szx - ((dX+szx) - (short)m_pRenderer->m_rcClipArea.right);
		if (szx < 0) {
			m_rcBound.top = -1;
			return;
		}
	}
		
	if (dY < m_pRenderer->m_rcClipArea.top) 								  
	{
		sy = sy	+ (m_pRenderer->m_rcClipArea.top - dY);
		szy = szy - (m_pRenderer->m_rcClipArea.top - dY);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
		dY = (short)m_pRenderer->m_rcClipArea.top;
	}
	else if (dY+szy > m_pRenderer->m_rcClipArea.bottom)
	{
		szy = szy - ((dY+szy) - (short)m_pRenderer->m_rcClipArea.bottom);
		if (szy < 0) {
			m_rcBound.top = -1;
			return;
		}
	}
		
	m_dwRefTime = dwTime;
		
	if (m_bIsSurfaceEmpty == TRUE) {
		if( _iOpenSprite() == FALSE ) return;
	}
	else {
		if (m_bAlphaEffect && (m_cAlphaDegree != G_cSpriteShadeDegree)) {
			if (G_cSpriteShadeDegree == 2) {
				_SetAlphaDegree();
			}
			else {
				_CloseOwn();
				if( _iOpenSprite() == FALSE ) return;
			}
		}
	}
	
	SetRect(&m_rcBound, dX, dY, dX + szx, dY + szy);
	m_bOnCriticalSection = FALSE;		// (only drawn on the graphics card: CHdMirrorScope)
}

void ReadFramePositions(HANDLE hPakFile, std::vector<int> & framePositions, int frames)
{
	DWORD * dwp, count;
	char * fileHeader = new char[frames*8 +8];
	SetFilePointer(hPakFile, 24, NULL, FILE_BEGIN);
	ReadFile(hPakFile, fileHeader,  frames *8, &count, NULL);
	dwp = (DWORD *) fileHeader;
	for(int i = 0; i < frames; i++, dwp+=2)
	{
		framePositions.push_back(*dwp);
	}
	delete [] fileHeader;
}

