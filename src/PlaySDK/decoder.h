#pragma once

#if __winvc
#pragma warning(disable: 4251)
#endif

#include "audiodecoder.h"

#include "../../inc/Player.h"

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
	Decoder() : m_audioDecoder(m_DecodeStatus)
	{
	}
	
private:
	tagDecodeStatus m_DecodeStatus;
	AudioDecoder m_audioDecoder;
	
	IAudioOpaque *m_pAudioOpaque = NULL;

	AVIOContext *m_avio = NULL;
	AVFormatContext *m_pFormatCtx = NULL;

	int m_audioIndex = 0;

    int m_duration = 0;

	int64_t m_seekPos = -1;

public:
    int duration() const
	{
		return m_duration;
	}

	E_DecodeStatus GetDecodeStatus() const
	{
		return m_DecodeStatus.eDecodeStatus;
	}

	uint64_t getClock()
	{
		return m_audioDecoder.getClock();
	}
	
	void seek(uint64_t pos);

    void setVolume(uint8_t volume);

    /*int check(const wstring& strFile)
	{
		return _check(strFile);
    }*/

    int check(IAudioOpaque& AudioOpaque)
	{
		return _check(AudioOpaque);
	}

    /*E_DecoderRetCode open(const wstring& strFile, bool bForce48000 = false)
	{
		return _open(strFile, bForce48000);
    }*/

	E_DecoderRetCode open(IAudioOpaque& AudioOpaque, bool bForce48000 = false)
	{
		return _open(AudioOpaque, bForce48000);
	}

	E_DecodeStatus start();

	void pause();
	void resume();

	void cancel();

private:
	template <typename T>
    int _check(T& input)
	{
		(void)_open(input);
        int duration = m_duration;
		_clearData();

		return duration;
	}

	template <typename T>
	E_DecoderRetCode _open(T& input, bool bForce48000)
	{
		m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Opening;

		auto eRet = _open(input);
		if (eRet != E_DecoderRetCode::DRC_Success)
		{
			_clearData();
			m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Cancel;
			return eRet;
		}

		if (!m_audioDecoder.open(*m_pFormatCtx->streams[m_audioIndex], bForce48000))
		{
			_clearData();
			m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Cancel;
			return E_DecoderRetCode::DRC_InitAudioDevFail;
		}

		return E_DecoderRetCode::DRC_Success;
	}

    E_DecoderRetCode _open(const wstring& strFile);
	E_DecoderRetCode _open(IAudioOpaque& AudioOpaque);

	E_DecoderRetCode _checkStream();

	void _clearData();
};
