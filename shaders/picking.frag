#version 450
#extension GL_ARB_separate_shader_objects : enable
precision mediump float;

layout(std140, set = 1, binding = 0) buffer _tBufferOut0{
    uint uID;
    float fDepth;
} tResultOut;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

layout(early_fragment_tests) in;

// input
layout(location = 0) flat in struct plShaderIn {
    vec2 tMousePos;
    uint uID;
} tShaderIn;

// output
layout(location = 0) out vec4 outColor;


//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{
    if(floor(gl_FragCoord.x) == tShaderIn.tMousePos.x)
    {
        if(floor(gl_FragCoord.y) == tShaderIn.tMousePos.y)
        {
            // tResultOut.tPixelColor = tShaderIn.tColor;
            float CurrentDepth = gl_FragCoord.z;
            float MaxDepth = tResultOut.fDepth;
            
            // if(CurrentDepth > MaxDepth)
            {
                tResultOut.uID = tShaderIn.uID;
                tResultOut.fDepth = CurrentDepth;
            }
            
        }
    }
    outColor = vec4(gl_FragCoord.z, 0, 0, 1);
}