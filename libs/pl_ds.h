/*
   pl_ds.h
     * data structures
*/

// library version
#define PL_DS_VERSION    "0.4.1"
#define PL_DS_VERSION_NUM 00401

/*
Index of this file:
// [SECTION] documentation
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api (stretchy buffer)
// [SECTION] public api (hashmap)
// [SECTION] internal
// [SECTION] public api implementation (hashmap)
*/

//-----------------------------------------------------------------------------
// [SECTION] documentation
//-----------------------------------------------------------------------------

/*

STRETCHY BUFFER (dynamic array)

    pl_sb_capacity:
        uint32_t pl_sb_capacity(T*);
            Returns capacity (max number of items with current allocation)

    pl_sb_size:
        uint32_t pl_sb_size(T*);
            Returns number of items in buffer

    pl_sb_reserve:
        void pl_sb_reserve(T*, n);
            Reserves enough memory for n items

    pl_sb_resize:
        void pl_sb_resize(T*, n);
            Changes size of buffer to n items.

    pl_sb_pop:
        T pl_sb_pop(T*);
            Returns last item added to buffer and shrinks the buffer by 1(ensure buffer isn't empty!)
    
    pl_sb_pop_n:
        void pl_sb_pop_n(T*, uint32_t n);
            Pops the last n items from the buffer (ensure buffer isn't empty!)
    
    pl_sb_top:
        T pl_sb_top(T*);
            Returns last item added to buffer(ensure buffer isn't empty!)

    pl_sb_back:
        T pl_sb_back(T*);
            Returns last item added to buffer(ensure buffer isn't empty!)

    pl_sb_free:
        void pl_sb_free(T*);
            Frees memory held by buffer and sets pointer to NULL
    
    pl_sb_reset:
        void pl_sb_reset(T*)
            Sets size of buffer back to zero without freeing any memory

    pl_sb_end:
        T* pl_sb_end(T*);
            Returns a pointer to the end of the buffer (after the last item!)

    pl_sb_add:
        uint32_t pl_sb_add(T*);
            Adds room for 1 more item and returns the index to that item

    pl_sb_add_n:
        uint32_t pl_sb_add_(T*, n);
            Adds room for n more item and returns the index to the first new item

    pl_sb_add_ptr:
        T* pl_sb_add_ptr(T*);
            Adds room for 1 more item and returns the pointer to it

    pl_sb_add_ptr_n:
        T* pl_sb_add_ptr_n(T*, n);
            Adds room for n more item and returns the pointer to the first new item

    pl_sb_push:
        T pl_sb_push(T*, T);
            Pushes an item into the buffer and returns a copy of it.

    pl_sb_del:
        void pl_sb_del(T*, i);
            Deletes the ith item from the buffer (uses memmove)

    pl_sb_del_n:
        void pl_sb_del_n(T*, i, n);
            Deletes n items starting at the ith index (uses memmove)

    pl_sb_del_swap:
        void pl_sb_del_swap(T*, i);
            Deletes the ith item from the buffer (swaps with last item, so faster but doesn't preserve order)

    pl_sb_insert:
        void pl_sb_insert(T*, i, T);
            Inserts new item v at the ith index (uses memmove)

    pl_sb_insert_n:
        void pl_sb_insert_n(T*, i, N);
            Inserts n new items starting at the ith index (uses memmove)

    pl_sb_sprintf:
        void pl_sb_sprintf(char*, pcFormat, ...);
            Inserts characters into a char stretchy buffer (similar to sprintf)

HASHMAPS

    pl_hm_hash_str:
        uint64_t pl_hm_hash_str(const char*);
            Returns the CRC64 hash of a string.

    pl_hm_hash:
        uint64_t pl_hm_hash(const void* pData, size_t szDataSize, uint64_t uSeed);
            Returns the CRC64 hash of some arbitrary data.

    pl_hm_resize:
        void pl_hm_resize(plHashMap*, uint32_t);
            Resizes the hashmap or frees it if zero is used.

    pl_hm_free:
        void pl_hm_free(plHashMap*);
            Frees the hashmap internal memory.

    pl_hm_insert:
        void pl_hm_insert(plHashMap*, uint64_t ulKey, uint64_t ulValue);
            Adds an entry to the hashmap where ulKey is a hashed key (usually a string) and
            ulValue is the index into the value array.

    pl_hm_remove:
        void pl_hm_remove(plHashMap*, uint64_t ulKey);
            Removes an entry from the hashmap and adds the index to the free index list.

    pl_hm_lookup:
        uint64_t pl_hm_lookup(plHashMap*, uint64_t ulKey);
            Returns the index into the value array if it already exists or UINT64_MAX if not.

    pl_hm_get_free_index:
        uint64_t pl_hm_get_free_index(plHashMap*);
            Returns a free index if one exists or UINT64_MAX if not.

    pl_hm_has_key:
        bool pl_hm_has_key(plHashMap*, uint64_t);
            Checks if key exists.

    pl_hm_has_key_str:
        bool pl_hm_has_key(plHashMap*, const char*);
            Same as pl_hm_has_key but performs the hash for you.

    pl_hm_insert_str:
        void pl_hm_insert_str(plHashMap*, const char* pcKey, uint64_t ulValue);
            Same as pl_hm_insert but performs the hash for you.

    pl_hm_lookup_str:
        uint64_t pl_hm_lookup_str(plHashMap*, const char* pcKey);
            Same as pl_hm_lookup but performs the hash for you.

    pl_hm_remove_str:
        void pl_hm_remove_str(plHashMap*, const char* pcKey);
            Same as pl_hm_remove but performs the hash for you.

COMPILE TIME OPTIONS

    * Change allocators by defining both:
        PL_LOG_ALLOC(x)
        PL_LOG_FREE(x)
    * Change initial hashmap size:
        PL_DS_HASHMAP_INITIAL_SIZE (default is 256)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DS_H
#define PL_DS_H

#if defined(PL_DS_ALLOC) && defined(PL_DS_FREE)
// ok
#elif !defined(PL_DS_ALLOC) && !defined(PL_DS_FREE)
// ok
#else
#error "Must define both or none of PL_DS_ALLOC and PL_DS_FREE"
#endif

#ifndef PL_DS_ALLOC
    #include <stdlib.h>
    #define PL_DS_ALLOC(x) malloc(x)
    #define PL_DS_FREE(x)  free((x))
#endif

#ifndef PL_DS_ASSERT
    #include <assert.h>
    #define PL_DS_ASSERT(x) assert((x))
#endif

#ifndef PL_DS_HASHMAP_INITIAL_SIZE
    #define PL_DS_HASHMAP_INITIAL_SIZE 256
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint32_t
#include <stdlib.h>  // malloc, free
#include <string.h>  // memset, memmove
#include <stdbool.h> // bool
#include <stdarg.h>  // arg vars
#include <stdio.h>   // vsprintf

//-----------------------------------------------------------------------------
// [SECTION] documentation
//-----------------------------------------------------------------------------

typedef struct _plHashMap
{
    uint32_t  _uItemCount;
    uint32_t  _uBucketCount;
    uint64_t* _aulKeys;         // stored keys used for rehashing during growth
    uint64_t* _aulValueIndices; // indices into value array (user held)
    uint64_t* _sbulFreeIndices; // free list of available indices
    uint64_t _aulStackKeys[PL_DS_HASHMAP_INITIAL_SIZE];
    uint64_t _aulStackValueIndices[PL_DS_HASHMAP_INITIAL_SIZE];
    bool     _bHeapOverflowInUse;
} plHashMap;

//-----------------------------------------------------------------------------
// [SECTION] public api (stretchy buffer)
//-----------------------------------------------------------------------------

#define pl_sb_capacity(buf) \
    ((buf) ? pl__sb_header((buf))->uCapacity : 0u)

#define pl_sb_size(buf) \
    ((buf) ? pl__sb_header((buf))->uSize : 0u)

#define pl_sb_pop(buf) \
    (buf)[--pl__sb_header((buf))->uSize]

#define pl_sb_pop_n(buf, n) \
    pl__sb_header((buf))->uSize-=(n)

#define pl_sb_top(buf) \
    ((buf)[pl__sb_header((buf))->uSize-1])

#define pl_sb_last(buf) \
    pl_sb_top((buf))

#define pl_sb_free(buf) \
    if((buf)){ PL_DS_FREE(pl__sb_header(buf));} (buf) = NULL;

#define pl_sb_reset(buf) \
    if((buf)){ pl__sb_header((buf))->uSize = 0u;}

#define pl_sb_back(buf) \
    pl_sb_top((buf))

#define pl_sb_end(buf) \
    ((buf) ? (buf) + pl__sb_header((buf))->uSize : (buf))

#define pl_sb_add_n(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), (n) ? (pl__sb_header(buf)->uSize += (n), pl__sb_header(buf)->uSize - (n)) : pl_sb_size(buf))

#define pl_sb_add(buf) \
    pl_sb_add_n((buf), 1)

#define pl_sb_add_ptr_n(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), (n) ? (pl__sb_header(buf)->uSize += (n), &(buf)[pl__sb_header(buf)->uSize - (n)]) : (buf))

#define pl_sb_add_ptr(buf, n) \
    pl_sb_add_ptr_n((buf), 1)

#define pl_sb_push(buf, v) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), 1, 8), (buf)[pl__sb_header((buf))->uSize++] = (v))

#define pl_sb_reserve(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n)))

#define pl_sb_resize(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), pl__sb_header((buf))->uSize = (n))

#define pl_sb_del_n(buf, i, n) \
    (memmove(&(buf)[i], &(buf)[(i) + (n)], sizeof *(buf) * (pl__sb_header(buf)->uSize - (n) - (i))), pl__sb_header(buf)->uSize -= (n))

#define pl_sb_del(buf, i) \
    pl_sb_del_n((buf), (i), 1)

#define pl_sb_del_swap(buf, i) \
    ((buf)[i] = pl_sb_last(buf), pl__sb_header(buf)->uSize -= 1)

#define pl_sb_insert_n(buf, i, n) \
    (pl_sb_add_n((buf), (n)), memmove(&(buf)[(i) + (n)], &(buf)[i], sizeof *(buf) * (pl__sb_header(buf)->uSize - (n) - (i))))

#define pl_sb_insert(buf, i, v) \
    (pl_sb_insert_n((buf), (i), 1), (buf)[i] = (v))

#define pl_sb_sprintf(buf, pcFormat, ...) \
    pl__sb_sprintf(&(buf), (pcFormat), __VA_ARGS__)

//-----------------------------------------------------------------------------
// [SECTION] public api (hashmap)
//-----------------------------------------------------------------------------

static inline void     pl_hm_resize        (plHashMap* ptHashMap, uint32_t uBucketCount);
static inline void     pl_hm_free          (plHashMap* ptHashMap) { pl_hm_resize(ptHashMap, 0);}
static inline void     pl_hm_insert        (plHashMap* ptHashMap, uint64_t ulKey, uint64_t ulValue);
static inline void     pl_hm_remove        (plHashMap* ptHashMap, uint64_t ulKey);
static inline uint64_t pl_hm_lookup        (const plHashMap* ptHashMap, uint64_t ulKey);
static inline uint64_t pl_hm_get_free_index(plHashMap* ptHashMap);
static inline bool     pl_hm_has_key       (plHashMap* ptHashMap, uint64_t ulKey);
static inline uint64_t pl_hm_hash_str      (const char* pcKey);
static inline uint64_t pl_hm_hash          (const void* pData, size_t szDataSize, uint64_t uSeed);
static inline void     pl_hm_insert_str    (plHashMap* ptHashMap, const char* pcKey, uint64_t ulValue);
static inline void     pl_hm_remove_str    (plHashMap* ptHashMap, const char* pcKey);
static inline uint64_t pl_hm_lookup_str    (const plHashMap* ptHashMap, const char* pcKey);
static inline bool     pl_hm_has_key_str   (plHashMap* ptHashMap, const char* pcKey);

//-----------------------------------------------------------------------------
// [SECTION] internal
//-----------------------------------------------------------------------------

#define pl__sb_header(buf) ((plSbHeader_*)(((char*)(buf)) - sizeof(plSbHeader_)))
#define pl__sb_may_grow(buf, s, n, m) pl__sb_may_grow_((void**)&(buf), (s), (n), (m))

typedef struct
{
    uint32_t uSize;
    uint32_t uCapacity;
} plSbHeader_;

static void
pl__sb_grow(void** ptrBuffer, size_t szElementSize, size_t szNewItems)
{

    plSbHeader_* ptOldHeader = pl__sb_header(*ptrBuffer);

    plSbHeader_* ptNewHeader = (plSbHeader_*)PL_DS_ALLOC((ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plSbHeader_)); //-V592
    memset(ptNewHeader, 0, (ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plSbHeader_));
    if(ptNewHeader)
    {
        ptNewHeader->uSize = ptOldHeader->uSize;
        ptNewHeader->uCapacity = ptOldHeader->uCapacity + (uint32_t)szNewItems;
        memcpy(&ptNewHeader[1], *ptrBuffer, ptOldHeader->uSize * szElementSize);
        PL_DS_FREE(ptOldHeader);
        *ptrBuffer = &ptNewHeader[1];
    }
}

static void
pl__sb_may_grow_(void** ptrBuffer, size_t szElementSize, size_t szNewItems, size_t szMinCapacity)
{
    if(*ptrBuffer)
    {   
        plSbHeader_* ptOriginalHeader = pl__sb_header(*ptrBuffer);
        if(ptOriginalHeader->uSize + szNewItems > ptOriginalHeader->uCapacity)
        {
            pl__sb_grow(ptrBuffer, szElementSize, szNewItems);
        }
    }
    else // first run
    {
        plSbHeader_* ptHeader = (plSbHeader_*)PL_DS_ALLOC(szMinCapacity * szElementSize + sizeof(plSbHeader_));
        memset(ptHeader, 0, szMinCapacity * szElementSize + sizeof(plSbHeader_));
        if(ptHeader)
        {
            *ptrBuffer = &ptHeader[1]; 
            ptHeader->uSize = 0u;
            ptHeader->uCapacity = (uint32_t)szMinCapacity;
        }
    }     
}

static void
pl__sb_vsprintf(char** ppcBuffer, const char* pcFormat, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    int32_t n = vsnprintf(NULL, 0, pcFormat, args2);
    va_end(args2);
    uint32_t an = pl_sb_size(*ppcBuffer);
    pl_sb_resize(*ppcBuffer, an + n + 1);
    vsnprintf(*ppcBuffer + an, n + 1, pcFormat, args);
}

static void
pl__sb_sprintf(char** ppcBuffer, const char* pcFormat, ...)
{
    va_list args;
    va_start(args, pcFormat);
    pl__sb_vsprintf(ppcBuffer, pcFormat, args);
    va_end(args);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation (hashmap)
//-----------------------------------------------------------------------------

static inline void
pl_hm_resize(plHashMap* ptHashMap, uint32_t uBucketCount)
{
    const uint32_t uOldBucketCount = ptHashMap->_uBucketCount;
    uint64_t* sbulOldValueIndices = ptHashMap->_aulValueIndices;
    uint64_t* aulOldKeys = ptHashMap->_aulKeys;

    ptHashMap->_uBucketCount = uBucketCount;
    if(uBucketCount > 0)
    {
        
        ptHashMap->_aulValueIndices = (uint64_t*)PL_DS_ALLOC(sizeof(uint64_t) * ptHashMap->_uBucketCount);
        ptHashMap->_aulKeys  = (uint64_t*)PL_DS_ALLOC(sizeof(uint64_t) * ptHashMap->_uBucketCount);
        memset(ptHashMap->_aulValueIndices, 0xff, sizeof(uint64_t) * ptHashMap->_uBucketCount);
        memset(ptHashMap->_aulKeys, 0xff, sizeof(uint64_t) * ptHashMap->_uBucketCount);
    
        for(uint32_t i = 0; i < uOldBucketCount; i++)
        {
            const uint64_t ulKey = aulOldKeys[i];
            uint64_t ulOldModKey = ulKey % uOldBucketCount;


            while(aulOldKeys[ulOldModKey] != ulKey && aulOldKeys[ulOldModKey] != UINT64_MAX)
                ulOldModKey = (ulOldModKey + 1) % uOldBucketCount;

            const uint64_t ulValue = sbulOldValueIndices[ulOldModKey];
            ptHashMap->_uItemCount--;
            pl_hm_insert(ptHashMap, ulKey, ulValue);
        }
    }
    else
    {
        ptHashMap->_aulValueIndices = NULL;
        ptHashMap->_aulKeys = NULL;
        pl_sb_free(ptHashMap->_sbulFreeIndices);
        ptHashMap->_uItemCount = 0;
    }

    if(ptHashMap->_bHeapOverflowInUse && sbulOldValueIndices)
    {
        PL_DS_FREE(sbulOldValueIndices);
    }
    if(ptHashMap->_bHeapOverflowInUse && aulOldKeys)
    {
        PL_DS_FREE(aulOldKeys);
    }

    ptHashMap->_bHeapOverflowInUse = true;
}

static inline void
pl_hm_insert(plHashMap* ptHashMap, uint64_t ulKey, uint64_t ulValue)
{
    if(ptHashMap->_uBucketCount == 0)
    {
        ptHashMap->_aulValueIndices = ptHashMap->_aulStackValueIndices;
        ptHashMap->_aulKeys = ptHashMap->_aulStackKeys;
        memset(ptHashMap->_aulStackValueIndices, 0xff, sizeof(uint64_t) * PL_DS_HASHMAP_INITIAL_SIZE);
        memset(ptHashMap->_aulStackKeys, 0xff, sizeof(uint64_t) * PL_DS_HASHMAP_INITIAL_SIZE);
        ptHashMap->_uBucketCount = PL_DS_HASHMAP_INITIAL_SIZE;
        ptHashMap->_bHeapOverflowInUse = false;
    }
    else if(((float)ptHashMap->_uItemCount / (float)ptHashMap->_uBucketCount) > 0.60f)
        pl_hm_resize(ptHashMap, ptHashMap->_uBucketCount * 2);

    uint64_t ulModKey = ulKey % ptHashMap->_uBucketCount;

    while(ptHashMap->_aulKeys[ulModKey] != ulKey && ptHashMap->_aulKeys[ulModKey] != UINT64_MAX)
        ulModKey = (ulModKey + 1)  % ptHashMap->_uBucketCount;

    ptHashMap->_aulKeys[ulModKey] = ulKey;
    ptHashMap->_aulValueIndices[ulModKey] = ulValue;
    ptHashMap->_uItemCount++;
}

static inline void
pl_hm_remove(plHashMap* ptHashMap, uint64_t ulKey)
{
    PL_DS_ASSERT(ptHashMap->_uBucketCount > 0 && "hashmap has no items");

    const uint64_t ulModKey = ulKey % ptHashMap->_uBucketCount;
    const uint64_t ulValue = ptHashMap->_aulValueIndices[ulModKey];
    pl_sb_push(ptHashMap->_sbulFreeIndices, ulValue);
    ptHashMap->_aulValueIndices[ulModKey] = UINT64_MAX;
    ptHashMap->_aulKeys[ulModKey] = UINT64_MAX;
    ptHashMap->_uItemCount--;
}

static const uint64_t __gauCrc64LookupTableDS[256] =
{
    0x0000000000000000ULL, 0x01B0000000000000ULL, 0x0360000000000000ULL, 0x02D0000000000000ULL, 0x06C0000000000000ULL, 0x0770000000000000ULL, 0x05A0000000000000ULL, 0x0410000000000000ULL,
    0x0D80000000000000ULL, 0x0C30000000000000ULL, 0x0EE0000000000000ULL, 0x0F50000000000000ULL, 0x0B40000000000000ULL, 0x0AF0000000000000ULL, 0x0820000000000000ULL, 0x0990000000000000ULL,
    0x1B00000000000000ULL, 0x1AB0000000000000ULL, 0x1860000000000000ULL, 0x19D0000000000000ULL, 0x1DC0000000000000ULL, 0x1C70000000000000ULL, 0x1EA0000000000000ULL, 0x1F10000000000000ULL,
    0x1680000000000000ULL, 0x1730000000000000ULL, 0x15E0000000000000ULL, 0x1450000000000000ULL, 0x1040000000000000ULL, 0x11F0000000000000ULL, 0x1320000000000000ULL, 0x1290000000000000ULL,
    0x3600000000000000ULL, 0x37B0000000000000ULL, 0x3560000000000000ULL, 0x34D0000000000000ULL, 0x30C0000000000000ULL, 0x3170000000000000ULL, 0x33A0000000000000ULL, 0x3210000000000000ULL,
    0x3B80000000000000ULL, 0x3A30000000000000ULL, 0x38E0000000000000ULL, 0x3950000000000000ULL, 0x3D40000000000000ULL, 0x3CF0000000000000ULL, 0x3E20000000000000ULL, 0x3F90000000000000ULL,
    0x2D00000000000000ULL, 0x2CB0000000000000ULL, 0x2E60000000000000ULL, 0x2FD0000000000000ULL, 0x2BC0000000000000ULL, 0x2A70000000000000ULL, 0x28A0000000000000ULL, 0x2910000000000000ULL,
    0x2080000000000000ULL, 0x2130000000000000ULL, 0x23E0000000000000ULL, 0x2250000000000000ULL, 0x2640000000000000ULL, 0x27F0000000000000ULL, 0x2520000000000000ULL, 0x2490000000000000ULL,
    0x6C00000000000000ULL, 0x6DB0000000000000ULL, 0x6F60000000000000ULL, 0x6ED0000000000000ULL, 0x6AC0000000000000ULL, 0x6B70000000000000ULL, 0x69A0000000000000ULL, 0x6810000000000000ULL,
    0x6180000000000000ULL, 0x6030000000000000ULL, 0x62E0000000000000ULL, 0x6350000000000000ULL, 0x6740000000000000ULL, 0x66F0000000000000ULL, 0x6420000000000000ULL, 0x6590000000000000ULL,
    0x7700000000000000ULL, 0x76B0000000000000ULL, 0x7460000000000000ULL, 0x75D0000000000000ULL, 0x71C0000000000000ULL, 0x7070000000000000ULL, 0x72A0000000000000ULL, 0x7310000000000000ULL,
    0x7A80000000000000ULL, 0x7B30000000000000ULL, 0x79E0000000000000ULL, 0x7850000000000000ULL, 0x7C40000000000000ULL, 0x7DF0000000000000ULL, 0x7F20000000000000ULL, 0x7E90000000000000ULL,
    0x5A00000000000000ULL, 0x5BB0000000000000ULL, 0x5960000000000000ULL, 0x58D0000000000000ULL, 0x5CC0000000000000ULL, 0x5D70000000000000ULL, 0x5FA0000000000000ULL, 0x5E10000000000000ULL,
    0x5780000000000000ULL, 0x5630000000000000ULL, 0x54E0000000000000ULL, 0x5550000000000000ULL, 0x5140000000000000ULL, 0x50F0000000000000ULL, 0x5220000000000000ULL, 0x5390000000000000ULL,
    0x4100000000000000ULL, 0x40B0000000000000ULL, 0x4260000000000000ULL, 0x43D0000000000000ULL, 0x47C0000000000000ULL, 0x4670000000000000ULL, 0x44A0000000000000ULL, 0x4510000000000000ULL,
    0x4C80000000000000ULL, 0x4D30000000000000ULL, 0x4FE0000000000000ULL, 0x4E50000000000000ULL, 0x4A40000000000000ULL, 0x4BF0000000000000ULL, 0x4920000000000000ULL, 0x4890000000000000ULL,
    0xD800000000000000ULL, 0xD9B0000000000000ULL, 0xDB60000000000000ULL, 0xDAD0000000000000ULL, 0xDEC0000000000000ULL, 0xDF70000000000000ULL, 0xDDA0000000000000ULL, 0xDC10000000000000ULL,
    0xD580000000000000ULL, 0xD430000000000000ULL, 0xD6E0000000000000ULL, 0xD750000000000000ULL, 0xD340000000000000ULL, 0xD2F0000000000000ULL, 0xD020000000000000ULL, 0xD190000000000000ULL,
    0xC300000000000000ULL, 0xC2B0000000000000ULL, 0xC060000000000000ULL, 0xC1D0000000000000ULL, 0xC5C0000000000000ULL, 0xC470000000000000ULL, 0xC6A0000000000000ULL, 0xC710000000000000ULL,
    0xCE80000000000000ULL, 0xCF30000000000000ULL, 0xCDE0000000000000ULL, 0xCC50000000000000ULL, 0xC840000000000000ULL, 0xC9F0000000000000ULL, 0xCB20000000000000ULL, 0xCA90000000000000ULL,
    0xEE00000000000000ULL, 0xEFB0000000000000ULL, 0xED60000000000000ULL, 0xECD0000000000000ULL, 0xE8C0000000000000ULL, 0xE970000000000000ULL, 0xEBA0000000000000ULL, 0xEA10000000000000ULL,
    0xE380000000000000ULL, 0xE230000000000000ULL, 0xE0E0000000000000ULL, 0xE150000000000000ULL, 0xE540000000000000ULL, 0xE4F0000000000000ULL, 0xE620000000000000ULL, 0xE790000000000000ULL,
    0xF500000000000000ULL, 0xF4B0000000000000ULL, 0xF660000000000000ULL, 0xF7D0000000000000ULL, 0xF3C0000000000000ULL, 0xF270000000000000ULL, 0xF0A0000000000000ULL, 0xF110000000000000ULL,
    0xF880000000000000ULL, 0xF930000000000000ULL, 0xFBE0000000000000ULL, 0xFA50000000000000ULL, 0xFE40000000000000ULL, 0xFFF0000000000000ULL, 0xFD20000000000000ULL, 0xFC90000000000000ULL,
    0xB400000000000000ULL, 0xB5B0000000000000ULL, 0xB760000000000000ULL, 0xB6D0000000000000ULL, 0xB2C0000000000000ULL, 0xB370000000000000ULL, 0xB1A0000000000000ULL, 0xB010000000000000ULL,
    0xB980000000000000ULL, 0xB830000000000000ULL, 0xBAE0000000000000ULL, 0xBB50000000000000ULL, 0xBF40000000000000ULL, 0xBEF0000000000000ULL, 0xBC20000000000000ULL, 0xBD90000000000000ULL,
    0xAF00000000000000ULL, 0xAEB0000000000000ULL, 0xAC60000000000000ULL, 0xADD0000000000000ULL, 0xA9C0000000000000ULL, 0xA870000000000000ULL, 0xAAA0000000000000ULL, 0xAB10000000000000ULL,
    0xA280000000000000ULL, 0xA330000000000000ULL, 0xA1E0000000000000ULL, 0xA050000000000000ULL, 0xA440000000000000ULL, 0xA5F0000000000000ULL, 0xA720000000000000ULL, 0xA690000000000000ULL,
    0x8200000000000000ULL, 0x83B0000000000000ULL, 0x8160000000000000ULL, 0x80D0000000000000ULL, 0x84C0000000000000ULL, 0x8570000000000000ULL, 0x87A0000000000000ULL, 0x8610000000000000ULL,
    0x8F80000000000000ULL, 0x8E30000000000000ULL, 0x8CE0000000000000ULL, 0x8D50000000000000ULL, 0x8940000000000000ULL, 0x88F0000000000000ULL, 0x8A20000000000000ULL, 0x8B90000000000000ULL,
    0x9900000000000000ULL, 0x98B0000000000000ULL, 0x9A60000000000000ULL, 0x9BD0000000000000ULL, 0x9FC0000000000000ULL, 0x9E70000000000000ULL, 0x9CA0000000000000ULL, 0x9D10000000000000ULL,
    0x9480000000000000ULL, 0x9530000000000000ULL, 0x97E0000000000000ULL, 0x9650000000000000ULL, 0x9240000000000000ULL, 0x93F0000000000000ULL, 0x9120000000000000ULL, 0x9090000000000000ULL
};

static inline uint64_t
pl_hm_hash_str(const char* pcKey)
{

    uint64_t uCrc = 0;
    const unsigned char* pucData = (const unsigned char*)pcKey;

    unsigned char c = *pucData++;
    while (c)
    {
        uCrc = (uCrc >> 8) ^ __gauCrc64LookupTableDS[(uCrc & 0xFF) ^ c];
        c = *pucData;
        pucData++; 
    }
    return ~uCrc;
}

static inline uint64_t
pl_hm_hash(const void* pData, size_t szDataSize, uint64_t uSeed)
{
    uint64_t uCrc = ~uSeed;
    const unsigned char* pucData = (const unsigned char*)pData;
    while (szDataSize-- != 0)
        uCrc = (uCrc >> 8) ^ __gauCrc64LookupTableDS[(uCrc & 0xFF) ^ *pucData++];
    return ~uCrc;  
}

static inline uint64_t
pl_hm_lookup(const plHashMap* ptHashMap, uint64_t ulKey)
{
    if(ptHashMap->_uBucketCount == 0)
        return UINT64_MAX;

    uint64_t ulModKey = ulKey % ptHashMap->_uBucketCount;

    while(ptHashMap->_aulKeys[ulModKey] != ulKey && ptHashMap->_aulKeys[ulModKey] != UINT64_MAX)
        ulModKey = (ulModKey + 1) % ptHashMap->_uBucketCount;

    if(ptHashMap->_aulKeys[ulModKey] == UINT64_MAX)
        return UINT64_MAX;
    
    return ptHashMap->_aulValueIndices[ulModKey];
}

static inline uint64_t
pl_hm_get_free_index(plHashMap* ptHashMap)
{
    uint64_t ulResult = UINT64_MAX;
    if(pl_sb_size(ptHashMap->_sbulFreeIndices) > 0)
    {
        ulResult = pl_sb_pop(ptHashMap->_sbulFreeIndices);
    }
    return ulResult;
}

static inline void
pl_hm_insert_str(plHashMap* ptHashMap, const char* pcKey, uint64_t ulValue)
{
    pl_hm_insert(ptHashMap, pl_hm_hash_str(pcKey), ulValue);
}

static inline void
pl_hm_remove_str(plHashMap* ptHashMap, const char* pcKey)
{
    pl_hm_remove(ptHashMap, pl_hm_hash_str(pcKey));
}

static inline uint64_t
pl_hm_lookup_str(const plHashMap* ptHashMap, const char* pcKey)
{
    return pl_hm_lookup(ptHashMap, pl_hm_hash_str(pcKey));
}

static inline bool
pl_hm_has_key(plHashMap* ptHashMap, uint64_t ulKey)
{
    if(ptHashMap->_uBucketCount == 0)
        return false;

    uint64_t ulModKey = ulKey % ptHashMap->_uBucketCount;

    while(ptHashMap->_aulKeys[ulModKey] != ulKey && ptHashMap->_aulKeys[ulModKey] != UINT64_MAX)
        ulModKey = (ulModKey + 1)  % ptHashMap->_uBucketCount;

    return ptHashMap->_aulKeys[ulModKey] != UINT64_MAX;
}

static inline bool
pl_hm_has_key_str(plHashMap* ptHashMap, const char* pcKey)
{
    return pl_hm_has_key(ptHashMap, pl_hm_hash_str(pcKey));
}

#endif // PL_DS_H
