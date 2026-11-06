#include "pl_sky.inc"
#include "pl_bg_scene.inc"
#include "pl_bg_view.inc"
#include "pl_cloud_transport.glsl"

// Reuse the aerial composite layout. Slot 2 is not needed by this pass.
layout(set = 2, binding = 0, rgba16f) uniform image2D tColorImage;
layout(set = 2, binding = 1) uniform texture2D tDepthImage;
layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    uint uCameraIndex;
} tObjectInfo;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy), size = imageSize(tColorImage);
    ivec2 viewport = ivec2(tViewInfo.tData.tViewportSize.xy);
#ifdef PL_CLOUD_LOW_RES
    if(any(greaterThanEqual(pixel, min(size, (viewport + 1) / 2))))
        return;
    ivec2 sourcePixel = min(pixel * 2 + 1, viewport - 1);
    imageStore(tColorImage, pixel, vec4(0.0, 0.0, 0.0, 1.0));
#else
    if(any(greaterThanEqual(pixel, min(size, viewport))))
        return;
    ivec2 sourcePixel = pixel;
#endif
    if(tGpuScene.tData.tCloudLayer.x <= 0.0 || tGpuScene.tData.tCloudLayer.w <= 0.0)
        return;

    float scale = max(tGpuScene.tData.fAtmosphereConversion, 1e-7);
    // Render targets have spare capacity for resizing. Projection coordinates
    // must use the active viewport, just like the G-buffer and sky passes.
    vec2 uv = cloudRayUV(vec2(sourcePixel) + 0.5, vec2(viewport));
    mat4 invProjection = tViewInfo.tData.tCameraProjectionInv[tObjectInfo.uCameraIndex];
    mat4 invView = tViewInfo.tData.tCameraViewInv[tObjectInfo.uCameraIndex];
    vec4 nearH = invProjection * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec3 nearView = nearH.xyz / nearH.w;
    // Perspective and orthographic cameras both reconstruct from their matrices.
    bool perspective = abs(tViewInfo.tData.tCameraProjection[tObjectInfo.uCameraIndex][3][3]) < 0.5;
    vec3 ray = normalize(mat3(invView) * (perspective ? nearView : vec3(0,0,1)));
    vec3 worldOrigin = perspective ? tViewInfo.tData.tCameraPos.xyz : (invView * vec4(nearView, 1.0)).xyz;
    float maxDistance = 80.0;
    float depth = texelFetch(sampler2D(tDepthImage, tSamplerNearestClamp), sourcePixel, 0).r;
    if(depth > 0.0)
    {
        vec4 viewH = invProjection * vec4(uv * 2.0 - 1.0, depth, 1.0);
        vec3 world = (invView * vec4(viewH.xyz / viewH.w, 1.0)).xyz;
        maxDistance = min(maxDistance, max(dot(world - worldOrigin, ray) * scale, 0.0));
    }
    vec4 transport = cloudIntegrateTransport(worldOrigin, ray, maxDistance);
#ifdef PL_CLOUD_LOW_RES
    imageStore(tColorImage, pixel, transport);
#else
    vec4 background = imageLoad(tColorImage, pixel);
    imageStore(tColorImage, pixel, vec4(transport.rgb + background.rgb * transport.a, background.a));
#endif
}
