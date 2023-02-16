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

#ifndef PL_DX11_H
#define PL_DX11_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_draw.h"
#include <d3d11_1.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_initialize_draw_context_dx11(plDrawContext* ptCtx, ID3D11Device* ptDevice, ID3D11DeviceContext* ptDeviceCtx);
void pl_setup_drawlist_dx11         (plDrawList* ptDrawlist);
void pl_submit_drawlist_dx11        (plDrawList* ptDrawlist, float fWidth, float fHeight);
void pl_new_draw_frame              (plDrawContext* ptCtx);

#endif // PL_DX11_H