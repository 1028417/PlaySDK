#pragma once

#include "util/util.h"

extern ITxtWriter& g_logger;

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}

enum class E_SLDevStatus
{
    Close,
    Ready,
    Pause
};

struct tagSLDevInfo
{
    uint8_t channels = 0;

	AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;

	int sample_rate = 0;
};

class IAudioDevEngine
{
public:
    virtual bool isOpen() const = 0;

    virtual bool open(tagSLDevInfo& DevInfo) = 0;

	virtual void pause(bool bPause) = 0;

    virtual void setVolume(uint8_t volume) = 0;

    virtual void clearbf() = 0;

    virtual void close() = 0;
};
