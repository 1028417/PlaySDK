
#include "../../inc/Player.h"

#include "decoder.h"

static Decoder m_Decoder;

static CUTF8Writer m_logger;
ITxtWriter& g_logger(m_logger);

int CPlayer::InitSDK()
{
    m_logger.open(L"playsdk.log", true);
    m_logger >> "InitSDK";

#if __android
    return CSLESEngine::init();
#else
    int nRet = CSDLEngine::init();
    if (nRet != 0)
    {
        g_logger << "initSDLEngine fail: " >> CSDLEngine::getErrMsg();
    }
    return nRet;
#endif
}

void CPlayer::QuitSDK()
{
    m_logger >> "QuitSDK";
    m_logger.close();

#if __android
    CSLESEngine::quit();
#else
    CSDLEngine::quit();
#endif
}

template <typename T>
int CPlayer::_CheckDuration(T& input, bool bLock)
{
    int64_t nDuration = 0;
    if (bLock)
    {
		static mutex s_mutex;
		s_mutex.lock();

		static Decoder Decoder;
        nDuration = Decoder.check(input);

		s_mutex.unlock();
    }
    else
    {
		Decoder Decoder;
        nDuration = Decoder.check(input);
    }

    return (int)nDuration;
}

template <typename T>
bool CPlayer::_Play(T& input, uint64_t uStartPos, bool bForce48000)
{
    m_stopedSignal.wait(true);

	if (m_Decoder.open(input, bForce48000) != E_DecoderRetCode::DRC_Success)
	{
		m_stopedSignal.set();
		return false;
	}

	mtutil::thread([&]() {
		E_DecodeStatus eRet = m_Decoder.start();

		m_stopedSignal.set();

		if (E_DecodeStatus::DS_Finished == eRet)
		{
			if (m_cbFinish)
			{
				m_cbFinish();
			}
		}
	});

	if (0 != uStartPos)
	{
		m_Decoder.seek(uStartPos);
	}
	
    return true;
}

int CPlayer::GetDuration() const
{
	return m_Decoder.duration();
}

int CPlayer::CheckDuration(IAudioOpaque& AudioOpaque, bool bLock)
{
    return _CheckDuration(AudioOpaque, bLock);
}

bool CPlayer::Play(IAudioOpaque& AudioOpaque, uint64_t uStartPos, bool bForce48000)
{
    return _Play(AudioOpaque, uStartPos, bForce48000);
}

E_PlayStatus CPlayer::GetPlayStatus()
{
	E_DecodeStatus eDecodeStatus = m_Decoder.GetDecodeStatus();
	switch (eDecodeStatus)
	{
	case E_DecodeStatus::DS_Opening:
	case E_DecodeStatus::DS_Decoding:
		return E_PlayStatus::PS_Play;
	case E_DecodeStatus::DS_Paused:
		return E_PlayStatus::PS_Pause;
	default:
		return E_PlayStatus::PS_Stop;
	}
}

void CPlayer::SetVolume(UINT uVolume)
{
	m_Decoder.setVolume(uVolume);
}

uint64_t CPlayer::getClock() const
{
    return m_Decoder.getClock();
}

void CPlayer::Seek(UINT uPos)
{
	m_Decoder.seek(uPos*__1e6);
}

void CPlayer::Pause()
{
	if (E_PlayStatus::PS_Play == GetPlayStatus())
	{
		m_Decoder.pause();
	}
}

void CPlayer::Resume()
{
    m_Decoder.resume();
}

void CPlayer::Stop()
{
	m_Decoder.cancel();

    m_stopedSignal.wait();
}
