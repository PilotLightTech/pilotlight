#ifndef PL_GRAPHICS_INTERNAL_EXT_H
#define PL_GRAPHICS_INTERNAL_EXT_H

#include "pl.h"
#include "pl_log_ext.h"
#include "pl_platform_ext.h" // threads
#include "pl_graphics_ext.h"
#include "pl_profile_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

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
static const plWindowI* gptWindows = NULL;

static plIO* gptIO = NULL;

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
    plBindGroupLayoutHandle*  sbtBindGroupLayouts;
    plShaderHandle*           sbtShaders;
    plComputeShaderHandle*    sbtComputeShaders;
    plRenderPassLayoutHandle* sbtRenderPassLayouts;
    plRenderPassHandle*       sbtRenderPasses;
    plDeviceMemoryAllocation* sbtMemory;
} plFrameGarbage;

typedef struct _plStackedBarrier
{
    plPipelineStageFlags tSrcStages;
    plPipelineStageFlags tDstStages;
    plBarrierScope tScope;
} plStackedBarrier;

typedef struct _plFrameContext plFrameContext;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context
bool              pl_graphics_initialize(const plGraphicsInit*);
void              pl_graphics_cleanup   (void);
plGraphicsBackend pl_graphics_get_backend(void);
const char*       pl_graphics_get_backend_string(void);

// devices
void                pl_graphics_enumerate_devices(plDeviceInfo*, uint32_t* deviceCountOut);
plDevice*           pl_graphics_create_device    (const plDeviceInit*);
void                pl_graphics_cleanup_device   (plDevice*);
void                pl_graphics_flush_device     (plDevice*);
const plDeviceInfo* pl_graphics_get_device_info  (plDevice*);

// surface
plSurface* pl_graphics_create_surface (plWindow*);
void       pl_graphics_cleanup_surface(plSurface*);

// swapchain
plSwapchain*     pl_graphics_create_swapchain       (plDevice*, plSurface*, const plSwapchainInit*);
void             pl_graphics_cleanup_swapchain      (plSwapchain*);
bool             pl_graphics_acquire_swapchain_image(plSwapchain*);
void             pl_graphics_recreate_swapchain     (plSwapchain*, const plSwapchainInit*);
plTextureHandle* pl_graphics_get_swapchain_images   (plSwapchain*, uint32_t* puSizeOut);
plSwapchainInfo  pl_graphics_get_swapchain_info     (plSwapchain*);
uint32_t         pl_graphics_get_current_swapchain_image_index(plSwapchain*);

// query
uint32_t pl_graphics_get_frames_in_flight   (void);
uint32_t pl_graphics_get_current_frame_index(void);
size_t   pl_graphics_get_host_memory_in_use (void);
size_t   pl_graphics_get_local_memory_in_use(void);

// per frame
void pl_graphics_begin_frame(plDevice*);
bool pl_graphics_present    (plCommandBuffer*, const plSubmitInfo*, plSwapchain**, uint32_t uSwapchainCount);

// timeline semaphore ops
plTimelineSemaphore* pl_graphics_create_semaphore   (plDevice*, bool hostVisible);
void                 pl_graphics_cleanup_semaphore  (plTimelineSemaphore*);
void                 pl_graphics_signal_semaphore   (plDevice*, plTimelineSemaphore*, uint64_t);
void                 pl_graphics_wait_semaphore     (plDevice*, plTimelineSemaphore*, uint64_t);
uint64_t             pl_graphics_get_semaphore_value(plDevice*, plTimelineSemaphore*);

// command pools & buffers
plCommandPool*   pl_graphics_create_command_pool    (plDevice*, const plCommandPoolDesc*);
void             pl_graphics_cleanup_command_pool   (plCommandPool*);
void             pl_graphics_reset_command_pool     (plCommandPool*, plCommandPoolResetFlags); // call at beginning of frame
plCommandBuffer* pl_graphics_request_command_buffer (plCommandPool*, const char* debugName);   // retrieve command buffer from the pool
void             pl_graphics_return_command_buffer  (plCommandBuffer*); // return command buffer to pool
void             pl_graphics_reset_command_buffer   (plCommandBuffer*); // call if reusing after submit/present
void             pl_graphics_wait_on_command_buffer (plCommandBuffer*); // call after submit to block/wait
void             pl_graphics_begin_command_recording(plCommandBuffer*);
void             pl_graphics_end_command_recording  (plCommandBuffer*);
void             pl_graphics_submit_command_buffer  (plCommandBuffer*, const plSubmitInfo*);

