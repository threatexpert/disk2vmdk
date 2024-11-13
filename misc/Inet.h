#pragma once


class InetEventNotify
{
public:
	virtual int OnInetEventNotify(int nCode, const char* pDetails) = 0;
};

class InetAborter
{
public:
	virtual bool aborted() = 0;
};

class Inet
{
public:
	enum
	{
		E_INET_ERR = -1,
		E_INET_WOULDBLOCK = -10035,
	};

	enum
	{
		INFD_READ = 1,
		INFD_WRITE = 2,
		INFD_EXCEPT = 4,
	};

    enum {
        INET_SD_RD = 0,
        INET_SD_WR = 1,
        INET_SD_RDWR = 2,
    };

	virtual int Reference() = 0;
	virtual int Dereference() = 0;
    virtual int Select(int infd, int timeout_sec) = 0;
	virtual bool Connect(const char* host, int port, int timeout_sec = -1) = 0;
	virtual int Write(const void* pbuf, int len, int timeout_sec = -1) = 0;
    virtual int Read(void* pbuf, int len, int timeout_sec = -1) = 0;
    virtual int Shutdown(int flags) = 0;
    virtual void Close() = 0;
    virtual int CheckStuckOutput() = 0;
    virtual int PushStuckOutput() = 0;
	virtual void SetAborter(InetAborter *aborter) = 0;
	virtual bool SetServiceProviderInterface(Inet *pSPI) = 0;
	virtual void SetInetEventNotify(InetEventNotify *cb) = 0;
};

class CInetAbortorBase : public InetAborter {
public:
    BOOL* _pflag;
    virtual bool aborted() {
        if (*_pflag)
            return true;
        return false;
    }
};

class CInetSPITemplate  : public Inet
{
protected:

    long m_references;
    InetEventNotify *m_pCB;
    Inet *m_pSPI;
	InetAborter *m_aborter;

    CInetSPITemplate ()
        : m_references(1)
        , m_pCB(NULL)
        , m_pSPI(NULL)
		, m_aborter(NULL)
    {

    }
    virtual ~CInetSPITemplate () {
        Close();
        if (m_pSPI)
            m_pSPI->Dereference();
    }

public:

    //
    virtual int Reference() {
        return (int)InterlockedIncrement(&m_references);
    }
    virtual int Dereference() {
        long ret = InterlockedDecrement(&m_references);
        if (ret == 0)
            delete this;

        return (int)ret;
    }
    virtual int Select(int infd, int timeout_sec) {
        return m_pSPI->Select(infd, timeout_sec);
    }
    virtual bool Connect(const char* host, int port, int timeout_sec = -1) {
        return m_pSPI->Connect(host, port, timeout_sec);
    }
    virtual int Write(const void* pbuf, int len, int timeout_sec = -1) {
        return m_pSPI->Write(pbuf, len, timeout_sec);
    }
    virtual int Read(void* pbuf, int len, int timeout_sec = -1) {
        return m_pSPI->Read(pbuf, len, timeout_sec);
    }
    virtual int CheckStuckOutput() {
        if (m_pSPI)
            return m_pSPI->CheckStuckOutput();
        return 0;
    }
    virtual int PushStuckOutput() {
        if (m_pSPI)
            return m_pSPI->PushStuckOutput();
        return 0;
    }
    virtual int Shutdown(int flags) {
        return m_pSPI->Shutdown(flags);
    }
    virtual void Close() {
        if (m_pSPI)
            m_pSPI->Close();
    }
    virtual bool SetServiceProviderInterface(Inet *pSPI) {
        if (m_pSPI)
        {
            m_pSPI->Dereference();
        }
        m_pSPI = pSPI;
        if (m_pSPI)
            m_pSPI->Reference();
        return true;
    }
    virtual void SetInetEventNotify(InetEventNotify *cb) {
        m_pCB = cb;
    }
	virtual void SetAborter(InetAborter *aborter) {
		m_aborter = aborter;
		if (m_pSPI)
			m_pSPI->SetAborter(aborter);
	}

    bool InsertSPI(Inet *pSPI)
    {
        Inet *prev = m_pSPI;
        m_pSPI = pSPI;
        pSPI->Reference();

        if (!m_pSPI->SetServiceProviderInterface(prev))
        {
            m_pSPI = prev;
            pSPI->Dereference();
            return false;
        }
        if (prev)
            prev->Dereference();
        return true;
    }
protected:
    int doInetEventNotify(int nCode, const char* pDetails = NULL) {
        if (m_pCB)
        {
            return m_pCB->OnInetEventNotify(nCode, pDetails);
        }
        return 0;
    }

};
