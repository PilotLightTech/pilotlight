/*
   pl_draw_vulkan.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api
// [SECTION] c file
// [SECTION] includes
// [SECTION] shaders
// [SECTION] internal structs
// [SECTION] internal helper forward declarations
// [SECTION] implementation
// [SECTION] internal helpers implementation
*/

#ifndef PL_DRAW_VULKAN_H
#define PL_DRAW_VULKAN_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_draw.h"
#include "vulkan/vulkan.h"

#ifndef PL_VULKAN
#include <assert.h>
#define PL_VULKAN(x) PL_ASSERT(x == VK_SUCCESS)
#endif

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// backend implementation
void pl_initialize_draw_context_vulkan(plDrawContext* ctx, VkPhysicalDevice tPhysicalDevice, uint32_t imageCount, VkDevice tLogicalDevice);
void pl_setup_drawlist_vulkan     (plDrawList* drawlist, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount);
void pl_submit_drawlist_vulkan    (plDrawList* drawlist, float width, float height, VkCommandBuffer cmdBuf, uint32_t currentFrameIndex);
void pl_new_draw_frame            (plDrawContext* ctx);

// misc
VkDescriptorSet pl_add_texture(plDrawContext* drawContext, VkImageView imageView, VkImageLayout imageLayout);

#endif // PL_DRAWING_VULKAN_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

#ifdef PL_DRAW_VULKAN_IMPLEMENTATION

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pl_ds.h"

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
// [SECTION] internal structs
//-----------------------------------------------------------------------------

// wraps a buffer to be freed
typedef struct
{
    VkBuffer       buffer;
    VkDeviceMemory deviceMemory;
    int64_t        slFreedFrame;
} plBufferReturn;

// wraps a texture to be freed
typedef struct
{
    VkImage        image;
    VkImageView    view;
    VkDeviceMemory deviceMemory;
    int64_t        slFreedFrame;
} plTextureReturn;

typedef struct
{
    VkDevice                         device;
    VkCommandPool                    tCmdPool;
    VkQueue                          tGraphicsQueue;
    uint32_t                         imageCount;
    VkPhysicalDeviceMemoryProperties memProps;
    VkDescriptorSetLayout            descriptorSetLayout;
    VkDescriptorPool                 descriptorPool;

    // fonts (temp until we have a proper atlas)
    VkSampler                        fontSampler;
    VkImage                          fontTextureImage;
    VkImageView                      fontTextureImageView;
    VkDeviceMemory                   fontTextureMemory;

    // committed buffers
    plBufferReturn* sbReturnedBuffers;
    plBufferReturn* sbReturnedBuffersTemp;
    uint32_t        bufferDeletionQueueSize;

    // committed textures
    plTextureReturn* sbReturnedTextures;
    plTextureReturn* sbReturnedTexturesTemp;
    uint32_t         textureDeletionQueueSize;

    // staging buffer
    size_t         stageByteSize;
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingMemory;
    void*          mapping; // persistent mapping for staging buffer

    // drawlist pipeline caching
    VkPipelineLayout                drawlistPipelineLayout;
    VkDescriptorSetLayout           drawlistDescriptorSetLayout;
    VkPipelineShaderStageCreateInfo pxlShdrStgInfo;
    VkPipelineShaderStageCreateInfo sdfShdrStgInfo;
    VkPipelineShaderStageCreateInfo vtxShdrStgInfo;

} plVulkanDrawContext;

typedef struct
{
    // vertex buffer
    VkBuffer*       sbVertexBuffer;
    VkDeviceMemory* sbVertexMemory;
    unsigned char** sbVertexBufferMap;
    unsigned int*   sbVertexByteSize;
    
    // index buffer
    VkBuffer*       sbIndexBuffer;
    VkDeviceMemory* sbIndexMemory;
    unsigned char** sbIndexBufferMap;
    unsigned int*   sbIndexByteSize;

    // pipelines (these are per drawlist since it depends on the renderpass)
    VkPipeline       regularPipeline;
    VkPipeline       sdfPipeline;

} plVulkanDrawList;

//-----------------------------------------------------------------------------
// [SECTION] internal helper forward declarations
//-----------------------------------------------------------------------------

extern void     pl__cleanup_font_atlas   (plFontAtlas* atlas); // in pl_draw.c
extern void     pl__new_draw_frame   (plDrawContext* ctx); // in pl_draw.c
static uint32_t pl__find_memory_type(VkPhysicalDeviceMemoryProperties memProps, uint32_t typeFilter, VkMemoryPropertyFlags properties);
static void     pl__grow_vulkan_vertex_buffer(plDrawList* ptrDrawlist, uint32_t uVtxBufSzNeeded, uint32_t currentFrameIndex);
static void     pl__grow_vulkan_index_buffer(plDrawList* ptrDrawlist, uint32_t uIdxBufSzNeeded, uint32_t currentFrameIndex);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

