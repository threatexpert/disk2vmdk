#pragma once

#include <Windows.h>
#include <atlbase.h>

typedef
HRESULT APIENTRY __CreateVssBackupComponentsInternal(
	class IVssBackupComponents** ppBackup
);;
typedef
void APIENTRY __VssFreeSnapshotPropertiesInternal(
	struct _VSS_SNAPSHOT_PROP* pProp
);

class VSSProvider
{
public:
	__CreateVssBackupComponentsInternal* m_pCreateVssBackupComponents;
	__VssFreeSnapshotPropertiesInternal* m_pFreeSnapshotProperties;
	class IVssBackupComponents* m_pBackup;
	HRESULT m_lasthr;

	VSSProvider()
		: m_pCreateVssBackupComponents(NULL)
		, m_pFreeSnapshotProperties(NULL)
		, m_pBackup(NULL)
		, m_lasthr(0)
	{}
	virtual ~VSSProvider() {};
	virtual BOOL Init() = 0;
	virtual BOOL Start() = 0;
	virtual BOOL IsVolumeSupported(LPCWSTR lpszVol) = 0;
	virtual BOOL AddVolume(LPCWSTR lpszVol) = 0;
	virtual BOOL MakeSnapshot() = 0;
	virtual BOOL GetSnapshotDevicePath(LPCWSTR lpszVol, BSTR *pbDevPath) = 0;
	virtual BOOL DeleteSnapshots() = 0;
	virtual void Complete() = 0;
	virtual void Cancel() = 0;
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
};

extern VSSProvider* CreateVSSProvXP();
extern VSSProvider* CreateVSSProv2003();

