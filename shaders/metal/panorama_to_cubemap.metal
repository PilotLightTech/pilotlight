#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#define M_PI 3.1415926535897932384626433832795

struct BindGroup_0
{

    device float4* tInputBuffer;
    device float4* FaceOut_0;
    device float4* FaceOut_1;
    device float4* FaceOut_2;
    device float4* FaceOut_3;
    device float4* FaceOut_4;
    device float4* FaceOut_5;
};

constant int resolution [[ function_constant(0) ]];
constant int width [[ function_constant(1) ]];
constant int height [[ function_constant(2) ]];

float3 uvToXYZ(int face, float2 uv)
{
    if (face == 0) // right
    return float3(1.f, -uv.y, -uv.x);

    else if (face == 1) // left
    return float3(-1.f, -uv.y, uv.x);

    else if (face == 2) // top
    return float3(uv.x, 1.f, +uv.y);

    else if (face == 3) // bottom
    return float3(uv.x, -1.f, -uv.y);

    else if (face == 4) // front
    return float3(uv.x, -uv.y, 1.f);

    else //if(face == 5)
    return float3(-uv.x, -uv.y, -1.f);
}

float2 dirToUV(float3 dir)
{
    return float2(
    0.5 + 0.5 * atan2(dir.z, dir.x) / M_PI,
    1.0 - acos(dir.y) / M_PI);
}

void writeFace(device BindGroup_0& bg0, int pixel, int face, float3 colorIn)
{
    float4 color = float4(colorIn.rgb, 1.0f);

    if (face == 0)
    bg0.FaceOut_0[pixel] = color;
    else if (face == 1)
    bg0.FaceOut_1[pixel] = color;
    else if (face == 2)
    bg0.FaceOut_2[pixel] = color;
    else if (face == 3)
    bg0.FaceOut_3[pixel] = color;
    else if (face == 4)
    bg0.FaceOut_4[pixel] = color;
    else //if(face == 5)
    bg0.FaceOut_5[pixel] = color;
}

kernel void kernel_main(device BindGroup_0& bg0 [[ buffer(0) ]], uint3 workgroup [[threadgroup_position_in_grid]], uint3 localindex [[thread_position_in_threadgroup]])
{
    const float xcoord = workgroup.x*16 + localindex.x;
    const float ycoord = workgroup.y*16 + localindex.y;
    const int face = int(workgroup.z * 3 + localindex.z);
    const float xinc = 1.0 / resolution;
    const float yinc = 1.0 / resolution;
    const float2 inUV = float2(xcoord * xinc, ycoord * yinc);
    const int currentPixel = int(xcoord + ycoord * resolution);

    float2 texCoordNew = inUV * 2.0 - 1.0;
    float3 scan = uvToXYZ(face, texCoordNew);
    float3 direction = normalize(scan);
    float2 src = dirToUV(direction);

    int columnindex = int(width - floor(src.x * width));
    int rowindex = int(height - floor(src.y * height));

    int srcpixelIndex = columnindex + rowindex * width;

    float3 color = float3(bg0.tInputBuffer[srcpixelIndex].r, bg0.tInputBuffer[srcpixelIndex].g, bg0.tInputBuffer[srcpixelIndex].b);
    writeFace(bg0, currentPixel, face, color);
}