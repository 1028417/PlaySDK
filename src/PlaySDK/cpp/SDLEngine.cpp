
#include "SDLEngine.h"

#if !__android
/* Minimum SDL audio buffer size, in samples. */
#define __MIN_BUFFER_SIZE 512

/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define __CALLBACK_PER_SEC 30
#define __caleSamples(wantSpec) wantSpec.samples = FFMAX(__MIN_BUFFER_SIZE, 2 << av_log2(wantSpec.freq / __CALLBACK_PER_SEC))
// = 4096

/*  soundtrack array use to adjust */
static const uint8_t g_lpNextNbChannels[] { 0, 0, 1, 6, 2, 6, 4, 6 };
static const int g_lpNextSampleRates[] { 44100, 48000, 96000, 192000 };
// { 44100, 48000, 88200, 96000, 192000, 352800, 384000 };

int CSDLEngine::init()
{
    SDL_SetMainReady();

    return SDL_Init(SDL_INIT_AUDIO); //| SDL_INIT_TIMER);
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

static map<SDL_AudioFormat, AVSampleFormat> g_mapSampleFormat {
    {AUDIO_U8, AV_SAMPLE_FMT_U8}
    , {AUDIO_S16, AV_SAMPLE_FMT_S16} // 16bit的flac、wav
    , {AUDIO_S32SYS, AV_SAMPLE_FMT_S32} // 24bit的flac、wav
    , {AUDIO_F32SYS, AV_SAMPLE_FMT_FLT} // mp3、ape、32bit浮点wav
};

bool CSDLEngine::open(tagSLDevInfo& DevInfo)
{
	SDL_AudioSpec wantSpec;
    memzero(wantSpec);

	wantSpec.channels = DevInfo.channels;

	switch (DevInfo.sample_fmt) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
		wantSpec.format = AUDIO_U8;
        break;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
		wantSpec.format = AUDIO_S16SYS;
        break;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
		wantSpec.format = AUDIO_S32SYS;
		break;
	default:
		wantSpec.format = AUDIO_F32SYS;
	}

	//DevInfo.sample_rate = MIN(DevInfo.sample_rate, 96000); //DST降频
	wantSpec.freq = DevInfo.sample_rate;
	__caleSamples(wantSpec);

    wantSpec.callback = _cb;
    wantSpec.userdata = this;
	
	cauto fnOpen = [&]() {
		auto t_wantSpec = wantSpec;
		t_wantSpec.channels = 2;
		m_devId = SDL_OpenAudioDevice(NULL, 0, &t_wantSpec, &m_spec, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
		if (m_devId >= 2) {
			return true;
		}

		int nextSampleRateIdx = FF_ARRAY_ELEMS(g_lpNextSampleRates) - 1;
		while (nextSampleRateIdx >= 0 && g_lpNextSampleRates[nextSampleRateIdx] >= wantSpec.freq) {
			nextSampleRateIdx--;
		}

		do {
			int t_nextSampleRateIdx = nextSampleRateIdx;
			while (true) {
				/*int nRet = SDL_OpenAudio(&wantSpec, &m_spec);
				if (0 == nRet) {
					if (m_spec.format == wantSpec.format) {
						m_devId = 1;
						return true;
					}
					close();
				}*/

				m_devId = SDL_OpenAudioDevice(NULL, 0, &wantSpec, &m_spec, 0); //SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
				if (m_devId >= 2) {
					return true;
				}

				if (t_nextSampleRateIdx < 0) {
					break;
				}

				wantSpec.freq = g_lpNextSampleRates[t_nextSampleRateIdx--];
				__caleSamples(wantSpec);
			}

			wantSpec.channels = g_lpNextNbChannels[FFMIN(7, wantSpec.channels)];
		} while (wantSpec.channels != 0);
		
		return false;
	};
	if (!fnOpen()) {
		return false;
	}
	
	DevInfo.channels = m_spec.channels;
	DevInfo.sample_fmt = g_mapSampleFormat[m_spec.format];
	DevInfo.sample_rate = m_spec.freq;

    //m_eStatus = E_SLDevStatus::Ready;
	SDL_PauseAudioDevice(m_devId, 0);

	return true;
}

inline void CSDLEngine::_cb(uint8_t *stream, int len)
{
	SDL_memset(stream, 0, len);

	while (true)
	{
		size_t uRetSize = 0;
		auto lpBuff = m_cb(len, uRetSize);
		if (NULL == lpBuff)
		{
			mtutil::usleep(50);
			break;
		}

		//SDL_MixAudio(stream, lpBuff, len, SDL_MIX_MAXVOLUME * m_volume / 100);
		SDL_MixAudioFormat(stream, lpBuff, m_spec.format, uRetSize, SDL_MIX_MAXVOLUME * m_volume / 100);

		len -= uRetSize;
		if (len <= 0)
		{
			break;
		}
		stream += uRetSize;
	}
}

void SDLCALL CSDLEngine::_cb(void *userdata, uint8_t *stream, int len)
{
	CSDLEngine *engine = (CSDLEngine*)userdata;
	engine->_cb(stream, len);
}

void CSDLEngine::pause(bool bPause)
{
    //m_eStatus = bPause?E_SLDevStatus::Pause:E_SLDevStatus::Ready;
	SDL_PauseAudioDevice(m_devId, bPause?1:0);
}

void CSDLEngine::clearbf()
{
    SDL_ClearQueuedAudio(m_devId);
}

void CSDLEngine::close()
{
    //m_eStatus = E_SLDevStatus::Close;
    SDL_ClearQueuedAudio(m_devId);
	SDL_CloseAudioDevice(m_devId);
}
#endif
