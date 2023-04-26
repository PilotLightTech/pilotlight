/*
   pl_metal.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api
*/

#ifndef PL_DRAW_METAL_H
#define PL_DRAW_METAL_H

#define PL_API_METAL_DRAW "PL_API_METAL_DRAW"

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_draw_ext.h"
#import <Metal/Metal.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

NS_ASSUME_NONNULL_BEGIN
typedef struct _plMetalDrawApiI
{
void (*initialize_context)(plDrawContext* ctx, id<MTLDevice> device);
void (*new_frame)         (plDrawContext* ctx, MTLRenderPassDescriptor* renderPassDescriptor);
void (*submit_drawlist)   (plDrawList* drawlist, float width, float height, id<MTLRenderCommandEncoder> renderEncoder);
} plMetalDrawApiI;

plMetalDrawApiI* pl_load_metal_draw_api(void);
NS_ASSUME_NONNULL_END

#endif // PL_METAL_H