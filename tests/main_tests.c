#include "pl_ds_tests.h"
#include "pl_json_tests.h"

int main()
{
    plTestContext* ptTestContext = pl_create_test_context();
    
    // data structure tests
    pl_test_register_test(hashmap_test_0, NULL);

    // json tests
    pl_test_register_test(json_test_0, NULL);

    if(!pl_test_run())
    {
        exit(1);
    }

    return 0;
}

#define PL_TEST_IMPLEMENTATION
#include "pl_test.h"

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"