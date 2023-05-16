#version 450
#extension GL_ARB_separate_shader_objects : enable

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

void main() 
{
    vec3 tVectorOut = normalize(tShaderIn.tWorldPosition);
    outColor = texture(samplerCubeMap, tVectorOut);
}