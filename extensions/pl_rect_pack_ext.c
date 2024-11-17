/*
   pl_rect_packer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_rect_pack_ext.h"
#include "stb_rect_pack.h"
#include <string.h>

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plRectPackContext
{
    stbrp_context tStbContext;
    int           iNodeCount;
    stbrp_node*   ptNodes;
} plRectPackContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plRectPackContext* gptRectPackCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void
pl_pack_rects(int iWidth, int iHeight, plPackRect* ptRects, uint32_t uRectCount)
{
    if(gptRectPackCtx->ptNodes == NULL)
    {
        gptRectPackCtx->iNodeCount = iWidth;
        gptRectPackCtx->ptNodes = PL_ALLOC(sizeof(stbrp_node) * iWidth);
    }
    else if(iWidth > gptRectPackCtx->iNodeCount)
    {
        PL_FREE(gptRectPackCtx->ptNodes);
        gptRectPackCtx->iNodeCount = iWidth;
        gptRectPackCtx->ptNodes = PL_ALLOC(sizeof(stbrp_node) * iWidth);
    }
    stbrp_init_target(&gptRectPackCtx->tStbContext, iWidth, iHeight,
        gptRectPackCtx->ptNodes, gptRectPackCtx->iNodeCount);
    stbrp_pack_rects(&gptRectPackCtx->tStbContext, (stbrp_rect*)ptRects, (int)uRectCount);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_rect_pack_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plRectPackI tApi = {
        .pack_rects = pl_pack_rects
    };
    pl_set_api(ptApiRegistry, plRectPackI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
        gptRectPackCtx = ptDataRegistry->get_data("plRectPackContext");
    else
    {
        static plRectPackContext gtRectPackCtx = {0};
        gptRectPackCtx = &gtRectPackCtx;
        ptDataRegistry->set_data("plRectPackContext", gptRectPackCtx);
    }
}

PL_EXPORT void
pl_unload_rect_pack_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plRectPackI* ptApi = pl_get_api_latest(ptApiRegistry, plRectPackI);
    ptApiRegistry->remove_api(ptApi);

    if(gptRectPackCtx->ptNodes)
        PL_FREE(gptRectPackCtx->ptNodes);
}

#ifndef PL_UNITY_BUILD

    #define STB_RECT_PACK_IMPLEMENTATION
    #include "stb_rect_pack.h"
    #undef STB_RECT_PACK_IMPLEMENTATION

#endif