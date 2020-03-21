#pragma once

#if __winvc
#pragma warning(disable: 4251)
#endif

#include "audiodecoder.h"

enum class E_DecoderRetCode
{
	DRC_Success = 0
	, DRC_Fail
	, DRC_OpenFail
	, DRC_InvalidAudioStream
	, DRC_NoAudioStream
	, DRC_InitAudioDevFail
};

class Decoder
{
public:
        Decoder(IAudioOpaque& AudioOpaque)
            : m_audioOpaque(AudioOpaque)
            , m_audioDecoder(m_DecodeStatus)
	{
	}
	
private:
        IAudioOpaque& m_audioOpaque;

	tagDecodeStatus m_DecodeStatus;

	AudioDecoder m_audioDecoder;

	AVIOContext *m_avio = NULL;
	AVFormatContext *m_pFormatCtx = NULL;

        int m_audioStreamIdx = -1;

	uint32_t m_duration = 0;
	uint32_t m_byteRate = 0;

	int64_t m_seekPos = -1;

private:
        static int _readOpaque(void *opaque, uint8_t *buf, int bufSize);
        static int64_t _seekOpaque(void *opaque, int64_t offset, int whence);

        E_DecoderRetCode _open();

	E_DecoderRetCode _checkStream();

	void _cleanup();

public:
        bool isOpened() const
        {
            return m_audioStreamIdx >= 0;
        }

        const E_DecodeStatus& decodeStatus() const
	{
		return m_DecodeStatus.eDecodeStatus;
	}

        const uint64_t& getClock()
	{
		return m_audioDecoder.clock();
	}

    uint32_t duration() const
    {
            return m_duration;
    }
    uint32_t byteRate() const
    {
            return m_byteRate;
    }

	int audioSampleRate() const
	{
		return m_audioDecoder.audioSampleRate();
	}
	int devSampleRate() const
	{
		return m_audioDecoder.devSampleRate();
	}

        bool packetQueueEmpty() const
        {
            return m_audioDecoder.packetQueueEmpty();
        }

        uint32_t check();

        E_DecoderRetCode open(bool bForce48KHz);

        E_DecodeStatus start(uint64_t uPos=0);

	void cancel();

	void pause();
	void resume();
	
	void seek(uint64_t pos);

        bool seekingFlag() const
        {
            return m_seekPos>=0;
        }

	void setVolume(uint8_t volume);
};
