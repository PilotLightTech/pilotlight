#version 450
#extension GL_ARB_separate_shader_objects : enable

// input
layout(location = 0) in vec3 inPos;

// output
layout(location = 0) out struct plShaderOut {
    vec4 tColor;
} tShaderOut;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    mat4 mvp;
} tDynamicData;

void main() 
{
    gl_Position = tDynamicData.mvp * vec4(inPos, 1.0);
    // gl_Position = tDynamicData.mvp * vec4(inPos.x, 0.0, inPos.z, 1.0);
    float fScale = max(inPos.y / 30.0, 0.2);
    tShaderOut.tColor = vec4(fScale.xxx, 1.0);
    // tShaderOut.tColor = vec4(1.0);
}