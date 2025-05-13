/*
   pl_platform_win32_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// #include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // memset
#include "pl.h"

// extensions
#include "pl_platform_ext.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sysinfoapi.h> // page size
#include <winsock2.h> // sockets
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

static const plMemoryI*  gptMemory = NULL;
#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)


//-----------------------------------------------------------------------------
// [SECTION] file ext
//-----------------------------------------------------------------------------

bool
pl_file_exists(const char* pcFile)
{
    FILE* ptDataFile = fopen(pcFile, "r");
    
    if(ptDataFile)
    {
        fclose(ptDataFile);
        return true;
    }
    return false;
}

plFileResult
pl_file_delete(const char* pcFile)
{
    BOOL bResult = DeleteFile(pcFile);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
}

plFileResult
pl_binary_read_file(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
{

    if(pszSizeIn == NULL)
        return PL_FILE_RESULT_FAIL;

    FILE* ptDataFile = fopen(pcFile, "rb");
    size_t uSize = 0u;

    if (ptDataFile == NULL)
    {
        *pszSizeIn = 0u;
        return PL_FILE_RESULT_FAIL;
    }

    // obtain file size
    fseek(ptDataFile, 0, SEEK_END);
    uSize = ftell(ptDataFile);
    
    if(pcBuffer == NULL)
    {
        *pszSizeIn = uSize;
        fclose(ptDataFile);
        return PL_FILE_RESULT_SUCCESS;
    }
    fseek(ptDataFile, 0, SEEK_SET);

    // copy the file into the buffer:
    size_t szResult = fread(pcBuffer, sizeof(char), uSize, ptDataFile);
    if (szResult != uSize)
    {
        if (feof(ptDataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(ptDataFile)) {
            perror("Error reading test.bin");
        }
        return PL_FILE_RESULT_FAIL;
    }

    fclose(ptDataFile);
    return PL_FILE_RESULT_SUCCESS;
}

plFileResult
pl_binary_write_file(const char* pcFile, size_t szSize, uint8_t* pcBuffer)
{
    FILE* ptDataFile = fopen(pcFile, "wb");
    if (ptDataFile)
    {
        fwrite(pcBuffer, 1, szSize, ptDataFile);
        fclose(ptDataFile);
        return PL_FILE_RESULT_SUCCESS;
    }
    return PL_FILE_RESULT_FAIL;
}

plFileResult
pl_copy_file(const char* pcSource, const char* pcDestination)
{
    BOOL bResult = CopyFile(pcSource, pcDestination, FALSE);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
}

//-----------------------------------------------------------------------------
// [SECTION] atomics ext
//-----------------------------------------------------------------------------

typedef struct _plAtomicCounter
{
    int64_t ilValue;
} plAtomicCounter;

plAtomicsResult
pl_create_atomic_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = (plAtomicCounter*)_aligned_malloc(sizeof(plAtomicCounter), 8);
    (*ptCounter)->ilValue = ilValue;
    return PL_ATOMICS_RESULT_SUCCESS;
}

void
pl_destroy_atomic_counter(plAtomicCounter** ptCounter)
{
    _aligned_free((*ptCounter));
    (*ptCounter) = NULL;
}

void
pl_atomic_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    ptCounter->ilValue = ilValue;
}

int64_t
pl_atomic_load(plAtomicCounter* ptCounter)
{
    return ptCounter->ilValue;
}

bool
pl_atomic_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    return InterlockedCompareExchange64(&ptCounter->ilValue, ilDesiredValue, ilExpectedValue) == ilExpectedValue;
}

int64_t
pl_atomic_increment(plAtomicCounter* ptCounter)
{
    return InterlockedIncrement64(&ptCounter->ilValue);
}

int64_t
pl_atomic_decrement(plAtomicCounter* ptCounter)
{
    return InterlockedDecrement64(&ptCounter->ilValue);
}

//-----------------------------------------------------------------------------
// [SECTION] network ext
//-----------------------------------------------------------------------------

typedef struct _plNetworkAddress
{
    struct addrinfo* tInfo;
} plNetworkAddress;

typedef struct _plSocket
{
    SOCKET tSocket;
    bool     bInitialized;
    plSocketFlags tFlags;
} plSocket;

bool
pl_network_initialize(void)
{
    WSADATA tWsaData = {0};
    if(WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0)
    {
        printf("Failed to start winsock with error code: %d\n", WSAGetLastError());
        return false;
    }
    return true;
}

void
pl_network_cleanup(void)
{
    WSACleanup();
}

plNetworkResult
pl_create_address(const char* pcAddress, const char* pcService, plNetworkAddressFlags tFlags, plNetworkAddress** pptAddress)
{
    
    struct addrinfo tHints;
    memset(&tHints, 0, sizeof(tHints));
    tHints.ai_socktype = SOCK_DGRAM;

    if(tFlags & PL_NETWORK_ADDRESS_FLAGS_TCP)
        tHints.ai_socktype = SOCK_STREAM;

    if(pcAddress == NULL)
        tHints.ai_flags = AI_PASSIVE;

    if(tFlags & PL_NETWORK_ADDRESS_FLAGS_IPV4)
        tHints.ai_family = AF_INET;
    else if(tFlags & PL_NETWORK_ADDRESS_FLAGS_IPV6)
        tHints.ai_family = AF_INET6;

    struct addrinfo* tInfo = NULL;
    if(getaddrinfo(pcAddress, pcService, &tHints, &tInfo))
    {
        printf("Could not create address : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }

    *pptAddress = (plNetworkAddress*)PL_ALLOC(sizeof(plNetworkAddress));
    (*pptAddress)->tInfo = tInfo;
    return PL_NETWORK_RESULT_SUCCESS;
}

void
pl_destroy_address(plNetworkAddress** pptAddress)
{
    plNetworkAddress* ptAddress = *pptAddress;
    if(ptAddress == NULL)
        return;

    freeaddrinfo(ptAddress->tInfo);
    PL_FREE(ptAddress);
    *pptAddress = NULL;
}

void
pl_create_socket(plSocketFlags tFlags, plSocket** pptSocketOut)
{
    *pptSocketOut = (plSocket*)PL_ALLOC(sizeof(plSocket));
    plSocket* ptSocket = *pptSocketOut;
    ptSocket->bInitialized = false;
    ptSocket->tFlags = tFlags;
}

void
pl_destroy_socket(plSocket** pptSocket)
{
    plSocket* ptSocket = *pptSocket;

    if(ptSocket == NULL)
        return;

    closesocket(ptSocket->tSocket);

    PL_FREE(ptSocket);
    *pptSocket = NULL;
}

plNetworkResult
pl_send_socket_data_to(plSocket* ptFromSocket, plNetworkAddress* ptAddress, const void* pData, size_t szSize, size_t* pszSentSize)
{

    if(!ptFromSocket->bInitialized)
    {
        
        ptFromSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        if(ptFromSocket->tSocket == INVALID_SOCKET)
        {
            printf("Could not create socket : %d\n", WSAGetLastError());
            return 0;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            u_long uMode = 1;
            ioctlsocket(ptFromSocket->tSocket, FIONBIO, &uMode);
        }

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = sendto(ptFromSocket->tSocket, (const char*)pData, (int)szSize, 0, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);
    if(iResult == SOCKET_ERROR)
    {
        printf("sendto() failed with error code : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }

    if(pszSentSize)
        *pszSentSize = (size_t)iResult;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_bind_socket(plSocket* ptSocket, plNetworkAddress* ptAddress)
{
    if(!ptSocket->bInitialized)
    {
        
        ptSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        if(ptSocket->tSocket == INVALID_SOCKET)
        {
            printf("Could not create socket : %d\n", WSAGetLastError());
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            u_long uMode = 1;
            ioctlsocket(ptSocket->tSocket, FIONBIO, &uMode);
        }

        ptSocket->bInitialized = true;
    }

    // bind socket
    if(bind(ptSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen) == SOCKET_ERROR)
    {
        printf("Bind socket failed with error code : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_get_socket_data_from(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize, plSocketReceiverInfo* ptReceiverInfo)
{
    struct sockaddr_storage tClientAddress = {0};
    socklen_t tClientLen = sizeof(tClientAddress);

    int iRecvLen = recvfrom(ptSocket->tSocket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tClientAddress, &tClientLen);
   

    if(iRecvLen == SOCKET_ERROR)
    {
        const int iLastError = WSAGetLastError();
        if(iLastError != WSAEWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
            return PL_NETWORK_RESULT_FAIL;
        }
    }

    if(iRecvLen > 0)
    {
        if(ptReceiverInfo)
        {
            getnameinfo((struct sockaddr*)&tClientAddress, tClientLen,
                ptReceiverInfo->acAddressBuffer, 100,
                ptReceiverInfo->acServiceBuffer, 100,
                NI_NUMERICHOST | NI_NUMERICSERV);
        }
        if(pszRecievedSize)
            *pszRecievedSize = (size_t)iRecvLen;
    }

    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_connect_socket(plSocket* ptFromSocket, plNetworkAddress* ptAddress)
{

    if(!ptFromSocket->bInitialized)
    {
        
        ptFromSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        if(ptFromSocket->tSocket == INVALID_SOCKET)
        {
            printf("Could not create socket : %d\n", WSAGetLastError());
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            u_long uMode = 1;
            ioctlsocket(ptFromSocket->tSocket, FIONBIO, &uMode);
        }

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = connect(ptFromSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);
    if(iResult)
    {
        printf("connect() failed with error code : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }

    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_get_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize)
{
    int iBytesReceived = recv(ptSocket->tSocket, (char*)pData, (int)szSize, 0);
    if(iBytesReceived < 1)
    {
        return PL_NETWORK_RESULT_FAIL; // connection closed by peer
    }
    if(pszRecievedSize)
        *pszRecievedSize = (size_t)iBytesReceived;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_select_sockets(plSocket** ptSockets, bool* abSelectedSockets, uint32_t uSocketCount, uint32_t uTimeOutMilliSec)
{
    fd_set tReads;
    FD_ZERO(&tReads);
    for(uint32_t i = 0; i < uSocketCount; i++)
    {
        FD_SET(ptSockets[i]->tSocket, &tReads);
    }

    struct timeval tTimeout = {0};
    tTimeout.tv_sec = 0;
    tTimeout.tv_usec = (int)uTimeOutMilliSec * 1000;

    if(select(0, &tReads, NULL, NULL, &tTimeout) < 0)
    {
        printf("select socket failed with error code : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }

    for(uint32_t i = 0; i < uSocketCount; i++)
    {
        if(FD_ISSET(ptSockets[i]->tSocket, &tReads))
            abSelectedSockets[i] = true;
        else
            abSelectedSockets[i] = false;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_accept_socket(plSocket* ptSocket, plSocket** pptSocketOut)
{
    *pptSocketOut = NULL; 
    struct sockaddr_storage tClientAddress = {0};
    socklen_t tClientLen = sizeof(tClientAddress);
    SOCKET tSocketClient = accept(ptSocket->tSocket, (struct sockaddr*)&tClientAddress, &tClientLen);

    if(tSocketClient == INVALID_SOCKET)
        return PL_NETWORK_RESULT_FAIL;

    *pptSocketOut = (plSocket*)PL_ALLOC(sizeof(plSocket));
    plSocket* ptNewSocket = *pptSocketOut;
    ptNewSocket->bInitialized = true;
    ptNewSocket->tFlags = ptSocket->tFlags;
    ptNewSocket->tSocket = tSocketClient;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_listen_socket(plSocket* ptSocket)
{
    if(listen(ptSocket->tSocket, 10) < 0)
    {
        return PL_NETWORK_RESULT_FAIL;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_send_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszSentSize)
{
    int iResult = send(ptSocket->tSocket, (char*)pData, (int)szSize, 0);
    if(iResult == SOCKET_ERROR)
        return PL_NETWORK_RESULT_FAIL;
    if(pszSentSize)
        *pszSentSize = (size_t)iResult;
    return PL_NETWORK_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// [SECTION] threads ext
//-----------------------------------------------------------------------------

typedef struct _plThreadData
{
  plThreadProcedure ptProcedure;
  void*             pData;
} plThreadData;

typedef struct _plThread
{
    HANDLE        tHandle;
    plThreadData* ptData;
    uint64_t      uID;
} plThread;

typedef struct _plMutex
{
    HANDLE tHandle;
} plMutex;

typedef struct _plCriticalSection
{
    CRITICAL_SECTION tHandle;
} plCriticalSection;

typedef struct _plSemaphore
{
    HANDLE tHandle;
} plSemaphore;

typedef struct _plBarrier
{
    SYNCHRONIZATION_BARRIER tHandle;
} plBarrier;

typedef struct _plConditionVariable
{
    CONDITION_VARIABLE tHandle;
} plConditionVariable;

typedef struct _plThreadKey
{
    DWORD dwIndex;
} plThreadKey;

void
pl_sleep(uint32_t uMillisec)
{
    Sleep((long)uMillisec);
}

static DWORD 
thread_procedure(void* lpParam)
{
    plThreadData* ptData = (plThreadData*)lpParam;
    ptData->ptProcedure(ptData->pData);
    return 1;
}

static void
thread_yield(void)
{
    SwitchToThread();
}

plThreadResult
pl_create_thread(plThreadProcedure ptProcedure, void* pData, plThread** ppThreadOut)
{
    plThreadData* ptData = (plThreadData*)PL_ALLOC(sizeof(plThreadData));
    ptData->ptProcedure = ptProcedure;
    ptData->pData       = pData;

    HANDLE tHandle = CreateThread(0, 1024, thread_procedure, ptData, 0, NULL);
    if(tHandle)
    {
        DWORD tID = GetThreadId(tHandle);
        *ppThreadOut = (plThread*)PL_ALLOC(sizeof(plThread));
        (*ppThreadOut)->ptData = ptData;
        (*ppThreadOut)->tHandle = tHandle;
        (*ppThreadOut)->uID = (uint64_t)tID;
        return PL_THREAD_RESULT_SUCCESS;
    }
    PL_FREE(ptData);
    return PL_THREAD_RESULT_FAIL;
    
}

void
pl_join_thread(plThread* ptThread)
{
    WaitForSingleObject(ptThread->tHandle, INFINITE);
}

void
pl_destroy_thread(plThread** ppThread)
{
    pl_join_thread(*ppThread);
    CloseHandle((*ppThread)->tHandle);
    PL_FREE((*ppThread)->ptData);
    PL_FREE(*ppThread);
    *ppThread = NULL;
}

void
pl_yield_thread(void)
{
    thread_yield();
}

plThreadResult
pl_create_mutex(plMutex** ppMutexOut)
{
    HANDLE tHandle = CreateMutex(NULL, FALSE, NULL);
    if(tHandle)
    {
        (*ppMutexOut) = (plMutex*)malloc(sizeof(plMutex));
        // (*ppMutexOut)->tHandle = CreateMutex(NULL, FALSE, NULL);
        (*ppMutexOut)->tHandle = tHandle;
        return PL_THREAD_RESULT_SUCCESS;
    }
    return PL_THREAD_RESULT_FAIL;
}

void
pl_destroy_mutex(plMutex** ptMutex)
{
    CloseHandle((*ptMutex)->tHandle);
    free((*ptMutex));
    (*ptMutex) = NULL;
}

void
pl_lock_mutex(plMutex* ptMutex)
{
    DWORD dwWaitResult = WaitForSingleObject(ptMutex->tHandle, INFINITE);
    PL_ASSERT(dwWaitResult == WAIT_OBJECT_0);
}

void
pl_unlock_mutex(plMutex* ptMutex)
{
    if(!ReleaseMutex(ptMutex->tHandle))
    {
        printf("ReleaseMutex error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

plThreadResult
pl_create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    (*pptCriticalSectionOut) = (plCriticalSection*)PL_ALLOC(sizeof(plCriticalSection));
    InitializeCriticalSection(&(*pptCriticalSectionOut)->tHandle);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_destroy_critical_section(plCriticalSection** pptCriticalSection)
{
    DeleteCriticalSection(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    (*pptCriticalSection) = NULL;
}

void
pl_enter_critical_section(plCriticalSection* ptCriticalSection)
{
    EnterCriticalSection(&ptCriticalSection->tHandle);
}

void
pl_leave_critical_section(plCriticalSection* ptCriticalSection)
{
    LeaveCriticalSection(&ptCriticalSection->tHandle);
}

uint32_t
pl_get_hardware_thread_count(void)
{

    static uint32_t uThreadCount = 0;

    if(uThreadCount == 0)
    {
        DWORD dwLength = 0;
        GetLogicalProcessorInformation(NULL, &dwLength);
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION atInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)PL_ALLOC(dwLength);
        GetLogicalProcessorInformation(atInfo, &dwLength);
        uint32_t uEntryCount = dwLength / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for(uint32_t i = 0; i < uEntryCount; i++)
        {
            if(atInfo[i].Relationship == RelationProcessorCore)
                uThreadCount++;
        }
        PL_FREE(atInfo);
    }
    return uThreadCount;
}

plThreadResult
pl_create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    (*pptBarrierOut) = (plBarrier*)PL_ALLOC(sizeof(plBarrier));
    InitializeSynchronizationBarrier(&(*pptBarrierOut)->tHandle, uThreadCount, -1);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_destroy_barrier(plBarrier** pptBarrier)
{
    DeleteSynchronizationBarrier(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
}

void
pl_wait_on_barrier(plBarrier* ptBarrier)
{
    EnterSynchronizationBarrier(&ptBarrier->tHandle, 0);
}

plThreadResult
pl_create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    (*pptSemaphoreOut) = (plSemaphore*)PL_ALLOC(sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = CreateSemaphore(NULL, 0, uIntialCount, NULL);
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_destroy_semaphore(plSemaphore** pptSemaphore)
{
    CloseHandle((*pptSemaphore)->tHandle);
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
}

void
pl_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    WaitForSingleObject(ptSemaphore->tHandle, INFINITE);
}

bool
pl_try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    DWORD dwWaitResult = WaitForSingleObject(ptSemaphore->tHandle, 0);
    switch (dwWaitResult)
    {
        case WAIT_OBJECT_0: return true;
        case WAIT_TIMEOUT:  return false;
    }
    PL_ASSERT(false);
    return false;
}

void
pl_release_semaphore(plSemaphore* ptSemaphore)
{
    if (!ReleaseSemaphore( 
            ptSemaphore->tHandle,  // handle to semaphore
            1,            // increase count by one
            NULL) )       // not interested in previous count
    {
        printf("ReleaseSemaphore error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
}

plThreadResult
pl_allocate_thread_local_key(plThreadKey** pptKeyOut)
{
    *pptKeyOut = (plThreadKey*)PL_ALLOC(sizeof(plThreadKey));
    memset(*pptKeyOut, 0, sizeof(plThreadKey));
    (*pptKeyOut)->dwIndex = TlsAlloc();
    return PL_THREAD_RESULT_SUCCESS;
}

void
pl_free_thread_local_key(plThreadKey** pptKey)
{
    TlsFree((*pptKey)->dwIndex);
    PL_FREE((*pptKey));
    *pptKey = NULL;
}

void*
pl_allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    LPVOID lpvData = LocalAlloc(LPTR, szSize);
    if(!TlsSetValue(ptKey->dwIndex, lpvData)) 
    {
        PL_ASSERT(false);
        return NULL;
    }
    return lpvData;
}

void*
pl_get_thread_local_data(plThreadKey* ptKey)
{
    LPVOID lpvData =  TlsGetValue(ptKey->dwIndex);
    if(lpvData == NULL)
    {
        PL_ASSERT(false);
    }
    return lpvData;
}

uint64_t
pl_get_thread_id(plThread* ptThread)
{
    return ptThread->uID;
}

uint64_t
pl_get_current_thread_id(void)
{
    DWORD tID = GetCurrentThreadId();
    return (uint64_t)tID;
}

void
pl_free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    LPVOID lpvData = TlsGetValue(ptKey->dwIndex);
    LocalFree(lpvData);
}

plThreadResult
pl_create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    *pptConditionVariableOut =(plConditionVariable*)PL_ALLOC(sizeof(plConditionVariable));
    InitializeConditionVariable(&(*pptConditionVariableOut)->tHandle);
    return PL_THREAD_RESULT_SUCCESS;
}

void               
pl_destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
}

void               
pl_wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    WakeConditionVariable(&ptConditionVariable->tHandle);
}

void               
pl_wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    WakeAllConditionVariable(&ptConditionVariable->tHandle);
}

void               
pl_sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    SleepConditionVariableCS(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle, INFINITE);
}

//-----------------------------------------------------------------------------
// [SECTION] virtual memory ext
//-----------------------------------------------------------------------------

size_t
pl_get_page_size(void)
{
    SYSTEM_INFO tInfo = {0};
    GetSystemInfo(&tInfo);
    return (size_t)tInfo.dwPageSize;
}

void*
pl_virtual_alloc(void* pAddress, size_t szSize)
{
    return VirtualAlloc(pAddress, szSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void*
pl_virtual_reserve(void* pAddress, size_t szSize)
{
    return VirtualAlloc(pAddress, szSize, MEM_RESERVE, PAGE_READWRITE);
}

void*
pl_virtual_commit(void* pAddress, size_t szSize)
{
    return VirtualAlloc(pAddress, szSize, MEM_COMMIT, PAGE_READWRITE);
}

void
pl_virtual_free(void* pAddress, size_t szSize)
{
    BOOL bResult = VirtualFree(pAddress, szSize, MEM_RELEASE);
    if(bResult)
    {
        printf("VirtualFree failed : %d\n", GetLastError());
        PL_ASSERT(false);
    };
}

void
pl_virtual_uncommit(void* pAddress, size_t szSize)
{
    BOOL bResult = VirtualFree(pAddress, szSize, MEM_DECOMMIT);
    if(bResult)
    {
        printf("VirtualFree failed : %d\n", GetLastError());
        PL_ASSERT(false);
    };
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plFileI tFileApi = {
        .copy         = pl_copy_file,
        .exists       = pl_file_exists,
        .remove       = pl_file_delete,
        .binary_read  = pl_binary_read_file,
        .binary_write = pl_binary_write_file
    };

    const plNetworkI tNetworkApi = {
        .initialize           = pl_network_initialize,
        .cleanup              = pl_network_cleanup,
        .create_address       = pl_create_address,
        .destroy_address      = pl_destroy_address,
        .create_socket        = pl_create_socket,
        .destroy_socket       = pl_destroy_socket,
        .bind_socket          = pl_bind_socket,
        .send_socket_data_to  = pl_send_socket_data_to,
        .get_socket_data_from = pl_get_socket_data_from,
        .connect_socket       = pl_connect_socket,
        .get_socket_data      = pl_get_socket_data,
        .listen_socket        = pl_listen_socket,
        .select_sockets       = pl_select_sockets,
        .accept_socket        = pl_accept_socket,
        .send_socket_data     = pl_send_socket_data,
    };

    const plThreadsI tThreadApi = {
        .get_hardware_thread_count   = pl_get_hardware_thread_count,
        .create_thread               = pl_create_thread,
        .destroy_thread              = pl_destroy_thread,
        .join_thread                 = pl_join_thread,
        .yield_thread                = pl_yield_thread,
        .sleep_thread                = pl_sleep,
        .get_thread_id               = pl_get_thread_id,
        .get_current_thread_id       = pl_get_current_thread_id,
        .create_mutex                = pl_create_mutex,
        .destroy_mutex               = pl_destroy_mutex,
        .lock_mutex                  = pl_lock_mutex,
        .unlock_mutex                = pl_unlock_mutex,
        .create_semaphore            = pl_create_semaphore,
        .destroy_semaphore           = pl_destroy_semaphore,
        .wait_on_semaphore           = pl_wait_on_semaphore,
        .try_wait_on_semaphore       = pl_try_wait_on_semaphore,
        .release_semaphore           = pl_release_semaphore,
        .allocate_thread_local_key   = pl_allocate_thread_local_key,
        .allocate_thread_local_data  = pl_allocate_thread_local_data,
        .free_thread_local_key       = pl_free_thread_local_key, 
        .get_thread_local_data       = pl_get_thread_local_data, 
        .free_thread_local_data      = pl_free_thread_local_data, 
        .create_critical_section     = pl_create_critical_section,
        .destroy_critical_section    = pl_destroy_critical_section,
        .enter_critical_section      = pl_enter_critical_section,
        .leave_critical_section      = pl_leave_critical_section,
        .create_condition_variable   = pl_create_condition_variable,
        .destroy_condition_variable  = pl_destroy_condition_variable,
        .wake_condition_variable     = pl_wake_condition_variable,
        .wake_all_condition_variable = pl_wake_all_condition_variable,
        .sleep_condition_variable    = pl_sleep_condition_variable,
        .create_barrier              = pl_create_barrier,
        .destroy_barrier             = pl_destroy_barrier,
        .wait_on_barrier             = pl_wait_on_barrier
    };

    const plAtomicsI tAtomicsApi = {
        .create_atomic_counter   = pl_create_atomic_counter,
        .destroy_atomic_counter  = pl_destroy_atomic_counter,
        .atomic_store            = pl_atomic_store,
        .atomic_load             = pl_atomic_load,
        .atomic_compare_exchange = pl_atomic_compare_exchange,
        .atomic_increment        = pl_atomic_increment,
        .atomic_decrement        = pl_atomic_decrement
    };

    const plVirtualMemoryI tVirtualMemoryApi = {
        .get_page_size = pl_get_page_size,
        .alloc         = pl_virtual_alloc,
        .reserve       = pl_virtual_reserve,
        .commit        = pl_virtual_commit,
        .uncommit      = pl_virtual_uncommit,
        .free          = pl_virtual_free,
    };

    pl_set_api(ptApiRegistry, plFileI, &tFileApi);
    pl_set_api(ptApiRegistry, plVirtualMemoryI, &tVirtualMemoryApi);
    pl_set_api(ptApiRegistry, plAtomicsI, &tAtomicsApi);
    pl_set_api(ptApiRegistry, plThreadsI, &tThreadApi);
    pl_set_api(ptApiRegistry, plNetworkI, &tNetworkApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plFileI*          ptApi0 = pl_get_api_latest(ptApiRegistry, plFileI);
    const plVirtualMemoryI* ptApi1 = pl_get_api_latest(ptApiRegistry, plVirtualMemoryI);
    const plAtomicsI*       ptApi2 = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    const plThreadsI*       ptApi3 = pl_get_api_latest(ptApiRegistry, plThreadsI);
    const plNetworkI*       ptApi4 = pl_get_api_latest(ptApiRegistry, plNetworkI);

    ptApiRegistry->remove_api(ptApi0);
    ptApiRegistry->remove_api(ptApi1);
    ptApiRegistry->remove_api(ptApi2);
    ptApiRegistry->remove_api(ptApi3);
    ptApiRegistry->remove_api(ptApi4);
}
