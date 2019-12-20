
#include "../../../inc/Player.h"

#include "decoder.h"

int Decoder::_readOpaque(void *opaque, uint8_t *buf, int bufSize)
{
    IAudioOpaque& audioOpaque = *(IAudioOpaque*)opaque;

    bool bOnlineAudio = audioOpaque.isOnline();
    bool bWaitFlag = false;

    int nReadSize = 0;
    while (true)
    {
        auto eStatus = audioOpaque.decodeStatus();
        if (E_DecodeStatus::DS_Cancel == eStatus)//E_DecodeStatus::DS_Finished E_DecodeStatus::DS_OpenFail
        {
            return AVERROR_EOF;
        }
        if (E_DecodeStatus::DS_Paused == eStatus)
        {
            mtutil::usleep(50);
            continue;
        }

        if (bOnlineAudio)
        {
            size_t uPreserveSize = audioOpaque.checkPreserveDataSize();
            if (bWaitFlag)
            {
                if (uPreserveSize > 0)
                {
                    UINT uByteRate = audioOpaque.byteRate();
                    if (0 == uByteRate)
                    {
                        uByteRate = 512000;
                    }

                    size_t uWaitSize = uByteRate;
                    if (E_DecodeStatus::DS_Decoding == eStatus)
                    {
                        uWaitSize = uByteRate*6;
                    }
                    if (uPreserveSize < uWaitSize)
                    {
                        mtutil::usleep(50);
                        continue;
                    }
                }

                bWaitFlag = false;
            }
            else
            {
                if (0 == uPreserveSize)
                {
                    bWaitFlag = true;
                }
            }
        }

        nReadSize = audioOpaque.read(buf, (size_t)bufSize);
        if (nReadSize > 0)
        {
            break;
        }
        if (nReadSize < 0)
        {
            return AVERROR_EOF;
        }

        mtutil::usleep(50);
    }

    return nReadSize;
}

int64_t Decoder::_seekOpaque(void *decoder, int64_t offset, int whence)
{
	if (AVSEEK_SIZE == whence)
	{
		return -1; // 可直接返回-1
		//return audioOpaque.size();
	}

    return ((IAudioOpaque*)decoder)->seek(offset, whence);
}

E_DecoderRetCode Decoder::_checkStream()
{
	if (0 == avformat_find_stream_info(m_pFormatCtx, NULL))
	{
        for (m_audioIndex = 0; m_audioIndex < (int)m_pFormatCtx->nb_streams; m_audioIndex++)
		{
            AVStream& stream = *m_pFormatCtx->streams[m_audioIndex];
            AVCodecParameters& codecpar = *stream.codecpar;
            if (AVMEDIA_TYPE_AUDIO == codecpar.codec_type)
            {
                if (m_pFormatCtx->duration > 0)
				{
					m_duration = uint32_t(m_pFormatCtx->duration / __1e6);
                }
				else
                {
                    m_duration = 0;
					//return E_DecoderRetCode::DRC_InvalidAudioStream;
				}

                m_byteRate = 0;
                if (m_pFormatCtx->bit_rate > 0)
                {
                    m_byteRate = uint32_t(m_pFormatCtx->bit_rate/8);
                }

				return E_DecoderRetCode::DRC_Success;
			}
		}
	}

	return E_DecoderRetCode::DRC_NoAudioStream;
}

#define __avioBuffSize 4096//32768

E_DecoderRetCode Decoder::_open(IAudioOpaque& AudioOpaque)
{
	if (NULL == (m_pFormatCtx = avformat_alloc_context()))
	{
		return E_DecoderRetCode::DRC_Fail;
	}

	cauto strFile = AudioOpaque.localFilePath();
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
		byte_p avioBuff = NULL;
		if (NULL == (avioBuff = (byte_p)av_malloc(__avioBuffSize)))
		{
			return E_DecoderRetCode::DRC_Fail;
		}
		if (NULL == (m_avio = avio_alloc_context(avioBuff, __avioBuffSize, 0
			, &AudioOpaque, _readOpaque, NULL, AudioOpaque.seekable() ? _seekOpaque : NULL)))
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

uint32_t Decoder::check(IAudioOpaque& AudioOpaque)
{
	(void)_open(AudioOpaque);

	uint32_t uDuration = m_duration;

	_cleanup();

	return uDuration;
}

E_DecoderRetCode Decoder::open(bool bForce48KHz, IAudioOpaque& AudioOpaque)
{
	m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Decoding;

	auto eRet = _open(AudioOpaque);
	if (eRet != E_DecoderRetCode::DRC_Success)
    {
        _cleanup();

		if (m_DecodeStatus.eDecodeStatus != E_DecodeStatus::DS_Cancel)
		{
			m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Finished;
		}

		return eRet;
	}

	if (!m_audioDecoder.open(*m_pFormatCtx->streams[m_audioIndex], bForce48KHz))
	{
        _cleanup();

		if (m_DecodeStatus.eDecodeStatus != E_DecodeStatus::DS_Cancel)
		{
			m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Finished;
		}

		return E_DecoderRetCode::DRC_InitAudioDevFail;
	}

	return E_DecoderRetCode::DRC_Success;
}

E_DecodeStatus Decoder::start(IAudioOpaque& AudioOpaque)
{
    (void)AudioOpaque;

    auto& bReadFinished = m_DecodeStatus.bReadFinished;
    bReadFinished = false;

	AVPacket packet;
	while (true)
	{
		if (E_DecodeStatus::DS_Paused == m_DecodeStatus.eDecodeStatus)
		{
            mtutil::usleep(50);
			continue;
		}
		else if (E_DecodeStatus::DS_Decoding != m_DecodeStatus.eDecodeStatus)
		{
			break;
		}

		/* this seek just use in playing music, while read finished
		 * & have out of loop, then jump back to seek position */
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
            bReadFinished = true;
			break;
		}

        if (packet.stream_index == m_audioIndex)
		{
			if (m_audioDecoder.packetEnqueue(packet) > 100)
			{
				mtutil::usleep(30);
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

        mtutil::usleep(50);
	}

    /* close audio device */
    m_audioDecoder.close();

	_cleanup();

	return m_DecodeStatus.eDecodeStatus;
}

void Decoder::cancel()
{
    m_DecodeStatus.eDecodeStatus = E_DecodeStatus::DS_Cancel;
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

void Decoder::_cleanup()
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
	
    m_duration = 0;
    m_byteRate = 0;

	m_seekPos = -1;
}
