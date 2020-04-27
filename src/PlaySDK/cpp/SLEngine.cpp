
#include "SLEngine.h"

#if __android
//#include <android/log.h>
//#define LOGD(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"ywl5320",FORMAT,##__VA_ARGS__);

static SLObjectItf g_engine = NULL;
static SLEngineItf g_engineIf = NULL;

static map<SLuint32, CSLPlayer> g_mapPlayer;

void CSLPlayer::destroy()
{
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
        m_bf = NULL;
        m_volumeIf = NULL;
    }
}

static void _cb(SLAndroidSimpleBufferQueueItf bf, void *context)
{
    auto engine = (CSLEngine*)context;
    engine->cb(bf);
}

bool CSLPlayer::_create(CSLEngine *engine, SLuint32 samplesPerSec, SLuint16 bitsPerSample)
{
    //创建混音器
    //const SLInterfaceID mids[] {SL_IID_ENVIRONMENTALREVERB}; //SL_IID_VOLUME??
    //const SLboolean mreq[] {SL_BOOLEAN_FALSE};
    SLresult re = (*g_engineIf)->CreateOutputMix(g_engineIf,&m_mix,0,NULL,NULL); // mids,mreg);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "CreateOutputMix fail" >> re;
        return false;
    }
    re = (*m_mix)->Realize(m_mix,SL_BOOLEAN_FALSE);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "Realize OutputMix fail" >> re;
        return false;
    }
    /*SLEnvironmentalReverbItf envReverb = NULL;
    re = (*m_mix)->GetInterface(m_mix, SL_IID_ENVIRONMENTALREVERB, &envReverb);
    if (SL_RESULT_SUCCESS == re) {
        SLEnvironmentalReverbSettings envReverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;
        (*envReverb)->SetEnvironmentalReverbProperties(envReverb, &envReverbSettings);
    }*/

    //缓冲队列
    SLDataLocator_AndroidSimpleBufferQueue que {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,10};
    SLDataFormat_PCM pcm {
            SL_DATAFORMAT_PCM,
            2,
            samplesPerSec,
            bitsPerSample,
            bitsPerSample,
            SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN //字节序，小端
    };

    //创建播放器
    SLDataSource src {&que,&pcm};

    SLDataLocator_OutputMix mix {SL_DATALOCATOR_OUTPUTMIX,m_mix};
    SLDataSink sink {&mix,NULL};

    const SLInterfaceID ids[] {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req[] {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    re = (*g_engineIf)->CreateAudioPlayer(g_engineIf,&m_player,&src,&sink,sizeof(ids)/sizeof(SLInterfaceID),ids,req);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "CreateAudioPlayer fail" >> re;
        return false;
    }
    re = (*m_player)->Realize(m_player,SL_BOOLEAN_FALSE);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "Realize Player fail" >> re;
        return false;
    }

    //获取player接口
    re = (*m_player)->GetInterface(m_player,SL_IID_PLAY,&m_playIf);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "GetInterface SL_IID_PLAY fail" >> re;
        return false;
    }

    re = (*m_player)->GetInterface(m_player,SL_IID_BUFFERQUEUE,&m_bf);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "GetInterface SL_IID_BUFFERQUEUE fail" >> re;
        return false;
    }
    re = (*m_bf)->RegisterCallback(m_bf,_cb,engine);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "bf RegisterCallback fail" >> re;
        return false;
    }

    re = (*m_player)->GetInterface(m_player,SL_IID_VOLUME,&m_volumeIf);
    if(SL_RESULT_SUCCESS != re)
    {
        //g_playsdkLogger << "GetInterface SL_IID_VOLUME fail" >> re;
        return false;
    }

    //SL_IID_EFFECTSEND??

    return true;
}

bool CSLPlayer::start(CSLEngine *engine, SLuint32 samplesPerSec, SLuint16 bitsPerSample)
{
    if (NULL == g_engine)
    {
        //g_playsdkLogger >> "engine not inited";
        return false;
    }

    if (!_create(engine, samplesPerSec, bitsPerSample))
    {
        destroy();
        return false;
    }

    //设置为播放状态
    pause(false);

    (*m_bf)->Enqueue(m_bf, "", 1);

    return true;
}

CSLEngine::CSLEngine(const CB_SLESStream& cb)
    : m_cb(cb)
    , m_pPlayer(&g_mapPlayer[44100+16])
{
}

int CSLEngine::init()
{
    SLresult re = slCreateEngine(&g_engine,0,NULL,0,NULL,NULL);
    if(re != SL_RESULT_SUCCESS)
    {
        //g_playsdkLogger << "slCreateEngine fail: " >> re;
        return re;
    }

    re = (*g_engine)->Realize(g_engine,SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS)
    {
        //g_playsdkLogger << "RealizeEngine fail: " >> re;
        return re;
    }

    re = (*g_engine)->GetInterface(g_engine,SL_IID_ENGINE,&g_engineIf);
    if(re != SL_RESULT_SUCCESS)
    {
        //g_playsdkLogger << "GetInterface SL_IID_ENGINE fail: " >> re;
        return re;
    }

    return 0;
}

void CSLEngine::quit()
{
    if (g_engine)
    {
        for (auto& pr : g_mapPlayer)
        {
            pr.second.destroy();
        }
        g_mapPlayer.clear();

        (*g_engine)->Destroy(g_engine);
        g_engine = NULL;
        g_engineIf = NULL;
    }
}

bool CSLEngine::open(tagSLDevInfo& DevInfo)
{
    DevInfo.channels = 2;

    SLuint16 bitsPerSample = 0;
    switch (DevInfo.sample_fmt)
    {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
        DevInfo.sample_fmt = AV_SAMPLE_FMT_U8;
        bitsPerSample = 8;
        break;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        DevInfo.sample_fmt = AV_SAMPLE_FMT_S16;
        bitsPerSample = 16;
        break;
    default:
        DevInfo.sample_fmt = AV_SAMPLE_FMT_S32;
        bitsPerSample = 32;
    };

    auto& sample_rate = DevInfo.sample_rate;
    /*if (sample_rate > 96000)
    {
        sample_rate = 192000;
    }
    else*/ if (sample_rate > 48000)
    {
        sample_rate = 96000;
    }
    else if (sample_rate > 44100)
    {
        sample_rate = 48000;
    }
    else
    {
        sample_rate = 44100;
    }

    m_pPlayer = &g_mapPlayer[sample_rate+bitsPerSample];
    if (!m_pPlayer->start(this, sample_rate*1000, bitsPerSample))
    {
        return false;
    }

    return true;
}

/*inline void CSLEngine::_cb()
{
    while (true) {
        const uint8_t *lpBuff = NULL;
        size_t len = m_cb(lpBuff);

        while (E_SLDevStatus::Pause == m_eStatus)
        {
            mtutil::usleep(50);
        }
        if (E_SLDevStatus::Close == m_eStatus)
        {
            return;
        }

        if (len > 0)
        {
            (*m_bf)->Enqueue(m_bf,lpBuff,len);
            break;
        }

        mtutil::usleep(50);
    }
}*/

void CSLEngine::cb(SLAndroidSimpleBufferQueueItf& bf)
{
    const uint8_t *lpBuff = NULL;
    size_t len = m_cb(lpBuff);
    if (len > 0)
    {
        (*bf)->Enqueue(bf,lpBuff,len);
        return;
    }

    mtutil::usleep(50);

    /*len = m_cb(lpBuff);
    if (len > 0)
    {
        (*bf)->Enqueue(bf,lpBuff,len);
        return;
    }*/

    (*bf)->Enqueue(bf,"",1);
}

void CSLEngine::pause(bool bPause)
{
    m_pPlayer->pause(bPause);
}

void CSLEngine::setVolume(uint8_t volume)
{
    volume = MIN(volume, 100);
    m_pPlayer->setVolume(volume);
}

void CSLEngine::clearbf()
{
    m_pPlayer->clearbf();
}

void CSLEngine::close()
{
    m_pPlayer->stop();
}
#endif
