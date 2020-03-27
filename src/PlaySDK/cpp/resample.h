#pragma once

struct tagSwrSrc
{
	tagSwrSrc() = default;

	tagSwrSrc(const AVCodecContext& codecCtx)
		: channels(codecCtx.channels)
		, channel_layout(codecCtx.channel_layout)
		, sample_fmt(codecCtx.sample_fmt)
		, sample_rate(codecCtx.sample_rate)
	{
	}

	tagSwrSrc(const AVFrame& frame)
		: channels(frame.channels)
		, channel_layout(frame.channel_layout)
		, sample_fmt((AVSampleFormat)frame.format)
		, sample_rate(frame.sample_rate)
	{
	}

	int channels = 0;
	int64_t channel_layout = 0;
	AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;
	int sample_rate = 0;

	inline int64_t genChannelLayout()
	{
		if (0 == channel_layout || channels != av_get_channel_layout_nb_channels(channel_layout))
		{
			channel_layout = av_get_default_channel_layout(channels);
		}

		return channel_layout;
	}
};

class CResample
{
public:
	CResample() = default;

private:
	SwrContext *m_swrCtx = NULL;

	tagSwrSrc m_src;

	tagSLDevInfo m_dst;

	int m_bytesPerSample = 0;
	int m_out_count = 0;

	DECLARE_ALIGNED(16, uint8_t, m_buf)[192000];
	uint8_t *m_out[1]{ m_buf };

private:
	inline void _free()
	{
		swr_free(&m_swrCtx);
		m_swrCtx = NULL;
	}

	SwrContext* _init(tagSwrSrc& src);

	SwrContext* _getCtx(const AVFrame& frame);

public:
	const uint8_t* data() const
	{
		return m_buf;
	}

	bool init(const AVCodecContext& codecCtx, const tagSLDevInfo& devInfo);

	int convert(const AVFrame& frame);
};