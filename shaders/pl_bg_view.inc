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

layout(set = PL_VIEW_SET_LOCATION, binding = 1) readonly buffer _tViewInfo2
{
    plGpuViewData data[];
} tViewInfo2;

layout(set = PL_VIEW_SET_LOCATION, binding = 2) uniform _plPointLightInfo
{
    plGpuPointLight atData[1];
} tPointLightInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 3) uniform _plSpotLightInfo
{
    plGpuSpotLight atData[1];
} tSpotLightInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 4) uniform _plDirectionLightInfo
{
    plGpuDirectionLight atData[1];
} tDirectionLightInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 5) readonly buffer plDirectionShadowData
{
    plGpuDirectionLightShadow atData[];
} tDirectionShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 6) readonly buffer plPointShadowData
{
    plGpuPointLightShadow atData[];
} tPointShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 7) readonly buffer plSpotShadowData
{
    plGpuSpotLightShadow atData[];
} tSpotShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 8) readonly buffer plProbeData
{
    plGpuProbe atData[];
} tProbeData;

#endif // BG_VIEW_INC