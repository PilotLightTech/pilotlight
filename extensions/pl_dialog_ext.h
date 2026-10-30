/*
   pl_dialog_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DIALOG_EXT_H
#define PL_DIALOG_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plDialogI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plFileDialogFilter plFileDialogFilter;
typedef struct _plFileDialogDesc   plFileDialogDesc;

// enums/flags
typedef int plFileDialogMode;
typedef int plDialogResult;


//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_dialog_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_dialog_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDialogI
{

    // Copies descriptor data. Returns false if a dialog is already active.
    bool (*open_file_dialog)(const plFileDialogDesc*);

    // Call each frame. ACCEPTED/CANCELLED are returned once.
    plDialogResult (*draw_file_dialog)(void);

    // Full selected path, valid until the next successful open or unload.
    // NULL until a selection has been accepted.
    const char* (*get_file_dialog_path)(void);

    // Programmatically dismiss; no accepted/cancelled event.
    void (*close_file_dialog)(void);

} plDialogI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// typedef struct _plFileDialogFilter
// {
//     const char*  pcName;          // e.g. "Images"
//     const char** apcExtensions;   // e.g. {"png", "jpg", "jpeg"}
//     uint32_t     uExtensionCount;
// } plFileDialogFilter;

typedef struct _plFileDialogDesc
{
    plFileDialogMode eMode;

    const char* pcTitle;             // NULL: mode-specific default
    const char* pcStartDirectory;    // NULL: implementation default
    const char* pcInitialFileName;   // optional, primarily for save

    // const plFileDialogFilter* atFilters;
    // uint32_t                 uFilterCount; // 0: all files
} plFileDialogDesc;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plFileDialogMode
{
    PL_FILE_DIALOG_MODE_OPEN_FILE,
    // PL_FILE_DIALOG_MODE_SAVE_FILE,
    // PL_FILE_DIALOG_MODE_SELECT_DIRECTORY,
};

enum _plDialogResult
{
    PL_DIALOG_RESULT_NONE,       // inactive or still open
    PL_DIALOG_RESULT_ACCEPTED,
    PL_DIALOG_RESULT_CANCELLED,
};

#ifdef __cplusplus
}
#endif

#endif // PL_DIALOG_EXT_H