void
pl_initialize_draw_context_vulkan(plDrawContext* ctx, VkPhysicalDevice tPhysicalDevice, uint32_t imageCount, VkDevice tLogicalDevice)
{
    plVulkanDrawContext* vulkanDrawContext = (plVulkanDrawContext*)PL_ALLOC(sizeof(plVulkanDrawContext));
    memset(vulkanDrawContext, 0, sizeof(plVulkanDrawContext));
    vulkanDrawContext->device = tLogicalDevice;
    vulkanDrawContext->imageCount = imageCount;
    ctx->_platformData = vulkanDrawContext;

    // get physical device properties
    vkGetPhysicalDeviceMemoryProperties(tPhysicalDevice, &vulkanDrawContext->memProps);

    // create descriptor pool
    // TODO: add a system to allow this to grow as needed
    VkDescriptorPoolSize poolSizes = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 1000u,
        .poolSizeCount = 1u,
        .pPoolSizes    = &poolSizes
    };
    PL_VULKAN(vkCreateDescriptorPool(tLogicalDevice, &poolInfo, NULL, &vulkanDrawContext->descriptorPool));

    // find queue that supports the graphics bit
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties queueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(tPhysicalDevice, &uQueueFamCnt, queueFamilies);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            vkGetDeviceQueue(tLogicalDevice, i, 0, &vulkanDrawContext->tGraphicsQueue);
            VkCommandPoolCreateInfo commandPoolInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .queueFamilyIndex = i,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            };
            PL_VULKAN(vkCreateCommandPool(tLogicalDevice, &commandPoolInfo, NULL, &vulkanDrawContext->tCmdPool));
            break;
        }
    }

    // create font sampler
    VkSamplerCreateInfo samplerInfo = {
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
    PL_VULKAN(vkCreateSampler(vulkanDrawContext->device, &samplerInfo, NULL, &vulkanDrawContext->fontSampler));

    // create descriptor set layout
    VkDescriptorSetLayoutBinding binding = {
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = &vulkanDrawContext->fontSampler,
    };

    VkDescriptorSetLayoutCreateInfo info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &binding,
    };

    PL_VULKAN(vkCreateDescriptorSetLayout(vulkanDrawContext->device, &info, NULL, &vulkanDrawContext->descriptorSetLayout));

    //-----------------------------------------------------------------------------
    // pipeline commons
    //-----------------------------------------------------------------------------

    VkDescriptorSetLayoutBinding bindings = {
        .binding = 0u,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = &vulkanDrawContext->fontSampler
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &bindings
    };

    PL_VULKAN(vkCreateDescriptorSetLayout(vulkanDrawContext->device, &layoutInfo, NULL, &vulkanDrawContext->drawlistDescriptorSetLayout));

    VkPushConstantRange pushConstant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(float) * 4
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1u,
        .pSetLayouts = &vulkanDrawContext->drawlistDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant,
    };

    PL_VULKAN(vkCreatePipelineLayout(vulkanDrawContext->device, &pipelineLayoutInfo, NULL, &vulkanDrawContext->drawlistPipelineLayout));

    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------

    vulkanDrawContext->vtxShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vulkanDrawContext->vtxShdrStgInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vulkanDrawContext->vtxShdrStgInfo.pName = "main";

    VkShaderModuleCreateInfo vtxShdrInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_vert_spv),
        .pCode = __glsl_shader_vert_spv
    };
    PL_ASSERT(vkCreateShaderModule(vulkanDrawContext->device, &vtxShdrInfo, NULL, &vulkanDrawContext->vtxShdrStgInfo.module) == VK_SUCCESS);

    //---------------------------------------------------------------------
    // fragment shader stage
    //---------------------------------------------------------------------
    vulkanDrawContext->pxlShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vulkanDrawContext->pxlShdrStgInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    vulkanDrawContext->pxlShdrStgInfo.pName = "main";

    VkShaderModuleCreateInfo pxlShdrInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_frag_spv),
        .pCode = __glsl_shader_frag_spv
    };
    PL_ASSERT(vkCreateShaderModule(vulkanDrawContext->device, &pxlShdrInfo, NULL, &vulkanDrawContext->pxlShdrStgInfo.module) == VK_SUCCESS);

    //---------------------------------------------------------------------
    // sdf fragment shader stage
    //---------------------------------------------------------------------
    vulkanDrawContext->sdfShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vulkanDrawContext->sdfShdrStgInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    vulkanDrawContext->sdfShdrStgInfo.pName = "main";

    VkShaderModuleCreateInfo sdfShdrInfo  = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_fragsdf_spv),
        .pCode = __glsl_shader_fragsdf_spv
    };
    PL_ASSERT(vkCreateShaderModule(vulkanDrawContext->device, &sdfShdrInfo, NULL, &vulkanDrawContext->sdfShdrStgInfo.module) == VK_SUCCESS);
}

