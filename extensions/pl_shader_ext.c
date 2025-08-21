/*
   pl_shader_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data & APIs
// [SECTION] implementation
// [SECTION] script loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"

// libs
#include "pl_string.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_shader_ext.h"
#include "pl_graphics_ext.h"
#include "pl_platform_ext.h" // file api
#include "pl_log_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_vfs_ext.h"

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

    static const plLogI*       gptLog       = NULL;
    static const plScreenLogI* gptScreenLog = NULL;
    static const plVfsI*       gptVfs       = NULL;
#endif

#include "pl_ds.h"

// external
#ifndef PL_OFFLINE_SHADERS_ONLY
#include "shaderc/shaderc.h"

#ifdef PL_INCLUDE_SPIRV_CROSS
#include "spirv_cross/spirv_cross_c.h"
#endif

#endif

//-----------------------------------------------------------------------------
// [SECTION] global data & APIs
//-----------------------------------------------------------------------------

#ifndef PL_OFFLINE_SHADERS_ONLY
#ifdef PL_INCLUDE_SPIRV_CROSS
static spvc_context tSpirvCtx;
#endif
#endif

typedef struct _plShaderContext
{
    plTempAllocator tTempAllocator;
    plTempAllocator tTempAllocator2;
    uint8_t**       sbptShaderBytecodeCache;
    plShaderOptions tDefaultShaderOptions;
    bool            bInitialized;
    uint64_t        uLogChannel;
} plShaderContext;

static plShaderContext* gptShaderCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

#ifndef PL_OFFLINE_SHADERS_ONLY

static shaderc_include_result*
pl_shaderc_include_resolve_fn(void* pUserData, const char* pcRequestedSource, int iType, const char* pcRequestingSource, size_t szIncludeDepth)
{
    plShaderOptions* ptOptions = pUserData;
    const char** apcIncludeDirectories = ptOptions ? ptOptions->apcIncludeDirectories : gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories;
    const uint32_t uIncludeDirectoriesCount = ptOptions ? ptOptions->_uIncludeDirectoriesCount : gptShaderCtx->tDefaultShaderOptions._uIncludeDirectoriesCount;

    shaderc_include_result* ptResult = pl_temp_allocator_alloc(&gptShaderCtx->tTempAllocator, sizeof(shaderc_include_result));
    ptResult->user_data = ptOptions;
    ptResult->source_name = pcRequestedSource;
    ptResult->source_name_length = strlen(pcRequestedSource);

    bool bFound = false;
    for(uint32_t i = 0; i < uIncludeDirectoriesCount; i++)
    {
        char* pcFullSourcePath = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator, "%s%s", apcIncludeDirectories[i], pcRequestedSource);

        if(gptVfs->does_file_exist(pcFullSourcePath))
        {
            size_t szShaderSize = gptVfs->get_file_size_str(pcFullSourcePath);
            uint8_t* puVertexShaderCode = pl_temp_allocator_alloc(&gptShaderCtx->tTempAllocator, szShaderSize);
            plVfsFileHandle tHandle = gptVfs->open_file(pcFullSourcePath, PL_VFS_FILE_MODE_READ);
            gptVfs->read_file(tHandle, puVertexShaderCode, &szShaderSize);
            gptVfs->close_file(tHandle);
            ptResult->content = (const char*)puVertexShaderCode;
            ptResult->content_length = szShaderSize;
            bFound = true;
            break;
        }
    }

    return bFound ? ptResult : NULL;

}

static void
pl_shaderc_include_result_release_fn(void* pUserData, shaderc_include_result* ptIncludeResult)
{
    
}

static void
pl_spvc_error_callback(void* pUserData, const char* pcError)
{
    printf("SPIR-V Cross Error: %s\n", pcError);
}

#endif

static void
pl_cleanup_shader_ext(void)
{
    pl_temp_allocator_free(&gptShaderCtx->tTempAllocator);
    pl_temp_allocator_free(&gptShaderCtx->tTempAllocator2);
}

static bool
pl_initialize_shader_ext(const plShaderOptions* ptShaderOptions)
{

    if(gptShaderCtx->bInitialized)
        return true;

    plLogExtChannelInit tLogInit = {
        .tType = PL_LOG_CHANNEL_TYPE_BUFFER | PL_LOG_CHANNEL_TYPE_CONSOLE
    };
    gptShaderCtx->uLogChannel = gptLog->add_channel("Shader", tLogInit);
    gptShaderCtx->bInitialized = true;

    if(ptShaderOptions->pcCacheOutputDirectory == NULL)
        gptShaderCtx->tDefaultShaderOptions.pcCacheOutputDirectory = "./";
    else
        gptShaderCtx->tDefaultShaderOptions.pcCacheOutputDirectory = ptShaderOptions->pcCacheOutputDirectory;
    gptShaderCtx->tDefaultShaderOptions.tFlags = ptShaderOptions->tFlags;
    if(ptShaderOptions->tFlags & PL_SHADER_FLAGS_AUTO_OUTPUT)
    {
        #ifdef PL_CPU_BACKEND
        #elif defined(PL_METAL_BACKEND)
            gptShaderCtx->tDefaultShaderOptions.tFlags |= PL_SHADER_FLAGS_METAL_OUTPUT;
        #elif defined(PL_VULKAN_BACKEND)
            gptShaderCtx->tDefaultShaderOptions.tFlags |= PL_SHADER_FLAGS_SPIRV_OUTPUT;
        #endif
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_INCLUDE_DEBUG)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "initialized flag PL_SHADER_FLAGS_INCLUDE_DEBUG");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_ALWAYS_COMPILE)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "initialized flag PL_SHADER_FLAGS_ALWAYS_COMPILE");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_NEVER_CACHE)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "initialized flag PL_SHADER_FLAGS_NEVER_CACHE");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_METAL_OUTPUT)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "initialized flag PL_SHADER_FLAGS_METAL_OUTPUT");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_SPIRV_OUTPUT)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "initialized flag PL_SHADER_FLAGS_SPIRV_OUTPUT");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_AUTO_OUTPUT)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "initialized flag PL_SHADER_FLAGS_AUTO_OUTPUT");
    }

    gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories[0] = "./";
    gptShaderCtx->tDefaultShaderOptions._uIncludeDirectoriesCount = 1;

    gptShaderCtx->tDefaultShaderOptions.apcDirectories[0] = "./";
    gptShaderCtx->tDefaultShaderOptions._uDirectoriesCount = 1;

    if(ptShaderOptions)
    {
        for(uint32_t i = 0; i < PL_MAX_SHADER_INCLUDE_DIRECTORIES; i++)
        {
            if(ptShaderOptions->apcIncludeDirectories[i])
            {
                gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories[i + 1] = ptShaderOptions->apcIncludeDirectories[i];
                pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "initialized include directory \"%s\"", ptShaderOptions->apcIncludeDirectories[i]);
            }
            else
                break;
            gptShaderCtx->tDefaultShaderOptions._uIncludeDirectoriesCount++;
        }

        for(uint32_t i = 0; i < PL_MAX_SHADER_DIRECTORIES; i++)
        {
            if(ptShaderOptions->apcDirectories[i])
            {
                gptShaderCtx->tDefaultShaderOptions.apcDirectories[i + 1] = ptShaderOptions->apcDirectories[i];
                pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "initialized directory \"%s\"", ptShaderOptions->apcDirectories[i]);
            }
            else
                break;
            gptShaderCtx->tDefaultShaderOptions._uDirectoriesCount++;
        }
    }
    
    #ifndef PL_OFFLINE_SHADERS_ONLY
    #ifdef PL_INCLUDE_SPIRV_CROSS
    spvc_context_create(&tSpirvCtx);
    spvc_context_set_error_callback(tSpirvCtx, pl_spvc_error_callback, NULL);
    pl_log_info(gptLog, gptShaderCtx->uLogChannel, "initialized SPIRV cross");
    gptScreenLog->add_message_ex(0, 5.0, PL_COLOR_32_CYAN, 1.0f, "%s", "initialized SPIR-V Cross");
    #endif
    #endif
    return true;
}

const plShaderOptions*
pl_shader_get_options(void)
{
    if(!gptShaderCtx->bInitialized)
        return NULL;
    return &gptShaderCtx->tDefaultShaderOptions;
}

static void
pl_shader_set_options(const plShaderOptions* ptShaderOptions)
{

    if(!gptShaderCtx->bInitialized)
        return;

    gptShaderCtx->tDefaultShaderOptions.tFlags = ptShaderOptions->tFlags;
    if(ptShaderOptions->tFlags & PL_SHADER_FLAGS_AUTO_OUTPUT)
    {
        #ifdef PL_CPU_BACKEND
        #elif defined(PL_METAL_BACKEND)
            gptShaderCtx->tDefaultShaderOptions.tFlags |= PL_SHADER_FLAGS_METAL_OUTPUT;
        #elif defined(PL_VULKAN_BACKEND)
            gptShaderCtx->tDefaultShaderOptions.tFlags |= PL_SHADER_FLAGS_SPIRV_OUTPUT;
        #endif
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_INCLUDE_DEBUG)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "set flag PL_SHADER_FLAGS_INCLUDE_DEBUG");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_ALWAYS_COMPILE)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "set flag PL_SHADER_FLAGS_ALWAYS_COMPILE");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_NEVER_CACHE)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "set flag PL_SHADER_FLAGS_NEVER_CACHE");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_METAL_OUTPUT)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "set flag PL_SHADER_FLAGS_METAL_OUTPUT");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_SPIRV_OUTPUT)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "set flag PL_SHADER_FLAGS_SPIRV_OUTPUT");
    }

    if(gptShaderCtx->tDefaultShaderOptions.tFlags & PL_SHADER_FLAGS_AUTO_OUTPUT)
    {
        pl_log_info(gptLog, gptShaderCtx->uLogChannel, "set flag PL_SHADER_FLAGS_AUTO_OUTPUT");
    }

    for(uint32_t i = 0; i < PL_MAX_SHADER_INCLUDE_DIRECTORIES; i++)
        gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories[i] = NULL;
    for(uint32_t i = 0; i < PL_MAX_SHADER_DIRECTORIES; i++)
        gptShaderCtx->tDefaultShaderOptions.apcDirectories[i] = NULL;

    gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories[0] = "./";
    gptShaderCtx->tDefaultShaderOptions._uIncludeDirectoriesCount = 1;

    gptShaderCtx->tDefaultShaderOptions.apcDirectories[0] = "./";
    gptShaderCtx->tDefaultShaderOptions._uDirectoriesCount = 1;

    if(ptShaderOptions)
    {
        for(uint32_t i = 0; i < PL_MAX_SHADER_INCLUDE_DIRECTORIES; i++)
        {
            if(ptShaderOptions->apcIncludeDirectories[i])
            {
                bool bDuplicate = false;
                for(uint32_t j = 0; j < PL_MAX_SHADER_INCLUDE_DIRECTORIES; j++)
                {
                    if(gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories[j])
                    {
                        if(pl_str_equal(gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories[j], ptShaderOptions->apcIncludeDirectories[i]))
                        {
                            bDuplicate = true;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                if(!bDuplicate)
                {
                    gptShaderCtx->tDefaultShaderOptions.apcIncludeDirectories[gptShaderCtx->tDefaultShaderOptions._uIncludeDirectoriesCount] = ptShaderOptions->apcIncludeDirectories[i];
                    pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "set include directory \"%s\"", ptShaderOptions->apcIncludeDirectories[i]);
                    gptShaderCtx->tDefaultShaderOptions._uIncludeDirectoriesCount++;
                }
            }
            else
                break;
        }

        for(uint32_t i = 0; i < PL_MAX_SHADER_DIRECTORIES; i++)
        {
            if(ptShaderOptions->apcDirectories[i])
            {
                bool bDuplicate = false;
                for(uint32_t j = 0; j < PL_MAX_SHADER_DIRECTORIES; j++)
                {
                    if(gptShaderCtx->tDefaultShaderOptions.apcDirectories[j])
                    {
                        if(pl_str_equal(gptShaderCtx->tDefaultShaderOptions.apcDirectories[j], ptShaderOptions->apcDirectories[i]))
                        {
                            bDuplicate = true;
                            break;
                        }
                    }
                    else
                        break;
                }
                if(!bDuplicate)
                {
                    gptShaderCtx->tDefaultShaderOptions.apcDirectories[gptShaderCtx->tDefaultShaderOptions._uDirectoriesCount] = ptShaderOptions->apcDirectories[i];
                    gptShaderCtx->tDefaultShaderOptions._uDirectoriesCount++;
                    pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "set directory \"%s\"", ptShaderOptions->apcDirectories[i]);
                }
            }
            else
                break;
            
        }
    }
}

static void
pl_write_to_disk(const char* pcShader, const plShaderModule* ptModule)
{
    plVfsFileHandle tHandle = gptVfs->open_file(pcShader, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tHandle, ptModule->puCode, ptModule->szCodeSize);
    gptVfs->close_file(tHandle);
    pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "write shader to disk \"%s\"", pcShader);
}

static plShaderModule
pl_read_from_disk(const char* pcShader, const char* pcEntryFunc)
{
    plShaderModule tModule = {0};
    if(pcShader && gptVfs->does_file_exist(pcShader))
    {
        tModule.szCodeSize = gptVfs->get_file_size_str(pcShader);
        plVfsFileHandle tHandle = gptVfs->open_file(pcShader, PL_VFS_FILE_MODE_READ);
        tModule.puCode = PL_ALLOC(tModule.szCodeSize + 1);
        memset(tModule.puCode, 0, tModule.szCodeSize + 1);
        gptVfs->read_file(tHandle, tModule.puCode, &tModule.szCodeSize);
        gptVfs->close_file(tHandle);
        pl_sb_push(gptShaderCtx->sbptShaderBytecodeCache, tModule.puCode);
        tModule.pcEntryFunc = pcEntryFunc;
    }
    pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "read shader from disk \"%s\", %s", pcShader, tModule.szCodeSize > 0 ? "SUCCESS" : "FAIL");
    return tModule;
}

static plShaderModule
pl_compile_glsl(const char* pcShader, const char* pcEntryFunc, plShaderOptions* ptOptions)
{

    plShaderModule tModule = {0};

    pl_log_debug_f(gptLog, gptShaderCtx->uLogChannel, "try to compile: \"%s\"", pcShader);

    #ifndef PL_OFFLINE_SHADERS_ONLY
    shaderc_compiler_t tCompiler = shaderc_compiler_initialize();
    shaderc_compile_options_t tShaderRcOptions = shaderc_compile_options_initialize();

    if(ptOptions && ptOptions != &gptShaderCtx->tDefaultShaderOptions)
    {
        ptOptions->_uIncludeDirectoriesCount = 0;
        ptOptions->_uDirectoriesCount = 0;
        for(uint32_t i = 0; i < PL_MAX_SHADER_INCLUDE_DIRECTORIES; i++)
        {
            if(ptOptions->apcIncludeDirectories[i])
                ptOptions->_uIncludeDirectoriesCount++;
            else
                break;
        }
        for(uint32_t i = 0; i < PL_MAX_SHADER_DIRECTORIES; i++)
        {
            if(ptOptions->apcDirectories[i])
                ptOptions->_uDirectoriesCount++;
            else
                break;
        }
    }

    shaderc_compile_options_set_include_callbacks(tShaderRcOptions, pl_shaderc_include_resolve_fn, pl_shaderc_include_result_release_fn, ptOptions);
    pl_temp_allocator_reset(&gptShaderCtx->tTempAllocator);

    if(ptOptions == NULL)
        ptOptions = &gptShaderCtx->tDefaultShaderOptions;

    const char* pcLocatedShader = NULL;

    for(uint32_t i = 0; i < ptOptions->_uDirectoriesCount; i++)
    {
        pcLocatedShader = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator2, "%s%s", ptOptions->apcDirectories[i], pcShader);
        if(gptVfs->does_file_exist(pcLocatedShader))
        {
            pl_log_debug_f(gptLog, gptShaderCtx->uLogChannel, "found shader: \"%s\"", pcLocatedShader);
            break;
        }
        pcLocatedShader = NULL;
    }

    if(pcLocatedShader == NULL)
        pcLocatedShader = pcShader;

    if(!gptVfs->does_file_exist(pcLocatedShader))
    {
        pl_log_warn_f(gptLog, gptShaderCtx->uLogChannel, "shader not found: \"%s\"", pcLocatedShader);
        pl_temp_allocator_reset(&gptShaderCtx->tTempAllocator2);
        return tModule;
    }
        
    switch(ptOptions->tOptimizationLevel)
    {

        case PL_SHADER_OPTIMIZATION_SIZE:
            shaderc_compile_options_set_optimization_level(tShaderRcOptions, shaderc_optimization_level_size);
            break;
        case PL_SHADER_OPTIMIZATION_PERFORMANCE:
            shaderc_compile_options_set_optimization_level(tShaderRcOptions, shaderc_optimization_level_performance);
            break;
        
        case PL_SHADER_OPTIMIZATION_NONE:
        default:
            shaderc_compile_options_set_optimization_level(tShaderRcOptions, shaderc_optimization_level_zero);
            break;
    }

    if(ptOptions->tFlags & PL_SHADER_FLAGS_INCLUDE_DEBUG)
        shaderc_compile_options_set_generate_debug_info(tShaderRcOptions);

    shaderc_compile_options_add_macro_definition(tShaderRcOptions, "PL_SHADER_CODE", 14, "1", 1);

    for(uint32_t i = 0; i < ptOptions->uMacroDefinitionCount; i++)
    {
        shaderc_compile_options_add_macro_definition(tShaderRcOptions,
            ptOptions->ptMacroDefinitions[i].pcName,
            ptOptions->ptMacroDefinitions[i].szNameLength,
            ptOptions->ptMacroDefinitions[i].pcValue,
            ptOptions->ptMacroDefinitions[i].szValueLength);
    }

    // shaderc_compile_options_set_forced_version_profile(tOptions, 450, shaderc_profile_core);
    
    size_t szShaderSize = gptVfs->get_file_size_str(pcLocatedShader);
    uint8_t* puShaderCode = PL_ALLOC(szShaderSize);
    memset(puShaderCode, 0, szShaderSize);
    plVfsFileHandle tHandle = gptVfs->open_file(pcLocatedShader, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tHandle, puShaderCode, &szShaderSize);
    pl_sb_push(gptShaderCtx->sbptShaderBytecodeCache, puShaderCode);
    gptVfs->close_file(tHandle);

    char acExtension[64] = {0};
    pl_str_get_file_extension(pcShader, acExtension, 64);

    shaderc_shader_kind tShaderKind = 0;
    if(acExtension[0] == 'c')
    {
        shaderc_compile_options_add_macro_definition(tShaderRcOptions, "PL_COMPUTE_CODE", 15, "1", 1);
        tShaderKind = shaderc_glsl_compute_shader;
    }
    else if(acExtension[0] == 'f')
    {
        shaderc_compile_options_add_macro_definition(tShaderRcOptions, "PL_FRAGMENT_CODE", 16, "1", 1);
        tShaderKind = shaderc_glsl_fragment_shader;
    }
    else if(acExtension[0] == 'v')
    {
        shaderc_compile_options_add_macro_definition(tShaderRcOptions, "PL_VERTEX_CODE", 14, "1", 1);
        tShaderKind = shaderc_glsl_vertex_shader;
    }
    else
    {
        PL_ASSERT("unknown glsl shader type");
    }

    shaderc_compilation_result_t tresult = shaderc_compile_into_spv(
        tCompiler,
        (const char*)puShaderCode,
        szShaderSize,
        tShaderKind,
        pcLocatedShader,
        pcEntryFunc,
        tShaderRcOptions);

    size_t uNumErrors = shaderc_result_get_num_errors(tresult);
    if(uNumErrors)
    {
        gptScreenLog->add_message_ex(0, 30.0, PL_COLOR_32_RED, 1.25f,"\"%s\" compilation errors: \"%s\"", pcShader, shaderc_result_get_error_message(tresult));
        pl_log_error_f(gptLog, gptShaderCtx->uLogChannel, "\"%s\" compilation errors: \"%s\"", pcShader, shaderc_result_get_error_message(tresult));
        tModule.szCodeSize = 0;
        tModule.puCode = NULL;
        tModule.pcEntryFunc = NULL;
    }
    else
    {
        gptScreenLog->add_message_ex(0, 3.0, PL_COLOR_32_CYAN, 1.0f, "compiled: \"%s\"", pcShader);
        pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "compiled: \"%s\"", pcShader);
        tModule.szCodeSize = shaderc_result_get_length(tresult);
        tModule.puCode = (uint8_t*)shaderc_result_get_bytes(tresult);
        tModule.pcEntryFunc = pcEntryFunc;
    }

    #ifdef PL_INCLUDE_SPIRV_CROSS
    if(ptOptions->tFlags & PL_SHADER_FLAGS_METAL_OUTPUT)
    {
        pl_log_info_f(gptLog, gptShaderCtx->uLogChannel, "cross compiling \"%s\" to MSL", pcShader);
        if(tShaderKind == shaderc_glsl_vertex_shader)
        {
            spvc_parsed_ir ir = NULL;
            spvc_context_parse_spirv(tSpirvCtx, (const SpvId*)tModule.puCode, tModule.szCodeSize / sizeof(SpvId) , &ir);

            spvc_compiler tMslCompiler;
            spvc_context_create_compiler(tSpirvCtx, SPVC_BACKEND_MSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &tMslCompiler);

            spvc_compiler_options tOptions;
            spvc_compiler_create_compiler_options(tMslCompiler, &tOptions);

            spvc_compiler_options_set_uint(tOptions, SPVC_COMPILER_OPTION_MSL_VERSION, 30000);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, true);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_ENABLE_DECORATION_BINDING, true);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_FLIP_VERTEX_Y, true);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_FORCE_ACTIVE_ARGUMENT_BUFFER_RESOURCES, true);

            spvc_compiler_rename_entry_point(tMslCompiler, tModule.pcEntryFunc, "vertex_main", SpvExecutionModelVertex);

            const spvc_reflected_resource *list = NULL;
            spvc_resources resources = NULL;
            size_t count = 0;
            spvc_compiler_create_shader_resources(tMslCompiler, &resources);
            spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);

            for (size_t i = 0; i < count; i++)
            {
                if(spvc_compiler_get_decoration(tMslCompiler, list[i].id, SpvDecorationDescriptorSet) == 3)
                {
                    spvc_compiler_msl_add_inline_uniform_block(tMslCompiler, 3, 0);
                    break;
                }
            }

            spvc_compiler_install_compiler_options(tMslCompiler, tOptions);
            spvc_compiler_compile(tMslCompiler, (const char**)&tModule.puCode);
        }
        else if(tShaderKind == shaderc_glsl_fragment_shader)
        {
            spvc_parsed_ir ir = NULL;
            spvc_context_parse_spirv(tSpirvCtx, (const SpvId*)tModule.puCode, tModule.szCodeSize / sizeof(SpvId) , &ir);

            spvc_compiler tMslCompiler;
            spvc_context_create_compiler(tSpirvCtx, SPVC_BACKEND_MSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &tMslCompiler);

            spvc_compiler_options tOptions;
            spvc_compiler_create_compiler_options(tMslCompiler, &tOptions);

            spvc_compiler_options_set_uint(tOptions, SPVC_COMPILER_OPTION_MSL_VERSION, 30000);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, true);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_ENABLE_DECORATION_BINDING, true);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_FORCE_ACTIVE_ARGUMENT_BUFFER_RESOURCES, true);

            spvc_compiler_rename_entry_point(tMslCompiler, tModule.pcEntryFunc, "fragment_main", SpvExecutionModelFragment);

            const spvc_reflected_resource *list = NULL;
            spvc_resources resources = NULL;
            size_t count = 0;
            spvc_compiler_create_shader_resources(tMslCompiler, &resources);
            spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);

            for (size_t i = 0; i < count; i++)
            {
                if(spvc_compiler_get_decoration(tMslCompiler, list[i].id, SpvDecorationDescriptorSet) == 3)
                {
                    spvc_compiler_msl_add_inline_uniform_block(tMslCompiler, 3, 0);
                    break;
                }
            }

            spvc_compiler_install_compiler_options(tMslCompiler, tOptions);
            spvc_compiler_compile(tMslCompiler, (const char**)&tModule.puCode);
        }
        else if(tShaderKind == shaderc_glsl_compute_shader)
        {
            spvc_parsed_ir ir = NULL;
            spvc_context_parse_spirv(tSpirvCtx, (const SpvId*)tModule.puCode, tModule.szCodeSize / sizeof(SpvId) , &ir);

            spvc_compiler tMslCompiler;
            spvc_context_create_compiler(tSpirvCtx, SPVC_BACKEND_MSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &tMslCompiler);

            spvc_compiler_options tOptions;
            spvc_compiler_create_compiler_options(tMslCompiler, &tOptions);

            spvc_compiler_options_set_uint(tOptions, SPVC_COMPILER_OPTION_MSL_VERSION, 30000);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, true);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_ENABLE_DECORATION_BINDING, true);
            spvc_compiler_options_set_bool(tOptions, SPVC_COMPILER_OPTION_MSL_FORCE_ACTIVE_ARGUMENT_BUFFER_RESOURCES, true);

            spvc_compiler_rename_entry_point(tMslCompiler, tModule.pcEntryFunc, "kernel_main", SpvExecutionModelGLCompute);

            const spvc_reflected_resource *list = NULL;
            spvc_resources resources = NULL;
            size_t count = 0;
            spvc_compiler_create_shader_resources(tMslCompiler, &resources);
            spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);

            for (size_t i = 0; i < count; i++)
            {
                if(spvc_compiler_get_decoration(tMslCompiler, list[i].id, SpvDecorationDescriptorSet) == 3)
                {
                    spvc_compiler_msl_add_inline_uniform_block(tMslCompiler, 3, 0);
                    break;
                }
            }

            spvc_compiler_install_compiler_options(tMslCompiler, tOptions);
            spvc_compiler_compile(tMslCompiler, (const char**)&tModule.puCode);
        }
        if(tModule.puCode)
            tModule.szCodeSize = strlen((const char*)tModule.puCode);
    }
    #endif // PL_INCLUDE_SPIRV_CROSS
    pl_temp_allocator_reset(&gptShaderCtx->tTempAllocator2);
    #endif // PL_OFFLINE_SHADERS_ONLY
    return tModule;
}

static plShaderModule
pl_load_glsl(const char* pcShader, const char* pcEntryFunc, const char* pcFile, plShaderOptions* ptOptions)
{
    pl_log_debug_f(gptLog, gptShaderCtx->uLogChannel, "try to load: \"%s\"", pcShader);

    if(ptOptions && ptOptions != &gptShaderCtx->tDefaultShaderOptions)
    {
        ptOptions->_uIncludeDirectoriesCount = 0;
        ptOptions->_uDirectoriesCount = 0;
        for(uint32_t i = 0; i < PL_MAX_SHADER_INCLUDE_DIRECTORIES; i++)
        {
            if(ptOptions->apcIncludeDirectories[i])
                ptOptions->_uIncludeDirectoriesCount++;
            else
                break;
        }
        for(uint32_t i = 0; i < PL_MAX_SHADER_DIRECTORIES; i++)
        {
            if(ptOptions->apcDirectories[i])
                ptOptions->_uDirectoriesCount++;
            else
                break;
        }

        if(ptOptions->tFlags & PL_SHADER_FLAGS_AUTO_OUTPUT)
        {
            #ifdef PL_CPU_BACKEND
            #elif defined(PL_METAL_BACKEND)
                ptOptions->tFlags |= PL_SHADER_FLAGS_METAL_OUTPUT;
            #elif defined(PL_VULKAN_BACKEND)
                ptOptions->tFlags |= PL_SHADER_FLAGS_SPIRV_OUTPUT;
            #endif
        }
        if(ptOptions->pcCacheOutputDirectory == NULL)
            ptOptions->pcCacheOutputDirectory = gptShaderCtx->tDefaultShaderOptions.pcCacheOutputDirectory;
    }

    if(ptOptions == NULL)
        ptOptions = &gptShaderCtx->tDefaultShaderOptions;

    const char* pcCacheFile = pcFile;
    if(pcCacheFile == NULL)
    {
        // char acTempBuffer[1024] = {0};
        const char* pcFileNameOnly = pl_str_get_file_name(pcShader, NULL, 0);

        for(uint32_t i = 0; i < ptOptions->_uDirectoriesCount; i++)
        {
            if(ptOptions->tFlags & PL_SHADER_FLAGS_METAL_OUTPUT)
                pcCacheFile = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator2, "%s%s.metal", ptOptions->apcDirectories[i], pcFileNameOnly);
            else
                pcCacheFile = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator2, "%s%s.spv", ptOptions->apcDirectories[i], pcFileNameOnly);
            if(gptVfs->does_file_exist(pcCacheFile))
            {
                pl_log_debug_f(gptLog, gptShaderCtx->uLogChannel, "cached shader found: \"%s\"", pcCacheFile);
                gptScreenLog->add_message_ex(0, 3.0, PL_COLOR_32_CYAN, 1.0f, "cached shader found: \"%s\"", pcCacheFile);
                break;
            }
            else
                pcCacheFile = NULL;

        }

        if(pcCacheFile == NULL)
        {
            pl_log_debug_f(gptLog, gptShaderCtx->uLogChannel, "no cached shader found for: \"%s\"", pcFileNameOnly);
            if(ptOptions->tFlags & PL_SHADER_FLAGS_METAL_OUTPUT)
                pcCacheFile = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator2, "%s%s.metal", ptOptions->pcCacheOutputDirectory, pcFileNameOnly);
            else
                pcCacheFile = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator2, "%s%s.spv", ptOptions->pcCacheOutputDirectory, pcFileNameOnly);
        }
    }

    plShaderModule tModule = {0};

    // unless overriden, try to load precompiled shader
    if(!(ptOptions->tFlags & PL_SHADER_FLAGS_ALWAYS_COMPILE))
        tModule = pl_read_from_disk(pcCacheFile, pcEntryFunc);

    // no precompiled shader available, compile it ourselves
    if(tModule.szCodeSize == 0)
    {
        const char* pcFileNameOnly = pl_str_get_file_name(pcShader, NULL, 0);
        if(ptOptions->tFlags & PL_SHADER_FLAGS_METAL_OUTPUT)
            pcCacheFile = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator2, "%s%s.metal", ptOptions->pcCacheOutputDirectory, pcFileNameOnly);
        else
            pcCacheFile = pl_temp_allocator_sprintf(&gptShaderCtx->tTempAllocator2, "%s%s.spv", ptOptions->pcCacheOutputDirectory, pcFileNameOnly);

        tModule = pl_compile_glsl(pcShader, pcEntryFunc, ptOptions);
        if(!(ptOptions->tFlags & PL_SHADER_FLAGS_NEVER_CACHE) && tModule.szCodeSize > 0)
            pl_write_to_disk(pcCacheFile, &tModule);
    }
    pl_temp_allocator_reset(&gptShaderCtx->tTempAllocator2);
    return tModule;
}

//-----------------------------------------------------------------------------
// [SECTION] script loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_shader_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plShaderI tApi = {
        .initialize     = pl_initialize_shader_ext,
        .cleanup        = pl_cleanup_shader_ext,
        .set_options    = pl_shader_set_options,
        .get_options    = pl_shader_get_options,
        .load_glsl      = pl_load_glsl,
        .compile_glsl   = pl_compile_glsl,
        .write_to_disk  = pl_write_to_disk,
        .read_from_disk = pl_read_from_disk,
    };
    pl_set_api(ptApiRegistry, plShaderI, &tApi);

    gptLog = pl_get_api_latest(ptApiRegistry, plLogI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptScreenLog = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptVfs = pl_get_api_latest(ptApiRegistry, plVfsI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    if(bReload)
    {
        gptShaderCtx = ptDataRegistry->get_data("plShaderContext");

        #ifndef PL_OFFLINE_SHADERS_ONLY
        #ifdef PL_INCLUDE_SPIRV_CROSS
        tSpirvCtx = *(spvc_context*)ptDataRegistry->get_data("spvc_context");
        #endif
        #endif
    }
    else // first load
    {
        static plShaderContext gtShaderCtx = {0};
        gptShaderCtx = &gtShaderCtx;
        ptDataRegistry->set_data("plShaderContext", gptShaderCtx);

        #ifndef PL_OFFLINE_SHADERS_ONLY
        #ifdef PL_INCLUDE_SPIRV_CROSS
        ptDataRegistry->set_data("spvc_context", &tSpirvCtx);
        #endif
        #endif
    }
}

PL_EXPORT void
pl_unload_shader_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;
        
    const plShaderI* ptApi = pl_get_api_latest(ptApiRegistry, plShaderI);
    ptApiRegistry->remove_api(ptApi);
        
    #ifndef PL_OFFLINE_SHADERS_ONLY
    #ifdef PL_INCLUDE_SPIRV_CROSS
    spvc_context_destroy(tSpirvCtx);
    #endif
    #endif
    for(uint32_t i = 0; i < pl_sb_size(gptShaderCtx->sbptShaderBytecodeCache); i++)
    {
        PL_FREE(gptShaderCtx->sbptShaderBytecodeCache[i]);
    }
    pl_sb_free(gptShaderCtx->sbptShaderBytecodeCache);
    gptShaderCtx = NULL;
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif