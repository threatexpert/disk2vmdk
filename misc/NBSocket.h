#pragma once

#include "Inet.h"



class Cnbsocket
	: public Inet
{
protected:
    Cnbsocket(void);
	virtual ~Cnbsocket(void);

public:

    static Cnbsocket* createInstance() {
        return new Cnbsocket();
    }

	void Attach(SOCKET s);
	SOCKET Detach();
    SOCKET GetSocketHandle();
	bool InitSocket();

    virtual int Reference() {
        return (int)InterlockedIncrement(&m_references);
    }
    virtual int Dereference() {
        long ret = InterlockedDecrement(&m_references);
        if (ret == 0)
            delete this;

        return (int)ret;
    }
    virtual int Select(int infd, int timeout_sec);
    virtual bool Connect(const char* host, int port, int timeout_sec = -1);
    virtual int Write(const void* pbuf, int len, int timeout_sec = -1);
    virtual int Read(void* pbuf, int len, int timeout_sec = -1);
    virtual int Shutdown(int flags);
    virtual void Close();
    virtual int CheckStuckOutput();
    virtual int PushStuckOutput();
    virtual bool SetServiceProviderInterface(Inet *pSPI);
    virtual void SetInetEventNotify(InetEventNotify *cb);
	virtual void SetAborter(InetAborter *aborter);

protected:
    bool _connect(const struct sockaddr* addr, int addrlen, int timeout_sec = -1);
    static bool sys_connect(SOCKET s, const struct sockaddr *addr, int addrlen, int timeout_sec = -1);
    static int sys_select(SOCKET s, int fds, int timeout_msec);
    static bool resolve(const char *name, int port, struct sockaddr_storage *addr, int *addrlen);
    static bool resolve_name(const char *name, int port, struct sockaddr_storage *addr, int *addrlen);

protected:
    long m_references;
    SOCKET m_hSocket;
	InetAborter *m_aborter;
};

bool InetWriteAll(Inet* pSPI, const void* lpData, int nLen, int timeout_sec = -1, int blocksize=1024*64 );
bool InetReadAll(Inet* pSPI, void* lpBuf, int nSize, int timeout_sec = -1);

class CInetTcp
    : public CInetSPITemplate
{
    Cnbsocket *m_sockBase;
public:
    CInetTcp(Inet *pSPI = NULL);
    CInetTcp(SOCKET s);
    virtual ~CInetTcp();

    bool InsertSPI(Inet* pSPI);
    SOCKET GetSocketHandle();
protected:


};

