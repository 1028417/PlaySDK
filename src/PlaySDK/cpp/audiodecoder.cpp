
#include "../../../inc/Player.h"

#include "audiodecoder.h"

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512

/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

AudioDecoder::AudioDecoder(tagDecodeStatus& DecodeStatus)
    : m_SLEngine(
#if __android
    [&](uint8_t*& lpBuff) {
		return _cb(DecodeStatus, lpBuff, 0);
	}
#else
    [&](uint8_t*& lpBuff, int size) {
		return _cb(DecodeStatus, lpBuff, size);
	}
#endif
)
{
}

bool AudioDecoder::open(AVStream& stream, bool bForce48KHz)
{
	m_timeBase = av_q2d(stream.time_base)*__1e6;

	m_codecCtx = avcodec_alloc_context3(NULL);
	if (NULL == m_codecCtx)
	{
        return false;
	}

	auto fnInitCodecCtx = [&](){	
		stream.discard = AVDISCARD_DEFAULT;
		if (avcodec_parameters_to_context(m_codecCtx, stream.codecpar) < 0)
		{
			return false;
		}

		if (m_codecCtx->channels <= 0 || m_codecCtx->sample_rate <= 0)
		{
			return false;
		}

		AVCodec *codec = avcodec_find_decoder(m_codecCtx->codec_id);
		if (NULL == codec)
		{
			return false;
		}

		if (avcodec_open2(m_codecCtx, codec, NULL) < 0)
		{
			return false;
		}

		return true;
	};

	if (!fnInitCodecCtx())
	{
		avcodec_free_context(&m_codecCtx);
		m_codecCtx = NULL;
        return false;
    }

	int sample_rate = m_DecodeData.sample_rate = m_codecCtx->sample_rate;
	if (bForce48KHz)
	{
		sample_rate = MAX(sample_rate, 48000);
	}
	
	int samples = 0;// FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(sample_rate / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
	if (!m_SLEngine.open(m_codecCtx->channels, sample_rate, samples, m_DevInfo))
	{
		this->_clearData();
        return false;
	}

    uint8_t audioDstDepth = 2;
    if (AV_SAMPLE_FMT_U8 == m_DevInfo.audioDstFmt)
    {
		audioDstDepth = 1;
    }
    else if (AV_SAMPLE_FMT_S32 == m_DevInfo.audioDstFmt || AV_SAMPLE_FMT_FLT == m_DevInfo.audioDstFmt)
    {
		audioDstDepth = 4;
    }
    m_dstByteRate = m_DevInfo.freq * m_codecCtx->channels * audioDstDepth;
	
	m_clock = 0;
    
    return true;
}

int AudioDecoder::_cb(tagDecodeStatus& DecodeStatus, uint8_t*& lpBuff, int nBufSize)
{
    if (E_DecodeStatus::DS_Cancel == DecodeStatus.eDecodeStatus
            || E_DecodeStatus::DS_Finished == DecodeStatus.eDecodeStatus)
	{
		return -1;
	}

	if (m_seekPos >= 0)
	{
		m_clock = m_seekPos;
		m_seekPos = -1;

		m_DecodeData.sendReturn = 0;
		m_DecodeData.audioBufSize = 0;

		avcodec_flush_buffers(m_codecCtx);

        return 0;
	}

	if (0 == m_DecodeData.audioBufSize)	/* no data in buffer */
	{
		bool bQueedEmpty = false;
        int32_t audioBufSize = _decodePacket(bQueedEmpty);
		if (audioBufSize < 0 || (bQueedEmpty && DecodeStatus.bReadFinished))
		{
			DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Finished;
			return -1;
		}

		if (0 == audioBufSize)
		{
			/* if not decoded data, just output silence */
			//m_DecodeData.audioBufSize = 1024;
			return 0;
		}

		m_DecodeData.audioBufSize = audioBufSize;
	}

    lpBuff = m_DecodeData.audioBuf;

	int len = m_DecodeData.audioBufSize;
	if (nBufSize > 0)
	{
        len = MIN(len, nBufSize);
	}

	m_DecodeData.audioBuf += len;
	m_DecodeData.audioBufSize -= len;

	return len;
}

int32_t AudioDecoder::_decodePacket(bool& bQueedEmpty)
{
	m_DecodeData.audioBuf = nullptr;

	AVPacket& packet = m_DecodeData.packet;
	/* get new packet while last packet all has been resolved */
	if (m_DecodeData.sendReturn != AVERROR(EAGAIN))
	{
		if (!m_packetQueue.dequeue(packet, false))
		{
			bQueedEmpty = true;
			return 0;
		}
	}
	
	/* while return -11 means packet have data not resolved, this packet cannot be unref */
	m_DecodeData.sendReturn = avcodec_send_packet(m_codecCtx, &packet);
	if (m_DecodeData.sendReturn < 0)
	{
		g_logger << "avcodec_send_packet fail: " >> m_DecodeData.sendReturn;

		//AVERROR_INVALIDDATA
		// AVERROR_EOF
		if (m_DecodeData.sendReturn != AVERROR(EAGAIN))
		{
			av_packet_unref(&packet);
			return -1;
		}
	}

    int32_t iRet = _receiveFrame();

	if (m_DecodeData.sendReturn != AVERROR(EAGAIN))
	{
        av_packet_unref(&packet);
	}

	return iRet;
}

