/*
   pl_draw_dx11.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api
// [SECTION] c file
// [SECTION] includes
// [SECTION] shaders
// [SECTION] internal structs
// [SECTION] internal helper forward declarations
// [SECTION] implementation
// [SECTION] internal helpers implementation
*/

#ifndef PL_DRAW_DX11_H
#define PL_DRAW_DX11_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_draw.h"
#include <d3d11_1.h>

#ifndef PL_DX11
#include <assert.h>
#define PL_DX11(x) assert(SUCCEEDED((x)))
#endif

#ifndef PL_COM
#define PL_COM(x) (x)->lpVtbl
#endif

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// backend implementation
void pl_initialize_draw_context_dx11(plDrawContext* ptCtx, ID3D11Device* ptDevice, ID3D11DeviceContext* ptDeviceCtx);
void pl_setup_drawlist_dx11         (plDrawList* ptDrawlist);
void pl_submit_drawlist_dx11        (plDrawList* ptDrawlist, float fWidth, float fHeight);
void pl_new_draw_frame              (plDrawContext* ptCtx);

#endif // PL_DRAW_DX11_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

#ifdef PL_DRAW_DX11_IMPLEMENTATION

#ifndef PL_COM_RELEASE
#define PL_COM_RELEASE(x) (x)->lpVtbl->Release((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include <d3dcompiler.h>
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] shaders
//-----------------------------------------------------------------------------

static const char* pcVertexShader =
    "cbuffer vertexBuffer : register(b0) \
    {\
        float4x4 ProjectionMatrix; \
    };\
    struct VS_INPUT\
    {\
        float2 pos : POSITION;\
        float4 col : COLOR0;\
        float2 uv  : TEXCOORD0;\
    };\
    \
    struct PS_INPUT\
    {\
        float4 pos : SV_POSITION;\
        float4 col : COLOR0;\
        float2 uv  : TEXCOORD0;\
    };\
    \
    PS_INPUT main(VS_INPUT input)\
    {\
        PS_INPUT output;\
        output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
        output.col = input.col;\
        output.uv  = input.uv;\
        return output;\
    }";

static const char* pcPixelShader =
    "struct PS_INPUT\
    {\
    float4 pos : SV_POSITION;\
    float4 col : COLOR0;\
    float2 uv  : TEXCOORD0;\
    };\
    sampler sampler0;\
    Texture2D texture0;\
    \
    float4 main(PS_INPUT input) : SV_Target\
    {\
    float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
    return out_col; \
    }";

static const char* pcSDFPixelShader =
    "struct PS_INPUT\
    {\
    float4 pos : SV_POSITION;\
    float4 col : COLOR0;\
    float2 uv  : TEXCOORD0;\
    };\
    sampler sampler0;\
    Texture2D texture0;\
    \
    float4 main(PS_INPUT input) : SV_Target\
    {\
    float fDistance = texture0.Sample(sampler0, input.uv).a;\
    float fSmoothWidth = fwidth(fDistance);	\
    float fAlpha = smoothstep(0.5 - fSmoothWidth, 0.5 + fSmoothWidth, fDistance);\
    float3 fRgbVec = input.col.rgb * texture0.Sample(sampler0, input.uv).rgb;\
    float4 out_col = float4(fRgbVec, fAlpha);	\
    return out_col; \
    }";

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct plDx11DrawContext_t
{
    ID3D11Device*             ptDevice;
    ID3D11DeviceContext*      ptContext;
    ID3D11Buffer*             ptConstantBuffer;
    ID3D11SamplerState*       ptSampler;
    ID3D11RasterizerState*    ptRasterizerState;
    ID3D11BlendState*         ptBlendState;
    ID3D11DepthStencilState*  ptDepthStencilState;
    ID3D11VertexShader*       ptVertexShader;
    ID3D11PixelShader*        ptPixelShader;
    ID3D11PixelShader*        ptSdfShader;
    ID3D11InputLayout*        ptInputLayout;  
    ID3D11ShaderResourceView* ptAtlasTexture; 
} plDx11DrawContext;

