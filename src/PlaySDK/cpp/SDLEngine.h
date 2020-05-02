#pragma once

#include "inc.h"

#if !__android
//#define SDL_MAIN_HANDLED
#include "../../../3rd/SDL2/include/SDL.h"
#undef main

using CB_SDLStream = function<size_t(const uint8_t*& lpBuff, int nBufSize)>;

class CSDLEngine : public IAudioDevEngine
{
public:
	CSDLEngine(const CB_SDLStream& cb)
		: m_cb(cb)
	{
		memzero(m_spec);
	}

private:
    SDL_AudioDeviceID m_devId = 1;

	SDL_AudioSpec m_spec;

    CB_SDLStream m_cb;
	
	uint8_t m_volume = 100;

    //E_SLDevStatus m_eStatus = E_SLDevStatus::Close;

public:
    static string getErrMsg();
    static int init();
    static void quit();

    bool open(tagSLDevInfo& DevInfo) override;

    void pause(bool bPause) override;

    void setVolume(uint8_t volume) override
    {
        m_volume = volume;
    }

    void clearbf() override;

    void close() override;

private:
    static void SDLCALL _cb(void *userdata, uint8_t *stream, int size);
    void _cb(uint8_t *stream, int size);
};
#endif
