#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

/*
   gbuffer_fill.vert
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] specialization constants
// [SECTION] bind group 0
// [SECTION] bind group 1
// [SECTION] dynamic bind group
// [SECTION] input & output
// [SECTION] entry
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "bg_scene.inc"
#include "bg_view.inc"

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynDeferredLighting tData;
} tObjectInfo;

//-----------------------------------------------------------------------------
// [SECTION] input & output
//-----------------------------------------------------------------------------

// input
layout(location = 0) in vec3 inPos;


//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

void main() 
{

    vec4 inPosition  = vec4(inPos, 1.0);

    mat4 tTransform = mat4(0);
    if(tObjectInfo.tData.iLightIndex != -1)
    {
        const plGpuLight tLight = tLightInfo.atData[tObjectInfo.tData.iLightIndex];
        tTransform = mat4(
            tLight.fRange, 0.0, 0.0, 0.0,
            0.0, tLight.fRange, 0.0, 0.0,
            0.0, 0.0, tLight.fRange, 0.0,
            tLight.tPosition.x, tLight.tPosition.y, tLight.tPosition.z, 1.0);
    }
    else if(tObjectInfo.tData.iProbeIndex != -1)
    {
        const plGpuProbe tProbe = tProbeData.atData[tObjectInfo.tData.iProbeIndex];
        float fRange = sqrt(tProbe.fRangeSqr);
        tTransform = mat4(
            fRange, 0.0, 0.0, 0.0,
            0.0, fRange, 0.0, 0.0,
            0.0, 0.0, fRange, 0.0,
            tProbe.tPosition.x, tProbe.tPosition.y, tProbe.tPosition.z, 1.0);
    }
    
    vec4 pos = tTransform * inPosition;
    gl_Position = tViewInfo2.data[tObjectInfo.tData.uGlobalIndex].tCameraViewProjection * pos;
}