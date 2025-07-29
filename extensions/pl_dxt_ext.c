/*
   pl_dxt_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h>
#include <math.h>
#include "pl.h"
#include "pl_dxt_ext.h"

// extensions
#include "pl_graphics_ext.h"

// libraries
#include "stb_dxt.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_dxt_compress(const plDxtInfo* ptInfo, uint8_t* puDataOut, size_t* szSizeOut)
{
    const uint32_t uAdjustedImageWidth = (uint32_t)ceilf((float)ptInfo->uWidth / 4.0f) * 4;
    const uint32_t uAdjustedImageHeight = (uint32_t)ceilf((float)ptInfo->uHeight / 4.0f) * 4;

    const uint32_t uBlockSize = (ptInfo->uChannels == 2 || ptInfo->uChannels == 4 ? 16 : 8);

    if(szSizeOut)
    {
        if(ptInfo->uChannels == 4)
            *szSizeOut = uAdjustedImageWidth * uAdjustedImageHeight;
        else if(ptInfo->uChannels == 3)
            *szSizeOut = uAdjustedImageWidth * uAdjustedImageHeight / 2;
        else
        {
            PL_ASSERT(false && "Only supporting 3 & 4 channels for now");
        }
    }

    if(puDataOut == NULL)
        return;

    uint32_t uOffset = 0;
    const uint8_t* puData = ptInfo->puData;

    uint8_t auInDataBuf[64] = {0};
    uint8_t auOutDataBuf[16] = {0};
    
    const uint32_t uBytesPerPixel = ptInfo->uChannels;
    const uint32_t uOverflowH = (4 - (uAdjustedImageWidth - ptInfo->uWidth)) % 4;
    const uint32_t uOverflowV = (4 - (uAdjustedImageHeight - ptInfo->uHeight)) % 4;
    const uint32_t uWrapPosH = ptInfo->uWidth - uOverflowH;
    const uint32_t uWrapPosV = ptInfo->uHeight - uOverflowV;

    uint8_t auPadded[4] = {0};

    int iDxtFlags = STB_DXT_NORMAL;
    if(ptInfo->tFlags & PL_DXT_FLAGS_HIGH_QUALITY)
        iDxtFlags = STB_DXT_HIGHQUAL;

    int iIncludeAlpha = 0;
    if(ptInfo->uChannels == 4)
        iIncludeAlpha = 1;

    if(uOverflowH == 0 && uOverflowV == 0 && ptInfo->uChannels > 2)
    {

        for (uint32_t uRowStart = 0; uRowStart < uAdjustedImageHeight; uRowStart += 4)
        {
            for (uint32_t uColumnStart = 0; uColumnStart < uAdjustedImageWidth; uColumnStart += 4)
            {
                for (uint32_t uRow = 0; uRow < 4; uRow++)
                {
                    for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
                    {
                        const uint8_t* ptSource = puData + (uRow * ptInfo->uWidth + uColumn) * ptInfo->uChannels;
                        for(uint32_t uChannel = 0; uChannel < ptInfo->uChannels; uChannel++)
                            auPadded[uChannel] = ptSource[uChannel];
                        memcpy(auInDataBuf + (uRow * 4 + uColumn) * 4, auPadded, 4);
                    }
                }

                stb_compress_dxt_block(auOutDataBuf, auInDataBuf, iIncludeAlpha, iDxtFlags);
                memcpy(&puDataOut[uOffset], auOutDataBuf, uBlockSize);

                uOffset += uBlockSize;
                puData += uBytesPerPixel * 4;
            }
            puData += ptInfo->uWidth * uBytesPerPixel * 3; // by 3 since we already moved first row across
        }
    }
    else // slow path
    {

        for (uint32_t uRowStart = 0; uRowStart < uAdjustedImageHeight; uRowStart += 4)
        {
            for (uint32_t uColumnStart = 0; uColumnStart < uAdjustedImageWidth; uColumnStart += 4)
            {
                if (uRowStart >= uWrapPosV && uColumnStart >= uWrapPosH) // overflow on bottom right corner
                {
                    memset(auInDataBuf, 255, 64);

                    for (uint32_t uRow = 0; uRow < uOverflowV; uRow++)
                    {
                        for (uint32_t uColumn = 0; uColumn < uOverflowH; uColumn++)
                        {
                            const uint8_t* ptSource = puData + (uRow * ptInfo->uWidth + uColumn) * 4;
                            memcpy(auInDataBuf + (uRow * 4 + uColumn) * ptInfo->uChannels, ptSource, ptInfo->uChannels);
                        }
                    }
                }
                else if(uColumnStart >= uWrapPosH) // overflow on right
                {
                    memset(auInDataBuf, 0, 64);

                    for (uint32_t uRow = 0; uRow < 4; uRow++)
                    {
                        for (uint32_t uColumn = 0; uColumn < uOverflowH; uColumn++)
                        {
                            const uint8_t* ptSource = puData + (uRow * ptInfo->uWidth + uColumn) * 4;
                            memcpy(auInDataBuf + (uRow * 4 + uColumn) * ptInfo->uChannels, ptSource, ptInfo->uChannels);

                            if(uColumn == uOverflowH - 1)
                            {
                                for(uint32_t iOverflow = 0; iOverflow < uOverflowV; iOverflow++)
                                {
                                    memcpy(auInDataBuf + (uRow * 4 + uColumn + iOverflow + 1) * ptInfo->uChannels, ptSource, ptInfo->uChannels);
                                }
                            }
                        }
                    }
                }
                else if (uRowStart >= uWrapPosV) // overflow on bottom
                {
                    memset(auInDataBuf, 255, 64);

                    for (uint32_t uRow = 0; uRow < uOverflowV; uRow++)
                    {
                        for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
                        {
                            const uint8_t* ptSource = puData + (uRow * ptInfo->uWidth + uColumn) * 4;
                            memcpy(auInDataBuf + (uRow * 4 + uColumn) * ptInfo->uChannels, ptSource, ptInfo->uChannels);
                            if(uRow == uOverflowV - 1)
                            {
                                for(uint32_t iOverflow = 0; iOverflow < uOverflowV; iOverflow++)
                                {
                                    memcpy(auInDataBuf + ((uRow + iOverflow + 1) * 4 + uColumn) * ptInfo->uChannels, ptSource, ptInfo->uChannels);
                                }
                            }
                        }
                    }
                }

                else
                {
                    for (uint32_t uRow = 0; uRow < 4; uRow++)
                    {
                        for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
                        {
                            const uint8_t* ptSource = puData + (uRow * ptInfo->uWidth + uColumn) * 4;
                            memcpy(auInDataBuf + (uRow * 4 + uColumn) * ptInfo->uChannels, ptSource, ptInfo->uChannels);
                        }
                    }
                }

                switch (ptInfo->uChannels)
                {
                    case 3:
                    case 4:
                        stb_compress_dxt_block(auOutDataBuf, auInDataBuf, iIncludeAlpha, iDxtFlags);
                        break;
                    case 1:
                        stb_compress_bc4_block(auOutDataBuf, auInDataBuf);
                        break;
                    case 2:
                        stb_compress_bc5_block(auOutDataBuf, auInDataBuf);
                        break;
                }
                memcpy(&puDataOut[uOffset], auOutDataBuf, 16);

                uOffset += 16;
                if(uColumnStart >= uWrapPosH && uOverflowH > 0)
                    puData += uBytesPerPixel * uOverflowH;
                else
                    puData += uBytesPerPixel * 4;
            }
            puData += ptInfo->uWidth * uBytesPerPixel * 3; // by 3 since we already moved first row across
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_dxt_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDxtI tApi = {
        .compress = pl_dxt_compress
    };
    pl_set_api(ptApiRegistry, plDxtI, &tApi);
}

PL_EXPORT void
pl_unload_dxt_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plDxtI* ptApi = pl_get_api_latest(ptApiRegistry, plDxtI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define STB_DXT_IMPLEMENTATION
    #include "stb_dxt.h"
    #undef STB_DXT_IMPLEMENTATION

#endif