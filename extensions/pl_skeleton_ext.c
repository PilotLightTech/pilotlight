/*
   pl_skeleton_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_skeleton_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_log_ext.h"
#include "pl_vfs_ext.h"
#include "pl_resource_ext.h"
#include "pl_asset_ext.h"
#include "pl_string_intern_ext.h"

// libraries
#include "pl_json.h"
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plLogI*          gptLog      = NULL;
    static const plVfsI*          gptVfs      = NULL;
    static const plResourceI*     gptResource = NULL;
    static const plAssetI*        gptAsset    = NULL;
    static const plMemoryI*       gptMemory   = NULL;
    static const plStringInternI* gptString   = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)


    #ifndef PL_JSON_ALLOC
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSkeletonContext
{
    int a;
} plSkeletonContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plSkeletonContext* gptSkeletonCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

void
pl_skeleton_serialize(const char* pcName, const plSkeleton* ptSkeleton)
{
    plJsonObject* ptRoot = pl_json_new_root_object("root");
    pl_json_add_string_member(ptRoot, "format", "plskeleton");
    pl_json_add_uint_member(ptRoot, "version", 1);

    plJsonObject* ptJoints = pl_json_add_member_array(ptRoot, "joints", ptSkeleton->uJointCount);

    for(uint32_t i = 0; i < ptSkeleton->uJointCount; i++)
    {
        plJsonObject* ptJoint = pl_json_member_by_index(ptJoints, i);

        pl_json_add_string_member(ptJoint, "name", ptSkeleton->atJoints[i].pcName);
        pl_json_add_int_member(ptJoint, "parent", ptSkeleton->atJoints[i].uParent == UINT32_MAX ? -1 : (int)ptSkeleton->atJoints[i].uParent);
        pl_json_add_float_array(ptJoint, "translation", ptSkeleton->atJoints[i].tTranslation.d, 3);
        pl_json_add_float_array(ptJoint, "rotation", ptSkeleton->atJoints[i].tRotation.d, 4);
        pl_json_add_float_array(ptJoint, "scale", ptSkeleton->atJoints[i].tScale.d, 3);
    }

    uint32_t uBufferSize = 0;
    pl_write_json(ptRoot, NULL, &uBufferSize);
    char* pcBuffer = PL_ALLOC(uBufferSize);
    memset(pcBuffer, 0, uBufferSize);
    pl_write_json(ptRoot, pcBuffer, &uBufferSize);
    
    gptVfs->register_file(pcName, false);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
    gptVfs->close_file(tFileHandle);

    PL_FREE(pcBuffer);
    pl_unload_json(&ptRoot);
}

void
pl_skeleton_deserialize(const char* pcName, plSkeleton* ptSkeleton)
{
    if(!gptVfs->does_file_exist(pcName))
        return;

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tFileHandle);

    plJsonObject* ptRoot = NULL;
    pl_load_json((const char*)puFileBuffer, &ptRoot);

    uint32_t uVersion = pl_json_uint_member(ptRoot, "version", 0);


    plJsonObject* ptJoints = pl_json_array_member(ptRoot, "joints", &ptSkeleton->uJointCount);
    ptSkeleton->atJoints = PL_ALLOC(ptSkeleton->uJointCount * sizeof(plSkeletonJoint));
    memset(ptSkeleton->atJoints, 0, ptSkeleton->uJointCount * sizeof(plSkeletonJoint));

    char acTempBuffer[256] = {0};

    for(uint32_t i = 0; i < ptSkeleton->uJointCount; i++)
    {
        plJsonObject* ptJoint = pl_json_member_by_index(ptJoints, i);

        pl_json_string_member(ptJoint, "name", acTempBuffer, 256);
        ptSkeleton->atJoints[i].pcName = gptString->intern(NULL, acTempBuffer);
        int iParent = pl_json_int_member(ptJoint, "parent", -1);
        ptSkeleton->atJoints[i].uParent = iParent == -1 ? UINT32_MAX : iParent;
        pl_json_float_array_member(ptRoot, "translation", ptSkeleton->atJoints[i].tTranslation.d, NULL);
        pl_json_float_array_member(ptRoot, "rotation", ptSkeleton->atJoints[i].tRotation.d, NULL);
        pl_json_float_array_member(ptRoot, "scale", ptSkeleton->atJoints[i].tScale.d, NULL);
    }

    PL_FREE(puFileBuffer);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_skeleton_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plSkeletonI tApi = {
        .serialize   = pl_skeleton_serialize,
        .deserialize = pl_skeleton_deserialize
    };
    pl_set_api(ptApiRegistry, plSkeletonI, &tApi);

    gptLog      = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVfs      = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptResource = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptMemory   = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAsset    = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptString   = pl_get_api_latest(ptApiRegistry, plStringInternI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptSkeletonCtx = ptDataRegistry->get_data("plSkeletonContext");
    }
    else // first load
    {
        static plSkeletonContext tCtx = {0};
        gptSkeletonCtx = &tCtx;
        ptDataRegistry->set_data("plSkeletonContext", gptSkeletonCtx);
    }
}

void
pl_unload_skeleton_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plSkeletonI* ptApi = pl_get_api_latest(ptApiRegistry, plSkeletonI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_JSON_IMPLEMENTATION
    #include "pl_json.h"
    #undef PL_JSON_IMPLEMENTATION

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif