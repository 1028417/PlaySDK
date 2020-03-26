#pragma once

#include "inc.h"

#if __android
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

using CB_SLESStream = std::function<size_t(const uint8_t*& lpBuff)>;

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
    SLAndroidSimpleBufferQueueItf m_bf = NULL;
    SLVolumeItf m_volumeIf = NULL;

    //E_SLDevStatus m_eStatus = E_SLDevStatus::Close;

public:
    static int init();
    static void quit();

    bool open(tagSLDevInfo& DevInfo) override;

    void pause(bool bPause) override;

    void setVolume(uint8_t volume) override;

    void clearbf() override;

    void close() override;

private:
    bool _create(uint8_t channels, SLuint32 samplesPerSec, SLuint16 bitsPerSample);

    void _destroy();

    static void _cb(SLAndroidSimpleBufferQueueItf, void *context);
    void _cb();
};
#endif
