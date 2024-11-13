#pragma once


BOOL socket_init_ws32();

SOCKET socket_createListener(DWORD dwIP, WORD wPort, int backlog, bool reuseaddr, bool overlapped);
BOOL socket_setbufsize(SOCKET s, int bufsize);
int socket_nonblocking(SOCKET sd, int enable);
int socket_close(SOCKET s);
int socket_err();
void socket_set_err(int e);
bool socket_would_block();

int util_inet_pton(int af, const char *src, void *dst);
const char * util_inet_ntop(int af, const void *src, char *dst, size_t size);
