/*
   pl_registry.h, v0.1 (WIP)
   * no dependencies
   * simple
   Do this:
        #define PL_REGISTRY_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_REGISTRY_IMPLEMENTATION
   #include "pl_registry.h"
*/

// library version
#define PL_REGISTRY_VERSION    "0.1.0"
#define PL_REGISTRY_VERSION_NUM 00100

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_REGISTRY_H
#define PL_REGISTRY_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plDataRegistry plDataRegistry;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// registry
plDataRegistry* pl_create_data_registry (void);
void            pl_cleanup_data_registry(void);
void            pl_set_data_registry    (plDataRegistry* ptRegistry);
plDataRegistry* pl_get_data_registry    (void);

// entries
void            pl_register_data(const char* pcName, void* pData);
void*           pl_get_data     (const char* pcName);

#endif // PL_REGISTRY_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global context
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <string.h>  // strcmp

#ifdef PL_REGISTRY_IMPLEMENTATION

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

#ifndef PL_REG_ALLOC
    #include <stdlib.h>
    #define PL_REG_ALLOC(x) malloc((x))
    #define PL_REG_FREE(x)  free((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plDataEntry
{
    const char* pcName;
    void*       pData;
} plDataEntry;

typedef struct _plDataRegistry
{
    bool          bOverflowInUse;
    plDataEntry   atDataItems[64];
    plDataEntry*  ptDataItems;
    uint32_t      uEntryCount;
    uint32_t      uEntryCapacity;
    uint32_t      uOverflowEntryCapacity;
} plDataRegistry;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

plDataRegistry* gptDataRegistry = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plDataEntry* pl__get_new_entry(void);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plDataRegistry*
pl_create_data_registry(void)
{
    plDataRegistry* ptRegistry = (plDataRegistry*)PL_REG_ALLOC(sizeof(plDataRegistry));
    memset(ptRegistry, 0, sizeof(plDataRegistry));
    gptDataRegistry = ptRegistry;
    if(gptDataRegistry)
    {
        gptDataRegistry->ptDataItems = gptDataRegistry->atDataItems;
        gptDataRegistry->uEntryCapacity = 64;
    }
    return gptDataRegistry;
}

void
pl_cleanup_data_registry(void)
{
    PL_ASSERT(gptDataRegistry && "global data registry not set");
    if(gptDataRegistry->bOverflowInUse)
        PL_REG_FREE(gptDataRegistry->ptDataItems);
}

void
pl_set_data_registry(plDataRegistry* ptRegistry)
{
    gptDataRegistry = ptRegistry;
}

plDataRegistry*
pl_get_data_registry(void)
{
    PL_ASSERT(gptDataRegistry && "global data registry not set");
    return gptDataRegistry;
}

void
pl_register_data(const char* pcName, void* pData)
{
    PL_ASSERT(gptDataRegistry && "global data registry not set");

    bool bDataExists = false;
    for(uint32_t i = 0; i < gptDataRegistry->uEntryCount; i++)
    {
        if(strcmp(pcName, gptDataRegistry->ptDataItems[i].pcName) == 0)
        {
            bDataExists = true;
            break;
        }
    }

    PL_ASSERT(!bDataExists && "global data registry already contains item");

    if(!bDataExists)
    {
        plDataEntry* ptEntry = pl__get_new_entry();
        ptEntry->pcName = pcName;
        ptEntry->pData = pData;
    }
}

void*
pl_get_data(const char* pcName)
{
    PL_ASSERT(gptDataRegistry && "global data registry not set");

    for(uint32_t i = 0; i < gptDataRegistry->uEntryCount; i++)
    {
        if(strcmp(pcName, gptDataRegistry->ptDataItems[i].pcName) == 0)
            return gptDataRegistry->ptDataItems[i].pData;
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plDataEntry*
pl__get_new_entry(void)
{
    plDataEntry* ptEntry = NULL;

    // check if new overflow
    if(!gptDataRegistry->bOverflowInUse && gptDataRegistry->uEntryCount == gptDataRegistry->uEntryCapacity)
    {
        gptDataRegistry->ptDataItems = (plDataEntry*)PL_REG_ALLOC(sizeof(plDataEntry) * 256);
        memset(gptDataRegistry->ptDataItems, 0, sizeof(plDataEntry) * 256);
        gptDataRegistry->uOverflowEntryCapacity = 256;

        // copy stack samples
        memcpy(gptDataRegistry->ptDataItems, gptDataRegistry->atDataItems, sizeof(plDataEntry) * gptDataRegistry->uEntryCapacity);
        gptDataRegistry->bOverflowInUse = true;
    }
    // check if overflow reallocation is needed
    else if(gptDataRegistry->bOverflowInUse && gptDataRegistry->uEntryCount == gptDataRegistry->uOverflowEntryCapacity)
    {
        plDataEntry* sbtOldInputEvents = gptDataRegistry->ptDataItems;
        gptDataRegistry->ptDataItems = (plDataEntry*)PL_REG_ALLOC(sizeof(plDataEntry) * gptDataRegistry->uOverflowEntryCapacity * 2);
        memset(gptDataRegistry->ptDataItems, 0, sizeof(plDataEntry) * gptDataRegistry->uOverflowEntryCapacity * 2);
        
        // copy old values
        memcpy(gptDataRegistry->ptDataItems, sbtOldInputEvents, sizeof(plDataEntry) * gptDataRegistry->uOverflowEntryCapacity);
        gptDataRegistry->uOverflowEntryCapacity *= 2;

        PL_REG_FREE(sbtOldInputEvents);
    }

    ptEntry = &gptDataRegistry->ptDataItems[gptDataRegistry->uEntryCount];
    gptDataRegistry->uEntryCount++;

    return ptEntry;
}

#endif // PL_REGISTRY_IMPLEMENTATION