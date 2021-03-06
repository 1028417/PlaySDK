#pragma once

#include "inc.h"

#if !__android
//#define SDL_MAIN_HANDLED
#include "../../../3rd/SDL2/include/SDL.h"
#undef main

using CB_SDLStream = function<const uint8_t*(size_t uBufSize, size_t& uRetSize)>;

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

    E_SLDevStatus m_eStatus = E_SLDevStatus::Close;

public:
    static string getErrMsg();
    static int init();
    static void quit();

    inline bool isOpen() const override
    {
        return m_eStatus != E_SLDevStatus::Close;
    }

    bool open(tagSLDevInfo& DevInfo) override;

    void pause(bool bPause) override;

    void setVolume(uint8_t volume) override
    {
        m_volume = volume;
    }

    void clearbf() override;

    void close() override;

private:
    static void SDLCALL _cb(void *userdata, uint8_t *stream, int len);
    void _cb(uint8_t *stream, int size);
};
#endif
