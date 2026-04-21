/*
   pl_ecs_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] components
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plLogI (v1.x)
        * plProfileI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ECS_EXT_H
#define PL_ECS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plEcsI_version {1, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>      // bool
#include "pl_ecs_ext.inl" // plEntity

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plEcsInit          plEcsInit;          // reserved for future use
typedef struct _plComponentDesc    plComponentDesc;    // describes a component
typedef struct _plComponentLibrary plComponentLibrary; // opaque

// ecs components
typedef struct _plTagComponent       plTagComponent;
typedef struct _plLayerComponent     plLayerComponent;
typedef struct _plTransformComponent plTransformComponent;
typedef struct _plHierarchyComponent plHierarchyComponent;

// flags
typedef int plTransformFlags;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_ecs_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ecs_ext(plApiRegistryI*, bool reload);

//-------------------------------GENERAL---------------------------------------

// system setup/shutdown
PL_API void         pl_ecs_initialize     (plEcsInit);
PL_API plEcsTypeKey pl_ecs_register_type  (plComponentDesc, const void* template_component); // can store
PL_API void         pl_ecs_finalize       (void);
PL_API void         pl_ecs_cleanup        (void);
PL_API uint64_t     pl_ecs_get_log_channel(void);

// libraries
PL_API bool                pl_ecs_create_library       (plComponentLibrary**);
PL_API void                pl_ecs_cleanup_library      (plComponentLibrary**);
PL_API void                pl_ecs_reset_library        (plComponentLibrary*);
PL_API void                pl_ecs_set_library_type_data(plComponentLibrary*, plEcsTypeKey, void*);
PL_API void*               pl_ecs_get_library_type_data(plComponentLibrary*, plEcsTypeKey);
PL_API plComponentLibrary* pl_ecs_get_default_library  (void);

// entities
PL_API plEntity pl_ecs_create_entity     (plComponentLibrary*, const char* name);
PL_API void     pl_ecs_remove_entity     (plComponentLibrary*, plEntity);
PL_API bool     pl_ecs_is_entity_valid   (plComponentLibrary*, plEntity);
PL_API plEntity pl_ecs_get_entity_by_name(plComponentLibrary*, const char* name);
PL_API plEntity pl_ecs_get_current_entity(plComponentLibrary*, plEntity);

// components
PL_API void*    pl_ecs_add_component (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
PL_API void*    pl_ecs_get_component (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
PL_API bool     pl_ecs_has_component (plComponentLibrary*, plEcsTypeKey, plEntity);
PL_API size_t   pl_ecs_get_index     (plComponentLibrary*, plEcsTypeKey, plEntity);
PL_API uint32_t pl_ecs_get_components(plComponentLibrary*, plEcsTypeKey, void**, const plEntity**); // do not store

//----------------------------CORE COMPONENTS----------------------------------

// component types (can store)
PL_API plEcsTypeKey pl_ecs_get_ecs_type_key_tag      (void);
PL_API plEcsTypeKey pl_ecs_get_ecs_type_key_layer    (void);
PL_API plEcsTypeKey pl_ecs_get_ecs_type_key_transform(void);
PL_API plEcsTypeKey pl_ecs_get_ecs_type_key_hierarchy(void);

// transforms
//   - do NOT store out parameter; use it immediately
PL_API plEntity pl_ecs_create_transform(plComponentLibrary*, const char* name, plTransformComponent**);

// hierarchy
PL_API void   pl_ecs_attach_component        (plComponentLibrary*, plEntity, plEntity tParent);
PL_API void   pl_ecs_deattach_component      (plComponentLibrary*, plEntity);
PL_API plMat4 pl_ecs_compute_parent_transform(plComponentLibrary*, plEntity);

// systems
PL_API void pl_ecs_run_transform_update_system(plComponentLibrary*);
PL_API void pl_ecs_run_hierarchy_update_system(plComponentLibrary*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plEcsI
{

    //-------------------------------GENERAL---------------------------------------

    // system setup/shutdown
    void         (*initialize)     (plEcsInit);
    plEcsTypeKey (*register_type)  (plComponentDesc, const void* template_component); // can store
    void         (*finalize)       (void);
    void         (*cleanup)        (void);
    uint64_t     (*get_log_channel)(void);
    
    // libraries
    bool                (*create_library)       (plComponentLibrary**);
    void                (*cleanup_library)      (plComponentLibrary**);
    void                (*reset_library)        (plComponentLibrary*);
    void                (*set_library_type_data)(plComponentLibrary*, plEcsTypeKey, void*);
    void*               (*get_library_type_data)(plComponentLibrary*, plEcsTypeKey);
    plComponentLibrary* (*get_default_library)  (void);
    
    // entities
    plEntity (*create_entity)     (plComponentLibrary*, const char* name);
    void     (*remove_entity)     (plComponentLibrary*, plEntity);
    bool     (*is_entity_valid)   (plComponentLibrary*, plEntity);
    plEntity (*get_entity_by_name)(plComponentLibrary*, const char* name);
    plEntity (*get_current_entity)(plComponentLibrary*, plEntity);

    // components
    void*    (*add_component) (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
    void*    (*get_component) (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
    bool     (*has_component) (plComponentLibrary*, plEcsTypeKey, plEntity);
    size_t   (*get_index)     (plComponentLibrary*, plEcsTypeKey, plEntity);
    uint32_t (*get_components)(plComponentLibrary*, plEcsTypeKey, void**, const plEntity**); // do not store

    //----------------------------CORE COMPONENTS----------------------------------

    // component types (can store)
    plEcsTypeKey (*get_ecs_type_key_tag)      (void);
    plEcsTypeKey (*get_ecs_type_key_layer)    (void);
    plEcsTypeKey (*get_ecs_type_key_transform)(void);
    plEcsTypeKey (*get_ecs_type_key_hierarchy)(void);

    // transforms
    //   - do NOT store out parameter; use it immediately
    plEntity (*create_transform)(plComponentLibrary*, const char* name, plTransformComponent**);

    // hierarchy
    void   (*attach_component)        (plComponentLibrary*, plEntity, plEntity tParent);
    void   (*deattach_component)      (plComponentLibrary*, plEntity);
    plMat4 (*compute_parent_transform)(plComponentLibrary*, plEntity);

    // systems
    void (*run_transform_update_system)(plComponentLibrary*);
    void (*run_hierarchy_update_system)(plComponentLibrary*);

} plEcsI;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plTagComponent
{
    char acName[128];
} plTagComponent;

typedef struct _plLayerComponent
{
    uint32_t uLayerMask;

    // [INTERNAL]
    uint32_t _uPropagationMask;
} plLayerComponent;

typedef struct _plHierarchyComponent
{
    plEntity tParent;
} plHierarchyComponent;

typedef struct _plTransformComponent
{
    plVec3           tScale;
    plVec4           tRotation;
    plVec3           tTranslation;
    plMat4           tWorld;
    plTransformFlags tFlags;
} plTransformComponent;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plEcsInit
{
    // [INTERNAL]
    uint32_t _uUnused;
} plEcsInit;

typedef struct _plComponentDesc
{
    const char* pcName;
    size_t      szSize;

    // optional callbacks
    void (*init)   (plComponentLibrary*);
    void (*cleanup)(plComponentLibrary*);
    void (*reset)  (plComponentLibrary*);

    // [INTERNAL]
    const void*  _pTemplate;
} plComponentDesc;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plTransformFlags
{
    PL_TRANSFORM_FLAGS_NONE  = 0,
    PL_TRANSFORM_FLAGS_DIRTY = 1 << 0,
};

#ifdef __cplusplus
}
#endif

#endif // PL_ECS_EXT_H