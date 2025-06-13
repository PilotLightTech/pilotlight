/*
   pl_compress_ext.c
*/

/*
Index of this file:
// [SECTION] notes
// [SECTION] includes
// [SECTION] global data
// [SECTION] helper macros
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] notes
//-----------------------------------------------------------------------------

/*
    The implementation here is based on "stb_compress". Light modifications
    were made to match the code style of this code base. Other changes were
    made to remove unnecessary features and to remove static analysis
    warnings.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "pl.h"
#include "pl_compress_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static uint8_t* gpuOut              = NULL;
static uint32_t guOutBytes          = 0;
static uint32_t guOutBytesAvailable = 0;
static int      gpiWindow           = 0x40000; // 256K
static uint32_t guHashSize          = 32768;
static uint32_t guRunningAdler      = 0;
static uint8_t* gpuBarrier          = NULL;
static uint8_t* gpuBarrier2         = NULL;
static uint8_t* gpuBarrier3         = NULL;
static uint8_t* gpuBarrier4         = NULL;
static uint8_t* gpuDOut             = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper macros
//-----------------------------------------------------------------------------

#define pl__out(v) do { ++guOutBytes; if(guOutBytes > guOutBytesAvailable) gpuOut = NULL; if (gpuOut) *gpuOut++ = (uint8_t) (v); } while (0)

static void pl__out2(uint32_t v) { pl__out(v >> 8); pl__out(v); }
static void pl__out3(uint32_t v) { pl__out(v >> 16); pl__out(v >> 8); pl__out(v); }
static void pl__out4(uint32_t v) { pl__out(v >> 24); pl__out(v >> 16); pl__out(v >> 8 ); pl__out(v); }

// note that you can play with the hashing functions all you
// want without needing to change the decompressor
#define pl__hc(q,h,c)      (((h) << 7) + ((h) >> 25) + q[c])
#define pl__hc2(q,h,c,d)   (((h) << 14) + ((h) >> 18) + (q[c] << 7) + q[d])
#define pl__hc3(q,c,d,e)   ((q[c] << 14) + (q[d] << 7) + q[e])

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static uint32_t
pl__adler32(uint32_t uAdler32, uint8_t* puBuffer, uint32_t uLength)
{
    uint32_t uS1 = uAdler32 & 0xffff;
    uint32_t uS2 = uAdler32 >> 16;
    uint32_t uBlockLength = 0;
    uint32_t i = 0;

    uBlockLength = uLength % 5552;
    while(uLength)
    {
        for (i = 0; i + 7 < uBlockLength; i += 8)
        {
            uS1 += puBuffer[0], uS2 += uS1;
            uS1 += puBuffer[1], uS2 += uS1;
            uS1 += puBuffer[2], uS2 += uS1;
            uS1 += puBuffer[3], uS2 += uS1;
            uS1 += puBuffer[4], uS2 += uS1;
            uS1 += puBuffer[5], uS2 += uS1;
            uS1 += puBuffer[6], uS2 += uS1;
            uS1 += puBuffer[7], uS2 += uS1;

            puBuffer += 8;
        }

        for (; i < uBlockLength; ++i)
        {
            uS1 += *puBuffer++;
            uS2 += uS1;
        }

        uS1 %= 65521; // ADLER_MOD
        uS2 %= 65521; // ADLER_MOD
        uLength -= uBlockLength;
        uBlockLength = 5552;
    }
    return (uS2 << 16) + uS1;
}

static uint32_t
pl__matchlen(uint8_t* pM1, uint8_t* pM2, uint32_t uMaxLength)
{
    uint32_t i = 0;
    for (i=0; i < uMaxLength; ++i)
    {
        if (pM1[i] != pM2[i])
            return i;
    }
    return i;
}

static void
pl__out_literals(uint8_t* puIn, ptrdiff_t tNumlit)
{
    while(tNumlit > 65536)
    {
        pl__out_literals(puIn, 65536);
        puIn += 65536;
        tNumlit -= 65536;
    }

    if (tNumlit == 0);
    else if (tNumlit <= 32)
        pl__out(0x000020 + (uint32_t)tNumlit - 1);
    else if (tNumlit <= 2048)
        pl__out2(0x000800 + (uint32_t)tNumlit - 1);
    else // tNumlit <= 65536)
        pl__out3(0x070000 + (uint32_t)tNumlit - 1);

    guOutBytes += (uint32_t)tNumlit;
    if (gpuOut)
    {
        if(guOutBytes > guOutBytesAvailable)
            gpuOut = NULL;
        else
        {
            memcpy(gpuOut, puIn, tNumlit);
            gpuOut += tNumlit;
        }
    }
    
}

static inline int
pl__not_crap(int iBest, int iDist)
{
    return ((iBest > 2  &&  iDist <= 0x00100)
        || (iBest > 5  &&  iDist <= 0x04000)
        || (iBest > 7  &&  iDist <= 0x80000));
}

static int
pl__compress_chunk(uint8_t* puHistory, uint8_t* puStart, uint8_t* puEnd,
    int iLength, int* piPendingLiterals, uint8_t** ppuCHash, uint32_t uMask)
{
    int iWindow = gpiWindow;
    uint32_t uMatchMax  = 0;
    uint8_t* puLitStart = puStart - *piPendingLiterals;
    uint8_t* puQ        = puStart;

    #define PL__SCRAMBLE(h) (((h) + ((h) >> 16)) & uMask)

    // stop short of the end so we don't scan off the end doing
    // the hashing; this means we won't compress the last few bytes
    // unless they were part of something longer
    while (puQ < puStart + iLength && puQ + 12 < puEnd)
    {
        int iM = 0;
        uint32_t uH1 = 0;
        uint32_t uH2 = 0;
        uint32_t uH3 = 0;
        uint32_t uH4 = 0;
        uint32_t uH = 0;
        uint8_t* puT = NULL;
        int iBest = 2;
        int iDist = 0;

        if (puQ + 65536 > puEnd)
            uMatchMax = (uint32_t) (puEnd - puQ);
        else
            uMatchMax = 65536u;

        #define pl__nc(b, d) ((d) <= iWindow && ((b) > 9 || pl__not_crap(b,d)))

        #define PL__TRY(t, p)  /* avoid retrying a match we already tried */ \
                        if (p ? iDist != (int) (puQ-t) : 1)                     \
                        if ((iM = (int) pl__matchlen(t, puQ, uMatchMax)) > iBest)\
                        if (pl__nc(iM, (int) (puQ-(t))))                        \
                            iBest = iM, iDist = (int) (puQ - (t))

        // rather than search for all matches, only try 4 candidate locations,
        // chosen based on 4 different hash functions of different lengths.
        // this strategy is inspired by LZO; hashing is unrolled here using the
        // 'hc' macro
        uH = pl__hc3(puQ,0, 1, 2);
        uH1 = PL__SCRAMBLE(uH);
        puT = ppuCHash[uH1];
        if(puT)
            PL__TRY(puT,0); //-V547
        uH = pl__hc2(puQ, uH, 3, 4);
        uH2 = PL__SCRAMBLE(uH);
        uH = pl__hc2(puQ, uH, 5, 6);
        puT = ppuCHash[uH2];
        if(puT)
            PL__TRY(puT,1);
        uH = pl__hc2(puQ, uH, 7, 8);
        uH3 = PL__SCRAMBLE(uH);
        uH = pl__hc2(puQ, uH, 9,10);
        puT = ppuCHash[uH3];
        if(puT)
            PL__TRY(puT,1);
        uH = pl__hc2(puQ, uH,11,12);
        uH4 = PL__SCRAMBLE(uH);
        puT = ppuCHash[uH4];
        if(puT)
            PL__TRY(puT,1);

        // because we use a shared hash table, can only update it
        // _after_ we've probed all of them
        ppuCHash[uH1] = ppuCHash[uH2] = ppuCHash[uH3] = ppuCHash[uH4] = puQ;

        if (iBest > 2)
        {
            PL_ASSERT(iDist > 0);
        }

        // see if our best match qualifies
        if(iBest < 3) // fast path literals
        {
            ++puQ;
        }
        else if(iBest <= 0x80 && iDist <= 0x100)
        {
            pl__out_literals(puLitStart, puQ - puLitStart);
            puLitStart = (puQ += iBest);
            pl__out(0x80 + iBest - 1);
            pl__out(iDist - 1);
        }
        else if(iBest > 5 && iBest <= 0x100 && iDist <= 0x4000)
        {
            pl__out_literals(puLitStart, puQ - puLitStart);
            puLitStart = (puQ += iBest);
            pl__out2(0x4000 + iDist - 1);
            pl__out(iBest - 1);
        }
        else if(iBest > 7 && iBest <= 0x100 && iDist <= 0x80000)
        {
            pl__out_literals(puLitStart, puQ - puLitStart);
            puLitStart = (puQ += iBest);
            pl__out3(0x180000 + iDist - 1);
            pl__out(iBest - 1);
        }
        else if(iBest > 8 && iBest <= 0x10000 && iDist <= 0x80000)
        {
            pl__out_literals(puLitStart, puQ - puLitStart);
            puLitStart = (puQ += iBest);
            pl__out3(0x100000 + iDist - 1);
            pl__out2(iBest - 1);
        }
        else if (iBest > 9 && iDist <= 0x1000000)
        {
            if(iBest > 65536)
                iBest = 65536;
            pl__out_literals(puLitStart, puQ - puLitStart);
            puLitStart = (puQ += iBest);
            if(iBest <= 0x100)
            {
                pl__out(0x06);
                pl__out3(iDist - 1);
                pl__out(iDist - 1);
            }
            else
            {
                pl__out(0x04);
                pl__out3(iDist - 1);
                pl__out2(iBest - 1);
            }
        }
        else // fallback literals if no match was a balanced tradeoff
        {
            ++puQ;
        }
    }

    // if we didn't get all the way, add the rest to literals
    if (puQ - puStart < iLength)
        puQ = puStart + iLength;

    // the literals are everything from lit_start to q
    *piPendingLiterals = (int) (puQ - puLitStart);

    guRunningAdler = pl__adler32(guRunningAdler, puStart, (int) (puQ - puStart));
    return (int)(puQ - puStart);
}

