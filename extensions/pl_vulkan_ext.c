/*
   pl_vulkan_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] shaders
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] drawing
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_os.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_string.h"
#include "pl_graphics_ext.c"

// vulkan stuff
#if defined(_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
    #define VK_USE_PLATFORM_METAL_EXT
#else // linux
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#define PL_DEVICE_ALLOCATION_BLOCK_SIZE 268435456
#define PL_DEVICE_LOCAL_LEVELS 8

#include "pl_ui.h"
#include "pl_ui_vulkan.h"
#include "vulkan/vulkan.h"

#ifdef _WIN32
#pragma comment(lib, "vulkan-1.lib")
#endif

#ifndef PL_VULKAN
    #include <assert.h>
    #define PL_VULKAN(x) assert(x == VK_SUCCESS)
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

const plFileApiI* gptFile = NULL;
static uint32_t uLogChannel = UINT32_MAX;

//-----------------------------------------------------------------------------
// [SECTION] shaders
//-----------------------------------------------------------------------------

/*
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(push_constant) uniform uPushConstant { mat4 tMVP; } pc;
out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;

void main()
{
    Out.Color = aColor;
    gl_Position = pc.tMVP * vec4(aPos, 1.0);
}
*/
static uint32_t __glsl_shader_vert_3d_spv[] =
{
	0x07230203,0x00010000,0x0008000b,0x00000027,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0009000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000a,0x0000000e,0x00000014,
	0x0000001e,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,0x00000000,
	0x00030005,0x00000008,0x00000000,0x00050006,0x00000008,0x00000000,0x6f6c6f43,0x00000072,
	0x00030005,0x0000000a,0x0074754f,0x00040005,0x0000000e,0x6c6f4361,0x0000726f,0x00060005,
	0x00000012,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000012,0x00000000,
	0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x00000014,0x00000000,0x00060005,0x00000016,
	0x73755075,0x6e6f4368,0x6e617473,0x00000074,0x00050006,0x00000016,0x00000000,0x50564d74,
	0x00000000,0x00030005,0x00000018,0x00006370,0x00040005,0x0000001e,0x736f5061,0x00000000,
	0x00040047,0x0000000a,0x0000001e,0x00000000,0x00040047,0x0000000e,0x0000001e,0x00000001,
	0x00050048,0x00000012,0x00000000,0x0000000b,0x00000000,0x00030047,0x00000012,0x00000002,
	0x00040048,0x00000016,0x00000000,0x00000005,0x00050048,0x00000016,0x00000000,0x00000023,
	0x00000000,0x00050048,0x00000016,0x00000000,0x00000007,0x00000010,0x00030047,0x00000016,
	0x00000002,0x00040047,0x0000001e,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,
	0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
	0x00000004,0x0003001e,0x00000008,0x00000007,0x00040020,0x00000009,0x00000003,0x00000008,
	0x0004003b,0x00000009,0x0000000a,0x00000003,0x00040015,0x0000000b,0x00000020,0x00000001,
	0x0004002b,0x0000000b,0x0000000c,0x00000000,0x00040020,0x0000000d,0x00000001,0x00000007,
	0x0004003b,0x0000000d,0x0000000e,0x00000001,0x00040020,0x00000010,0x00000003,0x00000007,
	0x0003001e,0x00000012,0x00000007,0x00040020,0x00000013,0x00000003,0x00000012,0x0004003b,
	0x00000013,0x00000014,0x00000003,0x00040018,0x00000015,0x00000007,0x00000004,0x0003001e,
	0x00000016,0x00000015,0x00040020,0x00000017,0x00000009,0x00000016,0x0004003b,0x00000017,
	0x00000018,0x00000009,0x00040020,0x00000019,0x00000009,0x00000015,0x00040017,0x0000001c,
	0x00000006,0x00000003,0x00040020,0x0000001d,0x00000001,0x0000001c,0x0004003b,0x0000001d,
	0x0000001e,0x00000001,0x0004002b,0x00000006,0x00000020,0x3f800000,0x00050036,0x00000002,
	0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000007,0x0000000f,
	0x0000000e,0x00050041,0x00000010,0x00000011,0x0000000a,0x0000000c,0x0003003e,0x00000011,
	0x0000000f,0x00050041,0x00000019,0x0000001a,0x00000018,0x0000000c,0x0004003d,0x00000015,
	0x0000001b,0x0000001a,0x0004003d,0x0000001c,0x0000001f,0x0000001e,0x00050051,0x00000006,
	0x00000021,0x0000001f,0x00000000,0x00050051,0x00000006,0x00000022,0x0000001f,0x00000001,
	0x00050051,0x00000006,0x00000023,0x0000001f,0x00000002,0x00070050,0x00000007,0x00000024,
	0x00000021,0x00000022,0x00000023,0x00000020,0x00050091,0x00000007,0x00000025,0x0000001b,
	0x00000024,0x00050041,0x00000010,0x00000026,0x00000014,0x0000000c,0x0003003e,0x00000026,
	0x00000025,0x000100fd,0x00010038
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

static uint32_t __glsl_shader_frag_3d_spv[] =
{
	0x07230203,0x00010000,0x0008000b,0x00000012,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000c,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00040005,0x00000009,0x6c6f4366,0x0000726f,0x00030005,0x0000000a,0x00000000,
	0x00050006,0x0000000a,0x00000000,0x6f6c6f43,0x00000072,0x00030005,0x0000000c,0x00006e49,
	0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000c,0x0000001e,0x00000000,
	0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
	0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,
	0x0004003b,0x00000008,0x00000009,0x00000003,0x0003001e,0x0000000a,0x00000007,0x00040020,
	0x0000000b,0x00000001,0x0000000a,0x0004003b,0x0000000b,0x0000000c,0x00000001,0x00040015,
	0x0000000d,0x00000020,0x00000001,0x0004002b,0x0000000d,0x0000000e,0x00000000,0x00040020,
	0x0000000f,0x00000001,0x00000007,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,
	0x000200f8,0x00000005,0x00050041,0x0000000f,0x00000010,0x0000000c,0x0000000e,0x0004003d,
	0x00000007,0x00000011,0x00000010,0x0003003e,0x00000009,0x00000011,0x000100fd,0x00010038
};

/*
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aInfo;
layout(location = 2) in vec3 aPosOther;
layout(location = 3) in vec4 aColor;
layout(push_constant) uniform uPushConstant { mat4 tMVP; float fAspect; } pc;
out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; } Out;

void main()
{
    Out.Color = aColor;

    // clip space
    vec4 tCurrentProj = pc.tMVP * vec4(aPos.xyz, 1.0);
    vec4 tOtherProj   = pc.tMVP * vec4(aPosOther.xyz, 1.0);

    // NDC space
    vec2 tCurrentNDC = tCurrentProj.xy / tCurrentProj.w;
    vec2 tOtherNDC = tOtherProj.xy / tOtherProj.w;

    // correct for aspect
    tCurrentNDC.x *= pc.fAspect;
    tOtherNDC.x *= pc.fAspect;

    // normal of line (B - A)
    vec2 dir = aInfo.z * normalize(tOtherNDC - tCurrentNDC);
    vec2 normal = vec2(-dir.y, dir.x);

    // extrude from center & correct aspect ratio
    normal *= aInfo.y / 2.0;
    normal.x /= pc.fAspect;

    // offset by the direction of this point in the pair (-1 or 1)
    vec4 offset = vec4(normal* aInfo.x, 0.0, 0.0);
    gl_Position = tCurrentProj + offset;
}
*/

static uint32_t __glsl_shader_vert_3d_line_spv[] =
{
	0x07230203,0x00010000,0x0008000b,0x00000080,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x000b000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000a,0x0000000e,0x0000001d,
	0x00000028,0x00000052,0x0000007b,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,
	0x6e69616d,0x00000000,0x00030005,0x00000008,0x00000000,0x00050006,0x00000008,0x00000000,
	0x6f6c6f43,0x00000072,0x00030005,0x0000000a,0x0074754f,0x00040005,0x0000000e,0x6c6f4361,
	0x0000726f,0x00060005,0x00000013,0x72754374,0x746e6572,0x6a6f7250,0x00000000,0x00060005,
	0x00000015,0x73755075,0x6e6f4368,0x6e617473,0x00000074,0x00050006,0x00000015,0x00000000,
	0x50564d74,0x00000000,0x00050006,0x00000015,0x00000001,0x70734166,0x00746365,0x00030005,
	0x00000017,0x00006370,0x00040005,0x0000001d,0x736f5061,0x00000000,0x00050005,0x00000025,
	0x68744f74,0x72507265,0x00006a6f,0x00050005,0x00000028,0x736f5061,0x6568744f,0x00000072,
	0x00050005,0x00000031,0x72754374,0x746e6572,0x0043444e,0x00050005,0x0000003b,0x68744f74,
	0x444e7265,0x00000043,0x00030005,0x00000051,0x00726964,0x00040005,0x00000052,0x666e4961,
	0x0000006f,0x00040005,0x0000005c,0x6d726f6e,0x00006c61,0x00040005,0x00000070,0x7366666f,
	0x00007465,0x00060005,0x00000079,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,
	0x00000079,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x0000007b,0x00000000,
	0x00040047,0x0000000a,0x0000001e,0x00000000,0x00040047,0x0000000e,0x0000001e,0x00000003,
	0x00040048,0x00000015,0x00000000,0x00000005,0x00050048,0x00000015,0x00000000,0x00000023,
	0x00000000,0x00050048,0x00000015,0x00000000,0x00000007,0x00000010,0x00050048,0x00000015,
	0x00000001,0x00000023,0x00000040,0x00030047,0x00000015,0x00000002,0x00040047,0x0000001d,
	0x0000001e,0x00000000,0x00040047,0x00000028,0x0000001e,0x00000002,0x00040047,0x00000052,
	0x0000001e,0x00000001,0x00050048,0x00000079,0x00000000,0x0000000b,0x00000000,0x00030047,
	0x00000079,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,
	0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x0003001e,0x00000008,
	0x00000007,0x00040020,0x00000009,0x00000003,0x00000008,0x0004003b,0x00000009,0x0000000a,
	0x00000003,0x00040015,0x0000000b,0x00000020,0x00000001,0x0004002b,0x0000000b,0x0000000c,
	0x00000000,0x00040020,0x0000000d,0x00000001,0x00000007,0x0004003b,0x0000000d,0x0000000e,
	0x00000001,0x00040020,0x00000010,0x00000003,0x00000007,0x00040020,0x00000012,0x00000007,
	0x00000007,0x00040018,0x00000014,0x00000007,0x00000004,0x0004001e,0x00000015,0x00000014,
	0x00000006,0x00040020,0x00000016,0x00000009,0x00000015,0x0004003b,0x00000016,0x00000017,
	0x00000009,0x00040020,0x00000018,0x00000009,0x00000014,0x00040017,0x0000001b,0x00000006,
	0x00000003,0x00040020,0x0000001c,0x00000001,0x0000001b,0x0004003b,0x0000001c,0x0000001d,
	0x00000001,0x0004002b,0x00000006,0x0000001f,0x3f800000,0x0004003b,0x0000001c,0x00000028,
	0x00000001,0x00040017,0x0000002f,0x00000006,0x00000002,0x00040020,0x00000030,0x00000007,
	0x0000002f,0x00040015,0x00000034,0x00000020,0x00000000,0x0004002b,0x00000034,0x00000035,
	0x00000003,0x00040020,0x00000036,0x00000007,0x00000006,0x0004002b,0x0000000b,0x00000042,
	0x00000001,0x00040020,0x00000043,0x00000009,0x00000006,0x0004002b,0x00000034,0x00000046,
	0x00000000,0x0004003b,0x0000001c,0x00000052,0x00000001,0x0004002b,0x00000034,0x00000053,
	0x00000002,0x00040020,0x00000054,0x00000001,0x00000006,0x0004002b,0x00000034,0x0000005d,
	0x00000001,0x0004002b,0x00000006,0x00000066,0x40000000,0x0004002b,0x00000006,0x00000075,
	0x00000000,0x0003001e,0x00000079,0x00000007,0x00040020,0x0000007a,0x00000003,0x00000079,
	0x0004003b,0x0000007a,0x0000007b,0x00000003,0x00050036,0x00000002,0x00000004,0x00000000,
	0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000012,0x00000013,0x00000007,0x0004003b,
	0x00000012,0x00000025,0x00000007,0x0004003b,0x00000030,0x00000031,0x00000007,0x0004003b,
	0x00000030,0x0000003b,0x00000007,0x0004003b,0x00000030,0x00000051,0x00000007,0x0004003b,
	0x00000030,0x0000005c,0x00000007,0x0004003b,0x00000012,0x00000070,0x00000007,0x0004003d,
	0x00000007,0x0000000f,0x0000000e,0x00050041,0x00000010,0x00000011,0x0000000a,0x0000000c,
	0x0003003e,0x00000011,0x0000000f,0x00050041,0x00000018,0x00000019,0x00000017,0x0000000c,
	0x0004003d,0x00000014,0x0000001a,0x00000019,0x0004003d,0x0000001b,0x0000001e,0x0000001d,
	0x00050051,0x00000006,0x00000020,0x0000001e,0x00000000,0x00050051,0x00000006,0x00000021,
	0x0000001e,0x00000001,0x00050051,0x00000006,0x00000022,0x0000001e,0x00000002,0x00070050,
	0x00000007,0x00000023,0x00000020,0x00000021,0x00000022,0x0000001f,0x00050091,0x00000007,
	0x00000024,0x0000001a,0x00000023,0x0003003e,0x00000013,0x00000024,0x00050041,0x00000018,
	0x00000026,0x00000017,0x0000000c,0x0004003d,0x00000014,0x00000027,0x00000026,0x0004003d,
	0x0000001b,0x00000029,0x00000028,0x00050051,0x00000006,0x0000002a,0x00000029,0x00000000,
	0x00050051,0x00000006,0x0000002b,0x00000029,0x00000001,0x00050051,0x00000006,0x0000002c,
	0x00000029,0x00000002,0x00070050,0x00000007,0x0000002d,0x0000002a,0x0000002b,0x0000002c,
	0x0000001f,0x00050091,0x00000007,0x0000002e,0x00000027,0x0000002d,0x0003003e,0x00000025,
	0x0000002e,0x0004003d,0x00000007,0x00000032,0x00000013,0x0007004f,0x0000002f,0x00000033,
	0x00000032,0x00000032,0x00000000,0x00000001,0x00050041,0x00000036,0x00000037,0x00000013,
	0x00000035,0x0004003d,0x00000006,0x00000038,0x00000037,0x00050050,0x0000002f,0x00000039,
	0x00000038,0x00000038,0x00050088,0x0000002f,0x0000003a,0x00000033,0x00000039,0x0003003e,
	0x00000031,0x0000003a,0x0004003d,0x00000007,0x0000003c,0x00000025,0x0007004f,0x0000002f,
	0x0000003d,0x0000003c,0x0000003c,0x00000000,0x00000001,0x00050041,0x00000036,0x0000003e,
	0x00000025,0x00000035,0x0004003d,0x00000006,0x0000003f,0x0000003e,0x00050050,0x0000002f,
	0x00000040,0x0000003f,0x0000003f,0x00050088,0x0000002f,0x00000041,0x0000003d,0x00000040,
	0x0003003e,0x0000003b,0x00000041,0x00050041,0x00000043,0x00000044,0x00000017,0x00000042,
	0x0004003d,0x00000006,0x00000045,0x00000044,0x00050041,0x00000036,0x00000047,0x00000031,
	0x00000046,0x0004003d,0x00000006,0x00000048,0x00000047,0x00050085,0x00000006,0x00000049,
	0x00000048,0x00000045,0x00050041,0x00000036,0x0000004a,0x00000031,0x00000046,0x0003003e,
	0x0000004a,0x00000049,0x00050041,0x00000043,0x0000004b,0x00000017,0x00000042,0x0004003d,
	0x00000006,0x0000004c,0x0000004b,0x00050041,0x00000036,0x0000004d,0x0000003b,0x00000046,
	0x0004003d,0x00000006,0x0000004e,0x0000004d,0x00050085,0x00000006,0x0000004f,0x0000004e,
	0x0000004c,0x00050041,0x00000036,0x00000050,0x0000003b,0x00000046,0x0003003e,0x00000050,
	0x0000004f,0x00050041,0x00000054,0x00000055,0x00000052,0x00000053,0x0004003d,0x00000006,
	0x00000056,0x00000055,0x0004003d,0x0000002f,0x00000057,0x0000003b,0x0004003d,0x0000002f,
	0x00000058,0x00000031,0x00050083,0x0000002f,0x00000059,0x00000057,0x00000058,0x0006000c,
	0x0000002f,0x0000005a,0x00000001,0x00000045,0x00000059,0x0005008e,0x0000002f,0x0000005b,
	0x0000005a,0x00000056,0x0003003e,0x00000051,0x0000005b,0x00050041,0x00000036,0x0000005e,
	0x00000051,0x0000005d,0x0004003d,0x00000006,0x0000005f,0x0000005e,0x0004007f,0x00000006,
	0x00000060,0x0000005f,0x00050041,0x00000036,0x00000061,0x00000051,0x00000046,0x0004003d,
	0x00000006,0x00000062,0x00000061,0x00050050,0x0000002f,0x00000063,0x00000060,0x00000062,
	0x0003003e,0x0000005c,0x00000063,0x00050041,0x00000054,0x00000064,0x00000052,0x0000005d,
	0x0004003d,0x00000006,0x00000065,0x00000064,0x00050088,0x00000006,0x00000067,0x00000065,
	0x00000066,0x0004003d,0x0000002f,0x00000068,0x0000005c,0x0005008e,0x0000002f,0x00000069,
	0x00000068,0x00000067,0x0003003e,0x0000005c,0x00000069,0x00050041,0x00000043,0x0000006a,
	0x00000017,0x00000042,0x0004003d,0x00000006,0x0000006b,0x0000006a,0x00050041,0x00000036,
	0x0000006c,0x0000005c,0x00000046,0x0004003d,0x00000006,0x0000006d,0x0000006c,0x00050088,
	0x00000006,0x0000006e,0x0000006d,0x0000006b,0x00050041,0x00000036,0x0000006f,0x0000005c,
	0x00000046,0x0003003e,0x0000006f,0x0000006e,0x0004003d,0x0000002f,0x00000071,0x0000005c,
	0x00050041,0x00000054,0x00000072,0x00000052,0x00000046,0x0004003d,0x00000006,0x00000073,
	0x00000072,0x0005008e,0x0000002f,0x00000074,0x00000071,0x00000073,0x00050051,0x00000006,
	0x00000076,0x00000074,0x00000000,0x00050051,0x00000006,0x00000077,0x00000074,0x00000001,
	0x00070050,0x00000007,0x00000078,0x00000076,0x00000077,0x00000075,0x00000075,0x0003003e,
	0x00000070,0x00000078,0x0004003d,0x00000007,0x0000007c,0x00000013,0x0004003d,0x00000007,
	0x0000007d,0x00000070,0x00050081,0x00000007,0x0000007e,0x0000007c,0x0000007d,0x00050041,
	0x00000010,0x0000007f,0x0000007b,0x0000000c,0x0003003e,0x0000007f,0x0000007e,0x000100fd,
	0x00010038

};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _pl3DBufferReturn
{
    VkBuffer       tBuffer;
    VkDeviceMemory tDeviceMemory;
    int64_t        slFreedFrame;
} pl3DBufferReturn;

typedef struct _pl3DVulkanPipelineEntry
{    
    VkRenderPass          tRenderPass;
    VkSampleCountFlagBits tMSAASampleCount;
    VkPipeline            tRegularPipeline;
    VkPipeline            tSecondaryPipeline;
    pl3DDrawFlags         tFlags;
} pl3DVulkanPipelineEntry;

typedef struct _pl3DVulkanBufferInfo
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
} pl3DVulkanBufferInfo;

