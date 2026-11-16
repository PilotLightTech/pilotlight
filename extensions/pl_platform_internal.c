/* Shared binary file/stream implementation for Win32, X11, Cocoa and null.
   Define _FILE_OFFSET_BITS=64 before system headers on 32-bit POSIX targets. */
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif
#include "pl_platform_ext.h"

static inline bool
pl__window_icons_valid(uint32_t uCount, const plWindowIcon* atIcons)
{
    if(uCount > 16 || (uCount && !atIcons))
        return false;
    for(uint32_t i = 0; i < uCount; i++)
    {
        if(!atIcons[i].pucPixels || !atIcons[i].uWidth || !atIcons[i].uHeight || atIcons[i].uWidth > 256 || atIcons[i].uHeight > 256)
            return false;
    }
    return true;
}

static int
pl__file_seek(FILE* ptFile, int64_t iOffset, int iOrigin)
{
#ifdef _WIN32
    return _fseeki64(ptFile, iOffset, iOrigin);
#else
    typedef char plFileRequires64BitOffT[sizeof(off_t) >= 8 ? 1 : -1];
    return fseeko(ptFile, (off_t)iOffset, iOrigin);
#endif
}

static int64_t
pl__file_tell(FILE* ptFile)
{
#ifdef _WIN32
    return _ftelli64(ptFile);
#else
    return (int64_t)ftello(ptFile);
#endif
}

static plFileStream*
pl_file_open_stream(const char* pcPath, plFileMode tMode)
{
    static const char* apcModes[] = {"rb", "wb", "ab", "rb+", "wb+", "ab+"};
    if(!pcPath || tMode < PL_FILE_MODE_READ || tMode > PL_FILE_MODE_APPEND_READ)
        return NULL;
    return (plFileStream*)fopen(pcPath, apcModes[tMode]);
}

