/*
   pl_test.h
     * simple test librsry
     
   Do this:
        #define PL_TEST_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_TEST_IMPLEMENTATION
   #include "pl_test.h"
   Notes:
   * for console color output on windows, define "PL_TEST_WIN32_COLOR" before
     including the implementation
*/

// library version (format XYYZZ)
#define PL_TEST_VERSION    "1.0.1"
#define PL_TEST_VERSION_NUM 10001

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] public api
// [SECTION] structs
// [SECTION] private
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TEST_H
#define PL_TEST_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint32_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plTestContext plTestContext;
typedef struct _plTestOptions plTestOptions;

typedef void (*PL_TEST_FUNCTION)(void*);

//-----------------------------------------------------------------------------
// [SECTION] macros
//-----------------------------------------------------------------------------

#define pl_test_register_test(TEST, DATA) pl__test_register_test((TEST), (DATA), #TEST)

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plTestContext* pl_create_test_context(plTestOptions);

// tests
void pl_test_run_suite(const char* pcSuiteName);
bool pl_test_finish(void);

// booleans
bool pl_test_expect_true (bool bValue, const char* pcMsg);
bool pl_test_expect_false(bool bValue, const char* pcMsg);

// integers
bool pl_test_expect_int_equal       (int iValue0, int iValue1, const char* pcMsg);
bool pl_test_expect_int_not_equal   (int iValue0, int iValue1, const char* pcMsg);
bool pl_test_expect_uint32_equal    (uint32_t uValue0, uint32_t uValue1, const char* pcMsg);
bool pl_test_expect_uint32_not_equal(uint32_t uValue0, uint32_t uValue1, const char* pcMsg);
bool pl_test_expect_uint64_equal    (uint64_t uValue0, uint64_t uValue1, const char* pcMsg);
bool pl_test_expect_uint64_not_equal(uint64_t uValue0, uint64_t uValue1, const char* pcMsg);

// floating point
bool pl_test_expect_float_near_equal     (float fValue0, float fValue1, float fError, const char* pcMsg);
bool pl_test_expect_float_near_not_equal (float fValue0, float fValue1, float fError, const char* pcMsg);
bool pl_test_expect_double_near_equal    (double dValue0, double dValue1, double dError, const char* pcMsg);
bool pl_test_expect_double_near_not_equal(double dValue0, double dValue1, double dError, const char* pcMsg);

// strings
bool pl_test_expect_string_equal    (const char* pcValue0, const char* pcValue1, const char* pcMsg);
bool pl_test_expect_string_not_equal(const char* pcValue0, const char* pcValue1, const char* pcMsg);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTestOptions
{
    bool bPrintSuiteResults;
    bool bPrintAllPassedChecks;
    bool bPrintColor;
} plTestOptions;

//-----------------------------------------------------------------------------
// [SECTION] private
//-----------------------------------------------------------------------------

void pl__test_register_test(PL_TEST_FUNCTION tTest, void* pData, const char* pcName);

#endif // PL_TEST_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] public api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifdef PL_TEST_IMPLEMENTATION

#if defined(PL_TEST_WIN32_COLOR) && defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    static DWORD  gtOriginalMode = 0;
    static HANDLE gtStdOutHandle = 0;
    static bool   gbActiveColor = 0;
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void pl__test_print_va   (const char* cPFormat, va_list args);
void static pl__test_print_red  (const char* cPFormat, const char* pcMsg, ...);
void static pl__test_print_green(const char* cPFormat, const char* pcMsg, ...);

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plTest
{
    bool             bFailureOccured;
    const char*      pcName;
    PL_TEST_FUNCTION tTest;
    void*            pData;
} plTest;

typedef struct _plTestContext
{
    plTest*       atTests;
    plTest*       ptCurrentTest;
    uint32_t      uTestSize;
    uint32_t      uTestCapacity;
    uint32_t      uFailedTest;
    plTestOptions tOptions;

    uint32_t uTotalPassedTests;
    uint32_t uTotalFailedTests;
} plTestContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

