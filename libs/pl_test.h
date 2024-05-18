/*
   pl_test
   Do this:
        #define PL_TEST_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_TEST_IMPLEMENTATION
   #include "pl_test.h"
*/

// library version
#define PL_TEST_VERSION    "0.1.0"
#define PL_TEST_VERSION_NUM 00100

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] public api
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

#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plTestContext plTestContext;

typedef void (*PL_TEST_FUNCTION)(void*);

//-----------------------------------------------------------------------------
// [SECTION] macros
//-----------------------------------------------------------------------------

#define pl_test_register_test(TEST, DATA) pl__test_register_test((TEST), (DATA), #TEST)


//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plTestContext* pl_create_test_context(void);

// tests
void pl__test_register_test(PL_TEST_FUNCTION tTest, void* pData, const char* pcName);
bool pl_test_run(void);

// booleans
bool pl_test_expect_true (bool bValue, const char* pcMsg);
bool pl_test_expect_false(bool bValue, const char* pcMsg);

// numbers
bool pl_test_expect_int_equal            (int iValue0, int iValue1, const char* pcMsg);
bool pl_test_expect_int_not_equal        (int iValue0, int iValue1, const char* pcMsg);
bool pl_test_expect_unsigned_equal       (uint32_t uValue0, uint32_t uValue1, const char* pcMsg);
bool pl_test_expect_unsigned_not_equal   (uint32_t uValue0, uint32_t uValue1, const char* pcMsg);
bool pl_test_expect_float_near_equal     (float fValue0, float fValue1, float fError, const char* pcMsg);
bool pl_test_expect_float_near_not_equal (float fValue0, float fValue1, float fError, const char* pcMsg);
bool pl_test_expect_double_near_equal    (double dValue0, double dValue1, double dError, const char* pcMsg);
bool pl_test_expect_double_near_not_equal(double dValue0, double dValue1, double dError, const char* pcMsg);

// strings
bool pl_test_expect_string_equal    (const char* pcValue0, const char* pcValue1, const char* pcMsg);
bool pl_test_expect_string_not_equal(const char* pcValue0, const char* pcValue1, const char* pcMsg);

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

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
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
    plTest*  atTests;
    plTest*  ptCurrentTest;
    uint32_t uTestSize;
    uint32_t uTestCapacity;
    uint32_t uFailedTest;
    bool     bPrintPasses;
} plTestContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

plTestContext* gptTestContext = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plTestContext*
pl_create_test_context(void)
{
    gptTestContext = (plTestContext*)malloc(sizeof(plTestContext));
    memset(gptTestContext, 0, sizeof(plTestContext));
    gptTestContext->uTestCapacity = 64;
    gptTestContext->bPrintPasses = false;
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

bool
pl_test_run(void)
{
    for(uint32_t i = 0; i < gptTestContext->uTestSize; i++)
    {
        
        gptTestContext->ptCurrentTest = &gptTestContext->atTests[i];
        gptTestContext->ptCurrentTest->bFailureOccured = false;

        printf("-----------------------------------\n");
        printf("\"%s\" running...\n\n", gptTestContext->ptCurrentTest->pcName);
        gptTestContext->ptCurrentTest->tTest(gptTestContext->ptCurrentTest->pData);

        if(gptTestContext->ptCurrentTest->bFailureOccured)
        {
            pl__test_print_red("\n\n\"%s\" failed", NULL, gptTestContext->ptCurrentTest->pcName);
            gptTestContext->uFailedTest++;
        }
        else
            printf("\n\n\"%s\" passed\n\n", gptTestContext->ptCurrentTest->pcName);
        printf("-----------------------------------\n");
    }

    return gptTestContext->uFailedTest == 0;
}

bool
pl_test_expect_true(bool bValue, const char* pcMsg)
{
    if(bValue)
    {
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

    pl__test_print_green("Value: false | Expected Value: false", pcMsg);
    return true;
}

bool
pl_test_expect_int_equal(int iValue0, int iValue1, const char* pcMsg)
{
    if(iValue0 == iValue1)
    {
        pl__test_print_green("%i equals %i | Equality Expected", pcMsg, iValue0, iValue1);
        return true;
    }

    pl__test_print_red("%i does not equal %i | Equality Expected", pcMsg, iValue0, iValue1);
    gptTestContext->ptCurrentTest->bFailureOccured = true;
    return false;
}

bool
pl_test_expect_int_not_equal(int iValue0, int iValue1, const char* pcMsg)
{
    if(iValue0 == iValue1)
    {
        pl__test_print_red("%i equals %i | Equality Not Expected", pcMsg, iValue0, iValue1);
        gptTestContext->ptCurrentTest->bFailureOccured = true;
        return false;
    }

    pl__test_print_green("%i does not equal %i | Equality Not Expected", pcMsg, iValue0, iValue1);
    return true;    
}

bool
pl_test_expect_unsigned_equal(uint32_t uValue0, uint32_t uValue1, const char* pcMsg)
{
    if(uValue0 == uValue1)
    {
        pl__test_print_green("%u equals %u | Equality Expected", pcMsg, uValue0, uValue1);
        return true;
    }

    pl__test_print_red("%u does not equal %u | Equality Expected", pcMsg, uValue0, uValue1);
    gptTestContext->ptCurrentTest->bFailureOccured = true;
    return false;
}

bool
pl_test_expect_unsigned_not_equal(uint32_t uValue0, uint32_t uValue1, const char* pcMsg)
{
    if(uValue0 == uValue1)
    {
        pl__test_print_red("%u equals %u | Equality Not Expected", pcMsg, uValue0, uValue1);
        gptTestContext->ptCurrentTest->bFailureOccured = true;
        return false;
    }

    pl__test_print_green("%u does not equal %u | Equality Not Expected", pcMsg, uValue0, uValue1);
    return true;    
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

    pl__test_print_green("%0.6f does not equal %0.6f | Equality Not Expected within %0.6f", pcMsg, dValue0, dValue1, dError);
    return true; 
}

bool
pl_test_expect_string_equal(const char* pcValue0, const char* pcValue1, const char* pcMsg)
{
    if(strcmp(pcValue0, pcValue1) == 0)
    {
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
    #ifdef _WIN32
        printf("[91m");
    #else
        printf("\033[91m");
    #endif

    va_list argptr;
    va_start(argptr, pcMsg);
    pl__test_print_va(cPFormat, argptr);
    va_end(argptr);  

    if(pcMsg)
        printf(" %s\n", pcMsg);
    else
        printf("\n");  

    #ifdef _WIN32
        printf("[0m");
    #else
        printf("\033[0m");
    #endif 
}

static void
pl__test_print_green(const char* cPFormat, const char* pcMsg, ...)
{
    if(!gptTestContext->bPrintPasses)
        return;

    #ifdef _WIN32
        printf("[92m");
    #else
        printf("\033[92m");
    #endif

    va_list argptr;
    va_start(argptr, pcMsg);
    pl__test_print_va(cPFormat, argptr);
    va_end(argptr); 

    if(pcMsg)
        printf(" %s\n", pcMsg);
    else
        printf("\n");  

    #ifdef _WIN32
        printf("[0m");
    #else
        printf("\033[0m");
    #endif 
}

#endif // PL_TEST_IMPLEMENTATION