
#include "SLESEngine.h"

#if __android
//#include <android/log.h>
//#define LOGD(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"ywl5320",FORMAT,##__VA_ARGS__);
extern ITxtWriter& g_playsdkLogger;

#define __samplesPerSec SL_SAMPLINGRATE_44_1
#define __freq 44100

#define __bitsPerSample SL_PCMSAMPLEFORMAT_FIXED_16
#define __audioDstFmt AV_SAMPLE_FMT_S16

static SLObjectItf g_engine = NULL;
static SLEngineItf g_engineIf = NULL;

int CSLESEngine::init()
{
    g_playsdkLogger >> "init SLESEngine";

    SLresult re = slCreateEngine(&g_engine,0,0,0,0,0);
    if(re != SL_RESULT_SUCCESS)
    {
        g_playsdkLogger << "slCreateEngine fail: " >> re;
        return re;
    }

    re = (*g_engine)->Realize(g_engine,SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS)
    {
        g_playsdkLogger << "RealizeEngine fail: " >> re;
        return re;
    }

    re = (*g_engine)->GetInterface(g_engine,SL_IID_ENGINE,&g_engineIf);
    if(re != SL_RESULT_SUCCESS)
    {
        g_playsdkLogger << "GetInterface SL_IID_ENGINE fail: " >> re;
        return re;
    }

    return 0;
}

void CSLESEngine::quit()
{
    if (g_engine)
    {
        (*g_engine)->Destroy(g_engine);
        g_engine = NULL;
        g_engineIf = NULL;
    }
}

bool CSLESEngine::_init()
{
    g_playsdkLogger >> "init SLESPlayer";
    if (!g_engine)
    {
        g_playsdkLogger >> "engine not inited";
        return false;
    }

    //2 创建混音器
    SLresult re = (*g_engineIf)->CreateOutputMix(g_engineIf,&m_mix,0,0,0);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "CreateOutputMix fail" >> re;
        return false;
    }
    re = (*m_mix)->Realize(m_mix,SL_BOOLEAN_FALSE);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "Realize OutputMix fail" >> re;
        return false;
    }
    SLDataLocator_OutputMix outmix = {SL_DATALOCATOR_OUTPUTMIX,m_mix};
    SLDataSink audioSink= {&outmix,0};

    //3 配置音频信息
    //缓冲队列
    SLDataLocator_AndroidSimpleBufferQueue que = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,10};
    //音频格式
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,
            2,//    声道数
            __samplesPerSec,
            __bitsPerSample,
            __bitsPerSample,
            SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN //字节序，小端
    };
    SLDataSource ds = {&que,&pcm};

    //4 创建播放器
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    re = (*g_engineIf)->CreateAudioPlayer(g_engineIf,&m_player,&ds,&audioSink,sizeof(ids)/sizeof(SLInterfaceID),ids,req);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "CreateAudioPlayer fail" >> re;
        return false;
    }
    re = (*m_player)->Realize(m_player,SL_BOOLEAN_FALSE);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "Realize Player fail" >> re;
        return false;
    }

    //获取player接口
    re = (*m_player)->GetInterface(m_player,SL_IID_PLAY,&m_playIf);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "GetInterface SL_IID_PLAY fail" >> re;
        return false;
    }

    re = (*m_player)->GetInterface(m_player,SL_IID_VOLUME,&m_volumeIf);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "GetInterface SL_IID_VOLUME fail" >> re;
        return false;
    }
    setVolume(m_volume);

    re = (*m_player)->GetInterface(m_player,SL_IID_BUFFERQUEUE,&m_bf);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "GetInterface SL_IID_BUFFERQUEUE fail" >> re;
        return false;
    }

    //设置回调函数
    re = (*m_bf)->RegisterCallback(m_bf, PcmCall, this);
    if(SL_RESULT_SUCCESS != re)
    {
        g_playsdkLogger << "bf RegisterCallback fail" >> re;
        return false;
    }

    return true;
}

void CSLESEngine::setVolume(uint8_t volume)
{
    if (volume<=100)
    {
        m_volume = volume;

        if (m_volumeIf)
        {
            (*m_volumeIf)->SetVolumeLevel(m_volumeIf, SLmillibel((1.0f - volume / 100.0f) * -5000));
            //(*m_volumeIf)->SetVolumeLevel(m_volumeIf, 20 * -50);
        }
    }
}

bool CSLESEngine::open(int channels, int sampleRate, int samples, tagSLDevInfo& DevInfo)
{
    (void)channels;
    (void)sampleRate;
    (void)samples;

    //_destroy();
    //quit();
    //init();

    if (!m_player)
    {
        if (!_init())
        {
            return false;
        }
    }

    //设置为播放状态
    pause(false);

    (*m_bf)->Enqueue(m_bf, "", 1);

    DevInfo.channels = 2;
    DevInfo.freq = __freq;
    DevInfo.audioDstFmt = __audioDstFmt;

    return true;
}

void CSLESEngine::PcmCall(SLAndroidSimpleBufferQueueItf bf, void *contex)
{
    (void)bf;
    CSLESEngine *pSLESEngine = (CSLESEngine*)contex;
    pSLESEngine->_PcmCall();
}

void CSLESEngine::_PcmCall()
{
    while (true)
    {
        uint8_t *lpBuff = NULL;
        int len = m_cb(lpBuff);
        if (0 == len)
        {
            //g_logger >> "PcmSize: 0";
            mtutil::usleep(50);
            continue;
        }

        if (len > 0)
        {
            //if (NULL != lpBuff)
            {
                (*m_bf)->Enqueue(m_bf,lpBuff,len);
            }
        }
        else
        {
            //g_logger >> "PcmSize: -1";
            mtutil::usleep(50);
            (*m_bf)->Enqueue(m_bf,"",1);
        }

        break;
    }
}

void CSLESEngine::pause(bool bPause)
{
    if (m_playIf)
    {
        if (bPause)
        {
            (*m_playIf)->SetPlayState(m_playIf, SL_PLAYSTATE_PAUSED);
        }
        else
        {
            (*m_playIf)->SetPlayState(m_playIf, SL_PLAYSTATE_PLAYING);
        }
    }
}

void CSLESEngine::close()
{
    g_playsdkLogger >> "SLES stoping";

    if (m_playIf)
    {
        (*m_bf)->Clear(m_bf);
        (*m_playIf)->SetPlayState(m_playIf,SL_PLAYSTATE_STOPPED);
    }
}

void CSLESEngine::_destroy()
{
    g_playsdkLogger >> "SLES destoy";

    if (m_mix)
    {
        (*m_mix)->Destroy(m_mix);
        m_mix = NULL;
    }

    if (m_player)
    {
        (*m_player)->Destroy(m_player);
        m_player = NULL;

        m_playIf = NULL;
        m_volumeIf = NULL;
        m_bf = NULL;
    }
}

#endif
