// FrameProfiler.cpp: see FrameProfiler.h.
//////////////////////////////////////////////////////////////////////

#include "FrameProfiler.h"
#include <mmsystem.h>

namespace prof {

State g = {};

void EndFrame()
{
	if (g.freq == 0) {
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		g.freq = f.QuadPart > 0 ? f.QuadPart : 1;
		g.dwSince = timeGetTime();
	}
	g.frames++;
	g.shown++;
	const LONGLONG tNow = Now();
	if (g.lastFrameTick != 0) g.acc[FRAME] += tNow - g.lastFrameTick;
	g.lastFrameTick = tNow;
	const DWORD dwNow = timeGetTime();
	const DWORD dwSpan = dwNow - g.dwSince;
	if (dwSpan < 1000) return;

	const double n = (g.frames > 0) ? (double)g.frames : 1.0;
	for (int i = 0; i < COUNT; i++) {
		g.ms[i] = (double)g.acc[i] * 1000.0 / (double)g.freq / n;
		g.acc[i] = 0;
	}
	for (int i = 0; i < kMaxDlg; i++) {
		g.dlgMs[i] = (double)g.dlg[i] * 1000.0 / (double)g.freq / n;
		g.dlg[i] = 0;
	}
	for (int i = 0; i < C_COUNT; i++) {
		g.per[i] = (i == C_MSGS) ? g.cnt[i] * 1000.0 / dwSpan : g.cnt[i] / n;
		g.cnt[i] = 0;
	}
	g.lastFrames = g.frames;
	g.frames = 0;
	g.dwSince = dwNow;
}

} // namespace prof
