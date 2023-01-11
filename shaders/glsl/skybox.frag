#version 450
#extension GL_ARB_separate_shader_objects : enable

//-----------------------------------------------------------------------------
// [SECTION] shader input/output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in struct plShaderOut {
    vec3 tPosition;
    vec3 tWorldPosition;
} tShaderIn;

// output
layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform samplerCube samplerCubeMap;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

layout(constant_id = 0) const int MeshVariantFlags = 0;
layout(constant_id = 1) const int VertexStride = 0;
layout(constant_id = 2) const int ShaderTextureFlags = 0;

const float PI = 3.1415926538;
const float PI_2 = 3.1415926538 / 2.0;

void main() 
{
    // const vec3 tSunPos = vec3(10.0, 2.0, 0.0);

    vec3 tVectorOut = normalize(tShaderIn.tWorldPosition);
    // const float fSunAngle = asin(tSunPos.y / length(tSunPos)) / PI_2;

    // // near sun
    // outColor = vec4( 0.65 - fSunAngle, 0.65 - fSunAngle * 0.5, 0.75 - fSunAngle * 0.25, 1.0);
    // outColor = clamp(outColor, 0, 1);

    // outColor.b = outColor.b * fSunAngle;
    // outColor.xy = outColor.xy * fSunAngle * 2.0;
    // outColor.r = outColor.r + fSunAngle * (1 - tVectorOut.y);

    // // sun
    // const float fStrength = floor(0.001 + dot(tSunPos, tVectorOut) / length(tSunPos));
    // outColor = outColor + clamp(vec4(fStrength, fStrength, fStrength, 1.0), 0, 1);

    outColor = texture(samplerCubeMap, tVectorOut);
}