#include "pch.h"
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2ipdef.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <assert.h>
#include "NBSocket.h"
#include "netutils.h"


Cnbsocket::Cnbsocket(void)
    : m_hSocket(INVALID_SOCKET)
    , m_references(1)
	, m_aborter(NULL)
{
}

Cnbsocket::~Cnbsocket(void)
{
    Close();
}

SOCKET Cnbsocket::GetSocketHandle()
{
	return m_hSocket;
}

bool Cnbsocket::InitSocket()
{
	SOCKET sd;

	if (m_hSocket != INVALID_SOCKET)
		return false;

	if ((sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return false;

#if defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
	// We do not want SIGPIPE if writing to socket.
	const int value = 1;
	if (0 != setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(int)))
	{
		closeSocket(sd);
		return false;
	}
#endif

	if (0 != socket_nonblocking(sd, true))
	{
		socket_close(sd);
		return false;
	}

	m_hSocket = sd;
	return true;
}

void Cnbsocket::Attach(SOCKET s)
{
	m_hSocket = s;
}

SOCKET Cnbsocket::Detach()
{
	SOCKET s = m_hSocket;

	m_hSocket = INVALID_SOCKET;
	return s;
}

bool Cnbsocket::Connect(const char* host, int port, int timeout_sec /*= -1*/)
{
	bool bSockInit = false;
    struct sockaddr_storage ss;
    int sslen = sizeof(ss);

    if (!resolve(host, port, &ss, &sslen))
    {
        return false;
    }

	if (m_hSocket == INVALID_SOCKET)
	{
		if (!InitSocket())
		{
			return false;
		}
		bSockInit = true;
	}

    if (!_connect((struct sockaddr*)&ss, sslen, timeout_sec))
    {
        if (bSockInit) {
            int ec = socket_err();
            Cnbsocket::Close();
            socket_set_err(ec);
        }
        return false;
    }

    return true;
}

int Cnbsocket::Write(const void* pbuf, int len, int timeout_sec /*= -1*/)
{
    int ret;
    if (timeout_sec == 0)
    {
        ret = (int)send(m_hSocket, (const char *)pbuf, len, 0);
    }
    else
    {
        ret = (int)send(m_hSocket, (char*)pbuf, len, 0);
        if (ret < 0)
        {
            if (!socket_would_block())
                return Inet::E_INET_ERR;
        }
        else {
            return ret;
        }
        ret = Select(INFD_WRITE, timeout_sec);

        if (ret < 0)
            return Inet::E_INET_ERR;
        else if (ret == 0)
            return Inet::E_INET_WOULDBLOCK;

        ret = (int)send(m_hSocket, (char*)pbuf, len, 0);
    }

    if (ret < 0)
    {
        if (socket_would_block())
            return Inet::E_INET_WOULDBLOCK;
        else
            return Inet::E_INET_ERR;
    }
    else
        return ret;
}

int Cnbsocket::Read(void* pbuf, int len, int timeout_sec /*= -1*/)
{
    int ret;

    if (timeout_sec == 0)
    {
        ret = (int)recv(m_hSocket, (char*)pbuf, len, 0);
    }
    else
    {
        ret = (int)recv(m_hSocket, (char*)pbuf, len, 0);
        if (ret < 0)
        {
            if (!socket_would_block())
                return Inet::E_INET_ERR;
        }
        else {
            return ret;
        }
        ret = Select(INFD_READ, timeout_sec);

        if (ret < 0)
            return Inet::E_INET_ERR;
        else if (ret == 0)
            return Inet::E_INET_WOULDBLOCK;

        ret = (int)recv(m_hSocket, (char*)pbuf, len, 0);
    }

    if (ret < 0)
    {
        if (socket_would_block())
            return Inet::E_INET_WOULDBLOCK;
        else
            return Inet::E_INET_ERR;
    }
    else
        return ret;
}

int Cnbsocket::Select(int infd, int timeout_sec)
{
	if (m_aborter && timeout_sec != 0) {
		int ret;
		unsigned int usedms = 0;
        DWORD timeout_ms = (unsigned int)timeout_sec * 1000;
        DWORD dwTick = GetTickCount();
        DWORD dwTickElapsed = 0;
        
		while (timeout_sec == -1 ||
            ( usedms < timeout_ms && (dwTickElapsed < timeout_ms) )
            )
        {
			if (m_aborter->aborted())
				return -1;

			ret = sys_select(m_hSocket, infd, 100);
			if (ret != 0)
				return ret;

			usedms += 100;
            dwTickElapsed = GetTickCount() - dwTick;
		}
		return 0;
	}
	else {
		return sys_select(m_hSocket, infd, timeout_sec== -1 ? -1 : timeout_sec*1000);
	}
}

int Cnbsocket::Shutdown(int flags)
{
	return shutdown(m_hSocket, flags);
}

void Cnbsocket::Close()
{
    if (m_hSocket != INVALID_SOCKET)
    {
        socket_close(m_hSocket);
        m_hSocket = INVALID_SOCKET;
    }
}

bool Cnbsocket::SetServiceProviderInterface(Inet *pSPI)
{
    if (pSPI == NULL)
        return true;

    return false;
}

void Cnbsocket::SetInetEventNotify(InetEventNotify*cb)
{

}

void Cnbsocket::SetAborter(InetAborter* aborter)
{
	m_aborter = aborter;
}

int Cnbsocket::CheckStuckOutput()
{
    return 0;
}

int Cnbsocket::PushStuckOutput()
{
    return 0;
}

bool Cnbsocket::_connect(const sockaddr* addr, int addrlen, int timeout_sec)
{
    if (connect(m_hSocket, (const struct sockaddr*)addr, addrlen) == SOCKET_ERROR)
    {
        if (!socket_would_block())
            goto error;
    }

    if (Select(Inet::INFD_WRITE|Inet::INFD_EXCEPT, timeout_sec) != Inet::INFD_WRITE)
    {
        goto error;
    }
    else
    {
#ifndef _WIN32
        int err = 0;
        socklen_t res_len = sizeof(err);

        if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &res_len) < 0
            || err)
        {
            if (err)
                errno = err;

            goto error;
        }
#endif
    }

    return true;
