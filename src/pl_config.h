/*
    pl_config.h
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CONFIG_H
#define PL_CONFIG_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

// general
#define PL_MEMORY_TRACKING_ON
#define PL_USE_STB_SPRINTF
// #define PL_MAX_API_FUNCTIONS 256
// #define PL_MAX_NAME_LENGTH 1024
// #define PL_MAX_PATH_LENGTH 1024

// profiling
#define PL_PROFILE_ON

// logging (see pl_log.h)
#define PL_LOG_ON
// #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL

// shaders
// #define PL_OFFLINE_SHADERS_ONLY
// #define PL_INCLUDE_SPIRV_CROSS

// experimental (don't use yet)
// #define PL_EXPERIMENTAL_RENDER_WHILE_RESIZE

#endif // PL_CONFIG_H