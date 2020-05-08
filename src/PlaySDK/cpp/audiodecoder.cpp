
#include "../../../inc/Player.h"

#include "audiodecoder.h"

AudioDecoder::AudioDecoder(AvPacketQueue& packetQueue)
    : m_packetQueue(packetQueue)
    , m_SLEngine(
#if __android
    [&](size_t& uRetSize) {
        return _cb(0, uRetSize);
	}
#else
    [&](size_t uBuffSize, size_t& uRetSize) {
        return _cb(uBuffSize, uRetSize);
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
    }
	
	m_devInfo.channels = 0;
	m_devInfo.sample_fmt = AV_SAMPLE_FMT_NONE;
	m_devInfo.sample_rate = 0;
	
	m_sample_rate = 0;

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
        g_logger >> "avcodec_alloc_context3 fail";
        return false;
	}

	bool bRet = false;
	do {
		stream.discard = AVDISCARD_DEFAULT;
        int nRet = avcodec_parameters_to_context(m_codecCtx, stream.codecpar);
        if (nRet)
		{
            g_logger << "avcodec_parameters_to_context fail: " >> nRet;
			break;
		}

        if (m_codecCtx->channels <= 0)
        {
            g_logger >> "channels invalid";
            break;
        }
        if (m_codecCtx->sample_rate <= 0)
		{
            g_logger >> "sample_rate invalid";
			break;
		}

		AVCodec *codec = avcodec_find_decoder(m_codecCtx->codec_id);
		if (NULL == codec)
		{
            g_logger >> "avcodec_find_decoder fail";
			break;
		}

        nRet = avcodec_open2(m_codecCtx, codec, NULL);
        if (nRet)
		{
            g_logger << "avcodec_open2 fail: " >> nRet;
			break;
		}

		bRet = true;
	} while (0);

	if (!bRet)
	{
        avcodec_free_context(&m_codecCtx);
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
}

const uint8_t* AudioDecoder::_cb(size_t uBufSize, size_t& uRetSize)
{
    if (m_seekPos != -1)
    {
        m_SLEngine.clearbf();

        avcodec_flush_buffers(m_codecCtx);

        m_DecodeData.reset();

        m_clock = m_seekPos;
        m_seekPos = -1;

        return NULL;
    }

	if (0 == m_DecodeData.audioBufSize)
	{
        int32_t audioBufSize = _decodePacket();
		if (audioBufSize <= 0)
		{
            return NULL;
		}
		m_DecodeData.audioBufSize = audioBufSize;
	}

	auto lpBuff = m_DecodeData.audioBuf;

	if (0 == uBufSize || uBufSize >= m_DecodeData.audioBufSize)
	{
		uRetSize = m_DecodeData.audioBufSize;
		m_DecodeData.audioBufSize = 0;
	}
	else
	{
		uRetSize = uBufSize;
		m_DecodeData.audioBufSize -= uBufSize;
		m_DecodeData.audioBuf += uBufSize;
	}

	return lpBuff;
}

int32_t AudioDecoder::_decodePacket()
{
	AVPacket& packet = m_DecodeData.packet;
	/* get new packet while last packet all has been resolved */
    if (m_DecodeData.sendReturn != __eagain)
    {
        int nRet = m_packetQueue.dequeue(packet);
        if (-1 == nRet)
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
