
#include "../../../inc/Player.h"

#include "decoder.h"

#define __avioBuffSize 4096*8

int Decoder::_readOpaque(void *opaque, uint8_t *buf, int size)
{
    auto pDecoder = (Decoder*)opaque;
    if (!pDecoder->m_audioOpaque.read(buf, (UINT&)size))
    {
        return AVERROR_EOF;
    }

    return size;
}

int64_t Decoder::_seekOpaque(void *opaque, int64_t offset, int whence)
{
    auto pDecoder = (Decoder*)opaque;
	if (AVSEEK_SIZE == whence)
    {
        return pDecoder->m_audioOpaque.size(); //return -1; // 可直接返回-1
	}

    return pDecoder->m_audioOpaque.seek(offset, whence);
}

E_DecoderRetCode Decoder::_checkStream()
{
	if (0 == avformat_find_stream_info(m_pFormatCtx, NULL))
	{
        for (UINT uIdx = 0; uIdx < m_pFormatCtx->nb_streams; uIdx++)
		{
            AVStream& stream = *m_pFormatCtx->streams[uIdx];
            AVCodecParameters& codecpar = *stream.codecpar;
            if (AVMEDIA_TYPE_AUDIO == codecpar.codec_type)
            {
                m_audioStreamIdx = uIdx;

                if (m_pFormatCtx->duration > 0)
				{
					m_duration = uint32_t(m_pFormatCtx->duration / AV_TIME_BASE);
                }
				else
                {
                    m_duration = 0;
					//return E_DecoderRetCode::DRC_InvalidAudioStream;
				}
				
				return E_DecoderRetCode::DRC_Success;
			}
		}
	}

	return E_DecoderRetCode::DRC_NoAudioStream;
}

E_DecoderRetCode Decoder::_open()
{
    m_pFormatCtx = avformat_alloc_context();
    if (NULL == m_pFormatCtx)
	{
		return E_DecoderRetCode::DRC_Fail;
	}

    cauto strFile = m_audioOpaque.localFilePath();
	if (!strFile.empty())
	{
		int nRet = avformat_open_input(&m_pFormatCtx, strutil::toUtf8(strFile).c_str(), NULL, NULL);
		if (nRet != 0)
		{
			return E_DecoderRetCode::DRC_OpenFail;
		}
	}
	else
    {
        auto avioBuff = (unsigned char*)av_malloc(__avioBuffSize);
        if (NULL == avioBuff)
		{
			return E_DecoderRetCode::DRC_Fail;
        }
		m_pAvioCtx = avio_alloc_context(avioBuff, __avioBuffSize, 0, this, _readOpaque, NULL, _seekOpaque);
        if (NULL == m_pAvioCtx)
        {
            av_free(avioBuff);
			return E_DecoderRetCode::DRC_Fail;
		}
		m_pFormatCtx->pb = m_pAvioCtx;
		m_pFormatCtx->flags = AVFMT_FLAG_CUSTOM_IO;

		AVInputFormat *pInFmt = NULL;
		if (av_probe_input_buffer(m_pFormatCtx->pb, &pInFmt, NULL, NULL, 0, 0) < 0) // TODO
		{
			return E_DecoderRetCode::DRC_OpenFail;
		}

		int nRet = avformat_open_input(&m_pFormatCtx, NULL, pInFmt, NULL);
		if (nRet != 0)
		{
			return E_DecoderRetCode::DRC_OpenFail;
		}
	}

    return  _checkStream();
}

uint32_t Decoder::check()
{
    (void)_open();

	uint32_t uDuration = m_duration;

	_cleanup();

	return uDuration;
}

E_DecoderRetCode Decoder::open(bool bForce48KHz)
{
    m_eDecodeStatus = E_DecodeStatus::DS_Decoding;

    auto eRet = _open();
	if (eRet != E_DecoderRetCode::DRC_Success)
    {
        _cleanup();

        if (m_eDecodeStatus != E_DecodeStatus::DS_Cancel)
		{
            m_eDecodeStatus = E_DecodeStatus::DS_Finished;
		}

		return eRet;
	}

    if (!m_audioDecoder.open(*m_pFormatCtx->streams[m_audioStreamIdx], bForce48KHz))
	{
        _cleanup();

        if (m_eDecodeStatus != E_DecodeStatus::DS_Cancel)
		{
            m_eDecodeStatus = E_DecodeStatus::DS_Finished;
		}

		return E_DecoderRetCode::DRC_InitAudioDevFail;
	}

	return E_DecoderRetCode::DRC_Success;
}

