#pragma once

class WMIProvider;
class CWMIUtil
{
public:
	CWMIUtil();
	virtual ~CWMIUtil();

private:
	CWMIUtil(const CWMIUtil&) {}
	CWMIUtil& operator= (const CWMIUtil&) {}

public:
	int Init(const wchar_t* lpszNamespace = L"ROOT\\CIMV2");
	int ExecQuery(const wchar_t* lpszClass
		, const wchar_t* lpszFilter = nullptr);
	bool Next();
	std::wstring Get(const wchar_t* szValue);
	HRESULT Get(const wchar_t* szValue, std::wstring &result);

private:
	bool				m_inited_com;
	bool				m_init;
	WMIProvider		   *m_pProvider;
};