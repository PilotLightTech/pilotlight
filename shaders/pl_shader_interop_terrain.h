#ifndef PL_SHADER_INTEROP_TERRAIN_H
#define PL_SHADER_INTEROP_TERRAIN_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_shader_interop.h"

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

PL_BEGIN_ENUM(plTerrainShaderFlags)
    PL_ENUM_ITEM(PL_TERRAIN_SHADER_FLAGS_NONE,    0)
    PL_ENUM_ITEM(PL_TERRAIN_SHADER_FLAGS_WIREFRAME,    1 << 0)
    PL_ENUM_ITEM(PL_TERRAIN_SHADER_FLAGS_SHOW_LEVELS,    1 << 1)
PL_END_ENUM

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

PL_BEGIN_STRUCT(plGpuDynTerrainData)
    mat4 tMvp;
    int iLevel;
    int tFlags;
PL_END_STRUCT(plGpuDynTerrainData)

#endif // PL_SHADER_INTEROP_TERRAIN_H