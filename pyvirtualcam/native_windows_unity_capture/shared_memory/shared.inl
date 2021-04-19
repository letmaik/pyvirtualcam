/*
  Unity Capture
  Copyright (c) 2018 Bernhard Schelling

  Based on UnityCam
  https://github.com/mrayy/UnityCam
  Copyright (c) 2016 MHD Yamen Saraiji
*/

#define _HAS_EXCEPTIONS 0
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <stdint.h>

#define MAX_SHARED_IMAGE_SIZE (3840 * 2160 * 4 * sizeof(short)) //4K (RGBA max 16bit per pixel)

#if _DEBUG
#define UCASSERT(cond) ((cond) ? ((void)0) : *(volatile int*)0 = 0xbad|(OutputDebugStringA("[FAILED ASSERT] " #cond "\n"),1))
#else
#define UCASSERT(cond) ((void)0)
#endif

struct SharedImageMemory
{
	SharedImageMemory(int32_t CapNum)
	{
		memset(this, 0, sizeof(*this));
		m_CapNum = CapNum;
	}

	~SharedImageMemory()
	{
		if (m_hMutex) CloseHandle(m_hMutex);
		if (m_hWantFrameEvent) CloseHandle(m_hWantFrameEvent);
		if (m_hSentFrameEvent) CloseHandle(m_hSentFrameEvent);
		if (m_hSharedFile) CloseHandle(m_hSharedFile);
	}

	int32_t GetCapNum() { return m_CapNum; }
	enum { MAX_CAPNUM = ('z' - '0') }; //see Open() for why this number
	enum { RECEIVE_MAX_WAIT = 200 }; //How many milliseconds to wait for new frame
	enum EFormat { FORMAT_UINT8, FORMAT_FP16_GAMMA, FORMAT_FP16_LINEAR };
	enum EResizeMode { RESIZEMODE_DISABLED = 0, RESIZEMODE_LINEAR = 1 };
	enum EMirrorMode { MIRRORMODE_DISABLED = 0, MIRRORMODE_HORIZONTALLY = 1 };
	enum EReceiveResult { RECEIVERES_CAPTUREINACTIVE, RECEIVERES_NEWFRAME, RECEIVERES_OLDFRAME };

	typedef void (*ReceiveCallbackFunc)(int width, int height, int stride, EFormat format, EResizeMode resizemode, EMirrorMode mirrormode, int timeout, uint8_t* buffer, void* callback_data);

	EReceiveResult Receive(ReceiveCallbackFunc callback, void* callback_data)
	{
		if (!Open(true) || !m_pSharedBuf->width) return RECEIVERES_CAPTUREINACTIVE;

		SetEvent(m_hWantFrameEvent);
		bool IsNewFrame = (WaitForSingleObject(m_hSentFrameEvent, RECEIVE_MAX_WAIT) == WAIT_OBJECT_0);

		WaitForSingleObject(m_hMutex, INFINITE); //lock mutex
		callback(m_pSharedBuf->width, m_pSharedBuf->height, m_pSharedBuf->stride, (EFormat)m_pSharedBuf->format, (EResizeMode)m_pSharedBuf->resizemode, (EMirrorMode)m_pSharedBuf->mirrormode, m_pSharedBuf->timeout, m_pSharedBuf->data, callback_data);
		ReleaseMutex(m_hMutex); //unlock mutex

		return (IsNewFrame ? RECEIVERES_NEWFRAME : RECEIVERES_OLDFRAME);
	}

	bool SendIsReady()
	{
		return Open(false);
	}