typedef struct _plVulkanBuffer
{
    VkBuffer       tBuffer;
    VkDeviceMemory tMemory;
} plVulkanBuffer;

typedef struct _plFrameGarbage
{
    VkImage*        sbtTextures;
    VkImageView*    sbtTextureViews;
    VkFramebuffer*  sbtFrameBuffers;
    VkDeviceMemory* sbtMemory;
} plFrameGarbage;

typedef struct _plFrameContext
{
    VkSemaphore     tImageAvailable;
    VkSemaphore     tRenderFinish;
    VkFence         tInFlight;
    VkCommandPool   tCmdPool;
    VkCommandBuffer tCmdBuf;
} plFrameContext;

typedef struct _plVulkanSwapchain
{
    VkSwapchainKHR           tSwapChain;
    VkExtent2D               tExtent;
    VkFramebuffer*           sbtFrameBuffers;
    VkFormat                 tFormat;
    VkFormat                 tDepthFormat;
    uint32_t                 uImageCount;
    VkImage*                 sbtImages;
    VkImageView*             sbtImageViews;
    VkImage                  tColorTexture;
    VkDeviceMemory           tColorTextureMemory;
    VkImageView              tColorTextureView;
    VkImage                  tDepthTexture;
    VkDeviceMemory           tDepthTextureMemory;
    VkImageView              tDepthTextureView;
    uint32_t                 uCurrentImageIndex; // current image to use within the swap chain
    bool                     bVSync;
    VkSampleCountFlagBits    tMsaaSamples;
    VkSurfaceFormatKHR*      sbtSurfaceFormats;

} plVulkanSwapchain;

typedef struct _plVulkanDevice
{
    VkDevice                                  tLogicalDevice;
    VkPhysicalDevice                          tPhysicalDevice;
    int                                       iGraphicsQueueFamily;
    int                                       iPresentQueueFamily;
    VkQueue                                   tGraphicsQueue;
    VkQueue                                   tPresentQueue;
    VkPhysicalDeviceProperties                tDeviceProps;
    VkPhysicalDeviceMemoryProperties          tMemProps;
    VkPhysicalDeviceMemoryProperties2         tMemProps2;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT tMemBudgetInfo;
    VkDeviceSize                              tMaxLocalMemSize;
    VkPhysicalDeviceFeatures                  tDeviceFeatures;
    bool                                      bSwapchainExtPresent;
    bool                                      bPortabilitySubsetPresent;
    VkCommandPool                             tCmdPool;
    uint32_t                                  uUniformBufferBlockSize;
    uint32_t                                  uCurrentFrame;

	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert;

    // [INTERNAL]
    plFrameGarbage* _sbtFrameGarbage;

} plVulkanDevice;

typedef struct _plVulkanGraphics
{
    VkInstance               tInstance;
    VkDebugUtilsMessengerEXT tDbgMessenger;
    VkSurfaceKHR             tSurface;
    plFrameContext*          sbFrames;
    VkRenderPass             tRenderPass;
    uint32_t                 uFramesInFlight;
    size_t                   szCurrentFrameIndex; // current frame being used
    VkDescriptorPool         tDescriptorPool;
    plVulkanSwapchain        tSwapchain;


    VkPipelineLayout                  g_pipelineLayout;
    VkPipeline                        g_pipeline;
    VkVertexInputAttributeDescription g_attributeDescriptions[2];
    VkVertexInputBindingDescription   g_bindingDescriptions[1];
    VkShaderModule                    g_vertexShaderModule;
    VkShaderModule                    g_pixelShaderModule;

    // drawing

    // committed buffers
    pl3DBufferReturn*                  sbReturnedBuffers;
    pl3DBufferReturn*                  sbReturnedBuffersTemp;
    uint32_t                           uBufferDeletionQueueSize;

    // vertex & index buffer
    pl3DVulkanBufferInfo*              sbt3DBufferInfo;
    pl3DVulkanBufferInfo*              sbtLineBufferInfo;

    // staging buffer
    size_t                            szStageByteSize;
    VkBuffer                          tStagingBuffer;
    VkDeviceMemory                    tStagingMemory;
    void*                             pStageMapping; // persistent mapping for staging buffer

    // 3D drawlist pipeline caching
    VkPipelineLayout                  t3DPipelineLayout;
    VkPipelineShaderStageCreateInfo   t3DPxlShdrStgInfo;
    VkPipelineShaderStageCreateInfo   t3DVtxShdrStgInfo;

    // 3D line drawlist pipeline caching
    VkPipelineLayout                  t3DLinePipelineLayout;
    VkPipelineShaderStageCreateInfo   t3DLineVtxShdrStgInfo;

    // pipelines
    pl3DVulkanPipelineEntry*            sbt3DPipelines;
} plVulkanGraphics;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// 3D drawing
static void                   pl__grow_vulkan_3d_vertex_buffer   (plGraphics* ptGraphics, uint32_t uVtxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo);
static void                   pl__grow_vulkan_3d_index_buffer    (plGraphics* ptGraphics, uint32_t uIdxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo);
static pl3DVulkanPipelineEntry* pl__get_3d_pipelines            (plGraphics* ptGfx, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount, pl3DDrawFlags tFlags);

static void pl__submit_3d_drawlist(plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags);

static plFrameContext*
pl_get_frame_resources(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx    = ptGraphics->_pInternalData;
    return &ptVulkanGfx->sbFrames[ptVulkanGfx->szCurrentFrameIndex];
}

static VkDeviceMemory
allocate_dedicated(plDevice* ptDevice, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    uint32_t uMemoryType = 0u;
    bool bFound = false;
    for (uint32_t i = 0; i < ptVulkanDevice->tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (ptVulkanDevice->tMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
        {
            uMemoryType = i;
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);

    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = (uint32_t)ulSize,
        .memoryTypeIndex = uMemoryType
    };

    VkDeviceMemory tMemory = VK_NULL_HANDLE;
    VkResult tResult = vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &tMemory);
    PL_VULKAN(tResult);

    return tMemory;
}

static VkSampleCountFlagBits
get_max_sample_count(plDevice* ptDevice)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    VkPhysicalDeviceProperties tPhysicalDeviceProperties = {0};
    vkGetPhysicalDeviceProperties(ptVulkanDevice->tPhysicalDevice, &tPhysicalDeviceProperties);

    VkSampleCountFlags tCounts = tPhysicalDeviceProperties.limits.framebufferColorSampleCounts & tPhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (tCounts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT; }
    if (tCounts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT; }
    return VK_SAMPLE_COUNT_1_BIT;    
}