void
pl_setup_drawlist_vulkan(plDrawList* drawlist, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount)
{
    if(drawlist->_platformData == NULL)
    {
        drawlist->_platformData = PL_ALLOC(sizeof(plVulkanDrawList));
        memset(drawlist->_platformData, 0, sizeof(plVulkanDrawList));
    }

    plVulkanDrawList* vulkanDrawlist = (plVulkanDrawList*)drawlist->_platformData;
    plVulkanDrawContext* vulkanDrawCtx = drawlist->ctx->_platformData;
    pl_sb_resize(vulkanDrawlist->sbVertexBuffer, PL_MAX_FRAMES_IN_FLIGHT);
    pl_sb_resize(vulkanDrawlist->sbVertexMemory, PL_MAX_FRAMES_IN_FLIGHT);
    pl_sb_resize(vulkanDrawlist->sbVertexBufferMap, PL_MAX_FRAMES_IN_FLIGHT);
    pl_sb_resize(vulkanDrawlist->sbVertexByteSize, PL_MAX_FRAMES_IN_FLIGHT);
    pl_sb_resize(vulkanDrawlist->sbIndexBuffer, PL_MAX_FRAMES_IN_FLIGHT);
    pl_sb_resize(vulkanDrawlist->sbIndexMemory, PL_MAX_FRAMES_IN_FLIGHT);
    pl_sb_resize(vulkanDrawlist->sbIndexBufferMap, PL_MAX_FRAMES_IN_FLIGHT);
    pl_sb_resize(vulkanDrawlist->sbIndexByteSize, PL_MAX_FRAMES_IN_FLIGHT);

    //---------------------------------------------------------------------
    // input assembler stage
    //---------------------------------------------------------------------

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32_SFLOAT,   0u},
        {1u, 0u, VK_FORMAT_R32G32_SFLOAT,   8u},
        {2u, 0u, VK_FORMAT_R8G8B8A8_UNORM, 16u}
    };
    
    VkVertexInputBindingDescription bindingDescription = {
        .binding   = 0u,
        .stride    = sizeof(plDrawVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1u,
        .vertexAttributeDescriptionCount = 3u,
        .pVertexBindingDescriptions = &bindingDescription,
        .pVertexAttributeDescriptions = attributeDescriptions
    };

    // dynamic, set per frame
    VkViewport viewport = {0};
    VkRect2D scissor = {0};

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
        .stencilTestEnable = VK_FALSE,
        .front = {0},
        .back = {0}
    };

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = tMSAASampleCount
    };

    VkDynamicState ptrDynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2u,
        .pDynamicStates = ptrDynamicStateEnables
    };

    //---------------------------------------------------------------------
    // Create Regular Pipeline
    //---------------------------------------------------------------------

    VkPipelineShaderStageCreateInfo ptrShaderStages[] = { vulkanDrawCtx->vtxShdrStgInfo, vulkanDrawCtx->pxlShdrStgInfo };

    VkGraphicsPipelineCreateInfo pipeInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2u,
        .pStages             = ptrShaderStages,
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = &dynamicState,
        .layout              = vulkanDrawCtx->drawlistPipelineLayout,
        .renderPass          = tRenderPass,
        .subpass             = 0u,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &depthStencil
    };
    PL_VULKAN(vkCreateGraphicsPipelines(vulkanDrawCtx->device, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &vulkanDrawlist->regularPipeline));

    //---------------------------------------------------------------------
    // Create SDF Pipeline
    //---------------------------------------------------------------------

    ptrShaderStages[1] = vulkanDrawCtx->sdfShdrStgInfo;
    pipeInfo.pStages = ptrShaderStages;

    PL_VULKAN(vkCreateGraphicsPipelines(vulkanDrawCtx->device, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &vulkanDrawlist->sdfPipeline));
}

void
pl_new_draw_frame(plDrawContext* ctx)
{
    plVulkanDrawContext* vulkanCtx = ctx->_platformData;

    //-----------------------------------------------------------------------------
    // buffer deletion queue
    //-----------------------------------------------------------------------------
    pl_sb_reset(vulkanCtx->sbReturnedBuffersTemp);
    if(vulkanCtx->bufferDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(vulkanCtx->sbReturnedBuffers); i++)
        {
            if(vulkanCtx->sbReturnedBuffers[i].slFreedFrame < (int64_t)ctx->frameCount)
            {
                vulkanCtx->bufferDeletionQueueSize--;
                vkDestroyBuffer(vulkanCtx->device, vulkanCtx->sbReturnedBuffers[i].buffer, NULL);
                vkFreeMemory(vulkanCtx->device, vulkanCtx->sbReturnedBuffers[i].deviceMemory, NULL);
            }
            else
            {
                pl_sb_push(vulkanCtx->sbReturnedBuffersTemp, vulkanCtx->sbReturnedBuffers[i]);
            }
        }     
    }

    pl_sb_reset(vulkanCtx->sbReturnedBuffers);
    for(uint32_t i = 0; i < pl_sb_size(vulkanCtx->sbReturnedBuffersTemp); i++)
        pl_sb_push(vulkanCtx->sbReturnedBuffers, vulkanCtx->sbReturnedBuffersTemp[i]);

    //-----------------------------------------------------------------------------
    // texture deletion queue
    //-----------------------------------------------------------------------------
    pl_sb_reset(vulkanCtx->sbReturnedTexturesTemp);
    if(vulkanCtx->textureDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(vulkanCtx->sbReturnedTextures); i++)
        {
            if(vulkanCtx->sbReturnedTextures[i].slFreedFrame < (int64_t)ctx->frameCount)
            {
                vulkanCtx->textureDeletionQueueSize--;
                vkDestroyImage(vulkanCtx->device, vulkanCtx->sbReturnedTextures[i].image, NULL);
                vkDestroyImageView(vulkanCtx->device, vulkanCtx->sbReturnedTextures[i].view, NULL);
                vkFreeMemory(vulkanCtx->device, vulkanCtx->sbReturnedTextures[i].deviceMemory, NULL);
            }
            else
            {
                pl_sb_push(vulkanCtx->sbReturnedTexturesTemp, vulkanCtx->sbReturnedTextures[i]);
            }
        }     
    }

    pl_sb_reset(vulkanCtx->sbReturnedTextures);
    for(uint32_t i = 0; i < pl_sb_size(vulkanCtx->sbReturnedTexturesTemp); i++)
        pl_sb_push(vulkanCtx->sbReturnedTextures, vulkanCtx->sbReturnedTexturesTemp[i]);

    pl__new_draw_frame(ctx);
}

