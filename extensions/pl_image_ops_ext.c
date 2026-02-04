/*
   pl_image_ops_ext.c
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
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_image_op_data(plImageOpInfo* ptInfo, plImageOpData* ptDataOut)
{
    ptDataOut->uDataSize = ptInfo->uWidth * ptInfo->uHeight * ptInfo->uStride;
    ptDataOut->puData = PL_ALLOC(ptDataOut->uDataSize);
    memset(ptDataOut->puData, 0, ptDataOut->uDataSize);
    ptDataOut->uWidth = ptInfo->uWidth;
    ptDataOut->uHeight = ptInfo->uHeight;
    ptDataOut->uChannels = ptInfo->uChannels;
    ptDataOut->uStride = ptInfo->uStride;
    ptDataOut->uChannelStride = ptInfo->uStride / ptInfo->uChannels;
}

void
pl_cleanup_image_op_data(plImageOpData* ptData)
{
    PL_FREE(ptData->puData);
    memset(ptData, 0, sizeof(plImageOpData));
}

void
pl_image_op_upsample(plImageOpData* ptData, uint32_t uFactor)
{

    uint64_t uOldDataSize = ptData->uDataSize;
    uint8_t* puOldData = ptData->puData;
    uint32_t uOldWidth = ptData->uWidth;
    uint32_t uOldHeight = ptData->uHeight;

    ptData->uWidth *= uFactor;
    ptData->uHeight *= uFactor;
    ptData->uDataSize = ptData->uWidth * ptData->uHeight * ptData->uStride;
    ptData->puData = PL_ALLOC(ptData->uDataSize);
    memset(ptData->puData, 0, ptData->uDataSize);

    for(uint32_t uRow = 0; uRow < uOldHeight; uRow++)
    {
        for(uint32_t uCol = 0; uCol < uOldWidth; uCol++)
        {
            const uint32_t uSourceIndex = uRow * uOldWidth + uCol;
            uint8_t auChannels[4] = {0};
            for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
            {
                auChannels[uChannel] = puOldData[uSourceIndex * ptData->uStride + uChannel * ptData->uChannelStride];
            }

            for(uint32_t uTapRow = 0; uTapRow < uFactor; uTapRow++)
            {
                for(uint32_t uTapCol = 0; uTapCol < uFactor; uTapCol++)
                {
                    const uint32_t uDestinationIndex = (uTapRow + uRow * uFactor) * ptData->uWidth + uCol * uFactor + uTapCol;
                    for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
                    {
                        ptData->puData[uDestinationIndex * ptData->uStride + uChannel * ptData->uChannelStride] = auChannels[uChannel];
                    }
                }
            }

        }
    }

    PL_FREE(puOldData);
}

void
pl_image_op_downsample(plImageOpData* ptData, uint32_t uFactor)
{

    uFactor = (uint32_t)pow(2.0, (double)uFactor);

    uint64_t uOldDataSize = ptData->uDataSize;
    uint8_t* puOldData = ptData->puData;
    uint32_t uOldWidth = ptData->uWidth;
    uint32_t uOldHeight = ptData->uHeight;

    ptData->uWidth /= uFactor;
    ptData->uHeight /= uFactor;
    ptData->uDataSize = ptData->uWidth * ptData->uHeight * ptData->uStride;
    ptData->puData = PL_ALLOC(ptData->uDataSize);
    memset(ptData->puData, 0, ptData->uDataSize);

    float fUvxInc = 1.0f / (float)uOldWidth;
    float fUvyInc = 1.0f / (float)uOldHeight;
    for(uint32_t uRow = 0; uRow < ptData->uHeight; uRow++)
    {
        float fUvy = (0.5f / (float)(ptData->uHeight)) + (float)(uRow - 0) / (float)ptData->uHeight;
        for(uint32_t uCol = 0; uCol < ptData->uWidth; uCol++)
        {
            float fUvx = (0.5f / (float)(ptData->uWidth)) + (float)(uCol - 0) / (float)ptData->uWidth;

            
            uint8_t auChannels[4] = {0};
            uint32_t auChannelsLarge[4] = {0};

            uint32_t uSumCount = 0;

            for(int x = 0; x < 4; x++)
            {
                for(int y = 0; y < 4; y++)
                {
                    float fUvxTap = fUvx + x * fUvxInc - fUvxInc * (4 - 1);
                    float fUvyTap = fUvy + y * fUvyInc - fUvyInc * (4 - 1);
                    if(fUvxTap < 0.0f || fUvxTap >= 1.0f || fUvyTap < 0.0f || fUvyTap >= 1.0f)
                    {
                    }
                    else
                    {
                        uint32_t uSrcRow = (uint32_t)(fUvyTap * (float)uOldHeight);
                        uint32_t uSrcCol = (uint32_t)(fUvxTap * (float)uOldWidth);
                        const uint32_t uSourceIndex = uSrcRow * uOldWidth + uSrcCol;
                        for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
                        {
                            auChannelsLarge[uChannel] += (uint32_t)puOldData[uSourceIndex * ptData->uStride + uChannel * ptData->uChannelStride];
                        }
                        uSumCount++;
                    }
                }
            }

            if(uSumCount > 0)
            {
                for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
                {
                    auChannels[uChannel] = (uint8_t)(auChannelsLarge[uChannel] / uSumCount);
                }
                // auChannels[3] = UINT8_MAX;
            }
            const uint32_t uDestinationIndex = uRow * ptData->uWidth + uCol;
            for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
            {
                ptData->puData[uDestinationIndex * ptData->uStride + uChannel * ptData->uChannelStride] = auChannels[uChannel];
            }

        }
    }
    PL_FREE(puOldData);
}

void
pl_image_op_square(plImageOpData* ptData)
{

    uint64_t uOldDataSize = ptData->uDataSize;
    uint8_t* puOldData = ptData->puData;
    uint32_t uOldWidth = ptData->uWidth;
    uint32_t uOldHeight = ptData->uHeight;

    if(uOldWidth == uOldHeight)
        return;

    uint32_t uTargetResolution = pl_max(uOldWidth, uOldHeight);
    ptData->uWidth = uTargetResolution;
    ptData->uHeight = uTargetResolution;

    ptData->uDataSize = ptData->uWidth * ptData->uHeight * ptData->uStride;
    ptData->puData = PL_ALLOC(ptData->uDataSize);
    memset(ptData->puData, 0, ptData->uDataSize);

    const uint8_t uChannelStrideIn = ptData->uStride / ptData->uChannels;

    for(uint32_t uRow = 0; uRow < uOldHeight; uRow++)
    {
        for(uint32_t uCol = 0; uCol < uOldWidth; uCol++)
        {
            const uint32_t uSourceIndex = uRow * uOldWidth + uCol;
            uint8_t auChannels[4] = {0};
            for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
            {
                auChannels[uChannel] = puOldData[uSourceIndex * ptData->uStride + uChannel * uChannelStrideIn];
            }

            const uint32_t uDestinationIndex = uRow * ptData->uWidth + uCol;
            for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
            {
                ptData->puData[uDestinationIndex * ptData->uStride + uChannel * ptData->uChannelStride] = auChannels[uChannel];
            }
        }
    }

    PL_FREE(puOldData);
}

void
pl_image_op_extract(plImageOpData* ptDataIn, uint32_t uXOffset, uint32_t uYOffset, uint32_t uWidth, uint32_t uHeight, plImageOpData* ptDataOut)
{
    plImageOpInfo tInfo = {
        .uWidth    = uWidth,
        .uHeight   = uHeight,
        .uChannels = ptDataIn->uChannels,
        .uStride   = ptDataIn->uStride
    };
    pl_initialize_image_op_data(&tInfo, ptDataOut);

    for(uint32_t uRow = 0; uRow < uHeight; uRow++)
    {
        for(uint32_t uCol = 0; uCol < uWidth; uCol++)
        {
            const uint32_t uSourceIndex = (uRow + uYOffset) * ptDataIn->uWidth + uCol + uXOffset;
            uint8_t auChannels[4] = {0};
            for(uint32_t uChannel = 0; uChannel < ptDataIn->uChannels; uChannel++)
            {
                auChannels[uChannel] = ptDataIn->puData[uSourceIndex * ptDataIn->uStride + uChannel * ptDataIn->uChannelStride];
            }

            const uint32_t uDestinationIndex = uRow * uWidth + uCol;
            for(uint32_t uChannel = 0; uChannel < ptDataOut->uChannels; uChannel++)
            {
                ptDataOut->puData[uDestinationIndex * ptDataOut->uStride + uChannel * ptDataOut->uChannelStride] = auChannels[uChannel];
            }
        }
    }
}

void
pl_add_image_op(plImageOpData* ptData, plImageOpInfo tInfo, uint32_t uXOffset, uint32_t uYOffset)
{
    const uint8_t uChannelStrideIn = tInfo.uStride / tInfo.uChannels;

    for(uint32_t uRow = 0; uRow < tInfo.uHeight; uRow++)
    {
        for(uint32_t uCol = 0; uCol < tInfo.uWidth; uCol++)
        {
            const uint32_t uSourceIndex = uRow * tInfo.uWidth + uCol;
            uint8_t auChannels[4] = {0};
            for(uint32_t uChannel = 0; uChannel < tInfo.uChannels; uChannel++)
            {
                auChannels[uChannel] = tInfo.puData[uSourceIndex * tInfo.uStride + uChannel * uChannelStrideIn];
            }

            const uint32_t uDestinationIndex = (uYOffset + uRow) * ptData->uWidth + uCol + uXOffset;
            for(uint32_t uChannel = 0; uChannel < ptData->uChannels; uChannel++)
            {
                ptData->puData[uDestinationIndex * ptData->uStride + uChannel * ptData->uChannelStride] = auChannels[uChannel];
            }
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_image_ops_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plImageOpsI tApi = {
        .initialize   = pl_initialize_image_op_data,
        .cleanup      = pl_cleanup_image_op_data,
        .add          = pl_add_image_op,
        .upsample     = pl_image_op_upsample,
        .downsample   = pl_image_op_downsample,
        .extract      = pl_image_op_extract,
        .square       = pl_image_op_square
    };
    pl_set_api(ptApiRegistry, plImageOpsI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

PL_EXPORT void
pl_unload_image_ops_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plImageOpsI* ptApi = pl_get_api_latest(ptApiRegistry, plImageOpsI);
    ptApiRegistry->remove_api(ptApi);
}