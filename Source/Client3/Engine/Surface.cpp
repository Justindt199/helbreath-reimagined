// Surface.cpp: implementation of CSurface (system-memory RGB565 surface).
//////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SFML/Graphics/Image.hpp>

#include <cstdint>
#include <cstdio>
#include <string.h>
#include <vector>

#include "Surface.h"

CSurface::CSurface(int iWidth, int iHeight)
{
	if (iWidth < 1)  iWidth = 1;
	if (iHeight < 1) iHeight = 1;
	m_iWidth  = iWidth;
	m_iHeight = iHeight;
	// Same row alignment DirectDraw used for these surfaces (multiple of 4 pixels)
	m_iPitch  = (iWidth + 3) & ~3;
	m_wColorKey = 0;
	m_pixels.assign((size_t)m_iPitch * (size_t)m_iHeight, 0);
}

CSurface::~CSurface()
{
}

void CSurface::Clear(WORD wValue)
{
	if (wValue == 0) memset(&m_pixels[0], 0, m_pixels.size() * sizeof(WORD));
	else for (size_t i = 0; i < m_pixels.size(); i++) m_pixels[i] = wValue;
}

void CSurface::BltFast(int dX, int dY, const CSurface * pSrc, const RECT * pSrcRect, DWORD dwFlags)
{
	if (pSrc == NULL) return;

	int sx = 0, sy = 0, sw = pSrc->m_iWidth, sh = pSrc->m_iHeight;
	if (pSrcRect != NULL) {
		sx = pSrcRect->left;
		sy = pSrcRect->top;
		sw = pSrcRect->right  - pSrcRect->left;
		sh = pSrcRect->bottom - pSrcRect->top;
	}

	// Clip source rect to the source surface
	if (sx < 0) { dX -= sx; sw += sx; sx = 0; }
	if (sy < 0) { dY -= sy; sh += sy; sy = 0; }
	if (sx + sw > pSrc->m_iWidth)  sw = pSrc->m_iWidth  - sx;
	if (sy + sh > pSrc->m_iHeight) sh = pSrc->m_iHeight - sy;

	// Clip destination to this surface
	if (dX < 0) { sx -= dX; sw += dX; dX = 0; }
	if (dY < 0) { sy -= dY; sh += dY; dY = 0; }
	if (dX + sw > m_iWidth)  sw = m_iWidth  - dX;
	if (dY + sh > m_iHeight) sh = m_iHeight - dY;

	if (sw <= 0 || sh <= 0) return;

	const WORD * pS = pSrc->GetPixels() + sx + sy * pSrc->m_iPitch;
	WORD * pD = GetPixels() + dX + dY * m_iPitch;

	if (dwFlags & BLT_SRCCOLORKEY) {
		const WORD wKey = pSrc->m_wColorKey;
		for (int y = 0; y < sh; y++) {
			for (int x = 0; x < sw; x++)
				if (pS[x] != wKey) pD[x] = pS[x];
			pS += pSrc->m_iPitch;
			pD += m_iPitch;
		}
	}
	else {
		for (int y = 0; y < sh; y++) {
			memcpy(pD, pS, sw * sizeof(WORD));
			pS += pSrc->m_iPitch;
			pD += m_iPitch;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Bitmap loading (replaces CMyDib + GDI SetDIBitsToDevice onto a DirectDraw surface)
//////////////////////////////////////////////////////////////////////

namespace
{
	inline WORD To565(unsigned r, unsigned g, unsigned b)
	{
		return (WORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
	}

	inline unsigned Rd16(const std::uint8_t * p) { return p[0] | (p[1] << 8); }
	inline std::uint32_t Rd32(const std::uint8_t * p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((std::uint32_t)p[3] << 24); }

	inline unsigned ExtractMasked(std::uint32_t v, std::uint32_t mask)
	{
		if (mask == 0) return 0;
		int shift = 0, bits = 0;
		while (((mask >> shift) & 1) == 0) shift++;
		while (((mask >> (shift + bits)) & 1) != 0 && shift + bits < 32) bits++;
		const unsigned c = (v & mask) >> shift;
		if (bits >= 8) return c >> (bits - 8);
		return (c * 255u) / ((1u << bits) - 1u);
	}

	// Fallback decoder for BMP variants stb_image (used by sf::Image) does not read, e.g. RLE8.
	CSurface * DecodeBmpFallback(const std::uint8_t * p, size_t n)
	{
		if (n < 14 + 40 || p[0] != 'B' || p[1] != 'M') return NULL;
		const std::uint32_t bfOffBits = Rd32(p + 10);
		const std::uint8_t * ih = p + 14;
		const std::uint32_t biSize = Rd32(ih);
		if (biSize < 40 || 14 + biSize > n) return NULL;
		const int  w     = (int)Rd32(ih + 4);
		int        h     = (int)Rd32(ih + 8);
		const unsigned bpp = Rd16(ih + 14);
		const std::uint32_t comp = Rd32(ih + 16);
		std::uint32_t clrUsed = Rd32(ih + 32);
		const bool bTopDown = h < 0;
		if (bTopDown) h = -h;
		if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return NULL;

		std::uint32_t rMask = 0x7C00, gMask = 0x03E0, bMask = 0x001F;
		if (bpp == 32) { rMask = 0xFF0000; gMask = 0xFF00; bMask = 0xFF; }
		if (comp == 3 /*BI_BITFIELDS*/ && 14 + biSize + 12 <= n) {
			const std::uint8_t * m = (biSize >= 52) ? ih + 40 : ih + biSize;
			rMask = Rd32(m); gMask = Rd32(m + 4); bMask = Rd32(m + 8);
		}

		// Palette
		std::vector<WORD> pal;
		if (bpp <= 8) {
			if (clrUsed == 0 || clrUsed > 256) clrUsed = 1u << bpp;
			const std::uint8_t * pp = ih + biSize + ((comp == 3 && biSize == 40) ? 12 : 0);
			pal.assign(256, 0);
			for (std::uint32_t i = 0; i < clrUsed && pp + i * 4 + 3 < p + n; i++)
				pal[i] = To565(pp[i * 4 + 2], pp[i * 4 + 1], pp[i * 4 + 0]);
		}

		// Pixel data offset (the old loader ignored bfOffBits and used header + palette)
		size_t off = bfOffBits;
		if (off < 14 + biSize || off >= n)
			off = 14 + biSize + (bpp <= 8 ? clrUsed * 4 : 0);
		if (off >= n) return NULL;
		const std::uint8_t * px = p + off;
		const size_t avail = n - off;

		CSurface * pSurf = new CSurface(w, h);
		WORD * out = pSurf->GetPixels();
		const int pitch = pSurf->GetPitch();
		auto row = [&](int y) -> WORD * { return out + (size_t)(bTopDown ? y : (h - 1 - y)) * pitch; };

		if (comp == 1 /*BI_RLE8*/ && bpp == 8) {
			size_t i = 0; int x = 0, y = 0;
			while (i + 1 < avail && y < h) {
				const unsigned a = px[i++], b = px[i++];
				if (a > 0) { for (unsigned k = 0; k < a && x < w; k++) row(y)[x++] = pal[b]; }
				else if (b == 0) { x = 0; y++; }
				else if (b == 1) break;
				else if (b == 2) { if (i + 1 >= avail) break; x += px[i++]; y += px[i++]; }
				else {
					for (unsigned k = 0; k < b && i < avail; k++) { const unsigned c = px[i++]; if (x < w && y < h) row(y)[x++] = pal[c]; }
					if (b & 1) i++;
				}
			}
			return pSurf;
		}
		if (comp != 0 /*BI_RGB*/ && comp != 3) { delete pSurf; return NULL; }

		const size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
		if (stride * h > avail) { delete pSurf; return NULL; }
		for (int y = 0; y < h; y++) {
			const std::uint8_t * s = px + stride * y;
			WORD * d = row(y);
			for (int x = 0; x < w; x++) {
				switch (bpp) {
				case 1:  d[x] = pal[(s[x >> 3] >> (7 - (x & 7))) & 1]; break;
				case 4:  d[x] = pal[(s[x >> 1] >> ((x & 1) ? 0 : 4)) & 15]; break;
				case 8:  d[x] = pal[s[x]]; break;
				case 16: { const unsigned v = Rd16(s + x * 2); d[x] = To565(ExtractMasked(v, rMask), ExtractMasked(v, gMask), ExtractMasked(v, bMask)); } break;
				case 24: d[x] = To565(s[x * 3 + 2], s[x * 3 + 1], s[x * 3]); break;
				case 32: { const std::uint32_t v = Rd32(s + x * 4); d[x] = To565(ExtractMasked(v, rMask), ExtractMasked(v, gMask), ExtractMasked(v, bMask)); } break;
				default: delete pSurf; return NULL;
				}
			}
		}
		return pSurf;
	}
}

CSurface * CSurface::CreateFromBitmapMemory(const void * pData, size_t iSize)
{
	if (pData == NULL || iSize == 0) return NULL;

	// Primary path: SFML's image loader.
	sf::Image img;
	if (img.loadFromMemory(pData, iSize)) {
		const sf::Vector2u sz = img.getSize();
		if (sz.x == 0 || sz.y == 0) return NULL;
		CSurface * pSurf = new CSurface((int)sz.x, (int)sz.y);
		const std::uint8_t * src = img.getPixelsPtr();
		for (unsigned y = 0; y < sz.y; y++) {
			WORD * d = pSurf->GetPixels() + (size_t)y * pSurf->GetPitch();
			const std::uint8_t * s = src + (size_t)y * sz.x * 4;
			for (unsigned x = 0; x < sz.x; x++, s += 4)
				d[x] = To565(s[0], s[1], s[2]);
		}
		return pSurf;
	}

	return DecodeBmpFallback(static_cast<const std::uint8_t *>(pData), iSize);
}

CSurface * CSurface::CreateFromBitmapFile(const char * pFileName, DWORD dwOffset)
{
	FILE * pFile = fopen(pFileName, "rb");
	if (pFile == NULL) return NULL;

	std::uint8_t fh[14];
	if (fseek(pFile, (long)dwOffset, SEEK_SET) != 0 || fread(fh, 1, sizeof(fh), pFile) != sizeof(fh) || fh[0] != 'B' || fh[1] != 'M') {
		fclose(pFile);
		return NULL;
	}
	const std::uint32_t bfSize = Rd32(fh + 2);
	if (bfSize <= sizeof(fh) || bfSize > 64u * 1024u * 1024u) {
		fclose(pFile);
		return NULL;
	}

	std::vector<std::uint8_t> data(bfSize);
	memcpy(data.data(), fh, sizeof(fh));
	const size_t got = fread(data.data() + sizeof(fh), 1, bfSize - sizeof(fh), pFile);
	fclose(pFile);
	data.resize(sizeof(fh) + got);

	// HD sprite files may hold a PNG or JPEG instead of a BMP, behind the same 14-byte 'BM'
	// header (bfSize = 14 + image size) so the .pak layout doesn't change. Keeps them small.
	if (data.size() > 18) {
		const std::uint8_t * p = data.data() + 14;
		const bool bPng = (p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G');
		const bool bJpg = (p[0] == 0xFF && p[1] == 0xD8);
		if (bPng || bJpg) return CreateFromBitmapMemory(p, data.size() - 14);
	}

	// The old loader (CMyDib) ignored bfOffBits and always took the pixels to start right
	// after the info header and palette. Normalise the header the same way so this loader
	// reads exactly what the old one did.
	if (data.size() >= 14 + 40) {
		const std::uint8_t * ih = data.data() + 14;
		const std::uint32_t biSize = Rd32(ih);
		const unsigned bpp = Rd16(ih + 14);
		std::uint32_t colors = Rd32(ih + 32);
		if (colors == 0) colors = (bpp == 8) ? 256 : (bpp == 4) ? 16 : (bpp == 1) ? 2 : 0;
		std::uint32_t off = 14 + biSize + colors * 4;
		if (Rd32(ih + 16) == 3 /*BI_BITFIELDS*/ && biSize == 40) off += 12;	// colour masks
		if (biSize >= 40 && off < data.size()) {
			data[10] = (std::uint8_t)(off & 0xFF);
			data[11] = (std::uint8_t)((off >> 8) & 0xFF);
			data[12] = (std::uint8_t)((off >> 16) & 0xFF);
			data[13] = (std::uint8_t)((off >> 24) & 0xFF);
		}
	}

	return CreateFromBitmapMemory(data.data(), data.size());
}
