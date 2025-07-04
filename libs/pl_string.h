/*
   pl_string.h
     * no dependencies
     * simple string ops

   Do this:
        #define PL_STRING_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_STRING_IMPLEMENTATION
   #include "pl_string.h"
*/

// library version (format XYYZZ)
#define PL_STRING_VERSION    "1.1.2"
#define PL_STRING_VERSION_NUM 10102

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] public api
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STRING_H
#define PL_STRING_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h> // uint32_t
#include <stdbool.h> // bool
#include <stddef.h> // size_t

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// hashing
uint32_t    pl_str_hash_data(const void* pData, size_t szDataSize, uint32_t uSeed);
uint32_t    pl_str_hash     (const char* pcData, size_t szDataSize, uint32_t uSeed);

// file/path string ops
const char* pl_str_get_file_extension(const char* pcFilePath, char* pcExtensionOut, size_t szOutSize);
const char* pl_str_get_file_name     (const char* pcFilePath, char* pcFileOut, size_t szOutSize);
bool        pl_str_get_file_name_only(const char* pcFilePath, char* pcFileOut, size_t szOutSize);
bool        pl_str_get_directory     (const char* pcFilePath, char* pcDirectoryOut, size_t szOutSize);

// misc. opts
bool               pl_str_concatenate    (const char* pcStr0, const char* pcStr1, char* pcStringOut, size_t szDataSize);
bool               pl_str_equal          (const char* pcStr0, const char* pcStr1);
bool               pl_str_contains       (const char* pcStr, const char* pcSub);
int                pl_text_char_from_utf8(uint32_t* puOutChars, const char* pcInText, const char* pcTextEnd);
static inline char pl_str_to_upper       (char c) { return (c >= 'a' && c <= 'z') ? c &= ~32 : c; }

#endif // PL_STRING_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] CRC lookup table
// [SECTION] public api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifdef PL_STRING_IMPLEMENTATION

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h>  // memcpy, strlen
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] CRC lookup table
//-----------------------------------------------------------------------------

// (borrowed from Dear ImGui)
// CRC32 needs a 1KB lookup table (not cache friendly)
static const uint32_t gauCrc32LookupTable[256] =
{
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
};

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

uint32_t
pl_str_hash_data(const void* pData, size_t szDataSize, uint32_t uSeed)
{
    uint32_t uCrc = ~uSeed;
    const unsigned char* pucData = (const unsigned char*)pData;
    const uint32_t* puCrc32Lut = gauCrc32LookupTable;
    while (szDataSize-- != 0)
        uCrc = (uCrc >> 8) ^ puCrc32Lut[(uCrc & 0xFF) ^ *pucData++];
    return ~uCrc;  
}

uint32_t
pl_str_hash(const char* pcData, size_t szDataSize, uint32_t uSeed)
{
    uSeed = ~uSeed;
    uint32_t uCrc = uSeed;
    const unsigned char* pucData = (const unsigned char*)pcData;
    const uint32_t* puCrc32Lut = gauCrc32LookupTable;
    if (szDataSize != 0)
    {
        while (szDataSize-- != 0)
        {
            unsigned char c = *pucData++;
            if (c == '#' && szDataSize >= 2 && pucData[0] == '#' && pucData[1] == '#')
                uCrc = uSeed;
            uCrc = (uCrc >> 8) ^ puCrc32Lut[(uCrc & 0xFF) ^ c];
        }
    }
    else
    {
        unsigned char c = *pucData++;
        while (c)
        {
            if (c == '#' && pucData[0] == '#' && pucData[1] == '#')
                uCrc = uSeed;
            uCrc = (uCrc >> 8) ^ puCrc32Lut[(uCrc & 0xFF) ^ c];
            c = *pucData;
            pucData++;
            
        }
    }
    return ~uCrc;
}