int32_t AudioDecoder::_receiveFrame()
{
	AVFrame *frame = av_frame_alloc();
	if (NULL == frame)
    {
		return -1;
	}

	int nRet = avcodec_receive_frame(m_codecCtx, frame);
	if (nRet < 0)
	{
        av_frame_free(&frame);

        g_logger << "avcodec_receive_frame fail: " >> nRet;
		if (nRet == AVERROR(EAGAIN))
		{
			return 0;
		}
		
		return nRet;
	}

	int32_t audioBufSize = _convertFrame(*frame);

    if (AV_NOPTS_VALUE != frame->pts)
    {
        m_clock = uint64_t(m_timeBase * frame->pts);
    }
    else if (audioBufSize > 0)
	{
        m_clock += (uint64_t)audioBufSize * __1e6 / m_dstByteRate;
    }

	av_frame_free(&frame);

	return audioBufSize;
}

int32_t AudioDecoder::_convertFrame(AVFrame& frame)
{
	/* get audio channels */
	int64_t inChannelLayout = frame.channel_layout;
	if (0 == inChannelLayout || frame.channels != av_get_channel_layout_nb_channels(inChannelLayout))
	{
		inChannelLayout = av_get_default_channel_layout(frame.channels);
	}
	if (frame.format != m_DecodeData.audioSrcFmt ||	inChannelLayout != m_DecodeData.audioSrcChannelLayout
		|| frame.sample_rate != m_DecodeData.audioSrcFreq || !m_DecodeData.aCovertCtx)
	{
		if (m_DecodeData.aCovertCtx)
		{
			swr_free(&m_DecodeData.aCovertCtx);
			m_DecodeData.aCovertCtx = NULL;
		}

		/* init swr audio convert context */
		auto channelLayout = av_get_default_channel_layout(m_DevInfo.channels);

		SwrContext *aCovertCtx = swr_alloc_set_opts(nullptr, channelLayout, m_DevInfo.audioDstFmt
			, m_DevInfo.freq, inChannelLayout, (AVSampleFormat)frame.format, frame.sample_rate, 0, NULL);
		if (NULL == aCovertCtx)
		{
			return -1;
		}

		if (swr_init(aCovertCtx) < 0)
		{
			swr_free(&aCovertCtx);
			return -1;
		}

		m_DecodeData.aCovertCtx = aCovertCtx;
		m_DecodeData.audioSrcFmt = (AVSampleFormat)frame.format;
		m_DecodeData.audioSrcChannelLayout = inChannelLayout;
		m_DecodeData.audioSrcFreq = frame.sample_rate;
	}

	uint32_t audioBufSize = 0;
	if (m_DecodeData.aCovertCtx)
	{
		const uint8_t **in = (const uint8_t **)frame.extended_data;
		uint8_t *out[] = { m_DecodeData.swrAudioBuf };

		auto bytesPerSample = av_get_bytes_per_sample(m_DevInfo.audioDstFmt);
		int outCount = sizeof(m_DecodeData.swrAudioBuf) / m_DevInfo.channels / bytesPerSample;

		int sampleSize = swr_convert(m_DecodeData.aCovertCtx, out, outCount, in, frame.nb_samples);
		if (sampleSize < 0)
		{
			return -1;
		}

        if (sampleSize == outCount)
        {
            // "audio buffer is probably too small";
            if (swr_init(m_DecodeData.aCovertCtx) < 0)
            {
				swr_free(&m_DecodeData.aCovertCtx);
				m_DecodeData.aCovertCtx = NULL;
			}
		}

		m_DecodeData.audioBuf = m_DecodeData.swrAudioBuf;
		audioBufSize = sampleSize * m_DevInfo.channels * bytesPerSample;
	}
	else
	{
		m_DecodeData.audioBuf = frame.data[0];
		audioBufSize = av_samples_get_buffer_size(NULL, frame.channels, frame.nb_samples, static_cast<AVSampleFormat>(frame.format), 1);
	}

	return audioBufSize;
}

void AudioDecoder::close()
{
	m_packetQueue.clear();

    m_SLEngine.close();

	_clearData();
}

void AudioDecoder::_clearData()
{
	if (m_codecCtx)
	{
		avcodec_close(m_codecCtx);
		avcodec_free_context(&m_codecCtx);
		m_codecCtx = NULL;
	}
	
	m_DecodeData.init();

    m_clock = 0;

    m_seekPos = -1;
}
