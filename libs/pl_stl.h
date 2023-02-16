/*
   pl_stl.h
*/

// library version
#define PL_STL_VERSION    "0.1.0"
#define PL_STL_VERSION_NUM 00100

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

typedef struct _plStlOptions plStlOptions;
typedef struct _plStlInfo plStlInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_load_stl(const char* pcData, size_t szDataSize, plStlOptions tOptions, float* afVertexStream0, float* afVertexStream1, uint32_t* auIndexBuffer, plStlInfo* ptInfoOut);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plStlOptions
{
    bool   bIncludeNormals;
    bool   bIncludeColor;
    float  afColor[4];
} plStlOptions;

typedef struct _plStlInfo
{
    size_t szVertexStream0Size;
    size_t szVertexStream1Size;
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
pl_load_stl(const char* pcData, size_t szDataSize, plStlOptions tOptions, float* afVertexStream0, float* afVertexStream1, uint32_t* auIndexBuffer, plStlInfo* ptInfoOut)
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

    ptInfoOut->szIndexBufferSize   = szFacetCount * 3;
    ptInfoOut->szVertexStream0Size = szVertexCount * 3;

    ptInfoOut->szVertexStream1Size = 0;
    if(tOptions.bIncludeColor && tOptions.bIncludeNormals)      ptInfoOut->szVertexStream1Size = szVertexCount * 8;
    else if(tOptions.bIncludeNormals || tOptions.bIncludeColor) ptInfoOut->szVertexStream1Size = szVertexCount * 4;

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
    if(afVertexStream0)
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
                    afVertexStream0[szVertexStream0Pos]     = strtof(pcReturnString, &pcEnd);
                    afVertexStream0[szVertexStream0Pos + 1] = strtof(pcEnd, &pcEnd);
                    afVertexStream0[szVertexStream0Pos + 2] = strtof(pcEnd, &pcEnd);
                    szVertexStream0Pos += 3;
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
                afVertexStream0[szVertexStream0Pos]     = afFacetBuffer[3];
                afVertexStream0[szVertexStream0Pos + 1] = afFacetBuffer[4];
                afVertexStream0[szVertexStream0Pos + 2] = afFacetBuffer[5];
                szVertexStream0Pos += 3;

                
                // vertex 1
                afVertexStream0[szVertexStream0Pos]     = afFacetBuffer[6];
                afVertexStream0[szVertexStream0Pos + 1] = afFacetBuffer[7];
                afVertexStream0[szVertexStream0Pos + 2] = afFacetBuffer[8];
                szVertexStream0Pos += 3;
                
                // vertex 2
                afVertexStream0[szVertexStream0Pos]     = afFacetBuffer[9];
                afVertexStream0[szVertexStream0Pos + 1] = afFacetBuffer[10];
                afVertexStream0[szVertexStream0Pos + 2] = afFacetBuffer[11];
                szVertexStream0Pos += 3;

                szCurrentCursor += 50;
            }
        }
    }

    szCurrentCursor = 0;
    size_t szVertexStream1Pos = 0;
    size_t szStride = 0;
    if(tOptions.bIncludeColor)   { szStride += 4; };
    if(tOptions.bIncludeNormals) { szStride += 4; szVertexStream1Pos = 4;};

    if(afVertexStream1 && tOptions.bIncludeColor)
    {

        for(size_t i = 0; i < szVertexCount; i++)
        {
            memcpy(&afVertexStream1[szVertexStream1Pos], tOptions.afColor, sizeof(float) * 4);
            szVertexStream1Pos += szStride;
        }
    }

    szVertexStream1Pos = 0;

    if(afVertexStream1 && tOptions.bIncludeNormals)
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
                    afVertexStream1[szVertexStream1Pos]     = fNx;
                    afVertexStream1[szVertexStream1Pos + 1] = fNy;
                    afVertexStream1[szVertexStream1Pos + 2] = fNz;
                    afVertexStream1[szVertexStream1Pos + 3] = 0.0f;
                    szVertexStream1Pos += szStride;

                    // vertex 1
                    afVertexStream1[szVertexStream1Pos]     = fNx;
                    afVertexStream1[szVertexStream1Pos + 1] = fNy;
                    afVertexStream1[szVertexStream1Pos + 2] = fNz;
                    afVertexStream1[szVertexStream1Pos + 3] = 0.0f;
                    szVertexStream1Pos += szStride;

                    // vertex 2
                    afVertexStream1[szVertexStream1Pos]     = fNx;
                    afVertexStream1[szVertexStream1Pos + 1] = fNy;
                    afVertexStream1[szVertexStream1Pos + 2] = fNz;
                    afVertexStream1[szVertexStream1Pos + 3] = 0.0f;
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
                afVertexStream1[szVertexStream1Pos]     = fNx;
                afVertexStream1[szVertexStream1Pos + 1] = fNy;
                afVertexStream1[szVertexStream1Pos + 2] = fNz;
                afVertexStream1[szVertexStream1Pos + 3] = 0.0f;
                szVertexStream1Pos += szStride;
                
                // vertex 1
                afVertexStream1[szVertexStream1Pos]     = fNx;
                afVertexStream1[szVertexStream1Pos + 1] = fNy;
                afVertexStream1[szVertexStream1Pos + 2] = fNz;
                afVertexStream1[szVertexStream1Pos + 3] = 0.0f;
                szVertexStream1Pos += szStride;

                // vertex 2
                afVertexStream1[szVertexStream1Pos]     = fNx;
                afVertexStream1[szVertexStream1Pos + 1] = fNy;
                afVertexStream1[szVertexStream1Pos + 2] = fNz;
                afVertexStream1[szVertexStream1Pos + 3] = 0.0f;
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