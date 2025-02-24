/*
   pl_screen_log_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// core
#include <float.h>
#include "pl.h"
#include "pl_screen_log_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_draw_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plMessageData
{
    uint64_t uKey;
    double   dStartTime;
    double   dTimeToDisplay;
    char     acBuffer[1024];
    size_t   szBufferSize;
    uint32_t uColor;
    float    fTextScale;
    bool     bEven;
} plMessageData;

typedef struct _plScreenLogContext
{
    plDrawList2D*  ptDrawlist;
    plDrawLayer2D* ptDrawLayer;
    plFont*        ptFont;

    plMessageData* sbtMessages;
    plMessageData* sbtTimedMessages;
    plMessageData* sbtSortMessages;
} plScreenLogContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plScreenLogContext* gptScreenLogCtx = NULL;

#ifndef PL_UNITY_BUILD
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plDrawI*          gptDraw          = NULL;
    static const plIOI*            gptIOI           = NULL;

    static plIO* gptIO = NULL;

#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_screen_log_initialize(plScreenLogSettings tSettings)
{
    gptScreenLogCtx->ptFont = tSettings.ptFont;
    gptScreenLogCtx->ptDrawlist = gptDraw->request_2d_drawlist();
    gptScreenLogCtx->ptDrawLayer = gptDraw->request_2d_layer(gptScreenLogCtx->ptDrawlist);
}

void
pl_screen_log_cleanup(void)
{
    // gptDraw->return_2d_layer(gptScreenLogCtx->ptDrawLayer);
    // gptDraw->return_2d_drawlist(gptScreenLogCtx->ptDrawlist);
    pl_sb_free(gptScreenLogCtx->sbtMessages);
    pl_sb_free(gptScreenLogCtx->sbtTimedMessages);
    pl_sb_free(gptScreenLogCtx->sbtSortMessages);
}

void
pl_screen_log_add_message_va(uint64_t uKey, double dTimeToDisplay, uint32_t uColor, float fTextScale, const char* pcFormat, va_list args)
{

    if(uColor == 0)
        uColor = PL_COLOR_32_WHITE;

    bool bFound = false;
    uKey = uKey == 0 ? UINT64_MAX : uKey;

    if(uKey != UINT64_MAX)
    {
            uint32_t uMessageCount = pl_sb_size(gptScreenLogCtx->sbtMessages);
            for(uint32_t i = 0; i < uMessageCount; i++)
            {
                if(gptScreenLogCtx->sbtMessages[i].uKey == uKey)
                {

                    gptScreenLogCtx->sbtMessages[i].dStartTime = gptIO->dTime;
                    gptScreenLogCtx->sbtMessages[i].dTimeToDisplay = dTimeToDisplay;
                    gptScreenLogCtx->sbtMessages[i].uColor = uColor;
                    gptScreenLogCtx->sbtMessages[i].fTextScale = fTextScale;

                    if(pcFormat)
                    {
                        va_list parm_copy;
                        va_copy(parm_copy, args);
                        pl_vnsprintf(gptScreenLogCtx->sbtMessages[i].acBuffer, 1024, pcFormat, parm_copy); 
                        va_end(parm_copy);
                    }

                    if(pcFormat == NULL)
                    {
                        pl_sb_del(gptScreenLogCtx->sbtMessages, i);
                    }
                    else if(dTimeToDisplay != 0.0)
                    {
                        pl_sb_push(gptScreenLogCtx->sbtTimedMessages, gptScreenLogCtx->sbtMessages[i]);
                        pl_sb_del(gptScreenLogCtx->sbtMessages, i);
                    }

                    bFound = true;
                    break;
                }
            }

            uMessageCount = pl_sb_size(gptScreenLogCtx->sbtTimedMessages);
            for(uint32_t i = 0; i < uMessageCount; i++)
            {
                if(gptScreenLogCtx->sbtTimedMessages[i].uKey == uKey)
                {

                    gptScreenLogCtx->sbtTimedMessages[i].dStartTime = gptIO->dTime;
                    gptScreenLogCtx->sbtTimedMessages[i].dTimeToDisplay = dTimeToDisplay;
                    gptScreenLogCtx->sbtTimedMessages[i].uColor = uColor;
                    gptScreenLogCtx->sbtTimedMessages[i].fTextScale = fTextScale;

                    if(pcFormat)
                    {
                        va_list parm_copy;
                        va_copy(parm_copy, args);
                        pl_vnsprintf(gptScreenLogCtx->sbtTimedMessages[i].acBuffer, 1024, pcFormat, parm_copy); 
                        va_end(parm_copy);
                    }

                    if(pcFormat == NULL)
                    {
                        pl_sb_del(gptScreenLogCtx->sbtMessages, i);
                    }
                    else if(dTimeToDisplay == 0.0)
                    {
                        pl_sb_push(gptScreenLogCtx->sbtMessages, gptScreenLogCtx->sbtTimedMessages[i]);
                        pl_sb_del(gptScreenLogCtx->sbtMessages, i);
                    }

                    bFound = true;
                    break;
                }
            }
    }

    if(!bFound)
    {
        plMessageData tData = {
            .uKey           = uKey,
            .dStartTime     = gptIO->dTime,
            .dTimeToDisplay = dTimeToDisplay,      
            .uColor         = uColor,
            .fTextScale     = fTextScale
        };

        va_list parm_copy;
        va_copy(parm_copy, args);
        pl_vnsprintf(tData.acBuffer, 1024, pcFormat, parm_copy); 
        va_end(parm_copy);

        tData.szBufferSize   = strlen(tData.acBuffer);

        if(dTimeToDisplay != 0.0)
        {
            tData.bEven = pl_sb_size(gptScreenLogCtx->sbtTimedMessages) % 2 == 0;
            pl_sb_push(gptScreenLogCtx->sbtTimedMessages, tData);
        }
        else
        {
            tData.bEven = pl_sb_size(gptScreenLogCtx->sbtMessages) % 2 == 0;
            pl_sb_push(gptScreenLogCtx->sbtMessages, tData);
        }
    }
}

void
pl_screen_log_add_message_ex(uint64_t uKey, double dTimeToDisplay, uint32_t uColor, float fTextScale, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl_screen_log_add_message_va(uKey, dTimeToDisplay, uColor, fTextScale, pcFormat, argptr);
    va_end(argptr);  
}

void
pl_screen_log_clear(void)
{
    pl_sb_reset(gptScreenLogCtx->sbtMessages);
    pl_sb_reset(gptScreenLogCtx->sbtTimedMessages);
}

void
pl_screen_log_add_message(double dTimeToDisplay, const char* pcMessage)
{
    pl_screen_log_add_message_ex(0, dTimeToDisplay, PL_COLOR_32_WHITE, 1.0f, "%s", pcMessage);
}

plDrawList2D*
pl_screen_log_get_drawlist(float fWidth, float fHeight)
{

    const double dCurrentTime = gptIO->dTime;

    plDrawTextOptions tDrawTextOptions = {
        .fSize = gptScreenLogCtx->ptFont->fSize,
        .ptFont = gptScreenLogCtx->ptFont,
        .uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 1.0f, 1.0f),
        .fWrap = fWidth * 0.2f
    };

    uint32_t uTimedMessageCount = pl_sb_size(gptScreenLogCtx->sbtTimedMessages);
    pl_sb_reset(gptScreenLogCtx->sbtSortMessages);
    pl_sb_reserve(gptScreenLogCtx->sbtSortMessages, uTimedMessageCount);

    for(uint32_t i = 0; i < uTimedMessageCount; i++)
    {
        if(gptScreenLogCtx->sbtTimedMessages[i].dTimeToDisplay < 0.0 || dCurrentTime - gptScreenLogCtx->sbtTimedMessages[i].dStartTime < gptScreenLogCtx->sbtTimedMessages[i].dTimeToDisplay)
        {
            pl_sb_push(gptScreenLogCtx->sbtSortMessages, gptScreenLogCtx->sbtTimedMessages[i]);
        }
    }

    pl_sb_reset(gptScreenLogCtx->sbtTimedMessages);
    uTimedMessageCount = pl_sb_size(gptScreenLogCtx->sbtSortMessages);

    float fStartY = 25.0f;
    for(uint32_t i = 0; i < uTimedMessageCount; i++)
    {

        plVec2 tStartPoint = {fWidth - fWidth * 0.25f, fStartY};

        float fAlpha = pl_maxf(0.2f, 1.0f - (float)((dCurrentTime - gptScreenLogCtx->sbtSortMessages[i].dStartTime) / gptScreenLogCtx->sbtSortMessages[i].dTimeToDisplay));
        if(gptScreenLogCtx->sbtSortMessages[i].dTimeToDisplay < 0.0)
            fAlpha = 1.0f;

        tDrawTextOptions.uColor = gptScreenLogCtx->sbtSortMessages[i].uColor & ~0xFF000000;
        tDrawTextOptions.uColor |=  PL_COLOR_32_RGBA(0.0f, 0.0f, 0.0f, fAlpha);
        tDrawTextOptions.fSize = gptScreenLogCtx->ptFont->fSize * gptScreenLogCtx->sbtSortMessages[i].fTextScale;

        plRect tTextBB = gptDraw->calculate_text_bb(tStartPoint, gptScreenLogCtx->sbtSortMessages[i].acBuffer, tDrawTextOptions);
        tTextBB = pl_rect_expand_vec2(&tTextBB, (plVec2){5.0f, 10.0f});

        if(gptScreenLogCtx->sbtSortMessages[i].bEven)        
            gptDraw->add_rect_rounded_filled(gptScreenLogCtx->ptDrawLayer, tTextBB.tMin, (plVec2){fWidth, tTextBB.tMax.y}, 0.0f, 0, PL_DRAW_RECT_FLAG_NONE, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.2f, 0.2f, 0.2f, fAlpha)});
        else
            gptDraw->add_rect_rounded_filled(gptScreenLogCtx->ptDrawLayer, tTextBB.tMin, (plVec2){fWidth, tTextBB.tMax.y}, 0.0f, 0, PL_DRAW_RECT_FLAG_NONE, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.1f, 0.1f, 0.1f, fAlpha)});

        gptDraw->add_text(gptScreenLogCtx->ptDrawLayer,
            (plVec2){fWidth - fWidth * 0.25f, fStartY},
            gptScreenLogCtx->sbtSortMessages[i].acBuffer,
            tDrawTextOptions);

        fStartY += pl_rect_height(&tTextBB);
        pl_sb_push(gptScreenLogCtx->sbtTimedMessages, gptScreenLogCtx->sbtSortMessages[i]);
    }
    pl_sb_reset(gptScreenLogCtx->sbtSortMessages);


    fStartY = fHeight * 0.25f;
    uint32_t uMessageCount = pl_sb_size(gptScreenLogCtx->sbtMessages);
    uint32_t uLastVisibleIndex = 0;
    for(uint32_t i = 0; i < uMessageCount; i++)
    {

        uint32_t uIndex = uMessageCount - 1 - i;

        tDrawTextOptions.uColor = gptScreenLogCtx->sbtMessages[uIndex].uColor;
        tDrawTextOptions.fSize = gptScreenLogCtx->ptFont->fSize * gptScreenLogCtx->sbtMessages[uIndex].fTextScale;

        plVec2 tTextSize = gptDraw->calculate_text_size(gptScreenLogCtx->sbtMessages[uIndex].acBuffer, tDrawTextOptions);

        fStartY += tTextSize.y + 10.0f;
        gptDraw->add_text(gptScreenLogCtx->ptDrawLayer,
            (plVec2){10.0f, fHeight - fStartY},
            gptScreenLogCtx->sbtMessages[uIndex].acBuffer,
            tDrawTextOptions);

        if(fHeight - fStartY + tTextSize.y < 0)
        {
            break;
        }

        uLastVisibleIndex = uIndex;
        
    }

    if(uLastVisibleIndex > 1)
    {
        pl_sb_del_n(gptScreenLogCtx->sbtMessages, 0, uLastVisibleIndex - 1);
    }

    gptDraw->submit_2d_layer(gptScreenLogCtx->ptDrawLayer);
    return gptScreenLogCtx->ptDrawlist;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_screen_log_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    const plScreenLogI tApi = {
        .initialize     = pl_screen_log_initialize,
        .cleanup        = pl_screen_log_cleanup,
        .add_message    = pl_screen_log_add_message,
        .add_message_ex = pl_screen_log_add_message_ex,
        .add_message_va = pl_screen_log_add_message_va,
        .clear          = pl_screen_log_clear,
        .get_drawlist   = pl_screen_log_get_drawlist
    };
    pl_set_api(ptApiRegistry, plScreenLogI, &tApi);

    const plDataRegistryI* ptDataRegistry  = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    #ifndef PL_UNITY_BUILD
        gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptIOI    = pl_get_api_latest(ptApiRegistry, plIOI);
        gptDraw   = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptIO = gptIOI->get_io();
    #endif

    if(bReload)
    {
        gptScreenLogCtx = ptDataRegistry->get_data("plScreenLogContext");
    }
    else // first load
    {
        static plScreenLogContext gtScreenLogCtx = {0
        };
        gptScreenLogCtx = &gtScreenLogCtx;
        ptDataRegistry->set_data("plScreenLogContext", gptScreenLogCtx);
    }
}

PL_EXPORT void
pl_unload_screen_log_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plScreenLogI* ptApi = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif