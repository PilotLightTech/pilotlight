/*
   pl_vulkan.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] shaders
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pl_vulkan.h"
#include "pl_ds.h"

#ifndef PL_VULKAN
#include <assert.h>
#define PL_VULKAN(x) PL_ASSERT(x == VK_SUCCESS)
#endif

//-----------------------------------------------------------------------------
// [SECTION] shaders
//-----------------------------------------------------------------------------

/*
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;

void main()
{
    Out.Color = aColor;
    Out.UV = aUV;
    gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
}
*/
static uint32_t __glsl_shader_vert_spv[] =
{
    0x07230203,0x00010000,0x00080001,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x000a000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x0000000f,0x00000015,
    0x0000001b,0x0000001c,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
    0x00000000,0x00030005,0x00000009,0x00000000,0x00050006,0x00000009,0x00000000,0x6f6c6f43,
    0x00000072,0x00040006,0x00000009,0x00000001,0x00005655,0x00030005,0x0000000b,0x0074754f,
    0x00040005,0x0000000f,0x6c6f4361,0x0000726f,0x00030005,0x00000015,0x00565561,0x00060005,
    0x00000019,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000019,0x00000000,
    0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x0000001b,0x00000000,0x00040005,0x0000001c,
    0x736f5061,0x00000000,0x00060005,0x0000001e,0x73755075,0x6e6f4368,0x6e617473,0x00000074,
    0x00050006,0x0000001e,0x00000000,0x61635375,0x0000656c,0x00060006,0x0000001e,0x00000001,
    0x61725475,0x616c736e,0x00006574,0x00030005,0x00000020,0x00006370,0x00040047,0x0000000b,
    0x0000001e,0x00000000,0x00040047,0x0000000f,0x0000001e,0x00000002,0x00040047,0x00000015,
    0x0000001e,0x00000001,0x00050048,0x00000019,0x00000000,0x0000000b,0x00000000,0x00030047,
    0x00000019,0x00000002,0x00040047,0x0000001c,0x0000001e,0x00000000,0x00050048,0x0000001e,
    0x00000000,0x00000023,0x00000000,0x00050048,0x0000001e,0x00000001,0x00000023,0x00000008,
    0x00030047,0x0000001e,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
    0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040017,
    0x00000008,0x00000006,0x00000002,0x0004001e,0x00000009,0x00000007,0x00000008,0x00040020,
    0x0000000a,0x00000003,0x00000009,0x0004003b,0x0000000a,0x0000000b,0x00000003,0x00040015,
    0x0000000c,0x00000020,0x00000001,0x0004002b,0x0000000c,0x0000000d,0x00000000,0x00040020,
    0x0000000e,0x00000001,0x00000007,0x0004003b,0x0000000e,0x0000000f,0x00000001,0x00040020,
    0x00000011,0x00000003,0x00000007,0x0004002b,0x0000000c,0x00000013,0x00000001,0x00040020,
    0x00000014,0x00000001,0x00000008,0x0004003b,0x00000014,0x00000015,0x00000001,0x00040020,
    0x00000017,0x00000003,0x00000008,0x0003001e,0x00000019,0x00000007,0x00040020,0x0000001a,
    0x00000003,0x00000019,0x0004003b,0x0000001a,0x0000001b,0x00000003,0x0004003b,0x00000014,
    0x0000001c,0x00000001,0x0004001e,0x0000001e,0x00000008,0x00000008,0x00040020,0x0000001f,
    0x00000009,0x0000001e,0x0004003b,0x0000001f,0x00000020,0x00000009,0x00040020,0x00000021,
    0x00000009,0x00000008,0x0004002b,0x00000006,0x00000028,0x00000000,0x0004002b,0x00000006,
    0x00000029,0x3f800000,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
    0x00000005,0x0004003d,0x00000007,0x00000010,0x0000000f,0x00050041,0x00000011,0x00000012,
    0x0000000b,0x0000000d,0x0003003e,0x00000012,0x00000010,0x0004003d,0x00000008,0x00000016,
    0x00000015,0x00050041,0x00000017,0x00000018,0x0000000b,0x00000013,0x0003003e,0x00000018,
    0x00000016,0x0004003d,0x00000008,0x0000001d,0x0000001c,0x00050041,0x00000021,0x00000022,
    0x00000020,0x0000000d,0x0004003d,0x00000008,0x00000023,0x00000022,0x00050085,0x00000008,
    0x00000024,0x0000001d,0x00000023,0x00050041,0x00000021,0x00000025,0x00000020,0x00000013,
    0x0004003d,0x00000008,0x00000026,0x00000025,0x00050081,0x00000008,0x00000027,0x00000024,
    0x00000026,0x00050051,0x00000006,0x0000002a,0x00000027,0x00000000,0x00050051,0x00000006,
    0x0000002b,0x00000027,0x00000001,0x00070050,0x00000007,0x0000002c,0x0000002a,0x0000002b,
    0x00000028,0x00000029,0x00050041,0x00000011,0x0000002d,0x0000001b,0x0000000d,0x0003003e,
    0x0000002d,0x0000002c,0x000100fd,0x00010038
};

/*
#version 450 core
layout(location = 0) out vec4 fColor;
layout(set=0, binding=0) uniform sampler2D sTexture;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
void main()
{
    fColor = In.Color * texture(sTexture, In.UV.st);
}
*/
static uint32_t __glsl_shader_frag_spv[] =
{
    0x07230203,0x00010000,0x00080001,0x0000001e,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x00030010,
    0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
    0x00000000,0x00040005,0x00000009,0x6c6f4366,0x0000726f,0x00030005,0x0000000b,0x00000000,
    0x00050006,0x0000000b,0x00000000,0x6f6c6f43,0x00000072,0x00040006,0x0000000b,0x00000001,
    0x00005655,0x00030005,0x0000000d,0x00006e49,0x00050005,0x00000016,0x78655473,0x65727574,
    0x00000000,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000d,0x0000001e,
    0x00000000,0x00040047,0x00000016,0x00000022,0x00000000,0x00040047,0x00000016,0x00000021,
    0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
    0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,
    0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,
    0x00000002,0x0004001e,0x0000000b,0x00000007,0x0000000a,0x00040020,0x0000000c,0x00000001,
    0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000001,0x00040015,0x0000000e,0x00000020,
    0x00000001,0x0004002b,0x0000000e,0x0000000f,0x00000000,0x00040020,0x00000010,0x00000001,
    0x00000007,0x00090019,0x00000013,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,
    0x00000001,0x00000000,0x0003001b,0x00000014,0x00000013,0x00040020,0x00000015,0x00000000,
    0x00000014,0x0004003b,0x00000015,0x00000016,0x00000000,0x0004002b,0x0000000e,0x00000018,
    0x00000001,0x00040020,0x00000019,0x00000001,0x0000000a,0x00050036,0x00000002,0x00000004,
    0x00000000,0x00000003,0x000200f8,0x00000005,0x00050041,0x00000010,0x00000011,0x0000000d,
    0x0000000f,0x0004003d,0x00000007,0x00000012,0x00000011,0x0004003d,0x00000014,0x00000017,
    0x00000016,0x00050041,0x00000019,0x0000001a,0x0000000d,0x00000018,0x0004003d,0x0000000a,
    0x0000001b,0x0000001a,0x00050057,0x00000007,0x0000001c,0x00000017,0x0000001b,0x00050085,
    0x00000007,0x0000001d,0x00000012,0x0000001c,0x0003003e,0x00000009,0x0000001d,0x000100fd,
    0x00010038
};

