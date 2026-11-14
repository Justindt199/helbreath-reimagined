// FrameProfiler.h: where the time of each frame goes, for the tooltip on the FPS counter.
//
// Sections are timed with QueryPerformanceCounter and added up; once a second the totals are
// divided by the frames drawn in that second, so the tooltip shows milliseconds per frame.
// Some sections sit inside others (circles and names are drawn while drawing the characters);
// the tooltip takes them out of the characters' time.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>

namespace prof {

enum Sec {
	MAP,		// the ground, its pre-drawn background and the effect lights
	OBJECTS,	// every character, monster, item and map object (rings and names included)
	RINGS,		// circle indicators (inside OBJECTS)
	NAMES,		// names over heads (inside OBJECTS)
	EFFECTS,	// spell effects, weather, chat over heads
	UI,			// windows, bars, the top strip, the cursor
	CONVERT,	// the finished picture turned into a texture and sent to the graphics card
	DISPLAY,	// putting it on the screen, including waiting for VSync / the FPS limit
	NET,		// reading and handling messages from the server
	LOGIC,		// moving and animating everything on the map, mouse commands
	TEXT,		// all text drawn, wherever it is (inside the other sections)
	PANELS,		// all panels drawn in code (FillPanel), wherever they are
	UPDATE,		// the game's update (CGame::UpdateScreen) when it showed a frame: the sections above and the rest
	IDLE,		// the game's update when it didn't: checking whether anything changed (see Wmain.cpp)
	WINMSG,		// handling window messages (mouse, keyboard, timers; server messages are inside it)
	FRAME,		// wall-clock time from one shown frame to the next
	COUNT
};
const int kTopLevel = TEXT;	// sections before this one add up to the frame

enum Cnt { C_RINGS, C_NAMES, C_CHARS, C_MSGS, C_TEXT, C_PANELS, C_IDLE, C_COUNT };
const int kMaxDlg = 80;		// window ids (MAXDIALOGBOX)

struct State {
	LONGLONG acc[COUNT];		// ticks this second
	double   ms[COUNT];			// last second: ms per drawn frame
	int      cnt[C_COUNT];		// this second
	double   per[C_COUNT];		// last second: per drawn frame (messages: per second)
	LONGLONG dlg[kMaxDlg];		// ticks this second, per window
	double   dlgMs[kMaxDlg];	// last second: ms per drawn frame, per window
	const char * dlgName[kMaxDlg];
	int      frames, lastFrames;
	LONGLONG lastFrameTick;		// when the last frame was shown
	unsigned int shown;			// frames shown so far (never reset)
	DWORD    dwSince;
	LONGLONG freq;
};
extern State g;

inline LONGLONG Now() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
inline void Add(Sec s, LONGLONG t0) { g.acc[s] += Now() - t0; }
inline void Count(Cnt c, int n = 1) { g.cnt[c] += n; }

struct Scope {
	Sec s; LONGLONG t0;
	explicit Scope(Sec sec) : s(sec), t0(Now()) {}
	~Scope() { g.acc[s] += Now() - t0; }
};

void EndFrame();		// after a frame was drawn: counts it, and once a second updates g.ms / g.per

} // namespace prof
