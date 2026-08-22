/*
   pl_animation_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
// [SECTION] components
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plEcsI     (v1.x)
        * plProfileI (v1.x)
        * plLogI     (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ANIMATION_EXT_H
#define PL_ANIMATION_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plAnimationI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_PATH_LENGTH
    #define PL_MAX_PATH_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stddef.h>       // size_t
#include <stdbool.h>      // bool
#include "pl_ecs_ext.inl" // plEntity
#include "pl_asset_ext.inl" // plAssetHandle

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plAnimationChannel plAnimationChannel;
typedef struct _plAnimationSampler plAnimationSampler;
typedef struct _plAnimationData    plAnimationData;
typedef struct _plAnimation        plAnimation;

// ecs components
typedef struct _plAnimationComponent plAnimationComponent;
typedef struct _plHumanoidComponent  plHumanoidComponent;

// enums & flags
typedef int plAnimationMode;
typedef int plAnimationPath;
typedef int plAnimationFlags;
typedef int plHumanoidBone;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_animation_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_animation_ext(plApiRegistryI*, bool reload);

// new
PL_API void pl_animation_serialize(const char* name, const plAnimation*);
PL_API void pl_animation_deserialize(const char* name, plAnimation*);
PL_API void pl_animation_destroy(plAnimation*);

// system setup/shutdown/etc
PL_API void pl_animation_register_ecs_system(void);

// entity helpers (creates entity and necessary components)
//   - do NOT store out parameter; use it immediately
PL_API plEntity pl_animation_create(plComponentLibrary*, const char* name, uint32_t channelCount, plAnimationComponent**);

// systems
PL_API void pl_animation_run_animation_update_system         (plComponentLibrary*, float deltaTime);

// ecs types
PL_API plEcsTypeKey pl_animation_get_ecs_type_key_animation(void);
PL_API plEcsTypeKey pl_animation_get_ecs_type_key_humanoid (void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plAnimationI
{

    void (*serialize)  (const char* name, const plAnimation*);
    void (*deserialize)(const char* name, plAnimation*);
    void (*destroy)(plAnimation*);

    // system setup/shutdown/etc
    void (*register_ecs_system)(void);

    // entity helpers (creates entity and necessary components)
    //   - do NOT store out parameter; use it immediately
    plEntity (*create)(plComponentLibrary*, const char* name, uint32_t channelCount, plAnimationComponent**);

    // systems
    void (*run_animation_update_system)(plComponentLibrary*, float fDeltaTime);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key_animation)(void);
    plEcsTypeKey (*get_ecs_type_key_humanoid)(void);
    
} plAnimationI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plAnimationMode
{
    PL_ANIMATION_MODE_UNKNOWN,
    PL_ANIMATION_MODE_LINEAR,
    PL_ANIMATION_MODE_STEP,
    PL_ANIMATION_MODE_CUBIC_SPLINE
};

enum _plAnimationPath
{
    PL_ANIMATION_PATH_UNKNOWN,
    PL_ANIMATION_PATH_TRANSLATION,
    PL_ANIMATION_PATH_ROTATION,
    PL_ANIMATION_PATH_SCALE,
    PL_ANIMATION_PATH_WEIGHTS
};

enum _plAnimationFlags
{
    PL_ANIMATION_FLAG_NONE    = 0,
    PL_ANIMATION_FLAG_PLAYING = 1 << 0,
    PL_ANIMATION_FLAG_LOOPED  = 1 << 1
};

enum _plHumanoidBone
{
    // torso
    PL_HUMANOID_BONE_HIPS,  // required
    PL_HUMANOID_BONE_SPINE, // required
    PL_HUMANOID_BONE_CHEST,
    PL_HUMANOID_BONE_UPPER_CHEST,
    PL_HUMANOID_BONE_NECK,

    // head
    PL_HUMANOID_BONE_HEAD, // required
    PL_HUMANOID_BONE_LEFT_EYE,
    PL_HUMANOID_BONE_RIGHT_EYE,
    PL_HUMANOID_BONE_JAW,

    // leg
    PL_HUMANOID_BONE_LEFT_UPPER_LEG, // required
    PL_HUMANOID_BONE_LEFT_LOWER_LEG, // required
    PL_HUMANOID_BONE_LEFT_FOOT,      // required
    PL_HUMANOID_BONE_LEFT_TOES,
    PL_HUMANOID_BONE_RIGHT_UPPER_LEG, // required
    PL_HUMANOID_BONE_RIGHT_LOWER_LEG, // required
    PL_HUMANOID_BONE_RIGHT_FOOT,	  // required
    PL_HUMANOID_BONE_RIGHT_TOES,

    // arm
    PL_HUMANOID_BONE_LEFT_SHOULDER,
    PL_HUMANOID_BONE_LEFT_UPPER_ARM, // required
    PL_HUMANOID_BONE_LEFT_LOWER_ARM, // required
    PL_HUMANOID_BONE_LEFT_HAND,      // required
    PL_HUMANOID_BONE_RIGHT_SHOULDER,
    PL_HUMANOID_BONE_RIGHT_UPPER_ARM, // required
    PL_HUMANOID_BONE_RIGHT_LOWER_ARM, // required
    PL_HUMANOID_BONE_RIGHT_HAND,      // required

    // finger
    PL_HUMANOID_BONE_LEFT_THUMB_METACARPAL,
    PL_HUMANOID_BONE_LEFT_THUMB_PROXIMAL,
    PL_HUMANOID_BONE_LEFT_THUMB_DISTAL,
    PL_HUMANOID_BONE_LEFT_INDEX_PROXIMAL,
    PL_HUMANOID_BONE_LEFT_INDEX_INTERMEDIATE,
    PL_HUMANOID_BONE_LEFT_INDEX_DISTAL,
    PL_HUMANOID_BONE_LEFT_MIDDLE_PROXIMAL,
    PL_HUMANOID_BONE_LEFT_MIDDLE_INTERMEDIATE,
    PL_HUMANOID_BONE_LEFT_MIDDLE_DISTAL,
    PL_HUMANOID_BONE_LEFT_RING_PROXIMAL,
    PL_HUMANOID_BONE_LEFT_RING_INTERMEDIATE,
    PL_HUMANOID_BONE_LEFT_RING_DISTAL,
    PL_HUMANOID_BONE_LEFT_LITTLE_PROXIMAL,
    PL_HUMANOID_BONE_LEFT_LITTLE_INTERMEDIATE,
    PL_HUMANOID_BONE_LEFT_LITTLE_DISTAL,
    PL_HUMANOID_BONE_RIGHT_THUMB_METACARPAL,
    PL_HUMANOID_BONE_RIGHT_THUMB_PROXIMAL,
    PL_HUMANOID_BONE_RIGHT_THUMB_DISTAL,
    PL_HUMANOID_BONE_RIGHT_INDEX_INTERMEDIATE,
    PL_HUMANOID_BONE_RIGHT_INDEX_DISTAL,
    PL_HUMANOID_BONE_RIGHT_INDEX_PROXIMAL,
    PL_HUMANOID_BONE_RIGHT_MIDDLE_PROXIMAL,
    PL_HUMANOID_BONE_RIGHT_MIDDLE_INTERMEDIATE,
    PL_HUMANOID_BONE_RIGHT_MIDDLE_DISTAL,
    PL_HUMANOID_BONE_RIGHT_RING_PROXIMAL,
    PL_HUMANOID_BONE_RIGHT_RING_INTERMEDIATE,
    PL_HUMANOID_BONE_RIGHT_RING_DISTAL,
    PL_HUMANOID_BONE_RIGHT_LITTLE_PROXIMAL,
    PL_HUMANOID_BONE_RIGHT_LITTLE_INTERMEDIATE,
    PL_HUMANOID_BONE_RIGHT_LITTLE_DISTAL,

    PL_HUMANOID_BONE_COUNT
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAnimationSampler
{
    plAnimationMode tMode;
    uint32_t        uDataIndex;
} plAnimationSampler;

typedef struct _plAnimationChannel
{
    plAnimationPath tPath;
    uint32_t        uTargetIndex;
    uint32_t        uSamplerIndex;
} plAnimationChannel;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plHumanoidComponent
{
    plEntity atBones[PL_HUMANOID_BONE_COUNT];
} plHumanoidComponent;

typedef struct _plAnimationData
{
    uint32_t uKeyFrameCount;
    size_t   szDataSize;
    float*   afKeyFrameTimes;
    void*    pKeyFrameData;
} plAnimationData;

typedef struct _plAnimation
{
    float               fStart;
    float               fEnd;

    uint32_t            uChannelCount;
    plAnimationChannel* atChannels;
    plAnimationSampler* atSamplers;
    plAnimationData*    atData;
    uint8_t*            puRawData;
} plAnimation;

typedef struct _plAnimationComponent
{
    plAnimationFlags    tFlags;
    float               fTimer;
    float               fSpeed;
    float               fBlendAmount;
    uint32_t            uTargetCount;
    plEntity*           atTargets;
    plAssetHandle       tAnimation;
} plAnimationComponent;

#ifdef __cplusplus
}
#endif

#endif // PL_ANIMATION_EXT_H