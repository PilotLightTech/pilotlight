#include "pl_ds.h"
#include "pl_graphics_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

typedef struct _plFrameGarbage
{
    plTextureHandle*          sbtTextures;
    plSamplerHandle*          sbtSamplers;
    plBufferHandle*           sbtBuffers;
    plBindGroupHandle*        sbtBindGroups;
    plShaderHandle*           sbtShaders;
    plComputeShaderHandle*    sbtComputeShaders;
    plRenderPassLayoutHandle* sbtRenderPassLayouts;
    plRenderPassHandle*       sbtRenderPasses;
    plDeviceMemoryAllocation* sbtMemory;
} plFrameGarbage;

static plFrameGarbage*
pl__get_frame_garbage(plGraphics* ptGraphics)
{
    return &ptGraphics->sbtGarbage[ptGraphics->uCurrentFrameIndex];
    
}

static size_t
pl__get_data_type_size(plDataType tType)
{
    switch(tType)
    {
        case PL_DATA_TYPE_BOOL:   return sizeof(int);
        case PL_DATA_TYPE_BOOL2:  return 2 * sizeof(int);
        case PL_DATA_TYPE_BOOL3:  return 3 * sizeof(int);
        case PL_DATA_TYPE_BOOL4:  return 4 * sizeof(int);
        
        case PL_DATA_TYPE_FLOAT:  return sizeof(float);
        case PL_DATA_TYPE_FLOAT2: return 2 * sizeof(float);
        case PL_DATA_TYPE_FLOAT3: return 3 * sizeof(float);
        case PL_DATA_TYPE_FLOAT4: return 4 * sizeof(float);

        case PL_DATA_TYPE_UNSIGNED_BYTE:
        case PL_DATA_TYPE_BYTE:  return sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT:
        case PL_DATA_TYPE_SHORT: return sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT:
        case PL_DATA_TYPE_INT:   return sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG:
        case PL_DATA_TYPE_LONG:  return sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE2:
        case PL_DATA_TYPE_BYTE2:  return 2 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT2:
        case PL_DATA_TYPE_SHORT2: return 2 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT2:
        case PL_DATA_TYPE_INT2:   return 2 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG2:
        case PL_DATA_TYPE_LONG2:  return 2 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE3:
        case PL_DATA_TYPE_BYTE3:  return 3 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT3:
        case PL_DATA_TYPE_SHORT3: return 3 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT3:
        case PL_DATA_TYPE_INT3:   return 3 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG3:
        case PL_DATA_TYPE_LONG3:  return 3 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE4:
        case PL_DATA_TYPE_BYTE4:  return 4 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT4:
        case PL_DATA_TYPE_SHORT4: return 4 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT4:
        case PL_DATA_TYPE_INT4:   return 4 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG4:
        case PL_DATA_TYPE_LONG4:  return 4 * sizeof(uint64_t);
    }

    PL_ASSERT(false && "Unsupported data type");
    return 0;
}

static plBuffer*
pl__get_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtBufferGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtBuffersCold[tHandle.uIndex];
}

static plTexture*
pl__get_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtTextureGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtTexturesCold[tHandle.uIndex];
}

static plBindGroup*
pl__get_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtBindGroupGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtBindGroupsCold[tHandle.uIndex];
}

static plShader*
pl__get_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtShaderGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtShadersCold[tHandle.uIndex];
}

static void
pl_queue_buffer_for_deletion(plDevice* ptDevice, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtBuffers, tHandle);
    pl_sb_push(ptGarbage->sbtMemory, ptGraphics->sbtBuffersCold[tHandle.uIndex].tMemoryAllocation);
    ptGraphics->sbtBufferGenerations[tHandle.uIndex]++;
}

static void
pl_queue_texture_for_deletion(plDevice* ptDevice, plTextureHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtTextures, tHandle);
    pl_sb_push(ptGarbage->sbtMemory, ptGraphics->sbtTexturesCold[tHandle.uIndex].tMemoryAllocation);
    ptGraphics->sbtTextureGenerations[tHandle.uIndex]++;
}

