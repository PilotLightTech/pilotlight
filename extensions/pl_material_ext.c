/*
   pl_material_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pl.h"
#include "pl_material_ext.h"

// extensions
#include "pl_asset_ext.h"
#include "pl_vfs_ext.h"
#include "pl_json_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plVfsI*    gptVfs    = NULL;
    static const plAssetI*  gptAsset  = NULL;
    static const plMemoryI* gptMemory = NULL;
    static const plJsonI* gptJson = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif


typedef struct _plMaterialContext
{
    plAssetTypeKey tAssetTypeKey;
} plMaterialContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static const char* gapcTextureSlotNames[] = {
    "base_color",
    "normal",
    "emissive",
    "occlusion",
    "metal_roughness",
    "clearcoat",
    "clearcoat_roughness",
    "clearcoat_normal",
    "sheen_color",
    "sheen_roughness",
    "iridescence",
    "iridescence_thickness",
    "anisotropy",
    "transmission_color",
    "transmission_thickness",
    "diffuse_transmission",
    "diffuse_transmission_color"
};

static plMaterialContext* gptMaterialCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

void
pl_material_init(plMaterial* ptMaterial)
{
    const plMaterial tMaterialDefault = {
        .eAlphaMode            = PL_MATERIAL_ALPHA_MODE_OPAQUE,
        .eFlags                = PL_MATERIAL_FLAG_NONE,
        .eMaterialModel        = PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS,
        .tBaseColor            = {1.0f, 1.0f, 1.0f, 1.0f},
        .tEmissiveColor        = {0.0f, 0.0f, 0.0f},
        .fRoughness            = 1.0f,
        .fMetalness            = 1.0f,
        .fAlphaCutoff          = 0.5f,
        .fNormalMapStrength    = 1.0f,
        .fEmissiveStrength     = 0.0f,
        .fOcclusionStrength    = 1.0f,
        .fIor                  = 1.5f,
        .tVolume = {
            .fThickness = 0.0f,
            .tAttenuationColor = {1.0f, 1.0f, 1.0f},
            .fAttenuationDistance = 100000.0f
        },
        .tDiffuseTransmission = {
            .fFactor = 0.0f,
            .tColor = {1.0f, 1.0f, 1.0f}
        },
        .tSheen = {
            .fRoughness = 0.0f,
            .tColor = {0}
        },
        .tIridescence = {
            .fIor = 1.3f,
            .fThicknessMin = 100.0f,
            .fThicknessMax = 400.0f
        },
        .tClearcoat = {
            .fFactor = 0.0f,
            .fRoughness = 0.0f,
            .fNormalMapStrength = 1.0f
        },
        .atTextures = {0}
    };
    *ptMaterial = tMaterialDefault;
}

static bool
pl__material_serialize(const char* pcName, const void* pMaterial, plAssetEncoding eEncoding)
{
    if(eEncoding == PL_ASSET_ENCODING_BINARY)
        return false;
        
    const plMaterial* ptMaterial = pMaterial;
    plJsonObject* ptRoot = gptJson->new_root_object("root");
    gptJson->add_string_member(ptRoot, "format", "plmaterial");
    gptJson->add_uint32_member(ptRoot, "version", 1);

    switch(ptMaterial->eMaterialModel)
    {
        case PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS:
            gptJson->add_string_member(ptRoot, "model", "pbr_metallic_roughness");
            break;

        default:
            PL_ASSERT(false && "unknown material model");
            break;

    }

    // base material stuff
    gptJson->add_float_array(ptRoot, "base_color", ptMaterial->tBaseColor.d, 4);
    gptJson->add_float_member(ptRoot, "metalness", ptMaterial->fMetalness);
    gptJson->add_float_member(ptRoot, "roughness", ptMaterial->fRoughness);
    gptJson->add_float_member(ptRoot, "ior", ptMaterial->fIor);
    gptJson->add_float_member(ptRoot, "normal_map_strength", ptMaterial->fNormalMapStrength);
    gptJson->add_float_member(ptRoot, "occlusion_strength", ptMaterial->fOcclusionStrength);
    gptJson->add_bool_member(ptRoot, "double_sided", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED));

    // alpha stuff
    plJsonObject* ptAlpha = gptJson->add_member(ptRoot, "alpha");
    switch(ptMaterial->eAlphaMode)
    {
        case PL_MATERIAL_ALPHA_MODE_MASK:
            gptJson->add_string_member(ptAlpha, "mode", "mask");
            break;

        case PL_MATERIAL_ALPHA_MODE_BLEND:
            gptJson->add_string_member(ptAlpha, "mode", "blend");
            break;
        
        case PL_MATERIAL_ALPHA_MODE_OPAQUE:
        default:
            gptJson->add_string_member(ptAlpha, "mode", "opaque");
            break;
    }
    gptJson->add_float_member(ptAlpha, "cutoff", ptMaterial->fAlphaCutoff);

    // clearcoat stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_CLEARCOAT)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "clearcoat");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tClearcoat.fFactor);
        gptJson->add_float_member(ptAdvanced, "roughness", ptMaterial->tClearcoat.fRoughness);
        gptJson->add_float_member(ptAdvanced, "normal_map_strength", ptMaterial->tClearcoat.fNormalMapStrength);
    }

    // sheen stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_SHEEN)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "sheen");
        gptJson->add_float_array(ptAdvanced, "color", ptMaterial->tSheen.tColor.d, 3);
        gptJson->add_float_member(ptAdvanced, "roughness", ptMaterial->tSheen.fRoughness);
    }

    // iridescence stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_IRIDESCENCE)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "iridescence");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tIridescence.fFactor);
        gptJson->add_float_member(ptAdvanced, "ior", ptMaterial->tIridescence.fIor);
        gptJson->add_float_member(ptAdvanced, "thickness_min", ptMaterial->tIridescence.fThicknessMin);
        gptJson->add_float_member(ptAdvanced, "thickness_max", ptMaterial->tIridescence.fThicknessMax);
    }

    // anisotropy stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_ANISOTROPY)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "anisotropy");
        gptJson->add_float_member(ptAdvanced, "strength", ptMaterial->tAnisotropy.fStrength);
        gptJson->add_float_member(ptAdvanced, "rotation", ptMaterial->tAnisotropy.fRotation);
    }

    // transmission stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_TRANSMISSION)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "transmission");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tTransmission.fFactor);
    }

    // volume stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_VOLUME)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "volume");
        gptJson->add_float_member(ptAdvanced, "thickness", ptMaterial->tVolume.fThickness);
        gptJson->add_float_member(ptAdvanced, "attenuation_distance", ptMaterial->tVolume.fAttenuationDistance);
        gptJson->add_float_array(ptAdvanced, "attenuation_color", ptMaterial->tVolume.tAttenuationColor.d, 3);
    }

    // dispersion stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DISPERSION)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "dispersion");
        gptJson->add_float_member(ptAdvanced, "dispersion", ptMaterial->tDispersion.fDispersion);
    }

    // diffuse transmission stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
    {
        plJsonObject* ptAdvanced = gptJson->add_member(ptRoot, "diffuse_transmission");
        gptJson->add_float_member(ptAdvanced, "factor", ptMaterial->tDiffuseTransmission.fFactor);
        gptJson->add_float_array(ptAdvanced, "color", ptMaterial->tDiffuseTransmission.tColor.d, 3);
    }

    // emissive stuff
    plJsonObject* ptEmissive = gptJson->add_member(ptRoot, "emissive");
    gptJson->add_float_array(ptEmissive, "color", ptMaterial->tEmissiveColor.d, 3);
    gptJson->add_float_member(ptEmissive, "strength", ptMaterial->fEmissiveStrength);

    // textures stuff
    uint32_t uTextureCount = 0;
    for(uint32_t i = 0; i < PL_MATERIAL_TEXTURE_SLOT_COUNT; i++)
    {
        if(gptAsset->is_valid(ptMaterial->atTextures[i].tTexture))
            uTextureCount++;
    }

    if(uTextureCount > 0)
    {
        plJsonObject* ptTextures = gptJson->add_member(ptRoot, "textures");
        for(uint32_t i = 0; i < PL_MATERIAL_TEXTURE_SLOT_COUNT; i++)
        {
            if(gptAsset->is_valid(ptMaterial->atTextures[i].tTexture))
            {
                plJsonObject* ptTexture = gptJson->add_member(ptTextures, gapcTextureSlotNames[i]);
                gptJson->add_string_member(ptTexture, "resource", gptAsset->get_path(ptMaterial->atTextures[i].tTexture));
                gptJson->add_uint32_member(ptTexture, "uv_set", ptMaterial->atTextures[i].uUVSet);
                gptJson->add_float_array(ptTexture, "scale", ptMaterial->atTextures[i].tScale.d, 2);
                gptJson->add_float_array(ptTexture, "offset", ptMaterial->atTextures[i].tOffset.d, 2);
                gptJson->add_float_member(ptTexture, "rotation", ptMaterial->atTextures[i].fRotation);
            }
        }
    }

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
    return true;
}

static bool
pl__material_deserialize(const char* pcName, void* pMaterial)
{
    plMaterial* ptMaterial = pMaterial;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
    uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
    memset(puFileBuffer, 0, szJsonFileSize + 1);

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
    gptVfs->close_file(tFileHandle);

    char acTempBuffer[256] = {0};

    plJsonObject* ptRoot = NULL;
    gptJson->load((const char*)puFileBuffer, &ptRoot);

    uint32_t uVersion = gptJson->uint32_member(ptRoot, "version", 0);

    pl_material_init(ptMaterial);

    // base material stuff
    gptJson->float_array_member(ptRoot, "base_color", ptMaterial->tBaseColor.d, NULL);
    ptMaterial->fMetalness = gptJson->float_member(ptRoot, "metalness", ptMaterial->fMetalness);
    ptMaterial->fRoughness = gptJson->float_member(ptRoot, "roughness", ptMaterial->fRoughness);
    ptMaterial->fIor = gptJson->float_member(ptRoot, "ior", ptMaterial->fIor);
    ptMaterial->fNormalMapStrength = gptJson->float_member(ptRoot, "normal_map_strength", ptMaterial->fNormalMapStrength);
    ptMaterial->fOcclusionStrength = gptJson->float_member(ptRoot, "occlusion_strength", ptMaterial->fOcclusionStrength);

    if(gptJson->bool_member(ptRoot, "double_sided", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)))
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_DOUBLE_SIDED;

    // alpha stuff
    plJsonObject* ptAlpha = gptJson->member(ptRoot, "alpha");
    if(ptAlpha)
    {
        ptMaterial->fAlphaCutoff = gptJson->float_member(ptAlpha, "cutoff", ptMaterial->fAlphaCutoff);
    
        strncpy(acTempBuffer, "opaque", 256);
        gptJson->string_member(ptAlpha, "mode", acTempBuffer, 256);
        if     (acTempBuffer[0] == 'm') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_MASK;
        else if(acTempBuffer[0] == 'b') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_BLEND;
        else if(acTempBuffer[0] == 'o') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_OPAQUE;
    }

    // clearcoat stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "clearcoat");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_CLEARCOAT;
            ptMaterial->tClearcoat.fFactor = gptJson->float_member(ptAdvanced, "factor", ptMaterial->tClearcoat.fFactor);
            ptMaterial->tClearcoat.fRoughness = gptJson->float_member(ptAdvanced, "roughness", ptMaterial->tClearcoat.fRoughness);
            ptMaterial->tClearcoat.fNormalMapStrength = gptJson->float_member(ptAdvanced, "normal_map_strength", ptMaterial->tClearcoat.fNormalMapStrength);
        }
    }

    // sheen stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "sheen");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_SHEEN;
            gptJson->float_array_member(ptAdvanced, "color", ptMaterial->tSheen.tColor.d, NULL);
            ptMaterial->tSheen.fRoughness = gptJson->float_member(ptAdvanced, "roughness", ptMaterial->tSheen.fRoughness);
        }
    }

    // iridescence stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "iridescence");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_IRIDESCENCE;
            ptMaterial->tIridescence.fFactor = gptJson->float_member(ptAdvanced, "factor", ptMaterial->tIridescence.fFactor);
            ptMaterial->tIridescence.fIor = gptJson->float_member(ptAdvanced, "ior", ptMaterial->tIridescence.fIor);
            ptMaterial->tIridescence.fThicknessMin = gptJson->float_member(ptAdvanced, "thickness_min", ptMaterial->tIridescence.fThicknessMin);
            ptMaterial->tIridescence.fThicknessMax = gptJson->float_member(ptAdvanced, "thickness_max", ptMaterial->tIridescence.fThicknessMax);
        }
    }

    // anisotropy stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "anisotropy");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_ANISOTROPY;
            ptMaterial->tAnisotropy.fStrength = gptJson->float_member(ptAdvanced, "strength", ptMaterial->tAnisotropy.fStrength);
            ptMaterial->tAnisotropy.fRotation = gptJson->float_member(ptAdvanced, "rotation", ptMaterial->tAnisotropy.fRotation);
        }
    }

    // transmission stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "transmission");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_TRANSMISSION;
            ptMaterial->tTransmission.fFactor = gptJson->float_member(ptAdvanced, "strength", ptMaterial->tTransmission.fFactor);
        }
    }

    // volume stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "volume");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_VOLUME;
            ptMaterial->tVolume.fThickness = gptJson->float_member(ptAdvanced, "thickness", ptMaterial->tVolume.fThickness);
            ptMaterial->tVolume.fAttenuationDistance = gptJson->float_member(ptAdvanced, "attenuation_distance", ptMaterial->tVolume.fAttenuationDistance);
            gptJson->float_array_member(ptAdvanced, "attenuation_color", ptMaterial->tVolume.tAttenuationColor.d, NULL);
        }
    }

    // dispersion stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "dispersion");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_DISPERSION;
            ptMaterial->tDispersion.fDispersion = gptJson->float_member(ptAdvanced, "dispersion", ptMaterial->tDispersion.fDispersion);
        }
    }

    // diffuse transmission stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "diffuse_transmission");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION;
            ptMaterial->tDiffuseTransmission.fFactor = gptJson->float_member(ptAdvanced, "factor", ptMaterial->tDiffuseTransmission.fFactor);
            gptJson->float_array_member(ptAdvanced, "color", ptMaterial->tDiffuseTransmission.tColor.d, NULL);
        }
    }

    // emissive stuff
    {
        plJsonObject* ptAdvanced = gptJson->member(ptRoot, "emissive");
        if(ptAdvanced)
        {
            ptMaterial->fEmissiveStrength = gptJson->float_member(ptAdvanced, "strength", ptMaterial->fEmissiveStrength);
            gptJson->float_array_member(ptAdvanced, "color", ptMaterial->tEmissiveColor.d, NULL);
        }
    }

    plJsonObject* ptTextures = gptJson->member(ptRoot, "textures");
    if(ptTextures)
    {
        uint32_t uTextureCount = UINT32_MAX;
        gptJson->member_list(ptTextures, NULL, &uTextureCount, NULL);
        for(uint32_t i = 0; i < uTextureCount; i++)
        {
            plJsonObject* ptTexture = gptJson->member_by_index(ptTextures, i);
            const char* pcTextureName = gptJson->get_name(ptTexture);

            plMaterialTextureSlot tTextureSlot = 0;
            if     (pcTextureName[0] == 'b') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_BASE_COLOR;
            else if(pcTextureName[0] == 'n') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_NORMAL;
            else if(pcTextureName[0] == 'e') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_EMISSIVE;
            else if(pcTextureName[0] == 'o') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_OCCLUSION;
            else if(pcTextureName[0] == 'm') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_METAL_ROUGHNESS;
            else if(pcTextureName[0] == 'a') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_ANISOTROPY;
            else if(pcTextureName[0] == 'c')
            {
                if     (pcTextureName[10] == 'r') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS;
                else if(pcTextureName[10] == 'n') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT_NORMAL;
                else                              tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_CLEARCOAT;
            }
            else if(pcTextureName[0] == 's')
            {
                if     (pcTextureName[6] == 'c') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_SHEEN_COLOR;
                else if(pcTextureName[6] == 'r') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_SHEEN_ROUGHNESS;
            }
            else if(pcTextureName[0] == 'i')
            {
                if     (pcTextureName[12] == 't') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS;
                else                              tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_IRIDESCENCE;
            }
            else if(pcTextureName[0] == 't')
            {
                if     (pcTextureName[13] == 'c') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_TRANSMISSION;
                else if(pcTextureName[13] == 't') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_THICKNESS;
            }
            else if(pcTextureName[0] == 'd')
            {
                if     (pcTextureName[21] == 'c') tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION_COLOR;
                else                              tTextureSlot = PL_MATERIAL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION;
            }

            ptMaterial->atTextures[tTextureSlot].uUVSet = gptJson->uint32_member(ptTexture, "uv_set", ptMaterial->atTextures[tTextureSlot].uUVSet);
            ptMaterial->atTextures[tTextureSlot].fRotation = gptJson->float_member(ptTexture, "rotation", ptMaterial->atTextures[tTextureSlot].fRotation);
            gptJson->float_array_member(ptTexture, "scale", ptMaterial->atTextures[tTextureSlot].tScale.d, NULL);
            gptJson->float_array_member(ptTexture, "offset", ptMaterial->atTextures[tTextureSlot].tOffset.d, NULL);
            gptJson->string_member(ptTexture, "resource", acTempBuffer, 256);
            ptMaterial->atTextures[tTextureSlot].tTexture = gptAsset->load(acTempBuffer);
        }
    }

    PL_FREE(puFileBuffer);
    gptJson->unload(&ptRoot);
    return true;
}

void
pl_material_register_asset_types(void)
{
    static const plAssetTypeDesc tDesc = {
        .pcName          = "Material",
        .pcFileExtension = "plmaterial",
        .szSize          = sizeof(plMaterial),
        .serialize       = pl__material_serialize,
        .deserialize     = pl__material_deserialize,
    };
    gptMaterialCtx->tAssetTypeKey = gptAsset->register_type(tDesc);
}

plAssetTypeKey
pl_material_get_asset_type_key(void)
{
    return gptMaterialCtx->tAssetTypeKey;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_material_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plMaterialI tApi = {
        .init        = pl_material_init,
        .register_asset_types = pl_material_register_asset_types,
        .get_asset_type_key   = pl_material_get_asset_type_key,
    };
    pl_set_api(ptApiRegistry, plMaterialI, &tApi);

    #ifndef PL_UNITY_BUILD
    gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAsset  = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptJson  = pl_get_api_latest(ptApiRegistry, plJsonI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptMaterialCtx = ptDataRegistry->get_data("plMaterialContext");
    }
    else // first load
    {
        static plMaterialContext tCtx = {0};
        gptMaterialCtx = &tCtx;
        ptDataRegistry->set_data("plMaterialContext", gptMaterialCtx);
    }
}

void
pl_unload_material_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plMaterialI* ptApi = pl_get_api_latest(ptApiRegistry, plMaterialI);
    ptApiRegistry->remove_api(ptApi);
}
