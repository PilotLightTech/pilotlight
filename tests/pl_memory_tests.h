#include "pl_test.h"
#include "pl_memory.h"
#include <string.h> // memset

typedef struct _plTestStruct
{
    const char* pcName;
    int         iAge;
} plTestStruct;

void
memory_test_aligned_alloc(void* pData)
{
    int* iBuffer0 = pl_aligned_alloc(0, sizeof(int));
    pl_test_expect_uint64_equal(((uint64_t)iBuffer0) % 4, 0, NULL);
    pl_aligned_free(iBuffer0);

    int* iBuffer1 = pl_aligned_alloc(16, sizeof(int));
    pl_test_expect_uint64_equal(((uint64_t)iBuffer1) % 16, 0, NULL);
    pl_aligned_free(iBuffer1);
}

void
memory_test_pool_allocator_0(void* pData)
{
    // here we are testing standard usage

    plPoolAllocator tAllocator = {0};

    // let library tell use required buffer size
    size_t szRequiredBufferSize = 0;
    pl_pool_allocator_init(&tAllocator, 5, sizeof(int), 0, &szRequiredBufferSize, NULL);

    int* iBuffer = malloc(szRequiredBufferSize);
    memset(iBuffer, 0, szRequiredBufferSize);
    pl_pool_allocator_init(&tAllocator, 5, sizeof(int), 0, &szRequiredBufferSize, iBuffer);

    int* iItem0 = pl_pool_allocator_alloc(&tAllocator);
    int* iItem1 = pl_pool_allocator_alloc(&tAllocator);
    int* iItem2 = pl_pool_allocator_alloc(&tAllocator);
    int* iItem3 = pl_pool_allocator_alloc(&tAllocator);
    int* iItem4 = pl_pool_allocator_alloc(&tAllocator);
    int* iItem5 = pl_pool_allocator_alloc(&tAllocator);

    *iItem0 = 0;
    *iItem1 = 1;
    *iItem2 = 2;
    *iItem3 = 3;
    *iItem4 = 4;
    
    pl_test_expect_int_equal(*iItem0, 0, NULL);
    pl_test_expect_int_equal(*iItem1, 1, NULL);
    pl_test_expect_int_equal(*iItem2, 2, NULL);
    pl_test_expect_int_equal(*iItem3, 3, NULL);
    pl_test_expect_int_equal(*iItem4, 4, NULL);
    pl_test_expect_uint64_equal(((uint64_t)iItem5), 0, NULL);
    free(iBuffer);
}

void
memory_test_pool_allocator_1(void* pData)
{
    // here we are testing usage with stack allocated memory

    plPoolAllocator tAllocator = {0};

    plTestStruct atData[5] = {0};

    // let library tell use required buffer size
    size_t szRequiredBufferSize = sizeof(atData);
    size_t szItemCount = pl_pool_allocator_init(&tAllocator, 0, sizeof(plTestStruct), 0, &szRequiredBufferSize, atData);
    pl_pool_allocator_init(&tAllocator, szItemCount, sizeof(plTestStruct), 0, &szRequiredBufferSize, atData);

    pl_test_expect_int_equal((int)szItemCount, 4, NULL);

    plTestStruct* ptData0 = pl_pool_allocator_alloc(&tAllocator);
    plTestStruct* ptData1 = pl_pool_allocator_alloc(&tAllocator);
    plTestStruct* ptData2 = pl_pool_allocator_alloc(&tAllocator);
    plTestStruct* ptData3 = pl_pool_allocator_alloc(&tAllocator);
    plTestStruct* ptData4 = pl_pool_allocator_alloc(&tAllocator);

    ptData0->iAge = 0;
    ptData1->iAge = 1;
    ptData2->iAge = 2;
    ptData3->iAge = 3;

    ptData0->pcName = "John";
    ptData1->pcName = "James";
    ptData2->pcName = "Mark";
    ptData3->pcName = "Matt";

    pl_test_expect_int_equal(ptData0->iAge, 0, NULL);
    pl_test_expect_int_equal(ptData1->iAge, 1, NULL);
    pl_test_expect_int_equal(ptData2->iAge, 2, NULL);
    pl_test_expect_int_equal(ptData3->iAge, 3, NULL);

    pl_test_expect_string_equal(ptData0->pcName, "John", NULL);
    pl_test_expect_string_equal(ptData1->pcName, "James", NULL);
    pl_test_expect_string_equal(ptData2->pcName, "Mark", NULL);
    pl_test_expect_string_equal(ptData3->pcName, "Matt", NULL);
    pl_test_expect_uint64_equal(((uint64_t)ptData4), 0, NULL);

    pl_pool_allocator_free(&tAllocator, ptData1);
    plTestStruct* ptData5 = pl_pool_allocator_alloc(&tAllocator);
    pl_test_expect_uint64_not_equal(((uint64_t)ptData5), 0, NULL);
}

void
memory_test_stack_allocator_0(void* pData)
{
    char acBuffer[1024] = {0};
    plStackAllocator tAllocator = {0};
    pl_stack_allocator_init(&tAllocator, 1024, acBuffer);

    plTestStruct* ptData0 = pl_stack_allocator_alloc(&tAllocator, sizeof(plTestStruct));
    ptData0->pcName = "John";
    ptData0->iAge = 31;

    plStackAllocatorMarker tMarker0 = pl_stack_allocator_marker(&tAllocator);

    plTestStruct* ptData1 = pl_stack_allocator_alloc(&tAllocator, sizeof(plTestStruct));
    ptData0->pcName = "Rachael";
    ptData0->iAge = 32;

    pl_stack_allocator_free_to_marker(&tAllocator, tMarker0);

    plTestStruct* ptData2 = pl_stack_allocator_alloc(&tAllocator, sizeof(plTestStruct));
    ptData0->pcName = "Charlie";
    ptData0->iAge = 4;

    pl_test_expect_uint64_equal((uint64_t)ptData1, (uint64_t)ptData2, NULL);
}

void
memory_test_temp_allocator_0(void* pData)
{
    plTempAllocator tAllocator = {0};

    char* pcBuffer0 = pl_temp_allocator_alloc(&tAllocator, 256);
    char* pcBuffer1 = pl_temp_allocator_alloc(&tAllocator, 256);
    char* pcBuffer2 = pl_temp_allocator_alloc(&tAllocator, 256);
    char* pcBuffer3 = pl_temp_allocator_alloc(&tAllocator, 256);
    char* pcBuffer4 = pl_temp_allocator_alloc(&tAllocator, 256);
    
    
    pl_temp_allocator_free(&tAllocator);
}

void
pl_memory_tests(void* pData)
{
    pl_test_register_test(memory_test_aligned_alloc, NULL);
    pl_test_register_test(memory_test_pool_allocator_0, NULL);
    pl_test_register_test(memory_test_pool_allocator_1, NULL);
    pl_test_register_test(memory_test_stack_allocator_0, NULL);
    pl_test_register_test(memory_test_temp_allocator_0, NULL);
}