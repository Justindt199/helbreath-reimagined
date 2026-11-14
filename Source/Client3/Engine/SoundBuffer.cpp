// SoundBuffer.cpp: SFML-based sound buffer (replaces DirectSound buffers).
//////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SFML/Audio.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>
#include <mmsystem.h>

#include "SoundBuffer.h"

struct CSoundBufferImpl
{
	sf::SoundBuffer				buffer;
	bool						bLoaded = false;
	bool						bFailed = false;
	std::unique_ptr<sf::Sound>	voices[MAXSOUNDBUFFERS];
	float						fVolume = 100.f;
};

namespace
{
	// DirectSound attenuation (hundredths of dB, <= 0) -> SFML volume 0..100
	float VolumeToSfml(long lVol)
	{
		if (lVol >= 0) return 100.f;
		if (lVol <= SOUND_VOLUME_MIN) return 0.f;
		return 100.f * (float)std::pow(10.0, (double)lVol / 2000.0);
	}

	// DirectSound pan (hundredths of dB of attenuation on the opposite side) -> SFML pan -1..1
	float PanToSfml(long lPan)
	{
		if (lPan < SOUND_PAN_LEFT) lPan = SOUND_PAN_LEFT;
		else if (lPan > SOUND_PAN_RIGHT) lPan = SOUND_PAN_RIGHT;
		if (lPan == 0) return 0.f;
		const float fOther = (float)std::pow(10.0, -std::fabs((double)lPan) / 2000.0);
		return (lPan > 0) ? (1.f - fOther) : -(1.f - fOther);
	}

