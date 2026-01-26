#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA {
    mat4 mvp;
} tDynamicData;

void main() {
    gl_Position = tDynamicData.mvp * vec4(inPos, 1.0);
}