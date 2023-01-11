/*
   pl_os_win32.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>  // file api
#include <stdlib.h> // malloc
#include "pl_os.h"
#include "pl_memory.h"

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert((x))
#endif


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib") // winsock2 library

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct
{
    HMODULE   tHandle;
    FILETIME  tLastWriteTime;
} plWin32SharedLibrary;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static FILETIME
pl__get_last_write_time(const char* pcFilename)
{
    FILETIME tLastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA tData = {0};
    if(GetFileAttributesExA(pcFilename, GetFileExInfoStandard, &tData))
        tLastWriteTime = tData.ftLastWriteTime;
    
    return tLastWriteTime;
}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

void
pl_read_file(const char* pcFile, unsigned* puSizeIn, char* pcBuffer, const char* pcMode)
{
    PL_ASSERT(puSizeIn);

    FILE* ptDataFile = fopen(pcFile, pcMode);
    unsigned uSize = 0u;

    if (ptDataFile == NULL)
    {
        PL_ASSERT(false && "File not found.");
        *puSizeIn = 0u;
        return;
    }

    // obtain file size
    fseek(ptDataFile, 0, SEEK_END);
    uSize = ftell(ptDataFile);
    fseek(ptDataFile, 0, SEEK_SET);

    if(pcBuffer == NULL)
    {
        *puSizeIn = uSize;
        fclose(ptDataFile);
        return;
    }

    // copy the file into the buffer:
    size_t szResult = fread(pcBuffer, sizeof(char), uSize, ptDataFile);
    if (szResult != uSize)
    {
        if (feof(ptDataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(ptDataFile)) {
            perror("Error reading test.bin");
        }
        PL_ASSERT(false && "File not read.");
    }

    fclose(ptDataFile);
}

void
pl_copy_file(const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer)
{
    CopyFile(pcSource, pcDestination, FALSE);
}

void
pl_create_udp_socket(plSocket* ptSocketOut, bool bNonBlocking)
{

    UINT_PTR tWin32Socket = 0;

    // create socket
    if((tWin32Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d\n", WSAGetLastError());
        PL_ASSERT(false && "Could not create socket");
    }

    // enable non-blocking
    if(bNonBlocking)
    {
        u_long uMode = 1;
        ioctlsocket(tWin32Socket, FIONBIO, &uMode);
    }

    ptSocketOut->_pPlatformData = (void*)tWin32Socket;
}

void
pl_bind_udp_socket(plSocket* ptSocket, int iPort)
{
    ptSocket->iPort = iPort;
    PL_ASSERT(ptSocket->_pPlatformData && "Socket not created yet");
    UINT_PTR tWin32Socket = (UINT_PTR)ptSocket->_pPlatformData;
    
    // prepare sockaddr_in struct
    struct sockaddr_in tServer = {
        .sin_family      = AF_INET,
        .sin_port        = htons((u_short)iPort),
        .sin_addr.s_addr = INADDR_ANY
    };

    // bind socket
    if(bind(tWin32Socket, (struct sockaddr* )&tServer, sizeof(tServer)) == SOCKET_ERROR)
    {
        printf("Bind socket failed with error code : %d\n", WSAGetLastError());
        PL_ASSERT(false && "Socket error");
    }
}

bool
pl_send_udp_data(plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize)
{
    PL_ASSERT(ptFromSocket->_pPlatformData && "Socket not created yet");
    UINT_PTR tWin32Socket = (UINT_PTR)ptFromSocket->_pPlatformData;

    struct sockaddr_in tDestSocket = {
        .sin_family           = AF_INET,
        .sin_port             = htons((u_short)iDestPort),
        .sin_addr.S_un.S_addr = inet_addr(pcDestIP)
    };
    static const size_t szLen = sizeof(tDestSocket);

    // send
    if(sendto(tWin32Socket, (const char*)pData, (int)szSize, 0, (struct sockaddr*)&tDestSocket, (int)szLen) == SOCKET_ERROR)
    {
        printf("sendto() failed with error code : %d\n", WSAGetLastError());
        PL_ASSERT(false && "Socket error");
        return false;
    }

    return true;
}

bool
pl_get_udp_data(plSocket* ptSocket, void* pData, size_t szSize)
{
    PL_ASSERT(ptSocket->_pPlatformData && "Socket not created yet");
    UINT_PTR tWin32Socket = (UINT_PTR)ptSocket->_pPlatformData;

    struct sockaddr_in tSiOther = {0};
    static int iSLen = (int)sizeof(tSiOther);
    memset(pData, 0, szSize);
    int iRecvLen = recvfrom(tWin32Socket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tSiOther, &iSLen);

    if(iRecvLen == SOCKET_ERROR)
    {
        const int iLastError = WSAGetLastError();
        if(iLastError != WSAEWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
            PL_ASSERT(false && "Socket error");
            return false;
        }
    }

    return iRecvLen > 0;
}

bool
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
    plWin32SharedLibrary* win32Library = ptLibrary->_pPlatformData;
    return CompareFileTime(&newWriteTime, &win32Library->tLastWriteTime) != 0;
}

bool
pl_load_library(plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile)
{
    if(ptLibrary->acPath[0] == 0)             strncpy(ptLibrary->acPath, pcName, PL_MAX_NAME_LENGTH);
    if(ptLibrary->acTransitionalName[0] == 0) strncpy(ptLibrary->acTransitionalName, pcTransitionalName, PL_MAX_NAME_LENGTH);
    if(ptLibrary->acLockFile[0] == 0)         strncpy(ptLibrary->acLockFile, pcLockFile, PL_MAX_NAME_LENGTH);
    ptLibrary->bValid = false;

    if(ptLibrary->_pPlatformData == NULL)
        ptLibrary->_pPlatformData = malloc(sizeof(plWin32SharedLibrary));
    plWin32SharedLibrary* ptWin32Library = ptLibrary->_pPlatformData;

    WIN32_FILE_ATTRIBUTE_DATA tIgnored;
    if(!GetFileAttributesExA(ptLibrary->acLockFile, GetFileExInfoStandard, &tIgnored))  // lock file gone
    {
        char acTemporaryName[2024] = {0};
        ptWin32Library->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dll");
        if(++ptLibrary->uTempIndex >= 1024)
        {
            ptLibrary->uTempIndex = 0;
        }
        pl_copy_file(ptLibrary->acPath, acTemporaryName, NULL, NULL);

        ptWin32Library->tHandle = NULL;
        ptWin32Library->tHandle = LoadLibraryA(acTemporaryName);
        if(ptWin32Library->tHandle)
            ptLibrary->bValid = true;
    }

    return ptLibrary->bValid;
}

void
pl_reload_library(plSharedLibrary* ptLibrary)
{
    ptLibrary->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl_load_library(ptLibrary, ptLibrary->acPath, ptLibrary->acTransitionalName, ptLibrary->acLockFile))
            break;
        pl_sleep(100);
    }
}

void*
pl_load_library_function(plSharedLibrary* ptLibrary, const char* name)
{
    PL_ASSERT(ptLibrary->bValid && "Library not valid");
    void* pLoadedFunction = NULL;
    if(ptLibrary->bValid)
    {
        plWin32SharedLibrary* ptWin32Library = ptLibrary->_pPlatformData;
        pLoadedFunction = (void*)GetProcAddress(ptWin32Library->tHandle, name);
    }
    return pLoadedFunction;
}

int
pl_sleep(uint32_t uMillisec)
{
    Sleep((long)uMillisec);
    return 0;
}