	// Fallback for WAV files whose header SFML refuses (the old client simply trusted a
	// fixed 44-byte PCM header, so some game files may rely on that).
	bool LoadLegacyWav(const char * pFileName, sf::SoundBuffer & buffer)
	{
		#pragma pack(push, 1)
		struct Waveheader
		{
			BYTE  RIFF[4];  DWORD dwSize; BYTE WAVE[4]; BYTE fmt_[4]; DWORD dw16;
			WORD  wOne_0;   WORD  wChnls; DWORD dwSRate; DWORD BytesPerSec;
			WORD  wBlkAlign; WORD BitsPerSample; BYTE DATA[4]; DWORD dwDSize;
		};
		#pragma pack(pop)

		FILE * pFile = fopen(pFileName, "rb");
		if (pFile == NULL) return false;
		Waveheader hdr;
		if (fread(&hdr, sizeof(hdr), 1, pFile) != 1 || hdr.wChnls == 0 || hdr.dwSRate == 0 ||
			(hdr.BitsPerSample != 8 && hdr.BitsPerSample != 16)) {
			fclose(pFile);
			return false;
		}
		std::vector<std::uint8_t> raw(hdr.dwDSize);
		const size_t got = raw.empty() ? 0 : fread(raw.data(), 1, raw.size(), pFile);
		fclose(pFile);
		raw.resize(got);

		std::vector<std::int16_t> samples;
		if (hdr.BitsPerSample == 8) {
			samples.resize(raw.size());
			for (size_t i = 0; i < raw.size(); i++) samples[i] = (std::int16_t)(((int)raw[i] - 128) << 8);
		}
		else {
			samples.resize(raw.size() / 2);
			memcpy(samples.data(), raw.data(), samples.size() * 2);
		}
		const unsigned int iChannels = hdr.wChnls > 1 ? 2u : 1u;
		std::vector<sf::SoundChannel> map;
		if (iChannels == 1) map.push_back(sf::SoundChannel::Mono);
		else { map.push_back(sf::SoundChannel::FrontLeft); map.push_back(sf::SoundChannel::FrontRight); }
		if (hdr.wChnls > 2) return false;
		return buffer.loadFromSamples(samples.data(), samples.size(), iChannels, hdr.dwSRate, map);
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSoundBuffer::CSoundBuffer(char * pWavFileName, BOOL bIsSingleLoad)
{
	ZeroMemory(m_cWavFileName, sizeof(m_cWavFileName));
	strncpy(m_cWavFileName, pWavFileName, sizeof(m_cWavFileName) - 1);

	m_cCurrentBufferIndex = 0;
	m_bIsSingleLoad = bIsSingleLoad;
	m_dwTime = 0;
	m_bIsLooping = FALSE;
	m_pImpl = new CSoundBufferImpl;
	// Like the original, the WAV file is only loaded the first time the sound is played.
}

CSoundBuffer::~CSoundBuffer()
{
	_ReleaseSoundBuffer();
	delete m_pImpl;
}

bool CSoundBuffer::_bLoad()
{
	if (m_pImpl->bLoaded) return true;
	if (m_pImpl->bFailed) return false;

	if (GetFileAttributes(m_cWavFileName) == INVALID_FILE_ATTRIBUTES ||
		(!m_pImpl->buffer.loadFromFile(std::string(m_cWavFileName)) && !LoadLegacyWav(m_cWavFileName, m_pImpl->buffer))) {
		m_pImpl->bFailed = true;
		return false;
	}

	for (int i = 0; i < MAXSOUNDBUFFERS; i++) {
		m_pImpl->voices[i] = std::make_unique<sf::Sound>(m_pImpl->buffer);
		m_pImpl->voices[i]->setSpatializationEnabled(false);	// plain 2D sound, panned by hand
		m_pImpl->voices[i]->setRelativeToListener(true);
		m_pImpl->voices[i]->setVolume(m_pImpl->fVolume);
	}
	m_pImpl->bLoaded = true;
	return true;
}

// Same voice-selection rules as the DirectSound version.
int CSoundBuffer::_iGetIdleVoice()
{
	sf::Sound * pCur = m_pImpl->voices[(int)m_cCurrentBufferIndex].get();
	if (pCur != NULL && pCur->getStatus() == sf::SoundSource::Status::Playing) {
		if (m_bIsSingleLoad == TRUE) {
			pCur->stop();
			m_dwTime = timeGetTime();
			return m_cCurrentBufferIndex;
		}
		m_cCurrentBufferIndex++;
		if (m_cCurrentBufferIndex >= MAXSOUNDBUFFERS) m_cCurrentBufferIndex = 0;
		sf::Sound * pNext = m_pImpl->voices[(int)m_cCurrentBufferIndex].get();
		if (pNext != NULL && pNext->getStatus() == sf::SoundSource::Status::Playing) pNext->stop();
	}
	m_dwTime = timeGetTime();
	return m_cCurrentBufferIndex;
}

BOOL CSoundBuffer::Play(BOOL bLoop, long lPan, int iVol)
{
	if (!_bLoad()) return FALSE;

	sf::Sound * pVoice = m_pImpl->voices[_iGetIdleVoice()].get();
	if (pVoice == NULL) return FALSE;

	SetVolume(iVol);
	pVoice->setPan(PanToSfml(lPan));

	m_bIsLooping = bLoop;
	pVoice->setLooping(bLoop ? true : false);
	pVoice->play();		// resumes if paused by bStop(TRUE), like DirectSound
	return TRUE;
}

void CSoundBuffer::SetVolume(LONG Volume)
{
	m_pImpl->fVolume = VolumeToSfml(Volume);
	for (int i = 0; i < MAXSOUNDBUFFERS; i++)
		if (m_pImpl->voices[i]) m_pImpl->voices[i]->setVolume(m_pImpl->fVolume);
}

void CSoundBuffer::bStop(BOOL bIsNoRewind)
{
	for (int i = 0; i < MAXSOUNDBUFFERS; i++) {
		if (!m_pImpl->voices[i]) continue;
		if (bIsNoRewind) m_pImpl->voices[i]->pause();
		else m_pImpl->voices[i]->stop();
	}
}

void CSoundBuffer::_ReleaseSoundBuffer()
{
	for (int i = 0; i < MAXSOUNDBUFFERS; i++) m_pImpl->voices[i].reset();
	m_pImpl->bLoaded = false;
	m_pImpl->bFailed = false;
}
