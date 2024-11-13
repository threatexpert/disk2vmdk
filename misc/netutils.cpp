#include "pch.h"
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2ipdef.h>
#include <Ws2tcpip.h>
#include "netutils.h"

#pragma comment(lib, "ws2_32")

BOOL socket_init_ws32()
{
	WSADATA wsadata;
	return WSAStartup(MAKEWORD(2,2),&wsadata) == 0;
}


int socket_close(SOCKET s)
{
	return ::closesocket(s);
}

SOCKET create_tcp_socket(bool nonblocking, bool overlapped)
{
	SOCKET sd;

	if (overlapped) {
		if ((sd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
			return INVALID_SOCKET;
	}
	else {
		if ((sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
			return INVALID_SOCKET;
	}
#if defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
	// We do not want SIGPIPE if writing to socket.
	const int value = 1;
	if (0 != setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(int)))
	{
		closeSocket(sd);
		return INVALID_SOCKET;
	}
#endif

	if (nonblocking)
	{
		if (0 != socket_nonblocking(sd, true))
		{
			socket_close(sd);
			return INVALID_SOCKET;
		}
	}

	return sd;
}

int socket_err()
{
	return WSAGetLastError();
}

void socket_set_err(int e)
{
	WSASetLastError(e);
}

bool socket_would_block()
{
	return (WSAGetLastError() == WSAEWOULDBLOCK);
}


BOOL socket_setbufsize(SOCKET s, int bufsize)
{
	int val = bufsize;
	int r1 = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&val, sizeof(int));
	int r2 = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&val, sizeof(int));

	return r1 == 0 && r2 == 0;
}

SOCKET socket_createListener(DWORD dwIP, WORD wPort, int backlog, bool reuseaddr, bool overlapped)
{
	SOCKET sockid;
	int errn;
	if ((sockid = create_tcp_socket(true, overlapped)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	int flag = 1, len = sizeof(int);
	if (reuseaddr)
	{
		setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, len);
	}

	struct sockaddr_in srv_addr = { 0 };
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = dwIP;
	srv_addr.sin_port = htons(wPort);

	if (::bind(sockid, (struct sockaddr*)&srv_addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		goto error;
	}

	if (::listen(sockid, backlog) == SOCKET_ERROR)
	{
		goto error;
	}

	return sockid;
error:
	errn = socket_err();
	socket_close(sockid);
	socket_set_err(errn);
	return INVALID_SOCKET;
}

int socket_nonblocking(SOCKET sd, int enable)
{
	int iMode = 1;//nonzero
	return ioctlsocket(sd, FIONBIO, (u_long FAR*) &iMode);//Enabled Nonblocking Mode
}

int util_inet_pton(int af, const char *src, void *dst)
{
	if (af == AF_INET)
	{
		if (inet_addr(src) != INADDR_NONE)
		{
			((IN_ADDR*)dst)->s_addr = inet_addr(src);
			return 1;
		}
		return 0;
	}
	else if (af == AF_INET6)
	{
		typedef INT WSAAPI D_inet_pton(INT Family, PCSTR pszAddrString, PVOID pAddrBuf);
		static D_inet_pton *p_inet_pton = NULL;
		static BOOL p_inet_pton_init = FALSE;
		if (!p_inet_pton_init)
		{
			HMODULE hws = GetModuleHandleA("ws2_32");
			if (hws)
				*(void**)&p_inet_pton = GetProcAddress(hws, "inet_pton");

			p_inet_pton_init = TRUE;
		}

		if (p_inet_pton)
		{
			return p_inet_pton(af, src, dst);
		}

		return -1;
	}
	else
		return -1;
}

const char * util_inet_ntop(int af, const void *src, char *dst, size_t size)
{
	if (af == AF_INET)
	{
		char *p = inet_ntoa(*(in_addr*)src);
		if (p)
		{
			strncpy_s(dst, size, p, size);
			p = dst;
		}
		return p;
	}
	else if (af == AF_INET6)
	{
		typedef PCSTR WSAAPI D_inet_ntop( INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize);
		static D_inet_ntop *p_inet_ntop = NULL;
		static BOOL p_inet_ntop_init = FALSE;
		if (!p_inet_ntop_init)
		{
			HMODULE hws = GetModuleHandleA("ws2_32");
			if (hws)
				*(void**)&p_inet_ntop = GetProcAddress(hws, "inet_ntop");

			p_inet_ntop_init = TRUE;
		}

		if (p_inet_ntop)
		{
			return p_inet_ntop(af, (PVOID)src, dst, size);
		}

		return NULL;
	}
	else
		return NULL;
}
