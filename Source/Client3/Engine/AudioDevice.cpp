// AudioDevice.cpp: audio availability check (replaces YWSound).
//////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SFML/Audio.hpp>
#include <windows.h>

#include "AudioDevice.h"

CAudioDevice::CAudioDevice()
{
	m_bAvailable = false;
}

CAudioDevice::~CAudioDevice()
{
}

bool CAudioDevice::Create(HWND /*hWnd*/)
{
	try {
		m_bAvailable = !sf::PlaybackDevice::getAvailableDevices().empty();
	}
	catch (...) {
		m_bAvailable = false;
	}
	if (!m_bAvailable) OutputDebugString("No audio playback device found, sound disabled.\n");
	return m_bAvailable;
}
