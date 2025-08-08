#ifndef BG_VIEW_INC
#define BG_VIEW_INC

#include "pl_shader_interop_renderer.h"
#include "lights.glsl"

#ifndef PL_VIEW_SET_LOCATION
#define PL_VIEW_SET_LOCATION 1
#endif

layout(set = PL_VIEW_SET_LOCATION, binding = 0) readonly uniform _plViewInfo
{
    plGpuViewData data[];
} tViewInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 1) readonly buffer _tViewInfo2
{
    plGpuViewData data[];
} tViewInfo2;

layout(set = PL_VIEW_SET_LOCATION, binding = 2) uniform _plLightInfo
{
    plGpuLight atData[1];
} tLightInfo;

layout(set = PL_VIEW_SET_LOCATION, binding = 3) readonly buffer plDShadowData
{
    plGpuLightShadow atData[];
} tDShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 4) readonly buffer plShadowData
{
    plGpuLightShadow atData[];
} tShadowData;

layout(set = PL_VIEW_SET_LOCATION, binding = 5) readonly buffer plProbeData
{
    plGpuProbe atData[];
} tProbeData;

#endif // BG_VIEW_INC