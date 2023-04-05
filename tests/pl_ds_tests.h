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
        plHashMap tHashMap = {0};

        int* sbiValues = NULL;
        pl_sb_push(sbiValues, 0);
        pl_sb_push(sbiValues, 69);
        pl_hm_insert(&tHashMap, pl_hm_hash_str("Dirty Number"), pl_sb_size(sbiValues) - 1);
        pl_sb_push(sbiValues, 117);
        pl_hm_insert(&tHashMap, pl_hm_hash_str("Spartan Number"), pl_sb_size(sbiValues) - 1);

        for(uint32_t i = 0; i < 3000; i++)
        {
            pl_sb_push(sbiValues, i);
            pl_hm_insert(&tHashMap, pl_hm_hash("Spartan Number2", strlen("Spartan Number2"), i), pl_sb_size(sbiValues) - 1);
        }

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Dirty Number"))], 69, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

        pl_hm_remove(&tHashMap, pl_hm_hash_str("Dirty Number"));

        uint64_t ulFreeIndex = pl_hm_get_free_index(&tHashMap);
        if(ulFreeIndex == UINT64_MAX)
        {
            pl_sb_add(sbiValues);
            ulFreeIndex = pl_sb_size(sbiValues) - 1;
        }
        sbiValues[ulFreeIndex] = 666999;
        pl_hm_insert(&tHashMap, pl_hm_hash_str("Extra dirty number"), ulFreeIndex);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Extra dirty number"))], 666999, NULL);

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Extra dirty number"))], 666999, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

        pl_hm_free(&tHashMap);
        pl_sb_free(sbiValues);
    }

    // hashmap 1
    {
        plHashMap tHashMap = {0};

        pl_hm_insert(&tHashMap, pl_hm_hash_str("Dirty Number"), 69);
        pl_hm_insert(&tHashMap, pl_hm_hash_str("Spartan Number"), 117);

        pl_test_expect_int_equal((int)pl_hm_lookup(&tHashMap, pl_hm_hash_str("Dirty Number")), 69, NULL);
        pl_test_expect_int_equal((int)pl_hm_lookup(&tHashMap, pl_hm_hash_str("Spartan Number")), 117, NULL);

        pl_hm_free(&tHashMap);
    }

    // hashmap 2
    {
        plHashMap tHashMap = {0};

        int* sbiValues = NULL;
        pl_sb_push(sbiValues, 0);
        pl_sb_push(sbiValues, 69);
        pl_hm_insert_str(&tHashMap, "Dirty Number", pl_sb_size(sbiValues) - 1);
        pl_sb_push(sbiValues, 117);
        pl_hm_insert_str(&tHashMap, "Spartan Number", pl_sb_size(sbiValues) - 1);

        for(uint32_t i = 0; i < 79; i++)
        {
            pl_sb_push(sbiValues, 118);
            pl_hm_insert_str(&tHashMap, "Spartan Number2", pl_sb_size(sbiValues) - 1);
        }

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Dirty Number")], 69, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Spartan Number")], 117, NULL);

        pl_hm_remove_str(&tHashMap, "Dirty Number");

        uint64_t ulFreeIndex = pl_hm_get_free_index(&tHashMap);
        if(ulFreeIndex == UINT64_MAX)
        {
            pl_sb_add(sbiValues);
            ulFreeIndex = pl_sb_size(sbiValues) - 1;
        }
        sbiValues[ulFreeIndex] = 666999;
        pl_hm_insert_str(&tHashMap, "Extra dirty number", ulFreeIndex);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Extra dirty number")], 666999, NULL);

        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Extra dirty number")], 666999, NULL);
        pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Spartan Number")], 117, NULL);

        pl_hm_free(&tHashMap);
        pl_sb_free(sbiValues);
    }
}