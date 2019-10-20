
#include "decoder.h"

static CUTF8Writer m_logger;
ITxtWriter& g_logger(m_logger);

#if !__windows
#define _aligned_free(p) free(p)
#endif

CAudioOpaque::CAudioOpaque()
{
#if __windows
	m_pDecoder = _aligned_malloc(sizeof(Decoder), 16);
#elif __android
    m_pDecoder = memalign(16, sizeof(Decoder));
#else
    (void)posix_memalign(&m_pDecoder, 16, sizeof(Decoder));
#endif

    new (m_pDecoder) Decoder;
}

CAudioOpaque::~CAudioOpaque()
{
	close();
	
	((Decoder*)m_pDecoder)->~Decoder();
	_aligned_free(m_pDecoder);
}

void CAudioOpaque::setFile(const wstring& strFile)
{
	close();

	m_size = -1;

    m_strFile = strFile;
}

int CAudioOpaque::checkDuration()
{
	return (int)((Decoder*)m_pDecoder)->check(*this);
}

bool CAudioOpaque::open()
{
	if (m_pf)
	{
		return m_size;
	}

	m_size = -1;
	m_uPos = 0;

	m_pf = fsutil::fopen(m_strFile, "rb");
	if (NULL == m_pf)
	{
		return false;
	}

	tagFileStat stat;
	memset(&stat, 0, sizeof stat);
	if (!fsutil::fileStat(m_pf, stat))
	{
		close();
		return false;
	}

	m_size = (int32_t)stat.st_size;
	if (0 <= m_size)
	{
		close();
		return false;
	}

	return true;
}

int64_t CAudioOpaque::seek(int64_t offset, E_SeekFileFlag eFlag)
{
	auto nRet = fsutil::seekFile(m_pf, offset, eFlag);
	if (nRet >= 0)
	{
		m_uPos = nRet;
	}

	return m_uPos;
}

size_t CAudioOpaque::read(uint8_t *buf, int buf_size)
{
	size_t uRet = fread(buf, 1, buf_size, m_pf);
	m_uPos += uRet;
	return uRet;
}

void CAudioOpaque::close()
{
	if (m_pf)
	{
		(void)fclose(m_pf);
		m_pf = NULL;
	}

	m_uPos = 0;
}

static Decoder g_Decoder;

int CPlayer::InitSDK()
{
    m_logger.open(L"playsdk.log", true);
    g_logger >> "InitSDK";

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
    g_logger >> "QuitSDK";
    m_logger.close();

#if __android
    CSLESEngine::quit();
#else
    CSDLEngine::quit();
#endif
}

template <typename T>
bool CPlayer::_Play(T& input, uint64_t uStartPos, bool bForce48000)
{
    m_stopedSignal.wait(true);

	if (g_Decoder.open(input, bForce48000) != E_DecoderRetCode::DRC_Success)
	{
		m_stopedSignal.set();
		return false;
	}

	mtutil::thread([&]() {
		E_DecodeStatus eRet = g_Decoder.start();

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
		g_Decoder.seek(uStartPos);
	}
	
    return true;
}

int CPlayer::GetDuration() const
{
	return g_Decoder.duration();
}

bool CPlayer::Play(IAudioOpaque& AudioOpaque, uint64_t uStartPos, bool bForce48000)
{
	Stop();

    return _Play(AudioOpaque, uStartPos, bForce48000);
}

E_PlayStatus CPlayer::GetPlayStatus()
{
	E_DecodeStatus eDecodeStatus = g_Decoder.GetDecodeStatus();
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
	g_Decoder.setVolume(uVolume);
}

uint64_t CPlayer::getClock() const
{
    return g_Decoder.getClock();
}

void CPlayer::Seek(UINT uPos)
{
	g_Decoder.seek(uPos*__1e6);
}

void CPlayer::Pause()
{
	if (E_PlayStatus::PS_Play == GetPlayStatus())
	{
		g_Decoder.pause();
	}
}

void CPlayer::Resume()
{
    g_Decoder.resume();
}

void CPlayer::Stop()
{
	g_Decoder.cancel();

    m_stopedSignal.wait();
}