static int
pl__compress_inner(uint8_t* puInput, uint32_t uLength)
{
    int iLiterals = 0;
    uint32_t uLen = 0;

    uint8_t** ppuCHash = PL_ALLOC(guHashSize * sizeof(uint8_t*));
    if (ppuCHash == NULL)
        return 0; // failure
    for (uint32_t i = 0; i < guHashSize; ++i)
        ppuCHash[i] = NULL;

    // stream signature
    pl__out(0x57);
    pl__out(0xbc);
    pl__out2(0);

    pl__out4(0); // 64-bit length requires 32-bit leading 0
    pl__out4(uLength);
    pl__out4(gpiWindow);

    guRunningAdler = 1;

    uLen = pl__compress_chunk(puInput, puInput, puInput + uLength, uLength, &iLiterals, ppuCHash, guHashSize - 1);
    PL_ASSERT(uLen == uLength);

    pl__out_literals(puInput + uLength - iLiterals, iLiterals);

    PL_FREE(ppuCHash);

    pl__out2(0x05fa); // end opcode
    pl__out4(guRunningAdler);
    return 1; // success
}

static inline uint32_t
pl__decompress_length(const uint8_t* puInput)
{
    return (puInput[8] << 24) + (puInput[9] << 16) + (puInput[10] << 8) + puInput[11];
}

