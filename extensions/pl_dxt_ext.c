/*
   pl_dxt_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
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

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// libraries
#include "stb_dxt.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline void
pl__dxt_sample(const uint8_t* puData, uint32_t uChannels, uint32_t uWidth, uint32_t uX, uint32_t uY, uint8_t* auOut)
{
    const uint8_t* ptSource = puData + (uY * uWidth + uX) * uChannels;
    for(uint32_t uChannel = 0; uChannel < uChannels; uChannel++)
        auOut[uChannel] = ptSource[uChannel];
}

static inline void
pl__dxt_sample_wrap(const uint8_t* puData, uint32_t uChannels, uint32_t uWidth, uint32_t uHeight, uint32_t uX, uint32_t uY, uint8_t* auOut)
{
    uX = pl_clampu(0, uX, uWidth - 1);
    uY = pl_clampu(0, uY, uHeight - 1);
    const uint8_t* ptSource = puData + (uY * uWidth + uX) * uChannels;
    for(uint32_t uChannel = 0; uChannel < uChannels; uChannel++)
        auOut[uChannel] = ptSource[uChannel];
}

static inline void
pl__dxt_copy(const uint8_t* puData, uint32_t uDxtBlockWidth, uint32_t uX, uint32_t uY, uint8_t* auOut)
{
    memcpy(auOut + (uY * 4 + uX) * uDxtBlockWidth, puData, uDxtBlockWidth);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_dxt_compress(const plDxtInfo* ptInfo, uint8_t* puDataOut, size_t* szSizeOut)
{

    PL_ASSERT(ptInfo);
    PL_ASSERT(ptInfo->uChannels < 5);

    const uint32_t uAdjustedImageWidth = (uint32_t)ceilf((float)ptInfo->uWidth / 4.0f) * 4;
    const uint32_t uAdjustedImageHeight = (uint32_t)ceilf((float)ptInfo->uHeight / 4.0f) * 4;

    const uint32_t uBlockSize = (ptInfo->uChannels == 2 || ptInfo->uChannels == 4 ? 16 : 8);

    if(szSizeOut)
        *szSizeOut = uAdjustedImageWidth * uAdjustedImageHeight * ptInfo->uChannels * uBlockSize / (16 * ptInfo->uChannels);

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
    const uint32_t uDxtBlockWidth = ptInfo->uChannels > 2 ? 4 : ptInfo->uChannels;

    uint32_t uBlocksPerRow = uAdjustedImageWidth / 4;
    uint32_t uBlocksPerColumn = uAdjustedImageHeight / 4;

    uint8_t auPadded[4] = {0};

    int iDxtFlags = STB_DXT_NORMAL;
    if(ptInfo->tFlags & PL_DXT_FLAGS_HIGH_QUALITY)
        iDxtFlags = STB_DXT_HIGHQUAL;

    int iIncludeAlpha = 0;
    if(ptInfo->uChannels == 4)
        iIncludeAlpha = 1;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~inner fill~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptInfo->uChannels > 2)
    {

        for (uint32_t uRowStart = 0; uRowStart < uAdjustedImageHeight; uRowStart += 4)
        {
            const uint32_t uBlockRowOffset = (uBlocksPerRow * uRowStart / 4);
            const uint32_t uBlockRowBytesOffset = (ptInfo->uWidth * uBytesPerPixel * uRowStart);
            for (uint32_t uColumnStart = 0; uColumnStart < uAdjustedImageWidth; uColumnStart += 4)
            {

                uint32_t uBlockIndex =  uBlockRowOffset + uColumnStart / 4;
                uOffset = uBlockSize * uBlockIndex;

                const uint32_t uOffsetBytes = uBlockRowBytesOffset + uColumnStart * uBytesPerPixel;
                puData = ptInfo->puData + uOffsetBytes;

                for (uint32_t uRow = 0; uRow < 4; uRow++)
                {
                    for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
                    {
                        pl__dxt_sample(puData, ptInfo->uChannels, ptInfo->uWidth, uColumn, uRow, auPadded);
                        pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn, uRow, auInDataBuf);
                    }
                }

                stb_compress_dxt_block(auOutDataBuf, auInDataBuf, iIncludeAlpha, iDxtFlags);
                memcpy(&puDataOut[uOffset], auOutDataBuf, uBlockSize);
            }
        }
    }
    else if(ptInfo->uChannels == 2)
    {
        for (uint32_t uRowStart = 0; uRowStart < uAdjustedImageHeight; uRowStart += 4)
        {
            const uint32_t uBlockRowOffset = (uBlocksPerRow * uRowStart / 4);
            const uint32_t uBlockRowBytesOffset = (ptInfo->uWidth * uBytesPerPixel * uRowStart);
            for (uint32_t uColumnStart = 0; uColumnStart < uAdjustedImageWidth; uColumnStart += 4)
            {

                uint32_t uBlockIndex = uBlockRowOffset + uColumnStart / 4;
                uOffset = uBlockSize * uBlockIndex;

                const uint32_t uOffsetBytes = uBlockRowBytesOffset + uColumnStart * uBytesPerPixel;
                puData = ptInfo->puData + uOffsetBytes;

                for (uint32_t uRow = 0; uRow < 4; uRow++)
                {
                    for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
                    {
                        pl__dxt_sample(puData, ptInfo->uChannels, ptInfo->uWidth, uColumn, uRow, auPadded);
                        pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn, uRow, auInDataBuf);
                    }
                }

                stb_compress_bc5_block(auOutDataBuf, auInDataBuf);
                memcpy(&puDataOut[uOffset], auOutDataBuf, uBlockSize);
            }
        }
    }
    else if(ptInfo->uChannels == 1)
    {
        for (uint32_t uRowStart = 0; uRowStart < uAdjustedImageHeight; uRowStart += 4)
        {
            const uint32_t uBlockRowOffset = (uBlocksPerRow * uRowStart / 4);
            const uint32_t uBlockRowBytesOffset = (ptInfo->uWidth * uBytesPerPixel * uRowStart);
            for (uint32_t uColumnStart = 0; uColumnStart < uAdjustedImageWidth; uColumnStart += 4)
            {
                uint32_t uBlockIndex = uBlockRowOffset + uColumnStart / 4;
                uOffset = uBlockSize * uBlockIndex;

                const uint32_t uOffsetBytes = uBlockRowBytesOffset + uColumnStart * uBytesPerPixel;
                puData = ptInfo->puData + uOffsetBytes;

                for (uint32_t uRow = 0; uRow < 4; uRow++)
                {
                    for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
                    {
                        pl__dxt_sample(puData, ptInfo->uChannels, ptInfo->uWidth, uColumn, uRow, auPadded);
                        pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn, uRow, auInDataBuf);
                    }
                }

                stb_compress_bc4_block(auOutDataBuf, auInDataBuf);
                memcpy(&puDataOut[uOffset], auOutDataBuf, uBlockSize);
            }
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~right fill~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(uOverflowH > 0)
    {
        for (uint32_t uRowStart = 0; uRowStart < uWrapPosV; uRowStart += 4)
        {
            uint32_t uBlockIndex = uRowStart * uAdjustedImageWidth / 16;
            uBlockIndex += uWrapPosH / 4;
            uOffset = uBlockSize * uBlockIndex;
            const uint32_t uOffsetBytes = (ptInfo->uWidth * uBytesPerPixel * uRowStart) + uWrapPosH * uBytesPerPixel;
            puData = ptInfo->puData + uOffsetBytes;

            memset(auInDataBuf, 255, 64);

            for (uint32_t uRow = 0; uRow < 4; uRow++)
            {
                for (uint32_t uColumn = 0; uColumn < uOverflowH; uColumn++)
                {
                    pl__dxt_sample(puData, ptInfo->uChannels, ptInfo->uWidth, uColumn, uRow, auPadded);
                    pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn, uRow, auInDataBuf);

                    if(uColumn == uOverflowH - 1)
                    {
                        for(uint32_t iOverflow = 0; iOverflow < 4 - uOverflowH; iOverflow++)
                            pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn + iOverflow + 1, uRow, auInDataBuf);
                    }
                }
            }

            memset(auOutDataBuf, 0, 16);

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
            memcpy(&puDataOut[uOffset], auOutDataBuf, uBlockSize);
        }

    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bottom fill~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(uOverflowV > 0)
    {
        for (uint32_t uColumnStart = 0; uColumnStart < uWrapPosH; uColumnStart += 4)
        {
            uint32_t uBlockIndex = uColumnStart / 4 + uBlocksPerRow * uWrapPosV / 4;
            uOffset = uBlockSize * uBlockIndex;
            const uint32_t uOffsetBytes = (ptInfo->uWidth * uBytesPerPixel * uWrapPosV) + uColumnStart * uBytesPerPixel;
            puData = ptInfo->puData + uOffsetBytes;

            memset(auInDataBuf, 255, 64);

            for (uint32_t uRow = 0; uRow < uOverflowV; uRow++)
            {
                for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
                {
                    pl__dxt_sample(puData, ptInfo->uChannels, ptInfo->uWidth, uColumn, uRow, auPadded);
                    pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn, uRow, auInDataBuf);
                    if(uRow == uOverflowV - 1)
                    {
                        for(uint32_t iOverflow = 0; iOverflow < 4 - uOverflowV; iOverflow++)
                            pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn, uRow + iOverflow + 1, auInDataBuf);
                    }
                }
            }

            memset(auOutDataBuf, 0, 16);

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
            memcpy(&puDataOut[uOffset], auOutDataBuf, uBlockSize);
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~corner fill~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    if(uOverflowV > 0 && uOverflowH > 0)
    {
        uint32_t uBlockIndex = uBlocksPerRow * uBlocksPerColumn - 1;
        uOffset = uBlockSize * uBlockIndex;

        memset(auInDataBuf, 255, 64);
        memset(auOutDataBuf, 0, 16);

        for (uint32_t uRow = 0; uRow < 4; uRow++)
        {
            for (uint32_t uColumn = 0; uColumn < 4; uColumn++)
            {
                pl__dxt_sample_wrap(ptInfo->puData, ptInfo->uChannels, ptInfo->uWidth, ptInfo->uHeight, uWrapPosH + uColumn, uWrapPosV + uRow, auPadded);
                pl__dxt_copy(auPadded, uDxtBlockWidth, uColumn, uRow, auInDataBuf);
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
        memcpy(&puDataOut[uOffset], auOutDataBuf, uBlockSize);
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