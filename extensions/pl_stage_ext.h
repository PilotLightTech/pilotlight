/*
   pl_stage_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STAGE_EXT_H
#define PL_STAGE_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
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

// external
typedef struct _plDevice          plDevice;          // pl_graphics_ext.h
typedef struct _plBufferImageCopy plBufferImageCopy; // pl_graphics_ext.h
typedef union plBufferHandle      plBufferHandle;    // pl_graphics_ext.h
typedef union plTextureHandle     plTextureHandle;   // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_stage_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_stage_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void     pl_stage_initialize(plStageInit);
PL_API void     pl_stage_cleanup   (void);

// per frame
PL_API void     pl_stage_new_frame(void);

// queuing
PL_API uint64_t pl_stage_queue_buffer_upload (plBufferHandle, uint64_t offset, const void* data, uint64_t size);
PL_API uint64_t pl_stage_queue_texture_upload(plTextureHandle, const plBufferImageCopy*, const void* data, uint64_t size, bool generateMips);
PL_API bool     pl_stage_completed           (uint64_t);

// staging
PL_API void     pl_stage_stage_buffer_upload (plBufferHandle, uint64_t offset, const void* data, uint64_t size);
PL_API void     pl_stage_stage_texture_upload(plTextureHandle, const plBufferImageCopy*, const void* data, uint64_t size, bool generateMips);
PL_API void     pl_stage_flush               (void);

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

#ifdef __cplusplus
}
#endif

#endif // PL_STAGE_EXT_H