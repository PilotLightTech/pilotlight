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
#include "pl_scene_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_log_ext.h"
#include "pl_ecs_ext.h"
#include "pl_vfs_ext.h"
#include "pl_resource_ext.h"
#include "pl_asset_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_camera_ext.h"
#include "pl_json_ext.h"
#include "pl_transform_ext.h"
#include "pl_renderer_ext.h"
#include "pl_animation_ext.h"
#include "pl_skeleton_ext.h"
#include "pl_script_ext.h"
#include "pl_terrain_ext.h"
#include "pl_shader_interop_renderer.h"

// libraries
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plLogI*          gptLog         = NULL;
    static const plVfsI*          gptVfs         = NULL;
    static const plResourceI*     gptResource    = NULL;
    static const plAssetI*        gptAsset       = NULL;
    static const plMemoryI*       gptMemory      = NULL;
    static const plStringInternI* gptString      = NULL;
    static const plEcsI*          gptEcs         = NULL;
    static const plTransformI*    gptTransform   = NULL;
    static const plAnimationI*    gptAnimation   = NULL;
    static const plSkeletonI*     gptSkeleton    = NULL;
    static const plCameraI*       gptCamera      = NULL;
    static const plCameraEcsI*    gptCameraEcs   = NULL;
    static const plRendererEcsI*  gptRendererEcs = NULL;
    static const plScriptI*       gptScript      = NULL;
    static const plJsonI*         gptJson        = NULL;
    static const plTerrainI*      gptTerrain     = NULL;
    static const plIOI*           gptIOI         = NULL;
    static plIO*                  gptIO          = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)


    #ifndef PL_JSON_ALLOC
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_JSON_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
    
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSceneContext
{
    plAssetTypeKey tAssetTypeKey;
    plEcsTypeKey tSceneInstanceComponentType;
} plSceneContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plSceneContext* gptSceneCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