/*
#version 450
layout(set = 0, binding = 0) uniform sampler2D samplerColor;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
layout (location = 0) out vec4 fOutFragColorVec;
void main() 
{
    float fDistance = texture(samplerColor, In.UV).a;
    float fSmoothWidth = fwidth(fDistance);	
    float fAlpha = smoothstep(0.5 - fSmoothWidth, 0.5 + fSmoothWidth, fDistance);
    vec3 fRgbVec = In.Color.rgb * texture(samplerColor, In.UV.st).rgb;
    fOutFragColorVec = vec4(fRgbVec, fAlpha);	
}
*/
static uint32_t __glsl_shader_fragsdf_spv[] =
{
	0x07230203,0x00010000,0x0008000a,0x0000003d,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000012,0x00000036,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00050005,0x00000008,0x73694466,0x636e6174,0x00000065,0x00060005,0x0000000c,
	0x706d6173,0x4372656c,0x726f6c6f,0x00000000,0x00030005,0x00000010,0x00000000,0x00050006,
	0x00000010,0x00000000,0x6f6c6f43,0x00000072,0x00040006,0x00000010,0x00000001,0x00005655,
	0x00030005,0x00000012,0x00006e49,0x00060005,0x0000001c,0x6f6d5366,0x5768746f,0x68746469,
	0x00000000,0x00040005,0x0000001f,0x706c4166,0x00006168,0x00040005,0x00000029,0x62675266,
	0x00636556,0x00070005,0x00000036,0x74754f66,0x67617246,0x6f6c6f43,0x63655672,0x00000000,
	0x00040047,0x0000000c,0x00000022,0x00000000,0x00040047,0x0000000c,0x00000021,0x00000000,
	0x00040047,0x00000012,0x0000001e,0x00000000,0x00040047,0x00000036,0x0000001e,0x00000000,
	0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
	0x00040020,0x00000007,0x00000007,0x00000006,0x00090019,0x00000009,0x00000006,0x00000001,
	0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,0x0000000a,0x00000009,
	0x00040020,0x0000000b,0x00000000,0x0000000a,0x0004003b,0x0000000b,0x0000000c,0x00000000,
	0x00040017,0x0000000e,0x00000006,0x00000004,0x00040017,0x0000000f,0x00000006,0x00000002,
	0x0004001e,0x00000010,0x0000000e,0x0000000f,0x00040020,0x00000011,0x00000001,0x00000010,
	0x0004003b,0x00000011,0x00000012,0x00000001,0x00040015,0x00000013,0x00000020,0x00000001,
	0x0004002b,0x00000013,0x00000014,0x00000001,0x00040020,0x00000015,0x00000001,0x0000000f,
	0x00040015,0x00000019,0x00000020,0x00000000,0x0004002b,0x00000019,0x0000001a,0x00000003,
	0x0004002b,0x00000006,0x00000020,0x3f000000,0x00040017,0x00000027,0x00000006,0x00000003,
	0x00040020,0x00000028,0x00000007,0x00000027,0x0004002b,0x00000013,0x0000002a,0x00000000,
	0x00040020,0x0000002b,0x00000001,0x0000000e,0x00040020,0x00000035,0x00000003,0x0000000e,
	0x0004003b,0x00000035,0x00000036,0x00000003,0x00050036,0x00000002,0x00000004,0x00000000,
	0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000007,0x00000008,0x00000007,0x0004003b,
	0x00000007,0x0000001c,0x00000007,0x0004003b,0x00000007,0x0000001f,0x00000007,0x0004003b,
	0x00000028,0x00000029,0x00000007,0x0004003d,0x0000000a,0x0000000d,0x0000000c,0x00050041,
	0x00000015,0x00000016,0x00000012,0x00000014,0x0004003d,0x0000000f,0x00000017,0x00000016,
	0x00050057,0x0000000e,0x00000018,0x0000000d,0x00000017,0x00050051,0x00000006,0x0000001b,
	0x00000018,0x00000003,0x0003003e,0x00000008,0x0000001b,0x0004003d,0x00000006,0x0000001d,
	0x00000008,0x000400d1,0x00000006,0x0000001e,0x0000001d,0x0003003e,0x0000001c,0x0000001e,
	0x0004003d,0x00000006,0x00000021,0x0000001c,0x00050083,0x00000006,0x00000022,0x00000020,
	0x00000021,0x0004003d,0x00000006,0x00000023,0x0000001c,0x00050081,0x00000006,0x00000024,
	0x00000020,0x00000023,0x0004003d,0x00000006,0x00000025,0x00000008,0x0008000c,0x00000006,
	0x00000026,0x00000001,0x00000031,0x00000022,0x00000024,0x00000025,0x0003003e,0x0000001f,
	0x00000026,0x00050041,0x0000002b,0x0000002c,0x00000012,0x0000002a,0x0004003d,0x0000000e,
	0x0000002d,0x0000002c,0x0008004f,0x00000027,0x0000002e,0x0000002d,0x0000002d,0x00000000,
	0x00000001,0x00000002,0x0004003d,0x0000000a,0x0000002f,0x0000000c,0x00050041,0x00000015,
	0x00000030,0x00000012,0x00000014,0x0004003d,0x0000000f,0x00000031,0x00000030,0x00050057,
	0x0000000e,0x00000032,0x0000002f,0x00000031,0x0008004f,0x00000027,0x00000033,0x00000032,
	0x00000032,0x00000000,0x00000001,0x00000002,0x00050085,0x00000027,0x00000034,0x0000002e,
	0x00000033,0x0003003e,0x00000029,0x00000034,0x0004003d,0x00000027,0x00000037,0x00000029,
	0x0004003d,0x00000006,0x00000038,0x0000001f,0x00050051,0x00000006,0x00000039,0x00000037,
	0x00000000,0x00050051,0x00000006,0x0000003a,0x00000037,0x00000001,0x00050051,0x00000006,
	0x0000003b,0x00000037,0x00000002,0x00070050,0x0000000e,0x0000003c,0x00000039,0x0000003a,
	0x0000003b,0x00000038,0x0003003e,0x00000036,0x0000003c,0x000100fd,0x00010038
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// wraps a buffer to be freed
typedef struct _plBufferReturn
{
    VkBuffer       tBuffer;
    VkDeviceMemory tDeviceMemory;
    int64_t        slFreedFrame;
} plBufferReturn;

// wraps a texture to be freed
typedef struct _plTextureReturn
{
    VkImage        tImage;
    VkImageView    tView;
    VkDeviceMemory tDeviceMemory;
    int64_t        slFreedFrame;
} plTextureReturn;

typedef struct _plVulkanPipelineEntry
{    
    VkRenderPass tRenderPass;
    VkPipeline   tRegularPipeline;
    VkPipeline   tSDFPipeline;
} plVulkanPipelineEntry;

typedef struct _plVulkanBufferInfo
{
    // vertex buffer
    VkBuffer        tVertexBuffer;
    VkDeviceMemory  tVertexMemory;
    unsigned char*  ucVertexBufferMap;
    uint32_t        uVertexByteSize;
    uint32_t        uVertexBufferOffset;

    // index buffer
    VkBuffer       tIndexBuffer;
    VkDeviceMemory tIndexMemory;
    unsigned char* ucIndexBufferMap;
    uint32_t       uIndexByteSize;
    uint32_t       uIndexBufferOffset;
} plVulkanBufferInfo;

typedef struct _plVulkanDrawContext
{
    VkDevice                         tDevice;
    VkCommandPool                    tCmdPool;
    VkQueue                          tGraphicsQueue;
    uint32_t                         uImageCount;
    VkPhysicalDeviceMemoryProperties tMemProps;
    VkDescriptorSetLayout            tDescriptorSetLayout;
    VkDescriptorPool                 tDescriptorPool;

    // fonts (temp until we have a proper atlas)
    VkSampler                        tFontSampler;
    VkImage                          tFontTextureImage;
    VkImageView                      tFontTextureImageView;
    VkDeviceMemory                   tFontTextureMemory;

    // committed buffers
    plBufferReturn*                  sbReturnedBuffers;
    plBufferReturn*                  sbReturnedBuffersTemp;
    uint32_t                         uBufferDeletionQueueSize;

    // committed textures
    plTextureReturn*                 sbReturnedTextures;
    plTextureReturn*                 sbReturnedTexturesTemp;
    uint32_t                         uTextureDeletionQueueSize;

    // vertex & index buffer
    plVulkanBufferInfo*              sbtBufferInfo;

    // staging buffer
    size_t                            szStageByteSize;
    VkBuffer                          tStagingBuffer;
    VkDeviceMemory                    tStagingMemory;
    void*                             pStageMapping; // persistent mapping for staging buffer

    // drawlist pipeline caching
    VkPipelineLayout                  tPipelineLayout;
    VkPipelineShaderStageCreateInfo   tPxlShdrStgInfo;
    VkPipelineShaderStageCreateInfo   tSdfShdrStgInfo;
    VkPipelineShaderStageCreateInfo   tVtxShdrStgInfo;

    // pipelines
    plVulkanPipelineEntry*            sbtPipelines;
    VkRenderPass                      tRenderPass; // default render pass
    VkSampleCountFlagBits             tMSAASampleCount;

} plVulkanDrawContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

extern void                   pl__cleanup_font_atlas       (plFontAtlas* ptAtlas); // in pl_draw.c
extern void                   pl__new_draw_frame           (plDrawContext* ptCtx); // in pl_draw.c
static uint32_t               pl__find_memory_type         (VkPhysicalDeviceMemoryProperties tMemProps, uint32_t typeFilter, VkMemoryPropertyFlags properties);
static void                   pl__grow_vulkan_vertex_buffer(plDrawContext* ptCtx, uint32_t uVtxBufSzNeeded, uint32_t uFrameIndex);
static void                   pl__grow_vulkan_index_buffer (plDrawContext* ptCtx, uint32_t uIdxBufSzNeeded, uint32_t uFrameIndex);
static plVulkanPipelineEntry* pl__get_pipelines            (plVulkanDrawContext* ptCtx, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount);


//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_draw_context_vulkan(plDrawContext* ptCtx, const plVulkanInit* ptInit)
{
    plVulkanDrawContext* ptVulkanDrawContext = PL_ALLOC(sizeof(plVulkanDrawContext));
    memset(ptVulkanDrawContext, 0, sizeof(plVulkanDrawContext));
    ptVulkanDrawContext->tDevice = ptInit->tLogicalDevice;
    ptVulkanDrawContext->uImageCount = ptInit->uImageCount;
    ptVulkanDrawContext->tRenderPass = ptInit->tRenderPass;
    ptVulkanDrawContext->tMSAASampleCount = ptInit->tMSAASampleCount;
    ptCtx->_platformData = ptVulkanDrawContext;

    // get physical tDevice properties
    vkGetPhysicalDeviceMemoryProperties(ptInit->tPhysicalDevice, &ptVulkanDrawContext->tMemProps);

    // create descriptor pool
    // TODO: add a system to allow this to grow as needed
    const VkDescriptorPoolSize atPoolSizes = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 };
    const VkDescriptorPoolCreateInfo tPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 1000u,
        .poolSizeCount = 1u,
        .pPoolSizes    = &atPoolSizes
    };
    PL_VULKAN(vkCreateDescriptorPool(ptInit->tLogicalDevice, &tPoolInfo, NULL, &ptVulkanDrawContext->tDescriptorPool));

    // find queue that supports the graphics bit
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ptInit->tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties aQueueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(ptInit->tPhysicalDevice, &uQueueFamCnt, aQueueFamilies);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (aQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            vkGetDeviceQueue(ptInit->tLogicalDevice, i, 0, &ptVulkanDrawContext->tGraphicsQueue);
            const VkCommandPoolCreateInfo tCommandPoolInfo = {
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .queueFamilyIndex = i,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            };
            PL_VULKAN(vkCreateCommandPool(ptInit->tLogicalDevice, &tCommandPoolInfo, NULL, &ptVulkanDrawContext->tCmdPool));
            break;
        }
    }

    // create font sampler
    const VkSamplerCreateInfo tSamplerInfo = {
        .sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter     = VK_FILTER_LINEAR,
        .minFilter     = VK_FILTER_LINEAR,
        .mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod        = -1000,
        .maxLod        = 1000,
        .maxAnisotropy = 1.0f
    };
    PL_VULKAN(vkCreateSampler(ptVulkanDrawContext->tDevice, &tSamplerInfo, NULL, &ptVulkanDrawContext->tFontSampler));

    // create descriptor set layout
    const VkDescriptorSetLayoutBinding tDescriptorSetLayoutBinding = {
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = &ptVulkanDrawContext->tFontSampler,
    };

    const VkDescriptorSetLayoutCreateInfo tDescriptorSetLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &tDescriptorSetLayoutBinding,
    };

    PL_VULKAN(vkCreateDescriptorSetLayout(ptVulkanDrawContext->tDevice, &tDescriptorSetLayoutInfo, NULL, &ptVulkanDrawContext->tDescriptorSetLayout));

    // create pipeline layout
    const VkPushConstantRange tPushConstant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset    = 0,
        .size      = sizeof(float) * 4
    };

    const VkPipelineLayoutCreateInfo tPipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &ptVulkanDrawContext->tDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &tPushConstant,
    };

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDrawContext->tDevice, &tPipelineLayoutInfo, NULL, &ptVulkanDrawContext->tPipelineLayout));

    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------

    ptVulkanDrawContext->tVtxShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanDrawContext->tVtxShdrStgInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    ptVulkanDrawContext->tVtxShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo tVtxShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_vert_spv),
        .pCode    = __glsl_shader_vert_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDrawContext->tDevice, &tVtxShdrInfo, NULL, &ptVulkanDrawContext->tVtxShdrStgInfo.module) == VK_SUCCESS);

    //---------------------------------------------------------------------
    // fragment shader stage
    //---------------------------------------------------------------------
    ptVulkanDrawContext->tPxlShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanDrawContext->tPxlShdrStgInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ptVulkanDrawContext->tPxlShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo tPxlShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_frag_spv),
        .pCode    = __glsl_shader_frag_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDrawContext->tDevice, &tPxlShdrInfo, NULL, &ptVulkanDrawContext->tPxlShdrStgInfo.module) == VK_SUCCESS);

    //---------------------------------------------------------------------
    // sdf fragment shader stage
    //---------------------------------------------------------------------
    ptVulkanDrawContext->tSdfShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanDrawContext->tSdfShdrStgInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ptVulkanDrawContext->tSdfShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo tSdfShdrInfo  = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_fragsdf_spv),
        .pCode    = __glsl_shader_fragsdf_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDrawContext->tDevice, &tSdfShdrInfo, NULL, &ptVulkanDrawContext->tSdfShdrStgInfo.module) == VK_SUCCESS);

    pl_sb_resize(ptVulkanDrawContext->sbtBufferInfo, PL_MAX_FRAMES_IN_FLIGHT);
}

