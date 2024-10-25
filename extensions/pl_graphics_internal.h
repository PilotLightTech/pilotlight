#ifndef PL_GRAPHICS_INTERNAL_EXT_H
#define PL_GRAPHICS_INTERNAL_EXT_H

#include "pl_graphics_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

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

// new handles
static plBufferHandle           pl__get_new_buffer_handle(plDevice*);
static plTextureHandle          pl__get_new_texture_handle(plDevice*);
static plSamplerHandle          pl__get_new_sampler_handle(plDevice*);
static plBindGroupHandle        pl__get_new_bind_group_handle(plDevice*);
static plShaderHandle           pl__get_new_shader_handle(plDevice*);
static plComputeShaderHandle    pl__get_new_compute_shader_handle(plDevice*);
static plRenderPassHandle       pl__get_new_render_pass_handle(plDevice*);
static plRenderPassLayoutHandle pl__get_new_render_pass_layout_handle(plDevice*);
static plSemaphoreHandle        pl__get_new_semaphore_handle(plDevice*);
static plRenderEncoder*         pl__get_new_render_encoder(void);
static plComputeEncoder*        pl__get_new_compute_encoder(void);
static plBlitEncoder*           pl__get_new_blit_encoder(void);

static void pl__return_render_encoder(plRenderEncoder*);
static void pl__return_compute_encoder(plComputeEncoder*);
static void pl__return_blit_encoder(plBlitEncoder*);

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

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plDrawStreamBits
{
    PL_DRAW_STREAM_BIT_NONE             = 0,
    PL_DRAW_STREAM_BIT_SHADER           = 1 << 0,
    PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET   = 1 << 1,
    PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER   = 1 << 2,
    PL_DRAW_STREAM_BIT_BINDGROUP_2      = 1 << 3,
    PL_DRAW_STREAM_BIT_BINDGROUP_1      = 1 << 4,
    PL_DRAW_STREAM_BIT_BINDGROUP_0      = 1 << 5,
    PL_DRAW_STREAM_BIT_INDEX_OFFSET     = 1 << 6,
    PL_DRAW_STREAM_BIT_VERTEX_OFFSET    = 1 << 7,
    PL_DRAW_STREAM_BIT_INDEX_BUFFER     = 1 << 8,
    PL_DRAW_STREAM_BIT_VERTEX_BUFFER    = 1 << 9,
    PL_DRAW_STREAM_BIT_TRIANGLES        = 1 << 10,
    PL_DRAW_STREAM_BIT_INSTANCE_START   = 1 << 11,
    PL_DRAW_STREAM_BIT_INSTANCE_COUNT   = 1 << 12
};

#endif // PL_GRAPHICS_INTERNAL_EXT_H