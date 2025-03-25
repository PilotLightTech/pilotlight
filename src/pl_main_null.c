/*
   pl_main_null.c
     * null platform backend (for testing, not complete)
*/

/*
Index of this file:
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] globals
// [SECTION] entry point
// [SECTION] windows procedure
// [SECTION] helper implementations
// [SECTION] window ext
// [SECTION] file ext
// [SECTION] library ext
// [SECTION] clipboard api
// [SECTION] atomics ext
// [SECTION] threads ext
// [SECTION] network ext
// [SECTION] virtual memory ext
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_internal.h"
#include "pl_ds.h"    // hashmap & stretchy buffer

// embedded extensions
#include "pl_window_ext.h"
#include "pl_library_ext.h"
#include "pl_file_ext.h"
#include "pl_atomics_ext.h"
#include "pl_threads_ext.h"
#include "pl_network_ext.h"
#include "pl_virtual_memory_ext.h"


#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__APPLE__)
    #include <time.h> // clock_gettime_nsec_np
    #include <sys/stat.h> // timespec
    #include <copyfile.h> // copyfile
    #include <dlfcn.h>    // dlopen, dlsym, dlclose
    #include <unistd.h> // close
    #include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
#else // linux
    #include <sys/sendfile.h> // sendfile
    #include <sys/stat.h> // stat, timespec
    #include <dlfcn.h> // dlopen, dlsym, dlclose
    #include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
    #include <time.h> // clock_gettime, clock_getres
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifdef _WIN32
typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acPath[PL_MAX_PATH_LENGTH];
    char          acTransitionalName[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    HMODULE       tHandle;
    FILETIME      tLastWriteTime;
} plSharedLibrary;

#elif defined(__APPLE__)

typedef struct _plSharedLibrary
{
    bool            bValid;
    uint32_t        uTempIndex;
    char            acPath[PL_MAX_PATH_LENGTH];
    char            acTransitionalName[PL_MAX_PATH_LENGTH];
    char            acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc   tDesc;
    void*           handle;
    struct timespec tLastWriteTime;
} plSharedLibrary;

#else // linux

typedef struct _plSharedLibrary
{
    bool          bValid;
    uint32_t      uTempIndex;
    char          acPath[PL_MAX_PATH_LENGTH];
    char          acTransitionalName[PL_MAX_PATH_LENGTH];
    char          acLockFile[PL_MAX_PATH_LENGTH];
    plLibraryDesc tDesc;
    void*         handle;
    time_t        tLastWriteTime;
} plSharedLibrary;

#endif

//-----------------------------------------------------------------------------
// [SECTION] entry point
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    const char* pcAppName = "app";

    for(int i = 1; i < argc; i++)
    { 
        if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--app") == 0)
        {
            pcAppName = argv[i + 1];
            i++;
        }
        else if(strcmp(argv[i], "--version") == 0)
        {
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug (NULL)\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release (NULL)\n\n");
            #endif
            return 0;
        }
        else if(strcmp(argv[i], "--extensions") == 0)
        {
            plVersion tWindowExtVersion = plWindowI_version;
            plVersion tFileExtVersion = plFileI_version;
            plVersion tVirtualMemoryExtVersion = plVirtualMemoryI_version;
            plVersion tAtomicsExtVersion = plAtomicsI_version;
            plVersion tThreadsExtVersion = plThreadsI_version;
            plVersion tNetworkExtVersion = plNetworkI_version;
            plVersion tLibraryVersion = plLibraryI_version;
            printf("\nPilot Light - light weight game engine\n\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
            #endif
            printf("Embedded Extensions:\n");
            printf("   pl_window_ext:         %u.%u.%u\n", tWindowExtVersion.uMajor, tWindowExtVersion.uMinor, tWindowExtVersion.uMinor);
            printf("   pl_file_ext:           %u.%u.%u\n", tFileExtVersion.uMajor, tFileExtVersion.uMinor, tFileExtVersion.uMinor);
            printf("   pl_virtual_memory_ext: %u.%u.%u\n", tVirtualMemoryExtVersion.uMajor, tVirtualMemoryExtVersion.uMinor, tVirtualMemoryExtVersion.uMinor);
            printf("   pl_atomics_ext:        %u.%u.%u\n", tAtomicsExtVersion.uMajor, tAtomicsExtVersion.uMinor, tAtomicsExtVersion.uMinor);
            printf("   pl_threads_ext:        %u.%u.%u\n", tThreadsExtVersion.uMajor, tThreadsExtVersion.uMinor, tThreadsExtVersion.uMinor);
            printf("   pl_network_ext:        %u.%u.%u\n", tNetworkExtVersion.uMajor, tNetworkExtVersion.uMinor, tNetworkExtVersion.uMinor);
            printf("   pl_library_ext:        %u.%u.%u\n", tLibraryVersion.uMajor, tLibraryVersion.uMinor, tLibraryVersion.uMinor);
            return 0;
        }
        else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("\nPilot Light - light weight game engine\n");
            printf("Version: %s\n", PILOT_LIGHT_VERSION_STRING);
            #ifdef PL_CONFIG_DEBUG
                printf("Config: debug\n\n");
            #endif
            #ifdef PL_CONFIG_RELEASE
                printf("Config: release\n\n");
            #endif
            printf("Usage: pilot_light.exe [options]\n\n");
            printf("Options:\n");
            printf("-h              %s\n", "Displays this information.");
            printf("--help          %s\n", "Displays this information.");
            printf("--version       %s\n", "Displays Pilot Light version information.");
            printf("--extensions    %s\n", "Displays embedded extensions.");
            printf("-a <app>        %s\n", "Sets app to load. Default is 'app'.");
            printf("--app <app>     %s\n", "Sets app to load. Default is 'app'.");
            return 0;
        }
    }

    // load core apis
    pl__load_core_apis();
    pl__load_ext_apis();

    gptIOCtx = gptIOI->get_io();

    // command line args
    gptIOCtx->iArgc = argc;
    gptIOCtx->apArgv = argv;

    // load app library
    const plLibraryI* ptLibraryApi = pl_get_api_latest(gptApiRegistry, plLibraryI);
    plLibraryDesc tLibraryDesc = {
        .pcName = pcAppName
    };

    if(ptLibraryApi->load(tLibraryDesc, &gptAppLibrary))
    {
        #ifdef _WIN32
            pl_app_load     = (void* (__cdecl  *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__cdecl  *)(void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            pl_app_info     = (bool  (__cdecl  *)(const plApiRegistryI*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");

        #else
            pl_app_load     = (void* (__attribute__(()) *)(const plApiRegistryI*, void*)) ptLibraryApi->load_function(gptAppLibrary, "pl_app_load");
            pl_app_shutdown = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_shutdown");
            pl_app_resize   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_resize");
            pl_app_update   = (void  (__attribute__(()) *)(void*))                        ptLibraryApi->load_function(gptAppLibrary, "pl_app_update");
            pl_app_info     = (bool  (__attribute__(()) *)(const plApiRegistryI*))        ptLibraryApi->load_function(gptAppLibrary, "pl_app_info");
        #endif

        if(pl_app_info)
        {
            if(!pl_app_info(gptApiRegistry))
                return 0;
        }
        gpUserData = pl_app_load(gptApiRegistry, NULL);
        bool bApisFound = pl__check_apis();
        if(!bApisFound)
            return 3;
    }

    // main loop
    while (gptIOCtx->bRunning)
    {

        pl__garbage_collect_data_reg();

        // setup time step
        // INT64 ilCurrentTime = 0;
        // QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
        // gptIOCtx->fDeltaTime = (float)(ilCurrentTime - ilTime) / ilTicksPerSecond;
        // ilTime = ilCurrentTime;
        if(!gptIOCtx->bViewportMinimized)
        {
            pl_app_update(gpUserData);
            // pl__handle_extension_reloads();
        }
    
        if(gbApisDirty)
            pl__check_apis();

    }

    // app cleanup
    pl_app_shutdown(gpUserData);

    // unload extensions & APIs
    pl__unload_all_extensions();
    pl__unload_ext_apis();
    pl__unload_core_apis();

    if(gptAppLibrary)
    {
        PL_FREE(gptAppLibrary);
    }

    pl__check_for_leaks();
}

//-----------------------------------------------------------------------------
// [SECTION] window ext
//-----------------------------------------------------------------------------

plWindowResult
pl_create_window(plWindowDesc tDesc, plWindow** pptWindowOut)
{
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_destroy_window(plWindow* ptWindow)
{
}

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
    int iResult = remove(pcFile);
    if(iResult)
        return PL_FILE_RESULT_FAIL;
    return PL_FILE_RESULT_SUCCESS;
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
pl_copy_file(const char* source, const char* destination)
{
    #ifdef _WIN32
        BOOL bResult = CopyFile(source, destination, FALSE);
        if(bResult)
            return PL_FILE_RESULT_SUCCESS;
        return PL_FILE_RESULT_FAIL;
    #elif defined(__APPLE__)
    copyfile_state_t tS = copyfile_state_alloc();
    copyfile(source, destination, tS, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(tS);
    return PL_FILE_RESULT_SUCCESS;
    #else
        size_t bufferSize = 0u;
        pl_binary_read_file(source, &bufferSize, NULL);

        struct stat stat_buf;
        int fromfd = open(source, O_RDONLY);
        fstat(fromfd, &stat_buf);
        int tofd = open(destination, O_WRONLY | O_CREAT, stat_buf.st_mode);
        int n = 1;
        while (n > 0)
            n = sendfile(tofd, fromfd, 0, bufferSize * 2);
        return PL_FILE_RESULT_SUCCESS;
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] library ext
//-----------------------------------------------------------------------------

#ifdef _WIN32
static inline FILETIME
pl__get_last_write_time(const char* pcFilename)
{
    FILETIME tLastWriteTime = {0};
    
    WIN32_FILE_ATTRIBUTE_DATA tData = {0};
    if(GetFileAttributesExA(pcFilename, GetFileExInfoStandard, &tData))
        tLastWriteTime = tData.ftLastWriteTime;
    
    return tLastWriteTime;
}
#elif defined(__APPLE__)
struct timespec
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtimespec;
}
#else
static inline time_t
pl__get_last_write_time(const char* filename)
{
    struct stat attr;
    stat(filename, &attr);
    return attr.st_mtime;
}
#endif


bool
pl_has_library_changed(plSharedLibrary* ptLibrary)
{
    PL_ASSERT(ptLibrary);
    if(ptLibrary)
    {
        #ifdef _WIN32
        FILETIME newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return CompareFileTime(&newWriteTime, &ptLibrary->tLastWriteTime) != 0;
        #elif defined(__APPLE__)
        struct timespec newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return newWriteTime.tv_sec != ptLibrary->tLastWriteTime.tv_sec;
        #else
        time_t newWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        return newWriteTime != ptLibrary->tLastWriteTime;
        #endif
    }
    return false;
}

plLibraryResult
pl_load_library(plLibraryDesc tDesc, plSharedLibrary** pptLibraryOut)
{

    plSharedLibrary* ptLibrary = NULL;

    if(*pptLibraryOut == NULL)
    {
        *pptLibraryOut = PL_ALLOC(sizeof(plSharedLibrary));
        memset((*pptLibraryOut), 0, sizeof(plSharedLibrary));

        ptLibrary = *pptLibraryOut;

        ptLibrary->bValid = false;
        ptLibrary->tDesc = tDesc;

        #ifdef _WIN32
        pl_sprintf(ptLibrary->acPath, "%s.dll", tDesc.pcName);
        #elif defined(__APPLE__)
        pl_sprintf(ptLibrary->acPath, "%s.dylib", tDesc.pcName);
        #else
        pl_sprintf(ptLibrary->acPath, "./%s.so", tDesc.pcName);
        #endif

        if(tDesc.pcTransitionalName)
            strncpy(ptLibrary->acTransitionalName, tDesc.pcTransitionalName, PL_MAX_PATH_LENGTH);
        else
        {
            #ifdef _WIN32
            pl_sprintf(ptLibrary->acTransitionalName, "%s_", tDesc.pcName);
            #elif defined(__APPLE__)
            pl_sprintf(ptLibrary->acTransitionalName, "%s_", tDesc.pcName);
            #else
            pl_sprintf(ptLibrary->acTransitionalName, "./%s_", tDesc.pcName);
            #endif
        }

        if(tDesc.pcLockFile)
            strncpy(ptLibrary->acLockFile, tDesc.pcLockFile, PL_MAX_PATH_LENGTH);
        else
            strncpy(ptLibrary->acLockFile, "lock.tmp", PL_MAX_PATH_LENGTH);
    }
    else
        ptLibrary = *pptLibraryOut;

    ptLibrary->bValid = false;

    #ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA tIgnored;
    if(!GetFileAttributesExA(ptLibrary->acLockFile, GetFileExInfoStandard, &tIgnored))  // lock file gone
    #else
    struct stat attr2;
    if(stat(ptLibrary->acLockFile, &attr2) == -1)  // lock file gone
    #endif
    {
        char acTemporaryName[2024] = {0};
        ptLibrary->tLastWriteTime = pl__get_last_write_time(ptLibrary->acPath);
        
        #ifdef _WIN32
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dll");
        #elif defined(__APPLE__)
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".dylib");
        #else
        pl_sprintf(acTemporaryName, "%s%u%s", ptLibrary->acTransitionalName, ptLibrary->uTempIndex, ".so");
        #endif
        if(++ptLibrary->uTempIndex >= 1024)
        {
            ptLibrary->uTempIndex = 0;
        }
        pl_copy_file(ptLibrary->acPath, acTemporaryName);

        
        #ifdef _WIN32
        ptLibrary->tHandle = NULL;
        ptLibrary->tHandle = LoadLibraryA(acTemporaryName);
        if(ptLibrary->tHandle)
            ptLibrary->bValid = true;
        else
        {
            DWORD iLastError = GetLastError();
            printf("LoadLibaryA() failed with error code : %d\n", iLastError);
        }
        #else
        ptLibrary->handle = NULL;
        ptLibrary->handle = dlopen(acTemporaryName, RTLD_NOW);
        if(ptLibrary->handle)
            ptLibrary->bValid = true;
        else
        {
            printf("\n\n%s\n\n", dlerror());
        }
        #endif
    }

    if(ptLibrary->bValid)
        return PL_LIBRARY_RESULT_SUCCESS;
    return PL_LIBRARY_RESULT_FAIL;
}

void
pl_reload_library(plSharedLibrary* ptLibrary)
{
    ptLibrary->bValid = false;
    for(uint32_t i = 0; i < 100; i++)
    {
        if(pl_load_library(ptLibrary->tDesc, &ptLibrary))
            break;
        pl_sleep(100);
    }
}

void*
pl_load_library_function(plSharedLibrary* ptLibrary, const char* name)
{
    PL_ASSERT(ptLibrary->bValid && "library not valid, should have been checked");
    void* pLoadedFunction = NULL;
    if(ptLibrary->bValid)
    {
        #ifdef _WIN32
        pLoadedFunction = (void*)GetProcAddress(ptLibrary->tHandle, name);
        #else
        pLoadedFunction = dlsym(ptLibrary->handle, name);
        #endif
    }
    return pLoadedFunction;
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
    *ptCounter = malloc(sizeof(plAtomicCounter));
    (*ptCounter)->ilValue = ilValue;
    return PL_ATOMICS_RESULT_SUCCESS;
}

void
pl_destroy_atomic_counter(plAtomicCounter** ptCounter)
{
    free((*ptCounter));
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
    return true;
}

int64_t
pl_atomic_increment(plAtomicCounter* ptCounter)
{
    return ptCounter->ilValue++;
}

int64_t
pl_atomic_decrement(plAtomicCounter* ptCounter)
{
    return ptCounter->ilValue--;
}

//-----------------------------------------------------------------------------
// [SECTION] network ext
//-----------------------------------------------------------------------------

plNetworkResult
pl_create_address(const char* pcAddress, const char* pcService, plNetworkAddressFlags tFlags, plNetworkAddress** pptAddress)
{
    return PL_NETWORK_RESULT_FAIL;
}

void
pl_destroy_address(plNetworkAddress** pptAddress)
{
}

void
pl_create_socket(plSocketFlags tFlags, plSocket** pptSocketOut)
{
}

void
pl_destroy_socket(plSocket** pptSocket)
{
}

plNetworkResult
pl_send_socket_data_to(plSocket* ptFromSocket, plNetworkAddress* ptAddress, const void* pData, size_t szSize, size_t* pszSentSize)
{
    return PL_NETWORK_RESULT_FAIL;
}

plNetworkResult
pl_bind_socket(plSocket* ptSocket, plNetworkAddress* ptAddress)
{
    return PL_NETWORK_RESULT_FAIL;
}

plNetworkResult
pl_get_socket_data_from(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize, plSocketReceiverInfo* ptReceiverInfo)
{
    return PL_NETWORK_RESULT_FAIL;
}

plNetworkResult
pl_connect_socket(plSocket* ptFromSocket, plNetworkAddress* ptAddress)
{
    return PL_NETWORK_RESULT_FAIL;
}

plNetworkResult
pl_get_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize)
{
    return PL_NETWORK_RESULT_FAIL; // connection closed by peer
}

plNetworkResult
pl_select_sockets(plSocket** ptSockets, bool* abSelectedSockets, uint32_t uSocketCount, uint32_t uTimeOutMilliSec)
{
    return PL_NETWORK_RESULT_FAIL;
}

plNetworkResult
pl_accept_socket(plSocket* ptSocket, plSocket** pptSocketOut)
{
    return PL_NETWORK_RESULT_FAIL;
}

plNetworkResult
pl_listen_socket(plSocket* ptSocket)
{
    return PL_NETWORK_RESULT_FAIL;
}

plNetworkResult
pl_send_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszSentSize)
{
    return PL_NETWORK_RESULT_FAIL;
}

//-----------------------------------------------------------------------------
// [SECTION] threads ext
//-----------------------------------------------------------------------------

void
pl_sleep(uint32_t uMillisec)
{
}

plThreadResult
pl_create_thread(plThreadProcedure ptProcedure, void* pData, plThread** ppThreadOut)
{
    return PL_THREAD_RESULT_FAIL;
}

void
pl_join_thread(plThread* ptThread)
{
}

void
pl_destroy_thread(plThread** ppThread)
{
}

void
pl_yield_thread(void)
{
}

plThreadResult
pl_create_mutex(plMutex** ppMutexOut)
{
    return PL_THREAD_RESULT_FAIL;
}

void
pl_destroy_mutex(plMutex** ptMutex)
{
}

void
pl_lock_mutex(plMutex* ptMutex)
{
}

void
pl_unlock_mutex(plMutex* ptMutex)
{
}

plThreadResult
pl_create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    return PL_THREAD_RESULT_FAIL;
}

void
pl_destroy_critical_section(plCriticalSection** pptCriticalSection)
{
}

void
pl_enter_critical_section(plCriticalSection* ptCriticalSection)
{
}

void
pl_leave_critical_section(plCriticalSection* ptCriticalSection)
{
}

uint32_t
pl_get_hardware_thread_count(void)
{
    return 1;
}

plThreadResult
pl_create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    return PL_THREAD_RESULT_FAIL;
}

void
pl_destroy_barrier(plBarrier** pptBarrier)
{
}

void
pl_wait_on_barrier(plBarrier* ptBarrier)
{
}

plThreadResult
pl_create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    return PL_THREAD_RESULT_FAIL;
}

void
pl_destroy_semaphore(plSemaphore** pptSemaphore)
{
}

void
pl_wait_on_semaphore(plSemaphore* ptSemaphore)
{
}

bool
pl_try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    return false;
}

void
pl_release_semaphore(plSemaphore* ptSemaphore)
{
}

plThreadResult
pl_allocate_thread_local_key(plThreadKey** pptKeyOut)
{
    return PL_THREAD_RESULT_FAIL;
}

void
pl_free_thread_local_key(plThreadKey** pptKey)
{
}

void*
pl_allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    return NULL;
}

void*
pl_get_thread_local_data(plThreadKey* ptKey)
{
    return NULL;
}

uint64_t
pl_get_thread_id(plThread* ptThread)
{
    return 0;
}

uint64_t
pl_get_current_thread_id(void)
{
    return 0;
}

void
pl_free_thread_local_data(plThreadKey* ptKey, void* pData)
{
}

plThreadResult
pl_create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    return PL_THREAD_RESULT_FAIL;
}

void               
pl_destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
}

void               
pl_wake_condition_variable(plConditionVariable* ptConditionVariable)
{
}

void               
pl_wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
}

void               
pl_sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
}

//-----------------------------------------------------------------------------
// [SECTION] virtual memory ext
//-----------------------------------------------------------------------------

size_t
pl_get_page_size(void)
{
    return 0;
}

void*
pl_virtual_alloc(void* pAddress, size_t szSize)
{
    return NULL;
}

void*
pl_virtual_reserve(void* pAddress, size_t szSize)
{
    return NULL;
}

void*
pl_virtual_commit(void* pAddress, size_t szSize)
{
    return NULL;
}

void
pl_virtual_free(void* pAddress, size_t szSize)
{
}

void
pl_virtual_uncommit(void* pAddress, size_t szSize)
{
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl.c"