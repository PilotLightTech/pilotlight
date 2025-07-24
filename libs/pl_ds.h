/*
   pl_ds.h
     * data structures
*/

// library version (format XYYZZ)
#define PL_DS_VERSION    "1.0.1"
#define PL_DS_VERSION_NUM 10001

/*
Index of this file:
// [SECTION] documentation
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api (stretchy buffer)
// [SECTION] public api (hashmap)
// [SECTION] public api (static hashmaps)
// [SECTION] internal (stretchy buffer)
// [SECTION] internal (hashmap)
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
        uint64_t pl_hm_hash(const void* pData, size_t dataSize, uint64_t seed);
            Returns the CRC64 hash of some arbitrary data.

    pl_hm_free:
        void pl_hm_free(plHashMap64*);
            Frees the hashmap internal memory.

    pl_hm_insert:
        void pl_hm_insert(plHashMap64*, uint64_t key, uint64_t value);
            Adds an entry to the hashmap where key is a hashed key (usually a string) and
            value is the index into the value array.

    pl_hm_remove:
        void pl_hm_remove(plHashMap64*, uint64_t key);
            Removes an entry from the hashmap and adds the index to the free index list.

    pl_hm_lookup:
        uint64_t pl_hm_lookup(plHashMap64*, uint64_t key);
            Returns the index into the value array if it already exists or PL_DS_HASH_INVALID if not.

    pl_hm_get_free_index:
        uint64_t pl_hm_get_free_index(plHashMap64*);
            Returns a free index if one exists or PL_DS_HASH_INVALID if not.

    pl_hm_has_key:
        bool pl_hm_has_key(plHashMap64*, uint64_t);
            Checks if key exists.

    pl_hm_has_key_ex:
        bool pl_hm_has_key_ex(plHashMap64*, uint64_t, uint64_t* puValue);
            Checks if key exists and fills out puValue if present.

    pl_hm_has_key_str:
        bool pl_hm_has_key(plHashMap64*, const char*);
            Same as pl_hm_has_key but performs the hash for you.

    pl_hm_has_key_str_ex:
        bool pl_hm_has_key(plHashMap64*, const char*, uint64_t* puValue);
            Same as pl_hm_has_key but performs the hash for you.

    pl_hm_insert_str:
        void pl_hm_insert_str(plHashMap64*, const char* key, uint64_t value);
            Same as pl__hm_insert but performs the hash for you.

    pl_hm_lookup_str:
        uint64_t pl_hm_lookup_str(plHashMap64*, const char* key);
            Same as pl_hm_lookup but performs the hash for you.

    pl_hm_remove_str:
        void pl_hm_remove_str(plHashMap64*, const char* key);
            Same as pl_hm_remove but performs the hash for you.

    pl_hm_size:
        uint32_t pl_hm_lookup_str(plHashMap64*);
            Returns the number of elements in the hashmap.

DEFINES

    * PL_DS_HASH_INVALID
    * PL_DS_HASH32_INVALID

COMPILE TIME OPTIONS

    * Change allocators by defining both:
        PL_DS_ALLOC(x)
        PL_DS_FREE(x)
    * Change initial hashmap size:
        PL_DS_HASHMAP_INITIAL_SIZE (default is 1024) // should be power of 2
    * Change assert by defining:
        PL_DS_ASSERT(x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DS_H
#define PL_DS_H

#if defined(PL_DS_ALLOC) && defined(PL_DS_FREE) && defined(PL_DS_ALLOC_INDIRECT)
// ok
#elif !defined(PL_DS_ALLOC) && !defined(PL_DS_FREE) && !defined(PL_DS_ALLOC_INDIRECT)
// ok
#else
#error "Must define all or none of PL_DS_ALLOC and PL_DS_FREE and PL_DS_ALLOC_INDIRECT"
#endif

#ifndef PL_DS_ALLOC
    #include <stdlib.h>
    #define PL_DS_ALLOC(x) malloc((x))
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) malloc((x))
    #define PL_DS_FREE(x)  free((x))
#endif

#ifndef PL_DS_ASSERT
    #include <assert.h>
    #define PL_DS_ASSERT(x) assert((x))
#endif

#ifndef PL_DS_HASHMAP_INITIAL_SIZE
    #define PL_DS_HASHMAP_INITIAL_SIZE 1024
#endif

#define PL_DS_HASH_INVALID UINT64_MAX
#define PL_DS_HASH32_INVALID UINT32_MAX

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint32_t
#include <string.h>  // memset, memmove
#include <stdbool.h> // bool
#include <stdarg.h>  // arg vars
#include <stdio.h>   // vsprintf

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
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n), __FILE__, __LINE__), (n) ? (pl__sb_header(buf)->uSize += (n), pl__sb_header(buf)->uSize - (n)) : pl_sb_size(buf))

#define pl_sb_add(buf) \
    pl_sb_add_n((buf), 1)

#define pl_sb_add_ptr_n(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n), __FILE__, __LINE__), (n) ? (pl__sb_header(buf)->uSize += (n), &(buf)[pl__sb_header(buf)->uSize - (n)]) : (buf))

#define pl_sb_add_ptr(buf, n) \
    pl_sb_add_ptr_n((buf), 1)

#define pl_sb_push(buf, v) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), 1, 8, __FILE__, __LINE__), (buf)[pl__sb_header((buf))->uSize++] = (v))

#define pl_sb_reserve(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n), __FILE__, __LINE__))

#define pl_sb_resize(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n), __FILE__, __LINE__), pl__sb_header((buf))->uSize = (n))

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

typedef struct _plHashMap32
{
    uint32_t  _uItemCount;
    uint32_t  _uBucketCapacity;
    uint64_t* _auKeys;         // stored keys used for rehashing during growth

    // specific to 32bit
    uint32_t* _auValueBucket;  // indices into value array (user held)
    uint32_t* _sbuFreeIndices; // free list of available indices
} plHashMap32;

typedef struct _plHashMap64
{
    uint32_t  _uItemCount;
    uint32_t  _uBucketCapacity;
    uint64_t* _auKeys;         // stored keys used for rehashing during growth

    // specific to 64bit
    uint64_t* _auValueBucket;  // indices into value array (user held)
    uint64_t* _sbuFreeIndices; // free list of available indices
} plHashMap64;

typedef plHashMap64 plHashMap;

// general
static inline uint64_t pl_hm_hash    (const void* data, size_t dataSize, uint64_t seed);
static inline uint64_t pl_hm_hash_str(const char*, uint64_t seed);

#define pl_hm_size(PLHM) \
    ((PLHM) ? (PLHM)->_uItemCount : 0)

#define pl_hm32_size         pl_hm_size
#define pl_hm64_size         pl_hm_size
#define pl_hm_insert         pl_hm64_insert
#define pl_hm_insert_str     pl_hm64_insert_str
#define pl_hm_lookup         pl_hm64_lookup
#define pl_hm_has_key        pl_hm64_has_key
#define pl_hm_has_key_ex     pl_hm64_has_key_ex
#define pl_hm_get_free_index pl_hm64_get_free_index
#define pl_hm_free           pl_hm64_free
#define pl_hm_remove         pl_hm64_remove
#define pl_hm_lookup_str     pl_hm64_lookup_str
#define pl_hm_remove_str     pl_hm64_remove_str
#define pl_hm_has_key_str    pl_hm64_has_key_str
#define pl_hm_has_key_str_ex pl_hm64_has_key_str_ex

// 64 bit
#define pl_hm64_insert(ptHashMap, uKey, uValue) \
    pl__hm_insert((ptHashMap), (uKey), (uValue), __FILE__, __LINE__)

#define pl_hm64_insert_str(ptHashMap, pcKey, uValue) \
    pl_hm64_insert((ptHashMap), (pl_hm_hash_str((pcKey), 0)), (uValue))

static inline uint64_t pl_hm64_lookup        (const plHashMap64*, uint64_t key);
static inline bool     pl_hm64_has_key       (const plHashMap64*, uint64_t key);
static inline bool     pl_hm64_has_key_ex    (const plHashMap64*, uint64_t key, uint64_t* valueOut);
static inline uint64_t pl_hm64_get_free_index(plHashMap64*);
static inline void     pl_hm64_free          (plHashMap64*);
static inline void     pl_hm64_remove        (plHashMap64*, uint64_t key);
static inline uint64_t pl_hm64_lookup_str    (const plHashMap64*, const char* key);
static inline void     pl_hm64_remove_str    (plHashMap64*, const char* key);
static inline bool     pl_hm64_has_key_str   (const plHashMap64*, const char* key);
static inline bool     pl_hm64_has_key_str_ex(const plHashMap64*, const char* key, uint64_t* valueOut);

// 32 bit
#define pl_hm32_insert(ptHashMap, uKey, uValue) \
    pl__hm_insert32((ptHashMap), (uKey), (uValue), __FILE__, __LINE__)

#define pl_hm32_insert_str(ptHashMap, pcKey, uValue) \
    pl_hm32_insert((ptHashMap), pl_hm_hash_str((pcKey), 0), (uValue))

static inline uint32_t pl_hm32_lookup        (const plHashMap32*, uint64_t key);
static inline bool     pl_hm32_has_key       (const plHashMap32*, uint64_t key);
static inline bool     pl_hm32_has_key_ex    (const plHashMap32*, uint64_t key, uint32_t* valueOut);
static inline uint32_t pl_hm32_get_free_index(plHashMap32*);
static inline void     pl_hm32_free          (plHashMap32*);
static inline void     pl_hm32_remove        (plHashMap32*, uint64_t key);
static inline uint32_t pl_hm32_lookup_str    (const plHashMap32*, const char* key);
static inline void     pl_hm32_remove_str    (plHashMap32*, const char* key);
static inline bool     pl_hm32_has_key_str   (const plHashMap32*, const char* key);
static inline bool     pl_hm32_has_key_str_ex(const plHashMap32*, const char* key, uint32_t* valueOut);

//-----------------------------------------------------------------------------
// [SECTION] public api (static hashmaps)
//-----------------------------------------------------------------------------

typedef struct _plHashMapStatic64
{
    uint64_t* auKeys;        // stored keys used for rehashing during growth
    uint64_t* auValueBucket; // indices into value array (user held)
    uint32_t  uBucketCount;
} plHashMapStatic64;

typedef struct _plHashMapStatic32
{
    uint64_t* auKeys;        // stored keys used for rehashing during growth
    uint32_t* auValueBucket; // indices into value array (user held)
    uint32_t  uBucketCount;
} plHashMapStatic32;

static inline void
pl_hms_clear(plHashMapStatic64* ptHashmap)
{
    memset(ptHashmap->auKeys, 0xff, sizeof(*ptHashmap->auKeys) * ptHashmap->uBucketCount);
    memset(ptHashmap->auValueBucket, 0xff, sizeof(*ptHashmap->auValueBucket) * ptHashmap->uBucketCount);
}

static inline void
pl_hms_clear32(plHashMapStatic32* ptHashmap)
{
    memset(ptHashmap->auKeys, 0xff, sizeof(*ptHashmap->auKeys) * ptHashmap->uBucketCount);
    memset(ptHashmap->auValueBucket, 0xff, sizeof(*ptHashmap->auValueBucket) * ptHashmap->uBucketCount);
}

static inline void
pl_hms_set(plHashMapStatic64* ptHashmap, uint64_t uKey, uint64_t uValue)
{
    uint64_t uBucketIndex = uKey % ptHashmap->uBucketCount;
    while(ptHashmap->auKeys[uBucketIndex] != uKey && ptHashmap->auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
        uBucketIndex = (uBucketIndex + 1) % ptHashmap->uBucketCount;
    ptHashmap->auKeys[uBucketIndex] = uKey;
    ptHashmap->auValueBucket[uBucketIndex] = uValue;
}

static inline void
pl_hms_set32(plHashMapStatic32* ptHashmap, uint64_t uKey, uint32_t uValue)
{
    uint64_t uBucketIndex = uKey % ptHashmap->uBucketCount;
    while(ptHashmap->auKeys[uBucketIndex] != uKey && ptHashmap->auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
        uBucketIndex = (uBucketIndex + 1) % ptHashmap->uBucketCount;
    ptHashmap->auKeys[uBucketIndex] = uKey;
    ptHashmap->auValueBucket[uBucketIndex] = uValue;
}

static inline uint64_t
pl_hms_get(const plHashMapStatic64* ptHashmap, uint64_t uKey)
{
    uint64_t uBucketIndex = uKey % ptHashmap->uBucketCount;
    while (ptHashmap->auKeys[uBucketIndex] != uKey && ptHashmap->auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
        uBucketIndex = (uBucketIndex + 1) % ptHashmap->uBucketCount;
    return ptHashmap->auKeys[uBucketIndex] == PL_DS_HASH_INVALID ? PL_DS_HASH_INVALID : ptHashmap->auValueBucket[uBucketIndex];
}

static inline uint32_t
pl_hms_get32(const plHashMapStatic32* ptHashmap, uint64_t uKey)
{
    uint64_t uBucketIndex = uKey % ptHashmap->uBucketCount;
    while (ptHashmap->auKeys[uBucketIndex] != uKey && ptHashmap->auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
        uBucketIndex = (uBucketIndex + 1) % ptHashmap->uBucketCount;
    return ptHashmap->auKeys[uBucketIndex] == PL_DS_HASH_INVALID ? PL_DS_HASH32_INVALID : ptHashmap->auValueBucket[uBucketIndex];
}

static inline void
pl_hms_set_str(plHashMapStatic64* ptHashmap, const char* pcKey, uint64_t uValue)
{
    pl_hms_set(ptHashmap, pl_hm_hash_str(pcKey, 0), uValue);
}

static inline void
pl_hms_set_str32(plHashMapStatic32* ptHashmap, const char* pcKey, uint32_t uValue)
{
    pl_hms_set32(ptHashmap, pl_hm_hash_str(pcKey, 0), uValue);
}

static inline uint64_t
pl_hms_get_str(const plHashMapStatic64* ptHashmap, const char* pcKey)
{
    return pl_hms_get(ptHashmap, pl_hm_hash_str(pcKey, 0));
}

static inline uint32_t
pl_hms_get_str32(const plHashMapStatic32* ptHashmap, const char* pcKey)
{
    return pl_hms_get32(ptHashmap, pl_hm_hash_str(pcKey, 0));
}

//-----------------------------------------------------------------------------
// [SECTION] internal (stretchy buffer)
//-----------------------------------------------------------------------------

#define pl__sb_header(buf) ((plSbHeader_*)(((char*)(buf)) - sizeof(plSbHeader_)))
#define pl__sb_may_grow(buf, s, n, m, X, Y) pl__sb_may_grow_((void**)&(buf), (s), (n), (m), __FILE__, __LINE__)

typedef struct
{
    uint32_t uSize;
    uint32_t uCapacity;
} plSbHeader_;

static void
pl__sb_grow(void** ptrBuffer, size_t szElementSize, size_t szNewItems, const char* pcFile, int iLine)
{

    plSbHeader_* ptOldHeader = pl__sb_header(*ptrBuffer);

    const size_t szNewSize = (ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plSbHeader_);
    plSbHeader_* ptNewHeader = (plSbHeader_*)PL_DS_ALLOC_INDIRECT(szNewSize, pcFile, iLine); //-V592
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
pl__sb_may_grow_(void** ptrBuffer, size_t szElementSize, size_t szNewItems, size_t szMinCapacity, const char* pcFile, int iLine)
{
    if(*ptrBuffer)
    {
        plSbHeader_* ptOriginalHeader = pl__sb_header(*ptrBuffer);
        if(ptOriginalHeader->uSize + szNewItems > ptOriginalHeader->uCapacity)
        {
            pl__sb_grow(ptrBuffer, szElementSize, szNewItems, pcFile, iLine);
        }
    }
    else // first run
    {
        const size_t szNewSize = szMinCapacity * szElementSize + sizeof(plSbHeader_);
        plSbHeader_* ptHeader = (plSbHeader_*)PL_DS_ALLOC_INDIRECT(szNewSize, pcFile, iLine);
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
// [SECTION] internal (hashmap)
//-----------------------------------------------------------------------------

static inline uint64_t pl__hm_lookup64(const plHashMap64*, uint64_t uKey, uint32_t* puBucketIndexOut, uint64_t* puValueOut);
static inline void     pl__hm_insert  (plHashMap64*, uint64_t uKey, uint64_t uValue, const char* pcFile, int iLine);
static inline uint32_t pl__hm_lookup32(const plHashMap32*, uint64_t uKey, uint32_t* puBucketIndexOut, uint32_t* puValueOut);
static inline void     pl__hm_insert32(plHashMap32*, uint64_t uKey, uint32_t uValue, const char* pcFile, int iLine);

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

static inline size_t
pl__ds_get_next_power_of_2(size_t n)
{ 
    size_t uResult = 1;
    if (n && !(n & (n - 1))) 
        uResult = n;
    while (uResult < n)
        uResult <<= 1;
    return uResult; 
}

static inline uint32_t
pl__hm_get_existing_bucket_index(const uint64_t* auKeys, uint32_t uBucketCount, uint64_t uKey)
{
    // this function assumes the key,pair definitely exists

	const uint32_t uMask = uBucketCount - 1; // assumes bucket count is power of 2
    uint32_t uBucketIndex = uKey & uMask;

    const uint32_t uOriginalBucketIndex = uBucketIndex;

    while(auKeys[uBucketIndex] != uKey && auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
    {
        uBucketIndex = (uBucketIndex + 1) & uMask;

        PL_DS_ASSERT(uBucketIndex != uOriginalBucketIndex && "should not be possible");

        // not found and we did a full wrap around
        if(uBucketIndex == uOriginalBucketIndex)
        {
            uBucketIndex = UINT32_MAX;
            break;
        }
    }

    return uBucketIndex;
}

static inline uint64_t
pl_hm_get_free_index(plHashMap64* ptHashMap)
{

    uint64_t uResult = PL_DS_HASH_INVALID;
    if(pl_sb_size(ptHashMap->_sbuFreeIndices) > 0)
    {
        uResult = pl_sb_pop(ptHashMap->_sbuFreeIndices);
    }
    return uResult;
}

static inline uint32_t
pl_hm32_get_free_index(plHashMap32* ptHashMap)
{

    uint32_t uResult = PL_DS_HASH32_INVALID;
    if(pl_sb_size(ptHashMap->_sbuFreeIndices) > 0)
    {
        uResult = pl_sb_pop(ptHashMap->_sbuFreeIndices);
    }
    return uResult;
}

static inline uint64_t
pl__hm_lookup64(const plHashMap64* ptHashMap, uint64_t uKey, uint32_t* puBucketIndexOut, uint64_t* puValueOut)
{

    // early exit checks
    if(ptHashMap == NULL || ptHashMap->_uBucketCapacity == 0)
        return PL_DS_HASH_INVALID;

	const uint32_t uMask = ptHashMap->_uBucketCapacity - 1; // assumes bucket count is power of 2
    uint32_t uBucketIndex = uKey & uMask;
    const uint32_t uOriginalBucketIndex = uBucketIndex; // to check for full wrap around

    // find where a value would be and handle collisions with linear probing
    while(ptHashMap->_auKeys[uBucketIndex] != uKey && ptHashMap->_auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
    {
        uBucketIndex = (uBucketIndex + 1) & uMask;

        // not find and we did a full wrap around
        if(uBucketIndex == uOriginalBucketIndex)
            return PL_DS_HASH_INVALID;     
    }

    if(puBucketIndexOut)
        *puBucketIndexOut = uBucketIndex;

    if(puValueOut)
        *puValueOut = ptHashMap->_auValueBucket[uBucketIndex];

    // exists, so return associated value
    return ptHashMap->_auValueBucket[uBucketIndex];
}

static inline uint32_t
pl__hm_lookup32(const plHashMap32* ptHashMap, uint64_t uKey, uint32_t* puBucketIndexOut, uint32_t* puValueOut)
{

    // early exit checks
    if(ptHashMap == NULL || ptHashMap->_uBucketCapacity == 0)
        return PL_DS_HASH32_INVALID;

	const uint32_t uMask = ptHashMap->_uBucketCapacity - 1; // assumes bucket count is power of 2
    uint32_t uBucketIndex = uKey & uMask;
    const uint32_t uOriginalBucketIndex = uBucketIndex; // to check for full wrap around

    // find where a value would be and handle collisions with linear probing
    while(ptHashMap->_auKeys[uBucketIndex] != uKey && ptHashMap->_auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
    {
        uBucketIndex = (uBucketIndex + 1) & uMask;

        // not find and we did a full wrap around
        if(uBucketIndex == uOriginalBucketIndex)
            return PL_DS_HASH32_INVALID;     
    }

    if(puBucketIndexOut)
        *puBucketIndexOut = uBucketIndex;

    if(puValueOut)
        *puValueOut = ptHashMap->_auValueBucket[uBucketIndex];

    // exists, so return associated value
    return ptHashMap->_auValueBucket[uBucketIndex];
}

static inline uint64_t
pl_hm64_lookup(const plHashMap64* ptHashMap, uint64_t uKey)
{
    return pl__hm_lookup64(ptHashMap, uKey, NULL, NULL);
}

static inline uint32_t
pl_hm32_lookup(const plHashMap32* ptHashMap, uint64_t uKey)
{
    return pl__hm_lookup32(ptHashMap, uKey, NULL, NULL);
}

static inline bool
pl_hm64_has_key(const plHashMap64* ptHashMap, uint64_t uKey)
{
    return pl_hm64_lookup(ptHashMap, uKey) != PL_DS_HASH_INVALID;
}

static inline bool
pl_hm32_has_key(const plHashMap32* ptHashMap, uint64_t uKey)
{
    return pl_hm32_lookup(ptHashMap, uKey) != PL_DS_HASH32_INVALID;
}

static inline bool
pl_hm64_has_key_ex(const plHashMap64* ptHashMap, uint64_t uKey, uint64_t* puValue)
{
    return pl__hm_lookup64(ptHashMap, uKey,  NULL, puValue) != PL_DS_HASH_INVALID;
}

static inline bool
pl_hm32_has_key_ex(const plHashMap32* ptHashMap, uint64_t uKey, uint32_t* puValue)
{
    return pl__hm_lookup32(ptHashMap, uKey,  NULL, puValue) != PL_DS_HASH32_INVALID;
}

static inline void
pl__hm_resize(plHashMap64* ptHashMap, uint32_t uBucketCount, const char* pcFile, int iLine)
{

    // store old info
    const uint32_t uOldBucketCount = ptHashMap->_uBucketCapacity;
    uint64_t* sbuOldBucket = ptHashMap->_auValueBucket;
    uint64_t* aulOldKeys = ptHashMap->_auKeys;

    // ensure our actual bucket count is a power of 2
    ptHashMap->_uBucketCapacity = uBucketCount < PL_DS_HASHMAP_INITIAL_SIZE ? PL_DS_HASHMAP_INITIAL_SIZE : (uint32_t)pl__ds_get_next_power_of_2(uBucketCount);
	
    // growing
    if(uBucketCount > 0)
    {
        
        ptHashMap->_auValueBucket = (uint64_t*)PL_DS_ALLOC_INDIRECT(sizeof(uint64_t) * ptHashMap->_uBucketCapacity, pcFile, iLine);
        ptHashMap->_auKeys  = (uint64_t*)PL_DS_ALLOC_INDIRECT(sizeof(uint64_t) * ptHashMap->_uBucketCapacity, pcFile, iLine);
        memset(ptHashMap->_auValueBucket, 0xff, sizeof(uint64_t) * ptHashMap->_uBucketCapacity);
        memset(ptHashMap->_auKeys, 0xff, sizeof(uint64_t) * ptHashMap->_uBucketCapacity);
    
        // move old data over
        for(uint32_t i = 0; i < uOldBucketCount; i++)
        {
            const uint64_t uKey = aulOldKeys[i];

            if(uKey < UINT64_MAX-1)
            {
                uint32_t uOldBucketIndex = pl__hm_get_existing_bucket_index(aulOldKeys, uOldBucketCount, uKey);
                const uint64_t uValue = sbuOldBucket[uOldBucketIndex];
                ptHashMap->_uItemCount--;
                pl__hm_insert(ptHashMap, uKey, uValue, pcFile, iLine);
            }
        }
    }
    else // freeing
    {
        ptHashMap->_auValueBucket = NULL;
        ptHashMap->_auKeys = NULL;
        pl_sb_free(ptHashMap->_sbuFreeIndices);
        ptHashMap->_uItemCount = 0;
        ptHashMap->_uBucketCapacity = 0;
    }

    if(sbuOldBucket)
    {
        PL_DS_FREE(sbuOldBucket);
    }
    if(aulOldKeys)
    {
        PL_DS_FREE(aulOldKeys);
    }
}

static inline void
pl__hm_resize32(plHashMap32* ptHashMap, uint32_t uBucketCount, const char* pcFile, int iLine)
{

    // store old info
    const uint32_t uOldBucketCount = ptHashMap->_uBucketCapacity;
    uint32_t* sbuOldBucket = ptHashMap->_auValueBucket;
    uint64_t* aulOldKeys = ptHashMap->_auKeys;

    // ensure our actual bucket count is a power of 2
    ptHashMap->_uBucketCapacity = uBucketCount < PL_DS_HASHMAP_INITIAL_SIZE ? PL_DS_HASHMAP_INITIAL_SIZE : (uint32_t)pl__ds_get_next_power_of_2(uBucketCount);
	
    // growing
    if(uBucketCount > 0)
    {
        
        ptHashMap->_auValueBucket = (uint32_t*)PL_DS_ALLOC_INDIRECT(sizeof(uint32_t) * ptHashMap->_uBucketCapacity, pcFile, iLine);
        ptHashMap->_auKeys  = (uint64_t*)PL_DS_ALLOC_INDIRECT(sizeof(uint64_t) * ptHashMap->_uBucketCapacity, pcFile, iLine);
        memset(ptHashMap->_auValueBucket, 0xff, sizeof(uint32_t) * ptHashMap->_uBucketCapacity);
        memset(ptHashMap->_auKeys, 0xff, sizeof(uint64_t) * ptHashMap->_uBucketCapacity);
    
        // move old data over
        for(uint32_t i = 0; i < uOldBucketCount; i++)
        {
            const uint64_t uKey = aulOldKeys[i];

            if(uKey < UINT64_MAX-1)
            {
                uint32_t uOldBucketIndex = pl__hm_get_existing_bucket_index(aulOldKeys, uOldBucketCount, uKey);
                const uint32_t uValue = sbuOldBucket[uOldBucketIndex];
                ptHashMap->_uItemCount--;
                pl__hm_insert32(ptHashMap, uKey, uValue, pcFile, iLine);
            }
        }
    }
    else // freeing
    {
        ptHashMap->_auValueBucket = NULL;
        ptHashMap->_auKeys = NULL;
        pl_sb_free(ptHashMap->_sbuFreeIndices);
        ptHashMap->_uItemCount = 0;
        ptHashMap->_uBucketCapacity = 0;
    }

    if(sbuOldBucket)
    {
        PL_DS_FREE(sbuOldBucket);
    }
    if(aulOldKeys)
    {
        PL_DS_FREE(aulOldKeys);
    }
}

static inline void
pl_hm64_free(plHashMap64* ptHashMap)
{
    pl__hm_resize(ptHashMap, 0, __FILE__, __LINE__);
}

static inline void
pl_hm32_free(plHashMap32* ptHashMap)
{
    pl__hm_resize32(ptHashMap, 0, __FILE__, __LINE__);
}

static inline void
pl__hm_insert(plHashMap64* ptHashMap, uint64_t uKey, uint64_t uValue, const char* pcFile, int iLine)
{

    if(ptHashMap->_uBucketCapacity == 0)
        pl__hm_resize(ptHashMap, PL_DS_HASHMAP_INITIAL_SIZE, pcFile, iLine);
    else if(((float)ptHashMap->_uItemCount / (float)ptHashMap->_uBucketCapacity) > 0.60f)
        pl__hm_resize(ptHashMap, ptHashMap->_uBucketCapacity * 2, pcFile, iLine);

    const uint32_t uMask = ptHashMap->_uBucketCapacity - 1; // assumes bucket count is power of 2
    uint32_t uBucketIndex = uKey & uMask;

    // see if key exists
    uint64_t uValueIndex = pl__hm_lookup64(ptHashMap, uKey, &uBucketIndex, NULL);

    if(uValueIndex == PL_DS_HASH_INVALID) // key doesn't exist
    {
        // handle collisions with linear probing
        while(ptHashMap->_auKeys[uBucketIndex] != uKey && ptHashMap->_auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
        {
            uBucketIndex = (uBucketIndex + 1) & uMask;

            // check if slot is empty
            if(ptHashMap->_auKeys[uBucketIndex] == UINT64_MAX-1)
                break;
        }

        ptHashMap->_auKeys[uBucketIndex] = uKey;
        ptHashMap->_auValueBucket[uBucketIndex] = uValue;
        ptHashMap->_uItemCount++;
    }
    else // key exists, so update value
        ptHashMap->_auValueBucket[uBucketIndex] = uValue;
}

static inline void
pl__hm_insert32(plHashMap32* ptHashMap, uint64_t uKey, uint32_t uValue, const char* pcFile, int iLine)
{

    if(ptHashMap->_uBucketCapacity == 0)
        pl__hm_resize32(ptHashMap, PL_DS_HASHMAP_INITIAL_SIZE, pcFile, iLine);
    else if(((float)ptHashMap->_uItemCount / (float)ptHashMap->_uBucketCapacity) > 0.60f)
        pl__hm_resize32(ptHashMap, ptHashMap->_uBucketCapacity * 2, pcFile, iLine);

    const uint32_t uMask = ptHashMap->_uBucketCapacity - 1; // assumes bucket count is power of 2
    uint32_t uBucketIndex = uKey & uMask;

    // see if key exists
    uint32_t uValueIndex = pl__hm_lookup32(ptHashMap, uKey, &uBucketIndex, NULL);

    if(uValueIndex == PL_DS_HASH32_INVALID) // key doesn't exist
    {
        // handle collisions with linear probing
        while(ptHashMap->_auKeys[uBucketIndex] != uKey && ptHashMap->_auKeys[uBucketIndex] != PL_DS_HASH_INVALID)
        {
            uBucketIndex = (uBucketIndex + 1) & uMask;

            // check if slot is empty
            if(ptHashMap->_auKeys[uBucketIndex] == UINT64_MAX-1)
                break;
        }

        ptHashMap->_auKeys[uBucketIndex] = uKey;
        ptHashMap->_auValueBucket[uBucketIndex] = uValue;
        ptHashMap->_uItemCount++;
    }
    else // key exists, so update value
        ptHashMap->_auValueBucket[uBucketIndex] = uValue;
}

static inline void
pl_hm64_remove(plHashMap64* ptHashMap, uint64_t uKey)
{

    uint32_t uBucketIndex = UINT32_MAX;
    uint64_t uValueIndex = pl__hm_lookup64(ptHashMap, uKey, &uBucketIndex, NULL);

    if(uValueIndex != PL_DS_HASH_INVALID)
    {
        const uint64_t uValue = ptHashMap->_auValueBucket[uBucketIndex];
        pl_sb_push(ptHashMap->_sbuFreeIndices, uValue);

        ptHashMap->_auValueBucket[uBucketIndex] = PL_DS_HASH_INVALID;
        ptHashMap->_auKeys[uBucketIndex] = UINT64_MAX-1;
        ptHashMap->_uItemCount--;
    }
}

static inline void
pl_hm32_remove(plHashMap32* ptHashMap, uint64_t uKey)
{

    uint32_t uBucketIndex = UINT32_MAX;
    uint32_t uValueIndex = pl__hm_lookup32(ptHashMap, uKey, &uBucketIndex, NULL);

    if(uValueIndex != PL_DS_HASH32_INVALID)
    {
        const uint32_t uValue = ptHashMap->_auValueBucket[uBucketIndex];
        pl_sb_push(ptHashMap->_sbuFreeIndices, uValue);

        ptHashMap->_auValueBucket[uBucketIndex] = PL_DS_HASH32_INVALID;
        ptHashMap->_auKeys[uBucketIndex] = UINT64_MAX-1;
        ptHashMap->_uItemCount--;
    }
}

static inline uint64_t
pl_hm_hash_str(const char* pcKey, uint64_t uSeed)
{
    uint64_t uCrc = uSeed;
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

static inline void
pl_hm64_remove_str(plHashMap64* ptHashMap, const char* pcKey)
{
    pl_hm64_remove(ptHashMap, pl_hm_hash_str(pcKey, 0));
}

static inline void
pl_hm32_remove_str(plHashMap32* ptHashMap, const char* pcKey)
{
    pl_hm32_remove(ptHashMap, pl_hm_hash_str(pcKey, 0));
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

static inline bool
pl_hm64_has_key_str(const plHashMap64* ptHashMap, const char* pcKey)
{
    return pl_hm64_lookup(ptHashMap, pl_hm_hash_str(pcKey, 0)) != PL_DS_HASH_INVALID;
}

static inline bool
pl_hm32_has_key_str(const plHashMap32* ptHashMap, const char* pcKey)
{
    return pl_hm32_lookup(ptHashMap, pl_hm_hash_str(pcKey, 0)) != PL_DS_HASH32_INVALID;
}

static inline bool
pl_hm64_has_key_str_ex(const plHashMap64* ptHashMap, const char* pcKey, uint64_t* puValue)
{
    return pl__hm_lookup64(ptHashMap, pl_hm_hash_str(pcKey, 0), NULL, puValue) != PL_DS_HASH_INVALID;
}

static inline bool
pl_hm32_has_key_str_ex(const plHashMap32* ptHashMap, const char* pcKey, uint32_t* puValue)
{
    return pl__hm_lookup32(ptHashMap, pl_hm_hash_str(pcKey, 0), NULL, puValue) != PL_DS_HASH32_INVALID;
}

static inline uint64_t
pl_hm64_lookup_str(const plHashMap64* ptHashMap, const char* pcKey)
{
    return pl__hm_lookup64(ptHashMap, pl_hm_hash_str(pcKey, 0), NULL, NULL);
}

static inline uint32_t
pl_hm32_lookup_str(const plHashMap32* ptHashMap, const char* pcKey)
{
    return pl__hm_lookup32(ptHashMap, pl_hm_hash_str(pcKey, 0), NULL, NULL);
}

#endif // PL_DS_H