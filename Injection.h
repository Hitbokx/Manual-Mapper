#pragma once

#include <iostream>
#include <Windows.h>
#include <fstream>
#include <TlHelp32.h>

using f_LoadLibraryA = HINSTANCE( WINAPI* )(const char* lpLibFileName);
using f_GetProcAddress = UINT_PTR( WINAPI* )(HINSTANCE hModule, const char* lpProcName);
using f_DLL_ENTRY_POINT = BOOL( WINAPI* )(void* hDll, DWORD dwReason, void* pReserved);

struct MANUAL_MAPPING_DATA
{
	f_LoadLibraryA pLoadLibraryA{};
	f_GetProcAddress pGetProcAddress{};
	HINSTANCE hMod{};
};

bool ManualMap( HANDLE hProc, const char* szDllPath );