#pragma once


class CThread 
{
public:

    CThread();
    virtual ~CThread();

    HANDLE GetHandle() const;
    DWORD GetThreadId();

    BOOL Start();
    BOOL Wait(DWORD timeoutMillis = INFINITE) const;
    void CloseThreadHandle();
    void Terminate(DWORD exitCode = 0);

    static void Sleep(unsigned long ms);
protected:

    virtual DWORD run() = 0;
    static DWORD WINAPI run(void *p);

    HANDLE m_hThread;
    DWORD m_dwThreadId;
};



class CMyCriticalSection
{
public:
    CMyCriticalSection() {
        InitializeCriticalSection(&m_cs);
    }
    ~CMyCriticalSection() {
        DeleteCriticalSection(&m_cs);
    }

    void lock() {
        EnterCriticalSection(&m_cs);
    }

    void unlock() {
        LeaveCriticalSection(&m_cs);
    }

private:

    CRITICAL_SECTION m_cs;
};

class CMyEvent
{
    HANDLE _e;
public:
    CMyEvent()
    {
        _e = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    ~CMyEvent()
    {
        CloseHandle(_e);
    }

    bool Reset() {
        return ResetEvent(_e) != 0;
    }
    bool Set() {
        return SetEvent(_e) != 0;
    }
    bool Wait(DWORD dwMilliseconds) {
        return WaitForSingleObject(_e, dwMilliseconds == -1 ? -1 : dwMilliseconds) == WAIT_OBJECT_0;
    }

};


class CAutoCriticalSection
{
    CMyCriticalSection *m_p;
public:
    CAutoCriticalSection(CMyCriticalSection &cs)
    {
        m_p = &cs;
        m_p->lock();
    }
    ~CAutoCriticalSection()
    {
        if (m_p)
            m_p->unlock();
    }

};

