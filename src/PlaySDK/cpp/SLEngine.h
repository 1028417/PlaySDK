﻿#pragma once

#include "inc.h"

#if __android
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

using CB_SLStream = std::function<const uint8_t*(size_t& uRetSize)>;

class CSLPlayer
{
public:
    CSLPlayer() = default;

private:
    SLObjectItf m_mix = NULL;
    SLObjectItf m_player = NULL;

    SLPlayItf m_playIf = NULL;
    SLAndroidSimpleBufferQueueItf m_bf = NULL;
    SLVolumeItf m_volumeIf = NULL;

private:
    bool _create(class CSLEngine *engine, SLuint32 samplesPerSec, SLuint16 bitsPerSample);

public:
    bool start(class CSLEngine *engine, SLuint32 samplesPerSec, SLuint16 bitsPerSample);

    void pause(bool bPause)
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

    void setVolume(uint8_t volume)
    {
        if (m_volumeIf)
        {
            (*m_volumeIf)->SetVolumeLevel(m_volumeIf, SLmillibel((1.0f - volume/100.0f) * -5000));
            //(*m_volumeIf)->SetVolumeLevel(m_volumeIf, 20 * -50);
        }
    }

    void clearbf()
    {
        if (m_bf)
        {
            (*m_bf)->Clear(m_bf);
        }
    }

    void stop()
    {
        if (m_playIf)
        {
            (*m_playIf)->SetPlayState(m_playIf,SL_PLAYSTATE_STOPPED);
            if (m_bf)
            {
                (*m_bf)->Clear(m_bf);
            }
        }
    }

    void destroy();
};

class CSLEngine : public IAudioDevEngine
{
friend CSLPlayer;
public:
    CSLEngine(const CB_SLStream& cb);

private:
    CB_SLStream m_cb;

    CSLPlayer *m_pPlayer = NULL;

    mutex m_mutex;

    E_SLDevStatus m_eStatus = E_SLDevStatus::Close;

private:
    static void _cb(SLAndroidSimpleBufferQueueItf bf, void *context)
    {
        ((CSLEngine*)context)->_cb(bf);
    }

    void _cb(SLAndroidSimpleBufferQueueItf& bf);

public:
    static int init();
    static void quit();

    inline bool isOpen() const override
    {
        return m_eStatus != E_SLDevStatus::Close;
    }

    bool open(tagSLDevInfo& DevInfo) override;

    void pause(bool bPause) override;

    void setVolume(uint8_t volume) override;

    void clearbf() override;

    void close() override;
};
#endif
