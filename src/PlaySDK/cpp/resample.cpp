
#include "inc.h"

#include "resample.h"

inline SwrContext* CResample::_init(tagSwrSrc& src)
{
	_free();

	auto dstChannelLayout = av_get_default_channel_layout(m_dst.channels);
	m_swrCtx = swr_alloc_set_opts(nullptr, dstChannelLayout, m_dst.sample_fmt
		, m_dst.sample_rate, src.channel_layout, src.sample_fmt, src.sample_rate, 0, NULL);
	if (NULL == m_swrCtx)
	{
		return NULL;
	}

	if (swr_init(m_swrCtx) < 0)
	{
		_free();
		return NULL;
	}

	m_src = src;

	return m_swrCtx;
}

inline SwrContext* CResample::_getCtx(const AVFrame& frame)
{
	tagSwrSrc src(frame);
	if (m_swrCtx)
	{
		if (src.channels == m_src.channels && src.sample_fmt == m_src.sample_fmt && src.sample_rate == m_src.sample_rate)
		{
			if (src.channel_layout == m_src.channel_layout || src.genChannelLayout() == m_src.channel_layout)
			{
				return m_swrCtx;
			}
		}
		else
		{
			src.genChannelLayout();
		}
	}
	else
	{
		src.genChannelLayout();
	}

	return _init(src);
}

bool CResample::init(const AVCodecContext& codecCtx, const tagSLDevInfo& devInfo)
{
	if (m_swrCtx)
	{
		if (0 == memcmp(&devInfo, &m_dst, sizeof(tagSLDevInfo))
			&& codecCtx.channels == m_src.channels && codecCtx.sample_fmt == m_src.sample_fmt && codecCtx.sample_rate == m_src.sample_rate)
		{
			return m_swrCtx;
		}
	}

	m_dst = devInfo;
	m_bytesPerSample = av_get_bytes_per_sample(m_dst.sample_fmt) * m_dst.channels;
	m_out_count = sizeof(m_buf) / m_bytesPerSample;

	tagSwrSrc src(codecCtx);
	src.genChannelLayout();
	return NULL != _init(src);
}

int CResample::convert(const AVFrame& frame)
{
	auto ctx = _getCtx(frame);
	if (NULL == ctx)
	{
		return -1;
	}
	
	auto in = (const uint8_t **)frame.extended_data;
	int sampleSize = swr_convert(ctx, m_out, m_out_count, in, frame.nb_samples);
	if (sampleSize <= 0)
	{
		g_logger >> "swr_convert fail: " >> sampleSize;
		return -1;
	}

	if (sampleSize == m_out_count)
	{
		g_logger >> "swr audio buffer is probably too small";
		if (swr_init(ctx) < 0)
		{
			_free();
			return -1;
		}
	}

	return sampleSize * m_bytesPerSample;
}
