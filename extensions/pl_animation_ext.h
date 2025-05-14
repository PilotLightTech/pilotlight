/*
   pl_animation_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api structs
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

#include <stdbool.h> // bool
#include "pl_ecs_ext.inl" // plEntity
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plAnimationChannel plAnimationChannel;
typedef struct _plAnimationSampler plAnimationSampler;

// ecs components
typedef struct _plAnimationComponent         plAnimationComponent;
typedef struct _plAnimationDataComponent     plAnimationDataComponent;
typedef struct _plInverseKinematicsComponent plInverseKinematicsComponent;
typedef struct _plHumanoidComponent          plHumanoidComponent;

// enums & flags
typedef int plAnimationMode;
typedef int plAnimationPath;
typedef int plAnimationFlags;
typedef int plHumanoidBone;

// external
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plAnimationI
{

    // system setup/shutdown/etc
    void (*register_ecs_system)(void);

    // entity helpers (creates entity and necessary components)
    //   - do NOT store out parameter; use it immediately
    plEntity (*create_animation)     (plComponentLibrary*, const char* pcName, plAnimationComponent**);
    plEntity (*create_animation_data)(plComponentLibrary*, const char* pcName, plAnimationDataComponent**);

    // systems
    void (*run_animation_update_system)         (plComponentLibrary*, float fDeltaTime);
    void (*run_inverse_kinematics_update_system)(plComponentLibrary*);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key_animation)(void);
    plEcsTypeKey (*get_ecs_type_key_animation_data)(void);
    plEcsTypeKey (*get_ecs_type_key_inverse_kinematics)(void);
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
    plEntity        tData;
} plAnimationSampler;

typedef struct _plAnimationChannel
{
    plAnimationPath tPath;
    plEntity        tTarget;
    uint32_t        uSamplerIndex;
} plAnimationChannel;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plHumanoidComponent
{
    plEntity atBones[PL_HUMANOID_BONE_COUNT];
} plHumanoidComponent;

typedef struct _plAnimationDataComponent
{
    float* sbfKeyFrameTimes;
    float* sbfKeyFrameData;
} plAnimationDataComponent;

typedef struct _plAnimationComponent
{
    plAnimationFlags    tFlags;
    float               fStart;
    float               fEnd;
    float               fTimer;
    float               fSpeed;
    float               fBlendAmount;
    plAnimationChannel* sbtChannels;
    plAnimationSampler* sbtSamplers;
} plAnimationComponent;

typedef struct _plInverseKinematicsComponent
{
    bool     bEnabled;
    plEntity tTarget;
    uint32_t uChainLength;
    uint32_t uIterationCount;
} plInverseKinematicsComponent;

#endif // PL_ANIMATION_EXT_H