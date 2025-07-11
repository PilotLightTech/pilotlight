/*
   app_tests.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "pl.h"

// libs
#include "pl_test.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// unstable extensions
#include "pl_collision_ext.h"
#include "pl_datetime_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plIOI*        gptIO        = NULL;
const plMemoryI*    gptMemory    = NULL;
const plCollisionI* gptCollision = NULL;
const plDateTimeI*  gptDateTime  = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    int a;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] test declarations
//-----------------------------------------------------------------------------

void collision_only_tests_0(void*);
void datetime_tests_0(void*);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_collision_ext", "pl_load_collision_ext", "pl_unload_collision_ext", false);
    ptExtensionRegistry->load("pl_datetime_ext", "pl_load_datetime_ext", "pl_unload_datetime_ext", false);

    // retrieve the IO API required to use plIO for "talking" with runtime)
    gptIO        = pl_get_api_latest(ptApiRegistry, plIOI);
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptCollision = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptDateTime  = pl_get_api_latest(ptApiRegistry, plDateTimeI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // create test context
    plTestOptions tOptions = {};
    tOptions.bPrintSuiteResults = true;
    tOptions.bPrintColor = true;
    plTestContext* ptTestContext = pl_create_test_context(tOptions);

    pl_test_register_test(collision_only_tests_0, ptAppData);
    pl_test_run_suite("pl_collision_ext.h");

    pl_test_register_test(datetime_tests_0, ptAppData);
    pl_test_run_suite("pl_datetime_ext.h");

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    PL_FREE(ptAppData);

    bool bResult = pl_test_finish();

    if(!bResult)
    {
        exit(1);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    gptIO->new_frame(); // must be called once at the beginning of a frame


    plIO* ptIO = gptIO->get_io();
    ptIO->bRunning = false;
}

//-----------------------------------------------------------------------------
// [SECTION] test implementations
//-----------------------------------------------------------------------------

void
collision_only_tests_0(void* pAppData)
{
    plAppData* ptAppData = (plAppData*)pAppData;

    plSphere tSphere0 = {};
    tSphere0.fRadius = 1.0f;
    tSphere0.tCenter = {0};

    plSphere tSphere1 = {};
    tSphere1.fRadius = 1.0f;
    tSphere1.tCenter = {0.4f, 0.0f, 0.0f};

    plSphere tSphere2 = {};
    tSphere2.fRadius = 1.0f;
    tSphere2.tCenter = {2.0f, 2.0f, 0.0f};

    plBox tBox0 = {};
    tBox0.tHalfSize = {0.5f, 0.5f, 0.5f};
    tBox0.tTransform = pl_identity_mat4();

    plBox tBox1 = {};
    tBox1.tHalfSize = {0.5f, 0.5f, 0.5f};
    tBox1.tTransform = pl_mat4_translate_xyz(0.25f, 0.25f, 0.1f);

    plBox tBox2 = {};
    tBox2.tHalfSize = {0.5f, 0.5f, 0.5f};
    tBox2.tTransform = pl_mat4_translate_xyz(1.0f, 1.0f, 0.3f);

    plPlane tPlane0 = {};
    tPlane0.fOffset = 0.0f;
    tPlane0.tDirection = {0.0f, 1.0f, 0.0f};

    plPlane tPlane1 = {};
    tPlane1.fOffset = -2.0f;
    tPlane1.tDirection = {0.0f, 1.0f, 0.0f};

    plPlane tPlane2 = {};
    tPlane2.fOffset = -1.1f;
    tPlane2.tDirection = {1.0f, 0.0f, 0.0f};

    pl_test_expect_true(gptCollision->sphere_sphere(&tSphere0, &tSphere0), "0: Sphere + Sphere");
    pl_test_expect_false(gptCollision->sphere_sphere(&tSphere0, &tSphere2), "1: Sphere + Sphere");
    pl_test_expect_true(gptCollision->box_box(&tBox0, &tBox1), "2: Box + Box");
    pl_test_expect_false(gptCollision->box_box(&tBox0, &tBox2), "3: Box + Box");

    pl_test_expect_true(gptCollision->box_sphere(&tBox0, &tSphere0), "4: Box + Sphere");
    pl_test_expect_true(gptCollision->box_sphere(&tBox0, &tSphere1), "5: Box + Sphere");
    pl_test_expect_false(gptCollision->box_sphere(&tBox0, &tSphere2), "6: Box + Sphere");

    pl_test_expect_true(gptCollision->sphere_half_space(&tSphere0, &tPlane0), "7: Sphere + Half Space");
    pl_test_expect_false(gptCollision->sphere_half_space(&tSphere0, &tPlane1), "8: Sphere + Half Space");
    pl_test_expect_false(gptCollision->sphere_half_space(&tSphere0, &tPlane2), "9: Sphere + Half Space");

    pl_test_expect_true(gptCollision->box_half_space(&tBox0, &tPlane0), "10: Box + Half Space");
    pl_test_expect_false(gptCollision->box_half_space(&tBox0, &tPlane1), "11: Box + Half Space");
    pl_test_expect_false(gptCollision->box_half_space(&tBox0, &tPlane2), "12: Box + Half Space");
}

void
datetime_tests_0(void* pAppData)
{
    {
        plDay tDay = gptDateTime->day_of_week(PL_JUNE, 1, 2025);
        pl_test_expect_true(tDay == PL_SUNDAY, "0: day of week check");
    }

    {
        plDay tDay = gptDateTime->day_of_week(PL_APRIL, 9, 2024);
        pl_test_expect_true(tDay == PL_TUESDAY, "1: day of week check");
    }

    {
        plDay tDay = gptDateTime->day_of_week(PL_APRIL, 11, 2024);
        pl_test_expect_true(tDay == PL_THURSDAY, "2: day of week check");
    }

    pl_test_expect_true(gptDateTime->month_from_string("JANUARY")   == PL_JANUARY,   " 0: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("FEBRUARY")  == PL_FEBRUARY,  " 1: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("MARCH")     == PL_MARCH,     " 2: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("APRIL")     == PL_APRIL,     " 3: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("MAY")       == PL_MAY,       " 4: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("JUNE")      == PL_JUNE,      " 5: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("JULY")      == PL_JULY,      " 6: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("AUGUST")    == PL_AUGUST,    " 7: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("SEPTEMBER") == PL_SEPTEMBER, " 8: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("OCTOBER")   == PL_OCTOBER,   " 9: month from string");
    pl_test_expect_true(gptDateTime->month_from_string("DECEMBER")  == PL_DECEMBER,  "10: month from string");
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_TEST_WIN32_COLOR
#define PL_TEST_IMPLEMENTATION
#include "pl_test.h"
