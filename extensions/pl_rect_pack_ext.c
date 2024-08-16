/*
   pl_rect_packer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api implementation
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_rect_pack_ext.h"
#include "stb_rect_pack.h"

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

static plRectPackContext gtCtx = {0};

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void
pl_pack_rects(int iWidth, int iHeight, plPackRect* ptRects, uint32_t uRectCount)
{
    if(gtCtx.ptNodes == NULL)
    {
        gtCtx.iNodeCount = iWidth;
        gtCtx.ptNodes = PL_ALLOC(sizeof(stbrp_node) * iWidth);
    }
    else if(iWidth > gtCtx.iNodeCount)
    {
        PL_FREE(gtCtx.ptNodes);
        gtCtx.iNodeCount = iWidth;
        gtCtx.ptNodes = PL_ALLOC(sizeof(stbrp_node) * iWidth);
    }
    stbrp_init_target(&gtCtx.tStbContext, iWidth, iHeight, gtCtx.ptNodes, gtCtx.iNodeCount);
    stbrp_pack_rects(&gtCtx.tStbContext, (stbrp_rect*)ptRects, (int)uRectCount);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static const plRectPackI*
pl_load_rect_pack_api(void)
{
    static const plRectPackI tApi = {
        .pack_rects = pl_pack_rects
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_RECT_PACK), pl_load_rect_pack_api());
    else
        ptApiRegistry->add(PL_API_RECT_PACK, pl_load_rect_pack_api());
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
    if(gtCtx.ptNodes)
        PL_FREE(gtCtx.ptNodes);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION
