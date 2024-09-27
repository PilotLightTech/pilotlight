#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pl_test.h"

#include <stdint.h>
#include "pl_json.h"

void
write_json_test(void* pData)
{

    char** ppcBuffer = pData;

    // root object
    plJsonObject tRootJsonObject = {0};
    pl_json_add_string_member(&tRootJsonObject, "first name", "John");
    pl_json_add_string_member(&tRootJsonObject, "last name", "Doe");
    pl_json_add_int_member(&tRootJsonObject, "age", 40);
    pl_json_add_bool_member(&tRootJsonObject, "tall", false);
    pl_json_add_bool_member(&tRootJsonObject, "hungry", true);
    int aScores[] = {100, 86, 46};
    pl_json_add_int_array(&tRootJsonObject, "scores", aScores, 3);

    char* aPets[] = {"Riley", "Luna", "Chester"};
    pl_json_add_string_array(&tRootJsonObject, "pets", aPets, 3);

    // member object
    plJsonObject tBestFriend = {0};
    pl_json_add_string_member(&tBestFriend, "first name", "John");
    pl_json_add_string_member(&tBestFriend, "last name", "Doe");
    pl_json_add_int_member(&tBestFriend, "age", 40);
    pl_json_add_bool_member(&tBestFriend, "tall", false);
    pl_json_add_bool_member(&tBestFriend, "hungry", true);
    pl_json_add_string_array(&tBestFriend, "pets", aPets, 3);
    pl_json_add_int_array(&tBestFriend, "scores", aScores, 3);

    pl_json_add_member(&tRootJsonObject, "best friend", &tBestFriend);

    // friend member object
    plJsonObject atFriends[2] = {0};
    int aScores0[] = {88, 86, 100};
    pl_json_add_string_member(&atFriends[0], "first name", "Jacob");
    pl_json_add_string_member(&atFriends[0], "last name", "Smith");
    pl_json_add_int_member(&atFriends[0], "age", 23);
    pl_json_add_bool_member(&atFriends[0], "tall", true);
    pl_json_add_bool_member(&atFriends[0], "hungry", false);
    pl_json_add_int_array(&atFriends[0], "scores", aScores0, 3);

    int aScores1[] = {80, 80, 100};
    pl_json_add_string_member(&atFriends[1], "first name", "Chance");
    pl_json_add_string_member(&atFriends[1], "last name", "Dale");
    pl_json_add_int_member(&atFriends[1], "age", 48);
    pl_json_add_bool_member(&atFriends[1], "tall", true);
    pl_json_add_bool_member(&atFriends[1], "hungry", true);
    pl_json_add_int_array(&atFriends[1], "scores", aScores1, 3);

    pl_json_add_member_array(&tRootJsonObject, "friends", atFriends, 2);

    uint32_t uBufferSize = 0;
    pl_write_json(&tRootJsonObject, NULL, &uBufferSize);

    *ppcBuffer = malloc(uBufferSize + 1);
    memset(*ppcBuffer, 0, uBufferSize + 1);
    pl_write_json(&tRootJsonObject, *ppcBuffer, &uBufferSize);

    pl_unload_json(&tRootJsonObject);
}

void
read_json_test(void* pData)
{

    char** ppcBuffer = pData;

    plJsonObject tRootJsonObject = {0};
    pl_load_json(*ppcBuffer, &tRootJsonObject);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~reading~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // check root members
    {
        int aiScores[3] = {0};
        pl_json_int_array_member(&tRootJsonObject, "scores", aiScores, NULL);
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
        pl_json_string_array_member(&tRootJsonObject, "pets", aacPets, NULL, auLengths);
        pl_test_expect_string_equal(acPet0, "Riley", NULL);
        pl_test_expect_string_equal(acPet1, "Luna", NULL);
        pl_test_expect_string_equal(acPet2, "Chester", NULL);
    }

    char acFirstName[64] = {0};
    char acLastName[64] = {0};
    pl_test_expect_string_equal(pl_json_string_member(&tRootJsonObject, "first name", acFirstName, 64), "John", NULL);
    pl_test_expect_string_equal(pl_json_string_member(&tRootJsonObject, "last name", acLastName, 64), "Doe", NULL);
    pl_test_expect_int_equal(pl_json_int_member(&tRootJsonObject, "age", 0), 40, NULL);
    pl_test_expect_false(pl_json_bool_member(&tRootJsonObject, "tall", false), NULL);
    pl_test_expect_true(pl_json_bool_member(&tRootJsonObject, "hungry", false), NULL);

    // check child members
    plJsonObject* ptBestFriend = pl_json_member(&tRootJsonObject, "best friend");
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
    plJsonObject* sbtFriends = pl_json_array_member(&tRootJsonObject, "friends", &uFriendCount);

    plJsonObject* ptFriend0 = &sbtFriends[0];
    plJsonObject* ptFriend1 = &sbtFriends[1];
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

    pl_unload_json(&tRootJsonObject);
}


void
pl_json_tests(void* pData)
{
    static char* pcBuffer = NULL;

    pl_test_register_test(write_json_test, &pcBuffer);
    pl_test_register_test(read_json_test, &pcBuffer);
}