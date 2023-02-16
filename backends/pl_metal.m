/*
   pl_metal.c
*/

/*
Index of this file:
// [SECTION] includes/imports
// [SECTION] structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] MetalBuffer
// [SECTION] FrameBufferDescriptor
// [SECTION] MetalContext
*/

//-----------------------------------------------------------------------------
// [SECTION] includes/imports
//-----------------------------------------------------------------------------

#include "pl_metal.h"
#include "pl_ds.h"
#import <time.h>

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

NS_ASSUME_NONNULL_BEGIN

// A wrapper around a MTLBuffer object that knows the last time it was reused (from Dear ImGui)
@interface MetalBuffer : NSObject
@property (nonatomic, strong) id<MTLBuffer> buffer;
@property (nonatomic, assign) double        lastReuseTime;
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer;
@end

// An object that encapsulates the data necessary to uniquely identify a
// render pipeline state. These are used as cache keys.  (from Dear ImGui)
@interface FramebufferDescriptor : NSObject<NSCopying>
@property (nonatomic, assign) unsigned long  sampleCount;
@property (nonatomic, assign) MTLPixelFormat colorPixelFormat;
@property (nonatomic, assign) MTLPixelFormat depthPixelFormat;
@property (nonatomic, assign) MTLPixelFormat stencilPixelFormat;
- (instancetype)initWithRenderPassDescriptor:(MTLRenderPassDescriptor*)renderPassDescriptor;
@end

// A singleton that stores long-lived objects that are needed by the Metal
// renderer backend. Stores the render pipeline state cache and the default
// font texture, and manages the reusable buffer cache.  (from Dear ImGui)
@interface MetalContext : NSObject
@property (nonatomic, strong) id<MTLDevice>                   device;
@property (nonatomic, strong) id<MTLDepthStencilState>        depthStencilState;
@property (nonatomic, strong) FramebufferDescriptor*          framebufferDescriptor; // framebuffer descriptor for current frame; transient
@property (nonatomic, strong) NSMutableDictionary*            renderPipelineStateCache; // pipeline cache; keyed on framebuffer descriptors
@property (nonatomic, strong) NSMutableDictionary*            renderPipelineStateSDFCache; // pipeline cache; keyed on framebuffer descriptors
@property (nonatomic, strong, nullable) id<MTLTexture>        fontTexture;
@property (nonatomic, strong, nullable) MTLTextureDescriptor* textureDescriptor;
@property (nonatomic, strong) NSMutableArray<MetalBuffer*>*   bufferCache;
@property (nonatomic, assign) double                          lastBufferCachePurge;
- (MetalBuffer*)dequeueReusableBufferOfLength:(NSUInteger)length device:(id<MTLDevice>)device;
- (id<MTLRenderPipelineState>)renderPipelineStateForFramebufferDescriptor:(FramebufferDescriptor*)descriptor device:(id<MTLDevice>)device;
- (id<MTLRenderPipelineState>)renderPipelineStateForFramebufferDescriptorSDF:(FramebufferDescriptor*)descriptor device:(id<MTLDevice>)device;
@end

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

extern void                  pl__cleanup_font_atlas(plFontAtlas* atlas); // in pl_draw.c
extern void                  pl__cleanup_draw_context(plDrawContext* ctx); // in pl_draw.c
extern void                  pl__new_draw_frame(plDrawContext* ctx); // in pl_draw.c
extern void                  pl__build_font_atlas(plFontAtlas* ctx); // in pl_draw.c
static inline CFTimeInterval GetMachAbsoluteTimeInSeconds() { return (CFTimeInterval)(double)clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1e9; }

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_draw_context_metal(plDrawContext* ctx, id<MTLDevice> device)
{
    ctx->_platformData = [[MetalContext alloc] init];
    MetalContext* metalCtx = ctx->_platformData;
    metalCtx.device = device;
}

void
pl_cleanup_draw_context(plDrawContext* ctx)
{
    MetalContext* metalCtx = ctx->_platformData;
    [metalCtx dealloc];
    pl__cleanup_draw_context(ctx);
}

void
pl_new_draw_frame(plDrawContext* ptCtx)
{
    
}

void
pl_new_draw_frame_metal(plDrawContext* ctx, MTLRenderPassDescriptor* renderPassDescriptor)
{
    MetalContext* metalCtx = ctx->_platformData;
    metalCtx.framebufferDescriptor = [[FramebufferDescriptor alloc] initWithRenderPassDescriptor:renderPassDescriptor];
    pl__new_draw_frame(ctx);
}

void
pl_submit_drawlist_metal(plDrawList* drawlist, float width, float height, id<MTLRenderCommandEncoder> renderEncoder)
{

    MetalContext* metalCtx = drawlist->ctx->_platformData;

    // early exit if vertex buffer is empty
    if(pl_sb_size(drawlist->sbVertexBuffer) == 0u)
        return;

    // ensure gpu vertex buffer size is adequate
    size_t vertexBufferLength = (size_t)pl_sb_size(drawlist->sbVertexBuffer) * sizeof(plDrawVertex);
    size_t indexBufferLength = (size_t)drawlist->indexBufferByteSize;
    MetalBuffer* vertexBuffer = [metalCtx dequeueReusableBufferOfLength:vertexBufferLength device:metalCtx.device];
    MetalBuffer* indexBuffer = [metalCtx dequeueReusableBufferOfLength:indexBufferLength device:metalCtx.device];

    // copy vertex data to gpu
    memcpy(vertexBuffer.buffer.contents, drawlist->sbVertexBuffer, sizeof(plDrawVertex) * pl_sb_size(drawlist->sbVertexBuffer));

    // index GPU data transfer
    uint32_t uTempIndexBufferOffset = 0u;  
    uint32_t globalIdxBufferIndexOffset = 0u;

    for(uint32_t i = 0u; i < pl_sb_size(drawlist->sbSubmittedLayers); i++)
    {
        plDrawCommand* lastCommand = NULL;
        plDrawLayer* layer = drawlist->sbSubmittedLayers[i];

        unsigned char* destination = indexBuffer.buffer.contents;
        memcpy(&destination[uTempIndexBufferOffset], layer->sbIndexBuffer, sizeof(uint32_t) * pl_sb_size(layer->sbIndexBuffer));

        uTempIndexBufferOffset += pl_sb_size(layer->sbIndexBuffer) * sizeof(uint32_t);

        // attempt to merge commands
        for(uint32_t j = 0u; j < pl_sb_size(layer->sbCommandBuffer); j++)
        {
            plDrawCommand *layerCommand = &layer->sbCommandBuffer[j];
            bool bCreateNewCommand = true;

            if(lastCommand)
            {
                // check for same texture (allows merging draw calls)
                if(lastCommand->textureId == layerCommand->textureId && lastCommand->sdf == layerCommand->sdf)
                {
                    lastCommand->elementCount += layerCommand->elementCount;
                    bCreateNewCommand = false;
                }

                // check for same clipping (allows merging draw calls)
                if(layerCommand->tClip.tMax.x != lastCommand->tClip.tMax.x || layerCommand->tClip.tMax.y != lastCommand->tClip.tMax.y ||
                    layerCommand->tClip.tMin.x != lastCommand->tClip.tMin.x || layerCommand->tClip.tMin.y != lastCommand->tClip.tMin.y)
                {
                    bCreateNewCommand = true;
                }
            }

            if(bCreateNewCommand)
            {
                layerCommand->indexOffset = globalIdxBufferIndexOffset + layerCommand->indexOffset;
                pl_sb_push(drawlist->sbDrawCommands, *layerCommand);       
                lastCommand = layerCommand;
  
            }
            
        }    
        globalIdxBufferIndexOffset += pl_sb_size(layer->sbIndexBuffer);    
    }
    
    // Try to retrieve a render pipeline state that is compatible with the framebuffer config for this frame
    // The hit rate for this cache should be very near 100%.
    id<MTLRenderPipelineState> renderPipelineState = metalCtx.renderPipelineStateCache[metalCtx.framebufferDescriptor];
    if (renderPipelineState == nil)
    {
        // No luck; make a new render pipeline state
        renderPipelineState = [metalCtx renderPipelineStateForFramebufferDescriptor:metalCtx.framebufferDescriptor device:metalCtx.device];

        // Cache render pipeline state for later reuse
        metalCtx.renderPipelineStateCache[metalCtx.framebufferDescriptor] = renderPipelineState;
    }

    id<MTLRenderPipelineState> renderPipelineStateSDF = metalCtx.renderPipelineStateSDFCache[metalCtx.framebufferDescriptor];
    if (renderPipelineStateSDF == nil)
    {
        // No luck; make a new render pipeline state
        renderPipelineStateSDF = [metalCtx renderPipelineStateForFramebufferDescriptorSDF:metalCtx.framebufferDescriptor device:metalCtx.device];

        // Cache render pipeline state for later reuse
        metalCtx.renderPipelineStateSDFCache[metalCtx.framebufferDescriptor] = renderPipelineStateSDF;
    }

    // update uniform buffer
    float L = 0.0f;
    float R = width;
    float T = 0.0f;
    float B = height;
    float N = 0.0f;
    float F = 1.0f;
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,   0.0f },
        { 0.0f,         0.0f,        1/(F-N),   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T), N/(F-N),   1.0f },
    };
    [renderEncoder setVertexBytes:&ortho_projection length:sizeof(ortho_projection) atIndex:1 ];
    [renderEncoder setDepthStencilState:metalCtx.depthStencilState];
    [renderEncoder setVertexBuffer:vertexBuffer.buffer offset:0 atIndex:0];

    bool sdf = false;
    const plVec2 tClipScale = drawlist->ctx->tFrameBufferScale;
    [renderEncoder setRenderPipelineState:renderPipelineState];
    for(uint32_t i = 0u; i < pl_sb_size(drawlist->sbDrawCommands); i++)
    {
        plDrawCommand cmd = drawlist->sbDrawCommands[i];

        if(cmd.sdf && !sdf)
        {
            [renderEncoder setRenderPipelineState:renderPipelineStateSDF];
            sdf = true;
        }
        else if(!cmd.sdf && sdf)
        {
            [renderEncoder setRenderPipelineState:renderPipelineState];
            sdf = false;
        }

        if(pl_rect_width(&cmd.tClip) == 0)
        {
            MTLScissorRect tScissorRect = {
                .x      = (NSUInteger)(0),
                .y      = (NSUInteger)(0),
                .width  = (NSUInteger)(width * tClipScale.x),
                .height = (NSUInteger)(height * tClipScale.y)
            };
            [renderEncoder setScissorRect:tScissorRect];
        }
        else
        {
            // clamp to viewport
            if (cmd.tClip.tMin.x < 0.0f)   { cmd.tClip.tMin.x = 0.0f; }
            if (cmd.tClip.tMin.y < 0.0f)   { cmd.tClip.tMin.y = 0.0f; }
            if (cmd.tClip.tMax.x > width)  { cmd.tClip.tMax.x = (float)width; }
            if (cmd.tClip.tMax.y > height) { cmd.tClip.tMax.y = (float)height; }
            if (cmd.tClip.tMax.x <= cmd.tClip.tMin.x || cmd.tClip.tMax.y <= cmd.tClip.tMin.y)
                continue;

            MTLScissorRect tScissorRect = {
                .x      = (NSUInteger)(cmd.tClip.tMin.x < 0 ? 0 : cmd.tClip.tMin.x),
                .y      = (NSUInteger)(cmd.tClip.tMin.y < 0 ? 0 : cmd.tClip.tMin.y),
                .width  = (NSUInteger)pl_rect_width(&cmd.tClip),
                .height = (NSUInteger)pl_rect_height(&cmd.tClip)
            };
            [renderEncoder setScissorRect:tScissorRect];
        }

        [renderEncoder setFragmentTexture:cmd.textureId atIndex:2];
        [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:cmd.elementCount indexType:MTLIndexTypeUInt32 indexBuffer:indexBuffer.buffer indexBufferOffset:cmd.indexOffset * sizeof(uint32_t)];
    }
}

