#pragma once

#include "NBSocket.h"
#include "ecdh.h"
#include "chacha20.hpp"
#include "rc4.h"

bool gen_rand(void* buf, int len);

class CSecLayer
	: public CInetSPITemplate
{
	uint8_t puba[ECC_PUB_KEY_SIZE];
	uint8_t prva[ECC_PRV_KEY_SIZE];
	uint8_t seca[ECC_PUB_KEY_SIZE];

	char m_writebuf[1024 * 1024 * 4];
	int m_writedatalen;
	int m_writebufpos;
	uint64_t m_written, m_readbytes;
	rc4_state m_rc4Enc, m_rc4Dec;
	Chacha20 *m_ccEnc, *m_ccDec;
public:
	CSecLayer(void);
	virtual ~CSecLayer(void);

public:
	int hello(const char* passwd);

	virtual int Write(const void* pbuf, int len, int timeout_sec = -1);
	virtual int Read(void* pbuf, int len, int timeout_sec = -1);
	virtual int CheckStuckOutput();
	virtual int PushStuckOutput();

protected:
	bool WriteBlock(const void* pbuf, int len, int timeout_sec = -1);
	bool ReadBlock(void* pbuf, int len, int timeout_sec = -1);
};