static VkFormat
find_supported_format(plDevice* ptDevice, VkFormatFeatureFlags tFlags, const VkFormat* ptFormats, uint32_t uFormatCount)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    for(uint32_t i = 0u; i < uFormatCount; i++)
    {
        VkFormatProperties tProps = {0};
        vkGetPhysicalDeviceFormatProperties(ptVulkanDevice->tPhysicalDevice, ptFormats[i], &tProps);
        if(tProps.optimalTilingFeatures & tFlags)
            return ptFormats[i];
    }

    PL_ASSERT(false && "no supported format found");
    return VK_FORMAT_UNDEFINED;
}

static VkFormat
find_depth_format(plDevice* ptDevice)
{
    const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 3);
}

static VkFormat
find_depth_stencil_format(plDevice* ptDevice)
{
     const VkFormat atFormats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    return find_supported_format(ptDevice, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, atFormats, 2);   
}

static bool
format_has_stencil(VkFormat tFormat)
{
    switch(tFormat)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
        case VK_FORMAT_D32_SFLOAT:
        default: return false;
    }
}

static void
transition_image_layout(VkCommandBuffer tCommandBuffer, VkImage tImage, VkImageLayout tOldLayout, VkImageLayout tNewLayout, VkImageSubresourceRange tSubresourceRange, VkPipelineStageFlags tSrcStageMask, VkPipelineStageFlags tDstStageMask)
{
    //VkCommandBuffer commandBuffer = mvBeginSingleTimeCommands();
    VkImageMemoryBarrier tBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = tOldLayout,
        .newLayout           = tNewLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = tImage,
        .subresourceRange    = tSubresourceRange,
    };

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (tOldLayout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            tBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            tBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            tBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            tBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (tNewLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            tBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            tBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            tBarrier.dstAccessMask = tBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (tBarrier.srcAccessMask == 0)
                tBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            tBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
    }
    vkCmdPipelineBarrier(tCommandBuffer, tSrcStageMask, tDstStageMask, 0, 0, NULL, 0, NULL, 1, &tBarrier);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
pl__debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT tMsgSeverity, VkDebugUtilsMessageTypeFlagsEXT tMsgType, const VkDebugUtilsMessengerCallbackDataEXT* ptCallbackData, void* pUserData) 
{
    if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("error validation layer: %s\n", ptCallbackData->pMessage);
        pl_log_error_to_f(uLogChannel, "error validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf("warn validation layer: %s\n", ptCallbackData->pMessage);
        pl_log_warn_to_f(uLogChannel, "warn validation layer: %s\n", ptCallbackData->pMessage);
    }

    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        // pl_log_trace_to_f(uLogChannel, "info validation layer: %s\n", ptCallbackData->pMessage);
    }
    else if(tMsgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        // pl_log_trace_to_f(uLogChannel, "trace validation layer: %s\n", ptCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

static void
create_swapchain(plGraphics* ptGraphics, uint32_t uWidth, uint32_t uHeight, plVulkanSwapchain* ptSwapchainOut)
{
    plVulkanGraphics* ptVulkanGfx    = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);

    ptSwapchainOut->tMsaaSamples = get_max_sample_count(&ptGraphics->tDevice);

    // query swapchain support

    VkSurfaceCapabilitiesKHR tCapabilities = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &tCapabilities));

    uint32_t uFormatCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uFormatCount, NULL));
    
    pl_sb_resize(ptSwapchainOut->sbtSurfaceFormats, uFormatCount);

    VkBool32 tPresentSupport = false;
    PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptVulkanDevice->tPhysicalDevice, 0, ptVulkanGfx->tSurface, &tPresentSupport));
    PL_ASSERT(uFormatCount > 0);
    PL_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uFormatCount, ptSwapchainOut->sbtSurfaceFormats));

    uint32_t uPresentModeCount = 0u;
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uPresentModeCount, NULL));
    PL_ASSERT(uPresentModeCount > 0 && uPresentModeCount < 16);

    VkPresentModeKHR atPresentModes[16] = {0};
    PL_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(ptVulkanDevice->tPhysicalDevice, ptVulkanGfx->tSurface, &uPresentModeCount, atPresentModes));

    // choose swap tSurface Format
    static VkFormat atSurfaceFormatPreference[4] = 
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

    bool bPreferenceFound = false;
    VkSurfaceFormatKHR tSurfaceFormat = ptSwapchainOut->sbtSurfaceFormats[0];
    ptSwapchainOut->tFormat = tSurfaceFormat.format;

    for(uint32_t i = 0u; i < 4; i++)
    {
        if(bPreferenceFound) break;
        
        for(uint32_t j = 0u; j < uFormatCount; j++)
        {
            if(ptSwapchainOut->sbtSurfaceFormats[j].format == atSurfaceFormatPreference[i] && ptSwapchainOut->sbtSurfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                tSurfaceFormat = ptSwapchainOut->sbtSurfaceFormats[j];
                ptSwapchainOut->tFormat = tSurfaceFormat.format;
                bPreferenceFound = true;
                break;
            }
        }
    }
    PL_ASSERT(bPreferenceFound && "no preferred surface format found");

    // chose swap present mode
    VkPresentModeKHR tPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!ptSwapchainOut->bVSync)
    {
        for(uint32_t i = 0 ; i < uPresentModeCount; i++)
        {
			if (atPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				tPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if (atPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				tPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // chose swap extent 
    VkExtent2D tExtent = {0};
    if(tCapabilities.currentExtent.width != UINT32_MAX)
        tExtent = tCapabilities.currentExtent;
    else
    {
        tExtent.width = pl_max(tCapabilities.minImageExtent.width, pl_min(tCapabilities.maxImageExtent.width, uWidth));
        tExtent.height = pl_max(tCapabilities.minImageExtent.height, pl_min(tCapabilities.maxImageExtent.height, uHeight));
    }
    ptSwapchainOut->tExtent = tExtent;

    // decide image count
    const uint32_t uOldImageCount = ptSwapchainOut->uImageCount;
    uint32_t uDesiredMinImageCount = tCapabilities.minImageCount + 1;
    if(tCapabilities.maxImageCount > 0 && uDesiredMinImageCount > tCapabilities.maxImageCount) 
        uDesiredMinImageCount = tCapabilities.maxImageCount;

	// find the transformation of the surface
	VkSurfaceTransformFlagsKHR tPreTransform;
	if (tCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		// We prefer a non-rotated transform
		tPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		tPreTransform = tCapabilities.currentTransform;
	}

	// find a supported composite alpha format (not all devices support alpha opaque)
	VkCompositeAlphaFlagBitsKHR tCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	
    // simply select the first composite alpha format available
	static const VkCompositeAlphaFlagBitsKHR atCompositeAlphaFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for (int i = 0; i < 4; i++)
    {
		if (tCapabilities.supportedCompositeAlpha & atCompositeAlphaFlags[i]) 
        {
			tCompositeAlpha = atCompositeAlphaFlags[i];
			break;
		};
	}

    VkSwapchainCreateInfoKHR tCreateSwapchainInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = ptVulkanGfx->tSurface,
        .minImageCount    = uDesiredMinImageCount,
        .imageFormat      = tSurfaceFormat.format,
        .imageColorSpace  = tSurfaceFormat.colorSpace,
        .imageExtent      = tExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = (VkSurfaceTransformFlagBitsKHR)tPreTransform,
        .compositeAlpha   = tCompositeAlpha,
        .presentMode      = tPresentMode,
        .clipped          = VK_TRUE, // setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        .oldSwapchain     = ptSwapchainOut->tSwapChain, // setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

	// enable transfer source on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// enable transfer destination on swap chain images if supported
	if (tCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		tCreateSwapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t auQueueFamilyIndices[] = { (uint32_t)ptVulkanDevice->iGraphicsQueueFamily, (uint32_t)ptVulkanDevice->iPresentQueueFamily};
    if (ptVulkanDevice->iGraphicsQueueFamily != ptVulkanDevice->iPresentQueueFamily)
    {
        tCreateSwapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        tCreateSwapchainInfo.queueFamilyIndexCount = 2;
        tCreateSwapchainInfo.pQueueFamilyIndices = auQueueFamilyIndices;
    }

    VkSwapchainKHR tOldSwapChain = ptSwapchainOut->tSwapChain;

    PL_VULKAN(vkCreateSwapchainKHR(ptVulkanDevice->tLogicalDevice, &tCreateSwapchainInfo, NULL, &ptSwapchainOut->tSwapChain));

    if(tOldSwapChain)
    {
        if(pl_sb_size(ptVulkanDevice->_sbtFrameGarbage) == 0)
        {
            pl_sb_resize(ptVulkanDevice->_sbtFrameGarbage, ptSwapchainOut->uImageCount);
        }
        for (uint32_t i = 0u; i < uOldImageCount; i++)
        {
            pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->sbtImageViews[i]);
            pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtFrameBuffers, ptSwapchainOut->sbtFrameBuffers[i]);
        }
        vkDestroySwapchainKHR(ptVulkanDevice->tLogicalDevice, tOldSwapChain, NULL);
    }

    // get swapchain images

    PL_VULKAN(vkGetSwapchainImagesKHR(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, NULL));
    pl_sb_resize(ptSwapchainOut->sbtImages, ptSwapchainOut->uImageCount);
    pl_sb_resize(ptSwapchainOut->sbtImageViews, ptSwapchainOut->uImageCount);

    PL_VULKAN(vkGetSwapchainImagesKHR(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tSwapChain, &ptSwapchainOut->uImageCount, ptSwapchainOut->sbtImages));

    for(uint32_t i = 0; i < ptSwapchainOut->uImageCount; i++)
    {

        ptSwapchainOut->sbtImageViews[i] = VK_NULL_HANDLE;

        VkImageViewCreateInfo tViewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = ptSwapchainOut->sbtImages[i],
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = ptSwapchainOut->tFormat,
            .subresourceRange = {
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
            },
            .components = {
                        VK_COMPONENT_SWIZZLE_R,
                        VK_COMPONENT_SWIZZLE_G,
                        VK_COMPONENT_SWIZZLE_B,
                        VK_COMPONENT_SWIZZLE_A
                    }
        };

        PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tViewInfo, NULL, &ptSwapchainOut->sbtImageViews[i]));
    }  //-V1020

    // color & depth
    if(ptSwapchainOut->tColorTextureView)  pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->tColorTextureView);
    if(ptSwapchainOut->tDepthTextureView)  pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextureViews, ptSwapchainOut->tDepthTextureView);
    if(ptSwapchainOut->tColorTexture)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextures, ptSwapchainOut->tColorTexture);
    if(ptSwapchainOut->tDepthTexture)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtTextures, ptSwapchainOut->tDepthTexture);
    if(ptSwapchainOut->tColorTextureMemory)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtMemory, ptSwapchainOut->tColorTextureMemory);
    if(ptSwapchainOut->tDepthTextureMemory)      pl_sb_push(ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame].sbtMemory, ptSwapchainOut->tDepthTextureMemory);

    ptSwapchainOut->tColorTextureView = VK_NULL_HANDLE;
    ptSwapchainOut->tColorTexture     = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthTextureView = VK_NULL_HANDLE;
    ptSwapchainOut->tDepthTexture     = VK_NULL_HANDLE;

    VkImageCreateInfo tDepthImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = find_depth_stencil_format(&ptGraphics->tDevice),
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    VkImageCreateInfo tColorImageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .extent        = 
        {
            .width  = ptSwapchainOut->tExtent.width,
            .height = ptSwapchainOut->tExtent.height,
            .depth  = 1
        },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .format        = ptSwapchainOut->tFormat,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .samples       = ptSwapchainOut->tMsaaSamples,
        .flags         = 0
    };

    PL_VULKAN(vkCreateImage(ptVulkanDevice->tLogicalDevice, &tDepthImageInfo, NULL, &ptSwapchainOut->tDepthTexture));
    PL_VULKAN(vkCreateImage(ptVulkanDevice->tLogicalDevice, &tColorImageInfo, NULL, &ptSwapchainOut->tColorTexture));

    VkMemoryRequirements tDepthMemReqs = {0};
    VkMemoryRequirements tColorMemReqs = {0};
    vkGetImageMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tDepthTexture, &tDepthMemReqs);
    vkGetImageMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tColorTexture, &tColorMemReqs);


    ptSwapchainOut->tColorTextureMemory = allocate_dedicated(&ptGraphics->tDevice, tColorMemReqs.memoryTypeBits, tColorMemReqs.size, tColorMemReqs.alignment, "swapchain color");
    ptSwapchainOut->tDepthTextureMemory = allocate_dedicated(&ptGraphics->tDevice, tDepthMemReqs.memoryTypeBits, tDepthMemReqs.size, tDepthMemReqs.alignment, "swapchain depth");

    PL_VULKAN(vkBindImageMemory(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tDepthTexture, ptSwapchainOut->tDepthTextureMemory, 0));
    PL_VULKAN(vkBindImageMemory(ptVulkanDevice->tLogicalDevice, ptSwapchainOut->tColorTexture, ptSwapchainOut->tColorTextureMemory, 0));

    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptVulkanDevice->tCmdPool,
        .commandBufferCount = 1u,
    };
    vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCommandBuffer);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo);

    VkImageSubresourceRange tRange = {
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT
    };

    transition_image_layout(tCommandBuffer, ptSwapchainOut->tColorTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    tRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    transition_image_layout(tCommandBuffer, ptSwapchainOut->tDepthTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, tRange, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);

    VkImageViewCreateInfo tDepthViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tDepthTexture,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tDepthImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
    };

    if(format_has_stencil(tDepthViewInfo.format))
        tDepthViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo tColorViewInfo = {
        .sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                           = ptSwapchainOut->tColorTexture,
        .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
        .format                          = tColorImageInfo.format,
        .subresourceRange.baseMipLevel   = 0,
        .subresourceRange.levelCount     = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount     = 1,
        .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tDepthViewInfo, NULL, &ptSwapchainOut->tDepthTextureView));
    PL_VULKAN(vkCreateImageView(ptVulkanDevice->tLogicalDevice, &tColorViewInfo, NULL, &ptSwapchainOut->tColorTextureView));

}

