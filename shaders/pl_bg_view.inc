#ifndef BG_VIEW_INC
#define BG_VIEW_INC

#include "pl_shader_interop_renderer.h"
#include "pl_lights.glsl"

#ifndef PL_VIEW_SET_LOCATION
#define PL_VIEW_SET_LOCATION 1
#endif

layout(set = PL_VIEW_SET_LOCATION, binding = 0) readonly uniform _plViewInfo
{
    plGpuViewData tData;
} tViewInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 1) uniform _plPointLightInfo
{
    plGpuPointLight atData[PL_MAX_LIGHTS];
} tPointLightInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 2) uniform _plSpotLightInfo
{
    plGpuSpotLight atData[PL_MAX_LIGHTS];
} tSpotLightInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 3) uniform _plDirectionLightInfo
{
    plGpuDirectionLight atData[PL_MAX_LIGHTS];
} tDirectionLightInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 4) readonly buffer plDirectionShadowData
{
    plGpuDirectionLightShadow atData[];
} tDirectionShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 5) readonly buffer plPointShadowData
{
    plGpuPointLightShadow atData[];
} tPointShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 6) readonly buffer plSpotShadowData
{
    plGpuSpotLightShadow atData[];
} tSpotShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 7) readonly buffer plProbeData
{
    plGpuProbe atData[];
} tProbeData;

#ifdef PL_CLUSTER_WRITE
layout(std430, set = PL_VIEW_SET_LOCATION, binding = 8) buffer plClusterData
#else
layout(std430, set = PL_VIEW_SET_LOCATION, binding = 8) readonly buffer plClusterData
#endif
{
    uint auData[];
} tClusterData;

layout(set = PL_VIEW_SET_LOCATION, binding = 9) uniform texture2D tScreenAO;

#endif // BG_VIEW_INC