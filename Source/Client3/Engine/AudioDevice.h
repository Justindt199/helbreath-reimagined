// AudioDevice.h: audio availability check (replaces YWSound / DirectSound device creation).
//
// SFML opens the audio device on demand, so all this class does is tell the game
// whether sound can be played at all (it used to be the DirectSoundCreate result).
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_AUDIODEVICE_H
#define HB_ENGINE_AUDIODEVICE_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <windows.h>

class CAudioDevice
{
public:
	CAudioDevice();
	virtual ~CAudioDevice();
	bool Create(HWND hWnd);
	bool IsAvailable() const { return m_bAvailable; }

private:
	bool m_bAvailable;
};

#endif // HB_ENGINE_AUDIODEVICE_H
