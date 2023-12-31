#include "Injection.h"

void __stdcall ShellCode( MANUAL_MAPPING_DATA* pData );

bool ManualMap( HANDLE hProc, const char* szDllPath )
{
    PBYTE pSrcData{ nullptr };
    PIMAGE_NT_HEADERS pOldNtHader{ nullptr };
    PIMAGE_OPTIONAL_HEADER pOldOptHeader{ nullptr };
    PIMAGE_FILE_HEADER pOldFileHeader{ nullptr };
    PBYTE pTargetBase{ nullptr };

    // Check if the file exists ( correct dll path is entered )
    DWORD dwCheck{ 0 };
    if ( GetFileAttributes( szDllPath ) == INVALID_FILE_ATTRIBUTES )
    {
        printf( "File doesn't exist\n" );
        return false;
    }

    // Opening the dll file
    std::ifstream File( szDllPath, std::ios::binary | std::ios::ate );
    if ( File.fail( ) )
    {
        printf( "Opening the file failed: %x\n", (DWORD)File.rdstate( ) );
        File.close( );
        return false;
    }

    // tell the file size in bytes
    auto FileSize{ File.tellg( ) };
    // Check according to the size if the file is a dll
    if ( FileSize < 0x1000 )
    {
        printf( "FileSize is invalid.\n" );
        File.close( );
        return false;
    }

    // Allocate some memory in heap of size 'FileSize'
    pSrcData = new BYTE[static_cast<UINT_PTR>(FileSize)];
    // if FileSize is too big, allocation may fail.
    if ( !pSrcData )
    {
        printf( "Memory allocation failed.\n" );
        File.close( );
        return false;
    }

    // Currently at the file end, go to the beginning of the file using seekg
    File.seekg( 0, std::ios::beg );

    // Read the whole file into memory i.e. copy the file contents into the pSrcData
    File.read( reinterpret_cast<char*>(pSrcData), FileSize );
    File.close( );

    // Check if the file is a PE file
    if ( reinterpret_cast<PIMAGE_DOS_HEADER>(pSrcData)->e_magic != 0x5A4D )
    {
        printf( "Invalid file.\n" );
        delete[] pSrcData;
        return false;
    }

    pOldNtHader = reinterpret_cast<PIMAGE_NT_HEADERS>(pSrcData + reinterpret_cast<PIMAGE_DOS_HEADER>(pSrcData)->e_lfanew);
    pOldOptHeader = &pOldNtHader->OptionalHeader;
    pOldFileHeader = &pOldNtHader->FileHeader;

#ifdef _WIN64

    // Check for a valid x64 file 
    if ( pOldFileHeader->Machine != IMAGE_FILE_MACHINE_AMD64 )
    {
        printf( "Invalid platform.\n" );
        delete[] pSrcData;
        return false;
    }

#else

    if ( pOldFileHeader->Machine != IMAGE_FILE_MACHINE_I386 )
    {
        printf( "Invalid platform.\n" );
        delete[] pSrcData;
        return false;
    }

#endif // _WIN64

    pTargetBase = reinterpret_cast<BYTE*>(VirtualAllocEx( hProc, reinterpret_cast<void*>(pOldOptHeader->ImageBase), pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE ));
    if ( !pTargetBase )
    {
        pTargetBase = reinterpret_cast<BYTE*>(VirtualAllocEx( hProc, nullptr, pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE ));
        if ( !pTargetBase )
        {
            printf( "Memory allocation failed (ex) 0x%X\n", GetLastError( ) );
            delete[] pSrcData;
            return false;
        }
    }

    MANUAL_MAPPING_DATA data{0};
    data.pLoadLibraryA = LoadLibraryA;
    data.pGetProcAddress = reinterpret_cast<f_GetProcAddress>(GetProcAddress);

    // Map the sections
    PIMAGE_SECTION_HEADER pSectionHeader{ IMAGE_FIRST_SECTION( pOldNtHader ) };
    for ( size_t i{ 0 }; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader )
    {
        if ( pSectionHeader->SizeOfRawData )
        {
            if ( !WriteProcessMemory( hProc, pTargetBase + pSectionHeader->VirtualAddress, pSrcData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData, nullptr ) )
            {
                printf( "Can't map sections. 0x%x\n", GetLastError( ) );
                delete[] pSrcData;
                VirtualFreeEx( hProc, pTargetBase, NULL, MEM_RELEASE );
                return false;
            }
        }
    }

    memcpy( pSrcData, &data, sizeof( data ) );
    WriteProcessMemory( hProc, pTargetBase, pSrcData, 0x1000, nullptr );

    delete[] pSrcData;

    void* pShellCode{ VirtualAllocEx( hProc, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE ) };
    if ( !pShellCode )
    {
        printf( "Memory allocation (ex)2 failed. 0x%x\n", GetLastError( ) );
        VirtualFreeEx( hProc, pTargetBase, NULL, MEM_RELEASE );
        return false;
    }
    
    WriteProcessMemory( hProc, pShellCode, ShellCode, 0x1000, nullptr );

    HANDLE hThread{ CreateRemoteThread( hProc, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pShellCode), pTargetBase, 0, nullptr ) };
    if ( hThread == INVALID_HANDLE_VALUE )
    {
        printf( "Thread creation failed. 0x%x\n", GetLastError( ) );
        VirtualFreeEx( hProc, pTargetBase, NULL, MEM_RELEASE );
        VirtualFreeEx( hProc, pShellCode, NULL, MEM_RELEASE );
        
        return false;
    }

    CloseHandle( hThread );

    HINSTANCE hCheck{ NULL };
    while ( !hCheck )
    {
        MANUAL_MAPPING_DATA dataChecked{ 0 };
        ReadProcessMemory( hProc, pTargetBase, &dataChecked, sizeof( dataChecked ), nullptr );
        hCheck = dataChecked.hMod;
        Sleep( 10 );
    }

    VirtualFreeEx( hProc, pShellCode, NULL, MEM_RELEASE );

    return true;
}

