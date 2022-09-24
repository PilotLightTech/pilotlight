/*
   metal_app.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] globals
// [SECTION] pl_app_setup
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_render
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "metal_pl.h"
#include "metal_pl_graphics.h"
#include <simd/simd.h>

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct
{
    vector_float2 position; // Positions in pixel space (i.e. a value of 100 indicates 100 pixels from the origin/center) 
    vector_float3 color; // 2D texture coordinate
} plVertex;

typedef struct
{
    float scale;
    vector_uint2 viewportSize;
} AAPLUniforms;

typedef struct
{
    id<MTLRenderPipelineState>  pipelineState;
    id<MTLBuffer>               vertices;
    id<MTLTexture>              depthTarget;
    MTLRenderPassDescriptor*    drawableRenderDescriptor;
    vector_uint2                viewportSize;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static plAppData gAppData = {0};

static const plVertex quadVertices[] =
{
    // Pixel positions, Color coordinates
    { {  250,  -250 },  { 1.f, 0.f, 0.f } },
    { { -250,  -250 },  { 0.f, 1.f, 0.f } },
    { { -250,   250 },  { 0.f, 0.f, 1.f } },

    { {  250,  -250 },  { 1.f, 0.f, 0.f } },
    { { -250,   250 },  { 0.f, 0.f, 1.f } },
    { {  250,   250 },  { 1.f, 0.f, 1.f } },
};

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

void
pl_app_setup()
{
    // create command queue
    gGraphics.cmdQueue = [gDevice.device newCommandQueue];

    // render pass descriptor
    gAppData.drawableRenderDescriptor = [MTLRenderPassDescriptor new];

    // color attachment
    gAppData.drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    gAppData.drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    gAppData.drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 1, 1, 1);

    // depth attachment
    gAppData.drawableRenderDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    gAppData.drawableRenderDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    gAppData.drawableRenderDescriptor.depthAttachment.clearDepth = 1.0;

    // shaders
    NSURL *URL = [NSURL URLWithString:@"pl.metallib"];
    NSError* error0 = nil;
    id<MTLLibrary> shaderLib = [gDevice.device newLibraryWithURL:URL error:&error0];
    if(!shaderLib)
    {
        NSLog(@" ERROR: Couldnt create a default shader library");
        // assert here because if the shader libary isn't loading, nothing good will happen
        return;
    }

    id <MTLFunction> vertexProgram = [shaderLib newFunctionWithName:@"vertexShader"];
    if(!vertexProgram)
    {
        NSLog(@">> ERROR: Couldn't load vertex function from default library");
        return;
    }

    id <MTLFunction> fragmentProgram = [shaderLib newFunctionWithName:@"fragmentShader"];
    if(!fragmentProgram)
    {
        NSLog(@" ERROR: Couldn't load fragment function from default library");
        return;
    }

    // Create a vertex buffer, and initialize it with the vertex data.
    gAppData.vertices = [gDevice.device newBufferWithBytes:quadVertices length:sizeof(quadVertices) options:MTLResourceStorageModeShared];
    gAppData.vertices.label = @"Quad";

    // Create a pipeline state descriptor to create a compiled pipeline state object
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.label                           = @"pl pipeline";
    pipelineDescriptor.vertexFunction                  = vertexProgram;
    pipelineDescriptor.fragmentFunction                = fragmentProgram;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    pipelineDescriptor.depthAttachmentPixelFormat      = MTLPixelFormatDepth32Float;

    // create pipeline
    NSError *error;
    gAppData.pipelineState = [gDevice.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if(!gAppData.pipelineState)
    {
        NSLog(@"ERROR: Failed aquiring pipeline state: %@", error);
        return;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

void
pl_app_shutdown()
{
    // not actually called yet
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

void
pl_app_resize()
{    
    // recreate depth texture
    MTLTextureDescriptor *depthTargetDescriptor = [MTLTextureDescriptor new];
    depthTargetDescriptor.width       = gAppData.viewportSize.x;
    depthTargetDescriptor.height      = gAppData.viewportSize.y;
    depthTargetDescriptor.pixelFormat = MTLPixelFormatDepth32Float;
    depthTargetDescriptor.storageMode = MTLStorageModePrivate;
    depthTargetDescriptor.usage       = MTLTextureUsageRenderTarget;
    gAppData.depthTarget = [gDevice.device newTextureWithDescriptor:depthTargetDescriptor];
    gAppData.drawableRenderDescriptor.depthAttachment.texture = gAppData.depthTarget;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_render
//-----------------------------------------------------------------------------

void
pl_app_render()
{
    gGraphics.currentFrame++;

    // update uniform data
    AAPLUniforms uniforms = {
        .scale = 0.5 + (1.0 + 0.5 * sin(gGraphics.currentFrame * 0.1)),
        .viewportSize = gAppData.viewportSize
    };

    // request command buffer
    id<MTLCommandBuffer> commandBuffer = [gGraphics.cmdQueue commandBuffer];

    // get next drawable
    id<CAMetalDrawable> currentDrawable = [gGraphics.metalLayer nextDrawable];

    if(!currentDrawable)
        return;

    // set colorattachment to next drawable
    gAppData.drawableRenderDescriptor.colorAttachments[0].texture = currentDrawable.texture;

    // create render command encoder
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:gAppData.drawableRenderDescriptor];

    // bind pipeline
    [renderEncoder setRenderPipelineState:gAppData.pipelineState];

    // bind vertex buffer
    [renderEncoder setVertexBuffer:gAppData.vertices offset:0 atIndex:0];

    // update uniform buffer
    [renderEncoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1 ];

    // draw
    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];
}