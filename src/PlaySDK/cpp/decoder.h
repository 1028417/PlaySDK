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
            , m_audioDecoder(m_packetQueue)
	{
	}
	
private:
        IAudioOpaque& m_audioOpaque;

        AvPacketQueue m_packetQueue;

	AudioDecoder m_audioDecoder;

        E_DecodeStatus m_eDecodeStatus = E_DecodeStatus::DS_Finished;

	AVFormatContext *m_fmtCtx = NULL;
	AVIOContext *m_avioCtx = NULL;

	int m_audioStreamIdx = -1;

	uint32_t m_duration = 0;

	int64_t m_seekPos = -1;

private:
        static int _readOpaque(void *opaque, uint8_t *buf, int bufSize);
        static int64_t _seekOpaque(void *opaque, int64_t offset, int whence);

        E_DecoderRetCode _open();

	E_DecoderRetCode _checkStream();

    void _start();

	void _cleanup();

public:
    const E_DecodeStatus& decodeStatus() const
	{
        return m_eDecodeStatus;
	}

        const uint64_t& getClock()
	{
		return m_audioDecoder.clock();
	}

    uint32_t duration() const
    {
            return m_duration;
    }
	
	int sampleRate() const
	{
		return m_audioDecoder.sampleRate();
	}

        bool packetQueueEmpty() const
        {
            return m_packetQueue.isEmpty();
        }

        uint32_t check();

        E_DecoderRetCode open(bool bForce48KHz);

        void start(uint64_t uPos=0);

	void cancel();

        bool pause();
        bool resume();
	
        bool seek(uint64_t pos);

        bool seekingFlag() const
        {
            return m_seekPos>=0;
        }

	void setVolume(uint8_t volume);
};