void
pl_new_draw_frame(plDrawContext* ptCtx)
{
    plVulkanDrawContext* ptVulkanDrawContext = ptCtx->_platformData;

    //-----------------------------------------------------------------------------
    // buffer deletion queue
    //-----------------------------------------------------------------------------
    pl_sb_reset(ptVulkanDrawContext->sbReturnedBuffersTemp);
    if(ptVulkanDrawContext->uBufferDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawContext->sbReturnedBuffers); i++)
        {
            if(ptVulkanDrawContext->sbReturnedBuffers[i].slFreedFrame < (int64_t)ptCtx->frameCount)
            {
                ptVulkanDrawContext->uBufferDeletionQueueSize--;
                vkDestroyBuffer(ptVulkanDrawContext->tDevice, ptVulkanDrawContext->sbReturnedBuffers[i].tBuffer, NULL);
                vkFreeMemory(ptVulkanDrawContext->tDevice, ptVulkanDrawContext->sbReturnedBuffers[i].tDeviceMemory, NULL);
            }
            else
            {
                pl_sb_push(ptVulkanDrawContext->sbReturnedBuffersTemp, ptVulkanDrawContext->sbReturnedBuffers[i]);
            }
        }     
    }

    pl_sb_reset(ptVulkanDrawContext->sbReturnedBuffers);
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawContext->sbReturnedBuffersTemp); i++)
        pl_sb_push(ptVulkanDrawContext->sbReturnedBuffers, ptVulkanDrawContext->sbReturnedBuffersTemp[i]);

    //-----------------------------------------------------------------------------
    // texture deletion queue
    //-----------------------------------------------------------------------------
    pl_sb_reset(ptVulkanDrawContext->sbReturnedTexturesTemp);
    if(ptVulkanDrawContext->uTextureDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawContext->sbReturnedTextures); i++)
        {
            if(ptVulkanDrawContext->sbReturnedTextures[i].slFreedFrame < (int64_t)ptCtx->frameCount)
            {
                ptVulkanDrawContext->uTextureDeletionQueueSize--;
                vkDestroyImage(ptVulkanDrawContext->tDevice, ptVulkanDrawContext->sbReturnedTextures[i].tImage, NULL);
                vkDestroyImageView(ptVulkanDrawContext->tDevice, ptVulkanDrawContext->sbReturnedTextures[i].tView, NULL);
                vkFreeMemory(ptVulkanDrawContext->tDevice, ptVulkanDrawContext->sbReturnedTextures[i].tDeviceMemory, NULL);
            }
            else
            {
                pl_sb_push(ptVulkanDrawContext->sbReturnedTexturesTemp, ptVulkanDrawContext->sbReturnedTextures[i]);
            }
        }     
    }

    pl_sb_reset(ptVulkanDrawContext->sbReturnedTextures);
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawContext->sbReturnedTexturesTemp); i++)
        pl_sb_push(ptVulkanDrawContext->sbReturnedTextures, ptVulkanDrawContext->sbReturnedTexturesTemp[i]);

    // reset buffer offsets
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawContext->sbtBufferInfo); i++)
    {
        ptVulkanDrawContext->sbtBufferInfo[i].uVertexBufferOffset = 0;
        ptVulkanDrawContext->sbtBufferInfo[i].uIndexBufferOffset = 0;
    }

    pl__new_draw_frame(ptCtx);
}

