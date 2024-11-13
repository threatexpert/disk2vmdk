#include "pch.h"
#include <string>
#include <vector>
#pragma warning( disable : 4091 )
#include "vssxp/vss.h"
#include "vssxp/vswriter.h"
#include "vssxp/vsbackup.h"
#include "vssProv.h"


struct SnapshotItemInfo{
	std::wstring vol;
	VSS_ID id;
};

class VSSProviderXP
	: public VSSProvider

{
	CComAutoCriticalSection m_csAsync;
	CComAutoCriticalSection m_csSelf;
	VSS_ID m_snapshotSetId;
	std::vector<SnapshotItemInfo> m_snapshotIds;
	CComPtr<IVssAsync> m_spAsync;
	BOOL m_bCanceled;
	BOOL m_bGatherWriterMetadata;
	BOOL m_bCompleted;
public:
	//
	VSSProviderXP()
		: VSSProvider()
	{
		m_pBackup = NULL;
		memset(&m_snapshotSetId, 0, sizeof(m_snapshotSetId));
		m_lasthr = 0;
		m_bCanceled = FALSE;
		m_bCompleted = m_bGatherWriterMetadata = FALSE;
	};
	virtual ~VSSProviderXP() {
		if (m_pBackup) {
			if (m_bGatherWriterMetadata) {
				m_pBackup->FreeWriterMetadata();
			}
			if (m_bCanceled) {
				m_pBackup->AbortBackup();
			}
			if (!m_bCompleted) {
				Complete();
			}
//#ifdef _DEBUG
//			ULONG ref;
//			ref = ((IUnknown*)m_pBackup)->AddRef();
//			ref = ((IUnknown*)m_pBackup)->Release();
//			assert(ref == 1);
//#endif
		}
	};

	BOOL Init() {
		m_lasthr = m_pCreateVssBackupComponents(&m_pBackup);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		m_lasthr = m_pBackup->InitializeForBackup();
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		return TRUE;
	}

	BOOL Start() {
		CComPtr<IVssAsync> spAsyncWriterMetadata;
		m_lasthr = m_pBackup->SetBackupState(true, true, VSS_BT_FULL);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		m_lasthr = m_pBackup->GatherWriterMetadata(&spAsyncWriterMetadata);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		m_bGatherWriterMetadata = TRUE;
		m_lasthr = WaitAndCheckForAsyncOperation(spAsyncWriterMetadata);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		m_lasthr = m_pBackup->StartSnapshotSet(&m_snapshotSetId);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		return TRUE;
	}

	BOOL IsVolumeSupported(LPCWSTR lpszVol)
	{
		CComBSTR bstrVol(lpszVol);
		BOOL bSupported = FALSE;
		m_lasthr = m_pBackup->IsVolumeSupported(GUID_NULL, bstrVol, &bSupported);
		return bSupported;
	}

	BOOL AddVolume(LPCWSTR lpszVol)
	{
		CComBSTR bstrVol(lpszVol);
		if (bstrVol.Length() && ((BSTR)bstrVol)[bstrVol.Length()-1] != '\\') {
			bstrVol += L"\\";
		}
		SnapshotItemInfo sii;
		m_lasthr = m_pBackup->AddToSnapshotSet(bstrVol, GUID_NULL, &sii.id);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		sii.vol = (LPCWSTR)bstrVol;
		m_snapshotIds.push_back(sii);
		return TRUE;
	}

	BOOL MakeSnapshot()
	{
		CComPtr<IVssAsync> spPrepare;
		CComPtr<IVssAsync> spDoShadowCopy;
		m_lasthr = m_pBackup->PrepareForBackup(&spPrepare);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		m_lasthr = WaitAndCheckForAsyncOperation(spPrepare);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		m_lasthr = m_pBackup->DoSnapshotSet(&spDoShadowCopy);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		m_lasthr = WaitAndCheckForAsyncOperation(spDoShadowCopy);
		if (FAILED(m_lasthr)) {
			return FALSE;
		}
		return TRUE;
	}

	BOOL GetSnapshotProp(LPCWSTR lpszVol, struct _VSS_SNAPSHOT_PROP* prop) {
		CComBSTR bstrVol(lpszVol);
		if (bstrVol.Length() && ((BSTR)bstrVol)[bstrVol.Length() - 1] != '\\') {
			bstrVol += L"\\";
		}
		for (auto& item : m_snapshotIds) {
			if (0 == _wcsicmp(item.vol.c_str(), bstrVol)) {
				memset(prop, 0, sizeof(_VSS_SNAPSHOT_PROP));
				m_lasthr = m_pBackup->GetSnapshotProperties(item.id, prop);
				if (FAILED(m_lasthr)) {
					return FALSE;
				}
				return TRUE;
			}
		}
		m_lasthr = E_FAIL;
		return FALSE;
	}

	BOOL GetSnapshotDevicePath(LPCWSTR lpszVol, BSTR* pbDevPath)
	{
		VSS_SNAPSHOT_PROP prop;
		memset(&prop, 0, sizeof(prop));
		if (!GetSnapshotProp(lpszVol, &prop)) {
			return FALSE;
		}
		CComBSTR out;
		out = prop.m_pwszSnapshotDeviceObject;
		*pbDevPath = out.Detach();
		m_pFreeSnapshotProperties(&prop);
		return TRUE;
	}

	BOOL DeleteSnapshots()
	{
		HRESULT result;
		LONG cDeletedSnapshots;
		GUID nonDeletedSnapshotId;
		result = m_pBackup->DeleteSnapshots(m_snapshotSetId, VSS_OBJECT_SNAPSHOT_SET, TRUE,
			&cDeletedSnapshots, &nonDeletedSnapshotId);
		return SUCCEEDED(result);
	}

	HRESULT WaitAndCheckForAsyncOperation(IVssAsync* p) {
		HRESULT hrStatus;
		HRESULT result;
		SetAsync(p);
		result = WaitAsync();
		SetAsync(NULL);
		if (FAILED(result)) {
			return result;
		}
		result = p->QueryStatus(&hrStatus, NULL);
		if (FAILED(result)) {
			return result;
		}
		if (hrStatus != VSS_S_ASYNC_FINISHED) {
			return hrStatus;
		}
		return result;
	}

	void Complete() {
		CComPtr<IVssAsync> spAsync;
		if (SUCCEEDED(m_pBackup->BackupComplete(&spAsync))) {
			if (SUCCEEDED(WaitAndCheckForAsyncOperation(spAsync))) {
				m_bCompleted = TRUE;
			}
		}
	}

	void Cancel() {
		m_csAsync.Lock();
		if (m_spAsync) {
			m_spAsync->Cancel();
		}
		m_bCanceled = TRUE;
		m_csAsync.Unlock();
	}

	void Lock() {
		m_csSelf.Lock();
	}

	void Unlock() {
		m_csSelf.Unlock();
	}

protected:
	void SetAsync(IVssAsync *p) {
		m_csAsync.Lock();
		m_spAsync = p;
		m_csAsync.Unlock();
	}
	HRESULT WaitAsync() {
		if (m_bCanceled) {
			Cancel();
		}
		return m_spAsync->Wait();
	}
};

VSSProvider* CreateVSSProvXP() {
	return new VSSProviderXP();
}