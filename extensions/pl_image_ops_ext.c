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

// extensions
#include "pl_graphics_ext.h"

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

static float
pl__half_to_float(uint16_t h)
{
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp        = (h & 0x7C00u) >> 10;
    uint32_t mant       = h & 0x03FFu;

    uint32_t f;

    if(exp == 0)
    {
        if(mant == 0)
        {
            f = sign;
        }
        else
        {
            // Normalize subnormal.
            exp = 1;
            while((mant & 0x0400u) == 0)
            {
                mant <<= 1;
                exp--;
            }

            mant &= 0x03FFu;
            exp = exp + (127 - 15);

            f = sign | (exp << 23) | (mant << 13);
        }
    }
    else if(exp == 31)
    {
        // Inf / NaN
        f = sign | 0x7F800000u | (mant << 13);
    }
    else
    {
        exp = exp + (127 - 15);
        f = sign | (exp << 23) | (mant << 13);
    }

    float result;
    memcpy(&result, &f, sizeof(float));
    return result;
}

static uint16_t
pl__float_to_half(float value)
{
    uint32_t f;
    memcpy(&f, &value, sizeof(uint32_t));

    const uint32_t sign = (f >> 16) & 0x8000u;
    int32_t exp         = (int32_t)((f >> 23) & 0xFFu) - 127 + 15;
    uint32_t mant       = f & 0x007FFFFFu;

    if(exp <= 0)
    {
        if(exp < -10)
            return (uint16_t)sign;

        mant = mant | 0x00800000u;

        const uint32_t shift = (uint32_t)(14 - exp);
        uint32_t halfMant = mant >> shift;

        // Round to nearest.
        if((mant >> (shift - 1)) & 1u)
            halfMant++;

        return (uint16_t)(sign | halfMant);
    }
    else if(exp >= 31)
    {
        // Inf/NaN
        if(mant == 0)
            return (uint16_t)(sign | 0x7C00u);

        return (uint16_t)(sign | 0x7C00u | (mant >> 13));
    }
    else
    {
        uint32_t half = sign | ((uint32_t)exp << 10) | (mant >> 13);

        // Round to nearest.
        if(mant & 0x00001000u)
            half++;

        return (uint16_t)half;
    }
}

static void
pl__mipmap_filter_2x2_rgba8(
    const uint8_t* p00,
    const uint8_t* p10,
    const uint8_t* p01,
    const uint8_t* p11,
    uint8_t* pOut
)
{
    pOut[0] = (uint8_t)(((uint32_t)p00[0] + p10[0] + p01[0] + p11[0] + 2) / 4);
    pOut[1] = (uint8_t)(((uint32_t)p00[1] + p10[1] + p01[1] + p11[1] + 2) / 4);
    pOut[2] = (uint8_t)(((uint32_t)p00[2] + p10[2] + p01[2] + p11[2] + 2) / 4);
    pOut[3] = (uint8_t)(((uint32_t)p00[3] + p10[3] + p01[3] + p11[3] + 2) / 4);
}

static void
pl__mipmap_filter_2x2_rgba16f(
    const uint8_t* p00Bytes,
    const uint8_t* p10Bytes,
    const uint8_t* p01Bytes,
    const uint8_t* p11Bytes,
    uint8_t* pOutBytes
)
{
    const uint16_t* p00 = (const uint16_t*)p00Bytes;
    const uint16_t* p10 = (const uint16_t*)p10Bytes;
    const uint16_t* p01 = (const uint16_t*)p01Bytes;
    const uint16_t* p11 = (const uint16_t*)p11Bytes;
    uint16_t* pOut      = (uint16_t*)pOutBytes;

    for(uint32_t c = 0; c < 4; c++)
    {
        const float f00 = pl__half_to_float(p00[c]);
        const float f10 = pl__half_to_float(p10[c]);
        const float f01 = pl__half_to_float(p01[c]);
        const float f11 = pl__half_to_float(p11[c]);

        const float fAvg = (f00 + f10 + f01 + f11) * 0.25f;

        pOut[c] = pl__float_to_half(fAvg);
    }
}

