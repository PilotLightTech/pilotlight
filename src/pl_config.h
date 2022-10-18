#ifndef PL_CONFIG_H
#define PL_CONFIG_H

//-----------------------------------------------------------------------------
// [SECTION] general settings
//-----------------------------------------------------------------------------

#define PL_MAX_FRAMES_IN_FLIGHT 2
#define PL_MAX_NAME_LENGTH 1024
#define PL_USE_STB_SPRINTF

//-----------------------------------------------------------------------------
// [SECTION] profile settings
//-----------------------------------------------------------------------------

#define PL_PROFILE_ON

//-----------------------------------------------------------------------------
// [SECTION] log settings
//-----------------------------------------------------------------------------

#define PL_LOG_ON
#ifndef PL_GLOBAL_LOG_LEVEL
    #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL
#endif

// #define PL_LOG_TRACE_BOLD
// #define PL_LOG_DEBUG_BOLD
// #define PL_LOG_INFO_BOLD 
// #define PL_LOG_WARN_BOLD 
#define PL_LOG_ERROR_BOLD
#define PL_LOG_FATAL_BOLD

// #define PL_LOG_TRACE_UNDERLINE
// #define PL_LOG_DEBUG_UNDERLINE
// #define PL_LOG_INFO_UNDERLINE 
// #define PL_LOG_WARN_UNDERLINE 
// #define PL_LOG_ERROR_UNDERLINE
// #define PL_LOG_FATAL_UNDERLINE

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