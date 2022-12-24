#include "pilotlight.h"
#include "pl_registry.h"
#include "pl_draw_extension.h"
#include "pl_draw.h"
#include "pl_math.h"

plDrawExtension gtDrawExtension = {0};

plApi gatApis[1] = {0};

static void
pl_ext_add_text(plDrawLayer* layer, plFont* font, float size, plVec2 p, plVec4 color, const char* text)
{
    pl_add_text(layer, font, size, p, color, text, 0.0f);
}

PL_EXPORT void
pl_load_draw_extension(plDataRegistry* ptDataRegistry, plExtensionRegistry* ptExtensionRegistry, plExtension* ptExtension, bool bReload)
{
    pl_set_extension_registry(ptExtensionRegistry);
    pl_set_data_registry(ptDataRegistry);

    gtDrawExtension.pl_add_text = pl_ext_add_text;
    gatApis[0].pcName = PL_EXT_API_DRAW;
    gatApis[0].pApi = &gtDrawExtension;
    ptExtension->atApis = gatApis;

    pl_load_extension(ptExtension);

    pl_set_draw_context(pl_get_data("draw"));
}

PL_EXPORT void
pl_unload_draw_extension(plDataRegistry* ptDataRegistry, plExtensionRegistry* ptExtensionRegistry, plExtension* ptExtension)
{
    
}