#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//-----------------------------------------------------------------------------
// [SECTION] bind group 0
//-----------------------------------------------------------------------------

struct BindGroup_0
{
    texture2d<float> inputImage;
    texture2d<float, access::write> outputImage;
};

//-----------------------------------------------------------------------------
// [SECTION] dynamic bind group
//-----------------------------------------------------------------------------

struct DynamicData
{
    float4 tJumpDistance;
};

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

static inline __attribute__((always_inline))
float3 jump(float3 minSeed, thread const int2& current, thread const int2& offset, texture2d<float> inputImage)
{
    int2 samplePos = current + offset;
    if (abs(float(clamp(samplePos.x, 0, int2(inputImage.get_width(), inputImage.get_height()).x) - samplePos.x)) > .0001)
    {
        return minSeed;
    }
    if (abs(float(clamp(samplePos.y, 0, int2(inputImage.get_width(), inputImage.get_height()).y) - samplePos.y)) > .0001)
    {
        return minSeed;
    }
    float2 seed = inputImage.read(uint2(samplePos)).xy;
    float2 cScaled = float2(current);
    float2 sScaled = floor(seed * float2(int2(inputImage.get_width(), inputImage.get_height())));
    float dist = distance(cScaled, sScaled);
    if (dist < minSeed.z)
    {
        return float3(seed.x, seed.y, dist);
    }
    return minSeed;
}

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

kernel void
kernel_main(
    device const BindGroup_0& bg0 [[ buffer(0) ]],
    device const DynamicData& tDynamicData [[ buffer(1) ]],
    uint3 tWorkGroup [[threadgroup_position_in_grid]],
    uint3 tLocalIndex [[thread_position_in_threadgroup]])
{
    const int iXCoord = int((tWorkGroup.x * 16) + tLocalIndex.x);
    const int iYCoord = int((tWorkGroup.y * 16) + tLocalIndex.y);

    // const int iXCoord = int(tWorkGroup.x);
    // const int iYCoord = int(tWorkGroup.y);

    float4 outColor = float4(0.0);

    int2 jumpDist = int2(int(tDynamicData.tJumpDistance.x));
    
    float3 curr = float3(1,1,9999999);
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2( 0,  0), bg0.inputImage); // cc
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2( 0, +1), bg0.inputImage); // nn
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2(+1, +1), bg0.inputImage); // ne
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2(+1,  0), bg0.inputImage); // ee
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2(+1, -1), bg0.inputImage); // se
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2( 0, -1), bg0.inputImage); // ss
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2(-1, -1), bg0.inputImage); // sw
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2(-1,  0), bg0.inputImage); // ww
    curr = jump(curr, int2(iXCoord, iYCoord), jumpDist * int2(-1, +1), bg0.inputImage); // nw
    outColor = float4(curr.x, curr.y, 0, 1);
    // float fWidth = float(bg0.inputImage.get_width());
    // float fHeight = float(bg0.inputImage.get_height());
    // outColor = float4(float(iXCoord)/float(bg0.inputImage.get_width()), float(iYCoord)/float(bg0.inputImage.get_height()), 0, 1);

    bg0.outputImage.write(outColor, uint2(int2(iXCoord, iYCoord)));
    // bg0.outputImage.write(outColor, uint2(int2(iXCoord, iYCoord)));
}