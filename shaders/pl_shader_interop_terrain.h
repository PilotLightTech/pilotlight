#ifndef PL_SHADER_INTEROP_TERRAIN_H
#define PL_SHADER_INTEROP_TERRAIN_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_shader_interop.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_TERRAIN_MAX_BINDLESS_TEXTURES 4096

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

PL_BEGIN_ENUM(plTerrainShaderFlags)
    PL_ENUM_ITEM(PL_TERRAIN_SHADER_FLAGS_NONE,             0)
    PL_ENUM_ITEM(PL_TERRAIN_SHADER_FLAGS_WIREFRAME,        1 << 0)
    PL_ENUM_ITEM(PL_TERRAIN_SHADER_FLAGS_SHOW_DEBUG_COLOR, 1 << 1)
PL_END_ENUM

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuTerrainElevationZone)
    vec4 tElevation;
    // x = min elevation
    // y = max elevation
    // z = blend size
    // w = unused

    int  iFlatMaterialIndex;
    int  iSteepMaterialIndex;
    int  _unused0;
    int  _unused1;
PL_END_STRUCT(plGpuTerrainElevationZone)

PL_BEGIN_STRUCT(plGpuDynTerrainData)

    int iLevel;
    int tFlags;
    int iZoneCount;
    uint uGlobalIndex;

    vec4 tUVInfo;

    int  iProbeCount;
    int  iPointLightCount;
    int  iSpotLightCount;
    int  iDirectionLightCount;

    float    fSlopeStart;
    float    fSlopeEnd;
    float    fPadding0;
    float    fPadding1;

    vec4 tDebugColor;

    plGpuTerrainElevationZone atElevationZones[5];

PL_END_STRUCT(plGpuDynTerrainData)

#endif // PL_SHADER_INTEROP_TERRAIN_H