E_DecodeStatus Decoder::start(uint64_t uPos)
{
	if (uPos > 0)
	{
		m_seekPos = uPos;
	}
	else
	{
		m_seekPos = -1;
	}

	_start();

	m_packetQueue.clear();

	/* close audio device */
	m_audioDecoder.close();

	_cleanup();

	return m_eDecodeStatus;
}

void Decoder::_start()
{
	bool bReadFinished = false;

	AVPacket packet;
	while (true)
	{
        if (E_DecodeStatus::DS_Paused == m_eDecodeStatus)
		{
            mtutil::usleep(50);
			continue;
		}
		
        if (E_DecodeStatus::DS_Decoding != m_eDecodeStatus)
		{
			return;
		}

	seek:
		if (m_seekPos>=0 && m_audioOpaque.seekable())
        {
            //avformat_flush(m_pFormatCtx);

            auto t_seekPos = av_rescale_q(m_seekPos, av_get_time_base_q()
				, m_pFormatCtx->streams[m_audioStreamIdx]->time_base);
            int nRet = av_seek_frame(m_pFormatCtx, m_audioStreamIdx, t_seekPos, AVSEEK_FLAG_BACKWARD); // seek到默认流指定位置之前最近的关键帧
			//av_seek_frame(m_pFormatCtx, -1, m_seekPos, AVSEEK_FLAG_BACKWARD);
            if (nRet < 0)
			{
                g_logger << "av_seek_frame fail: " >> nRet;
				
                if (bReadFinished)
				{
					m_seekPos = -1;
					break;
                }
            }
            else
            {
                if (m_pFormatCtx->pb)
                {
                    avio_flush(m_pFormatCtx->pb);
                }

                m_packetQueue.clear();

                m_audioDecoder.seek(m_seekPos);
            }
		}
		m_seekPos = -1;

		int nRet = av_read_frame(m_pFormatCtx, &packet);
		if (nRet < 0)
        {
			if (AVERROR_EOF != nRet)
			{
				g_logger << "av_read_frame fail: " >> nRet;
			}
			
			bReadFinished = true;
            break;
		}

        if (packet.stream_index == m_audioStreamIdx)
		{
            if (m_packetQueue.enqueue(packet) > 100)
			{
				mtutil::usleep(30);
			}
		}
		else
		{
			av_packet_unref(&packet);
		}
	}

	while (!m_packetQueue.isEmpty())
	{
        if (E_DecodeStatus::DS_Cancel == m_eDecodeStatus)
		{
            return;
		}

		if (m_seekPos>=0 && m_audioOpaque.seekable())
		{
			goto seek;
		}

        mtutil::usleep(50);
	}

    m_eDecodeStatus = E_DecodeStatus::DS_Finished;
}

void Decoder::cancel()
{
    m_eDecodeStatus = E_DecodeStatus::DS_Cancel;
}

bool Decoder::pause()
{
    if (E_DecodeStatus::DS_Decoding == m_eDecodeStatus)
    {
        if (m_audioStreamIdx >= 0)
        {
            m_eDecodeStatus = E_DecodeStatus::DS_Paused;

            m_audioDecoder.pause(true);
        }
        else
        {
            cancel();
        }

        return true;
	}

    return false;
}

bool Decoder::resume()
{
    if (E_DecodeStatus::DS_Paused == m_eDecodeStatus)
    {
        m_eDecodeStatus = E_DecodeStatus::DS_Decoding;
		m_audioDecoder.pause(false);

        return true;
	}

    return false;
}

bool Decoder::seek(uint64_t pos)
{
    if (m_audioStreamIdx >= 0)
    {
        //if (-1 == m_seekPos)
        m_seekPos = pos;

        if (E_DecodeStatus::DS_Paused == m_eDecodeStatus)
        {
            m_packetQueue.clear();

            m_eDecodeStatus = E_DecodeStatus::DS_Decoding;
            m_audioDecoder.pause(false);
        }

        return true;
    }

    return false;
}

void Decoder::setVolume(uint8_t volume)
{
    m_audioDecoder.setVolume(volume);
}

void Decoder::_cleanup()
{
    if (m_pFormatCtx)
	{
		if (m_pAvioCtx)
        {
			av_freep(&m_pAvioCtx->buffer);
			avio_context_free(&m_pAvioCtx);
			m_pFormatCtx->pb = NULL;
		}

        //avformat_free_context(m_pFormatCtx);
        //m_pFormatCtx = NULL;
		avformat_close_input(&m_pFormatCtx);
    }

    m_audioStreamIdx = -1;

    m_duration = 0;
    
	m_seekPos = -1;
}
