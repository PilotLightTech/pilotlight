
//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_EXT_INC
#define PL_UNITY_EXT_INC

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
static const struct _plStringInternI*      gptString            = 0;
static const struct _plProfileI*           gptProfile           = 0;
static const struct _plLogI*               gptLog               = 0;
static const struct _plConsoleI*           gptConsole           = 0;
static const struct _plScreenLogI*         gptScreenLog         = 0;
static const struct _plStarterI*           gptStarter           = 0;
static const struct _plToolsI*             gptTools             = 0;

// experimental
static const struct _plConfigI*        gptConfig        = 0;
static const struct _plCollisionI*     gptCollision     = 0;
static const struct _plBVHI*           gptBvh           = 0;
static const struct _plCameraI*        gptCamera        = 0;
static const struct _plResourceI*      gptResource      = 0;
static const struct _plEcsI*           gptECS           = 0;
static const struct _plRendererI*      gptRenderer      = 0;
static const struct _plEcsToolsI*      gptEcsTools      = 0;
static const struct _plGizmoI*         gptGizmo         = 0;
static const struct _plPhysicsI*       gptPhysics       = 0;
static const struct _plAnimationI*     gptAnimation     = 0;
static const struct _plMeshI*          gptMesh          = 0;
static const struct _plShaderVariantI* gptShaderVariant = 0;

// context
static struct _plIO* gptIO = 0;

// helpers
#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_MEMORY_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_MEMORY_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#ifndef PL_DS_ALLOC
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#endif // PL_UNITY_EXT_INC