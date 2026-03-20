/*
   pl_stage_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations & basic types
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STAGE_EXT_H
#define PL_STAGE_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plStageI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plStageInit          plStageInit;

// typedef struct _plStagingInit           plStagingInit;
// typedef struct _plStagingAllocator      plStagingAllocator;
// typedef struct _plStagingPage           plStagingPage;
// typedef struct _plStagingAllocation     plStagingAllocation;
// typedef struct _plStagingCopyBufferInfo plStagingCopyBufferInfo;

// // engine types
// typedef uint32_t plHandle;
// typedef plHandle plBufferHandle;
// typedef plHandle plCommandBufferHandle;

// external
typedef struct _plDevice          plDevice;          // pl_graphics_ext.h
typedef struct _plBufferImageCopy plBufferImageCopy; // pl_graphics_ext.h
typedef union plBufferHandle      plBufferHandle;    // pl_graphics_ext.h
typedef union plTextureHandle     plTextureHandle;   // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plStageI
{

    // setup/shutdown
    void (*initialize)(plStageInit);
    void (*cleanup)   (void);

    // per frame
    void (*new_frame)(void);

    // queuing
    uint64_t (*queue_buffer_upload)(plBufferHandle, uint64_t offset, const void* data, uint64_t size);
    uint64_t (*queue_texture_upload)(plTextureHandle, const plBufferImageCopy*, const void* data, uint64_t size, bool generateMips);
    bool     (*completed)          (uint64_t);


    // staging
    void (*stage_buffer_upload) (plBufferHandle, uint64_t offset, const void* data, uint64_t size);
    void (*stage_texture_upload)(plTextureHandle, const plBufferImageCopy*, const void* data, uint64_t size, bool generateMips);
    void (*flush)               (void);
} plStageI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plStageInit
{
    plDevice* ptDevice;
} plStageInit;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

// typedef enum _plStagingFlags
// {
//     PL_STAGING_FLAGS_NONE              = 0,
//     PL_STAGING_FLAGS_PERSISTENT_MAP    = 1 << 0,
//     PL_STAGING_FLAGS_HOST_COHERENT     = 1 << 1,
//     PL_STAGING_FLAGS_RING              = 1 << 2,
//     PL_STAGING_FLAGS_GROWABLE          = 1 << 3
// } plStagingFlags;

#endif // PL_STAGE_EXT_H