// render encoder
void pl_graphics_begin_render_pass(plCommandBuffer*, const plRenderInfo*, const plPassResources*);
void pl_graphics_end_render_pass  (plCommandBuffer*);

// render encoder: draw stream (preferred system)
//   Notes:
//     - call reset_draw_stream(...) with the maximum number of possible calls to
//       pl_add_to_draw_stream before the next call to draw_stream so any memory
//       allocations can happen before a hot loop where pl_add_to_draw_stream is called.
void pl_graphics_reset_draw_stream  (plDrawStream*, uint32_t drawCount);
void pl_graphics_cleanup_draw_stream(plDrawStream*);
void pl_graphics_draw_stream        (plCommandBuffer*, uint32_t areaCount, plDrawArea*); // decodes drawstream (does not reset draw stream)
// INLINED -> void pl_add_to_draw_stream(plDrawStream*, plDrawStreamData);

// render encoder: direct (prefer draw stream system, this will be used for bindless mostly)
void pl_graphics_bind_graphics_bind_groups(plCommandBuffer*, plShaderHandle, uint32_t first, uint32_t count, const plBindGroupHandle*, uint32_t dynamicCount, const plDynamicBinding*);
void pl_graphics_set_depth_bias           (plCommandBuffer*, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);
void pl_graphics_set_viewport             (plCommandBuffer*, const plRenderViewport*);
void pl_graphics_set_scissor_region       (plCommandBuffer*, const plScissor*);
void pl_graphics_bind_vertex_buffer       (plCommandBuffer*, plBufferHandle);
void pl_graphics_bind_vertex_buffers      (plCommandBuffer*, uint32_t first, uint32_t count, const plBufferHandle*, const size_t* offsets); // offsets can be NULL or array of "count" length
void pl_graphics_draw                     (plCommandBuffer*, uint32_t count, const plDraw*);
void pl_graphics_draw_indexed             (plCommandBuffer*, uint32_t count, const plDrawIndex*);
void pl_graphics_bind_shader              (plCommandBuffer*, plShaderHandle);

// compute encoder
void pl_graphics_begin_compute_pass      (plCommandBuffer*, const plPassResources*); // do not store
void pl_graphics_end_compute_pass        (plCommandBuffer*);
void pl_graphics_dispatch                (plCommandBuffer*, uint32_t dispatchCount, const plDispatch*);
void pl_graphics_bind_compute_shader     (plCommandBuffer*, plComputeShaderHandle);
void pl_graphics_bind_compute_bind_groups(plCommandBuffer*, plComputeShaderHandle, uint32_t first, uint32_t count, const plBindGroupHandle*, uint32_t dynamicCount, const plDynamicBinding*);
void pl_graphics_copy_buffer_to_texture  (plCommandBuffer*, plBufferHandle, plTextureHandle, uint32_t regionCount, const plBufferImageCopy*);
void pl_graphics_copy_texture_to_buffer  (plCommandBuffer*, plTextureHandle, plBufferHandle, uint32_t regionCount, const plBufferImageCopy*);
void pl_graphics_copy_texture            (plCommandBuffer*, plTextureHandle src, plTextureHandle dst, uint32_t regionCount, const plImageCopy*);
void pl_graphics_generate_mipmaps        (plCommandBuffer*, plTextureHandle);
void pl_graphics_copy_buffer             (plCommandBuffer*, plBufferHandle source, plBufferHandle destination, uint64_t sourceOffset, uint64_t destinationOffset, size_t);

// barriers
void pl_graphics_intra_pass_barrier(plCommandBuffer*, plPipelineStageFlags srcStages, plPipelineStageFlags dstStages, plBarrierScope, const plPassResources*);
void pl_graphics_consumer_barrier  (plCommandBuffer*, plPipelineStageFlags srcStages, plPipelineStageFlags dstStages, plBarrierScope);
void pl_graphics_producer_barrier  (plCommandBuffer*, plPipelineStageFlags srcStages, plPipelineStageFlags dstStages, plBarrierScope);

//-----------------------------------------------------------------------------

// buffers
plBufferHandle pl_graphics_create_buffer            (plDevice*, const plBufferDesc*, plBuffer**);
void           pl_graphics_bind_buffer_to_memory    (plDevice*, plBufferHandle, const plDeviceMemoryAllocation*);
void           pl_graphics_queue_buffer_for_deletion(plDevice*, plBufferHandle);
void           pl_graphics_destroy_buffer           (plDevice*, plBufferHandle);
plBuffer*      pl_graphics_get_buffer               (plDevice*, plBufferHandle); // do not store
bool           pl_graphics_is_buffer_valid          (plDevice*, plBufferHandle);