static void
pl_queue_render_pass_for_deletion(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtRenderPasses, tHandle);
    ptGraphics->sbtRenderPassGenerations[tHandle.uIndex]++;
}

static void
pl_queue_render_pass_layout_for_deletion(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtRenderPassLayouts, tHandle);
    ptGraphics->sbtRenderPassLayoutGenerations[tHandle.uIndex]++;
}

static void
pl_queue_shader_for_deletion(plDevice* ptDevice, plShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtShaders, tHandle);
    ptGraphics->sbtShaderGenerations[tHandle.uIndex]++;
}

static void
pl_queue_compute_shader_for_deletion(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtComputeShaders, tHandle);
    ptGraphics->sbtComputeShaderGenerations[tHandle.uIndex]++;
}

static void
pl_queue_bind_group_for_deletion(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtBindGroups, tHandle);
    ptGraphics->sbtBindGroupGenerations[tHandle.uIndex]++;
}

static void
pl_queue_sampler_for_deletion(plDevice* ptDevice, plSamplerHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtSamplers, tHandle);
    ptGraphics->sbtSamplerGenerations[tHandle.uIndex]++;
}

static void
pl__register_3d_drawlist(plGraphics* ptGraphics, plDrawList3D* ptDrawlist)
{
    memset(ptDrawlist, 0, sizeof(plDrawList3D));
    ptDrawlist->ptGraphics = ptGraphics;
    pl_sb_push(ptGraphics->sbt3DDrawlists, ptDrawlist);
}

static void
pl__add_3d_triangle_filled(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor)
{

    pl_sb_reserve(ptDrawlist->sbtSolidVertexBuffer, pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) + 3);
    pl_sb_reserve(ptDrawlist->sbtSolidIndexBuffer, pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) + 3);

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);

    uint32_t tU32Color = 0;
    tU32Color = (uint32_t)  (255.0f * tColor.r + 0.5f);
    tU32Color |= (uint32_t) (255.0f * tColor.g + 0.5f) << 8;
    tU32Color |= (uint32_t) (255.0f * tColor.b + 0.5f) << 16;
    tU32Color |= (uint32_t) (255.0f * tColor.a + 0.5f) << 24;

    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP0.x, tP0.y, tP0.z}, tU32Color}));
    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP1.x, tP1.y, tP1.z}, tU32Color}));
    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP2.x, tP2.y, tP2.z}, tU32Color}));

    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 1);
    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 2);
}

static void
pl__add_3d_line(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness)
{
    uint32_t tU32Color = 0;
    tU32Color = (uint32_t)  (255.0f * tColor.r + 0.5f);
    tU32Color |= (uint32_t) (255.0f * tColor.g + 0.5f) << 8;
    tU32Color |= (uint32_t) (255.0f * tColor.b + 0.5f) << 16;
    tU32Color |= (uint32_t) (255.0f * tColor.a + 0.5f) << 24;

    pl_sb_reserve(ptDrawlist->sbtLineVertexBuffer, pl_sb_size(ptDrawlist->sbtLineVertexBuffer) + 4);
    pl_sb_reserve(ptDrawlist->sbtLineIndexBuffer, pl_sb_size(ptDrawlist->sbtLineIndexBuffer) + 6);

    plDrawVertex3DLine tNewVertex0 = {
        {tP0.x, tP0.y, tP0.z},
        -1.0f,
        fThickness,
        1.0f,
        {tP1.x, tP1.y, tP1.z},
        tU32Color
    };

    plDrawVertex3DLine tNewVertex1 = {
        {tP1.x, tP1.y, tP1.z},
        -1.0f,
        fThickness,
        -1.0f,
        {tP0.x, tP0.y, tP0.z},
        tU32Color
    };

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtLineVertexBuffer);
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex0);
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex1);

    tNewVertex0.fDirection = 1.0f;
    tNewVertex1.fDirection = 1.0f;
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex1);
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex0);

    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 1);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 2);

    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 2);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 3);
}