static uint32_t
pl__mipmap_get_format_bpp(plFormat eFormat)
{
    switch(eFormat)
    {
        case PL_FORMAT_R8G8B8A8_UNORM:
            return 4;

        case PL_FORMAT_R16G16B16A16_FLOAT:
            return 8;

        default:
            return 0;
    }
}

static bool
pl__mipmap_generate_next_level_2d(plFormat eFormat, const plMipLevel* ptSrcLevel, plMipLevel* ptDstLevel, uint32_t uLayerCount)
{
    const uint32_t uBpp = pl__mipmap_get_format_bpp(eFormat);
    if(uBpp == 0)
        return false;

    uint8_t* puSrcBase = (uint8_t*)ptSrcLevel->pData;
    uint8_t* puDstBase = (uint8_t*)ptDstLevel->pData;

    for(uint32_t uLayer = 0; uLayer < uLayerCount; uLayer++)
    {
        uint8_t* puSrcLayer = puSrcBase + uLayer * ptSrcLevel->szFaceStride;
        uint8_t* puDstLayer = puDstBase + uLayer * ptDstLevel->szFaceStride;

        for(uint32_t y = 0; y < ptDstLevel->uHeight; y++)
        {
            for(uint32_t x = 0; x < ptDstLevel->uWidth; x++)
            {
                const uint32_t srcX0 = x * 2;
                const uint32_t srcY0 = y * 2;

                const uint32_t srcX1 = srcX0 + 1 < ptSrcLevel->uWidth  ? srcX0 + 1 : srcX0;
                const uint32_t srcY1 = srcY0 + 1 < ptSrcLevel->uHeight ? srcY0 + 1 : srcY0;

                const uint8_t* p00 = puSrcLayer + srcY0 * ptSrcLevel->szRowStride + srcX0 * uBpp;
                const uint8_t* p10 = puSrcLayer + srcY0 * ptSrcLevel->szRowStride + srcX1 * uBpp;
                const uint8_t* p01 = puSrcLayer + srcY1 * ptSrcLevel->szRowStride + srcX0 * uBpp;
                const uint8_t* p11 = puSrcLayer + srcY1 * ptSrcLevel->szRowStride + srcX1 * uBpp;

                uint8_t* pOut = puDstLayer + y * ptDstLevel->szRowStride + x * uBpp;

                switch(eFormat)
                {
                    case PL_FORMAT_R8G8B8A8_UNORM:
                        pl__mipmap_filter_2x2_rgba8(p00, p10, p01, p11, pOut);
                        break;

                    case PL_FORMAT_R16G16B16A16_FLOAT:
                        pl__mipmap_filter_2x2_rgba16f(p00, p10, p01, p11, pOut);
                        break;

                    default:
                        return false;
                }
            }
        }
    }

    return true;
}

uint32_t
pl_mipmap_calculate_mip_count(uint32_t uWidth, uint32_t uHeight)
{
    uint32_t uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)uWidth, (int)uHeight))) + 1;

    for(uint32_t uMipLevel = 1; uMipLevel < uMips; uMipLevel++)
    {
        int iCurrentWidth = (int)uWidth / ((1 << (int)uMipLevel));
        int iCurrentHeight = (int)uHeight / ((1 << (int)uMipLevel));

        if(iCurrentHeight < 4 || iCurrentWidth < 4)
        {
            uMips = uMipLevel;
            break;
        }
    }
    return uMips;
}

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