error:
    return false;
}

bool Cnbsocket::sys_connect(SOCKET s, const struct sockaddr *addr, int addrlen, int timeout_sec /*= -1*/)
{
    if (connect(s, (const struct sockaddr*)addr, addrlen) == SOCKET_ERROR)
    {
        if (!socket_would_block())
            goto error;
    }

    if (sys_select(s, Inet::INFD_WRITE | Inet::INFD_EXCEPT, timeout_sec*1000) != Inet::INFD_WRITE)
    {
        goto error;
    }
    else
    {
#ifndef _WIN32
        int err = 0;
        socklen_t res_len = sizeof(err);

        if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &res_len) < 0
            || err)
        {
            if (err)
                errno = err;

            goto error;
        }
#endif
    }

    return true;
error:
    return false;
}

int Cnbsocket::sys_select(SOCKET s, int fds, int timeout_msec)
{
    int ret;
    fd_set FdRead, FdWrite, FdExcept;
    struct timeval TimeOut;
    bool read = (fds & INFD_READ) != 0;
    bool write = (fds & INFD_WRITE) != 0;
    bool except = (fds & INFD_EXCEPT) != 0;

    TimeOut.tv_sec = timeout_msec/1000;
    TimeOut.tv_usec = (timeout_msec%1000)*1000;

    if (read)
    {
        FD_ZERO(&FdRead);
        FD_SET(s, &FdRead);
    }
    if (write)
    {
        FD_ZERO(&FdWrite);
        FD_SET(s, &FdWrite);
    }
    if (except)
    {
        FD_ZERO(&FdExcept);
        FD_SET(s, &FdExcept);
    }

    ret = select((int)s + 1,
        read ? &FdRead : NULL,
        write ? &FdWrite : NULL,
        except ? &FdExcept : NULL,
		timeout_msec == -1 ? NULL : &TimeOut);

    if (ret > 0)
    {
        ret = 0;

        if (read && FD_ISSET(s, &FdRead))
            ret |= Inet::INFD_READ;

        if (write && FD_ISSET(s, &FdWrite))
            ret |= Inet::INFD_WRITE;

        if (except && FD_ISSET(s, &FdExcept))
        {
#ifdef _WIN32
            int nErr;
            int optlen = sizeof(int);
            if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&nErr, &optlen) == 0)
            {
                SetLastError(nErr);
            }
#else

            int err = 0;
            socklen_t res_len = sizeof(err);

            if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &res_len) < 0
                || err)
            {
                if (err)
                    errno = err;
            }

#endif

            ret |= Inet::INFD_EXCEPT;
        }
    }

    return ret;
}