static void
pl__add_3d_point(plDrawList3D* ptDrawlist, plVec3 tP, plVec4 tColor, float fLength, float fThickness)
{
    const plVec3 tVerticies[6] = {
        {  tP.x - fLength / 2.0f,  tP.y, tP.z},
        {  tP.x + fLength / 2.0f,  tP.y, tP.z},
        {  tP.x,  tP.y - fLength / 2.0f, tP.z},
        {  tP.x,  tP.y + fLength / 2.0f, tP.z},
        {  tP.x,  tP.y, tP.z - fLength / 2.0f},
        {  tP.x,  tP.y, tP.z + fLength / 2.0f}
    };

    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[3], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[4], tVerticies[5], tColor, fThickness);
}

static void
pl__add_3d_transform(plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness)
{

    const plVec3 tOrigin = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, 0.0f});
    const plVec3 tXAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){fLength, 0.0f, 0.0f});
    const plVec3 tYAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, fLength, 0.0f});
    const plVec3 tZAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, fLength});

    pl__add_3d_line(ptDrawlist, tOrigin, tXAxis, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, fThickness);
    pl__add_3d_line(ptDrawlist, tOrigin, tYAxis, (plVec4){0.0f, 1.0f, 0.0f, 1.0f}, fThickness);
    pl__add_3d_line(ptDrawlist, tOrigin, tZAxis, (plVec4){0.0f, 0.0f, 1.0f, 1.0f}, fThickness);
}

static void
pl__add_3d_frustum(plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness)
{
    const float fSmallHeight = tanf(fYFov / 2.0f) * fNearZ;
    const float fSmallWidth  = fSmallHeight * fAspect;
    const float fBigHeight   = tanf(fYFov / 2.0f) * fFarZ;
    const float fBigWidth    = fBigHeight * fAspect;

    const plVec3 tVerticies[8] = {
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth,  fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth, -fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth, -fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth,  fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,    fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,   -fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,   -fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,    fBigHeight,   fFarZ})
    };

    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[2], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[3], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[0], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[4], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[4], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[5], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[6], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[7], tVerticies[4], tColor, fThickness);
}

static void
pl__add_3d_centered_box(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness)
{
    const plVec3 tWidthVec  = {fWidth / 2.0f, 0.0f, 0.0f};
    const plVec3 tHeightVec = {0.0f, fHeight / 2.0f, 0.0f};
    const plVec3 tDepthVec  = {0.0f, 0.0f, fDepth / 2.0f};

    const plVec3 tVerticies[8] = {
        {  tCenter.x - fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z + fDepth / 2.0f}
    };

    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[2], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[3], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[0], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[4], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[4], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[5], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[6], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[7], tVerticies[4], tColor, fThickness);
}

static void
pl__add_3d_aabb(plDrawList3D* ptDrawlist, plVec3 tMin, plVec3 tMax, plVec4 tColor, float fThickness)
{

    const plVec3 tVerticies[] = {
        {  tMin.x, tMin.y, tMin.z },
        {  tMax.x, tMin.y, tMin.z },
        {  tMax.x, tMax.y, tMin.z },
        {  tMin.x, tMax.y, tMin.z },
        {  tMin.x, tMin.y, tMax.z },
        {  tMax.x, tMin.y, tMax.z },
        {  tMax.x, tMax.y, tMax.z },
        {  tMin.x, tMax.y, tMax.z },
    };

    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[2], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[3], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[0], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[4], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[4], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[5], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[6], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[7], tVerticies[4], tColor, fThickness);
}

