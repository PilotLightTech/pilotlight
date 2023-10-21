#include "pl_ds.h"
#include "pl_graphics_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

static plBuffer*
pl__get_buffer(plDevice* ptDevice, plBufferHandle* ptHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(ptHandle->uGeneration != ptGraphics->sbtBufferGenerations[ptHandle->uIndex])
        return NULL;
    return &ptGraphics->sbtBuffersCold[ptHandle->uIndex];
}

static plTexture*
pl__get_texture(plDevice* ptDevice, plTextureHandle* ptHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(ptHandle->uGeneration != ptGraphics->sbtTextureGenerations[ptHandle->uIndex])
        return NULL;
    return &ptGraphics->sbtTexturesCold[ptHandle->uIndex];
}

static plTextureView*
pl__get_texture_view(plDevice* ptDevice, plTextureViewHandle* ptHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(ptHandle->uGeneration != ptGraphics->sbtTextureViewGenerations[ptHandle->uIndex])
        return NULL;
    return &ptGraphics->sbtTextureViewsCold[ptHandle->uIndex];
}

static plBindGroup*
pl__get_bind_group(plDevice* ptDevice, plBindGroupHandle* ptHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(ptHandle->uGeneration != ptGraphics->sbtBindGroupGenerations[ptHandle->uIndex])
        return NULL;
    return &ptGraphics->sbtBindGroupsCold[ptHandle->uIndex];
}

static plShader*
pl__get_shader(plDevice* ptDevice, plShaderHandle* ptHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(ptHandle->uGeneration != ptGraphics->sbtShaderGenerations[ptHandle->uIndex])
        return NULL;
    return &ptGraphics->sbtShadersCold[ptHandle->uIndex];
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

typedef struct _plDeviceAllocatorData
{
    plDevice*                 ptDevice;
    plDeviceAllocationBlock*  sbtBlocks;

    // buddy allocator data
    // char*                   sbcDebugNames;
    // char**                  sbtDebugNamesInDir;
    // plDeviceAllocationNode* sbtNodes;
    // uint32_t                auFreeList[PL_DEVICE_LOCAL_LEVELS];
} plDeviceAllocatorData;

static plDeviceAllocationBlock*
pl_get_allocator_blocks(struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    if(puSizeOut)
    {
        *puSizeOut = pl_sb_size(ptData->sbtBlocks);
    }
    return ptData->sbtBlocks;
}

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------
enum plDrawStreamBits
{
    PL_DRAW_STREAM_BIT_NONE             = 0,
    PL_DRAW_STREAM_BIT_SHADER           = 1 << 0,
    PL_DRAW_STREAM_BIT_BINDGROUP_0      = 1 << 1,
    PL_DRAW_STREAM_BIT_BINDGROUP_1      = 1 << 2,
    PL_DRAW_STREAM_BIT_BINDGROUP_2      = 1 << 3,
    PL_DRAW_STREAM_BIT_INDEX_OFFSET     = 1 << 4,
    PL_DRAW_STREAM_BIT_VERTEX_OFFSET    = 1 << 5,
    PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET   = 1 << 6,
    PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER   = 1 << 7,
    PL_DRAW_STREAM_BIT_INDEX_BUFFER     = 1 << 8,
    PL_DRAW_STREAM_BIT_VERTEX_BUFFER    = 1 << 9,
    PL_DRAW_STREAM_BIT_TRIANGLES        = 1 << 10,
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

    if(ptStream->tCurrentDraw.uBindGroup0 != tDraw.uBindGroup0)
    {
        ptStream->tCurrentDraw.uBindGroup0 = tDraw.uBindGroup0;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_0;
    }

    if(ptStream->tCurrentDraw.uBindGroup1 != tDraw.uBindGroup1)
    {
        ptStream->tCurrentDraw.uBindGroup1 = tDraw.uBindGroup1;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_1;
    }

    if(ptStream->tCurrentDraw.uBindGroup2 != tDraw.uBindGroup2)
    {
        ptStream->tCurrentDraw.uBindGroup2 = tDraw.uBindGroup2;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_2;
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

    pl_sb_push(ptStream->sbtStream, uDirtyMask);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uShaderVariant);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup0);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup1);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup2);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uIndexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uVertexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uDynamicBufferOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uDynamicBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uIndexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uVertexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uTriangleCount);
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