// samplers
plSamplerHandle pl_graphics_create_sampler            (plDevice*, const plSamplerDesc*);
void            pl_graphics_destroy_sampler           (plDevice*, plSamplerHandle);
void            pl_graphics_queue_sampler_for_deletion(plDevice*, plSamplerHandle);
plSampler*      pl_graphics_get_sampler               (plDevice*, plSamplerHandle); // do not store
bool            pl_graphics_is_sampler_valid          (plDevice*, plSamplerHandle);

// textures
plTextureHandle pl_graphics_create_texture            (plDevice*, const plTextureDesc*, plTexture**);
plTextureHandle pl_graphics_create_texture_view       (plDevice*, const plTextureViewDesc*);
void            pl_graphics_bind_texture_to_memory    (plDevice*, plTextureHandle, const plDeviceMemoryAllocation*);
void            pl_graphics_queue_texture_for_deletion(plDevice*, plTextureHandle);
void            pl_graphics_destroy_texture           (plDevice*, plTextureHandle);
plTexture*      pl_graphics_get_texture               (plDevice*, plTextureHandle); // do not store
bool            pl_graphics_is_texture_valid          (plDevice*, plTextureHandle);

// bind groups
plBindGroupPool*  pl_graphics_create_bind_group_pool       (plDevice*, const plBindGroupPoolDesc*);
void              pl_graphics_cleanup_bind_group_pool      (plBindGroupPool*);
void              pl_graphics_reset_bind_group_pool        (plBindGroupPool*);
plBindGroupHandle pl_graphics_create_bind_group            (plDevice*, const plBindGroupDesc*);
void              pl_graphics_update_bind_group            (plDevice*, plBindGroupHandle, const plBindGroupUpdateData*);
void              pl_graphics_queue_bind_group_for_deletion(plDevice*, plBindGroupHandle);
void              pl_graphics_destroy_bind_group           (plDevice*, plBindGroupHandle);
plBindGroup*      pl_graphics_get_bind_group               (plDevice*, plBindGroupHandle); // do not store
bool              pl_graphics_is_bind_group_valid          (plDevice*, plBindGroupHandle);

// bind group layouts
plBindGroupLayoutHandle pl_graphics_create_bind_group_layout            (plDevice*, const plBindGroupLayoutDesc*);
void                    pl_graphics_destroy_bind_group_layout           (plDevice*, plBindGroupLayoutHandle);
void                    pl_graphics_queue_bind_group_layout_for_deletion(plDevice*, plBindGroupLayoutHandle);
plBindGroupLayout*      pl_graphics_get_bind_group_layout               (plDevice*, plBindGroupLayoutHandle); // do not store
bool                    pl_graphics_is_bind_group_layout_valid          (plDevice*, plBindGroupLayoutHandle);

// pixel & vertex shaders
plShaderHandle pl_graphics_create_shader            (plDevice*, const plShaderDesc*);
void           pl_graphics_queue_shader_for_deletion(plDevice*, plShaderHandle);
void           pl_graphics_destroy_shader           (plDevice*, plShaderHandle);
plShader*      pl_graphics_get_shader               (plDevice*, plShaderHandle); // do not store
bool           pl_graphics_is_shader_valid          (plDevice*, plShaderHandle);

// compute shaders
plComputeShaderHandle pl_graphics_create_compute_shader            (plDevice*, const plComputeShaderDesc*);
void                  pl_graphics_queue_compute_shader_for_deletion(plDevice*, plComputeShaderHandle);
void                  pl_graphics_destroy_compute_shader           (plDevice*, plComputeShaderHandle);
bool                  pl_graphics_is_compute_shader_valid          (plDevice*, plComputeShaderHandle);
plComputeShader*      pl_graphics_get_compute_shader               (plDevice*, plComputeShaderHandle); // do not store

// dynamic data system
//   Notes:
//     - call "allocate_dynamic_data_block" once at the beginning of the frame
//       then use "pl_allocate_dynamic_data" throughout the frame (it will allocate new
//       blocks as needed)
//     - do not store the plDynamicDataBlock returned from "allocate_dynamic_data_block"
//       longer than a single frame (they are reset every frame)
plDynamicDataBlock pl_graphics_allocate_dynamic_data_block(plDevice*); // do not store longer than a single frame (this ar)
void               pl_graphics_reset_dynamic_data_blocks(plDevice*);
// INLINED -> plDynamicBinding pl_allocate_dynamic_data(const plGraphicsI*,  plDevice*, plDynamicDataBlock*, uint32_t size)

