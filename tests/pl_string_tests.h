#include "pl_test.h"
#include "pl_string.h"

void
string_test_0(void* pData)
{
    const char* pcFilePath0 = "C:/Users/hoffstadt/file1.txt";
    const char* pcFilePath1 = "C:\\Users\\hoffstadt\\file1.txt";
    const char* pcFilePath2 = "C:\\Users/hoffstadt\\file1.txt";
    const char* pcFilePath3 = "file1.txt";
    const char* pcFilePath4 = "file1";
    const char* pcFilePath5 = "/tmp/dir2/file1.txt";
    const char* pcFilePath6 = "/tmp/file1.txt";
    const char* pcFilePath7 = "/file1.txt";

    const char* pcExt0 = pl_str_get_file_extension(pcFilePath0, NULL, 0);
    const char* pcExt1 = pl_str_get_file_extension(pcFilePath1, NULL, 0);
    const char* pcExt2 = pl_str_get_file_extension(pcFilePath2, NULL, 0);
    const char* pcExt3 = pl_str_get_file_extension(pcFilePath3, NULL, 0);
    const char* pcExt4 = pl_str_get_file_extension(pcFilePath4, NULL, 0);

    pl_test_expect_string_equal(pcExt0, "txt", NULL);
    pl_test_expect_string_equal(pcExt1, "txt", NULL);
    pl_test_expect_string_equal(pcExt2, "txt", NULL);
    pl_test_expect_string_equal(pcExt3, "txt", NULL);
    pl_test_expect_uint64_equal(((uint64_t)pcExt4), 0, NULL);

    const char* pcFile0 = pl_str_get_file_name(pcFilePath0, NULL, 0);
    const char* pcFile1 = pl_str_get_file_name(pcFilePath1, NULL, 0);
    const char* pcFile2 = pl_str_get_file_name(pcFilePath2, NULL, 0);
    const char* pcFile3 = pl_str_get_file_name(pcFilePath3, NULL, 0);
    const char* pcFile4 = pl_str_get_file_name(pcFilePath4, NULL, 0);

    pl_test_expect_string_equal(pcFile0, "file1.txt", "pl_str_get_file_name");
    pl_test_expect_string_equal(pcFile1, "file1.txt", "pl_str_get_file_name");
    pl_test_expect_string_equal(pcFile2, "file1.txt", "pl_str_get_file_name");
    pl_test_expect_string_equal(pcFile3, "file1.txt", "pl_str_get_file_name");
    pl_test_expect_string_equal(pcFile4, "file1", "pl_str_get_file_name");

    char acFileName0[256] = {0};
    char acFileName1[256] = {0};
    char acFileName2[256] = {0};
    char acFileName3[256] = {0};
    char acFileName4[256] = {0};
    pl_str_get_file_name_only(pcFilePath0, acFileName0, 256);
    pl_str_get_file_name_only(pcFilePath1, acFileName1, 256);
    pl_str_get_file_name_only(pcFilePath2, acFileName2, 256);
    pl_str_get_file_name_only(pcFilePath3, acFileName3, 256);
    pl_str_get_file_name_only(pcFilePath4, acFileName4, 256);

    pl_test_expect_string_equal(acFileName0, "file1", "pl_str_get_file_name_only");
    pl_test_expect_string_equal(acFileName1, "file1", "pl_str_get_file_name_only");
    pl_test_expect_string_equal(acFileName2, "file1", "pl_str_get_file_name_only");
    pl_test_expect_string_equal(acFileName3, "file1", "pl_str_get_file_name_only");
    pl_test_expect_string_equal(acFileName4, "file1", "pl_str_get_file_name_only");

    char acDirectory0[128] = {0};
    char acDirectory1[128] = {0};
    char acDirectory2[128] = {0};
    char acDirectory3[128] = {0};
    char acDirectory4[128] = {0};
    char acDirectory5[128] = {0};
    char acDirectory6[128] = {0};
    char acDirectory7[128] = {0};

    pl_str_get_directory(pcFilePath0, acDirectory0, 128);
    pl_str_get_directory(pcFilePath1, acDirectory1, 128);
    pl_str_get_directory(pcFilePath2, acDirectory2, 128);
    pl_str_get_directory(pcFilePath3, acDirectory3, 128);
    pl_str_get_directory(pcFilePath4, acDirectory4, 128);
    pl_str_get_directory(pcFilePath5, acDirectory5, 128);
    pl_str_get_directory(pcFilePath6, acDirectory6, 128);

    pl_test_expect_string_equal(acDirectory0, "C:/Users/hoffstadt/", NULL);
    pl_test_expect_string_equal(acDirectory1, "C:\\Users\\hoffstadt\\", NULL);
    pl_test_expect_string_equal(acDirectory2, "C:\\Users/hoffstadt\\", NULL);
    pl_test_expect_string_equal(acDirectory3, "./", NULL);
    pl_test_expect_string_equal(acDirectory4, "./", NULL);
    pl_test_expect_string_equal(acDirectory5, "/tmp/dir2/", NULL);
    pl_test_expect_string_equal(acDirectory6, "/tmp/", NULL);
}

void
pl_string_tests(void* pData)
{
    pl_test_register_test(string_test_0, NULL);
}