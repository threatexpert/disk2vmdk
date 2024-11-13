#include "pch.h"
#include "GetPCInfo.h"
#include "..\core\osutils.h"
#include "..\core\wmiutil.h"
#include "..\core\json.hpp"
using json = nlohmann::ordered_json;

using std::wstring;
using std::string;
using std::list;

#define _CW2UTF8(x) CW2A(x.c_str(), CP_UTF8)


bool json_from_pcinfo2(json& _json)
{
	class pcinfo
	{
		CWMIUtil m_Wmi;
	public:
		json _out;

		pcinfo()
		{
			m_Wmi.Init();

			_out = {
				{"os", {}},
				{"mainboard", {}},
				{"bios", {}},
				{"cpu", {}},
				{"ram", {}},
				{"nif", {}}
			};
		}
		~pcinfo()
		{

		}

		bool mainboard()
		{
			bool res = false;
			json j = {
				{"Manufacturer", ""},
				{"Product", ""},
				{"Version", ""},
				{"SerialNumber", ""}
			};
			std::wstring val;

			if (m_Wmi.ExecQuery(L"Win32_BaseBoard") != 0)
				goto _ERROR;
			if (!m_Wmi.Next())
				goto _ERROR;

			if (SUCCEEDED(m_Wmi.Get(L"Manufacturer", val)))
				j["Manufacturer"] = (LPCSTR)_CW2UTF8(val);
			if (SUCCEEDED(m_Wmi.Get(L"Product", val)))
				j["Product"] = (LPCSTR)_CW2UTF8(val);
			if (SUCCEEDED(m_Wmi.Get(L"Version", val)))
				j["Version"] = (LPCSTR)_CW2UTF8(val);
			if (SUCCEEDED(m_Wmi.Get(L"SerialNumber", val)))
				j["SerialNumber"] = (LPCSTR)_CW2UTF8(val);

			res = true;
		_ERROR:
			_out["mainboard"] = j;
			return res;
		}

		bool bios()
		{
			bool res = false;
			json j = {
				{"Manufacturer", ""},
				{"Name", ""},
				{"Version", ""},
				{"SerialNumber", ""},
				{"ReleaseDate", ""}
			};
			std::wstring val;

			if (m_Wmi.ExecQuery(L"Win32_BIOS") != 0)
				goto _ERROR;
			if (!m_Wmi.Next())
				goto _ERROR;

			if (SUCCEEDED(m_Wmi.Get(L"Manufacturer", val)))
				j["Manufacturer"] = (LPCSTR)_CW2UTF8(val);
			if (SUCCEEDED(m_Wmi.Get(L"Name", val)))
				j["Name"] = (LPCSTR)_CW2UTF8(val);
			if (SUCCEEDED(m_Wmi.Get(L"Version", val)))
				j["Version"] = (LPCSTR)_CW2UTF8(val);
			if (SUCCEEDED(m_Wmi.Get(L"SerialNumber", val)))
				j["SerialNumber"] = (LPCSTR)_CW2UTF8(val);
			if (SUCCEEDED(m_Wmi.Get(L"ReleaseDate", val)))
				j["ReleaseDate"] = (LPCSTR)_CW2UTF8(val);

			if (m_Wmi.ExecQuery(L"win32_computersystemproduct") != 0)
				goto _ERROR;
			if (!m_Wmi.Next())
				goto _ERROR;
			if (SUCCEEDED(m_Wmi.Get(L"uuid", val)))
				j["uuid"] = (LPCSTR)_CW2UTF8(val);

			res = true;
		_ERROR:
			_out["bios"] = j;
			return res;
		}

		bool cpu()
		{
			bool res = false;

			std::wstring val;
			int count = 0;

			if (m_Wmi.ExecQuery(L"Win32_Processor") != 0)
				goto _ERROR;

			while (m_Wmi.Next())
			{
				json j = {
					{"Manufacturer", ""},
					{"MaxClockSpeed", ""},
					{"Name", ""},
					{"NumberOfCores", ""},
					{"NumberOfLogicalProcessors", ""}
				};

				if (SUCCEEDED(m_Wmi.Get(L"Manufacturer", val)))
					j["Manufacturer"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"MaxClockSpeed", val)))
					j["MaxClockSpeed"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"Name", val)))
					j["Name"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"NumberOfCores", val)))
					j["NumberOfCores"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"NumberOfLogicalProcessors", val)))
					j["NumberOfLogicalProcessors"] = (LPCSTR)_CW2UTF8(val);

				_out["cpu"] += j;
				count++;
			}

			res = count > 0;
		_ERROR:
			return res;
		}

		bool ram()
		{
			bool res = false;

			std::wstring val;
			int count = 0;

			if (m_Wmi.ExecQuery(L"Win32_PhysicalMemory") != 0)
				goto _ERROR;

			while (m_Wmi.Next())
			{
				json j = {
					{"Manufacturer", ""},
					{"Speed", ""},
					{"Capacity", ""}
				};

				if (SUCCEEDED(m_Wmi.Get(L"Manufacturer", val)))
					j["Manufacturer"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"Speed", val)))
					j["Speed"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"Capacity", val)))
					j["Capacity"] = (LPCSTR)_CW2UTF8(val);

				_out["ram"] += j;
				count++;
			}

			res = count > 0;
		_ERROR:
			return res;
		}

		bool nif()
		{
			bool res = false;

			std::wstring val;
			int count = 0;

			if (m_Wmi.ExecQuery(L"Win32_NetworkAdapter", L" PNPDeviceID LIKE 'PCI\\\\%'") == -1)
			{
				goto _ERROR;
			}

			while (m_Wmi.Next())
			{
				json j = {
					{"Manufacturer", ""},
					{"Name", ""},
					{"MACAddress", ""},
					{"PhysicalAdapter", ""},
					{"NetConnectionID", ""},
					{"PNPDeviceID", ""},
					{"IPAddress", ""}
				};

				std::wstring macaddr;

				if (SUCCEEDED(m_Wmi.Get(L"Manufacturer", val)))
					j["Manufacturer"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"Name", val)))
					j["Name"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"MACAddress", val))) {
					macaddr = val;
					j["MACAddress"] = (LPCSTR)_CW2UTF8(val);
				}
				if (SUCCEEDED(m_Wmi.Get(L"PhysicalAdapter", val)))
					j["PhysicalAdapter"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"NetConnectionID", val)))
					j["NetConnectionID"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"PNPDeviceID", val)))
					j["PNPDeviceID"] = (LPCSTR)_CW2UTF8(val);

				if (macaddr.size()) {
					CWMIUtil wmi;
					wmi.Init();
					std::wstring filter{};
					std::wstring str_ip{};
					std::wstring settingid{};

					json jaddr = {
						{"IPAddress", ""},
						{"IPSubnet", ""},
						{"IPEnabled", ""},
						{"DefaultIPGateway", ""}
					};

					filter = L" MACAddress='" + macaddr + L"'";
					if (wmi.ExecQuery(L"Win32_NetworkAdapterConfiguration",
						filter.c_str()) == 0 && wmi.Next())
					{
						settingid = wmi.Get(L"SettingID");
						str_ip = wmi.Get(L"IPAddress");
						jaddr["IPAddress"] = (LPCSTR)_CW2UTF8(str_ip);
						jaddr["IPSubnet"] = (LPCSTR)_CW2UTF8(wmi.Get(L"IPSubnet"));
						jaddr["IPEnabled"] = (LPCSTR)_CW2UTF8(wmi.Get(L"IPEnabled"));
						jaddr["DefaultIPGateway"] = (LPCSTR)_CW2UTF8(wmi.Get(L"DefaultIPGateway"));

						j["IPAddress"] = jaddr;
					}
				}

				_out["nif"] += j;
				count++;
			}

			res = count > 0;
		_ERROR:
			return res;
		}

		bool os()
		{
			bool res = false;

			std::wstring val;
			json j = {
				{"Caption", ""},
				{"Version", ""},
				{"OSArchitecture", ""},
				{"CSName", ""},
				{"windir", ""}
			};

			if (m_Wmi.ExecQuery(L"Win32_OperatingSystem") != 0)
				goto _ERROR;
			if (!m_Wmi.Next())
				goto _ERROR;
			else
			{
				if (SUCCEEDED(m_Wmi.Get(L"Caption", val)))
					j["Caption"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"Version", val)))
					j["Version"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"OSArchitecture", val)))
					j["OSArchitecture"] = (LPCSTR)_CW2UTF8(val);
				if (SUCCEEDED(m_Wmi.Get(L"CSName", val)))
					j["CSName"] = (LPCSTR)_CW2UTF8(val);
			}

			WCHAR szWindir[MAX_PATH];
			GetWindowsDirectoryW(szWindir, MAX_PATH);
			val = szWindir;
			j["windir"] = (LPCSTR)_CW2UTF8(val);
			res = true;
		_ERROR:
			_out["os"] = j;
			return res;
		}


		const json& build()
		{
			os();
			mainboard();
			bios();
			cpu();
			ram();
			nif();

			return _out;
		}
	}data;

	_json = data.build();
	return true;
}

std::string get_pcinfo()
{
	json js;
	if (json_from_pcinfo2(js)) {
		return js.dump(2);
	}
	return "{}";
}
