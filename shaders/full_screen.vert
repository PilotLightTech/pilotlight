#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 tUV;

void main() 
{
    tUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(tUV * 2.0 - 1.0, 0.0, 1.0);
}