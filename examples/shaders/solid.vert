#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable


layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    mat4 tCameraViewProjection;
    vec4 tCameraPosHigh;
    vec4 tCameraPosLow;
} tObjectInfo;

layout(location = 0) in vec3 inHighPos;
layout(location = 1) in vec3 inLowPos;

layout(location = 0) out struct plShaderOut {
    vec3 tPosition;
    vec3 tActualPosition;
} tShaderIn;

void main()
{
    // vec3 highDifference = inHighPos - tObjectInfo.tCameraPosHigh.xyz;
    // vec3 lowDifference = inLowPos - tObjectInfo.tCameraPosLow.xyz;

    vec3 t1 = inLowPos - tObjectInfo.tCameraPosLow.xyz;
    vec3 e = t1 - inLowPos;

    vec3 t2 = ((-tObjectInfo.tCameraPosLow.xyz - e) + (inLowPos - (t1 - e))) + inHighPos - tObjectInfo.tCameraPosHigh.xyz;
    vec3 highDifference = t1 + t2;
    vec3 lowDifference = t2 - (highDifference - t1);


    tShaderIn.tPosition = lowDifference + highDifference;
    tShaderIn.tActualPosition = inHighPos;
    gl_Position = tObjectInfo.tCameraViewProjection * vec4(tShaderIn.tPosition, 1.0);
}