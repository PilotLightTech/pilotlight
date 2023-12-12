#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct BindGroupData_0
{
    uint uRowStride;  
};

struct BindGroup_0
{
    device BindGroupData_0 *data;  

    device float4* tInputBuffer;
    device float4* tOutputBuffer;
};

constant bool bSwitchChannels [[ function_constant(0) ]];

kernel void kernel_main(device BindGroup_0& bg0 [[ buffer(0) ]], uint3 index [[thread_position_in_grid]])
{
    const uint row_stride = bg0.data->uRowStride;
    uint pixel_x = index[0];
    uint pixel_y = index[1];

    if(bSwitchChannels)
    {
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].r = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].r;
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].g = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].b;
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].b = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].g;
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].a = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].a;
    }
    else
    {
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].r = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].r;
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].g = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].g;
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].b = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].b;
        bg0.tOutputBuffer[pixel_x + pixel_y * row_stride].a = bg0.tInputBuffer[pixel_x + pixel_y * row_stride].a;
    }
}