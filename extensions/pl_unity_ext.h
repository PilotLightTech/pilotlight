/*
   pl_unity_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plAnimationI (v0.1.x)
        * plAssetI (v1.x)
        * plAssetToolsI (v0.1.x)
        * plAtomicsI (v2.x)
        * plAudioI (v1.x)
        * plBVHI (v0.2.x)
        * plCameraEcsI (v0.1.x)
        * plCameraI (v1.x)
        * plCollisionI (v0.2.x)
        * plCompressI (v1.x)
        * plConfigI (v1.x)
        * plConsoleI (v2.x)
        * plDataRegistryI (v1.x)
        * plDateTimeI (v2.x)
        * plDdsI (v2.x)
        * plDialogI (v0.1.x)
        * plDrawI (v4.x)
        * plDxtI (v2.x)
        * plEcsI (v3.x)
        * plExtensionRegistryI (v1.x)
        * plFileI (v2.x)
        * plFreeListI (v0.2.x)
        * plGPUAllocatorsI (v2.x)
        * plGizmoI (v0.3.x)
        * plGjkI (v0.1.x)
        * plGltfI (v0.1.x)
        * plGraphicsI (v2.x)
        * plIOI (v1.x)
        * plIkI (v0.3.x)
        * plImageI (v1.x)
        * plImageOpsI (v0.2.x)
        * plJobI (v2.x)
        * plJsonI (v1.x)
        * plLibraryI (v1.x)
        * plLogI (v2.x)
        * plMaterialI (v0.5.x)
        * plMemoryI (v1.x)
        * plMeshBuilderI (v0.1.x)
        * plMeshI (v0.3.x)
        * plPakI (v2.x)
        * plPathI (v1.x)
        * plPhysicsI (v0.2.x)
        * plProfileI (v2.x)
        * plRectPackI (v2.x)
        * plRendererI (v0.3.x)
        * plResourceI (v1.x)
        * plScreenLogI (v2.x)
        * plScriptI (v0.1.x)
        * plShaderI (v2.x)
        * plShaderLibraryI (v0.3.x)
        * plSkeletonI (v0.1.x)
        * plStageI (v0.3.x)
        * plStarterI (v2.x)
        * plStatsI (v1.x)
        * plStlI (v0.1.x)
        * plStringInternI (v3.x)
        * plTerrainI (v0.1.x)
        * plTextureI (v0.1.x)
        * plThreadsI (v1.x)
        * plTimerI (v1.x)
        * plToolsI (v1.x)
        * plTransformI (v1.x)
        * plUiI (v2.x)
        * plVfsI (v3.x)
        * plVirtualMemoryI (v2.x)
        * plVoxelI (v1.x)
        * plWindowI (v2.x)

    The unity module registers its bundled extensions itself. This list also
    includes lookups of those bundled APIs; they are not separate DLL prerequisites.
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_EXT_H
#define PL_UNITY_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plUnityI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include "pl.h"
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ext(plApiRegistryI*, bool reload);

#ifdef __cplusplus
}
#endif

#endif // PL_UNITY_EXT_H