// order of the bezier curve inputs are 0=start, 1=control, 2=ending
static void
pl__add_3d_bezier_quad(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments)
{

    if(uSegments == 0)
        uSegments = 12;

    // set up first point
    plVec3 tVerticies[2] = {(plVec3){0.0, 0.0, 0.0},tP0};

    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        
        const plVec3 p0 = pl_mul_vec3_scalarf(tP0, uu);
        const plVec3 p1 = pl_mul_vec3_scalarf(tP1, (2.0f * u * t)); 
        const plVec3 p2 = pl_mul_vec3_scalarf(tP2, tt); 
        const plVec3 p3 = pl_add_vec3(p0,p1);
        const plVec3 p4 = pl_add_vec3(p2,p3);
        
        // shift and add next point
        tVerticies[0] = tVerticies[1];
        tVerticies[1] = p4;

        pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    }

    // set up last point
    tVerticies[0] = tVerticies[1];
    tVerticies[1] = tP2;
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
}

// order of the bezier curve inputs are 0=start, 1=control 1, 2=control 2, 3=ending
static void
pl__add_3d_bezier_cubic(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments)
{

    if(uSegments == 0)
        uSegments = 12;

    // set up first point
    plVec3 tVerticies[2] = {(plVec3){0.0, 0.0, 0.0},tP0};

    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        const float uuu = uu * u;
        const float ttt = tt * t;
        
        const plVec3 p0 = pl_mul_vec3_scalarf(tP0, uuu);
        const plVec3 p1 = pl_mul_vec3_scalarf(tP1, (3.0f * uu * t)); 
        const plVec3 p2 = pl_mul_vec3_scalarf(tP2, (3.0f * u * tt)); 
        const plVec3 p3 = pl_mul_vec3_scalarf(tP3, (ttt));
        const plVec3 p5 = pl_add_vec3(p0,p1);
        const plVec3 p6 = pl_add_vec3(p2,p3);
        const plVec3 p7 = pl_add_vec3(p5,p6);
        
        // shift and add next point
        tVerticies[0] = tVerticies[1];
        tVerticies[1] = p7;

        pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    }

    // set up last point
    tVerticies[0] = tVerticies[1];
    tVerticies[1] = tP3;
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
}

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plDrawStreamBits
{
    PL_DRAW_STREAM_BIT_NONE             = 0,
    PL_DRAW_STREAM_BIT_SHADER           = 1 << 0,
    PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET   = 1 << 1,
    PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER   = 1 << 2,
    PL_DRAW_STREAM_BIT_BINDGROUP_2      = 1 << 3,
    PL_DRAW_STREAM_BIT_BINDGROUP_1      = 1 << 4,
    PL_DRAW_STREAM_BIT_BINDGROUP_0      = 1 << 5,
    PL_DRAW_STREAM_BIT_INDEX_OFFSET     = 1 << 6,
    PL_DRAW_STREAM_BIT_VERTEX_OFFSET    = 1 << 7,
    PL_DRAW_STREAM_BIT_INDEX_BUFFER     = 1 << 8,
    PL_DRAW_STREAM_BIT_VERTEX_BUFFER    = 1 << 9,
    PL_DRAW_STREAM_BIT_TRIANGLES        = 1 << 10,
    PL_DRAW_STREAM_BIT_INSTANCE_START   = 1 << 11,
    PL_DRAW_STREAM_BIT_INSTANCE_COUNT   = 1 << 12
};

static void
pl_drawstream_cleanup(plDrawStream* ptStream)
{
    memset(&ptStream->tCurrentDraw, 255, sizeof(plDraw)); 
    pl_sb_free(ptStream->sbtStream);
}

static void
pl_drawstream_reset(plDrawStream* ptStream)
{
    memset(&ptStream->tCurrentDraw, 255, sizeof(plDraw));
    ptStream->tCurrentDraw.uIndexBuffer = UINT32_MAX - 1;
    ptStream->tCurrentDraw.uDynamicBufferOffset = 0;
    pl_sb_reset(ptStream->sbtStream);
}

