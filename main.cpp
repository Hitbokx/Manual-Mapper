#include "Injection.h"

const char szDllFilePath[]{ "E:\\DEV\\VS Projects\\testdll\\Debug\\testdll.dll" };
const char szProc[]{ "calc.exe" };

int main( )
{
	PROCESSENTRY32 PE32{ 0 };
	PE32.dwSize = sizeof( PE32 );

	HANDLE hSnap{ CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) };
	if ( hSnap == INVALID_HANDLE_VALUE )
	{
		printf( "CreateToolhelp32Snapshot failed: 0x%x\n", GetLastError( ) );
		system( "PAUSE" );
		return 0;
	}

	DWORD PID{};
	BOOL bRet{ Process32First( hSnap, &PE32 ) };
	while( bRet )
	{
		if ( !strcmp( szProc, PE32.szExeFile ) )
		{
			PID = PE32.th32ProcessID;
		}
		bRet = Process32Next( hSnap, &PE32 );
	}

	CloseHandle( hSnap );

	HANDLE hProc{ OpenProcess( PROCESS_ALL_ACCESS, FALSE, PID ) };
	if ( !hProc )
	{
		printf( "OpenProcess failed: 0x%x\n", GetLastError( ) );
		system( "PAUSE" );
		return 0;
	}

	if ( !ManualMap( hProc, szDllFilePath ) )
	{
		CloseHandle( hProc );
		printf( "Uh-oh, Sometihing went wrong FoolsBadMan\n" );
		system( "PAUSE" );
		return 0;
	}

	CloseHandle( hProc );
	return 0;
}