#pragma once

#include <string>
#include <vector>
#ifndef vssclient_NO_THREAD
#include "Thread.h"
#endif

class VSSProvider;

class vssclient_Callback {
public:
	enum {
		Starting,
		AddingVolume,
		MakingSnapshot,
		MadeSnapshot,
	};
	virtual void vssclient_OnProgress(int evt, HRESULT result, LPCWSTR lpszDesc) = 0;
};

BOOL InitVSSAPI();

class Cvssclient
#ifndef vssclient_NO_THREAD
	: public CThread
#endif
{
public:
	Cvssclient();
	virtual ~Cvssclient();

public:
	BOOL Init();
	BOOL IsVolumeSupported(LPCWSTR lpszVol);
#ifndef vssclient_NO_THREAD
	BOOL AsyncStart(vssclient_Callback* pCB);
#endif
	BOOL StartSnapshot();
	BOOL AddVolume(LPCWSTR lpszVol, BOOL bAsync);
	int  GetVolumeCount();
	BOOL MakeSnapshot();
	BOOL GetVolumeSnapshotDevice(LPCWSTR lpszVol, std::wstring &strDevicePath);
	void Cancel();
	void DeleteSnapshots();
	HRESULT LastErr();

protected:
#ifndef vssclient_NO_THREAD
	virtual DWORD run();
#endif
	vssclient_Callback* m_cb;
	std::vector<std::wstring> m_vols;
private:
	bool	m_inited_com;
	bool	m_init;
	VSSProvider* m_pProvider;
};