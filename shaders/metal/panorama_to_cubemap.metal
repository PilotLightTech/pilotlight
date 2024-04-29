#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#define PL_PI 3.1415926535897932384626433832795

struct BindGroup_0
{

    device float4* tInputBuffer;
    device float4* atFaceOut0;
    device float4* atFaceOut1;
    device float4* atFaceOut2;
    device float4* atFaceOut3;
    device float4* atFaceOut4;
    device float4* atFaceOut5;
};

constant int iResolution [[ function_constant(0) ]];
constant int iWidth [[ function_constant(1) ]];
constant int iHeight [[ function_constant(2) ]];

float3
pl_uv_to_xyz(int iFace, float2 tUv)
{
    if (iFace == 0) // right
        return float3(1.0, -tUv.y, -tUv.x);
    else if (iFace == 1) // left
        return float3(-1.0, -tUv.y, tUv.x);
    else if (iFace == 2) // top
        return float3(tUv.x, 1.0, +tUv.y);
    else if (iFace == 3) // bottom
        return float3(tUv.x, -1.0, -tUv.y);
    else if (iFace == 4) // front
        return float3(tUv.x, -tUv.y, 1.0);
    else //if(iFace == 5)
        return float3(-tUv.x, -tUv.y, -1.0);
}

float2
pl_direction_to_UV(float3 tDir)
{
    return float2(0.5 + ((0.5 * precise::atan2(tDir.z, tDir.x)) / PL_PI), 1.0 - (precise::acos(tDir.y) / PL_PI));
}

void
pl_write_face(device BindGroup_0& tBg0, int iPixel, int iFace, float3 tColorIn)
{
    float4 tColor = float4(tColorIn.rgb, 1.0);

    if (iFace == 0)
        tBg0.atFaceOut0[iPixel] = tColor;
    else if (iFace == 1)
        tBg0.atFaceOut1[iPixel] = tColor;
    else if (iFace == 2)
        tBg0.atFaceOut2[iPixel] = tColor;
    else if (iFace == 3)
        tBg0.atFaceOut3[iPixel] = tColor;
    else if (iFace == 4)
        tBg0.atFaceOut4[iPixel] = tColor;
    else //if(iFace == 5)
        tBg0.atFaceOut5[iPixel] = tColor;
}

kernel void
kernel_main(device BindGroup_0& tBg0 [[ buffer(0) ]], uint3 tWorkGroup [[threadgroup_position_in_grid]], uint3 tLocalIndex [[thread_position_in_threadgroup]])
{
    const float fXCoord = tWorkGroup.x * 16 + tLocalIndex.x;
    const float fYCoord = tWorkGroup.y * 16 + tLocalIndex.y;
    const int   iFace   = int((tWorkGroup.z * 3) + tLocalIndex.z);
    
    const float fXinc = 1.0 / float(iResolution);
    const float fYinc = 1.0 / float(iResolution);
    const float2 tInUV = float2(fXCoord * fXinc, fYCoord * fYinc);
    const int iCurrentPixel = int(fXCoord + (fYCoord * float(iResolution)));

    const float2 tTexCoordNew = tInUV * 2.0 - float2(1.0);
    const float3 tScan = pl_uv_to_xyz(iFace, tTexCoordNew);
    const float3 tDirection = fast::normalize(tScan);
    const float2 tSrc = pl_direction_to_UV(tDirection);

    const int iColumnindex = int(float(iWidth) - floor(tSrc.x * float(iWidth)));
    const int iRowindex = int(float(iHeight) - floor(tSrc.y * float(iHeight)));

    const int iSrcPixelIndex = iColumnindex + iRowindex * iWidth;

    const float3 tColor = float3(tBg0.tInputBuffer[iSrcPixelIndex].r, tBg0.tInputBuffer[iSrcPixelIndex].g, tBg0.tInputBuffer[iSrcPixelIndex].b);
    pl_write_face(tBg0, iCurrentPixel, iFace, tColor);
}