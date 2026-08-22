/*
   pl_material_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_material_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_log_ext.h"
#include "pl_vfs_ext.h"
#include "pl_resource_ext.h"
#include "pl_asset_ext.h"

// libraries
#include "pl_json.h"
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plLogI*      gptLog      = NULL;
    static const plVfsI*      gptVfs      = NULL;
    static const plResourceI* gptResource = NULL;
    static const plAssetI*    gptAsset    = NULL;
    static const plMemoryI*   gptMemory = NULL;

    #ifndef PL_JSON_ALLOC
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMaterialContext
{
    int a;
} plMaterialContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plMaterialContext* gptMaterialCtx = NULL;

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
        .fEmissiveStrength     = 1.0f,
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
            .fRoughness = 0.0f
        },
        .atTextures = {0}
    };
    *ptMaterial = tMaterialDefault;
}

void
pl_material_serialize(const char* pcName, const plMaterial* ptMaterial)
{
    plJsonObject* ptRoot = pl_json_new_root_object("root");
    pl_json_add_string_member(ptRoot, "format", "plmaterial");
    plVersion tMaterialVersion = plMaterialI_version;
    uint32_t auVersion[] = {tMaterialVersion.uMajor, tMaterialVersion.uMinor};
    pl_json_add_uint_array(ptRoot, "version", auVersion, 2);

    switch(ptMaterial->eMaterialModel)
    {
        case PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS:
            pl_json_add_string_member(ptRoot, "model", "pbr_metallic_roughness");
            break;

        default:
            PL_ASSERT(false && "unknown material model");
            break;

    }

    // base material stuff
    pl_json_add_float_array(ptRoot, "base_color", ptMaterial->tBaseColor.d, 4);
    pl_json_add_float_member(ptRoot, "metalness", ptMaterial->fMetalness);
    pl_json_add_float_member(ptRoot, "roughness", ptMaterial->fRoughness);
    pl_json_add_float_member(ptRoot, "ior", ptMaterial->fIor);
    pl_json_add_float_member(ptRoot, "normal_map_strength", ptMaterial->fNormalMapStrength);
    pl_json_add_float_member(ptRoot, "occlusion_strength", ptMaterial->fOcclusionStrength);
    pl_json_add_bool_member(ptRoot, "double_sided", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED));

    // alpha stuff
    plJsonObject* ptAlpha = pl_json_add_member(ptRoot, "alpha");
    switch(ptMaterial->eAlphaMode)
    {
        case PL_MATERIAL_ALPHA_MODE_MASK:
            pl_json_add_string_member(ptAlpha, "mode", "mask");
            break;

        case PL_MATERIAL_ALPHA_MODE_BLEND:
            pl_json_add_string_member(ptAlpha, "mode", "blend");
            break;
        
        case PL_MATERIAL_ALPHA_MODE_OPAQUE:
        default:
            pl_json_add_string_member(ptAlpha, "mode", "opaque");
            break;
    }
    pl_json_add_float_member(ptAlpha, "cutoff", ptMaterial->fAlphaCutoff);

    // clearcoat stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_CLEARCOAT)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "clearcoat");
        pl_json_add_float_member(ptAdvanced, "factor", ptMaterial->tClearcoat.fFactor);
        pl_json_add_float_member(ptAdvanced, "roughness", ptMaterial->tClearcoat.fRoughness);
    }

    // sheen stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_SHEEN)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "sheen");
        pl_json_add_float_array(ptAdvanced, "color", ptMaterial->tSheen.tColor.d, 3);
        pl_json_add_float_member(ptAdvanced, "roughness", ptMaterial->tSheen.fRoughness);
    }

    // iridescence stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_IRIDESCENCE)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "iridescence");
        pl_json_add_float_member(ptAdvanced, "factor", ptMaterial->tIridescence.fFactor);
        pl_json_add_float_member(ptAdvanced, "ior", ptMaterial->tIridescence.fIor);
        pl_json_add_float_member(ptAdvanced, "thickness_min", ptMaterial->tIridescence.fThicknessMin);
        pl_json_add_float_member(ptAdvanced, "thickness_max", ptMaterial->tIridescence.fThicknessMax);
    }

    // anisotropy stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_ANISOTROPY)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "anisotropy");
        pl_json_add_float_member(ptAdvanced, "strength", ptMaterial->tAnisotropy.fStrength);
        pl_json_add_float_member(ptAdvanced, "rotation", ptMaterial->tAnisotropy.fRotation);
    }

    // transmission stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_TRANSMISSION)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "transmission");
        pl_json_add_float_member(ptAdvanced, "factor", ptMaterial->tTransmission.fFactor);
    }

    // volume stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_VOLUME)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "volume");
        pl_json_add_float_member(ptAdvanced, "thickness", ptMaterial->tVolume.fThickness);
        pl_json_add_float_member(ptAdvanced, "attenuation_distance", ptMaterial->tVolume.fAttenuationDistance);
        pl_json_add_float_array(ptAdvanced, "attenuation_color", ptMaterial->tVolume.tAttenuationColor.d, 3);
    }

    // dispersion stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DISPERSION)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "dispersion");
        pl_json_add_float_member(ptAdvanced, "dispersion", ptMaterial->tDispersion.fDispersion);
    }

    // diffuse transmission stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
    {
        plJsonObject* ptAdvanced = pl_json_add_member(ptRoot, "diffuse_transmission");
        pl_json_add_float_member(ptAdvanced, "factor", ptMaterial->tDiffuseTransmission.fFactor);
        pl_json_add_float_array(ptAdvanced, "color", ptMaterial->tDiffuseTransmission.tColor.d, 3);
    }

    // emissive stuff
    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_EMISSIVE)
    {
        plJsonObject* ptEmissive = pl_json_add_member(ptRoot, "emissive");
        pl_json_add_float_array(ptEmissive, "color", ptMaterial->tEmissiveColor.d, 3);
        pl_json_add_float_member(ptEmissive, "strength", ptMaterial->fEmissiveStrength);
    }

    // textures stuff
    uint32_t uTextureCount = 0;
    for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
    {
        if(gptAsset->is_valid(ptMaterial->atTextures[i].tTexture))
            uTextureCount++;
    }

    if(uTextureCount > 0)
    {
        plJsonObject* ptTextures = pl_json_add_member(ptRoot, "textures");
        for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
        {
            if(gptAsset->is_valid(ptMaterial->atTextures[i].tTexture))
            {
                plJsonObject* ptTexture = pl_json_add_member(ptTextures, gapcTextureSlotNames[i]);
                pl_json_add_string_member(ptTexture, "resource", gptAsset->get_name(ptMaterial->atTextures[i].tTexture));
                pl_json_add_uint_member(ptTexture, "uv_set", ptMaterial->atTextures[i].uUVSet);
                pl_json_add_float_array(ptTexture, "scale", ptMaterial->atTextures[i].tScale.d, 2);
                pl_json_add_float_array(ptTexture, "offset", ptMaterial->atTextures[i].tOffset.d, 2);
                pl_json_add_float_member(ptTexture, "rotation", ptMaterial->atTextures[i].fRotation);
            }
        }
    }

    uint32_t uBufferSize = 0;
    pl_write_json(ptRoot, NULL, &uBufferSize);
    char* pcBuffer = PL_ALLOC(uBufferSize);
    memset(pcBuffer, 0, uBufferSize);
    pl_write_json(ptRoot, pcBuffer, &uBufferSize);
    
    gptVfs->register_file(pcName, false);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
    gptVfs->close_file(tFileHandle);

    PL_FREE(pcBuffer);
    pl_unload_json(&ptRoot);
}

bool
pl_material_load(const char* pcName, plMaterial* ptMaterial)
{
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
    pl_load_json((const char*)puFileBuffer, &ptRoot);

    plVersion tMaterialVersion = plMaterialI_version;
    uint32_t auVersion[2] = {0};
    pl_json_uint_array_member(ptRoot, "version", auVersion, NULL);

    pl_material_init(ptMaterial);

    // base material stuff
    pl_json_float_array_member(ptRoot, "base_color", ptMaterial->tBaseColor.d, NULL);
    ptMaterial->fMetalness = pl_json_float_member(ptRoot, "metalness", ptMaterial->fMetalness);
    ptMaterial->fRoughness = pl_json_float_member(ptRoot, "roughness", ptMaterial->fRoughness);
    ptMaterial->fIor = pl_json_float_member(ptRoot, "ior", ptMaterial->fIor);
    ptMaterial->fNormalMapStrength = pl_json_float_member(ptRoot, "normal_map_strength", ptMaterial->fNormalMapStrength);
    ptMaterial->fOcclusionStrength = pl_json_float_member(ptRoot, "occlusion_strength", ptMaterial->fOcclusionStrength);

    if(pl_json_bool_member(ptRoot, "double_sided", (bool)(ptMaterial->eFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)))
        ptMaterial->eFlags |= PL_MATERIAL_FLAG_DOUBLE_SIDED;

    // alpha stuff
    plJsonObject* ptAlpha = pl_json_member(ptRoot, "alpha");
    if(ptAlpha)
    {
        ptMaterial->fAlphaCutoff = pl_json_float_member(ptAlpha, "cutoff", ptMaterial->fAlphaCutoff);
    
        strncpy(acTempBuffer, "opaque", 256);
        pl_json_string_member(ptAlpha, "mode", acTempBuffer, 256);
        if     (acTempBuffer[0] == 'm') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_MASK;
        else if(acTempBuffer[0] == 'b') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_BLEND;
        else if(acTempBuffer[0] == 'o') ptMaterial->eAlphaMode = PL_MATERIAL_ALPHA_MODE_OPAQUE;
    }

    // clearcoat stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "clearcoat");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_CLEARCOAT;
            ptMaterial->tClearcoat.fFactor = pl_json_float_member(ptAdvanced, "factor", ptMaterial->tClearcoat.fFactor);
            ptMaterial->tClearcoat.fRoughness = pl_json_float_member(ptAdvanced, "roughness", ptMaterial->tClearcoat.fRoughness);
        }
    }

    // sheen stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "sheen");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_SHEEN;
            pl_json_float_array_member(ptAdvanced, "color", ptMaterial->tSheen.tColor.d, NULL);
            ptMaterial->tSheen.fRoughness = pl_json_float_member(ptAdvanced, "roughness", ptMaterial->tSheen.fRoughness);
        }
    }

    // iridescence stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "iridescence");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_IRIDESCENCE;
            ptMaterial->tIridescence.fFactor = pl_json_float_member(ptAdvanced, "factor", ptMaterial->tIridescence.fFactor);
            ptMaterial->tIridescence.fIor = pl_json_float_member(ptAdvanced, "ior", ptMaterial->tIridescence.fIor);
            ptMaterial->tIridescence.fThicknessMin = pl_json_float_member(ptAdvanced, "thickness_min", ptMaterial->tIridescence.fThicknessMin);
            ptMaterial->tIridescence.fThicknessMax = pl_json_float_member(ptAdvanced, "thickness_max", ptMaterial->tIridescence.fThicknessMax);
        }
    }

    // anisotropy stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "anisotropy");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_ANISOTROPY;
            ptMaterial->tAnisotropy.fStrength = pl_json_float_member(ptAdvanced, "strength", ptMaterial->tAnisotropy.fStrength);
            ptMaterial->tAnisotropy.fRotation = pl_json_float_member(ptAdvanced, "rotation", ptMaterial->tAnisotropy.fRotation);
        }
    }

    // transmission stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "transmission");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_TRANSMISSION;
            ptMaterial->tTransmission.fFactor = pl_json_float_member(ptAdvanced, "strength", ptMaterial->tTransmission.fFactor);
        }
    }

    // volume stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "volume");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_VOLUME;
            ptMaterial->tVolume.fThickness = pl_json_float_member(ptAdvanced, "thickness", ptMaterial->tVolume.fThickness);
            ptMaterial->tVolume.fAttenuationDistance = pl_json_float_member(ptAdvanced, "attenuation_distance", ptMaterial->tVolume.fAttenuationDistance);
            pl_json_float_array_member(ptAdvanced, "attenuation_color", ptMaterial->tVolume.tAttenuationColor.d, NULL);
        }
    }

    // dispersion stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "dispersion");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_DISPERSION;
            ptMaterial->tDispersion.fDispersion = pl_json_float_member(ptAdvanced, "dispersion", ptMaterial->tDispersion.fDispersion);
        }
    }

    // diffuse transmission stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "diffuse_transmission");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION;
            ptMaterial->tDiffuseTransmission.fFactor = pl_json_float_member(ptAdvanced, "factor", ptMaterial->tDiffuseTransmission.fFactor);
            pl_json_float_array_member(ptAdvanced, "color", ptMaterial->tDiffuseTransmission.tColor.d, NULL);
        }
    }

    // emissive stuff
    {
        plJsonObject* ptAdvanced = pl_json_member(ptRoot, "emissive");
        if(ptAdvanced)
        {
            ptMaterial->eFlags |= PL_MATERIAL_FLAG_EMISSIVE;
            ptMaterial->fEmissiveStrength = pl_json_float_member(ptAdvanced, "strength", ptMaterial->fEmissiveStrength);
            pl_json_float_array_member(ptAdvanced, "color", ptMaterial->tEmissiveColor.d, NULL);
        }
    }

    plJsonObject* ptTextures = pl_json_member(ptRoot, "textures");
    if(ptTextures)
    {
        uint32_t uTextureCount = UINT32_MAX;
        pl_json_member_list(ptTextures, NULL, &uTextureCount, NULL);
        for(uint32_t i = 0; i < uTextureCount; i++)
        {
            plJsonObject* ptTexture = pl_json_member_by_index(ptTextures, i);
            const char* pcTextureName = pl_json_get_name(ptTexture);

            plTextureSlot tTextureSlot = 0;
            if     (pcTextureName[0] == 'b') tTextureSlot = PL_TEXTURE_SLOT_BASE_COLOR;
            else if(pcTextureName[0] == 'n') tTextureSlot = PL_TEXTURE_SLOT_NORMAL;
            else if(pcTextureName[0] == 'e') tTextureSlot = PL_TEXTURE_SLOT_EMISSIVE;
            else if(pcTextureName[0] == 'o') tTextureSlot = PL_TEXTURE_SLOT_OCCLUSION;
            else if(pcTextureName[0] == 'm') tTextureSlot = PL_TEXTURE_SLOT_METAL_ROUGHNESS;
            else if(pcTextureName[0] == 'a') tTextureSlot = PL_TEXTURE_SLOT_ANISOTROPY;
            else if(pcTextureName[0] == 'c')
            {
                if     (pcTextureName[10] == 'r') tTextureSlot = PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS;
                else if(pcTextureName[10] == 'n') tTextureSlot = PL_TEXTURE_SLOT_CLEARCOAT_NORMAL;
                else                              tTextureSlot = PL_TEXTURE_SLOT_CLEARCOAT;
            }
            else if(pcTextureName[0] == 's')
            {
                if     (pcTextureName[6] == 'c') tTextureSlot = PL_TEXTURE_SLOT_SHEEN_COLOR;
                else if(pcTextureName[6] == 'r') tTextureSlot = PL_TEXTURE_SLOT_SHEEN_ROUGHNESS;
            }
            else if(pcTextureName[0] == 'i')
            {
                if     (pcTextureName[12] == 't') tTextureSlot = PL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS;
                else                              tTextureSlot = PL_TEXTURE_SLOT_IRIDESCENCE;
            }
            else if(pcTextureName[0] == 't')
            {
                if     (pcTextureName[13] == 'c') tTextureSlot = PL_TEXTURE_SLOT_TRANSMISSION;
                else if(pcTextureName[13] == 't') tTextureSlot = PL_TEXTURE_SLOT_THICKNESS;
            }
            else if(pcTextureName[0] == 'd')
            {
                if     (pcTextureName[21] == 'c') tTextureSlot = PL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION_COLOR;
                else                              tTextureSlot = PL_TEXTURE_SLOT_DIFFUSE_TRANSMISSION;
            }

            ptMaterial->atTextures[tTextureSlot].uUVSet = pl_json_uint_member(ptTexture, "uv_set", ptMaterial->atTextures[tTextureSlot].uUVSet);
            ptMaterial->atTextures[tTextureSlot].fRotation = pl_json_float_member(ptTexture, "rotation", ptMaterial->atTextures[tTextureSlot].fRotation);
            pl_json_float_array_member(ptTexture, "scale", ptMaterial->atTextures[tTextureSlot].tScale.d, NULL);
            pl_json_float_array_member(ptTexture, "offset", ptMaterial->atTextures[tTextureSlot].tOffset.d, NULL);
            pl_json_string_member(ptTexture, "resource", acTempBuffer, 256);
            ptMaterial->atTextures[tTextureSlot].tTexture = gptAsset->load(acTempBuffer);
        }
    }

    PL_FREE(puFileBuffer);
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_material_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plMaterialI tApi = {
        .init = pl_material_init,
        .load = pl_material_load,
        .serialize = pl_material_serialize,
    };
    pl_set_api(ptApiRegistry, plMaterialI, &tApi);

    gptLog      = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVfs      = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptResource = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptMemory   = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAsset    = pl_get_api_latest(ptApiRegistry, plAssetI);

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

#ifndef PL_UNITY_BUILD

    #define PL_JSON_IMPLEMENTATION
    #include "pl_json.h"
    #undef PL_JSON_IMPLEMENTATION

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif