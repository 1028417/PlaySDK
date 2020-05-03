
#include "../../../inc/Player.h"

#include "audiodecoder.h"

AudioDecoder::AudioDecoder(AvPacketQueue& packetQueue)
    : m_packetQueue(packetQueue)
    , m_SLEngine(
#if __android
    [&](const uint8_t*& lpBuff) {
        return _cb(lpBuff, 0);
	}
#else
    [&](const uint8_t*& lpBuff, int size) {
        return _cb(lpBuff, size);
	}
#endif
)
{
}

void AudioDecoder::_cleanup()
{
    if (m_codecCtx)
    {
        avcodec_close(m_codecCtx);
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = NULL;
    }
	
	m_sample_rate = 0;
	
	m_devInfo.channels = 0;
	m_devInfo.sample_fmt = AV_SAMPLE_FMT_NONE;
	m_devInfo.sample_rate = 0;

	m_dstByteRate = 0;

    m_timeBase = 0;
    m_clock = 0;
    
	m_seekPos = -1;

    m_DecodeData.reset();
}

bool AudioDecoder::open(AVStream& stream, bool bForce48KHz)
{
	m_codecCtx = avcodec_alloc_context3(NULL);
	if (NULL == m_codecCtx)
	{
        return false;
	}

	bool bRet = false;
	do {
		stream.discard = AVDISCARD_DEFAULT;
		if (avcodec_parameters_to_context(m_codecCtx, stream.codecpar) < 0)
		{
			break;
		}

		if (m_codecCtx->channels <= 0 || m_codecCtx->sample_rate <= 0)
		{
			break;
		}

		AVCodec *codec = avcodec_find_decoder(m_codecCtx->codec_id);
		if (NULL == codec)
		{
			break;
		}

		if (avcodec_open2(m_codecCtx, codec, NULL) < 0)
		{
			break;
		}

		bRet = true;
	} while (0);

	if (!bRet)
	{
		avcodec_free_context(&m_codecCtx);
		m_codecCtx = NULL;
        return false;
    }

	m_devInfo.channels = m_codecCtx->channels;

	m_devInfo.sample_fmt = m_codecCtx->sample_fmt;

	m_devInfo.sample_rate = m_sample_rate = m_codecCtx->sample_rate;
	if (bForce48KHz && m_devInfo.sample_rate < 48000)
	{
		m_devInfo.sample_rate = 48000;
	}
	
    if (!m_SLEngine.open(m_devInfo))
	{
		this->_cleanup();
        return false;
	}

	if (!m_DecodeData.swr.init(*m_codecCtx, m_devInfo))
	{
		this->_cleanup();
		return false;
	}

    uint8_t audioDstDepth = 2;
    if (AV_SAMPLE_FMT_U8 == m_devInfo.sample_fmt)
    {
		audioDstDepth = 1;
    }
    else if (AV_SAMPLE_FMT_S32 == m_devInfo.sample_fmt || AV_SAMPLE_FMT_FLT == m_devInfo.sample_fmt)
    {
		audioDstDepth = 4;
    }
    m_dstByteRate = m_codecCtx->channels * m_devInfo.sample_rate * audioDstDepth;

	m_timeBase = av_q2d(stream.time_base)*AV_TIME_BASE;
	m_clock = 0;
    
    return true;
}

void AudioDecoder::pause(bool bPause)
{
    m_SLEngine.pause(bPause);
}

void AudioDecoder::seek(uint64_t pos)
{
    m_seekPos = pos;
    mtutil::usleep(50);
}

size_t AudioDecoder::_cb(const uint8_t*& lpBuff, int nBufSize)
{
    if (m_seekPos != -1)
    {
        m_SLEngine.clearbf();

        avcodec_flush_buffers(m_codecCtx);

        m_DecodeData.reset();

        m_clock = m_seekPos;
        m_seekPos = -1;

        return 0;
    }

	if (0 == m_DecodeData.audioBufSize)
	{
		m_DecodeData.audioBuf = nullptr;
        int32_t audioBufSize = _decodePacket();
		if (audioBufSize <= 0)
		{
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

int32_t AudioDecoder::_decodePacket()
{
	AVPacket& packet = m_DecodeData.packet;
	/* get new packet while last packet all has been resolved */
    if (m_DecodeData.sendReturn != __eagain)
	{
		if (!m_packetQueue.dequeue(packet))
		{
			return 0;
		}
	}
	
	/* while return -11 means packet have data not resolved, this packet cannot be unref */
	m_DecodeData.sendReturn = avcodec_send_packet(m_codecCtx, &packet);
	if (m_DecodeData.sendReturn < 0)
    {
		//AVERROR_INVALIDDATA
		// AVERROR_EOF
        if (m_DecodeData.sendReturn != __eagain)
		{
            av_packet_unref(&packet);

            g_logger << "avcodec_send_packet fail: " >> m_DecodeData.sendReturn;
            return m_DecodeData.sendReturn;
		}
	}

    int32_t nRet = _recvFrame();

    if (m_DecodeData.sendReturn != __eagain)
	{
        av_packet_unref(&packet);
	}

	return nRet;
}

int32_t AudioDecoder::_recvFrame()
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
        if (__eagain == nRet)
		{
			return 0;
		}

        g_logger << "avcodec_receive_frame fail: " >> nRet;
		return nRet;
	}

	if (0 == frame->channels)
	{
		frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
	}

	int32_t audioBufSize = 0;
	if (frame->channels == m_devInfo.channels && frame->format == m_devInfo.sample_fmt && frame->sample_rate == m_devInfo.sample_rate)
	{
		m_DecodeData.audioBuf = frame->data[0];
		audioBufSize = av_samples_get_buffer_size(NULL //frame->linesize
			, frame->channels, frame->nb_samples, (AVSampleFormat)frame->format, 1);
	}
	else
	{
		audioBufSize = m_DecodeData.swr.convert(*frame);
		if (audioBufSize > 0)
		{
			m_DecodeData.audioBuf = m_DecodeData.swr.data();
		}
	}

    if (AV_NOPTS_VALUE != frame->pts)
    {
        m_clock = uint64_t(m_timeBase * frame->pts);
    }
    else if (audioBufSize > 0)
	{
        m_clock += (uint64_t)audioBufSize * AV_TIME_BASE / m_dstByteRate;
    }

	av_frame_free(&frame);

	return audioBufSize;
}

void AudioDecoder::close()
{
    m_SLEngine.close();

	_cleanup();
}
