
//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_EXT_INC
#define PL_EXT_INC

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

// apis
static const struct _plApiRegistryI*       gptApiRegistry       = NULL;
static const struct _plExtensionRegistryI* gptExtensionRegistry = NULL;
static const struct _plDataRegistryI*      gptDataRegistry      = NULL;
static const struct _plResourceI*          gptResource          = NULL;
static const struct _plEcsI*               gptECS               = NULL;
static const struct _plGraphicsI*          gptGfx               = NULL;
static const struct _plCameraI*            gptCamera            = NULL;
static const struct _plImageI*             gptImage             = NULL;
static const struct _plStatsI*             gptStats             = NULL;
static const struct _plGPUAllocatorsI*     gptGpuAllocators     = NULL;
static const struct _plJobI*               gptJob               = NULL;
static const struct _plDrawI*              gptDraw              = NULL;
static const struct _plUiI*                gptUI                = NULL;
static const struct _plIOI*                gptIOI               = NULL;
static const struct _plShaderI*            gptShader            = NULL;
static const struct _plThreadsI*           gptThreads           = NULL;
static const struct _plAtomicsI*           gptAtomics           = NULL;
static const struct _plRectPackI*          gptRect              = NULL;
static const struct _plFileI*              gptFile              = NULL;

// context
static struct _plIO* gptIO = NULL;

#endif // PL_EXT_INC