// memory (only guaranteed 64 allocations, so suballocate)
//   Notes:
//     - if allocation fails, the "ulSize" member will be zero
plDeviceMemoryAllocation        pl_graphics_allocate_memory  (plDevice*, size_t, plMemoryFlags, uint32_t typeFilter, const char* debugName);
void                            pl_graphics_free_memory      (plDevice*, plDeviceMemoryAllocation*);
bool                            pl_graphics_flush_memory     (plDevice*, uint32_t rangeCount, const plDeviceMemoryRange*); // call after writing from host on non-coherent memory
bool                            pl_graphics_invalidate_memory(plDevice*, uint32_t rangeCount, const plDeviceMemoryRange*); // call after writing from device on non-coherent memory
const plDeviceMemoryAllocation* pl_graphics_get_allocations(plDevice*, uint32_t* sizeOut);

//------------------------------DEBUGGING--------------------------------------

void pl_graphics_push_debug_group  (plCommandBuffer*, const char*, plVec4 color);
void pl_graphics_pop_debug_group   (plCommandBuffer*);
void pl_graphics_insert_debug_label(plCommandBuffer*, const char*, plVec4 color); // vulkan only

//---------------------------------MISC----------------------------------------

size_t       pl_graphics_get_data_type_size (plDataType);
plBlendState pl_graphics_get_blend_state    (plBlendMode);
uint32_t     pl_graphics_calculate_mip_count(uint32_t width, uint32_t height);
const char*  pl_graphics_get_format_as_string(plFormat);

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static struct _plGraphics* gptGraphics = NULL;
static uint64_t uLogChannelGraphics = UINT64_MAX;

// // getters (generational pool)
// static plSampler*          pl_graphics_get_sampler           (plDevice*, plSamplerHandle);
// static plRenderPassLayout* pl_graphics_get_render_pass_layout(plDevice*, plRenderPassLayoutHandle);
// static plRenderPass*       pl_graphics_get_render_pass       (plDevice*, plRenderPassHandle);
// static plBuffer*           pl_graphics_get_buffer           (plDevice*, plBufferHandle);
// static plTexture*          pl_graphics_get_texture          (plDevice*, plTextureHandle);
// static plBindGroup*        pl_graphics_get_bind_group       (plDevice*, plBindGroupHandle);
// static plBindGroupLayout*  pl_graphics_get_bind_group_layout(plDevice*, plBindGroupLayoutHandle);
// static plShader*           pl_graphics_get_shader           (plDevice*, plShaderHandle);
// static plComputeShader*    pl_graphics_get_compute_shader   (plDevice*, plComputeShaderHandle);

// static bool pl_graphics_is_buffer_valid            (plDevice*, plBufferHandle);
// static bool pl_graphics_is_sampler_valid           (plDevice*, plSamplerHandle);
// static bool pl_graphics_is_texture_valid           (plDevice*, plTextureHandle);
// static bool pl_graphics_is_bind_group_valid        (plDevice*, plBindGroupHandle);
// static bool pl_graphics_is_render_pass_valid       (plDevice*, plRenderPassHandle);
// static bool pl_graphics_is_render_pass_layout_valid(plDevice*, plRenderPassLayoutHandle);
// static bool pl_graphics_is_shader_valid            (plDevice*, plShaderHandle);
// static bool pl_graphics_is_compute_shader_valid    (plDevice*, plComputeShaderHandle);

// new handles
static plBufferHandle           pl__get_new_buffer_handle(plDevice*);
static plTextureHandle          pl__get_new_texture_handle(plDevice*);
static plSamplerHandle          pl__get_new_sampler_handle(plDevice*);
static plBindGroupHandle        pl__get_new_bind_group_handle(plDevice*);
static plBindGroupLayoutHandle  pl__get_new_bind_group_layout_handle(plDevice*);
static plShaderHandle           pl__get_new_shader_handle(plDevice*);
static plComputeShaderHandle    pl__get_new_compute_shader_handle(plDevice*);
static plRenderPassHandle       pl__get_new_render_pass_handle(plDevice*);
static plRenderPassLayoutHandle pl__get_new_render_pass_layout_handle(plDevice*);
static plTimelineSemaphore*     pl__get_new_semaphore(plDevice*);

static void pl__return_semaphore(plDevice*, plTimelineSemaphore*);

// deletion
static plFrameGarbage* pl__get_frame_garbage(plDevice*);
static plFrameContext* pl__get_frame_resources(plDevice*);

// helpers
static uint32_t pl__format_stride(plFormat);
static size_t   pl__get_vertex_attribute_size(plVertexFormat);

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