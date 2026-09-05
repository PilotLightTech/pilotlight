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

// main
PL_API bool          pl_json_load           (const char* pcJson, plJsonObject** pptJsonOut);
PL_API void          pl_json_unload         (plJsonObject**);
PL_API plJsonObject* pl_json_new_root_object(const char* pcName); // for writing
PL_API char*         pl_json_write          (plJsonObject*, char* pcBuffer, uint32_t* puBufferSize);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~reading~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// members
PL_API plJsonObject* pl_json_member_by_index(plJsonObject*, uint32_t uIndex); // used in writing too
PL_API void          pl_json_member_list    (plJsonObject*, char** pcListOut, uint32_t* puSizeOut, uint32_t* puLength);
PL_API bool          pl_json_member_exist   (plJsonObject*, const char* pcName);
PL_API plJsonType    pl_json_get_type       (plJsonObject*);
PL_API const char*   pl_json_get_name       (plJsonObject*);

// retrieve and cast values (default used if member isn't present)
PL_API int           pl_json_int_member   (plJsonObject*, const char* pcName,      int iDefaultValue);
PL_API uint32_t      pl_json_uint32_member(plJsonObject*, const char* pcName, uint32_t uDefaultValue);
PL_API uint64_t      pl_json_uint64_member(plJsonObject*, const char* pcName, uint64_t uDefaultValue);
PL_API float         pl_json_float_member (plJsonObject*, const char* pcName,    float fDefaultValue);
PL_API double        pl_json_double_member(plJsonObject*, const char* pcName,   double dDefaultValue);
PL_API char*         pl_json_string_member(plJsonObject*, const char* pcName,    char* pcDefaultValue, uint32_t uLength);
PL_API bool          pl_json_bool_member  (plJsonObject*, const char* pcName,    bool bDefaultValue);
PL_API plJsonObject* pl_json_member       (plJsonObject*, const char* pcName);
PL_API plJsonObject* pl_json_array_member (plJsonObject*, const char* pcName, uint32_t* puSizeOut);

// retrieve and cast array values (default used if member isn't present)
PL_API void pl_json_int_array_member   (plJsonObject*, const char* pcName,      int* piOut, uint32_t* puSizeOut);
PL_API void pl_json_uint32_array_member(plJsonObject*, const char* pcName, uint32_t* puOut, uint32_t* puSizeOut);
PL_API void pl_json_uint64_array_member(plJsonObject*, const char* pcName, uint64_t* puOut, uint32_t* puSizeOut);
PL_API void pl_json_float_array_member (plJsonObject*, const char* pcName,    float* pfOut, uint32_t* puSizeOut);
PL_API void pl_json_double_array_member(plJsonObject*, const char* pcName,   double* pdOut, uint32_t* puSizeOut);
PL_API void pl_json_bool_array_member  (plJsonObject*, const char* pcName,     bool* pbOut, uint32_t* puSizeOut);
PL_API void pl_json_string_array_member(plJsonObject*, const char* pcName,    char** pcOut, uint32_t* puSizeOut, uint32_t* puLength);

// cast values
PL_API int         pl_json_as_int   (plJsonObject*);
PL_API uint32_t    pl_json_as_uint32(plJsonObject*);
PL_API uint64_t    pl_json_as_uint64(plJsonObject*);
PL_API float       pl_json_as_float (plJsonObject*);
PL_API double      pl_json_as_double(plJsonObject*);
PL_API const char* pl_json_as_string(plJsonObject*); // do not store
PL_API bool        pl_json_as_bool  (plJsonObject*);

// cast array values
PL_API void pl_json_as_int_array   (plJsonObject*,      int* piOut, uint32_t* puSizeOut);
PL_API void pl_json_as_uint32_array(plJsonObject*, uint32_t* puOut, uint32_t* puSizeOut);
PL_API void pl_json_as_uint64_array(plJsonObject*, uint64_t* puOut, uint32_t* puSizeOut);
PL_API void pl_json_as_float_array (plJsonObject*,    float* pfOut, uint32_t* puSizeOut);
PL_API void pl_json_as_double_array(plJsonObject*,   double* pdOut, uint32_t* puSizeOut);
PL_API void pl_json_as_bool_array  (plJsonObject*,     bool* bpOut, uint32_t* puSizeOut);
PL_API void pl_json_as_string_array(plJsonObject*,    char** pcOut, uint32_t* puSizeOut, uint32_t* puLength);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~writing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// simple
PL_API void pl_json_add_int_member   (plJsonObject*, const char* pcName,         int);
PL_API void pl_json_add_uint32_member(plJsonObject*, const char* pcName,    uint32_t);
PL_API void pl_json_add_uint64_member(plJsonObject*, const char* pcName,    uint64_t);
PL_API void pl_json_add_float_member (plJsonObject*, const char* pcName,       float);
PL_API void pl_json_add_double_member(plJsonObject*, const char* pcName,      double);
PL_API void pl_json_add_bool_member  (plJsonObject*, const char* pcName,        bool);
PL_API void pl_json_add_string_member(plJsonObject*, const char* pcName, const char*);

// arrays
PL_API void pl_json_add_int_array   (plJsonObject*, const char* pcName, const int*, uint32_t uCount);
PL_API void pl_json_add_uint32_array(plJsonObject*, const char* pcName, const uint32_t*, uint32_t uCount);
PL_API void pl_json_add_uint64_array(plJsonObject*, const char* pcName, const uint64_t*, uint32_t uCount);
PL_API void pl_json_add_float_array (plJsonObject*, const char* pcName, const float*, uint32_t uCount);
PL_API void pl_json_add_double_array(plJsonObject*, const char* pcName, const double*, uint32_t uCount);
PL_API void pl_json_add_bool_array  (plJsonObject*, const char* pcName, const bool*, uint32_t uCount);
PL_API void pl_json_add_string_array(plJsonObject*, const char* pcName, const char**, uint32_t uCount);

// objects & object arrays
PL_API plJsonObject* pl_json_add_member      (plJsonObject*, const char* pcName);                  // returns object to be modified with above commands
PL_API plJsonObject* pl_json_add_member_array(plJsonObject*, const char* pcName, uint32_t uCount); // returns array of uCount length

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