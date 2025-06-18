/*
   pl_config_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] enums
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h>
#include "pl.h"
#include "pl_config_ext.h"

// extensions
#include "pl_vfs_ext.h"

// libs
#include "pl_json.h"
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #define PL_MEMORY_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_MEMORY_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plVfsI* gptVfs = NULL;

#endif

#include "pl_memory.h"
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConfigVar     plConfigVar;
typedef struct _plConfigContext plConfigContext;

// enums
typedef int plConfigVarType;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plConfigVarType
{
    PL_CONFIG_VAR_TYPE_NONE = 0,
    PL_CONFIG_VAR_TYPE_BOOL,
    PL_CONFIG_VAR_TYPE_STRING,
    PL_CONFIG_VAR_TYPE_INT,
    PL_CONFIG_VAR_TYPE_UINT,
    PL_CONFIG_VAR_TYPE_DOUBLE,
    PL_CONFIG_VAR_TYPE_VEC2,
};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plConfigVar
{
    plConfigVarType tType;
    char acName[PL_MAX_NAME_LENGTH];
    union{
        bool     bValue;
        int      iValue;
        uint32_t uValue;
        double   dValue;
        plVec2   tValueVec2;
    };

} plConfigVar;

typedef struct _plConfigContext
{
    plConfigSettings tSettings;
    plHashMap32      tHashmap;
    plConfigVar*     sbtVars;
    plTempAllocator  tTempAllocator;
} plConfigContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plConfigContext* gptConfigCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_config_initialize(plConfigSettings tSettings)
{
    gptConfigCtx->tSettings = tSettings;
}

void
pl_config_cleanup(void)
{
    pl_sb_free(gptConfigCtx->sbtVars);
    pl_hm32_free(&gptConfigCtx->tHashmap);
    pl_temp_allocator_free(&gptConfigCtx->tTempAllocator);
}

bool
pl_config_load_bool(const char* pcName, bool bDefaultValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_BOOL)
            return gptConfigCtx->sbtVars[uIndex].bValue;
        return bDefaultValue;
    }

    uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
    if(uIndex == PL_DS_HASH32_INVALID)
    {
        uIndex = pl_sb_size(gptConfigCtx->sbtVars);
        pl_sb_add(gptConfigCtx->sbtVars);
    }

    plConfigVar tVar = {
        .tType = PL_CONFIG_VAR_TYPE_BOOL,
        .bValue = bDefaultValue
    };
    strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
    gptConfigCtx->sbtVars[uIndex] = tVar;

    pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);

    return bDefaultValue;
}

int
pl_config_load_int(const char* pcName, int iDefaultValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_INT)
            return gptConfigCtx->sbtVars[uIndex].iValue;
        return iDefaultValue;
    }

    uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
    if(uIndex == PL_DS_HASH32_INVALID)
    {
        uIndex = pl_sb_size(gptConfigCtx->sbtVars);
        pl_sb_add(gptConfigCtx->sbtVars);
    }

    plConfigVar tVar = {
        .tType = PL_CONFIG_VAR_TYPE_INT,
        .iValue = iDefaultValue
    };
    strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
    gptConfigCtx->sbtVars[uIndex] = tVar;

    pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);

    return iDefaultValue;
}

uint32_t
pl_config_load_uint(const char* pcName, uint32_t uDefaultValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_UINT)
            return gptConfigCtx->sbtVars[uIndex].uValue;
        return uDefaultValue;
    }

    uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
    if(uIndex == PL_DS_HASH32_INVALID)
    {
        uIndex = pl_sb_size(gptConfigCtx->sbtVars);
        pl_sb_add(gptConfigCtx->sbtVars);
    }

    plConfigVar tVar = {
        .tType = PL_CONFIG_VAR_TYPE_UINT,
        .uValue = uDefaultValue
    };
    strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
    gptConfigCtx->sbtVars[uIndex] = tVar;

    pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);

    return uDefaultValue;
}

double
pl_config_load_double(const char* pcName, double dDefaultValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_DOUBLE)
            return gptConfigCtx->sbtVars[uIndex].dValue;
        return dDefaultValue;
    }

    uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
    if(uIndex == PL_DS_HASH32_INVALID)
    {
        uIndex = pl_sb_size(gptConfigCtx->sbtVars);
        pl_sb_add(gptConfigCtx->sbtVars);
    }

    plConfigVar tVar = {
        .tType = PL_CONFIG_VAR_TYPE_DOUBLE,
        .dValue = dDefaultValue
    };
    strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
    gptConfigCtx->sbtVars[uIndex] = tVar;

    pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);

    return dDefaultValue;
}

plVec2
pl_config_load_vec2(const char* pcName, plVec2 tDefaultValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_VEC2)
            return gptConfigCtx->sbtVars[uIndex].tValueVec2;
        return tDefaultValue;
    }

    uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
    if(uIndex == PL_DS_HASH32_INVALID)
    {
        uIndex = pl_sb_size(gptConfigCtx->sbtVars);
        pl_sb_add(gptConfigCtx->sbtVars);
    }

    plConfigVar tVar = {
        .tType = PL_CONFIG_VAR_TYPE_VEC2,
        .tValueVec2 = tDefaultValue
    };
    strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
    gptConfigCtx->sbtVars[uIndex] = tVar;

    pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);

    return tDefaultValue;
}

void
pl_config_set_bool(const char* pcName, bool bValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_BOOL)
            gptConfigCtx->sbtVars[uIndex].bValue = bValue;
    }
    else
    {

        uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
        if(uIndex == PL_DS_HASH32_INVALID)
        {
            uIndex = pl_sb_size(gptConfigCtx->sbtVars);
            pl_sb_add(gptConfigCtx->sbtVars);
        }

        plConfigVar tVar = {
            .tType = PL_CONFIG_VAR_TYPE_BOOL,
            .bValue = bValue
        };
        strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
        gptConfigCtx->sbtVars[uIndex] = tVar;

        pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);
    }
}

void
pl_config_set_int(const char* pcName, int iValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_INT)
            gptConfigCtx->sbtVars[uIndex].iValue = iValue;
    }
    else
    {

        uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
        if(uIndex == PL_DS_HASH32_INVALID)
        {
            uIndex = pl_sb_size(gptConfigCtx->sbtVars);
            pl_sb_add(gptConfigCtx->sbtVars);
        }

        plConfigVar tVar = {
            .tType = PL_CONFIG_VAR_TYPE_INT,
            .iValue = iValue
        };
        strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
        gptConfigCtx->sbtVars[uIndex] = tVar;

        pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);
    }
}

void
pl_config_set_uint(const char* pcName, uint32_t uValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_UINT)
            gptConfigCtx->sbtVars[uIndex].uValue = uValue;
    }
    else
    {

        uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
        if(uIndex == PL_DS_HASH32_INVALID)
        {
            uIndex = pl_sb_size(gptConfigCtx->sbtVars);
            pl_sb_add(gptConfigCtx->sbtVars);
        }

        plConfigVar tVar = {
            .tType = PL_CONFIG_VAR_TYPE_UINT,
            .uValue = uValue
        };
        strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
        gptConfigCtx->sbtVars[uIndex] = tVar;

        pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);
    }
}

void
pl_config_set_double(const char* pcName, double dValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_DOUBLE)
            gptConfigCtx->sbtVars[uIndex].dValue = dValue;
    }
    else
    {

        uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
        if(uIndex == PL_DS_HASH32_INVALID)
        {
            uIndex = pl_sb_size(gptConfigCtx->sbtVars);
            pl_sb_add(gptConfigCtx->sbtVars);
        }

        plConfigVar tVar = {
            .tType = PL_CONFIG_VAR_TYPE_DOUBLE,
            .dValue = dValue
        };
        strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
        gptConfigCtx->sbtVars[uIndex] = tVar;

        pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);
    }
}

void
pl_config_set_vec2(const char* pcName, plVec2 tValue)
{

    const uint64_t uHash = pl_hm_hash_str(pcName, 0);

    uint32_t uIndex = 0;
    if(pl_hm32_has_key_ex(&gptConfigCtx->tHashmap, uHash, &uIndex))
    {
        if(gptConfigCtx->sbtVars[uIndex].tType == PL_CONFIG_VAR_TYPE_VEC2)
            gptConfigCtx->sbtVars[uIndex].tValueVec2 = tValue;
    }
    else
    {

        uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
        if(uIndex == PL_DS_HASH32_INVALID)
        {
            uIndex = pl_sb_size(gptConfigCtx->sbtVars);
            pl_sb_add(gptConfigCtx->sbtVars);
        }

        plConfigVar tVar = {
            .tType = PL_CONFIG_VAR_TYPE_VEC2,
            .tValueVec2 = tValue
        };
        strncpy(tVar.acName, pcName, PL_MAX_NAME_LENGTH);
        gptConfigCtx->sbtVars[uIndex] = tVar;

        pl_hm32_insert(&gptConfigCtx->tHashmap, uHash, uIndex);
    }
}

void
pl_config_load_from_disk(const char* pcFileName)
{

    if(pcFileName == NULL)
        pcFileName = "pl_config.json";

    if(!gptVfs->does_file_exist(pcFileName))
        return;


    size_t szJsonFileSize = gptVfs->get_file_size_str(pcFileName);
    plVfsFileHandle tHandle = gptVfs->open_file(pcFileName, PL_VFS_FILE_MODE_READ);

    uint8_t* puFileBuffer = pl_temp_allocator_alloc(&gptConfigCtx->tTempAllocator, szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);
    gptVfs->read_file(tHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tHandle);

    plJsonObject* ptRootJsonObject = NULL;
    pl_load_json((const char*)puFileBuffer, &ptRootJsonObject);

    uint32_t uVarCount = 0;
    pl_json_member_list(ptRootJsonObject, NULL, &uVarCount, NULL);
    for(uint32_t i = 0; i < uVarCount; i++)
    {
        plJsonObject* ptVarObject = pl_json_member_by_index(ptRootJsonObject, i);

        plConfigVar tVar = {0};

        strncpy(tVar.acName, pl_json_get_name(ptVarObject), PL_MAX_NAME_LENGTH);

        char acTypeBuffer[64] = {0};
        const char* pcTypeString = pl_json_string_member(ptVarObject, "type", acTypeBuffer, 64);

        
        if(pl_str_equal(pcTypeString, "bool"))
        {
            tVar.tType = PL_CONFIG_VAR_TYPE_BOOL;
            tVar.bValue = pl_json_bool_member(ptVarObject, "value", false);
        }
        else if(pl_str_equal(pcTypeString, "int"))
        {
            tVar.tType = PL_CONFIG_VAR_TYPE_INT;
            tVar.iValue = pl_json_int_member(ptVarObject, "value", 0);
        }
        else if(pl_str_equal(pcTypeString, "uint"))
        {
            tVar.tType = PL_CONFIG_VAR_TYPE_UINT;
            tVar.uValue = pl_json_uint_member(ptVarObject, "value", 0);
        }
        else if(pl_str_equal(pcTypeString, "double"))
        {
            tVar.tType = PL_CONFIG_VAR_TYPE_DOUBLE;
            tVar.dValue = pl_json_double_member(ptVarObject, "value", 0.0);
            
        }
        else if(pl_str_equal(pcTypeString, "vec2"))
        {
            tVar.tType = PL_CONFIG_VAR_TYPE_VEC2;
            pl_json_float_array_member(ptVarObject, "value", tVar.tValueVec2.d, NULL);
        }

        uint32_t uIndex = pl_hm32_get_free_index(&gptConfigCtx->tHashmap);
        if(uIndex == PL_DS_HASH32_INVALID)
        {
            uIndex = pl_sb_size(gptConfigCtx->sbtVars);
            pl_sb_add(gptConfigCtx->sbtVars);
        }

        pl_hm32_insert_str(&gptConfigCtx->tHashmap, tVar.acName, uIndex);

        gptConfigCtx->sbtVars[uIndex] = tVar;
    }

    pl_temp_allocator_reset(&gptConfigCtx->tTempAllocator);
}

void
pl_config_save_to_disk(const char* pcFileName)
{
    if(pcFileName == NULL)
        pcFileName = "pl_config.json";

    plJsonObject* ptRootJsonObject = pl_json_new_root_object("ROOT");

    const uint32_t uVarCount = pl_sb_size(gptConfigCtx->sbtVars);

    for(uint32_t i = 0; i < uVarCount; i++)
    {
        const plConfigVar* ptVar = &gptConfigCtx->sbtVars[i];

        plJsonObject* ptNewJsonVar = pl_json_add_member(ptRootJsonObject, ptVar->acName); 

        switch(ptVar->tType)
        {
            case PL_CONFIG_VAR_TYPE_BOOL:
                pl_json_add_string_member(ptNewJsonVar, "type", "bool");
                pl_json_add_bool_member(ptNewJsonVar, "value", ptVar->bValue);
                break;
            case PL_CONFIG_VAR_TYPE_INT:
                pl_json_add_string_member(ptNewJsonVar, "type", "int");
                pl_json_add_bool_member(ptNewJsonVar, "value", ptVar->iValue);
                break;
            case PL_CONFIG_VAR_TYPE_UINT:
                pl_json_add_string_member(ptNewJsonVar, "type", "uint");
                pl_json_add_bool_member(ptNewJsonVar, "value", ptVar->uValue);
                break;
            case PL_CONFIG_VAR_TYPE_DOUBLE:
                pl_json_add_string_member(ptNewJsonVar, "type", "double");
                pl_json_add_bool_member(ptNewJsonVar, "value", ptVar->dValue);
                break;
            case PL_CONFIG_VAR_TYPE_VEC2:
                pl_json_add_string_member(ptNewJsonVar, "type", "vec2");
                pl_json_add_float_array(ptNewJsonVar, "value", ptVar->tValueVec2.d, 2);
                break;
            default:
                PL_ASSERT(false);
                break;
        }
    }

    uint32_t uBufferSize = 0;
    pl_write_json(ptRootJsonObject, NULL, &uBufferSize);

    char* pcBuffer = pl_temp_allocator_alloc(&gptConfigCtx->tTempAllocator, uBufferSize + 1);
    memset(pcBuffer, 0, uBufferSize + 1);
    pl_write_json(ptRootJsonObject, pcBuffer, &uBufferSize);

    plVfsFileHandle tHandle = gptVfs->open_file(pcFileName, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tHandle, (uint8_t*)pcBuffer, uBufferSize);
    gptVfs->close_file(tHandle);
    

    pl_unload_json(&ptRootJsonObject);

    pl_temp_allocator_reset(&gptConfigCtx->tTempAllocator);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_config_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plConfigI tApi = {
        .initialize     = pl_config_initialize,
        .cleanup        = pl_config_cleanup,
        .load_from_disk = pl_config_load_from_disk,
        .save_to_disk   = pl_config_save_to_disk,
        .load_bool      = pl_config_load_bool,
        .load_int       = pl_config_load_int,
        .load_uint      = pl_config_load_uint,
        .load_double    = pl_config_load_double,
        .load_vec2      = pl_config_load_vec2,
        .set_bool       = pl_config_set_bool,
        .set_int        = pl_config_set_int,
        .set_uint       = pl_config_set_uint,
        .set_double     = pl_config_set_double,
        .set_vec2       = pl_config_set_vec2,
    };
    pl_set_api(ptApiRegistry, plConfigI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptVfs   = pl_get_api_latest(ptApiRegistry, plVfsI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptConfigCtx = ptDataRegistry->get_data("plConfigContext");
    }
    else
    {
        static plConfigContext gtConfigCtx = {0};
        gptConfigCtx = &gtConfigCtx;
        ptDataRegistry->set_data("plConfigContext", gptConfigCtx);
    }
}

PL_EXPORT void
pl_unload_config_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plConfigI* ptApi = pl_get_api_latest(ptApiRegistry, plConfigI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

   #define PL_JSON_IMPLEMENTATION
   #include "pl_json.h"
   #undef PL_JSON_IMPLEMENTATION

   #define PL_MEMORY_IMPLEMENTATION
   #include "pl_memory.h"
   #undef PL_MEMORY_IMPLEMENTATION

#endif