// Create exclusively beside the destination so replacement stays on one filesystem.
static plFileStream*
pl_file_open_temp_stream(const char* pcDestination, char* pcPathOut, size_t szCapacity)
{
    if(!pcDestination || !pcPathOut || !szCapacity) return NULL;
    pcPathOut[0] = 0;
    for(unsigned i = 0; i < 1024; i++)
    {
#ifdef _WIN32
        unsigned uPid = (unsigned)_getpid();
#else
        unsigned uPid = (unsigned)getpid();
#endif
        int n = snprintf(pcPathOut, szCapacity, "%s.tmp.%u.%u", pcDestination, uPid, i);
        if(n < 0 || (size_t)n >= szCapacity) { pcPathOut[0] = 0; return NULL; }
#ifdef _WIN32
        int fd = _open(pcPathOut, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
        int fd = open(pcPathOut, O_CREAT | O_EXCL | O_RDWR, 0666);
#endif
        if(fd < 0)
        {
            if(errno == EEXIST) continue;
            pcPathOut[0] = 0;
            return NULL;
        }
#ifdef _WIN32
        FILE* f = _fdopen(fd, "wb+");
        if(!f) _close(fd);
#else
        FILE* f = fdopen(fd, "wb+");
        if(!f) close(fd);
#endif
        if(!f) { remove(pcPathOut); pcPathOut[0] = 0; }
        return (plFileStream*)f;
    }
    pcPathOut[0] = 0;
    return NULL;
}

static plFileResult
pl_file_replace_file(const char* pcSource, const char* pcDestination)
{
    if(!pcSource || !pcDestination) return PL_FILE_RESULT_FAIL;
#ifdef _WIN32
    return MoveFileExA(pcSource, pcDestination, MOVEFILE_REPLACE_EXISTING) ? PL_FILE_RESULT_SUCCESS : PL_FILE_RESULT_FAIL;
#else
    // Replacement should retain the destination's access permissions. A new
    // destination keeps the creation permissions already filtered by umask.
    struct stat tDestination;
    if(stat(pcDestination, &tDestination) == 0)
    {
        if(chmod(pcSource, tDestination.st_mode & 0777) != 0)
            return PL_FILE_RESULT_FAIL;
    }
    else if(errno != ENOENT)
        return PL_FILE_RESULT_FAIL;
    return rename(pcSource, pcDestination) == 0 ? PL_FILE_RESULT_SUCCESS : PL_FILE_RESULT_FAIL;
#endif
}

static plFileResult
pl_file_close_stream(plFileStream* ptStream)
{
    return ptStream && fclose((FILE*)ptStream) == 0 ? PL_FILE_RESULT_SUCCESS : PL_FILE_RESULT_FAIL;
}

static size_t
pl_file_read_stream(plFileStream* ptStream, void* pBuffer, size_t szBytes)
{
    return ptStream && pBuffer && szBytes ? fread(pBuffer, 1, szBytes, (FILE*)ptStream) : 0;
}

static size_t
pl_file_write_stream(plFileStream* ptStream, const void* pBuffer, size_t szBytes)
{
    return ptStream && pBuffer && szBytes ? fwrite(pBuffer, 1, szBytes, (FILE*)ptStream) : 0;
}

static plFileResult
pl_file_seek_stream(plFileStream* ptStream, int64_t iOffset, plFileSeekOrigin tOrigin)
{
    static const int aiOrigins[] = {SEEK_SET, SEEK_CUR, SEEK_END};
    if(!ptStream || tOrigin < PL_FILE_SEEK_START || tOrigin > PL_FILE_SEEK_END)
        return PL_FILE_RESULT_FAIL;
    return pl__file_seek((FILE*)ptStream, iOffset, aiOrigins[tOrigin]) == 0 ? PL_FILE_RESULT_SUCCESS : PL_FILE_RESULT_FAIL;
}

static plFileResult
pl_file_tell_stream(plFileStream* ptStream, uint64_t* puPosition)
{
    if(!puPosition)
        return PL_FILE_RESULT_FAIL;
    *puPosition = 0;
    if(!ptStream)
        return PL_FILE_RESULT_FAIL;
    int64_t iPosition = pl__file_tell((FILE*)ptStream);
    if(iPosition < 0)
        return PL_FILE_RESULT_FAIL;
    *puPosition = (uint64_t)iPosition;
    return PL_FILE_RESULT_SUCCESS;
}

static plFileResult
pl_file_size_stream(plFileStream* ptStream, uint64_t* puSize)
{
    uint64_t uPosition = 0;
    if(!puSize)
        return PL_FILE_RESULT_FAIL;
    *puSize = 0;
    if(!pl_file_tell_stream(ptStream, &uPosition) || !pl_file_seek_stream(ptStream, 0, PL_FILE_SEEK_END))
        return PL_FILE_RESULT_FAIL;
    plFileResult tResult = pl_file_tell_stream(ptStream, puSize);
    if(!pl_file_seek_stream(ptStream, (int64_t)uPosition, PL_FILE_SEEK_START))
        return PL_FILE_RESULT_FAIL;
    return tResult;
}

static plFileResult
pl_file_flush_stream(plFileStream* ptStream)
{
    return ptStream && fflush((FILE*)ptStream) == 0 ? PL_FILE_RESULT_SUCCESS : PL_FILE_RESULT_FAIL;
}

plFileResult
pl_file_binary_read(const char* pcPath, size_t* pszSize, uint8_t* puBuffer)
{
    if(!pszSize)
        return PL_FILE_RESULT_FAIL;
    plFileStream* ptStream = pl_file_open_stream(pcPath, PL_FILE_MODE_READ);
    uint64_t uSize = 0;
    bool bSuccess = pl_file_size_stream(ptStream, &uSize) == PL_FILE_RESULT_SUCCESS && uSize <= SIZE_MAX;
    *pszSize = bSuccess ? (size_t)uSize : 0;
    if(bSuccess && puBuffer && uSize)
        bSuccess = pl_file_read_stream(ptStream, puBuffer, (size_t)uSize) == uSize;
    if(ptStream && !pl_file_close_stream(ptStream))
        bSuccess = false;
    return bSuccess ? PL_FILE_RESULT_SUCCESS : PL_FILE_RESULT_FAIL;
}

plFileResult
pl_file_binary_write(const char* pcPath, size_t szSize, uint8_t* puBuffer)
{
    if(szSize && !puBuffer)
        return PL_FILE_RESULT_FAIL;
    plFileStream* ptStream = pl_file_open_stream(pcPath, PL_FILE_MODE_WRITE);
    if(!ptStream)
        return PL_FILE_RESULT_FAIL;
    bool bSuccess = pl_file_write_stream(ptStream, puBuffer, szSize) == szSize;
    if(!pl_file_close_stream(ptStream))
        bSuccess = false;
    return bSuccess ? PL_FILE_RESULT_SUCCESS : PL_FILE_RESULT_FAIL;
}
