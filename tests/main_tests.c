#include "pl_ds_tests.h"
#include "pl_json_tests.h"
#include "pl_memory_tests.h"
#include "pl_string_tests.h"

int main()
{
    // create test context
    plTestOptions tOptions = {
        .bPrintSuiteResults = true,
        .bPrintColor = true
    };
    plTestContext* ptTestContext = pl_create_test_context(tOptions);

    // pl_json.h tests
    pl_json_tests(NULL);
    pl_test_run_suite("pl_json.h");

    // pl_memory.h tests
    pl_memory_tests(NULL);
    pl_test_run_suite("pl_memory.h");

    // pl_ds.h tests
    pl_ds_tests(NULL);
    pl_test_run_suite("pl_ds.h");

    // pl_string.h tests
    pl_string_tests(NULL);
    pl_test_run_suite("pl_string.h");

    bool bResult = pl_test_finish();

    if(!bResult)
    {
        exit(1);
    }
    return 0;
}

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"

#define PL_MEMORY_IMPLEMENTATION
#include "pl_memory.h"

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"

#define PL_TEST_WIN32_COLOR
#define PL_TEST_IMPLEMENTATION
#include "pl_test.h"