bool
pl_image_ops_generate_mip_chain(const plMipMapCpuDesc* ptDesc, plMipMapChain* ptChain)
{
    if(ptDesc == NULL || ptChain == NULL || ptDesc->pData == NULL)
        return false;

    if(ptDesc->uWidth == 0 || ptDesc->uHeight == 0)
        return false;

    const uint32_t uBpp = pl__mipmap_get_format_bpp(ptDesc->eFormat);
    if(uBpp == 0)
        return false;

    uint32_t uLayerCount = ptDesc->uLayers;
    if(uLayerCount == 0)
        uLayerCount = 1;

    if(ptDesc->tTextureType == PL_TEXTURE_TYPE_CUBE && uLayerCount != 6)
        return false;

    memset(ptChain, 0, sizeof(plMipMapChain));

    ptChain->uMipCount = ptDesc->uMipCount == 0 ?
        pl_mipmap_calculate_mip_count(ptDesc->uWidth, ptDesc->uHeight) :
        ptDesc->uMipCount;

    if(ptChain->uMipCount == 0)
        return false;

    ptChain->atLevels = PL_ALLOC(sizeof(plMipLevel) * ptChain->uMipCount);
    if(ptChain->atLevels == NULL)
        return false;

    memset(ptChain->atLevels, 0, sizeof(plMipLevel) * ptChain->uMipCount);

    ptChain->atLevels[0].pData        = (void*)ptDesc->pData;
    ptChain->atLevels[0].uWidth       = ptDesc->uWidth;
    ptChain->atLevels[0].uHeight      = ptDesc->uHeight;
    ptChain->atLevels[0].szRowStride  = ptDesc->uWidth * uBpp;
    ptChain->atLevels[0].szFaceStride = ptChain->atLevels[0].szRowStride * ptDesc->uHeight;
    ptChain->atLevels[0].szSize       = ptChain->atLevels[0].szFaceStride * uLayerCount;

    for(uint32_t uCurrentMip = 1; uCurrentMip < ptChain->uMipCount; uCurrentMip++)
    {
        plMipLevel* ptSrcLevel = &ptChain->atLevels[uCurrentMip - 1];
        plMipLevel* ptDstLevel = &ptChain->atLevels[uCurrentMip];

        ptDstLevel->uWidth  = ptSrcLevel->uWidth  > 1 ? ptSrcLevel->uWidth  / 2 : 1;
        ptDstLevel->uHeight = ptSrcLevel->uHeight > 1 ? ptSrcLevel->uHeight / 2 : 1;

        ptDstLevel->szRowStride  = ptDstLevel->uWidth * uBpp;
        ptDstLevel->szFaceStride = ptDstLevel->szRowStride * ptDstLevel->uHeight;
        ptDstLevel->szSize       = ptDstLevel->szFaceStride * uLayerCount;

        ptDstLevel->pData = PL_ALLOC(ptDstLevel->szSize);
        if(ptDstLevel->pData == NULL)
        {
            for(uint32_t i = 1; i < uCurrentMip; i++)
            {
                PL_FREE(ptChain->atLevels[i].pData);
                ptChain->atLevels[i].pData = NULL;
            }

            PL_FREE(ptChain->atLevels);
            memset(ptChain, 0, sizeof(plMipMapChain));
            return false;
        }

        memset(ptDstLevel->pData, 0, ptDstLevel->szSize);

        if(!pl__mipmap_generate_next_level_2d(
            ptDesc->eFormat,
            ptSrcLevel,
            ptDstLevel,
            uLayerCount
        ))
        {
            for(uint32_t i = 1; i <= uCurrentMip; i++)
            {
                PL_FREE(ptChain->atLevels[i].pData);
                ptChain->atLevels[i].pData = NULL;
            }

            PL_FREE(ptChain->atLevels);
            memset(ptChain, 0, sizeof(plMipMapChain));
            return false;
        }
    }

    return true;
}

void
pl_image_ops_free_mip_chain(plMipMapChain* ptChain)
{
    if(ptChain == NULL || ptChain->atLevels == NULL)
        return;

    for(uint32_t i = 1; i < ptChain->uMipCount; i++)
    {
        PL_FREE(ptChain->atLevels[i].pData);
    }

    PL_FREE(ptChain->atLevels);
    memset(ptChain, 0, sizeof(plMipMapChain));
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_image_ops_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plImageOpsI tApi = {
        .initialize         = pl_image_ops_initialize,
        .cleanup            = pl_image_ops_cleanup,
        .add                = pl_image_ops_add,
        .extract            = pl_image_ops_extract,
        .square             = pl_image_ops_square,
        .add_region         = pl_image_ops_add_region,
        .cleanup_extract    = pl_image_ops_cleanup_extract,
        .generate_mip_chain = pl_image_ops_generate_mip_chain,
        .free_mip_chain     = pl_image_ops_free_mip_chain
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