const char*
pl_str_get_file_extension(const char* pcFilePath, char* pcExtensionOut, size_t szOutSize)
{
    const char* pcResult = NULL;
    const size_t szLen = strlen(pcFilePath);

    // check if string includes directory
    bool bHasExtension = false;
    for(size_t i = 0; i < szLen; i++)
    {
        char c = pcFilePath[szLen - i - 1];
        if(c == '/' || c == '\\')
        {
            break;
        }

        if(c == '.')
        {
            bHasExtension = true;
            break;
        }
    }

    if(bHasExtension)
    {
        for(size_t i = 0; i < szLen; i++)
        {
            char c = pcFilePath[szLen - i - 1];
            if(c == '.')
            {
                if(pcExtensionOut)
                    strncpy(pcExtensionOut, &pcFilePath[szLen - i], szOutSize);
                pcResult = &pcFilePath[szLen - i];
                break;
            }
        }
    }
    else
    {
        if(pcExtensionOut)
            memset(pcExtensionOut, 0, szOutSize);
    }

    return pcResult; 
}

const char*
pl_str_get_file_name(const char* pcFilePath, char* pcFileOut, size_t szOutSize)
{
    const char* pcResult = pcFilePath;
    const size_t szLen = strlen(pcFilePath);

    // check if string includes directory
    uint32_t uSlashCount = 0;
    for(size_t i = 0; i < szLen; i++)
    {
        char c = pcFilePath[szLen - i - 1];
        if(c == '/' || c == '\\')
            uSlashCount++;
    }

    if(uSlashCount > 0)
    {
        for(size_t i = 0; i < szLen; i++)
        {
            char c = pcFilePath[i];
            if(c == '/' || c == '\\')
                uSlashCount--;

            if(uSlashCount == 0)
            {
                if(pcFileOut)
                    strncpy(pcFileOut, &pcFilePath[i + 1], szOutSize);
                pcResult = &pcFilePath[i + 1];
                break;
            }
        }
    }
    else
    {
        if(pcFileOut)
        {
            size_t szCopySize = szLen + 1;
            if(szCopySize > szOutSize)
                szCopySize = szOutSize;
            memcpy(pcFileOut, pcFilePath, szCopySize);
        }
    }

    return pcResult;
}

bool
pl_str_get_file_name_only(const char* pcFilePath, char* pcFileOut, size_t szOutSize)
{
    PL_ASSERT(pcFileOut && "pl_str_get_file_name_only requires pcFileOut to be valid pointer");
    
    if(pcFileOut == NULL)
        return false;
    
    const size_t szLen = strlen(pcFilePath);

    // check if string includes directory
    uint32_t uSlashCount = 0;
    for(size_t i = 0; i < szLen; i++)
    {
        char c = pcFilePath[szLen - i - 1];
        if(c == '/' || c == '\\')
            uSlashCount++;
    }

    if(uSlashCount > 0)
    {
        for(size_t i = 0; i < szLen; i++)
        {
            char c = pcFilePath[i];
            if(c == '/' || c == '\\')
                uSlashCount--;

            if(uSlashCount == 0)
            {
                strncpy(pcFileOut, &pcFilePath[i + 1], szOutSize);
                break;
            }
        }
    }
    else
    {
        if(szLen + 1 > szOutSize)
            return false;
        memcpy(pcFileOut, pcFilePath, szLen + 1);
    }

    if(szLen > szOutSize)
        return false;
    bool bPeriodReached = false;
    for(size_t i = 0; i < szLen; i++)
    {
        char c = pcFileOut[i];
        if(c == '.')
        {
            bPeriodReached = true;
        }

        if(bPeriodReached)
        {
            pcFileOut[i] = 0;
        }
    }
    return true;
}

bool
pl_str_get_directory(const char* pcFilePath, char* pcDirectoryOut, size_t szOutSize)
{
    size_t szLen = strlen(pcFilePath);
    strncpy(pcDirectoryOut, pcFilePath, szOutSize);

    if(szLen > szOutSize || szOutSize < 2)
        return false;

    while(szLen > 0)
    {
        szLen--;

        if(pcDirectoryOut[szLen] == '/' || pcDirectoryOut[szLen] == '\\')
            break;
        pcDirectoryOut[szLen] = 0;
    }

    if(szLen == 0)
    {
        pcDirectoryOut[0] = '.';
        pcDirectoryOut[1] = '/';
    }
    return true;
}

