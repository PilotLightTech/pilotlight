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
typedef struct _plTagComponent plTagComponent;

// external
typedef struct _plJsonObject plJsonObject; // pl_json_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_ecs_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ecs_ext(plApiRegistryI*, bool reload);

//-------------------------------GENERAL---------------------------------------

// system setup/shutdown
// initialize -> register component types -> finalize
//
// Component types must be registered before finalize(). After finalization,
// component registration is no longer permitted.
PL_API void                   pl_ecs_initialize     (plEcsInit);
PL_API plEcsTypeKey           pl_ecs_register_type  (plComponentDesc, const void* template_component); // can store & template_component -> Default component value copied into newly added components
PL_API void                   pl_ecs_finalize       (void);
PL_API void                   pl_ecs_cleanup        (void);
PL_API uint64_t               pl_ecs_get_log_channel(void);

// libraries
// Component libraries contain entities and component storage.
// reset_library removes all entities/components but keeps the library usable.
// cleanup_library destroys the library and sets the supplied pointer to NULL.
PL_API bool                   pl_ecs_create_library       (plComponentLibrary**);
PL_API void                   pl_ecs_cleanup_library      (plComponentLibrary**);
PL_API void                   pl_ecs_reset_library        (plComponentLibrary*);
PL_API const plComponentDesc* pl_ecs_get_type_description (plEcsTypeKey);
PL_API uint32_t               pl_ecs_get_type_descriptions(const plComponentDesc**);

// Associates extension-specific data with a component type in this library.
// The ECS does not own or free this pointer.
PL_API void  pl_ecs_set_library_type_data(plComponentLibrary*, plEcsTypeKey, void*);
PL_API void* pl_ecs_get_library_type_data(plComponentLibrary*, plEcsTypeKey);

// entities
PL_API plEntity   pl_ecs_create_entity        (plComponentLibrary*, const char* name);
PL_API void       pl_ecs_remove_entity        (plComponentLibrary*, plEntity);
PL_API bool       pl_ecs_is_entity_valid      (plComponentLibrary*, plEntity);
PL_API plEntity   pl_ecs_get_entity_by_name   (plComponentLibrary*, const char* name);
PL_API plEntity   pl_ecs_get_current_entity   (plComponentLibrary*, plEntity);
PL_API plEntity   pl_ecs_create_entity_with_id(plComponentLibrary*, const char* name, plEntityId);
PL_API plEntityId pl_ecs_get_entity_id        (plComponentLibrary*, plEntity);
PL_API plEntity   pl_ecs_get_entity_by_id     (plComponentLibrary*, plEntityId);
PL_API void       pl_ecs_set_entity_name      (plComponentLibrary*, plEntity, const char* name);

// If entities is NULL, writes the required entity count to count.
// Otherwise writes up to *count entities and updates *count with the number written.
PL_API void pl_ecs_get_entities(plComponentLibrary*, plEntity*, uint32_t*);

// components
PL_API void*    pl_ecs_add_component (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
PL_API void*    pl_ecs_get_component (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
PL_API bool     pl_ecs_has_component (plComponentLibrary*, plEcsTypeKey, plEntity);
PL_API size_t   pl_ecs_get_index     (plComponentLibrary*, plEcsTypeKey, plEntity);
PL_API uint32_t pl_ecs_get_components(plComponentLibrary*, plEcsTypeKey, void**, const plEntity**); // do not store

// utilities

// generates an unique ID if using library (will ignore path & seed) or will create a hash
// based on path + seed (should be unique but can't be guaranteed)
PL_API plEntityId pl_ecs_generate_id(plComponentLibrary*, const char* path, uint64_t seed);

//----------------------------CORE COMPONENTS----------------------------------

// component types (can store)
PL_API plEcsTypeKey pl_ecs_get_ecs_type_key_tag(void);

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

    // libraries
    // Component libraries contain entities and component storage.
    // reset_library removes all entities/components but keeps the library usable.
    // cleanup_library destroys the library and sets the supplied pointer to NULL.
    bool                   (*create_library)       (plComponentLibrary**);
    void                   (*cleanup_library)      (plComponentLibrary**);
    void                   (*reset_library)        (plComponentLibrary*);
    const plComponentDesc* (*get_type_description) (plEcsTypeKey tTypeKey);
    uint32_t               (*get_type_descriptions)(const plComponentDesc**);

    // Associates extension-specific data with a component type in this library.
    // The ECS does not own or free this pointer.
    void  (*set_library_type_data)(plComponentLibrary*, plEcsTypeKey, void*);
    void* (*get_library_type_data)(plComponentLibrary*, plEcsTypeKey);
    
    // entities
    plEntity  (*create_entity)        (plComponentLibrary*, const char* name);
    plEntity  (*create_entity_with_id)(plComponentLibrary*, const char* name, plEntityId);
    void      (*remove_entity)        (plComponentLibrary*, plEntity);
    bool      (*is_entity_valid)      (plComponentLibrary*, plEntity);
    plEntity  (*get_entity_by_name)   (plComponentLibrary*, const char* name);
    plEntity  (*get_current_entity)   (plComponentLibrary*, plEntity);
    plEntityId(*get_entity_id)        (plComponentLibrary*, plEntity);
    plEntity  (*get_entity_by_id)     (plComponentLibrary*, plEntityId);
    void      (*set_entity_name)      (plComponentLibrary*, plEntity, const char* name);


    // If entities is NULL, writes the required entity count to count.
    // Otherwise writes up to *count entities and updates *count with the number written.
    void (*get_entities)(plComponentLibrary*, plEntity*, uint32_t*);

    // components
    void*    (*add_component) (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
    void*    (*get_component) (plComponentLibrary*, plEcsTypeKey, plEntity); // do not store
    bool     (*has_component) (plComponentLibrary*, plEcsTypeKey, plEntity);
    size_t   (*get_index)     (plComponentLibrary*, plEcsTypeKey, plEntity);
    uint32_t (*get_components)(plComponentLibrary*, plEcsTypeKey, void**, const plEntity**); // do not store

    // utilities

    // generates an unique ID if using library (will ignore path & seed) or will create a hash
    // based on path + seed (should be unique but can't be guaranteed)
    plEntityId (*generate_id)(plComponentLibrary*, const char* path, uint64_t seed);

    //----------------------------CORE COMPONENTS----------------------------------

    // component types (can store)
    plEcsTypeKey (*get_ecs_type_key_tag)(void);

} plEcsI;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plTagComponent
{
    const char* pcName;
} plTagComponent;

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
    const char* pcDisplayName; // human-readable name, e.g. "Transform"
    size_t      szSize;        // component size

    // serialization
    const char* pcName; // stable serialized component name, e.g. "transform"
    void (*serialize)  (void*, plJsonObject*);
    void (*deserialize)(plJsonObject*, void*);

    // optional callbacks, called once per component library
    void (*init)   (plComponentLibrary*);
    void (*cleanup)(plComponentLibrary*);
    void (*reset)  (plComponentLibrary*);

    // populated after registration
    plEcsTypeKey tTypeKey;

    // [INTERNAL]
    void* _pTemplate;
} plComponentDesc;

#ifdef __cplusplus
}
#endif

#endif // PL_ECS_EXT_H