static char*
read_file(const char* file, unsigned* size, const char* mode)
{
    FILE* dataFile = fopen(file, mode);

    if (dataFile == NULL)
    {
        assert(false && "File not found.");
        return NULL;
    }

    // obtain file size:
    fseek(dataFile, 0, SEEK_END);
    *size = ftell(dataFile);
    fseek(dataFile, 0, SEEK_SET);

    // allocate memory to contain the whole file:
    char* data = (char*)malloc(sizeof(char)*(*size));

    // copy the file into the buffer:
    size_t result = fread(data, sizeof(char), *size, dataFile);
    if (result != *size)
    {
        if (feof(dataFile))
            printf("Error reading test.bin: unexpected end of file\n");
        else if (ferror(dataFile)) {
            perror("Error reading test.bin");
        }
        assert(false && "File not read.");
    }

    fclose(dataFile);

    return data;
}

static uint32_t
find_memory_type(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    uint32_t uMemoryType = 0u;
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
        {
            uMemoryType = i;
            break;
        }
    }
    return uMemoryType;    
}

static uint32_t
pl__find_memory_type_(VkPhysicalDeviceMemoryProperties tMemProps, uint32_t uTypeFilter, VkMemoryPropertyFlags tProperties)
{
    for (uint32_t i = 0; i < tMemProps.memoryTypeCount; i++) 
    {
        if ((uTypeFilter & (1 << i)) && (tMemProps.memoryTypes[i].propertyFlags & tProperties) == tProperties) 
            return i;
    }
    return 0;
}

static void
pl__grow_vulkan_3d_vertex_buffer(plGraphics* ptGfx, uint32_t uVtxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo)
{
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    // buffer currently exists & mapped, submit for cleanup
    if(ptBufferInfo->ucVertexBufferMap)
    {
        const pl3DBufferReturn tReturnBuffer = {
            .tBuffer       = ptBufferInfo->tVertexBuffer,
            .tDeviceMemory = ptBufferInfo->tVertexMemory,
            .slFreedFrame  = (int64_t)(pl_get_io()->ulFrameCount + ptVulkanGfx->uFramesInFlight * 2)
        };
        pl_sb_push(ptVulkanGfx->sbReturnedBuffers, tReturnBuffer);
        ptVulkanGfx->uBufferDeletionQueueSize++;
        vkUnmapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexMemory);
    }

    // create new buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = uVtxBufSzNeeded,
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tVertexBuffer));

    // check memory requirements
    VkMemoryRequirements tMemReqs = {0};
    vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, &tMemReqs);

    // allocate memory & bind buffer
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = tMemReqs.size,
        .memoryTypeIndex = pl__find_memory_type_(ptVulkanDevice->tMemProps, tMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    ptBufferInfo->uVertexByteSize = (uint32_t)tMemReqs.size;
    PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &ptBufferInfo->tVertexMemory));
    PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexBuffer, ptBufferInfo->tVertexMemory, 0));

    // map memory persistently
    PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tVertexMemory, 0, tMemReqs.size, 0, (void**)&ptBufferInfo->ucVertexBufferMap));

    ptBufferInfo->uVertexBufferOffset = 0;
}

static void
pl__grow_vulkan_3d_index_buffer(plGraphics* ptGfx, uint32_t uIdxBufSzNeeded, pl3DVulkanBufferInfo* ptBufferInfo)
{
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    // buffer currently exists & mapped, submit for cleanup
    if(ptBufferInfo->ucIndexBufferMap)
    {
        const pl3DBufferReturn tReturnBuffer = {
            .tBuffer       = ptBufferInfo->tIndexBuffer,
            .tDeviceMemory = ptBufferInfo->tIndexMemory,
            .slFreedFrame  = (int64_t)(pl_get_io()->ulFrameCount + ptVulkanGfx->uFramesInFlight * 2)
        };
        pl_sb_push(ptVulkanGfx->sbReturnedBuffers, tReturnBuffer);
        ptVulkanGfx->uBufferDeletionQueueSize++;
        vkUnmapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexMemory);
    }

    // create new buffer
    const VkBufferCreateInfo tBufferCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = uIdxBufSzNeeded,
        .usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &tBufferCreateInfo, NULL, &ptBufferInfo->tIndexBuffer));

    // check memory requirements
    VkMemoryRequirements tMemReqs = {0};
    vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, &tMemReqs);

    // alllocate memory & bind buffer
    const VkMemoryAllocateInfo tAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = tMemReqs.size,
        .memoryTypeIndex = pl__find_memory_type_(ptVulkanDevice->tMemProps, tMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    ptBufferInfo->uIndexByteSize = (uint32_t)tMemReqs.size;
    PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &tAllocInfo, NULL, &ptBufferInfo->tIndexMemory));
    PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexBuffer, ptBufferInfo->tIndexMemory, 0));

    // map memory persistently
    PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, ptBufferInfo->tIndexMemory, 0, tMemReqs.size, 0, (void**)&ptBufferInfo->ucIndexBufferMap));

    ptBufferInfo->uIndexBufferOffset = 0;
}

static pl3DVulkanPipelineEntry*
pl__get_3d_pipelines(plGraphics* ptGfx, VkRenderPass tRenderPass, VkSampleCountFlagBits tMSAASampleCount, pl3DDrawFlags tFlags)
{
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    // return pipeline entry if it exists
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbt3DPipelines); i++)
    {
        if(ptVulkanGfx->sbt3DPipelines[i].tRenderPass == tRenderPass && tMSAASampleCount == ptVulkanGfx->sbt3DPipelines[i].tMSAASampleCount && ptVulkanGfx->sbt3DPipelines[i].tFlags == tFlags)
            return &ptVulkanGfx->sbt3DPipelines[i];
    }

    // create new pipeline entry
    pl3DVulkanPipelineEntry tEntry = {
        .tRenderPass      = tRenderPass,
        .tMSAASampleCount = tMSAASampleCount,
        .tFlags           = tFlags
    };

    const VkPipelineInputAssemblyStateCreateInfo tInputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkVertexInputAttributeDescription aAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 0u},
        {1u, 0u, VK_FORMAT_R8G8B8A8_UNORM,  12u}
    };
    
    const VkVertexInputBindingDescription tBindingDescription = {
        .binding   = 0u,
        .stride    = sizeof(plDrawVertex3DSolid),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    const VkPipelineVertexInputStateCreateInfo tVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1u,
        .vertexAttributeDescriptionCount = 2u,
        .pVertexBindingDescriptions      = &tBindingDescription,
        .pVertexAttributeDescriptions    = aAttributeDescriptions
    };

    const VkVertexInputAttributeDescription aLineAttributeDescriptions[] = {
        {0u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 0u},
        {1u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 12u},
        {2u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 24u},
        {3u, 0u, VK_FORMAT_R8G8B8A8_UNORM,  36u}
    };
    
    const VkVertexInputBindingDescription tLineBindingDescription = {
        .binding   = 0u,
        .stride    = sizeof(plDrawVertex3DLine),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    const VkPipelineVertexInputStateCreateInfo tLineVertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1u,
        .vertexAttributeDescriptionCount = 4u,
        .pVertexBindingDescriptions      = &tLineBindingDescription,
        .pVertexAttributeDescriptions    = aLineAttributeDescriptions
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

    VkCullModeFlags tCullFlags = VK_CULL_MODE_NONE;
    if(tFlags & PL_PIPELINE_FLAG_CULL_FRONT)
        tCullFlags = VK_CULL_MODE_FRONT_BIT;
    else if(tFlags & PL_PIPELINE_FLAG_CULL_BACK)
        tCullFlags = VK_CULL_MODE_BACK_BIT;

    const VkPipelineRasterizationStateCreateInfo tRasterizer = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = tCullFlags,
        .frontFace               = tFlags & PL_PIPELINE_FLAG_FRONT_FACE_CW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE
    };

    const VkPipelineDepthStencilStateCreateInfo tDepthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = tFlags & PL_PIPELINE_FLAG_DEPTH_TEST ? VK_TRUE : VK_FALSE,
        .depthWriteEnable      = tFlags & PL_PIPELINE_FLAG_DEPTH_WRITE ? VK_TRUE : VK_FALSE,
        .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
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
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
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

    VkPipelineShaderStageCreateInfo atShaderStages[] = { ptVulkanGfx->t3DVtxShdrStgInfo, ptVulkanGfx->t3DPxlShdrStgInfo };

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
        .layout              = ptVulkanGfx->t3DPipelineLayout,
        .renderPass          = tRenderPass,
        .subpass             = 0u,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .pDepthStencilState  = &tDepthStencil
    };
    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &tEntry.tRegularPipeline));

    // //---------------------------------------------------------------------
    // // Create SDF Pipeline
    // //---------------------------------------------------------------------

    atShaderStages[0] = ptVulkanGfx->t3DLineVtxShdrStgInfo;
    pipeInfo.pStages = atShaderStages;
    pipeInfo.pVertexInputState = &tLineVertexInputInfo;
    pipeInfo.layout = ptVulkanGfx->t3DLinePipelineLayout;

    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &pipeInfo, NULL, &tEntry.tSecondaryPipeline));

    // add to entries
    pl_sb_push(ptVulkanGfx->sbt3DPipelines, tEntry);

    return &pl_sb_back(ptVulkanGfx->sbt3DPipelines); 
}

static void
pl__submit_3d_drawlist(plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags)
{
    plGraphics* ptGfx = ptDrawlist->ptGraphics;
    plVulkanGraphics* ptVulkanGfx = ptGfx->_pInternalData;
    plVulkanDevice* ptVulkanDevice = ptGfx->tDevice._pInternalData;

    pl3DVulkanPipelineEntry* tPipelineEntry = pl__get_3d_pipelines(ptGfx, ptVulkanGfx->tRenderPass, ptVulkanGfx->tSwapchain.tMsaaSamples, tFlags);
    const float fAspectRatio = fWidth / fHeight;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGfx);

    // regular 3D
    if(pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) > 0u)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);

        pl3DVulkanBufferInfo* ptBufferInfo = &ptVulkanGfx->sbt3DBufferInfo[ptVulkanGfx->szCurrentFrameIndex];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexByteSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
            pl__grow_vulkan_3d_vertex_buffer(ptGfx, uVtxBufSzNeeded * 2, ptBufferInfo);

        // vertex GPU data transfer
        unsigned char* pucMappedVertexBufferLocation = ptBufferInfo->ucVertexBufferMap;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtSolidVertexBuffer, sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = ptBufferInfo->uIndexByteSize - ptBufferInfo->uIndexBufferOffset;

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
            pl__grow_vulkan_3d_index_buffer(ptGfx, uIdxBufSzNeeded * 2, ptBufferInfo);

        // index GPU data transfer
        unsigned char* pucMappedIndexBufferLocation = ptBufferInfo->ucIndexBufferMap;
        memcpy(&pucMappedIndexBufferLocation[ptBufferInfo->uIndexBufferOffset], ptDrawlist->sbtSolidIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer));
        
        const VkMappedMemoryRange aRange[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tVertexMemory,
                .size = VK_WHOLE_SIZE
            },
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tIndexMemory,
                .size = VK_WHOLE_SIZE
            }
        };
        PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDevice->tLogicalDevice, 2, aRange));

        static const VkDeviceSize tOffsets = { 0u };
        vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptBufferInfo->tIndexBuffer, 0u, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptBufferInfo->tVertexBuffer, &tOffsets);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DSolid);
        const int32_t iIndexOffset = ptBufferInfo->uIndexBufferOffset / sizeof(uint32_t);

        vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tPipelineEntry->tRegularPipeline); 
        vkCmdPushConstants(ptCurrentFrame->tCmdBuf, ptVulkanGfx->t3DPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, ptMVP);
        vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, pl_sb_size(ptDrawlist->sbtSolidIndexBuffer), 1, iIndexOffset, iVertexOffset, 0);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        ptBufferInfo->uIndexBufferOffset += uIdxBufSzNeeded;
    }

    // 3D lines
    if(pl_sb_size(ptDrawlist->sbtLineVertexBuffer) > 0u)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer);

        pl3DVulkanBufferInfo* ptBufferInfo = &ptVulkanGfx->sbtLineBufferInfo[ptVulkanGfx->szCurrentFrameIndex];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexByteSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
            pl__grow_vulkan_3d_vertex_buffer(ptGfx, uVtxBufSzNeeded * 2, ptBufferInfo);

        // vertex GPU data transfer
        unsigned char* pucMappedVertexBufferLocation = ptBufferInfo->ucVertexBufferMap;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtLineVertexBuffer, sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = ptBufferInfo->uIndexByteSize - ptBufferInfo->uIndexBufferOffset;

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
            pl__grow_vulkan_3d_index_buffer(ptGfx, uIdxBufSzNeeded * 2, ptBufferInfo);

        // index GPU data transfer
        unsigned char* pucMappedIndexBufferLocation = ptBufferInfo->ucIndexBufferMap;
        memcpy(&pucMappedIndexBufferLocation[ptBufferInfo->uIndexBufferOffset], ptDrawlist->sbtLineIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer));
        
        const VkMappedMemoryRange aRange[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tVertexMemory,
                .size = VK_WHOLE_SIZE
            },
            {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = ptBufferInfo->tIndexMemory,
                .size = VK_WHOLE_SIZE
            }
        };
        PL_VULKAN(vkFlushMappedMemoryRanges(ptVulkanDevice->tLogicalDevice, 2, aRange));

        static const VkDeviceSize tOffsets = { 0u };
        vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptBufferInfo->tIndexBuffer, 0u, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptBufferInfo->tVertexBuffer, &tOffsets);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DLine);
        const int32_t iIndexOffset = ptBufferInfo->uIndexBufferOffset / sizeof(uint32_t);

        vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tPipelineEntry->tSecondaryPipeline); 
        vkCmdPushConstants(ptCurrentFrame->tCmdBuf, ptVulkanGfx->t3DLinePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, ptMVP);
        vkCmdPushConstants(ptCurrentFrame->tCmdBuf, ptVulkanGfx->t3DLinePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 16, sizeof(float), &fAspectRatio);
        vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, pl_sb_size(ptDrawlist->sbtLineIndexBuffer), 1, iIndexOffset, iVertexOffset, 0);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        ptBufferInfo->uIndexBufferOffset += uIdxBufSzNeeded;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static uint32_t