void
pl_cleanup_font_atlas(plFontAtlas* atlas)
{
    plVulkanDrawContext* vulkanDrawCtx = atlas->ctx->_platformData;
    plTextureReturn returnTexture = 
    {
        .image = vulkanDrawCtx->fontTextureImage,
        .view = vulkanDrawCtx->fontTextureImageView,
        .deviceMemory = vulkanDrawCtx->fontTextureMemory,
        .slFreedFrame = (int64_t)(atlas->ctx->frameCount + vulkanDrawCtx->imageCount*2)
    };
    pl_sb_push(vulkanDrawCtx->sbReturnedTextures, returnTexture);
    vulkanDrawCtx->textureDeletionQueueSize++;
    pl__cleanup_font_atlas(atlas);
}

void
pl_submit_drawlist_vulkan(plDrawList* drawlist, float width, float height, VkCommandBuffer cmdBuf, uint32_t currentFrameIndex)
{
    if(pl_sb_size(drawlist->sbVertexBuffer) == 0u)
        return;

    plDrawContext* drawContext = drawlist->ctx;
    plVulkanDrawContext* vulkanData = drawContext->_platformData;
    plVulkanDrawList* drawlistVulkanData = drawlist->_platformData;

    // ensure gpu vertex buffer size is adequate
    uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex) * pl_sb_size(drawlist->sbVertexBuffer) * PL_MAX_FRAMES_IN_FLIGHT;
    if(uVtxBufSzNeeded >= drawlistVulkanData->sbVertexByteSize[currentFrameIndex])
        pl__grow_vulkan_vertex_buffer(drawlist, uVtxBufSzNeeded * 2, currentFrameIndex);

    // ensure gpu index buffer size is adequate
    uint32_t uIdxBufSzNeeded = drawlist->indexBufferByteSize * PL_MAX_FRAMES_IN_FLIGHT;
    if(uIdxBufSzNeeded >= drawlistVulkanData->sbIndexByteSize[currentFrameIndex])
        pl__grow_vulkan_index_buffer(drawlist, uIdxBufSzNeeded * 2, currentFrameIndex);

    // vertex GPU data transfer
    memcpy(drawlistVulkanData->sbVertexBufferMap[currentFrameIndex], drawlist->sbVertexBuffer, sizeof(plDrawVertex) * pl_sb_size(drawlist->sbVertexBuffer)); //-V1004
    
    // index GPU data transfer
    uint32_t uTempIndexBufferOffset = 0u;
    uint32_t globalIdxBufferIndexOffset = 0u;

    for(uint32_t i = 0u; i < pl_sb_size(drawlist->sbSubmittedLayers); i++)
    {
        plDrawCommand* lastCommand = NULL;
        plDrawLayer* layer = drawlist->sbSubmittedLayers[i];

        unsigned char* destination = drawlistVulkanData->sbIndexBufferMap[currentFrameIndex];
        memcpy(&destination[uTempIndexBufferOffset], layer->sbIndexBuffer, sizeof(uint32_t) * pl_sb_size(layer->sbIndexBuffer)); //-V1004

        uTempIndexBufferOffset += pl_sb_size(layer->sbIndexBuffer)*sizeof(uint32_t);

        // attempt to merge commands
        for(uint32_t j = 0u; j < pl_sb_size(layer->sbCommandBuffer); j++)
        {
            plDrawCommand *layerCommand = &layer->sbCommandBuffer[j];
            bool bCreateNewCommand = true;

            if(lastCommand)
            {
                // check for same texture (allows merging draw calls)
                if(lastCommand->textureId == layerCommand->textureId && lastCommand->sdf == layerCommand->sdf)
                {
                    lastCommand->elementCount += layerCommand->elementCount;
                    bCreateNewCommand = false;
                }

                // check for same clipping (allows merging draw calls)
                if(layerCommand->tClip.tMax.x != lastCommand->tClip.tMax.x || layerCommand->tClip.tMax.y != lastCommand->tClip.tMax.y ||
                    layerCommand->tClip.tMin.x != lastCommand->tClip.tMin.x || layerCommand->tClip.tMin.y != lastCommand->tClip.tMin.y)
                {
                    bCreateNewCommand = true;
                }
                
            }

            if(bCreateNewCommand)
            {
                layerCommand->indexOffset = globalIdxBufferIndexOffset + layerCommand->indexOffset;
                pl_sb_push(drawlist->sbDrawCommands, *layerCommand);       
                lastCommand = layerCommand;
            }
            
        }    
        globalIdxBufferIndexOffset += pl_sb_size(layer->sbIndexBuffer);    
    }

    VkMappedMemoryRange range[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = drawlistVulkanData->sbVertexMemory[currentFrameIndex],
            .size = VK_WHOLE_SIZE
        },
        {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = drawlistVulkanData->sbIndexMemory[currentFrameIndex],
            .size = VK_WHOLE_SIZE
        }
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(vulkanData->device, 2, range));

    VkDeviceSize offsets = { 0u };
    vkCmdBindIndexBuffer(cmdBuf, drawlistVulkanData->sbIndexBuffer[currentFrameIndex], 0u, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &drawlistVulkanData->sbVertexBuffer[currentFrameIndex], &offsets);

    const float fScale[] = { 2.0f / width, 2.0f / height};
    const float fTranslate[] = {-1.0f, -1.0f};
    bool sdf = false;
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, drawlistVulkanData->regularPipeline); 
    for(uint32_t i = 0u; i < pl_sb_size(drawlist->sbDrawCommands); i++)
    {
        plDrawCommand cmd = drawlist->sbDrawCommands[i];

        if(cmd.sdf && !sdf) // delay
        {
            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, drawlistVulkanData->sdfPipeline); 
            sdf = true;
        }
        else if(!cmd.sdf && sdf)
        {
            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, drawlistVulkanData->regularPipeline); 
            sdf = false;
        }

        if(pl_rect_width(&cmd.tClip) == 0)
        {
            const VkRect2D tScissor = {
                .extent.width = (int32_t) width,
                .extent.height = (int32_t) height
            };
            vkCmdSetScissor(cmdBuf, 0, 1, &tScissor);
        }
        else
        {
            const float fOrigWidth = pl_rect_width(&cmd.tClip);
            const float fOrigHeight = pl_rect_height(&cmd.tClip);
            const VkRect2D tScissor = {
                .offset.x      = (int32_t) (cmd.tClip.tMin.x < 0 ? 0 : cmd.tClip.tMin.x),
                .offset.y      = (int32_t) (cmd.tClip.tMin.y < 0 ? 0 : cmd.tClip.tMin.y),
                .extent.width  = (cmd.tClip.tMin.x + fOrigWidth  > width ? (int32_t)width - (int32_t)cmd.tClip.tMin.x : (int32_t)fOrigWidth),
                .extent.height = (cmd.tClip.tMin.y + fOrigHeight  > height ? (int32_t)height - (int32_t)cmd.tClip.tMin.y: (int32_t)fOrigHeight)
            };
            vkCmdSetScissor(cmdBuf, 0, 1, &tScissor);
        }

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanData->drawlistPipelineLayout, 0, 1, (const VkDescriptorSet*)&cmd.textureId, 0u, NULL);
        vkCmdPushConstants(cmdBuf, vulkanData->drawlistPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, fScale);
        vkCmdPushConstants(cmdBuf, vulkanData->drawlistPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, fTranslate);
        vkCmdDrawIndexed(cmdBuf, cmd.elementCount, 1, cmd.indexOffset, 0, 0);
    }
}