#define RELOC_FLAG32(RelInfo) ((RelInfo >> 0x0C) == IMAGE_REL_BASED_HIGHLOW)
#define RELOC_FLAG64(RelInfo) ((RelInfo >> 0x0C) == IMAGE_REL_BASED_DIR64)

#ifdef _WIN64

#define RELOC_FLAG RELOC_FLAG64

#else

#define RELOC_FLAG RELOC_FLAG32

#endif // _WIN64

UINT __forceinline _strlenA( const char* szString )
{
    UINT ret{ 0 };
    for ( ; *szString; ++szString, ++ret );
    return ret;
}

void __stdcall ShellCode( MANUAL_MAPPING_DATA* pData )
{
    if ( !pData )
        return;

    BYTE* pBase{ reinterpret_cast<BYTE*>(pData) };
    PIMAGE_OPTIONAL_HEADER pOptionalHeader{ &reinterpret_cast<PIMAGE_NT_HEADERS>(pBase + reinterpret_cast<PIMAGE_DOS_HEADER>(pData)->e_lfanew)->OptionalHeader };

    auto _LoadLibraryA{ pData->pLoadLibraryA };
    auto _GetProcAddress{ pData->pGetProcAddress };
    auto _DllMain{ reinterpret_cast<f_DLL_ENTRY_POINT>(pBase + pOptionalHeader->AddressOfEntryPoint) };

    BYTE* locationData{ pBase - pOptionalHeader->ImageBase };
    if ( locationData )
    {
        if ( !pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size )
            return;

        PIMAGE_BASE_RELOCATION pRelocData{ reinterpret_cast <PIMAGE_BASE_RELOCATION>(pBase + pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress) };

        while ( pRelocData->VirtualAddress )
        {
            size_t numEntries{ (pRelocData->SizeOfBlock - sizeof( IMAGE_BASE_RELOCATION )) / sizeof( WORD ) };
            WORD* pRelativeInfo{ reinterpret_cast<PWORD>(pRelocData + 1) };
            for ( size_t i{ 0 }; i != numEntries; ++i, ++pRelativeInfo )
            {
                if ( RELOC_FLAG( *pRelativeInfo ) )
                {
                    PUINT_PTR pPatch{ reinterpret_cast<PUINT_PTR>(pBase + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF)) };

                    *pPatch += reinterpret_cast<UINT_PTR>(locationData);
                }
            }

            pRelocData = reinterpret_cast<PIMAGE_BASE_RELOCATION>(reinterpret_cast<PBYTE>(pRelocData) + pRelocData->SizeOfBlock);
        }
    }

    if ( pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size )
    {
        PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor{ reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(pBase + pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) };
        while ( pImportDescriptor->Name )
        {
            // Name of the module from which names are taken
            char* modName{ reinterpret_cast<char*>(pBase + pImportDescriptor->Name) };
            HINSTANCE hDll{ _LoadLibraryA( modName ) };

            PULONG_PTR pNameThunkRef{ reinterpret_cast<PULONG_PTR>(pBase + pImportDescriptor->OriginalFirstThunk) };
            PULONG_PTR pFuncThunkRef{ reinterpret_cast<PULONG_PTR>(pBase + pImportDescriptor->FirstThunk) };

            if ( !pNameThunkRef )
                pNameThunkRef = pFuncThunkRef;

            for ( ; *pNameThunkRef; ++pNameThunkRef, ++pFuncThunkRef )
            {
                if ( IMAGE_SNAP_BY_ORDINAL( *pNameThunkRef ) )
                {
                    *pFuncThunkRef = _GetProcAddress( hDll, reinterpret_cast<char*>(*pNameThunkRef & 0xFFFF) );
                }
                else
                {
                    PIMAGE_IMPORT_BY_NAME pImport{ reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(pBase + (*pNameThunkRef)) };
                    *pFuncThunkRef= _GetProcAddress( hDll, pImport->Name );
                }
            }
            ++pImportDescriptor;
        }
    }

    if ( pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size )
    {
        PIMAGE_TLS_DIRECTORY pTLSDirectory{ reinterpret_cast<PIMAGE_TLS_DIRECTORY>(pBase + pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress) };
        PIMAGE_TLS_CALLBACK* pTLSCallbacks{ reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLSDirectory->AddressOfCallBacks) };

        for ( ; pTLSCallbacks && *pTLSCallbacks; ++pTLSCallbacks )
            (*pTLSCallbacks)(pBase, DLL_PROCESS_ATTACH, nullptr);
    }

    _DllMain( pBase, DLL_PROCESS_ATTACH, nullptr );

    pData->hMod = reinterpret_cast<HINSTANCE>(pBase);
}