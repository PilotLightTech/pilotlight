/*
   pl_datetime_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <time.h>
#include "pl.h"
#include "pl_datetime_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static const char* gacMonthNames[] = {
    "ERROR",
    "JANUARY",
    "FEBRUARY",
    "MARCK",
    "APRIL",
    "MAY",
    "JUNE",
    "JULY",
    "AUGUST",
    "SEPTEMBER",
    "OCTOBER",
    "NOVEMBER",
    "DECEMBER"
};

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_datetime_leap_year(int iYear)
{
    if(iYear % 4 != 0)   return false;
    if(iYear % 100 != 0) return true;
    if(iYear % 400 == 0) return true;
    return false;
}

int
pl_datetime_days_in_month(plMonth tMonth, int iYear)
{
    switch(tMonth)
    {
        case PL_JANUARY:   return 31;
        case PL_FEBRUARY:  return pl_datetime_leap_year(iYear) ? 29 : 28;
        case PL_MARCH:     return 31;
        case PL_APRIL:     return 30;
        case PL_MAY:       return 31;
        case PL_JUNE:      return 30;
        case PL_JULY:      return 31;
        case PL_AUGUST:    return 31;
        case PL_SEPTEMBER: return 30;
        case PL_OCTOBER:   return 31;
        case PL_NOVEMBER:  return 30;
        case PL_DECEMBER:  return 31;
        default:           return -1;
    }
}

int
pl_datetime_day_of_year(plMonth tMonth, int iDay, int iYear)
{
    int iDayOfYear = 0;
    int iCurrentMonth = 1;
    while(iCurrentMonth < tMonth)
    {
        iDayOfYear += pl_datetime_days_in_month(iCurrentMonth, iYear);
        iCurrentMonth++;
    }
    iDayOfYear += iDay - 1;
    return iDayOfYear;
}

const char*
pl_datetime_month_as_string(plMonth tMonth)
{
    return gacMonthNames[tMonth];
}

static inline char
pl__datetime_to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? c &= ~32 : c;
}

plMonth
pl_datetime_month_from_string(const char* pcMonth)
{

    // NOTE (JHH): not sure if this is faster

    const char c0 = pl__datetime_to_upper(pcMonth[0]);
    
    switch(c0)
    {
        case 'F': return PL_FEBRUARY;
        case 'S': return PL_SEPTEMBER;
        case 'O': return PL_OCTOBER;
        case 'D': return PL_DECEMBER;
        case 'N': return PL_NOVEMBER;
    }

    const char c1 = pl__datetime_to_upper(pcMonth[1]);
    if(c1 == 'P')
        return PL_APRIL;

    if(c0 == 'A')
        return PL_AUGUST;

    const char c2 = pl__datetime_to_upper(pcMonth[2]);

    switch(c2)
    {
        case 'Y': return PL_MAY;
        case 'L': return PL_JULY;
        case 'R': return PL_MARCH;
    }

    if(c1 == 'A')
        return PL_JANUARY;

    return PL_JUNE;
}

plDate
pl_datetime_day_of_year_date(int iDay)
{
    static int iCurrentYear = 0;
    if(iCurrentYear == 0)
    {
        time_t tTime = time(0);
        struct tm* ptLocalTime = localtime(&tTime);
        iCurrentYear = ptLocalTime->tm_year + 1900;
    }

    int iDayOfYear = 0;
    int iCurrentMonth = 1;
    while(iCurrentMonth < 13)
    {
        iDayOfYear += pl_datetime_days_in_month(iCurrentMonth, iCurrentYear);
        if(iDay <= iDayOfYear)
        {
            iDayOfYear -= pl_datetime_days_in_month(iCurrentMonth, iCurrentYear);
            plDate tResult = {
                iCurrentMonth,
                iDay - iDayOfYear + 1,
                iCurrentYear
            };
            return tResult;
        }
        iCurrentMonth++;
    }
    plDate tResult = {1, 1, iCurrentYear};
    return tResult;
}

plDateTime
pl_datetime_now(void)
{
    time_t tTime = time(0);
    struct tm* ptLocalTime = localtime(&tTime);

    plDateTime tResult = {
        .tDate = {
            .tMonth = ptLocalTime->tm_mon + 1,
            .iDay   = ptLocalTime->tm_mday,
            .iYear  = ptLocalTime->tm_year + 1900
        },
        .tTime = {
            .iHour  = ptLocalTime->tm_hour,
            .iMinute = ptLocalTime->tm_min,
            .iSecond = ptLocalTime->tm_sec,
        }
    };
    return tResult;
}

plDay
pl_datetime_day_of_week(plMonth tMonth, int iDay, int iYear)
{
    tMonth--;
    iYear -= 1900;
    plDay tResult = (iDay                                                  
        + ((153 * (tMonth + 12 * ((14 - tMonth) / 12) - 3) + 2) / 5) 
        + (365 * (iYear + 4800 - ((14 - tMonth) / 12)))              
        + ((iYear + 4800 - ((14 - tMonth) / 12)) / 4)                
        - ((iYear + 4800 - ((14 - tMonth) / 12)) / 100)              
        + ((iYear + 4800 - ((14 - tMonth) / 12)) / 400)              
        - 32045                                                    
      ) % 7;
    if(tResult == 0)
      tResult = 7;
    return tResult;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_datetime_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDateTimeI tApi = {
        .now               = pl_datetime_now,
        .leap_year         = pl_datetime_leap_year,
        .month_as_string   = pl_datetime_month_as_string,
        .month_from_string = pl_datetime_month_from_string,
        .day_of_year       = pl_datetime_day_of_year,
        .days_in_month     = pl_datetime_days_in_month,
        .day_of_year_date  = pl_datetime_day_of_year_date,
        .day_of_week       = pl_datetime_day_of_week,
    };
    pl_set_api(ptApiRegistry, plDateTimeI, &tApi);
}

PL_EXPORT void
pl_unload_datetime_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plDateTimeI* ptApi = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    ptApiRegistry->remove_api(ptApi);
}