static void
pl_drawstream_draw(plDrawStream* ptStream, plDraw tDraw)
{

    uint32_t uDirtyMask = PL_DRAW_STREAM_BIT_NONE;

    if(ptStream->tCurrentDraw.uShaderVariant != tDraw.uShaderVariant)
    {
        ptStream->tCurrentDraw.uShaderVariant = tDraw.uShaderVariant;
        uDirtyMask |= PL_DRAW_STREAM_BIT_SHADER;
    }

    if(ptStream->tCurrentDraw.uDynamicBufferOffset != tDraw.uDynamicBufferOffset)
    {   
        ptStream->tCurrentDraw.uDynamicBufferOffset = tDraw.uDynamicBufferOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET;
    }

    if(ptStream->tCurrentDraw.uDynamicBuffer != tDraw.uDynamicBuffer)
    {
        ptStream->tCurrentDraw.uDynamicBuffer = tDraw.uDynamicBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER;
    }

    if(ptStream->tCurrentDraw.uBindGroup2 != tDraw.uBindGroup2)
    {
        ptStream->tCurrentDraw.uBindGroup2 = tDraw.uBindGroup2;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_2;
    }

    if(ptStream->tCurrentDraw.uBindGroup1 != tDraw.uBindGroup1)
    {
        ptStream->tCurrentDraw.uBindGroup1 = tDraw.uBindGroup1;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_1;
    }

    if(ptStream->tCurrentDraw.uBindGroup0 != tDraw.uBindGroup0)
    {
        ptStream->tCurrentDraw.uBindGroup0 = tDraw.uBindGroup0;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_0;
    }

    if(ptStream->tCurrentDraw.uIndexOffset != tDraw.uIndexOffset)
    {   
        ptStream->tCurrentDraw.uIndexOffset = tDraw.uIndexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_OFFSET;
    }

    if(ptStream->tCurrentDraw.uVertexOffset != tDraw.uVertexOffset)
    {   
        ptStream->tCurrentDraw.uVertexOffset = tDraw.uVertexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_OFFSET;
    }

    if(ptStream->tCurrentDraw.uIndexBuffer != tDraw.uIndexBuffer)
    {
        ptStream->tCurrentDraw.uIndexBuffer = tDraw.uIndexBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_BUFFER;
    }

    if(ptStream->tCurrentDraw.uVertexBuffer != tDraw.uVertexBuffer)
    {
        ptStream->tCurrentDraw.uVertexBuffer = tDraw.uVertexBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_BUFFER;
    }

    if(ptStream->tCurrentDraw.uTriangleCount != tDraw.uTriangleCount)
    {
        ptStream->tCurrentDraw.uTriangleCount = tDraw.uTriangleCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_TRIANGLES;
    }

    if(ptStream->tCurrentDraw.uInstanceStart != tDraw.uInstanceStart)
    {
        ptStream->tCurrentDraw.uInstanceStart = tDraw.uInstanceStart;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_START;
    }

    if(ptStream->tCurrentDraw.uInstanceCount != tDraw.uInstanceCount)
    {
        ptStream->tCurrentDraw.uInstanceCount = tDraw.uInstanceCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_COUNT;
    }

    pl_sb_push(ptStream->sbtStream, uDirtyMask);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uShaderVariant);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uDynamicBufferOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uDynamicBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup2);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup1);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup0);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uIndexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uVertexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uIndexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uVertexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uTriangleCount);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_START)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uInstanceStart);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uInstanceCount);
}

static const plDrawStreamI*
pl_load_drawstream_api(void)
{
    static const plDrawStreamI tApi = {
        .reset   = pl_drawstream_reset,
        .draw    = pl_drawstream_draw,
        .cleanup = pl_drawstream_cleanup
    };
    return &tApi;
}

