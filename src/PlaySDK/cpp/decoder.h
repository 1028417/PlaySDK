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
	Decoder(class CAudioOpaque& AudioOpaque)
		: m_AudioOpaque(AudioOpaque)
		, m_audioDecoder(m_DecodeStatus)
	{
	}
	
	~Decoder() {}

private:
	class CAudioOpaque& m_AudioOpaque;

	AudioDecoder m_audioDecoder;

	tagDecodeStatus m_DecodeStatus;

	AVIOContext *m_avio = NULL;
	AVFormatContext *m_pFormatCtx = NULL;

	int m_audioIndex = 0;

	uint32_t m_duration = 0;

	int64_t m_seekPos = -1;

public:
        E_DecodeStatus decodeStatus() const
        {
                return m_DecodeStatus.eDecodeStatus;
        }

	uint64_t getClock()
	{
		return m_audioDecoder.clock();
	}

        uint32_t duration() const
        {
                return m_duration;
        }

	uint32_t check();

        E_DecoderRetCode open(bool bForce48000 = false);

	E_DecodeStatus start();

        void cancel();

	void pause();
	void resume();

        void seek(uint64_t pos);

        void setVolume(uint8_t volume);

private:
	E_DecoderRetCode _open();

	E_DecoderRetCode _checkStream();

	void _cleanup();
};