bool
pl_str_concatenate(const char* pcStr0, const char* pcStr1, char* pcStringOut, size_t szDataSize)
{
    const size_t szLen0 = strlen(pcStr0);
    const size_t szLen1 = strlen(pcStr1);

    if(szLen0 + szLen1 > szDataSize)
    {
        return false;
    }

    if(pcStringOut)
    {
        memcpy(pcStringOut, pcStr0, szLen0);
        memcpy(&pcStringOut[szLen0], pcStr1, szLen1);
    }

    return true;
}

bool
pl_str_equal(const char* pcStr0, const char* pcStr1)
{
    return strcmp(pcStr0, pcStr1) == 0;
}

bool
pl_str_contains(const char* pcStr, const char* pcSub)
{
    const char* pcSubString = strstr(pcStr, pcSub);
    return pcSubString != NULL;
}

// Convert UTF-8 to 32-bit character, process single character input.
// A nearly-branchless UTF-8 decoder, based on work of Christopher Wellons (https://github.com/skeeto/branchless-utf8).
// We handle UTF-8 decoding error by skipping forward.

#define pl_string_min(Value1, Value2) ((Value1) > (Value2) ? (Value2) : (Value1))
int
pl_text_char_from_utf8(uint32_t* puOutChars, const char* pcInText, const char* pcTextEnd)
{
    static const char lengths[32] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };
    static const int masks[]  = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
    static const uint32_t mins[] = { 0x400000, 0, 0x80, 0x800, 0x10000 };
    static const int shiftc[] = { 0, 18, 12, 6, 0 };
    static const int shifte[] = { 0, 6, 4, 2, 0 };
    int len = lengths[*(const unsigned char*)pcInText >> 3];
    int wanted = len + (len ? 0 : 1);

    if (pcTextEnd == NULL)
        pcTextEnd = pcInText + wanted; // Max length, nulls will be taken into account.

    // Copy at most 'len' bytes, stop copying at 0 or past pcTextEnd. Branch predictor does a good job here,
    // so it is fast even with excessive branching.
    unsigned char s[4];
    s[0] = pcInText + 0 < pcTextEnd ? pcInText[0] : 0;
    s[1] = pcInText + 1 < pcTextEnd ? pcInText[1] : 0;
    s[2] = pcInText + 2 < pcTextEnd ? pcInText[2] : 0;
    s[3] = pcInText + 3 < pcTextEnd ? pcInText[3] : 0;

    // Assume a four-byte character and load four bytes. Unused bits are shifted out.
    *puOutChars  = (uint32_t)(s[0] & masks[len]) << 18;
    *puOutChars |= (uint32_t)(s[1] & 0x3f) << 12;
    *puOutChars |= (uint32_t)(s[2] & 0x3f) <<  6;
    *puOutChars |= (uint32_t)(s[3] & 0x3f) <<  0;
    *puOutChars >>= shiftc[len];

    // Accumulate the various error conditions.
    int e = 0;
    e  = (*puOutChars < mins[len]) << 6; // non-canonical encoding
    e |= ((*puOutChars >> 11) == 0x1b) << 7;  // surrogate half?
    e |= (*puOutChars > 0xFFFF) << 8;  // out of range?
    e |= (s[1] & 0xc0) >> 2;
    e |= (s[2] & 0xc0) >> 4;
    e |= (s[3]       ) >> 6;
    e ^= 0x2a; // top two bits of each tail byte correct?
    e >>= shifte[len];

    if (e)
    {
        // No bytes are consumed when *pcInText == 0 || pcInText == pcTextEnd.
        // One byte is consumed in case of invalid first byte of pcInText.
        // All available bytes (at most `len` bytes) are consumed on incomplete/invalid second to last bytes.
        // Invalid or incomplete input may consume less bytes than wanted, therefore every byte has to be inspected in s.
        wanted = pl_string_min(wanted, !!s[0] + !!s[1] + !!s[2] + !!s[3]);
        *puOutChars = 0xFFFD;
    }

    return wanted;
}

#endif // PL_STRING_IMPLEMENTATION