static void
pl__match(const uint8_t* puData, uint32_t uLength)
{
    // INVERSE of memmove... write each byte before copying the next...
    PL_ASSERT(gpuDOut + uLength <= gpuBarrier);
    if(gpuDOut + uLength > gpuBarrier)
    {
        gpuDOut += uLength;
        return;
    }

    if(puData < gpuBarrier4)
    {
        gpuDOut = gpuBarrier + 1;
        return;
    }
    
    while(uLength--)
        *gpuDOut++ = *puData++;
}

static void
pl__lit(const uint8_t* puData, uint32_t uLength)
{
    PL_ASSERT(gpuDOut + uLength <= gpuBarrier);
    if(gpuDOut + uLength > gpuBarrier)
    {
        gpuDOut += uLength;
        return;
    }

    if(puData < gpuBarrier2)
    {
        gpuDOut = gpuBarrier + 1;
        return;
    }

    memcpy(gpuDOut, puData, uLength);
    gpuDOut += uLength;
}

#define pl__in2(x) ((puI[x] << 8) + puI[(x)+1])
#define pl__in3(x) ((puI[x] << 16) + pl__in2((x)+1))
#define pl__in4(x) ((puI[x] << 24) + pl__in3((x)+1))

static uint8_t*
pl__decompress_token(uint8_t* puI)
{
    if (*puI >= 0x20) // use fewer if's for cases that expand small
    { 
        if (*puI >= 0x80)
        {
            pl__match(gpuDOut - puI[1] - 1, puI[0] - 0x80 + 1);
            puI += 2;
        }
        else if (*puI >= 0x40) 
        {
            pl__match(gpuDOut - (pl__in2(0) - 0x4000 + 1), puI[2] + 1);
            puI += 3;
        }
        else /* *i >= 0x20 */
        {
            pl__lit(puI + 1, puI[0] - 0x20 + 1);
            puI += 1 + (puI[0] - 0x20 + 1);
        }
    }
    else // more ifs for cases that expand large, since overhead is amortized
    { 
        if (*puI >= 0x18)
        {
            pl__match(gpuDOut - (pl__in3(0) - 0x180000 + 1), puI[3] + 1);
            puI += 4;
        }
        else if (*puI >= 0x10)
        {
            pl__match(gpuDOut - (pl__in3(0) - 0x100000 + 1), pl__in2(3) + 1);
            puI += 5;
        }
        else if (*puI >= 0x08)
        {
            pl__lit(puI + 2, pl__in2(0) - 0x0800 + 1);
            puI += 2 + (pl__in2(0) - 0x0800 + 1);
        }
        else if (*puI == 0x07)
        {
            pl__lit(puI + 3, pl__in2(1) + 1);
            puI += 3 + (pl__in2(1) + 1);
        }
        else if (*puI == 0x06)
        {
            pl__match(gpuDOut - (pl__in3(1) + 1), puI[4] + 1);
            puI += 5;
        }
        else if (*puI == 0x04)
        {
            pl__match(gpuDOut - (pl__in3(1) + 1), pl__in2(4) + 1);
            puI += 6;
        }
    }
    return puI;
}

