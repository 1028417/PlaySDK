#pragma once

#include "SLEngine.h"

#include "SDLEngine.h"

#include "avpacketqueue.h"

#include "resample.h"

#define __eagain (AVERROR(EAGAIN))

struct tagDecodeData
{
	tagDecodeData() = default;

	void reset()
	{
		//audioBuf = NULL;
		audioBufSize = 0;

        if (__eagain == sendReturn)
        {
            av_packet_unref(&packet);
        }
		sendReturn = 0;
	}

	AVPacket packet;

	CResample swr;
	
	const uint8_t *audioBuf = NULL;
	size_t audioBufSize = 0;

	int sendReturn = 0;
};

class AudioDecoder
{
public:
    AudioDecoder(AvPacketQueue& packetQueue);

private:
    AvPacketQueue& m_packetQueue;

	AVCodecContext *m_codecCtx = NULL;

	tagSLDevInfo m_devInfo;
	
	int m_sample_rate = 0;

	int m_dstByteRate = 0;

	double m_timeBase = 0;
	uint64_t m_clock = 0;

    int64_t m_seekPos = -1;

	tagDecodeData m_DecodeData;

#if __android
    CSLEngine
#else
    CSDLEngine
#endif
    m_SLEngine;

public:
    void setVolume(uint8_t volume)
    {
        m_SLEngine.setVolume(volume);
    }

    bool isOpen() const
    {
        return m_SLEngine.isOpen();
    }

    bool open(AVStream& stream, bool bForce48KHz=false);

    void pause(bool bPause);

    void seek(uint64_t pos);

    const uint64_t& clock() const
	{
		return m_clock;
	}

	int sampleRate() const
	{
		return m_sample_rate;
	}

	void close();

private:
     const uint8_t* _cb(size_t uBufSize, size_t& uRetSize);

    int32_t _decodePacket();
    int32_t _recvFrame();

	void _cleanup();
};
