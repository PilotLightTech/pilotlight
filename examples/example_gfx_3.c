/*
   example_gfx_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates drawing extension (2D & 3D)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper function declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper function definitions
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pl.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#include "pl_ds.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_starter_ext.h"

// globe stuff
#include "pl_mesh_ext.h"
#include "pl_shader_ext.h"
#include "pl_vfs_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCamera
{
    plDVec3 tPos;
    float  fNearZ;
    float  fFarZ;
    float  fFieldOfView;
    float  fAspectRatio;  // width/height
    plMat4 tViewMat;      // cached
    plMat4 tViewMat2;      // cached
    plMat4 tProjMat;      // cached
    plMat4 tProjMat2;      // cached
    plMat4 tTransformMat; // cached
    plMat4 tTransformMat2; // cached

    // rotations
    float fPitch; // rotation about right vector
    float fYaw;   // rotation about up vector
    float fRoll;  // rotation about forward vector

    // direction vectors
    plVec3 _tUpVec;
    plVec3 _tForwardVec;
    plVec3 _tRightVec;
} plCamera;

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // globe testing
    uint32_t       uGlobeIndexCount;
    uint32_t       uGlobeVertexCount;
    plDVec3*       ptGlobeVertexBuffer;
    plVec3*        ptGlobeVertexBuffer0;
    plVec3*        ptGlobeVertexBuffer1;
    uint32_t*      puGlobeIndexBuffer;
    plBufferHandle tGlobeIndexBuffer;
    plBufferHandle tGlobeVertexBuffer0;
    plBufferHandle tGlobeVertexBuffer1;
    plShaderHandle tGlobeShader;
    plShaderHandle tGlobeWireframeShader;

    // globe options
    bool bWireframe;

    // camera
    float fCameraSpeed;
} plAppData;

typedef struct _plTesselationTriangle
{
    plDVec3  atPoints[3];
    uint32_t uDivisionLevel;
} plTesselationTriangle;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*            gptIO            = NULL;
const plWindowI*        gptWindows       = NULL;
const plGraphicsI*      gptGfx           = NULL;
const plDrawI*          gptDraw          = NULL;
const plDrawBackendI*   gptDrawBackend   = NULL;
const plStarterI*       gptStarter       = NULL;
const plMeshBuilderI*   gptMeshBuilder   = NULL;
const plShaderI*        gptShader        = NULL;
const plVfsI*           gptVfs           = NULL;
const plUiI*            gptUi            = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

// camera helpers
void camera_translate(plCamera*, double dDx, double dDy, double dDz);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_update   (plCamera*);
void camera_update2   (plCamera*);

static void
pl__split_double(double dValue, float* ptHighOut, float* ptLowOut)
{
    // if(dValue >= 0.0)
    // {
    //     double dDoubleHigh = floor(dValue / 65536.0) * 65536.0;
    //     *ptHighOut = (float)(dDoubleHigh);
    //     *ptLowOut = (float)(dValue - dDoubleHigh);
    // }
    // else
    // {
    //     double dDoubleHigh = floor(-dValue / 65536.0) * 65536.0;
    //     *ptHighOut = (float)(-dDoubleHigh);
    //     *ptLowOut = (float)(dValue + dDoubleHigh);
    // }
    *ptHighOut = (float)dValue;
    *ptLowOut = (float)(dValue - *ptHighOut);
}

static void
draw_point(plAppData* ptAppData, float fLongitude, float fLatitude, uint32_t uColor)
{

    fLongitude = pl_radiansf(fLongitude);
    fLatitude = pl_radiansf(fLatitude);

    plSphere tSphere = {
        .fRadius = 10000.0f,
    };

    // PL COORDINATES 
    plVec3 N = {
        cosf(fLatitude) * sinf(fLongitude),
        sinf(fLatitude),
        cosf(fLatitude) * cosf(fLongitude)
    };

    float R = 1737400.0;
    float R2 = R * R;

    float v = sqrtf(R2 * pl_square(N.x) + R2 * pl_square(N.y) + R2 * pl_square(N.z));
    plVec3 P = {
        R2 * N.x / v,
        R2 * N.y / v,
        R2 * N.z / v
    };
    tSphere.tCenter = P;
    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist, tSphere, 0, 0, (plDrawSolidOptions){.uColor = uColor});
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);
        
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptMeshBuilder = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));


    ptAppData->bWireframe = false;
    ptAppData->fCameraSpeed = 1000000.0f;

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions (makes their APIs available)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptMeshBuilder = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);


    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we want the starter extension to include a depth buffer
    // when setting up the render pass
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_REVERSE_Z;
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../examples/shaders/"
        },
        .apcDirectories = {
            "../shaders/",
            "../examples/shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE | PL_SHADER_FLAGS_INCLUDE_DEBUG
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // request 3d drawlists
    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    // create camera
    ptAppData->tCamera = (plCamera){
        .tPos         = {1737500.0, 0.0f, 0.0f},
        .fNearZ       = 0.01f,
        .fFarZ        = 5000000.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = 1.0f,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
    };
    camera_update(&ptAppData->tCamera);

    plDevice* ptDevice = gptStarter->get_device();

    uint32_t uSubdivisions = 6;

    plMeshBuilderOptions tOptions = {0};
    plMeshBuilder* ptBuilder = gptMeshBuilder->create(tOptions);

    plDVec3 tP0 = (plDVec3){ 0.0, 1.0, 0.0};
    plDVec3 tP1 = (plDVec3){ 0.0, -1.0 / 3.0, 2.0 * sqrt(2.0) / 3.0};
    plDVec3 tP2 = (plDVec3){-sqrt(6.0) / 3.0, -1.0 / 3.0, -sqrt(2.0) / 3.0};
    plDVec3 tP3 = (plDVec3){ sqrt(6.0) / 3.0, -1.0 / 3.0, -sqrt(2.0) / 3.0};

    // plDVec3 tRadi = {4.0, 3.0, 4.0};
    plDVec3 tRadi = {1737400.0, 1737400.0, 1737400.0};

    plTesselationTriangle* sbtTessTris = NULL;
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP0, tP1, tP2}}));
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP0, tP3, tP1}}));
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP0, tP2, tP3}}));
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP1, tP3, tP2}}));

    while(pl_sb_size(sbtTessTris) > 0)
    {
        plTesselationTriangle tTri = pl_sb_pop(sbtTessTris);

        if(tTri.uDivisionLevel < uSubdivisions)
        {

            plDVec3 tP01 = pl_norm_vec3_d(pl_mul_vec3_scalard(pl_add_vec3_d(tTri.atPoints[0], tTri.atPoints[1]), 0.5));
            plDVec3 tP12 = pl_norm_vec3_d(pl_mul_vec3_scalard(pl_add_vec3_d(tTri.atPoints[1], tTri.atPoints[2]), 0.5));
            plDVec3 tP20 = pl_norm_vec3_d(pl_mul_vec3_scalard(pl_add_vec3_d(tTri.atPoints[2], tTri.atPoints[0]), 0.5));

            plTesselationTriangle tTri0 = {
                .atPoints       = {tP01, tTri.atPoints[1], tP12},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };

            plTesselationTriangle tTri1 = {
                .atPoints       = {tTri.atPoints[0], tP01, tP20},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };

            plTesselationTriangle tTri2 = {
                .atPoints       = {tP20, tP12, tTri.atPoints[2]},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };

            plTesselationTriangle tTri3 = {
                .atPoints       = {tP01, tP12, tP20},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };
            
            pl_sb_push(sbtTessTris, tTri0);
            pl_sb_push(sbtTessTris, tTri1);
            pl_sb_push(sbtTessTris, tTri2);
            pl_sb_push(sbtTessTris, tTri3);
        }
        else
        {
            tTri.atPoints[0] = pl_mul_vec3_d(tTri.atPoints[0], tRadi);
            tTri.atPoints[1] = pl_mul_vec3_d(tTri.atPoints[1], tRadi);
            tTri.atPoints[2] = pl_mul_vec3_d(tTri.atPoints[2], tRadi);
            gptMeshBuilder->add_triangle_double(ptBuilder, tTri.atPoints[2], tTri.atPoints[1], tTri.atPoints[0]);
        }
    }

    pl_sb_free(sbtTessTris);

    // build
    gptMeshBuilder->commit_double(ptBuilder, NULL, NULL, &ptAppData->uGlobeIndexCount, &ptAppData->uGlobeVertexCount);
    ptAppData->puGlobeIndexBuffer = (uint32_t*)malloc(sizeof(uint32_t) * ptAppData->uGlobeIndexCount);
    ptAppData->ptGlobeVertexBuffer = (plDVec3*)malloc(sizeof(plDVec3) * ptAppData->uGlobeVertexCount);
    gptMeshBuilder->commit_double(ptBuilder, ptAppData->puGlobeIndexBuffer, ptAppData->ptGlobeVertexBuffer, &ptAppData->uGlobeIndexCount, &ptAppData->uGlobeVertexCount);
    gptMeshBuilder->cleanup(ptBuilder);

    ptAppData->ptGlobeVertexBuffer0 = (plVec3*)malloc(sizeof(plVec3) * ptAppData->uGlobeVertexCount);
    ptAppData->ptGlobeVertexBuffer1 = (plVec3*)malloc(sizeof(plVec3) * ptAppData->uGlobeVertexCount);

    // split into 2 floats
    for(uint32_t i = 0; i < ptAppData->uGlobeVertexCount; i++)
    {
        pl__split_double(ptAppData->ptGlobeVertexBuffer[i].x, &ptAppData->ptGlobeVertexBuffer0[i].x, &ptAppData->ptGlobeVertexBuffer1[i].x);
        pl__split_double(ptAppData->ptGlobeVertexBuffer[i].y, &ptAppData->ptGlobeVertexBuffer0[i].y, &ptAppData->ptGlobeVertexBuffer1[i].y);
        pl__split_double(ptAppData->ptGlobeVertexBuffer[i].z, &ptAppData->ptGlobeVertexBuffer0[i].z, &ptAppData->ptGlobeVertexBuffer1[i].z);
    }

    // submit to GPU
    plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX,
        .szByteSize  = sizeof(plVec3) * ptAppData->uGlobeVertexCount,
        .pcDebugName = "vertex buffer",
    };

    plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX,
        .szByteSize  = sizeof(uint32_t) * ptAppData->uGlobeIndexCount,
        .pcDebugName = "index buffer",
    };

    ptAppData->tGlobeVertexBuffer0 = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);
    ptAppData->tGlobeVertexBuffer1 = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);
    ptAppData->tGlobeIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, NULL);

    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tGlobeIndexBuffer);

    const plDeviceMemoryAllocation tIndexMemory = gptGfx->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "clipmap index memory");

    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tGlobeVertexBuffer0);

    const plDeviceMemoryAllocation tVertexMemory0 = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "clipmap vertex memory");

    const plDeviceMemoryAllocation tVertexMemory1 = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "clipmap vertex memory");

    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tGlobeIndexBuffer, &tIndexMemory);
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tGlobeVertexBuffer0, &tVertexMemory0);
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tGlobeVertexBuffer1, &tVertexMemory1);


    size_t szMaxBufferSize = tVertexBufferDesc.szByteSize + tIndexBufferDesc.szByteSize;

    // create staging buffer
    plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_STAGING,
        .szByteSize  = szMaxBufferSize,
        .pcDebugName = "staging buffer"
    };

    plBuffer* ptStagingBuffer = NULL;
    plBufferHandle tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptStagingBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_HOST_COHERENT | PL_MEMORY_FLAGS_HOST_VISIBLE,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, tStagingBuffer, &tStagingBufferAllocation);

    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptAppData->puGlobeIndexBuffer, tIndexBufferDesc.szByteSize);
    free(ptAppData->puGlobeIndexBuffer);
    ptAppData->puGlobeIndexBuffer = NULL;
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[tIndexBufferDesc.szByteSize], ptAppData->ptGlobeVertexBuffer0, tVertexBufferDesc.szByteSize);
    free(ptAppData->ptGlobeVertexBuffer0);
    ptAppData->ptGlobeVertexBuffer0 = NULL;


    plCommandBuffer* ptCmdBuffer = gptStarter->get_temporary_command_buffer();

    plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->copy_buffer(ptBlit, tStagingBuffer, ptAppData->tGlobeIndexBuffer, 0, 0, tIndexBufferDesc.szByteSize);
    gptGfx->copy_buffer(ptBlit, tStagingBuffer, ptAppData->tGlobeVertexBuffer0, (uint32_t)tIndexBufferDesc.szByteSize, 0, tVertexBufferDesc.szByteSize);
    gptGfx->end_blit_pass(ptBlit);

    gptStarter->submit_temporary_command_buffer(ptCmdBuffer);

    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[tIndexBufferDesc.szByteSize], ptAppData->ptGlobeVertexBuffer1, tVertexBufferDesc.szByteSize);
    free(ptAppData->ptGlobeVertexBuffer1);
    ptAppData->ptGlobeVertexBuffer1 = NULL;

    ptCmdBuffer = gptStarter->get_temporary_command_buffer();

    ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->copy_buffer(ptBlit, tStagingBuffer, ptAppData->tGlobeVertexBuffer1, (uint32_t)tIndexBufferDesc.szByteSize, 0, tVertexBufferDesc.szByteSize);
    gptGfx->end_blit_pass(ptBlit);

    gptStarter->submit_temporary_command_buffer(ptCmdBuffer);

    gptGfx->queue_buffer_for_deletion(ptDevice, tStagingBuffer);

    plShaderDesc tGlobeShaderDesc = {
        .tVertexShader     = gptShader->load_glsl("solid.vert", "main", NULL, NULL),
        .tPixelShader      = gptShader->load_glsl("solid.frag", "main", NULL, NULL),
        .tRenderPassLayout = gptStarter->get_render_pass_layout(),
        .tMSAASampleCount  = PL_SAMPLE_COUNT_1,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                }
            },
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled   = true,
                .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tColorOp        = PL_BLEND_OP_ADD,
                .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tAlphaOp        = PL_BLEND_OP_ADD
            }
        },
    };
    ptAppData->tGlobeShader = gptGfx->create_shader(ptDevice, &tGlobeShaderDesc);

    plShaderDesc tGlobeWireframeShaderDesc = {
        .tVertexShader     = gptShader->load_glsl("solid.vert", "main", NULL, NULL),
        .tPixelShader      = gptShader->load_glsl("solid.frag", "main", NULL, NULL),
        .tRenderPassLayout = gptStarter->get_render_pass_layout(),
        .tMSAASampleCount  = PL_SAMPLE_COUNT_1,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulWireframe          = 1,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                }
            },
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled   = true,
                .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tColorOp        = PL_BLEND_OP_ADD,
                .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tAlphaOp        = PL_BLEND_OP_ADD
            }
        },
    };
    ptAppData->tGlobeWireframeShader = gptGfx->create_shader(ptDevice, &tGlobeWireframeShaderDesc);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    plDevice* ptDevice = gptStarter->get_device();
    gptGfx->flush_device(ptDevice);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tGlobeIndexBuffer);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tGlobeVertexBuffer0);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tGlobeVertexBuffer1);
    gptGfx->destroy_shader(ptDevice, ptAppData->tGlobeShader);
    gptGfx->destroy_shader(ptDevice, ptAppData->tGlobeWireframeShader);

    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();

    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    // for convience
    plIO* ptIO = gptIO->get_io();

    static const float fCameraRotationSpeed = 0.005f;

    plCamera* ptCamera = &ptAppData->tCamera;

    if(gptIO->is_key_pressed(PL_KEY_P, false))
    {
        plShaderDesc tGlobeShaderDesc = {
            .tVertexShader     = gptShader->load_glsl("solid.vert", "main", NULL, NULL),
            .tPixelShader      = gptShader->load_glsl("solid.frag", "main", NULL, NULL),
            .tRenderPassLayout = gptStarter->get_render_pass_layout(),
            .tMSAASampleCount  = PL_SAMPLE_COUNT_1,
            .tGraphicsState = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_CULL_BACK,
                .ulWireframe          = 0,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
            },
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 3,
                    .atAttributes = {
                        {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                },
                {
                    .uByteStride = sizeof(float) * 3,
                    .atAttributes = {
                        {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .tColorOp        = PL_BLEND_OP_ADD,
                    .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .tAlphaOp        = PL_BLEND_OP_ADD
                }
            },
        };
        gptGfx->queue_shader_for_deletion(gptStarter->get_device(), ptAppData->tGlobeShader);
        ptAppData->tGlobeShader = gptGfx->create_shader(gptStarter->get_device(), &tGlobeShaderDesc);
    }

    // camera space
    if(gptIO->is_key_down(PL_KEY_W)) camera_translate(ptCamera,  0.0f,  0.0f,  ptAppData->fCameraSpeed * ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_S)) camera_translate(ptCamera,  0.0f,  0.0f, -ptAppData->fCameraSpeed* ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_A)) camera_translate(ptCamera, -ptAppData->fCameraSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(gptIO->is_key_down(PL_KEY_D)) camera_translate(ptCamera,  ptAppData->fCameraSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(gptIO->is_key_down(PL_KEY_F)) { camera_translate(ptCamera,  0.0f, -ptAppData->fCameraSpeed * ptIO->fDeltaTime,  0.0f); }
    if(gptIO->is_key_down(PL_KEY_R)) { camera_translate(ptCamera,  0.0f,  ptAppData->fCameraSpeed * ptIO->fDeltaTime,  0.0f); }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        camera_rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    camera_update(ptCamera);
    camera_update2(ptCamera);

    if(gptUi->begin_window("Debug", NULL, 0))
    {
        gptUi->checkbox("Wireframe", &ptAppData->bWireframe);
        gptUi->input_float("Camera Speed", &ptAppData->fCameraSpeed, NULL, 0);
        gptUi->input_float("Camera Far Plane", &ptAppData->tCamera.fFarZ, NULL, 0);
        gptUi->end_window();
    }

    // 3d drawing API usage
    const plMat4 tOrigin = pl_identity_mat4();
    gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 1.1f * 1737400.0f, (plDrawLineOptions){.fThickness = 1000.0f});


    draw_point(ptAppData, 0.0f,  0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, 15.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, 30.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, 45.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, 60.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, 75.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, 90.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, -15.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, -30.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, -45.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, -60.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, -75.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 0.0f, -90.0f, PL_COLOR_32_YELLOW);

    draw_point(ptAppData, 15.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 30.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 45.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 60.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 75.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, 90.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, -15.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, -30.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, -45.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, -60.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, -75.0f, 0.0f, PL_COLOR_32_YELLOW);
    draw_point(ptAppData, -90.0f, 0.0f, PL_COLOR_32_YELLOW);

    // draw_point(ptAppData, -45.0f, -29.0f, PL_COLOR_32_GREEN);
    // draw_point(ptAppData, -135.0f, -29.0f, PL_COLOR_32_GREEN);
    // draw_point(ptAppData, 45.0f, -29.0f, PL_COLOR_32_GREEN);
    // draw_point(ptAppData, 135.0f, -29.0f, PL_COLOR_32_GREEN);

    draw_point(ptAppData, -45.0f, 29.0f, PL_COLOR_32_GREEN);
    draw_point(ptAppData, -135.0f, 29.0f, PL_COLOR_32_GREEN);
    draw_point(ptAppData, 45.0f, 29.0f, PL_COLOR_32_GREEN);
    draw_point(ptAppData, 135.0f, 29.0f, PL_COLOR_32_GREEN);

    float fLongitude = 45.0f;
    float fLatitude = -29.0f;

    fLongitude = pl_radiansf(fLongitude);
    fLatitude = pl_radiansf(fLatitude);

    float R = 1737400.0;
    // gptDraw->add_3d_plane_yz_filled(ptAppData->pt3dDrawlist, (plVec3){0}, R * 4.0f, R * 4.0f, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 0.0f, 0.0f, 0.5f)});

    plVec3 N = {
        cosf(fLatitude) * sinf(fLongitude),
        sinf(fLatitude),
        cosf(fLatitude) * cosf(fLongitude)
    };

    float R2 = R * R;

    float v = sqrtf(R2 * pl_square(N.x) + R2 * pl_square(N.y) + R2 * pl_square(N.z));
    plVec3 P = {
        R2 * N.x / v,
        // R2 * N.y / v,
        // R2 * N.z / v
    };

    gptDraw->add_3d_plane_yz_filled(ptAppData->pt3dDrawlist, P, R * 2.0f, R * 2.0f, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 0.5f)});


    plSphere tSphere = {
        .fRadius = 10000.0f,
        .tCenter = P
    };
    gptDraw->add_3d_sphere_filled(ptAppData->pt3dDrawlist, tSphere, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_WHITE});

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    const plMat4 tMVP2 = pl_mul_mat4(&ptCamera->tProjMat2, &ptCamera->tViewMat2);

    plDevice* ptDevice = gptStarter->get_device();

    plDynamicDataBlock tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(ptDevice);


    plShaderHandle tShader = ptAppData->bWireframe ? ptAppData->tGlobeWireframeShader : ptAppData->tGlobeShader;

    gptGfx->bind_shader(ptEncoder, tShader);
    gptGfx->bind_vertex_buffer(ptEncoder, ptAppData->tGlobeVertexBuffer0);
    gptGfx->bind_vertex_buffers(ptEncoder, 1, 1, &ptAppData->tGlobeVertexBuffer1, NULL);

    typedef struct _plDynamicGlobeData {
        
        plMat4 tCameraViewProjection;
        plVec4 tCameraPosHigh;
        plVec4 tCameraPosLow;
    } plDynamicGlobeData;

    plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &tCurrentDynamicDataBlock);
    plDynamicGlobeData* ptDynamicData = (plDynamicGlobeData*)tDynamicBinding.pcData;

    ptDynamicData->tCameraViewProjection = tMVP;
    pl__split_double(ptCamera->tPos.x, &ptDynamicData->tCameraPosHigh.x, &ptDynamicData->tCameraPosLow.x);
    pl__split_double(ptCamera->tPos.y, &ptDynamicData->tCameraPosHigh.y, &ptDynamicData->tCameraPosLow.y);
    pl__split_double(ptCamera->tPos.z, &ptDynamicData->tCameraPosHigh.z, &ptDynamicData->tCameraPosLow.z);

    gptGfx->bind_graphics_bind_groups(ptEncoder, tShader, 0, 0, NULL, 1, &tDynamicBinding);

    plDrawIndex tDraw = {
        .uInstanceCount = 1,
        .uIndexCount    = ptAppData->uGlobeIndexCount,
        .tIndexBuffer   = ptAppData->tGlobeIndexBuffer
    };

    gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    // submit 3d drawlist
    gptDrawBackend->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &tMVP2,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE | PL_DRAW_FLAG_REVERSE_Z_DEPTH,
        gptGfx->get_swapchain_info(gptStarter->get_swapchain()).tSampleCount);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

static inline float
wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

void
camera_translate(plCamera* ptCamera, double dDx, double dDy, double dDz)
{
    plDVec3 tRightVec = {
        (double)ptCamera->_tRightVec.x,
        (double)ptCamera->_tRightVec.y,
        (double)ptCamera->_tRightVec.z
    };

    plDVec3 tForwardVec = {
        (double)ptCamera->_tForwardVec.x,
        (double)ptCamera->_tForwardVec.y,
        (double)ptCamera->_tForwardVec.z
    };
    ptCamera->tPos = pl_add_vec3_d(ptCamera->tPos, pl_mul_vec3_scalard(tRightVec, dDx));
    ptCamera->tPos = pl_add_vec3_d(ptCamera->tPos, pl_mul_vec3_scalard(tForwardVec, dDz));
    ptCamera->tPos.y += dDy;
}

void
camera_rotate(plCamera* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;

    ptCamera->fYaw = wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

void
camera_update(plCamera* ptCamera)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    // const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});
    const plMat4 tTranslate = pl_identity_mat4();

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat   = pl_mat4t_invert(&ptCamera->tTransformMat);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fNearZ / (ptCamera->fNearZ - ptCamera->fFarZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fNearZ - ptCamera->fFarZ);
    ptCamera->tProjMat.col[3].w = 0.0f;  
}

void
camera_update2(plCamera* ptCamera)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){(float)ptCamera->tPos.x, (float)ptCamera->tPos.y, (float)ptCamera->tPos.z});

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat2 = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat2   = pl_mat4t_invert(&ptCamera->tTransformMat2);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat2   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat2);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat2.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat2.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat2.col[2].z = ptCamera->fNearZ / (ptCamera->fNearZ - ptCamera->fFarZ);
    ptCamera->tProjMat2.col[2].w = 1.0f;
    ptCamera->tProjMat2.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fNearZ - ptCamera->fFarZ);
    ptCamera->tProjMat2.col[3].w = 0.0f;  
}
