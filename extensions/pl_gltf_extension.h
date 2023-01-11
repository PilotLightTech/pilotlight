/*
   pl_gltf_extension.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GLTF_EXTENSION_H
#define PL_GLTF_EXTENSION_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

typedef struct _plMaterial plMaterial;
typedef struct _plMesh plMesh;
typedef struct _plRenderer plRenderer;
typedef struct _plGltf plGltf;
typedef struct _plBindGroup plBindGroup;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

bool pl_ext_load_gltf(plRenderer* ptRenderer, const char* pcPath, plGltf* ptGltfOut);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plGltf
{
   const char*  pcPath;
   plMaterial*  sbtMaterials;
   plMesh*      sbtMeshes;
   uint32_t*    sbuVertexOffsets;
   uint32_t*    sbuMaterialIndices;
} plGltf;

#endif // PL_GLTF_EXTENSION_H