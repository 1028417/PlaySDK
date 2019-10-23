
#include "../../../inc/Player.h"

#include "decoder.h"

void Decoder::setVolume(uint8_t volume)
{
	m_audioDecoder.setVolume(volume);
}

void Decoder::seek(uint64_t pos)
{
	//if (-1 == m_seekPos)
	{
		m_seekPos = pos;
	}
}

static int read_file(void *opaque, uint8_t *buf, int bufSize)
{
    CAudioOpaque& audioOpaque = *(CAudioOpaque*)opaque;

    int nReadSize = 0;
    while (true)
    {
        nReadSize = audioOpaque.read(buf, (size_t)bufSize);
        if (nReadSize > 0)
        {
            break;
        }

        if (nReadSize < 0)
        {
            return AVERROR_EOF;
        }

        if (audioOpaque.pos() == 0)
        {
            mtutil::usleep(100);
        }
        else
        {
			Decoder *pDecoder = (Decoder*)audioOpaque.decoder();
            for (UINT uIdx = 0; uIdx < 100; )
            {
                auto eStatus = pDecoder->decodeStatus();
                if (E_DecodeStatus::DS_Cancel == eStatus)
                {
                    return AVERROR_EOF;
                }

                if (eStatus != E_DecodeStatus::DS_Paused)
                {
                    uIdx++;
                }

                mtutil::usleep(100);
            }
        }
    }

    return nReadSize;
}

static int64_t seek_file(void *opaque, int64_t offset, int whence)
{
	if (AVSEEK_SIZE == whence)
	{
		return -1; // 可直接返回-1
		//return audioOpaque.size();
	}

    CAudioOpaque& audioOpaque = *(CAudioOpaque*)opaque;
	return audioOpaque.seek(offset, (E_SeekFileFlag)whence);
}

E_DecoderRetCode Decoder::_checkStream()
{
	if (0 == avformat_find_stream_info(m_pFormatCtx, NULL))
	{
		for (unsigned int i = 0; i < m_pFormatCtx->nb_streams; i++)
		{
			if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				m_audioIndex = i;

				if (m_pFormatCtx->duration > 0)
				{
					m_duration = uint32_t(m_pFormatCtx->duration / __1e6);
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

#define __avioBuffSize 4096//32768

E_DecoderRetCode Decoder::_open()
{
	if (NULL == (m_pFormatCtx = avformat_alloc_context()))
	{
		return E_DecoderRetCode::DRC_Fail;
	}

    cauto strFile = m_AudioOpaque.file();
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
		unsigned char *avioBuff = NULL;
		if (NULL == (avioBuff = (unsigned char *)av_malloc(__avioBuffSize)))
		{
			return E_DecoderRetCode::DRC_Fail;
		}
		if (NULL == (m_avio = avio_alloc_context(avioBuff, __avioBuffSize, 0
			, &m_AudioOpaque, read_file, NULL, m_AudioOpaque.seekable() ? seek_file : NULL)))
		{
			return E_DecoderRetCode::DRC_Fail;
		}
		m_pFormatCtx->pb = m_avio;
		m_pFormatCtx->flags = AVFMT_FLAG_CUSTOM_IO;

		AVInputFormat *pInFmt = NULL;
		if (av_probe_input_buffer(m_avio, &pInFmt, NULL, NULL, 0, 0) < 0)
		{
			return E_DecoderRetCode::DRC_OpenFail;
		}

		int nRet = avformat_open_input(&m_pFormatCtx, NULL, pInFmt, NULL);
		if (nRet != 0)
		{
			return E_DecoderRetCode::DRC_Fail;
		}
	}

    return  _checkStream();
}

E_DecoderRetCode Decoder::open(bool bForce48000)
{
	m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Opening;

	auto eRet = _open();
	if (eRet != E_DecoderRetCode::DRC_Success)
	{
		_clearData();
		m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Cancel;
		return eRet;
	}

	if (!m_audioDecoder.open(*m_pFormatCtx->streams[m_audioIndex], bForce48000))
	{
		_clearData();
		m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Cancel;
		return E_DecoderRetCode::DRC_InitAudioDevFail;
	}

	return E_DecoderRetCode::DRC_Success;
}

E_DecodeStatus Decoder::start()
{    
	m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Decoding;

    auto& bReadFinished = m_DecodeStatus.bReadFinished;
    bReadFinished = false;

	AVPacket packet;
	while (true)
	{
		if (E_DecodeStatus::DS_Paused == m_DecodeStatus.eDecodeStatus)
		{
            mtutil::usleep(10);
			continue;
		}
		else if (E_DecodeStatus::DS_Decoding != m_DecodeStatus.eDecodeStatus)
		{
			break;
		}

		/* this seek just use in playing music, while read finished
		 * & have out of loop, then jump back to seek position
		 */
	seek:
		if (m_seekPos>=0)
		{
			int64_t seekPos = av_rescale_q(m_seekPos, av_get_time_base_q(), m_pFormatCtx->streams[m_audioIndex]->time_base);
			if (av_seek_frame(m_pFormatCtx, m_audioIndex, seekPos, AVSEEK_FLAG_BACKWARD) < 0)
			{
				//QDebug() << "Seek failed.";
                if (bReadFinished)
				{
					break;
				}
				else
				{
					continue;
				}
			}

            bReadFinished = false;

			m_audioDecoder.seek(m_seekPos);
			
			m_seekPos = -1;
		}

		/* judge haven't reall all frame */
		int nRet = av_read_frame(m_pFormatCtx, &packet);
		if (nRet < 0)
		{
			//QDebug() << "Read file completed.";
            bReadFinished = true;
            //mtutil::usleep(10);
			break;
		}

		if (packet.stream_index == m_audioIndex) {
			if (m_audioDecoder.packetEnqueue(packet) > 1000)
			{
				mtutil::usleep(50);
			}
		}
		else
		{
			av_packet_unref(&packet);
		}
	}

	while (E_DecodeStatus::DS_Cancel != m_DecodeStatus.eDecodeStatus
		&& E_DecodeStatus::DS_Finished != m_DecodeStatus.eDecodeStatus)
    {
        /* just use at audio playing */
		if (m_seekPos>=0)
		{
			goto seek;
		}

        mtutil::usleep(10);
	}

    /* close audio device */
    m_audioDecoder.close();

	_clearData();

	return m_DecodeStatus.eDecodeStatus;
}

void Decoder::pause()
{
	if (E_DecodeStatus::DS_Decoding == m_DecodeStatus.eDecodeStatus)
	{
		m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Paused;

		//av_read_pause(m_pFormatCtx); // 暂停网络流

		m_audioDecoder.pause(true);
	}
}

void Decoder::resume()
{
	if (E_DecodeStatus::DS_Paused == m_DecodeStatus.eDecodeStatus)
	{
		//av_read_play(m_pFormatCtx); // 恢复网络流
		m_audioDecoder.pause(false);

		m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Decoding;
	}
}

void Decoder::cancel()
{
	m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Cancel;
}

void Decoder::_clearData()
{
    if (m_pFormatCtx)
	{
		avformat_close_input(&m_pFormatCtx);
		avformat_free_context(m_pFormatCtx);
		m_pFormatCtx = NULL;
	}

    if (m_avio)
	{
		av_freep(&m_avio->buffer);
		avio_context_free(&m_avio);
		m_avio = NULL;
	}
	
	m_audioIndex = -1;

    m_duration = 0;

	m_seekPos = -1;
}
