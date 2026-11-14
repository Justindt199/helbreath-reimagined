// SoundBuffer.h: SFML-based sound effect / music buffer (replaces the DirectSound version).
//
// Keeps the original interface, including DirectSound-style units so no game code
// had to change its maths:
//   volume: 0 (full) .. -10000 (silent), in hundredths of a decibel
//   pan:    -10000 (left) .. 0 (centre) .. 10000 (right), in hundredths of a decibel
//////////////////////////////////////////////////////////////////////

#ifndef HB_ENGINE_SOUNDBUFFER_H
#define HB_ENGINE_SOUNDBUFFER_H

#if _MSC_VER >= 1000
#pragma once
#endif

#include <windows.h>

#define MAXSOUNDBUFFERS		2

#define SOUND_PAN_LEFT		-10000
#define SOUND_PAN_RIGHT		10000
#define SOUND_VOLUME_MIN	-10000
#define SOUND_VOLUME_MAX	0

struct CSoundBufferImpl;

class CSoundBuffer
{
public:
	CSoundBuffer(char * pWavFileName, BOOL bIsSingleLoad = FALSE);
	virtual ~CSoundBuffer();

	BOOL Play(BOOL bLoop = FALSE, long lPan = 0, int iVol = 0);
	void bStop(BOOL bIsNoRewind = FALSE);	// bIsNoRewind = pause (next Play resumes)
	void SetVolume(LONG Volume);
	void _ReleaseSoundBuffer();

	char				m_cWavFileName[32];
	BOOL				m_bIsSingleLoad;
	BOOL				m_bIsLooping;
	DWORD				m_dwTime;

private:
	bool _bLoad();
	int  _iGetIdleVoice();

	char				m_cCurrentBufferIndex;
	CSoundBufferImpl *	m_pImpl;
};

#endif // HB_ENGINE_SOUNDBUFFER_H
