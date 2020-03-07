#pragma once

#include "SLESEngine.h"

#include "SDLEngine.h"

#include "avpacketqueue.h"

struct tagDecodeData
{
	tagDecodeData()
	{
		init();
	}

	void init()
	{
		sendReturn = 0;

		audioBuf = nullptr;
		audioBufSize = 0;

		audioSrcFmt = AV_SAMPLE_FMT_NONE;
		audioSrcChannelLayout = 0;
		audioSrcFreq = 0;
	}

	int sendReturn = 0;

	AVPacket packet;

	SwrContext *aCovertCtx = NULL;
	DECLARE_ALIGNED(16, uint8_t, swrAudioBuf)[192000];

	uint8_t *audioBuf = NULL;
    uint32_t audioBufSize = 0;

	AVSampleFormat audioSrcFmt;
	int64_t audioSrcChannelLayout = 0;
	int audioSrcFreq = 0;

	int sample_rate = 0;
};

struct tagDecodeStatus
{
	E_DecodeStatus eDecodeStatus = E_DecodeStatus::DS_Finished;

	bool bReadFinished = false;
};

class AudioDecoder
{
public:
	AudioDecoder(tagDecodeStatus& DecodeStatus);

private:
	AVCodecContext *m_codecCtx = NULL;          // audio codec context
	
	tagSLDevInfo m_DevInfo;

	double m_timeBase = 0;
	int m_dstByteRate = 0;
	uint64_t m_clock = 0;

	int64_t m_seekPos = -1;

	tagDecodeData m_DecodeData;

	AvPacketQueue m_packetQueue;

#if __android
    CSLESEngine
#else
    CSDLEngine
#endif
    m_SLEngine;

public:
	size_t packetEnqueue(AVPacket& packet)
	{
		return m_packetQueue.enqueue(&packet);
	}

	void setVolume(uint8_t volume)
	{
        m_SLEngine.setVolume(volume);
	}

    bool open(AVStream& stream, bool bForce48KHz=false);

	void pause(bool bPause)
	{
        m_SLEngine.pause(bPause);
	}

	void seek(uint64_t pos)
	{
		m_seekPos = pos;
		m_packetQueue.clear();
	}

	uint64_t clock() const
	{
		return m_clock;
	}

	int audioSampleRate() const
	{
		return m_DecodeData.sample_rate;
	}
	int devSampleRate() const
	{
		return m_DevInfo.freq;
	}

	void close();

private:
    int _cb(tagDecodeStatus& DecodeStatus, int nMaxBufSize, uint8_t*& lpBuff);

    int32_t _decodePacket(bool& bQueedEmpty);
    int32_t _receiveFrame();
	int32_t _convertFrame(AVFrame& frame);

	void _clearData();
};