void
pl_cleanup_draw_context(plDrawContext* ctx)
{
    plVulkanDrawContext* vulkanDrawCtx = ctx->_platformData;

    vkDestroyShaderModule(vulkanDrawCtx->device, vulkanDrawCtx->vtxShdrStgInfo.module, NULL);
    vkDestroyShaderModule(vulkanDrawCtx->device, vulkanDrawCtx->pxlShdrStgInfo.module, NULL);
    vkDestroyShaderModule(vulkanDrawCtx->device, vulkanDrawCtx->sdfShdrStgInfo.module, NULL);
    vkDestroyBuffer(vulkanDrawCtx->device, vulkanDrawCtx->stagingBuffer, NULL);
    vkFreeMemory(vulkanDrawCtx->device, vulkanDrawCtx->stagingMemory, NULL);
    vkDestroyDescriptorSetLayout(vulkanDrawCtx->device, vulkanDrawCtx->descriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(vulkanDrawCtx->device, vulkanDrawCtx->drawlistDescriptorSetLayout, NULL);
    vkDestroySampler(vulkanDrawCtx->device, vulkanDrawCtx->fontSampler, NULL);
    vkDestroyPipelineLayout(vulkanDrawCtx->device, vulkanDrawCtx->drawlistPipelineLayout, NULL);

    for(uint32_t i = 0u; i < pl_sb_size(ctx->sbDrawlists); i++)
    {
        plDrawList* drawlist = ctx->sbDrawlists[i];
        plVulkanDrawList* vulkanDrawlist = (plVulkanDrawList*)drawlist->_platformData;
        
        for(uint32_t j = 0u; j < pl_sb_size(vulkanDrawlist->sbIndexBuffer); j++)
            vkDestroyBuffer(vulkanDrawCtx->device, vulkanDrawlist->sbIndexBuffer[j], NULL);

        for(uint32_t j = 0u; j < pl_sb_size(vulkanDrawlist->sbIndexMemory); j++)
            vkFreeMemory(vulkanDrawCtx->device, vulkanDrawlist->sbIndexMemory[j], NULL);

        for(uint32_t j = 0u; j < pl_sb_size(vulkanDrawlist->sbVertexBuffer); j++)
            vkDestroyBuffer(vulkanDrawCtx->device, vulkanDrawlist->sbVertexBuffer[j], NULL);

        for(uint32_t j = 0u; j < pl_sb_size(vulkanDrawlist->sbVertexMemory); j++)
            vkFreeMemory(vulkanDrawCtx->device, vulkanDrawlist->sbVertexMemory[j], NULL);
        
        vkDestroyPipeline(vulkanDrawCtx->device, vulkanDrawlist->regularPipeline, NULL);
        vkDestroyPipeline(vulkanDrawCtx->device, vulkanDrawlist->sdfPipeline, NULL);
    }

    if(vulkanDrawCtx->bufferDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(vulkanDrawCtx->sbReturnedBuffers); i++)
        {
            vkDestroyBuffer(vulkanDrawCtx->device, vulkanDrawCtx->sbReturnedBuffers[i].buffer, NULL);
            vkFreeMemory(vulkanDrawCtx->device, vulkanDrawCtx->sbReturnedBuffers[i].deviceMemory, NULL);
        }     
    }

    if(vulkanDrawCtx->textureDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(vulkanDrawCtx->sbReturnedTextures); i++)
        {
            vkDestroyImage(vulkanDrawCtx->device, vulkanDrawCtx->sbReturnedTextures[i].image, NULL);
            vkDestroyImageView(vulkanDrawCtx->device, vulkanDrawCtx->sbReturnedTextures[i].view, NULL);
            vkFreeMemory(vulkanDrawCtx->device, vulkanDrawCtx->sbReturnedTextures[i].deviceMemory, NULL);
        }     
    }

    vkDestroyCommandPool(vulkanDrawCtx->device, vulkanDrawCtx->tCmdPool, NULL);
    vkDestroyDescriptorPool(vulkanDrawCtx->device, vulkanDrawCtx->descriptorPool, NULL);
}

