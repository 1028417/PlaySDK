
#include "../../../inc/Player.h"

#include "decoder.h"

#define __avioBuffSize 4096*8

int Decoder::_readOpaque(void *opaque, uint8_t *buf, int size)
{
    int nRet = ((IAudioOpaque*)opaque)->read(buf, (UINT)size);
    if (nRet < 0)
    {
        nRet = AVERROR_EXIT;
    }
    else if (0 == nRet)
    {
        nRet = AVERROR_EOF;
    }

    return nRet;
}

int64_t Decoder::_seekOpaque(void *opaque, int64_t offset, int whence)
{
    auto pAudioOpaque = (IAudioOpaque*)opaque;
	if (AVSEEK_SIZE == whence)
    {        
        //return 0;
        return pAudioOpaque->size(); //return -1; //据说可以直接返回-1
	}

    return pAudioOpaque->seek(offset, whence);
}

E_DecoderRetCode Decoder::_checkStream()
{
	if (0 == avformat_find_stream_info(m_fmtCtx, NULL))
	{
        for (UINT uIdx = 0; uIdx < m_fmtCtx->nb_streams; uIdx++)
		{
            AVStream& stream = *m_fmtCtx->streams[uIdx];
            if (AVMEDIA_TYPE_AUDIO == stream.codecpar->codec_type)
            {
                m_audioStreamIdx = uIdx;

                if (m_fmtCtx->duration > 0)
				{
					m_duration = uint32_t(m_fmtCtx->duration / AV_TIME_BASE);
                }
				else
                {
                    g_logger >> "duration invalid";
					//return E_DecoderRetCode::DRC_InvalidAudioStream;
				}
				
                if (m_fmtCtx->bit_rate > 0)
                {
                    m_byteRate = uint32_t(m_fmtCtx->bit_rate/8);
                }

				return E_DecoderRetCode::DRC_Success;
			}
		}
	}

    g_logger >> "_checkStream fail";
	return E_DecoderRetCode::DRC_NoAudioStream;
}

E_DecoderRetCode Decoder::_open()
{
    cauto strFile = m_audioOpaque.localFilePath();
	if (!strFile.empty())
	{
        m_fmtCtx = avformat_alloc_context();
        if (NULL == m_fmtCtx)
        {
            g_logger >> "avformat_alloc_context fail";
            return E_DecoderRetCode::DRC_Fail;
        }

		int nRet = avformat_open_input(&m_fmtCtx, strutil::toUtf8(strFile).c_str(), NULL, NULL);
        if (nRet)
        {
            g_logger << "avformat_open_input fail: " >> nRet;
			return E_DecoderRetCode::DRC_OpenFail;
		}
	}
	else
    {
        auto avioBuff = (unsigned char*)av_malloc(__avioBuffSize);
        if (NULL == avioBuff)
		{
            g_logger >> "av_malloc fail";
			return E_DecoderRetCode::DRC_Fail;
        }
        m_avioCtx = avio_alloc_context(avioBuff, __avioBuffSize, 0, &m_audioOpaque, _readOpaque, NULL, _seekOpaque);
        if (NULL == m_avioCtx)
        {
            g_logger >> "avio_alloc_context fail";
            av_free(avioBuff);
			return E_DecoderRetCode::DRC_Fail;
		}

        AVInputFormat *pInFmt = NULL;

/*
        //m_bProbing = true;
        int nScore = av_probe_input_buffer2(m_avioCtx, &pInFmt, NULL, NULL, 0, 0); //__avioBuffSize*3);
        if (nScore <= 0)
        {
            if (m_eDecodeStatus != E_DecodeStatus::DS_Stop)
            {
                g_logger << "av_probe_input_buffer fail, score: " >> nScore;
            }
			return E_DecoderRetCode::DRC_OpenFail;
		}
        //m_bProbing = false;
*/
        //pInFmt = av_find_input_format("m4a"); // 不探测直接指定格式

        m_fmtCtx = avformat_alloc_context();
        if (NULL == m_fmtCtx)
        {
            g_logger >> "avformat_alloc_context fail";
            return E_DecoderRetCode::DRC_Fail;
        }
		m_fmtCtx->pb = m_avioCtx;
        m_fmtCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

        int nRet = avformat_open_input(&m_fmtCtx, NULL, pInFmt, NULL);
        if (nRet)
        {
            g_logger << "avformat_open_input fail: " >> nRet;
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
		m_eDecodeStatus = E_DecodeStatus::DS_Stop;		
		return eRet;
	}

    if (!m_audioDecoder.open(*m_fmtCtx->streams[m_audioStreamIdx], bForce48KHz))
	{
        _cleanup();		
		m_eDecodeStatus = E_DecodeStatus::DS_Stop;
		return E_DecoderRetCode::DRC_InitAudioDevFail;
	}

	return E_DecoderRetCode::DRC_Success;
}

bool Decoder::start(uint64_t uPos)
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

	m_audioDecoder.close();

    _cleanup();

	if (E_DecodeStatus::DS_Stop == m_eDecodeStatus)
	{
		return false;
	}

	m_eDecodeStatus = E_DecodeStatus::DS_Stop;
	return true;
}

void Decoder::_start()
{
	bool bReadFinished = false;

	AVPacket packet;
    while (true)
    {
        if (E_DecodeStatus::DS_Stop == m_eDecodeStatus)
        {
            //m_packetQueue.clear();
            break;
        }

        if (E_DecodeStatus::DS_Paused == m_eDecodeStatus)
		{
            __usleep(50);
			continue;
        }

		if (m_seekPos >= 0)
		{
			auto t_seekPos = m_seekPos;
			m_seekPos = -1;
			if (m_audioOpaque.seekable())
			{
				auto tbPos = av_rescale_q(t_seekPos, av_get_time_base_q()
					, m_fmtCtx->streams[m_audioStreamIdx]->time_base);
				int nRet = av_seek_frame(m_fmtCtx, m_audioStreamIdx, tbPos, AVSEEK_FLAG_BACKWARD); // seek到默认流指定位置之前最近的关键帧
				//av_seek_frame(m_pFormatCtx, -1, t_seekPos, AVSEEK_FLAG_BACKWARD);
				if (nRet < 0)
				{
					g_logger << "av_seek_frame fail: " >> nRet;

//					if (bReadFinished)
//					{
//						continue;
//					}
				}
				else
				{
					bReadFinished = false;

                    m_packetQueue.clear();

					m_audioDecoder.seek(t_seekPos);

                    if (m_avioCtx)
                    {
                        avio_flush(m_avioCtx);
                    }
				}
			}
		}

        if (bReadFinished)
        {
            if (m_packetQueue.isEmpty())
            {
                //m_eDecodeStatus = E_DecodeStatus::DS_Finished;
                break;
            }

            __usleep(50);
            continue;
        }

		int nRet = av_read_frame(m_fmtCtx, &packet);
		if (nRet < 0)
        {
            if (AVERROR_EXIT == nRet)
            {
                //m_eDecodeStatus = E_DecodeStatus::DS_Finished;
                //m_packetQueue.clear();
                break;
            }

			if (AVERROR_EOF != nRet)
			{
				g_logger << "av_read_frame fail: " >> nRet;
			}
			
			bReadFinished = true;
            continue;
		}

        if (packet.stream_index == m_audioStreamIdx)
		{
            if (m_packetQueue.enqueue(packet) > 300)
			{
				__usleep(30);
			}
		}
		else
		{
			av_packet_unref(&packet);
		}
    }
}

void Decoder::cancel()
{
    m_eDecodeStatus = E_DecodeStatus::DS_Stop;
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
    if (m_fmtCtx)
    {
        if (m_avioCtx)
        {
            av_freep(&m_avioCtx->buffer);
            avio_context_free(&m_avioCtx);
            m_fmtCtx->pb = NULL;
        }

		avformat_close_input(&m_fmtCtx);
    }

    m_audioStreamIdx = -1;

    m_duration = 0;
    m_byteRate = 0;
    
	m_seekPos = -1;
}
