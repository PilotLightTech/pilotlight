
#include "pl_shader_interop.h"

PL_BEGIN_STRUCT(plTerrainDynamicData)

    vec4 tPos;
    vec4 tSunDirection;
    mat4 tCameraViewProjection;

    float fMetersPerHeightFieldTexel;
    float fGlobalMaxHeight;
    float fGlobalMinHeight;
    float fXUVOffset;

    float fYUVOffset;
    float fStencilRadius;
    float fBlurRadius;
    int    _iUnused0;

    vec4 tMinMax;

PL_END_STRUCT(plTerrainDynamicData)

PL_BEGIN_STRUCT(plTerrainPrepDynamicData)

    float  fMetersPerHeightFieldTexel;
    float  fMaxHeight;
    float  fMinHeight;

    int    iXOffset;
    int    iYOffset;
    int    iNormalCalcReach;

    float fGlobalMaxHeight;
    float fGlobalMinHeight;

PL_END_STRUCT(plTerrainPrepDynamicData)