VkDescriptorSet
pl_add_texture(plDrawContext* drawContext, VkImageView imageView, VkImageLayout imageLayout)
{
    plVulkanDrawContext* vulkanData = drawContext->_platformData;

    // Create Descriptor Set:
    VkDescriptorSet descriptor_set = {0};

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vulkanData->descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vulkanData->descriptorSetLayout
    };

    VkResult err = vkAllocateDescriptorSets(vulkanData->device, &alloc_info, &descriptor_set);

    // Update the Descriptor Set:
    VkDescriptorImageInfo desc_image = {
        .sampler = vulkanData->fontSampler,
        .imageView = imageView,
        .imageLayout = imageLayout
    };

    VkWriteDescriptorSet write_desc = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &desc_image
    };

    vkUpdateDescriptorSets(vulkanData->device, 1, &write_desc, 0, NULL);
    return descriptor_set;
}

void
pl_build_font_atlas(plDrawContext* ctx, plFontAtlas* atlas)
{
    plVulkanDrawContext* vulkanDrawCtx = ctx->_platformData;

    pl__build_font_atlas(atlas);
    atlas->ctx = ctx;
    ctx->fontAtlas = atlas;

    VkImageCreateInfo imageInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = atlas->atlasSize[0],
        .extent.height = atlas->atlasSize[1],
        .extent.depth = 1u,
        .mipLevels = 1u,
        .arrayLayers = 1u,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = 0
    };
    PL_VULKAN(vkCreateImage(vulkanDrawCtx->device, &imageInfo, NULL, &vulkanDrawCtx->fontTextureImage));

    VkMemoryRequirements memoryRequirements = {0};
    vkGetImageMemoryRequirements(vulkanDrawCtx->device, vulkanDrawCtx->fontTextureImage, &memoryRequirements);

    VkMemoryAllocateInfo finalAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = pl__find_memory_type(vulkanDrawCtx->memProps, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    PL_VULKAN(vkAllocateMemory(vulkanDrawCtx->device, &finalAllocInfo, NULL, &vulkanDrawCtx->fontTextureMemory));
    PL_VULKAN(vkBindImageMemory(vulkanDrawCtx->device, vulkanDrawCtx->fontTextureImage, vulkanDrawCtx->fontTextureMemory, 0));

    // upload data
    uint32_t dataSize = atlas->atlasSize[0] * atlas->atlasSize[1] * 4u;
    if(dataSize > vulkanDrawCtx->stageByteSize)
    {
        if(vulkanDrawCtx->stagingMemory)
        {
            vkUnmapMemory(vulkanDrawCtx->device, vulkanDrawCtx->stagingMemory);
            vkDestroyBuffer(vulkanDrawCtx->device, vulkanDrawCtx->stagingBuffer, NULL);
            vkFreeMemory(vulkanDrawCtx->device, vulkanDrawCtx->stagingMemory, NULL);
        }

        // double staging buffer size
        VkBufferCreateInfo stagingBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = dataSize * 2,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        PL_VULKAN(vkCreateBuffer(vulkanDrawCtx->device, &stagingBufferInfo, NULL, &vulkanDrawCtx->stagingBuffer));

        VkMemoryRequirements stagingMemoryRequirements = {0};
        vkGetBufferMemoryRequirements(vulkanDrawCtx->device, vulkanDrawCtx->stagingBuffer, &stagingMemoryRequirements);
        vulkanDrawCtx->stageByteSize = stagingMemoryRequirements.size;

        VkMemoryAllocateInfo stagingAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = stagingMemoryRequirements.size,
            .memoryTypeIndex = pl__find_memory_type(vulkanDrawCtx->memProps, stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };

        PL_VULKAN(vkAllocateMemory(vulkanDrawCtx->device, &stagingAllocInfo, NULL, &vulkanDrawCtx->stagingMemory));
        PL_VULKAN(vkBindBufferMemory(vulkanDrawCtx->device, vulkanDrawCtx->stagingBuffer, vulkanDrawCtx->stagingMemory, 0));   
        PL_VULKAN(vkMapMemory(vulkanDrawCtx->device, vulkanDrawCtx->stagingMemory, 0, VK_WHOLE_SIZE, 0, &vulkanDrawCtx->mapping));
    }
    memcpy(vulkanDrawCtx->mapping, atlas->pixelsAsRGBA32, dataSize);

    VkMappedMemoryRange range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = vulkanDrawCtx->stagingMemory,
        .size = VK_WHOLE_SIZE
    };
    PL_VULKAN(vkFlushMappedMemoryRanges(vulkanDrawCtx->device, 1, &range));

    VkImageSubresourceRange subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0u,
        .levelCount = 1u,
        .baseArrayLayer = 0u,
        .layerCount = 1u
    };

    VkCommandBufferAllocateInfo allocInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = vulkanDrawCtx->tCmdPool,
        .commandBufferCount = 1u
    };
    VkCommandBuffer commandBuffer = {0};
    PL_VULKAN(vkAllocateCommandBuffers(vulkanDrawCtx->device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    PL_VULKAN(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vulkanDrawCtx->fontTextureImage,
        .subresourceRange = subresourceRange,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier1);

    // copy buffer to image
    VkBufferImageCopy region = {
        .bufferOffset = 0u,
        .bufferRowLength = 0u,
        .bufferImageHeight = 0u,
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0u,
        .imageSubresource.baseArrayLayer = 0u,
        .imageSubresource.layerCount = 1u,
        .imageOffset = {0},
        .imageExtent = {
            .width = atlas->atlasSize[0], 
            .height = atlas->atlasSize[1],
            .depth = 1
        }
    };
    vkCmdCopyBufferToImage(commandBuffer, vulkanDrawCtx->stagingBuffer, vulkanDrawCtx->fontTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);     

    // transition image layout for shader usage
    VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vulkanDrawCtx->fontTextureImage,
        .subresourceRange = subresourceRange,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers = &commandBuffer
    };
    PL_VULKAN(vkEndCommandBuffer(commandBuffer));
    vkQueueSubmit(vulkanDrawCtx->tGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    PL_VULKAN(vkDeviceWaitIdle(vulkanDrawCtx->device));
    vkFreeCommandBuffers(vulkanDrawCtx->device, vulkanDrawCtx->tCmdPool, 1, &commandBuffer);

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vulkanDrawCtx->fontTextureImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange.baseMipLevel = 0u,
        .subresourceRange.levelCount = imageInfo.mipLevels,
        .subresourceRange.baseArrayLayer = 0u,
        .subresourceRange.layerCount = 1u,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
    };
    PL_VULKAN(vkCreateImageView(vulkanDrawCtx->device, &viewInfo, NULL, &vulkanDrawCtx->fontTextureImageView));

    ctx->fontAtlas->texture = pl_add_texture(ctx, vulkanDrawCtx->fontTextureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

//-----------------------------------------------------------------------------
// [SECTION] internal helpers implementation
//-----------------------------------------------------------------------------

static uint32_t
pl__find_memory_type(VkPhysicalDeviceMemoryProperties memProps, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    uint32_t memoryType = 0u;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) 
    {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) 
        {
            memoryType = i;
            break;
        }
    }
    return memoryType;
}