void
pl_build_font_atlas(plDrawContext* ctx, plFontAtlas* atlas)
{
    pl__build_font_atlas(atlas);
    atlas->ctx = ctx;
    ctx->fontAtlas = atlas;

    MetalContext* metalCtx = ctx->_platformData;

    // create font atlas texture
    metalCtx.textureDescriptor = [[MTLTextureDescriptor alloc] init];

    // Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
    // an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
    metalCtx.textureDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;

    // Set the pixel dimensions of the texture
    metalCtx.textureDescriptor.width = atlas->atlasSize[0];
    metalCtx.textureDescriptor.height = atlas->atlasSize[1];

    // Create the texture from the device by using the descriptor
    metalCtx.fontTexture = [metalCtx.device newTextureWithDescriptor:metalCtx.textureDescriptor];  

    MTLRegion region = {
        { 0, 0, 0 }, // MTLOrigin
        {atlas->atlasSize[0], atlas->atlasSize[1], 1}    // MTLSize
    };

    NSUInteger bytesPerRow = 4 * atlas->atlasSize[0];

    [metalCtx.fontTexture replaceRegion:region
                mipmapLevel:0
                withBytes:atlas->pixelsAsRGBA32
                bytesPerRow:bytesPerRow];

    ctx->fontAtlas->texture = metalCtx.fontTexture;
}

void
pl_cleanup_font_atlas(plFontAtlas* atlas)
{
    MetalContext* metalCtx = atlas->ctx->_platformData;
    [metalCtx.textureDescriptor dealloc];
    pl__cleanup_font_atlas(atlas);
}

//-----------------------------------------------------------------------------
// [SECTION] MetalBuffer
//-----------------------------------------------------------------------------

@implementation MetalBuffer
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer
{
    if ((self = [super init]))
    {
        _buffer = buffer;
        _lastReuseTime = GetMachAbsoluteTimeInSeconds();
    }
    return self;
}
@end

//-----------------------------------------------------------------------------
// [SECTION] FrameBufferDescriptor
//-----------------------------------------------------------------------------

@implementation FramebufferDescriptor
- (instancetype)initWithRenderPassDescriptor:(MTLRenderPassDescriptor*)renderPassDescriptor
{
    if ((self = [super init]))
    {
        _sampleCount = renderPassDescriptor.colorAttachments[0].texture.sampleCount;
        _colorPixelFormat = renderPassDescriptor.colorAttachments[0].texture.pixelFormat;
        _depthPixelFormat = renderPassDescriptor.depthAttachment.texture.pixelFormat;
        _stencilPixelFormat = renderPassDescriptor.stencilAttachment.texture.pixelFormat;
    }
    return self;
}

- (id)copyWithZone:(nullable NSZone*)zone
{
    FramebufferDescriptor* copy = [[FramebufferDescriptor allocWithZone:zone] init];
    copy.sampleCount = self.sampleCount;
    copy.colorPixelFormat = self.colorPixelFormat;
    copy.depthPixelFormat = self.depthPixelFormat;
    copy.stencilPixelFormat = self.stencilPixelFormat;
    return copy;
}

- (NSUInteger)hash
{
    NSUInteger sc = _sampleCount & 0x3;
    NSUInteger cf = _colorPixelFormat & 0x3FF;
    NSUInteger df = _depthPixelFormat & 0x3FF;
    NSUInteger sf = _stencilPixelFormat & 0x3FF;
    NSUInteger hash = (sf << 22) | (df << 12) | (cf << 2) | sc;
    return hash;
}