	enum ESendResult { SENDRES_TOOLARGE, SENDRES_WARN_FRAMESKIP, SENDRES_OK };
	ESendResult Send(int width, int height, int stride, DWORD DataSize, EFormat format, EResizeMode resizemode, EMirrorMode mirrormode, int timeout, const uint8_t* buffer)
	{
		UCASSERT(buffer);
		UCASSERT(m_pSharedBuf);
		if (m_pSharedBuf->maxSize < DataSize) return SENDRES_TOOLARGE;

		WaitForSingleObject(m_hMutex, INFINITE); //lock mutex
		m_pSharedBuf->width = width;
		m_pSharedBuf->height = height;
		m_pSharedBuf->stride = stride;
		m_pSharedBuf->format = format;
		m_pSharedBuf->resizemode = resizemode;
		m_pSharedBuf->mirrormode = mirrormode;
		m_pSharedBuf->timeout = timeout;
		memcpy(m_pSharedBuf->data, buffer, DataSize);
		ReleaseMutex(m_hMutex); //unlock mutex

		SetEvent(m_hSentFrameEvent);
		bool DidSkipFrame = (WaitForSingleObject(m_hWantFrameEvent, 0) != WAIT_OBJECT_0);

		return (DidSkipFrame ? SENDRES_WARN_FRAMESKIP : SENDRES_OK);
	}

private:
	bool Open(bool ForReceiving)
	{
		if (m_pSharedBuf) return true; //already open

		UCASSERT(m_CapNum <= MAX_CAPNUM);
		if (m_CapNum > MAX_CAPNUM) m_CapNum = MAX_CAPNUM;
		char CSCapNumChar = (m_CapNum ? '0' + m_CapNum : '\0'); //use NULL terminator for CapNum 0 to be compatible with old filter DLLs before multi cap
		char CS_NAME_MUTEX      [] = "UnityCapture_Mutx0"; CS_NAME_MUTEX      [sizeof(CS_NAME_MUTEX      ) - 2] = CSCapNumChar;
		char CS_NAME_EVENT_WANT [] = "UnityCapture_Want0"; CS_NAME_EVENT_WANT [sizeof(CS_NAME_EVENT_WANT ) - 2] = CSCapNumChar;
		char CS_NAME_EVENT_SENT [] = "UnityCapture_Sent0"; CS_NAME_EVENT_SENT [sizeof(CS_NAME_EVENT_SENT ) - 2] = CSCapNumChar;
		char CS_NAME_SHARED_DATA[] = "UnityCapture_Data0"; CS_NAME_SHARED_DATA[sizeof(CS_NAME_SHARED_DATA) - 2] = CSCapNumChar;

		if (!m_hMutex)
		{
			if (ForReceiving) m_hMutex = CreateMutexA(NULL, FALSE,      CS_NAME_MUTEX);
			else              m_hMutex = OpenMutexA(SYNCHRONIZE, FALSE, CS_NAME_MUTEX);
			if (!m_hMutex) return false;
		}

		WaitForSingleObject(m_hMutex, INFINITE); //lock mutex
		struct UnlockAtReturn { ~UnlockAtReturn() { ReleaseMutex(m); }; HANDLE m; } cs = { m_hMutex };

		if (!m_hWantFrameEvent)
		{
			if (ForReceiving) m_hWantFrameEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, CS_NAME_EVENT_WANT);
			else              m_hWantFrameEvent = CreateEventA(NULL, FALSE, FALSE,      CS_NAME_EVENT_WANT);
			if (!m_hWantFrameEvent) return false;
		}

		if (!m_hSentFrameEvent)
		{
			if (ForReceiving) m_hSentFrameEvent = CreateEventA(NULL, FALSE, FALSE,      CS_NAME_EVENT_SENT);
			else              m_hSentFrameEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, CS_NAME_EVENT_SENT);
			if (!m_hSentFrameEvent) return false;
		}

		if (!m_hSharedFile)
		{
			if (ForReceiving) m_hSharedFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, NULL, sizeof(SharedMemHeader) + MAX_SHARED_IMAGE_SIZE, CS_NAME_SHARED_DATA);
			else              m_hSharedFile = OpenFileMappingA(FILE_MAP_WRITE, FALSE, CS_NAME_SHARED_DATA);
			if (!m_hSharedFile) return false;
		}

		m_pSharedBuf = (SharedMemHeader*)MapViewOfFile(m_hSharedFile, FILE_MAP_WRITE, 0, 0, 0);
		if (!m_pSharedBuf) return false;

		if (ForReceiving && m_pSharedBuf->maxSize != MAX_SHARED_IMAGE_SIZE)
			m_pSharedBuf->maxSize = MAX_SHARED_IMAGE_SIZE;

		return true;
	}

	struct SharedMemHeader
	{
		DWORD maxSize;
		int width;
		int height;
		int stride;
		int format;
		int resizemode;
		int mirrormode;
		int timeout;
		uint8_t data[1];
	};

	int32_t m_CapNum;
	HANDLE m_hMutex;
	HANDLE m_hWantFrameEvent;
	HANDLE m_hSentFrameEvent;
	HANDLE m_hSharedFile;
	SharedMemHeader* m_pSharedBuf;
};
