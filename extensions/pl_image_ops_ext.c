/*
   pl_image_ops_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include <math.h> // pow
#include "pl.h"
#include "pl_image_ops_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plImageOpRegion
{
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    uint64_t uDataSize;
    uint8_t* puData;
} plImageOpRegion;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline void
pl__region_union(plImageOpRegion* out, const plImageOpRegion* a, const plImageOpRegion* b)
{
    const int32_t x0 = (a->x < b->x) ? a->x : b->x;
    const int32_t y0 = (a->y < b->y) ? a->y : b->y;
    const int32_t x1 = (a->x + a->w > b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    const int32_t y1 = (a->y + a->h > b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0;
}

static inline bool
pl__region_intersect(plImageOpRegion* out, const plImageOpRegion* a, const plImageOpRegion* b)
{
    int32_t x0 = (int32_t)((a->x > b->x) ? a->x : b->x);
    int32_t y0 = (int32_t)((a->y > b->y) ? a->y : b->y);
    int32_t x1 = (int32_t)((a->x + a->w < b->x + b->w) ? (a->x + a->w) : (b->x + b->w));
    int32_t y1 = (int32_t)((a->y + a->h < b->y + b->h) ? (a->y + a->h) : (b->y + b->h));
    if (x1 <= x0 || y1 <= y0)
        return false;
    out->x = (int32_t)x0; out->y = (int32_t)y0; out->w = (uint32_t)(x1 - x0); out->h = (uint32_t)(y1 - y0);
    return true;
}

static void
pl__update_active_from_regions(plImageOpData* p)
{
    if (p->_uRegionCount == 0)
        return;

    plImageOpRegion uni = p->_atRegions[0];
    for (uint32_t i = 1; i < p->_uRegionCount; i++)
        pl__region_union(&uni, &uni, &p->_atRegions[i]);

    p->uActiveXOffset = uni.x;
    p->uActiveYOffset = uni.y;
    p->uActiveWidth   = uni.w;
    p->uActiveHeight  = uni.h;
}

static void
pl__maybe_grow_image_op_data(plImageOpData* ptData)
{
    if(ptData->_uRegionCount + 1 >= ptData->_uRegionCapacity)
    {
        // first run
        if(ptData->_uRegionCount == 0)
        {
            ptData->_uRegionCapacity = 4;
            ptData->_atRegions = PL_ALLOC(sizeof(plImageOpRegion) * ptData->_uRegionCapacity);
            memset(ptData->_atRegions, 0, sizeof(plImageOpRegion) * ptData->_uRegionCapacity);
        }
        else // capacity reached
        {
            plImageOpRegion* atOldRegions = ptData->_atRegions;
            ptData->_atRegions = PL_ALLOC(sizeof(plImageOpRegion) * ptData->_uRegionCapacity * 2);
            memset(&ptData->_atRegions[ptData->_uRegionCapacity], 0, sizeof(plImageOpRegion) * ptData->_uRegionCapacity);
            memcpy(ptData->_atRegions, atOldRegions, sizeof(plImageOpRegion) * ptData->_uRegionCapacity);
            PL_FREE(atOldRegions);
            ptData->_uRegionCapacity *= 2;
        }
        
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_image_ops_initialize(plImageOpInit* ptInfo, plImageOpData* ptDataOut)
{
    ptDataOut->uVirtualWidth   = ptInfo->uVirtualWidth;
    ptDataOut->uVirtualHeight  = ptInfo->uVirtualHeight;
    ptDataOut->_uChannels = ptInfo->uChannels;
    ptDataOut->_uStride = ptInfo->uStride;
    ptDataOut->_uChannelStride = ptInfo->uStride / ptInfo->uChannels;
}

void
pl_image_ops_cleanup(plImageOpData* ptData)
{
    if(ptData->_atRegions)
    {
        for (uint32_t i = 0; i < ptData->_uRegionCount; i++)
        {
            PL_FREE(ptData->_atRegions[i].puData);
        }
        PL_FREE(ptData->_atRegions);
    }
    memset(ptData, 0, sizeof(plImageOpData));
}

void
pl_image_ops_cleanup_extract(uint8_t* puData)
{
    PL_FREE(puData);
}

uint8_t*
pl_image_ops_extract(plImageOpData* ptDataIn, int iXOffset, int iYOffset, uint32_t uWidth, uint32_t uHeight, uint64_t* puSizeOut)
{

    const size_t row_bytes = (size_t)uWidth * (size_t)ptDataIn->_uStride;
    const size_t total_bytes = row_bytes * (size_t)uHeight;

    plImageOpRegion tNewRegion = {
        .x = iXOffset,
        .y = iYOffset,
        .w = uWidth,
        .h = uHeight,
        .uDataSize = total_bytes,
        .puData = PL_ALLOC(total_bytes)
    };
    memset(tNewRegion.puData, 0, tNewRegion.uDataSize);

    const uint8_t uChannelStrideIn = ptDataIn->_uStride / ptDataIn->_uChannels;

    for (uint32_t i = 0; i < ptDataIn->_uRegionCount; i++)
    {
        plImageOpRegion tIntersectRegion = {0};
        const plImageOpRegion* ptCurrentRegion = &ptDataIn->_atRegions[i];
        if(pl__region_intersect(&tIntersectRegion, &tNewRegion, ptCurrentRegion))
        {
            // intersection in world coords
            const int ix0 = tIntersectRegion.x;
            const int iy0 = tIntersectRegion.y;
            const uint32_t iw = tIntersectRegion.w;
            const uint32_t ih = tIntersectRegion.h;

            // src start in source-local pixels
            const uint32_t src_x0 = (uint32_t)(ix0 - ptCurrentRegion->x);
            const uint32_t src_y0 = (uint32_t)(iy0 - ptCurrentRegion->y);

            // dst start in newregion-local pixels
            const uint32_t dst_x0 = (uint32_t)(ix0 - tNewRegion.x);
            const uint32_t dst_y0 = (uint32_t)(iy0 - tNewRegion.y);

            const size_t src_row_bytes = (size_t)ptCurrentRegion->w * (size_t)ptDataIn->_uStride;
            const size_t dst_row_bytes = (size_t)tNewRegion.w        * (size_t)ptDataIn->_uStride;
            const size_t copy_bytes    = (size_t)iw                  * (size_t)ptDataIn->_uStride;

            for (uint32_t r = 0; r < ih; r++)
            {
                const uint8_t* src = ptCurrentRegion->puData
                    + (size_t)(src_y0 + r) * src_row_bytes
                    + (size_t)src_x0 * (size_t)ptDataIn->_uStride;

                uint8_t* dst = tNewRegion.puData
                    + (size_t)(dst_y0 + r) * dst_row_bytes
                    + (size_t)dst_x0 * (size_t)ptDataIn->_uStride;

                memcpy(dst, src, copy_bytes);
            }
        }
    }

    if(puSizeOut)
        *puSizeOut = tNewRegion.uDataSize;
    return tNewRegion.puData;
}

void
pl_image_ops_add(plImageOpData* ptData, int iXOffset, int iYOffset, uint32_t uWidth, uint32_t uHeight, uint8_t* puData)
{

    plImageOpRegion tNewRegion = {
        .x = iXOffset,
        .y = iYOffset,
        .w = uWidth,
        .h = uHeight,
        .uDataSize = uWidth * uHeight * ptData->_uStride,
        .puData = PL_ALLOC(uWidth * uHeight * ptData->_uStride)
    };
    memcpy(tNewRegion.puData, puData, tNewRegion.uDataSize);

    pl__maybe_grow_image_op_data(ptData);
    ptData->_atRegions[ptData->_uRegionCount++] = tNewRegion;
    pl__update_active_from_regions(ptData);
}

void
pl_image_ops_add_region(plImageOpData* ptData, int iXOffset, int iYOffset, uint32_t uWidth, uint32_t uHeight, plImageOpColor tColor)
{

    plImageOpRegion tNewRegion = {
        .x = iXOffset,
        .y = iYOffset,
        .w = uWidth,
        .h = uHeight,
        .uDataSize = uWidth * uHeight * ptData->_uStride,
        .puData = PL_ALLOC(uWidth * uHeight * ptData->_uStride)
    };

    if(tColor == PL_IMAGE_OP_COLOR_WHITE)
        memset(tNewRegion.puData, 255, tNewRegion.uDataSize);
    else
        memset(tNewRegion.puData, 0, tNewRegion.uDataSize);

    pl__maybe_grow_image_op_data(ptData);
    ptData->_atRegions[ptData->_uRegionCount++] = tNewRegion;
    pl__update_active_from_regions(ptData);
}

void
pl_image_ops_square(plImageOpData* ptData)
{
    uint32_t uTarget = pl_max(ptData->uVirtualWidth, ptData->uVirtualHeight);
    ptData->uVirtualWidth = uTarget;
    ptData->uVirtualHeight = uTarget;

    uTarget = pl_max(ptData->uActiveWidth, ptData->uActiveHeight);
    ptData->uActiveWidth = uTarget;
    ptData->uActiveHeight = uTarget;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_image_ops_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plImageOpsI tApi = {
        .initialize      = pl_image_ops_initialize,
        .cleanup         = pl_image_ops_cleanup,
        .add             = pl_image_ops_add,
        .extract         = pl_image_ops_extract,
        .square          = pl_image_ops_square,
        .add_region      = pl_image_ops_add_region,
        .cleanup_extract = pl_image_ops_cleanup_extract,
    };
    pl_set_api(ptApiRegistry, plImageOpsI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

void
pl_unload_image_ops_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plImageOpsI* ptApi = pl_get_api_latest(ptApiRegistry, plImageOpsI);
    ptApiRegistry->remove_api(ptApi);
}