static uint32_t
pl__format_stride(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_D32_FLOAT_S8_UINT:  return 5;
        case PL_FORMAT_R32G32B32A32_FLOAT: return 16;
        case PL_FORMAT_R32G32_FLOAT:       return 8;
        case PL_FORMAT_R8G8B8A8_SRGB:
        case PL_FORMAT_B8G8R8A8_SRGB:
        case PL_FORMAT_B8G8R8A8_UNORM:
        case PL_FORMAT_R8G8B8A8_UNORM:     return 4;
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D32_FLOAT:          return 1;
        
    }

    PL_ASSERT(false && "Unsupported format");
    return 0;
}

static void
pl__cleanup_common_graphics(plGraphics* ptGraphics)
{
    for(uint32_t i = 0u; i < pl_sb_size(ptGraphics->sbt3DDrawlists); i++)
    {
        plDrawList3D* drawlist = ptGraphics->sbt3DDrawlists[i];
        pl_sb_free(drawlist->sbtSolidIndexBuffer);
        pl_sb_free(drawlist->sbtSolidVertexBuffer);
        pl_sb_free(drawlist->sbtLineVertexBuffer);
        pl_sb_free(drawlist->sbtLineIndexBuffer);
    }
    pl_sb_free(ptGraphics->sbt3DDrawlists);

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->sbtGarbage); i++)
    {
        plFrameGarbage* ptGarbage = &ptGraphics->sbtGarbage[i];
        pl_sb_free(ptGarbage->sbtMemory);
        pl_sb_free(ptGarbage->sbtTextures);
        pl_sb_free(ptGarbage->sbtBuffers);
        pl_sb_free(ptGarbage->sbtComputeShaders);
        pl_sb_free(ptGarbage->sbtShaders);
        pl_sb_free(ptGarbage->sbtRenderPasses);
        pl_sb_free(ptGarbage->sbtRenderPassLayouts);
        pl_sb_free(ptGarbage->sbtBindGroups);
    }

    pl_sb_free(ptGraphics->sbtGarbage);
    pl_sb_free(ptGraphics->tSwapchain.sbtSwapchainTextureViews);
    pl_sb_free(ptGraphics->sbtShadersCold);
    pl_sb_free(ptGraphics->sbtBuffersCold);
    pl_sb_free(ptGraphics->sbtBufferFreeIndices);
    pl_sb_free(ptGraphics->sbtTexturesCold);
    pl_sb_free(ptGraphics->sbtSamplersCold);
    pl_sb_free(ptGraphics->sbtBindGroupsCold);
    pl_sb_free(ptGraphics->sbtShaderGenerations);
    pl_sb_free(ptGraphics->sbtBufferGenerations);
    pl_sb_free(ptGraphics->sbtTextureGenerations);
    pl_sb_free(ptGraphics->sbtBindGroupGenerations);
    pl_sb_free(ptGraphics->sbtRenderPassesCold);
    pl_sb_free(ptGraphics->sbtRenderPassGenerations);
    pl_sb_free(ptGraphics->sbtTextureFreeIndices);
    pl_sb_free(ptGraphics->sbtRenderPassLayoutsCold);
    pl_sb_free(ptGraphics->sbtComputeShadersCold);
    pl_sb_free(ptGraphics->sbtComputeShaderGenerations);
    pl_sb_free(ptGraphics->sbtRenderPassLayoutGenerations);
    pl_sb_free(ptGraphics->sbtSamplerGenerations);
    pl_sb_free(ptGraphics->sbtBindGroupFreeIndices);
    pl_sb_free(ptGraphics->sbtSemaphoreGenerations);
    pl_sb_free(ptGraphics->sbtSemaphoreFreeIndices);
    pl_sb_free(ptGraphics->sbtSamplerFreeIndices);

    PL_FREE(ptGraphics->_pInternalData);
    PL_FREE(ptGraphics->tDevice._pInternalData);
    PL_FREE(ptGraphics->tSwapchain._pInternalData);
}
