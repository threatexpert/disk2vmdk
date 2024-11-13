#include "pch.h"

#define _WIN32_DCOM

#include <comdef.h>
#include <Wbemidl.h>
#include <atlbase.h>
#include <string>

#include "WMIUtil.h"

#pragma comment(lib, "wbemuuid.lib")

class WMIProvider
{
public:
	WMIProvider() {};
	~WMIProvider() {
		mClsObj.Release();
		mEnum.Release();
		mSvc.Release();
	};
	CComPtr<IWbemServices>			mSvc;
	CComPtr<IEnumWbemClassObject>	mEnum;
	CComPtr<IWbemClassObject>		mClsObj;
};

CWMIUtil::CWMIUtil(void)
	: m_inited_com(false)
	, m_init(false)
	, m_pProvider()
{
	m_inited_com = SUCCEEDED(CoInitializeEx(0, COINIT_MULTITHREADED));
	m_pProvider = new WMIProvider();
}

CWMIUtil::~CWMIUtil(void)
{
	delete m_pProvider;
	if (m_inited_com)
	{
		CoUninitialize();
	}
}

int CWMIUtil::Init(const wchar_t * lpszNamespace)
{
	HRESULT hres;

	// Set general COM security levels
	hres = CoInitializeSecurity(
		NULL,
		-1,                          // COM authentication
		NULL,                        // Authentication services
		NULL,                        // Reserved
		RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
		RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
		NULL,                        // Authentication info
		EOAC_NONE,                   // Additional capabilities 
		NULL                         // Reserved
	);
	if (FAILED(hres) && hres != RPC_E_TOO_LATE)
	{
		return -1;
	}

	// Obtain the initial locator to WMI
	CComPtr<IWbemLocator> spLoc{};
	hres = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID*)&spLoc);
	if (FAILED(hres))
	{
		return -1;
	}

	// Connect to WMI through the IWbemLocator::ConnectServer method
	hres = spLoc->ConnectServer(
		_bstr_t(lpszNamespace),    // Object path of WMI namespace
		NULL,                       // User name. NULL = current user
		NULL,                       // User password. NULL = current
		0,                          // Locale. NULL indicates current
		NULL,                       // Security flags.
		0,                          // Authority (for example, Kerberos)
		0,                          // Context object 
		&m_pProvider->mSvc          // pointer to IWbemServices proxy
	);
	if (FAILED(hres))
	{
		return -1;
	}

	// Set security levels on the proxy
	hres = CoSetProxyBlanket(
		m_pProvider->mSvc,           // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
		NULL,                        // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE                    // proxy capabilities 
	);
	if (FAILED(hres))
	{
		return -1;
	}

	m_init = true;
	return 0;
}

int CWMIUtil::ExecQuery(const wchar_t* lpszClass, const wchar_t* lpszFilter)
{
	m_pProvider->mEnum.Release();

	if (!m_init)
	{
		return -1;
	}

	HRESULT hres;

	bstr_t bFilter = lpszFilter ?
		bstr_t(L" WHERE ") + lpszFilter : bstr_t(L"");
	// Use the IWbemServices pointer to make requests of WMI
	hres = m_pProvider->mSvc->ExecQuery(
		bstr_t(L"WQL"),
		bstr_t(L"SELECT * FROM ") + lpszClass + bFilter,
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&m_pProvider->mEnum);
	if (FAILED(hres))
	{
		return -1;
	}

	return 0;
}

bool CWMIUtil::Next()
{
	m_pProvider->mClsObj.Release();

	if (!m_init || !m_pProvider->mEnum)
	{
		return false;
	}

	ULONG uReturn = 0;
	HRESULT hr = m_pProvider->mEnum->Next(WBEM_INFINITE, 1,
		&m_pProvider->mClsObj, &uReturn);
	if (FAILED(hr) || !uReturn)
	{
		return false;
	}

	return true;
}

std::wstring CWMIUtil::Get(const wchar_t* lpszValue)
{
	std::wstring ret;
	Get(lpszValue, ret);
	return ret;
}

HRESULT CWMIUtil::Get(const wchar_t * szValue, std::wstring & result)
{
	result.clear();

	if (!m_init || !m_pProvider->mClsObj)
	{
		return E_FAIL;
	}

	VARIANT vtProp;
	HRESULT hr = m_pProvider->mClsObj->Get(bstr_t(szValue),
		0, &vtProp, 0, 0);

	if (SUCCEEDED(hr)) {
		if (V_VT(&vtProp) == VT_NULL) {
			hr = E_FAIL;
		}
		else if (V_ISARRAY(&vtProp)) {
			VARIANT vtTmp{};
			V_VT(&vtTmp) = V_VT(&vtProp) & 0xFF;
			BSTR* pData{};
			SafeArrayAccessData(V_ARRAY(&vtProp), (PVOID*)&pData);
			for (UINT i=0; i<V_ARRAY(&vtProp)->rgsabound[0].cElements; ++i)
			{
				V_BSTR(&vtTmp) = pData[i];
				if (V_VT(&vtTmp) != VT_BSTR)
				{
					hr = VariantChangeType(&vtTmp, &vtTmp, 0, VT_BSTR);
					if (!SUCCEEDED(hr)) break;
				}

				result += V_BSTR(&vtTmp);
				result += L", ";
			}
			SafeArrayUnaccessData(V_ARRAY(&vtProp));
			result.resize(result.size()-2);
		}
		else {
			if (V_VT(&vtProp) != VT_BSTR) {
				hr = VariantChangeType(&vtProp, &vtProp, 0, VT_BSTR);
			}
			if (SUCCEEDED(hr)) {
				result = V_BSTR(&vtProp);
			}
		}
		VariantClear(&vtProp);
	}

	return hr;
}
