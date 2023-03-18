/*
   pl_ds.h
     * data structures
*/

// library version
#define PL_DS_VERSION    "0.2.0"
#define PL_DS_VERSION_NUM 00200

/*
Index of this file:
// [SECTION] documentation
// [SECTION] header mess
// [SECTION] includes
// [SECTION] public api
// [SECTION] internal
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

COMPILE TIME OPTIONS

    * Change allocators by defining both:
        PL_LOG_ALLOC(x)
        PL_LOG_FREE(x)
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

    plSbHeader_* ptNewHeader = (plSbHeader_*)PL_DS_ALLOC((ptOldHeader->uCapacity + szNewItems) * szElementSize + sizeof(plSbHeader_));
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
        if(ptHeader)
        {
            *ptrBuffer = &ptHeader[1]; 
            ptHeader->uSize = 0u;
            ptHeader->uCapacity = (uint32_t)szMinCapacity;
        }
    }     
}

#endif // PL_DS_H