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
* Created from source code found in DDrawCompat v0.5.4
* https://github.com/narzoul/DDrawCompat
*/

#include "Hook.h"
#include <sstream>
#include <stdio.h>
#include <psapi.h>

// Get pointer for function name from binary file in memory
FARPROC Hook::GetProcAddress(HMODULE hModule, LPCSTR functionName)
{
	if (!hModule || !functionName)
	{
		Logging::LogFormat(__FUNCTION__ " Invalid module or function name.");
		return nullptr;
	}

	// Use Windows' built-in function as fallback
	FARPROC fallback = ::GetProcAddress(hModule, functionName);
	if (!fallback)
	{
		if (HIWORD(functionName) == 0)
		{
			Logging::LogFormat(__FUNCTION__ " Windows API couldn't find ordinal %u", LOWORD(functionName));
		}
		else
		{
			Logging::LogFormat(__FUNCTION__ " Windows API couldn't find %s", functionName);
		}
	}

	__try
	{
		auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(hModule);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		{
			Logging::LogFormat(__FUNCTION__ " Not a valid DOS header.");
			return fallback;
		}

		auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
			reinterpret_cast<const BYTE*>(hModule) + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
		{
			Logging::LogFormat(__FUNCTION__ " Not a valid NT header.");
			return fallback;
		}

		const IMAGE_DATA_DIRECTORY& exportDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		if (!exportDir.VirtualAddress || !exportDir.Size)
		{
			Logging::LogFormat(__FUNCTION__ " No export directory present.");
			return fallback;
		}

		auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
			reinterpret_cast<const BYTE*>(hModule) + exportDir.VirtualAddress);

		const DWORD* funcAddrs = reinterpret_cast<const DWORD*>(
			reinterpret_cast<const BYTE*>(hModule) + exports->AddressOfFunctions);
		const DWORD* nameAddrs = reinterpret_cast<const DWORD*>(
			reinterpret_cast<const BYTE*>(hModule) + exports->AddressOfNames);
		const WORD* ordinals = reinterpret_cast<const WORD*>(
			reinterpret_cast<const BYTE*>(hModule) + exports->AddressOfNameOrdinals);

		// Handle ordinal lookup
		if (HIWORD(functionName) == 0)
		{
			WORD ordinal = LOWORD(functionName);
			WORD base = static_cast<WORD>(exports->Base);

			if (ordinal < base || (ordinal - base) >= (int)exports->NumberOfFunctions)
			{
				Logging::LogFormat(__FUNCTION__ " Ordinal %u out of range (Base=%u Count=%u)", ordinal, base, exports->NumberOfFunctions);
				return fallback;
			}

			DWORD rva = funcAddrs[ordinal - base];
			return reinterpret_cast<FARPROC>(reinterpret_cast<const BYTE*>(hModule) + rva);
		}
		// Handle name lookup
		else
		{
			for (DWORD i = 0; i < exports->NumberOfNames; ++i)
			{
				const char* name = reinterpret_cast<const char*>(
					reinterpret_cast<const BYTE*>(hModule) + nameAddrs[i]);

				if (strcmp(functionName, name) == 0)
				{
					WORD ordinal = ordinals[i];
					DWORD rva = funcAddrs[ordinal];
					return reinterpret_cast<FARPROC>(reinterpret_cast<const BYTE*>(hModule) + rva);
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Logging::LogFormat(__FUNCTION__ " Exception caught while parsing export table. Code=0x%08X", GetExceptionCode());
	}

	if (HIWORD(functionName) == 0)
	{
		Logging::LogFormat(__FUNCTION__ " Ordinal %u not found in export table.", LOWORD(functionName));
	}
	else
	{
		Logging::LogFormat(__FUNCTION__ " %s not found in export table.", functionName);
	}

	return fallback;
}

std::string Hook::funcPtrToStr(const void* funcPtr)
{
	std::ostringstream oss;
	HMODULE module = nullptr;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		static_cast<const char*>(funcPtr), &module);
	if (module)
	{
		char path[MAX_PATH] = {};
		GetModuleFileNameA(module, path, MAX_PATH);
		oss << path << "+0x" << std::hex << reinterpret_cast<DWORD>(funcPtr) - reinterpret_cast<DWORD>(module) << std::dec;
	}
	else
	{
		oss << funcPtr;
	}
	return oss.str();
}

// Get function name by ordinal from binary file
bool Hook::CheckExportAddress(HMODULE hModule, void* AddressCheck)
{
	if (!hModule)
	{
		Logging::LogFormat(__FUNCTION__ " NULL module.");
		return false;
	}

#ifdef _DEBUG
	Logging::LogFormat(__FUNCTION__ " Checking address %p.", AddressCheck);
#endif

	__try {
		PIMAGE_DOS_HEADER pIDH = (PIMAGE_DOS_HEADER)hModule;

		if (pIDH->e_magic != IMAGE_DOS_SIGNATURE)
		{
			Logging::LogFormat(__FUNCTION__ " Error: Module is not IMAGE_DOS_SIGNATURE.");
			return false;
		}

		PIMAGE_NT_HEADERS pINH = (PIMAGE_NT_HEADERS)((LPBYTE)hModule + pIDH->e_lfanew);

		if (pINH->Signature != IMAGE_NT_SIGNATURE)
		{
			Logging::LogFormat(__FUNCTION__ " Error: Module is not IMAGE_NT_SIGNATURE.");
			return false;
		}

		if (pINH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0 ||
			pINH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
		{
			Logging::LogFormat(__FUNCTION__ " Error: Could not get VirtualAddress.");
			return false;
		}

		PIMAGE_EXPORT_DIRECTORY pIED = (PIMAGE_EXPORT_DIRECTORY)((LPBYTE)hModule +
			pINH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

		if (pIED->NumberOfFunctions == 0)
		{
			Logging::LogFormat(__FUNCTION__ " Error: No export functions in the module.");
			return false;
		}

		PDWORD Address = (PDWORD)((LPBYTE)hModule + pIED->AddressOfFunctions);
		PDWORD Name = (PDWORD)((LPBYTE)hModule + pIED->AddressOfNames);
		PWORD Ordinals = (PWORD)((LPBYTE)hModule + pIED->AddressOfNameOrdinals);

		for (DWORD i = 0; i < pIED->NumberOfNames; i++)
		{
			if (::GetProcAddress(hModule, (char*)hModule + Name[i]) == AddressCheck || (void*)((DWORD)Address[Ordinals[i]] + (DWORD)hModule) == AddressCheck)
			{
				return true;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		DWORD ErrorCode = GetExceptionCode();
		Logging::LogFormat(__FUNCTION__ " Error: Exception caught: %x", ErrorCode);
	}

	// Exit function
	return false;
}

// Get pointer for function name from binary file
HMODULE Hook::GetModuleHandle(char* ProcName)
{
	DWORD processID = GetCurrentProcessId();
	HMODULE hMods[1024];
	HANDLE hProcess;
	DWORD cbNeeded;

	if (!ProcName)
	{
		Logging::Log() << __FUNCTION__ << " Error: NULL process name.";
		return nullptr;
	}

#ifdef _DEBUG
	Logging::Log() << __FUNCTION__ << " Searching for " << ProcName << ".";
#endif

	// Get a handle to the process.
	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
	if (!hProcess)
	{
		Logging::Log() << __FUNCTION__ << " Error: Could not open process.";
		return nullptr;
	}

	// Get a list of all the modules in this process.
	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for (UINT i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			char szModName[MAX_PATH];

			// Get the full path to the module's file.
			if (GetModuleFileNameExA(hProcess, hMods[i], szModName,
				sizeof(szModName) / sizeof(char)))
			{
				// Check the module name.
				if (!_stricmp(ProcName, szModName))
				{
					// Release the handle to the process.
					CloseHandle(hProcess);

					// Return module handle
					return hMods[i];
				}
			}
		}
	}

	// Release the handle to the process.
	CloseHandle(hProcess);

	// Exit function
	Logging::Log() << __FUNCTION__ << " Error: Could not file module " << ProcName << ".";
	return nullptr;
}
