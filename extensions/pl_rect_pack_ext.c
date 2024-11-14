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
#include "pl_ext.inc"
#include <string.h>

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

static void
pl_load_rect_pack_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plRectPackI tApi = {
        .pack_rects = pl_pack_rects
    };
    pl_set_api(ptApiRegistry, plRectPackI, &tApi);

    if(bReload)
        gptRectPackCtx = gptDataRegistry->get_data("plRectPackContext");
    else
    {
        static plRectPackContext gtRectPackCtx = {0};
        gptRectPackCtx = &gtRectPackCtx;
        gptDataRegistry->set_data("plRectPackContext", gptRectPackCtx);
    }
}

static void
pl_unload_rect_pack_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plRectPackI* ptApi = pl_get_api_latest(ptApiRegistry, plRectPackI);
    ptApiRegistry->remove_api(ptApi);

    if(gptRectPackCtx->ptNodes)
        PL_FREE(gptRectPackCtx->ptNodes);
}
