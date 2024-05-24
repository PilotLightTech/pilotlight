/*
   pl_debug_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// core
#include <float.h>
#include "pilotlight.h"
#include "pl_ds.h"
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
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_gpu_allocators_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// apis
static const plApiRegistryI*   gptApiRegistry   = NULL;
static const plStatsI*         ptStatsApi       = NULL;
static const plDataRegistryI*  ptDataRegistry   = NULL;
static const plGPUAllocatorsI* gptGpuAllocators = NULL;
static const plUiI*            gptUI            = NULL;
static const plIOI*            gptIO            = NULL;
static const plDrawI*          gptDraw          = NULL;

// contexts
static plMemoryContext* ptMemoryCtx = NULL;
static plIO*            ptIOCtx     = NULL;

// other
static plDevice*       ptDevice       = NULL;
static plTempAllocator tTempAllocator = {0};

// stat data
static double**     sbppdValues      = NULL;
static const char** ppcNames         = NULL;
static double***    sbppdFrameValues = NULL; // values to write to
static double*      sbdRawValues     = NULL; // raw values
static bool*        sbbValues        = NULL;

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

static const plDebugApiI*
pl_load_debug_api(void)
{
    static const plDebugApiI tApi = {
        .show_debug_windows = pl_show_debug_windows
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

    if(!ptDevice)
        ptDevice = ptDataRegistry->get_data("device");

    if(gptUI->begin_window("Memory Allocations", bValue, false))
    {
        gptUI->layout_dynamic(0.0f, 1);
        if(ptMemoryCtx->szMemoryUsage > 1000000000)
            gptUI->text("General Memory Usage:       %0.3f gb", (double)ptMemoryCtx->szMemoryUsage / 1000000000);
        else if(ptMemoryCtx->szMemoryUsage > 1000000)
            gptUI->text("General Memory Usage:       %0.3f mb", (double)ptMemoryCtx->szMemoryUsage / 1000000);
        else if(ptMemoryCtx->szMemoryUsage > 1000)
            gptUI->text("General Memory Usage:       %0.3f kb", (double)ptMemoryCtx->szMemoryUsage / 1000);
        else
            gptUI->text("General Memory Usage:       %llu bytes", (double)ptMemoryCtx->szMemoryUsage);
    
        if(ptDevice->ptGraphics->szHostMemoryInUse > 1000000000)
            gptUI->text("Host Graphics Memory Usage: %0.3f gb", (double)ptDevice->ptGraphics->szHostMemoryInUse / 1000000000);
        else if(ptDevice->ptGraphics->szHostMemoryInUse > 1000000)
            gptUI->text("Host Graphics Memory Usage: %0.3f mb", (double)ptDevice->ptGraphics->szHostMemoryInUse / 1000000);
        else if(ptDevice->ptGraphics->szHostMemoryInUse > 1000)
            gptUI->text("Host Graphics Memory Usage: %0.3f kb", (double)ptDevice->ptGraphics->szHostMemoryInUse / 1000);
        else
            gptUI->text("Host Graphics Memory Usage: %llu bytes", (double)ptDevice->ptGraphics->szHostMemoryInUse);

        gptUI->text("Active Allocations:         %u", ptMemoryCtx->szActiveAllocations);
        gptUI->text("Freed Allocations:          %u", ptMemoryCtx->szAllocationFrees);

        static char pcFile[1024] = {0};

        gptUI->layout_template_begin(30.0f);
        gptUI->layout_template_push_static(50.0f);
        gptUI->layout_template_push_variable(300.0f);
        gptUI->layout_template_push_variable(50.0f);
        gptUI->layout_template_push_variable(50.0f);
        gptUI->layout_template_end();

        gptUI->text("%s", "Entry");
        gptUI->text("%s", "File");
        gptUI->text("%s", "Line");
        gptUI->text("%s", "Size");

        gptUI->layout_dynamic(0.0f, 1);
        gptUI->separator();

        gptUI->layout_template_begin(30.0f);
        gptUI->layout_template_push_static(50.0f);
        gptUI->layout_template_push_variable(300.0f);
        gptUI->layout_template_push_variable(50.0f);
        gptUI->layout_template_push_variable(50.0f);
        gptUI->layout_template_end();

        const uint32_t uOriginalAllocationCount = pl_sb_size(ptMemoryCtx->sbtAllocations);
        
        plUiClipper tClipper = {uOriginalAllocationCount};
        while(gptUI->step_clipper(&tClipper))
        {
            for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
            {
                plAllocationEntry tEntry = ptMemoryCtx->sbtAllocations[i];
                strncpy(pcFile, tEntry.pcFile, 1024);
                gptUI->text("%i", i);
                gptUI->text("%s", pcFile);
                gptUI->text("%i", tEntry.iLine);
                gptUI->text("%u", tEntry.szSize);
            } 
        }
        gptUI->end_window();
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

    static bool bFirstRun = true;

    if(gptUI->begin_window("Profiling (WIP)", bValue, false))
    {
        const plVec2 tWindowSize = gptUI->get_window_size();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        plProfileSample* ptSamples = NULL;
        uint32_t uSampleSize = 0;
        if(bFirstRun && ptIOCtx->ulFrameCount == 1)
        {
            ptSamples = pl_get_last_frame_samples(&uSampleSize);
            pl_sb_resize(sbtSamples, uSampleSize);
            memcpy(sbtSamples, ptSamples, sizeof(plProfileSample) * uSampleSize);
            fDeltaTime = ptIOCtx->fDeltaTime;
            bFirstRun = false;
        }
        else
        {
            ptSamples = sbtSamples;
            uSampleSize = pl_sb_size(sbtSamples);
            bFirstRun = false;
        }

        if(uSampleSize == 0)
        {
            ptSamples = pl_get_last_frame_samples(&uSampleSize);
            fDeltaTime = ptIOCtx->fDeltaTime;
        }

        gptUI->layout_static(0.0f, 100.0f, 2);
        if(pl_sb_size(sbtSamples) == 0)
        {
            if(gptUI->button("Capture Frame"))
            {
                pl_sb_resize(sbtSamples, uSampleSize);
                memcpy(sbtSamples, ptSamples, sizeof(plProfileSample) * uSampleSize);
            }
        }
        else
        {
            if(gptUI->button("Release Frame"))
            {
                pl_sb_reset(sbtSamples);
            }
        }
        gptUI->text("Frame Time: %0.3f", fDeltaTime);

        gptUI->layout_dynamic(0.0f, 1);

        gptUI->separator();

        if(gptUI->begin_tab_bar("profiling tabs"))
        {
            if(gptUI->begin_tab("Table"))
            {
                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_variable(300.0f);
                gptUI->layout_template_push_variable(50.0f);
                gptUI->layout_template_push_variable(50.0f);
                gptUI->layout_template_push_variable(100.0f);
                gptUI->layout_template_end();

                gptUI->text("Sample Name");
                gptUI->text("Time (ms)");
                gptUI->text("Start (ms)");
                gptUI->text("Frame Time");

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator();

                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_variable(300.0f);
                gptUI->layout_template_push_variable(50.0f);
                gptUI->layout_template_push_variable(50.0f);
                gptUI->layout_template_push_variable(100.0f);
                gptUI->layout_template_end();

                plUiClipper tClipper = {uSampleSize};

                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        gptUI->indent(15.0f * (float)(ptSamples[i].uDepth + 1));
                        gptUI->color_text(atColors[ptSamples[i].uDepth % 6], "%s", ptSamples[i].pcName);
                        gptUI->unindent(15.0f * (float)(ptSamples[i].uDepth + 1));
                        gptUI->text("%7.3f", ptSamples[i].dDuration * 1000.0);
                        gptUI->text("%7.3f", ptSamples[i].dStartTime * 1000.0);
                        gptUI->push_theme_color(PL_UI_COLOR_PROGRESS_BAR, &atColors[ptSamples[i].uDepth % 6]);
                        gptUI->progress_bar((float)(ptSamples[i].dDuration / (double)fDeltaTime), (plVec2){-1.0f, 0.0f}, NULL);
                        gptUI->pop_theme_color(1);
                    } 
                }
                gptUI->end_tab();
            }

            if(gptUI->begin_tab("Graph"))
            {

                const plVec2 tParentCursorPos = gptUI->get_cursor_pos();
                gptUI->layout_dynamic(tWindowEnd.y - tParentCursorPos.y - 5.0f, 1);
                if(gptUI->begin_child("timeline"))
                {

                    const plVec2 tChildWindowSize = gptUI->get_window_size();
                    const plVec2 tCursorPos = gptUI->get_cursor_pos();
                    gptUI->layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, gptUI->get_window_size().y - 50.0f, uSampleSize + 1);

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
                    plDrawLayer2D* ptFgLayer = gptUI->get_window_fg_drawlayer();
                    
                    gptUI->layout_space_push(0.0f, 0.0f, (float)(dMaxTime * dConvertToPixel), 50.0f);
                    const plVec2 tTimelineSize = {(float)(dMaxTime * dConvertToPixel), tWindowEnd.y - tParentCursorPos.y - 15.0f};
                    const plVec2 tTimelineBarSize = {(float)(dMaxTime * dConvertToPixel), 50.0f};
                    // pl_invisible_button("hitregion", tTimelineSize);

                    // bool bHovered = pl_was_last_item_hovered();
                    const plVec2 tMousePos = gptIO->get_mouse_pos();
                    const plRect tWidgetRect = pl_calculate_rect(tCursorPos, tTimelineSize);
                    bool bHovered = pl_rect_contains_point(&tWidgetRect, tMousePos);
                    if(bHovered)
                    {
                        
                        const double dStartVisibleTime = dInitialVisibleTime;
                        float fWheel = gptIO->get_mouse_wheel();
                        if(fWheel < 0)      dInitialVisibleTime += dInitialVisibleTime * 0.2;
                        else if(fWheel > 0) dInitialVisibleTime -= dInitialVisibleTime * 0.2;
                        dInitialVisibleTime = pl_clampd(0.0001, dInitialVisibleTime, fDeltaTime);

                        if(fWheel != 0)
                        {
                            const double dNewConvertToPixel = tChildWindowSize.x / dInitialVisibleTime;
                            const double dNewConvertToTime = dInitialVisibleTime / tChildWindowSize.x;

                            const double dTimeHovered = (double)dConvertToTime * (double)(tMousePos.x - tParentCursorPos.x + gptUI->get_window_scroll().x);
                            const float fConservedRatio = (tMousePos.x - tParentCursorPos.x) / tChildWindowSize.x;
                            const double dOldPixelStart = dConvertToPixel * dTimeHovered;
                            const double dNewPixelStart = dNewConvertToPixel * (dTimeHovered - fConservedRatio * dInitialVisibleTime);
                            gptUI->set_window_scroll((plVec2){(float)dNewPixelStart, 0.0f});
                        }

                        if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 5.0f))
                        {
                            const plVec2 tWindowScroll = gptUI->get_window_scroll();
                            const plVec2 tMouseDrag = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 5.0f);
                            gptUI->set_window_scroll((plVec2){tWindowScroll.x - tMouseDrag.x, tWindowScroll.y});
                            gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                        }
                    }
                    
                    gptDraw->add_rect_filled(ptFgLayer, tCursorPos, pl_add_vec2(tCursorPos, tTimelineBarSize), (plVec4){0.5f, 0.0f, 0.0f, 0.7f});

                    const double dUnitMultiplier = 1000.0;
                    uint32_t uDecimalPlaces = 0;

                    // major ticks
                    const float fScrollStartPosPixel = gptUI->get_window_scroll().x;
                    const float fScrollEndPosPixel = fScrollStartPosPixel + gptUI->get_window_size().x;
                    const double dScrollStartPosTime = dConvertToTime * (double)fScrollStartPosPixel;
                    const double dScrollEndPosTime = dConvertToTime * (double)fScrollEndPosPixel;
                    const uint32_t uScrollStartPosNearestUnit = (uint32_t)round(dScrollStartPosTime / dIncrement);
                    const uint32_t uScrollEndPosNearestUnit = (uint32_t)round(dScrollEndPosTime / dIncrement);

                    while(true)
                    {
                        const double dTime0 = (double)uScrollStartPosNearestUnit * dIncrement;
                        char* pcDecimals = pl_temp_allocator_sprintf(&tTempAllocator, "%%0.%uf", uDecimalPlaces);
                        char* pcText0 = pl_temp_allocator_sprintf(&tTempAllocator, pcDecimals, (double)dTime0 * dUnitMultiplier);
  
                        const double dTime1 = dTime0 + dIncrement * 0.5;
                        char* pcText1 = pl_temp_allocator_sprintf(&tTempAllocator, pcDecimals, (double)dTime1 * dUnitMultiplier);
                        pl_temp_allocator_reset(&tTempAllocator);

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
                        char* pcDecimals = pl_temp_allocator_sprintf(&tTempAllocator, " %%0.%uf ms ", uDecimalPlaces);
                        char* pcText0 = pl_temp_allocator_sprintf(&tTempAllocator, pcDecimals, (double)dTime0 * dUnitMultiplier);
                        const plRect tBB0 = gptDraw->calculate_text_bb(gptUI->get_default_font(), 13.0f, (plVec2){roundf((float)dLineX0), tCursorPos.y + 20.0f}, pcText0, 0.0f);

                        const double dTime1 = dTime0 + dIncrement;
                        const float dLineX1 = (float)(dTime1 * dConvertToPixel) + tCursorPos.x;
                        char* pcText1 = pl_temp_allocator_sprintf(&tTempAllocator, pcDecimals, (double)dTime1 * dUnitMultiplier);
                        const plRect tBB1 = gptDraw->calculate_text_bb(gptUI->get_default_font(), 13.0f, (plVec2){roundf((float)dLineX1), tCursorPos.y + 20.0f}, pcText1, 0.0f);
                        pl_temp_allocator_reset(&tTempAllocator);

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
                        gptDraw->add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 10.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        dCurrentTime += dIncrement * 0.5f;
                    }

                    // micro ticks
                    dCurrentTime = dStartTime - dIncrement * 0.1f;
                    while(dCurrentTime < dEndTime)
                    {
                        const float fLineX = (float)((dCurrentTime * dConvertToPixel)) + tCursorPos.x;
                        gptDraw->add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 5.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        dCurrentTime += dIncrement * 0.1f;
                    }

                    // major ticks
                    dCurrentTime = dStartTime;
                    while(dCurrentTime < dEndTime)
                    {
                        const float fLineX = (float)((dCurrentTime * dConvertToPixel)) + tCursorPos.x;
                        char* pcDecimals = pl_temp_allocator_sprintf(&tTempAllocator, "%%0.%uf ms", uDecimalPlaces);
                        char* pcText = pl_temp_allocator_sprintf(&tTempAllocator, pcDecimals, (double)dCurrentTime * dUnitMultiplier);
                        const float fTextWidth = gptDraw->calculate_text_size(gptUI->get_default_font(), 13.0f, pcText, 0.0f).x;
                        gptDraw->add_line(ptFgLayer, (plVec2){fLineX, tCursorPos.y}, (plVec2){fLineX, tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), 13.0f, (plVec2){roundf(fLineX - fTextWidth / 2.0f), tCursorPos.y + 20.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        pl_temp_allocator_reset(&tTempAllocator);  
                        dCurrentTime += dIncrement;
                    }

                    for(uint32_t i = 0; i < uSampleSize; i++)
                    {
                        const float fPixelWidth = (float)(dConvertToPixel * ptSamples[i].dDuration);
                        const float fPixelStart = (float)(dConvertToPixel * ptSamples[i].dStartTime);
                        gptUI->layout_space_push(fPixelStart, (float)ptSamples[i].uDepth * 25.0f + 55.0f, fPixelWidth, 20.0f);
                        char* pcTempBuffer = pl_temp_allocator_sprintf(&tTempAllocator, "%s##pro%u", ptSamples[i].pcName, i);
                        gptUI->push_theme_color(PL_UI_COLOR_BUTTON, &atColors[ptSamples[i].uDepth % 6]);
                        if(gptUI->button(pcTempBuffer))
                        {
                            dInitialVisibleTime = pl_clampd(0.0001, ptSamples[i].dDuration, (double)fDeltaTime);
                            const double dNewConvertToPixel = tChildWindowSize.x / dInitialVisibleTime;
                            const double dNewPixelStart = dNewConvertToPixel * (ptSamples[i].dStartTime + 0.5 * ptSamples[i].dDuration);
                            const double dNewScrollX = dNewPixelStart - dNewConvertToPixel * dInitialVisibleTime * 0.5;
                            gptUI->set_window_scroll((plVec2){(float)dNewScrollX, 0.0f});
                        }
                        pl_temp_allocator_reset(&tTempAllocator);
                        if(gptUI->was_last_item_hovered())
                        {
                            // bHovered = false;
                            gptUI->begin_tooltip();
                            gptUI->color_text(atColors[ptSamples[i].uDepth % 6], "%s", ptSamples[i].pcName);
                            gptUI->text("Duration:   %0.7f seconds", ptSamples[i].dDuration);
                            gptUI->text("Start Time: %0.7f seconds", ptSamples[i].dStartTime);
                            gptUI->color_text(atColors[ptSamples[i].uDepth % 6], "Frame Time: %0.2f %%", 100.0 * ptSamples[i].dDuration / (double)fDeltaTime);
                            gptUI->end_tooltip(); 
                        }
                        gptUI->pop_theme_color(1);
                    }

                    if(bHovered)
                    {
                        gptDraw->add_line(ptFgLayer, (plVec2){tMousePos.x, tCursorPos.y}, (plVec2){tMousePos.x, tWindowEnd.y}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                        char* pcText = pl_temp_allocator_sprintf(&tTempAllocator, "%0.6f", (double)dConvertToTime * (double)(tMousePos.x - tParentCursorPos.x + gptUI->get_window_scroll().x));
                        gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), 13.0f, tMousePos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, pcText, 0.0f);
                        pl_temp_allocator_reset(&tTempAllocator);
                    }

                    gptUI->layout_space_end();

                    gptUI->end_child();
                }
                gptUI->end_tab();
            }
            gptUI->end_tab_bar();
        }
        gptUI->end_window();
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
            float fCurrentWidth = gptDraw->calculate_text_size(gptUI->get_default_font(), 13.0f, ppcNames[i], 0.0f).x;
            if(fCurrentWidth > fLegendWidth)
                fLegendWidth = fCurrentWidth;
        }

        fLegendWidth += 5.0f;
    }

    if(gptUI->begin_window("Statistics", bValue, false))
    {
        static bool bAllowNegative = false;
        gptUI->text("Frame rate: %.0f FPS", ptIOCtx->fFrameRate);
        gptUI->text("Frame time: %.6f s", ptIOCtx->fDeltaTime);
        const plVec2 tCursor = gptUI->get_cursor_pos();

        plDrawLayer2D* ptFgLayer = gptUI->get_window_fg_drawlayer();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowSize = pl_sub_vec2(gptUI->get_window_size(), pl_sub_vec2(tCursor, tWindowPos));
        
        const plVec2 tWindowEnd = pl_add_vec2(gptUI->get_window_size(), tWindowPos);

        gptUI->layout_template_begin(tWindowSize.y - 15.0f);
        gptUI->layout_template_push_static(fLegendWidth * 2.0f);
        gptUI->layout_template_push_dynamic();
        gptUI->layout_template_end();
      
        if(gptUI->begin_child("left"))
        {
            gptUI->layout_dynamic(0.0f, 1);
 
            const plVec4 tNewHeaderColor = (plVec4){0.0f, 0.5f, 0.0f, 0.75f};
            gptUI->push_theme_color(PL_UI_COLOR_HEADER, &tNewHeaderColor);
            for(uint32_t i = 0; i < pl_sb_size(sbppdValues); i++)
            {
                if(gptUI->selectable(ppcNames[i], &sbbValues[i]))
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
            gptUI->pop_theme_color(1);
            gptUI->end_child();
        }

        uint32_t uSelectionSlot = 0;
        apcTempNames = pl_temp_allocator_alloc(&tTempAllocator, sizeof(const char*) * uSelectedCount);
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

        
        if(gptUI->begin_tab_bar("stat tabs"))
        {
            if(gptUI->begin_tab("Plot"))
            {
                
                gptUI->layout_template_begin(tWindowSize.y - 30.0f);
                gptUI->layout_template_push_variable(300.0f);
                gptUI->layout_template_push_static(fLegendWidth * 2.0f + 10.0f);
                gptUI->layout_template_end();
                
                const plVec2 tCursor0 = gptUI->get_cursor_pos();
                const plVec2 tPlotSize = {tWindowEnd.x - tCursor0.x - 10.0f - fLegendWidth - 20.0f, tWindowSize.y - 40.0f};
                const plVec2 tLegendSize = {fLegendWidth + 20.0f, tWindowSize.y - 40.0f};
                gptUI->invisible_button("plot", tPlotSize);

                const plVec2 tCursor1 = gptUI->get_cursor_pos();
                gptUI->invisible_button("legend", tLegendSize);
                gptDraw->add_rect_filled(ptFgLayer, 
                    tCursor0, 
                    pl_add_vec2(tCursor0, tPlotSize),
                    (plVec4){0.2f, 0.0f, 0.0f, 0.5f});
       
                static const plVec4 atColors[6] = {
                    {0.0f, 1.0f, 1.0f, 0.75f},
                    {1.0f, 0.5f, 0.0f, 0.75f},
                    {0.0f, 1.0f, 0.0f, 0.75f},
                    {0.0f, 0.5f, 1.0f, 0.75f},
                    {1.0f, 1.0f, 0.0f, 0.75f},
                    {1.0f, 0.0f, 1.0f, 0.75}
                };

                const double dAxisMultiplier = bAllowNegative ? 2.0 : 1.0;

                // const double dYCenter = tCursor0.y + tPlotSize.y * 0.5f;            
                const double dYCenter = tCursor0.y + tPlotSize.y / dAxisMultiplier;    

                if(bAllowNegative)
                {
                    gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), 13.0f, (plVec2){roundf(tCursor0.x), roundf((float)dYCenter)}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, "0", 0.0f);
                    gptDraw->add_line(ptFgLayer, (plVec2){tCursor0.x, (float)dYCenter}, (plVec2){tCursor0.x + tPlotSize.x, (float)dYCenter}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
                }

                bAllowNegative = false;
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
                        if(dValues[j] < 0.0) bAllowNegative = true;
                    }

                    double dYRange = dAxisMultiplier * pl_maxd(fabs(dMaxValue), fabs(dMinValue)) * (1.1f + (float)i * 0.1f);

                    const double dConversion = dYRange != 0.0 ? (tPlotSize.y - 15.0f) / dYRange : 0.0;
                    
                    const double dXIncrement = (tPlotSize.x - 50.0f) / PL_STATS_MAX_FRAMES;

                    uint32_t uIndexStart = (uint32_t)ptIOCtx->ulFrameCount;

                    const plVec2 tTextPoint = {tCursor1.x + 5.0f, tCursor1.y + i * 15.0f};
                    gptDraw->add_rect_filled(ptFgLayer, tTextPoint, (plVec2){tTextPoint.x + 13.0f, tTextPoint.y + 13.0f}, *ptColor);
                    gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), 13.0f, (plVec2){roundf(tTextPoint.x + 15.0f), roundf(tTextPoint.y)}, *ptColor, apcTempNames[i], 0.0f);
        
                    for(uint32_t j = 0; j < PL_STATS_MAX_FRAMES - 1; j++)
                    {
                        uint32_t uActualIndex0 = (uIndexStart + j) % PL_STATS_MAX_FRAMES;
                        uint32_t uActualIndex1 = (uIndexStart + j + 1) % PL_STATS_MAX_FRAMES;
                        const plVec2 tLineStart = {tCursor0.x + (float)(j * dXIncrement), (float)(dYCenter - dValues[uActualIndex0] * dConversion)};
                        const plVec2 tLineEnd = {tCursor0.x + (float)((j + 1) * dXIncrement), (float)(dYCenter - dValues[uActualIndex1] * dConversion)};
                        gptDraw->add_line(ptFgLayer, tLineStart, tLineEnd, *ptColor, 1.0f);

                        if(j == PL_STATS_MAX_FRAMES - 2)
                        {
                            char acTextBuffer[32] = {0};
                            pl_sprintf(acTextBuffer, "%0.0f", dValues[uActualIndex1]);
                            gptDraw->add_text(ptFgLayer, gptUI->get_default_font(), 13.0f, (plVec2){roundf(tLineEnd.x), roundf(tLineEnd.y) - 6.0f}, *ptColor, acTextBuffer, 0.0f);
                        }
                    }
                    
                }
                gptUI->end_tab();
            }

            if(gptUI->begin_tab("Table"))
            {
                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_static(35.0f);
                for(uint32_t i = 0; i < uSelectedCount; i++)
                    gptUI->layout_template_push_static(100.0f);
                gptUI->layout_template_end();

                gptUI->text("Stat");
                for(uint32_t i = 0; i < uSelectedCount; i++)
                {
                    const float fXPos = gptUI->get_cursor_pos().x - 5.0f;
                    const float fYPos = gptUI->get_cursor_pos().y;
                    gptDraw->add_line(ptFgLayer, (plVec2){fXPos, fYPos}, (plVec2){fXPos, 3000.0f}, (plVec4){0.7f, 0.0f, 0.0f, 1.0f}, 1.0f);
                    gptUI->button(apcTempNames[i]);
                }

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator();

                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_static(35.0f);
                for(uint32_t i = 0; i < uSelectedCount; i++)
                    gptUI->layout_template_push_static(100.0f);
                gptUI->layout_template_end();

                uint32_t uIndexStart = (uint32_t)ptIOCtx->ulFrameCount;

                plUiClipper tClipper = {PL_STATS_MAX_FRAMES};
                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        gptUI->text("%u", i);
                        for(uint32_t j = 0; j < uSelectedCount; j++)
                        {
                            double* dValues = &sbdRawValues[j * PL_STATS_MAX_FRAMES];
                            uint32_t uActualIndex0 = (uIndexStart + j) % PL_STATS_MAX_FRAMES;
                            gptUI->text("%13.6f", dValues[uActualIndex0]);
                        }
                    } 
                }

                gptUI->end_tab();
            }
            gptUI->end_tab_bar();
        }
        pl_temp_allocator_reset(&tTempAllocator);
        gptUI->end_window();
    } 
}

static void
pl__show_device_memory(bool* bValue)
{
    static const plVec4 tAvailableColor = {0.234f, 0.703f, 0.234f, 1.0f};
    static const plVec4 tUsedColor      = {0.703f, 0.234f, 0.234f, 1.0f};
    static const plVec4 tWastedColor    = {0.703f, 0.703f, 0.234f, 1.0f};
    static const plVec4 tWhiteColor     = {1.0f, 1.0f, 1.0f, 1.0f};
    static const plVec4 tButtonColor    = {0.05f, 0.05f, 0.05f, 1.0f};

    if(!ptDevice)
        ptDevice = ptDataRegistry->get_data("device");
        
    if(gptUI->begin_window("Device Memory Analyzer", bValue, false))
    {
        plDrawLayer2D* ptFgLayer = gptUI->get_window_fg_drawlayer();
        const plVec2 tWindowSize = gptUI->get_window_size();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);
        const plVec2 tMousePos = gptIO->get_mouse_pos();

        if(ptDevice->ptGraphics->szLocalMemoryInUse > 1000000000)
            gptUI->text("Device Local Memory: %0.3f gb", (double)ptDevice->ptGraphics->szLocalMemoryInUse / 1000000000);
        else if(ptDevice->ptGraphics->szLocalMemoryInUse > 1000000)
            gptUI->text("Device Local Memory: %0.3f mb", (double)ptDevice->ptGraphics->szLocalMemoryInUse / 1000000);
        else if(ptDevice->ptGraphics->szLocalMemoryInUse > 1000)
            gptUI->text("Device Local Memory: %0.3f kb", (double)ptDevice->ptGraphics->szLocalMemoryInUse / 1000);
        else
            gptUI->text("Device Local Memory: %llu bytes", (double)ptDevice->ptGraphics->szLocalMemoryInUse);

        if(ptDevice->ptGraphics->szHostMemoryInUse > 1000000000)
            gptUI->text("Host Memory: %0.3f gb", (double)ptDevice->ptGraphics->szHostMemoryInUse / 1000000000);
        else if(ptDevice->ptGraphics->szHostMemoryInUse > 1000000)
            gptUI->text("Host Memory: %0.3f mb", (double)ptDevice->ptGraphics->szHostMemoryInUse / 1000000);
        else if(ptDevice->ptGraphics->szHostMemoryInUse > 1000)
            gptUI->text("Host Memory: %0.3f kb", (double)ptDevice->ptGraphics->szHostMemoryInUse / 1000);
        else
            gptUI->text("Host Memory: %llu bytes", (double)ptDevice->ptGraphics->szHostMemoryInUse);

        const plDeviceMemoryAllocatorI atAllocators[] = {
            *gptGpuAllocators->get_local_buddy_allocator(ptDevice),
            *gptGpuAllocators->get_local_dedicated_allocator(ptDevice),
            *gptGpuAllocators->get_staging_uncached_allocator(ptDevice),
            *gptGpuAllocators->get_staging_cached_allocator(ptDevice)
        };

        const char* apcAllocatorNames[] = {
            "Device Memory: Local Buddy",
            "Device Memory: Local Dedicated",
            "Device Memory: Staging Uncached",
            "Device Memory: Staging Cached"
        };

        gptUI->push_theme_color(PL_UI_COLOR_BUTTON, &tButtonColor);
        gptUI->push_theme_color(PL_UI_COLOR_BUTTON_ACTIVE, &tButtonColor);
        gptUI->push_theme_color(PL_UI_COLOR_BUTTON_HOVERED, &tButtonColor);
        for(uint32_t uAllocatorIndex = 0; uAllocatorIndex < 4; uAllocatorIndex++)
        {
            uint32_t uBlockCount = 0;
            uint32_t uRangeCount = 0;
            plDeviceMemoryAllocation* sbtBlocks = gptGpuAllocators->get_blocks(&atAllocators[uAllocatorIndex], &uBlockCount);
            plDeviceAllocationRange* sbtRanges = gptGpuAllocators->get_ranges(&atAllocators[uAllocatorIndex], &uRangeCount);
            if(uBlockCount > 0)
            {

                gptUI->layout_dynamic(0.0f, 1);
                gptUI->separator();
                gptUI->text(apcAllocatorNames[uAllocatorIndex]);

                gptUI->layout_template_begin(30.0f);
                gptUI->layout_template_push_static(150.0f);
                gptUI->layout_template_push_variable(300.0f);
                gptUI->layout_template_end();

                float fWidth0 = -1.0f;
                float fHeight0 = -1.0f;
                uint64_t ulHoveredBlock = UINT64_MAX;

                static const uint64_t ulMaxBlockSize = PL_DEVICE_BUDDY_BLOCK_SIZE;

                uint32_t iCurrentBlock = 0;

                for(uint32_t i = 0; i < uBlockCount; i++)
                {
                    plDeviceMemoryAllocation* ptBlock = &sbtBlocks[i];
                    if(ptBlock->ulSize == 0)
                        continue;

                    char* pcTempBuffer0 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u: %0.1fMB##%u", iCurrentBlock, ((double)ptBlock->ulSize)/1000000.0, uAllocatorIndex);
                    char* pcTempBuffer1 = pl_temp_allocator_sprintf(&tTempAllocator, "Block %u##%u", iCurrentBlock, uAllocatorIndex);

                    gptUI->button(pcTempBuffer0);
                    
                    plVec2 tCursor0 = gptUI->get_cursor_pos();

                    if(fHeight0 == -1.0f)
                    {
                        fWidth0  = tCursor0.x;
                        fHeight0 = tCursor0.y;
                    }

                    const float fWidthAvailable = tWindowEnd.x - tCursor0.x;
                    const float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;

                    if(ptBlock->uHandle == 0)
                        gptDraw->add_rect(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth - 6.0f, 30.0f + tCursor0.y}, tAvailableColor, 1.0f);
                    else
                        gptDraw->add_rect_filled(ptFgLayer, tCursor0, (plVec2){tCursor0.x + fTotalWidth, 30.0f + tCursor0.y}, tAvailableColor);
                    gptUI->invisible_button(pcTempBuffer1, (plVec2){fTotalWidth, 30.0f});
                    if(gptUI->was_last_item_hovered())
                    {
                        ulHoveredBlock = (uint64_t)i;
                    }
                    pl_temp_allocator_reset(&tTempAllocator);
                    ptBlock->uCurrentIndex = iCurrentBlock;
                    iCurrentBlock++;
                }

                for(uint32_t i = 0; i < uRangeCount; i++)
                {

                    plDeviceAllocationRange* ptRange = &sbtRanges[i];
                    plDeviceMemoryAllocation* ptBlock = &sbtBlocks[ptRange->ulBlockIndex];

                    if(ptRange->ulUsedSize == 0 || ptRange->ulUsedSize == UINT64_MAX)
                        continue;
                    
                    const float fWidthAvailable = tWindowEnd.x - fWidth0;
                    // const float fTotalWidth = fWidthAvailable * ((float)ptBlock->ulSize) / (float)ulMaxBlockSize;
                    const float fStartPos       = fWidth0 + fWidthAvailable * ((float)ptRange->ulOffset) / (float)ulMaxBlockSize;
                    const float fUsedWidth      = fWidthAvailable * ((float)ptRange->ulUsedSize) / (float)ulMaxBlockSize;
                    const float fAvailableWidth = fWidthAvailable * ((float)ptRange->ulTotalSize) / (float)ulMaxBlockSize;

                    const float fYPos = fHeight0 + 34.0f * (float)ptBlock->uCurrentIndex;
                    gptDraw->add_rect_filled(ptFgLayer, (plVec2){fStartPos, fYPos}, (plVec2){fStartPos + fAvailableWidth, 30.0f + fYPos}, tWastedColor);
                    gptDraw->add_rect_filled(ptFgLayer, (plVec2){fStartPos, fYPos}, (plVec2){fStartPos + fUsedWidth, 30.0f + fYPos}, tUsedColor);

                    if(ptRange->ulBlockIndex == ulHoveredBlock)
                    {
                        const plRect tHitBox = pl_calculate_rect((plVec2){fStartPos, fYPos}, (plVec2){fAvailableWidth, 30});
                        if(pl_rect_contains_point(&tHitBox, tMousePos))
                        {
                            gptDraw->add_rect(ptFgLayer, tHitBox.tMin, tHitBox.tMax, tWhiteColor, 1.0f);
                            gptUI->begin_tooltip();
                            gptUI->text(ptRange->acName);
                            gptUI->text("Offset:          %lu", ptRange->ulOffset);
                            gptUI->text("Requested Size:  %lu", ptRange->ulUsedSize);
                            gptUI->text("Allocated Size:  %lu", ptRange->ulTotalSize);
                            gptUI->text("Wasted:          %lu", ptRange->ulTotalSize - ptRange->ulUsedSize);
                            gptUI->end_tooltip();
                        }
                    }
                }
            }
        }
        gptUI->pop_theme_color(3);
        gptUI->end_window();
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

    if(gptUI->begin_window("Logging", bValue, false))
    {
        const plVec2 tWindowSize = gptUI->get_window_size();
        const plVec2 tWindowPos = gptUI->get_window_pos();
        const plVec2 tWindowEnd = pl_add_vec2(tWindowSize, tWindowPos);

        static bool bActiveLevels[6] = { true, true, true, true, true, true};

        gptUI->layout_dynamic(0.0f, 6);
        gptUI->checkbox("Trace", &bActiveLevels[0]);
        gptUI->checkbox("Debug", &bActiveLevels[1]);
        gptUI->checkbox("Info", &bActiveLevels[2]);
        gptUI->checkbox("Warn", &bActiveLevels[3]);
        gptUI->checkbox("Error", &bActiveLevels[4]);
        gptUI->checkbox("Fatal", &bActiveLevels[5]);

        bool bUseClipper = true;
        for(uint32_t i = 0; i < 6; i++)
        {
            if(bActiveLevels[i] == false)
            {
                bUseClipper = false;
                break;
            }
        }

        gptUI->layout_dynamic(0.0f, 1);
        if(gptUI->begin_tab_bar("tab bar"))
        {
            uint32_t uChannelCount = 0;
            plLogChannel* ptChannels = pl_get_log_channels(&uChannelCount);
            for(uint32_t i = 0; i < uChannelCount; i++)
            {
                plLogChannel* ptChannel = &ptChannels[i];
                
                if(ptChannel->tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER)
                {
                    if(gptUI->begin_tab(ptChannel->pcName))
                    {
                        const plVec2 tCursorPos = gptUI->get_cursor_pos();
                        gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(gptUI->begin_child(ptChannel->pcName))
                        {
                            gptUI->layout_dynamic(0.0f, 1);
                            const uint64_t uIndexStart = ptChannel->uEntryCount;
                            const uint64_t uLogCount = pl_min(PL_LOG_CYCLIC_BUFFER_SIZE, ptChannel->uEntryCount);

                            
                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)uLogCount};
                                while(gptUI->step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogEntry* ptEntry = &ptChannel->atEntries[uActualIndex0];
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset + ptChannel->uBufferCapacity * (ptEntry->uGeneration % 2)]);
                                    } 
                                }
                            }
                            else
                            {
                                    for(uint32_t j = i; j < uLogCount; j++)
                                    {
                                        uint32_t uActualIndex0 = (uIndexStart + j) % (uint32_t)uLogCount;
                                        const plLogEntry* ptEntry = &ptChannel->atEntries[uActualIndex0];
                                        if(bActiveLevels[ptEntry->uLevel / 1000 - 5])
                                            gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset + ptChannel->uBufferCapacity * (ptEntry->uGeneration % 2)]);
                                    }  
                            }
                            gptUI->end_child();
                        }
                        gptUI->end_tab();
                    }
                }
                else if(ptChannel->tType & PL_CHANNEL_TYPE_BUFFER)
                {
                    if(gptUI->begin_tab(ptChannel->pcName))
                    {
                        const plVec2 tCursorPos = gptUI->get_cursor_pos();
                        gptUI->layout_dynamic(tWindowEnd.y - tCursorPos.y - 20.0f, 1);
                        if(gptUI->begin_child(ptChannel->pcName))
                        {
                            gptUI->layout_dynamic(0.0f, 1);

                            if(bUseClipper)
                            {
                                plUiClipper tClipper = {(uint32_t)ptChannel->uEntryCount};
                                while(gptUI->step_clipper(&tClipper))
                                {
                                    for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                    {
                                        plLogEntry* ptEntry = &ptChannel->pEntries[j];
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset]);
                                    } 
                                }
                            }
                            else
                            {
                                for(uint32_t j = 0; j < ptChannel->uEntryCount; j++)
                                {
                                    plLogEntry* ptEntry = &ptChannel->pEntries[j];
                                    if(bActiveLevels[ptEntry->uLevel / 1000 - 5])
                                        gptUI->color_text(atColors[ptEntry->uLevel / 1000 - 5], &ptChannel->pcBuffer0[ptEntry->uOffset]);
                                } 
                            }
                            gptUI->end_child();
                        }
                        gptUI->end_tab();
                    }
                }
            }

            gptUI->end_tab_bar();
        }
        gptUI->end_window();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    gptApiRegistry = ptApiRegistry;
    ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    ptMemoryCtx = ptDataRegistry->get_data(PL_CONTEXT_MEMORY);
    pl_set_memory_context(ptMemoryCtx);
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_log_context(ptDataRegistry->get_data("log"));

    // load required extensions (may already be loaded)
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_gpu_allocators_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_ui_ext", NULL, NULL, true);

    // load required APIs
    ptStatsApi       = ptApiRegistry->first(PL_API_STATS);
    gptGpuAllocators = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);
    gptDraw          = ptApiRegistry->first(PL_API_DRAW);
    gptUI            = ptApiRegistry->first(PL_API_UI);
    gptIO            = ptApiRegistry->first(PL_API_IO);

    // load required contexts
    ptIOCtx = gptIO->get_io();

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
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
    pl_sb_free(sbppdValues);
    pl_sb_free(sbppdFrameValues);
    pl_sb_free(sbdRawValues);
    pl_sb_free(sbbValues);
    pl_sb_free(sbtSamples);
    pl_temp_allocator_free(&tTempAllocator);
}
