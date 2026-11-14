#ifndef PL_SHADER_INTEROP_TERRAIN_H
#define PL_SHADER_INTEROP_TERRAIN_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_shader_interop_renderer.h"

// Byte comments give each group size and its offset from the start of the struct.
// Totals match the C layout and std430 array stride; scalar arrays require std430.

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
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~
    // x = min elevation
    // y = max elevation
    // z = blend size
    // w = unused

    int  iFlatMaterialIndex;
    int  iSteepMaterialIndex;
    int  _unused0;
    int  _unused1;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~32 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuTerrainElevationZone)

PL_BEGIN_STRUCT(plGpuDynTerrainData)

    uint uProbeSelection;
    uint _uProbeSelectionPadding0;
    uint _uProbeSelectionPadding1;
    uint _uProbeSelectionPadding2;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 0)~~~~~~~~~~~~~~~~

    int iLevel;
    int tFlags;
    int iZoneCount;
    uint uGlobalIndex;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 16)~~~~~~~~~~~~~~~~

    vec4 tUVInfo;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 32)~~~~~~~~~~~~~~~~

    int  _iUnusedProbeCount;
    int  iPointLightCount;
    int  iSpotLightCount;
    int  iDirectionLightCount;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 48)~~~~~~~~~~~~~~~~

    float    fSlopeStart;
    float    fSlopeEnd;
    float    fPadding0;
    float    fPadding1;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 64)~~~~~~~~~~~~~~~~

    vec4 tDebugColor;
    // ~~~~~~~~~~~~~~~~16 bytes (offset 80)~~~~~~~~~~~~~~~~

    plGpuTerrainElevationZone atElevationZones[5];
    // ~~~~~~~~~~~~~~~~160 bytes (offset 96)~~~~~~~~~~~~~~~~

    // ~~~~~~~~~~~~~~~~256 bytes total~~~~~~~~~~~~~~~~
PL_END_STRUCT(plGpuDynTerrainData)

#endif // PL_SHADER_INTEROP_TERRAIN_H