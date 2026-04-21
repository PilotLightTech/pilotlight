/*
   pl_datetime_ext.h
     - simple date & time operations
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DATETIME_EXT_H
#define PL_DATETIME_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plDateTimeI_version {1, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDate     plDate;
typedef struct _plTime     plTime;
typedef struct _plDateTime plDateTime;

// enums
typedef int plMonth; // -> enum _plMonth PL_XXXX
typedef int plDay;   // -> enum _plDay   PL_XXDAY

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDate
{
    plMonth tMonth;
    int     iDay; // 1 - 31
    int     iYear;
} plDate;

typedef struct _plTime
{  
    int iHour;   // 0 - 23 -> hours after midnight
    int iMinute; // 0 - 59 -> minutes after hour
    int iSecond; // 0 - 59 -> seconds after minute
} plTime;

typedef struct _plDateTime
{
    plDate tDate;
    plTime tTime;
} plDateTime;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_datetime_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_datetime_ext(plApiRegistryI*, bool reload);

PL_API plDateTime  pl_datetime_now              (void);
PL_API bool        pl_datetime_leap_year        (int);
PL_API const char* pl_datetime_month_as_string  (plMonth);
PL_API plMonth     pl_datetime_month_from_string(const char*);
PL_API int         pl_datetime_day_of_year      (plMonth, int day, int year);
PL_API plDay       pl_datetime_day_of_week      (plMonth, int day, int year);
PL_API int         pl_datetime_days_in_month    (plMonth, int year);
PL_API plDate      pl_datetime_day_of_year_date (int day);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDateTimeI
{
    plDateTime  (*now)              (void);
    bool        (*leap_year)        (int);
    const char* (*month_as_string)  (plMonth);
    plMonth     (*month_from_string)(const char*);
    int         (*day_of_year)      (plMonth, int day, int year);
    plDay       (*day_of_week)      (plMonth, int day, int year);
    int         (*days_in_month)    (plMonth, int year);
    plDate      (*day_of_year_date) (int day);
} plDateTimeI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plMonth {
    PL_JANUARY   = 1,
    PL_FEBRUARY  = 2,
    PL_MARCH     = 3,
    PL_APRIL     = 4,
    PL_MAY       = 5,
    PL_JUNE      = 6,
    PL_JULY      = 7,
    PL_AUGUST    = 8,
    PL_SEPTEMBER = 9,
    PL_OCTOBER   = 10,
    PL_NOVEMBER  = 11,
    PL_DECEMBER  = 12
};

enum _plDay {
    PL_SUNDAY    = 1,
    PL_MONDAY    = 2,
    PL_TUESDAY   = 3,
    PL_WEDNESDAY = 4,
    PL_THURSDAY  = 5,
    PL_FRIDAY    = 6,
    PL_SATURDAY  = 7,
};

#ifdef __cplusplus
}
#endif

#endif // PL_DATETIME_EXT_H