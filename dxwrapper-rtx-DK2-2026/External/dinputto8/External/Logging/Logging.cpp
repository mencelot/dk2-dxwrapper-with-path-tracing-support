/**
* Copyright (C) 2025 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*
* Code taken from DDrawCompat for logging
* https://github.com/narzoul/DDrawCompat/
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <vector>
#include <psapi.h>
#include <shlwapi.h>
#include <VersionHelpers.h>
#include <tlhelp32.h>
#include "Logging.h"

// Function pointers for GetProcessImageFileNameA and GetProcessImageFileNameW
typedef DWORD(WINAPI* GetProcessImageFileNameA_t)(HANDLE, LPSTR, DWORD);
typedef DWORD(WINAPI* GetProcessImageFileNameW_t)(HANDLE, LPWSTR, DWORD);

static GetProcessImageFileNameA_t realGetProcessImageFileNameA = nullptr;
static GetProcessImageFileNameW_t realGetProcessImageFileNameW = nullptr;

// Initialization to load the real GetProcessImageFileName functions
static void InitializeGetProcessImageFileName()
{
	// Try to load from kernel32.dll first
	HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
	if (hKernel32)
	{
		// Check for K32-prefixed functions first in kernel32.dll
		realGetProcessImageFileNameA = (GetProcessImageFileNameA_t)GetProcAddress(hKernel32, "K32GetProcessImageFileNameA");
		realGetProcessImageFileNameW = (GetProcessImageFileNameW_t)GetProcAddress(hKernel32, "K32GetProcessImageFileNameW");

		// If K32-prefixed versions are not found, try the original names
		if (!realGetProcessImageFileNameA)
		{
			realGetProcessImageFileNameA = (GetProcessImageFileNameA_t)GetProcAddress(hKernel32, "GetProcessImageFileNameA");
		}
		if (!realGetProcessImageFileNameW)
		{
			realGetProcessImageFileNameW = (GetProcessImageFileNameW_t)GetProcAddress(hKernel32, "GetProcessImageFileNameW");
		}
	}

	// If the function wasn't found in kernel32.dll, fall back to psapi.dll
	if (!realGetProcessImageFileNameA || !realGetProcessImageFileNameW)
	{
		HMODULE hPsapi = LoadLibraryW(L"psapi.dll");
		if (hPsapi)
		{
			// Check for K32-prefixed functions first in psapi.dll (for completeness, although unlikely)
			if (!realGetProcessImageFileNameA)
			{
				realGetProcessImageFileNameA = (GetProcessImageFileNameA_t)GetProcAddress(hPsapi, "K32GetProcessImageFileNameA");
			}
			if (!realGetProcessImageFileNameW)
			{
				realGetProcessImageFileNameW = (GetProcessImageFileNameW_t)GetProcAddress(hPsapi, "K32GetProcessImageFileNameW");
			}

			// If K32-prefixed versions are not found, try the original names in psapi.dll
			if (!realGetProcessImageFileNameA)
			{
				realGetProcessImageFileNameA = (GetProcessImageFileNameA_t)GetProcAddress(hPsapi, "GetProcessImageFileNameA");
			}
			if (!realGetProcessImageFileNameW)
			{
				realGetProcessImageFileNameW = (GetProcessImageFileNameW_t)GetProcAddress(hPsapi, "GetProcessImageFileNameW");
			}
		}
	}
}

// The replacement functions
#pragma warning(suppress: 4505)
static DWORD NewGetProcessImageFileName(HANDLE hProcess, LPSTR lpImageFileName, DWORD nSize)
{
	if (!realGetProcessImageFileNameA)
	{
		InitializeGetProcessImageFileName();
	}
	if (realGetProcessImageFileNameA)
	{
		return realGetProcessImageFileNameA(hProcess, lpImageFileName, nSize);
	}
	SetLastError(ERROR_PROC_NOT_FOUND);
	return 0;
}

#pragma warning(suppress: 4505)
static DWORD NewGetProcessImageFileName(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize)
{
	if (!realGetProcessImageFileNameW)
	{
		InitializeGetProcessImageFileName();
	}
	if (realGetProcessImageFileNameW)
	{
		return realGetProcessImageFileNameW(hProcess, lpImageFileName, nSize);
	}
	SetLastError(ERROR_PROC_NOT_FOUND);
	return 0;
}

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

namespace Logging
{
	bool EnableLogging = true;

	void GetOsVersion(OSVERSIONINFOA*);
	void GetVersionReg(OSVERSIONINFO *);
	void GetVersionFile(OSVERSIONINFO *);
	void GetProductNameFromRegistry(char *Name, DWORD Size);
	void GetProductNameFromVersion(OSVERSIONINFOA& inVersion, char* outName, size_t outSize);
	DWORD GetParentProcessId();
	bool CheckProcessNameFromPID(DWORD pid, char *name);
	bool CheckEachParentFolder(char *file, char *path);
	void LogGOGGameType();
	void LogSteamGameType();
}

namespace
{
	template <typename DevMode>
	std::ostream& streamDevMode(std::ostream& os, const DevMode& dm)
	{
		return Logging::LogStruct(os)
			<< dm.dmPelsWidth
			<< dm.dmPelsHeight
			<< dm.dmBitsPerPel
			<< dm.dmDisplayFrequency
			<< dm.dmDisplayFlags;
	}
}

std::ostream& operator<<(std::ostream& os, const char* str)
{
	if (!str)
	{
		return os << "null";
	}

	if (!Logging::Log::isPointerDereferencingAllowed())
	{
		return os << static_cast<const void*>(str);
	}

	return os.write(str, strlen(str));
}

std::ostream& operator<<(std::ostream& os, const unsigned char* data)
{
	return os << static_cast<const void*>(data);
}

std::ostream& operator<<(std::ostream& os, const wchar_t* wstr)
{
	if (!wstr)
	{
		return os << "null";
	}

	if (!Logging::Log::isPointerDereferencingAllowed())
	{
		return os << static_cast<const void*>(wstr);
	}

	return os << static_cast<std::wstring>(std::wstring(wstr));
}

std::ostream& operator<<(std::ostream& os, std::nullptr_t)
{
	return os << "null";
}

std::ostream& operator<<(std::ostream& os, std::string str)
{
	return os << static_cast<const char*>(str.c_str());
}

std::ostream& operator<<(std::ostream& os, std::wstring wstr)
{
	size_t size = wstr.size() + 1;
	std::string str;
	str.resize(size, '\0');
	wcstombs_s(nullptr, &str[0], size, wstr.c_str(), _TRUNCATE);
	return os << str;
}

namespace Logging
{
	DWORD Log::s_outParamDepth = 0;
	bool Log::s_isLeaveLog = false;
}

void Logging::LogFormat(char * fmt, ...)
{
	if (!EnableLogging)
	{
		return;
	}

	// Format arg list
	va_list ap;
	va_start(ap, fmt);
	auto size = vsnprintf(nullptr, 0, fmt, ap);
	va_end(ap);

	std::string output(size + 1, '\0');

	// Reinitialize va_list for the second usage
	va_start(ap, fmt);
	vsprintf_s(&output[0], size + 1, fmt, ap);
	va_end(ap);

	// Log formatted text
	Log() << output.c_str();
}

void Logging::LogFormat(wchar_t * fmt, ...)
{
	if (!EnableLogging)
	{
		return;
	}

	// Format arg list
	va_list ap;
	va_start(ap, fmt);
#pragma warning(suppress: 4996)
	auto size = _vsnwprintf(nullptr, 0, fmt, ap);
	va_end(ap);

	std::wstring output(size + 1, '\0');

	// Reinitialize va_list for the second usage
	va_start(ap, fmt);
	vswprintf_s(&output[0], size + 1, fmt, ap);
	va_end(ap);

	// Log formatted text
	Log() << output.c_str();
}

// Logs the process name and PID
void Logging::LogProcessNameAndPID()
{
	if (!EnableLogging)
	{
		return;
	}

	// Get process name
	wchar_t exepath[MAX_PATH] = { 0 };
	GetModuleFileNameW(nullptr, exepath, MAX_PATH);

	// Remove path and add process name
	wchar_t *pdest = wcsrchr(exepath, '\\');

	// Log process name and ID
	Log() << (++pdest) << " (PID:" << GetCurrentProcessId() << ")";
}

// Get Windows Operating System version number from RtlGetVersion
void Logging::GetOsVersion(OSVERSIONINFOA* outVersion)
{
	if (!outVersion)
	{
		return;
	}

	// Clear and initialize the version info struct
	ZeroMemory(outVersion, sizeof(OSVERSIONINFOA));
	outVersion->dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

	// Try to get a handle to kernel32.dll without increasing the ref count
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	if (!hKernel32)
	{
		Log() << __FUNCTION__ << " Warning: kernel32.dll not found.";
		return;
	}

	// Try to locate the GetVersionExA function
	using GetVersionExA_t = BOOL(WINAPI*)(LPOSVERSIONINFOA);
	auto pGetVersionExA = reinterpret_cast<GetVersionExA_t>(GetProcAddress(hKernel32, "GetVersionExA"));
	if (!pGetVersionExA)
	{
		Log() << __FUNCTION__ << " Warning: GetVersionExA not available.";
		return;
	}

	// Retrieve version information
	if (!pGetVersionExA(outVersion))
	{
		Log() << __FUNCTION__ << " Warning: GetVersionExA call failed.";
	}
}

// Get Windows Operating System version number from the registry
void Logging::GetVersionReg(OSVERSIONINFO* outVersion)
{
	if (!outVersion)
	{
		return;
	}

	// Clear and initialize the version info structure
	ZeroMemory(outVersion, sizeof(OSVERSIONINFO));
	outVersion->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// Registry key to query
	const char* regPath = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
	const char* regMajor = "CurrentMajorVersionNumber";
	const char* regMinor = "CurrentMinorVersionNumber";

	DWORD major = 0;
	DWORD minor = 0;
	DWORD dataSize = sizeof(DWORD);
	DWORD valueType = 0;
	HKEY hKey = nullptr;

	// Open the registry key
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		// Read both major and minor version values
		if (RegQueryValueExA(hKey, regMajor, nullptr, &valueType, reinterpret_cast<LPBYTE>(&major), &dataSize) == ERROR_SUCCESS &&
			RegQueryValueExA(hKey, regMinor, nullptr, &valueType, reinterpret_cast<LPBYTE>(&minor), &dataSize) == ERROR_SUCCESS)
		{
			outVersion->dwMajorVersion = major;
			outVersion->dwMinorVersion = minor;
		}

		RegCloseKey(hKey);
	}
}

// Get Windows Operating System version number from kernel32.dll
void Logging::GetVersionFile(OSVERSIONINFO* versionInfo)
{
	if (!versionInfo)
	{
		return;
	}

	// Initialize output structure
	ZeroMemory(versionInfo, sizeof(OSVERSIONINFO));
	versionInfo->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// Get full path to kernel32.dll
	char systemPath[MAX_PATH] = {};
	if (GetSystemDirectoryA(systemPath, MAX_PATH) == 0)
	{
		Log() << "Failed to get system directory.";
		return;
	}

	strcat_s(systemPath, "\\kernel32.dll");

	// Load version.dll and retrieve required function pointers
	HMODULE hVersion = GetModuleHandleA("version.dll");
	if (!hVersion)
	{
		hVersion = LoadLibraryA("version.dll");
	}
	if (!hVersion)
	{
		Log() << "Unable to load version.dll.";
		return;
	}

	auto pGetFileVersionInfoSizeA = reinterpret_cast<decltype(&GetFileVersionInfoSizeA)>(
		GetProcAddress(hVersion, "GetFileVersionInfoSizeA"));
	auto pGetFileVersionInfoA = reinterpret_cast<decltype(&GetFileVersionInfoA)>(
		GetProcAddress(hVersion, "GetFileVersionInfoA"));
	auto pVerQueryValueA = reinterpret_cast<decltype(&VerQueryValueA)>(
		GetProcAddress(hVersion, "VerQueryValueA"));

	if (!pGetFileVersionInfoSizeA || !pGetFileVersionInfoA || !pVerQueryValueA)
	{
		Log() << "Missing version functions in version.dll.";
		return;
	}

	DWORD handle = 0;
	DWORD verSize = pGetFileVersionInfoSizeA(systemPath, &handle);
	if (verSize == 0)
	{
		return;
	}

	std::vector<BYTE> versionData(verSize);
	if (!pGetFileVersionInfoA(systemPath, handle, verSize, versionData.data()))
	{
		return;
	}

	VS_FIXEDFILEINFO* fileInfo = nullptr;
	UINT len = 0;
	if (pVerQueryValueA(versionData.data(), "\\", reinterpret_cast<LPVOID*>(&fileInfo), &len))
	{
		if (fileInfo && fileInfo->dwSignature == 0xFEEF04BD)
		{
			versionInfo->dwMajorVersion = HIWORD(fileInfo->dwFileVersionMS);
			versionInfo->dwMinorVersion = LOWORD(fileInfo->dwFileVersionMS);
			versionInfo->dwBuildNumber = HIWORD(fileInfo->dwFileVersionLS);
		}
	}
}

// Get Windows Operating System name from registry
void Logging::GetProductNameFromRegistry(char* Name, DWORD Size)
{
	if (!Name || Size == 0)
	{
		return;
	}

	Name[0] = '\0';

	HKEY hKey = nullptr;
	const char* regPath = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
	const char* valueName = "ProductName";

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD dataSize = Size;
		DWORD valueType = 0;

		LONG result = RegQueryValueExA(hKey, valueName, nullptr, &valueType, reinterpret_cast<LPBYTE>(Name), &dataSize);
		RegCloseKey(hKey);

		if (result == ERROR_SUCCESS && (valueType == REG_SZ || valueType == REG_EXPAND_SZ))
		{
			// Ensure null termination
			if (dataSize > 0 && dataSize <= Size)
			{
				// dataSize includes the terminating null, so we are good
				Name[dataSize - 1] = '\0';
			}
			else
			{
				// Defensive: forcibly null terminate last char
				Name[Size - 1] = '\0';
			}
		}
		else
		{
			// Failed to read or wrong type
			Name[0] = '\0';
		}
	}
}

void Logging::GetProductNameFromVersion(OSVERSIONINFOA& inVersion, char* outName, size_t outSize)
{
	if (!outName || outSize == 0)
	{
		return;
	}

	outName[0] = '\0'; // Clear output

	// Helper macro to safely copy strings
#define SAFE_STRCPY(dest, src) strncpy_s(dest, outSize, src, _TRUNCATE)

// Map version numbers to names based on official Microsoft versioning info
// See: https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-osversioninfoexa

	if (inVersion.dwMajorVersion == 10 && inVersion.dwMinorVersion == 0)
	{
		if (inVersion.dwBuildNumber >= 22000)
		{
			// Windows 11 was introduced with build 22000+
			SAFE_STRCPY(outName, "Windows 11");
		}
		else if (inVersion.dwBuildNumber >= 19041)
		{
			// Windows 10, 2004 or later
			SAFE_STRCPY(outName, "Windows 10");
		}
		else
		{
			// Windows 10 earlier builds
			SAFE_STRCPY(outName, "Windows 10 (Pre-2004)");
		}
	}
	else if (inVersion.dwMajorVersion == 6)
	{
		switch (inVersion.dwMinorVersion)
		{
		case 3:
			SAFE_STRCPY(outName, "Windows 8.1 / Windows Server 2012 R2");
			break;
		case 2:
			SAFE_STRCPY(outName, "Windows 8 / Windows Server 2012");
			break;
		case 1:
			SAFE_STRCPY(outName, "Windows 7 / Windows Server 2008 R2");
			break;
		case 0:
			SAFE_STRCPY(outName, "Windows Vista / Windows Server 2008");
			break;
		default:
			SAFE_STRCPY(outName, "Windows NT 6.x (Unknown)");
			break;
		}
	}
	else if (inVersion.dwMajorVersion == 5)
	{
		switch (inVersion.dwMinorVersion)
		{
		case 2:
			SAFE_STRCPY(outName, "Windows Server 2003 / Windows XP x64");
			break;
		case 1:
			SAFE_STRCPY(outName, "Windows XP");
			break;
		case 0:
			SAFE_STRCPY(outName, "Windows 2000");
			break;
		default:
			SAFE_STRCPY(outName, "Windows NT 5.x (Unknown)");
			break;
		}
	}
	else
	{
		// Unknown or unsupported version
		SAFE_STRCPY(outName, "Unknown Windows Version");
	}

#undef SAFE_STRCPY
}

// Log hardware manufacturer
void Logging::LogComputerManufacturer()
{
	if (!EnableLogging)
	{
		return;
	}

	std::string manufacturer, alternate;
	std::string chassisType;
	DWORD currentIndex = 0, dataSize = 0, result = 0;
	BYTE* smbiosData = nullptr;

	// Open registry key to access raw SMBIOS data
	HKEY hKey;
	result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\mssmbios\\Data", 0, KEY_READ, &hKey);
	if (result != ERROR_SUCCESS)
	{
		return;
	}

	DWORD valueType = 0;
	result = RegQueryValueExA(hKey, "SMBiosData", nullptr, &valueType, nullptr, &dataSize);
	if (result != ERROR_SUCCESS || valueType != REG_BINARY || dataSize < 8)
	{
		RegCloseKey(hKey);
		return;
	}

	smbiosData = new (std::nothrow) BYTE[dataSize];
	if (!smbiosData)
	{
		RegCloseKey(hKey);
		return;
	}

	result = RegQueryValueExA(hKey, "SMBiosData", nullptr, nullptr, smbiosData, &dataSize);
	RegCloseKey(hKey);

	if (result != ERROR_SUCCESS)
	{
		delete[] smbiosData;
		return;
	}

	// Parse SMBIOS structures
	DWORD structTableStart = 8 + *(WORD*)(smbiosData + 6);
	DWORD structTableEnd = min(dataSize, 8U + *(WORD*)(smbiosData + 4U));
	if (structTableEnd <= structTableStart)
	{
		delete[] smbiosData;
		return;
	}

	currentIndex = structTableStart;
	smbiosData[structTableEnd - 1] = 0x00;  // Null-terminate just in case

	bool foundChassis = false;
	UINT state = 0;

	while (currentIndex + 1 < structTableEnd && smbiosData[currentIndex] != 0x7F)
	{
		BYTE type = smbiosData[currentIndex];
		BYTE length = smbiosData[currentIndex + 1];
		UINT stringIndex = 1;
		DWORD structStart = currentIndex;
		currentIndex += length;

		if (currentIndex >= structTableEnd)
		{
			break;
		}

		do
		{
			const char* str = reinterpret_cast<const char*>(smbiosData + currentIndex);
			bool used = false;

			if (type == 0x01 && state == 0)
			{
				BYTE idx = smbiosData[structStart + 4];
				if (idx == stringIndex)
				{
					if (_stricmp(str, "System manufacturer") == 0)
					{
						state++;
						used = true;
					}
				}
			}
			else if (type == 0x02)
			{
				BYTE idx = smbiosData[structStart + 4];
				if (idx == stringIndex)
				{
					if (state == 1)
					{
						manufacturer = str;
						state++;
						used = true;
					}
					else if (state == 0)
					{
						alternate = str;
						used = true;
					}
				}
			}
			else if (type == 0x03 && stringIndex == 1 && !foundChassis)
			{
				BYTE formFactor = smbiosData[structStart + 5];
				switch (formFactor)
				{
				case 0x02: chassisType = "(Unknown)"; break;
				case 0x03: chassisType = "(Desktop)"; break;
				case 0x04: chassisType = "(Low Profile Desktop)"; break;
				case 0x06: chassisType = "(Mini Tower)"; break;
				case 0x07: chassisType = "(Tower)"; break;
				case 0x08: chassisType = "(Portable)"; break;
				case 0x09: chassisType = "(Laptop)"; break;
				case 0x0A: chassisType = "(Notebook)"; break;
				case 0x0E: chassisType = "(Sub Notebook)"; break;
				default:   chassisType = "(Other)"; break;
				}
				foundChassis = true;
			}

			if (used)
			{
				currentIndex += (DWORD)strlen(str);
			}

			currentIndex++;
			stringIndex++;
		} while (currentIndex < structTableEnd && smbiosData[currentIndex] != 0x00);

		while (currentIndex < structTableEnd && smbiosData[currentIndex] == 0x00)
		{
			currentIndex++;
		}

		if (foundChassis)
		{
			break;
		}
	}

	delete[] smbiosData;

	if (!manufacturer.empty())
	{
		Log() << manufacturer << (chassisType.empty() ? "" : (" " + chassisType));
	}
	if (!alternate.empty())
	{
		Log() << alternate << (chassisType.empty() ? "" : (" " + chassisType));
	}
}

// Log Windows Operating System type
void Logging::LogOSVersion()
{
	if (!EnableLogging)
	{
		return;
	}

	// Declare vars
	OSVERSIONINFOA oOS_version;
	OSVERSIONINFO fOS_version, rOS_version;

	// GetVersion from RtlGetVersion which is needed for some cases (Need for Speed III)
	GetOsVersion(&oOS_version);
	std::string str(" " + std::string(oOS_version.szCSDVersion));
	char *ServicePack = (str.size() > 1) ? &str[0] : "";

	// GetVersion from registry which is more relayable for Windows 10
	GetVersionReg(&rOS_version);

	// GetVersion from a file which is needed to get the build number
	GetVersionFile(&fOS_version);

	// Choose whichever version is higher
	// Newer OS's report older version numbers for compatibility
	// This allows function to get the proper OS version number
	if (rOS_version.dwMajorVersion > oOS_version.dwMajorVersion)
	{
		oOS_version.dwMajorVersion = rOS_version.dwMajorVersion;
		oOS_version.dwMinorVersion = rOS_version.dwMinorVersion;
		ServicePack = "";
	}
	if (fOS_version.dwMajorVersion > oOS_version.dwMajorVersion)
	{
		oOS_version.dwMajorVersion = fOS_version.dwMajorVersion;
		oOS_version.dwMinorVersion = fOS_version.dwMinorVersion;
		ServicePack = "";
	}
	// The file almost always has the right build number
	if (fOS_version.dwBuildNumber != 0)
	{
		oOS_version.dwBuildNumber = fOS_version.dwBuildNumber;
	}

	// Get OS string name
	char sOSName[MAX_PATH];
	GetProductNameFromRegistry(sOSName, MAX_PATH);

	// Get bitness (32bit vs 64bit)
	SYSTEM_INFO SystemInfo;
	GetNativeSystemInfo(&SystemInfo);

	// Log operating system version and type from registry
	Log() << sOSName << ((SystemInfo.wProcessorArchitecture == 9) ? " 64-bit" : "") << " (" << oOS_version.dwMajorVersion << "." << oOS_version.dwMinorVersion << "." << oOS_version.dwBuildNumber << ")" << ServicePack;

	// Get OS string name from version
	GetProductNameFromVersion(oOS_version, sOSName, MAX_PATH);

	// Log operating system version and type from version
	Log() << sOSName << ((SystemInfo.wProcessorArchitecture == 9) ? " 64-bit" : "") << " (" << oOS_version.dwMajorVersion << "." << oOS_version.dwMinorVersion << "." << oOS_version.dwBuildNumber << ")" << ServicePack;
}

// Log video card type
void Logging::LogVideoCard()
{
	if (!EnableLogging)
	{
		return;
	}

	DISPLAY_DEVICE DispDev;
	ZeroMemory(&DispDev, sizeof(DispDev));
	DispDev.cb = sizeof(DispDev);
	DispDev.StateFlags = DISPLAY_DEVICE_PRIMARY_DEVICE;
	DWORD nDeviceIndex = 0;
	EnumDisplayDevices(NULL, nDeviceIndex, &DispDev, 0);
	Log() << DispDev.DeviceString;
}

// Get process parent PID
DWORD Logging::GetParentProcessId()
{
	DWORD currentPid = GetCurrentProcessId();
	DWORD parentPid = 0;

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return 0;

	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(entry);

	if (Process32First(snapshot, &entry))
	{
		do
		{
			if (entry.th32ProcessID == currentPid)
			{
				parentPid = entry.th32ParentProcessID;
				break;
			}
		} while (Process32Next(snapshot, &entry));
	}

	CloseHandle(snapshot);
	return parentPid;
}

// Check if process name matches the requested PID
bool Logging::CheckProcessNameFromPID(DWORD pid, char *name)
{
	// Open process handle
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, false, pid);
	if (!hProcess)
	{
		return false;
	}

	// Get process path
	char pidname[MAX_PATH] = {};
	DWORD size = NewGetProcessImageFileName(hProcess, (LPSTR)&pidname, MAX_PATH);
	if (!size)
	{
		return false;
	}

	// Get process name
	char* p_wName = strrchr(pidname, '\\');
	if (!p_wName || !(p_wName > pidname && p_wName < pidname + MAX_PATH))
	{
		return false;
	}
	p_wName++;

	// Check if name matches
	if (!_stricmp(name, p_wName))
	{
		return true;
	}

	return false;
}

// Check each parent path
bool Logging::CheckEachParentFolder(char *file, char *path)
{
	if (!file || !path)
	{
		return false;
	}

	// Predefined variables
	std::string separator("\\");
	char name[MAX_PATH];
	strcpy_s(name, MAX_PATH, path);

	// Check each parent path
	do {
		char* p_wName = strrchr(name, '\\');
		if (!p_wName || !(p_wName > name && p_wName < name + MAX_PATH))
		{
			return false;
		}
		*p_wName = '\0';

		// Check if file exists in the path
		if (PathFileExistsA(std::string(name + separator + file).c_str()))
		{
			return true;
		}
	} while (true);

	return false;
}

// Log if game is a GOG game
void Logging::LogGOGGameType()
{
	// Get process path
	char name[MAX_PATH] = { 0 };
	GetModuleFileNameA(nullptr, name, MAX_PATH);

	// Check if game is a GOG game
	if (GetModuleHandleA("goggame.dll") || CheckEachParentFolder("goggame.dll", name))
	{
		Log() << "Good Old Games (GOG) game detected!";
		return;
	}
}

// Log if game is a Steam game
void Logging::LogSteamGameType()
{
	// Get process path
	char name[MAX_PATH] = { 0 };
	GetModuleFileNameA(nullptr, name, MAX_PATH);

	// Check if game is a Steam game
	if (GetModuleHandleA("steam_api.dll") || CheckEachParentFolder("steam_api.dll", name) || CheckProcessNameFromPID(GetParentProcessId(), "steam.exe"))
	{
		Log() << "Steam game detected!";
		return;
	}
}

// Log if game type if found (GOG or Steam)
void Logging::LogGameType()
{
	LogGOGGameType();
	LogSteamGameType();
}

// Log environment variable
void Logging::LogEnvironmentVariable(const char* var)
{
	const DWORD size = GetEnvironmentVariableA(var, nullptr, 0);
	std::string value(size, 0);
	if (!value.empty())
	{
		GetEnvironmentVariableA(var, &value.front(), size);
		value.pop_back();
	}
	Log() << "Environment variable " << var << " = \"" << value.c_str() << '"';
}

// Log compatibility layer
void Logging::LogCompatLayer()
{
	LogEnvironmentVariable("__COMPAT_LAYER");
}
