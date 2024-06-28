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

#include "pilotlight.h"
#include "pl_os.h"
#include "pl_ds.h"

// libs
#include "pl_string.h"
#include "pl_memory.h"

// extensions
#include "pl_shader_ext.h"
#include "pl_graphics_ext.h"

// external
#include "shaderc/shaderc.h"

#ifdef PL_METAL_BACKEND
#include "spirv_cross/spirv_cross_c.h"
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data & APIs
//-----------------------------------------------------------------------------

#ifdef PL_METAL_BACKEND
static spvc_context tSpirvCtx;
#endif

// data
static plTempAllocator tTempAllocator = {0};
static uint8_t** sbptShaderBytecodeCache = NULL;
static plShaderExtInit gtShaderExtInit = {0};

// apis
static const plFileI* gptFile = NULL;

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static shaderc_include_result*
pl_shaderc_include_resolve_fn(void* pUserData, const char* pcRequestedSource, int iType, const char* pcRequestingSource, size_t szIncludeDepth)
{

    char* pcFullSourcePath = pl_temp_allocator_sprintf(&tTempAllocator, "%s%s", gtShaderExtInit.pcIncludeDirectory, pcRequestedSource);
    
    uint32_t uShaderSize = 0;
    gptFile->read(pcFullSourcePath, &uShaderSize, NULL, "rb");
    uint8_t* puVertexShaderCode = PL_ALLOC((size_t)uShaderSize);
    gptFile->read(pcFullSourcePath, &uShaderSize, puVertexShaderCode, "rb");
    pl_temp_allocator_reset(&tTempAllocator);

    shaderc_include_result* result = PL_ALLOC(sizeof(shaderc_include_result));
    result->source_name = pcRequestedSource;
    result->source_name_length = strlen(pcRequestedSource);
    result->content = (const char*)puVertexShaderCode;
    result->content_length = (size_t)uShaderSize;
    result->user_data = pUserData;

    return result;

}

static void
pl_shaderc_include_result_release_fn(void* pUserData, shaderc_include_result* ptIncludeResult)
{
    PL_FREE((char*)ptIncludeResult->content);
    PL_FREE(ptIncludeResult);
}

static void
pl_spvc_error_callback(void* pUserData, const char* pcError)
{
    printf("SPIR-V Cross Error: %s\n", pcError);
}

static void
pl_initialize_shader_ext(const plShaderExtInit* ptInit)
{
    if(gtShaderExtInit.pcIncludeDirectory)
        return;
    
    #ifdef PL_METAL_BACKEND
    spvc_context_create(&tSpirvCtx);
    spvc_context_set_error_callback(tSpirvCtx, pl_spvc_error_callback, NULL);
    #endif
    gtShaderExtInit = *ptInit;
    if(gtShaderExtInit.pcIncludeDirectory == NULL)
    {
        gtShaderExtInit.pcIncludeDirectory = "./";
    }
}

static plShaderModule
pl_compile_glsl(const char* pcShader, const char* pcEntryFunc)
{
    shaderc_compiler_t tCompiler = shaderc_compiler_initialize();
    shaderc_compile_options_t tOptions = shaderc_compile_options_initialize();
    shaderc_compile_options_set_include_callbacks(tOptions, pl_shaderc_include_resolve_fn, pl_shaderc_include_result_release_fn, NULL);
    // shaderc_compile_options_set_optimization_level(tOptions, shaderc_optimization_level_performance);
        
    uint32_t uShaderSize = 0;
    gptFile->read(pcShader, &uShaderSize, NULL, "rb");
    uint8_t* puShaderCode = PL_ALLOC((size_t)uShaderSize);
    memset(puShaderCode, 0, uShaderSize);
    gptFile->read(pcShader, &uShaderSize, puShaderCode, "rb");
    pl_sb_push(sbptShaderBytecodeCache, puShaderCode);

    char acExtension[64] = {0};
    pl_str_get_file_extension(pcShader, acExtension);

    shaderc_shader_kind tShaderKind = 0;
    if(acExtension[0] == 'c')
        tShaderKind = shaderc_glsl_compute_shader;
    else if(acExtension[0] == 'f')
        tShaderKind = shaderc_glsl_fragment_shader;
    else if(acExtension[0] == 'v')
        tShaderKind = shaderc_glsl_vertex_shader;
    else
    {
        PL_ASSERT("unknown glsl shader type");
    }

    shaderc_compilation_result_t tresult = shaderc_compile_into_spv(
        tCompiler,
        (const char*)puShaderCode,
        (size_t)uShaderSize,
        tShaderKind,
        pcShader,
        pcEntryFunc,
        tOptions);

    size_t uNumErrors = shaderc_result_get_num_errors(tresult);
    if(uNumErrors)
    {
        printf("%s\n", shaderc_result_get_error_message(tresult));
    }

    plShaderModule tModule = {
        .szCodeSize = shaderc_result_get_length(tresult),
        .puCode = (uint8_t*)shaderc_result_get_bytes(tresult),
        .pcEntryFunc = pcEntryFunc
    };

    #ifdef PL_METAL_BACKEND
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
            if(pl_str_contains(list[i].name, "PL_DYNAMIC_DATA"))
            {
                spvc_compiler_msl_add_inline_uniform_block(tMslCompiler, spvc_compiler_get_decoration(tMslCompiler, list[i].id, SpvDecorationDescriptorSet), 0);
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
            if(pl_str_contains(list[i].name, "PL_DYNAMIC_DATA"))
            {
                spvc_compiler_msl_add_inline_uniform_block(tMslCompiler, spvc_compiler_get_decoration(tMslCompiler, list[i].id, SpvDecorationDescriptorSet), 0);
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

        spvc_compiler_rename_entry_point(tMslCompiler, tModule.pcEntryFunc, "kernel_main", SpvExecutionModelGLCompute);

        const spvc_reflected_resource *list = NULL;
        spvc_resources resources = NULL;
        size_t count = 0;
        spvc_compiler_create_shader_resources(tMslCompiler, &resources);
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);

        for (size_t i = 0; i < count; i++)
        {
            if(pl_str_contains(list[i].name, "PL_DYNAMIC_DATA"))
            {
                spvc_compiler_msl_add_inline_uniform_block(tMslCompiler, spvc_compiler_get_decoration(tMslCompiler, list[i].id, SpvDecorationDescriptorSet), 0);
                break;
            }
        }

        spvc_compiler_install_compiler_options(tMslCompiler, tOptions);
        spvc_compiler_compile(tMslCompiler, (const char**)&tModule.puCode);
    }
    #endif
    return tModule;
}

//-----------------------------------------------------------------------------
// [SECTION] script loading
//-----------------------------------------------------------------------------

static const plShaderI*
pl_load_shader_api(void)
{
    static const plShaderI tApi = {
        .initialize = pl_initialize_shader_ext,
        .compile_glsl = pl_compile_glsl
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    gptFile = ptApiRegistry->first(PL_API_FILE);



    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_SHADER), pl_load_shader_api());
    else
        ptApiRegistry->add(PL_API_SHADER, pl_load_shader_api());
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
    #ifdef PL_METAL_BACKEND
    spvc_context_destroy(tSpirvCtx);
    #endif
    for(uint32_t i = 0; i < pl_sb_size(sbptShaderBytecodeCache); i++)
    {
        PL_FREE(sbptShaderBytecodeCache[i]);
    }
    pl_sb_free(sbptShaderBytecodeCache);
}
