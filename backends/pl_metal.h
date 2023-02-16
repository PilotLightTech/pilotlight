/*
   pl_metal.h
*/

/*
Index of this file:
// [SECTION] includess
// [SECTION] public api
*/

#ifndef PL_DRAW_METAL_H
#define PL_DRAW_METAL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_draw.h"
#import <Metal/Metal.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

NS_ASSUME_NONNULL_BEGIN
void pl_initialize_draw_context_metal(plDrawContext* ctx, id<MTLDevice> device);
void pl_new_draw_frame_metal         (plDrawContext* ctx, MTLRenderPassDescriptor* renderPassDescriptor);
void pl_submit_drawlist_metal        (plDrawList* drawlist, float width, float height, id<MTLRenderCommandEncoder> renderEncoder);
NS_ASSUME_NONNULL_END

#endif // PL_METAL_H