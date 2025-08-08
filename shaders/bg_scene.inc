#ifndef BG_SCENE_INC
#define BG_SCENE_INC

#include "pl_shader_interop_renderer.h"
#include "lights.glsl"
#include "material_info.glsl"

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(std140, set = 0, binding = 0) buffer _tGpuScene
{
    plGpuSceneData tData;
} tGpuScene;

layout(std140, set = 0, binding = 1) readonly buffer _tVertexBuffer
{
	vec4 atVertexData[];
} tVertexBuffer;

layout(std140, set = 0, binding = 2) readonly buffer _tTransformBuffer
{
	mat4 atTransform[];
} tTransformBuffer;

layout(set = 0, binding = 3) readonly buffer plMaterialInfo
{
    plGpuMaterial atMaterials[];
} tMaterialInfo;

// samplers
layout(set = 0, binding = 4)  uniform sampler tSamplerLinearClamp;
layout(set = 0, binding = 5)  uniform sampler tSamplerNearestClamp;
layout(set = 0, binding = 6)  uniform sampler tSamplerLinearRepeat;
layout(set = 0, binding = 7)  uniform sampler tSamplerNearestRepeat;

layout(set = 0, binding = 8)  uniform texture2D at2DTextures[PL_MAX_BINDLESS_TEXTURES];
layout(set = 0, binding = PL_MAX_BINDLESS_CUBE_TEXTURE_SLOT)  uniform textureCube atCubeTextures[PL_MAX_BINDLESS_TEXTURES];

#endif // BG_SCENE_INC