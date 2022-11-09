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

/*
Index of this file:
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_REGISTRY_H
#define PL_REGISTRY_H

#ifndef PL_DECLARE_STRUCT
    #define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
PL_DECLARE_STRUCT(plDataRegistry);
PL_DECLARE_STRUCT(plDataEntry);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// registry
void            pl_initialize_data_registry(plDataRegistry* ptRegistry);
void            pl_cleanup_data_registry   (void);
void            pl_set_data_registry       (plDataRegistry* ptRegistry);
plDataRegistry* pl_get_data_registry       (void);

// entries
void            pl_register_data(const char* pcName, void* pData);
void*           pl_get_data     (const char* pcName);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDataEntry
{
    const char* pcName;
    void*       pData;
} plDataEntry;

typedef struct _plDataRegistry
{
    plDataEntry*  sbtDataItems;
} plDataRegistry;

#endif // PL_REGISTRY_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] includes
// [SECTION] global context
// [SECTION] public api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <string.h>  // strcmp
#include "pl_ds.h"

#ifdef PL_REGISTRY_IMPLEMENTATION

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

plDataRegistry* gptDataRegistry = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_data_registry(plDataRegistry* ptRegistry)
{
    gptDataRegistry = ptRegistry;
}

void
pl_cleanup_data_registry(void)
{
    PL_ASSERT(gptDataRegistry && "global data registry not set");
    pl_sb_free(gptDataRegistry->sbtDataItems);
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
    for(uint32_t i = 0; i < pl_sb_size(gptDataRegistry->sbtDataItems); i++)
    {
        if(strcmp(pcName, gptDataRegistry->sbtDataItems[i].pcName) == 0)
        {
            bDataExists = true;
            break;
        }
    }

    PL_ASSERT(!bDataExists && "global data registry already contains item");

    if(!bDataExists)
    {
        plDataEntry tEntry = {
            .pcName = pcName,
            .pData = pData
        };
        pl_sb_push(gptDataRegistry->sbtDataItems, tEntry);
    }
}

void*
pl_get_data(const char* pcName)
{
    PL_ASSERT(gptDataRegistry && "global data registry not set");

    for(uint32_t i = 0; i < pl_sb_size(gptDataRegistry->sbtDataItems); i++)
    {
        if(strcmp(pcName, gptDataRegistry->sbtDataItems[i].pcName) == 0)
            return gptDataRegistry->sbtDataItems[i].pData;
    }

    return NULL;
}

#endif // PL_REGISTRY_IMPLEMENTATION