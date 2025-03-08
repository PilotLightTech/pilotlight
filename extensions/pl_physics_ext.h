/*
   pl_physics_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PHYSICS_EXT_H
#define PL_PHYSICS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plPhysicsI_version (plVersion){0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plPhysicsI
{
    // setup shutdown
    void (*initialize)(void);
    void (*cleanup)(void);
    void (*reset)(void);

    void (*action)(plComponentLibrary*);

    // per frame
    void (*update)(float deltaTime, plComponentLibrary*);
    void (*draw)(plComponentLibrary*, plDrawList3D*);
} plPhysicsI;

#endif // PL_PHYSICS_EXT_H