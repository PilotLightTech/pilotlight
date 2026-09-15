/*
   pl_json_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_JSON_EXT_H
#define PL_JSON_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plJsonI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plJsonObject plJsonObject; // opaque pointer to json object

// enums
typedef int plJsonType;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_json_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_json_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plJsonI
{
    // main
    bool          (*load)           (const char* pcJson, plJsonObject** pptJsonOut);
    void          (*unload)         (plJsonObject**);
    plJsonObject* (*new_root_object)(const char* pcName); // for writing
    char*         (*write)          (plJsonObject*, char* pcBuffer, uint32_t* puBufferSize);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~reading~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // members
    plJsonObject* (*member_by_index)(plJsonObject*, uint32_t uIndex); // used in writing too
    void          (*member_list)    (plJsonObject*, char** pcListOut, uint32_t* puSizeOut, uint32_t* puLength);
    bool          (*member_exist)   (plJsonObject*, const char* pcName);
    plJsonType    (*get_type)       (plJsonObject*);
    const char*   (*get_name)       (plJsonObject*);

    // retrieve and cast values (default used if member isn't present)
    int           (*int_member)   (plJsonObject*, const char* pcName,      int iDefaultValue);
    uint32_t      (*uint32_member)(plJsonObject*, const char* pcName, uint32_t uDefaultValue);
    uint64_t      (*uint64_member)(plJsonObject*, const char* pcName, uint64_t uDefaultValue);
    float         (*float_member) (plJsonObject*, const char* pcName,    float fDefaultValue);
    double        (*double_member)(plJsonObject*, const char* pcName,   double dDefaultValue);
    char*         (*string_member)(plJsonObject*, const char* pcName,    char* pcDefaultValue, uint32_t uLength);
    bool          (*bool_member)  (plJsonObject*, const char* pcName,    bool bDefaultValue);
    plJsonObject* (*member)       (plJsonObject*, const char* pcName);
    plJsonObject* (*array_member) (plJsonObject*, const char* pcName, uint32_t* puSizeOut);

    // retrieve and cast array values (default used if member isn't present)
    void (*int_array_member)   (plJsonObject*, const char* pcName,      int* piOut, uint32_t* puSizeOut);
    void (*uint32_array_member)(plJsonObject*, const char* pcName, uint32_t* puOut, uint32_t* puSizeOut);
    void (*uint64_array_member)(plJsonObject*, const char* pcName, uint64_t* puOut, uint32_t* puSizeOut);
    void (*float_array_member) (plJsonObject*, const char* pcName,    float* pfOut, uint32_t* puSizeOut);
    void (*double_array_member)(plJsonObject*, const char* pcName,   double* pdOut, uint32_t* puSizeOut);
    void (*bool_array_member)  (plJsonObject*, const char* pcName,     bool* pbOut, uint32_t* puSizeOut);
    void (*string_array_member)(plJsonObject*, const char* pcName,    char** pcOut, uint32_t* puSizeOut, uint32_t* puLength);

    // cast values
    int         (*as_int)   (plJsonObject*);
    uint32_t    (*as_uint32)(plJsonObject*);
    uint64_t    (*as_uint64)(plJsonObject*);
    float       (*as_float) (plJsonObject*);
    double      (*as_double)(plJsonObject*);
    const char* (*as_string)(plJsonObject*); // do not store
    bool        (*as_bool)  (plJsonObject*);

    // cast array values
    void (*as_int_array)   (plJsonObject*,      int* piOut, uint32_t* puSizeOut);
    void (*as_uint32_array)(plJsonObject*, uint32_t* puOut, uint32_t* puSizeOut);
    void (*as_uint64_array)(plJsonObject*, uint64_t* puOut, uint32_t* puSizeOut);
    void (*as_float_array) (plJsonObject*,    float* pfOut, uint32_t* puSizeOut);
    void (*as_double_array)(plJsonObject*,   double* pdOut, uint32_t* puSizeOut);
    void (*as_bool_array)  (plJsonObject*,     bool* bpOut, uint32_t* puSizeOut);
    void (*as_string_array)(plJsonObject*,    char** pcOut, uint32_t* puSizeOut, uint32_t* puLength);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~writing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // simple
    void (*add_int_member)   (plJsonObject*, const char* pcName,         int);
    void (*add_uint32_member)(plJsonObject*, const char* pcName,    uint32_t);
    void (*add_uint64_member)(plJsonObject*, const char* pcName,    uint64_t);
    void (*add_float_member) (plJsonObject*, const char* pcName,       float);
    void (*add_double_member)(plJsonObject*, const char* pcName,      double);
    void (*add_bool_member)  (plJsonObject*, const char* pcName,        bool);
    void (*add_string_member)(plJsonObject*, const char* pcName, const char*);

    // arrays
    void (*add_int_array)   (plJsonObject*, const char* pcName, const int*, uint32_t uCount);
    void (*add_uint32_array)(plJsonObject*, const char* pcName, const uint32_t*, uint32_t uCount);
    void (*add_uint64_array)(plJsonObject*, const char* pcName, const uint64_t*, uint32_t uCount);
    void (*add_float_array) (plJsonObject*, const char* pcName, const float*, uint32_t uCount);
    void (*add_double_array)(plJsonObject*, const char* pcName, const double*, uint32_t uCount);
    void (*add_bool_array)  (plJsonObject*, const char* pcName, const bool*, uint32_t uCount);
    void (*add_string_array)(plJsonObject*, const char* pcName, const char**, uint32_t uCount);

    // objects & object arrays
    plJsonObject* (*add_member)      (plJsonObject*, const char* pcName);                  // returns object to be modified with above commands
    plJsonObject* (*add_member_array)(plJsonObject*, const char* pcName, uint32_t uCount); // returns array of uCount length

} plJsonI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plJsonType_
{
	PL_JSON_TYPE_UNSPECIFIED,
	PL_JSON_TYPE_STRING,
	PL_JSON_TYPE_ARRAY,
	PL_JSON_TYPE_NUMBER,
	PL_JSON_TYPE_BOOL,
	PL_JSON_TYPE_OBJECT,
	PL_JSON_TYPE_NULL,
};

#ifdef __cplusplus
}
#endif

#endif // PL_JSON_EXT_H