void
pl_cleanup_font_atlas(plFontAtlas* ptAtlas)
{
    plVulkanDrawContext* ptVulkanDrawContext = ptAtlas->ctx->_platformData;
    const plTextureReturn tReturnTexture = {
        .tImage        = ptVulkanDrawContext->tFontTextureImage,
        .tView         = ptVulkanDrawContext->tFontTextureImageView,
        .tDeviceMemory = ptVulkanDrawContext->tFontTextureMemory,
        .slFreedFrame = (int64_t)(ptAtlas->ctx->frameCount + ptVulkanDrawContext->uImageCount * 2)
    };
    pl_sb_push(ptVulkanDrawContext->sbReturnedTextures, tReturnTexture);
    ptVulkanDrawContext->uTextureDeletionQueueSize++;
    pl__cleanup_font_atlas(ptAtlas);
}

void
pl_submit_drawlist_vulkan(plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex)
{
    plDrawContext* ptDrawContext = ptDrawlist->ctx;
    plVulkanDrawContext* ptVulkanDrawCtx = ptDrawContext->_platformData;
    pl_submit_drawlist_vulkan_ex(ptDrawlist, fWidth, fHeight, tCmdBuf, uFrameIndex, ptVulkanDrawCtx->tRenderPass, ptVulkanDrawCtx->tMSAASampleCount);
}