typedef struct plDx11DrawList_t
{
    // vertex buffer
    ID3D11Buffer*  ptVertexBuffer;
    unsigned int   uVertexByteSize;
    
    // index buffer
    ID3D11Buffer*  ptIndexBuffer;
    unsigned int   uIndexByteSize;
} plDx11DrawList;

//-----------------------------------------------------------------------------
// [SECTION] internal helper forward declarations
//-----------------------------------------------------------------------------

extern void pl__cleanup_font_atlas     (plFontAtlas* atlas); // in pl_draw.c
extern void pl__new_draw_frame         (plDrawContext* ctx); // in pl_draw.c
static void pl__grow_dx11_vertex_buffer(plDrawList* ptDrawlist, uint32_t uVtxBufSzNeeded);
static void pl__grow_dx11_index_buffer (plDrawList* ptDrawlist, uint32_t uIdxBufSzNeeded);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

void
pl_initialize_draw_context_dx11(plDrawContext* ptCtx, ID3D11Device* ptDevice, ID3D11DeviceContext* ptDeviceCtx)
{
    memset(ptCtx, 0, sizeof(plDrawContext));

    plDx11DrawContext* ptDx11Context = PL_ALLOC(sizeof(plDx11DrawContext));
    memset(ptDx11Context, 0, sizeof(plDx11DrawContext));
    ptDx11Context->ptDevice = ptDevice;
    ptDx11Context->ptContext = ptDeviceCtx;
    ptCtx->_platformData = ptDx11Context;

    ID3DBlob* vsBlob = NULL;
    ID3DBlob* psBlob = NULL;
    ID3DBlob* sdfpsBlob = NULL;

    ID3DBlob* shaderCompileErrorsBlob;
    PL_DX11(D3DCompile(pcVertexShader, strlen(pcVertexShader), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &vsBlob, &shaderCompileErrorsBlob));

    PL_DX11(PL_COM(ptDx11Context->ptDevice)->CreateVertexShader(ptDx11Context->ptDevice, PL_COM(vsBlob)->GetBufferPointer(vsBlob), PL_COM(vsBlob)->GetBufferSize(vsBlob), NULL, &ptDx11Context->ptVertexShader));

    PL_DX11(D3DCompile(pcPixelShader, strlen(pcPixelShader), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &psBlob, &shaderCompileErrorsBlob));
    PL_DX11(PL_COM(ptDx11Context->ptDevice)->CreatePixelShader(ptDx11Context->ptDevice, PL_COM(psBlob)->GetBufferPointer(psBlob), PL_COM(psBlob)->GetBufferSize(psBlob), NULL, &ptDx11Context->ptPixelShader));
    PL_COM_RELEASE(psBlob);

    PL_DX11(D3DCompile(pcSDFPixelShader, strlen(pcSDFPixelShader), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &sdfpsBlob, &shaderCompileErrorsBlob));
    PL_DX11(PL_COM(ptDx11Context->ptDevice)->CreatePixelShader(ptDx11Context->ptDevice, PL_COM(sdfpsBlob)->GetBufferPointer(sdfpsBlob), PL_COM(sdfpsBlob)->GetBufferSize(sdfpsBlob), NULL, &ptDx11Context->ptSdfShader));
    PL_COM_RELEASE(sdfpsBlob);

    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        { "Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "Color", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };   

    PL_COM(ptDx11Context->ptDevice)->CreateInputLayout(ptDx11Context->ptDevice, inputElementDesc, 3, PL_COM(vsBlob)->GetBufferPointer(vsBlob), PL_COM(vsBlob)->GetBufferSize(vsBlob), &ptDx11Context->ptInputLayout);
    PL_COM_RELEASE(vsBlob);

    D3D11_BUFFER_DESC tConstantBufferDescription = {
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .Usage = D3D11_USAGE_DYNAMIC,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        .MiscFlags = 0u,
        .ByteWidth = sizeof(float) * 16,
        .StructureByteStride = 0u
    };
    PL_COM(ptDx11Context->ptDevice)->CreateBuffer(ptDx11Context->ptDevice, &tConstantBufferDescription, NULL, &ptDx11Context->ptConstantBuffer);

    D3D11_SAMPLER_DESC tSamplerDescription = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
        .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
    };
    PL_COM(ptDx11Context->ptDevice)->CreateSamplerState(ptDx11Context->ptDevice, &tSamplerDescription, &ptDx11Context->ptSampler);

    D3D11_BLEND_DESC tBlendingDescription = {
        .AlphaToCoverageEnable = false,
        .RenderTarget[0].BlendEnable = true,
        .RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA,
        .RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
        .RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD,
        .RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE,
        .RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
        .RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD,
        .RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL
    };
    PL_COM(ptDx11Context->ptDevice)->CreateBlendState(ptDx11Context->ptDevice, &tBlendingDescription, &ptDx11Context->ptBlendState);

    D3D11_RASTERIZER_DESC tRasterizerState = {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_NONE,
            .ScissorEnable = false,
            .DepthClipEnable = true
    };
    PL_COM(ptDx11Context->ptDevice)->CreateRasterizerState(ptDx11Context->ptDevice, &tRasterizerState, &ptDx11Context->ptRasterizerState);

    D3D11_DEPTH_STENCIL_DESC tDepthStencilState = {
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_ALWAYS,
        .FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP,
        .FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
        .FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP,
        .FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS,
        .BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP,
        .BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
        .BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP,
        .BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS
    };
    PL_COM(ptDx11Context->ptDevice)->CreateDepthStencilState(ptDx11Context->ptDevice, &tDepthStencilState, &ptDx11Context->ptDepthStencilState);
}

