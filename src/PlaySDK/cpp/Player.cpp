
#include "../../../inc/Player.h"

#include "decoder.h"

static CUTF8TxtWriter m_logger;
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

    m_nFileSize = -1;
}

long long CAudioOpaque::open(cwstr strFile)
{
    m_pf = fsutil::fopen(strFile, "rb");
    if (NULL == m_pf)
    {
        m_nFileSize = -1;
        return -1;
    }

    (void)fseek64(m_pf, 0, SEEK_END);
    m_nFileSize = (long long )ftell64(m_pf);
    if (m_nFileSize <= 0)
	{
		fclose(m_pf);
		m_pf = NULL;
    }
    (void)fseek64(m_pf, 0, SEEK_SET);

    //setbuf(m_pf, NULL);

    return m_nFileSize;
}

uint32_t CAudioOpaque::checkDuration()
{
    return ((Decoder*)m_pDecoder)->check();
}

const E_DecodeStatus& CAudioOpaque::decodeStatus() const
{
	return ((const Decoder*)m_pDecoder)->decodeStatus();
}

int64_t CAudioOpaque::seek(int64_t offset, int origin)
{
    if (feof(m_pf))
    {
        rewind(m_pf);
    }

    return fseek64(m_pf, offset, origin);

    /*long long pos = lseek64(m_pf, offset, origin);
	if (pos < 0)
    {
        return -1;
    }
    return 0;*/
}

int CAudioOpaque::read(byte_p buf, UINT size)
{
    return fread(buf, 1, size, m_pf);
}

bool CAudioOpaque::seekingFlag() const
{
    return ((Decoder*)m_pDecoder)->seekingFlag();
}

uint32_t CAudioOpaque::duration() const
{
	return ((Decoder*)m_pDecoder)->duration();
}

uint32_t CAudioOpaque::byteRate() const
{
    return ((Decoder*)m_pDecoder)->byteRate();
}

int CAudioOpaque::sampleRate() const
{
	return ((Decoder*)m_pDecoder)->sampleRate();
}

uint64_t CAudioOpaque::clock() const
{
    return ((Decoder*)m_pDecoder)->getClock();
}

//bool CAudioOpaque::probing() const
//{
//    return ((Decoder*)m_pDecoder)->probing();
//}

int CPlayer::InitSDK()
{
    m_logger.open("playsdk.log", true);
	g_logger >> "InitSDK";

#if __android
    return CSLEngine::init();
#else
	return CSDLEngine::init();
#endif
}

void CPlayer::QuitSDK()
{
	g_logger >> "QuitSDK";
    m_logger.close();

#if __android
    CSLEngine::quit();
#else
    CSDLEngine::quit();
#endif
}

#define __decoder (*(Decoder*)m_AudioOpaque.decoder())

inline void CPlayer::_Stop()
{
    if (m_thread.joinable())
    {
        __decoder.cancel();
        m_thread.join();
    }
}

void CPlayer::Stop()
{
    mutex_lock lock(m_mutex);
    _Stop();
}

bool CPlayer::Play(uint64_t uStartPos, bool bForce48KHz, cfn_void cbStop)
{
    mutex_lock lock(m_mutex);
    _Stop();

    auto eRet = __decoder.open(bForce48KHz);
    if (eRet != E_DecoderRetCode::DRC_Success)
    {
        return false;
    }

    m_thread = thread([=]() {
        (void)__decoder.start(uStartPos);
        cbStop();
	});
	
    return true;
}

void CPlayer::PlayStream(bool bForce48KHz, cfn_void_t<bool> cbStop)
{
    mutex_lock lock(m_mutex);
    _Stop();

    m_thread = thread([=]() {
        auto eRet = __decoder.open(bForce48KHz);
        if (eRet != E_DecoderRetCode::DRC_Success)
        {
            cbStop(false);
            return;
        }

        // TODO cbStart();
        (void)__decoder.start();
        cbStop(true);
    });
}

bool CPlayer::Pause()
{
    bool bRet = false;
    if (m_mutex.try_lock())
    {
        bRet = __decoder.pause();
        m_mutex.unlock();
    }
    return bRet;
}

bool CPlayer::Resume()
{
    bool bRet = false;
    if (m_mutex.try_lock())
    {
        bRet = __decoder.resume();
        m_mutex.unlock();
    }
    return bRet;
}

bool CPlayer::Seek(UINT uPos)
{
    bool bRet = false;
    if (m_mutex.try_lock())
    {
        bRet =__decoder.seek(uPos*AV_TIME_BASE);
        m_mutex.unlock();
    }
    return bRet;
}

void CPlayer::SetVolume(UINT uVolume)
{
	__decoder.setVolume(uVolume);
}

bool CPlayer::packetQueueEmpty() const
{
    return __decoder.packetQueueEmpty();
}