pl_create_index_buffer(plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    const uint32_t uBufferIndex = pl_sb_size(ptDevice->sbtBuffers);

    plVulkanBuffer* ptBuffer = PL_ALLOC(sizeof(plVulkanBuffer));
    memset(ptBuffer, 0, sizeof(plVulkanBuffer));

    plBuffer tBuffer = {
        .pBuffer = ptBuffer
    };
    pl_sb_push(ptDevice->sbtBuffers, tBuffer);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferDeviceMemory;

    { // create staging buffer

        VkBufferCreateInfo bufferInfo = {0};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = szSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &bufferInfo, NULL, &stagingBuffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {0};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(ptVulkanDevice->tMemProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &allocInfo, NULL, &stagingBufferDeviceMemory));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, stagingBuffer, stagingBufferDeviceMemory, 0));

        void* mapping;
        PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, stagingBufferDeviceMemory, 0, bufferInfo.size, 0, &mapping));
        memcpy(mapping, pData, bufferInfo.size);
        vkUnmapMemory(ptVulkanDevice->tLogicalDevice, stagingBufferDeviceMemory);
    }

    { // create final buffer

        VkBufferCreateInfo bufferInfo = {0};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = szSize;
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &bufferInfo, NULL, &ptBuffer->tBuffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBuffer->tBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {0};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(ptVulkanDevice->tMemProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &allocInfo, NULL, &ptBuffer->tMemory));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBuffer->tBuffer, ptBuffer->tMemory, 0));
    }

    // copy buffer
    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptVulkanDevice->tCmdPool,
        .commandBufferCount = 1u,
    };
    vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCommandBuffer);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo);

    VkBufferCopy copyRegion = {0};
    copyRegion.size = szSize;
    vkCmdCopyBuffer(tCommandBuffer, stagingBuffer, ptBuffer->tBuffer, 1, &copyRegion);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);

    vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, stagingBuffer, NULL);
    vkFreeMemory(ptVulkanDevice->tLogicalDevice, stagingBufferDeviceMemory, NULL);
    return uBufferIndex;
}

static uint32_t
pl_create_vertex_buffer(plDevice* ptDevice, size_t szSize, size_t szStride, const void* pData, const char* pcName)
{
    plVulkanDevice* ptVulkanDevice = ptDevice->_pInternalData;

    const uint32_t uBufferIndex = pl_sb_size(ptDevice->sbtBuffers);

    plVulkanBuffer* ptBuffer = PL_ALLOC(sizeof(plVulkanBuffer));
    memset(ptBuffer, 0, sizeof(plVulkanBuffer));

    plBuffer tBuffer = {
        .pBuffer = ptBuffer
    };
    pl_sb_push(ptDevice->sbtBuffers, tBuffer);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferDeviceMemory;

    { // create staging buffer

        VkBufferCreateInfo bufferInfo = {0};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = szSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &bufferInfo, NULL, &stagingBuffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {0};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(ptVulkanDevice->tMemProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &allocInfo, NULL, &stagingBufferDeviceMemory));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, stagingBuffer, stagingBufferDeviceMemory, 0));

        void* mapping;
        PL_VULKAN(vkMapMemory(ptVulkanDevice->tLogicalDevice, stagingBufferDeviceMemory, 0, bufferInfo.size, 0, &mapping));
        memcpy(mapping, pData, bufferInfo.size);
        vkUnmapMemory(ptVulkanDevice->tLogicalDevice, stagingBufferDeviceMemory);
    }

    { // create final buffer

        VkBufferCreateInfo bufferInfo = {0};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = szSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        PL_VULKAN(vkCreateBuffer(ptVulkanDevice->tLogicalDevice, &bufferInfo, NULL, &ptBuffer->tBuffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(ptVulkanDevice->tLogicalDevice, ptBuffer->tBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {0};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(ptVulkanDevice->tMemProps, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        PL_VULKAN(vkAllocateMemory(ptVulkanDevice->tLogicalDevice, &allocInfo, NULL, &ptBuffer->tMemory));
        PL_VULKAN(vkBindBufferMemory(ptVulkanDevice->tLogicalDevice, ptBuffer->tBuffer, ptBuffer->tMemory, 0));
    }

    // copy buffer
    VkCommandBuffer tCommandBuffer = {0};
    
    const VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = ptVulkanDevice->tCmdPool,
        .commandBufferCount = 1u,
    };
    vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tCommandBuffer);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(tCommandBuffer, &tBeginInfo);

    VkBufferCopy copyRegion = {0};
    copyRegion.size = szSize;
    vkCmdCopyBuffer(tCommandBuffer, stagingBuffer, ptBuffer->tBuffer, 1, &copyRegion);

    PL_VULKAN(vkEndCommandBuffer(tCommandBuffer));
    const VkSubmitInfo tSubmitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &tCommandBuffer,
    };

    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, VK_NULL_HANDLE));
    PL_VULKAN(vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice));
    vkFreeCommandBuffers(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, 1, &tCommandBuffer);

    vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, stagingBuffer, NULL);
    vkFreeMemory(ptVulkanDevice->tLogicalDevice, stagingBufferDeviceMemory, NULL);
    return uBufferIndex;
}

static void
pl_begin_recording(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    const VkCommandBufferBeginInfo tBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    PL_VULKAN(vkResetCommandPool(ptVulkanDevice->tLogicalDevice, ptCurrentFrame->tCmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));
    PL_VULKAN(vkBeginCommandBuffer(ptCurrentFrame->tCmdBuf, &tBeginInfo));  

    VkRenderPassBeginInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = ptVulkanGfx->tRenderPass;
    renderPassInfo.framebuffer = ptVulkanGfx->tSwapchain.sbtFrameBuffers[ptVulkanGfx->tSwapchain.uCurrentImageIndex];
    renderPassInfo.renderArea.extent = ptVulkanGfx->tSwapchain.tExtent;

    VkClearValue clearValues[2];
    clearValues[0].color.float32[0] = 0.0f;
    clearValues[0].color.float32[1] = 0.0f;
    clearValues[0].color.float32[2] = 0.0f;
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 0.0f;
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    VkRect2D scissor = {0};
    scissor.extent = ptVulkanGfx->tSwapchain.tExtent;

    VkViewport tViewport = {0};
    tViewport.x = 0.0f;
    tViewport.y = 0.0f;
    tViewport.width = (float)ptVulkanGfx->tSwapchain.tExtent.width;
    tViewport.height = (float)ptVulkanGfx->tSwapchain.tExtent.height;

    vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);
    vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &scissor);  

    vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    pl_new_draw_frame_vulkan();

}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);

    PL_VULKAN(vkEndCommandBuffer(ptCurrentFrame->tCmdBuf));
}

static void
pl_draw_list(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    plIO* ptIOCtx = pl_get_io();
    for(uint32_t i = 0; i < uListCount; i++)
    {
        pl_submit_vulkan_drawlist(&atLists[i], ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptVulkanGfx->szCurrentFrameIndex);
    }
}

