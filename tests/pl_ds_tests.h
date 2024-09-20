#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pl_test.h"

#include <stdint.h>
#include "pl_ds.h"

static void
hashmap_test_0(void* pData)
{
    // hashmap 0
    {
        plHashMap* ptHashMap = NULL;

        int* sbiValues = NULL;
        pl_sb_push(sbiValues, 0);
        pl_sb_push(sbiValues, 69);
        pl_hm_insert(ptHashMap, pl_hm_hash_str("Dirty Number"), pl_sb_size(sbiValues) - 1);
        pl_sb_push(sbiValues, 117);
        pl_hm_insert(ptHashMap, pl_hm_hash_str("Spartan Number"), pl_sb_size(sbiValues) - 1);

        for(uint32_t i = 0; i < 3000; i++)
        {
            pl_sb_push(sbiValues, i);
            pl_hm_insert(ptHashMap, pl_hm_hash("Spartan Number2", strlen("Spartan Number2"), i), pl_sb_size(sbiValues) - 1);
        }

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(ptHashMap, pl_hm_hash_str("Dirty Number"))], 69, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(ptHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

        pl_hm_remove(ptHashMap, pl_hm_hash_str("Dirty Number"));

        uint64_t ulFreeIndex = pl_hm_get_free_index(ptHashMap);
        if(ulFreeIndex == UINT64_MAX)
        {
            pl_sb_add(sbiValues);
            ulFreeIndex = pl_sb_size(sbiValues) - 1;
        }
        sbiValues[ulFreeIndex] = 666999;
        pl_hm_insert(ptHashMap, pl_hm_hash_str("Extra dirty number"), ulFreeIndex);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(ptHashMap, pl_hm_hash_str("Extra dirty number"))], 666999, NULL);

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(ptHashMap, pl_hm_hash_str("Extra dirty number"))], 666999, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(ptHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

        pl_hm_free(ptHashMap);
        pl_sb_free(sbiValues);
    }

    // hashmap 1
    {
        plHashMap* ptHashMap = NULL;

        pl_hm_insert(ptHashMap, pl_hm_hash_str("Dirty Number"), 69);
        pl_hm_insert(ptHashMap, pl_hm_hash_str("Spartan Number"), 117);

        pl_test_expect_int_equal((int)pl_hm_lookup(ptHashMap, pl_hm_hash_str("Dirty Number")), 69, NULL);
        pl_test_expect_int_equal((int)pl_hm_lookup(ptHashMap, pl_hm_hash_str("Spartan Number")), 117, NULL);

        pl_hm_free(ptHashMap);
    }

    // hashmap 2
    {
        plHashMap* ptHashMap = NULL;

        int* sbiValues = NULL;
        pl_sb_push(sbiValues, 0);
        pl_sb_push(sbiValues, 69);
        pl_hm_insert_str(ptHashMap, "Dirty Number", pl_sb_size(sbiValues) - 1);
        pl_sb_push(sbiValues, 117);
        pl_hm_insert_str(ptHashMap, "Spartan Number", pl_sb_size(sbiValues) - 1);

        for(uint32_t i = 0; i < 79; i++)
        {
            pl_sb_push(sbiValues, 118);
            pl_hm_insert_str(ptHashMap, "Spartan Number2", pl_sb_size(sbiValues) - 1);
        }

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(ptHashMap, "Dirty Number")], 69, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(ptHashMap, "Spartan Number")], 117, NULL);

        pl_hm_remove_str(ptHashMap, "Dirty Number");

        uint64_t ulFreeIndex = pl_hm_get_free_index(ptHashMap);
        if(ulFreeIndex == UINT64_MAX)
        {
            pl_sb_add(sbiValues);
            ulFreeIndex = pl_sb_size(sbiValues) - 1;
        }
        sbiValues[ulFreeIndex] = 666999;
        pl_hm_insert_str(ptHashMap, "Extra dirty number", ulFreeIndex);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(ptHashMap, "Extra dirty number")], 666999, NULL);

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(ptHashMap, "Extra dirty number")], 666999, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(ptHashMap, "Spartan Number")], 117, NULL);

        pl_hm_free(ptHashMap);
        pl_sb_free(sbiValues);
    }
}