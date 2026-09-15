/*
   pl_ecs_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
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

        * plLogI          (v2.x)
        * plProfileI      (v2.x)
        * plStringInternI (v3.x)
        * plTimerI        (v1.1)
        * plJsonI         (v1.x)

    Pointer Lifetime:
        Functions marked "can store" return values/pointers that remain valid
        for the lifetime documented by that function.

        Functions marked "do not store" return pointers into ECS-owned storage.
        These pointers may be invalidated when components/entities are added,
        removed, or when the component library is reset. (this restriction will
        be removed soon)

    Names vs. IDs:
        Entity names are intended for human-readable lookup and are not stable IDs.
        Entity IDs remain stable across serialization/deserialization.
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

#define plEcsI_version {3, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>        // bool
#include "pl_ecs_ext.inl"   // plEntity
#include "pl_asset_ext.inl" // plAssetHandle

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plEcsInit          plEcsInit;          // reserved for future use
typedef struct _plComponentDesc    plComponentDesc;    // describes a component
typedef struct _plComponentLibrary plComponentLibrary; // opaque

// ecs components
typedef struct _plTagComponent     plTagComponent;
typedef struct _plLibraryComponent plLibraryComponent;

// callbacks
typedef void (*plEcsLibraryDataCleanup)(void*);

// external
typedef struct _plJsonObject plJsonObject; // pl_json_ext.h
typedef struct _plHashMap64 plHashMap64;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_ecs_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ecs_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plEcsI
{

    //-------------------------------GENERAL---------------------------------------

    // system setup/shutdown
    // initialize -> register component types -> finalize
    //
    // Component types must be registered before finalize(). After finalization,
    // component registration is no longer permitted.
    void         (*initialize)     (plEcsInit);
    plEcsTypeKey (*register_type)  (plComponentDesc, const void* template_component); // can store & template_component -> Default component value copied into newly added components
    void         (*finalize)       (void);
    void         (*cleanup)        (void);
    uint64_t     (*get_log_channel)(void);

    // assets
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key)(void);

    // libraries - Component libraries contain entities and component storage.
    
    // memory owned by caller
    void (*init_library)   (plComponentLibrary*);
    void (*cleanup_library)(plComponentLibrary*);

    // memory owned by extension
    plComponentLibrary* (*create_library) (void);
    void                (*destroy_library)(plComponentLibrary*);

    // library misc.
    const plComponentDesc* (*get_type_description) (plEcsTypeKey tTypeKey);
    uint32_t               (*get_type_descriptions)(const plComponentDesc**);

    // Preserves entity IDs.
    // Existing destination entities are patched.
    // Missing entities/components are added.
    // Existing components are overwritten.
    bool (*merge_library)(plComponentLibrary* dest, plComponentLibrary* src);

    // Creates new destination entities with new IDs.
    // Clones components and fills source -> destination entity mapping.
    bool (*clone_library_into)(plComponentLibrary* dest, plComponentLibrary* src, plHashMap64*);

    // Associates extension-specific data with a component type in this library.
    // The ECS does not own or free this pointer.
    void  (*set_library_type_data)(plComponentLibrary*, plEcsTypeKey, void*, plEcsLibraryDataCleanup);
    void* (*get_library_type_data)(plComponentLibrary*, plEcsTypeKey);
    
    // entities
    plEntity  (*create_entity)        (plComponentLibrary*, const char* name);
    plEntity  (*create_entity_with_id)(plComponentLibrary*, const char* name, plEntityId);
    void      (*remove_entity)        (plComponentLibrary*, plEntity);
    bool      (*is_entity_valid)      (const plComponentLibrary*, plEntity);
    plEntity  (*get_current_entity)   (plComponentLibrary*, plEntity);
    plEntityId(*get_entity_id)        (const plComponentLibrary*, plEntity);
    plEntity  (*get_entity_by_id)     (const plComponentLibrary*, plEntityId);

    // If entities is NULL, writes the required entity count to count.
    // Otherwise writes up to *count entities and updates *count with the number written.
    void (*get_entities)(const plComponentLibrary*, plEntity*, uint32_t*);

    // components
    void*    (*add_component) (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
    void*    (*get_component) (const plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
    bool     (*has_component) (const plComponentLibrary*, plEcsTypeKey, plEntity);
    size_t   (*get_index)     (const plComponentLibrary*, plEcsTypeKey, plEntity);
    uint32_t (*get_components)(const plComponentLibrary*, plEcsTypeKey, void**, const plEntity**); // do not store

    // utilities

    // generates an unique ID if using library (will ignore path & seed) or will create a hash
    // based on path + seed (should be unique but can't be guaranteed)
    plEntityId (*generate_id)(plComponentLibrary*, const char* path, uint64_t seed);

    //----------------------------CORE COMPONENTS----------------------------------

    // component types (can store)
    plEcsTypeKey (*get_ecs_type_key_tag)(void);
    plEcsTypeKey (*get_ecs_type_key_library)(void);

} plEcsI;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plTagComponent
{
    const char* pcName;
} plTagComponent;

typedef struct _plLibraryComponent
{
    plAssetHandle tSourceLibrary;

    // [INTERNAL]
    plComponentLibrary* _ptLibrary;
    plComponentLibrary* _ptPatchLibrary;
    plHashMap64*        _ptPatchHash;
} plLibraryComponent;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plEcsInit
{
    // [INTERNAL]
    uint32_t _uUnused;
} plEcsInit;

// NOTES:
//      * serialize/deserialize == NULL -> component is transient/not serialized
//      * resolve == NULL -> component contains no entity references requiring fixup
//      * destroy == NULL -> component owns nothing; removal is just storage removal
//      * clone   == NULL -> ECS uses memcpy; component is trivially copyable
//      * clone   != NULL -> callback performs the deep copy
typedef struct _plComponentDesc
{
    const char* pcName; // stable serialized component name, e.g. "transform"
    size_t      szSize; // component size

    // persistence
    void (*serialize)  (void* component, const plComponentLibrary*, plEntityId, plJsonObject*);
    void (*deserialize)(plJsonObject*, plComponentLibrary*, plEntityId, void*);
    void (*resolve)    (plComponentLibrary*, plEntityId, plHashMap64*, void*);
    
    // lifetime
    void (*destroy)(void* component, const plComponentLibrary*);
    void (*clone)  (const void* src, plComponentLibrary* srcLib, void* dest, plComponentLibrary* destLib);

    // populated by ECS
    plEcsTypeKey tTypeKey;

    // [INTERNAL]
    void* _pTemplate;
} plComponentDesc;

#ifdef __cplusplus
}
#endif

#endif // PL_ECS_EXT_H