plTestContext* gptTestContext = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plTestContext*
pl_create_test_context(plTestOptions tOptions)
{

    #if defined(PL_TEST_WIN32_COLOR) && defined(_WIN32)
        DWORD tCurrentMode = 0;
        gbActiveColor = true;
        gtStdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            if(gtStdOutHandle == INVALID_HANDLE_VALUE)
                gbActiveColor = false;
            else if(!GetConsoleMode(gtStdOutHandle, &tCurrentMode))
                gbActiveColor = false;
            gtOriginalMode = tCurrentMode;
            tCurrentMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // enable ANSI escape codes
            if(!SetConsoleMode(gtStdOutHandle, tCurrentMode))
                gbActiveColor = false;

        if(!gbActiveColor)
            tOptions.bPrintColor = false;
    #endif

    gptTestContext = (plTestContext*)malloc(sizeof(plTestContext));
    memset(gptTestContext, 0, sizeof(plTestContext));
    gptTestContext->uTestCapacity = 64;
    gptTestContext->tOptions = tOptions;
    gptTestContext->atTests = (plTest*)malloc(gptTestContext->uTestCapacity * sizeof(plTest));
    memset(gptTestContext->atTests, 0, sizeof(gptTestContext->uTestCapacity * sizeof(plTest)));
    return gptTestContext;
}

void
pl__test_register_test(PL_TEST_FUNCTION tTest, void* pData, const char* pcName)
{   
    gptTestContext->uTestSize++;
    if(gptTestContext->uTestSize == gptTestContext->uTestCapacity)
    {
        plTest* atOldTests = gptTestContext->atTests;
        gptTestContext->atTests = (plTest*)malloc(gptTestContext->uTestCapacity * sizeof(plTest) * 2);
        memset(gptTestContext->atTests, 0, sizeof(gptTestContext->uTestCapacity * sizeof(plTest) * 2));
        memcpy(gptTestContext->atTests, atOldTests, gptTestContext->uTestCapacity * sizeof(plTest));
        free(atOldTests);
        gptTestContext->uTestCapacity *= 2;
    }
    gptTestContext->atTests[gptTestContext->uTestSize - 1].pcName = pcName;
    gptTestContext->atTests[gptTestContext->uTestSize - 1].tTest = tTest;
    gptTestContext->atTests[gptTestContext->uTestSize - 1].bFailureOccured = false;
    gptTestContext->atTests[gptTestContext->uTestSize - 1].pData = pData;
}

void
pl_test_run_suite(const char* pcSuiteName)
{
    printf("\n------%s suite------\n\n", pcSuiteName);

    for(uint32_t i = 0; i < gptTestContext->uTestSize; i++)
    {
        
        gptTestContext->ptCurrentTest = &gptTestContext->atTests[i];
        gptTestContext->ptCurrentTest->bFailureOccured = false;

        printf("running -> \"%s\"\n", gptTestContext->ptCurrentTest->pcName);
        gptTestContext->ptCurrentTest->tTest(gptTestContext->ptCurrentTest->pData);

        if(gptTestContext->ptCurrentTest->bFailureOccured)
        {
            pl__test_print_red("%s", NULL, "        -> failed");
            gptTestContext->uFailedTest++;
        }
        else
            pl__test_print_green("%s", NULL, "           passed");
    }

    if(gptTestContext->tOptions.bPrintSuiteResults)
    {
        printf("\nPassed: ");
        pl__test_print_green("%u", NULL, gptTestContext->uTestSize - gptTestContext->uFailedTest);

        printf("Failed: ");
        pl__test_print_red("%u", NULL, gptTestContext->uFailedTest);
    }

    // printf("\n------End tests------\n\n");

    // reset context
    gptTestContext->uTotalPassedTests += gptTestContext->uTestSize - gptTestContext->uFailedTest;
    gptTestContext->uTotalFailedTests += gptTestContext->uFailedTest;
    gptTestContext->uTestSize = 0;
    gptTestContext->uFailedTest = 0;
    gptTestContext->ptCurrentTest = NULL;
    memset(gptTestContext->atTests, 0, sizeof(plTest) * gptTestContext->uTestCapacity);
}

