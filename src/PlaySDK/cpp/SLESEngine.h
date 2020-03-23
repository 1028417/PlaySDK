#pragma once

#include "inc.h"

#if __android
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

using CB_SLESStream = std::function<size_t(uint8_t*& lpBuff)>;

//1 创建引擎
class CSLESEngine : public IAudioDevEngine
{
public:
    CSLESEngine(const CB_SLESStream& cb)
        : m_cb(cb)
    {
    }

private:
    CB_SLESStream m_cb;

    uint8_t m_volume = 100;

    SLObjectItf m_mix = NULL;
    SLObjectItf m_player = NULL;

    SLPlayItf m_playIf = NULL;
    SLVolumeItf m_volumeIf = NULL;
    SLAndroidSimpleBufferQueueItf m_bf = NULL;

    E_SLDevStatus m_eStatus = E_SLDevStatus::Close;

public:
    static int init();
    static void quit();

    void setVolume(uint8_t volume) override;

    bool open(int channels, int sampleRate, int samples, tagSLDevInfo& DevInfo) override;

    void pause(bool bPause) override;

    void close() override;

private:
    bool _create();

    void _destroy();

    static void _cb(SLAndroidSimpleBufferQueueItf, void *contex);
    void _cb();
};

#endif