- (BOOL)isEqual:(id)object
{
    FramebufferDescriptor* other = object;
    if (![other isKindOfClass:[FramebufferDescriptor class]])
        return NO;
    return other.sampleCount == self.sampleCount      &&
    other.colorPixelFormat   == self.colorPixelFormat &&
    other.depthPixelFormat   == self.depthPixelFormat &&
    other.stencilPixelFormat == self.stencilPixelFormat;
}

@end

//-----------------------------------------------------------------------------
// [SECTION] MetalContext
//-----------------------------------------------------------------------------

@implementation MetalContext
- (instancetype)init
{
    if ((self = [super init]))
    {
        self.renderPipelineStateCache = [NSMutableDictionary dictionary];
        self.renderPipelineStateSDFCache = [NSMutableDictionary dictionary];
        self.bufferCache = [NSMutableArray array];
        _lastBufferCachePurge = GetMachAbsoluteTimeInSeconds();
    }
    return self;
}

- (MetalBuffer*)dequeueReusableBufferOfLength:(NSUInteger)length device:(id<MTLDevice>)device
{
    uint64_t now = GetMachAbsoluteTimeInSeconds();

    @synchronized(self.bufferCache)
    {
        // Purge old buffers that haven't been useful for a while
        if (now - self.lastBufferCachePurge > 1.0)
        {
            NSMutableArray* survivors = [NSMutableArray array];
            for (MetalBuffer* candidate in self.bufferCache)
                if (candidate.lastReuseTime > self.lastBufferCachePurge)
                    [survivors addObject:candidate];
            self.bufferCache = [survivors mutableCopy];
            self.lastBufferCachePurge = now;
        }

        // See if we have a buffer we can reuse
        MetalBuffer* bestCandidate = nil;
        for (MetalBuffer* candidate in self.bufferCache)
            if (candidate.buffer.length >= length && (bestCandidate == nil || bestCandidate.lastReuseTime > candidate.lastReuseTime))
                bestCandidate = candidate;

        if (bestCandidate != nil)
        {
            [self.bufferCache removeObject:bestCandidate];
            bestCandidate.lastReuseTime = now;
            return bestCandidate;
        }
    }

    // No luck; make a new buffer
    id<MTLBuffer> backing = [device newBufferWithLength:length options:MTLResourceStorageModeShared];
    return [[MetalBuffer alloc] initWithBuffer:backing];
}

// Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.
- (id<MTLRenderPipelineState>)renderPipelineStateForFramebufferDescriptor:(FramebufferDescriptor*)descriptor device:(id<MTLDevice>)device
{
    NSError* error = nil;

    NSString* shaderSource = @""
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct Uniforms {\n"
    "    float4x4 projectionMatrix;\n"
    "};\n"
    "\n"
    "struct VertexIn {\n"
    "    float2 position  [[attribute(0)]];\n"
    "    float2 texCoords [[attribute(1)]];\n"
    "    uchar4 color     [[attribute(2)]];\n"
    "};\n"
    "\n"
    "struct VertexOut {\n"
    "    float4 position [[position]];\n"
    "    float2 texCoords;\n"
    "    float4 color;\n"
    "};\n"
    "\n"
    "vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],\n"
    "                             constant Uniforms &uniforms [[buffer(1)]]) {\n"
    "    VertexOut out;\n"
    "    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);\n"
    "    out.texCoords = in.texCoords;\n"
    "    out.color = float4(in.color) / float4(255.0);\n"
    "    return out;\n"
    "}\n"
    "\n"
    "fragment half4 fragment_main(VertexOut in [[stage_in]],\n"
    "                             texture2d<half, access::sample> texture [[texture(2)]]) {\n"
    "    constexpr sampler linearSampler(coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear);\n"
    "    half4 texColor = texture.sample(linearSampler, in.texCoords);\n"
    "    return half4(in.color) * texColor;\n"
    "}\n";

    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
    if (library == nil)
    {
        NSLog(@"Error: failed to create Metal library: %@", error);
        return nil;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

    if (vertexFunction == nil || fragmentFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        return nil;
    }

    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2; // position
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].offset = sizeof(float) * 2;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2; // texCoords
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[2].offset = sizeof(float) * 4;
    vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4; // color
    vertexDescriptor.attributes[2].bufferIndex = 0;
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stride = sizeof(plDrawVertex);

    MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
    depthDescriptor.depthWriteEnabled = NO;
    self.depthStencilState = [device newDepthStencilStateWithDescriptor:depthDescriptor];

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.rasterSampleCount = 1;
    // pipelineDescriptor.rasterSampleCount = self.framebufferDescriptor.sampleCount;
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.framebufferDescriptor.colorPixelFormat;
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
    pipelineDescriptor.depthAttachmentPixelFormat = self.framebufferDescriptor.depthPixelFormat;
    pipelineDescriptor.stencilAttachmentPixelFormat = self.framebufferDescriptor.stencilPixelFormat;

    id<MTLRenderPipelineState> renderPipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    return renderPipelineState;
}

- (id<MTLRenderPipelineState>)renderPipelineStateForFramebufferDescriptorSDF:(FramebufferDescriptor*)descriptor device:(id<MTLDevice>)device
{
    NSError* error = nil;

    NSString* shaderSource = @""
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct Uniforms {\n"
    "    float4x4 projectionMatrix;\n"
    "};\n"
    "\n"
    "struct VertexIn {\n"
    "    float2 position  [[attribute(0)]];\n"
    "    float2 texCoords [[attribute(1)]];\n"
    "    uchar4 color     [[attribute(2)]];\n"
    "};\n"
    "\n"
    "struct VertexOut {\n"
    "    float4 position [[position]];\n"
    "    float2 texCoords;\n"
    "    float4 color;\n"
    "};\n"
    "\n"
    "vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],\n"
    "                             constant Uniforms &uniforms [[buffer(1)]]) {\n"
    "    VertexOut out;\n"
    "    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);\n"
    "    out.texCoords = in.texCoords;\n"
    "    out.color = float4(in.color) / float4(255.0);\n"
    "    return out;\n"
    "}\n"
    "\n"
    "fragment float4 fragment_main(VertexOut in [[stage_in]],\n"
    "                             texture2d<float, access::sample> texture [[texture(2)]]) {\n"
    "    constexpr sampler linearSampler(coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear);\n"
    "    float distance = texture.sample(linearSampler, in.texCoords).a;\n"
    "    float smoothWidth = fwidth(distance);\n"
    "    float alpha = smoothstep(0.5 - smoothWidth, 0.5 + smoothWidth, distance);\n"
    "    float3 texColor = texture.sample(linearSampler, in.texCoords).rgb * float3(in.color.rgb);\n"
    "    return float4(texColor, alpha);\n"
    "}\n";

    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
    if (library == nil)
    {
        NSLog(@"Error: failed to create Metal library: %@", error);
        return nil;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

    if (vertexFunction == nil || fragmentFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        return nil;
    }

    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2; // position
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].offset = sizeof(float) * 2;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2; // texCoords
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[2].offset = sizeof(float) * 4;
    vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4; // color
    vertexDescriptor.attributes[2].bufferIndex = 0;
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stride = sizeof(plDrawVertex);

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.rasterSampleCount = 1;
    // pipelineDescriptor.rasterSampleCount = self.framebufferDescriptor.sampleCount;
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.framebufferDescriptor.colorPixelFormat;
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
    pipelineDescriptor.depthAttachmentPixelFormat = self.framebufferDescriptor.depthPixelFormat;
    pipelineDescriptor.stencilAttachmentPixelFormat = self.framebufferDescriptor.stencilPixelFormat;

    id<MTLRenderPipelineState> renderPipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    return renderPipelineState;
}
@end

NS_ASSUME_NONNULL_END