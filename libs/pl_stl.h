/*
   pl_stl.h
*/

// library version
#define PL_STL_VERSION    "0.2.0"
#define PL_STL_VERSION_NUM 00200

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STL_H
#define PL_STL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

typedef struct _plStlInfo plStlInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_load_stl(const char* pcData, size_t szDataSize, float* afPositionStream, float* afNormalStream, uint32_t* auIndexBuffer, plStlInfo* ptInfoOut);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plStlInfo
{
    size_t szPositionStreamSize;
    size_t szNormalStreamSize;
    size_t szIndexBufferSize;
    bool   bPreloaded;
} plStlInfo;

#endif // PL_STL_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifdef PL_STL_IMPLEMENTATION

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <float.h>

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool pl__move_to_next_line(const char* pcData, size_t szDataSize, size_t* pszCurrentPos);
static char pl__move_to_first_char(const char* pcData, size_t szDataSize, size_t* pszCurrentPos);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_load_stl(const char* pcData, size_t szDataSize, float* afPositionStream, float* afNormalStream, uint32_t* auIndexBuffer, plStlInfo* ptInfoOut)
{
    plStlInfo _tInternalInfo = {0};

    if(ptInfoOut == NULL)
        ptInfoOut = &_tInternalInfo;

    bool bAsci = strncmp(pcData, "solid", 5) == 0;
    size_t szFacetCount = ptInfoOut->bPreloaded ? ptInfoOut->szIndexBufferSize / 3 : 0;
    size_t szCurrentCursor = 0;
    size_t szVertexCount = ptInfoOut->bPreloaded ? ptInfoOut->szIndexBufferSize : 0;

    if(!ptInfoOut->bPreloaded)
    {

        // find number of vertices & facets
        if(bAsci)
        {
            while(pl__move_to_next_line(pcData, szDataSize, &szCurrentCursor))
            {
                const char cFirstChar = pl__move_to_first_char(pcData, szDataSize, &szCurrentCursor);
                switch(cFirstChar)
                {
                    case 'v': szVertexCount++; break;
                    case 'f': szFacetCount++;  break;
                    default: break;
                }
            }
        }
        else
        {
            szFacetCount = *(unsigned int*)&pcData[80];
            szVertexCount = szFacetCount * 3;
        }

        ptInfoOut->bPreloaded = true;
    }

    ptInfoOut->szIndexBufferSize    = szFacetCount * 3;
    ptInfoOut->szPositionStreamSize = szVertexCount * 3;
    ptInfoOut->szNormalStreamSize   = szVertexCount * 3;

    // fill index buffer if provided
    if(auIndexBuffer)
    {
        for(size_t i = 0; i < ptInfoOut->szIndexBufferSize; i++)
        {
            auIndexBuffer[i] = (uint32_t)i;
        }
    }

    // reset cursor
    szCurrentCursor = 0;
    size_t szVertexStream0Pos = 0;
    
    // fill vertex stream 0 if provided
    if(afPositionStream)
    {
        if(bAsci)
        {
            while(pl__move_to_next_line(pcData, szDataSize, &szCurrentCursor))
            {
                const char cFirstChar = pl__move_to_first_char(pcData, szDataSize, &szCurrentCursor);
                if(cFirstChar == 'v')
                {
                    szCurrentCursor += 6;
                    const char* pcReturnString = &pcData[szCurrentCursor];
                    char* pcEnd = NULL;
                    afPositionStream[szVertexStream0Pos]     = strtof(pcReturnString, &pcEnd);
                    afPositionStream[szVertexStream0Pos + 1] = strtof(pcEnd, &pcEnd);
                    afPositionStream[szVertexStream0Pos + 2] = strtof(pcEnd, &pcEnd);
                    afPositionStream += 3;
                }
            }
        }
        else
        {
            szCurrentCursor = 84;
            float afFacetBuffer[12] = {0};
            for(int facet = 0; facet < szFacetCount; facet++)
            {
                memcpy(afFacetBuffer, &pcData[szCurrentCursor], 48);
                
                // vertex 0
                afPositionStream[szVertexStream0Pos]     = afFacetBuffer[3];
                afPositionStream[szVertexStream0Pos + 1] = afFacetBuffer[4];
                afPositionStream[szVertexStream0Pos + 2] = afFacetBuffer[5];
                szVertexStream0Pos += 3;

                
                // vertex 1
                afPositionStream[szVertexStream0Pos]     = afFacetBuffer[6];
                afPositionStream[szVertexStream0Pos + 1] = afFacetBuffer[7];
                afPositionStream[szVertexStream0Pos + 2] = afFacetBuffer[8];
                szVertexStream0Pos += 3;
                
                // vertex 2
                afPositionStream[szVertexStream0Pos]     = afFacetBuffer[9];
                afPositionStream[szVertexStream0Pos + 1] = afFacetBuffer[10];
                afPositionStream[szVertexStream0Pos + 2] = afFacetBuffer[11];
                szVertexStream0Pos += 3;

                szCurrentCursor += 50;
            }
        }
    }

    szCurrentCursor = 0;
    size_t szVertexStream1Pos = 0;
    const size_t szStride = 3;

    if(afNormalStream)
    {
        if(bAsci)
        {
            while(pl__move_to_next_line(pcData, szDataSize, &szCurrentCursor))
            {
                const char cFirstChar = pl__move_to_first_char(pcData, szDataSize, &szCurrentCursor);
                if(cFirstChar == 'f')
                {
                    szCurrentCursor += 12;
                    const char* pcReturnString = &pcData[szCurrentCursor];
                    char* pcEnd = NULL;

                    const float fNx = strtof(pcReturnString, &pcEnd);
                    const float fNy = strtof(pcEnd, &pcEnd);
                    const float fNz = strtof(pcEnd, &pcEnd);

                    // vertex 0
                    afNormalStream[szVertexStream1Pos]     = fNx;
                    afNormalStream[szVertexStream1Pos + 1] = fNy;
                    afNormalStream[szVertexStream1Pos + 2] = fNz;
                    szVertexStream1Pos += szStride;

                    // vertex 1
                    afNormalStream[szVertexStream1Pos]     = fNx;
                    afNormalStream[szVertexStream1Pos + 1] = fNy;
                    afNormalStream[szVertexStream1Pos + 2] = fNz;
                    szVertexStream1Pos += szStride;

                    // vertex 2
                    afNormalStream[szVertexStream1Pos]     = fNx;
                    afNormalStream[szVertexStream1Pos + 1] = fNy;
                    afNormalStream[szVertexStream1Pos + 2] = fNz;
                    szVertexStream1Pos += szStride;
                }
            }
        }
        else
        {
            szCurrentCursor = 84;
            float afFacetBuffer[12] = {0};
            for(uint32_t i = 0; i < szFacetCount; i++)
            {
                memcpy(afFacetBuffer, &pcData[szCurrentCursor], 48);
                
                const float fNx = afFacetBuffer[0];
                const float fNy = afFacetBuffer[1];
                const float fNz = afFacetBuffer[2];

                // vertex 0
                afNormalStream[szVertexStream1Pos]     = fNx;
                afNormalStream[szVertexStream1Pos + 1] = fNy;
                afNormalStream[szVertexStream1Pos + 2] = fNz;
                szVertexStream1Pos += szStride;
                
                // vertex 1
                afNormalStream[szVertexStream1Pos]     = fNx;
                afNormalStream[szVertexStream1Pos + 1] = fNy;
                afNormalStream[szVertexStream1Pos + 2] = fNz;
                szVertexStream1Pos += szStride;

                // vertex 2
                afNormalStream[szVertexStream1Pos]     = fNx;
                afNormalStream[szVertexStream1Pos + 1] = fNy;
                afNormalStream[szVertexStream1Pos + 2] = fNz;
                szVertexStream1Pos += szStride;

                szCurrentCursor += 50;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static bool
pl__move_to_next_line(const char* pcData, size_t szDataSize, size_t* pszCurrentPos)
{
    bool bLineFound = false;
    while(*pszCurrentPos < szDataSize)
    {
        const char cCurrentCharacter = pcData[*pszCurrentPos];

        if(cCurrentCharacter == '\n')
        {
            bLineFound = true;
            (*pszCurrentPos)++;
            break;
        }
        (*pszCurrentPos)++;
    }

    return bLineFound;
}

static char
pl__move_to_first_char(const char* pcData, size_t szDataSize, size_t* pszCurrentPos)
{
    while(*pszCurrentPos < szDataSize)
    {
        const char cCurrentCharacter = pcData[*pszCurrentPos];

        switch(cCurrentCharacter)
        {
            // end of line or string
            case 0:
            case '\n':
                (*pszCurrentPos)++;
                return 0;

            // whitespace
            case ' ':
            case '\r':
            case '\t': break;

            // actual characters
            default: return cCurrentCharacter;
        }
        (*pszCurrentPos)++;
    }
    return 0;   
}

#endif // PL_STL_IMPLEMENTATION