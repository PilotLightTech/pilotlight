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

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plIOI*        gptIO        = NULL;
const plMemoryI*    gptMemory    = NULL;
const plCollisionI* gptCollision = NULL;

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

    // retrieve the IO API required to use plIO for "talking" with runtime)
    gptIO        = pl_get_api_latest(ptApiRegistry, plIOI);
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptCollision = pl_get_api_latest(ptApiRegistry, plCollisionI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // create test context
    plTestOptions tOptions = {
        .bPrintSuiteResults = true,
        .bPrintColor = true
    };
    plTestContext* ptTestContext = pl_create_test_context(tOptions);

    pl_test_register_test(collision_only_tests_0, ptAppData);
    pl_test_run_suite("pl_collision_ext.h");

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
pl_app_resize(plAppData* ptAppData)
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
    plAppData* ptAppData = pAppData;

    plSphere tSphere0 = {
        .fRadius = 1.0f,
        .tCenter = {0}
    };

    plSphere tSphere1 = {
        .fRadius = 1.0f,
        .tCenter = {0.4f, 0.0f, 0.0f}
    };

    plSphere tSphere2 = {
        .fRadius = 1.0f,
        .tCenter = {2.0f, 2.0f, 0.0f}
    };

    plBox tBox0 = {
        .tHalfSize = {0.5f, 0.5f, 0.5f},
        .tTransform = pl_identity_mat4()
    };

    plBox tBox1 = {
        .tHalfSize = {0.5f, 0.5f, 0.5f},
        .tTransform = pl_mat4_translate_xyz(0.25f, 0.25f, 0.1f)
    };

    plBox tBox2 = {
        .tHalfSize = {0.5f, 0.5f, 0.5f},
        .tTransform = pl_mat4_translate_xyz(1.0f, 1.0f, 0.3f)
    };

    plPlane tPlane0 = {
        .fOffset = 0.0f,
        .tDirection = {0.0f, 1.0f, 0.0f}
    };

    plPlane tPlane1 = {
        .fOffset = -2.0f,
        .tDirection = {0.0f, 1.0f, 0.0f}
    };

    plPlane tPlane2 = {
        .fOffset = -1.1f,
        .tDirection = {1.0f, 0.0f, 0.0f}
    };

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

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_TEST_WIN32_COLOR
#define PL_TEST_IMPLEMENTATION
#include "pl_test.h"