static bool
pl__scene_asset_serialize(const char* pcName, const void* pScene, plAssetEncoding eEncoding)
{
    if(eEncoding == PL_ASSET_ENCODING_BINARY)
        return false;

    const plScene* ptScene = pScene;

    plJsonObject* ptRoot = gptJson->new_root_object("root");
    gptJson->add_string_member(ptRoot, "format", "plscene");
    gptJson->add_uint32_member(ptRoot, "version", 1);
    if(gptAsset->is_valid(ptScene->tEnvironment))
        gptJson->add_string_member(ptRoot, "environment", gptAsset->get_path(ptScene->tEnvironment));
    if(gptAsset->is_valid(ptScene->tRendererSettings))
        gptJson->add_string_member(ptRoot, "renderer", gptAsset->get_path(ptScene->tRendererSettings));

    
    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = gptEcs->get_type_descriptions(&atCompDescs);

    uint32_t uEntityCount = 0;
    gptEcs->get_entities(ptScene->ptLibrary, NULL, &uEntityCount);
    plEntity* atEntities = PL_ALLOC(uEntityCount * sizeof(plEntity));
    gptEcs->get_entities(ptScene->ptLibrary, atEntities, &uEntityCount);
    
    plJsonObject* ptJsonEntities = gptJson->add_member_array(ptRoot, "entities", uEntityCount);

    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(ptJsonEntities, uEntityIndex);

        plEntity tEntity = atEntities[uEntityIndex];
        plEntityId tEntityId = gptEcs->get_entity_id(ptScene->ptLibrary, tEntity);

        gptJson->add_uint64_member(ptJsonNode, "id", tEntityId);

        for(uint32_t uComponentIndex = 0; uComponentIndex < uComponentDescCount; uComponentIndex++)
        {
            const plComponentDesc* ptDesc = &atCompDescs[uComponentIndex];

            if(gptEcs->has_component(ptScene->ptLibrary, ptDesc->tTypeKey, tEntity))
            {
                plJsonObject* ptJsonComponent = gptJson->add_member(ptJsonNode, ptDesc->pcName);
                void* pComponent = gptEcs->get_component(ptScene->ptLibrary, ptDesc->tTypeKey, tEntity);
                if(ptDesc->serialize)
                {
                    ptDesc->serialize(pComponent, ptJsonComponent);
                }
                else
                {
                    PL_ASSERT(false && "Component needs serialization implemented");
                }
            }
        }
    }

    PL_FREE(atEntities);

    uint32_t uBufferSize = 0;
    gptJson->write(ptRoot, NULL, &uBufferSize);
    char* pcBuffer = PL_ALLOC(uBufferSize);
    memset(pcBuffer, 0, uBufferSize);
    gptJson->write(ptRoot, pcBuffer, &uBufferSize);
    
    gptVfs->register_file(pcName, false);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
    gptVfs->close_file(tFileHandle);

    PL_FREE(pcBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

static bool
pl__scene_asset_deserialize_ex(const char* pcName, void* pScene, plComponentLibrary* ptLibrary)
{
    if(!gptVfs->does_file_exist(pcName))
        return false;

    plEcsTypeKey tObjectType = gptRendererEcs->get_ecs_type_key_object();
    plEcsTypeKey tSkinType = gptSkeleton->get_ecs_type_key_skin();
    plEcsTypeKey tAnimationType = gptAnimation->get_ecs_type_key_animation();
    plEcsTypeKey tHierarchyType = gptTransform->get_ecs_type_key_hierarchy();
    plEcsTypeKey tTerrainType = gptRendererEcs->get_ecs_type_key_terrain();
    plEcsTypeKey tCameraType = gptCameraEcs->get_ecs_type_key();
    plEcsTypeKey tScriptType = gptScript->get_ecs_type_key();

    plScene* ptScene = pScene;

    char acTempBuffer0[1024] = {0};

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tFileHandle);

    plJsonObject* ptRoot = NULL;
    gptJson->load((const char*)puFileBuffer, &ptRoot);

    uint32_t uVersion = gptJson->uint32_member(ptRoot, "version", 0);

    strncpy(acTempBuffer0, "/assets/environments/realistic.plenvironment", 1024);
    if(gptJson->member_exist(ptRoot, "environment"))
    {
        gptJson->string_member(ptRoot, "environment", acTempBuffer0, 1024);
        ptScene->tEnvironment = gptAsset->load(acTempBuffer0);
    }

    strncpy(acTempBuffer0, "/assets/settings/basic.renderer", 1024);
    if(gptJson->member_exist(ptRoot, "renderer"))
    {
        gptJson->string_member(ptRoot, "renderer", acTempBuffer0, 1024);
        ptScene->tRendererSettings = gptAsset->load(acTempBuffer0);
    }

    uint32_t uEntityCount = 0;
    plJsonObject* atJsonNodes = gptJson->array_member(ptRoot, "entities", &uEntityCount);

    // create all entities
    plEntity* atEntities = PL_ALLOC(uEntityCount * sizeof(plEntity));
    for(uint32_t i = 0; i < uEntityCount; i++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(atJsonNodes, i);
        plJsonType tJsonType = gptJson->get_type(gptJson->member(ptJsonNode, "id"));
        uint64_t uId = 0;
        if(tJsonType == PL_JSON_TYPE_NUMBER)
            uId = gptJson->uint64_member(ptJsonNode, "id", 0);
        else if(tJsonType == PL_JSON_TYPE_STRING)
        {
            gptJson->string_member(ptJsonNode, "id", acTempBuffer0, 1024);
            uId = gptEcs->generate_id(NULL, acTempBuffer0, 0);
        }
        else
        {
            PL_ASSERT(false);
        }

        char acName[256] = {0};
        plJsonObject* ptJsonTag = gptJson->member(ptJsonNode, "tag");
        if(ptJsonTag)
        {
            gptJson->string_member(ptJsonTag, "name", acName, 256);
        }

        atEntities[i] = gptEcs->create_entity_with_id(ptLibrary, acName, uId);
    }

    // add entity components
    const plComponentDesc* atCompDescs = NULL;
    uint32_t uComponentDescCount = gptEcs->get_type_descriptions(&atCompDescs);

    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(atJsonNodes, uEntityIndex);

        plEntity tEntity = atEntities[uEntityIndex];
        plEntityId tEntityId = gptEcs->get_entity_id(ptScene->ptLibrary, tEntity);

        for(uint32_t uCompIndex = 0; uCompIndex < uComponentDescCount; uCompIndex++)
        {
            const plComponentDesc* ptCompDesc = &atCompDescs[uCompIndex];

            plJsonObject* ptJsonComponent = gptJson->member(ptJsonNode, ptCompDesc->pcName);
            if(ptJsonComponent)
            {
                void* pComponentECSData = gptEcs->add_component(ptLibrary, ptCompDesc->tTypeKey, tEntity);
                if(ptCompDesc->deserialize)
                {
                    ptCompDesc->deserialize(ptJsonComponent, pComponentECSData);   
                }
                else
                {
                    PL_ASSERT(false && "Component needs deserialization implemented");
                }
            }
        }
    }

    // resolve references
    // add all components
    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(atJsonNodes, uEntityIndex);

        plEntity tEntity = atEntities[uEntityIndex];
        plEntityId tEntityId = gptEcs->get_entity_id(ptScene->ptLibrary, tEntity);

        for(uint32_t uCompIndex = 0; uCompIndex < uComponentDescCount; uCompIndex++)
        {
            const plComponentDesc* ptCompDesc = &atCompDescs[uCompIndex];
            void* pComponentECSData = gptEcs->get_component(ptLibrary, ptCompDesc->tTypeKey, tEntity);

            if(pComponentECSData == NULL)
                continue;

            if(ptCompDesc->tTypeKey == tObjectType)
            {
                plObjectComponent* ptObjectEcs = pComponentECSData;
                ptObjectEcs->tTransform = gptEcs->get_entity_by_id(ptLibrary, ptObjectEcs->tTransformId);
            }
            else if(ptCompDesc->tTypeKey == tCameraType)
            {
                plCamera* ptCamera = pComponentECSData;
                gptCamera->set_position(ptCamera, ptCamera->tPosition);
                gptCamera->set_euler(ptCamera, ptCamera->fPitch, ptCamera->fYaw, ptCamera->fRoll);
                ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_ALL;
                gptCamera->update(ptCamera);
            }
            else if(ptCompDesc->tTypeKey == tHierarchyType)
            {
                plHierarchyComponent* ptHierarchyEcs = pComponentECSData;
                plEntity tParent = gptEcs->get_entity_by_id(ptLibrary, ptHierarchyEcs->tParentId);
                gptTransform->attach_component(ptLibrary, tEntity, tParent);
            }
            else if(ptCompDesc->tTypeKey == tSkinType)
            {
                plSkinComponent* ptSkinEcs = pComponentECSData;
                plSkin* ptSkin = gptAsset->get_data(ptSkinEcs->tSkin);

                plEntity* atJoints = PL_ALLOC(sizeof(plEntity) * ptSkin->uJointCount);

                for(uint32_t i = 0; i < ptSkin->uJointCount; i++)
                {
                    atJoints[i] = gptEcs->get_entity_by_id(ptLibrary, ptSkin->atJoints[i]);
                }

                const bool bResult = gptSkeleton->bind_skin(ptLibrary, tEntity, atJoints, ptSkin->uJointCount);
                PL_ASSERT(bResult);
                PL_FREE(atJoints);
            }
            else if(ptCompDesc->tTypeKey == tTerrainType)
            {
                plTerrainComponent* ptTerrainEcs = pComponentECSData;
                plTerrainAsset* ptTerrain = gptAsset->get_data(ptTerrainEcs->tTerrain);
                gptTerrain->process(ptTerrain);
            }
            else if(ptCompDesc->tTypeKey == tAnimationType)
            {
                plAnimationComponent* ptAnimationEcs = pComponentECSData;
                ptAnimationEcs->atTargets = PL_ALLOC(sizeof(plEntity) * ptAnimationEcs->uTargetCount);
                for(uint32_t i = 0; i < ptAnimationEcs->uTargetCount; i++)
                {
                    ptAnimationEcs->atTargets[i] = gptEcs->get_entity_by_id(ptLibrary, ptAnimationEcs->atTargetIds[i]);
                }
            }
            else if(ptCompDesc->tTypeKey == tScriptType)
            {
                plScriptComponent* ptScriptEcs = pComponentECSData;
                gptScript->load(ptScene->ptLibrary, tEntity);
            }
        }
    }

    // resolve scenes
    for(uint32_t uEntityIndex = 0; uEntityIndex < uEntityCount; uEntityIndex++)
    {
        plJsonObject* ptJsonNode = gptJson->member_by_index(atJsonNodes, uEntityIndex);

        plEntity tEntity = atEntities[uEntityIndex];
        plEntityId tEntityId = gptEcs->get_entity_id(ptScene->ptLibrary, tEntity);

        for(uint32_t uCompIndex = 0; uCompIndex < uComponentDescCount; uCompIndex++)
        {
            const plComponentDesc* ptCompDesc = &atCompDescs[uCompIndex];
            void* pComponentECSData = gptEcs->get_component(ptLibrary, ptCompDesc->tTypeKey, tEntity);

            if(pComponentECSData == NULL)
                continue;

            if(ptCompDesc->tTypeKey == gptSceneCtx->tSceneInstanceComponentType)
            {
                plSceneInstanceComponent* ptSceneInstanceEcs = pComponentECSData;
                plScene* ptSceneInstance = gptAsset->get_data(ptSceneInstanceEcs->tScene);
                pl__scene_asset_deserialize_ex(gptAsset->get_path(ptSceneInstanceEcs->tScene), ptSceneInstance, ptLibrary);
            }
        }
    }
    PL_FREE(atEntities);
    PL_FREE(puFileBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

static bool
pl__scene_asset_deserialize(const char* pcName, void* pScene)
{
    plScene* ptScene = pScene;
    gptEcs->create_library(&ptScene->ptLibrary);
    return pl__scene_asset_deserialize_ex(pcName, pScene, ptScene->ptLibrary);
}

static void
pl__scene_destroy(void* pScene)
{
    plScene* ptScene = pScene;
    if(ptScene->ptLibrary)
        gptEcs->cleanup_library(&ptScene->ptLibrary);
    ptScene->ptLibrary = NULL;
}

static void
pl__ecs_scene_serialize(void* pComponent, plJsonObject* ptJson)
{
    plSceneInstanceComponent* ptComponent = pComponent;
    gptJson->add_string_member(ptJson, "scene", gptAsset->get_path(ptComponent->tScene));
}

static void
pl__ecs_scene_deserialize(plJsonObject* ptJson, void* pComponent)
{
    plSceneInstanceComponent* ptComponent = pComponent;
    char acName[1024] = {0};
    gptJson->string_member(ptJson, "scene", acName, 1024);
    ptComponent->tScene = gptAsset->load(acName);
}

void
pl_scene_register_ecs_components(void)
{
    const plComponentDesc tSceneInstanceDesc = {
        .pcDisplayName  = "Scene Instance",
        .pcName  = "scene_instance",
        .szSize  = sizeof(plSceneInstanceComponent),
        .serialize = pl__ecs_scene_serialize,
        .deserialize = pl__ecs_scene_deserialize
    };

    static const plSceneInstanceComponent tSceneInstanceComponentDefault = {
        0
    };
    gptSceneCtx->tSceneInstanceComponentType = gptEcs->register_type(tSceneInstanceDesc, &tSceneInstanceComponentDefault);

}

plEcsTypeKey
pl_scene_get_ecs_type_key_scene_instance(void)
{
    return gptSceneCtx->tSceneInstanceComponentType;
}

void
pl_scene_register_asset_type(void)
{
    static const plAssetTypeDesc tDesc = {
        .pcName          = "Scene",
        .pcFileExtension = "plscene",
        .szSize          = sizeof(plScene),
        .serialize       = pl__scene_asset_serialize,
        .deserialize     = pl__scene_asset_deserialize,
        .cleanup         = pl__scene_destroy
    };
    gptSceneCtx->tAssetTypeKey = gptAsset->register_type(tDesc);
}

plAssetTypeKey
pl_scene_get_asset_type_key(void)
{
    return gptSceneCtx->tAssetTypeKey;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_scene_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plSceneI tApi = {
        .register_ecs_components             = pl_scene_register_ecs_components,
        .get_ecs_type_key_scene_instance             = pl_scene_get_ecs_type_key_scene_instance,
        .register_asset_types = pl_scene_register_asset_type,
        .get_asset_type_key = pl_scene_get_asset_type_key
    };
    pl_set_api(ptApiRegistry, plSceneI, &tApi);

    gptLog         = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptResource    = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAsset       = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptString      = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptEcs         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptTransform   = pl_get_api_latest(ptApiRegistry, plTransformI);
    gptRendererEcs = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
    gptAnimation   = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptSkeleton    = pl_get_api_latest(ptApiRegistry, plSkeletonI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptCameraEcs   = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
    gptJson        = pl_get_api_latest(ptApiRegistry, plJsonI);
    gptIOI         = pl_get_api_latest(ptApiRegistry, plIOI);
    gptScript      = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptTerrain     = pl_get_api_latest(ptApiRegistry, plTerrainI);
    gptIO = gptIOI->get_io();

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptSceneCtx = ptDataRegistry->get_data("plSceneContext");
    }
    else // first load
    {
        static plSceneContext tCtx = {0};
        gptSceneCtx = &tCtx;
        ptDataRegistry->set_data("plSceneContext", gptSceneCtx);
    }
}

void
pl_unload_scene_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plSceneI* ptApi = pl_get_api_latest(ptApiRegistry, plSceneI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif