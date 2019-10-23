#pragma once

#include "inc.h"

#if !__android

using CB_SDLStream = function<int(int nMaxBufSize, Uint8*& lpBuff)>;

class CSDLEngine : public IAudioDevEngine
{
public:
	CSDLEngine(const CB_SDLStream& cb)
		: m_cb(cb)
	{
		memzero(m_spec);
	}

private:
	SDL_AudioSpec m_spec;

    CB_SDLStream m_cb;
	
	uint8_t m_volume = 100;

public:
    static string getErrMsg();
    static int init();
    static void quit();

	void setVolume(uint8_t volume) override
	{
		m_volume = volume;
	}

	bool open(int channels, int sampleRate, int samples, tagSLDevInfo& DevInfo) override;

    void pause(bool bPause) override;

    void close() override;

private:
    static void SDLCALL audioCallback(void *userdata, uint8_t *stream, int nBufSize);
};

#endif
