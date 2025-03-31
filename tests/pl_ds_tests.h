#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pl_test.h"

#include <stdint.h>
#include "pl_ds.h"

void
hashmap_test_0(void* pData)
{
    plHashMap tHashMap = {0};

    int* sbiValues = NULL;
    pl_sb_push(sbiValues, 0);
    pl_sb_push(sbiValues, 907);
    pl_hm_insert(&tHashMap, pl_hm_hash_str("Dirty Number"), pl_sb_size(sbiValues) - 1);
    pl_sb_push(sbiValues, 117);
    pl_hm_insert(&tHashMap, pl_hm_hash_str("Spartan Number"), pl_sb_size(sbiValues) - 1);

    for(uint32_t i = 0; i < 3000; i++)
    {
        pl_sb_push(sbiValues, i);
        pl_hm_insert(&tHashMap, pl_hm_hash("Spartan Number2", strlen("Spartan Number2"), i), pl_sb_size(sbiValues) - 1);
    }

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Dirty Number"))], 907, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

    pl_hm_remove(&tHashMap, pl_hm_hash_str("Dirty Number"));

    uint64_t ulFreeIndex = pl_hm_get_free_index(&tHashMap);
    if(ulFreeIndex == PL_DS_HASH_INVALID)
    {
        pl_sb_add(sbiValues);
        ulFreeIndex = pl_sb_size(sbiValues) - 1;
    }
    sbiValues[ulFreeIndex] = 123689;
    pl_hm_insert(&tHashMap, pl_hm_hash_str("Extra dirty number"), ulFreeIndex);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Extra dirty number"))], 123689, NULL);

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Extra dirty number"))], 123689, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup(&tHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

    pl_hm_free(&tHashMap);
    pl_sb_free(sbiValues);
}

void
hashmap_test_1(void* pData)
{
    plHashMap tHashMap = {0};

    pl_hm_insert(&tHashMap, pl_hm_hash_str("Dirty Number"), 945);
    pl_hm_insert(&tHashMap, pl_hm_hash_str("Spartan Number"), 117);

    pl_test_expect_int_equal((int)pl_hm_lookup(&tHashMap, pl_hm_hash_str("Dirty Number")), 945, NULL);
    pl_test_expect_int_equal((int)pl_hm_lookup(&tHashMap, pl_hm_hash_str("Spartan Number")), 117, NULL);

    pl_hm_free(&tHashMap);
}

void
hashmap_test_2(void* pData)
{
    plHashMap tHashMap = {0};

    int* sbiValues = NULL;
    pl_sb_push(sbiValues, 0);
    pl_sb_push(sbiValues, 945);
    pl_hm_insert_str(&tHashMap, "Dirty Number", pl_sb_size(sbiValues) - 1);
    pl_sb_push(sbiValues, 117);
    pl_hm_insert_str(&tHashMap, "Spartan Number", pl_sb_size(sbiValues) - 1);

    for(uint32_t i = 0; i < 79; i++)
    {
        pl_sb_push(sbiValues, 118);
        pl_hm_insert_str(&tHashMap, "Spartan Number2", pl_sb_size(sbiValues) - 1);
    }

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Dirty Number")], 945, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Spartan Number")], 117, NULL);

    pl_hm_remove_str(&tHashMap, "Dirty Number");

    uint64_t ulFreeIndex = pl_hm_get_free_index(&tHashMap);
    if(ulFreeIndex == PL_DS_HASH_INVALID)
    {
        pl_sb_add(sbiValues);
        ulFreeIndex = pl_sb_size(sbiValues) - 1;
    }
    sbiValues[ulFreeIndex] = 945945;
    pl_hm_insert_str(&tHashMap, "Extra dirty number", ulFreeIndex);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Extra dirty number")], 945945, NULL);

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Extra dirty number")], 945945, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str(&tHashMap, "Spartan Number")], 117, NULL);

    pl_hm_free(&tHashMap);
    pl_sb_free(sbiValues);
}

