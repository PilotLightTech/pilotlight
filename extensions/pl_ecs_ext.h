/*
   pl_ecs_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] forward declarations & basic types
// [SECTION] public api structs
// [SECTION] components
// [SECTION] structs
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

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plEcsI_version {0, 3, 1}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

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
typedef struct _plScriptComponent    plScriptComponent;

// flags
typedef int plTransformFlags;
typedef int plScriptFlags;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
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
    plEcsTypeKey (*get_ecs_type_key_script)   (void);

    // entity helpers (creates entity and necessary components)
    //   - do NOT store out parameter; use it immediately
    plEntity (*create_transform)(plComponentLibrary*, const char* name, plTransformComponent**);

    // scripts
    plEntity (*create_script)(plComponentLibrary*, const char* file, plScriptFlags, plScriptComponent**);
    void     (*attach_script)(plComponentLibrary*, const char* file, plScriptFlags, plEntity, plScriptComponent**);

    // hierarchy
    void   (*attach_component)        (plComponentLibrary*, plEntity, plEntity tParent);
    void   (*deattach_component)      (plComponentLibrary*, plEntity);
    plMat4 (*compute_parent_transform)(plComponentLibrary*, plEntity);

    // systems
    void (*run_transform_update_system)(plComponentLibrary*);
    void (*run_hierarchy_update_system)(plComponentLibrary*);
    void (*run_script_update_system)   (plComponentLibrary*);

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

typedef struct _plScriptComponent
{
    plScriptFlags tFlags;
    char          acFile[PL_MAX_PATH_LENGTH];

    // [INTERNAL]
    const struct _plScriptI* _ptApi;
} plScriptComponent;

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

enum _plScriptFlags
{
    PL_SCRIPT_FLAG_NONE       = 0,
    PL_SCRIPT_FLAG_PLAYING    = 1 << 0,
    PL_SCRIPT_FLAG_PLAY_ONCE  = 1 << 1,
    PL_SCRIPT_FLAG_RELOADABLE = 1 << 2
};

#endif // PL_ECS_EXT_H