void
pl_setup_drawlist_dx11(plDrawList* ptDrawlist)
{
     if(ptDrawlist->_platformData == NULL)
    {
        ptDrawlist->_platformData = PL_ALLOC(sizeof(plDx11DrawList));
        memset(ptDrawlist->_platformData, 0, sizeof(plDx11DrawList));
    }
}

void
pl_new_draw_frame(plDrawContext* ptCtx)
{
    pl__new_draw_frame(ptCtx);
}

void
pl_cleanup_font_atlas(plFontAtlas* ptAtlas)
{
    plDx11DrawContext* ptDx11DrawCtx = ptAtlas->ctx->_platformData;
    PL_COM_RELEASE(ptDx11DrawCtx->ptAtlasTexture);
    pl__cleanup_font_atlas(ptAtlas);
}

void
pl_submit_drawlist_dx11(plDrawList* ptDrawlist, float fWidth, float fHeight)
{
    if(pl_sb_size(ptDrawlist->sbVertexBuffer) == 0u)
        return;

    plDrawContext* drawContext = ptDrawlist->ctx;
    plDx11DrawList* ptDx11Drawlist = ptDrawlist->_platformData;
    plDx11DrawContext* ptDx11DrawCtx = ptDrawlist->ctx->_platformData;

    // ensure gpu vertex buffer size is adequate
    uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex) * pl_sb_size(ptDrawlist->sbVertexBuffer);
    if(uVtxBufSzNeeded >= ptDx11Drawlist->uVertexByteSize)
        pl__grow_dx11_vertex_buffer(ptDrawlist, uVtxBufSzNeeded * 2);

    // ensure gpu index buffer size is adequate
    uint32_t uIdxBufSzNeeded = ptDrawlist->indexBufferByteSize;
    if(uIdxBufSzNeeded >= ptDrawlist->indexBufferByteSize)
        pl__grow_dx11_index_buffer(ptDrawlist, uIdxBufSzNeeded * 2);

    // vertex GPU data transfer
    D3D11_MAPPED_SUBRESOURCE vtxMapping;
    PL_DX11(PL_COM(ptDx11DrawCtx->ptContext)->Map(ptDx11DrawCtx->ptContext, (ID3D11Resource*)ptDx11Drawlist->ptVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtxMapping));
    memcpy(vtxMapping.pData, ptDrawlist->sbVertexBuffer, sizeof(plDrawVertex) * pl_sb_size(ptDrawlist->sbVertexBuffer));
    PL_COM(ptDx11DrawCtx->ptContext)->Unmap(ptDx11DrawCtx->ptContext, (ID3D11Resource*)ptDx11Drawlist->ptVertexBuffer, 0);
    
    // index GPU data transfer
    uint32_t uTempIndexBufferOffset = 0;
    uint32_t globalIdxBufferIndexOffset = 0u;
    D3D11_MAPPED_SUBRESOURCE idxMapping;
    PL_DX11(PL_COM(ptDx11DrawCtx->ptContext)->Map(ptDx11DrawCtx->ptContext, (ID3D11Resource*)ptDx11Drawlist->ptIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &idxMapping));
    for(uint32_t i = 0u; i < pl_sb_size(ptDrawlist->sbSubmittedLayers); i++)
    {
        plDrawCommand* ptLastCommand = NULL;
        plDrawLayer* layer = ptDrawlist->sbSubmittedLayers[i];
        memcpy(&((uint32_t*)idxMapping.pData)[globalIdxBufferIndexOffset], layer->sbIndexBuffer, sizeof(uint32_t) * pl_sb_size(layer->sbIndexBuffer));
        uTempIndexBufferOffset += pl_sb_size(layer->sbIndexBuffer)*sizeof(uint32_t);

        // attempt to merge commands
        for(uint32_t j = 0u; j < pl_sb_size(layer->sbCommandBuffer); j++)
        {
            plDrawCommand* layerCommand = &layer->sbCommandBuffer[j];
            bool bCreateNewCommand = true;

            if(ptLastCommand)
            {
                // check for same texture (allows merging draw calls)
                if(ptLastCommand->textureId == layerCommand->textureId && ptLastCommand->sdf == layerCommand->sdf)
                {
                    ptLastCommand->elementCount += layerCommand->elementCount;
                    bCreateNewCommand = false;
                }
            }

            if(bCreateNewCommand)
            {
                layerCommand->indexOffset = globalIdxBufferIndexOffset + layerCommand->indexOffset;
                pl_sb_push(ptDrawlist->sbDrawCommands, *layerCommand);       
                ptLastCommand = layerCommand;
            }
            
        }    
        globalIdxBufferIndexOffset += pl_sb_size(layer->sbIndexBuffer);    
    }
    PL_COM(ptDx11DrawCtx->ptContext)->Unmap(ptDx11DrawCtx->ptContext, (ID3D11Resource*)ptDx11Drawlist->ptIndexBuffer, 0);

    // setup regular pipeline

    // bind constant buffer
    PL_COM(ptDx11DrawCtx->ptContext)->VSSetConstantBuffers(ptDx11DrawCtx->ptContext, 0u, 1u, &ptDx11DrawCtx->ptConstantBuffer);

    // bind primitive topology
    PL_COM(ptDx11DrawCtx->ptContext)->IASetPrimitiveTopology(ptDx11DrawCtx->ptContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // bind vertex layout
    PL_COM(ptDx11DrawCtx->ptContext)->IASetInputLayout(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptInputLayout);

    // bind shaders
    PL_COM(ptDx11DrawCtx->ptContext)->VSSetShader(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptVertexShader, NULL, 0);
    PL_COM(ptDx11DrawCtx->ptContext)->PSSetShader(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptPixelShader, NULL, 0);

    // bind index and vertex buffers
    PL_COM(ptDx11DrawCtx->ptContext)->IASetIndexBuffer(ptDx11DrawCtx->ptContext, ptDx11Drawlist->ptIndexBuffer, DXGI_FORMAT_R32_UINT, 0u);

    static UINT offset = 0;
    static UINT stride = sizeof(plDrawVertex);
    PL_COM(ptDx11DrawCtx->ptContext)->IASetVertexBuffers(ptDx11DrawCtx->ptContext, 0, 1, &ptDx11Drawlist->ptVertexBuffer, &stride, &offset);

    // bind sampler and texture
    PL_COM(ptDx11DrawCtx->ptContext)->PSSetSamplers(ptDx11DrawCtx->ptContext, 0u, 1, &ptDx11DrawCtx->ptSampler);

    // Setup blend state
    const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
    PL_COM(ptDx11DrawCtx->ptContext)->OMSetBlendState(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptBlendState, blend_factor, 0xffffffff);
    PL_COM(ptDx11DrawCtx->ptContext)->OMSetDepthStencilState(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptDepthStencilState, 0);
    PL_COM(ptDx11DrawCtx->ptContext)->RSSetState(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptRasterizerState);

    float L = 0.0f;
    float R = 0.0f + fWidth;
    float T = 0.0f;
    float B = 0.0f + fHeight;
    float mvp[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,           0.5f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };

    D3D11_MAPPED_SUBRESOURCE mappedSubresource;
    PL_COM(ptDx11DrawCtx->ptContext)->Map(ptDx11DrawCtx->ptContext, (ID3D11Resource*)ptDx11DrawCtx->ptConstantBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mappedSubresource);
    memcpy(mappedSubresource.pData, mvp, sizeof(float)*16);
    PL_COM(ptDx11DrawCtx->ptContext)->Unmap(ptDx11DrawCtx->ptContext, (ID3D11Resource*)ptDx11DrawCtx->ptConstantBuffer, 0u);

    uint32_t uCurrentDelayIndex = 0u;
    bool sdf = false;
    for(uint32_t i = 0u; i < pl_sb_size(ptDrawlist->sbDrawCommands); i++)
    {
        plDrawCommand cmd = ptDrawlist->sbDrawCommands[i];

        if(cmd.sdf && !sdf) // delay
        {
            PL_COM(ptDx11DrawCtx->ptContext)->PSSetShader(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptSdfShader, NULL, 0);
            sdf = true;
        }
        else if(!cmd.sdf && sdf)
        {
            PL_COM(ptDx11DrawCtx->ptContext)->PSSetShader(ptDx11DrawCtx->ptContext, ptDx11DrawCtx->ptPixelShader, NULL, 0);
            sdf = false;
        }
        PL_COM(ptDx11DrawCtx->ptContext)->PSSetShaderResources(ptDx11DrawCtx->ptContext, 0u, 1, &(ID3D11ShaderResourceView*)cmd.textureId);
        PL_COM(ptDx11DrawCtx->ptContext)->DrawIndexed(ptDx11DrawCtx->ptContext, cmd.elementCount, cmd.indexOffset, 0);
    }
}

void
pl_cleanup_draw_context(plDrawContext* ctx)
{
    plDx11DrawContext* ptDx11DrawCtx = ctx->_platformData;

    PL_COM_RELEASE(ptDx11DrawCtx->ptBlendState);
    PL_COM_RELEASE(ptDx11DrawCtx->ptConstantBuffer);
    PL_COM_RELEASE(ptDx11DrawCtx->ptDepthStencilState);
    PL_COM_RELEASE(ptDx11DrawCtx->ptInputLayout);
    PL_COM_RELEASE(ptDx11DrawCtx->ptRasterizerState);
    PL_COM_RELEASE(ptDx11DrawCtx->ptSampler);
    PL_COM_RELEASE(ptDx11DrawCtx->ptSdfShader);
    PL_COM_RELEASE(ptDx11DrawCtx->ptPixelShader);
    PL_COM_RELEASE(ptDx11DrawCtx->ptVertexShader);

    for(uint32_t i = 0u; i < pl_sb_size(ctx->sbDrawlists); i++)
    {
        plDrawList* drawlist = ctx->sbDrawlists[i];
        plDx11DrawList* ptDx11Drawlist = drawlist->_platformData;

        PL_COM_RELEASE(ptDx11Drawlist->ptVertexBuffer);
        PL_COM_RELEASE(ptDx11Drawlist->ptIndexBuffer);
    }
}

void
pl_build_font_atlas(plDrawContext* ctx, plFontAtlas* atlas)
{
    plDx11DrawContext* ptDx11DrawCtx = ctx->_platformData;

    pl__build_font_atlas(atlas);
    atlas->ctx = ctx;
    ctx->fontAtlas = atlas;

    if(atlas->texture)
        PL_COM_RELEASE(ptDx11DrawCtx->ptAtlasTexture);

    ID3D11Texture2D* ptTexture = NULL;

    D3D11_TEXTURE2D_DESC tTextureDescription = {
        .Width = atlas->atlasSize[0],
        .Height = atlas->atlasSize[1],
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc.Count = 1,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0
    };

    D3D11_SUBRESOURCE_DATA tSubresourceData = {
        .pSysMem = atlas->pixelsAsRGBA32,
        .SysMemPitch = tTextureDescription.Width * 4,
        .SysMemSlicePitch = 0
    };
    PL_DX11(PL_COM(ptDx11DrawCtx->ptDevice)->CreateTexture2D(ptDx11DrawCtx->ptDevice, &tTextureDescription, &tSubresourceData, &ptTexture));

    // create the resource view on the texture
    D3D11_SHADER_RESOURCE_VIEW_DESC tShaderResourceViewDescription = {
        .Format = tTextureDescription.Format,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D.MipLevels = 1
    };
    PL_DX11(PL_COM(ptDx11DrawCtx->ptDevice)->CreateShaderResourceView(ptDx11DrawCtx->ptDevice, (ID3D11Resource*)ptTexture, &tShaderResourceViewDescription, &ptDx11DrawCtx->ptAtlasTexture));
    atlas->texture = ptDx11DrawCtx->ptAtlasTexture;
    PL_COM_RELEASE(ptTexture);
}

//-----------------------------------------------------------------------------
// [SECTION] internal helpers implementation
//-----------------------------------------------------------------------------

static void
pl__grow_dx11_vertex_buffer(plDrawList* ptDrawlist, uint32_t uVtxBufSzNeeded)
{
    plDx11DrawList* ptDx11Drawlist = ptDrawlist->_platformData;
    plDx11DrawContext* ptDx11DrawCtx = ptDrawlist->ctx->_platformData;

    if(ptDx11Drawlist->ptVertexBuffer)
        PL_COM_RELEASE(ptDx11Drawlist->ptVertexBuffer);

    D3D11_BUFFER_DESC tBufferDescription = {
        .ByteWidth = uVtxBufSzNeeded,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
    };
    PL_DX11(PL_COM(ptDx11DrawCtx->ptDevice)->CreateBuffer(ptDx11DrawCtx->ptDevice, &tBufferDescription, NULL, &ptDx11Drawlist->ptVertexBuffer));
    ptDx11Drawlist->uVertexByteSize = uVtxBufSzNeeded;
}

static void
pl__grow_dx11_index_buffer(plDrawList* ptDrawlist, uint32_t uIdxBufSzNeeded)
{
    plDx11DrawList* ptDx11Drawlist = ptDrawlist->_platformData;
    plDx11DrawContext* ptDx11DrawCtx = ptDrawlist->ctx->_platformData;

    if(ptDx11Drawlist->ptIndexBuffer)
        PL_COM_RELEASE(ptDx11Drawlist->ptIndexBuffer);

    D3D11_BUFFER_DESC tBufferDescription = {
        .ByteWidth = uIdxBufSzNeeded,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
    };
    PL_DX11(PL_COM(ptDx11DrawCtx->ptDevice)->CreateBuffer(ptDx11DrawCtx->ptDevice, &tBufferDescription, NULL, &ptDx11Drawlist->ptIndexBuffer));
    ptDx11Drawlist->uIndexByteSize = uIdxBufSzNeeded;
}

#endif // PL_DRAW_DX11_IMPLEMENTATION