void
hashmap_test_3(void* pData)
{
    plHashMap tHashMap = {0};

    // test empty map
    pl_test_expect_uint32_equal(pl_hm_size(&tHashMap), 0, NULL);

    pl_hm_insert(&tHashMap, 417, 117);
    pl_test_expect_uint32_equal(pl_hm_size(&tHashMap), 1, NULL);

    // test removal
    pl_hm_remove(&tHashMap, 417);
    pl_test_expect_uint32_equal(pl_hm_size(&tHashMap), 0, NULL);

    // test collisions
    const uint32_t uMask = tHashMap._uBucketCapacity - 1; // assumes bucket count is power of 2
    uint32_t uBucketIndex = 417 & uMask;
    uint32_t uBucketIndex0 = (417 + tHashMap._uBucketCapacity) & uMask;

    pl_hm_insert(&tHashMap, 417, 117);
    pl_hm_insert(&tHashMap, 417 + tHashMap._uBucketCapacity, 118);
    pl_hm_insert(&tHashMap, 417 + tHashMap._uBucketCapacity * 2, 119);

    pl_test_expect_uint64_equal(pl_hm_lookup(&tHashMap, 417), 117, NULL);
    pl_test_expect_uint64_equal(pl_hm_lookup(&tHashMap, 417 + tHashMap._uBucketCapacity), 118, NULL);
    pl_test_expect_uint64_equal(pl_hm_lookup(&tHashMap, 417 + 2 * tHashMap._uBucketCapacity), 119, NULL);

    pl_hm_remove(&tHashMap, 417 + tHashMap._uBucketCapacity * 2);
    pl_test_expect_uint64_equal(pl_hm_lookup(&tHashMap, 417 + 2 * tHashMap._uBucketCapacity), UINT64_MAX, NULL);
    pl_hm_remove(&tHashMap, 417);
    pl_test_expect_uint64_equal(pl_hm_lookup(&tHashMap, 417), UINT64_MAX, NULL);
    pl_hm_remove(&tHashMap, 417 + tHashMap._uBucketCapacity);
    pl_test_expect_uint64_equal(pl_hm_lookup(&tHashMap, 417 + tHashMap._uBucketCapacity), UINT64_MAX, NULL);
    
    pl_test_expect_uint32_equal(pl_hm_size(&tHashMap), 0, NULL);
}

void
shashmap_test_0(void* pData)
{
    uint64_t auKeys[1024] = {0};
    uint32_t auValues[1024] = {0};
    plHashMapStatic32 tHashMap = {
        .auKeys = auKeys,
        .auValueBucket = auValues,
        .uBucketCount = 1024
    };

    pl_hms_clear32(&tHashMap);

    int* sbiValues = NULL;
    pl_sb_push(sbiValues, 0);
    pl_sb_push(sbiValues, 907);
    pl_hms_set_str32(&tHashMap, "Dirty Number", pl_sb_size(sbiValues) - 1);
    pl_sb_push(sbiValues, 117);
    pl_hms_set_str32(&tHashMap, "Spartan Number", pl_sb_size(sbiValues) - 1);

    for(uint32_t i = 0; i < 256; i++)
    {
        pl_sb_push(sbiValues, i);
        pl_hms_set32(&tHashMap, pl_hm_hash("Spartan Number2", strlen("Spartan Number2"), i), pl_sb_size(sbiValues) - 1);
    }

    pl_test_expect_int_equal(sbiValues[pl_hms_get_str32(&tHashMap, "Dirty Number")], 907, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hms_get_str32(&tHashMap, "Spartan Number")], 117, NULL);
}

void
hashmap32_test_0(void* pData)
{
    plHashMap32 tHashMap = {0};

    int* sbiValues = NULL;
    pl_sb_push(sbiValues, 0);
    pl_sb_push(sbiValues, 907);
    pl_hm_insert32(&tHashMap, pl_hm_hash_str("Dirty Number"), pl_sb_size(sbiValues) - 1);
    pl_sb_push(sbiValues, 117);
    pl_hm_insert32(&tHashMap, pl_hm_hash_str("Spartan Number"), pl_sb_size(sbiValues) - 1);

    for(uint32_t i = 0; i < 3000; i++)
    {
        pl_sb_push(sbiValues, i);
        pl_hm_insert32(&tHashMap, pl_hm_hash("Spartan Number2", strlen("Spartan Number2"), i), pl_sb_size(sbiValues) - 1);
    }

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup32(&tHashMap, pl_hm_hash_str("Dirty Number"))], 907, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup32(&tHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

    pl_hm_remove32(&tHashMap, pl_hm_hash_str("Dirty Number"));

    uint32_t ulFreeIndex = pl_hm_get_free_index32(&tHashMap);
    if(ulFreeIndex == PL_DS_HASH32_INVALID)
    {
        pl_sb_add(sbiValues);
        ulFreeIndex = pl_sb_size(sbiValues) - 1;
    }
    sbiValues[ulFreeIndex] = 123689;
    pl_hm_insert32(&tHashMap, pl_hm_hash_str("Extra dirty number"), ulFreeIndex);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup32(&tHashMap, pl_hm_hash_str("Extra dirty number"))], 123689, NULL);

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup32(&tHashMap, pl_hm_hash_str("Extra dirty number"))], 123689, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup32(&tHashMap, pl_hm_hash_str("Spartan Number"))], 117, NULL);

    pl_hm_free32(&tHashMap);
    pl_sb_free(sbiValues);
}

void
hashmap32_test_1(void* pData)
{
    plHashMap32 tHashMap = {0};

    pl_hm_insert32(&tHashMap, pl_hm_hash_str("Dirty Number"), 945);
    pl_hm_insert32(&tHashMap, pl_hm_hash_str("Spartan Number"), 117);

    pl_test_expect_int_equal((int)pl_hm_lookup32(&tHashMap, pl_hm_hash_str("Dirty Number")), 945, NULL);
    pl_test_expect_int_equal((int)pl_hm_lookup32(&tHashMap, pl_hm_hash_str("Spartan Number")), 117, NULL);

    pl_hm_free32(&tHashMap);
}