bool
pl_test_finish(void)
{

    printf("\n------Results------\n");

    printf("\nTests passed: ");
    pl__test_print_green("%u", NULL, gptTestContext->uTotalPassedTests);

    printf("Tests failed: ");
    pl__test_print_red("%u", NULL, gptTestContext->uTotalFailedTests);

    #if defined(PL_TEST_WIN32_COLOR) && defined(_WIN32)
    if(gbActiveColor)
    {
        if(!SetConsoleMode(gtStdOutHandle, gtOriginalMode))
            exit(GetLastError());
    }
    #endif

    return gptTestContext->uTotalFailedTests == 0;
}

bool
pl_test_expect_true(bool bValue, const char* pcMsg)
{
    if(bValue)
    {
        if(gptTestContext->tOptions.bPrintAllPassedChecks)
            pl__test_print_green("Value: true  | Expected Value: true", pcMsg);
        return true;
    }

    pl__test_print_red("Value: false | Expected Value: true", pcMsg);
    gptTestContext->ptCurrentTest->bFailureOccured = true;
    return false;
}

bool
pl_test_expect_false(bool bValue, const char* pcMsg)
{
    if(bValue)
    {
        pl__test_print_red("Value: true  | Expected Value: false", pcMsg);
        gptTestContext->ptCurrentTest->bFailureOccured = true;
        return false;
    }

    if(gptTestContext->tOptions.bPrintAllPassedChecks)
        pl__test_print_green("Value: false | Expected Value: false", pcMsg);
    return true;
}

#define pl__test_expect_equal(value0, value1, pcMsg, format) \
    if((value0) == (value1)) \
    { \
        if(gptTestContext->tOptions.bPrintAllPassedChecks) \
            pl__test_print_green(format " equals " format " | Equality Expected", (pcMsg), (value0), (value1)); \
        return true; \
    } \
    pl__test_print_red(format " does not equal " format " | Equality Expected", (pcMsg), (value0), (value1)); \
    gptTestContext->ptCurrentTest->bFailureOccured = true; \
    return false;

#define pl__test_expect_not_equal(value0, value1, pcMsg, format) \
    if((value0) == (value1)) \
    { \
        pl__test_print_red(format " equals " format " | Equality Not Expected", (pcMsg), (value0), (value1)); \
        gptTestContext->ptCurrentTest->bFailureOccured = true; \
        return false; \
    } \
    if(gptTestContext->tOptions.bPrintAllPassedChecks) \
        pl__test_print_green(format " does not equal " format " | Equality Expected", (pcMsg), (value0), (value1)); \
    return true;

bool
pl_test_expect_int_equal(int iValue0, int iValue1, const char* pcMsg)
{
    pl__test_expect_equal(iValue0, iValue1, pcMsg, "%i");
}

bool
pl_test_expect_int_not_equal(int iValue0, int iValue1, const char* pcMsg)
{
    pl__test_expect_not_equal(iValue0, iValue1, pcMsg, "%i");   
}

bool
pl_test_expect_uint64_equal(uint64_t uValue0, uint64_t uValue1, const char* pcMsg)
{
    pl__test_expect_equal(uValue0, uValue1, pcMsg, "%llu");
}

bool
pl_test_expect_uint64_not_equal(uint64_t uValue0, uint64_t uValue1, const char* pcMsg)
{
    pl__test_expect_not_equal(uValue0, uValue1, pcMsg, "%llu");   
}

bool
pl_test_expect_uint32_equal(uint32_t uValue0, uint32_t uValue1, const char* pcMsg)
{
    pl__test_expect_equal(uValue0, uValue1, pcMsg, "%u");   
}

bool
pl_test_expect_uint32_not_equal(uint32_t uValue0, uint32_t uValue1, const char* pcMsg)
{
    pl__test_expect_not_equal(uValue0, uValue1, pcMsg, "%u");   
}

bool
pl_test_expect_float_near_equal(float fValue0, float fValue1, float fError, const char* pcMsg)
{
    return pl_test_expect_double_near_equal((double)fValue0, (double)fValue1, (double)fError, pcMsg);
}

bool
pl_test_expect_float_near_not_equal(float fValue0, float fValue1, float fError, const char* pcMsg)
{
    return pl_test_expect_double_near_not_equal((double)fValue0, (double)fValue1, (double)fError, pcMsg);
}

