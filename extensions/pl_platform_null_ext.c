#include <stdlib.h>
#include <stdio.h>
#include "pl.h"

// extensions
#include "pl_platform_ext.h"

#define PL_NO_PLATFORM_WINDOW

#ifdef PL_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sysinfoapi.h> // page size
#include <winsock2.h> // sockets
#include <ws2tcpip.h> // socklen_t
#pragma comment(lib, "ws2_32.lib")
#elif defined(PL_PLATFORM_LINUX)
#include <time.h>     // clock_gettime, clock_getres
#include <sys/stat.h> // stat, timespec
#include <sys/types.h>
#include <fcntl.h>    // O_RDONLY, O_WRONLY ,O_CREAT
#include <pthread.h>
#include <unistd.h> // rmdir
#include <sys/sendfile.h> // sendfile
#include <stdatomic.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <dirent.h> // directory operations
#elif defined(PL_PLATFORM_APPLE)
#import <Cocoa/Cocoa.h> // dispatch semaphore stuff
#import <time.h>
#include <sys/stat.h> // timespec
#include <copyfile.h> // copyfile
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h> // close, rmdir
#include <dirent.h> // directory operations
#include <fcntl.h> // O_RDONLY, O_WRONLY ,O_CREAT
#include <sys/mman.h>
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static const plIOI*      gptIOI    = NULL;
static const plMemoryI*  gptMemory = NULL;
static plIO*             gptIO     = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#ifndef PL_DS_ALLOC
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#include "pl_ds.h"

typedef struct _plWindowSurfaceImageNull
{
    uint32_t uWidth;
    uint32_t uHeight;
    void*    pPixels;
    uint32_t uPixelCapacity;
} plWindowSurfaceImageNull;

typedef struct _plWindowSurface
{
    plWindow* ptWindow;
    uint32_t  uCurrentImage;
    uint32_t  uImageCount;
    plWindowSurfaceImageNull* atImages;
} plWindowSurface;

typedef struct _plWindowData
{
    uint32_t uWidth;
    uint32_t uHeight;
} plWindowData;

typedef struct _plPlatformExtData
{
    plWindow*   ptMainWindow;
    plWindow**  sbtWindows;
    bool        bMouseTracked;

    // timer stuff
    #ifdef PL_PLATFORM_WINDOWS
    int64_t ilTime;
    int64_t ilTicksPerSecond;
    #elif defined(PL_PLATFORM_LINUX)
    double dFrequency;
    double dStartTime;
    #elif defined(PL_PLATFORM_APPLE)
    double dInitialTime;
    #endif
} plPlatformExtData;

static plPlatformExtData* gptPlatformExtCtx = NULL;
plThread** gsbtThreads;

static inline void
write_u16(FILE* f, uint16_t value)
{
   fputc((uint8_t)(value >> 0), f);
   fputc((uint8_t)(value >> 8), f);
}
static inline void
write_u32(FILE* f, uint32_t value)
{
   fputc((uint8_t)(value >> 0),  f);
   fputc((uint8_t)(value >> 8),  f);
   fputc((uint8_t)(value >> 16), f);
   fputc((uint8_t)(value >> 24), f);
}

static int
write_bmp(const char* filename, const uint8_t* pixels, uint32_t width, uint32_t height)
{
   FILE* f = fopen(filename, "wb");
   if(!f)
       return 0;
   const uint32_t bytes_per_pixel = 4;
   const uint32_t image_size = width * height * bytes_per_pixel;
   const uint32_t header_size = 14 + 40;
   const uint32_t file_size = header_size + image_size;
   // ---------------------------------------------------------
   // BITMAPFILEHEADER - 14 bytes
   // ---------------------------------------------------------
   write_u16(f, 0x4D42);       // "BM"
   write_u32(f, file_size);
   write_u16(f, 0);            // reserved
   write_u16(f, 0);            // reserved
   write_u32(f, header_size);  // pixel data offset
   // ---------------------------------------------------------
   // BITMAPINFOHEADER - 40 bytes
   // ---------------------------------------------------------
   write_u32(f, 40);           // header size
   write_u32(f, width);
   // Negative height = top-down image.

   // This is convenient for framebuffer data where row 0
   // represents the top of the image.
   write_u32(f, (uint32_t)(-(int32_t)height));
   write_u16(f, 1);            // planes
   write_u16(f, 32);           // bits per pixel
   write_u32(f, 0);            // BI_RGB
   write_u32(f, image_size);
   write_u32(f, 0);            // horizontal resolution
   write_u32(f, 0);            // vertical resolution
   write_u32(f, 0);            // colors used
   write_u32(f, 0);            // important colors
   // BMP BI_RGB 32-bit expects B,G,R,A byte ordering.
   fwrite(pixels, image_size, 1, f);
   fclose(f);
   return 1;
}

//-----------------------------------------------------------------------------
// [SECTION] timer api
//-----------------------------------------------------------------------------

double
pl_timer_get_time(void)
{
    #ifdef PL_PLATFORM_WINDOWS
        int64_t ilCurrentTime = 0;
        QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
        return (double)(ilCurrentTime - gptPlatformExtCtx->ilTime) / (double)gptPlatformExtCtx->ilTicksPerSecond;
    #elif defined(PL_PLATFORM_LINUX)
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
        {
            PL_ASSERT(false && "clock_gettime() failed");
        }
        uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
        return ((double)nsec_count / gptPlatformExtCtx->dFrequency) - gptPlatformExtCtx->dStartTime;
    #elif defined(PL_PLATFORM_APPLE)
    double dNewTime = (CFTimeInterval)((double)clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9);
    return dNewTime - gptPlatformExtCtx->dInitialTime;
    #endif
}

double
pl_timer_get_raw_time(void)
{
    #ifdef PL_PLATFORM_WINDOWS
        int64_t ilCurrentTime = 0;
        QueryPerformanceCounter((LARGE_INTEGER*)&ilCurrentTime);
        return (double)ilCurrentTime / (double)gptPlatformExtCtx->ilTicksPerSecond;
    #elif defined(PL_PLATFORM_LINUX)
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
        {
            PL_ASSERT(false && "clock_gettime() failed");
        }
        uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
        return (double)nsec_count / gptPlatformExtCtx->dFrequency;
    #elif defined(PL_PLATFORM_APPLE)
    double dNewTime = (CFTimeInterval)((double)clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9);
    return dNewTime;
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] window api
//-----------------------------------------------------------------------------

plWindowResult
pl_window_create(plWindowDesc tDesc, plWindow** pptWindowOut)
{
    plWindowData* ptData = PL_ALLOC(sizeof(plWindowData));
    plWindow* ptWindow = PL_ALLOC(sizeof(plWindow));
    pl_sb_push(gptPlatformExtCtx->sbtWindows, ptWindow);
    *pptWindowOut = ptWindow;

    if(gptPlatformExtCtx->ptMainWindow == NULL)
        gptPlatformExtCtx->ptMainWindow = ptWindow;

    ptWindow->_pBackendData = ptData; //-V522
    ptData->uWidth = tDesc.uWidth;
    ptData->uHeight = tDesc.uHeight;

    // show window
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_window_destroy(plWindow* ptWindow)
{
    PL_FREE(ptWindow);
}

void
pl_window_show(plWindow* ptWindow)
{
}

plWindowResult
pl_window_create_surface(plWindow* ptWindow, const plWindowSurfaceDesc* ptDesc, plWindowSurface** ptSurfaceOut)
{
    plWindowSurface* ptSurface = PL_ALLOC(sizeof(plWindowSurface));
    
    ptSurface->ptWindow = ptWindow;
    ptSurface->atImages = PL_ALLOC(sizeof(plWindowSurfaceImageNull) * ptDesc->uImageCount);
    memset(ptSurface->atImages, 0, sizeof(plWindowSurfaceImageNull) * ptDesc->uImageCount);
    ptSurface->uImageCount = ptDesc->uImageCount;

    *ptSurfaceOut = ptSurface;
    return PL_WINDOW_RESULT_SUCCESS;
}

void
pl_window_destroy_surface(plWindowSurface** ptSurfaceIn)
{
    plWindowSurface* ptSurface = *ptSurfaceIn;
    plWindow* ptWindow = ptSurface->ptWindow;
    for(uint32_t i = 0; i < ptSurface->uImageCount; i++)
    {
        PL_FREE(ptSurface->atImages[i].pPixels);
    }
    PL_FREE(ptSurface->atImages);
    memset(ptSurface, 0, sizeof(plWindowSurface));
    PL_FREE(*ptSurfaceIn);
    *ptSurfaceIn = NULL;
}

bool
pl_window_acquire_surface_image(plWindowSurface* ptSurface, plWindowSurfaceImage* ptImageOut)
{
    plWindow* ptWindow = ptSurface->ptWindow;

    plWindowSurfaceImageNull* ptImageWin32 = &ptSurface->atImages[ptSurface->uCurrentImage];

    plWindowData* ptData = ptWindow->_pBackendData;
    int iClientWidth  = ptData->uWidth;
    int iClientHeight = ptData->uHeight;

    uint32_t uPixelsNeeded = (uint32_t)iClientWidth * (uint32_t)iClientHeight;
    bool bResizeNeeded = uPixelsNeeded > ptImageWin32->uPixelCapacity;

    if(bResizeNeeded)
    {
        if(ptImageWin32->pPixels)
        {
            PL_FREE(ptImageWin32->pPixels);
            ptImageWin32->pPixels = NULL;
        }
        ptImageWin32->pPixels = PL_ALLOC(uPixelsNeeded * sizeof(uint32_t));
        memset(ptImageWin32->pPixels, 0, uPixelsNeeded * sizeof(uint32_t));
        ptImageWin32->uPixelCapacity = uPixelsNeeded;
    }

    ptImageOut->pPixels = ptImageWin32->pPixels;
    ptImageOut->uWidth = (uint32_t)iClientWidth;
    ptImageOut->uHeight = (uint32_t)iClientHeight;
    ptImageOut->uRowPitch = ptImageOut->uWidth * sizeof(uint32_t);
    ptImageOut->uImageIndex = ptSurface->uCurrentImage;
    ptImageOut->tFormat = PL_WINDOW_SURFACE_FORMAT_B8G8R8A8_UNORM;

    ptImageWin32->uWidth = ptImageOut->uWidth;
    ptImageWin32->uHeight = ptImageOut->uHeight;

    return true;
}

void
pl_window_present_surface_image(plWindowSurface* ptSurface, uint32_t uImageIndex)
{
    plWindow* ptWindow = ptSurface->ptWindow;
    plWindowSurfaceImageNull* ptImageWin32 = &ptSurface->atImages[uImageIndex];
    write_bmp("window.bmp", ptImageWin32->pPixels, ptImageWin32->uWidth, ptImageWin32->uHeight);
    ptSurface->uCurrentImage++;
    ptSurface->uCurrentImage %= ptSurface->uImageCount; 
}

bool 
pl_window_set_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, const plWindowAttributeValue* ptValue)
{
    return false;
}

bool
pl_window_get_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, plWindowAttributeValue* ptValue)
{
    return false;
}

bool
pl_window_set_cursor_mode(plWindow* ptWindow, plCursorMode tMode)
{
    return tMode == PL_CURSOR_MODE_NORMAL;
}

plCursorMode
pl_window_get_cursor_mode(plWindow* ptWindow)
{
    return PL_CURSOR_MODE_NORMAL;
}

bool
pl_window_set_raw_mouse_input(plWindow* ptWindow, bool bValue)
{
    return !bValue;
}

bool
pl_window_set_fullscreen(plWindow* ptWindow, const plFullScreenDesc* tDesc)
{
    return tDesc->tMode == PL_FULLSCREEN_MODE_NONE;
}

const plWindowCapabilities*
pl_window_get_capabilities(void)
{
    static plWindowCapabilities tCapabilities = {0};

    tCapabilities.uCursorModeCount = 1;
    tCapabilities.uAttributeCount = 1;
    tCapabilities.uFullScreenModeCount = 2;

    static const plWindowAttribute atSupportedAttributes[] = {
        -1
    };

    static const plCursorMode atSupportedCursorModes[] = {
        PL_CURSOR_MODE_NORMAL
    };

    static const plFullScreenMode atSupportedScreenModes[] = {
        PL_FULLSCREEN_MODE_NONE,
        PL_FULLSCREEN_MODE_EXCLUSIVE
    };

    tCapabilities.atCursorModes = atSupportedCursorModes;
    tCapabilities.atFullScreenModes = atSupportedScreenModes;
    tCapabilities.atWindowAttributes = atSupportedAttributes;
    tCapabilities.tFlags = PL_WINDOW_CAPABILITY_FLAGS_NONE;

    return &tCapabilities;
}

void
pl_window_set_callback(plWindow* ptWindow, plWindowEventCallback tCallback, void* pUserData)
{
    // TODO: implement
}

plWindowEventCallback
pl_window_get_callback(plWindow* ptWindow)
{
    plWindowEventCallback tCallback = PL_ZERO_INIT;
    return tCallback;
}

const char*
pl_get_clipboard_text(void* user_data_ctx)
{
    return NULL;
}

void
pl_set_clipboard_text(void* pUnused, const char* text)
{
}

//-----------------------------------------------------------------------------
// [SECTION] atomics api
//-----------------------------------------------------------------------------


typedef struct _plAtomicCounter
{
    #ifdef PL_PLATFORM_WINDOWS
        int64_t ilValue;
    #else
        atomic_int_fast64_t ilValue;
    #endif
} plAtomicCounter;

plAtomicsResult
pl_atomics_create_counter(int64_t ilValue, plAtomicCounter** ptCounter)
{
    #ifdef PL_PLATFORM_WINDOWS
        *ptCounter = (plAtomicCounter*)_aligned_malloc(sizeof(plAtomicCounter), 8);
        (*ptCounter)->ilValue = ilValue;
    #else
        *ptCounter = PL_ALLOC(sizeof(plAtomicCounter));
        atomic_init(&(*ptCounter)->ilValue, ilValue); //-V522
    #endif

    return PL_ATOMICS_RESULT_SUCCESS;
}

