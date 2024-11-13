#pragma once

#include <string>

class CVolumeReader
{
	HANDLE m_hVol;
	uint64_t m_Size;
	uint64_t m_pos;
	std::wstring m_strVolName;
	DWORD m_dwOptions;
	BOOL m_bOpenLater;

	void* _pVolBitMapBuf;
	DWORD _BitMapBytes;
	DWORD _ClusterUnitSize;
	DWORD _SectorsPerCluster;
	DWORD _BytesPerSector;
	DWORD _NumberOfFreeClusters;
	DWORD _TotalNumberOfCluster;
	uint64_t _TotalNumberOfClusterInByte;

	BYTE _BootSector[512];
	int _FSType;
	uint64_t m_1stClusterOffset;
	BOOL _OnlyUsedSpace;

	std::wstring m_lasterr;

	FILETIME m_ft_beforeSnapshot;
	uint64_t m_calcedSize;
public:
	enum {
		OptOnlyUsedSpace = 1,
		OptIgnorePagefile = 2,
		OptIgnoreHiberfil = 4,
		OptIgnoreSVI = 8,
	};

	CVolumeReader();
	virtual ~CVolumeReader();

	DWORD getOptions();
	BOOL setOptions(DWORD dwOptions);
	BOOL setSource(LPCWSTR lpszSrc, uint64_t nSrcSize, BOOL openLater);
	std::wstring getSource(BOOL *pOpened);
	BOOL GetSize(uint64_t* pSize);
	BOOL SetPos(uint64_t pos);
	BOOL Read(void* lpBuffer, DWORD nNumberOfBytesToRead, uint64_t* lpNumberOfBytesRead, BOOL * pIsFreeSpace);
	void Close();
	std::wstring lasterr() { return m_lasterr; };
	void SetSnapshotTime();
	BOOL CalcDataSize(uint64_t* pSize);

protected:
	BOOL openSource();
	BOOL GetBitmap();
	int64_t GetContiguous(int64_t start, bool& stat);
	BOOL ReadBootSector();
	BOOL ParseBootSector();
	BOOL GetFStype(int &type);
	uint64_t UnmaskSomeFiles();
	bool is_snapshot_time_valid();
};