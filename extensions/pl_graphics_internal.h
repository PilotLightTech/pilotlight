#ifndef PL_GRAPHICS_INTERNAL_EXT_H
#define PL_GRAPHICS_INTERNAL_EXT_H

#include "pl.h"
#include "pl_log_ext.h"
#include "pl_threads_ext.h"
#include "pl_graphics_ext.h"
#include "pl_window_ext.h"
#include "pl_profile_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

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

    static const plDataRegistryI* gptDataRegistry = NULL;
    static const plThreadsI* gptThreads = NULL;
    static const plProfileI* gptProfile = NULL;
    static const plLogI* gptLog = NULL;
    static const plIOI* gptIOI = NULL;

    static plIO* gptIO = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

typedef struct _plFrameGarbage
{
    plTextureHandle*          sbtTextures;
    plSamplerHandle*          sbtSamplers;
    plBufferHandle*           sbtBuffers;
    plBindGroupHandle*        sbtBindGroups;
    plShaderHandle*           sbtShaders;
    plComputeShaderHandle*    sbtComputeShaders;
    plRenderPassLayoutHandle* sbtRenderPassLayouts;
    plRenderPassHandle*       sbtRenderPasses;
    plDeviceMemoryAllocation* sbtMemory;
} plFrameGarbage;

typedef struct _plFrameContext plFrameContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static struct _plGraphics* gptGraphics = NULL;
static uint64_t uLogChannelGraphics = UINT64_MAX;

// getters (generational pool)
static plSampler*          pl_get_sampler           (plDevice*, plSamplerHandle);
static plRenderPassLayout* pl_get_render_pass_layout(plDevice*, plRenderPassLayoutHandle);
static plRenderPass*       pl_get_render_pass       (plDevice*, plRenderPassHandle);
static plBuffer*           pl__get_buffer           (plDevice*, plBufferHandle);
static plTexture*          pl__get_texture          (plDevice*, plTextureHandle);
static plBindGroup*        pl__get_bind_group       (plDevice*, plBindGroupHandle);
static plShader*           pl__get_shader           (plDevice*, plShaderHandle);
static plComputeShader*    pl__get_compute_shader   (plDevice*, plComputeShaderHandle);

static bool pl_is_buffer_valid            (plDevice*, plBufferHandle);
static bool pl_is_sampler_valid           (plDevice*, plSamplerHandle);
static bool pl_is_texture_valid           (plDevice*, plTextureHandle);
static bool pl_is_bind_group_valid        (plDevice*, plBindGroupHandle);
static bool pl_is_render_pass_valid       (plDevice*, plRenderPassHandle);
static bool pl_is_render_pass_layout_valid(plDevice*, plRenderPassLayoutHandle);
static bool pl_is_shader_valid            (plDevice*, plShaderHandle);
static bool pl_is_compute_shader_valid    (plDevice*, plComputeShaderHandle);

// new handles
static plBufferHandle           pl__get_new_buffer_handle(plDevice*);
static plTextureHandle          pl__get_new_texture_handle(plDevice*);
static plSamplerHandle          pl__get_new_sampler_handle(plDevice*);
static plBindGroupHandle        pl__get_new_bind_group_handle(plDevice*);
static plShaderHandle           pl__get_new_shader_handle(plDevice*);
static plComputeShaderHandle    pl__get_new_compute_shader_handle(plDevice*);
static plRenderPassHandle       pl__get_new_render_pass_handle(plDevice*);
static plRenderPassLayoutHandle pl__get_new_render_pass_layout_handle(plDevice*);
static plRenderEncoder*         pl__get_new_render_encoder(void);
static plComputeEncoder*        pl__get_new_compute_encoder(void);
static plBlitEncoder*           pl__get_new_blit_encoder(void);
static plTimelineSemaphore*     pl__get_new_semaphore(plDevice*);

static void pl__return_render_encoder(plRenderEncoder*);
static void pl__return_compute_encoder(plComputeEncoder*);
static void pl__return_blit_encoder(plBlitEncoder*);
static void pl__return_semaphore(plDevice*, plTimelineSemaphore*);

// deletion
static plFrameGarbage* pl__get_frame_garbage(plDevice*);
static plFrameContext* pl__get_frame_resources(plDevice*);

static void pl_queue_buffer_for_deletion(plDevice*, plBufferHandle);
static void pl_queue_texture_for_deletion(plDevice*, plTextureHandle);
static void pl_queue_render_pass_for_deletion(plDevice*, plRenderPassHandle);
static void pl_queue_render_pass_layout_for_deletion(plDevice*, plRenderPassLayoutHandle);
static void pl_queue_shader_for_deletion(plDevice*, plShaderHandle);
static void pl_queue_compute_shader_for_deletion(plDevice*, plComputeShaderHandle);
static void pl_queue_bind_group_for_deletion(plDevice*, plBindGroupHandle);
static void pl_queue_sampler_for_deletion(plDevice*, plSamplerHandle);

// helpers
static size_t   pl__get_data_type_size(plDataType);
static uint32_t pl__format_stride(plFormat);

// backends
static uint32_t pl_get_current_frame_index(void);
static size_t   pl_get_local_memory_in_use(void);
static size_t   pl_get_host_memory_in_use(void);

// drawstream
static void pl_drawstream_cleanup(plDrawStream*);
static void pl_drawstream_reset  (plDrawStream*);
static void pl_drawstream_draw   (plDrawStream*, plDrawStreamData);

// temp
static void pl__cleanup_common_device(plDevice*);
static void pl__cleanup_common_graphics(void);
static void pl__cleanup_common_swapchain(plSwapchain*);

// misc.
static void pl__garbage_collect(plDevice*);

#endif // PL_GRAPHICS_INTERNAL_EXT_H