static uint32_t
pl__decompress(uint8_t* puOutput, uint8_t* puI, uint32_t uLength)
{
    uint32_t uOLen = 0;
    if (pl__in4(0) != 0x57bC0000)
        return 0;
    if (pl__in4(4) != 0) // error! stream is > 4GB
        return 0; 
    uOLen = pl__decompress_length(puI);
    gpuBarrier2 = puI;
    gpuBarrier3 = puI + uLength;
    gpuBarrier = puOutput + uOLen;
    gpuBarrier4 = puOutput;
    puI += 16;

    gpuDOut = puOutput;
    while (1)
    {
        uint8_t* puOldI = puI;
        puI = pl__decompress_token(puI);
        if (puI == puOldI)
        {
            if (*puI == 0x05 && puI[1] == 0xfa)
            {
                PL_ASSERT(gpuDOut == puOutput + uOLen);
                if (gpuDOut != puOutput + uOLen)
                    return 0;
                if (pl__adler32(1, puOutput, uOLen) != (uint32_t) pl__in4(2))
                    return 0;
                return uOLen;
            }
            else
            {
                PL_ASSERT(0); // NOTREACHED
                return 0;
            }
        }
        PL_ASSERT(gpuDOut <= puOutput + uOLen);
        if (gpuDOut > puOutput + uOLen)
            return 0;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

uint32_t
pl_compress(uint8_t* puDataIn, uint32_t uSize, uint8_t* puDataOut, uint32_t uSizeOut)
{
    gpuOut = puDataOut;
    guOutBytes = 0;
    guOutBytesAvailable = uSizeOut;
    pl__compress_inner(puDataIn, uSize);
    return guOutBytes;
}

uint32_t
pl_decompress(uint8_t* puDataIn, uint32_t size, uint8_t* puDataOut, uint32_t uSizeOut)
{
    if(puDataOut == NULL)
        return pl__decompress_length(puDataIn);
        
    uint32_t uResult = pl__decompress(puDataOut, puDataIn, size);
    PL_ASSERT(uResult <= uSizeOut);
    return uResult;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_compress_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plCompressI tApi = {
        .compress   = pl_compress,
        .decompress = pl_decompress,
    };
    pl_set_api(ptApiRegistry, plCompressI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    #endif

}

PL_EXPORT void
pl_unload_compress_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plCompressI* ptApi = pl_get_api_latest(ptApiRegistry, plCompressI);
    ptApiRegistry->remove_api(ptApi);
}
