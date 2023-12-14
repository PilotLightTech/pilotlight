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
// #define PL_LOG_CYCLIC_BUFFER_SIZE 256
#define PL_LOG_ON
#define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL
#define PL_LOG_ERROR_BOLD
#define PL_LOG_FATAL_BOLD
#define PL_LOG_TRACE_BG_COLOR PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_DEBUG_BG_COLOR PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_INFO_BG_COLOR  PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_WARN_BG_COLOR  PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_ERROR_BG_COLOR PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_FATAL_BG_COLOR PL_LOG_BG_COLOR_CODE_RED
#define PL_LOG_TRACE_FG_COLOR PL_LOG_FG_COLOR_CODE_GREEN
#define PL_LOG_DEBUG_FG_COLOR PL_LOG_FG_COLOR_CODE_CYAN
#define PL_LOG_INFO_FG_COLOR  PL_LOG_FG_COLOR_CODE_WHITE
#define PL_LOG_WARN_FG_COLOR  PL_LOG_FG_COLOR_CODE_YELLOW
#define PL_LOG_ERROR_FG_COLOR PL_LOG_FG_COLOR_CODE_RED
#define PL_LOG_FATAL_FG_COLOR PL_LOG_FG_COLOR_CODE_WHITE

#endif // PL_CONFIG_H