void
pl_submit_drawlist_vulkan_ex(plDrawList* ptDrawlist, float fWidth, float fHeight, VkCommandBuffer tCmdBuf, uint32_t uFrameIndex, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount)
{
    
    if(pl_sb_size(ptDrawlist->sbVertexBuffer) == 0u)
        return;

    plDrawContext* ptCtx = ptDrawlist->ctx;
    plVulkanDrawContext* ptVulkanDrawCtx = ptCtx->_platformData;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // ensure gpu vertex buffer size is adequate
    const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex) * pl_sb_size(ptDrawlist->sbVertexBuffer);
    if(uVtxBufSzNeeded == 0)
        return;

    plVulkanBufferInfo* tBufferInfo = &ptVulkanDrawCtx->sbtBufferInfo[uFrameIndex];

    // space left in vertex buffer
    const uint32_t uAvailableVertexBufferSpace = tBufferInfo->uVertexByteSize - tBufferInfo->uVertexBufferOffset;

    // grow buffer if not enough room
    if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
        pl__grow_vulkan_vertex_buffer(ptCtx, uVtxBufSzNeeded * 2, uFrameIndex);

    // vertex GPU data transfer
    unsigned char* pucMappedVertexBufferLocation = tBufferInfo->ucVertexBufferMap;
    memcpy(&pucMappedVertexBufferLocation[tBufferInfo->uVertexBufferOffset], ptDrawlist->sbVertexBuffer, sizeof(plDrawVertex) * pl_sb_size(ptDrawlist->sbVertexBuffer)); //-V1004

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // ensure gpu index buffer size is adequate
    const uint32_t uIdxBufSzNeeded = ptDrawlist->indexBufferByteSize;
    if(uIdxBufSzNeeded == 0)
        return;

    // space left in index buffer
    const uint32_t uAvailableIndexBufferSpace = tBufferInfo->uIndexByteSize - tBufferInfo->uIndexBufferOffset;

    if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
        pl__grow_vulkan_index_buffer(ptCtx, uIdxBufSzNeeded * 2, uFrameIndex);

    unsigned char* pucMappedIndexBufferLocation = tBufferInfo->ucIndexBufferMap;
    unsigned char* pucDestination = &pucMappedIndexBufferLocation[tBufferInfo->uIndexBufferOffset];
    
    // index GPU data transfer
    uint32_t uTempIndexBufferOffset = 0u;
    uint32_t globalIdxBufferIndexOffset = 0u;

    for(uint32_t i = 0u; i < pl_sb_size(ptDrawlist->sbSubmittedLayers); i++)
    {
        plDrawCommand* ptLastCommand = NULL;
        plDrawLayer* ptLayer = ptDrawlist->sbSubmittedLayers[i];

        memcpy(&pucDestination[uTempIndexBufferOffset], ptLayer->sbIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptLayer->sbIndexBuffer)); //-V1004

        uTempIndexBufferOffset += pl_sb_size(ptLayer->sbIndexBuffer)*sizeof(uint32_t);

        // attempt to merge commands
        for(uint32_t j = 0u; j < pl_sb_size(ptLayer->sbCommandBuffer); j++)
        {
            plDrawCommand* ptLayerCommand = &ptLayer->sbCommandBuffer[j];
            bool bCreateNewCommand = true;

            if(ptLastCommand)
            {
                // check for same texture (allows merging draw calls)
                if(ptLastCommand->textureId == ptLayerCommand->textureId && ptLastCommand->sdf == ptLayerCommand->sdf)
                {
                    ptLastCommand->elementCount += ptLayerCommand->elementCount;
                    bCreateNewCommand = false;
                }

                // check for same clipping (allows merging draw calls)
                if(ptLayerCommand->tClip.tMax.x != ptLastCommand->tClip.tMax.x || ptLayerCommand->tClip.tMax.y != ptLastCommand->tClip.tMax.y ||
                    ptLayerCommand->tClip.tMin.x != ptLastCommand->tClip.tMin.x || ptLayerCommand->tClip.tMin.y != ptLastCommand->tClip.tMin.y)
                {
                    bCreateNewCommand = true;
                }
                
            }

            if(bCreateNewCommand)
            {
                ptLayerCommand->indexOffset = globalIdxBufferIndexOffset + ptLayerCommand->indexOffset;
                pl_sb_push(ptDrawlist->sbDrawCommands, *ptLayerCommand);       
                ptLastCommand = ptLayerCommand;
            }
            
        }    
        globalIdxBufferIndexOffset += pl_sb_size(ptLayer->sbIndexBuffer);    
    }

    const VkMappedMemoryRange aRange[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = tBufferInfo->tVertexMemory,
            .size = VK_WHOLE_SIZE
        },
        {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = tBufferInfo->tIndexMemory,
            .size = VK_WHOLE_SIZE
        }
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDrawCtx->tDevice, 2, aRange));

    static const VkDeviceSize tOffsets = { 0u };
    vkCmdBindIndexBuffer(tCmdBuf, tBufferInfo->tIndexBuffer, 0u, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(tCmdBuf, 0, 1, &tBufferInfo->tVertexBuffer, &tOffsets);

    const int32_t iVertexOffset = tBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex);
    const int32_t iIndexOffset = tBufferInfo->uIndexBufferOffset / sizeof(uint32_t);

    plVulkanPipelineEntry* tPipelineEntry = pl__get_pipelines(ptVulkanDrawCtx, tRenderPass, tMSAASampleCount);

    const float fScale[] = { 2.0f / fWidth, 2.0f / fHeight};
    const float fTranslate[] = {-1.0f, -1.0f};
    bool bSdf = false;
    vkCmdBindPipeline(tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tPipelineEntry->tRegularPipeline); 
    for(uint32_t i = 0u; i < pl_sb_size(ptDrawlist->sbDrawCommands); i++)
    {
        plDrawCommand cmd = ptDrawlist->sbDrawCommands[i];

        if(cmd.sdf && !bSdf)
        {
            vkCmdBindPipeline(tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tPipelineEntry->tSDFPipeline); 
            bSdf = true;
        }
        else if(!cmd.sdf && bSdf)
        {
            vkCmdBindPipeline(tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tPipelineEntry->tRegularPipeline); 
            bSdf = false;
        }

        if(pl_rect_width(&cmd.tClip) == 0)
        {
            const VkRect2D tScissor = {
                .extent.width = (int32_t) fWidth,
                .extent.height = (int32_t) fHeight
            };
            vkCmdSetScissor(tCmdBuf, 0, 1, &tScissor);
        }
        else
        {

            // clamp to viewport
            if (cmd.tClip.tMin.x < 0.0f)   { cmd.tClip.tMin.x = 0.0f; }
            if (cmd.tClip.tMin.y < 0.0f)   { cmd.tClip.tMin.y = 0.0f; }
            if (cmd.tClip.tMax.x > fWidth)  { cmd.tClip.tMax.x = (float)fWidth; }
            if (cmd.tClip.tMax.y > fHeight) { cmd.tClip.tMax.y = (float)fHeight; }
            if (cmd.tClip.tMax.x <= cmd.tClip.tMin.x || cmd.tClip.tMax.y <= cmd.tClip.tMin.y)
                continue;

            const VkRect2D tScissor = {
                .offset.x      = (int32_t) (cmd.tClip.tMin.x < 0 ? 0 : cmd.tClip.tMin.x),
                .offset.y      = (int32_t) (cmd.tClip.tMin.y < 0 ? 0 : cmd.tClip.tMin.y),
                .extent.width  = (int32_t)pl_rect_width(&cmd.tClip),
                .extent.height = (int32_t)pl_rect_height(&cmd.tClip)
            };

            vkCmdSetScissor(tCmdBuf, 0, 1, &tScissor);
        }

        vkCmdBindDescriptorSets(tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanDrawCtx->tPipelineLayout, 0, 1, (const VkDescriptorSet*)&cmd.textureId, 0u, NULL);
        vkCmdPushConstants(tCmdBuf, ptVulkanDrawCtx->tPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, fScale);
        vkCmdPushConstants(tCmdBuf, ptVulkanDrawCtx->tPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, fTranslate);
        vkCmdDrawIndexed(tCmdBuf, cmd.elementCount, 1, cmd.indexOffset + iIndexOffset, iVertexOffset, 0);
    }

    // bump vertex & index buffer offset
    tBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
    tBufferInfo->uIndexBufferOffset += uIdxBufSzNeeded;
}

