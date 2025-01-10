#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform _plGlobalInfo
{
    vec4 tViewportSize;
    vec4 tCameraPos;
    mat4 tCameraView;
    mat4 tCameraProjection;
    mat4 tCameraViewProjection;
    uint uLambertianEnvSampler;
    uint uGGXEnvSampler;
    uint uGGXLUT;
    uint _uUnUsed;
} tGlobalInfo;

layout(set = 0, binding = 1)  uniform sampler tDefaultSampler;

//-----------------------------------------------------------------------------
// [SECTION] bind group 1
//-----------------------------------------------------------------------------

layout(set = 1, binding = 0) uniform texture2D samplerCubeMap;

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA { mat4 tModel;} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input
//-----------------------------------------------------------------------------

layout(location = 0) in struct plShaderOut {
    vec3 tWorldPosition;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] output
//-----------------------------------------------------------------------------

layout(location = 0) out vec4 outColor;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

vec3 sampleCube(vec3 v)
{
	vec3 vAbs = abs(v);
	float ma;
	vec2 uv;
    float faceIndex = 0.0;
	if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
	{
		faceIndex = v.z < 0.0 ? 1.0 : 0.0;
		ma = 0.5 / vAbs.z;
		uv = vec2(v.z < 0.0 ? -v.x : v.x, -v.y);
	}
	else if(vAbs.y >= vAbs.x)
	{
		faceIndex = v.y < 0.0 ? 5.0 : 4.0;
		ma = 0.5 / vAbs.y;
		uv = vec2(v.x, v.y < 0.0 ? -v.z : v.z);
	}
	else
	{
		faceIndex = v.x < 0.0 ? 3.0 : 2.0;
		ma = 0.5 / vAbs.x;
		uv = vec2(v.x < 0.0 ? v.z : -v.z, -v.y);
	}
	vec2 result = uv * ma + vec2(0.5, 0.5);
    
    return vec3(result, faceIndex);
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void
main() 
{
    vec3 tVectorOut = normalize(tShaderIn.tWorldPosition);

    const vec2 faceoffsets[6] = {
        vec2(0.0, 0.0 / 3.0),
        vec2(0.5, 0.0 / 3.0),
        vec2(0.0, 1.0 / 3.0),
        vec2(0.5, 1.0 / 3.0),
        vec2(0.0, 2.0 / 3.0),
        vec2(0.5, 2.0 / 3.0)
    };

    vec3 result = sampleCube(tVectorOut);
    result.xy *= vec2(0.5, 1.0/3.0);

    outColor = vec4(texture(sampler2D(samplerCubeMap, tDefaultSampler), result.xy + faceoffsets[int(result.z)]).rgb, 1.0);
    // outColor = vec4(texture(samplerCube(samplerCubeMap, tDefaultSampler), tVectorOut).rgb, 1.0);
}