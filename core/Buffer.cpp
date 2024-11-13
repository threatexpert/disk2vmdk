#include "pch.h"
#include "Buffer.h"


CBuffers::CBuffers()
{
    m_bAbort = FALSE;
}

CBuffers::~CBuffers()
{
    clear();
}

bool CBuffers::init_pool(DWORD size, DWORD count)
{
    for (DWORD i = 0; i < count; i++) {

        Buffer* b = new Buffer();
        if (!b->init(size))
            return false;
        m_items.push_back(b);
    }
    m_bAbort = FALSE;
    return true;
}

void CBuffers::clear()
{
    for (std::list<Buffer*>::iterator it = m_items.begin(); it != m_items.end(); it++)
    {
        Buffer* b = *it;
        delete b;
    }
    m_items.clear();
}

CBuffers::Buffer* CBuffers::Pop(DWORD wait_ms)
{
    Buffer* ret = NULL;
    while (!m_bAbort) {
        m_lc.lock();
        if (m_items.size() > 0) {
            ret = *m_items.begin();
            m_items.pop_front();
        }
        m_lc.unlock();
        if (ret)
            return ret;
        if (!m_e.Wait(wait_ms))
            return NULL;
    }
    return NULL;
}

void CBuffers::Push(CBuffers::Buffer* p)
{
    m_lc.lock();
    m_items.push_back(p);
    m_lc.unlock();
    m_e.Set();
}

void CBuffers::Abort(BOOL force)
{
    if (force) {
        m_bAbort = TRUE;
        m_e.Set();
    }
    else {
        //等链表为空时，再标志中止
        while (1) {
            size_t sz;
            m_lc.lock();
            sz = m_items.size();
            if (!sz) {
                m_bAbort = TRUE;
                m_e.Set();
            }
            m_lc.unlock();
            if (m_bAbort)
                break;
            else
                Sleep(1);
        }
    }
}

void CBuffers::ResetAbort()
{
    m_bAbort = FALSE;
}
