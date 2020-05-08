
#include "inc.h"

#include "resample.h"

inline SwrContext* CResample::_init(int64_t src_channel_layout, AVSampleFormat src_sample_fmt, int src_sample_rate)
{
	_free();

	m_swrCtx = swr_alloc_set_opts(nullptr, m_dst_channel_layout, m_dst_sample_fmt
		, m_dst_sample_rate, src_channel_layout, src_sample_fmt, src_sample_rate, 0, NULL);
	if (NULL == m_swrCtx)
	{
        g_logger >> "swr_alloc_set_opts fail";
		return NULL;
	}

    int nRet = swr_init(m_swrCtx);
    if (nRet)
	{
        g_logger << "swr_init fail: " >> nRet;
		_free();
		return NULL;
	}

	m_src_channel_layout = src_channel_layout;
	m_src_sample_fmt = src_sample_fmt;
	m_src_sample_rate = src_sample_rate;

	return m_swrCtx;
}

inline static int64_t _get_channel_layout(int channels, int64_t channel_layout)
{
	if (channels > 0)
	{
		if (0 == channel_layout || av_get_channel_layout_nb_channels(channel_layout) != channels)
		{
			return av_get_default_channel_layout(channels);
		}
	}

	return channel_layout;
}

inline SwrContext* CResample::_getCtx(const AVFrame& frame)
{
	auto src_channel_layout = _get_channel_layout(frame.channels, frame.channel_layout);

	if (m_swrCtx)
	{
		if (src_channel_layout == m_src_channel_layout && frame.format == m_src_sample_fmt && frame.sample_rate == m_src_sample_rate)
		{
			return m_swrCtx;
		}
	}

	return _init(src_channel_layout, (AVSampleFormat)frame.format, frame.sample_rate);
}

bool CResample::init(const AVCodecContext& codecCtx, const tagSLDevInfo& devInfo)
{
	auto src_channel_layout = _get_channel_layout(codecCtx.channels, codecCtx.channel_layout);

	if (m_swrCtx)
	{
        if (devInfo.channels == m_dst_channels && devInfo.sample_fmt == m_dst_sample_fmt && devInfo.sample_rate == m_dst_sample_rate
			&& src_channel_layout == m_src_channel_layout && codecCtx.sample_fmt == m_src_sample_fmt && codecCtx.sample_rate == m_src_sample_rate)
		{
            return true;
		}
	}

	m_dst_channels = devInfo.channels;
	m_dst_channel_layout = av_get_default_channel_layout(devInfo.channels);
	m_dst_sample_fmt = devInfo.sample_fmt;
	m_dst_sample_rate = devInfo.sample_rate;

	m_bytesPerSample = av_get_bytes_per_sample(m_dst_sample_fmt) * m_dst_channels;
	if (0 == m_bytesPerSample)
	{
		g_logger << "init resample fail, invalid dst_sample_fmt: " >> m_dst_sample_fmt;
		return false;
	}
	m_out_count = sizeof(m_buf) / m_bytesPerSample;

	return NULL != _init(src_channel_layout, codecCtx.sample_fmt, codecCtx.sample_rate);
}

int CResample::convert(const AVFrame& frame)
{
	auto ctx = _getCtx(frame);
	if (NULL == ctx)
	{
		return -1;
	}
	
	auto in = (const uint8_t **)frame.extended_data;
	int samples = swr_convert(ctx, m_out, m_out_count, in, frame.nb_samples);
	if (samples <= 0)
	{
		g_logger << "swr_convert fail: " >> samples;
		return -1;
	}

	if (samples == m_out_count)
	{
		g_logger >> "swr audio buffer is probably too small";
		if (swr_init(ctx) < 0)
		{
			_free();
			return -1;
		}
	}

	return samples * m_bytesPerSample;
}
