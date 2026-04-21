/*
   pl_freelist_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_FREELIST_EXT_H
#define PL_FREELIST_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plFreeListI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h> // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plFreeList     plFreeList;
typedef struct _plFreeListNode plFreeListNode;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_freelist_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_freelist_ext(plApiRegistryI*, bool reload);

PL_API void            pl_freelist_create (uint64_t size, uint64_t minSize, plFreeList* freelistOut);
PL_API void            pl_freelist_cleanup(plFreeList* freelistOut);

PL_API plFreeListNode* pl_freelist_get_node   (plFreeList*, uint64_t size);
PL_API void            pl_freelist_return_node(plFreeList*, plFreeListNode*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plFreeListI
{
    void            (*create)     (uint64_t size, uint64_t minSize, plFreeList* freelistOut);
    void            (*cleanup)    (plFreeList* freelistOut);
    plFreeListNode* (*get_node)   (plFreeList*, uint64_t size);
    void            (*return_node)(plFreeList*, plFreeListNode*);
} plFreeListI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plFreeListNode
{

    uint64_t uOffset;
    uint64_t uSize;
    
    // [INTERNAL]
    uint64_t        _uIndex;
    plFreeListNode* _ptNext;
    plFreeListNode* _ptPrev;
} plFreeListNode;

typedef struct _plFreeList
{
    uint64_t uUsedSpace;
    uint64_t uSize;

    // [INTERNAL]
    uint64_t        _uMinNodeSize;
    uint64_t*       _sbuFreeNodeHoleSlot;
    plFreeListNode* _atNodeHoles;
    plFreeListNode  _tFreeList;
} plFreeList;

#ifdef __cplusplus
}
#endif

#endif // PL_FREELIST_EXT_H