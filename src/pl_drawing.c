/*
   pl_drawing.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
// [SECTION] implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_drawing.h"
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void pl__prepare_draw_command(plDrawLayer* layer, plTextureId texture);
static void pl__reserve_triangles(plDrawLayer* layer, uint32_t indexCount, uint32_t vertexCount);
static void pl__add_vertex(plDrawLayer* layer, plVec2 pos, plVec4 color, plVec2 uv);
static void pl__add_index(plDrawLayer* layer, uint32_t vertexStart, uint32_t i0, uint32_t i1, uint32_t i2);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

void
pl_create_drawlist(plDrawContext* ctx, plDrawList* drawlistOut)
{
   memset(drawlistOut, 0, sizeof(plDrawList));
   drawlistOut->ctx = ctx;
   pl_sb_push(ctx->sbDrawlists, drawlistOut);
}

plDrawLayer*
pl_request_draw_layer(plDrawList* drawlist, const char* name)
{
   plDrawLayer* layer = NULL;
   
   // check if drawlist has any cached layers
   // which reduces allocations necessary since
   // cached layers' buffers are only reset
   if(pl_sb_size(drawlist->sbLayerCache) > 0)
   {
        layer = pl_sb_pop(drawlist->sbLayerCache);
   }

   else // create new layer
   {
      drawlist->layersCreated++;
      layer = PL_ALLOC(sizeof(plDrawLayer));
      memset(layer, 0, sizeof(plDrawLayer));
      layer->drawlist = drawlist;
      pl_sb_push(drawlist->sbLayersCreated, layer);
   }

   if(name)
      strncpy(layer->name, name, PL_MAX_NAME_LENGTH);

   return layer;
}

void
pl_return_draw_layer(plDrawLayer* layer)
{
    layer->name[0] = 0;
    layer->_lastCommand = NULL;
    layer->vertexCount = 0u;
    pl_sb_reset(layer->sbCommandBuffer);
    pl_sb_reset(layer->sbIndexBuffer);
    pl_sb_reset(layer->sbPath);
    pl_sb_push(layer->drawlist->sbLayerCache, layer);
}

void
pl__new_draw_frame(plDrawContext* ctx)
{
    // reset drawlists
    for(uint32_t i = 0u; i < pl_sb_size(ctx->sbDrawlists); i++)
    {
        plDrawList* drawlist = ctx->sbDrawlists[i];

        drawlist->indexBufferByteSize = 0u;
        pl_sb_reset(drawlist->sbDrawCommands);
        pl_sb_reset(drawlist->sbVertexBuffer);

        // reset submitted layers
        for(uint32_t j = 0; j < pl_sb_size(drawlist->sbSubmittedLayers); j++)
        {
            pl_sb_reset(drawlist->sbSubmittedLayers[j]->sbCommandBuffer);
            pl_sb_reset(drawlist->sbSubmittedLayers[j]->sbIndexBuffer);   
            pl_sb_reset(drawlist->sbSubmittedLayers[j]->sbPath);  
            drawlist->sbSubmittedLayers[j]->vertexCount = 0u;
            drawlist->sbSubmittedLayers[j]->_lastCommand = NULL;
        }
        pl_sb_reset(drawlist->sbSubmittedLayers);       
    }
    ctx->frameCount++;
}

void
pl_submit_draw_layer(plDrawLayer* layer)
{
    pl_sb_push(layer->drawlist->sbSubmittedLayers, layer);
    layer->drawlist->indexBufferByteSize += pl_sb_size(layer->sbIndexBuffer) * sizeof(uint32_t);
}

void
pl_add_triangle_filled(plDrawLayer* layer, plVec2 p0, plVec2 p1, plVec2 p2, plVec4 color)
{
    pl__prepare_draw_command(layer, layer->drawlist->ctx->fontAtlas);
    pl__reserve_triangles(layer, 3, 3);

    uint32_t vertexStart = pl_sb_size(layer->drawlist->sbVertexBuffer);
    pl__add_vertex(layer, p0, color, (plVec2){0.0f, 0.0f});
    pl__add_vertex(layer, p1, color, (plVec2){0.0f, 0.0f});
    pl__add_vertex(layer, p2, color, (plVec2){0.0f, 0.0f});

    pl__add_index(layer, vertexStart, 0, 1, 2);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__prepare_draw_command(plDrawLayer* layer, plTextureId textureID)
{
    bool createNewCommand = true;
    
    if(layer->_lastCommand)
    {
        // check if last command has same texture
        if(layer->_lastCommand->textureId == textureID)
        {
            createNewCommand = false;
        }
    }

    // new command needed
    if(createNewCommand)
    {
        plDrawCommand newdrawCommand = 
        {
            .vertexOffset = pl_sb_size(layer->drawlist->sbVertexBuffer),
            .indexOffset = pl_sb_size(layer->sbIndexBuffer),
            .elementCount = 0u,
            .textureId = textureID
        };
        pl_sb_push(layer->sbCommandBuffer, newdrawCommand);
        layer->_lastCommand = &pl_sb_top(layer->sbCommandBuffer);
    }
}

static void
pl__reserve_triangles(plDrawLayer* layer, uint32_t indexCount, uint32_t vertexCount)
{
    pl_sb_reserve(layer->drawlist->sbVertexBuffer, pl_sb_size(layer->drawlist->sbVertexBuffer) + vertexCount);
    pl_sb_reserve(layer->sbIndexBuffer, pl_sb_size(layer->sbIndexBuffer) + indexCount);
    layer->_lastCommand->elementCount += indexCount; 
    layer->vertexCount += vertexCount;
}

static void
pl__add_vertex(plDrawLayer* layer, plVec2 pos, plVec4 color, plVec2 uv)
{
    pl_sb_push(layer->drawlist->sbVertexBuffer,
    ((plDrawVertex){
        .pos[0] = pos.x,
        .pos[1] = pos.y,
        .uv[0] = uv.u,
        .uv[1] = uv.v,
        .color[0] = color.r,
        .color[1] = color.g,
        .color[2] = color.b,
        .color[3] = color.a
    })
    );
}

static void
pl__add_index(plDrawLayer* layer, uint32_t vertexStart, uint32_t i0, uint32_t i1, uint32_t i2)
{
    pl_sb_push(layer->sbIndexBuffer, vertexStart + i0);
    pl_sb_push(layer->sbIndexBuffer, vertexStart + i1);
    pl_sb_push(layer->sbIndexBuffer, vertexStart + i2);
}