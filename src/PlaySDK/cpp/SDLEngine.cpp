
#include "SDLEngine.h"

#if !__android

/*  soundtrack array use to adjust */
static const int g_lpNextNbChannels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };

static const int g_lpNextSampleRates[] = { 44100, 48000, 96000, 192000 };

int CSDLEngine::init()
{
    SDL_SetMainReady();

    return SDL_Init(SDL_INIT_AUDIO);// | SDL_INIT_TIMER);
}

void CSDLEngine::quit()
{
    SDL_Quit();
}

string CSDLEngine::getErrMsg()
{
    auto lpErrMsg = SDL_GetError();
    if (NULL != lpErrMsg)
    {
        return lpErrMsg;
    }

    return "";
}

/*int CSDLEngine::getEnvChannel()
{
    const char *env = SDL_getenv("SDL_AUDIO_CHANNELS");
	if (NULL == env)
	{
		return 0;
	}
	
	return atoi(env);
}*/

inline void CSDLEngine::_audioCallback(uint8_t *stream, int size)
{
    while (true)
    {
        Uint8 *lpBuff = NULL;
        int len = m_cb(lpBuff, size);
		if (-1 == len)
		{
			break;
		}

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
			//if (lpBuff)
			SDL_MixAudio(stream, lpBuff, len, SDL_MIX_MAXVOLUME * m_volume / 100);

            size -= len;
            if (size <= 0)
			{
				break;
            }
			stream += len;
        }
		else
		{
			mtutil::usleep(50);
		}
    }
}

void SDLCALL CSDLEngine::_audioCallback(void *userdata, uint8_t *stream, int size)
{
    SDL_memset(stream, 0, size);
    CSDLEngine *engine = (CSDLEngine*)userdata;
    engine->_audioCallback(stream, size);
}

bool CSDLEngine::open(int channels, int sampleRate, int samples, tagSLDevInfo& DevInfo)
{
	SDL_AudioSpec wantSpec;
	memzero(wantSpec);

	wantSpec.format = AUDIO_F32LSB;
	wantSpec.samples = (Uint16)samples;

	//wantSpec.silence = 0;

	wantSpec.callback = _audioCallback;
	wantSpec.userdata = this;

	int nextSampleRateIdx = FF_ARRAY_ELEMS(g_lpNextSampleRates) - 1;
	while (nextSampleRateIdx>=0 && g_lpNextSampleRates[nextSampleRateIdx] >= sampleRate)
	{
		nextSampleRateIdx--;
	}
	
	for (wantSpec.channels = (Uint8)channels; ;)
	{
		bool bFlag = false;

		int t_nextSampleRateIdx = nextSampleRateIdx;
		for (wantSpec.freq = sampleRate; ; )
		{
			if (0 == SDL_OpenAudio(&wantSpec, &m_spec))
			{
				if (m_spec.format == wantSpec.format)
				{
					bFlag = true;
					break;
				}

				close();
			}

			if (t_nextSampleRateIdx < 0)
			{
				break;
			}
			wantSpec.freq = g_lpNextSampleRates[t_nextSampleRateIdx--];
		}

		if (bFlag)
		{
			break;
		}

		wantSpec.channels = g_lpNextNbChannels[FFMIN(7, wantSpec.channels)];
		if (0 == wantSpec.channels)
		{
			return false;
		}
	}
	
	DevInfo.channels = m_spec.channels;
	DevInfo.freq = m_spec.freq;

	switch (m_spec.format)
	{
    case AUDIO_U8:
		DevInfo.audioDstFmt = AV_SAMPLE_FMT_U8;
        break;
    case AUDIO_S32SYS:
		DevInfo.audioDstFmt = AV_SAMPLE_FMT_S32;
		break;
    case AUDIO_F32SYS:
		DevInfo.audioDstFmt = AV_SAMPLE_FMT_FLT;
		break;
    default:
		DevInfo.audioDstFmt = AV_SAMPLE_FMT_S16;
    }

    m_eStatus = E_SLDevStatus::Ready;
	SDL_PauseAudio(0);

	return true;
}

void CSDLEngine::pause(bool bPause)
{
    m_eStatus = bPause?E_SLDevStatus::Pause:E_SLDevStatus::Ready;
	//SDL_PauseAudio(bPause?1:0);
}

void CSDLEngine::close()
{
    m_eStatus = E_SLDevStatus::Close;
	SDL_CloseAudio();
}
#endif
