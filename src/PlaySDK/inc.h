#pragma once

#include "util.h"

extern ITxtWriter& g_logger;

extern "C"
{
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif

#include <stdint.h>

#if !__android
//#define SDL_MAIN_HANDLED
#include "SDL.h"
#undef main
#endif

//#include "libavfilter/avfilter.h"
//#include "libavfilter/buffersink.h"
//#include "libavfilter/buffersrc.h"

//#include "libavdevice/avdevice.h"
//#include "libswscale/swscale.h"

#include "libavutil/pixfmt.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"

#include "libswresample/swresample.h"

#include "libavcodec/avfft.h"
#include "libavcodec/avcodec.h"

#include "libavformat/avformat.h"
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
