#include "pch.h"
#include "SecureLayer.h"
#include "netutils.h"
#include <windows.h>
#include <Wincrypt.h>
#include <assert.h>

static HCRYPTPROV hcrypt = NULL;

//#define CRYPTOR_RC4

static void sys_win_rand_init(void)
{
    if (!CryptAcquireContext(&hcrypt, NULL, NULL, PROV_RSA_FULL, 0)) {
        if (!CryptAcquireContext(&hcrypt, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {

        }
    }
}

bool gen_rand(void *buf, int len)
{
    if (!hcrypt) {
        sys_win_rand_init();
    }
    if (hcrypt && CryptGenRandom(hcrypt, len, (BYTE*)buf)) {
        return true;
    }

    return false;
}


CSecLayer::CSecLayer(void)
{
    assert(ECC_PUB_KEY_SIZE >= 32);
    memset(puba, 0, sizeof(puba));
    memset(prva, 0, sizeof(prva));
    memset(seca, 0, sizeof(seca));
    memset(m_writebuf, 0, sizeof(m_writebuf));
    m_writedatalen = 0;
    m_written = m_readbytes = m_writebufpos = 0;
    m_ccEnc = NULL;
    m_ccDec = NULL;
}

CSecLayer::~CSecLayer(void)
{
    delete m_ccEnc;
    delete m_ccDec;
}

int CSecLayer::hello(const char* passwd)
{
    uint8_t pubb[ECC_PUB_KEY_SIZE];
    if (!gen_rand(prva, sizeof(prva))) {
        return -1;
    }
    if (!ecdh_generate_keys(puba, prva)) {
        return -1;
    }
    if (!InetWriteAll(m_pSPI, puba, sizeof(puba), 30)) {
        return -1;
    }
    if (!InetReadAll(m_pSPI, pubb, sizeof(pubb), 30)) {
        return -1;
    }
    if (!ecdh_shared_secret(prva, pubb, seca)) {
        return -1;
    }
    int plen = (int)min(8, strlen(passwd));
    if (plen)
        memcpy(seca + 8, passwd, plen);

#ifdef CRYPTOR_RC4
    rc4_init(&m_rc4Enc, seca, sizeof(seca));
    rc4_init(&m_rc4Dec, seca, sizeof(seca));
#else
    m_ccEnc = new Chacha20(seca, &seca[sizeof(seca) - 8]);
    m_ccDec = new Chacha20(seca, &seca[sizeof(seca) - 8]);
#endif

    if (!gen_rand(pubb, 16)) {
        return -1;
    }
    if (!WriteBlock(pubb, 16, 30)) {
        return -1;
    }
    if (!ReadBlock(pubb, 16, 30)) {
        return -1;
    }
    return 0;
}

int CSecLayer::Write(const void* pbuf, int len, int timeout_sec)
{
    if (m_writedatalen == m_writebufpos) {
        m_writebufpos = m_writedatalen = 0;
        m_writedatalen = min(len, sizeof(m_writebuf));
#ifdef CRYPTOR_RC4
        rc4_crypt(&m_rc4Enc, (BYTE*)pbuf, (BYTE*)m_writebuf, m_writedatalen);
#else
        m_ccEnc->crypt((BYTE*)pbuf, (BYTE*)m_writebuf, m_writedatalen);
#endif
        m_written += m_writedatalen;
    }

    int ret = m_pSPI->Write(m_writebuf + m_writebufpos, m_writedatalen - m_writebufpos, timeout_sec);
    if (ret > 0) {
        m_writebufpos += ret;
        if (m_writebufpos == m_writedatalen) {
            if (m_writedatalen > len) {
                assert(false);
                return -1;
            }
            return m_writedatalen;
        }
        else
            return Inet::E_INET_WOULDBLOCK;
    }
    else {
        return ret;
    }
}

int CSecLayer::PushStuckOutput()
{
    if (m_writedatalen - m_writebufpos == 0)
        return 0;

    int ret = m_pSPI->Write(m_writebuf + m_writebufpos, m_writedatalen - m_writebufpos, 0);
    if (ret > 0) {
        m_writebufpos += ret;
    }
    return ret;
}

int CSecLayer::CheckStuckOutput()
{
    return m_writedatalen - m_writebufpos;
}

bool CSecLayer::WriteBlock(const void* pbuf, int len, int timeout_sec)
{
    int nSent = 0;
    int nBytesLeft = len;
    int ret;

    while (nBytesLeft > 0)
    {
        ret = Write((const char*)pbuf + nSent, nBytesLeft, timeout_sec);
        if (ret <= 0)
        {
            return false;
        }

        nSent += ret;
        nBytesLeft -= ret;
    }
    return true;
}

bool CSecLayer::ReadBlock(void* pbuf, int len, int timeout_sec)
{
    int nRet;
    int nBytesRecvd, nBytesTotal;

    nBytesTotal = len;
    nBytesRecvd = 0;

    while (nBytesRecvd < nBytesTotal)
    {
        nRet = Read((char*)pbuf + nBytesRecvd, nBytesTotal - nBytesRecvd, timeout_sec);
        if (nRet <= 0)
        {
            return false;
        }

        nBytesRecvd += nRet;
    }

    return true;
}

int CSecLayer::Read(void* pbuf, int len, int timeout_sec)
{
    int ret = m_pSPI->Read(pbuf, len, timeout_sec);
    if (ret > 0) {
#ifdef CRYPTOR_RC4
        rc4_crypt(&m_rc4Dec, (BYTE*)pbuf, (BYTE*)pbuf, ret);
#else
        m_ccDec->crypt((BYTE*)pbuf, (BYTE*)pbuf, ret);
#endif
        m_readbytes += ret;
    }
    return ret;
}