bool Cnbsocket::resolve(const char *name, int port, struct sockaddr_storage *addr, int *addrlen)
{
    sockaddr_in *addr_v4;
    sockaddr_in6 *addr_v6;
    IN_ADDR in4;
    IN6_ADDR in6;

    memset(addr, 0, *addrlen);

    if (util_inet_pton(AF_INET, name, &in4) == 1)
    {
        if (*addrlen < sizeof(sockaddr_in))
            return false;

        addr_v4 = (struct sockaddr_in*)addr;
        addr_v4->sin_family = AF_INET;
        addr_v4->sin_addr = in4;
        addr_v4->sin_port = htons((u_short)port);
        *addrlen = sizeof(sockaddr_in);
        return true;
    }
    else if (util_inet_pton(AF_INET6, name, &in6) == 1)
    {
        if (*addrlen < sizeof(sockaddr_in6))
            return false;

        addr_v6 = (struct sockaddr_in6*)addr;
        addr_v6->sin6_family = AF_INET;
        addr_v6->sin6_addr = in6;
        addr_v6->sin6_port = htons((u_short)port);
        *addrlen = sizeof(sockaddr_in6);
        return true;
    }
    else
    {
        return resolve_name(name, port, addr, addrlen);
    }

    return false;
}


bool Cnbsocket::resolve_name(const char *name, int port, struct sockaddr_storage *addr, int *addrlen)
{
    char serv[64];
    struct addrinfo *result = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;
    DWORD dwRetval;
    bool bok = false;

    sprintf_s(serv, 64, "%d", port);
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    dwRetval = getaddrinfo(name, serv, &hints, &result);
    if (dwRetval != 0) {
        return false;
    }

    for (ptr = result; !bok && ptr != NULL; ptr = ptr->ai_next) {
        switch (ptr->ai_family) {
        case AF_UNSPEC:
            break;
        case AF_INET:
            *addrlen = (int)ptr->ai_addrlen;
            memcpy(addr, ptr->ai_addr, *addrlen);
            bok = true;
            break;
        case AF_INET6:
            *addrlen = (int)ptr->ai_addrlen;
            memcpy(addr, ptr->ai_addr, *addrlen);
            bok = true;
            break;

        default:
            break;
        }
    }

    freeaddrinfo(result);
    return bok;
}


bool InetWriteAll(Inet* pSPI, const void* lpData, int nLen, int timeout_sec /* = -1*/, int blocksize /*= 1024*64*/)
{
    int nSent = 0;
    int nBytesLeft = nLen;
    int sendLen;
    int ret;

	assert(timeout_sec != 0);

    while (nBytesLeft > 0)
    {
        if (blocksize <= 0) {
            sendLen = nBytesLeft;
        }
        else {
            sendLen = min(nBytesLeft, blocksize);
        }

        ret = pSPI->Write((char*)lpData + nSent, sendLen, timeout_sec);
        if (ret <= 0)
        {
            return false;
        }

        nSent += ret;
        nBytesLeft -= ret;
    }

	while ( (ret = pSPI->CheckStuckOutput()) > 0)
	{
		ret = pSPI->Select(Inet::INFD_WRITE, timeout_sec);
		if (ret <= 0)
			return false;
		ret = pSPI->PushStuckOutput();
		if (ret < 0)
		{
			if (ret != Inet::E_INET_WOULDBLOCK)
				return false;
		}
	}

    return true;
}

bool InetReadAll(Inet* pSPI, void* lpBuf, int nSize, int timeout_sec /*= -1*/)
{
    int nRet;
    int nBytesRecvd, nBytesTotal;

    nBytesTotal = nSize;
    nBytesRecvd = 0;

    while (nBytesRecvd < nBytesTotal)
    {
        nRet = pSPI->Read((char*)lpBuf + nBytesRecvd, nBytesTotal - nBytesRecvd, timeout_sec);
        if (nRet <= 0)
        {
            return false;
        }

        nBytesRecvd += nRet;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////



CInetTcp::CInetTcp(Inet *pSPI /*= NULL*/)
{
    if (pSPI == NULL) {
        m_sockBase = Cnbsocket::createInstance();
        pSPI = m_sockBase;
    }
    SetServiceProviderInterface(pSPI);
    if (m_sockBase)
        m_sockBase->Dereference();
}

CInetTcp::CInetTcp(SOCKET s)
{
    m_sockBase = Cnbsocket::createInstance();
	if (s != INVALID_SOCKET)
		m_sockBase->Attach(s);
    SetServiceProviderInterface(m_sockBase);
    m_sockBase->Dereference();
}

CInetTcp::~CInetTcp()
{

}

bool CInetTcp::InsertSPI(Inet *pSPI)
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

SOCKET CInetTcp::GetSocketHandle()
{
    if (m_sockBase)
        return m_sockBase->GetSocketHandle();
    return INVALID_SOCKET;
}