void
pl_cleanup_draw_context(plDrawContext* ptCtx)
{
    plVulkanDrawContext* ptVulkanDrawCtx = ptCtx->_platformData;

    vkDestroyShaderModule(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tVtxShdrStgInfo.module, NULL);
    vkDestroyShaderModule(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tPxlShdrStgInfo.module, NULL);
    vkDestroyShaderModule(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tSdfShdrStgInfo.module, NULL);
    vkDestroyBuffer(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingBuffer, NULL);
    vkFreeMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingMemory, NULL);
    vkDestroyDescriptorSetLayout(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tDescriptorSetLayout, NULL);
    vkDestroySampler(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tFontSampler, NULL);
    vkDestroyPipelineLayout(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tPipelineLayout, NULL);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawCtx->sbtBufferInfo); i++)
    {
        vkDestroyBuffer(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbtBufferInfo[i].tVertexBuffer, NULL);
        vkFreeMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbtBufferInfo[i].tVertexMemory, NULL);
        vkDestroyBuffer(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbtBufferInfo[i].tIndexBuffer, NULL);
        vkFreeMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbtBufferInfo[i].tIndexMemory, NULL);
    }

    for(uint32_t i = 0u; i < pl_sb_size(ptVulkanDrawCtx->sbtPipelines); i++)
    {
        vkDestroyPipeline(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbtPipelines[i].tRegularPipeline, NULL);
        vkDestroyPipeline(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbtPipelines[i].tSDFPipeline, NULL);
    }

    if(ptVulkanDrawCtx->uBufferDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawCtx->sbReturnedBuffers); i++)
        {
            vkDestroyBuffer(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbReturnedBuffers[i].tBuffer, NULL);
            vkFreeMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbReturnedBuffers[i].tDeviceMemory, NULL);
        }     
    }

    if(ptVulkanDrawCtx->uTextureDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanDrawCtx->sbReturnedTextures); i++)
        {
            vkDestroyImage(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbReturnedTextures[i].tImage, NULL);
            vkDestroyImageView(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbReturnedTextures[i].tView, NULL);
            vkFreeMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->sbReturnedTextures[i].tDeviceMemory, NULL);
        }     
    }

    vkDestroyCommandPool(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tCmdPool, NULL);
    vkDestroyDescriptorPool(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tDescriptorPool, NULL);

    PL_FREE(ptCtx->_platformData);
    ptCtx->_platformData = NULL;
}

VkDescriptorSet
pl_add_texture(plDrawContext* ptCtx, VkImageView tImageView, VkImageLayout tImageLayout)
{
    plVulkanDrawContext* ptVulkanDrawCtx = ptCtx->_platformData;

    VkDescriptorSet tDescriptorSet = {0};

    const VkDescriptorSetAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = ptVulkanDrawCtx->tDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &ptVulkanDrawCtx->tDescriptorSetLayout
    };

    VkResult tErr = vkAllocateDescriptorSets(ptVulkanDrawCtx->tDevice, &tAllocInfo, &tDescriptorSet);

    const VkDescriptorImageInfo tDescImage = {
        .sampler     = ptVulkanDrawCtx->tFontSampler,
        .imageView   = tImageView,
        .imageLayout = tImageLayout
    };

    const VkWriteDescriptorSet tWrite = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = tDescriptorSet,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &tDescImage
    };

    vkUpdateDescriptorSets(ptVulkanDrawCtx->tDevice, 1, &tWrite, 0, NULL);
    return tDescriptorSet;
}

