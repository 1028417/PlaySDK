
#include "../../../inc/Player.h"

#include "decoder.h"

static CUTF8TxtWriter m_logger;
ITxtWriter& g_playsdkLogger(m_logger);

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
	((Decoder*)m_pDecoder)->~Decoder();
	_aligned_free(m_pDecoder);
}

void CAudioOpaque::close()
{
    if (m_pf)
    {
        (void)fclose(m_pf);
        m_pf = NULL;
    }

    //m_uPos = 0;
}

long long CAudioOpaque::open(const wstring& strFile)
{
    m_pf = fsutil::fopen(strFile, "rb");
    if (NULL == m_pf)
    {
        return -1;
    }

	long long nFileSize = fsutil::lSeek64(m_pf, 0, SEEK_END);
	if (nFileSize <= 0)
	{
		fclose(m_pf);
		m_pf = NULL;
	}
	(void)fsutil::lSeek64(m_pf, 0, SEEK_SET);
    
	return nFileSize;
}

UINT CAudioOpaque::checkDuration()
{
	return ((Decoder*)m_pDecoder)->check(*this);
}

E_DecodeStatus CAudioOpaque::decodeStatus() const
{
	return ((const Decoder*)m_pDecoder)->decodeStatus();
}

int64_t CAudioOpaque::seek(int64_t offset, int origin)
{
	long long pos = fsutil::lSeek64(m_pf, offset, origin);
	if (pos < 0)
    {
        return -1;
    }

    //m_uPos = (uint64_t)pos;

    return 0;
}

size_t CAudioOpaque::read(byte_p buf, size_t size)
{
    return fread(buf, 1, size, m_pf);
}

int CPlayer::InitSDK()
{
	if (!m_logger.is_open())
	{
		m_logger.open(L"playsdk.log", true);
		g_playsdkLogger >> "InitSDK";
	}

#if __android
    return CSLESEngine::init();
#else
    int nRet = CSDLEngine::init();
    if (nRet != 0)
    {
        g_playsdkLogger << "initSDLEngine fail: " >> CSDLEngine::getErrMsg();
    }
    return nRet;
#endif
}

void CPlayer::QuitSDK()
{
    if (m_logger.is_open())
    {
        g_playsdkLogger >> "QuitSDK";
        m_logger.close();
    }

#if __android
    CSLESEngine::quit();
#else
    CSDLEngine::quit();
#endif
}

#define __decoder (*(Decoder*)m_AudioOpaque.decoder())

void CPlayer::Stop()
{
	mutex_lock lock(m_mutex);

	__decoder.cancel();
	m_thread.cancel();
}

bool CPlayer::Play(uint64_t uStartPos, bool bForce48KHz, CB_PlayStop cbStop)
{
    mutex_lock lock(m_mutex);

	__decoder.cancel();
	m_thread.cancel();

	bool bOnline = m_AudioOpaque.isOnline();
	if (!bOnline)
	{
		auto eRet = __decoder.open(bForce48KHz, m_AudioOpaque);
		if (eRet != E_DecoderRetCode::DRC_Success)
		{
			if (cbStop)
			{
                cbStop(__decoder.decodeStatus() != E_DecodeStatus::DS_Cancel);
			}
			
			return false;
		}

		if (0 != uStartPos)
		{
			__decoder.seek(uStartPos);
		}
	}

    m_thread.start([=]() {
		if (bOnline)
		{
			auto eRet = __decoder.open(bForce48KHz, m_AudioOpaque);
			if (eRet != E_DecoderRetCode::DRC_Success)
			{
				if (cbStop)
				{
                    cbStop(__decoder.decodeStatus() != E_DecodeStatus::DS_Cancel);
				}

				return;
			}

			if (0 != uStartPos)
			{
				__decoder.seek(uStartPos);
			}
		}

		(void)__decoder.start(m_AudioOpaque);
		if (cbStop)
		{
			cbStop(false);
		}
	});
	
    return true;
}

uint32_t CPlayer::GetDuration()
{
	mutex_lock lock(m_mutex);

	return __decoder.duration();
}

uint64_t CPlayer::GetClock()
{
    mutex_lock lock(m_mutex);

    return __decoder.getClock();
}

void CPlayer::Seek(UINT uPos)
{
    mutex_lock lock(m_mutex);

    __decoder.seek(uPos*__1e6);
}

void CPlayer::Pause()
{
    mutex_lock lock(m_mutex);
	
	__decoder.pause();
}

void CPlayer::Resume()
{
    mutex_lock lock(m_mutex);

	__decoder.resume();
}

void CPlayer::SetVolume(UINT uVolume)
{
	mutex_lock lock(m_mutex);

	__decoder.setVolume(uVolume);
}