bool
pl_test_expect_double_near_equal(double dValue0, double dValue1, double dError, const char* pcMsg)
{
    if(dValue0 >= dValue1 - dError && dValue0 <= dValue1 + dError)
    {
        if(gptTestContext->tOptions.bPrintAllPassedChecks)
            pl__test_print_green("%0.6f equals %0.6f | Equality Expected within %0.6f", pcMsg, dValue0, dValue1, dError);
        return true;
    }

    pl__test_print_red("%0.6f does not equal %0.6f | Equality Expected within %0.6f", pcMsg, dValue0, dValue1, dError);
    gptTestContext->ptCurrentTest->bFailureOccured = true;
    return false;
}

bool
pl_test_expect_double_near_not_equal(double dValue0, double dValue1, double dError, const char* pcMsg)
{
    if(dValue0 >= dValue1 - dError && dValue0 <= dValue1 + dError)
    {
        pl__test_print_red("%0.f equals %0.6f | Equality Not Expected within %0.6f", pcMsg, dValue0, dValue1, dError);
        gptTestContext->ptCurrentTest->bFailureOccured = true;
        return false;
    }

    if(gptTestContext->tOptions.bPrintAllPassedChecks)
        pl__test_print_green("%0.6f does not equal %0.6f | Equality Not Expected within %0.6f", pcMsg, dValue0, dValue1, dError);
    return true; 
}

bool
pl_test_expect_string_equal(const char* pcValue0, const char* pcValue1, const char* pcMsg)
{
    if(strcmp(pcValue0, pcValue1) == 0)
    {
        if(gptTestContext->tOptions.bPrintAllPassedChecks)
            pl__test_print_green("\"%s\" equals \"%s\" | Equality Expected", pcMsg, pcValue0, pcValue1);
        return true;
    }

    pl__test_print_red("\"%s\" does not equal \"%s\" | Equality Expected", pcMsg, pcValue0, pcValue1);
    gptTestContext->ptCurrentTest->bFailureOccured = true;
    return false;   
}

bool
pl_test_expect_string_not_equal(const char* pcValue0, const char* pcValue1, const char* pcMsg)
{
    if(strcmp(pcValue0, pcValue1) == 0)
    {
        if(gptTestContext->tOptions.bPrintAllPassedChecks)
            pl__test_print_green("\"%s\" equals \"%s\" | Equality Not Expected", pcMsg, pcValue0, pcValue1);
        gptTestContext->ptCurrentTest->bFailureOccured = true;
        return false;
    }

    pl__test_print_red("\"%s\" does not equal \"%s\" | Equality Not Expected", pcMsg, pcValue0, pcValue1);
    return true;   
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__test_print_va(const char* cPFormat, va_list args)
{
    
    char dest[1024];
    vsnprintf(dest, 1024, cPFormat, args); 
    printf("%s", dest); 

}

void static
pl__test_print_red(const char* cPFormat, const char* pcMsg, ...)
{
    if(gptTestContext->tOptions.bPrintColor)
    {
        #ifdef _WIN32
            printf("[91m");
        #else
            printf("\033[91m");
        #endif
    }

    va_list argptr;
    va_start(argptr, pcMsg);
    pl__test_print_va(cPFormat, argptr);
    va_end(argptr);  

    if(pcMsg)
        printf(" %s\n", pcMsg);
    else
        printf("\n");  

    if(gptTestContext->tOptions.bPrintColor)
    {
        #ifdef _WIN32
            printf("[0m");
        #else
            printf("\033[0m");
        #endif 
    }
}

static void
pl__test_print_green(const char* cPFormat, const char* pcMsg, ...)
{
    if(gptTestContext->tOptions.bPrintColor)
    {
        #ifdef _WIN32
            printf("[92m");
        #else
            printf("\033[92m");
        #endif
    }

    va_list argptr;
    va_start(argptr, pcMsg);
    pl__test_print_va(cPFormat, argptr);
    va_end(argptr); 

    if(pcMsg)
        printf(" %s\n", pcMsg);
    else
        printf("\n");  

    if(gptTestContext->tOptions.bPrintColor)
    {
        #ifdef _WIN32
            printf("[0m");
        #else
            printf("\033[0m");
        #endif 
    }
}

#endif // PL_TEST_IMPLEMENTATION