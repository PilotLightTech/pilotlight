/*
    pl_os_ext_win32.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] network api
// [SECTION] thread api
// [SECTION] atomic api
// [SECTION] virtual memory api
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <winsock2.h> // sockets
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM(), GET_Y_LPARAM()
#include <sysinfoapi.h> // page size

#include "pl.h"
#include "pl_os_ext_internal.h"

#pragma comment(lib, "ws2_32.lib")

//-----------------------------------------------------------------------------
// [SECTION] structs
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

typedef struct _plAtomicCounter
{
    int64_t ilValue;
} plAtomicCounter;

typedef struct _plThreadData
{
  plThreadProcedure ptProcedure;
  void*             pData;
} plThreadData;

typedef struct _plThread
{
    HANDLE        tHandle;
    plThreadData* ptData;
    uint32_t      uID;
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

//-----------------------------------------------------------------------------
// [SECTION] network api
//-----------------------------------------------------------------------------

plNetworkResult
pl_initialize_network(void)
{
    // initialize winsock
    WSADATA tWsaData = {0};
    if(WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0)
    {
        printf("Failed to start winsock with error code: %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

void
pl_shutdown_network(void)
{
    // cleanup winsock
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

    *pptAddress = PL_ALLOC(sizeof(plNetworkAddress));
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
    *pptSocketOut = PL_ALLOC(sizeof(plSocket));
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

    *pptSocketOut = PL_ALLOC(sizeof(plSocket));
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
// [SECTION] thread api
//-----------------------------------------------------------------------------

void
pl_sleep(uint32_t uMillisec)
{
    Sleep((long)uMillisec);
}

static DWORD 
thread_procedure(void* lpParam)
{
    plThreadData* ptData = lpParam;
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
    plThreadData* ptData = PL_ALLOC(sizeof(plThreadData));
    ptData->ptProcedure = ptProcedure;
    ptData->pData       = pData;

    HANDLE tHandle = CreateThread(0, 1024, thread_procedure, ptData, 0, NULL);
    if(tHandle)
    {
        static uint32_t uNextThreadId = 0;
        uNextThreadId++;
        *ppThreadOut = PL_ALLOC(sizeof(plThread));
        (*ppThreadOut)->uID = uNextThreadId;
        (*ppThreadOut)->ptData = ptData;
        (*ppThreadOut)->tHandle = tHandle;
        return PL_THREAD_RESULT_SUCCESS;
    }
    PL_FREE(ptData);
    return PL_THREAD_RESULT_FAIL;
    
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
pl_join_thread(plThread* ptThread)
{
    WaitForSingleObject(ptThread->tHandle, INFINITE);
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
        (*ppMutexOut) = malloc(sizeof(plMutex));
        (*ppMutexOut)->tHandle = CreateMutex(NULL, FALSE, NULL);
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
    (*pptCriticalSectionOut) = PL_ALLOC(sizeof(plCriticalSection));
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
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION atInfo = PL_ALLOC(dwLength);
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
    (*pptBarrierOut) = PL_ALLOC(sizeof(plBarrier));
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
    (*pptSemaphoreOut) = PL_ALLOC(sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = CreateSemaphore(NULL, uIntialCount, uIntialCount, NULL);
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
    *pptKeyOut = PL_ALLOC(sizeof(plThreadKey));
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

uint32_t
pl_get_thread_id(plThread* ptThread)
{
    return ptThread->uID;
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
    *pptConditionVariableOut = PL_ALLOC(sizeof(plConditionVariable));
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
// [SECTION] atomic api
//-----------------------------------------------------------------------------

plAtomicResult
pl_create_atomic_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    *ptCounter = _aligned_malloc(sizeof(plAtomicCounter), 8);
    (*ptCounter)->ilValue = ilValue;
    return PL_ATOMIC_RESULT_SUCCESS;
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
// [SECTION] virtual memory api
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