#pragma once

class CResample
{
public:
	CResample() = default;

private:
	SwrContext *m_swrCtx = NULL;

	int64_t m_src_channel_layout = 0;
	AVSampleFormat m_src_sample_fmt = AV_SAMPLE_FMT_NONE;
	int m_src_sample_rate = 0;

	uint8_t m_dst_channels = 0;
	int64_t m_dst_channel_layout = 0;
	AVSampleFormat m_dst_sample_fmt = AV_SAMPLE_FMT_NONE;
	int m_dst_sample_rate = 0;

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

	SwrContext* _init(int64_t src_channel_layout, AVSampleFormat src_sample_fmt, int src_sample_rate);
	
	SwrContext* _getCtx(const AVFrame& frame);

public:
    bool init(const AVCodecContext& codecCtx, const tagSLDevInfo& devInfo);

    const uint8_t* convert(const AVFrame& frame, int& nRetSize);
};