void
pl_build_font_atlas(plDrawContext* ptCtx, plFontAtlas* ptAtlas)
{
    plVulkanDrawContext* ptVulkanDrawCtx = ptCtx->_platformData;

    pl__build_font_atlas(ptAtlas);
    ptAtlas->ctx = ptCtx;
    ptCtx->fontAtlas = ptAtlas;

    const VkImageCreateInfo tImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent.width  = ptAtlas->atlasSize[0],
        .extent.height = ptAtlas->atlasSize[1],
        .extent.depth  = 1u,
        .mipLevels     = 1u,
        .arrayLayers   = 1u,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .flags         = 0
    };
    PL_VULKAN(vkCreateImage(ptVulkanDrawCtx->tDevice, &tImageInfo, NULL, &ptVulkanDrawCtx->tFontTextureImage));

    VkMemoryRequirements tMemoryRequirements = {0};
    vkGetImageMemoryRequirements(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tFontTextureImage, &tMemoryRequirements);

    const VkMemoryAllocateInfo tFinalAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemoryRequirements.size,
        .memoryTypeIndex = pl__find_memory_type(ptVulkanDrawCtx->tMemProps, tMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    PL_VULKAN(vkAllocateMemory(ptVulkanDrawCtx->tDevice, &tFinalAllocInfo, NULL, &ptVulkanDrawCtx->tFontTextureMemory));
    PL_VULKAN(vkBindImageMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tFontTextureImage, ptVulkanDrawCtx->tFontTextureMemory, 0));

    // upload data
    uint32_t uDataSize = ptAtlas->atlasSize[0] * ptAtlas->atlasSize[1] * 4u;
    if(uDataSize > ptVulkanDrawCtx->szStageByteSize)
    {
        if(ptVulkanDrawCtx->tStagingMemory)
        {
            vkUnmapMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingMemory);
            vkDestroyBuffer(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingBuffer, NULL);
            vkFreeMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingMemory, NULL);
        }

        // double staging buffer size
        const VkBufferCreateInfo tStagingBufferInfo = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = uDataSize * 2,
            .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        PL_VULKAN(vkCreateBuffer(ptVulkanDrawCtx->tDevice, &tStagingBufferInfo, NULL, &ptVulkanDrawCtx->tStagingBuffer));

        VkMemoryRequirements tStagingMemoryRequirements = {0};
        vkGetBufferMemoryRequirements(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingBuffer, &tStagingMemoryRequirements);
        ptVulkanDrawCtx->szStageByteSize = tStagingMemoryRequirements.size;

        const VkMemoryAllocateInfo tStagingAllocInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = tStagingMemoryRequirements.size,
            .memoryTypeIndex = pl__find_memory_type(ptVulkanDrawCtx->tMemProps, tStagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };

        PL_VULKAN(vkAllocateMemory(ptVulkanDrawCtx->tDevice, &tStagingAllocInfo, NULL, &ptVulkanDrawCtx->tStagingMemory));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingBuffer, ptVulkanDrawCtx->tStagingMemory, 0));   
        PL_VULKAN(vkMapMemory(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tStagingMemory, 0, VK_WHOLE_SIZE, 0, &ptVulkanDrawCtx->pStageMapping));
    }
    memcpy(ptVulkanDrawCtx->pStageMapping, ptAtlas->pixelsAsRGBA32, uDataSize);

    const VkMappedMemoryRange tRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = ptVulkanDrawCtx->tStagingMemory,
        .size   = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDrawCtx->tDevice, 1, &tRange));

    const VkImageSubresourceRange tSubresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0u,
        .levelCount     = 1u,
        .baseArrayLayer = 0u,
        .layerCount     = 1u
    };

    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptVulkanDrawCtx->tCmdPool,
        .commandBufferCount = 1u
    };
    VkCommandBuffer tCommandBuffer = {0};
    PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDrawCtx->tDevice, &tAllocInfo, &tCommandBuffer));

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    PL_VULKAN(vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo));

    VkImageMemoryBarrier tBarrier1 = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = ptVulkanDrawCtx->tFontTextureImage,
        .subresourceRange    = tSubresourceRange,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT
    };
    vkCmdPipelineBarrier(tCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &tBarrier1);

    // copy buffer to image
    const VkBufferImageCopy tRegion = {
        .bufferOffset      = 0u,
        .bufferRowLength   = 0u,
        .bufferImageHeight = 0u,
        .imageSubresource  = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u
        },
        .imageOffset = {0},
        .imageExtent = {
            .width  = ptAtlas->atlasSize[0], 
            .height = ptAtlas->atlasSize[1],
            .depth  = 1
        }
    };
    vkCmdCopyBufferToImage(tCommandBuffer, ptVulkanDrawCtx->tStagingBuffer, ptVulkanDrawCtx->tFontTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &tRegion);     

    // transition image layout for shader usage
    VkImageMemoryBarrier tBarrier2 = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = ptVulkanDrawCtx->tFontTextureImage,
        .subresourceRange    = tSubresourceRange,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(tCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &tBarrier2);

    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer
    };
    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    vkQueueSubmit(ptVulkanDrawCtx->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE);
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDrawCtx->tDevice));
    vkFreeCommandBuffers(ptVulkanDrawCtx->tDevice, ptVulkanDrawCtx->tCmdPool, 1, &tCommandBuffer);

    const VkImageViewCreateInfo tViewInfo = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = ptVulkanDrawCtx->tFontTextureImage,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .baseMipLevel   = 0u,
            .levelCount     = tImageInfo.mipLevels,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
        }
    };
    PL_VULKAN(vkCreateImageView(ptVulkanDrawCtx->tDevice, &tViewInfo, NULL, &ptVulkanDrawCtx->tFontTextureImageView));

    ptCtx->fontAtlas->texture = pl_add_texture(ptCtx, ptVulkanDrawCtx->tFontTextureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static uint32_t
pl__find_memory_type(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
            return i;
    }
    return 0;
}

static void
pl__grow_vulkan_vertex_buffer(plDrawContext* ptCtx, uint32_t uVtxBufSzNeeded, uint32_t uFrameIndex)
{
    plVulkanDrawContext* ptVulkanDrawCtx = ptCtx->_platformData;
    plVulkanBufferInfo* tBufferInfo = &ptVulkanDrawCtx->sbtBufferInfo[uFrameIndex];

    // buffer currently exists & mapped, submit for cleanup
    if(tBufferInfo->ucVertexBufferMap)
    {
        const plBufferReturn tReturnBuffer = {
            .tBuffer       = tBufferInfo->tVertexBuffer,
            .tDeviceMemory = tBufferInfo->tVertexMemory,
            .slFreedFrame  = (int64_t)(ptCtx->frameCount + PL_MAX_FRAMES_IN_FLIGHT * 2)
        };
        pl_sb_push(ptVulkanDrawCtx->sbReturnedBuffers, tReturnBuffer);
        ptVulkanDrawCtx->uBufferDeletionQueueSize++;
        vkUnmapMemory(ptVulkanDrawCtx->tDevice, tBufferInfo->tVertexMemory);
    }

    // create new buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = uVtxBufSzNeeded,
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(ptVulkanDrawCtx->tDevice, &tBufferCreateInfo, NULL, &tBufferInfo->tVertexBuffer));

    // check memory requirements
    VkMemoryRequirements tMemReqs = {0};
    vkGetBufferMemoryRequirements(ptVulkanDrawCtx->tDevice, tBufferInfo->tVertexBuffer, &tMemReqs);

    // allocate memory & bind buffer
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemReqs.size,
        .memoryTypeIndex = pl__find_memory_type(ptVulkanDrawCtx->tMemProps, tMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    tBufferInfo->uVertexByteSize = (uint32_t)tMemReqs.size;
    PL_VULKAN(vkAllocateMemory(ptVulkanDrawCtx->tDevice, &tAllocInfo, NULL, &tBufferInfo->tVertexMemory));
    PL_VULKAN(vkBindBufferMemory(ptVulkanDrawCtx->tDevice, tBufferInfo->tVertexBuffer, tBufferInfo->tVertexMemory, 0));

    // map memory persistently
    PL_VULKAN(vkMapMemory(ptVulkanDrawCtx->tDevice, tBufferInfo->tVertexMemory, 0, tMemReqs.size, 0, (void**)&tBufferInfo->ucVertexBufferMap));

    tBufferInfo->uVertexBufferOffset = 0;
}

