/*
   pl_ds.h
     * data structures
*/

// library version
#define PL_DS_VERSION    "0.1.0"
#define PL_DS_VERSION_NUM 00100

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] public api
// [SECTION] internal
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DS_H
#define PL_DS_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h> // uint32_t
#include <stdlib.h> // malloc, free
#include <string.h> // memset, memmove

//-----------------------------------------------------------------------------
// [SECTION] public api
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
    if((buf)){ free(pl__sb_header(buf));} (buf) = NULL;

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

#define pl_sb_add_ptr(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), (n) ? (pl__sb_header(buf)->uSize += (n), &(buf)[pl__sb_header(buf)->uSize - (n)]) : (buf))

#define pl_sb_push(buf, v) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), 1, 8), (buf)[pl__sb_header((buf))->uSize++] = (v))

#define pl_sb_reserve(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n)))

#define pl_sb_resize(buf, n) \
    (pl__sb_may_grow((buf), sizeof(*(buf)), (n), (n)), memset((buf), 0, sizeof(*(buf)) * (n)), pl__sb_header((buf))->uSize = (n))

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

    plSbHeader_* ptNewHeader = malloc((ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plSbHeader_));
    if(ptNewHeader)
    {
        ptNewHeader->uSize = ptOldHeader->uSize;
        ptNewHeader->uCapacity = ptOldHeader->uCapacity + (uint32_t)szNewItems;
        memcpy(&ptNewHeader[1], *ptrBuffer, ptOldHeader->uSize * szElementSize);
        free(ptOldHeader);
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
        plSbHeader_* ptHeader = malloc(szMinCapacity * szElementSize + sizeof(plSbHeader_));
        if(ptHeader)
        {
            *ptrBuffer = &ptHeader[1]; 
            ptHeader->uSize = 0u;
            ptHeader->uCapacity = (uint32_t)szMinCapacity;
        }
    }     
}

#endif // PL_DS_H