static void
pl_initialize_graphics(plGraphics* ptGraphics)
{

    plIO* ptIOCtx = pl_get_io();

    ptGraphics->_pInternalData = PL_ALLOC(sizeof(plVulkanGraphics));
    memset(ptGraphics->_pInternalData, 0, sizeof(plVulkanGraphics));

    ptGraphics->tDevice._pInternalData = PL_ALLOC(sizeof(plVulkanDevice));
    memset(ptGraphics->tDevice._pInternalData, 0, sizeof(plVulkanDevice));

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;
    
    ptVulkanGfx->uFramesInFlight = 2;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create instance~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const bool bEnableValidation = true;

    static const char* pcKhronosValidationLayer = "VK_LAYER_KHRONOS_validation";

    const char** sbpcEnabledExtensions = NULL;
    pl_sb_push(sbpcEnabledExtensions, VK_KHR_SURFACE_EXTENSION_NAME);

    #ifdef _WIN32
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(__APPLE__)
        pl_sb_push(sbpcEnabledExtensions, "VK_EXT_metal_surface");
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    #else // linux
        pl_sb_push(sbpcEnabledExtensions, VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #endif

    if(bEnableValidation)
    {
        pl_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        pl_sb_push(sbpcEnabledExtensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    // retrieve supported layers
    uint32_t uInstanceLayersFound = 0u;
    VkLayerProperties* ptAvailableLayers = NULL;
    PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, NULL));
    if(uInstanceLayersFound > 0)
    {
        ptAvailableLayers = (VkLayerProperties*)PL_ALLOC(sizeof(VkLayerProperties) * uInstanceLayersFound);
        PL_VULKAN(vkEnumerateInstanceLayerProperties(&uInstanceLayersFound, ptAvailableLayers));
    }

    // retrieve supported extensions
    uint32_t uInstanceExtensionsFound = 0u;
    VkExtensionProperties* ptAvailableExtensions = NULL;
    PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, NULL));
    if(uInstanceExtensionsFound > 0)
    {
        ptAvailableExtensions = (VkExtensionProperties*)PL_ALLOC(sizeof(VkExtensionProperties) * uInstanceExtensionsFound);
        PL_VULKAN(vkEnumerateInstanceExtensionProperties(NULL, &uInstanceExtensionsFound, ptAvailableExtensions));
    }

    // ensure extensions are supported
    const char** sbpcMissingExtensions = NULL;
    for(uint32_t i = 0; i < pl_sb_size(sbpcEnabledExtensions); i++)
    {
        const char* requestedExtension = sbpcEnabledExtensions[i];
        bool extensionFound = false;
        for(uint32_t j = 0; j < uInstanceExtensionsFound; j++)
        {
            if(strcmp(requestedExtension, ptAvailableExtensions[j].extensionName) == 0)
            {
                pl_log_trace_to_f(uLogChannel, "extension %s found", ptAvailableExtensions[j].extensionName);
                extensionFound = true;
                break;
            }
        }

        if(!extensionFound)
        {
            pl_sb_push(sbpcMissingExtensions, requestedExtension);
        }
    }

    // report if all requested extensions aren't found
    if(pl_sb_size(sbpcMissingExtensions) > 0)
    {
        // pl_log_error_to_f(uLogChannel, "%d %s", pl_sb_size(sbpcMissingExtensions), "Missing Extensions:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingExtensions); i++)
        {
            pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingExtensions[i]);
        }

        PL_ASSERT(false && "Can't find all requested extensions");
    }

    // ensure layers are supported
    const char** sbpcMissingLayers = NULL;
    for(uint32_t i = 0; i < 1; i++)
    {
        const char* pcRequestedLayer = (&pcKhronosValidationLayer)[i];
        bool bLayerFound = false;
        for(uint32_t j = 0; j < uInstanceLayersFound; j++)
        {
            if(strcmp(pcRequestedLayer, ptAvailableLayers[j].layerName) == 0)
            {
                pl_log_trace_to_f(uLogChannel, "layer %s found", ptAvailableLayers[j].layerName);
                bLayerFound = true;
                break;
            }
        }

        if(!bLayerFound)
        {
            pl_sb_push(sbpcMissingLayers, pcRequestedLayer);
        }
    }

    // report if all requested layers aren't found
    if(pl_sb_size(sbpcMissingLayers) > 0)
    {
        pl_log_error_to_f(uLogChannel, "%d %s", pl_sb_size(sbpcMissingLayers), "Missing Layers:");
        for(uint32_t i = 0; i < pl_sb_size(sbpcMissingLayers); i++)
        {
            pl_log_error_to_f(uLogChannel, "  * %s", sbpcMissingLayers[i]);
        }
        PL_ASSERT(false && "Can't find all requested layers");
    }

    // Setup debug messenger for vulkan tInstance
    VkDebugUtilsMessengerCreateInfoEXT tDebugCreateInfo = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = pl__debug_callback,
        .pNext           = VK_NULL_HANDLE
    };

    // create vulkan tInstance
    VkApplicationInfo tAppInfo = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_2
    };

    VkInstanceCreateInfo tCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &tAppInfo,
        .pNext                   = bEnableValidation ? (VkDebugUtilsMessengerCreateInfoEXT*)&tDebugCreateInfo : VK_NULL_HANDLE,
        .enabledExtensionCount   = pl_sb_size(sbpcEnabledExtensions),
        .ppEnabledExtensionNames = sbpcEnabledExtensions,
        .enabledLayerCount       = 1,
        .ppEnabledLayerNames     = &pcKhronosValidationLayer,

        #ifdef __APPLE__
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        #endif
    };

    PL_VULKAN(vkCreateInstance(&tCreateInfo, NULL, &ptVulkanGfx->tInstance));
    pl_log_trace_to_f(uLogChannel, "created vulkan instance");

    // cleanup
    if(ptAvailableLayers)     PL_FREE(ptAvailableLayers);
    if(ptAvailableExtensions) PL_FREE(ptAvailableExtensions);
    pl_sb_free(sbpcMissingLayers);
    pl_sb_free(sbpcMissingExtensions);

    if(bEnableValidation)
    {
        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptVulkanGfx->tInstance, "vkCreateDebugUtilsMessengerEXT");
        PL_ASSERT(func != NULL && "failed to set up debug messenger!");
        PL_VULKAN(func(ptVulkanGfx->tInstance, &tDebugCreateInfo, NULL, &ptVulkanGfx->tDbgMessenger));     
        pl_log_trace_to_f(uLogChannel, "enabled Vulkan validation layers");
    }

    pl_sb_free(sbpcEnabledExtensions);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create surface~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    #ifdef _WIN32
        const VkWin32SurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = *(HWND*)ptIOCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateWin32SurfaceKHR(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #elif defined(__APPLE__)
        const VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = (CAMetalLayer*)ptIOCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #else // linux
        struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
        struct tPlatformData* ptPlatformData = (struct tPlatformData*)ptIOCtx->pBackendPlatformData;
        const VkXcbSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = ptPlatformData->tWindow,
            .connection = ptPlatformData->ptConnection
        };
        PL_VULKAN(vkCreateXcbSurfaceKHR(ptVulkanGfx->tInstance, &tSurfaceCreateInfo, NULL, &ptVulkanGfx->tSurface));
    #endif   

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create device~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptVulkanDevice->iGraphicsQueueFamily = -1;
    ptVulkanDevice->iPresentQueueFamily = -1;
    ptVulkanDevice->tMemProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    ptVulkanDevice->tMemBudgetInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    ptVulkanDevice->tMemProps2.pNext = &ptVulkanDevice->tMemBudgetInfo;
    // int iDeviceIndex = pl__select_physical_device(ptVulkanGfx->tInstance, ptVulkanDevice);

    uint32_t uDeviceCount = 0u;
    int iBestDvcIdx = 0;
    bool bDiscreteGPUFound = false;
    VkDeviceSize tMaxLocalMemorySize = 0u;

    PL_VULKAN(vkEnumeratePhysicalDevices(ptVulkanGfx->tInstance, &uDeviceCount, NULL));
    PL_ASSERT(uDeviceCount > 0 && "failed to find GPUs with Vulkan support!");

    // check if device is suitable
    VkPhysicalDevice atDevices[16] = {0};
    PL_VULKAN(vkEnumeratePhysicalDevices(ptVulkanGfx->tInstance, &uDeviceCount, atDevices));

    // prefer discrete, then memory size
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        vkGetPhysicalDeviceProperties(atDevices[i], &ptVulkanDevice->tDeviceProps);
        vkGetPhysicalDeviceMemoryProperties(atDevices[i], &ptVulkanDevice->tMemProps);

        for(uint32_t j = 0; j < ptVulkanDevice->tMemProps.memoryHeapCount; j++)
        {
            if(ptVulkanDevice->tMemProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT && ptVulkanDevice->tMemProps.memoryHeaps[j].size > tMaxLocalMemorySize && !bDiscreteGPUFound)
            {
                    tMaxLocalMemorySize = ptVulkanDevice->tMemProps.memoryHeaps[j].size;
                iBestDvcIdx = i;
            }
        }

        if(ptVulkanDevice->tDeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && !bDiscreteGPUFound)
        {
            iBestDvcIdx = i;
            bDiscreteGPUFound = true;
        }
    }

    ptVulkanDevice->tPhysicalDevice = atDevices[iBestDvcIdx];

    PL_ASSERT(ptVulkanDevice->tPhysicalDevice != VK_NULL_HANDLE && "failed to find a suitable GPU!");
    vkGetPhysicalDeviceProperties(atDevices[iBestDvcIdx], &ptVulkanDevice->tDeviceProps);
    vkGetPhysicalDeviceMemoryProperties(atDevices[iBestDvcIdx], &ptVulkanDevice->tMemProps);
    static const char* pacDeviceTypeName[] = {"Other", "Integrated", "Discrete", "Virtual", "CPU"};

    // print info on chosen device
    pl_log_info_to_f(uLogChannel, "Device ID: %u", ptVulkanDevice->tDeviceProps.deviceID);
    pl_log_info_to_f(uLogChannel, "Vendor ID: %u", ptVulkanDevice->tDeviceProps.vendorID);
    pl_log_info_to_f(uLogChannel, "API Version: %u", ptVulkanDevice->tDeviceProps.apiVersion);
    pl_log_info_to_f(uLogChannel, "Driver Version: %u", ptVulkanDevice->tDeviceProps.driverVersion);
    pl_log_info_to_f(uLogChannel, "Device Type: %s", pacDeviceTypeName[ptVulkanDevice->tDeviceProps.deviceType]);
    pl_log_info_to_f(uLogChannel, "Device Name: %s", ptVulkanDevice->tDeviceProps.deviceName);

    uint32_t uExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(ptVulkanDevice->tPhysicalDevice, NULL, &uExtensionCount, NULL);
    VkExtensionProperties* ptExtensions = PL_ALLOC(uExtensionCount * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(ptVulkanDevice->tPhysicalDevice, NULL, &uExtensionCount, ptExtensions);

    for(uint32_t i = 0; i < uExtensionCount; i++)
    {
        if(pl_str_equal(ptExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) ptVulkanDevice->bSwapchainExtPresent = true; //-V522
        if(pl_str_equal(ptExtensions[i].extensionName, "VK_KHR_portability_subset"))     ptVulkanDevice->bPortabilitySubsetPresent = true; //-V522
    }

    PL_FREE(ptExtensions);

    ptVulkanDevice->tMaxLocalMemSize = ptVulkanDevice->tMemProps.memoryHeaps[iBestDvcIdx].size;
    ptVulkanDevice->uUniformBufferBlockSize = pl_minu(PL_DEVICE_ALLOCATION_BLOCK_SIZE, ptVulkanDevice->tDeviceProps.limits.maxUniformBufferRange);

    // find queue families
    uint32_t uQueueFamCnt = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(ptVulkanDevice->tPhysicalDevice, &uQueueFamCnt, NULL);

    VkQueueFamilyProperties auQueueFamilies[64] = {0};
    vkGetPhysicalDeviceQueueFamilyProperties(ptVulkanDevice->tPhysicalDevice, &uQueueFamCnt, auQueueFamilies);

    for(uint32_t i = 0; i < uQueueFamCnt; i++)
    {
        if (auQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ptVulkanDevice->iGraphicsQueueFamily = i;

        VkBool32 tPresentSupport = false;
        PL_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(ptVulkanDevice->tPhysicalDevice, i, ptVulkanGfx->tSurface, &tPresentSupport));

        if (tPresentSupport) ptVulkanDevice->iPresentQueueFamily  = i;

        if (ptVulkanDevice->iGraphicsQueueFamily > -1 && ptVulkanDevice->iPresentQueueFamily > -1) // complete
            break;
        i++;
    }

    // create logical device

    vkGetPhysicalDeviceFeatures(ptVulkanDevice->tPhysicalDevice, &ptVulkanDevice->tDeviceFeatures);

    const float fQueuePriority = 1.0f;
    VkDeviceQueueCreateInfo atQueueCreateInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptVulkanDevice->iGraphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ptVulkanDevice->iPresentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &fQueuePriority   
        }
    };
    
    static const char* pcValidationLayers = "VK_LAYER_KHRONOS_validation";

    const char** sbpcDeviceExts = NULL;
    if(ptVulkanDevice->bSwapchainExtPresent)      pl_sb_push(sbpcDeviceExts, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(ptVulkanDevice->bPortabilitySubsetPresent) pl_sb_push(sbpcDeviceExts, "VK_KHR_portability_subset");
    pl_sb_push(sbpcDeviceExts, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    VkDeviceCreateInfo tCreateDeviceInfo = {
        .sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount     = atQueueCreateInfos[0].queueFamilyIndex == atQueueCreateInfos[1].queueFamilyIndex ? 1 : 2,
        .pQueueCreateInfos        = atQueueCreateInfos,
        .pEnabledFeatures         = &ptVulkanDevice->tDeviceFeatures,
        .ppEnabledExtensionNames  = sbpcDeviceExts,
        .enabledLayerCount        = bEnableValidation ? 1 : 0,
        .ppEnabledLayerNames      = bEnableValidation ? &pcValidationLayers : NULL,
        .enabledExtensionCount    = pl_sb_size(sbpcDeviceExts)
    };
    PL_VULKAN(vkCreateDevice(ptVulkanDevice->tPhysicalDevice, &tCreateDeviceInfo, NULL, &ptVulkanDevice->tLogicalDevice));

    pl_sb_free(sbpcDeviceExts);

    // get device queues
    vkGetDeviceQueue(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->iGraphicsQueueFamily, 0, &ptVulkanDevice->tGraphicsQueue);
    vkGetDeviceQueue(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->iPresentQueueFamily, 0, &ptVulkanDevice->tPresentQueue);


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug markers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	ptVulkanDevice->vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkDebugMarkerSetObjectTagEXT");
	ptVulkanDevice->vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkDebugMarkerSetObjectNameEXT");
	ptVulkanDevice->vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerBeginEXT");
	ptVulkanDevice->vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerEndEXT");
	ptVulkanDevice->vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(ptVulkanDevice->tLogicalDevice, "vkCmdDebugMarkerInsertEXT");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~command pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptVulkanDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    PL_VULKAN(vkCreateCommandPool(ptVulkanDevice->tLogicalDevice, &tCommandPoolInfo, NULL, &ptVulkanDevice->tCmdPool));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~swapchain~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptVulkanGfx->tSwapchain.bVSync = true;
    create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main renderpass~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkAttachmentDescription atAttachments[] = {

        // multisampled color attachment (render to this)
        {
            .format         = ptVulkanGfx->tSwapchain.tFormat,
            .samples        = ptVulkanGfx->tSwapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },

        // depth attachment
        {
            .format         = find_depth_stencil_format(&ptGraphics->tDevice),
            .samples        = ptVulkanGfx->tSwapchain.tMsaaSamples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },

        // color resolve attachment
        {
            .format         = ptVulkanGfx->tSwapchain.tFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
    };

    const VkSubpassDependency tSubpassDependencies[] = {

        // color attachment
        { 
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = 0,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        },
        // depth attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0
        }
    };

    const VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 2,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL     
        }
    };

    const VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1],
        .pResolveAttachments     = &atAttachmentReferences[2]
    };

    const VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };

    PL_VULKAN(vkCreateRenderPass(ptVulkanDevice->tLogicalDevice, &tRenderPassInfo, NULL, &ptVulkanGfx->tRenderPass));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
    for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
    {
        ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

        VkImageView atViewAttachments[] = {
            ptVulkanGfx->tSwapchain.tColorTextureView,
            ptVulkanGfx->tSwapchain.tDepthTextureView,
            ptVulkanGfx->tSwapchain.sbtImageViews[i]
        };

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanGfx->tRenderPass,
            .attachmentCount = 3,
            .pAttachments    = atViewAttachments,
            .width           = ptVulkanGfx->tSwapchain.tExtent.width,
            .height          = ptVulkanGfx->tSwapchain.tExtent.height,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame resources~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const VkCommandPoolCreateInfo tFrameCommandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ptVulkanDevice->iGraphicsQueueFamily,
        .flags            = 0
    };
    
    const VkSemaphoreCreateInfo tSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo tFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    pl_sb_resize(ptVulkanGfx->sbFrames, ptVulkanGfx->uFramesInFlight);
    for(uint32_t i = 0; i < ptVulkanGfx->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {0};
        PL_VULKAN(vkCreateSemaphore(ptVulkanDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tImageAvailable));
        PL_VULKAN(vkCreateSemaphore(ptVulkanDevice->tLogicalDevice, &tSemaphoreInfo, NULL, &tFrame.tRenderFinish));
        PL_VULKAN(vkCreateFence(ptVulkanDevice->tLogicalDevice, &tFenceInfo, NULL, &tFrame.tInFlight));
        PL_VULKAN(vkCreateCommandPool(ptVulkanDevice->tLogicalDevice, &tFrameCommandPoolInfo, NULL, &tFrame.tCmdPool));

        const VkCommandBufferAllocateInfo tAllocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = tFrame.tCmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        PL_VULKAN(vkAllocateCommandBuffers(ptVulkanDevice->tLogicalDevice, &tAllocInfo, &tFrame.tCmdBuf));  
        ptVulkanGfx->sbFrames[i] = tFrame;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main descriptor pool~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VkDescriptorPoolSize atPoolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                100000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       100000 }
    };
    VkDescriptorPoolCreateInfo tDescriptorPoolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 100000 * 11,
        .poolSizeCount = 11u,
        .pPoolSizes    = atPoolSizes,
    };
    PL_VULKAN(vkCreateDescriptorPool(ptVulkanDevice->tLogicalDevice, &tDescriptorPoolInfo, NULL, &ptVulkanGfx->tDescriptorPool));
    

    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptVulkanDevice->tPhysicalDevice,
        .tLogicalDevice   = ptVulkanDevice->tLogicalDevice,
        .uImageCount      = ptVulkanGfx->tSwapchain.uImageCount,
        .tRenderPass      = ptVulkanGfx->tRenderPass,
        .tMSAASampleCount = ptVulkanGfx->tSwapchain.tMsaaSamples,
        .uFramesInFlight  = ptVulkanGfx->uFramesInFlight
    };
    pl_initialize_vulkan(&tVulkanInit);

    // app crap

    ptVulkanGfx->g_bindingDescriptions[0].binding = 0;
    ptVulkanGfx->g_bindingDescriptions[0].stride = sizeof(float)*7;
    ptVulkanGfx->g_bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    ptVulkanGfx->g_attributeDescriptions[0].binding = 0;
    ptVulkanGfx->g_attributeDescriptions[0].location = 0;
    ptVulkanGfx->g_attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    ptVulkanGfx->g_attributeDescriptions[0].offset = 0u;

    ptVulkanGfx->g_attributeDescriptions[1].binding = 0;
    ptVulkanGfx->g_attributeDescriptions[1].location = 1;
    ptVulkanGfx->g_attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ptVulkanGfx->g_attributeDescriptions[1].offset = sizeof(float)*3;

    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &pipelineLayoutInfo, NULL, &ptVulkanGfx->g_pipelineLayout));

    unsigned vertexFileSize = 0u;
    unsigned pixelFileSize = 0u;
    char* vertexShaderCode = read_file("primitive.vert.spv", &vertexFileSize, "rb");
    char* pixelShaderCode = read_file("primitive.frag.spv", &pixelFileSize, "rb");

    {
        VkShaderModuleCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = vertexFileSize;
        createInfo.pCode = (const uint32_t*)(vertexShaderCode);
        assert(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &createInfo, NULL, &ptVulkanGfx->g_vertexShaderModule) == VK_SUCCESS);
    }

    {
        VkShaderModuleCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = pixelFileSize;
        createInfo.pCode = (const uint32_t*)(pixelShaderCode);
        assert(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &createInfo, NULL, &ptVulkanGfx->g_pixelShaderModule) == VK_SUCCESS);
    }

    //---------------------------------------------------------------------
    // input assembler stage
    //---------------------------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1u;
    vertexInputInfo.vertexAttributeDescriptionCount = 2u;
    vertexInputInfo.pVertexBindingDescriptions = ptVulkanGfx->g_bindingDescriptions;
    vertexInputInfo.pVertexAttributeDescriptions = ptVulkanGfx->g_attributeDescriptions;

    //---------------------------------------------------------------------
    // vertex shader stage
    //---------------------------------------------------------------------
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = ptVulkanGfx->g_vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    //---------------------------------------------------------------------
    // tesselation stage
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    // geometry shader stage
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    // rasterization stage
    //---------------------------------------------------------------------

    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)ptIOCtx->afMainViewportSize[0];
    viewport.height = (float)ptIOCtx->afMainViewportSize[1];
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
    scissor.extent.width = (unsigned)viewport.width;
    scissor.extent.height = (unsigned)viewport.y;

    VkPipelineViewportStateCreateInfo viewportState = {0};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    //---------------------------------------------------------------------
    // fragment shader stage
    //---------------------------------------------------------------------
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {0};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = ptVulkanGfx->g_pixelShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineDepthStencilStateCreateInfo depthStencil = {0};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;

    //---------------------------------------------------------------------
    // color blending stage
    //---------------------------------------------------------------------
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {0};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling2 = {0};
    multisampling2.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling2.sampleShadingEnable = VK_FALSE;
    multisampling2.rasterizationSamples = ptVulkanGfx->tSwapchain.tMsaaSamples;

    //---------------------------------------------------------------------
    // Create Pipeline
    //---------------------------------------------------------------------
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo
    };


    VkDynamicState dynamicStateEnables[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
    VkPipelineDynamicStateCreateInfo dynamicState = {0};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 3;
    dynamicState.pDynamicStates = dynamicStateEnables;
    
    VkGraphicsPipelineCreateInfo pipelineInfo = {0};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2u;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling2;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = ptVulkanGfx->g_pipelineLayout;
    pipelineInfo.renderPass = ptVulkanGfx->tRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pDepthStencilState = &depthStencil;

    PL_VULKAN(vkCreateGraphicsPipelines(ptVulkanDevice->tLogicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &ptVulkanGfx->g_pipeline));

    // no longer need these
    vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->g_vertexShaderModule, NULL);
    vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->g_pixelShaderModule, NULL);
    ptVulkanGfx->g_vertexShaderModule = VK_NULL_HANDLE;
    ptVulkanGfx->g_pixelShaderModule = VK_NULL_HANDLE;

    ///////////////////////////////

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3d setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create pipeline layout
    const VkPushConstantRange t3DPushConstant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset    = 0,
        .size      = sizeof(float) * 16
    };

    const VkPipelineLayoutCreateInfo t3DPipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 0u,
        .pSetLayouts            = NULL,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &t3DPushConstant,
    };

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &t3DPipelineLayoutInfo, NULL, &ptVulkanGfx->t3DPipelineLayout));

    // vertex shader stage

    ptVulkanGfx->t3DVtxShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanGfx->t3DVtxShdrStgInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    ptVulkanGfx->t3DVtxShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo t3DVtxShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_vert_3d_spv),
        .pCode    = __glsl_shader_vert_3d_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &t3DVtxShdrInfo, NULL, &ptVulkanGfx->t3DVtxShdrStgInfo.module) == VK_SUCCESS);

    // fragment shader stage
    ptVulkanGfx->t3DPxlShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanGfx->t3DPxlShdrStgInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ptVulkanGfx->t3DPxlShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo t3DPxlShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_frag_3d_spv),
        .pCode    = __glsl_shader_frag_3d_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &t3DPxlShdrInfo, NULL, &ptVulkanGfx->t3DPxlShdrStgInfo.module) == VK_SUCCESS);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3d line setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create pipeline layout
    const VkPushConstantRange t3DLinePushConstant = 
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset    = 0,
        .size      = sizeof(float) * 17
    };

    const VkPipelineLayoutCreateInfo t3DLinePipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 0u,
        .pSetLayouts            = NULL,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &t3DLinePushConstant,
    };

    PL_VULKAN(vkCreatePipelineLayout(ptVulkanDevice->tLogicalDevice, &t3DLinePipelineLayoutInfo, NULL, &ptVulkanGfx->t3DLinePipelineLayout));

    // vertex shader stage

    ptVulkanGfx->t3DLineVtxShdrStgInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ptVulkanGfx->t3DLineVtxShdrStgInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    ptVulkanGfx->t3DLineVtxShdrStgInfo.pName = "main";

    const VkShaderModuleCreateInfo t3DLineVtxShdrInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(__glsl_shader_vert_3d_line_spv),
        .pCode    = __glsl_shader_vert_3d_line_spv
    };
    PL_ASSERT(vkCreateShaderModule(ptVulkanDevice->tLogicalDevice, &t3DLineVtxShdrInfo, NULL, &ptVulkanGfx->t3DLineVtxShdrStgInfo.module) == VK_SUCCESS);

    pl_sb_resize(ptVulkanGfx->sbt3DBufferInfo, ptVulkanGfx->uFramesInFlight);
    pl_sb_resize(ptVulkanGfx->sbtLineBufferInfo, ptVulkanGfx->uFramesInFlight);
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    // cleanup queue
    plFrameGarbage* ptGarbage = &ptVulkanDevice->_sbtFrameGarbage[ptVulkanDevice->uCurrentFrame];

    if(ptGarbage)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
            vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtTextures[i], NULL);

        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextureViews); i++)
            vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtTextureViews[i], NULL);

        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtFrameBuffers); i++)
            vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtFrameBuffers[i], NULL);

        for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtMemory); i++)
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptGarbage->sbtMemory[i], NULL);

        pl_sb_reset(ptGarbage->sbtTextures);
        pl_sb_reset(ptGarbage->sbtTextureViews);
        pl_sb_reset(ptGarbage->sbtFrameBuffers);
        pl_sb_reset(ptGarbage->sbtMemory);
    }

    PL_VULKAN(vkWaitForFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));
    VkResult err = vkAcquireNextImageKHR(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tSwapChain, UINT64_MAX, ptCurrentFrame->tImageAvailable, VK_NULL_HANDLE, &ptVulkanGfx->tSwapchain.uCurrentImageIndex);
    if(err == VK_SUBOPTIMAL_KHR || err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if(err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

            // recreate frame buffers
            pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
            for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
            {
                ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

                VkImageView atViewAttachments[] = {
                    ptVulkanGfx->tSwapchain.tColorTextureView,
                    ptVulkanGfx->tSwapchain.tDepthTextureView,
                    ptVulkanGfx->tSwapchain.sbtImageViews[i]
                };

                VkFramebufferCreateInfo tFrameBufferInfo = {
                    .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass      = ptVulkanGfx->tRenderPass,
                    .attachmentCount = 3,
                    .pAttachments    = atViewAttachments,
                    .width           = ptVulkanGfx->tSwapchain.tExtent.width,
                    .height          = ptVulkanGfx->tSwapchain.tExtent.height,
                    .layers          = 1u,
                };
                PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
            }

            pl_end_profile_sample();
            return false;
        }
    }
    else
    {
        PL_VULKAN(err);
    }

    if (ptCurrentFrame->tInFlight != VK_NULL_HANDLE)
        PL_VULKAN(vkWaitForFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight, VK_TRUE, UINT64_MAX));

    //-----------------------------------------------------------------------------
    // buffer deletion queue
    //-----------------------------------------------------------------------------

    pl_sb_reset(ptVulkanGfx->sbReturnedBuffersTemp);
    if(ptVulkanGfx->uBufferDeletionQueueSize > 0u)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbReturnedBuffers); i++)
        {
            if(ptVulkanGfx->sbReturnedBuffers[i].slFreedFrame < (int64_t)ptVulkanGfx->szCurrentFrameIndex)
            {
                ptVulkanGfx->uBufferDeletionQueueSize--;
                vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tBuffer, NULL);
                vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tDeviceMemory, NULL);
            }
            else
            {
                pl_sb_push(ptVulkanGfx->sbReturnedBuffersTemp, ptVulkanGfx->sbReturnedBuffers[i]);
            }
        }     
    }

    pl_sb_reset(ptVulkanGfx->sbReturnedBuffers);
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbReturnedBuffersTemp); i++)
        pl_sb_push(ptVulkanGfx->sbReturnedBuffers, ptVulkanGfx->sbReturnedBuffersTemp[i]);

    // reset buffer offsets
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbt3DBufferInfo); i++)
    {
        ptVulkanGfx->sbt3DBufferInfo[i].uVertexBufferOffset = 0;
        ptVulkanGfx->sbt3DBufferInfo[i].uIndexBufferOffset = 0;
    }

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtLineBufferInfo); i++)
    {
        ptVulkanGfx->sbtLineBufferInfo[i].uVertexBufferOffset = 0;
        ptVulkanGfx->sbtLineBufferInfo[i].uIndexBufferOffset = 0;
    }

    // reset 3d drawlists
    for(uint32_t i = 0u; i < pl_sb_size(ptGraphics->sbt3DDrawlists); i++)
    {
        plDrawList3D* drawlist = ptGraphics->sbt3DDrawlists[i];

        pl_sb_reset(drawlist->sbtSolidVertexBuffer);
        pl_sb_reset(drawlist->sbtLineVertexBuffer);
        pl_sb_reset(drawlist->sbtSolidIndexBuffer);    
        pl_sb_reset(drawlist->sbtLineIndexBuffer);    
    }

    pl_end_profile_sample();
    return true; 
}

