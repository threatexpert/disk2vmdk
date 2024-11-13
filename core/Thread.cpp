#include "pch.h"
#include "Thread.h"


CThread::CThread()
   :  m_hThread(NULL)
{

}
      
CThread::~CThread()
{
    Wait();
    CloseThreadHandle();
}

HANDLE CThread::GetHandle() const
{
   return m_hThread;
}

DWORD CThread::GetThreadId()
{
	return m_dwThreadId;
}

void CThread::CloseThreadHandle()
{
	if (m_hThread != NULL)
	{
		::CloseHandle(m_hThread);
		m_hThread = NULL;
	}
}


BOOL CThread::Start()
{
	assert(m_hThread == NULL);

	m_hThread = CreateThread(0, 0, run, (void*)this, 0, &m_dwThreadId);

	if (m_hThread == NULL)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CThread::Wait(DWORD timeoutMillis /* = INFINITE */) const
{
   if (!m_hThread)
       return FALSE;

   DWORD result = ::WaitForSingleObject(m_hThread, timeoutMillis);
   if (result == WAIT_OBJECT_0)
	   return TRUE;
   else
	   return FALSE;
}

DWORD WINAPI CThread::run(void *p)
{
   DWORD result = 0;

   CThread* pThis = (CThread*)p;
   
   if (pThis)
   {
        result = pThis->run();
   }

   return result;
}

void CThread::Terminate(DWORD exitCode /* = 0 */)
{
   if (!::TerminateThread(m_hThread, exitCode))
   {
	 //  assert(0);
   }
}

void CThread::Sleep(unsigned long ms)
{
    ::Sleep(ms);
}
