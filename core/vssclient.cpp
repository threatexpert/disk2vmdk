#include "pch.h"

#define _WIN32_DCOM

#include <comdef.h>
#include <Wbemidl.h>
#include <atlbase.h>
#include <string>
#include <assert.h>
#include "vssProv.h"
#include "vssclient.h"
#include <vector>
#include <sstream>
#include "osutils.h"

__CreateVssBackupComponentsInternal* pCreateVssBackupComponents;
__VssFreeSnapshotPropertiesInternal* pVssFreeSnapshotProperties;

BOOL InitVSSAPI()
{
	if (pCreateVssBackupComponents && pVssFreeSnapshotProperties)
		return TRUE;
	HMODULE hVSSAPI = LoadLibraryA("vssapi.dll");
	if (!hVSSAPI)
	{
		//fprintf(stderr, "err: load vssapi\n");
		return FALSE;
	}

	if (!IsWindowsVistaOrGreater())
	{
		*(void**)&pCreateVssBackupComponents = GetProcAddress(hVSSAPI, "CreateVssBackupComponents");
		if (!pCreateVssBackupComponents) {
#ifdef _WIN64
			* (void**)&pCreateVssBackupComponents = GetProcAddress(hVSSAPI, "?CreateVssBackupComponents@@YAJPEAPEAVIVssBackupComponents@@@Z");
#else
			* (void**)&pCreateVssBackupComponents = GetProcAddress(hVSSAPI, "?CreateVssBackupComponents@@YGJPAPAVIVssBackupComponents@@@Z");
#endif
		}
		*(void**)&pVssFreeSnapshotProperties = GetProcAddress(hVSSAPI, "VssFreeSnapshotProperties");
	}
	else
	{
		*(void**)&pCreateVssBackupComponents = GetProcAddress(hVSSAPI, "CreateVssBackupComponentsInternal");
		*(void**)&pVssFreeSnapshotProperties = GetProcAddress(hVSSAPI, "VssFreeSnapshotPropertiesInternal");
	}
	//if (!pCreateVssBackupComponents) {
	//	fprintf(stderr, "err: load CreateVssBackupComponents\n");
	//}
	//if (!pVssFreeSnapshotProperties) {
	//	fprintf(stderr, "err: load VssFreeSnapshotPropertiesInternal\n");
	//}
	return pCreateVssBackupComponents && pVssFreeSnapshotProperties;
}


Cvssclient::Cvssclient()
	: m_inited_com(false)
	, m_init(false)
	, m_pProvider(NULL)
	, m_cb(NULL)
{
	m_inited_com = SUCCEEDED(CoInitializeEx(0, COINIT_MULTITHREADED));
	::CoInitializeSecurity(
		NULL,
		-1,
		NULL,
		NULL,
		RPC_C_AUTHN_LEVEL_NONE,
		RPC_C_IMP_LEVEL_IDENTIFY,
		NULL,
		EOAC_NONE,
		NULL);

	m_pProvider = NULL;
}

Cvssclient::~Cvssclient(void)
{
	if (m_pProvider)
	{
		Cancel();
		m_pProvider->Lock();
		m_pProvider->Unlock();
	#ifndef vssclient_NO_THREAD
		CThread::Wait();
	#endif
		delete m_pProvider;
	}

	if (m_inited_com) {
		CoUninitialize();
	}
}

BOOL Cvssclient::Init()
{
	BOOL bRes;
	if (IsWow64()) {
		return FALSE;
	}

	if (!InitVSSAPI()) {
		return FALSE;
	}
#ifdef _DEBUG
	Sleep(2000);
#endif
	if (IsWindows2003OrGreater()) {
		m_pProvider = CreateVSSProv2003();
	}
	else {
		m_pProvider = CreateVSSProvXP();
	}

	m_pProvider->m_pCreateVssBackupComponents = pCreateVssBackupComponents;
	m_pProvider->m_pFreeSnapshotProperties = pVssFreeSnapshotProperties;

	m_pProvider->Lock();
	bRes = m_pProvider->Init();
	m_pProvider->Unlock();
	return bRes;
}

BOOL Cvssclient::IsVolumeSupported(LPCWSTR lpszVol)
{
	CComBSTR bstrVol(lpszVol);
	if (bstrVol.Length() && ((BSTR)bstrVol)[bstrVol.Length() - 1] != '\\') {
		bstrVol += L"\\";
	}
	return m_pProvider->IsVolumeSupported(bstrVol);
}