static void
pl__grow_vulkan_index_buffer(plDrawContext* ptCtx, uint32_t uIdxBufSzNeeded, uint32_t uFrameIndex)
{
    plVulkanDrawContext* ptVulkanDrawCtx = ptCtx->_platformData;
    plVulkanBufferInfo* tBufferInfo = &ptVulkanDrawCtx->sbtBufferInfo[uFrameIndex];

    // buffer currently exists & mapped, submit for cleanup
    if(tBufferInfo->ucIndexBufferMap)
    {
        const plBufferReturn tReturnBuffer = {
            .tBuffer       = tBufferInfo->tIndexBuffer,
            .tDeviceMemory = tBufferInfo->tIndexMemory,
            .slFreedFrame  = (int64_t)(ptCtx->frameCount + PL_MAX_FRAMES_IN_FLIGHT * 2)
        };
        pl_sb_push(ptVulkanDrawCtx->sbReturnedBuffers, tReturnBuffer);
        ptVulkanDrawCtx->uBufferDeletionQueueSize++;
        vkUnmapMemory(ptVulkanDrawCtx->tDevice, tBufferInfo->tIndexMemory);
    }

    // create new buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = uIdxBufSzNeeded,
        .usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(ptVulkanDrawCtx->tDevice, &tBufferCreateInfo, NULL, &tBufferInfo->tIndexBuffer));

    // check memory requirements
    VkMemoryRequirements tMemReqs = {0};
    vkGetBufferMemoryRequirements(ptVulkanDrawCtx->tDevice, tBufferInfo->tIndexBuffer, &tMemReqs);

    // alllocate memory & bind buffer
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = tMemReqs.size,
        .memoryTypeIndex = pl__find_memory_type(ptVulkanDrawCtx->tMemProps, tMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    tBufferInfo->uIndexByteSize = (uint32_t)tMemReqs.size;
    PL_VULKAN(vkAllocateMemory(ptVulkanDrawCtx->tDevice, &tAllocInfo, NULL, &tBufferInfo->tIndexMemory));
    PL_VULKAN(vkBindBufferMemory(ptVulkanDrawCtx->tDevice, tBufferInfo->tIndexBuffer, tBufferInfo->tIndexMemory, 0));

    // map memory persistently
    PL_VULKAN(vkMapMemory(ptVulkanDrawCtx->tDevice, tBufferInfo->tIndexMemory, 0, tMemReqs.size, 0, (void**)&tBufferInfo->ucIndexBufferMap));

    tBufferInfo->uIndexBufferOffset = 0;
}

static plVulkanPipelineEntry*
pl__get_pipelines(plVulkanDrawContext* ptCtx, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount)
{
    // return pipeline entry if it exists
    for(uint32_t i = 0; i < pl_sb_size(ptCtx->sbtPipelines); i++)
    {
        if(ptCtx->sbtPipelines[i].tRenderPass == tRenderPass)
            return &ptCtx->sbtPipelines[i];
    }

    // create new pipeline entry
    plVulkanPipelineEntry tEntry = {
        .tRenderPass = tRenderPass
    };

    const VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkVertexInputAttributeDescription aAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32_SFLOAT,   0u},
        {1u, 0u, VK_FORMAT_R32G32_SFLOAT,   8u},
        {2u, 0u, VK_FORMAT_R8G8B8A8_UNORM, 16u}
    };
    
    const VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0u,
        .stride    = sizeof(plDrawVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    const VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1u,
        .vertexAttributeDescriptionCount = 3u,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = aAttributeDescriptions
    };

    // dynamic, set per frame
    VkViewport tViewport = {0};
    VkRect2D tScissor = {0};

    const VkPipelineViewportStateCreateInfo tViewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &tViewport,
        .scissorCount  = 1,
        .pScissors     = &tScissor
    };

    const VkPipelineRasterizationStateCreateInfo tRasterizer = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE
    };

    const VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = VK_FALSE,
        .depthWriteEnable      = VK_FALSE,
        .depthCompareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f,
        .stencilTestEnable     = VK_FALSE,
        .front                 = {0},
        .back                  = {0}
    };

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------

    const VkPipelineColorBlendAttachmentState tColorBlendAttachment = {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD
    };

    const VkPipelineColorBlendStateCreateInfo tColorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &tColorBlendAttachment,
        .blendConstants  = {0}
    };

    const VkPipelineMultisampleStateCreateInfo tMultisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable  = VK_FALSE,
        .rasterizationSamples = tMSAASampleCount
    };

    VkDynamicState atDynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    const VkPipelineDynamicStateCreateInfo tDynamicState = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2u,
        .pDynamicStates    = atDynamicStateEnables
    };

    //---------------------------------------------------------------------
    // Create Regular Pipeline
    //---------------------------------------------------------------------

    VkPipelineShaderStageCreateInfo atShaderStages[] = { ptCtx->tVtxShdrStgInfo, ptCtx->tPxlShdrStgInfo };

    VkGraphicsPipelineCreateInfo pipeInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2u,
        .pStages             = atShaderStages,
        .pVertexInputState   = &tVertexInputInfo,
        .pInputAssemblyState = &tInputAssembly,
        .pViewportState      = &tViewportState,
        .pRasterizationState = &tRasterizer,
        .pMultisampleState   = &tMultisampling,
        .pColorBlendState    = &tColorBlending,
        .pDynamicState       = &tDynamicState,
        .layout              = ptCtx->tPipelineLayout,
        .renderPass          = tRenderPass,
        .subpass             = 0u,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };
    PL_VULKAN(vkCreateGraphicsPipelines(ptCtx->tDevice, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &tEntry.tRegularPipeline));

    //---------------------------------------------------------------------
    // Create SDF Pipeline
    //---------------------------------------------------------------------

    atShaderStages[1] = ptCtx->tSdfShdrStgInfo;
    pipeInfo.pStages = atShaderStages;

    PL_VULKAN(vkCreateGraphicsPipelines(ptCtx->tDevice, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &tEntry.tSDFPipeline));

    // add to entries
    pl_sb_push(ptCtx->sbtPipelines, tEntry);

    return &pl_sb_back(ptCtx->sbtPipelines);
}