void
hashmap32_test_2(void* pData)
{
    plHashMap32 tHashMap = {0};

    int* sbiValues = NULL;
    pl_sb_push(sbiValues, 0);
    pl_sb_push(sbiValues, 945);
    pl_hm_insert_str32(&tHashMap, "Dirty Number", pl_sb_size(sbiValues) - 1);
    pl_sb_push(sbiValues, 117);
    pl_hm_insert_str32(&tHashMap, "Spartan Number", pl_sb_size(sbiValues) - 1);

    for(uint32_t i = 0; i < 79; i++)
    {
        pl_sb_push(sbiValues, 118);
        pl_hm_insert_str32(&tHashMap, "Spartan Number2", pl_sb_size(sbiValues) - 1);
    }

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str32(&tHashMap, "Dirty Number")], 945, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str32(&tHashMap, "Spartan Number")], 117, NULL);

    pl_hm_remove_str32(&tHashMap, "Dirty Number");

    uint32_t ulFreeIndex = pl_hm_get_free_index32(&tHashMap);
    if(ulFreeIndex == PL_DS_HASH32_INVALID)
    {
        pl_sb_add(sbiValues);
        ulFreeIndex = pl_sb_size(sbiValues) - 1;
    }
    sbiValues[ulFreeIndex] = 945945;
    pl_hm_insert_str32(&tHashMap, "Extra dirty number", ulFreeIndex);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str32(&tHashMap, "Extra dirty number")], 945945, NULL);

    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str32(&tHashMap, "Extra dirty number")], 945945, NULL);
    pl_test_expect_int_equal(sbiValues[pl_hm_lookup_str32(&tHashMap, "Spartan Number")], 117, NULL);

    pl_hm_free32(&tHashMap);
    pl_sb_free(sbiValues);
}

void
hashmap32_test_3(void* pData)
{
    plHashMap32 tHashMap = {0};

    // test empty map
    pl_test_expect_uint32_equal(pl_hm_size32(&tHashMap), 0, NULL);

    pl_hm_insert32(&tHashMap, 417, 117);
    pl_test_expect_uint32_equal(pl_hm_size32(&tHashMap), 1, NULL);

    // test removal
    pl_hm_remove32(&tHashMap, 417);
    pl_test_expect_uint32_equal(pl_hm_size32(&tHashMap), 0, NULL);

    // test collisions
    const uint32_t uMask = tHashMap._uBucketCapacity - 1; // assumes bucket count is power of 2
    uint32_t uBucketIndex = 417 & uMask;
    uint32_t uBucketIndex0 = (417 + tHashMap._uBucketCapacity) & uMask;

    pl_hm_insert32(&tHashMap, 417, 117);
    pl_hm_insert32(&tHashMap, 417 + tHashMap._uBucketCapacity, 118);
    pl_hm_insert32(&tHashMap, 417 + tHashMap._uBucketCapacity * 2, 119);

    pl_test_expect_uint64_equal(pl_hm_lookup32(&tHashMap, 417), 117, NULL);
    pl_test_expect_uint64_equal(pl_hm_lookup32(&tHashMap, 417 + tHashMap._uBucketCapacity), 118, NULL);
    pl_test_expect_uint64_equal(pl_hm_lookup32(&tHashMap, 417 + 2 * tHashMap._uBucketCapacity), 119, NULL);

    pl_hm_remove32(&tHashMap, 417 + tHashMap._uBucketCapacity * 2);
    pl_test_expect_uint64_equal(pl_hm_lookup32(&tHashMap, 417 + 2 * tHashMap._uBucketCapacity), UINT32_MAX, NULL);
    pl_hm_remove32(&tHashMap, 417);
    pl_test_expect_uint64_equal(pl_hm_lookup32(&tHashMap, 417), UINT32_MAX, NULL);
    pl_hm_remove32(&tHashMap, 417 + tHashMap._uBucketCapacity);
    pl_test_expect_uint64_equal(pl_hm_lookup32(&tHashMap, 417 + tHashMap._uBucketCapacity), UINT32_MAX, NULL);
    
    pl_test_expect_uint32_equal(pl_hm_size32(&tHashMap), 0, NULL);
}

void
pl_ds_tests(void* pData)
{
    pl_test_register_test(hashmap_test_0, NULL);
    pl_test_register_test(hashmap_test_1, NULL);
    pl_test_register_test(hashmap_test_2, NULL);
    pl_test_register_test(hashmap_test_3, NULL);

    pl_test_register_test(shashmap_test_0, NULL);

    pl_test_register_test(hashmap32_test_0, NULL);
    pl_test_register_test(hashmap32_test_1, NULL);
    pl_test_register_test(hashmap32_test_2, NULL);
    pl_test_register_test(hashmap32_test_3, NULL);
}