#ifndef vssclient_NO_THREAD

BOOL Cvssclient::AsyncStart(vssclient_Callback* pCB)
{
	m_cb = pCB;
	return CThread::Start();
}

DWORD Cvssclient::run()
{
	BOOL bRes;
	std::wostringstream msgs;

	m_cb->vssclient_OnProgress(vssclient_Callback::Starting, 0, LSTRW(RID_VSS_init));
	if (!Init()) {
		m_cb->vssclient_OnProgress(vssclient_Callback::Starting, LastErr() == 0 ? -1 : LastErr(), LSTRW(RID_VSS_init_err));
		goto _END;
	}
	m_cb->vssclient_OnProgress(vssclient_Callback::Starting, 0, LSTRW(RID_VSS_BeforeStartSnapshot));
	bRes = StartSnapshot();
	if (!bRes) {
		m_cb->vssclient_OnProgress(vssclient_Callback::Starting, LastErr(), LSTRW(RID_VSS_StartSnapshotErr));
		goto _END;
	}

	for (auto& vol : m_vols) {
		m_cb->vssclient_OnProgress(vssclient_Callback::AddingVolume, 0, msgs.str().c_str());
		bRes = AddVolume(vol.c_str(), FALSE);
		if (!bRes) {
			msgs.str(L"");
			msgs << LSTRW(RID_VSS_AddVol) << vol << LSTRW(RID_VSS_AddVolErr);
			m_cb->vssclient_OnProgress(vssclient_Callback::AddingVolume, LastErr(), msgs.str().c_str());
			goto _END;
		}
	}

	msgs.str(L"");
	msgs << LSTRW(RID_VSS_BeforeMakeSnapshot);
	m_cb->vssclient_OnProgress(vssclient_Callback::MakingSnapshot, LastErr(), msgs.str().c_str());
	bRes = MakeSnapshot();
	if (!bRes) {
		msgs.str(L"");
		msgs << LSTRW(RID_VSS_MakeSnapshotErr) << std::hex << LastErr();
		m_cb->vssclient_OnProgress(vssclient_Callback::MakingSnapshot, LastErr(), msgs.str().c_str());
		goto _END;
	}
	msgs.str(L"");
	msgs << LSTRW(RID_VSS_SnapshotDone);
	m_cb->vssclient_OnProgress(vssclient_Callback::MadeSnapshot, 0, msgs.str().c_str());

_END:
	return 0;
}
#endif

BOOL Cvssclient::StartSnapshot()
{
	BOOL bRes;
	m_pProvider->Lock();
	bRes = m_pProvider->Start();
	m_pProvider->Unlock();
	return bRes;
}

BOOL Cvssclient::AddVolume(LPCWSTR lpszVol, BOOL bAsync)
{
	BOOL bRes;
	if (bAsync) {
		m_vols.push_back(lpszVol);
		return TRUE;
	}
	m_pProvider->Lock();
	bRes = m_pProvider->AddVolume(lpszVol);
	m_pProvider->Unlock();
	return bRes;
}

int Cvssclient::GetVolumeCount()
{
	return (int)m_vols.size();
}

BOOL Cvssclient::MakeSnapshot()
{
	BOOL bRes;
	m_pProvider->Lock();
	bRes = m_pProvider->MakeSnapshot();
	m_pProvider->Unlock();
	return bRes;
}

BOOL Cvssclient::GetVolumeSnapshotDevice(LPCWSTR lpszVol, std::wstring &strDevicePath)
{
	CComBSTR out;
	if (!m_pProvider->GetSnapshotDevicePath(lpszVol, &out)) {
		return FALSE;
	}
	strDevicePath = (LPCWSTR)out;
	return TRUE;
}

void Cvssclient::Cancel()
{
	if (m_pProvider) {
		m_pProvider->Cancel();
	}
}

void Cvssclient::DeleteSnapshots()
{
	if (m_pProvider) {
		m_pProvider->DeleteSnapshots();
	}
}

HRESULT Cvssclient::LastErr()
{
	if (m_pProvider) {
		return m_pProvider->m_lasthr;
	}

	return 0;
}
