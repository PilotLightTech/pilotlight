
//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_EXT_INC
#define PL_EXT_INC

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

// apis
static const struct _plApiRegistryI*       gptApiRegistry       = 0;
static const struct _plExtensionRegistryI* gptExtensionRegistry = 0;
static const struct _plDataRegistryI*      gptDataRegistry      = 0;
static const struct _plGraphicsI*          gptGfx               = 0;
static const struct _plImageI*             gptImage             = 0;
static const struct _plStatsI*             gptStats             = 0;
static const struct _plGPUAllocatorsI*     gptGpuAllocators     = 0;
static const struct _plJobI*               gptJob               = 0;
static const struct _plDrawI*              gptDraw              = 0;
static const struct _plDrawBackendI*       gptDrawBackend       = 0;
static const struct _plUiI*                gptUI                = 0;
static const struct _plIOI*                gptIOI               = 0;
static const struct _plShaderI*            gptShader            = 0;
static const struct _plThreadsI*           gptThreads           = 0;
static const struct _plAtomicsI*           gptAtomics           = 0;
static const struct _plRectPackI*          gptRect              = 0;
static const struct _plFileI*              gptFile              = 0;
static const struct _plMemoryI*            gptMemory            = 0;

// experimental
static const struct _plCameraI*   gptCamera   = 0;
static const struct _plResourceI* gptResource = 0;
static const struct _plEcsI*      gptECS      = 0;

// context
static struct _plIO* gptIO = 0;

// helpers
#define PL_ALLOC(x)      gptMemory->realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->realloc((x), 0, __FILE__, __LINE__)

#ifndef PL_DS_ALLOC
    #define PL_DS_ALLOC(x)                      gptMemory->realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->realloc((x), 0, __FILE__, __LINE__)
#endif

#endif // PL_EXT_INC