static void
pl_end_gfx_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    // submit
    const VkPipelineStageFlags atWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    const VkSubmitInfo tSubmitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &ptCurrentFrame->tImageAvailable,
        .pWaitDstStageMask    = atWaitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &ptCurrentFrame->tCmdBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &ptCurrentFrame->tRenderFinish
    };
    PL_VULKAN(vkResetFences(ptVulkanDevice->tLogicalDevice, 1, &ptCurrentFrame->tInFlight));
    PL_VULKAN(vkQueueSubmit(ptVulkanDevice->tGraphicsQueue, 1, &tSubmitInfo, ptCurrentFrame->tInFlight));          
    
    // present                        
    const VkPresentInfoKHR tPresentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &ptCurrentFrame->tRenderFinish,
        .swapchainCount     = 1,
        .pSwapchains        = &ptVulkanGfx->tSwapchain.tSwapChain,
        .pImageIndices      = &ptVulkanGfx->tSwapchain.uCurrentImageIndex,
    };
    const VkResult tResult = vkQueuePresentKHR(ptVulkanDevice->tPresentQueue, &tPresentInfo);
    if(tResult == VK_SUBOPTIMAL_KHR || tResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

        // recreate frame buffers
        pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
        for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
        {
            ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

            VkImageView atViewAttachments[] = {
                ptVulkanGfx->tSwapchain.tColorTextureView,
                ptVulkanGfx->tSwapchain.tDepthTextureView,
                ptVulkanGfx->tSwapchain.sbtImageViews[i]
            };

            VkFramebufferCreateInfo tFrameBufferInfo = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = ptVulkanGfx->tRenderPass,
                .attachmentCount = 3,
                .pAttachments    = atViewAttachments,
                .width           = ptVulkanGfx->tSwapchain.tExtent.width,
                .height          = ptVulkanGfx->tSwapchain.tExtent.height,
                .layers          = 1u,
            };
            PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
        }
    }
    else
    {
        PL_VULKAN(tResult);
    }

    ptVulkanGfx->szCurrentFrameIndex = (ptVulkanGfx->szCurrentFrameIndex + 1) % ptVulkanGfx->uFramesInFlight;

    pl_end_profile_sample();
}

static void
pl_resize(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics);

    create_swapchain(ptGraphics, (uint32_t)ptIOCtx->afMainViewportSize[0], (uint32_t)ptIOCtx->afMainViewportSize[1], &ptVulkanGfx->tSwapchain);

    // recreate frame buffers
    pl_sb_resize(ptVulkanGfx->tSwapchain.sbtFrameBuffers, ptVulkanGfx->tSwapchain.uImageCount);
    for(uint32_t i = 0; i < ptVulkanGfx->tSwapchain.uImageCount; i++)
    {
        ptVulkanGfx->tSwapchain.sbtFrameBuffers[i] = VK_NULL_HANDLE;

        VkImageView atViewAttachments[] = {
            ptVulkanGfx->tSwapchain.tColorTextureView,
            ptVulkanGfx->tSwapchain.tDepthTextureView,
            ptVulkanGfx->tSwapchain.sbtImageViews[i]
        };

        VkFramebufferCreateInfo tFrameBufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = ptVulkanGfx->tRenderPass,
            .attachmentCount = 3,
            .pAttachments    = atViewAttachments,
            .width           = ptVulkanGfx->tSwapchain.tExtent.width,
            .height          = ptVulkanGfx->tSwapchain.tExtent.height,
            .layers          = 1u,
        };
        PL_VULKAN(vkCreateFramebuffer(ptVulkanDevice->tLogicalDevice, &tFrameBufferInfo, NULL, &ptVulkanGfx->tSwapchain.sbtFrameBuffers[i]));
    }

    ptVulkanGfx->szCurrentFrameIndex = 0;

    pl_end_profile_sample();
}

static void
pl_shutdown(plGraphics* ptGraphics)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    vkDeviceWaitIdle(ptVulkanDevice->tLogicalDevice);

    pl_cleanup_vulkan();

    // cleanup 3d
    {
        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbt3DBufferInfo); i++)
        {
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tVertexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tVertexMemory, NULL);
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tIndexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DBufferInfo[i].tIndexMemory, NULL);
        }

        for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbtLineBufferInfo); i++)
        {
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tVertexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tVertexMemory, NULL);
            vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tIndexBuffer, NULL);
            vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbtLineBufferInfo[i].tIndexMemory, NULL);
        }

        for(uint32_t i = 0u; i < pl_sb_size(ptVulkanGfx->sbt3DPipelines); i++)
        {
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DPipelines[i].tRegularPipeline, NULL);
            vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbt3DPipelines[i].tSecondaryPipeline, NULL);
        }

        if(ptVulkanGfx->uBufferDeletionQueueSize > 0u)
        {
            for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbReturnedBuffers); i++)
            {
                vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tBuffer, NULL);
                vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->sbReturnedBuffers[i].tDeviceMemory, NULL);
            }     
        }

        vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DPxlShdrStgInfo.module, NULL);
        vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DVtxShdrStgInfo.module, NULL);
        vkDestroyShaderModule(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DLineVtxShdrStgInfo.module, NULL);
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tStagingBuffer, NULL);
        vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tStagingMemory, NULL);
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DPipelineLayout, NULL);
        vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->t3DLinePipelineLayout, NULL);

        pl_sb_free(ptVulkanGfx->sbReturnedBuffers);
        pl_sb_free(ptVulkanGfx->sbReturnedBuffersTemp);
        pl_sb_free(ptVulkanGfx->sbt3DBufferInfo);
        pl_sb_free(ptVulkanGfx->sbtLineBufferInfo);
        pl_sb_free(ptVulkanGfx->sbt3DPipelines);
        
        for(uint32_t i = 0u; i < pl_sb_size(ptGraphics->sbt3DDrawlists); i++)
        {
            plDrawList3D* drawlist = ptGraphics->sbt3DDrawlists[i];
            pl_sb_free(drawlist->sbtSolidIndexBuffer);
            pl_sb_free(drawlist->sbtSolidVertexBuffer);
            pl_sb_free(drawlist->sbtLineVertexBuffer);
            pl_sb_free(drawlist->sbtLineIndexBuffer);  
        }
        pl_sb_free(ptGraphics->sbt3DDrawlists);
    }

    // cleanup buffers
    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->tDevice.sbtBuffers); i++)
    {
        plVulkanBuffer* ptBuffer = ptGraphics->tDevice.sbtBuffers[i].pBuffer;
        vkDestroyBuffer(ptVulkanDevice->tLogicalDevice, ptBuffer->tBuffer, NULL);
        vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptBuffer->tMemory, NULL);
        PL_FREE(ptGraphics->tDevice.sbtBuffers[i].pBuffer);
    }

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptVulkanGfx->sbFrames[i];
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tImageAvailable, NULL);
        vkDestroySemaphore(ptVulkanDevice->tLogicalDevice, ptFrame->tRenderFinish, NULL);
        vkDestroyFence(ptVulkanDevice->tLogicalDevice, ptFrame->tInFlight, NULL);
        vkDestroyCommandPool(ptVulkanDevice->tLogicalDevice, ptFrame->tCmdPool, NULL);
    }

    // swapchain stuff
    vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tColorTextureView, NULL);
    vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tDepthTextureView, NULL);
    vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tColorTexture, NULL);
    vkDestroyImage(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tDepthTexture, NULL);
    vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tColorTextureMemory, NULL);
    vkFreeMemory(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.tDepthTextureMemory, NULL);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->tSwapchain.sbtImageViews); i++)
        vkDestroyImageView(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.sbtImageViews[i], NULL);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanGfx->tSwapchain.sbtFrameBuffers); i++)
        vkDestroyFramebuffer(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tSwapchain.sbtFrameBuffers[i], NULL);
    

    vkDestroyPipelineLayout(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->g_pipelineLayout, NULL);
    vkDestroyPipeline(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->g_pipeline, NULL);

    vkDestroyRenderPass(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tRenderPass, NULL);

    vkDestroyDescriptorPool(ptVulkanDevice->tLogicalDevice, ptVulkanGfx->tDescriptorPool, NULL);

    // destroy command pool
    vkDestroyCommandPool(ptVulkanDevice->tLogicalDevice, ptVulkanDevice->tCmdPool, NULL);

    // destroy device
    vkDestroyDevice(ptVulkanDevice->tLogicalDevice, NULL);

    if(ptVulkanGfx->tDbgMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT tFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ptVulkanGfx->tInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (tFunc != NULL)
            tFunc(ptVulkanGfx->tInstance, ptVulkanGfx->tDbgMessenger, NULL);
    }

    // destroy tSurface
    vkDestroySurfaceKHR(ptVulkanGfx->tInstance, ptVulkanGfx->tSurface, NULL);

    // destroy tInstance
    vkDestroyInstance(ptVulkanGfx->tInstance, NULL);

    for(uint32_t i = 0; i < pl_sb_size(ptVulkanDevice->_sbtFrameGarbage); i++)
    {
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtFrameBuffers);
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtMemory);
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtTextures);
        pl_sb_free(ptVulkanDevice->_sbtFrameGarbage[i].sbtTextureViews);
    }

    pl_sb_free(ptVulkanDevice->_sbtFrameGarbage);
    pl_sb_free(ptVulkanGfx->sbFrames);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtSurfaceFormats);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtImages);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtFrameBuffers);
    pl_sb_free(ptVulkanGfx->tSwapchain.sbtImageViews);
    pl_sb_free(ptGraphics->tDevice.sbtBuffers);
    PL_FREE(ptGraphics->_pInternalData);
    PL_FREE(ptGraphics->tDevice._pInternalData);
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws)
{
    plVulkanGraphics* ptVulkanGfx = ptGraphics->_pInternalData;
    plVulkanDevice*   ptVulkanDevice = ptGraphics->tDevice._pInternalData;

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(ptGraphics); 

    static VkDeviceSize offsets = { 0 };
    vkCmdSetDepthBias(ptCurrentFrame->tCmdBuf, 0.0f, 0.0f, 0.0f);
    vkCmdBindPipeline(ptCurrentFrame->tCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ptVulkanGfx->g_pipeline);

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];

        for(uint32_t j = 0; j < ptArea->uDrawCount; j++)
        {
            plDraw* ptDraw = &atDraws[ptArea->uDrawOffset];

            plVulkanBuffer* ptVertexBuffer = ptGraphics->tDevice.sbtBuffers[ptDraw->ptMesh->uVertexBuffer].pBuffer;
            plVulkanBuffer* ptIndexBuffer = ptGraphics->tDevice.sbtBuffers[ptDraw->ptMesh->uIndexBuffer].pBuffer;
            vkCmdBindIndexBuffer(ptCurrentFrame->tCmdBuf, ptIndexBuffer->tBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindVertexBuffers(ptCurrentFrame->tCmdBuf, 0, 1, &ptVertexBuffer->tBuffer, &offsets);
            vkCmdDrawIndexed(ptCurrentFrame->tCmdBuf, ptDraw->ptMesh->uIndexCount, 1, 0, 0, 0);
        }
        
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static const plGraphicsI*
pl_load_graphics_api(void)
{
    static const plGraphicsI tApi = {
        .initialize               = pl_initialize_graphics,
        .resize                   = pl_resize,
        .begin_frame              = pl_begin_frame,
        .end_frame                = pl_end_gfx_frame,
        .begin_recording          = pl_begin_recording,
        .end_recording            = pl_end_recording,
        .draw_areas               = pl_draw_areas,
        .draw_lists               = pl_draw_list,
        .cleanup                  = pl_shutdown,
        .create_font_atlas        = pl_create_vulkan_font_texture,
        .destroy_font_atlas       = pl_cleanup_vulkan_font_texture,
        .add_3d_triangle_filled   = pl__add_3d_triangle_filled,
        .add_3d_line              = pl__add_3d_line,
        .add_3d_point             = pl__add_3d_point,
        .add_3d_transform         = pl__add_3d_transform,
        .add_3d_frustum           = pl__add_3d_frustum,
        .add_3d_centered_box      = pl__add_3d_centered_box,
        .add_3d_bezier_quad       = pl__add_3d_bezier_quad,
        .add_3d_bezier_cubic      = pl__add_3d_bezier_cubic,
        .register_3d_drawlist     = pl__register_3d_drawlist,
        .submit_3d_drawlist       = pl__submit_3d_drawlist
    };
    return &tApi;
}

static const plDeviceI*
pl_load_device_api(void)
{
    static const plDeviceI tApi = {
        .create_index_buffer  = pl_create_index_buffer,
        .create_vertex_buffer = pl_create_vertex_buffer
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_context(ptDataRegistry->get_data("ui"));
    gptFile = ptApiRegistry->first(PL_API_FILE);
    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_GRAPHICS), pl_load_graphics_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE), pl_load_device_api());

        // find log channel
        uint32_t uChannelCount = 0;
        plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
        for(uint32_t i = 0; i < uChannelCount; i++)
        {
            if(strcmp(ptChannels[i].pcName, "Vulkan") == 0)
            {
                uLogChannel = i;
                break;
            }
        }
    }
    else
    {
        ptApiRegistry->add(PL_API_GRAPHICS, pl_load_graphics_api());
        ptApiRegistry->add(PL_API_DEVICE, pl_load_device_api());      
        uLogChannel = pl_add_log_channel("Vulkan", PL_CHANNEL_TYPE_CYCLIC_BUFFER);
    }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_ui_vulkan.c"