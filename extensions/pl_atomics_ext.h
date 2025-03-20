/*
   pl_atomics_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ATOMICS_EXT_H
#define PL_ATOMICS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plAtomicsI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plAtomicCounter plAtomicCounter; // opaque type

// enums
typedef int plAtomicsResult; // -> enum _plAtomicsResult // Enum:

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plAtomicsI
{

    plAtomicsResult (*create_atomic_counter)  (int64_t value, plAtomicCounter** counterPtrOut);
    void            (*destroy_atomic_counter) (plAtomicCounter**);
    void            (*atomic_store)           (plAtomicCounter*, int64_t value);
    int64_t         (*atomic_load)            (plAtomicCounter*);
    bool            (*atomic_compare_exchange)(plAtomicCounter*, int64_t expectedValue, int64_t desiredValue);
    int64_t         (*atomic_increment)       (plAtomicCounter*);
    int64_t         (*atomic_decrement)       (plAtomicCounter*);

} plAtomicsI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plAtomicsResult
{
    PL_ATOMICS_RESULT_FAIL    = 0,
    PL_ATOMICS_RESULT_SUCCESS = 1
};

#endif // PL_ATOMICS_EXT_H