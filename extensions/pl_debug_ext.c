/*
   pl_debug_ext.c
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

// pl_ds.h allocators (so they can be tracked)
#define PL_DS_ALLOC(x, FILE, LINE) pl_alloc((x), FILE, LINE)
#define PL_DS_FREE(x)  pl_free((x))

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// core
#include <float.h>
#include "pilotlight.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_debug_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extra
#include "pl_profile.h"
#include "pl_memory.h"
#include "pl_log.h"

// extensions
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_ui_internal.h"
#include "pl_stats_ext.h"
#include "pl_vulkan_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// apis
static plApiRegistryApiI*   gptApiRegistry  = NULL;
static plUiApiI*            ptUi            = NULL;
static plIOApiI*            ptIo            = NULL;
static plStatsApiI*         ptStatsApi      = NULL;
static plDrawApiI*          ptDrawApi       = NULL;
static plTempAllocatorApiI* ptTempMemoryApi = NULL;
static plGraphicsApiI*      ptGfx           = NULL;
static plDataRegistryApiI*  ptDataRegistry  = NULL;

// contexts
static plMemoryContext* ptMemoryCtx = NULL;
static plIOContext*     ptIOCtx     = NULL;

// other
static plDevice*       ptDevice       = NULL;
static plTempAllocator tTempAllocator = {0};

// stat data
static double**     sbppdValues = NULL;
static const char** ppcNames = NULL;
static double***    sbppdFrameValues = NULL; // values to write to
static double*      sbdRawValues = NULL; // raw values
static bool*        sbbValues = NULL;

// profile data
static plProfileSample* sbtSamples = NULL;
static float fDeltaTime = 0.0f;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void pl_show_debug_windows(plDebugApiInfo* ptInfo);

static void pl__show_memory_allocations(bool* bValue);
static void pl__show_profiling         (bool* bValue);
static void pl__show_statistics        (bool* bValue);
static void pl__show_device_memory     (bool* bValue);
static void pl__show_logging           (bool* bValue);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plDebugApiI*
pl_load_debug_api(void)
{
    static plDebugApiI tApi = {
        .show_windows = pl_show_debug_windows
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl_show_debug_windows(plDebugApiInfo* ptInfo)
{
    pl_begin_profile_sample(__FUNCTION__);

    if(ptInfo->bShowMemoryAllocations)
    {
        pl_begin_profile_sample("Memory Allocations");
        pl__show_memory_allocations(&ptInfo->bShowMemoryAllocations);
        pl_end_profile_sample();
    }

    if(ptInfo->bShowProfiling)
    {
        pl_begin_profile_sample("Profiling");
        pl__show_profiling(&ptInfo->bShowProfiling);
        pl_end_profile_sample();
    }

    if(ptInfo->bShowStats)
    {
        pl_begin_profile_sample("Statistics");
        pl__show_statistics(&ptInfo->bShowStats);
        pl_end_profile_sample();
    }

    if(ptInfo->bShowDeviceMemoryAnalyzer)
    {
        pl_begin_profile_sample("Device Memory Analyzer");
        pl__show_device_memory(&ptInfo->bShowDeviceMemoryAnalyzer);
        pl_end_profile_sample();
    }

    if(ptInfo->bShowLogging)
    {
        pl_begin_profile_sample("Logging");
        pl__show_logging(&ptInfo->bShowLogging);
        pl_end_profile_sample();
    }

    pl_end_profile_sample();
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__show_memory_allocations(bool* bValue)
{

    if(ptUi->begin_window("Memory Allocations", bValue, false))
    {
        ptUi->layout_dynamic(0.0f, 1);

        ptUi->text("Active Allocations: %u", ptMemoryCtx->szActiveAllocations);
        ptUi->text("Freed Allocations: %u", ptMemoryCtx->szAllocationFrees);

        static char pcFile[1024] = {0};

        ptUi->layout_template_begin(30.0f);
        ptUi->layout_template_push_static(50.0f);
        ptUi->layout_template_push_variable(300.0f);
        ptUi->layout_template_push_variable(50.0f);
        ptUi->layout_template_push_variable(50.0f);
        ptUi->layout_template_end();

        ptUi->text("%s", "Entry");
        ptUi->text("%s", "File");
        ptUi->text("%s", "Line");
        ptUi->text("%s", "Size");

        ptUi->layout_dynamic(0.0f, 1);
        ptUi->separator();

        ptUi->layout_template_begin(30.0f);
        ptUi->layout_template_push_static(50.0f);
        ptUi->layout_template_push_variable(300.0f);
        ptUi->layout_template_push_variable(50.0f);
        ptUi->layout_template_push_variable(50.0f);
        ptUi->layout_template_end();

        const uint32_t uOriginalAllocationCount = pl_sb_size(ptMemoryCtx->sbtAllocations);
        
        plUiClipper tClipper = {uOriginalAllocationCount};
        while(ptUi->step_clipper(&tClipper))
        {
            for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
            {
                plAllocationEntry tEntry = ptMemoryCtx->sbtAllocations[i];
                strncpy(pcFile, tEntry.pcFile, 1024);
                ptUi->text("%i", i);
                ptUi->text("%s", pcFile);
                ptUi->text("%i", tEntry.iLine);
                ptUi->text("%u", tEntry.szSize);
            } 
        }
        ptUi->end_window();
    }
}

static void
pl__show_profiling(bool* bValue)
{
    static const plVec4 atColors[6] = {
        {0.0f, 1.0f, 1.0f, 0.75f},
        {1.0f, 0.5f, 0.0f, 0.75f},
        {0.0f, 1.0f, 0.0f, 0.75f},
        {0.0f, 0.5f, 1.0f, 0.75f},
        {1.0f, 1.0f, 0.0f, 0.75f},
        {1.0f, 0.0f, 1.0f, 0.75}
    };

    if(ptUi->begin_window("Profiling", bValue, false))
    {
        const plVec2 tWindowSize = ptUi->get_window_size();
        const plVec2 tWindowPos = ptUi->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        plProfileSample* ptSamples = sbtSamples;
        uint32_t uSampleSize = pl_sb_size(sbtSamples);
        if(uSampleSize == 0)
        {
            ptSamples = pl_get_last_frame_samples(&uSampleSize);
            fDeltaTime = ptIOCtx->fDeltaTime;
        }

        ptUi->layout_static(0.0f, 100.0f, 1);
        if(pl_sb_size(sbtSamples) == 0)
        {
            if(ptUi->button("Capture Frame"))
            {
                pl_sb_resize(sbtSamples, uSampleSize);
                memcpy(sbtSamples, ptSamples, sizeof(plProfileSample) * uSampleSize);
            }
        }
        else
        {
            if(ptUi->button("Release Frame"))
            {
                pl_sb_reset(sbtSamples);
            }
        }

        ptUi->layout_dynamic(0.0f, 1);

        ptUi->separator();

        if(ptUi->begin_tab_bar("profiling tabs"))
        {
            if(ptUi->begin_tab("Table"))
            {
                ptUi->layout_template_begin(0.0f);
                ptUi->layout_template_push_variable(300.0f);
                ptUi->layout_template_push_variable(50.0f);
                ptUi->layout_template_push_variable(50.0f);
                ptUi->layout_template_push_variable(100.0f);
                ptUi->layout_template_end();

                ptUi->text("Sample Name");
                ptUi->text("Time (ms)");
                ptUi->text("Start (ms)");
                ptUi->text("Frame Time");

                ptUi->layout_dynamic(0.0f, 1);
                ptUi->separator();

                ptUi->layout_template_begin(0.0f);
                ptUi->layout_template_push_variable(300.0f);
                ptUi->layout_template_push_variable(50.0f);
                ptUi->layout_template_push_variable(50.0f);
                ptUi->layout_template_push_variable(100.0f);
                ptUi->layout_template_end();

                plUiClipper tClipper = {uSampleSize};

                const plVec4 tOriginalProgressColor = ptUi->get_context()->tColorScheme.tProgressBarCol;
                plVec4* tTempProgressColor = &ptUi->get_context()->tColorScheme.tProgressBarCol;
                while(ptUi->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        ptUi->indent(15.0f * (float)(ptSamples[i].uDepth + 1));
                        ptUi->color_text(atColors[ptSamples[i].uDepth % 6], "%s", ptSamples[i].pcName);
                        ptUi->unindent(15.0f * (float)(ptSamples[i].uDepth + 1));
                        ptUi->text("%7.3f", ptSamples[i].dDuration * 1000.0);
                        ptUi->text("%7.3f", ptSamples[i].dStartTime * 1000.0);
                        *tTempProgressColor = atColors[ptSamples[i].uDepth % 6];
                        ptUi->progress_bar((float)(ptSamples[i].dDuration / (double)fDeltaTime), (plVec2){-1.0f, 0.0f}, NULL);
                    } 
                }
                *tTempProgressColor = tOriginalProgressColor;

                ptUi->end_tab();
            }

            if(ptUi->begin_tab("Graph"))
            {

                const plVec2 tParentCursorPos = ptUi->get_cursor_pos();
                ptUi->layout_dynamic(tWindowEnd.y - tParentCursorPos.y - 5.0f, 1);
                if(ptUi->begin_child("timeline"))
                {

                    const plVec2 tChildWindowSize = ptUi->get_window_size();
                    const plVec2 tCursorPos = ptUi->get_cursor_pos();
                    ptUi->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, ptUi->get_window_size().y - 50.0f, uSampleSize + 1);

                    (void)tWindowSize;
                    static double dInitialVisibleTime = 0.016;
                    static double dIncrement = 0.001;

                    const double dVisibleTime = dInitialVisibleTime;
                    const double dMaxTime = pl_maxd(fDeltaTime, dVisibleTime);

                    while(dVisibleTime/dIncrement < 20.0)
                    {
                        dIncrement *= 0.5f;
                    }

                    while(dVisibleTime/dIncrement > 30.0)
                    {
                        dIncrement *= 2.0f;
                    }

                    const double dConvertToPixel = tChildWindowSize.x / dVisibleTime;
                    const double dConvertToTime = dVisibleTime / tChildWindowSize.x;

                    // timeline bar
                    plDrawLayer* ptFgLayer = ptUi->get_window_fg_drawlayer();
                    
                    ptUi->layout_space_push(0.0f, 0.0f, (float)(dMaxTime * dConvertToPixel), 50.0f);
                    const plVec2 tTimelineSize = {(float)(dMaxTime * dConvertToPixel), tWindowEnd.y - tParentCursorPos.y - 15.0f};
                    const plVec2 tTimelineBarSize = {(float)(dMaxTime * dConvertToPixel), 50.0f};
                    ptUi->invisible_button("hitregion", tTimelineSize);
                    bool bHovered = ptUi->was_last_item_hovered();
                    if(bHovered)
                    {
                        
                        const double dStartVisibleTime = dInitialVisibleTime;
                        float fWheel = ptIo->get_mouse_wheel();
                        if(fWheel < 0)      dInitialVisibleTime += dInitialVisibleTime * 0.2;
                        else if(fWheel > 0) dInitialVisibleTime -= dInitialVisibleTime * 0.2;
                        dInitialVisibleTime = pl_clampd(0.0001, dInitialVisibleTime, fDeltaTime);

                        if(fWheel != 0)
                        {
                            const double dNewConvertToPixel = tChildWindowSize.x / dInitialVisibleTime;
                            const double dNewConvertToTime = dInitialVisibleTime / tChildWindowSize.x;

                            const plVec2 tMousePos = ptIo->get_mouse_pos();
                            const double dTimeHovered = (double)dConvertToTime * (double)(tMousePos.x - tParentCursorPos.x + ptUi->get_window_scroll().x);
                            const float fConservedRatio = (tMousePos.x - tParentCursorPos.x) / tChildWindowSize.x;
                            const double dOldPixelStart = dConvertToPixel * dTimeHovered;
                            const double dNewPixelStart = dNewConvertToPixel * (dTimeHovered - fConservedRatio * dInitialVisibleTime);
                            ptUi->set_window_scroll((plVec2){(float)dNewPixelStart, 0.0f});
                        }

                        if(ptIo->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 5.0f))
                        {
                            const plVec2 tWindowScroll = ptUi->get_window_scroll();
                            const plVec2 tMouseDrag = ptIo->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 5.0f);
                            ptUi->set_window_scroll((plVec2){tWindowScroll.x - tMouseDrag.x, tWindowScroll.y});
                            ptIo->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                        }
                    }
                    
                    ptDrawApi->add_rect_filled(ptFgLayer, tCursorPos, pl_add_vec2(tCursorPos, tTimelineBarSize), (plVec4){0.5f, 0.0f, 0.0f, 0.7f});

                    const double dUnitMultiplier = 1000.0;
                    uint32_t uDecimalPlaces = 0;

                    // major ticks
                    const float fScrollStartPosPixel = ptUi->get_window_scroll().x;
                    const float fScrollEndPosPixel = fScrollStartPosPixel + ptUi->get_window_size().x;
                    const double dScrollStartPosTime = dConvertToTime * (double)fScrollStartPosPixel;
                    const double dScrollEndPosTime = dConvertToTime * (double)fScrollEndPosPixel;
                    const uint32_t uScrollStartPosNearestUnit = (uint32_t)round(dScrollStartPosTime / dIncrement);
                    const uint32_t uScrollEndPosNearestUnit = (uint32_t)round(dScrollEndPosTime / dIncrement);

                    while(true)
                    {
                        const double dTime0 = (double)uScrollStartPosNearestUnit * dIncrement;
                        char* pcDecimals = ptTempMemoryApi->printf(&tTempAllocator, "%%0.%uf", uDecimalPlaces);
                        char* pcText0 = ptTempMemoryApi->printf(&tTempAllocator, pcDecimals, (double)dTime0 * dUnitMultiplier);
  
                        const double dTime1 = dTime0 + dIncrement * 0.5;
                        char* pcText1 = ptTempMemoryApi->printf(&tTempAllocator, pcDecimals, (double)dTime1 * dUnitMultiplier);
                        ptTempMemoryApi->reset(&tTempAllocator);

                        if(strcmp(pcText0, pcText1) != 0)
                            break;
                        uDecimalPlaces++;

                        if(uDecimalPlaces > 6)
                            break;
                    }

                    while(true)
                    {
                        const double dTime0 = (double)uScrollEndPosNearestUnit * dIncrement;
                        const double dLineX0 = (double)(dTime0 * dConvertToPixel) + tCursorPos.x;
                        char* pcDecimals = ptTempMemoryApi->printf(&tTempAllocator, " %%0.%uf ms ", uDecimalPlaces);
                        char* pcText0 = ptTempMemoryApi->printf(&tTempAllocator, pcDecimals, (double)dTime0 * dUnitMultiplier);
                        const plRect tBB0 = ptDrawApi->calculate_text_bb(ptUi->get_default_font(), 13.0f, (plVec2){roundf((float)dLineX0), tCursorPos.y + 20.0f}, pcText0, 0.0f);

                        const double dTime1 = dTime0 + dIncrement;
                        const float dLineX1 = (float)(dTime1 * dConvertToPixel) + tCursorPos.x;
                        char* pcText1 = ptTempMemoryApi->printf(&tTempAllocator, pcDecimals, (double)dTime1 * dUnitMultiplier);
                        const plRect tBB1 = ptDrawApi->calculate_text_bb(ptUi->get_default_font(), 13.0f, (plVec2){roundf((float)dLineX1), tCursorPos.y + 20.0f}, pcText1, 0.0f);
                        ptTempMemoryApi->reset(&tTempAllocator);

                        if(!pl_rect_overlaps_rect(&tBB0, &tBB1))
                            break;
                        dIncrement *= 2.0;
                    }

                    
                    const uint32_t uScrollStartPosNearestUnit1 = (uint32_t)round(dScrollStartPosTime / dIncrement);
                    const uint32_t uScrollEndPosNearestUnit1 = (uint32_t)round(dScrollEndPosTime / dIncrement);
                    const double dStartTime = (double)uScrollStartPosNearestUnit1 * dIncrement;
                    const double dEndTime = (double)uScrollEndPosNearestUnit1 * dIncrement;

                    // minor ticks
                    double dCurrentTime = dStartTime - dIncrement * 0.5f;
                    while(dCurrentTime < dEndTime)
                    {
                        const float fLineX = (float)((dCurrentTime * dConvertToPixel)) + tCursorPos.x;
                        ptDrawApi->add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 10.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        dCurrentTime += dIncrement * 0.5f;
                    }

                    // micro ticks
                    dCurrentTime = dStartTime - dIncrement * 0.1f;
                    while(dCurrentTime < dEndTime)
                    {
                        const float fLineX = (float)((dCurrentTime * dConvertToPixel)) + tCursorPos.x;
                        ptDrawApi->add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 5.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        dCurrentTime += dIncrement * 0.1f;
                    }

                    // major ticks
                    dCurrentTime = dStartTime;
                    while(dCurrentTime < dEndTime)
                    {
                        const float fLineX = (float)((dCurrentTime * dConvertToPixel)) + tCursorPos.x;
                        char* pcDecimals = ptTempMemoryApi->printf(&tTempAllocator, "%%0.%uf ms", uDecimalPlaces);
                        char* pcText = ptTempMemoryApi->printf(&tTempAllocator, pcDecimals, (double)dCurrentTime * dUnitMultiplier);
                        const float fTextWidth = ptDrawApi->calculate_text_size(ptUi->get_default_font(), 13.0f, pcText, 0.0f).x;
                        ptDrawApi->add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        ptDrawApi->add_text(ptFgLayer, ptUi->get_default_font(), 13.0f, (plVec2){roundf(fLineX - fTextWidth / 2.0f), tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        ptTempMemoryApi->reset(&tTempAllocator);  
                        dCurrentTime += dIncrement;
                    }

                    const plVec4 tOriginalButtonColor = ptUi->get_context()->tColorScheme.tButtonCol;
                    plVec4* tTempButtonColor = &ptUi->get_context()->tColorScheme.tButtonCol;
                    for(uint32_t i = 0; i < uSampleSize; i++)
                    {
                        const float fPixelWidth = (float)(dConvertToPixel * ptSamples[i].dDuration);
                        const float fPixelStart = (float)(dConvertToPixel * ptSamples[i].dStartTime);
                        ptUi->layout_space_push(fPixelStart, (float)ptSamples[i].uDepth * 25.0f + 55.0f, fPixelWidth, 20.0f);
                        char* pcTempBuffer = ptTempMemoryApi->printf(&tTempAllocator, "%s##pro%u", ptSamples[i].pcName, i);
                        *tTempButtonColor = atColors[ptSamples[i].uDepth % 6];
                        if(ptUi->button(pcTempBuffer))
                        {
                            dInitialVisibleTime = pl_clampd(0.0001, ptSamples[i].dDuration, (double)fDeltaTime);
                            const double dNewConvertToPixel = tChildWindowSize.x / dInitialVisibleTime;
                            const double dNewPixelStart = dNewConvertToPixel * (ptSamples[i].dStartTime + 0.5 * ptSamples[i].dDuration);
                            const double dNewScrollX = dNewPixelStart - dNewConvertToPixel * dInitialVisibleTime * 0.5;
                            ptUi->set_window_scroll((plVec2){(float)dNewScrollX, 0.0f});
                        }
                        ptTempMemoryApi->reset(&tTempAllocator);
                        if(ptUi->was_last_item_hovered())
                        {
                            bHovered = false;
                            ptUi->begin_tooltip();
                            ptUi->color_text(atColors[ptSamples[i].uDepth % 6], "%s", ptSamples[i].pcName);
                            ptUi->text("Duration:   %0.7f seconds", ptSamples[i].dDuration);
                            ptUi->text("Start Time: %0.7f seconds", ptSamples[i].dStartTime);
                            ptUi->color_text(atColors[ptSamples[i].uDepth % 6], "Frame Time: %0.2f %%", 100.0 * ptSamples[i].dDuration / (double)fDeltaTime);
                            ptUi->end_tooltip(); 
                        }
                    }
                    *tTempButtonColor = tOriginalButtonColor;

                    if(bHovered)
                    {
                        const plVec2 tMousePos = ptIo->get_mouse_pos();
                        ptDrawApi->add_line(ptFgLayer, (plVec2){tMousePos.x, tCursorPos.y}, (plVec2){tMousePos.x, tWindowEnd.y}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        char* pcText = ptTempMemoryApi->printf(&tTempAllocator, "%0.6f", (double)dConvertToTime * (double)(tMousePos.x - tParentCursorPos.x + ptUi->get_window_scroll().x));
                        ptDrawApi->add_text(ptFgLayer, ptUi->get_default_font(), 13.0f, tMousePos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        ptTempMemoryApi->reset(&tTempAllocator);
                    }

                    ptUi->layout_space_end();

                    ptUi->end_child();
                }
                ptUi->end_tab();
            }
            ptUi->end_tab_bar();
        }
        ptUi->end_window();
    }
}

static void
pl__show_statistics(bool* bValue)
{

    static uint32_t uSelectedCount = 0;
    static uint32_t uMaxSelectedCount = 0;

    const char** apcTempNames = NULL;
    static float fLegendWidth = 0.0f;

    // selectable values
    
    if(sbbValues == NULL) // first run
    {
        uint32_t uNameCount = 0;
        ppcNames = ptStatsApi->get_names(&uNameCount);
        pl_sb_resize(sbbValues, uNameCount);
        pl_sb_resize(sbppdValues, uNameCount);
        pl_sb_resize(sbppdFrameValues, uNameCount);
        sbbValues[0] = true;
        uSelectedCount++;
        uMaxSelectedCount = uSelectedCount;
        pl_sb_resize(sbdRawValues, PL_STATS_MAX_FRAMES *  uMaxSelectedCount);
        *ptStatsApi->get_counter_data(ppcNames[0]) = sbppdValues[0];
        for(uint32_t i = 0; i < uNameCount; i++)
        {
            sbppdFrameValues[i] = ptStatsApi->get_counter_data(ppcNames[i]);
            float fCurrentWidth = ptDrawApi->calculate_text_size(ptUi->get_default_font(), 13.0f, ppcNames[i], 0.0f).x;
            if(fCurrentWidth > fLegendWidth)
                fLegendWidth = fCurrentWidth;
        }
    }

    if(ptUi->begin_window("Statistics", bValue, false))
    {
        ptUi->text("Frame rate: %.0f FPS", ptIOCtx->fFrameRate);
        ptUi->text("Frame time: %.6f ms", ptIOCtx->fDeltaTime);
        const plVec2 tCursor = ptUi->get_cursor_pos();

        plDrawLayer* ptFgLayer = ptUi->get_window_fg_drawlayer();
        const plVec2 tWindowPos = ptUi->get_window_pos();
        const plVec2 tWindowSize = pl_sub_vec2(ptUi->get_window_size(), pl_sub_vec2(tCursor, tWindowPos));
        
        const plVec2 tWindowEnd = pl_add_vec2(ptUi->get_window_size(), tWindowPos);

        ptUi->layout_template_begin(tWindowSize.y - 15.0f);
        ptUi->layout_template_push_static(fLegendWidth * 2.0f);
        ptUi->layout_template_push_dynamic();
        ptUi->layout_template_end();
      
        if(ptUi->begin_child("left"))
        {
            ptUi->layout_dynamic(0.0f, 1);
 
            const plVec4 tOriginalButtonColor = ptUi->get_context()->tColorScheme.tHeaderCol;
            ptUi->get_context()->tColorScheme.tHeaderCol = (plVec4){0.0f, 0.5f, 0.0f, 0.75f};
            for(uint32_t i = 0; i < pl_sb_size(sbppdValues); i++)
            {
                if(ptUi->selectable(ppcNames[i], &sbbValues[i]))
                {
                    if(sbbValues[i])
                    {
                        uSelectedCount++;
                        if(uSelectedCount > uMaxSelectedCount)
                        {
                            uMaxSelectedCount = uSelectedCount;
                            pl_sb_resize(sbdRawValues, PL_STATS_MAX_FRAMES *  uMaxSelectedCount);
                        }
                        *ptStatsApi->get_counter_data(ppcNames[i]) = sbppdValues[i];
                    }
                    else
                    {
                        uSelectedCount--;
                        *ptStatsApi->get_counter_data(ppcNames[i]) = NULL;
                    }
                }
            }
            ptUi->get_context()->tColorScheme.tHeaderCol = tOriginalButtonColor;
            ptUi->end_child();
        }

        uint32_t uSelectionSlot = 0;
        apcTempNames = ptTempMemoryApi->alloc(&tTempAllocator, sizeof(const char*) * uSelectedCount);
        for(uint32_t i = 0; i < pl_sb_size(sbbValues); i++)
        {
            if(sbbValues[i])
            {
                apcTempNames[uSelectionSlot] = ppcNames[i];
                *sbppdFrameValues[i] = &sbdRawValues[uSelectionSlot * PL_STATS_MAX_FRAMES];
                uSelectionSlot++;
            }
            else
            {
                *sbppdFrameValues[i] = NULL;
            }
        }

        
        if(ptUi->begin_tab_bar("stat tabs"))
        {
            if(ptUi->begin_tab("Plot"))
            {
                
                ptUi->layout_template_begin(tWindowSize.y - 30.0f);
                ptUi->layout_template_push_variable(300.0f);
                ptUi->layout_template_push_static(fLegendWidth * 2.0f + 10.0f);
                ptUi->layout_template_end();
                
                const plVec2 tCursor0 = ptUi->get_cursor_pos();
                const plVec2 tPlotSize = {tWindowEnd.x - tCursor0.x - 10.0f - fLegendWidth - 20.0f, tWindowSize.y - 40.0f};
                const plVec2 tLegendSize = {fLegendWidth + 20.0f, tWindowSize.y - 40.0f};
                ptUi->invisible_button("plot", tPlotSize);

                const plVec2 tCursor1 = ptUi->get_cursor_pos();
                ptUi->invisible_button("legend", tLegendSize);
                ptDrawApi->add_rect_filled(ptFgLayer, 
                    tCursor0, 
                    pl_add_vec2(tCursor0, tPlotSize),
                    (plVec4){0.2f, 0.0f, 0.0f, 0.5f});
                
                // frame times

                static const plVec4 atColors[6] = {
                    {0.0f, 1.0f, 1.0f, 0.75f},
                    {1.0f, 0.5f, 0.0f, 0.75f},
                    {0.0f, 1.0f, 0.0f, 0.75f},
                    {0.0f, 0.5f, 1.0f, 0.75f},
                    {1.0f, 1.0f, 0.0f, 0.75f},
                    {1.0f, 0.0f, 1.0f, 0.75}
                };

                const double dYCenter = tCursor0.y + tPlotSize.y * 0.5f;            
                ptDrawApi->add_text(ptFgLayer, ptUi->get_default_font(), 13.0f, (plVec2){roundf(tCursor0.x), roundf((float)dYCenter)}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "0", 0.0f);
                ptDrawApi->add_line(ptFgLayer, (plVec2){tCursor0.x, (float)dYCenter}, (plVec2){tCursor0.x + tPlotSize.x, (float)dYCenter}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);

                for(uint32_t i = 0; i < uSelectedCount; i++)
                {
                    const plVec4* ptColor = &atColors[i % 6];
                    double* dValues = &sbdRawValues[i * PL_STATS_MAX_FRAMES];
                    double dMaxValue = -DBL_MAX;
                    double dMinValue = DBL_MAX;

                    for(uint32_t j = 0; j < PL_STATS_MAX_FRAMES; j++)
                    {
                        if(dValues[j] > dMaxValue) dMaxValue = dValues[j];
                        if(dValues[j] < dMinValue) dMinValue = dValues[j];
                    }

                    double dYRange = 2.0f * pl_maxd(fabs(dMaxValue), fabs(dMinValue)) * 1.1f;

                    const double dConversion = dYRange != 0.0 ? (tPlotSize.y - 15.0f) / dYRange : 0.0;
                    
                    const double dXIncrement = tPlotSize.x / PL_STATS_MAX_FRAMES;

                    uint32_t uIndexStart = (uint32_t)ptIOCtx->ulFrameCount;

                    const plVec2 tTextPoint = {tCursor1.x, tCursor1.y + i * 15.0f};
                    ptDrawApi->add_rect_filled(ptFgLayer, tTextPoint, (plVec2){tTextPoint.x + 13.0f, tTextPoint.y + 13.0f}, *ptColor);
                    ptDrawApi->add_text(ptFgLayer, ptUi->get_default_font(), 13.0f, (plVec2){roundf(tTextPoint.x + 15.0f), roundf(tTextPoint.y)}, *ptColor, apcTempNames[i], 0.0f);
        
                    for(uint32_t j = 0; j < PL_STATS_MAX_FRAMES - 1; j++)
                    {
                        uint32_t uActualIndex0 = (uIndexStart + j) % PL_STATS_MAX_FRAMES;
                        uint32_t uActualIndex1 = (uIndexStart + j + 1) % PL_STATS_MAX_FRAMES;
                        const plVec2 tLineStart = {tCursor0.x + (float)(j * dXIncrement), (float)(dYCenter - dValues[uActualIndex0] * dConversion)};
                        const plVec2 tLineEnd = {tCursor0.x + (float)((j + 1) * dXIncrement), (float)(dYCenter - dValues[uActualIndex1] * dConversion)};
                        ptDrawApi->add_line(ptFgLayer, tLineStart, tLineEnd, *ptColor, 1.0f);
                    }
                    
                }
                ptUi->end_tab();
            }

            if(ptUi->begin_tab("Table"))
            {
                ptUi->layout_template_begin(0.0f);
                ptUi->layout_template_push_static(35.0f);
                for(uint32_t i = 0; i < uSelectedCount; i++)
                    ptUi->layout_template_push_static(100.0f);
                ptUi->layout_template_end();

                ptUi->text("Stat");
                for(uint32_t i = 0; i < uSelectedCount; i++)
                {
                    const float fXPos = ptUi->get_cursor_pos().x - 5.0f;
                    const float fYPos = ptUi->get_cursor_pos().y;
                    ptDrawApi->add_line(ptFgLayer, (plVec2){fXPos, fYPos}, (plVec2){fXPos, 3000.0f}, (plVec4){0.7f, 0.0f, 0.0f, 1.0f}, 1.0f);
                    ptUi->button(apcTempNames[i]);
                }

                ptUi->layout_dynamic(0.0f, 1);
                ptUi->separator();

                ptUi->layout_template_begin(0.0f);
                ptUi->layout_template_push_static(35.0f);
                for(uint32_t i = 0; i < uSelectedCount; i++)
                    ptUi->layout_template_push_static(100.0f);
                ptUi->layout_template_end();

                uint32_t uIndexStart = (uint32_t)ptIOCtx->ulFrameCount;

                plUiClipper tClipper = {PL_STATS_MAX_FRAMES};
                while(ptUi->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        ptUi->text("%u", i);
                        for(uint32_t j = 0; j < uSelectedCount; j++)
                        {
                            double* dValues = &sbdRawValues[j * PL_STATS_MAX_FRAMES];
                            uint32_t uActualIndex0 = (uIndexStart + j) % PL_STATS_MAX_FRAMES;
                            ptUi->text("%13.6f", dValues[uActualIndex0]);
                        }
                    } 
                }

                ptUi->end_tab();
            }
            ptUi->end_tab_bar();
        }
        ptTempMemoryApi->reset(&tTempAllocator);
        ptUi->end_window();
    } 
}

static void
pl__show_device_memory(bool* bValue)
{
    if(!ptDevice)
        ptDevice = ptDataRegistry->get_data("device");
        
    if(ptUi->begin_window("Device Memory Analyzer", bValue, false))
    {
        plDrawLayer* ptFgLayer = ptUi->get_window_fg_drawlayer();
        const plVec2 tWindowSize = ptUi->get_window_size();
        const plVec2 tWindowPos = ptUi->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        uint32_t uBlockCount = 0;
        plDeviceAllocationBlock* sbtBlocks = ptDevice->tStagingCachedAllocator.blocks(ptDevice->tStagingCachedAllocator.ptInst, &uBlockCount);

        if(uBlockCount > 0)
        {
            ptUi->text("Device Memory: Staging Cached");

            ptUi->layout_template_begin(30.0f);
            ptUi->layout_template_push_static(150.0f);
            ptUi->layout_template_push_variable(300.0f);
            ptUi->layout_template_end();

            static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

            for(uint32_t i = 0; i < uBlockCount; i++)
            {
                // ptAppData->tTempAllocator
                plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                char* pcTempBuffer0 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u: %0.1fMB##sc", i, ((double)ptBlock->ulSize)/1000000.0);
                char* pcTempBuffer1 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u##sc", i);

                ptUi->button(pcTempBuffer0);
                

                plVec2 tCursor0 = ptUi->get_cursor_pos();
                const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

                ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                if(ptUi->was_last_item_active())
                    ptDrawApi->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);

                if(ptUi->was_last_item_hovered())
                {
                    ptUi->begin_tooltip();
                    ptUi->text(ptBlock->sbtRanges[0].pcName);
                    ptUi->end_tooltip();
                }

                ptTempMemoryApi->reset(&tTempAllocator);
            }

        }

        sbtBlocks = ptDevice->tStagingUnCachedAllocator.blocks(ptDevice->tStagingUnCachedAllocator.ptInst, &uBlockCount);
        if(uBlockCount > 0)
        {
            ptUi->layout_dynamic(0.0f, 1);
            ptUi->separator();
            ptUi->text("Device Memory: Staging Uncached");

            ptUi->layout_template_begin(30.0f);
            ptUi->layout_template_push_static(150.0f);
            ptUi->layout_template_push_variable(300.0f);
            ptUi->layout_template_end();

            static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

            for(uint32_t i = 0; i < uBlockCount; i++)
            {
                plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                char* pcTempBuffer0 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u: %0.1fMB##suc", i, ((double)ptBlock->ulSize)/1000000.0);
                char* pcTempBuffer1 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u##suc", i);

                ptUi->button(pcTempBuffer0);
                

                plVec2 tCursor0 = ptUi->get_cursor_pos();
                const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

                ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f});
                ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                if(ptUi->was_last_item_active())
                    ptDrawApi->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);

                if(ptUi->was_last_item_hovered())
                {
                    ptUi->begin_tooltip();
                    ptUi->text(ptBlock->sbtRanges[0].pcName);
                    ptUi->end_tooltip();
                }

                ptTempMemoryApi->reset(&tTempAllocator);
            }

        }

        sbtBlocks = ptDevice->tLocalBuddyAllocator.blocks(ptDevice->tLocalBuddyAllocator.ptInst, &uBlockCount);
        if(uBlockCount > 0)
        {

            ptUi->layout_dynamic(0.0f, 1);
            ptUi->separator();
            ptUi->text("Device Memory: Local Buddy");

            ptUi->layout_template_begin(30.0f);
            ptUi->layout_template_push_static(150.0f);
            ptUi->layout_template_push_variable(300.0f);
            ptUi->layout_template_end();

            uint32_t uNodeCount = 0;
            plDeviceAllocationNode* sbtNodes = ptDevice->tLocalBuddyAllocator.nodes(ptDevice->tLocalBuddyAllocator.ptInst, &uNodeCount);
            const char** sbDebugNames = ptDevice->tLocalBuddyAllocator.names(ptDevice->tLocalBuddyAllocator.ptInst, &uNodeCount);

            const uint32_t uNodesPerBlock = uNodeCount / uBlockCount;
            for(uint32_t i = 0; i < uBlockCount; i++)
            {
                plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                char* pcTempBuffer0 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u: 256 MB##b", i);
                char* pcTempBuffer1 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u ##b", i);
                ptUi->button(pcTempBuffer0);

                plVec2 tCursor0 = ptUi->get_cursor_pos();
                const plVec2 tMousePos = ptIo->get_mouse_pos();
                uint32_t uHoveredNode = 0;
                const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                ptDrawApi->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x, tCursor0.y}, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.234f, 0.703f, 0.234f, 1.0f}); 
                bool bFreeNode = false;

                for(uint32_t j = 0; j < uNodesPerBlock; j++)
                {
                    plDeviceAllocationNode* ptNode = &sbtNodes[uNodesPerBlock * i + j];

                    if(ptNode->ulSizeWasted == ptNode->ulSize)
                        continue;

                    float fStart = fWidthAvailable * ((float) ptNode->ulOffset) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                    
                    if(ptNode->ulSizeWasted > ptNode->ulSize)
                    {
                        float fFreeWidth = fWidthAvailable * ((float)ptNode->ulSize) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                        if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fStart + fFreeWidth)
                        uHoveredNode = (uint32_t)ptNode->uNodeIndex;
                        bFreeNode = true;
                        continue;
                    }

                    float fUsedWidth = fWidthAvailable * ((float)ptNode->ulSize - ptNode->ulSizeWasted) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                    
                    ptDrawApi->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x + fStart, tCursor0.y}, (plVec2){tCursor0.x + fStart + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f}); 

                    if(ptNode->ulSizeWasted > 0)
                    {
                        
                        const float fWastedWidth = fWidthAvailable * ((float)ptNode->ulSizeWasted) / (float)PL_DEVICE_ALLOCATION_BLOCK_SIZE;
                        const float fWasteStart = fStart + fUsedWidth;
                        ptDrawApi->add_rect_filled(ptFgLayer, (plVec2){tCursor0.x + fWasteStart, tCursor0.y}, (plVec2){tCursor0.x + fWasteStart + fWastedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f}); 
                        if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fWasteStart + fWastedWidth)
                            uHoveredNode = (uint32_t)ptNode->uNodeIndex;
                    }
                    else if(tMousePos.x > tCursor0.x + fStart && tMousePos.x < tCursor0.x + fStart + fUsedWidth)
                        uHoveredNode = (uint32_t)ptNode->uNodeIndex;
                }

                if(ptUi->was_last_item_hovered())
                {
                    ptUi->begin_tooltip();
                    ptUi->text(sbDebugNames[uHoveredNode]);
                    ptUi->text("Total Size:  %u", sbtNodes[uHoveredNode].ulSize);
                    ptUi->text("Size Used:   %u", bFreeNode ? 0 : sbtNodes[uHoveredNode].ulSize - sbtNodes[uHoveredNode].ulSizeWasted);
                    ptUi->text("Size Wasted: %u", bFreeNode ? 0 : sbtNodes[uHoveredNode].ulSizeWasted);
                    ptUi->text("Offset:      %u", sbtNodes[uHoveredNode].ulOffset);
                    ptUi->text("Memory Type: %u", sbtNodes[uHoveredNode].uMemoryType);
                    ptUi->end_tooltip();
                }
            }

        }

        sbtBlocks = ptDevice->tLocalDedicatedAllocator.blocks(ptDevice->tLocalDedicatedAllocator.ptInst, &uBlockCount);
        if(uBlockCount > 0)
        {

            ptUi->layout_dynamic(0.0f, 1);
            ptUi->separator();
            ptUi->text("Device Memory: Local Dedicated");

            ptUi->layout_template_begin(30.0f);
            ptUi->layout_template_push_static(150.0f);
            ptUi->layout_template_push_variable(300.0f);
            ptUi->layout_template_end();

            static const uint64_t ulMaxBlockSize = PL_DEVICE_ALLOCATION_BLOCK_SIZE;

            for(uint32_t i = 0; i < uBlockCount; i++)
            {
                plDeviceAllocationBlock* ptBlock = &sbtBlocks[i];
                char* pcTempBuffer0 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u: %0.1fMB###d", i, ((double)ptBlock->ulSize)/1000000.0);
                char* pcTempBuffer1 = ptTempMemoryApi->printf(&tTempAllocator, "Block %u###d", i);

                ptUi->button(pcTempBuffer0);
                
                plVec2 tCursor0 = ptUi->get_cursor_pos();
                const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                float fUsedWidth = fWidthAvailable * ((float)ptBlock->sbtRanges[0].tAllocation.ulSize) / (float)ulMaxBlockSize;

                if(fUsedWidth < 10.0f)
                    fUsedWidth = 10.0f;
                else if (fUsedWidth > 500.0f)
                    fUsedWidth = 525.0f;
                    
                if (fTotalWidth > 500.0f)
                    fTotalWidth = 525.0f;
                
                ptUi->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                if(ptUi->was_last_item_active())
                {
                    ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f});
                    ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                    ptDrawApi->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x +  fTotalWidth, 30.0f + tCursor0.y}, (plVec4){ 1.f, 1.0f, 1.0f, 1.0f}, 2.0f);
                }
                else
                {
                    ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.703f, 0.234f, 1.0f});
                    ptDrawApi->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fUsedWidth, 30.0f + tCursor0.y}, (plVec4){0.703f, 0.234f, 0.234f, 1.0f});
                }

                if(ptUi->was_last_item_hovered())
                {
                    ptUi->begin_tooltip();
                    ptUi->text(ptBlock->sbtRanges[0].pcName);
                    ptUi->end_tooltip();
                }

                ptTempMemoryApi->reset(&tTempAllocator);
            }
        }

        ptUi->end_window();
    }
}

static void
pl__show_logging(bool* bValue)
{

    static const plVec4 atColors[6] = {
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 1.0f}
    };

    if(ptUi->begin_window("Logging", bValue, false))
    {
        const plVec2 tWindowSize = ptUi->get_window_size();
        const plVec2 tWindowPos = ptUi->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        static bool bActiveLevels[6] = { true, true, true, true, true, true};

        ptUi->layout_dynamic(0.0f, 6);
        ptUi->checkbox("Trace", &bActiveLevels[0]);
        ptUi->checkbox("Debug", &bActiveLevels[1]);
        ptUi->checkbox("Info", &bActiveLevels[2]);
        ptUi->checkbox("Warn", &bActiveLevels[3]);
        ptUi->checkbox("Error", &bActiveLevels[4]);
        ptUi->checkbox("Fatal", &bActiveLevels[5]);

        bool bUseClipper = true;
        for(uint32_t i = 0; i < 6; i++)
        {
            if(bActiveLevels[i] == false)
            {
                bUseClipper = false;
                break;
            }
        }

        ptUi->layout_dynamic(0.0f, 1);
        if(ptUi->begin_tab_bar("tab bar"))
        {
            uint32_t uChannelCount = 0;
            plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
            for(uint32_t i = 0; i < uChannelCount; i++)
            {
                plLogChannel* ptChannel = &ptChannels[i];
                
                if(ptChannel->tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER)
                {
                    if(ptUi->begin_tab(ptChannel->pcName))
                    {
                        const plVec2 tCursorPos = ptUi->get_cursor_pos();
                        ptUi->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(ptUi->begin_child(ptChannel->pcName))
                        {
                            ptUi->layout_dynamic(0.0f, 1);
                            const uint64_t uIndexStart = ptChannel->uEntryCount;
                            const uint64_t uLogCount = pl_min(PL_LOG_CYCLIC_BUFFER_SIZE, ptChannel->uEntryCount);

                            
                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)ptChannel->uEntryCount};
                                while(ptUi->step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogEntry* ptEntry = &ptChannel->atEntries[uActualIndex0];
                                        ptUi->color_text(atColors[ptEntry->uLevel / 1000 - 5], ptEntry->cPBuffer);
                                    } 
                                }
                            }
                            else
                            {
                                    for(uint32_t j = i; j < ptChannel->uEntryCount; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogEntry* ptEntry = &ptChannel->atEntries[uActualIndex0];
                                        if(bActiveLevels[ptEntry->uLevel / 1000 - 5])
                                            ptUi->color_text(atColors[ptEntry->uLevel / 1000 - 5], ptEntry->cPBuffer);
                                    }  
                            }
                            ptUi->end_child();
                        }
                        ptUi->end_tab();
                    }
                }
                else if(ptChannel->tType & PL_CHANNEL_TYPE_BUFFER)
                {
                    if(ptUi->begin_tab(ptChannel->pcName))
                    {
                        const plVec2 tCursorPos = ptUi->get_cursor_pos();
                        ptUi->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(ptUi->begin_child(ptChannel->pcName))
                        {
                            ptUi->layout_dynamic(0.0f, 1);

                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)ptChannel->uEntryCount};
                                while(ptUi->step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        plLogEntry* ptEntry = &ptChannel->pEntries[j];
                                        ptUi->color_text(atColors[ptEntry->uLevel / 1000 - 5], ptEntry->cPBuffer);
                                    } 
                                }
                            }
                            else
                            {
                                for(uint32_t j = 0; j < ptChannel->uEntryCount; j++)
                                {
                                    plLogEntry* ptEntry = &ptChannel->pEntries[j];
                                    if(bActiveLevels[ptEntry->uLevel / 1000 - 5])
                                        ptUi->color_text(atColors[ptEntry->uLevel / 1000 - 5], ptEntry->cPBuffer);
                                } 
                            }
                            ptUi->end_child();
                        }
                        ptUi->end_tab();
                    }
                }
            }

            ptUi->end_tab_bar();
        }
        ptUi->end_window();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_debug_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    gptApiRegistry = ptApiRegistry;
    ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    ptMemoryCtx = ptDataRegistry->get_data("memory");
    pl_set_memory_context(ptMemoryCtx);
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    ptUi = ptApiRegistry->first(PL_API_UI);
    ptIo = ptApiRegistry->first(PL_API_IO);
    ptStatsApi = ptApiRegistry->first(PL_API_STATS);
    ptTempMemoryApi = ptApiRegistry->first(PL_API_TEMP_ALLOCATOR);
    ptIOCtx = ptIo->get_context();
    ptDrawApi = ptApiRegistry->first(PL_API_DRAW);
    ptGfx = ptApiRegistry->first(PL_API_GRAPHICS);
    ptDevice = ptDataRegistry->get_data("device");

    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEBUG), pl_load_debug_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_DEBUG, pl_load_debug_api());
    }
}

PL_EXPORT void
pl_unload_debug_ext(plApiRegistryApiI* ptApiRegistry)
{
    ptTempMemoryApi->free(&tTempAllocator);

    for(uint32_t i = 0; i < pl_sb_size(sbppdValues); i++)
    {
        *ptStatsApi->get_counter_data(ppcNames[i]) = NULL;
    }

    pl_sb_free(sbppdValues);
    pl_sb_free(sbppdFrameValues);
    pl_sb_free(sbdRawValues);
    pl_sb_free(sbbValues);
}
