/*
   pl_ik_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_ik_ext.h"
#include "pl_math.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_profile_ext.h"
#include "pl_transform_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plProfileI*   gptProfile   = NULL;
    static const plEcsI*       gptEcs       = NULL;
    static const plTransformI* gptTransform = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plComponentLibraryData
{
    plTransformComponent* sbtTransformsCopy; // used for inverse kinematics system
} plComponentLibraryData;

typedef struct _plIkContext
{
    plEcsTypeKey tHierarchyComponentType;
    plEcsTypeKey tTransformComponentType;
    plEcsTypeKey tInverseKinematicsComponentType;
} plIkContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plIkContext* gptIkCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void
pl__ecs_ik_init(plComponentLibrary* ptLibrary)
{
    void* pData = PL_ALLOC(sizeof(plComponentLibraryData));
    memset(pData, 0, sizeof(plComponentLibraryData));
    gptEcs->set_library_type_data(ptLibrary, gptIkCtx->tInverseKinematicsComponentType, pData);
}

static void
pl__ecs_ik_cleanup(plComponentLibrary* ptLibrary)
{
    plComponentLibraryData* ptData = gptEcs->get_library_type_data(ptLibrary, gptIkCtx->tInverseKinematicsComponentType);
    pl_sb_free(ptData->sbtTransformsCopy);
    PL_FREE(ptData);
    gptEcs->set_library_type_data(ptLibrary, gptIkCtx->tInverseKinematicsComponentType, NULL);
}

static void
pl__ecs_ik_reset(plComponentLibrary* ptLibrary)
{
    plComponentLibraryData* ptData = gptEcs->get_library_type_data(ptLibrary, gptIkCtx->tInverseKinematicsComponentType);
    pl_sb_reset(ptData->sbtTransformsCopy);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

plEcsTypeKey
pl_ik_get_ecs_type_key(void)
{
    return gptIkCtx->tInverseKinematicsComponentType;
}

void
pl_ik_run_update_system(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plInverseKinematicsComponent* ptComponents = NULL;
    const plEntity* ptEntities = NULL;
    const uint32_t uComponentCount = gptEcs->get_components(ptLibrary, gptIkCtx->tInverseKinematicsComponentType, (void**)&ptComponents, &ptEntities);

    if(uComponentCount == 0)
    {
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
        return;
    }


    plTransformComponent* ptTransforms = NULL;
    const uint32_t uTransformCount = gptEcs->get_components(ptLibrary, gptIkCtx->tTransformComponentType, (void**)&ptTransforms, NULL);

    plComponentLibraryData* ptData = gptEcs->get_library_type_data(ptLibrary, gptIkCtx->tInverseKinematicsComponentType);
    pl_sb_resize(ptData->sbtTransformsCopy, uTransformCount);
    memcpy(ptData->sbtTransformsCopy, ptTransforms, uTransformCount * sizeof(plTransformComponent));
    gptEcs->set_library_type_data(ptLibrary, gptIkCtx->tInverseKinematicsComponentType, ptData);
    
    bool bRecomputeHierarchy = false;
    for(uint32_t i = 0; i < uComponentCount; i++)
    {

        const plEntity tIKEntity = ptEntities[i];
        const size_t uIKIndex = gptEcs->get_index(ptLibrary, gptIkCtx->tInverseKinematicsComponentType, tIKEntity);

        const plInverseKinematicsComponent* ptInverseKinematicsComponent = &ptComponents[uIKIndex];
        
        if(!ptInverseKinematicsComponent->bEnabled)
            continue;

        const size_t uTransformIndex = gptEcs->get_index(ptLibrary, gptIkCtx->tTransformComponentType, tIKEntity);
        const size_t uTargetIndex = gptEcs->get_index(ptLibrary, gptIkCtx->tTransformComponentType, ptInverseKinematicsComponent->tTarget);

        plTransformComponent* ptTransform = &ptData->sbtTransformsCopy[uTransformIndex];
        plTransformComponent* ptTarget = &ptData->sbtTransformsCopy[uTargetIndex];
        plHierarchyComponent* ptHierComp = gptEcs->get_component(ptLibrary, gptIkCtx->tHierarchyComponentType, tIKEntity);
        
        PL_ASSERT(uTransformIndex != UINT64_MAX);
        PL_ASSERT(uTargetIndex != UINT64_MAX);
        PL_ASSERT(ptHierComp);

        const plVec3 tTargetPos = ptTarget->tWorld.col[3].xyz;
        for(uint32_t j = 0; j < ptInverseKinematicsComponent->uIterationCount; j++)
        {
            plTransformComponent* aptStack[32] = {0};
            plEntity tParentEntity = ptHierComp->tParent;
            plTransformComponent* ptChildTransform = ptTransform;
            for(uint32_t uChain = 0; uChain < pl_min(ptInverseKinematicsComponent->uChainLength, 32); ++uChain)
            {
                bRecomputeHierarchy = true;

                // stack stores all traversed chain links so far
                aptStack[uChain] = ptChildTransform;

                // compute required parent rotation that moves ik transform closer to target transform
                const size_t uParentIndex = gptEcs->get_index(ptLibrary, gptIkCtx->tTransformComponentType, tParentEntity);
                PL_ASSERT(uParentIndex != UINT64_MAX);
                plTransformComponent* ptParentTransform =  &ptData->sbtTransformsCopy[uParentIndex];
                const plVec3 tParentPos = ptParentTransform->tWorld.col[3].xyz;
                const plVec3 tDirParentToIk = pl_norm_vec3(pl_sub_vec3(ptTransform->tWorld.col[3].xyz, tParentPos));
                const plVec3 tDirParentToTarget = pl_norm_vec3(pl_sub_vec3(tTargetPos, tParentPos));

                // TODO: check if this transform is part of a humanoid and need some constraining

                plVec4 tQ = {0};

                // simple shortest rotation without constraint
                const plVec3 tAxis = pl_norm_vec3(pl_cross_vec3(tDirParentToIk, tDirParentToTarget));
                const float fAngle = acosf(pl_clampf(-1.0f, pl_dot_vec3(tDirParentToIk, tDirParentToTarget), 1.0f));
                tQ = pl_norm_vec4(pl_quat_rotation_vec3(fAngle, tAxis));

                // parent to world space
                pl_decompose_matrix(&ptParentTransform->tWorld, &ptParentTransform->tScale, &ptParentTransform->tRotation, &ptParentTransform->tTranslation);

                // rotate parent
                ptParentTransform->tRotation = pl_norm_vec4(pl_mul_quat(tQ, ptParentTransform->tRotation));
                ptParentTransform->tWorld = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);

                // parent back to local space (if parent has parent)
                plHierarchyComponent* ptHierParentComp = gptEcs->get_component(ptLibrary, gptIkCtx->tHierarchyComponentType, tParentEntity);
                if(ptHierParentComp)
                {
                    plEntity tParentOfParentEntity = ptHierParentComp->tParent;
                    const size_t uGrandParentIndex = gptEcs->get_index(ptLibrary, gptIkCtx->tTransformComponentType, tParentOfParentEntity);
                    PL_ASSERT(uGrandParentIndex != UINT64_MAX);
                    plTransformComponent* ptParentOfParentTransform = &ptData->sbtTransformsCopy[uGrandParentIndex];
                    const plMat4 tParentOfParentInverse = pl_mat4_invert(&ptParentOfParentTransform->tWorld);
                    plMat4 tW = pl_rotation_translation_scale(ptParentTransform->tRotation, ptParentTransform->tTranslation, ptParentTransform->tScale);
                    plMat4 tNewMatrix = pl_mul_mat4(&tParentOfParentInverse, &tW);
                    pl_decompose_matrix(&tNewMatrix, &ptParentTransform->tScale, &ptParentTransform->tRotation, &ptParentTransform->tTranslation);
                    // keep parent world matrix in world space!
                }

                // update chain from parent to children
                const plTransformComponent* ptRecurseParent = ptParentTransform;
                for(int recurse_chain = (int)uChain; recurse_chain >=0; --recurse_chain)
                {
                    plMat4 tW = pl_rotation_translation_scale(aptStack[recurse_chain]->tRotation, aptStack[recurse_chain]->tTranslation, aptStack[recurse_chain]->tScale);
                    aptStack[recurse_chain]->tWorld = pl_mul_mat4(&ptRecurseParent->tWorld, &tW);
                    ptRecurseParent = aptStack[recurse_chain];
                }

                if(ptHierParentComp == NULL)
                {
                    // chain root reached, exit
                    break;
                }

                // move up in the chain by one
                ptChildTransform = ptParentTransform;
                tParentEntity = ptHierParentComp->tParent;
                PL_ASSERT(uChain < 32);
            }
        }

    }

    if(bRecomputeHierarchy)
    {
        const plEntity* ptHierarchyEntities = NULL;
        const uint32_t uHierarchyCount = gptEcs->get_components(ptLibrary, gptIkCtx->tHierarchyComponentType, NULL, &ptHierarchyEntities);
        for(uint32_t i = 0; i < uHierarchyCount; i++)
        {
            const plEntity tChildEntity = ptHierarchyEntities[i];
            
            const size_t uChildIndex = gptEcs->get_index(ptLibrary, gptIkCtx->tTransformComponentType, tChildEntity);
            PL_ASSERT(uChildIndex != UINT64_MAX);

            const plTransformComponent* ptTransformChild = &ptData->sbtTransformsCopy[uChildIndex];

            plMat4 tWorldMatrix = pl_rotation_translation_scale(ptTransformChild->tRotation, ptTransformChild->tTranslation, ptTransformChild->tScale);

            plHierarchyComponent* ptHierarchyComponent = gptEcs->get_component(ptLibrary, gptIkCtx->tHierarchyComponentType, tChildEntity);
            
            plEntity tParentID = ptHierarchyComponent->tParent;
            while(tParentID.uIndex != UINT32_MAX)
            {
                const size_t uParentIndex = gptEcs->get_index(ptLibrary, gptIkCtx->tTransformComponentType, tParentID);
                if(uParentIndex == UINT64_MAX)
                    break;
                plTransformComponent* ptTransformParent = &ptData->sbtTransformsCopy[uParentIndex];
                plMat4 tLocalMatrix = pl_rotation_translation_scale(ptTransformParent->tRotation, ptTransformParent->tTranslation, ptTransformParent->tScale);
                tWorldMatrix = pl_mul_mat4(&tLocalMatrix, &tWorldMatrix);

                const plHierarchyComponent* ptHierarchyRecursive = gptEcs->get_component(ptLibrary, gptIkCtx->tHierarchyComponentType, tParentID);
                if(ptHierarchyRecursive)
                    tParentID = ptHierarchyRecursive->tParent;
                else
                    tParentID.uIndex = UINT32_MAX;
            }

            ptTransforms[uChildIndex].eFlags |= PL_TRANSFORM_FLAGS_DIRTY;
            ptTransforms[uChildIndex].tWorld = tWorldMatrix;
        }

    }

    pl_sb_reset(ptData->sbtTransformsCopy);

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_ik_register_ecs_components(void)
{

    gptIkCtx->tTransformComponentType = gptTransform->get_ecs_type_key_transform();
    gptIkCtx->tHierarchyComponentType = gptTransform->get_ecs_type_key_hierarchy();

    const plComponentDesc tIKDesc = {
        .pcName = "inverse_kinematics",
        .szSize = sizeof(plInverseKinematicsComponent),
        .init   = pl__ecs_ik_init,
        .cleanup = pl__ecs_ik_cleanup,
        .reset = pl__ecs_ik_reset
    };

    static const plInverseKinematicsComponent tIkComponentDefault = {
        .bEnabled = true,
        .tTarget = UINT32_MAX,
        .uIterationCount = 1
    };
    gptIkCtx->tInverseKinematicsComponentType = gptEcs->register_type(tIKDesc, &tIkComponentDefault);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_ik_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plIkI tApi = {
        .register_ecs_components   = pl_ik_register_ecs_components,
        .run_ecs_update_system = pl_ik_run_update_system,
        .get_ecs_type_key      = pl_ik_get_ecs_type_key,
    };
    pl_set_api(ptApiRegistry, plIkI, &tApi);

    #ifndef PL_UNITY_BUILD
    gptEcs = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptTransform = pl_get_api_latest(ptApiRegistry, plTransformI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptIkCtx = ptDataRegistry->get_data("plIkContext");
    }
    else // first load
    {
        static plIkContext tCtx = {0};
        gptIkCtx = &tCtx;
        ptDataRegistry->set_data("plIkContext", gptIkCtx);
    }
}

void
pl_unload_ik_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plIkI* ptApi = pl_get_api_latest(ptApiRegistry, plIkI);
    ptApiRegistry->remove_api(ptApi);
}