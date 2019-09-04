
#include "SDLEngine.h"

#if !__android

/*  soundtrack array use to adjust */
static const int g_lpNextNbChannels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };

static const int g_lpNextSampleRates[] = { 44100, 48000, 96000, 192000 };

int CSDLEngine::init()
{
#if __android
    SDL_SetMainReady();
#endif

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

void CSDLEngine::audioCallback(void *userdata, uint8_t *stream, int nBufSize)
{
    SDL_memset(stream, 0, nBufSize);

    CSDLEngine *pThis = (CSDLEngine*)userdata;
    while (true)
    {
        Uint8 *lpBuff = NULL;
        int len = pThis->m_cb(nBufSize, lpBuff);
        if (len > 0)
		{
			//if (NULL != lpBuff)
			{
				SDL_MixAudio(stream, lpBuff, len, pThis->m_volume*SDL_MIX_MAXVOLUME / 100);
			}

            nBufSize -= len;
            if (nBufSize <= 0)
            {
                break;
            }

            stream += len;
        }
        else if (0 == len)
		{
			mtutil::usleep(1);
			continue;
		}
		else
		{
			//mtutil::usleep(10);
			break;
		}
    }
}

bool CSDLEngine::open(int channels, int sampleRate, int samples, tagSLDevInfo& DevInfo)
{
	SDL_AudioSpec wantSpec;
	memset(&wantSpec, 0, sizeof(wantSpec));

	wantSpec.format = AUDIO_F32LSB;
	wantSpec.samples = (Uint16)samples;

	//wantSpec.silence = 0;

	wantSpec.callback = audioCallback;
	wantSpec.userdata = this;

	int nextSampleRateIdx = FF_ARRAY_ELEMS(g_lpNextSampleRates) - 1;
	while (nextSampleRateIdx>=0 && g_lpNextSampleRates[nextSampleRateIdx] >= sampleRate)
	{
		nextSampleRateIdx--;
	}
	
	auto fnOpen = [&]()->bool {
		return 0 == SDL_OpenAudio(&wantSpec, &m_spec);
	};

	for (wantSpec.channels = (Uint8)channels; ;)
	{
		bool bFlag = false;

		int t_nextSampleRateIdx = nextSampleRateIdx;
		for (wantSpec.freq = sampleRate; ; )
		{
			if (fnOpen())
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

	pause(false);

	return true;
}

void CSDLEngine::pause(bool bPause)
{
   SDL_PauseAudio(bPause?1:0);
}

void CSDLEngine::close()
{
   SDL_CloseAudio();
}

/*static void lock(bool bLock)
{
   if (bLock)
   {
       SDL_LockAudio();
   }
   else
   {
       SDL_UnlockAudio();
   }
}*/

#endif