static void
pl__grow_vulkan_vertex_buffer(plDrawList* drawlist, uint32_t vertexBufferSize, uint32_t currentFrameIndex)
{
    plDrawContext* drawCtx = drawlist->ctx;
    plVulkanDrawContext* vulkanDrawCtx = drawCtx->_platformData;
    plVulkanDrawList* vulkanDrawlist = drawlist->_platformData;

    if(vulkanDrawlist->sbVertexBufferMap[currentFrameIndex])
    {
        plBufferReturn returnBuffer = {
            .buffer = vulkanDrawlist->sbVertexBuffer[currentFrameIndex],
            .deviceMemory = vulkanDrawlist->sbVertexMemory[currentFrameIndex],
            .slFreedFrame = (int64_t)(drawCtx->frameCount + PL_MAX_FRAMES_IN_FLIGHT * 2)
        };
        pl_sb_push(vulkanDrawCtx->sbReturnedBuffers, returnBuffer);
        vulkanDrawCtx->bufferDeletionQueueSize++;
        vkUnmapMemory(vulkanDrawCtx->device, vulkanDrawlist->sbVertexMemory[currentFrameIndex]);
    }

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertexBufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    PL_VULKAN(vkCreateBuffer(vulkanDrawCtx->device, &bufferInfo, NULL, &vulkanDrawlist->sbVertexBuffer[currentFrameIndex]));

    VkMemoryRequirements memReqs = {0};
    vkGetBufferMemoryRequirements(vulkanDrawCtx->device, vulkanDrawlist->sbVertexBuffer[currentFrameIndex], &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = pl__find_memory_type(vulkanDrawCtx->memProps, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    vulkanDrawlist->sbVertexByteSize[currentFrameIndex] = (uint32_t)memReqs.size;

    PL_VULKAN(vkAllocateMemory(vulkanDrawCtx->device, &allocInfo, NULL, &vulkanDrawlist->sbVertexMemory[currentFrameIndex]));
    PL_VULKAN(vkBindBufferMemory(vulkanDrawCtx->device, vulkanDrawlist->sbVertexBuffer[currentFrameIndex], vulkanDrawlist->sbVertexMemory[currentFrameIndex], 0));
    PL_VULKAN(vkMapMemory(vulkanDrawCtx->device, vulkanDrawlist->sbVertexMemory[currentFrameIndex], 0, memReqs.size, 0, (void**)&vulkanDrawlist->sbVertexBufferMap[currentFrameIndex]));
}

static void
pl__grow_vulkan_index_buffer(plDrawList* drawlist, uint32_t indexBufferSize, uint32_t currentFrameIndex)
{
    plDrawContext* drawCtx = drawlist->ctx;
    plVulkanDrawContext* vulkanDrawCtx = drawCtx->_platformData;
    plVulkanDrawList* vulkanDrawlist = drawlist->_platformData;

    if(vulkanDrawlist->sbIndexBufferMap[currentFrameIndex])
    {
        plBufferReturn returnBuffer = {
            .buffer = vulkanDrawlist->sbIndexBuffer[currentFrameIndex],
            .deviceMemory = vulkanDrawlist->sbIndexMemory[currentFrameIndex],
            .slFreedFrame = (int64_t)(drawCtx->frameCount + PL_MAX_FRAMES_IN_FLIGHT * 2)
        };
        pl_sb_push(vulkanDrawCtx->sbReturnedBuffers, returnBuffer);
        vulkanDrawCtx->bufferDeletionQueueSize++;
        vkUnmapMemory(vulkanDrawCtx->device, vulkanDrawlist->sbIndexMemory[currentFrameIndex]);
    }

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = indexBufferSize,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    PL_VULKAN(vkCreateBuffer(vulkanDrawCtx->device, &bufferInfo, NULL, &vulkanDrawlist->sbIndexBuffer[currentFrameIndex]));

    VkMemoryRequirements memReqs = {0};
    vkGetBufferMemoryRequirements(vulkanDrawCtx->device, vulkanDrawlist->sbIndexBuffer[currentFrameIndex], &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = pl__find_memory_type(vulkanDrawCtx->memProps, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    vulkanDrawlist->sbIndexByteSize[currentFrameIndex] = (uint32_t)memReqs.size;

    PL_VULKAN(vkAllocateMemory(vulkanDrawCtx->device, &allocInfo, NULL, &vulkanDrawlist->sbIndexMemory[currentFrameIndex]));
    PL_VULKAN(vkBindBufferMemory(vulkanDrawCtx->device, vulkanDrawlist->sbIndexBuffer[currentFrameIndex], vulkanDrawlist->sbIndexMemory[currentFrameIndex], 0));
    PL_VULKAN(vkMapMemory(vulkanDrawCtx->device, vulkanDrawlist->sbIndexMemory[currentFrameIndex], 0, memReqs.size, 0, (void**)&vulkanDrawlist->sbIndexBufferMap[currentFrameIndex]));
}

#endif // PL_DRAW_VULKAN_IMPLEMENTATION