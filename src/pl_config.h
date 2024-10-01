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
#define PL_USE_STB_SPRINTF
// #define PL_EXPERIMENTAL_RENDER_WHILE_RESIZE
//#define PL_MAX_NAME_LENGTH 1024
//#define PL_MAX_PATH_LENGTH 1024

// profiling
#define PL_PROFILE_ON

// logging (see pl_log.h)
#define PL_LOG_ON
#define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL

// shaders
// #define PL_OFFLINE_SHADERS_ONLY

// runtime options
// #define PL_HEADLESS_APP

// core extension options
#define PL_CORE_EXTENSION_INCLUDE_GRAPHICS
#define PL_CORE_EXTENSION_INCLUDE_SHADER

#endif // PL_CONFIG_H