#pragma once

#include "util/util.h"

#if __winvc
#pragma warning(disable: 4251)
#endif

#ifdef __PlaySDKPrj
	#define __PlaySDKExt __dllexport
#else
	#define __PlaySDKExt __dllimport
#endif

enum class E_DecodeStatus
{
	DS_Decoding,
	DS_Paused,
	DS_Stop,
};

class IAudioOpaque
{
public:
    virtual wstring localFilePath() const
    {
        return L"";
    }

    virtual int64_t size() const = 0;

	virtual bool seekable() const
	{
		return true;
	}

    virtual int64_t seek(int64_t offset, int origin) = 0;

    virtual int read(byte_p buf, UINT size) = 0;
};

class __PlaySDKExt CAudioOpaque : private IAudioOpaque
{
public:
    CAudioOpaque();

    virtual ~CAudioOpaque();

private:
    void *m_pDecoder = NULL;

    FILE *m_pf = NULL;

protected:
    int64_t m_nFileSize = -1;
	
public:
	void* decoder()
	{
		return m_pDecoder;
	}

    virtual void close();

    int64_t open(cwstr strFile);

	uint32_t checkDuration();

    const E_DecodeStatus& decodeStatus() const;

	uint32_t duration() const;

    uint32_t byteRate() const;

    int sampleRate() const;

	uint64_t clock() const;

public:
	virtual int64_t size() const override
	{
	    return m_nFileSize;
	}

        virtual int64_t seek(int64_t offset, int origin) override;

        virtual int read(byte_p buf, UINT size) override;

protected:
        bool seekingFlag() const;

        //bool probingFlag() const;
};

class __PlaySDKExt CPlayer
{
public:
    CPlayer(CAudioOpaque& AudioOpaque)
        : m_AudioOpaque(AudioOpaque)
    {
    }

    ~CPlayer()
    {
        Stop();
    }
	
private:
    CAudioOpaque& m_AudioOpaque;
	
    mutex m_mutex;

    thread m_thread;

private:
    void _Stop();

public:
    static int InitSDK();
    static void QuitSDK();

    void Stop();
    bool Play(uint64_t uStartPos, bool bForce48KHz, const function<void(bool bPlayFinish)>& cbStop);
    void PlayStream(bool bForce48KHz, const function<void(bool bOpenSuccess, bool bPlayFinish)>& cbStop);

    bool Pause();
    bool Resume();

    bool Seek(UINT uPos);

    void SetVolume(UINT uVolume);

    bool packetQueueEmpty() const;
};
