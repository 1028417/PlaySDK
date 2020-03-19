#pragma once

#include "util/util.h"

//ITxtWriter& g_playsdkLogger(m_logger);

/*#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif*/
#include <stdint.h>

extern "C"
{
#if !__android
//#define SDL_MAIN_HANDLED
#include "../../../3rd/SDL2/include/SDL.h"
#undef main
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}

struct tagSLDevInfo
{
	int channels = 0;

	int freq = 0;

    AVSampleFormat audioDstFmt = AV_SAMPLE_FMT_NONE;   // audio decode sample format
};

class IAudioDevEngine
{
public:
	virtual void setVolume(uint8_t volume) = 0;

	virtual bool open(int channels, int sampleRate, int samples, tagSLDevInfo& DevInfo) = 0;

	virtual void pause(bool bPause) = 0;

    virtual void close() = 0;
};
