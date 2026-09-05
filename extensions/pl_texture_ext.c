/*
   pl_texture_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_texture_ext.h"
#include "pl_math.h"

// extensions
#include "pl_asset_ext.h"
#include "pl_vfs_ext.h"
#include "pl_graphics_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_json_ext.h"

// libraries
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plAssetI*        gptAsset     = NULL;
    static const plVfsI*          gptVfs       = NULL;
    static const plGraphicsI*     gptGfx       = NULL;
    static const plStringInternI* gptString    = NULL;
    static const plJsonI*         gptJson      = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTextureContext
{
    plAssetTypeKey tAssetTypeKey;
} plTextureContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plTextureContext* gptTextureCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool
pl__texture_serialize(const char* pcName, const void* pTexture, plAssetEncoding eEncoding)
{
    const plTextureAsset* ptTexture = pTexture;
    if(eEncoding == PL_ASSET_ENCODING_BINARY)
    {
        return false;
    }
    else
    {
        plJsonObject* ptRoot = gptJson->new_root_object("root");

        gptJson->add_string_member(ptRoot, "file_format", "pltexture");
        gptJson->add_uint32_member(ptRoot, "version", 1);
        gptJson->add_string_member(ptRoot, "type", "2d");
        gptJson->add_string_member(ptRoot, "color_space", ptTexture->bSRGB ? "srgb" : "linear");
        gptJson->add_string_member(ptRoot, "format", gptGfx->get_format_as_string(ptTexture->eFormat));
        gptJson->add_bool_member(ptRoot, "generate_mips", ptTexture->bGenerateMips);
        gptJson->add_bool_member(ptRoot, "compress", ptTexture->bCompress);
        gptJson->add_string_member(ptRoot, "source", ptTexture->pcSourceFile);

        uint32_t uBufferSize = 0;
        gptJson->write(ptRoot, NULL, &uBufferSize);
        char* pcBuffer = PL_ALLOC(uBufferSize);
        memset(pcBuffer, 0, uBufferSize);
        gptJson->write(ptRoot, pcBuffer, &uBufferSize);
        
        gptVfs->register_file(pcName, false);
        plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
        gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
        gptVfs->close_file(tFileHandle);

        PL_FREE(pcBuffer);
        gptJson->unload(&ptRoot);
    }
    return true;   
}

static bool
pl__texture_deserialize(const char* pcName, void* pTexture)
{
    plTextureAsset* ptTexture = pTexture;
    if(!gptVfs->does_file_exist(pcName))
        return false;

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);

    plAssetFileHeader tAssetHeader = {0};
    gptVfs->read_file_stream(tFileHandle, sizeof(plAssetFileHeader), 1, &tAssetHeader);

    if(tAssetHeader.uMagic == PL_ASSET_MAGIC)
    {
        PL_ASSERT(false && "binary texture serialization not implemented");
    }
    else // json
    {
        char acTempBuffer[256] = {0};

        gptVfs->set_file_stream_position(tFileHandle, 0);
        size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
        uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);
        
        gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);

        plJsonObject* ptRoot = NULL;
        gptJson->load((const char*)puFileBuffer, &ptRoot);

        gptJson->string_member(ptRoot, "format", acTempBuffer, 256);

        if     (pl_str_equal(acTempBuffer, "PL_FORMAT_R32G32B32A32_FLOAT")) ptTexture->eFormat = PL_FORMAT_R32G32B32A32_FLOAT;
        else if(pl_str_equal(acTempBuffer, "PL_FORMAT_R16G16B16A16_UNORM")) ptTexture->eFormat = PL_FORMAT_R16G16B16A16_UNORM;
        else if(pl_str_equal(acTempBuffer, "PL_FORMAT_BC3_UNORM"))          ptTexture->eFormat = PL_FORMAT_BC3_UNORM;
        else if(pl_str_equal(acTempBuffer, "PL_FORMAT_R8G8B8A8_UNORM"))     ptTexture->eFormat = PL_FORMAT_R8G8B8A8_UNORM;

        ptTexture->bGenerateMips = gptJson->bool_member(ptRoot, "generate_mips", false);
        ptTexture->bCompress = gptJson->bool_member(ptRoot, "compress", false);

        gptJson->string_member(ptRoot, "color_space", acTempBuffer, 256);
        if     (acTempBuffer[0] == 's') ptTexture->bSRGB = true;
        else if(acTempBuffer[0] == 'l') ptTexture->bSRGB = false;

        gptJson->string_member(ptRoot, "source", acTempBuffer, 256);
        ptTexture->pcSourceFile = gptString->intern(acTempBuffer);


        PL_FREE(puFileBuffer);
        gptJson->unload(&ptRoot);
    }

    gptVfs->close_file(tFileHandle);
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] public api implmentation
//-----------------------------------------------------------------------------

void
pl_texture_register_asset_type(void)
{
    static const plAssetTypeDesc tDesc = {
        .pcName          = "Texture",
        .pcFileExtension = "pltexture",
        .szSize          = sizeof(plTextureAsset),
        .serialize       = pl__texture_serialize,
        .deserialize     = pl__texture_deserialize,
    };
    gptTextureCtx->tAssetTypeKey = gptAsset->register_type(tDesc);
}

plAssetTypeKey
pl_texture_get_asset_type_key(void)
{
    return gptTextureCtx->tAssetTypeKey;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_texture_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plTextureI tApi = {
        .register_asset_types = pl_texture_register_asset_type,
        .get_asset_type_key  = pl_texture_get_asset_type_key
    };
    pl_set_api(ptApiRegistry, plTextureI, &tApi);

    #ifndef PL_UNITY_BUILD
    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAsset     = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptVfs       = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptGfx       = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptString    = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptJson      = pl_get_api_latest(ptApiRegistry, plJsonI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptTextureCtx = ptDataRegistry->get_data("plTextureContext");
    }
    else // first load
    {
        static plTextureContext tCtx = {0};
        gptTextureCtx = &tCtx;
        ptDataRegistry->set_data("plTextureContext", gptTextureCtx);
    }
}

void
pl_unload_texture_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plTextureI* ptApi = pl_get_api_latest(ptApiRegistry, plTextureI);
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

#endif