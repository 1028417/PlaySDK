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
    virtual int64_t size() const = 0;

    virtual bool seekable() const = 0;

    virtual int64_t seek(int64_t offset, int origin) = 0;

    virtual int read(byte_p buf, size_t size) = 0;
};

class __PlaySDKExt CAudioOpaque : private IAudioOpaque
{
public:
    CAudioOpaque();

    virtual ~CAudioOpaque();

private:
    void *m_pDecoder = NULL;

    FILE *m_pf = NULL;

    int64_t m_nFileSize = 0;
	
public:
	void* decoder()
	{
		return m_pDecoder;
	}

    virtual void close();

    int64_t open(cwstr strFile);

	uint32_t checkDuration();
	uint32_t checkDuration(cwstr strFile);

    const E_DecodeStatus& decodeStatus() const;

	uint32_t duration() const;

    //uint32_t byteRate() const;

    int sampleRate() const;

	uint64_t clock() const;

public:
	virtual wstring localFilePath() const
	{
		return L"";
	}

    virtual int64_t size() const override
    {
        return m_nFileSize;
    }

    virtual bool seekable() const override
    {
        return true;
    }

protected:
    void setSize(int64_t nFileSize)
    {
        m_nFileSize = nFileSize;
    }

    virtual int64_t seek(int64_t offset, int origin) override;

    virtual int read(byte_p buf, size_t size) override;

    bool seekingFlag() const;
    //bool probingFlag() const;
};

class __PlaySDKExt CPlayer
{
public:
    CPlayer(CAudioOpaque& opaque)
        : m_audioOpaque(opaque)
    {
    }

    /*提速~CPlayer()
    {
        Stop();
    }*/
	
private:
    CAudioOpaque& m_audioOpaque;	
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

    bool isOpen() const;

    bool Pause();
    bool Resume();

    bool Seek(UINT uPos);

    void SetVolume(UINT uVolume);

    bool packetQueueEmpty() const;

    static UINT CheckDuration(cwstr strFile);
};

#define __checkDuration(file) CPlayer::CheckDuration(file)
