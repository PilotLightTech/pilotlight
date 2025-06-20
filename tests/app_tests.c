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
#include "pl_json.h"
#include "pl_string.h"

// stable extensions
#include "pl_platform_ext.h"

// stable extensions
#include "pl_datetime_ext.h"
#include "pl_compress_ext.h"
#include "pl_pak_ext.h"
#include "pl_vfs_ext.h"
#include "pl_string_intern_ext.h"

// unstable extensions
#include "pl_collision_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plIOI*           gptIO        = NULL;
const plMemoryI*       gptMemory    = NULL;
const plCollisionI*    gptCollision = NULL;
const plDateTimeI*     gptDateTime  = NULL;
const plVfsI*          gptVfs       = NULL;
const plPakI*          gptPak       = NULL;
const plCompressI*     gptCompress  = NULL;
const plFileI*         gptFile      = NULL;
const plStringInternI* gptString    = NULL;

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
    plStringRepository* ptStringRepo;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] test declarations
//-----------------------------------------------------------------------------

void collision_only_tests_0(void*);
void datetime_tests_0(void*);
void vfs_tests_0(void*);
void file_tests_0(void*);
void string_intern_tests_0(void*);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_info
//-----------------------------------------------------------------------------

PL_EXPORT bool
pl_app_info(const plApiRegistryI* ptApiRegistry)
{
    // root object
    plJsonObject* ptRootJsonObject = pl_json_new_root_object("ROOT");
    pl_json_add_string_member(ptRootJsonObject, "first name", "John");
    pl_json_add_string_member(ptRootJsonObject, "last name", "Doe");
    pl_json_add_int_member(ptRootJsonObject, "age", 40);
    pl_json_add_bool_member(ptRootJsonObject, "tall", false);
    pl_json_add_bool_member(ptRootJsonObject, "hungry", true);
    int aScores[] = {100, 86, 46};
    pl_json_add_int_array(ptRootJsonObject, "scores", aScores, 3);

    const char* aPets[] = {"Riley", "Luna", "Chester"};
    pl_json_add_string_array(ptRootJsonObject, "pets", aPets, 3);

    // member object

    plJsonObject* ptBestFriend = pl_json_add_member(ptRootJsonObject, "best friend");
    pl_json_add_string_member(ptBestFriend, "first name", "John");
    pl_json_add_string_member(ptBestFriend, "last name", "Doe");
    pl_json_add_int_member(ptBestFriend, "age", 40);
    pl_json_add_bool_member(ptBestFriend, "tall", false);
    pl_json_add_bool_member(ptBestFriend, "hungry", true);
    pl_json_add_string_array(ptBestFriend, "pets", aPets, 3);
    pl_json_add_int_array(ptBestFriend, "scores", aScores, 3);

    // friend member object
    plJsonObject* ptFriends = pl_json_add_member_array(ptRootJsonObject, "friends", 2);

    plJsonObject* ptFriend0 = pl_json_member_by_index(ptFriends, 0);
    int aScores0[] = {88, 86, 100};
    pl_json_add_string_member(ptFriend0, "first name", "Jacob");
    pl_json_add_string_member(ptFriend0, "last name", "Smith");
    pl_json_add_int_member(ptFriend0, "age", 23);
    pl_json_add_bool_member(ptFriend0, "tall", true);
    pl_json_add_bool_member(ptFriend0, "hungry", false);
    pl_json_add_int_array(ptFriend0, "scores", aScores0, 3);

    plJsonObject* ptFriend1 = pl_json_member_by_index(ptFriends, 1);
    int aScores1[] = {80, 80, 100};
    pl_json_add_string_member(ptFriend1, "first name", "Chance");
    pl_json_add_string_member(ptFriend1, "last name", "Dale");
    pl_json_add_int_member(ptFriend1, "age", 48);
    pl_json_add_bool_member(ptFriend1, "tall", true);
    pl_json_add_bool_member(ptFriend1, "hungry", true);
    pl_json_add_int_array(ptFriend1, "scores", aScores1, 3);

    uint32_t uBufferSize = 0;
    pl_write_json(ptRootJsonObject, NULL, &uBufferSize);

    char* pucBuffer = (char*)malloc(uBufferSize);
    memset(pucBuffer, 0, uBufferSize);
    pl_write_json(ptRootJsonObject, pucBuffer, &uBufferSize);

    pl_unload_json(&ptRootJsonObject);

    FILE* ptDataFile = fopen("testing.json", "wb");
    fwrite(pucBuffer, 1, uBufferSize, ptDataFile);
    fclose(ptDataFile);
    free(pucBuffer);
    return true;
}

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
    ptExtensionRegistry->load("pl_compress_ext", "pl_load_compress_ext", "pl_unload_compress_ext", false);
    ptExtensionRegistry->load("pl_vfs_ext", "pl_load_vfs_ext", "pl_unload_vfs_ext", false);
    ptExtensionRegistry->load("pl_pak_ext", "pl_load_pak_ext", "pl_unload_pak_ext", false);
    ptExtensionRegistry->load("pl_string_intern_ext", "pl_load_string_intern_ext", "pl_unload_string_intern_ext", false);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);

    // retrieve the IO API required to use plIO for "talking" with runtime)
    gptIO        = pl_get_api_latest(ptApiRegistry, plIOI);
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptCollision = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptDateTime  = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptVfs       = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak       = pl_get_api_latest(ptApiRegistry, plPakI);
    gptCompress  = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptFile      = pl_get_api_latest(ptApiRegistry, plFileI);
    gptString    = pl_get_api_latest(ptApiRegistry, plStringInternI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    ptAppData->ptStringRepo = gptString->create_string_repository();

    // mount some directories
    gptVfs->mount_directory("/testing", "../out", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_memory("/ram", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_memory("/", PL_VFS_MOUNT_FLAGS_NONE);

    plPakFile* ptPak = NULL;
    gptPak->begin_packing("../out/testing.pak", 1, &ptPak);
    gptPak->add_from_disk(ptPak, "testing_compressed.json", "/testing/testing.json", true);
    gptPak->add_from_disk(ptPak, "testing_uncompressed.json", "/testing/testing.json", false);
    int iSpartan = 117;
    gptPak->add_from_memory(ptPak, "spartan.bin", (uint8_t*)&iSpartan, sizeof(int), false);
    gptPak->end_packing(&ptPak);

    gptVfs->mount_pak("/data", "/testing/testing.pak", PL_VFS_MOUNT_FLAGS_NONE);

    // copy file to memory file
    size_t szFileSize = gptVfs->get_file_size_str("/data/testing_compressed.json");
    plVfsFileHandle tHandle = gptVfs->open_file("/data/testing_compressed.json", PL_VFS_FILE_MODE_READ);
    uint8_t* puBuffer = malloc(szFileSize);
    memset(puBuffer, 0, szFileSize);
    gptVfs->read_file(tHandle, puBuffer, &szFileSize);
    gptVfs->close_file(tHandle);
    tHandle = gptVfs->open_file("/ram/testing_compressed.json", PL_VFS_FILE_MODE_WRITE);
    szFileSize = gptVfs->write_file(tHandle, puBuffer, szFileSize);
    tHandle = gptVfs->open_file("/testing_compressed.json", PL_VFS_FILE_MODE_WRITE);
    szFileSize = gptVfs->write_file(tHandle, puBuffer, szFileSize);
    free(puBuffer);

    // create test context
    plTestOptions tOptions = {
        .bPrintSuiteResults = true,
        .bPrintColor = true
    };
    plTestContext* ptTestContext = pl_create_test_context(tOptions);

    pl_test_register_test(collision_only_tests_0, ptAppData);
    pl_test_run_suite("pl_collision_ext.h");

    pl_test_register_test(datetime_tests_0, ptAppData);
    pl_test_run_suite("pl_datetime_ext.h");

    pl_test_register_test(vfs_tests_0, ptAppData);
    pl_test_run_suite("pl_vfs_ext.h");

    pl_test_register_test(file_tests_0, ptAppData);
    pl_test_run_suite("pl_platform_ext.h (plFileI)"); 

    pl_test_register_test(string_intern_tests_0, ptAppData);
    pl_test_run_suite("pl_string_intern.h");

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptString->destroy_string_repository(ptAppData->ptStringRepo);

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

void
vfs_tests_0(void* pAppData)
{

    plVfsFileHandle tHandle = gptVfs->open_file("/data/spartan.bin", PL_VFS_FILE_MODE_READ);
    int iSpartan = 0;
    size_t szIntSize = sizeof(int);
    gptVfs->read_file(tHandle, &iSpartan, &szIntSize);
    gptVfs->close_file(tHandle);


    const char* acFiles[] = {
        "/ram/testing_compressed.json",
        "/data/testing_compressed.json",
        "/data/testing_uncompressed.json",
        "/testing_compressed.json",
    };

    for(uint32_t i = 0; i < 4; i++)
    {

        size_t szFileSize = gptVfs->get_file_size_str(acFiles[i]);
        if(szFileSize == 0)
        {
            pl_test_expect_true(false, NULL);
            continue;
        }
        tHandle = gptVfs->open_file(acFiles[i], PL_VFS_FILE_MODE_READ);
        char* pucBuffer = PL_ALLOC(szFileSize + 1);
        memset(pucBuffer, 0, szFileSize + 1);
        gptVfs->read_file(tHandle, pucBuffer, &szFileSize);
        gptVfs->close_file(tHandle);

        plJsonObject* ptRootJsonObject = NULL;
        pl_load_json(pucBuffer, &ptRootJsonObject);

        // check root members
        {
            int aiScores[3] = {0};
            pl_json_int_array_member(ptRootJsonObject, "scores", aiScores, NULL);
            pl_test_expect_int_equal(aiScores[0], 100, NULL); 
            pl_test_expect_int_equal(aiScores[1], 86, NULL); 
            pl_test_expect_int_equal(aiScores[2], 46, NULL); 
        }
        {
            char acPet0[64] = {0};
            char acPet1[64] = {0};
            char acPet2[64] = {0};
            char* aacPets[3] = {acPet0, acPet1, acPet2};
            uint32_t auLengths[3] = {64, 64, 64};
            pl_json_string_array_member(ptRootJsonObject, "pets", aacPets, NULL, auLengths);
            pl_test_expect_string_equal(acPet0, "Riley", NULL);
            pl_test_expect_string_equal(acPet1, "Luna", NULL);
            pl_test_expect_string_equal(acPet2, "Chester", NULL);
        }

        char acFirstName[64] = {0};
        char acLastName[64] = {0};
        pl_test_expect_string_equal(pl_json_string_member(ptRootJsonObject, "first name", acFirstName, 64), "John", NULL);
        pl_test_expect_string_equal(pl_json_string_member(ptRootJsonObject, "last name", acLastName, 64), "Doe", NULL);
        pl_test_expect_int_equal(pl_json_int_member(ptRootJsonObject, "age", 0), 40, NULL);
        pl_test_expect_false(pl_json_bool_member(ptRootJsonObject, "tall", false), NULL);
        pl_test_expect_true(pl_json_bool_member(ptRootJsonObject, "hungry", false), NULL);

        // check child members
        plJsonObject* ptBestFriend = pl_json_member(ptRootJsonObject, "best friend");
        {
            int aiScores[3] = {0};
            pl_json_int_array_member(ptBestFriend, "scores", aiScores, NULL);
            pl_test_expect_int_equal(aiScores[0], 100, NULL); 
            pl_test_expect_int_equal(aiScores[1], 86, NULL); 
            pl_test_expect_int_equal(aiScores[2], 46, NULL); 
        }
        {
            char acPet0[64] = {0};
            char acPet1[64] = {0};
            char acPet2[64] = {0};
            char* aacPets[3] = {acPet0, acPet1, acPet2};
            uint32_t auLengths[3] = {64, 64, 64};
            pl_json_string_array_member(ptBestFriend, "pets", aacPets, NULL, auLengths);
            pl_test_expect_string_equal(acPet0, "Riley", NULL);
            pl_test_expect_string_equal(acPet1, "Luna", NULL);
            pl_test_expect_string_equal(acPet2, "Chester", NULL);
        }

        pl_test_expect_string_equal(pl_json_string_member(ptBestFriend, "first name", acFirstName, 64), "John", NULL);
        pl_test_expect_string_equal(pl_json_string_member(ptBestFriend, "last name", acLastName, 64), "Doe", NULL);
        pl_test_expect_int_equal(pl_json_int_member(ptBestFriend, "age", 0), 40, NULL);
        pl_test_expect_false(pl_json_bool_member(ptBestFriend, "tall", false), NULL);
        pl_test_expect_true(pl_json_bool_member(ptBestFriend, "hungry", false), NULL);

        uint32_t uFriendCount = 0;
        plJsonObject* ptFriends = pl_json_array_member(ptRootJsonObject, "friends", &uFriendCount);

        plJsonObject* ptFriend0 = pl_json_member_by_index(ptFriends, 0);
        plJsonObject* ptFriend1 = pl_json_member_by_index(ptFriends, 1);
        {
            int aiScores[3] = {0};
            pl_json_int_array_member(ptFriend0, "scores", aiScores, NULL);
            pl_test_expect_int_equal(aiScores[0], 88, NULL); 
            pl_test_expect_int_equal(aiScores[1], 86, NULL); 
            pl_test_expect_int_equal(aiScores[2], 100, NULL); 
        }

        {
            int aiScores[3] = {0};
            pl_json_int_array_member(ptFriend1, "scores", aiScores, NULL);
            pl_test_expect_int_equal(aiScores[0], 80, NULL); 
            pl_test_expect_int_equal(aiScores[1], 80, NULL); 
            pl_test_expect_int_equal(aiScores[2], 100, NULL); 
        }

        pl_test_expect_string_equal(pl_json_string_member(ptFriend0, "first name", acFirstName, 64), "Jacob", NULL);
        pl_test_expect_string_equal(pl_json_string_member(ptFriend0, "last name", acLastName, 64), "Smith", NULL);
        pl_test_expect_int_equal(pl_json_int_member(ptFriend0, "age", 0), 23, NULL);
        pl_test_expect_true(pl_json_bool_member(ptFriend0, "tall", false), NULL);
        pl_test_expect_false(pl_json_bool_member(ptFriend0, "hungry", false), NULL);  

        pl_test_expect_string_equal(pl_json_string_member(ptFriend1, "first name", acFirstName, 64), "Chance", NULL);
        pl_test_expect_string_equal(pl_json_string_member(ptFriend1, "last name", acLastName, 64), "Dale", NULL);
        pl_test_expect_int_equal(pl_json_int_member(ptFriend1, "age", 0), 48, NULL);
        pl_test_expect_true(pl_json_bool_member(ptFriend1, "tall", false), NULL);
        pl_test_expect_true(pl_json_bool_member(ptFriend1, "hungry", false), NULL);

        pl_unload_json(&ptRootJsonObject);

        PL_FREE(pucBuffer);
    }
}

void
string_intern_tests_0(void* pAppData)
{
    plAppData* ptAppData = (plAppData*)pAppData;

    const char* pcName0 = gptString->intern(ptAppData->ptStringRepo, "Jonathan");
    const char* pcName1 = gptString->intern(ptAppData->ptStringRepo, "Jonathan");
    const char* pcName2 = gptString->intern(ptAppData->ptStringRepo, "Jonathan");

    pl_test_expect_true(pcName0 == pcName1, NULL);
    pl_test_expect_true(pcName1 == pcName2, NULL);

    gptString->remove(ptAppData->ptStringRepo, pcName0);
    gptString->remove(ptAppData->ptStringRepo, pcName1);
    gptString->remove(ptAppData->ptStringRepo, pcName2);
}

void
file_tests_0(void* pAppData)
{

    const char** sbcFiles = NULL;
    pl_sb_push(sbcFiles, "pl_ds.h");
    pl_sb_push(sbcFiles, "pl_json.h");
    pl_sb_push(sbcFiles, "pl_log.h");
    pl_sb_push(sbcFiles, "pl_math.h");
    pl_sb_push(sbcFiles, "pl_memory.h");
    pl_sb_push(sbcFiles, "pl_profile.h");
    pl_sb_push(sbcFiles, "pl_stl.h");
    pl_sb_push(sbcFiles, "pl_string.h");
    pl_sb_push(sbcFiles, "pl_test.h");

    plDirectoryInfo tInfo = {0};
    gptFile->get_directory_info("../libs", &tInfo);

    pl_test_expect_uint32_equal(tInfo.uEntryCount, pl_sb_size(sbcFiles), NULL); 

    bool bFindMath = false;
    uint32_t uMathIndex = 0;
    for(uint32_t i = 0; i < tInfo.uEntryCount; i++)
    {

        if(pl_str_equal(tInfo.sbtEntries[i].acName, "pl_math.h"))
        {
            bFindMath = true;
            uMathIndex = i;
            break;
        }
    }
    pl_test_expect_true(bFindMath, NULL);
    pl_test_expect_true(tInfo.sbtEntries[uMathIndex].tType == PL_DIRECTORY_ENTRY_TYPE_FILE, NULL);

    gptFile->cleanup_directory_info(&tInfo);
    pl_sb_free(sbcFiles);

    pl_test_expect_true(gptFile->directory_exists("../libs"), NULL);
    pl_test_expect_false(gptFile->directory_exists("../libs-offset"), NULL);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_TEST_WIN32_COLOR
#define PL_TEST_IMPLEMENTATION
#include "pl_test.h"

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
