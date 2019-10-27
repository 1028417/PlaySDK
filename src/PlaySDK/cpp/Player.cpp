
#include "../../../inc/Player.h"

#include "decoder.h"

static CUTF8Writer m_logger;
const ITxtWriter& g_logger(m_logger);

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

    new (m_pDecoder) Decoder(*this);
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

    m_uPos = 0;
}

int CAudioOpaque::open(const wstring& strFile)
{
    m_pf = fsutil::fopen(strFile, "rb");
    if (NULL == m_pf)
    {
        return -1;
    }

    int nFileSize = fsutil::GetFileSize(m_pf);
    if (nFileSize <= 0)
    {
        fclose(m_pf);
        m_pf = NULL;
    }

    return nFileSize;
}

UINT CAudioOpaque::checkDuration()
{
	return ((Decoder*)m_pDecoder)->check();
}

E_DecodeStatus CAudioOpaque::decodeStatus()
{
	return ((Decoder*)m_pDecoder)->decodeStatus();
}

int64_t CAudioOpaque::seek(int64_t offset, E_SeekFileFlag eFlag)
{
    auto nPos = fsutil::seekFile(m_pf, offset, eFlag);
    if (nPos >= 0)
	{
        m_uPos = (unsigned)nPos;
	}

	return m_uPos;
}

int CAudioOpaque::read(uint8_t *buf, size_t size)
{
    size_t uCount = fread(buf, 1, size, m_pf);
    if (0 == uCount)
    {
        return -1;
    }

    m_uPos += uCount;
    return uCount;
}

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

#define __decoder (*(Decoder*)m_AudioOpaque.decoder())

void CPlayer::Stop()
{
	mutex_lock lock(m_mutex);

	__decoder.cancel();
	m_thread.cancel();
}

bool CPlayer::Play(uint64_t uStartPos, bool bForce48000, const CB_PlayFinish& cbFinish)
{
    mutex_lock lock(m_mutex);

	__decoder.cancel();
	m_thread.cancel();

    m_thread.start([&, cbFinish]() {
        E_DecodeStatus eStatus = E_DecodeStatus::DS_OpenFail;
        auto eRet = __decoder.open(bForce48000);
        if (E_DecoderRetCode::DRC_Success == eRet)
        {
            eStatus = __decoder.start();
        }

        _onFinish(eStatus);
        if (cbFinish)
        {
            cbFinish(eStatus);
        }
	});

	if (0 != uStartPos)
	{
		__decoder.seek(uStartPos);
	}
	
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