void
pl_atomics_destroy_counter(plAtomicCounter** ptCounter)
{
    #ifdef PL_PLATFORM_WINDOWS
        _aligned_free((*ptCounter));
        (*ptCounter) = NULL;
    #else
        PL_FREE((*ptCounter));
        (*ptCounter) = NULL;
    #endif
}

void
pl_atomics_store(plAtomicCounter* ptCounter, int64_t ilValue)
{
    #ifdef PL_PLATFORM_WINDOWS
        ptCounter->ilValue = ilValue;
    #else
        atomic_store(&ptCounter->ilValue, ilValue);
    #endif
}

int64_t
pl_atomics_load(plAtomicCounter* ptCounter)
{
    #ifdef PL_PLATFORM_WINDOWS
        return ptCounter->ilValue;
    #else
        return atomic_load(&ptCounter->ilValue);
    #endif
}

bool
pl_atomics_compare_exchange(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue)
{
    #ifdef PL_PLATFORM_WINDOWS
        return InterlockedCompareExchange64(&ptCounter->ilValue, ilDesiredValue, ilExpectedValue) == ilExpectedValue;
    #else
        return atomic_compare_exchange_strong(&ptCounter->ilValue, &ilExpectedValue, ilDesiredValue);
    #endif
}

int64_t
pl_atomics_increment(plAtomicCounter* ptCounter)
{
    #ifdef PL_PLATFORM_WINDOWS
        return InterlockedIncrement64(&ptCounter->ilValue);
    #else
        return atomic_fetch_add(&ptCounter->ilValue, 1);
    #endif
}

int64_t
pl_atomics_decrement(plAtomicCounter* ptCounter)
{
    #ifdef PL_PLATFORM_WINDOWS
        return InterlockedDecrement64(&ptCounter->ilValue);
    #else
        return atomic_fetch_sub(&ptCounter->ilValue, 1);
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] threads api
//-----------------------------------------------------------------------------

#ifdef PL_PLATFORM_APPLE
typedef int pthread_barrierattr_t;
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;

int
pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count)
{
    if(count == 0)
    {
        errno = EINVAL;
        return PL_THREAD_RESULT_FAIL;
    }
    if(pthread_mutex_init(&barrier->mutex, 0) < 0)
    {
        return PL_THREAD_RESULT_FAIL;
    }
    if(pthread_cond_init(&barrier->cond, 0) < 0)
    {
        pthread_mutex_destroy(&barrier->mutex);
        return PL_THREAD_RESULT_FAIL;
    }
    barrier->tripCount = count;
    barrier->count = 0;

    return 0;
}

int
pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
    return 0;
}

int
pthread_barrier_wait(pthread_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    ++(barrier->count);
    if(barrier->count >= barrier->tripCount)
    {
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;
    }
    else
    {
        pthread_cond_wait(&barrier->cond, &(barrier->mutex));
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}
#endif

typedef struct _plThreadData
{
    #ifdef PL_PLATFORM_WINDOWS
    plThreadProcedure ptProcedure;
    void*             pData;
    #elif defined(PL_PLATFORM_LINUX)
    int _unUsed;
    #endif
} plThreadData;

typedef struct _plThread
{
    #ifdef PL_PLATFORM_WINDOWS
    HANDLE        tHandle;
    plThreadData* ptData;
    uint64_t      uID;
    #else
    pthread_t tHandle;
    uint64_t  uID;
    #endif
} plThread;

typedef struct _plMutex
{
    #ifdef PL_PLATFORM_WINDOWS
    HANDLE tHandle;
    #else
    pthread_mutex_t tHandle;
    #endif
} plMutex;

typedef struct _plCriticalSection
{
    #ifdef PL_PLATFORM_WINDOWS
    CRITICAL_SECTION tHandle;
    #else
    pthread_mutex_t tHandle;
    #endif
} plCriticalSection;

typedef struct _plSemaphore
{
    #ifdef PL_PLATFORM_WINDOWS
    HANDLE tHandle;
    #elif defined(PL_PLATFORM_LINUX)
    sem_t tHandle;
    #elif defined(PL_PLATFORM_APPLE)
    dispatch_semaphore_t tHandle;
    #endif
} plSemaphore;

typedef struct _plBarrier
{
    #ifdef PL_PLATFORM_WINDOWS
    SYNCHRONIZATION_BARRIER tHandle;
    #else
    pthread_barrier_t tHandle;
    #endif
} plBarrier;

typedef struct _plConditionVariable
{
    #ifdef PL_PLATFORM_WINDOWS
    CONDITION_VARIABLE tHandle;
    #else
    pthread_cond_t tHandle;
    #endif
} plConditionVariable;

typedef struct _plThreadKey
{
    #ifdef PL_PLATFORM_WINDOWS
    DWORD dwIndex;
    #else
    pthread_key_t tKey;
    #endif
} plThreadKey;

void
pl_threads_sleep_thread(uint32_t uMillisec)
{
    #ifdef PL_PLATFORM_WINDOWS
    Sleep((long)uMillisec);
    #else
    struct timespec ts = {0};
    int res;

    ts.tv_sec = uMillisec / 1000;
    ts.tv_nsec = (uMillisec % 1000) * 1000000;

    do 
    {
        res = nanosleep(&ts, &ts);
    } 
    while (res);
    #endif
}

#ifdef PL_PLATFORM_WINDOWS
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
#endif

plThreadResult
pl_threads_create_thread(plThreadProcedure ptProcedure, void* pData, plThread** pptThreadOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    plThreadData* ptData = (plThreadData*)PL_ALLOC(sizeof(plThreadData));
    ptData->ptProcedure = ptProcedure;
    ptData->pData       = pData;

    HANDLE tHandle = CreateThread(0, 1024, thread_procedure, ptData, 0, NULL);
    if(tHandle)
    {
        DWORD tID = GetThreadId(tHandle);
        *pptThreadOut = (plThread*)PL_ALLOC(sizeof(plThread));
        (*pptThreadOut)->ptData = ptData;
        (*pptThreadOut)->tHandle = tHandle;
        (*pptThreadOut)->uID = (uint64_t)tID;
        return PL_THREAD_RESULT_SUCCESS;
    }
    PL_FREE(ptData);
    return PL_THREAD_RESULT_FAIL;

    #else

    *pptThreadOut = PL_ALLOC(sizeof(plThread));
    plThread* ptThread = *pptThreadOut;
    if(pthread_create(&ptThread->tHandle, NULL, ptProcedure, pData))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    static uint64_t uThreadID = 1;
    (*pptThreadOut)->uID = uThreadID++;
    pl_sb_push(gsbtThreads, ptThread);
    return PL_THREAD_RESULT_SUCCESS;
    #endif
    
}

void
pl_threads_join_thread(plThread* ptThread)
{
    #ifdef PL_PLATFORM_WINDOWS
    WaitForSingleObject(ptThread->tHandle, INFINITE);
    #else
    pthread_join(ptThread->tHandle, NULL);
    #endif
}

void
pl_threads_destroy_thread(plThread** ppThread)
{
    #ifdef PL_PLATFORM_WINDOWS
    pl_threads_join_thread(*ppThread);
    CloseHandle((*ppThread)->tHandle);
    PL_FREE((*ppThread)->ptData);

    #else

    pl_threads_join_thread(*ppThread);

    const uint32_t uThreadCount = pl_sb_size(gsbtThreads);
    for(uint32_t i = 0; i < uThreadCount; i++)
    {
        if(gsbtThreads[i] == (*ppThread))
        {
            pl_sb_del_swap(gsbtThreads, i);
            break;
        }
    }

    #endif

    PL_FREE(*ppThread);
    *ppThread = NULL;
}

void
pl_threads_yield_thread(void)
{
    #ifdef PL_PLATFORM_WINDOWS
    thread_yield();
    #else
    sched_yield();
    #endif
}

plThreadResult
pl_threads_create_mutex(plMutex** pptMutexOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    HANDLE tHandle = CreateMutex(NULL, FALSE, NULL);
    if(tHandle)
    {
        (*pptMutexOut) = (plMutex*)PL_ALLOC(sizeof(plMutex));
        // (*pptMutexOut)->tHandle = CreateMutex(NULL, FALSE, NULL);
        (*pptMutexOut)->tHandle = tHandle;
        return PL_THREAD_RESULT_SUCCESS;
    }
    return PL_THREAD_RESULT_FAIL;

    #else

    *pptMutexOut = PL_ALLOC(sizeof(plMutex));
    if(pthread_mutex_init(&(*pptMutexOut)->tHandle, NULL)) //-V522
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
    #endif
}

void
pl_threads_destroy_mutex(plMutex** pptMutex)
{
    #ifdef PL_PLATFORM_WINDOWS
    CloseHandle((*pptMutex)->tHandle);
    PL_FREE((*pptMutex));
    (*pptMutex) = NULL;
    #else
    pthread_mutex_destroy(&(*pptMutex)->tHandle);
    PL_FREE((*pptMutex));
    *pptMutex = NULL;
    #endif
}

void
pl_threads_lock_mutex(plMutex* ptMutex)
{
    #ifdef PL_PLATFORM_WINDOWS
    DWORD dwWaitResult = WaitForSingleObject(ptMutex->tHandle, INFINITE);
    PL_ASSERT(dwWaitResult == WAIT_OBJECT_0);
    #else
    pthread_mutex_lock(&ptMutex->tHandle);
    #endif
}

void
pl_threads_unlock_mutex(plMutex* ptMutex)
{
    #ifdef PL_PLATFORM_WINDOWS
    if(!ReleaseMutex(ptMutex->tHandle))
    {
        printf("ReleaseMutex error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
    #else
    pthread_mutex_unlock(&ptMutex->tHandle);
    #endif
}

plThreadResult
pl_threads_create_critical_section(plCriticalSection** pptCriticalSectionOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    (*pptCriticalSectionOut) = (plCriticalSection*)PL_ALLOC(sizeof(plCriticalSection));
    InitializeCriticalSection(&(*pptCriticalSectionOut)->tHandle);
    return PL_THREAD_RESULT_SUCCESS;

    #else

    *pptCriticalSectionOut = PL_ALLOC(sizeof(plCriticalSection));
    if(pthread_mutex_init(&(*pptCriticalSectionOut)->tHandle, NULL))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
    #endif
}

void
pl_threads_destroy_critical_section(plCriticalSection** pptCriticalSection)
{
    #ifdef PL_PLATFORM_WINDOWS
    DeleteCriticalSection(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    (*pptCriticalSection) = NULL;

    #else

    pthread_mutex_destroy(&(*pptCriticalSection)->tHandle);
    PL_FREE((*pptCriticalSection));
    *pptCriticalSection = NULL;
    #endif
}

void
pl_threads_enter_critical_section(plCriticalSection* ptCriticalSection)
{
    #ifdef PL_PLATFORM_WINDOWS
    EnterCriticalSection(&ptCriticalSection->tHandle);
    #else
    pthread_mutex_lock(&ptCriticalSection->tHandle);
    #endif
}

void
pl_threads_leave_critical_section(plCriticalSection* ptCriticalSection)
{
    #ifdef PL_PLATFORM_WINDOWS
    LeaveCriticalSection(&ptCriticalSection->tHandle);
    #else
    pthread_mutex_unlock(&ptCriticalSection->tHandle);
    #endif
}

uint32_t
pl_threads_get_hardware_thread_count(void)
{
    #ifdef PL_PLATFORM_WINDOWS
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
    #else
    int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)numCPU;
    #endif
}

plThreadResult
pl_threads_create_barrier(uint32_t uThreadCount, plBarrier** pptBarrierOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    (*pptBarrierOut) = (plBarrier*)PL_ALLOC(sizeof(plBarrier));
    InitializeSynchronizationBarrier(&(*pptBarrierOut)->tHandle, uThreadCount, -1);
    return PL_THREAD_RESULT_SUCCESS;

    #else

    *pptBarrierOut = PL_ALLOC(sizeof(plBarrier));
    pthread_barrier_init(&(*pptBarrierOut)->tHandle, NULL, uThreadCount);
    return PL_THREAD_RESULT_SUCCESS;
    #endif
}

void
pl_threads_destroy_barrier(plBarrier** pptBarrier)
{
    #ifdef PL_PLATFORM_WINDOWS
    DeleteSynchronizationBarrier(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
    #else
    pthread_barrier_destroy(&(*pptBarrier)->tHandle);
    PL_FREE((*pptBarrier));
    *pptBarrier = NULL;
    #endif
}

void
pl_threads_wait_on_barrier(plBarrier* ptBarrier)
{
    #ifdef PL_PLATFORM_WINDOWS
    EnterSynchronizationBarrier(&ptBarrier->tHandle, 0);
    #else
    pthread_barrier_wait(&ptBarrier->tHandle);
    #endif
}

plThreadResult
pl_threads_create_semaphore(uint32_t uIntialCount, plSemaphore** pptSemaphoreOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    (*pptSemaphoreOut) = (plSemaphore*)PL_ALLOC(sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = CreateSemaphore(NULL, 0, uIntialCount, NULL);
    return PL_THREAD_RESULT_SUCCESS;
    #elif defined(PL_PLATFORM_LINUX)
    *pptSemaphoreOut = PL_ALLOC(sizeof(plSemaphore));
    memset((*pptSemaphoreOut), 0, sizeof(plSemaphore));
    if(sem_init(&(*pptSemaphoreOut)->tHandle, 0, uIntialCount))
    {
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
    #elif defined(PL_PLATFORM_APPLE)
    *pptSemaphoreOut = PL_ALLOC(sizeof(plSemaphore));
    memset((*pptSemaphoreOut), 0, sizeof(plSemaphore));
    (*pptSemaphoreOut)->tHandle = dispatch_semaphore_create(uIntialCount);
    return PL_THREAD_RESULT_SUCCESS;
    #endif
}

void
pl_threads_destroy_semaphore(plSemaphore** pptSemaphore)
{
    #ifdef PL_PLATFORM_WINDOWS
    CloseHandle((*pptSemaphore)->tHandle);
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
    #elif defined(PL_PLATFORM_LINUX)
    sem_destroy(&(*pptSemaphore)->tHandle);
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
    #elif defined(PL_PLATFORM_APPLE)
    PL_FREE((*pptSemaphore));
    *pptSemaphore = NULL;
    #endif
}

void
pl_threads_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    #ifdef PL_PLATFORM_WINDOWS
    WaitForSingleObject(ptSemaphore->tHandle, INFINITE);
    #elif defined(PL_PLATFORM_LINUX)
    sem_wait(&ptSemaphore->tHandle);
    #elif defined(PL_PLATFORM_APPLE)
    dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_FOREVER);
    #endif
}

bool
pl_threads_try_wait_on_semaphore(plSemaphore* ptSemaphore)
{
    #ifdef PL_PLATFORM_WINDOWS
    DWORD dwWaitResult = WaitForSingleObject(ptSemaphore->tHandle, 0);
    switch (dwWaitResult)
    {
        case WAIT_OBJECT_0: return true;
        case WAIT_TIMEOUT:  return false;
    }
    PL_ASSERT(false);
    return false;
    #elif defined(PL_PLATFORM_LINUX)
    return sem_trywait(&ptSemaphore->tHandle) == 0;
    #elif defined(PL_PLATFORM_APPLE)
    return dispatch_semaphore_wait(ptSemaphore->tHandle, DISPATCH_TIME_NOW) == 0;
    #endif
}

void
pl_threads_release_semaphore(plSemaphore* ptSemaphore)
{
    #ifdef PL_PLATFORM_WINDOWS
    if (!ReleaseSemaphore( 
            ptSemaphore->tHandle,  // handle to semaphore
            1,            // increase count by one
            NULL) )       // not interested in previous count
    {
        printf("ReleaseSemaphore error: %d\n", GetLastError());
        PL_ASSERT(false);
    }
    #elif defined(PL_PLATFORM_LINUX)
    sem_post(&ptSemaphore->tHandle);
    #elif defined(PL_PLATFORM_APPLE)
    dispatch_semaphore_signal(ptSemaphore->tHandle);
    #endif
}

plThreadResult
pl_threads_allocate_thread_local_key(plThreadKey** pptKeyOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    *pptKeyOut = (plThreadKey*)PL_ALLOC(sizeof(plThreadKey));
    memset(*pptKeyOut, 0, sizeof(plThreadKey));
    (*pptKeyOut)->dwIndex = TlsAlloc();
    return PL_THREAD_RESULT_SUCCESS;

    #else

    *pptKeyOut = PL_ALLOC(sizeof(plThreadKey));
    int iStatus = pthread_key_create(&(*pptKeyOut)->tKey, NULL);
    if(iStatus != 0)
    {
        printf("pthread_key_create failed, errno=%d", errno);
        PL_ASSERT(false);
        return PL_THREAD_RESULT_FAIL;
    }
    return PL_THREAD_RESULT_SUCCESS;
    #endif
}

void
pl_threads_free_thread_local_key(plThreadKey** pptKey)
{
    #ifdef PL_PLATFORM_WINDOWS
    TlsFree((*pptKey)->dwIndex);
    PL_FREE((*pptKey));
    *pptKey = NULL;
    #else
    pthread_key_delete((*pptKey)->tKey);
    PL_FREE((*pptKey));
    *pptKey = NULL;
    #endif
}

void*
pl_threads_allocate_thread_local_data(plThreadKey* ptKey, size_t szSize)
{
    #ifdef PL_PLATFORM_WINDOWS
    LPVOID lpvData = LocalAlloc(LPTR, szSize);
    if(!TlsSetValue(ptKey->dwIndex, lpvData)) 
    {
        PL_ASSERT(false);
        return NULL;
    }
    return lpvData;
    #else
    void* pData = PL_ALLOC(szSize);
    memset(pData, 0, szSize);
    pthread_setspecific(ptKey->tKey, pData);
    return pData;
    #endif
}

void*
pl_threads_get_thread_local_data(plThreadKey* ptKey)
{
    #ifdef PL_PLATFORM_WINDOWS
    LPVOID lpvData =  TlsGetValue(ptKey->dwIndex);
    if(lpvData == NULL)
    {
        PL_ASSERT(false);
    }
    return lpvData;
    #else
    void* pData = pthread_getspecific(ptKey->tKey);
    return pData;
    #endif
}

uint64_t
pl_threads_get_thread_id(plThread* ptThread)
{
    #ifdef PL_PLATFORM_WINDOWS
    return ptThread->uID;
    #else
    return ptThread->uID;
    #endif
}

uint64_t
pl_threads_get_current_thread_id(void)
{
    #ifdef PL_PLATFORM_WINDOWS
    DWORD tID = GetCurrentThreadId();
    return (uint64_t)tID;
    #else
    pthread_t tId = pthread_self();

    const uint32_t uThreadCount = pl_sb_size(gsbtThreads);
    for(uint32_t i = 0; i < uThreadCount; i++)
    {
        if(pthread_equal(tId, gsbtThreads[i]->tHandle))
        {
            return gsbtThreads[i]->uID;
        }
    }

    return UINT64_MAX;
    #endif
}

void
pl_threads_free_thread_local_data(plThreadKey* ptKey, void* pData)
{
    #ifdef PL_PLATFORM_WINDOWS
    LPVOID lpvData = TlsGetValue(ptKey->dwIndex);
    LocalFree(lpvData);
    #else
    PL_FREE(pData);
    #endif
}

plThreadResult
pl_threads_create_condition_variable(plConditionVariable** pptConditionVariableOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    *pptConditionVariableOut =(plConditionVariable*)PL_ALLOC(sizeof(plConditionVariable));
    InitializeConditionVariable(&(*pptConditionVariableOut)->tHandle);
    return PL_THREAD_RESULT_SUCCESS;
    #else
    *pptConditionVariableOut = PL_ALLOC(sizeof(plConditionVariable));
    pthread_cond_init(&(*pptConditionVariableOut)->tHandle, NULL);
    return PL_THREAD_RESULT_SUCCESS;
    #endif
}

void               
pl_threads_destroy_condition_variable(plConditionVariable** pptConditionVariable)
{
    #ifdef PL_PLATFORM_WINDOWS
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
    #else
    pthread_cond_destroy(&(*pptConditionVariable)->tHandle);
    PL_FREE((*pptConditionVariable));
    *pptConditionVariable = NULL;
    #endif
}

void               
pl_threads_wake_condition_variable(plConditionVariable* ptConditionVariable)
{
    #ifdef PL_PLATFORM_WINDOWS
    WakeConditionVariable(&ptConditionVariable->tHandle);
    #else
    pthread_cond_signal(&ptConditionVariable->tHandle);
    #endif
}

void               
pl_threads_wake_all_condition_variable(plConditionVariable* ptConditionVariable)
{
    #ifdef PL_PLATFORM_WINDOWS
    WakeAllConditionVariable(&ptConditionVariable->tHandle);
    #else
    if(ptConditionVariable)
        pthread_cond_broadcast(&ptConditionVariable->tHandle);
    #endif
}

void               
pl_threads_sleep_condition_variable(plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection)
{
    #ifdef PL_PLATFORM_WINDOWS
    SleepConditionVariableCS(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle, INFINITE);
    #else
    pthread_cond_wait(&ptConditionVariable->tHandle, &ptCriticalSection->tHandle);
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] virtual memory api
//-----------------------------------------------------------------------------

size_t
pl_virtual_memory_get_page_size(void)
{
    #ifdef PL_PLATFORM_WINDOWS
    SYSTEM_INFO tInfo = {0};
    GetSystemInfo(&tInfo);
    return (size_t)tInfo.dwPageSize;
    #else
    return (size_t)getpagesize();
    #endif
}

void*
pl_virtual_memory_alloc(size_t szSize)
{
    #ifdef PL_PLATFORM_WINDOWS
    return VirtualAlloc(NULL, szSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    #else
    void* pResult = mmap(NULL, szSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pResult == MAP_FAILED ? NULL : pResult;
    #endif
}

void*
pl_virtual_memory_reserve(size_t szSize)
{
    #ifdef PL_PLATFORM_WINDOWS
    return VirtualAlloc(NULL, szSize, MEM_RESERVE, PAGE_NOACCESS);
    #else
    void* pResult = mmap(NULL, szSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pResult == MAP_FAILED ? NULL : pResult;
    #endif
}

void*
pl_virtual_memory_commit(void* pAddress, size_t szSize)
{
    #ifdef PL_PLATFORM_WINDOWS
    return VirtualAlloc(pAddress, szSize, MEM_COMMIT, PAGE_READWRITE);
    #else
    if(mprotect(pAddress, szSize, PROT_READ | PROT_WRITE) != 0)
    {
        PL_ASSERT(false);
        return NULL;
    }

    return pAddress;
    #endif
}

void
pl_virtual_memory_free(void* pAddress, size_t szSize)
{
    #ifdef PL_PLATFORM_WINDOWS
    BOOL bResult = VirtualFree(pAddress, 0, MEM_RELEASE);
    if(!bResult)
    {
        printf("VirtualFree failed : %d\n", GetLastError());
        PL_ASSERT(false);
    };
    #else
    if(munmap(pAddress, szSize) != 0)
    {
        PL_ASSERT(false);
    }
    #endif
}

void
pl_virtual_memory_decommit(void* pAddress, size_t szSize)
{
    #ifdef PL_PLATFORM_WINDOWS
    BOOL bResult = VirtualFree(pAddress, szSize, MEM_DECOMMIT);
    if(!bResult)
    {
        printf("VirtualFree failed : %d\n", GetLastError());
        PL_ASSERT(false);
    };
    #else
    #ifdef PL_PLATFORM_APPLE
    int iResult = madvise(pAddress, szSize, MADV_FREE);
    #else
    int iResult = madvise(pAddress, szSize, MADV_DONTNEED);
    #endif
    PL_ASSERT(iResult == 0);

    iResult = mprotect(pAddress, szSize, PROT_NONE);
    PL_ASSERT(iResult == 0);
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] file api
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
pl_file_remove(const char* pcFile)
{
    #ifdef PL_PLATFORM_WINDOWS
    BOOL bResult = DeleteFile(pcFile);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
    #else
    int iResult = remove(pcFile);
    if(iResult)
        return PL_FILE_RESULT_FAIL;
    return PL_FILE_RESULT_SUCCESS;
    #endif
}

plFileResult
pl_file_binary_read(const char* pcFile, size_t* pszSizeIn, uint8_t* pcBuffer)
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
pl_file_binary_write(const char* pcFile, size_t szSize, uint8_t* pcBuffer)
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
pl_file_copy(const char* pcSource, const char* pcDestination)
{
    #ifdef PL_PLATFORM_WINDOWS
    BOOL bResult = CopyFile(pcSource, pcDestination, FALSE);
    if(bResult)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
    #elif defined(PL_PLATFORM_LINUX)
    size_t bufferSize = 0u;
    pl_file_binary_read(pcSource, &bufferSize, NULL);

    struct stat stat_buf;
    int fromfd = open(pcSource, O_RDONLY);
    fstat(fromfd, &stat_buf);
    int tofd = open(pcDestination, O_WRONLY | O_CREAT, stat_buf.st_mode);
    int n = 1;
    while (n > 0)
        n = sendfile(tofd, fromfd, 0, bufferSize * 2);
    return PL_FILE_RESULT_SUCCESS;
    #elif defined(PL_PLATFORM_APPLE)
    copyfile_state_t tS = copyfile_state_alloc();
    copyfile(pcSource, pcDestination, tS, COPYFILE_XATTR | COPYFILE_DATA);
    copyfile_state_free(tS);
    return PL_FILE_RESULT_SUCCESS;
    #endif
}

bool
pl_file_directory_exists(const char* pcPath)
{
    #ifdef PL_PLATFORM_WINDOWS
    DWORD dwAttrib = GetFileAttributes(pcPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    #else
    struct stat st = {0};

    if (stat(pcPath, &st) == -1)
        return false;
    return true;
    #endif
}

plFileResult
pl_file_create_directory(const char* pcPath)
{
    #ifdef PL_PLATFORM_WINDOWS
    if(pl_file_directory_exists(pcPath))
        return PL_FILE_DIRECTORY_ALREADY_EXIST;
    BOOL bResult = CreateDirectoryA(pcPath, NULL);
    if(bResult != 0)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
    #else
    struct stat st = {0};

    if (stat(pcPath, &st) == -1)
    {
        mkdir(pcPath, 0700);
        return PL_FILE_RESULT_SUCCESS;
    }
    return PL_FILE_DIRECTORY_ALREADY_EXIST;
    #endif
}

plFileResult
pl_file_remove_directory(const char* pcPath)
{
    #ifdef PL_PLATFORM_WINDOWS
    BOOL bResult = RemoveDirectoryA(pcPath);
    if(bResult != 0)
        return PL_FILE_RESULT_SUCCESS;
    return PL_FILE_RESULT_FAIL;
    #else
    if(pl_file_directory_exists(pcPath))
    {
        rmdir(pcPath);
        return PL_FILE_RESULT_SUCCESS;
    }
    return PL_FILE_RESULT_FAIL;
    #endif
}

void
pl_file_cleanup_directory_info(plDirectoryInfo* ptInfoOut)
{
    pl_sb_free(ptInfoOut->sbtEntries);
    ptInfoOut->uEntryCount = 0;
}

plFileResult
pl_file_get_directory_info(const char* pcPath, plDirectoryInfo* ptInfoOut)
{
    #ifdef PL_PLATFORM_WINDOWS
    char acFixedName[PL_MAX_PATH_LENGTH] = {0};
    size_t szLen = strnlen(pcPath, PL_MAX_PATH_LENGTH);
    if(pcPath[szLen - 1] == '/')
        sprintf(acFixedName, "%s*", pcPath);
    else
        sprintf(acFixedName, "%s/*", pcPath);
    WIN32_FIND_DATAA tFindData = {0};
    HANDLE tFoundHandle = FindFirstFileA(acFixedName, &tFindData); // should be "."

    BOOL bResult = FindNextFileA(tFoundHandle, &tFindData); // should be ".."
    bResult = FindNextFileA(tFoundHandle, &tFindData);

    while(bResult != 0)
    {
        

        pl_sb_add(ptInfoOut->sbtEntries);
        plDirectoryEntry* ptNewEntry = &pl_sb_top(ptInfoOut->sbtEntries);

        if(tFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ptInfoOut->uDirectoryCount++;
            ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_DIRECTORY;
        }
        else
        {
            ptInfoOut->uFileCount++;
            ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_FILE;
        }
        strncpy(ptNewEntry->acName, tFindData.cFileName, PL_MAX_PATH_LENGTH);

        // (nFileSizeHigh * (MAXDWORD+1)) + nFileSizeLow

        bResult = FindNextFileA(tFoundHandle, &tFindData);
    }
    ptInfoOut->uEntryCount = pl_sb_size(ptInfoOut->sbtEntries);
    FindClose(tFoundHandle);
    return PL_FILE_RESULT_SUCCESS;

    #else

    DIR* ptDirectoryPath = opendir(pcPath);
    struct dirent* ptEntry = NULL;

    if (ptDirectoryPath == NULL)
    {
        perror("Error opening directory");
        return PL_FILE_RESULT_FAIL;
    }

    // Read directory entries
    while ((ptEntry = readdir(ptDirectoryPath)) != NULL)
    {
        // Skip "." and ".." entries (current and parent directory)
        if (strcmp(ptEntry->d_name, ".") == 0 || strcmp(ptEntry->d_name, "..") == 0)
        {
            continue;
        }

        pl_sb_add(ptInfoOut->sbtEntries);
        plDirectoryEntry* ptNewEntry = &pl_sb_top(ptInfoOut->sbtEntries);

        switch(ptEntry->d_type)
        {
            case DT_REG: ptInfoOut->uFileCount++; ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_FILE; break;
            case DT_DIR: ptInfoOut->uDirectoryCount++; ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_DIRECTORY; break;
            case DT_LNK: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_LINK; break;
            case DT_FIFO: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_PIPE; break;
            case DT_SOCK: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_SOCKET; break;
            case DT_BLK: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_BLOCK_DEVICE; break;
            case DT_CHR: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_CHARACTER_DEVICE; break;
            case DT_UNKNOWN: ptNewEntry->eType = PL_DIRECTORY_ENTRY_TYPE_UNKNOWN; break;
            default:
                PL_ASSERT(false && "unknown dirent file type");
                break;
        }

        strncpy(ptNewEntry->acName, ptEntry->d_name, PL_MAX_PATH_LENGTH);
    }

    if (closedir(ptDirectoryPath) == -1)
    {
        perror("Error closing directory");
        return PL_FILE_RESULT_FAIL;
    }

    ptInfoOut->uEntryCount = pl_sb_size(ptInfoOut->sbtEntries);
    return PL_FILE_RESULT_SUCCESS;
    #endif
}

//-----------------------------------------------------------------------------
// [SECTION] network api
//-----------------------------------------------------------------------------

typedef struct _plNetworkAddress
{
    struct addrinfo* tInfo;
} plNetworkAddress;

#ifndef PL_PLATFORM_WINDOWS
#define SOCKET int
#endif
typedef struct _plSocket
{
    SOCKET tSocket;
    bool     bInitialized;
    plSocketFlags tFlags;
} plSocket;

bool
pl_network_initialize(void)
{
    #ifdef PL_PLATFORM_WINDOWS
    WSADATA tWsaData = {0};
    if(WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0)
    {
        printf("Failed to start winsock with error code: %d\n", WSAGetLastError());
        return false;
    }
    #endif
    return true;
}

void
pl_network_cleanup(void)
{
    #ifdef PL_PLATFORM_WINDOWS
        WSACleanup();
    #endif
}

plNetworkResult
pl_network_create_address(const char* pcAddress, const char* pcService, plNetworkAddressFlags tFlags, plNetworkAddress** pptAddress)
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
        #ifdef PL_PLATFORM_WINDOWS
        printf("Could not create address : %d\n", WSAGetLastError());
        #else
        printf("Could not create address : %d\n", errno);
        #endif
        return PL_NETWORK_RESULT_FAIL;
    }

    *pptAddress = (plNetworkAddress*)PL_ALLOC(sizeof(plNetworkAddress));
    (*pptAddress)->tInfo = tInfo;
    return PL_NETWORK_RESULT_SUCCESS;
}

void
pl_network_destroy_address(plNetworkAddress** pptAddress)
{
    plNetworkAddress* ptAddress = *pptAddress;
    if(ptAddress == NULL)
        return;

    freeaddrinfo(ptAddress->tInfo);
    PL_FREE(ptAddress);
    *pptAddress = NULL;
}

void
pl_network_create_socket(plSocketFlags tFlags, plSocket** pptSocketOut)
{
    *pptSocketOut = (plSocket*)PL_ALLOC(sizeof(plSocket));
    plSocket* ptSocket = *pptSocketOut;
    ptSocket->bInitialized = false;
    ptSocket->tFlags = tFlags;
}

void
pl_network_destroy_socket(plSocket** pptSocket)
{
    plSocket* ptSocket = *pptSocket;

    if(ptSocket == NULL)
        return;

    #ifdef PL_PLATFORM_WINDOWS
    closesocket(ptSocket->tSocket);
    #else
    close(ptSocket->tSocket);
    #endif

    PL_FREE(ptSocket);
    *pptSocket = NULL;
}

plNetworkResult
pl_network_send_socket_data_to(plSocket* ptFromSocket, plNetworkAddress* ptAddress, const void* pData, size_t szSize, size_t* pszSentSize)
{

    if(!ptFromSocket->bInitialized)
    {
        
        ptFromSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        #ifdef PL_PLATFORM_WINDOWS
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

        #else

        if(ptFromSocket->tSocket < 0) // invalid socket
        {
            printf("Could not create socket : %d\n", errno);
            return 0;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            int iFlags = fcntl(ptFromSocket->tSocket, F_GETFL);
            fcntl(ptFromSocket->tSocket, F_SETFL, iFlags | O_NONBLOCK);
        }

        #endif

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = sendto(ptFromSocket->tSocket, (const char*)pData, (int)szSize, 0, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);

    #ifdef PL_PLATFORM_WINDOWS
    if(iResult == SOCKET_ERROR)
    {
        printf("sendto() failed with error code : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }
    #endif

    if(pszSentSize)
        *pszSentSize = (size_t)iResult;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_bind_socket(plSocket* ptSocket, plNetworkAddress* ptAddress)
{
    if(!ptSocket->bInitialized)
    {
        
        ptSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        #ifdef PL_PLATFORM_WINDOWS
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
        #else
        if(ptSocket->tSocket < 0)
        {
            printf("Could not create socket : %d\n", errno);
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            int iFlags = fcntl(ptSocket->tSocket, F_GETFL);
            fcntl(ptSocket->tSocket, F_SETFL, iFlags | O_NONBLOCK);
        }
        #endif

        ptSocket->bInitialized = true;
    }

    // bind socket
    #ifdef PL_PLATFORM_WINDOWS
    if(bind(ptSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen) == SOCKET_ERROR)
    {
        printf("Bind socket failed with error code : %d\n", WSAGetLastError());
        return PL_NETWORK_RESULT_FAIL;
    }
    #else
    if(bind(ptSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen))
    {
        printf("Bind socket failed with error code : %d\n", errno);
        return PL_NETWORK_RESULT_FAIL;
    }
    #endif
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_get_socket_data_from(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize, plSocketReceiverInfo* ptReceiverInfo)
{
    struct sockaddr_storage tClientAddress = {0};
    socklen_t tClientLen = sizeof(tClientAddress);

    int iRecvLen = recvfrom(ptSocket->tSocket, (char*)pData, (int)szSize, 0, (struct sockaddr*)&tClientAddress, &tClientLen);
   
    #ifdef PL_PLATFORM_WINDOWS
    if(iRecvLen == SOCKET_ERROR)
    {
        const int iLastError = WSAGetLastError();
        if(iLastError != WSAEWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
            return PL_NETWORK_RESULT_FAIL;
        }
    }
    #else
    if(iRecvLen == -1)
    {
        if(errno != EWOULDBLOCK)
        {
            printf("recvfrom() failed with error code : %d\n", errno);
            return PL_NETWORK_RESULT_FAIL;
        }
    }
    #endif

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
pl_network_connect_socket(plSocket* ptFromSocket, plNetworkAddress* ptAddress)
{

    if(!ptFromSocket->bInitialized)
    {
        
        ptFromSocket->tSocket = socket(ptAddress->tInfo->ai_family, ptAddress->tInfo->ai_socktype, ptAddress->tInfo->ai_protocol);

        #ifdef PL_PLATFORM_WINDOWS
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
        #else
        if(ptFromSocket->tSocket < 0)
        {
            printf("Could not create socket : %d\n", errno);
            return PL_NETWORK_RESULT_FAIL;
        }

        // enable non-blocking
        if(ptFromSocket->tFlags & PL_SOCKET_FLAGS_NON_BLOCKING)
        {
            int iFlags = fcntl(ptFromSocket->tSocket, F_GETFL);
            fcntl(ptFromSocket->tSocket, F_SETFL, iFlags | O_NONBLOCK);
        }
        #endif

        ptFromSocket->bInitialized = true;
    }

    // send
    int iResult = connect(ptFromSocket->tSocket, ptAddress->tInfo->ai_addr, (int)ptAddress->tInfo->ai_addrlen);
    if(iResult)
    {
        #ifdef PL_PLATFORM_WINDOWS
        printf("connect() failed with error code : %d\n", WSAGetLastError());
        #else
        printf("connect() failed with error code : %d\n", errno);
        #endif
        return PL_NETWORK_RESULT_FAIL;
    }

    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_get_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszRecievedSize)
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
pl_network_select_sockets(plSocket** ptSockets, bool* abSelectedSockets, uint32_t uSocketCount, uint32_t uTimeOutMilliSec)
{
    #ifdef PL_PLATFORM_WINDOWS
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

    #else

    SOCKET tMaxSocket = 0;
    fd_set tReads;
    FD_ZERO(&tReads);
    for(uint32_t i = 0; i < uSocketCount; i++)
    {
        FD_SET(ptSockets[i]->tSocket, &tReads);
        if(ptSockets[i]->tSocket > tMaxSocket)
            tMaxSocket = ptSockets[i]->tSocket;
    }

    struct timeval tTimeout = {0};
    tTimeout.tv_sec = 0;
    tTimeout.tv_usec = (int)uTimeOutMilliSec * 1000;

    if(select(tMaxSocket + 1, &tReads, NULL, NULL, &tTimeout) < 0)
    {
        printf("select socket failed with error code : %d\n", errno);
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
    #endif
}

plNetworkResult
pl_network_accept_socket(plSocket* ptSocket, plSocket** pptSocketOut)
{
    *pptSocketOut = NULL; 
    struct sockaddr_storage tClientAddress = {0};
    socklen_t tClientLen = sizeof(tClientAddress);
    SOCKET tSocketClient = accept(ptSocket->tSocket, (struct sockaddr*)&tClientAddress, &tClientLen);

    #ifdef PL_PLATFORM_WINDOWS
    if(tSocketClient == INVALID_SOCKET)
        return PL_NETWORK_RESULT_FAIL;
    #else
    if(tSocketClient < 1)
        return PL_NETWORK_RESULT_FAIL;
    #endif

    *pptSocketOut = (plSocket*)PL_ALLOC(sizeof(plSocket));
    plSocket* ptNewSocket = *pptSocketOut;
    ptNewSocket->bInitialized = true;
    ptNewSocket->tFlags = ptSocket->tFlags;
    ptNewSocket->tSocket = tSocketClient;
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_listen_socket(plSocket* ptSocket)
{
    if(listen(ptSocket->tSocket, 10) < 0)
    {
        return PL_NETWORK_RESULT_FAIL;
    }
    return PL_NETWORK_RESULT_SUCCESS;
}

plNetworkResult
pl_network_send_socket_data(plSocket* ptSocket, void* pData, size_t szSize, size_t* pszSentSize)
{
    int iResult = send(ptSocket->tSocket, (char*)pData, (int)szSize, 0);
    #ifdef PL_PLATFORM_WINDOWS
    if(iResult == SOCKET_ERROR)
        return PL_NETWORK_RESULT_FAIL;
    #else
    if(iResult == -1)
        return PL_NETWORK_RESULT_FAIL;
    #endif
    if(pszSentSize)
        *pszSentSize = (size_t)iResult;
    return PL_NETWORK_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
// [SECTION] platform functions
//-----------------------------------------------------------------------------

void*
pl_platform_setup(void)
{
    return gptPlatformExtCtx;
}

void
pl_platform_new_frame(void* pPlatformData)
{
    // pl__update_mouse_cursor();
}

void
pl_platform_cleanup(void* pPlatformData)
{
    plPlatformExtData* ptPlatformData = (plPlatformExtData*)pPlatformData;
    while(pl_sb_size(ptPlatformData->sbtWindows) > 0)
        pl_window_destroy(pl_sb_last(ptPlatformData->sbtWindows));

    if(ptPlatformData)
        pl_sb_free(ptPlatformData->sbtWindows);
    pl_sb_free(gsbtThreads);
    gptPlatformExtCtx = NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_platform_ext.c"
